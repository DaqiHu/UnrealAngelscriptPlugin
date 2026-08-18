#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/AngelscriptStaticJITCaptureProfile.h"
#include "StaticJIT/AngelscriptStaticJITEntryPlan.h"
#include "StaticJIT/AngelscriptStaticJITFunctionMetadata.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallTarget.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITExecutionProfile.h"

#include "StaticJIT/AngelscriptJITGeneration.h"

#if AS_CAN_GENERATE_JIT

class asIScriptFunction;
class asIScriptModule;
class asITypeInfo;
class asCTypedSemanticFunction;
struct FAngelscriptModuleDesc;

/**
 * Stable identifier for an internal, build-time StaticJIT backend.
 *
 * This is deliberately a different C++ type and namespace from the exported
 * Runtime JIT BackendId. Static backends are Runtime-module-owned and are not
 * a third-party binary interface.
 */
struct FAngelscriptStaticJITBackendId
{
public:
	FAngelscriptStaticJITBackendId() = default;

	static FAngelscriptStaticJITBackendId Bytecode()
	{
		return FAngelscriptStaticJITBackendId(TEXT("bytecode"));
	}

	static FAngelscriptStaticJITBackendId TypedAST()
	{
		return FAngelscriptStaticJITBackendId(TEXT("typed-ast"));
	}

	static bool TryParse(
		const FStringView Value,
		FAngelscriptStaticJITBackendId& OutBackendId)
	{
		OutBackendId = {};
		if (Value.Equals(TEXT("bytecode"), ESearchCase::CaseSensitive))
		{
			OutBackendId = Bytecode();
			return true;
		}
		if (Value.Equals(TEXT("typed-ast"), ESearchCase::CaseSensitive))
		{
			OutBackendId = TypedAST();
			return true;
		}
		return false;
	}

	bool IsValid() const
	{
		return Value == TEXT("bytecode") || Value == TEXT("typed-ast");
	}

	const FString& ToString() const
	{
		return Value;
	}

	friend bool operator==(
		const FAngelscriptStaticJITBackendId& Left,
		const FAngelscriptStaticJITBackendId& Right)
	{
		return Left.Value == Right.Value;
	}

	friend bool operator!=(
		const FAngelscriptStaticJITBackendId& Left,
		const FAngelscriptStaticJITBackendId& Right)
	{
		return !(Left == Right);
	}

	friend bool operator<(
		const FAngelscriptStaticJITBackendId& Left,
		const FAngelscriptStaticJITBackendId& Right)
	{
		return Left.Value < Right.Value;
	}

private:
	explicit FAngelscriptStaticJITBackendId(const TCHAR* InValue)
		: Value(InValue)
	{
	}

	FString Value;
};

/** One function visible to a synchronous Static generation task. */
struct FAngelscriptStaticJITCompiledFunctionView
{
	asIScriptFunction* EngineLocalFunction = nullptr;
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptStableFunctionKey FunctionKey;
	FAngelscriptHash256 ExecutionHash;
	FAngelscriptHash256 DebugHash;
	FAngelscriptHash256 EntryAbiHash;
	FStringView CanonicalDeclaration;
	TConstArrayView<uint8> Bytecode;
	const asCTypedSemanticFunction* VerifiedTypedHIR = nullptr;
	bool bHasVerifiedArtifactIdentity = false;
	bool bHasVerifiedArtifactReferences = false;
	bool bHasVMEntry = false;
	bool bHasRawEntry = false;
	bool bHasParmsEntry = false;
	EAngelscriptStaticJITLiteralAssetFunctionRole LiteralAssetRole =
		EAngelscriptStaticJITLiteralAssetFunctionRole::None;
	/** Exact pointer-free plan owned by the synchronous generation snapshot. */
	const FStaticJITEntryPlan* EntryPlan = nullptr;
};

/** One type visible to task-wide analysis. EngineLocalType dies with the task. */
struct FAngelscriptStaticJITCompiledTypeView
{
	asITypeInfo* EngineLocalType = nullptr;
	FAngelscriptHash256 StableTypeKey;
	FStringView CanonicalDeclaration;
	bool bTargetMaterializesScriptReflection = false;
};

