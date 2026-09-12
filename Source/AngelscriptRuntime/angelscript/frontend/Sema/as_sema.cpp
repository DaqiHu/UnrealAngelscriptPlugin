#include "frontend/Sema/as_sema.h"

#include "frontend/AST/as_ast_cast.h"
#include "frontend/AST/as_attr.h"
#include "frontend/AST/as_type_context.h"
#include "Containers/StringConv.h"
#include <charconv>
#include <cmath>

BEGIN_AS_NAMESPACE

asCSema::asCSema(asCASTContext& InContext, asCDiagnosticsEngine& InDiagnostics, FString InStableScope)
		: Context(InContext), Diagnostics(InDiagnostics), StableScope(MoveTemp(InStableScope)) {}

	void asCSema::BeginFragment(asCDeclarationFragment& Fragment)
	{
		CurrentFragment = &Fragment; ContextStack.Reset();
		DiagnosticFragment = MakeUnique<asCDiagnosticFragment>(Diagnostics.CreateFragment(Fragment.GetLogicalSourceKey()));
	}

	void asCSema::EndFragment()
	{
		if (DiagnosticFragment) Diagnostics.Submit(MoveTemp(*DiagnosticFragment));
		DiagnosticFragment.Reset(); CurrentFragment = nullptr; ContextStack.Reset();
	}

	bool asCSema::Attach(asCDecl* Decl, TConstArrayView<asCAttr*> Attrs)
	{
		if (!Decl || !CurrentFragment) return false;
		for (asCAttr* Attr : Attrs)
		{
			bool bTargetValid = Attr && Attr->AllowsTarget(Decl->GetKind());
			if (const asCAnnotationAttr* Annotation = dyn_cast<asCAnnotationAttr>(Attr))
			{
				const auto ValidateArguments = [&](auto&& Self, TConstArrayView<asSAnnotationArgument> Arguments) -> bool
				{
					bool bValid = true;
					for (const asSAnnotationArgument& Argument : Arguments)
					{
						if (Argument.Name.Equals(TEXT("BlueprintGetter"), ESearchCase::IgnoreCase)
							|| Argument.Name.Equals(TEXT("BlueprintSetter"), ESearchCase::IgnoreCase))
						{
							bValid = false;
							if (DiagnosticFragment)
							{
								asSDiagnosticRecord Record;
								Record.DiagnosticID = 3002;
								Record.Severity = asEDiagnosticSeverity::Error;
								Record.PrimaryRange = Argument.Range;
								Record.PrimaryRange.End = Record.PrimaryRange.Begin;
								Record.PrimaryRange.End.ByteOffset += static_cast<asUINT>(Argument.Name.Len());
								Record.Arguments.Add(asSDiagnosticArgument::FromText(TEXT("blueprint-accessor-metadata-removed")));
								Record.Arguments.Add(asSDiagnosticArgument::FromText(Argument.Name));
								DiagnosticFragment->Report(MoveTemp(Record));
							}
						}
						if (!Self(Self, Argument.Arguments)) bValid = false;
					}
					return bValid;
				};
				if (!ValidateArguments(ValidateArguments, Annotation->GetArguments()))
				{
					Decl->MarkInvalid();
					CurrentFragment->MarkError();
					continue;
				}
				if (const asCRecordDecl* Record = dyn_cast<asCRecordDecl>(Decl))
				{
					if (Annotation->GetAnnotationKind() == asEAnnotationKind::UClass) bTargetValid &= Record->GetDeclaredTypeKind() == asETypeDeclKind::Class;
					if (Annotation->GetAnnotationKind() == asEAnnotationKind::UStruct) bTargetValid &= Record->GetDeclaredTypeKind() == asETypeDeclKind::Struct;
					if (Annotation->GetAnnotationKind() == asEAnnotationKind::UEnum) bTargetValid &= Record->GetDeclaredTypeKind() == asETypeDeclKind::Enum;
				}
			}
			if (!bTargetValid)
			{
				Decl->MarkInvalid();
				CurrentFragment->MarkError();
				if (DiagnosticFragment)
				{
					asSDiagnosticRecord Record;
					Record.DiagnosticID = 3002;
					Record.Severity = asEDiagnosticSeverity::Error;
					Record.PrimaryRange = Attr ? Attr->GetSourceRange() : Decl->GetSourceRange();
					Record.Arguments.Add(asSDiagnosticArgument::FromText(TEXT("invalid-attribute-target")));
					DiagnosticFragment->Report(MoveTemp(Record));
				}
				continue;
			}
			Decl->AddAttr(Attr);
		}
		if (ContextStack.IsEmpty()) return CurrentFragment->AddTopLevelDecl(Decl);
		return ContextStack.Last()->AddDecl(Decl);
	}

	asCNamespaceDecl* asCSema::ActOnNamespace(FString Name, asCSourceRange Range, TConstArrayView<asCAttr*> Attrs)
	{
		auto* Decl = Context.Create<asCNamespaceDecl>(MoveTemp(Name), MoveTemp(Range)); Attach(Decl, Attrs); return Decl;
	}

	asCTypeAliasDecl* asCSema::ActOnTypeAlias(FString Name, asSTypeSyntax Type, asCSourceRange Range, TConstArrayView<asCAttr*> Attrs)
	{
		auto* Decl = Context.Create<asCTypeAliasDecl>(MoveTemp(Name), MoveTemp(Type), MoveTemp(Range));
		Attach(Decl, Attrs); return Decl;
	}

	asCRecordDecl* asCSema::ActOnRecord(FString Name, asCSourceRange Range, asETypeDeclKind DeclaredTypeKind, TConstArrayView<FString> Bases, TConstArrayView<asCAttr*> Attrs)
	{
		auto* Decl = Context.Create<asCRecordDecl>(MoveTemp(Name), MoveTemp(Range), DeclaredTypeKind);
		for (const FString& Base : Bases) Decl->AddUnresolvedBase(Base);
		// Identity is assigned once the declaration barrier has established its
		// complete lexical namespace and nominal owner.
		Attach(Decl, Attrs); return Decl;
	}

	asCEnumConstantDecl* asCSema::ActOnEnumConstant(
		FString Name,
		int64 Value,
		bool bExplicitValue,
		asCSourceRange Range,
		TConstArrayView<asCAttr*> Attrs)
	{
		auto* Decl = Context.Create<asCEnumConstantDecl>(MoveTemp(Name), Value, bExplicitValue, MoveTemp(Range));
		Attach(Decl, Attrs);
		return Decl;
	}

	asCFunctionDecl* asCSema::ActOnFunction(FString Name, FString ReturnType, asCSourceRange Range, asCSourceRange DeferredBody, TConstArrayView<asCAttr*> Attrs)
	{
		auto* Decl = Context.Create<asCFunctionDecl>(MoveTemp(Name), MoveTemp(Range)); Decl->SetReturnTypeSpelling(MoveTemp(ReturnType));
		if (DeferredBody.IsOrdered()) Decl->SetDeferredBodyRange(MoveTemp(DeferredBody)); Attach(Decl, Attrs); return Decl;
	}

	asCCallableTypeDecl* asCSema::ActOnCallableType(bool bMulticast, FString Name, asSTypeSyntax ReturnType,
		asCSourceRange Range, TConstArrayView<asCAttr*> Attrs)
	{
		asCCallableTypeDecl* Decl = bMulticast
			? static_cast<asCCallableTypeDecl*>(Context.Create<asCEventDecl>(MoveTemp(Name), MoveTemp(Range)))
			: static_cast<asCCallableTypeDecl*>(Context.Create<asCDelegateDecl>(MoveTemp(Name), MoveTemp(Range)));
		Decl->SetReturnTypeSyntax(MoveTemp(ReturnType));
		Attach(Decl, Attrs);
		return Decl;
	}

	asCVarDecl* asCSema::ActOnVariable(FString Name, FString Type, bool bParameter, asCSourceRange Range, TConstArrayView<asCAttr*> Attrs)
	{
		auto* Decl = Context.Create<asCVarDecl>(bParameter ? asEDeclKind::ParmVar : asEDeclKind::Var, MoveTemp(Name), MoveTemp(Range)); Decl->SetTypeSpelling(MoveTemp(Type));
		if (bParameter && !ContextStack.IsEmpty() && ContextStack.Last()->GetOwnerDecl() && ContextStack.Last()->GetOwnerDecl()->GetKind() == asEDeclKind::Function)
		{
			for (asCAttr* Attr : Attrs)
			{
				if (Attr && Attr->AllowsTarget(Decl->GetKind())) Decl->AddAttr(Attr);
				else { Decl->MarkInvalid(); if (CurrentFragment) CurrentFragment->MarkError(); }
			}
			static_cast<asCFunctionDecl*>(ContextStack.Last()->GetOwnerDecl())->AddParameter(Decl);
		}
		else if (bParameter && !ContextStack.IsEmpty() && asCCallableTypeDecl::classof(ContextStack.Last()->GetOwnerDecl()))
		{
			static_cast<asCCallableTypeDecl*>(ContextStack.Last()->GetOwnerDecl())->AddParameter(Decl);
		}
		else
			Attach(Decl, Attrs);
		return Decl;
	}

	asCRecoveryDecl* asCSema::ActOnRecovery(asCSourceRange Range, FString Message, asUINT DiagnosticID)
	{
		auto* Decl = Context.Create<asCRecoveryDecl>(Range); if (CurrentFragment) CurrentFragment->MarkError(); Attach(Decl, {});
		if (DiagnosticFragment && DiagnosticID != 0) { asSDiagnosticRecord Record; Record.DiagnosticID = DiagnosticID; Record.Severity = asEDiagnosticSeverity::Error; Record.PrimaryRange = Range; Record.Arguments.Add(asSDiagnosticArgument::FromText(Message)); DiagnosticFragment->Report(MoveTemp(Record)); }
		return Decl;
	}

	void asCSema::DiagnoseRemovedFeature(asERemovedLanguageFeature Feature, asCSourceRange Range)
	{
		const TCHAR* Name = asRemovedLanguageFeatureName(Feature);
		if (CurrentFragment) CurrentFragment->MarkError();
		if (CurrentBody) CurrentBody->MarkError(Name);
		if (CurrentInitializer) CurrentInitializer->MarkInvalid();
		bExpressionErrors = true;
		if (DiagnosticFragment)
		{
			asSDiagnosticRecord Record;
			Record.DiagnosticID = asRemovedLanguageFeatureDiagnostic;
			Record.Severity = asEDiagnosticSeverity::Error;
			Record.PrimaryRange = Range;
			Record.Arguments.Add(asSDiagnosticArgument::FromText(Name));
			DiagnosticFragment->Report(MoveTemp(Record));
		}
	}

	asCAnnotationAttr* asCSema::CreateAnnotation(asEAnnotationKind Kind, TArray<asSAnnotationArgument> Arguments, asCSourceRange Range)
	{
		asEAttrTarget Target = asEAttrTarget::Record;
		if (Kind == asEAnnotationKind::UFunction) Target = asEAttrTarget::Function;
		else if (Kind == asEAnnotationKind::UProperty) Target = asEAttrTarget::Variable;
		else if (Kind == asEAnnotationKind::UMeta) Target = asEAttrTarget::EnumConstant;
		return Context.Create<asCAnnotationAttr>(Kind, MoveTemp(Arguments), MoveTemp(Range), Target);
	}

	asCFlagAttr* asCSema::CreateAnnotation(FString Name, asCSourceRange Range)
	{
		asEAttrTarget Target = asEAttrTarget::Any;
		if (Name.Equals(TEXT("UFUNCTION"), ESearchCase::IgnoreCase)) Target = asEAttrTarget::Function;
		else if (Name.Equals(TEXT("UPROPERTY"), ESearchCase::IgnoreCase)) Target = asEAttrTarget::Variable;
		else if (Name.Equals(TEXT("UCLASS"), ESearchCase::IgnoreCase) || Name.Equals(TEXT("USTRUCT"), ESearchCase::IgnoreCase) || Name.Equals(TEXT("UENUM"), ESearchCase::IgnoreCase)) Target = asEAttrTarget::Record;
		return Context.Create<asCFlagAttr>(MoveTemp(Name), MoveTemp(Range), Target);
	}

	asCStringAttr* asCSema::CreateStringAnnotation(FString Name, FString Value, asCSourceRange Range)
	{
		return Context.Create<asCStringAttr>(MoveTemp(Name), MoveTemp(Value), MoveTemp(Range), asEAttrTarget::Any);
	}

	void asCSema::BeginBody(
		asCBodyFragment& Fragment,
		asFBodyNameLookup InLookup,
		asFBodyTypeLookup InTypeLookup,
		asFStructuredTypeLookup InStructuredLookup,
		asFNominalDeclLookup InNominalLookup, asFBodyInputLookup InBodyInputLookup, asFListInitializationLookup InListLookup,
		asFExternalCallableLookup InExternalLookup)
	{
		CurrentBody = &Fragment;
		BodyLookup = MoveTemp(InLookup);
		BodyTypeLookup = MoveTemp(InTypeLookup);
		StructuredTypeLookup = MoveTemp(InStructuredLookup);
		NominalDeclLookup = MoveTemp(InNominalLookup);
		BodyInputLookup = MoveTemp(InBodyInputLookup);
		ListInitializationLookup = MoveTemp(InListLookup);
		ExternalCallableLookup = MoveTemp(InExternalLookup);
		CurrentInitializer = nullptr;
		bExpressionErrors = false;
		BodyScopes.Reset();
		ControlTargets.Reset();
		NextLocalOrdinal = 0;
		DiagnosticFragment = MakeUnique<asCDiagnosticFragment>(
			Diagnostics.CreateFragment(Fragment.GetWorkItem().StableFunctionKey.ToHexString()));
	}

	void asCSema::EndBody()
	{
		if (DiagnosticFragment) Diagnostics.Submit(MoveTemp(*DiagnosticFragment));
		DiagnosticFragment.Reset();
		BodyLookup = {};
		BodyTypeLookup = {};
		StructuredTypeLookup = {};
		NominalDeclLookup = {};
		BodyInputLookup = {};
		ExternalCallableLookup = {};
		BodyScopes.Reset();
		ControlTargets.Reset();
		CurrentBody = nullptr;
	}


	void asCSema::BeginInitializer(asCVarDecl& Declaration, asFBodyNameLookup InLookup, asFBodyTypeLookup InTypeLookup,
		asFStructuredTypeLookup InStructuredLookup, asFNominalDeclLookup InNominalLookup, asFBodyInputLookup InBodyInputLookup, asFListInitializationLookup InListLookup,
		asFExternalCallableLookup InExternalLookup)
	{
		BeginInitializerContext(Declaration, MoveTemp(InLookup), MoveTemp(InTypeLookup), MoveTemp(InStructuredLookup), MoveTemp(InNominalLookup), MoveTemp(InBodyInputLookup), MoveTemp(InListLookup), MoveTemp(InExternalLookup));
	}

	void asCSema::BeginConstantInitializer(asCEnumConstantDecl& Declaration, asFBodyNameLookup InLookup, asFBodyTypeLookup InTypeLookup,
		asFStructuredTypeLookup InStructuredLookup, asFNominalDeclLookup InNominalLookup)
	{
		BeginInitializerContext(Declaration, MoveTemp(InLookup), MoveTemp(InTypeLookup), MoveTemp(InStructuredLookup), MoveTemp(InNominalLookup));
	}

	void asCSema::BeginInitializerContext(asCNamedDecl& Declaration, asFBodyNameLookup InLookup, asFBodyTypeLookup InTypeLookup,
		asFStructuredTypeLookup InStructuredLookup, asFNominalDeclLookup InNominalLookup, asFBodyInputLookup InBodyInputLookup, asFListInitializationLookup InListLookup,
		asFExternalCallableLookup InExternalLookup)
	{
		CurrentBody = nullptr;
		CurrentInitializer = &Declaration;
		ListInitializationLookup = MoveTemp(InListLookup);
		BodyInputLookup = MoveTemp(InBodyInputLookup);
		ExternalCallableLookup = MoveTemp(InExternalLookup);
		bExpressionErrors = false;
		BodyLookup = MoveTemp(InLookup);
		BodyTypeLookup = MoveTemp(InTypeLookup);
		StructuredTypeLookup = MoveTemp(InStructuredLookup);
		NominalDeclLookup = MoveTemp(InNominalLookup);
		BodyScopes.Reset();
		ControlTargets.Reset();
		DiagnosticFragment = MakeUnique<asCDiagnosticFragment>(Diagnostics.CreateFragment(Declaration.GetSemanticKey()));
	}

	bool asCSema::EndInitializer()
	{
		const bool bSucceeded = !bExpressionErrors;
		if (DiagnosticFragment) Diagnostics.Submit(MoveTemp(*DiagnosticFragment));
		DiagnosticFragment.Reset();
		CurrentInitializer = nullptr;
		BodyLookup = {}; BodyTypeLookup = {}; StructuredTypeLookup = {}; NominalDeclLookup = {};
		BodyInputLookup = {};
		ExternalCallableLookup = {};
		return bSucceeded;
	}

	bool asCSema::ActOnInitializer(asCVarDecl& Declaration, asCExpr* Value)
	{
		if (Declaration.GetTypeSyntax().NameParts.Num() == 1 && Declaration.GetTypeSyntax().NameParts[0] == TEXT("auto"))
		{
			const asCQualType Inferred = ResolveAutoVariableType(Declaration.GetTypeSyntax(), Value);
			if (!Inferred.IsValid() || !Declaration.SetResolvedQualType(Inferred))
			{ ReportBodyError(Declaration.GetSourceRange(), TEXT("invalid-auto-initializer")); return false; }
		}
		asCExpr* Converted = ConvertTo(Value, Declaration.GetResolvedQualType());
		if (!Converted)
		{
			ReportBodyError(Declaration.GetDeferredInitializerRange(), TEXT("incompatible-declaration-initializer"));
			return false;
		}
		return Declaration.SetInitializer(Converted) && !bExpressionErrors;
	}

	asCQualType asCSema::ResolveBodyType(const asSTypeSyntax& Syntax) const
	{
		if (StructuredTypeLookup) return StructuredTypeLookup(Syntax);
		asCQualType Result;
		if (!BodyTypeLookup || !Syntax.GenericArguments.IsEmpty() || !Syntax.ArrayDimensions.IsEmpty()) return Result;
		asCType* Type = BodyTypeLookup((Syntax.bGlobal ? TEXT("::") : TEXT("")) + Syntax.GetQualifiedName());
		asETypeQualifiers Qualifiers = Syntax.bConst ? asETypeQualifiers::Const : asETypeQualifiers::None;
		if (Syntax.bHandle) Qualifiers = Qualifiers | asETypeQualifiers::Handle | asETypeQualifiers::Nullable;
		if (Syntax.Reference != asETypeReferenceDirection::None) Qualifiers = Qualifiers | asETypeQualifiers::Reference;
		asCQualType::TryCreate(Type, Qualifiers, Result);
		return Result;
	}

	asCRecordDecl* asCSema::GetCurrentRecord() const
	{
		for (int32 Index = ContextStack.Num() - 1; Index >= 0; --Index)
			if (asCRecordDecl* Record = dyn_cast<asCRecordDecl>(ContextStack[Index]->GetOwnerDecl())) return Record;
		if (asCFunctionDecl* Function = GetCurrentFunction())
		{
			if (Function->GetDeclContext())
				if (asCRecordDecl* Record = dyn_cast<asCRecordDecl>(Function->GetDeclContext()->GetOwnerDecl())) return Record;
		}
		if (CurrentInitializer && CurrentInitializer->GetDeclContext())
			return dyn_cast<asCRecordDecl>(CurrentInitializer->GetDeclContext()->GetOwnerDecl());
		return nullptr;
	}

	asCFunctionDecl* asCSema::GetCurrentFunction() const
	{
		if (CurrentBody) return CurrentBody->GetWorkItem().Function;
		if (CurrentInitializer && CurrentInitializer->GetDeclContext())
			return dyn_cast<asCFunctionDecl>(CurrentInitializer->GetDeclContext()->GetOwnerDecl());
		return nullptr;
	}

	void asCSema::ReportBodyError(const asCSourceRange& Range, FString Message)
	{
		bExpressionErrors = true;
		if (CurrentInitializer) CurrentInitializer->MarkInvalid();
		if (CurrentBody) CurrentBody->MarkError(Message);
		if (DiagnosticFragment)
		{
			asSDiagnosticRecord Record;
			Record.DiagnosticID = 4001;
			Record.Severity = asEDiagnosticSeverity::Error;
			Record.PrimaryRange = Range;
			Record.Arguments.Add(asSDiagnosticArgument::FromText(Message));
			DiagnosticFragment->Report(MoveTemp(Record));
		}
	}

	asCIntegerLiteral* asCSema::ActOnIntegerLiteral(int64 Value, asCSourceRange Range)
	{
		asCIntegerLiteral* Literal = Context.Create<asCIntegerLiteral>(Value, MoveTemp(Range));
		Literal->SetExpressionSemantics(Context.GetBuiltinType(asEPrimitiveType::Int32), asEValueCategory::PRValue);
		return Literal;
	}

	asCExpr* asCSema::ActOnDeclReference(FString Name, asCSourceRange Range)
	{
		asCDecl* Referenced = nullptr;
		for (int32 ScopeIndex = BodyScopes.Num() - 1; !Referenced && ScopeIndex >= 0; --ScopeIndex)
			Referenced = BodyScopes[ScopeIndex].FindRef(Name);
		if (!Referenced && CurrentBody && CurrentBody->GetWorkItem().Function)
		{
			for (asCVarDecl* Parameter : CurrentBody->GetWorkItem().Function->GetParameters())
			{
				if (Parameter->GetName() == Name)
				{
					Referenced = Parameter;
					break;
				}
			}
		}
		if (!Referenced && BodyLookup)
		{
			const TConstArrayView<asCDecl*> Found = BodyLookup(Name);
			if (Found.Num() == 1) Referenced = Found[0];
		}
		if (!Referenced && !Name.Contains(TEXT("::")))
		{
			TArray<asCDecl*> Members;
			LookupMembers(GetCurrentRecord(), Name, Members);
			if (Members.Num() == 1) Referenced = Members[0];
		}
		if (!Referenced)
		{
			ReportBodyError(Range, FString::Printf(TEXT("unresolved-reference:%s"), *Name));
			return Context.Create<asCRecoveryExpr>(Context.Create<asCErrorType>(), MoveTemp(Range));
		}

		if (Referenced->GetDeclContext() && dyn_cast<asCRecordDecl>(Referenced->GetDeclContext()->GetOwnerDecl())
			&& !isa<asCEnumConstantDecl>(Referenced))
			return ActOnMember(ActOnThis(true, Range), Name, Range, Range);
		asCDeclRefExpr* Reference = Context.Create<asCDeclRefExpr>(Referenced, MoveTemp(Range));
		asCType* Type = nullptr;
		asEValueCategory Category = asEValueCategory::LValue;
		if (asCVarDecl* Variable = dyn_cast<asCVarDecl>(Referenced))
		{
			Type = Variable->GetResolvedType();
			Reference->SetExpressionQualType(Variable->GetResolvedQualType(), Category);
			return Reference;
		}
		else if (asCFunctionDecl* Function = dyn_cast<asCFunctionDecl>(Referenced))
		{
			Type = Function->GetResolvedSignature();
			Category = asEValueCategory::PRValue;
		}
		else if (asCEnumConstantDecl* Constant = dyn_cast<asCEnumConstantDecl>(Referenced))
		{
			asCRecordDecl* Owner = Constant->GetDeclContext() ? dyn_cast<asCRecordDecl>(Constant->GetDeclContext()->GetOwnerDecl()) : nullptr;
			asSStableTypeUseDescriptor Use;
			Use.Kind = asETypeUseKind::Nominal;
			Use.Declaration = Owner ? Owner->GetStableIdentity() : asSStableKey{};
			asSStableKey Key;
			if (Owner && Owner->GetDeclaredTypeKind() == asETypeDeclKind::Enum
				&& Context.GetTypeContext()->GetIdentityRegistry().InternTypeUse(Use, Key) == asETypeIdentityStatus::Succeeded)
				Type = Context.GetNominalType(Use.Declaration, Key);
			Category = asEValueCategory::PRValue;
		}
		if (!Type) Type = Context.Create<asCErrorType>();
		Reference->SetExpressionSemantics(Type, Category);
		return Reference;
	}

	namespace
	{
		bool SameBodyType(const asCType* Left, const asCType* Right)
		{
			if (Left == Right) return true;
			if (!Left || !Right || Left->GetKind() != Right->GetKind()) return false;
			if (const asCBuiltinType* LeftBuiltin = dyn_cast<asCBuiltinType>(Left))
			{
				const asCBuiltinType* RightBuiltin = dyn_cast<asCBuiltinType>(Right);
				return RightBuiltin && LeftBuiltin->GetPrimitive() == RightBuiltin->GetPrimitive();
			}
			return Left->HasStableIdentity() && Right->HasStableIdentity() &&
				Left->GetStableIdentity() == Right->GetStableIdentity();
		}

		bool IsEnumBodyType(const asCType* Type)
		{
			const asCNominalType* Nominal = dyn_cast<asCNominalType>(Type);
			asSNominalTypeDescriptor Descriptor;
			return Nominal && Nominal->GetOwnerContext()
				&& Nominal->GetOwnerContext()->GetTypeContext()->GetIdentityRegistry().ResolveNominalType(Nominal->GetDeclarationIdentity(), Descriptor) == asETypeIdentityStatus::Succeeded
				&& Descriptor.Kind == asETypeDeclKind::Enum;
		}

		bool IsNumericBodyType(const asCType* Type)
		{
			const asCBuiltinType* Builtin = dyn_cast<asCBuiltinType>(Type);
			return IsEnumBodyType(Type) || (Builtin && Builtin->GetPrimitive() != asEPrimitiveType::Void && Builtin->GetPrimitive() != asEPrimitiveType::Bool);
		}

		bool IsBoolBodyType(const asCType* Type)
		{
			const asCBuiltinType* Builtin = dyn_cast<asCBuiltinType>(Type);
			return Builtin && Builtin->GetPrimitive() == asEPrimitiveType::Bool;
		}

		bool IsIntegralBodyType(const asCType* Type)
		{
			const asCBuiltinType* Builtin = dyn_cast<asCBuiltinType>(Type);
			return IsEnumBodyType(Type) || (IsNumericBodyType(Type) && Builtin->GetPrimitive() != asEPrimitiveType::Float32
				&& Builtin->GetPrimitive() != asEPrimitiveType::Float64);
		}

		asEPrimitiveType CommonNumericPrimitive(const asCType* Left, const asCType* Right)
		{
			const asEPrimitiveType A = IsEnumBodyType(Left) ? asEPrimitiveType::Int32 : cast<asCBuiltinType>(Left)->GetPrimitive();
			const asEPrimitiveType B = IsEnumBodyType(Right) ? asEPrimitiveType::Int32 : cast<asCBuiltinType>(Right)->GetPrimitive();
			for (asEPrimitiveType Wide : {asEPrimitiveType::Float64, asEPrimitiveType::Float32,
				asEPrimitiveType::UInt64, asEPrimitiveType::Int64, asEPrimitiveType::UInt32})
				if (A == Wide || B == Wide) return Wide;
			return asEPrimitiveType::Int32;
		}

		bool IsAssignment(asEBinaryOperator Operator)
		{
			return Operator >= asEBinaryOperator::Assign;
		}

		bool RequiresIntegral(asEBinaryOperator Operator)
		{
			switch (Operator)
			{
			case asEBinaryOperator::ShiftLeft: case asEBinaryOperator::ShiftRight: case asEBinaryOperator::ShiftRightArithmetic:
			case asEBinaryOperator::BitwiseAnd: case asEBinaryOperator::BitwiseXor: case asEBinaryOperator::BitwiseOr:
			case asEBinaryOperator::ShiftLeftAssign: case asEBinaryOperator::ShiftRightAssign: case asEBinaryOperator::ShiftRightArithmeticAssign:
			case asEBinaryOperator::BitwiseAndAssign: case asEBinaryOperator::BitwiseXorAssign: case asEBinaryOperator::BitwiseOrAssign:
				return true;
			default: return false;
			}
		}
	}

	asCExpr* asCSema::ActOnNumericLiteral(FString Spelling, asCSourceRange Range)
	{
		// A pp-number is deliberately more permissive than a language literal.
		// Like Clang's NumericLiteralParser, validate the entire token, then
		// select a representable type; never silently truncate a valid prefix.
		auto Invalid = [&]() -> asCExpr*
		{
			return ActOnRecoveryExpression(Range, FString::Printf(TEXT("invalid-numeric-literal:%s"), *Spelling));
		};
		int32 Radix = 10;
		int32 Index = 0;
		bool bBased = false;
		if (Spelling.Len() >= 2 && Spelling[0] == TEXT('0'))
		{
			switch (Spelling[1])
			{
			case TEXT('x'): case TEXT('X'): Radix = 16; bBased = true; break;
			case TEXT('b'): case TEXT('B'): Radix = 2; bBased = true; break;
			case TEXT('o'): case TEXT('O'): Radix = 8; bBased = true; break;
			case TEXT('d'): case TEXT('D'): Radix = 10; bBased = true; break;
			default: break;
			}
			if (bBased) Index = 2;
		}
		const bool bFloating = !bBased && (Spelling.Contains(TEXT(".")) || Spelling.Contains(TEXT("e")) || Spelling.Contains(TEXT("E")));
		if (bFloating)
		{
			bool bFloat32 = Spelling.EndsWith(TEXT("f"), ESearchCase::IgnoreCase);
			const int32 End = Spelling.Len() - (bFloat32 ? 1 : 0);
			int32 Digits = 0;
			while (Index < End && Spelling[Index] >= TEXT('0') && Spelling[Index] <= TEXT('9')) { ++Digits; ++Index; }
			if (Index < End && Spelling[Index] == TEXT('.'))
			{
				++Index;
				while (Index < End && Spelling[Index] >= TEXT('0') && Spelling[Index] <= TEXT('9')) { ++Digits; ++Index; }
			}
			if (Digits == 0) return Invalid();
			if (Index < End && (Spelling[Index] == TEXT('e') || Spelling[Index] == TEXT('E')))
			{
				++Index;
				if (Index < End && (Spelling[Index] == TEXT('+') || Spelling[Index] == TEXT('-'))) ++Index;
				const int32 ExponentBegin = Index;
				while (Index < End && Spelling[Index] >= TEXT('0') && Spelling[Index] <= TEXT('9')) ++Index;
				if (Index == ExponentBegin) return Invalid();
			}
			if (Index != End) return Invalid();
			FTCHARToUTF8 Bytes(*Spelling, End);
			double Value = 0.0;
			const std::from_chars_result Parsed = std::from_chars(Bytes.Get(), Bytes.Get() + Bytes.Length(), Value);
			if (Parsed.ec != std::errc() || Parsed.ptr != Bytes.Get() + Bytes.Length() || !std::isfinite(Value)) return Invalid();
			if (bFloat32)
			{
				const float Narrow = static_cast<float>(Value);
				if (!std::isfinite(Narrow)) return Invalid();
				Value = static_cast<double>(Narrow);
			}
			asCFloatingLiteral* Literal = Context.Create<asCFloatingLiteral>(Value, Range);
			Literal->SetExpressionSemantics(Context.GetBuiltinType(bFloat32 ? asEPrimitiveType::Float32 : asEPrimitiveType::Float64), asEValueCategory::PRValue);
			return Literal;
		}
		if (Index >= Spelling.Len()) return Invalid();
		uint64 Value = 0;
		for (; Index < Spelling.Len(); ++Index)
		{
			const TCHAR Character = Spelling[Index];
			const int32 Digit = Character >= TEXT('0') && Character <= TEXT('9') ? Character - TEXT('0')
				: Character >= TEXT('a') && Character <= TEXT('f') ? Character - TEXT('a') + 10
				: Character >= TEXT('A') && Character <= TEXT('F') ? Character - TEXT('A') + 10 : -1;
			if (Digit < 0 || Digit >= Radix || Value > (MAX_uint64 - static_cast<uint64>(Digit)) / static_cast<uint64>(Radix)) return Invalid();
			Value = Value * static_cast<uint64>(Radix) + static_cast<uint64>(Digit);
		}
		const asEPrimitiveType Primitive = bBased
			? (Value <= MAX_uint32 ? asEPrimitiveType::UInt32 : asEPrimitiveType::UInt64)
			: Value <= MAX_int32 ? asEPrimitiveType::Int32
			: Value <= static_cast<uint64>(MAX_int64) ? asEPrimitiveType::Int64 : asEPrimitiveType::UInt64;
		asCIntegerLiteral* Literal = Context.Create<asCIntegerLiteral>(static_cast<int64>(Value), Range);
		Literal->SetExpressionSemantics(Context.GetBuiltinType(Primitive), asEValueCategory::PRValue);
		return Literal;
	}

	asCBooleanLiteral* asCSema::ActOnBooleanLiteral(bool Value, asCSourceRange Range)
	{
		asCBooleanLiteral* Literal = Context.Create<asCBooleanLiteral>(Value, MoveTemp(Range));
		Literal->SetExpressionSemantics(Context.GetBuiltinType(asEPrimitiveType::Bool), asEValueCategory::PRValue);
		return Literal;
	}

	asCExpr* asCSema::ConvertTo(asCExpr* Value, asCType* Type)
	{
		if (!Value || !Type) return nullptr;
		if (SameBodyType(Value->GetExpressionType(), Type)) return Value;
		asCQualType Target;
		asCQualType::TryCreate(Type, asETypeQualifiers::None, Target);
		if (asCExpr* UserConversion = ConvertUserDefined(Value, Target, false)) return UserConversion;
		if (IsEnumBodyType(Type)) return nullptr; // numeric-to-enum requires an explicit cast
		if (!IsNumericBodyType(Value->GetExpressionType()) || !IsNumericBodyType(Type)) return nullptr;
		asCImplicitCastExpr* Conversion = Context.Create<asCImplicitCastExpr>(Value, asEImplicitConversionKind::Numeric, Value->GetSourceRange());
		Conversion->SetExpressionSemantics(Type, asEValueCategory::PRValue);
		return Conversion;
	}

	asCExpr* asCSema::ActOnParen(asCExpr* SubExpr, asCSourceRange Range)
	{
		if (!SubExpr) return ActOnRecoveryExpression(Range, TEXT("missing-parenthesized-expression"));
		asCParenExpr* Result = Context.Create<asCParenExpr>(SubExpr, Range);
		Result->SetExpressionQualType(SubExpr->GetExpressionQualType(), SubExpr->GetValueCategory());
		Result->SetReadOnly(SubExpr->IsReadOnly());
		return Result;
	}

	asCExpr* asCSema::ActOnUnary(asEUnaryOperator Operator, asCExpr* Operand, asCSourceRange OperatorRange, asCSourceRange Range)
	{
		if (!Operand) return ActOnRecoveryExpression(Range, TEXT("missing-unary-operand"));
		if (Operator == asEUnaryOperator::LogicalNot && !CheckCondition(Operand))
			return ActOnRecoveryExpression(Range, TEXT("invalid-unary-operand-type"));
		asCType* Type = Operand->GetExpressionType();
		const bool bIncrement = Operator >= asEUnaryOperator::PreIncrement;
		if (Operator == asEUnaryOperator::LogicalNot ? !IsBoolBodyType(Type)
			: Operator == asEUnaryOperator::BitwiseNot ? !IsIntegralBodyType(Type) : !IsNumericBodyType(Type))
			return ActOnRecoveryExpression(Range, TEXT("invalid-unary-operand-type"));
		if (bIncrement && (Operand->GetValueCategory() != asEValueCategory::LValue || Operand->IsReadOnly()))
			return ActOnRecoveryExpression(Range, TEXT("increment-requires-modifiable-lvalue"));
		if (!bIncrement && IsIntegralBodyType(Type))
		{
			Type = Context.GetBuiltinType(CommonNumericPrimitive(Type, Context.GetBuiltinType(asEPrimitiveType::Int32)));
			Operand = ConvertTo(Operand, Type);
		}
		asCUnaryOperator* Result = Context.Create<asCUnaryOperator>(Operator, Operand, OperatorRange, Range);
		const bool bPreIncrement = Operator == asEUnaryOperator::PreIncrement || Operator == asEUnaryOperator::PreDecrement;
		Result->SetExpressionSemantics(Type, bPreIncrement ? asEValueCategory::LValue : asEValueCategory::PRValue);
		return Result;
	}

	asCExpr* asCSema::ActOnBinary(asEBinaryOperator Operator, asCExpr* Left, asCExpr* Right, asCSourceRange OperatorRange, asCSourceRange Range)
	{
		if (!Left || !Right) return ActOnRecoveryExpression(Range, TEXT("missing-binary-operand"));
		asCType* LeftType = Left->GetExpressionType();
		asCType* RightType = Right->GetExpressionType();
		const bool bAssignment = IsAssignment(Operator);
		const bool bLogical = Operator == asEBinaryOperator::LogicalAnd || Operator == asEBinaryOperator::LogicalOr || Operator == asEBinaryOperator::LogicalXor;
		const bool bEquality = Operator == asEBinaryOperator::Equal || Operator == asEBinaryOperator::NotEqual;
		const bool bComparison = bEquality || Operator == asEBinaryOperator::Less || Operator == asEBinaryOperator::Greater
			|| Operator == asEBinaryOperator::LessEqual || Operator == asEBinaryOperator::GreaterEqual;
		if (bAssignment && (Left->GetValueCategory() != asEValueCategory::LValue || Left->IsReadOnly()))
			return ActOnRecoveryExpression(Range, TEXT("assignment-requires-modifiable-lvalue"));
		asCType* ResultType = LeftType;
		if (Operator == asEBinaryOperator::Assign)
		{
			Right = ConvertTo(Right, Left->GetExpressionQualType());
			if (!Right) return ActOnRecoveryExpression(Range, TEXT("incompatible-assignment"));
		}
		else if (bLogical || (bEquality && IsBoolBodyType(LeftType) && IsBoolBodyType(RightType)))
		{
			if (!CheckCondition(Left) || !CheckCondition(Right))
				return ActOnRecoveryExpression(Range, TEXT("logical-operator-requires-bool"));
			ResultType = Context.GetBuiltinType(asEPrimitiveType::Bool);
		}
		else
		{
			if (!IsNumericBodyType(LeftType) || !IsNumericBodyType(RightType)
				|| (RequiresIntegral(Operator) && (!IsIntegralBodyType(LeftType) || !IsIntegralBodyType(RightType))))
				return ActOnRecoveryExpression(Range, TEXT("invalid-binary-operand-types"));
			asCType* Common = Context.GetBuiltinType(CommonNumericPrimitive(LeftType, RightType));
			if (!bAssignment) Left = ConvertTo(Left, Common);
			Right = ConvertTo(Right, bAssignment ? LeftType : Common);
			ResultType = bAssignment ? LeftType : bComparison ? Context.GetBuiltinType(asEPrimitiveType::Bool) : Common;
		}
		asCBinaryOperator* Result = Context.Create<asCBinaryOperator>(Operator, Left, Right, OperatorRange, Range);
		Result->SetExpressionSemantics(ResultType, bAssignment ? asEValueCategory::LValue : asEValueCategory::PRValue);
		return Result;
	}

	bool asCSema::CheckCondition(asCExpr*& Condition)
	{
		if (Condition && IsBoolBodyType(Condition->GetExpressionType())) return true;
		asCQualType BoolType;
		asCQualType::TryCreate(Context.GetBuiltinType(asEPrimitiveType::Bool), asETypeQualifiers::None, BoolType);
		if (asCExpr* Converted = ConvertUserDefined(Condition, BoolType, false)) { Condition = Converted; return true; }
		ReportBodyError(Condition ? Condition->GetSourceRange() : asCSourceRange{}, TEXT("condition-requires-bool"));
		return false;
	}

	asCExpr* asCSema::ActOnConditional(asCExpr* Condition, asCExpr* TrueExpr, asCExpr* FalseExpr, asCSourceRange Range)
	{
		if (!CheckCondition(Condition) || !TrueExpr || !FalseExpr)
			return ActOnRecoveryExpression(Range, TEXT("invalid-conditional-operands"));
		asCQualType TrueQual = TrueExpr->GetExpressionQualType();
		asCQualType FalseQual = FalseExpr->GetExpressionQualType();
		asCType* ResultType = TrueExpr->GetExpressionType();
		if (!SameBodyType(ResultType, FalseExpr->GetExpressionType()) || TrueQual.IsHandle() != FalseQual.IsHandle())
		{
			if (!IsNumericBodyType(ResultType) || !IsNumericBodyType(FalseExpr->GetExpressionType())
				|| TrueQual.IsHandle() || FalseQual.IsHandle())
				return ActOnRecoveryExpression(Range, TEXT("incompatible-conditional-branches"));
			ResultType = Context.GetBuiltinType(CommonNumericPrimitive(ResultType, FalseExpr->GetExpressionType()));
			TrueExpr = ConvertTo(TrueExpr, ResultType);
			FalseExpr = ConvertTo(FalseExpr, ResultType);
			if (!TrueExpr || !FalseExpr) return ActOnRecoveryExpression(Range, TEXT("incompatible-conditional-branches"));
			TrueQual = TrueExpr->GetExpressionQualType();
			FalseQual = FalseExpr->GetExpressionQualType();
		}
		uint8 Qualifiers = static_cast<uint8>(TrueQual.GetQualifiers()) | static_cast<uint8>(FalseQual.GetQualifiers());
		asCQualType ResultQual;
		if (!asCQualType::TryCreateRaw(ResultType, Qualifiers, ResultQual))
			return ActOnRecoveryExpression(Range, TEXT("incompatible-conditional-branches"));
		asCConditionalOperator* Result = Context.Create<asCConditionalOperator>(Condition, TrueExpr, FalseExpr, Range);
		const bool bLValue = TrueExpr->GetValueCategory() == asEValueCategory::LValue && FalseExpr->GetValueCategory() == asEValueCategory::LValue;
		Result->SetExpressionQualType(ResultQual, bLValue ? asEValueCategory::LValue : asEValueCategory::PRValue);
		return Result;
	}


	asCExpr* asCSema::ActOnCall(FString Name, TArray<asCExpr*> Arguments, asCSourceRange Range)
	{
		TArray<asSParsedCallArgument> Parsed;
		for (asCExpr* Value : Arguments) Parsed.Add({Value, {}, {}});
		return ActOnCallWithArguments(MoveTemp(Name), MoveTemp(Parsed), MoveTemp(Range));
	}

END_AS_NAMESPACE
