#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/Backends/AngelscriptStaticJITBackend.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITModel.h"

#if AS_CAN_GENERATE_JIT

struct FAngelscriptClassDesc;
struct FAngelscriptFunctionDesc;

enum class EAngelscriptTypedASTJITUFunctionFlags : uint8
{
	None = 0,
	RpcOrNet = 1 << 0,
	BlueprintEvent = 1 << 1,
	BlueprintOverride = 1 << 2,
	ThreadSafeSpecial = 1 << 3,
	WorldContextGenerated = 1 << 4,
};
ENUM_CLASS_FLAGS(EAngelscriptTypedASTJITUFunctionFlags);

enum class EAngelscriptTypedASTJITSignatureFlags : uint8
{
	None = 0,
	Reference = 1 << 0,
	OutParameter = 1 << 1,
	ReturnOnStack = 1 << 2,
	Object = 1 << 3,
	Struct = 1 << 4,
	Container = 1 << 5,
	Delegate = 1 << 6,
};
ENUM_CLASS_FLAGS(EAngelscriptTypedASTJITSignatureFlags);

enum class EAngelscriptTypedASTJITEligibilityPurpose : uint8
{
	StandaloneRoot = 0,
	RootCallClosure,
	InternalHelperCallClosure,
};

/**
 * Task-local normalized facts consumed synchronously by the deterministic
 * production evaluator. The sealed canonical AST is the only body-semantic
 * input; the returned diagnostic is pointer-free and may cross the backend
 * output boundary.
 */
struct ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITEligibilityInput
{
	bool bIsUFunctionRoot = false;
	EAngelscriptTypedASTJITEligibilityPurpose Purpose =
		EAngelscriptTypedASTJITEligibilityPurpose::StandaloneRoot;
	const class asCASTContext* SealedAST = nullptr;
	asASTDeclId CanonicalFunctionDecl;
	FAngelscriptTypedASTJITFunctionShape Shape;
	asDWORD RawTraitBits = 0;
	bool bVirtualOrNonFinal = false;
	EAngelscriptTypedASTJITUFunctionFlags UFunctionFlags =
		EAngelscriptTypedASTJITUFunctionFlags::None;
	EAngelscriptTypedASTJITSignatureFlags SignatureFlags =
		EAngelscriptTypedASTJITSignatureFlags::None;
	TArray<FAngelscriptTypedASTJITGlobalDependency> GlobalDependencies;
	FString CapabilityDetail;
};

/** Stable eligibility/fallback diagnostic ready for commandlet/console dumps. */
struct ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITEligibility
{
	bool bEligible = false;
	EAngelscriptTypedASTJITFallbackReason Reason =
		EAngelscriptTypedASTJITFallbackReason::None;
	asSSourceSpan SourceSpan;
	FString Detail;

	FString ToDiagnosticString() const;
};

/**
 * One final Unreal-facing function root selected from the resolved descriptor
 * graph. Engine/descriptor pointers are synchronous generation-task handles;
 * only the stable keys may cross the backend output boundary.
 */
struct ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITRoot
{
	asIScriptFunction* EngineLocalFunction = nullptr;
	const FAngelscriptClassDesc* ClassDescriptor = nullptr;
	const FAngelscriptFunctionDesc* FunctionDescriptor = nullptr;
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptStableFunctionKey FunctionKey;
	FAngelscriptHash256 StableDescriptorKey;
	const FStaticJITEntryPlan* EntryPlan = nullptr;
	bool bGeneratedStaticsClassSurface = false;
};

/** Deterministically ordered, task-local UFUNCTION root index. */
struct ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITRootIndex
{
	TArray<FAngelscriptTypedASTJITRoot> Roots;

	const FAngelscriptTypedASTJITRoot* FindByFunction(
		const asIScriptFunction* Function) const;
};

/**
 * Builds the final TypedASTJIT root set by walking the resolved
 * ModuleDesc/ClassDesc::Methods graph and exact-matching each
 * FunctionDesc->ScriptFunction against the complete compiled function view.
 * Names and declaration text are never function identity.
 */
ANGELSCRIPTRUNTIME_API bool BuildAngelscriptTypedASTJITRootIndex(
	const FAngelscriptStaticJITCompiledSourceGraph& Graph,
	FAngelscriptTypedASTJITRootIndex& OutIndex,
	FString* OutError = nullptr);

/** Pure deterministic evaluator used by production and exhaustive tests. */
ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITEligibility
EvaluateAngelscriptTypedASTJITEligibility(
	const FAngelscriptTypedASTJITEligibilityInput& Input);

/** Normalizes the exact Engine-local root/function into task-local facts. */
ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITEligibilityInput
BuildAngelscriptTypedASTJITEligibilityInput(
	const FAngelscriptTypedASTJITRoot* Root,
	const FAngelscriptStaticJITCompiledFunctionView& Function);

#endif // AS_CAN_GENERATE_JIT
