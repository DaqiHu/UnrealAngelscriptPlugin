/* Maintained Unreal AngelScript fork — concrete UE-aware frontend result. */
#pragma once

#include "Core/AngelscriptDescriptors.h"
#include "frontend/as_compilation_session.h"
#include "frontend/as_dependency_graph.h"

BEGIN_AS_NAMESPACE

enum class EAngelscriptPreprocessResultState : uint8
	{
		Failed,
		Parsed,
		Resolved,
	};

	struct ANGELSCRIPTRUNTIME_API FAngelscriptPreprocessResult
	{
		bool IsSuccess() const
		{
			return State == EAngelscriptPreprocessResultState::Resolved
				&& SemanticSession.IsValid()
				&& DeclarationDependencies.IsFinalized()
				&& BodyInvalidation.IsFinalized();
		}
		bool HasPublishableDescriptors() const { return IsSuccess() && !Modules.IsEmpty(); }
		EAngelscriptPreprocessResultState GetState() const { return State; }
		uint64 GetConfigurationDigest() const { return ConfigurationDigest; }
		TConstArrayView<TSharedRef<FAngelscriptModuleDesc>> GetModules() const { return MakeArrayView(Modules); }
		TConstArrayView<asSDiagnosticRecord> GetDiagnostics() const { return MakeArrayView(Diagnostics); }

		EAngelscriptPreprocessResultState State = EAngelscriptPreprocessResultState::Failed;
		uint64 ConfigurationDigest = 0;
		TSharedPtr<asCCompilationSession> SemanticSession;
		TArray<TSharedPtr<asSDirectiveProcessResult>> PreprocessingResults;
		TArray<TSharedRef<FAngelscriptModuleDesc>> Modules;
		FAngelscriptModuleDependencyGraph DeclarationDependencies;
		FAngelscriptBodyInvalidationGraph BodyInvalidation;
		TArray<asSDiagnosticRecord> Diagnostics;

		void RejectPublishableOutput();
	};

END_AS_NAMESPACE
