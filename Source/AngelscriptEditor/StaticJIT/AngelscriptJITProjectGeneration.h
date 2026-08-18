#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/AngelscriptJITGeneratedFileStore.h"

struct IAngelscriptSourceProvider;

namespace AngelscriptEditor::StaticJIT
{
	struct FAngelscriptJITProjectGenerationResult
	{
		bool bSuccess = false;
		FString Error;
		/** Deterministic, human-readable Verify identity and difference report. */
		FString Report;
		FString VerificationDirectory;
		FAngelscriptJITGeneratedFileComparison Comparison;
		FAngelscriptJITGeneratedFilePublication Publication;
		TArray<FString> AddedModuleSources;
		TArray<FString> RemovedModuleSources;

		bool HasModuleSourceSetChange() const
		{
			return !AddedModuleSources.IsEmpty()
				|| !RemovedModuleSources.IsEmpty();
		}
	};

	/** Editor-owned project output orchestration over Runtime-only primitives. */
	class ANGELSCRIPTEDITOR_API FAngelscriptJITProjectGeneration
	{
	public:
		static FString GetProfileOutputDirectory(
			FStringView ProjectFile,
			EAngelscriptJITTargetProfile TargetProfile);

		/** Keep module sources compilable and rename only provider implementation. */
		static bool PrepareProjectProfileOutput(
			const FAngelscriptJITGenerationOutput& RuntimeOutput,
			FAngelscriptJITGenerationOutput& OutProjectOutput,
			FString* OutError = nullptr);

		/** Discover/compile the project Script root in one isolated target Engine. */
		static bool BuildProjectArtifacts(
			FStringView ProjectFile,
			EAngelscriptJITTargetProfile TargetProfile,
			FAngelscriptJITGenerationOutput& OutGeneratedOutput,
			FString* OutError = nullptr);

		/** Build from one already-validated immutable Editor source snapshot. */
		static bool BuildProjectArtifacts(
			FStringView ProjectFile,
			EAngelscriptJITTargetProfile TargetProfile,
			TSharedPtr<IAngelscriptSourceProvider> SourceProviderOverride,
			FAngelscriptJITGenerationOutput& OutGeneratedOutput,
			FString* OutError = nullptr);

		static FAngelscriptJITProjectGenerationResult Generate(
			FStringView OutputDirectory,
			const FAngelscriptJITGenerationOutput& GeneratedOutput);

		/**
		 * Materialize expected bytes beneath a deterministic isolated temporary
		 * root, compare them, then compare project output without writing it.
		 */
		static FAngelscriptJITProjectGenerationResult Verify(
			FStringView OutputDirectory,
			FStringView TemporaryRootDirectory,
			const FAngelscriptJITGenerationOutput& GeneratedOutput);
	};
}
