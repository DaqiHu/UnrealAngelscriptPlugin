#pragma once

#include "CoreMinimal.h"
#include "Core/Artifacts/AngelscriptArtifactReference.h"
#include "StaticJIT/AngelscriptJITProvider.h"

#include "StartAngelscriptHeaders.h"
#include "as_datatype.h"
#include "as_module.h"
#include "EndAngelscriptHeaders.h"

struct FAngelscriptTypedASTJITGenerationCapabilityProfile;

/** Exact task-local reconciliation of one Canonical global coordinate to stable authority. */
struct FAngelscriptTypedASTJITGlobalDependency
{
	int32 EngineLocalGlobalPropertyId = INDEX_NONE;
	/** Exact namespace-qualified Decl key used by the sealed canonical AST. */
	FString CanonicalASTDeclKey;
	EAngelscriptArtifactSemanticDependencyKind Kind =
		EAngelscriptArtifactSemanticDependencyKind::Invalid;
	FAngelscriptArtifactReference Reference;
	TOptional<FAngelscriptHash256> ExpectedContentOrValue;
};

ANGELSCRIPTRUNTIME_API bool
ValidateAngelscriptTypedASTJITFoldedGlobalDependency(
	TConstArrayView<FAngelscriptTypedASTJITGlobalDependency> Dependencies,
	int32 EngineLocalGlobalPropertyId,
	FString* OutError = nullptr);

enum class EAngelscriptTypedASTJITFallbackReason : uint8
{
	None,
	NotUFunctionRoot,
	MissingCanonicalAST,
	UnsupportedFunctionKind,
	UnsupportedFunctionTrait,
	UnsupportedReceiver,
	UnsupportedUFunctionFlags,
	UnsupportedSignature,
	UnsupportedType,
	UnsupportedStatement,
	UnsupportedExpression,
	UnsupportedCall,
	UnsupportedGlobalStorage,
	UnsupportedGlobalInitializer,
	UnsupportedImportedRoute,
	SemanticDependencyMismatch,
	RuntimeTypeBindingMismatch,
	NonPortableNumericConversion,
	UnsupportedExecutionObservability,
	UnsupportedExecutionControl,
	UnsupportedLifetime,
	UnsupportedExceptionRegion,
	ExceptionPayloadUnavailable,
	BridgeExceptionAdoptionUnavailable,
	SuspendOrExceptionState,
	InvalidEffectiveReceiver,
	InvalidCallEvaluationSequence,
	InvalidControlTarget,
	InvalidCleanupPlan,
	InvalidCanonicalAST,
	EmitterFailure,
	BackendUnavailable,
};

/** Canonical invocation category consumed by TypedASTJIT planning. */
enum class EAngelscriptTypedASTJITInvocationKind : uint8
{
	Global,
	GlobalInitializer,
	InstanceMethod,
	Constructor,
	Destructor,
	Factory,
	Imported,
	System,
	CallableType,
	Synthesized,
};

/** Effective receiver selected by Canonical Sema. */
enum class EAngelscriptTypedASTJITReceiverKind : uint8
{
	None,
	NativeObjectThis,
	ExternalImplicitThis,
	MixinFirstParameter,
};

/** Exact resolved call route represented by a sealed Canonical call node. */
enum class EAngelscriptTypedASTJITCallTargetKind : uint8
{
	Unspecified,
	ScriptFunction,
	SystemFunction,
	ImportedFunction,
};

/** Canonical lifetime proof summarized for generation diagnostics. */
enum class EAngelscriptTypedASTJITCleanupPlanState : uint8
{
	Unverified,
	VerifiedEmpty,
	NonEmpty,
	PartialConstruction,
	ScriptDestructor,
	CompilerExceptionRegion,
};

/** Typed action families copied from the authenticated Canonical lifetime view. */
enum class EAngelscriptTypedASTJITLifetimeActionFlags : uint32
{
	None = 0,
	DestroyValue = 1u << 0,
	ReleaseReference = 1u << 1,
	CommitDeferredOut = 1u << 2,
	DestroyElement = 1u << 3,
};
ENUM_CLASS_FLAGS(EAngelscriptTypedASTJITLifetimeActionFlags)

/** Typed exit-plan families copied from the authenticated Canonical lifetime view. */
enum class EAngelscriptTypedASTJITLifetimeExitPlanFlags : uint32
{
	None = 0,
	Normal = 1u << 0,
	Transfer = 1u << 1,
	ForEach = 1u << 2,
	FullExpression = 1u << 3,
};
ENUM_CLASS_FLAGS(EAngelscriptTypedASTJITLifetimeExitPlanFlags)

