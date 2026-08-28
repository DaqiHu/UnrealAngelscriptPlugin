#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallTarget.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEligibility.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITExecutionProfile.h"

#if AS_CAN_GENERATE_JIT

enum class EAngelscriptTypedASTJITCallDisposition : uint8
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

enum class EAngelscriptTypedASTJITClosureFunctionDisposition : uint8
{
	Root = 0,
	DirectScript,
	InternalSemanticHelper,
	Bridge,
	Unsupported,
};

/** One normalized same-generation function consumed by the pure planner. */
struct ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITCallClosureFunctionInput
{
	int32 EngineLocalFunctionId = INDEX_NONE;
	FAngelscriptStableFunctionKey FunctionKey;
	FAngelscriptTypedASTJITEligibilityInput Eligibility;
	bool bIsUFunctionRoot = false;
	bool bScalarBridgeAvailable = false;
	FAngelscriptTypedASTJITGenerationCapabilityProfile CapabilityProfile;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITCallClosureOptions
{
	FAngelscriptTypedASTJITExecutionRequirementsSnapshot
		ExecutionRequirements;
	TConstArrayView<FAngelscriptStaticJITNativeCallTarget> NativeCallTargets;
	TSet<FName> KnownNativeCallModules;
	TSet<FName> PublicNativeCallDependencyModules;
	bool bNativeScalarBridgeAvailable = false;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITCallClosureFunction
{
	FAngelscriptStableFunctionKey FunctionKey;
	EAngelscriptTypedASTJITClosureFunctionDisposition Disposition =
		EAngelscriptTypedASTJITClosureFunctionDisposition::Unsupported;
	FAngelscriptTypedASTJITEligibility Eligibility;
	int32 ComponentIndex = INDEX_NONE;
	bool bRecursiveComponent = false;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITCallClosureCall
{
	FAngelscriptStableFunctionKey CallerFunctionKey;
	FAngelscriptStableFunctionKey CalleeFunctionKey;
	/** Exact call coordinate in the sealed canonical AST. */
	asASTExprId Expression;
	asSSourceSpan SourceSpan;
	EAngelscriptTypedASTJITCallTargetKind TargetKind =
		EAngelscriptTypedASTJITCallTargetKind::Unspecified;
	int32 EngineLocalTargetFunctionId = INDEX_NONE;
	EAngelscriptTypedASTJITCallDisposition Disposition =
		EAngelscriptTypedASTJITCallDisposition::Unsupported;
	FAngelscriptTypedASTJITEligibility CalleeEligibility;
	FAngelscriptStaticJITNativeCallDescriptor ExternalCallDescriptor;
	FAngelscriptStaticJITNativeCallValidation ExternalCallValidation;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITCallClosureComponent
{
	TArray<FAngelscriptStableFunctionKey> Members;
	bool bRecursive = false;
};

/** Pointer-free deterministic plan for one selected UFUNCTION root. */
struct ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITCallClosurePlan
{
	bool bEligible = false;
	FAngelscriptStableFunctionKey RootFunctionKey;
	FAngelscriptTypedASTJITEligibility RootEligibility;
	EAngelscriptTypedASTJITDirectCapability RequiredDirectCapabilities =
		EAngelscriptTypedASTJITDirectCapability::None;
	EAngelscriptTypedASTJITDirectCapability AvailableDirectCapabilities =
		EAngelscriptTypedASTJITDirectCapability::None;
	FAngelscriptHash256 ExecutionRequirementsIdentityHash;
	FAngelscriptHash256 AvailableCapabilityProfileIdentityHash;
	TArray<FAngelscriptTypedASTJITCallClosureFunction> Functions;
	TArray<FAngelscriptTypedASTJITCallClosureCall> Calls;
	TArray<FAngelscriptTypedASTJITCallClosureComponent> Components;

	const FAngelscriptTypedASTJITCallClosureFunction* FindFunction(
		const FAngelscriptStableFunctionKey& FunctionKey) const;
	FString DumpNormalized() const;
};

/**
 * Normalizes the frozen task graph into exact function-id/stable-key facts.
 * No returned structure owns or persists an Engine-local pointer.
 */
ANGELSCRIPTRUNTIME_API bool BuildAngelscriptTypedASTJITCallClosureInputs(
	const FAngelscriptStaticJITCompiledSourceGraph& Graph,
	const FAngelscriptTypedASTJITRootIndex& RootIndex,
	TArray<FAngelscriptTypedASTJITCallClosureFunctionInput>& OutFunctions,
	FString* OutError = nullptr);

/**
 * Builds one root's complete direct script-helper closure. Semantic fallback
 * is a successful plan with bEligible=false; malformed task coordinates fail
 * the call and return no partial plan.
 */
ANGELSCRIPTRUNTIME_API bool BuildAngelscriptTypedASTJITCallClosure(
	TConstArrayView<FAngelscriptTypedASTJITCallClosureFunctionInput> Functions,
	const FAngelscriptStableFunctionKey& RootFunctionKey,
	const FAngelscriptTypedASTJITCallClosureOptions& Options,
	FAngelscriptTypedASTJITCallClosurePlan& OutPlan,
	FString* OutError = nullptr);

#endif // AS_CAN_GENERATE_JIT
