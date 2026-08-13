#pragma once

#include "CoreMinimal.h"

#include "Artifacts/AngelscriptArtifactIdentity.h"
#include "Artifacts/AngelscriptArtifactReference.h"
#include "Features/IModularFeatures.h"

#include "AngelscriptInclude.h"

namespace FAngelscriptJITProviderAbi
{
	inline constexpr uint32 Revision = 2;
	static constexpr uint32 MaximumEntryCount = 1u << 20;
	static constexpr uint32 MaximumReferenceSlotCount = 1u << 16;
}

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderId
{
	FAngelscriptHash256 Hash;

	bool IsValid() const
	{
		return !Hash.IsZero();
	}

	friend bool operator==(
		const FAngelscriptJITProviderId& Left,
		const FAngelscriptJITProviderId& Right)
	{
		return Left.Hash == Right.Hash;
	}

	friend bool operator!=(
		const FAngelscriptJITProviderId& Left,
		const FAngelscriptJITProviderId& Right)
	{
		return !(Left == Right);
	}

	friend bool operator<(
		const FAngelscriptJITProviderId& Left,
		const FAngelscriptJITProviderId& Right)
	{
		return Left.Hash < Right.Hash;
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderGeneration
{
	FAngelscriptHash256 Hash;

	bool IsValid() const
	{
		return !Hash.IsZero();
	}

	friend bool operator==(
		const FAngelscriptJITProviderGeneration& Left,
		const FAngelscriptJITProviderGeneration& Right)
	{
		return Left.Hash == Right.Hash;
	}

	friend bool operator!=(
		const FAngelscriptJITProviderGeneration& Left,
		const FAngelscriptJITProviderGeneration& Right)
	{
		return !(Left == Right);
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITEntryPoints
{
	asJITFunction VMEntry = nullptr;
	asJITFunction_Raw RawEntry = nullptr;
	asJITFunction_ParmsEntry ParmsEntry = nullptr;

	bool HasAny() const
	{
		return VMEntry != nullptr || RawEntry != nullptr || ParmsEntry != nullptr;
	}
};

enum class EAngelscriptJITReferenceSlotFlags : uint32
{
	None = 0,
	Optional = 1u << 0,
};
ENUM_CLASS_FLAGS(EAngelscriptJITReferenceSlotFlags)

enum class EAngelscriptJITArtifactEntryFlags : uint32
{
	None = 0,
	RequireExactDebugIdentity = 1u << 0,
	ImmutableDirectCallSet = 1u << 1,
};
ENUM_CLASS_FLAGS(EAngelscriptJITArtifactEntryFlags)

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITReferenceSlot
{
	uint32 StructSize = sizeof(FAngelscriptJITReferenceSlot);
	uint32 SlotIndex = 0;
	EAngelscriptJITReferenceSlotFlags Flags =
		EAngelscriptJITReferenceSlotFlags::None;
	uint32 Reserved = 0;
	FAngelscriptArtifactReference Reference;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITArtifactEntry
{
	uint32 StructSize = sizeof(FAngelscriptJITArtifactEntry);
	EAngelscriptJITArtifactEntryFlags Flags =
		EAngelscriptJITArtifactEntryFlags::None;
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptStableFunctionKey FunctionKey;
	FAngelscriptHash256 ExecutionHash;
	FAngelscriptHash256 DebugHash;
	FAngelscriptArtifactProfileKey ArtifactProfile;
	FAngelscriptHash256 EntryAbiHash;
	FAngelscriptHash256 NativeEnvironmentFingerprint;
	FAngelscriptJITEntryPoints EntryPoints;
	const FAngelscriptJITReferenceSlot* ReferenceSlots = nullptr;
	uint32 ReferenceSlotCount = 0;
	uint32 Reserved = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderView
{
	uint32 StructSize = sizeof(FAngelscriptJITProviderView);
	uint32 AbiRevision = FAngelscriptJITProviderAbi::Revision;
	FAngelscriptJITProviderId ProviderId;
	FAngelscriptJITProviderGeneration ProviderGeneration;
	FAngelscriptHash256 ArtifactSetDigest;
	FAngelscriptArtifactProfileKey ArtifactProfile;
	FAngelscriptHash256 NativeEnvironmentFingerprint;
	const TCHAR* ProviderName = nullptr;
	const TCHAR* OwnerModuleName = nullptr;
	const FAngelscriptJITArtifactEntry* Entries = nullptr;
	uint32 EntryCount = 0;
};

enum class EAngelscriptJITProviderValidationCode : uint8
{
	Valid = 0,
	MissingProviderView,
	InvalidViewSize,
	UnsupportedAbiRevision,
	InvalidProviderId,
	InvalidProviderGeneration,
	MissingProviderName,
	MissingOwnerModuleName,
	InvalidArtifactSetDigest,
	InvalidArtifactProfile,
	InvalidEnvironmentFingerprint,
	InvalidEntryCount,
	MissingEntryTable,
	InvalidEntrySize,
	InvalidEntryFlags,
	InvalidModuleKey,
	InvalidFunctionKey,
	InvalidExecutionHash,
	InvalidDebugHash,
	InvalidEntryProfile,
	InvalidEntryEnvironment,
	InvalidEntryAbi,
	MissingEntryPoints,
	InvalidReferenceCount,
	MissingReferenceTable,
	InvalidReferenceSize,
	InvalidReferenceFlags,
	InvalidReference,
	InvalidReferenceAbi,
	InvalidReferenceOrder,
	UnsortedEntries,
	DuplicateEntry,
	ArtifactSetDigestMismatch,
	ProviderGenerationDigestMismatch,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderValidationResult
{
	EAngelscriptJITProviderValidationCode Code =
		EAngelscriptJITProviderValidationCode::Valid;
	int32 EntryIndex = INDEX_NONE;
	int32 ReferenceIndex = INDEX_NONE;

	bool IsValid() const
	{
		return Code == EAngelscriptJITProviderValidationCode::Valid;
	}
};

class ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderManifest
{
public:
	static FAngelscriptHash256 BuildArtifactSetDigest(
		TConstArrayView<FAngelscriptJITArtifactEntry> Entries);

	/**
	 * Build the content identity for one complete provider publication.
	 * Diagnostic strings and process-local entry addresses are intentionally
	 * excluded; their semantic entry-point availability is already part of the
	 * artifact-set digest.
	 */
	static FAngelscriptJITProviderGeneration BuildProviderGeneration(
		const FAngelscriptJITProviderView& View);

	static FAngelscriptJITProviderValidationResult Validate(
		const FAngelscriptJITProviderView& View);
};

class ANGELSCRIPTRUNTIME_API IAngelscriptJITArtifactProvider
	: public IModularFeature
{
public:
	virtual ~IAngelscriptJITArtifactProvider() = default;

	virtual const FAngelscriptJITProviderView* GetAngelscriptJITProviderView()
		const = 0;

	static FName FeatureName();
};

static_assert(sizeof(FAngelscriptJITProviderId) == 32,
	"ProviderId must retain the full 256-bit stable identity.");
static_assert(sizeof(FAngelscriptJITProviderGeneration) == 32,
	"ProviderGeneration must retain the full 256-bit content identity.");
static_assert(sizeof(FAngelscriptJITEntryPoints) == sizeof(void*) * 3,
	"JIT entry-point ABI changed; bump FAngelscriptJITProviderAbi::Revision.");