/**
 * Detached, pointer-free authentication summary for one Canonical function.
 *
 * StableKey hashes only stable declaration/type identities and typed protocol
 * ordinals. ABIKey hashes the cleanup ABI requirements. Neither key contains
 * AST pointers, snapshot-local IDs, Engine-local TypeIds, or rendered dumps.
 */
struct FAngelscriptTypedASTJITCanonicalLifetimeSummary
{
	uint32 ProtocolRevision = 0;
	FAngelscriptHash256 StableKey;
	FAngelscriptHash256 ABIKey;
	EAngelscriptTypedASTJITLifetimeActionFlags ActionFlags =
		EAngelscriptTypedASTJITLifetimeActionFlags::None;
	EAngelscriptTypedASTJITLifetimeExitPlanFlags ExitPlanFlags =
		EAngelscriptTypedASTJITLifetimeExitPlanFlags::None;
	uint32 SupportedExitMask = 0;
	uint32 RecordCount = 0;
	uint32 ExitPlanCount = 0;
	bool bNativeObjectFrameABIAvailable = false;

	bool IsAuthenticated() const
	{
		return ProtocolRevision != 0
			&& !StableKey.IsZero()
			&& !ABIKey.IsZero();
	}
};

/**
 * Pointer-free lifetime facts proven from one exact sealed Canonical function.
 *
 * A verified-empty result means that the structural function body contains no
 * cleanup action. It does not infer exception, suspension, or partial-
 * construction behavior from bytecode or from a later runtime artifact.
 */
struct FAngelscriptTypedASTJITCanonicalLifetimeFacts
{
	EAngelscriptTypedASTJITCleanupPlanState CleanupPlanState =
		EAngelscriptTypedASTJITCleanupPlanState::Unverified;
	FAngelscriptTypedASTJITCanonicalLifetimeSummary LifetimeSummary;
	bool bHasSuspendState = false;
	bool bHasExceptionCleanup = false;
	bool bCleanupPlanCoversAllTransfers = false;
};

/** Frozen, pointer-free diagnostic token consumed by dump tooling. */
ANGELSCRIPTRUNTIME_API const TCHAR* LexToString(
	EAngelscriptTypedASTJITFallbackReason Reason);

enum class EAngelscriptTypedASTJITBodyParameterABI : uint8
{
	ScalarValue = 0,
	BorrowedValueAddress,
};

/**
 * Provider-independent, already-resolved scalar entry shape.
 *
 * Production generation freezes the exact C++ body signature from the shared
 * EntryPlan before this value reaches eligibility, dependency analysis, or
 * emission. Provider-independent scalar inspection tests may omit that frozen
 * signature; the no-frozen path never admits managed/reference parameters.
 *
 * This structure intentionally carries no script function pointer, bytecode,
 * provider catalog, UObject, UFunction, or runtime coordinator state.
 */
struct FAngelscriptTypedASTJITFunctionShape
{
	FString SymbolName;
	TArray<asCDataType> ParameterTypes;
	asCDataType ReturnType;
	bool bHasFrozenBodySignature = false;
	FString FrozenBodyReturnCppType;
	TArray<FString> FrozenBodyParameterCppTypes;
	TArray<EAngelscriptTypedASTJITBodyParameterABI> FrozenBodyParameterABIs;
	EAngelscriptTypedASTJITInvocationKind InvocationKind =
		EAngelscriptTypedASTJITInvocationKind::Global;
	EAngelscriptTypedASTJITReceiverKind ReceiverKind =
		EAngelscriptTypedASTJITReceiverKind::None;
	int32 ReceiverParameterIndex = INDEX_NONE;
	bool bHasNativeObjectSlot = false;
	bool bReturnsOnStack = false;
};

enum class EAngelscriptTypedASTJITCallEmissionRoute : uint8
{
	Direct = 0,
	Bridge,
	CurrentNativeBinding,
};

/** Exact, already-validated native-call decision consumed by pure C++ emit. */
struct FAngelscriptTypedASTJITNativeCallEmissionPlan
{
	asASTExprId Expression;
	int32 EngineLocalTargetFunctionId = INDEX_NONE;
	EAngelscriptTypedASTJITCallEmissionRoute Route =
		EAngelscriptTypedASTJITCallEmissionRoute::Direct;
	EAngelscriptJITNativeCallSiteRoute NativeCallSiteRoute =
		EAngelscriptJITNativeCallSiteRoute::DirectExported;
	FString CanonicalDeclaration;
	FString RegisteredTarget;
	/**
	 * Same-compilation stable target selected from the authoritative compiler
	 * dependency/reference graph. Direct and bound routes retain this value even
	 * when only the latter consumes a runtime reference slot.
	 */
	FAngelscriptArtifactReference TargetReference;

