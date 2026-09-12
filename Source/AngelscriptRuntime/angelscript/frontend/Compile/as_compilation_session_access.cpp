#include "frontend/Compile/as_compilation_session.h"
#include "frontend/AST/as_ast_cast.h"
#include "Containers/Map.h"

BEGIN_AS_NAMESPACE
bool asCCompilationSession::ResolveAccessPolicies()
	{
		bool Valid = true;
		auto Error = [&](asCDecl& Decl, const TCHAR* Message)
		{
			Valid = false;
			Decl.MarkInvalid();
			const auto* Named = dyn_cast<asCNamedDecl>(&Decl);
			const FString Coordinate = Named ? Named->GetSemanticKey() : TEXT("access");
			StableDiagnostics.Add(Coordinate + TEXT(":") + Message);
			auto Fragment = Diagnostics.CreateFragment(Coordinate);
			asSDiagnosticRecord Record;
			Record.DiagnosticID = 4104;
			Record.Severity = asEDiagnosticSeverity::Error;
			Record.PrimaryRange = Decl.GetSourceRange();
			Record.Arguments.Add(asSDiagnosticArgument::FromText(Message));
			Fragment.Report(MoveTemp(Record));
			Diagnostics.Submit(MoveTemp(Fragment));
		};
		for (asCASTNode* Node : Context.GetNodes())
		{
			auto* Record = dyn_cast<asCRecordDecl>(Node);
			if (!Record || Record->IsInvalid()) continue;
			TMap<FString, asCAccessSpecifierDecl*> Policies;
			for (asCDecl* Decl : Record->GetDecls())
			{
				auto* Access = dyn_cast<asCAccessSpecifierDecl>(Decl);
				if (!Access || Access->IsInvalid()) continue;
				if (Policies.Contains(Access->GetName()))
				{
					Error(*Access, TEXT("duplicate-access-policy"));
					continue;
				}
				Policies.Add(Access->GetName(), Access);
			}
			for (asCDecl* Decl : Record->GetDecls())
			{
				asCVarDecl* Variable = dyn_cast<asCVarDecl>(Decl);
				asCFunctionDecl* Function = dyn_cast<asCFunctionDecl>(Decl);
				const asSAuthoredAccessUse* Use = Variable ? Variable->GetAuthoredAccessUse()
					: Function ? Function->GetAuthoredAccessUse() : nullptr;
				if (!Use) continue;
				asCAccessSpecifierDecl* Policy = Policies.FindRef(Use->Name);
				if (!Policy || Policy->GetDeclContext() != Record)
				{
					Error(*Decl, Policy ? TEXT("foreign-access-policy") : TEXT("unknown-access-policy"));
					continue;
				}
				const bool Bound = Variable ? Variable->ResolveAuthoredAccessUse(Policy)
					: Function->ResolveAuthoredAccessUse(Policy);
				if (!Bound) Error(*Decl, TEXT("unresolved-access-policy"));
			}
		}
		return Valid;
	}
END_AS_NAMESPACE
