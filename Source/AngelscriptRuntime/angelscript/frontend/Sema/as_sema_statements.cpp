#include "frontend/Sema/as_sema.h"

#include "frontend/AST/as_ast_cast.h"
#include "frontend/Sema/as_constant_evaluator.h"

#include "Containers/Set.h"

BEGIN_AS_NAMESPACE

namespace
	{
		asCQualType StatementValueType(asCQualType Type)
		{
			asCQualType Result;
			asCQualType::TryCreateRaw(Type.GetType(),
				static_cast<uint8>(Type.GetQualifiers()) & ~static_cast<uint8>(asETypeQualifiers::Reference),
				Result);
			return Result;
		}

		bool StatementSameType(asCQualType Left, asCQualType Right)
		{
			if (!Left.IsValid() || !Right.IsValid()) return false;
			if (Left.GetType() == Right.GetType()) return true;
			return Left.GetType()->HasStableIdentity() && Right.GetType()->HasStableIdentity()
				&& Left.GetType()->GetStableIdentity() == Right.GetType()->GetStableIdentity();
		}

		bool StatementIsNumeric(asCQualType Type)
		{
			const auto* Builtin = dyn_cast<asCBuiltinType>(Type.GetType());
			return Builtin && Builtin->GetPrimitive() > asEPrimitiveType::Bool
				&& Builtin->GetPrimitive() <= asEPrimitiveType::Float64;
		}

		bool StatementIsBool(asCQualType Type)
		{
			const auto* Builtin = dyn_cast<asCBuiltinType>(Type.GetType());
			return Builtin && Builtin->GetPrimitive() == asEPrimitiveType::Bool;
		}

		bool StatementCanInitialize(asCQualType Target, asCQualType Source)
		{
			if (!Target.IsValid() || !Source.IsValid() || Target.IsHandle() != Source.IsHandle()) return false;
			if (Target.IsReference() && !Target.IsConst()
				&& (!Source.IsReference() || Source.IsConst())) return false;
			if (StatementSameType(StatementValueType(Target), StatementValueType(Source)))
			{
				if (Source.IsConst() && !Target.IsConst()) return false;
				return true;
			}
			return !Target.IsReference() && !Target.IsHandle()
				&& StatementIsNumeric(Target) && StatementIsNumeric(Source);
		}

		asCQualType StatementResolveForeachAuto(const asSTypeSyntax& Syntax, asCQualType Produced)
		{
			if (!Produced.IsValid() || Syntax.NameParts.Num() != 1 || Syntax.NameParts[0] != TEXT("auto")
				|| Syntax.bGlobal || !Syntax.GenericArguments.IsEmpty() || !Syntax.ArrayDimensions.IsEmpty()
				|| (Syntax.bHandle && !Produced.IsHandle())) return {};
			uint8 Qualifiers = static_cast<uint8>(Produced.GetQualifiers())
				& ~static_cast<uint8>(asETypeQualifiers::Reference);
			if (!Produced.IsHandle()) Qualifiers &= ~static_cast<uint8>(asETypeQualifiers::Const);
			if (Syntax.bConst) Qualifiers |= static_cast<uint8>(asETypeQualifiers::Const);
			if (Syntax.Reference != asETypeReferenceDirection::None)
				Qualifiers |= static_cast<uint8>(asETypeQualifiers::Reference);
			asCQualType Result;
			asCQualType::TryCreateRaw(Produced.GetType(), Qualifiers, Result);
			return Result;
		}

		bool StatementIsTerminator(const asCStmt* Statement)
		{
			return isa<asCBreakStmt>(Statement) || isa<asCContinueStmt>(Statement)
				|| isa<asCReturnStmt>(Statement) || isa<asCFallthroughStmt>(Statement);
		}
	}

	asCReturnStmt* asCSema::ActOnReturn(asCExpr* Value, asCSourceRange Range)
	{
		asCFunctionDecl* Function = CurrentBody ? CurrentBody->GetWorkItem().Function : nullptr;
		asCType* ReturnType = Function ? Function->GetResolvedReturnType() : nullptr;
		const asCBuiltinType* Builtin = dyn_cast<asCBuiltinType>(ReturnType);
		const bool bVoid = Builtin && Builtin->GetPrimitive() == asEPrimitiveType::Void;
		bool bValid = ReturnType && (bVoid ? Value == nullptr : Value != nullptr);
		if (bValid && Value)
		{
			asCExpr* Converted = ConvertTo(Value, Function->GetResolvedSignature()->GetReturnType());
			bValid = Converted != nullptr;
			if (Converted) Value = Converted;
		}
		asCReturnStmt* Return = Context.Create<asCReturnStmt>(Value, Range, Function);
		if (!bValid)
		{
			Return->MarkInvalid();
			ReportBodyError(Range, TEXT("incompatible-return-value"));
		}
		return Return;
	}

	asCCompoundStmt* asCSema::ActOnCompound(TArray<asCStmt*> Statements, asCSourceRange Range)
	{
		asCCompoundStmt* Compound = Context.Create<asCCompoundStmt>(MoveTemp(Range));
		for (asCStmt* Statement : Statements) Compound->AddStatement(Statement);
		return Compound;
	}

	void asCSema::PushBodyScope()
	{
		BodyScopes.AddDefaulted();
	}

	void asCSema::PopBodyScope()
	{
		if (!BodyScopes.IsEmpty()) BodyScopes.Pop();
	}

	asCDeclStmt* asCSema::ActOnLocalVariable(FString TypeName, FString Name, asCExpr* Initializer, asCSourceRange Range)
	{
		asSTypeSyntax Type;
		Type.NameParts.Add(MoveTemp(TypeName));
		Type.ComponentRanges.Add(Range);
		Type.Range = Range;
		return ActOnLocalVariable(MoveTemp(Type), MoveTemp(Name), Initializer, MoveTemp(Range));
	}

	asCDeclStmt* asCSema::ActOnLocalVariable(asSTypeSyntax TypeSyntax, FString Name, asCExpr* Initializer, asCSourceRange Range)
	{
		asCVarDecl* Declaration = Context.Create<asCVarDecl>(asEDeclKind::Var, Name, Range);
		const FString TypeName = (TypeSyntax.bGlobal ? TEXT("::") : TEXT("")) + TypeSyntax.GetQualifiedName();
		const bool bAuto = TypeSyntax.NameParts.Num() == 1 && TypeSyntax.NameParts[0] == TEXT("auto");
		const asCQualType ResolvedType = bAuto
			? ResolveAutoVariableType(TypeSyntax, Initializer) : ResolveBodyType(TypeSyntax);
		Declaration->SetTypeSyntax(MoveTemp(TypeSyntax));
		asCType* Type = ResolvedType.GetType();
		const asCBuiltinType* Builtin = dyn_cast<asCBuiltinType>(Type);
		if (!Type || (Builtin && Builtin->GetPrimitive() == asEPrimitiveType::Void))
		{
			Declaration->SetResolvedType(Context.Create<asCErrorType>());
			Declaration->MarkInvalid();
			ReportBodyError(Range, bAuto ? TEXT("invalid-auto-initializer")
				: Type ? FString::Printf(TEXT("invalid-local-type:%s"), *TypeName)
				: FString::Printf(TEXT("unresolved-local-type:%s"), *TypeName));
		}
		else
		{
			Declaration->SetResolvedQualType(ResolvedType);
			if (Initializer)
			{
				asCExpr* Converted = ConvertTo(Initializer, ResolvedType);
				if (Converted) Initializer = Converted;
				else
				{
					Declaration->MarkInvalid();
					ReportBodyError(Range, TEXT("incompatible-local-initializer"));
				}
			}
		}
		if (!BodyScopes.IsEmpty() && BodyScopes.Last().Contains(Name))
		{
			Declaration->MarkInvalid();
			ReportBodyError(Range, FString::Printf(TEXT("duplicate-local:%s"), *Name));
		}
		if (!Declaration->IsInvalid() && !BodyScopes.IsEmpty()) BodyScopes.Last().Add(Name, Declaration);
		return Context.Create<asCDeclStmt>(Declaration, Initializer, MoveTemp(Range));
	}

	asCWhileStmt* asCSema::ActOnWhileStart(asCExpr* Condition, asCSourceRange Range)
	{
		const bool bValid = CheckCondition(Condition);
		asCWhileStmt* Result = Context.Create<asCWhileStmt>(Condition, nullptr, MoveTemp(Range));
		if (!bValid) Result->MarkInvalid();
		return Result;
	}

	asCIfStmt* asCSema::ActOnIf(asCExpr* Condition, asCStmt* Then, asCStmt* Else, asCSourceRange Range)
	{
		const bool bValid = CheckCondition(Condition);
		asCIfStmt* Result = Context.Create<asCIfStmt>(Condition, Then, Else, Range);
		if (!bValid || !Then) Result->MarkInvalid();
		return Result;
	}

	asCForStmt* asCSema::ActOnForStart(
		TArray<asCStmt*> Initializers,
		asCExpr* Condition,
		TArray<asCExpr*> Increments,
		asCSourceRange HeaderRange)
	{
		const bool bValid = !Condition || CheckCondition(Condition);
		asCForStmt* Result = Context.Create<asCForStmt>(MoveTemp(Initializers), Condition,
			MoveTemp(Increments), nullptr, HeaderRange, HeaderRange);
		if (!bValid) Result->MarkInvalid();
		return Result;
	}

	asCDoStmt* asCSema::ActOnDoStart(asCSourceRange Range)
	{
		return Context.Create<asCDoStmt>(nullptr, nullptr, MoveTemp(Range));
	}

	asCNullStmt* asCSema::ActOnNullStatement(asCSourceRange Range)
	{
		return Context.Create<asCNullStmt>(MoveTemp(Range));
	}

	asCWhileStmt* asCSema::ActOnWhileFinish(asCWhileStmt* While, asCStmt* Body)
	{
		if (!While || !While->SetBody(Body))
		{
			if (CurrentBody) CurrentBody->MarkError(TEXT("invalid-while-body"));
			return nullptr;
		}
		return While;
	}

	asCSwitchStmt* asCSema::ActOnSwitchStart(asCExpr* Selector, asCSourceRange HeaderRange)
	{
		asCSwitchStmt* Result = Context.Create<asCSwitchStmt>(Selector, HeaderRange, HeaderRange);
		bool bValid = Selector != nullptr;
		if (const auto* Builtin = Selector ? dyn_cast<asCBuiltinType>(Selector->GetExpressionType()) : nullptr)
		{
			bValid = Builtin->GetPrimitive() >= asEPrimitiveType::Int8
				&& Builtin->GetPrimitive() <= asEPrimitiveType::UInt64;
		}
		else if (const auto* Nominal = Selector ? dyn_cast<asCNominalType>(Selector->GetExpressionType()) : nullptr)
		{
			asSNominalTypeDescriptor Descriptor;
			bValid = Context.GetTypeContext()->GetIdentityRegistry().ResolveNominalType(
				Nominal->GetDeclarationIdentity(), Descriptor) == asETypeIdentityStatus::Succeeded
				&& Descriptor.Kind == asETypeDeclKind::Enum;
		}
		else bValid = false;
		if (!bValid)
		{
			Result->MarkInvalid();
			ReportBodyError(Selector ? Selector->GetSourceRange() : HeaderRange,
				TEXT("switch-requires-integral-or-enum"));
		}
		return Result;
	}

	asCCaseStmt* asCSema::ActOnCase(
		asCSwitchStmt& Switch,
		asCExpr* Label,
		asCSourceRange KeywordRange,
		asCSourceRange ColonRange)
	{
		asCCaseStmt* Result = Context.Create<asCCaseStmt>(&Switch, Label, KeywordRange, ColonRange,
			asCSourceRange{KeywordRange.Begin, ColonRange.End});
		const asSIntegralConstantEvaluation Evaluation = Label
			? asEvaluateIntegralConstant(*Label) : asSIntegralConstantEvaluation{};
		uint64 Converted = 0;
		if (!Label || !Switch.GetSelector()
			|| Evaluation.Status != asEConstantEvaluationStatus::Succeeded
			|| !asTryConvertIntegralConstant(Evaluation, Switch.GetSelector()->GetExpressionQualType(), Converted)
			|| !Result->SetResolvedValueBits(Converted))
		{
			Result->MarkInvalid();
			ReportBodyError(Label ? Label->GetSourceRange() : ColonRange,
				Evaluation.Status == asEConstantEvaluationStatus::Succeeded
					? TEXT("case-type-mismatch") : TEXT("case-requires-integral-constant"));
		}
		Switch.AddClause(Result);
		return Result;
	}

	asCDefaultStmt* asCSema::ActOnDefault(
		asCSwitchStmt& Switch,
		asCSourceRange KeywordRange,
		asCSourceRange ColonRange)
	{
		asCDefaultStmt* Result = Context.Create<asCDefaultStmt>(&Switch, KeywordRange, ColonRange,
			asCSourceRange{KeywordRange.Begin, ColonRange.End});
		Switch.AddClause(Result);
		return Result;
	}

	asCFallthroughStmt* asCSema::ActOnFallthrough(asCSourceRange Range)
	{
		return Context.Create<asCFallthroughStmt>(nullptr, MoveTemp(Range));
	}

	bool asCSema::FinishSwitch(asCSwitchStmt& Switch)
	{
		TSet<uint64> Values;
		int32 DefaultCount = 0;
		bool bValid = !Switch.IsInvalid();
		for (int32 ClauseIndex = 0; ClauseIndex < Switch.GetClauses().Num(); ++ClauseIndex)
		{
			asCSwitchClause* Clause = Switch.GetClauses()[ClauseIndex];
			if (asCCaseStmt* Case = dyn_cast<asCCaseStmt>(Clause))
			{
				if (!Case->HasResolvedValue() || Values.Contains(Case->GetResolvedValueBits()))
				{
					bValid = false;
					ReportBodyError(Case->GetSourceRange(), TEXT("duplicate-case-value"));
				}
				else Values.Add(Case->GetResolvedValueBits());
			}
			else if (++DefaultCount > 1)
			{
				bValid = false;
				ReportBodyError(Clause->GetSourceRange(), TEXT("duplicate-default"));
			}
			const TConstArrayView<asCStmt*> Statements = Clause->GetStatements();
			if (!Statements.IsEmpty())
			{
				for (int32 StatementIndex = 0; StatementIndex + 1 < Statements.Num(); ++StatementIndex)
				{
					if (const asCFallthroughStmt* Fallthrough = dyn_cast<asCFallthroughStmt>(Statements[StatementIndex]))
					{
						bValid = false;
						ReportBodyError(Fallthrough->GetSourceRange(), TEXT("fallthrough-must-terminate-case"));
					}
				}
				if (const asCFallthroughStmt* Fallthrough = dyn_cast<asCFallthroughStmt>(Statements.Last()))
				{
					if (ClauseIndex + 1 >= Switch.GetClauses().Num()
						|| Fallthrough->GetTarget() != Switch.GetClauses()[ClauseIndex + 1])
					{
						bValid = false;
						ReportBodyError(Fallthrough->GetSourceRange(), TEXT("fallthrough-from-last-case"));
					}
				}
				else if (ClauseIndex + 1 < Switch.GetClauses().Num()
					&& !StatementIsTerminator(Statements.Last()))
				{
					ReportBodyWarning(Statements.Last()->GetSourceRange(), TEXT("implicit-case-fallthrough"));
				}
			}
		}
		if (Switch.GetClauses().IsEmpty())
		{
			bValid = false;
			ReportBodyError(Switch.GetSourceRange(), TEXT("empty-switch"));
		}
		if (!bValid) Switch.MarkInvalid();
		return bValid;
	}

	asCForeachStmt* asCSema::ActOnForeachStart(
		asSParsedForeachVariable Value,
		asSParsedForeachVariable Key,
		asCExpr* RangeExpression,
		asCSourceRange HeaderRange,
		asCSourceRange ColonRange)
	{
		asSForeachProtocol Protocol;
		Protocol.ContainerType = RangeExpression ? RangeExpression->GetExpressionQualType() : asCQualType{};
		asCRecordDecl* Record = RangeExpression ? ResolveRecord(RangeExpression->GetExpressionType()) : nullptr;
		const bool bConstReceiver = RangeExpression && RangeExpression->IsReadOnly();
		bool bAmbiguous = false;
		auto Select = [&](const TCHAR* Name, auto&& Accept) -> asCFunctionDecl*
		{
			TArray<asCDecl*> Candidates;
			LookupMembers(Record, Name, Candidates);
			asCFunctionDecl* Best = nullptr;
			int32 BestScore = MAX_int32;
			bAmbiguous = false;
			for (asCDecl* CandidateDecl : Candidates)
			{
				asCFunctionDecl* Candidate = dyn_cast<asCFunctionDecl>(CandidateDecl);
				if (!Candidate || Candidate->IsInvalid() || !Candidate->GetResolvedSignature()
					|| !CanAccessMember(Candidate) || (bConstReceiver && !Candidate->IsConstMethod())
					|| !Accept(*Candidate)) continue;
				const int32 Score = !bConstReceiver && Candidate->IsConstMethod() ? 1 : 0;
				if (Score < BestScore) { Best = Candidate; BestScore = Score; bAmbiguous = false; }
				else if (Score == BestScore) bAmbiguous = true;
			}
			return bAmbiguous ? nullptr : Best;
		};
		auto ReportProtocolFailure = [&](const TCHAR* Name)
		{
			ReportBodyError(HeaderRange, FString::Printf(TEXT("foreach-%s-%s"), Name,
				bAmbiguous ? TEXT("ambiguous") : TEXT("not-found")));
		};

		if (Record)
		{
			Protocol.BeginFunction = Select(TEXT("opForBegin"), [](const asCFunctionDecl& Function)
			{
				const asCFunctionType* Signature = Function.GetResolvedSignature();
				const auto* Return = Signature ? dyn_cast<asCBuiltinType>(Signature->GetReturnType().GetType()) : nullptr;
				return Signature && Signature->GetParameterTypes().IsEmpty()
					&& (!Return || Return->GetPrimitive() != asEPrimitiveType::Void);
			});
			if (!Protocol.BeginFunction) ReportProtocolFailure(TEXT("begin"));
		}
		else ReportBodyError(HeaderRange, TEXT("foreach-range-not-iterable"));

		if (Protocol.BeginFunction)
			Protocol.IteratorType = StatementValueType(Protocol.BeginFunction->GetResolvedSignature()->GetReturnType());
		auto IteratorParameter = [&](const asCFunctionDecl& Function)
		{
			const asCFunctionType* Signature = Function.GetResolvedSignature();
			if (!Signature || Signature->GetParameterTypes().Num() != 1
				|| Signature->GetParameterPassingModes().Num() != 1
				|| !StatementSameType(StatementValueType(Signature->GetParameterTypes()[0]), Protocol.IteratorType)) return false;
			const asEParameterPassingMode Mode = Signature->GetParameterPassingModes()[0];
			return Mode == asEParameterPassingMode::Value || Mode == asEParameterPassingMode::In;
		};
		if (Protocol.IteratorType.IsValid())
		{
			Protocol.EndFunction = Select(TEXT("opForEnd"), [&](const asCFunctionDecl& Function)
			{
				return IteratorParameter(Function) && StatementIsBool(Function.GetResolvedSignature()->GetReturnType());
			});
			if (!Protocol.EndFunction) ReportProtocolFailure(TEXT("end"));
			Protocol.NextFunction = Select(TEXT("opForNext"), IteratorParameter);
			if (!Protocol.NextFunction) ReportProtocolFailure(TEXT("next"));
			Protocol.ValueFunction = Select(TEXT("opForValue"), IteratorParameter);
			if (!Protocol.ValueFunction) ReportProtocolFailure(TEXT("value"));
			if (Key.IsValid())
			{
				Protocol.KeyFunction = Select(TEXT("opForKey"), IteratorParameter);
				if (!Protocol.KeyFunction) ReportProtocolFailure(TEXT("key"));
			}
		}

		auto CreateVariable = [&](asSParsedForeachVariable Parsed, asCQualType Produced) -> asCDeclStmt*
		{
			asCVarDecl* Declaration = Context.Create<asCVarDecl>(asEDeclKind::Var, Parsed.Name, Parsed.Range);
			const bool bAuto = Parsed.Type.NameParts.Num() == 1 && Parsed.Type.NameParts[0] == TEXT("auto");
			const asCQualType Resolved = bAuto
				? StatementResolveForeachAuto(Parsed.Type, Produced) : ResolveBodyType(Parsed.Type);
			Declaration->SetTypeSyntax(MoveTemp(Parsed.Type));
			if (Resolved.IsValid()) Declaration->SetResolvedQualType(Resolved);
			else Declaration->SetResolvedType(Context.Create<asCErrorType>());
			if (!Resolved.IsValid() || !StatementCanInitialize(Declaration->GetResolvedQualType(), Produced))
			{
				Declaration->MarkInvalid();
				ReportBodyError(Parsed.Range, TEXT("foreach-variable-conversion-failed"));
			}
			if (!BodyScopes.IsEmpty() && BodyScopes.Last().Contains(Parsed.Name))
			{
				Declaration->MarkInvalid();
				ReportBodyError(Parsed.Range, FString::Printf(TEXT("duplicate-local:%s"), *Parsed.Name));
			}
			if (!Declaration->IsInvalid() && !BodyScopes.IsEmpty()) BodyScopes.Last().Add(Parsed.Name, Declaration);
			return Context.Create<asCDeclStmt>(Declaration, nullptr, Parsed.Range);
		};

		const asCQualType ValueResult = Protocol.ValueFunction
			? Protocol.ValueFunction->GetResolvedSignature()->GetReturnType() : asCQualType{};
		asCDeclStmt* ValueStatement = CreateVariable(MoveTemp(Value), ValueResult);
		asCDeclStmt* KeyStatement = nullptr;
		if (Key.IsValid())
		{
			const asCQualType KeyResult = Protocol.KeyFunction
				? Protocol.KeyFunction->GetResolvedSignature()->GetReturnType() : asCQualType{};
			KeyStatement = CreateVariable(MoveTemp(Key), KeyResult);
		}
		asCForeachStmt* Result = Context.Create<asCForeachStmt>(ValueStatement, KeyStatement,
			RangeExpression, MoveTemp(Protocol), HeaderRange, ColonRange, HeaderRange);
		if (!Record || !Result->GetProtocol().BeginFunction || !Result->GetProtocol().EndFunction
			|| !Result->GetProtocol().NextFunction || !Result->GetProtocol().ValueFunction
			|| (KeyStatement && !Result->GetProtocol().KeyFunction)
			|| ValueStatement->GetDeclaration()->IsInvalid()
			|| (KeyStatement && KeyStatement->GetDeclaration()->IsInvalid())) Result->MarkInvalid();
		return Result;
	}

	asCStmt* asCSema::ActOnBreak(asCSourceRange Range)
	{
		for (int32 Index = ControlTargets.Num() - 1; Index >= 0; --Index)
			if (ControlTargets[Index].bBreakable)
				return Context.Create<asCBreakStmt>(ControlTargets[Index].Target, MoveTemp(Range));
		return ActOnRecoveryStatement(MoveTemp(Range), TEXT("break-outside-control"));
	}

	void asCSema::PushControlTarget(asCStmt* Target, bool bContinuable)
	{
		if (Target) ControlTargets.Add({Target, true, bContinuable});
	}

	void asCSema::PopControlTarget()
	{
		if (!ControlTargets.IsEmpty()) ControlTargets.Pop();
	}

	asCStmt* asCSema::ActOnContinue(asCSourceRange Range)
	{
		for (int32 Index = ControlTargets.Num() - 1; Index >= 0; --Index)
			if (ControlTargets[Index].bContinuable)
				return Context.Create<asCContinueStmt>(ControlTargets[Index].Target, MoveTemp(Range));
		return ActOnRecoveryStatement(MoveTemp(Range), TEXT("continue-outside-control"));
	}

	void asCSema::ReportBodyWarning(const asCSourceRange& Range, FString Message)
	{
		if (!DiagnosticFragment) return;
		asUINT ID = asDiagnosticIdFromSemanticReason(Message);
		if (ID == 0) ID = static_cast<asUINT>(asEDiagnosticCatalogID::GenericStatement);
		asSDiagnosticGroup Group;
		Group.Primary.DiagnosticID = ID;
		Group.Primary.Severity = asEDiagnosticSeverity::Warning;
		Group.Primary.PrimaryRange = Range;
		Group.Primary.bHasLocation = Range.IsOrdered() || Range.Begin.IsValid();
		Group.Primary.Arguments.Add(asSDiagnosticArgument::FromText(Message));
		DiagnosticFragment->ReportGroup(MoveTemp(Group));
	}

	asCRecoveryExpr* asCSema::ActOnRecoveryExpression(asCSourceRange Range, FString Message)
	{
		if (const asUINT Mapped = asDiagnosticIdFromSyntaxReason(Message))
		{
			ReportSyntax(Range, Message, Mapped);
		}
		else
		{
			ReportBodyError(Range, Message);
		}
		return Context.Create<asCRecoveryExpr>(Context.Create<asCErrorType>(), MoveTemp(Range));
	}

	asCRecoveryStmt* asCSema::ActOnRecoveryStatement(asCSourceRange Range, FString Message)
	{
		if (const asUINT Mapped = asDiagnosticIdFromSyntaxReason(Message))
		{
			ReportSyntax(Range, Message, Mapped);
		}
		else
		{
			ReportBodyError(Range, Message);
		}
		return Context.Create<asCRecoveryStmt>(MoveTemp(Range));
	}

END_AS_NAMESPACE