	// Direct-route metadata. The emitted expression names Symbol and includes
	// Include, and consumes the exact structurally validated C++ callable type;
	// none of these fields is consulted by a current-Engine bound route.
	FString Symbol;
	FString Include;
	FString ReviewedCppReturnType;
	TArray<FString> ReviewedCppParameterTypes;

	// Current-Engine bound metadata. Dispatch is the pre-adopted reference slot;
	// the declaration and registered target are diagnostic text only.
	FString CallSiteSymbol;
	FAngelscriptStableFunctionKey StableTargetKey;
	FAngelscriptHash256 ExpectedABI;
	int32 ReferenceSlotIndex = INDEX_NONE;
};

/** Exact direct script-helper edge consumed by the pure C++ emitter. */
struct FAngelscriptTypedASTJITScriptCallEmissionPlan
{
	asASTExprId Expression;
	int32 EngineLocalTargetFunctionId = INDEX_NONE;
	FString Symbol;
	/** Exact declaration reference reconciled against compiler dependencies. */
	FAngelscriptArtifactReference TargetReference;
	/** Directly emitted helper bodies additionally depend on callee content. */
	bool bRequiresFunctionContent = false;
};

/** Exact owned call-site facts observed while emitting one Canonical expression. */
struct FAngelscriptTypedASTJITNativeCallSiteMetadata
{
	asASTExprId Expression;
	EAngelscriptJITNativeCallSiteRoute Route =
		EAngelscriptJITNativeCallSiteRoute::Invalid;
	FString CanonicalDeclaration;
	FString EmittedCppCallee;
	FString RuntimeDllCore;
	FString MetadataRowSymbol;
	FString RegisteredTarget;
	FAngelscriptHash256 StableTargetKey;
	FAngelscriptHash256 ExpectedAbi;
	int32 ReferenceSlotIndex = INDEX_NONE;
};

/**
 * Assigns deterministic readable identifiers to all bridge call-site rows.
 * The target suffix starts at eight hex digits and expands only for colliding
 * readable stems. The complete owning-root key scopes the C++ identifier so
 * independent root closures remain unique when one AS module is concatenated
 * into one .jit.cpp. Repeated uses of one exact target inside that root still
 * share one identifier and therefore one immutable metadata row.
 */
ANGELSCRIPTRUNTIME_API bool
AssignAngelscriptTypedASTJITBridgeCallSiteSymbols(
	TArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan> NativeCalls,
	FString* OutError = nullptr);

ANGELSCRIPTRUNTIME_API bool
AssignAngelscriptTypedASTJITBridgeCallSiteSymbols(
	TArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan> NativeCalls,
	const FAngelscriptStableFunctionKey& OwningRootFunctionKey,
	FString* OutError = nullptr);

struct FAngelscriptTypedASTJITEmitOptions
{
	TConstArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan> NativeCalls;
	TConstArrayView<FAngelscriptTypedASTJITScriptCallEmissionPlan> ScriptCalls;
	TConstArrayView<FAngelscriptTypedASTJITGlobalDependency> GlobalDependencies;
	/**
	 * Explicit immutable profile for generated runtime hooks. A null profile is
	 * permitted only for standalone pure-body inspection and emits no hooks.
	 */
	const FAngelscriptTypedASTJITGenerationCapabilityProfile*
		InstrumentationProfile = nullptr;
	// Standalone body emission owns its bridge rows by default. A production
	// root closure emits the de-duplicated rows once before all member bodies.
	bool bEmitBridgeCallSiteRows = true;
	bool bEmitTestProbe = false;
	bool bEmitNormalizedComments = false;
};

struct FAngelscriptTypedASTJITAnalysis
{
	bool bEligible = false;
	EAngelscriptTypedASTJITFallbackReason FailureReason =
		EAngelscriptTypedASTJITFallbackReason::None;
	asSSourceSpan FailureSpan;
	FString FailureDetail;
};

struct FAngelscriptTypedASTJITEmission
{
	bool bSuccess = false;
	bool bRequiresExecutionState = false;
	bool bRequiresInstrumentationFrame = false;
	FAngelscriptHash256 InstrumentationProfileIdentityHash;
	FString Declaration;
	FString Definition;
	TArray<FString> RequiredIncludes;
	TArray<FAngelscriptTypedASTJITNativeCallSiteMetadata> NativeCallSites;
	EAngelscriptTypedASTJITFallbackReason FailureReason =
		EAngelscriptTypedASTJITFallbackReason::None;
	asSSourceSpan FailureSpan;
	FString FailureDetail;
};
