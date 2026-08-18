#pragma once

#include "CoreMinimal.h"

#include "Artifacts/AngelscriptArtifactIdentity.h"
#include "StaticJIT/StaticJITConfig.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITModel.h"

#if AS_CAN_GENERATE_JIT

/**
 * Execution capabilities that must be satisfied by every directly emitted
 * TypedASTJIT function in one root closure.
 *
 * These flags are internal generation/route facts. They are not a ProviderId,
 * a Cache V2 ArtifactProfile, or a frozen third-party ABI.
 */
enum class EAngelscriptTypedASTJITDirectCapability : uint16
{
	None = 0,
	FramePosition = 1 << 0,
	LineCallback = 1 << 1,
	DebuggerStep = 1 << 2,
	DebuggerLocals = 1 << 3,
	Coverage = 1 << 4,
	LoopTimeout = 1 << 5,
	AbortSuspend = 1 << 6,
	RecursionBudget = 1 << 7,
};
ENUM_CLASS_FLAGS(EAngelscriptTypedASTJITDirectCapability);

/** Stable ordered display used by diagnostics and normalized planner dumps. */
ANGELSCRIPTRUNTIME_API FString LexToString(
	EAngelscriptTypedASTJITDirectCapability Capabilities);

/**
 * Immutable description of the hooks actually emitted by one TypedASTJIT
 * generation profile. IdentityHash changes whenever generated semantics may
 * differ even when the source HIR is identical.
 */
struct ANGELSCRIPTRUNTIME_API
	FAngelscriptTypedASTJITGenerationCapabilityProfile
{
	static constexpr uint32 SchemaRevision = 1;

	EAngelscriptTypedASTJITDirectCapability AvailableCapabilities =
		EAngelscriptTypedASTJITDirectCapability::None;
	FAngelscriptHash256 IdentityHash;

	bool IsValid() const;
	FString DumpNormalized() const;
};

/** Caller-owned current state captured without retaining runtime pointers. */
struct ANGELSCRIPTRUNTIME_API
	FAngelscriptTypedASTJITExecutionRequirementsInput
{
	bool bRequireFramePosition = false;
	bool bBreakpointActive = false;
	bool bStepActive = false;
	bool bDebuggerLocalsRequired = false;
	bool bCoverageRecording = false;
	bool bLoopTimeoutRequired = false;
	bool bAbortOrSuspendPollingRequired = false;
	bool bRecursionBudgetRequired = false;
};

/** Frozen normalized requirements for one selection/generation decision. */
struct ANGELSCRIPTRUNTIME_API
	FAngelscriptTypedASTJITExecutionRequirementsSnapshot
{
	static constexpr uint32 SchemaRevision = 1;

	EAngelscriptTypedASTJITDirectCapability RequiredCapabilities =
		EAngelscriptTypedASTJITDirectCapability::None;
	FAngelscriptHash256 IdentityHash;

	bool IsValid() const;
	FString DumpNormalized() const;
};

enum class EAngelscriptTypedASTJITExecutionRoute : uint8
{
	TypedASTJIT = 0,
	VM,
};

/** Pure fail-closed comparison used before entering generated Typed code. */
struct ANGELSCRIPTRUNTIME_API
	FAngelscriptTypedASTJITExecutionRouteEligibility
{
	EAngelscriptTypedASTJITExecutionRoute Route =
		EAngelscriptTypedASTJITExecutionRoute::VM;
	EAngelscriptTypedASTJITDirectCapability MissingCapabilities =
		EAngelscriptTypedASTJITDirectCapability::None;
	EAngelscriptTypedASTJITFallbackReason Reason =
		EAngelscriptTypedASTJITFallbackReason::BackendUnavailable;
	FString StableDetail;

	bool UsesTypedASTJIT() const
	{
		return Route == EAngelscriptTypedASTJITExecutionRoute::TypedASTJIT;
	}
};

ANGELSCRIPTRUNTIME_API bool
BuildAngelscriptTypedASTJITGenerationCapabilityProfile(
	EAngelscriptTypedASTJITDirectCapability AvailableCapabilities,
	FAngelscriptTypedASTJITGenerationCapabilityProfile& OutProfile,
	FString* OutError = nullptr);

ANGELSCRIPTRUNTIME_API bool
BuildAngelscriptTypedASTJITExecutionRequirementsSnapshot(
	const FAngelscriptTypedASTJITExecutionRequirementsInput& Input,
	FAngelscriptTypedASTJITExecutionRequirementsSnapshot& OutSnapshot,
	FString* OutError = nullptr);

/** Rebuilds a normalized snapshot after closure analysis adds requirements. */
ANGELSCRIPTRUNTIME_API bool
BuildAngelscriptTypedASTJITExecutionRequirementsSnapshot(
	EAngelscriptTypedASTJITDirectCapability RequiredCapabilities,
	FAngelscriptTypedASTJITExecutionRequirementsSnapshot& OutSnapshot,
	FString* OutError = nullptr);

ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITExecutionRouteEligibility
EvaluateAngelscriptTypedASTJITExecutionRoute(
	const FAngelscriptTypedASTJITGenerationCapabilityProfile& Profile,
	const FAngelscriptTypedASTJITExecutionRequirementsSnapshot& Requirements,
	bool bDirectCalleeMismatch = false);

/**
 * Builds the body identity later assigned to a Typed emitted function.
 * Returning zero is fail-closed for an invalid source hash or profile.
 */
ANGELSCRIPTRUNTIME_API FAngelscriptHash256
BuildAngelscriptTypedASTJITProfiledContentIdentity(
	const FAngelscriptHash256& SourceExecutionHash,
	const FAngelscriptTypedASTJITGenerationCapabilityProfile& Profile);

#endif // AS_CAN_GENERATE_JIT
