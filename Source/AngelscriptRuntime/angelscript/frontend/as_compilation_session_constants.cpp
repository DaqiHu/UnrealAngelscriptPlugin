#include "frontend/as_compilation_session.h"
#include "frontend/as_constant_evaluator.h"
#include "frontend/as_ast_cast.h"
#include "frontend/as_ast_visitor.h"
#include "Containers/Set.h"

BEGIN_AS_NAMESPACE
bool asCCompilationSession::ResolveConstantDeclarations()
	{
		TArray<asCEnumConstantDecl*> Constants;
		for (asCASTNode* Node : Context.GetNodes())
			if (auto* Constant = dyn_cast<asCEnumConstantDecl>(Node)) Constants.Add(Constant);
		if (Constants.IsEmpty()) return true;
		bool Valid = true;
		auto Error = [&](asCEnumConstantDecl& Constant, const TCHAR* Message, const asCExpr* Failure = nullptr)
		{
			Valid = false; Constant.MarkInvalid();
			StableDiagnostics.Add(Constant.GetSemanticKey() + TEXT(":") + Message);
			auto Fragment = Diagnostics.CreateFragment(Constant.GetSemanticKey());
			asSDiagnosticRecord Record;
			Record.DiagnosticID = 4101; Record.Severity = asEDiagnosticSeverity::Error;
			Record.PrimaryRange = Failure ? Failure->GetSourceRange() : Constant.GetSourceRange();
			Record.Arguments.Add(asSDiagnosticArgument::FromText(Message)); Fragment.Report(MoveTemp(Record)); Diagnostics.Submit(MoveTemp(Fragment));
		};
		auto LookupTypeName = [&](const FString& Name, const asCDecl& Scope)
		{
			asSTypeSyntax Syntax; Syntax.NameParts.Add(Name); Syntax.Range = Scope.GetSourceRange();
			return ResolveTypeSyntax(Syntax, Scope).GetType();
		};
		for (asCEnumConstantDecl* Constant : Constants)
		{
			if (!Constant->HasDeferredInitializer() || Constant->GetInitializer() || Constant->IsInvalid()) continue;
			asCSema ConstantSema(Context, Diagnostics, StableScope);
			ConstantSema.BeginConstantInitializer(*Constant,
				[this, Constant](const FString& Name) { return LookupInBodyScope(Name, *Constant); },
				[&LookupTypeName, Constant](const FString& Name) { return LookupTypeName(Name, *Constant); },
				[this, Constant](const asSTypeSyntax& Syntax) { return ResolveTypeSyntax(Syntax, *Constant); },
				[this](const asSStableKey& Key) { return FindNominalDeclaration(Key); });
			asCParser Parser(ConstantSema);
			auto* Initializer = Parser.ParseConstantExpression(GetRetainedBodyInput(Constant->GetDeferredInitializerRange()), Constant->GetDeferredInitializerRange());
			const bool Analyzed = ConstantSema.EndInitializer();
			if (!Initializer || !Analyzed || !Constant->SetInitializer(Initializer)) Error(*Constant, TEXT("enum-initializer-analysis-failed"));
		}
		// A global const used by an enum is needed during declaration semantics.
		// Parse only that dependency closure; ordinary defaults remain deferred
		// until AnalyzeBodies, and no original source is parsed a second time.
		TSet<asCVarDecl*> PreparedVariables;
		TFunction<bool(asCExpr*, int32)> PrepareReferences = [&](asCExpr* Expression, int32 Depth)
		{
			if (!Expression || Depth > 256) return false;
			bool Prepared = true;
			asCASTVisitor::Walk(Expression, [&](const asCASTNode& Node)
			{
				const auto* Reference = dyn_cast<asCDeclRefExpr>(&Node);
				auto* Variable = Reference ? dyn_cast<asCVarDecl>(Reference->GetDecl()) : nullptr;
				if (!Variable || !Variable->GetResolvedQualType().IsConst() || PreparedVariables.Contains(Variable)) return;
				PreparedVariables.Add(Variable);
				if (!Variable->GetInitializer() && Variable->HasDeferredInitializer())
				{
					asCSema DependencySema(Context, Diagnostics, StableScope);
					DependencySema.BeginInitializer(*Variable,
						[this, Variable](const FString& Name) { return LookupInBodyScope(Name, *Variable); },
						[&LookupTypeName, Variable](const FString& Name) { return LookupTypeName(Name, *Variable); },
						[this, Variable](const asSTypeSyntax& Syntax) { return ResolveTypeSyntax(Syntax, *Variable); },
						[this](const asSStableKey& Key) { return FindNominalDeclaration(Key); });
					asCParser Parser(DependencySema);
					const bool Parsed = Parser.ParseInitializer(GetRetainedBodyInput(Variable->GetDeferredInitializerRange()), *Variable);
					const bool Analyzed = DependencySema.EndInitializer();
					Prepared = Parsed && Analyzed && Prepared;
				}
				if (Variable->GetInitializer()) Prepared = PrepareReferences(Variable->GetInitializer(), Depth + 1) && Prepared;
			});
			return Prepared;
		};
		for (asCEnumConstantDecl* Constant : Constants)
			if (Constant->GetInitializer() && !PrepareReferences(Constant->GetInitializer(), 0)) Error(*Constant, TEXT("enum-constant-dependency-analysis-failed"));
		asCQualType EnumInteger;
		asCQualType::TryCreate(Context.GetBuiltinType(asEPrimitiveType::Int32), asETypeQualifiers::None, EnumInteger);
		for (;;)
		{
			bool Progress = false;
			int32 Pending = 0;
			for (asCEnumConstantDecl* Constant : Constants)
			{
				if (Constant->IsInvalid() || Constant->HasResolvedValue()) continue;
				if (!Constant->HasExplicitValue())
				{
					const asCEnumConstantDecl* Previous = nullptr;
					for (const auto* Decl : Constant->GetDeclContext()->GetDecls())
					{
						if (Decl == Constant) break;
						if (const auto* Candidate = dyn_cast<asCEnumConstantDecl>(Decl)) Previous = Candidate;
					}
					if (Previous && !Previous->HasResolvedValue()) { ++Pending; continue; }
					if (!Constant->SetResolvedValue(Previous ? Previous->GetValue() + 1 : 0)) Error(*Constant, TEXT("enum-value-out-of-range"));
					Progress = true; continue;
				}
				if (!Constant->GetInitializer()) { Error(*Constant, TEXT("enum-initializer-missing")); continue; }
				const auto Value = asEvaluateIntegralConstant(*Constant->GetInitializer());
				if (Value.Status == asEConstantEvaluationStatus::UnresolvedReference) { ++Pending; continue; }
				uint64 Bits = 0;
				if (Value.Status != asEConstantEvaluationStatus::Succeeded || !asTryConvertIntegralConstant(Value, EnumInteger, Bits))
				{
					const TCHAR* Reason = Value.Status == asEConstantEvaluationStatus::DivideByZero ? TEXT("enum-constant-divide-by-zero") :
						Value.Status == asEConstantEvaluationStatus::InvalidShift ? TEXT("enum-constant-invalid-shift") :
						Value.Status == asEConstantEvaluationStatus::Overflow || Value.Status == asEConstantEvaluationStatus::Succeeded ? TEXT("enum-value-out-of-range") : TEXT("enum-initializer-not-integral-constant");
					Error(*Constant, Reason, Value.FailureNode); continue;
				}
				Constant->SetResolvedValue(Bits & (uint64(1) << 31) ? int64(Bits) - (int64(1) << 32) : int64(Bits));
				Progress = true;
			}
			if (!Pending) break;
			if (!Progress)
			{
				for (asCEnumConstantDecl* Constant : Constants)
					if (!Constant->IsInvalid() && !Constant->HasResolvedValue()) Error(*Constant, TEXT("enum-constant-dependency-cycle-or-unresolved"));
				break;
			}
		}
		return Valid;
	}
END_AS_NAMESPACE
