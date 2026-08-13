#pragma once

#include "CoreMinimal.h"

class asIScriptEngine;

namespace AngelscriptStaticJITAotFixture
{
	ANGELSCRIPTTEST_API const FName& GetModuleName();
	ANGELSCRIPTTEST_API const FString& GetSourceFilename();
	ANGELSCRIPTTEST_API const FString& GetScriptSource();
	ANGELSCRIPTTEST_API const FName& GetImportProviderModuleName();
	ANGELSCRIPTTEST_API const FString& GetImportProviderSourceFilename();
	ANGELSCRIPTTEST_API const FString& GetImportProviderScriptSource();
	ANGELSCRIPTTEST_API const FName& GetGeneratedClassName();
	ANGELSCRIPTTEST_API const FName& GetVirtualParentClassName();
	ANGELSCRIPTTEST_API const FName& GetVirtualChildClassName();
	ANGELSCRIPTTEST_API const FString& GetEntryDeclaration();
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
	ANGELSCRIPTTEST_API const FString& GetStaticWorldContextDeclaration();
	ANGELSCRIPTTEST_API int32 GetExpectedEntryResult();
	ANGELSCRIPTTEST_API int32 GetExpectedNoParamsStoredValue();
	ANGELSCRIPTTEST_API int32 GetExpectedPrimitiveArgStoredValue();
	ANGELSCRIPTTEST_API int32 GetExpectedPrimitiveReturnValue();
	ANGELSCRIPTTEST_API int32 GetExpectedReferenceArgValue();
	ANGELSCRIPTTEST_API int32 GetExpectedReferenceReturnValue();
	ANGELSCRIPTTEST_API int32 GetExpectedMultiArgResult();
	ANGELSCRIPTTEST_API int32 GetExpectedThreadSafeResult();
	ANGELSCRIPTTEST_API int32 GetExpectedStaticWorldContextResult();
	ANGELSCRIPTTEST_API int32 GetExpectedObjectLastNativeResult();
	ANGELSCRIPTTEST_API const FString& GetExpectedExceptionText();
	ANGELSCRIPTTEST_API int32 GetExpectedReferenceEntryResult();
	ANGELSCRIPTTEST_API int32 GetExpectedObjectLifetimeResult();
	ANGELSCRIPTTEST_API int32 GetExpectedRecursionResult();
	ANGELSCRIPTTEST_API int32 GetExpectedImportResult();
	ANGELSCRIPTTEST_API int32 GetExpectedVirtualOverrideResult();
	ANGELSCRIPTTEST_API bool RegisterObjectLastNativeSurface(asIScriptEngine& ScriptEngine);
	ANGELSCRIPTTEST_API void ResetObjectLastNativeObservation();
	ANGELSCRIPTTEST_API int32 GetObjectLastNativeCallCount();
	ANGELSCRIPTTEST_API int32 GetObjectLastNativeLeftSentinel();
	ANGELSCRIPTTEST_API int32 GetObjectLastNativeRightSentinel();
	ANGELSCRIPTTEST_API int32 GetObjectLastNativeObjectValue();
}