/** One global visible to task-wide analysis. */
struct FAngelscriptStaticJITCompiledGlobalView
{
	const void* EngineLocalGlobal = nullptr;
	int32 EngineLocalGlobalPropertyId = INDEX_NONE;
	FAngelscriptHash256 StableGlobalKey;
	FStringView CanonicalDeclaration;
};

enum class EAngelscriptStaticJITDescriptorKind : uint8
{
	Class = 0,
	Struct,
	Enum,
	Delegate,
	Property,
	Function,
};

/** Pointer-free descriptor facts plus an optional synchronous local handle. */
struct FAngelscriptStaticJITDescriptorView
{
	const void* EngineLocalDescriptor = nullptr;
	FAngelscriptHash256 StableDescriptorKey;
	FAngelscriptHash256 EntryPlanHash;
	FStringView CanonicalDeclaration;
	EAngelscriptStaticJITDescriptorKind Kind =
		EAngelscriptStaticJITDescriptorKind::Class;
	FStringView CanonicalName;
	FStringView ReceiverCanonicalName;
	bool bUFunctionRoot = false;
	bool bHasVMEntry = false;
	bool bHasRawEntry = false;
	bool bHasParmsEntry = false;
	/** Descriptor-specific overlay, including reflected generated parameters. */
	const FStaticJITEntryPlan* EntryPlan = nullptr;
};

/** Stable dependency or external native-call fact used by a backend. */
struct FAngelscriptStaticJITDependencyView
{
	FAngelscriptStableFunctionKey OwnerFunctionKey;
	FAngelscriptArtifactReference Reference;
	FStringView CanonicalDeclaration;
};

/** Typed semantic dependency with an optional current-Engine global coordinate. */
struct FAngelscriptStaticJITSemanticDependencyView
{
	FAngelscriptStableFunctionKey OwnerFunctionKey;
	EAngelscriptArtifactSemanticDependencyKind Kind =
		EAngelscriptArtifactSemanticDependencyKind::Invalid;
	FAngelscriptArtifactReference Reference;
	TOptional<FAngelscriptHash256> ExpectedContentOrValue;
	int32 EngineLocalGlobalPropertyId = INDEX_NONE;
};

/**
 * Complete Provider-domain graph available to a backend before any function
 * is emitted. Engine-local pointers are synchronous views only; backend
 * output must contain stable, pointer-free values.
 */
struct FAngelscriptStaticJITCompiledSourceGraph
{
	FAngelscriptJITProviderGenerationSettings Settings;
	/** Typed-only internal profile; not a ProviderId or Cache ArtifactProfile. */
	FAngelscriptTypedASTJITGenerationCapabilityProfile
		TypedASTJITCapabilityProfile;
	/** Frozen requirements for the exact generation/selection decision. */
	FAngelscriptTypedASTJITExecutionRequirementsSnapshot
		TypedASTJITExecutionRequirements;
	TConstArrayView<asIScriptModule*> Modules;
	/**
	 * Exact descriptor roots that produced Modules. Valid only for this
	 * synchronous generation task and never retained by backend output.
	 */
	TConstArrayView<const FAngelscriptModuleDesc*>
		ResolvedModuleDescriptors;
	TConstArrayView<FAngelscriptStaticJITCompiledFunctionView> Functions;
	TConstArrayView<FAngelscriptStaticJITCompiledTypeView> Types;
	TConstArrayView<FAngelscriptStaticJITCompiledGlobalView> Globals;
	TConstArrayView<FAngelscriptStaticJITDescriptorView> Descriptors;
	TConstArrayView<FAngelscriptStaticJITDependencyView> Dependencies;
	TConstArrayView<FAngelscriptStaticJITSemanticDependencyView>
		SemanticDependencies;
	TConstArrayView<FAngelscriptStaticJITDependencyView> ExternalNativeCalls;
	TConstArrayView<FAngelscriptStaticJITNativeCallTarget> NativeCallTargets;
	uint32 SourceCompilePassCount = 0;
	EAngelscriptStaticJITCaptureProfile CaptureProfile =
		EAngelscriptStaticJITCaptureProfile::Bytecode;
};

