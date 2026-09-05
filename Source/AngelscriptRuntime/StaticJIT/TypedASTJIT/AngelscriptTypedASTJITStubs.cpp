#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITBridgeABI.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallBridge.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallClosure.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITDependencies.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEligibility.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITEmitter.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITExecutionProfile.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITModel.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITProviderEmitter.h"
#include "StaticJIT/AngelscriptStaticJITGenerator.h"

#if AS_CAN_GENERATE_JIT

bool RegisterAngelscriptTypedASTJITBackend(FString* OutError)
{
	if (OutError)
	{
		*OutError = TEXT("TypedASTJIT backend is isolated from the active compile graph.");
	}
	return false;
}

FString LexToString(EAngelscriptTypedASTJITDirectCapability Capabilities)
{
	return FString::Printf(TEXT("TypedASTJITCapabilities(%u)"),
		static_cast<uint32>(Capabilities));
}

bool FAngelscriptTypedASTJITGenerationCapabilityProfile::IsValid() const
{
	return false;
}

FString FAngelscriptTypedASTJITGenerationCapabilityProfile::DumpNormalized() const
{
	return TEXT("isolated");
}

bool FAngelscriptTypedASTJITExecutionRequirementsSnapshot::IsValid() const
{
	return false;
}

FString FAngelscriptTypedASTJITExecutionRequirementsSnapshot::DumpNormalized() const
{
	return TEXT("isolated");
}

bool BuildAngelscriptTypedASTJITGenerationCapabilityProfile(
	EAngelscriptTypedASTJITDirectCapability AvailableCapabilities,
	FAngelscriptTypedASTJITGenerationCapabilityProfile& OutProfile,
	FString* OutError)
{
	(void)AvailableCapabilities;
	OutProfile = {};
	if (OutError)
	{
		*OutError = TEXT("TypedASTJIT is isolated");
	}
	return false;
}

bool BuildAngelscriptTypedASTJITExecutionRequirementsSnapshot(
	const FAngelscriptTypedASTJITExecutionRequirementsInput& Input,
	FAngelscriptTypedASTJITExecutionRequirementsSnapshot& OutSnapshot,
	FString* OutError)
{
	(void)Input;
	OutSnapshot = {};
	if (OutError)
	{
		*OutError = TEXT("TypedASTJIT is isolated");
	}
	return false;
}

bool BuildAngelscriptTypedASTJITExecutionRequirementsSnapshot(
	EAngelscriptTypedASTJITDirectCapability RequiredCapabilities,
	FAngelscriptTypedASTJITExecutionRequirementsSnapshot& OutSnapshot,
	FString* OutError)
{
	(void)RequiredCapabilities;
	OutSnapshot = {};
	if (OutError)
	{
		*OutError = TEXT("TypedASTJIT is isolated");
	}
	return false;
}

FAngelscriptTypedASTJITExecutionRouteEligibility
EvaluateAngelscriptTypedASTJITExecutionRoute(
	const FAngelscriptTypedASTJITGenerationCapabilityProfile& Profile,
	const FAngelscriptTypedASTJITExecutionRequirementsSnapshot& Requirements,
	bool bDirectCalleeMismatch)
{
	(void)Profile;
	(void)Requirements;
	(void)bDirectCalleeMismatch;
	return {};
}

FAngelscriptHash256 BuildAngelscriptTypedASTJITProfiledContentIdentity(
	const FAngelscriptHash256& SourceExecutionHash,
	const FAngelscriptTypedASTJITGenerationCapabilityProfile& Profile)
{
	(void)SourceExecutionHash;
	(void)Profile;
	return {};
}

bool FAngelscriptTypedASTJITVMBridgeABIIdentity::IsValid() const
{
	return false;
}

FAngelscriptTypedASTJITVMBridgeABIIdentity
FAngelscriptTypedASTJITVMBridgeABIIdentity::FromCanonical(FStringView Canonical)
{
	FAngelscriptTypedASTJITVMBridgeABIIdentity Identity;
	Identity.Canonical = FString(Canonical);
	return Identity;
}

