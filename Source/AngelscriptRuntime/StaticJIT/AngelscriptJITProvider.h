#pragma once

#include "CoreMinimal.h"

#include "Artifacts/AngelscriptArtifactIdentity.h"
#include "Artifacts/AngelscriptArtifactReference.h"
#include "Features/IModularFeatures.h"

#include "AngelscriptInclude.h"

namespace FAngelscriptJITProviderAbi
{
	inline constexpr uint32 Revision = 8;
	static constexpr uint32 MaximumEntryCount = 1u << 20;
	static constexpr uint32 MaximumReferenceSlotCount = 1u << 16;
	static constexpr uint32 MaximumNativeCallSiteCount = 1u << 16;
	static constexpr uint32 MaximumSemanticDependencyCount = 1u << 20;
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
	HasExecutionCapabilityProfile = 1u << 2,
};
ENUM_CLASS_FLAGS(EAngelscriptJITArtifactEntryFlags)

/** Provider-neutral hooks that one installed entry can satisfy at invocation. */
enum class EAngelscriptJITExecutionCapability : uint32
{
	None = 0,
	FramePosition = 1u << 0,
	LineCallback = 1u << 1,
	DebuggerStep = 1u << 2,
	DebuggerLocals = 1u << 3,
	Coverage = 1u << 4,
	LoopTimeout = 1u << 5,
	AbortSuspend = 1u << 6,
	RecursionBudget = 1u << 7,
};
ENUM_CLASS_FLAGS(EAngelscriptJITExecutionCapability)

namespace FAngelscriptJITExecutionCapabilityMask
{
	inline constexpr uint32 All =
		static_cast<uint32>(EAngelscriptJITExecutionCapability::FramePosition)
		| static_cast<uint32>(EAngelscriptJITExecutionCapability::LineCallback)
		| static_cast<uint32>(EAngelscriptJITExecutionCapability::DebuggerStep)
		| static_cast<uint32>(EAngelscriptJITExecutionCapability::DebuggerLocals)
		| static_cast<uint32>(EAngelscriptJITExecutionCapability::Coverage)
		| static_cast<uint32>(EAngelscriptJITExecutionCapability::LoopTimeout)
		| static_cast<uint32>(EAngelscriptJITExecutionCapability::AbortSuspend)
		| static_cast<uint32>(EAngelscriptJITExecutionCapability::RecursionBudget);
}

struct FAngelscriptJITProviderDiagnostics;

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITReferenceSlot
{
	uint32 StructSize = sizeof(FAngelscriptJITReferenceSlot);
	uint32 SlotIndex = 0;
	EAngelscriptJITReferenceSlotFlags Flags =
		EAngelscriptJITReferenceSlotFlags::None;
	uint32 Reserved = 0;
	FAngelscriptArtifactReference Reference;
};

/** Stable semantic route rendered by generated source, manifests and dumps. */
enum class EAngelscriptJITNativeCallSiteRoute : uint32
{
	Invalid = 0,
	DirectExported,
	DirectInline,
	RuntimeThunk,
	Bridge,
	CurrentNativeBinding,
};

/**
 * Immutable pointer-free identity plus diagnostic strings for one generated
 * native call expression. Strings never participate in dispatch. A bridge is
 * bound only by StableTargetKey/ExpectedAbi/ReferenceSlotIndex, which must
 * exactly name one reference slot in the containing entry.
 */
struct ANGELSCRIPTRUNTIME_API FAngelscriptJITNativeCallSite
{
	uint32 StructSize = sizeof(FAngelscriptJITNativeCallSite);
	uint32 CallSiteIndex = 0;
	EAngelscriptJITNativeCallSiteRoute Route =
		EAngelscriptJITNativeCallSiteRoute::Invalid;
	const TCHAR* CanonicalDeclaration = nullptr;
	const TCHAR* EmittedCppCallee = nullptr;
	const TCHAR* RuntimeDllCore = nullptr;
	const TCHAR* MetadataRowSymbol = nullptr;
	const TCHAR* RegisteredTarget = nullptr;
	FAngelscriptHash256 StableTargetKey;
	FAngelscriptHash256 ExpectedAbi;
	int32 ReferenceSlotIndex = INDEX_NONE;
	uint32 Reserved = 0;
};