/** Immutable task-wide input; EmitModuleSet restricts output, not analysis. */
struct FAngelscriptStaticJITBackendRequest
{
	FAngelscriptStaticJITBackendId RequestedBackendId;
	const FAngelscriptStaticJITCompiledSourceGraph& CompiledSourceGraph;
	TConstArrayView<FAngelscriptStableModuleKey> EmitModuleSet;
};

enum class EAngelscriptStaticJITFunctionDisposition : uint8
{
	Emitted = 0,
	Unsupported,
	Failed,
};

/** Owned processed-source coordinate; no parser/HIR pointer crosses the task. */
struct FAngelscriptStaticJITProcessedSource
{
	FString Section;
	uint32 Row = 0;
	uint32 Column = 0;
};

enum class EAngelscriptStaticJITTypedHIRState : uint8
{
	Missing = 0,
	Verified,
	Invalid,
};

enum class EAngelscriptStaticJITTypedClosureFunctionRole : uint8
{
	NonRoot = 0,
	UFunctionRoot,
	DirectScript,
	InternalSemanticHelper,
	Bridge,
	Unsupported,
};

enum class EAngelscriptStaticJITTypedExecutionProfileKind : uint8
{
	Unavailable = 0,
	PositionOnly,
	Instrumented,
};

enum class EAngelscriptStaticJITExceptionControlState : uint8
{
	Unavailable = 0,
	NotRequired,
	DirectExecutionState,
};

enum class EAngelscriptStaticJITExceptionPayloadState : uint8
{
	Unavailable = 0,
	NotRequired,
	DirectFailureRecord,
	NestedBridgeAdoption,
};

enum class EAngelscriptStaticJITTypedCallDisposition : uint8
{
	DirectScript = 0,
	InternalSemanticHelper,
	DirectExported,
	DirectInline,
	RuntimeThunk,
	CurrentNativeBinding,
	Bridge,
	Unsupported,
};

/** Pointer-free plan plus actual emitted spelling for one Typed call edge. */
struct FAngelscriptStaticJITTypedCallDiagnostic
{
	FAngelscriptStableFunctionKey CallerFunctionKey;
	FAngelscriptStableFunctionKey CalleeFunctionKey;
	uint32 ExpressionIndex = MAX_uint32;
	FAngelscriptStaticJITProcessedSource ProcessedSource;
	TOptional<FAngelscriptStaticJITProcessedSource> AuthoredSource;
	TOptional<FAngelscriptStaticJITProcessedSource> GeneratedSource;
	asETypedSemanticCallTargetKind TargetKind =
		asETypedSemanticCallTargetKind::Unspecified;
	EAngelscriptStaticJITTypedCallDisposition Disposition =
		EAngelscriptStaticJITTypedCallDisposition::Unsupported;
	EAngelscriptTypedASTJITFallbackReason CalleeEligibilityReason =
		EAngelscriptTypedASTJITFallbackReason::None;
	FString CalleeEligibilityDetail;
	FAngelscriptStaticJITProcessedSource CalleeEligibilitySource;
	bool bHasNativeLinkageValidation = false;
	EAngelscriptStaticJITNativeCallValidationCode NativeLinkageValidation =
		EAngelscriptStaticJITNativeCallValidationCode::Absent;
	FString NativeLinkageValidationDetail;
	FString CanonicalDeclaration;
	EAngelscriptJITNativeCallSiteRoute EmittedRoute =
		EAngelscriptJITNativeCallSiteRoute::Invalid;
	FString EmittedCppCallee;
	FString RuntimeDllCore;
	FString MetadataRowSymbol;
	FString RegisteredTarget;
	FString DirectCppSymbol;
	FAngelscriptHash256 StableTargetKey;
	FAngelscriptHash256 ExpectedAbi;
	int32 ReferenceSlotIndex = INDEX_NONE;
};