bool TryBuildAngelscriptTypedASTJITVMBridgeABIIdentity(
	const asIScriptFunction& ScriptFunction,
	FAngelscriptTypedASTJITVMBridgeABIIdentity& OutIdentity,
	FString* OutError)
{
	(void)ScriptFunction;
	OutIdentity = {};
	if (OutError)
	{
		*OutError = TEXT("TypedASTJIT is isolated");
	}
	return false;
}

namespace AngelscriptTypedASTJIT
{
	const TCHAR* LexToString(EAngelscriptTypedASTJITBridgeResult Result)
	{
		(void)Result;
		return TEXT("Isolated");
	}

	EAngelscriptTypedASTJITBridgeResult InvokeBoundViaVM(
		FScriptExecution& Execution,
		const FAngelscriptTypedASTJITBoundCallSite& CallSite,
		const FAngelscriptTypedASTJITScalarValue* Arguments,
		uint32 ArgumentCount,
		EAngelscriptTypedASTJITScalarKind ReturnKind,
		FAngelscriptTypedASTJITScalarValue* OutReturnValue)
	{
		(void)Execution;
		(void)CallSite;
		(void)Arguments;
		(void)ArgumentCount;
		(void)ReturnKind;
		if (OutReturnValue)
		{
			*OutReturnValue = {};
		}
		return EAngelscriptTypedASTJITBridgeResult::MissingFunction;
	}

	EAngelscriptTypedASTJITBridgeResult ResolveBoundNative(
		FScriptExecution& Execution,
		const FAngelscriptTypedASTJITBoundCallSite& CallSite,
		const EAngelscriptTypedASTJITScalarKind* ArgumentKinds,
		uint32 ArgumentCount,
		EAngelscriptTypedASTJITScalarKind ReturnKind,
		asFUNCTION_t& OutFunction)
	{
		(void)Execution;
		(void)CallSite;
		(void)ArgumentKinds;
		(void)ArgumentCount;
		(void)ReturnKind;
		OutFunction = nullptr;
		return EAngelscriptTypedASTJITBridgeResult::MissingNativeAddress;
	}
}

bool ValidateAngelscriptTypedASTJITFoldedGlobalDependency(
	TConstArrayView<FAngelscriptTypedASTJITGlobalDependency> Dependencies,
	int32 EngineLocalGlobalPropertyId,
	FString* OutError)
{
	(void)Dependencies;
	(void)EngineLocalGlobalPropertyId;
	if (OutError)
	{
		*OutError = TEXT("TypedASTJIT is isolated");
	}
	return false;
}

const TCHAR* LexToString(EAngelscriptTypedASTJITFallbackReason Reason)
{
	(void)Reason;
	return TEXT("Isolated");
}

bool AssignAngelscriptTypedASTJITBridgeCallSiteSymbols(
	TArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan> NativeCalls,
	FString* OutError)
{
	(void)NativeCalls;
	if (OutError)
	{
		*OutError = TEXT("TypedASTJIT is isolated");
	}
	return false;
}

bool AssignAngelscriptTypedASTJITBridgeCallSiteSymbols(
	TArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan> NativeCalls,
	const FAngelscriptStableFunctionKey& OwningRootFunctionKey,
	FString* OutError)
{
	(void)NativeCalls;
	(void)OwningRootFunctionKey;
	if (OutError)
	{
		*OutError = TEXT("TypedASTJIT is isolated");
	}
	return false;
}

const TCHAR* LexToString(EAngelscriptTypedASTJITSemanticUseKind Kind)
{
	(void)Kind;
	return TEXT("Isolated");
}

bool BuildAngelscriptTypedASTJITRootIndex(
	const FAngelscriptStaticJITCompiledSourceGraph& Graph,
	FAngelscriptTypedASTJITRootIndex& OutIndex,
	FString* OutError)
{
	(void)Graph;
	OutIndex = {};
	if (OutError)
	{
		*OutError = TEXT("TypedASTJIT is isolated");
	}
	return false;
}

FAngelscriptTypedASTJITEligibility EvaluateAngelscriptTypedASTJITEligibility(
	const FAngelscriptTypedASTJITEligibilityInput& Input)
{
	(void)Input;
	return {};
}

