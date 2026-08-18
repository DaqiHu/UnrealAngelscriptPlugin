#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/AngelscriptStaticJITEntryPlan.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEmitter.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITExecutionProfile.h"

class asCScriptFunction;

using EAngelscriptTypedASTJITProviderParameterMarshalling =
	EAngelscriptStaticJITEntryParameterMarshalling;
using EAngelscriptTypedASTJITReflectedParmsLayout =
	EAngelscriptStaticJITReflectedParmsLayout;
using FAngelscriptTypedASTJITProviderParameterPlan =
	FStaticJITEntryParameterPlan;

/**
 * Backend-neutral Provider adapter plan. The Engine-local function is consumed
 * only while constructing this value and is never retained by generated output.
 */
struct FAngelscriptTypedASTJITProviderEntryPlan : FStaticJITEntryPlan
{
	/** Exact hook profile shared by the wrapper and emitted body. */
	FAngelscriptTypedASTJITGenerationCapabilityProfile
		InstrumentationProfile;
};

struct FAngelscriptTypedASTJITProviderEmission
{
	bool bSuccess = false;
	FString RawEntryDeclaration;
	FString ImplementationTemplate;
	TArray<FString> RequiredIncludes;
	FString FailureDetail;
};

/** Pointer-free wrapper plan for one provider-internal direct script helper. */
struct FAngelscriptTypedASTJITInternalHelperPlan
{
	FString Symbol;
	FString BodySymbol;
	FString ReturnBodyCppType;
	FString CanonicalDeclaration;
	FString SourceSection;
	uint32 SourceLine = 0;
	FAngelscriptTypedASTJITGenerationCapabilityProfile
		InstrumentationProfile;
	int32 FunctionReferenceSlotIndex = INDEX_NONE;
	TArray<FAngelscriptTypedASTJITProviderParameterPlan> Parameters;
};

struct FAngelscriptTypedASTJITInternalHelperEmission
{
	bool bSuccess = false;
	FString ForwardDeclaration;
	FString Implementation;
	TArray<FString> RequiredIncludes;
	FString FailureDetail;
};

/** Build the narrow scalar plus exact borrowed managed-input ABI plan. */
ANGELSCRIPTRUNTIME_API bool BuildAngelscriptTypedASTJITProviderEntryPlan(
	const FStaticJITEntryPlan& SharedEntryPlan,
	FAngelscriptTypedASTJITProviderEntryPlan& OutPlan,
	FString* OutError = nullptr);

/** Compatibility helper for provider-independent tests; production consumes the frozen plan. */
ANGELSCRIPTRUNTIME_API bool BuildAngelscriptTypedASTJITProviderEntryPlan(
	const asCScriptFunction& ScriptFunction,
	bool bHasVMEntry,
	bool bHasRawEntry,
	bool bHasParmsEntry,
	FAngelscriptTypedASTJITProviderEntryPlan& OutPlan,
	FString* OutError = nullptr);

/** Wrap a verified pure Typed body in exact raw/VM/optional Parms adapters. */
ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITProviderEmission
EmitAngelscriptTypedASTJITProviderFunction(
	const FAngelscriptTypedASTJITProviderEntryPlan& Plan,
	const FAngelscriptTypedASTJITEmission& Body);

/** Wrap one internal Typed body in current-function frame/depth RAII. */
ANGELSCRIPTRUNTIME_API FAngelscriptTypedASTJITInternalHelperEmission
EmitAngelscriptTypedASTJITInternalHelper(
	const FAngelscriptTypedASTJITInternalHelperPlan& Plan,
	const FAngelscriptTypedASTJITEmission& Body);
