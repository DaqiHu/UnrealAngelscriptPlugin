#include "CQTest.h"

#include "Core/Artifacts/AngelscriptArtifactIdentity.h"
#include "StaticJIT/AngelscriptStaticJITGenerator.h"
#include "StaticJIT/StaticJITDiagnostics.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_WITH_STATIC_JIT_DIAGNOSTICS \
	&& AS_CAN_GENERATE_JIT

namespace AngelscriptStaticJITAotCallDiagnosticsTests_Private
{
	FAngelscriptStableFunctionKey BuildFunctionKey(const TCHAR* Name)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("static-jit-typed-call-diagnostics-test"));
		Writer.WriteString(Name);
		return {Writer.FinalizeHash()};
	}

	FAngelscriptHash256 BuildHash(const TCHAR* Domain, const TCHAR* Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(Domain);
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotCallDiagnosticTests,
	"Angelscript.TestModule.StaticJIT.AOT.Diagnostics.Calls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CallsSeparateGeneratedCoreAndRegisteredTargets)
	{
		using namespace
			AngelscriptStaticJITAotCallDiagnosticsTests_Private;

		FAngelscriptStaticJITGeneratorOutput GeneratorOutput;
		GeneratorOutput.Diagnostics.RequestedBackendId =
			FAngelscriptStaticJITBackendId::TypedAST();
		GeneratorOutput.Diagnostics.CaptureProfile =
			EAngelscriptStaticJITCaptureProfile::VerifiedCanonicalAST;

		FAngelscriptStaticJITFunctionResult& Function =
			GeneratorOutput.Functions.AddDefaulted_GetRef();
		Function.FunctionKey = BuildFunctionKey(TEXT("Root"));
		Function.ActualBackendId =
			FAngelscriptStaticJITBackendId::TypedAST();
		Function.Disposition =
			EAngelscriptStaticJITFunctionDisposition::Emitted;
		Function.TypedDiagnostic.Emplace();
		Function.TypedDiagnostic->CallClosure.Emplace();
		FAngelscriptStaticJITTypedCallClosureDiagnostic& Closure =
			Function.TypedDiagnostic->CallClosure.GetValue();
		Closure.bEligible = true;

		FAngelscriptStaticJITTypedCallDiagnostic& BridgeCall =
			Closure.Calls.AddDefaulted_GetRef();
		BridgeCall.CallerFunctionKey = Function.FunctionKey;
		BridgeCall.CalleeFunctionKey = BuildFunctionKey(TEXT("Print"));
		BridgeCall.ExpressionIndex = 7;
		BridgeCall.ProcessedSource.Section =
			TEXT("Script/Diagnostics/Calls.processed.as");
		BridgeCall.ProcessedSource.Row = 12;
		BridgeCall.ProcessedSource.Column = 5;
		BridgeCall.AuthoredSource.Emplace();
		BridgeCall.AuthoredSource->Section =
			TEXT("Script/Diagnostics/Calls.as");
		BridgeCall.AuthoredSource->Row = 9;
		BridgeCall.AuthoredSource->Column = 3;
		BridgeCall.GeneratedSource.Emplace();
		BridgeCall.GeneratedSource->Section =
			TEXT("/Script/Generated/Diagnostics/Calls.generated.as");
		BridgeCall.GeneratedSource->Row = 31;
		BridgeCall.GeneratedSource->Column = 11;
		BridgeCall.TargetKind =
			EAngelscriptTypedASTJITCallTargetKind::SystemFunction;
		BridgeCall.Disposition =
			EAngelscriptStaticJITTypedCallDisposition::Bridge;
		BridgeCall.CanonicalDeclaration =
			TEXT("void Print(const FString&in Message)");
		BridgeCall.EmittedRoute = EAngelscriptJITNativeCallSiteRoute::Bridge;
		BridgeCall.EmittedCppCallee =
			TEXT("AngelscriptTypedASTJIT::InvokeBound<void, const FString&>");
		BridgeCall.RuntimeDllCore =
			TEXT("AngelscriptTypedASTJIT::InvokeBoundViaVM");
		BridgeCall.MetadataRowSymbol = TEXT("GTypedCallSite_Print_91c4cce0");
		BridgeCall.RegisteredTarget = TEXT("FAngelscriptBinds::Print");
		BridgeCall.StableTargetKey = BuildHash(
			TEXT("typed-call-target"), TEXT("Print"));
		BridgeCall.ExpectedAbi = BuildHash(
			TEXT("typed-call-abi"), TEXT("void(FString)"));
		BridgeCall.ReferenceSlotIndex = 3;
		BridgeCall.bHasNativeLinkageValidation = true;
		BridgeCall.NativeLinkageValidation =
			EAngelscriptStaticJITNativeCallValidationCode::Valid;
		BridgeCall.NativeLinkageValidationDetail =
			TEXT("Exported Runtime bridge metadata is current.");

		FAngelscriptStaticJITTypedCallDiagnostic& DirectCall =
			Closure.Calls.AddDefaulted_GetRef();
		DirectCall.CallerFunctionKey = Function.FunctionKey;
		DirectCall.CalleeFunctionKey = BuildFunctionKey(TEXT("Abs"));
		DirectCall.ExpressionIndex = 3;
		DirectCall.TargetKind =
			EAngelscriptTypedASTJITCallTargetKind::SystemFunction;
		DirectCall.Disposition =
			EAngelscriptStaticJITTypedCallDisposition::DirectInline;
		DirectCall.CanonicalDeclaration = TEXT("int Math::Abs(int Value)");
		DirectCall.EmittedRoute =
			EAngelscriptJITNativeCallSiteRoute::DirectInline;
		DirectCall.EmittedCppCallee = TEXT("FMath::Abs");
		DirectCall.DirectCppSymbol = TEXT("FMath::Abs");
		DirectCall.RegisteredTarget = TEXT("FMath::Abs");
		DirectCall.bHasNativeLinkageValidation = true;
		DirectCall.NativeLinkageValidation =
			EAngelscriptStaticJITNativeCallValidationCode::Valid;

		struct FDispositionCase
		{
			EAngelscriptStaticJITTypedCallDisposition Disposition;
			const TCHAR* Name;
		};
		static constexpr FDispositionCase DispositionCases[] = {
			{EAngelscriptStaticJITTypedCallDisposition::DirectScript,
				TEXT("DirectScript")},
			{EAngelscriptStaticJITTypedCallDisposition::InternalSemanticHelper,
				TEXT("InternalSemanticHelper")},
			{EAngelscriptStaticJITTypedCallDisposition::DirectExported,
				TEXT("DirectExported")},
			{EAngelscriptStaticJITTypedCallDisposition::RuntimeThunk,
				TEXT("RuntimeThunk")},
			{EAngelscriptStaticJITTypedCallDisposition::CurrentNativeBinding,
				TEXT("CurrentNativeBinding")},
			{EAngelscriptStaticJITTypedCallDisposition::Unsupported,
				TEXT("Unsupported")},
		};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(DispositionCases); ++Index)
		{
			FAngelscriptStaticJITTypedCallDiagnostic& VocabularyCall =
				Closure.Calls.AddDefaulted_GetRef();
			VocabularyCall.CallerFunctionKey = Function.FunctionKey;
			VocabularyCall.CalleeFunctionKey = BuildFunctionKey(
				DispositionCases[Index].Name);
			VocabularyCall.ExpressionIndex = 20 + Index;
			VocabularyCall.Disposition = DispositionCases[Index].Disposition;
			VocabularyCall.CanonicalDeclaration = DispositionCases[Index].Name;
			if (VocabularyCall.Disposition
				== EAngelscriptStaticJITTypedCallDisposition::Unsupported)
			{
				VocabularyCall.CalleeEligibilityReason =
					EAngelscriptTypedASTJITFallbackReason::UnsupportedReceiver;
				VocabularyCall.CalleeEligibilityDetail = TEXT(
					"ExternalImplicitThis receiver cannot use this direct route.");
				VocabularyCall.CalleeEligibilitySource.Section =
					TEXT("Script/Diagnostics/Calls.processed.as");
				VocabularyCall.CalleeEligibilitySource.Row = 44;
				VocabularyCall.CalleeEligibilitySource.Column = 8;
				VocabularyCall.bHasNativeLinkageValidation = true;
				VocabularyCall.NativeLinkageValidation =
					EAngelscriptStaticJITNativeCallValidationCode::PrivateDependency;
				VocabularyCall.NativeLinkageValidationDetail = TEXT(
					"Owning module is not a public dependency of the Provider module.");
			}
		}

		FStaticJITDiagnostics::FSnapshot Snapshot;
		Snapshot.ArtifactRequest =
			FStaticJITDiagnostics::BuildArtifactRequestRecord(GeneratorOutput);
		const FStaticJITDiagnostics::FJsonResult Result =
			FStaticJITDiagnostics::Serialize(Snapshot);

		ASSERT_THAT(IsTrue(Result.IsSuccess(), Result.Detail));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"disposition\":\"Bridge\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"targetKind\":\"SystemFunction\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"processedSource\":{\"section\":\"Script/Diagnostics/Calls.processed.as\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"authoredSource\":{\"section\":\"Script/Diagnostics/Calls.as\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"generatedSource\":{\"section\":\"/Script/Generated/Diagnostics/Calls.generated.as\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"emittedCppCallee\":\"AngelscriptTypedASTJIT::InvokeBound<void, const FString&>\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"runtimeDllCore\":\"AngelscriptTypedASTJIT::InvokeBoundViaVM\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"metadataRowSymbol\":\"GTypedCallSite_Print_91c4cce0\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"registeredTarget\":\"FAngelscriptBinds::Print\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"referenceSlotIndex\":3"))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"nativeLinkageValidation\":\"Valid\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"nativeLinkageValidationDetail\":\"Exported Runtime bridge metadata is current.\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"calleeEligibilityReason\":\"UnsupportedReceiver\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"calleeEligibilityDetail\":\"ExternalImplicitThis receiver cannot use this direct route.\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"nativeLinkageValidation\":\"PrivateDependency\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"nativeLinkageValidationDetail\":\"Owning module is not a public dependency of the Provider module.\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("void Print(const FString&in Message) -> AngelscriptTypedASTJIT::InvokeBound<void, const FString&> -> AngelscriptTypedASTJIT::InvokeBoundViaVM -> slot 3 -> FAngelscriptBinds::Print"))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"disposition\":\"DirectInline\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"directCppSymbol\":\"FMath::Abs\""))));
		for (const FDispositionCase& DispositionCase : DispositionCases)
		{
			ASSERT_THAT(IsTrue(Result.Json.Contains(FString::Printf(
				TEXT("\"disposition\":\"%s\""),
				DispositionCase.Name))));
		}
		const int32 DirectExpressionOffset = Result.Json.Find(
			TEXT("\"expressionIndex\":3"));
		const int32 BridgeExpressionOffset = Result.Json.Find(
			TEXT("\"expressionIndex\":7"));
		ASSERT_THAT(IsTrue(DirectExpressionOffset != INDEX_NONE));
		ASSERT_THAT(IsTrue(BridgeExpressionOffset > DirectExpressionOffset));
		ASSERT_THAT(IsFalse(Result.Json.Contains(TEXT("0x"),
			ESearchCase::IgnoreCase)));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS && AS_WITH_STATIC_JIT_DIAGNOSTICS
	// && AS_CAN_GENERATE_JIT