/** Owned semantic facts copied while the generation HIR is still alive. */
struct FAngelscriptStaticJITTypedSemanticFunctionDiagnostic
{
	FAngelscriptStableFunctionKey FunctionKey;
	EAngelscriptStaticJITTypedClosureFunctionRole Role =
		EAngelscriptStaticJITTypedClosureFunctionRole::NonRoot;
	uint32 RawTraitBits = 0;
	uint32 UnknownTraitBits = 0;
	asETypedSemanticInvocationKind InvocationKind =
		asETypedSemanticInvocationKind::Global;
	asETypedSemanticReceiverKind ReceiverKind =
		asETypedSemanticReceiverKind::None;
	int32 ReceiverParameterIndex = INDEX_NONE;
	EAngelscriptStaticJITTypedHIRState HIRState =
		EAngelscriptStaticJITTypedHIRState::Missing;
	asETypedSemanticCleanupPlanState CleanupPlanState =
		asETypedSemanticCleanupPlanState::Unverified;
	bool bHasSuspendState = false;
	bool bHasExceptionCleanup = false;
	bool bCleanupPlanCoversAllTransfers = false;
	int32 ComponentIndex = INDEX_NONE;
	bool bRecursiveComponent = false;
	EAngelscriptTypedASTJITFallbackReason EligibilityReason =
		EAngelscriptTypedASTJITFallbackReason::None;
	FString EligibilityDetail;
	FAngelscriptStaticJITProcessedSource EligibilitySource;
};

/** Complete pointer-free root/helper closure facts for one Typed attempt. */
struct FAngelscriptStaticJITTypedCallClosureDiagnostic
{
	bool bEligible = false;
	EAngelscriptTypedASTJITDirectCapability RequiredCapabilities =
		EAngelscriptTypedASTJITDirectCapability::None;
	EAngelscriptTypedASTJITDirectCapability AvailableCapabilities =
		EAngelscriptTypedASTJITDirectCapability::None;
	FAngelscriptHash256 ExecutionRequirementsIdentityHash;
	FAngelscriptHash256 AvailableCapabilityProfileIdentityHash;
	EAngelscriptStaticJITTypedExecutionProfileKind ProfileKind =
		EAngelscriptStaticJITTypedExecutionProfileKind::Unavailable;
	EAngelscriptStaticJITExceptionControlState ExceptionControlState =
		EAngelscriptStaticJITExceptionControlState::Unavailable;
	EAngelscriptStaticJITExceptionPayloadState ExceptionPayloadState =
		EAngelscriptStaticJITExceptionPayloadState::Unavailable;
	bool bRecursionGuardAvailable = false;
	TArray<FAngelscriptStaticJITTypedSemanticFunctionDiagnostic> Functions;
	TArray<FAngelscriptStaticJITTypedCallDiagnostic> Calls;
};

/** Structured TypedAST selection/fallback facts owned by backend output. */
struct FAngelscriptStaticJITTypedFunctionDiagnostic
{
	EAngelscriptTypedASTJITFallbackReason FallbackReason =
		EAngelscriptTypedASTJITFallbackReason::None;
	FString Detail;
	FAngelscriptStaticJITProcessedSource ProcessedSource;
	TOptional<FAngelscriptStaticJITTypedSemanticFunctionDiagnostic>
		SemanticFunction;
	TOptional<FAngelscriptStaticJITTypedCallClosureDiagnostic> CallClosure;
};

/** Generator-owned pointer-free provenance for one backend attempt. */
struct FAngelscriptStaticJITBackendAttemptDiagnostic
{
	FAngelscriptStaticJITBackendId BackendId;
	EAngelscriptStaticJITFunctionDisposition Disposition =
		EAngelscriptStaticJITFunctionDisposition::Unsupported;
	FString Detail;
	TOptional<FAngelscriptStaticJITTypedFunctionDiagnostic> TypedDiagnostic;
};

/** Pointer-free result for one stable function identity. */
struct FAngelscriptStaticJITFunctionResult
{
	EAngelscriptStaticJITFunctionDisposition Disposition =
		EAngelscriptStaticJITFunctionDisposition::Unsupported;
	FAngelscriptStaticJITBackendId ActualBackendId;
	FAngelscriptStableFunctionKey FunctionKey;
	FString Detail;
	TOptional<FAngelscriptJITGenerationFunction> EmittedFunction;
	TArray<FAngelscriptStaticJITBackendAttemptDiagnostic> BackendAttempts;
	TOptional<FAngelscriptStaticJITTypedFunctionDiagnostic> TypedDiagnostic;
};

enum class EAngelscriptStaticJITTaskDisposition : uint8
{
	Succeeded = 0,
	FatalError,
};

