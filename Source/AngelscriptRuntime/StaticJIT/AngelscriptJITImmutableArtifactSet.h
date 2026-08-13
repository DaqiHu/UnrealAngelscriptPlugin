#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/AngelscriptJITProviderMatcher.h"

enum class EAngelscriptJITArtifactSetRuntimeProfile : uint8
{
	Reloadable = 0,
	ImmutableCooked,
};

enum class EAngelscriptJITImmutableArtifactSetValidationCode : uint8
{
	Eligible = 0,
	MissingProvider,
	ReloadableProfile,
	MissingImmutableDirectEntries,
	ArtifactSetMismatch,
	ProfileMismatch,
	EnvironmentMismatch,
	MissingEntryMatch,
	DuplicateEntryMatch,
	EntryNotExact,
	SelectionMismatch,
	MissingEntryPoint,
	MissingReferenceTable,
	MissingRequiredReference,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITImmutableArtifactEntryMatch
{
	int32 ProviderEntryIndex = INDEX_NONE;
	FAngelscriptJITFunctionMatchResult Match;
};

struct ANGELSCRIPTRUNTIME_API
	FAngelscriptJITImmutableArtifactSetValidationInput
{
	TSharedPtr<const FAngelscriptJITProviderCatalog,
		ESPMode::ThreadSafe> Provider;
	EAngelscriptJITArtifactSetRuntimeProfile RuntimeProfile =
		EAngelscriptJITArtifactSetRuntimeProfile::Reloadable;
	FAngelscriptHash256 CurrentArtifactSetDigest;
	FAngelscriptArtifactProfileKey CurrentArtifactProfile;
	FAngelscriptHash256 CurrentNativeEnvironmentFingerprint;
	TArray<FAngelscriptJITImmutableArtifactEntryMatch> EntryMatches;
};

/**
 * Opaque proof that one copied Provider catalog was validated as a complete
 * immutable cooked set. Default-constructed and rejected results grant no
 * direct-dispatch authority.
 */
class ANGELSCRIPTRUNTIME_API
	FAngelscriptJITImmutableArtifactSetValidationResult
{
public:
	EAngelscriptJITImmutableArtifactSetValidationCode Code() const
	{
		return ValidationCode;
	}

	int32 FailedEntryIndex() const
	{
		return FailedEntry;
	}

	bool IsEligible() const
	{
		return ValidationCode
			== EAngelscriptJITImmutableArtifactSetValidationCode::Eligible;
	}

	bool AllowsDirectSelection(
		const FAngelscriptJITArtifactSelection& Selection) const;

private:
	friend class FAngelscriptJITImmutableArtifactSetValidator;

	EAngelscriptJITImmutableArtifactSetValidationCode ValidationCode =
		EAngelscriptJITImmutableArtifactSetValidationCode::MissingProvider;
	int32 FailedEntry = INDEX_NONE;
	TSharedPtr<const FAngelscriptJITProviderCatalog,
		ESPMode::ThreadSafe> ValidatedProvider;
	TSet<int32> DirectEntryIndices;
};

/**
 * Performs the all-or-nothing gate required before content-specific cooked
 * direct links may become executable. Per-function matcher success is not
 * enough: every Provider entry, required entry point, and resolved reference
 * must belong to the same exact immutable catalog publication.
 */
class ANGELSCRIPTRUNTIME_API FAngelscriptJITImmutableArtifactSetValidator
{
public:
	static FAngelscriptJITImmutableArtifactSetValidationResult Validate(
		const FAngelscriptJITImmutableArtifactSetValidationInput& Input);
};
