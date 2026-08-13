#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/AngelscriptJITGeneration.h"

enum class EAngelscriptJITGeneratedFileDifference : uint8
{
	Missing = 0,
	ContentMismatch,
	UnexpectedOwnedFile,
	OwnershipConflict,
	TargetProfileMismatch,
	ProviderIdMismatch,
	InvalidInventory,
	InvalidExpectedOutput,
	ReadFailure,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITGeneratedFileDifferenceRecord
{
	EAngelscriptJITGeneratedFileDifference Kind =
		EAngelscriptJITGeneratedFileDifference::Missing;
	FString RelativePath;
	FAngelscriptHash256 ExpectedHash;
	FAngelscriptHash256 ActualHash;
	FString Detail;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITGeneratedFileComparison
{
	FString RootDirectory;
	TArray<FAngelscriptJITGeneratedFileDifferenceRecord> Differences;

	bool IsCurrent() const
	{
		return Differences.IsEmpty();
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITGeneratedFilePublication
{
	bool bSuccess = false;
	FString Error;
	FAngelscriptJITGeneratedFileComparison Comparison;
	TArray<FString> WrittenFiles;
	TArray<FString> UnchangedFiles;
	TArray<FString> RemovedFiles;
	/** Module-source path set changes require a normal UBT build. */
	TArray<FString> AddedModuleSources;
	TArray<FString> RemovedModuleSources;
};

/** Read-only result for a complete revision-2 owned output root. */
struct ANGELSCRIPTRUNTIME_API FAngelscriptJITOwnedOutputInspection
{
	bool bExists = false;
	bool bValid = false;
	FString RootDirectory;
	FString Error;
	TArray<FString> OwnedFiles;
	TArray<FString> ModuleSources;
};

/** Result of retiring exactly the files named by a validated inventory. */
struct ANGELSCRIPTRUNTIME_API FAngelscriptJITOwnedOutputRetirement
{
	bool bSuccess = false;
	FString Error;
	TArray<FString> RemovedFiles;
	TArray<FString> RemovedModuleSources;
};

/**
 * Shared Runtime-owned filesystem boundary for deterministic generated output.
 * It understands only versioned owned files; source discovery and project/test
 * orchestration remain outside Runtime.
 */
class ANGELSCRIPTRUNTIME_API FAngelscriptJITGeneratedFileStore
{
public:
	static FAngelscriptJITGeneratedFileComparison Compare(
		FStringView RootDirectory,
		const FAngelscriptJITGenerationOutput& Expected);

	/**
	 * Preflight every target, stage changed bytes beside their destinations,
	 * atomically replace individual owned files, and preserve identical files.
	 */
	static FAngelscriptJITGeneratedFilePublication Publish(
		FStringView RootDirectory,
		const FAngelscriptJITGenerationOutput& Expected);

	/** Inspect a revision-2 inventory without changing the output root. */
	static FAngelscriptJITOwnedOutputInspection InspectOwnedOutput(
		FStringView RootDirectory,
		EAngelscriptJITTargetProfile ExpectedProfile,
		const FAngelscriptJITProviderId& ExpectedProviderId);

	/** Revalidate and remove only revision-2 inventory-listed owned files. */
	static FAngelscriptJITOwnedOutputRetirement RetireOwnedOutput(
		FStringView RootDirectory,
		EAngelscriptJITTargetProfile ExpectedProfile,
		const FAngelscriptJITProviderId& ExpectedProviderId);
};