/**
 * Backend-neutral task result consumed by the generator and deterministic
 * Provider packager. Provenance is diagnostic metadata, never Provider ID.
 */
struct FAngelscriptStaticJITBackendOutput
{
	EAngelscriptStaticJITTaskDisposition Disposition =
		EAngelscriptStaticJITTaskDisposition::Succeeded;
	FString Detail;
	TArray<FAngelscriptJITGenerationModule> Modules;
	TArray<FAngelscriptStaticJITFunctionResult> Functions;
	FString ModuleSourcePreamble;
	FString ModuleSourceFooter;
	FString ProviderSourcePreamble;
};

/** One synchronous backend instance created for exactly one generation task. */
class IAngelscriptStaticJITBackend
{
public:
	virtual ~IAngelscriptStaticJITBackend() = default;

	virtual bool Generate(
		const FAngelscriptStaticJITBackendRequest& Request,
		FAngelscriptStaticJITBackendOutput& OutOutput,
		FString* OutError) = 0;
};

/** Internal factory; explicit module registration avoids constructor order. */
class IAngelscriptStaticJITBackendFactory
{
public:
	virtual ~IAngelscriptStaticJITBackendFactory() = default;

	virtual FAngelscriptStaticJITBackendId GetBackendId() const = 0;
	virtual TUniquePtr<IAngelscriptStaticJITBackend> CreateBackend() const = 0;
};

enum class EAngelscriptStaticJITBackendRegistrationCode : uint8
{
	Registered = 0,
	InvalidBackendId,
	DuplicateBackendId,
};

struct FAngelscriptStaticJITBackendRegistrationResult
{
	EAngelscriptStaticJITBackendRegistrationCode Code =
		EAngelscriptStaticJITBackendRegistrationCode::InvalidBackendId;
	FAngelscriptStaticJITBackendId BackendId;

	bool IsAccepted() const
	{
		return Code ==
			EAngelscriptStaticJITBackendRegistrationCode::Registered;
	}
};

/** Small, task-orchestrator-owned registry with deterministic ID lookup. */
class FAngelscriptStaticJITBackendRegistry
{
public:
	FAngelscriptStaticJITBackendRegistrationResult RegisterFactory(
		const IAngelscriptStaticJITBackendFactory& Factory)
	{
		const FAngelscriptStaticJITBackendId BackendId =
			Factory.GetBackendId();
		if (!BackendId.IsValid())
		{
			return {
				EAngelscriptStaticJITBackendRegistrationCode::InvalidBackendId,
				BackendId};
		}
		if (FindFactory(BackendId) != nullptr)
		{
			return {
				EAngelscriptStaticJITBackendRegistrationCode::DuplicateBackendId,
				BackendId};
		}

		Factories.Add(&Factory);
		Factories.Sort([](
			const IAngelscriptStaticJITBackendFactory& Left,
			const IAngelscriptStaticJITBackendFactory& Right)
		{
			return Left.GetBackendId() < Right.GetBackendId();
		});
		return {
			EAngelscriptStaticJITBackendRegistrationCode::Registered,
			BackendId};
	}

	const IAngelscriptStaticJITBackendFactory* FindFactory(
		const FAngelscriptStaticJITBackendId& BackendId) const
	{
		if (!BackendId.IsValid())
		{
			return nullptr;
		}
		const IAngelscriptStaticJITBackendFactory* const* Found =
			Factories.FindByPredicate(
				[&BackendId](
					const IAngelscriptStaticJITBackendFactory* Candidate)
				{
					return Candidate != nullptr
						&& Candidate->GetBackendId() == BackendId;
				});
		return Found != nullptr ? *Found : nullptr;
	}

	TArray<FAngelscriptStaticJITBackendId> GetRegisteredBackendIds() const
	{
		TArray<FAngelscriptStaticJITBackendId> Result;
		Result.Reserve(Factories.Num());
		for (const IAngelscriptStaticJITBackendFactory* Factory : Factories)
		{
			if (Factory != nullptr)
			{
				Result.Add(Factory->GetBackendId());
			}
		}
		return Result;
	}

private:
	TArray<const IAngelscriptStaticJITBackendFactory*> Factories;
};

#endif