enum class EAngelscriptJITSemanticDependencyFlags : uint32
{
	None = 0,
	HasExpectedContentOrValue = 1u << 0,
};
ENUM_CLASS_FLAGS(EAngelscriptJITSemanticDependencyFlags)

/**
 * One Provider-owned semantic dependency row. Entries address a slice of the
 * Provider-wide table by start/count. Identity is kind + target reference;
 * display strings never participate.
 */
struct ANGELSCRIPTRUNTIME_API FAngelscriptJITSemanticDependencyRow
{
	uint32 StructSize = sizeof(FAngelscriptJITSemanticDependencyRow);
	uint32 RowIndex = 0;
	EAngelscriptArtifactSemanticDependencyKind Kind =
		EAngelscriptArtifactSemanticDependencyKind::Invalid;
	EAngelscriptArtifactReferenceKind TargetKind =
		EAngelscriptArtifactReferenceKind::Invalid;
	EAngelscriptJITSemanticDependencyFlags Flags =
		EAngelscriptJITSemanticDependencyFlags::None;
	uint32 Reserved = 0;
	FAngelscriptHash256 TargetStableKey;
	FAngelscriptHash256 ExpectedAbi;
	FAngelscriptHash256 ExpectedContentOrValue;
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
	const FAngelscriptJITNativeCallSite* NativeCallSites = nullptr;
	uint32 NativeCallSiteCount = 0;
	uint32 Reserved = 0;
	EAngelscriptJITExecutionCapability AvailableExecutionCapabilities =
		EAngelscriptJITExecutionCapability::None;
	FAngelscriptHash256 ExecutionCapabilityProfileHash;
	/**
	 * Capabilities that become required only when the corresponding runtime
	 * facility is active. For example, LoopTimeout is required only for a
	 * closure that contains a loop backedge and while the timeout is enabled.
	 */
	EAngelscriptJITExecutionCapability RuntimeExecutionRequirementTriggers =
		EAngelscriptJITExecutionCapability::None;
	uint32 SemanticDependencyStartIndex = 0;
	uint32 SemanticDependencyCount = 0;
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
	/** Optional non-Shipping metadata; excluded from execution identity. */
	const FAngelscriptJITProviderDiagnostics* Diagnostics = nullptr;
	const FAngelscriptJITSemanticDependencyRow* SemanticDependencies = nullptr;
	uint32 SemanticDependencyCount = 0;
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
	InvalidExecutionCapabilities,
	InvalidExecutionCapabilityProfile,
	InvalidExecutionRequirementTriggers,
	InvalidReferenceCount,
	MissingReferenceTable,
	InvalidReferenceSize,
	InvalidReferenceFlags,
	InvalidReference,
	InvalidReferenceAbi,
	InvalidReferenceOrder,
	InvalidNativeCallSiteCount,
	MissingNativeCallSiteTable,
	InvalidNativeCallSiteSize,
	InvalidNativeCallSiteRoute,
	InvalidNativeCallSiteOrder,
	MissingNativeCallSiteDeclaration,
	MissingNativeCallSiteCallee,
	MissingNativeCallSiteTarget,
	InvalidDirectNativeCallSite,
	InvalidBridgeNativeCallSite,
	InvalidCurrentNativeBindingCallSite,
	InvalidDiagnosticSize,
	UnsupportedDiagnosticSchema,
	InvalidDiagnosticDigest,
	InvalidDiagnosticFallbackChain,
	InvalidDiagnosticFunctionCount,
	MissingDiagnosticFunctionTable,
	InvalidDiagnosticFunctionSize,
	InvalidDiagnosticFunctionKey,
	InvalidDiagnosticFunctionOrder,
	InvalidDiagnosticAttemptCount,
	MissingDiagnosticAttemptTable,
	InvalidDiagnosticAttemptSize,
	InvalidDiagnosticAttemptRange,
	InvalidDiagnosticSemanticCount,
	MissingDiagnosticSemanticTable,
	InvalidDiagnosticSemanticSize,
	InvalidDiagnosticSemanticRange,
	InvalidDiagnosticCallCount,
	MissingDiagnosticCallTable,
	InvalidDiagnosticCallSize,
	InvalidDiagnosticCallRange,
	InvalidDiagnosticString,
	DiagnosticDigestMismatch,
	UnsortedEntries,
	DuplicateEntry,
	ArtifactSetDigestMismatch,
	ProviderGenerationDigestMismatch,
	InvalidSemanticDependencyCount,
	MissingSemanticDependencyTable,
	InvalidSemanticDependencySize,
	InvalidSemanticDependencyKind,
	InvalidSemanticDependencyReference,
	InvalidSemanticDependencyKey,
	InvalidSemanticDependencyAbi,
	MissingSemanticDependencyExpectedContent,
	InvalidSemanticDependencyOrder,
	InvalidSemanticDependencyRange,
	DuplicateSemanticDependency,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderValidationResult
{
	EAngelscriptJITProviderValidationCode Code =
		EAngelscriptJITProviderValidationCode::Valid;
	int32 EntryIndex = INDEX_NONE;
	int32 ReferenceIndex = INDEX_NONE;
	int32 NativeCallSiteIndex = INDEX_NONE;
	int32 DiagnosticFunctionIndex = INDEX_NONE;
	int32 DiagnosticAttemptIndex = INDEX_NONE;
	int32 DiagnosticSemanticIndex = INDEX_NONE;
	int32 DiagnosticCallIndex = INDEX_NONE;
	int32 SemanticDependencyIndex = INDEX_NONE;

	bool IsValid() const
	{
		return Code == EAngelscriptJITProviderValidationCode::Valid;
	}
};

enum class EAngelscriptJITSemanticDependencyCanonicalCode : uint8
{
	Valid = 0,
	InvalidKind,
	InvalidTarget,
	MissingExpectedContentOrValue,
	ConflictingDuplicate,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITSemanticDependencyCanonicalResult
{
	EAngelscriptJITSemanticDependencyCanonicalCode Code =
		EAngelscriptJITSemanticDependencyCanonicalCode::Valid;
	int32 RowIndex = INDEX_NONE;
	TArray<FAngelscriptArtifactSemanticDependency> Rows;

	bool IsValid() const
	{
		return Code == EAngelscriptJITSemanticDependencyCanonicalCode::Valid;
	}
};

class ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderManifest
{
public:
	static FAngelscriptHash256 BuildArtifactSetDigest(
		TConstArrayView<FAngelscriptJITArtifactEntry> Entries,
		TConstArrayView<FAngelscriptJITSemanticDependencyRow>
			SemanticDependencies = {});

	/** Build identity for metadata only; excluded from Provider generation. */
	static FAngelscriptHash256 BuildDiagnosticsDigest(
		const FAngelscriptJITProviderDiagnostics& Diagnostics);

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

	static bool SemanticDependencyRequiresExpectedContent(
		EAngelscriptArtifactSemanticDependencyKind Kind);

	static void FillSemanticDependencyRow(
		const FAngelscriptArtifactSemanticDependency& Dependency,
		uint32 RowIndex,
		FAngelscriptJITSemanticDependencyRow& OutRow);

	static FAngelscriptJITSemanticDependencyCanonicalResult
		CanonicalizeSemanticDependencies(
			TConstArrayView<FAngelscriptArtifactSemanticDependency> Rows);
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
