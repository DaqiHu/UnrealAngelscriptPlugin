#pragma once

#include "CoreMinimal.h"

class asIScriptEngine;
struct FAngelscriptEngine;

namespace AngelscriptStaticJITAotFixture
{
	struct FTypedASTFrameObservation
	{
		int32 Marker = 0;
		FString PublicFilename;
		int32 PublicLine = INDEX_NONE;
		FString TopFilename;
		FString TopFunction;
		int32 TopLine = INDEX_NONE;
		FString ParentFilename;
		FString ParentFunction;
		int32 ParentLine = INDEX_NONE;
		int32 FrameDepth = 0;
	};

	enum class ETypedASTPrivateBridgeRegistration : uint8
	{
		Primary,
		Replacement,
	};

	ANGELSCRIPTTEST_API const FName& GetModuleName();
	ANGELSCRIPTTEST_API const FString& GetSourceFilename();
	ANGELSCRIPTTEST_API const FString& GetScriptSource();
	ANGELSCRIPTTEST_API const FName& GetImportProviderModuleName();
	ANGELSCRIPTTEST_API const FString& GetImportProviderSourceFilename();
	ANGELSCRIPTTEST_API const FString& GetImportProviderScriptSource();
	ANGELSCRIPTTEST_API const FName& GetTypedPrintModuleName();
	ANGELSCRIPTTEST_API const FString& GetTypedPrintSourceFilename();
	ANGELSCRIPTTEST_API const FString& GetTypedPrintScriptSource();
	ANGELSCRIPTTEST_API const FName& GetTypedPrivateBridgeModuleName();
	ANGELSCRIPTTEST_API const FName& GetTypedPrivateBridgeStaticsClassName();
	ANGELSCRIPTTEST_API const FString& GetTypedPrivateBridgeSourceFilename();
	ANGELSCRIPTTEST_API const FString& GetTypedPrivateBridgeScriptSource();
	ANGELSCRIPTTEST_API const FName& GetTypedRecursionModuleName();
	ANGELSCRIPTTEST_API const FString& GetTypedRecursionSourceFilename();
	ANGELSCRIPTTEST_API const FString& GetTypedRecursionScriptSource();
	ANGELSCRIPTTEST_API const FName& GetTypedControlFlowModuleName();
	ANGELSCRIPTTEST_API const FName& GetTypedControlFlowStaticsClassName();
	ANGELSCRIPTTEST_API const FName& GetTypedFinalScalarModuleName();
	ANGELSCRIPTTEST_API const FName& GetTypedFinalScalarReceiverClassName();
	ANGELSCRIPTTEST_API const FString& GetTypedControlFlowSourceFilename();
	ANGELSCRIPTTEST_API const FString& GetTypedControlFlowScriptSource();
	ANGELSCRIPTTEST_API const FString& GetTypedFinalScalarSourceFilename();
	ANGELSCRIPTTEST_API const FString& GetTypedFinalScalarScriptSource();
	ANGELSCRIPTTEST_API const FName& GetIneligibleReceiverModuleName();
	ANGELSCRIPTTEST_API const FString& GetIneligibleReceiverSourceFilename();
	ANGELSCRIPTTEST_API const FString& GetIneligibleReceiverScriptSource();
	ANGELSCRIPTTEST_API const FString& GetMixinReceiverRootDeclaration();
	ANGELSCRIPTTEST_API const FString& GetExternalReceiverRootDeclaration();
	ANGELSCRIPTTEST_API const FString& GetGeneratedLifecycleRootDeclaration();
	ANGELSCRIPTTEST_API int32 GetExpectedMixinReceiverRootResult();
	ANGELSCRIPTTEST_API int32 GetExpectedExternalReceiverRootResult();
	ANGELSCRIPTTEST_API int32 GetExpectedGeneratedLifecycleRootResult();
	ANGELSCRIPTTEST_API const FName& GetGeneratedClassName();
	ANGELSCRIPTTEST_API const FName& GetVirtualParentClassName();
	ANGELSCRIPTTEST_API const FName& GetVirtualChildClassName();
	ANGELSCRIPTTEST_API const FString& GetEntryDeclaration();
	ANGELSCRIPTTEST_API const FString& GetSemanticScalarBranchDeclaration();
	ANGELSCRIPTTEST_API const FString& GetSemanticMutationParityDeclaration();
	ANGELSCRIPTTEST_API const FString&
		GetSemanticStructuredControlFlowDeclaration();
	ANGELSCRIPTTEST_API const FString&
		GetSemanticExhaustiveEnumDeclaration();
	ANGELSCRIPTTEST_API const FString&
		GetSemanticCheckedDivideDeclaration();
	ANGELSCRIPTTEST_API const FString&
		GetSemanticNumericBoundaryDeclaration();
	ANGELSCRIPTTEST_API const FString&
		GetSemanticCloneableStateDeclaration();
	ANGELSCRIPTTEST_API const FString& GetTypedFinalScalarRawDeclaration();
	ANGELSCRIPTTEST_API const FString& GetTypedDifferentialScalarDeclaration();
	ANGELSCRIPTTEST_API const FString& GetSemanticImportedBindingDeclaration();
	ANGELSCRIPTTEST_API const FString& GetSemanticPowerBoundaryDeclaration();
	ANGELSCRIPTTEST_API const FString& GetSemanticPowerFloatDeclaration();
	ANGELSCRIPTTEST_API const FString& GetSemanticPowerDoubleDeclaration();
	ANGELSCRIPTTEST_API const FString& GetSemanticPowerDoubleIntDeclaration();
	ANGELSCRIPTTEST_API const FString& GetTypedASTCapabilityShowcaseDeclaration();
	ANGELSCRIPTTEST_API const FString& GetTypedASTExportedSymbolShowcaseDeclaration();
	ANGELSCRIPTTEST_API const FString& GetTypedASTPrintShowcaseDeclaration();
	ANGELSCRIPTTEST_API const FString& GetTypedASTPrivateBridgeShowcaseDeclaration();
	ANGELSCRIPTTEST_API const FString& GetTypedASTRuntimeThunkShowcaseDeclaration();
	ANGELSCRIPTTEST_API const FString& GetTypedASTPrivateBridgeExceptionDeclaration();
	ANGELSCRIPTTEST_API const FString&
		GetTypedASTPublicUASFunctionExceptionEntryDeclaration();
	ANGELSCRIPTTEST_API const FString& GetTypedASTOrdinaryBridgeSqrtDeclaration();
	ANGELSCRIPTTEST_API const FString& GetTypedASTSelfRecursionDeclaration();
	ANGELSCRIPTTEST_API const FString& GetTypedASTMutualRecursionDeclaration();
	ANGELSCRIPTTEST_API const FString& GetTypedASTFrameEntryDeclaration();
	ANGELSCRIPTTEST_API const FString& GetTypedASTExceptionHelperDeclaration();
	ANGELSCRIPTTEST_API const FString& GetTypedASTExceptionEntryDeclaration();
	ANGELSCRIPTTEST_API const FString& GetDoubleToInt64Declaration();
	ANGELSCRIPTTEST_API const FString& GetDoubleToUint64Declaration();
	ANGELSCRIPTTEST_API const FString& GetObjectLastNativeEntryDeclaration();
	ANGELSCRIPTTEST_API const FString& GetExceptionEntryDeclaration();
	ANGELSCRIPTTEST_API const FString& GetReferenceEntryDeclaration();
	ANGELSCRIPTTEST_API const FString& GetObjectLifetimeEntryDeclaration();
	ANGELSCRIPTTEST_API const FString& GetRecursionEntryDeclaration();
	ANGELSCRIPTTEST_API const FString& GetImportEntryDeclaration();
	ANGELSCRIPTTEST_API const FString& GetVirtualCallDeclaration();
	ANGELSCRIPTTEST_API const FString& GetVirtualOverrideDeclaration();
	ANGELSCRIPTTEST_API const FString& GetVirtualOverrideImplementationDeclaration();
	ANGELSCRIPTTEST_API const FString& GetMethodNoParamsDeclaration();
	ANGELSCRIPTTEST_API const FString& GetMethodPrimitiveArgDeclaration();
	ANGELSCRIPTTEST_API const FString& GetMethodPrimitiveReturnDeclaration();
	ANGELSCRIPTTEST_API const FString& GetMethodReferenceArgDeclaration();
	ANGELSCRIPTTEST_API const FString& GetMethodReferenceDeclaration();
	ANGELSCRIPTTEST_API const FString& GetMethodObjectReturnDeclaration();
	ANGELSCRIPTTEST_API const FString& GetMethodMultiArgDeclaration();
	ANGELSCRIPTTEST_API const FString& GetMethodThreadSafeDeclaration();
	ANGELSCRIPTTEST_API const FString& GetMethodRpcImplementationDeclaration();
	ANGELSCRIPTTEST_API const FString& GetStaticWorldContextDeclaration();
	ANGELSCRIPTTEST_API const FString&
		GetBytecodeFallbackGeneratedWorldContextDeclaration();
	ANGELSCRIPTTEST_API int32 GetExpectedEntryResult();
	ANGELSCRIPTTEST_API int32 GetExpectedNoParamsStoredValue();
	ANGELSCRIPTTEST_API int32 GetExpectedPrimitiveArgStoredValue();
	ANGELSCRIPTTEST_API int32 GetExpectedPrimitiveReturnValue();
	ANGELSCRIPTTEST_API int32 GetExpectedReferenceArgValue();
	ANGELSCRIPTTEST_API int32 GetExpectedReferenceReturnValue();
	ANGELSCRIPTTEST_API int32 GetExpectedMultiArgResult();
	ANGELSCRIPTTEST_API int32 GetExpectedThreadSafeResult();
	ANGELSCRIPTTEST_API int32 GetExpectedRpcStoredValue();
	ANGELSCRIPTTEST_API int32 GetExpectedStaticWorldContextResult();
	ANGELSCRIPTTEST_API int32
		GetExpectedBytecodeFallbackGeneratedWorldContextResult();
	ANGELSCRIPTTEST_API int32 GetExpectedObjectLastNativeResult();
	ANGELSCRIPTTEST_API const FString& GetExpectedExceptionText();
	ANGELSCRIPTTEST_API const FString& GetExceptionThrowDeclaration();
	ANGELSCRIPTTEST_API int32 GetExpectedReferenceEntryResult();
	ANGELSCRIPTTEST_API int32 GetExpectedObjectLifetimeResult();
	ANGELSCRIPTTEST_API int32 GetExpectedRecursionResult();
	ANGELSCRIPTTEST_API int32 GetExpectedImportResult();
	ANGELSCRIPTTEST_API int32 GetExpectedNestedThrowLine();
	ANGELSCRIPTTEST_API int32 GetExpectedVirtualOverrideResult();
	ANGELSCRIPTTEST_API bool RegisterObjectLastNativeSurface(asIScriptEngine& ScriptEngine);
	ANGELSCRIPTTEST_API bool RegisterTypedASTPrivateBridgeSurface(
		FAngelscriptEngine& Engine,
		ETypedASTPrivateBridgeRegistration Registration =
			ETypedASTPrivateBridgeRegistration::Primary);
	ANGELSCRIPTTEST_API void ResetTypedASTPrivateBridgeObservation();
	ANGELSCRIPTTEST_API int32 GetTypedASTPrivateBridgeCallCount();
	ANGELSCRIPTTEST_API int32
		GetTypedASTPrivateBridgeReplacementCallCount();
	ANGELSCRIPTTEST_API int32 GetTypedASTPrivateBridgeExceptionCallCount();
	ANGELSCRIPTTEST_API void ResetTypedASTRuntimeThunkObservation();
	ANGELSCRIPTTEST_API int32 GetTypedASTRuntimeThunkRegisteredCallCount();
	ANGELSCRIPTTEST_API int32 GetTypedASTPublicUASFunctionInvokeCallCount();
	ANGELSCRIPTTEST_API int32 GetExpectedTypedASTPrivateBridgeResult();
	ANGELSCRIPTTEST_API int32
		GetExpectedTypedASTPrivateBridgeReplacementResult();
	ANGELSCRIPTTEST_API const FString&
		GetExpectedTypedASTPrivateBridgeExceptionText();
	ANGELSCRIPTTEST_API int32 GetExpectedTypedASTSelfRecursionResult();
	ANGELSCRIPTTEST_API int32 GetExpectedTypedASTMutualRecursionResult();
	ANGELSCRIPTTEST_API int32 GetExpectedTypedASTFrameResult(int32 Value);
	ANGELSCRIPTTEST_API int32
		GetExpectedTypedASTFrameObservationLine(int32 Marker);
	ANGELSCRIPTTEST_API int32 GetExpectedTypedASTFrameHelperCallLine();
	ANGELSCRIPTTEST_API void ResetTypedASTFrameObservations();
	ANGELSCRIPTTEST_API int32 GetTypedASTFrameObservationCount();
	ANGELSCRIPTTEST_API bool TryGetTypedASTFrameObservation(
		int32 Index,
		FTypedASTFrameObservation& OutObservation);
	ANGELSCRIPTTEST_API int32 GetExpectedTypedASTExceptionLine();
	ANGELSCRIPTTEST_API void ResetObjectLastNativeObservation();
	ANGELSCRIPTTEST_API int32 GetObjectLastNativeCallCount();
	ANGELSCRIPTTEST_API int32 GetObjectLastNativeLeftSentinel();
	ANGELSCRIPTTEST_API int32 GetObjectLastNativeRightSentinel();
	ANGELSCRIPTTEST_API int32 GetObjectLastNativeObjectValue();
}
