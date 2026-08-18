#pragma once

#include "CoreMinimal.h"
#include "Core/Artifacts/AngelscriptArtifactReference.h"
#include "StaticJIT/AngelscriptJITProvider.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

struct FAngelscriptTypedASTJITGenerationCapabilityProfile;

/** Exact task-local reconciliation of one HIR global coordinate to stable authority. */
struct FAngelscriptTypedASTJITGlobalDependency
{
	int32 EngineLocalGlobalPropertyId = INDEX_NONE;
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
	MissingTypedHIR,
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
	InvalidTypedHIR,
	EmitterFailure,
	BackendUnavailable,
};

/** Frozen, pointer-free diagnostic token consumed by dump tooling. */
ANGELSCRIPTRUNTIME_API const TCHAR* LexToString(
	EAngelscriptTypedASTJITFallbackReason Reason);

/**
 * Provider-independent, already-resolved scalar entry shape.
 *
 * This structure intentionally carries no script function pointer, bytecode,
 * provider catalog, UObject, UFunction, or runtime coordinator state.
 */
struct FAngelscriptTypedASTJITFunctionShape
{
	FString SymbolName;
	TArray<asCDataType> ParameterTypes;
	asCDataType ReturnType;
	asETypedSemanticInvocationKind InvocationKind =
		asETypedSemanticInvocationKind::Global;
	asETypedSemanticReceiverKind ReceiverKind =
		asETypedSemanticReceiverKind::None;
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
	asTypedSemanticExpressionId Expression;
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
	// Include; neither field is consulted by a current-Engine bound route.
	FString Symbol;
	FString Include;

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
	asTypedSemanticExpressionId Expression;
	int32 EngineLocalTargetFunctionId = INDEX_NONE;
	FString Symbol;
	/** Exact declaration reference reconciled against compiler dependencies. */
	FAngelscriptArtifactReference TargetReference;
	/** Directly emitted helper bodies additionally depend on callee content. */
	bool bRequiresFunctionContent = false;
};

/** Exact owned call-site facts observed while emitting one HIR expression. */
struct FAngelscriptTypedASTJITNativeCallSiteMetadata
{
	asTypedSemanticExpressionId Expression;
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
	asSTypedSemanticSourceSpan FailureSpan;
	FString FailureDetail;
	TArray<asTypedSemanticSymbolId> OrderedSymbols;
	TArray<asTypedSemanticStatementId> OrderedStatements;
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
	asSTypedSemanticSourceSpan FailureSpan;
	FString FailureDetail;
};
