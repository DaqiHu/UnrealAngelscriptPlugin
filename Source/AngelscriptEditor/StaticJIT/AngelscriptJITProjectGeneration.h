#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/AngelscriptJITGeneratedFileStore.h"

struct IAngelscriptSourceProvider;
struct FAngelscriptEngine;

namespace AngelscriptEditor::StaticJIT
{
	enum class EAngelscriptCanonicalASTGenerateCode : uint8
	{
		Succeeded = 0,
		ASTSnapshotRequired,
		AuthoritativeEngineStale,
	};

	struct FAngelscriptCanonicalASTGenerateResult
	{
		bool bSuccess = false;
		EAngelscriptCanonicalASTGenerateCode Code =
			EAngelscriptCanonicalASTGenerateCode::ASTSnapshotRequired;
		FString Detail;
		int32 CreatedGenerationEngineCount = 0;
		int32 LiveGenerationEngineCount = 0;
		int32 MaxLiveGenerationEngines = 0;
		bool bCompiledSource = false;
		int32 LeasedASTSnapshotCount = 0;
		int32 CatalogRecipeHits = 0;
		int32 CatalogRecipeMisses = 0;
		bool bHotReloadFrozenWhileHeld = false;
		TArray<FString> WrittenOwnedFiles;
	};

	struct FAngelscriptCanonicalASTGenerateRequest
	{
		FAngelscriptEngine* PrimaryEngine = nullptr;
		FString ProjectFile;
		EAngelscriptJITTargetProfile TargetProfile =
			EAngelscriptJITTargetProfile::Invalid;
		FString OutputDirectory;
		TFunction<void()> OnHoldingLeases;
	};

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

		/**
		 * Matching-profile Generate: lease the current primary compiled graph and
		 * retained AST. Never constructs StaticJITGeneration or recompiles source.
		 */
		static FAngelscriptCanonicalASTGenerateResult GenerateMatchingProfile(
			FAngelscriptEngine& PrimaryEngine,
			FStringView ProjectFile,
			EAngelscriptJITTargetProfile TargetProfile);

		static FAngelscriptCanonicalASTGenerateResult GenerateMatchingProfile(
			const FAngelscriptCanonicalASTGenerateRequest& Request);

		/** One contained generation Engine at a time for non-matching profiles. */
		static FAngelscriptCanonicalASTGenerateResult GenerateContainedProfile(
			FAngelscriptEngine& PrimaryEngine,
			FStringView ProjectFile,
			EAngelscriptJITTargetProfile TargetProfile,
			FStringView OutputDirectory = FStringView());

		/** Sequential profiles; matching uses the primary lease path. */
		static TArray<FAngelscriptCanonicalASTGenerateResult> GenerateProfilesSequential(
			FAngelscriptEngine& PrimaryEngine,
			FStringView ProjectFile,
			TConstArrayView<EAngelscriptJITTargetProfile> Profiles,
			FStringView OutputRoot = FStringView());

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
