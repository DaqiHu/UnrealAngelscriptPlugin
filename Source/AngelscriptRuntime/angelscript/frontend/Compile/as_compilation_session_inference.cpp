#include "frontend/Compile/as_compilation_session.h"
#include "frontend/AST/as_ast_cast.h"

BEGIN_AS_NAMESPACE
bool asCCompilationSession::ResolveInferredDeclarations()
	{
		TArray<asCVarDecl*> Variables;
		for (asCASTNode* Node : Context.GetNodes())
			if (auto* Variable = dyn_cast<asCVarDecl>(Node); Variable && !Variable->IsInvalid() && !Variable->GetResolvedType())
			{
				const auto& Syntax = Variable->GetTypeSyntax();
				if (Syntax.NameParts.Num() == 1 && Syntax.NameParts[0] == TEXT("auto")) Variables.Add(Variable);
			}
		TMap<asCVarDecl*, uint8> Colors;
		TFunction<bool(asCVarDecl*, int32)> Resolve = [&](asCVarDecl* Variable, int32 Depth)
		{
			if (Variable->GetResolvedType()) return !Variable->IsInvalid();
			const uint8 Color = Colors.FindRef(Variable);
			if (Color == 2) return false;
			if (Color == 1 || Depth > 256)
			{
				Variable->MarkInvalid();
				const TCHAR* Message = Color == 1 ? TEXT("auto-initializer-dependency-cycle") : TEXT("auto-initializer-dependency-budget");
				StableDiagnostics.Add(Variable->GetSemanticKey() + TEXT(":") + Message);
				SubmitSemanticGroup(Variable->GetSourceRange(), Message, Variable->GetSemanticKey());
				return false;
			}
			Colors.Add(Variable, 1);
			asCSema Initializer(Context, Diagnostics, StableScope);
			Initializer.BeginInitializer(*Variable,
				[&, Variable, Depth](const FString& Name)
				{
					const auto Found = LookupInBodyScope(Name, *Variable);
					for (asCDecl* Decl : Found)
						if (auto* Dependency = dyn_cast<asCVarDecl>(Decl); Dependency && Variables.Contains(Dependency)) Resolve(Dependency, Depth + 1);
					return Found;
				},
				[this, Variable](const FString& Name) { asSTypeSyntax Syntax; Syntax.NameParts.Add(Name); Syntax.Range = Variable->GetSourceRange(); return ResolveTypeSyntax(Syntax, *Variable).GetType(); },
				[this, Variable](const asSTypeSyntax& Syntax) { return ResolveTypeSyntax(Syntax, *Variable); },
				[this](const asSStableKey& Key) { return FindNominalDeclaration(Key); },
				[this](const asCSourceRange& Range) { return GetRetainedBodyInput(Range); },
				[this](asCQualType Type, asSListInitialization& Out) { return ResolveListInitializer(Type, Out); });
			asCParser Parser(Initializer);
			const bool Parsed = Parser.ParseInitializer(GetRetainedBodyInput(Variable->GetDeferredInitializerRange()), *Variable);
			const bool Analyzed = Initializer.EndInitializer();
			Colors.Add(Variable, 2);
			if (!Parsed || !Analyzed || Variable->IsInvalid() || !Variable->GetResolvedQualType().IsValid())
			{
				Variable->MarkInvalid();
				StableDiagnostics.Add(Variable->GetSemanticKey() + TEXT(":auto-initializer-analysis-failed"));
				SubmitSemanticGroup(Variable->GetSourceRange(), TEXT("auto-initializer-analysis-failed"), Variable->GetSemanticKey());
				return false;
			}
			return true;
		};
		bool Valid = true;
		for (auto* Variable : Variables) Valid = Resolve(Variable, 0) && Valid;
		return Valid;
	}
END_AS_NAMESPACE