bool BuildAngelscriptTypedASTJITFunctionShape(
	const FAngelscriptStaticJITCompiledFunctionView& Function,
	const FStaticJITEntryPlan& EntryPlan,
	const FString& SymbolName,
	FAngelscriptTypedASTJITFunctionShape& OutShape,
	FString* OutError)
{
	(void)Function;
	(void)EntryPlan;
	(void)SymbolName;
	OutShape = {};
	if (OutError)
	{
		*OutError = TEXT("TypedASTJIT is isolated");
	}
	return false;
}

FAngelscriptTypedASTJITEligibilityInput
BuildAngelscriptTypedASTJITEligibilityInput(
	const FAngelscriptTypedASTJITRoot* Root,
	const FAngelscriptStaticJITCompiledFunctionView& Function)
{
	(void)Root;
	(void)Function;
	return {};
}

bool TrySpellAngelscriptTypedASTJITCppType(
	const asCDataType& Type,
	FString& OutType)
{
	(void)Type;
	OutType.Reset();
	return false;
}

bool EmitAngelscriptTypedASTJITBridgeCallSiteRows(
	TConstArrayView<FAngelscriptTypedASTJITNativeCallEmissionPlan> NativeCalls,
	FString& OutDefinitions,
	FString* OutError)
{
	(void)NativeCalls;
	OutDefinitions.Reset();
	if (OutError)
	{
		*OutError = TEXT("TypedASTJIT is isolated");
	}
	return false;
}

bool BuildAngelscriptTypedASTJITCallClosureInputs(
	const FAngelscriptStaticJITCompiledSourceGraph& Graph,
	const FAngelscriptTypedASTJITRootIndex& RootIndex,
	TArray<FAngelscriptTypedASTJITCallClosureFunctionInput>& OutFunctions,
	FString* OutError)
{
	(void)Graph;
	(void)RootIndex;
	OutFunctions.Reset();
	if (OutError)
	{
		*OutError = TEXT("TypedASTJIT is isolated");
	}
	return false;
}

bool BuildAngelscriptTypedASTJITCallClosure(
	TConstArrayView<FAngelscriptTypedASTJITCallClosureFunctionInput> Functions,
	const FAngelscriptStableFunctionKey& RootFunctionKey,
	const FAngelscriptTypedASTJITCallClosureOptions& Options,
	FAngelscriptTypedASTJITCallClosurePlan& OutPlan,
	FString* OutError)
{
	(void)Functions;
	(void)RootFunctionKey;
	(void)Options;
	OutPlan = {};
	if (OutError)
	{
		*OutError = TEXT("TypedASTJIT is isolated");
	}
	return false;
}

bool BuildAngelscriptTypedASTJITProviderEntryPlan(
	const FStaticJITEntryPlan& SharedEntryPlan,
	FAngelscriptTypedASTJITProviderEntryPlan& OutPlan,
	FString* OutError)
{
	(void)SharedEntryPlan;
	OutPlan = {};
	if (OutError)
	{
		*OutError = TEXT("TypedASTJIT is isolated");
	}
	return false;
}

bool BuildAngelscriptTypedASTJITProviderEntryPlan(
	const asCScriptFunction& ScriptFunction,
	bool bHasVMEntry,
	bool bHasRawEntry,
	bool bHasParmsEntry,
	FAngelscriptTypedASTJITProviderEntryPlan& OutPlan,
	FString* OutError)
{
	(void)ScriptFunction;
	(void)bHasVMEntry;
	(void)bHasRawEntry;
	(void)bHasParmsEntry;
	OutPlan = {};
	if (OutError)
	{
		*OutError = TEXT("TypedASTJIT is isolated");
	}
	return false;
}

FAngelscriptTypedASTJITProviderEmission
EmitAngelscriptTypedASTJITProviderFunction(
	const FAngelscriptTypedASTJITProviderEntryPlan& Plan,
	const FAngelscriptTypedASTJITEmission& Body)
{
	(void)Plan;
	(void)Body;
	return {};
}

FAngelscriptTypedASTJITInternalHelperEmission
EmitAngelscriptTypedASTJITInternalHelper(
	const FAngelscriptTypedASTJITInternalHelperPlan& Plan,
	const FAngelscriptTypedASTJITEmission& Body)
{
	(void)Plan;
	(void)Body;
	return {};
}

#endif
