#include "CQTest.h"

#include "AngelscriptTestMacros.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotGeneration.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITBackend.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptStaticJITAotGenerationVerificationTests_Private
{
	using namespace AngelscriptStaticJITAotGeneration;

	static void AssertContainedTypedASTTasks(
		FAutomationTestBase& Test,
		const TCHAR* const Label,
		const FAngelscriptTypedASTJITTaskObservation& Observation)
	{
		Test.TestEqual(
			*FString::Printf(TEXT(
				"%s should create production plus twelve isolated TypedAST backends"),
				Label),
			Observation.CreatedInstanceCount, 13);
		Test.TestEqual(
			*FString::Printf(TEXT(
				"%s should invoke all thirteen contained TypedAST backends"),
				Label),
			Observation.GenerateInvocationCount, 13);
		Test.TestEqual(
			*FString::Printf(TEXT(
				"%s should destroy all thirteen contained TypedAST backends"),
				Label),
			Observation.DestroyedInstanceCount, 13);
		Test.TestEqual(
			*FString::Printf(TEXT(
				"%s should accept all thirteen contained TypedAST tasks"), Label),
			Observation.AcceptedTaskCount, 13);
		Test.TestEqual(
			*FString::Printf(TEXT("%s should reject no TypedAST task"), Label),
			Observation.RejectedTaskCount, 0);
		Test.TestTrue(
			*FString::Printf(TEXT(
				"%s should consume same-compilation typed HIR"), Label),
			Observation.LastVerifiedTypedHIRCount > 0
				&& Observation.bLastTaskUsedOnlySameCompilationHIR);
		Test.TestTrue(
			*FString::Printf(TEXT(
				"%s should index resolved UFUNCTION roots"), Label),
			Observation.LastUFunctionRootCount > 0);
		Test.TestTrue(
			*FString::Printf(TEXT(
				"%s should retain structured semantic diagnostics"), Label),
			Observation.StructuredFunctionDiagnosticCount > 0
				&& Observation.StructuredRootClosureDiagnosticCount > 0
				&& Observation.StructuredVerifiedHIRDiagnosticCount > 0);
		Test.TestTrue(
			*FString::Printf(TEXT(
				"%s should retain helper and exception payload diagnostics"),
				Label),
			Observation.StructuredHelperDiagnosticCount > 0
				&& Observation.StructuredExceptionPayloadDiagnosticCount > 0);
		Test.TestTrue(
			*FString::Printf(TEXT(
				"%s should retain actual per-call lowering diagnostics"), Label),
			Observation.StructuredCallDiagnosticCount > 0
				&& Observation.StructuredEmittedCallDiagnosticCount > 0
				&& Observation.StructuredBridgeCallDiagnosticCount > 0);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotGenerationVerificationTests,
	"Angelscript.TestModule.StaticJIT.AOT.GenerationVerification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(RepeatedTypedGenerationIsByteDeterministicAndCurrent)
	{
		using namespace AngelscriptStaticJITAotGeneration;
		using namespace AngelscriptStaticJITAotGenerationVerificationTests_Private;

		FAngelscriptTypedASTJIT::ResetTaskObservationForTesting();
		const FStaticJITAotGenerationResult First =
			Run(EStaticJITAotGenerationMode::Verify);
		const FAngelscriptTypedASTJITTaskObservation FirstObservation =
			FAngelscriptTypedASTJIT::GetTaskObservationForTesting();
		FAngelscriptTypedASTJIT::ResetTaskObservationForTesting();
		const FStaticJITAotGenerationResult Second =
			Run(EStaticJITAotGenerationMode::Verify);
		const FAngelscriptTypedASTJITTaskObservation SecondObservation =
			FAngelscriptTypedASTJIT::GetTaskObservationForTesting();

		ASSERT_THAT(IsTrue(First.bSuccess, First.Error));
		ASSERT_THAT(IsTrue(Second.bSuccess, Second.Error));
		AssertContainedTypedASTTasks(
			*TestRunner, TEXT("first generation"), FirstObservation);
		AssertContainedTypedASTTasks(
			*TestRunner, TEXT("second generation"), SecondObservation);
		ASSERT_THAT(AreEqual(
			First.ProviderGeneration, Second.ProviderGeneration));
		ASSERT_THAT(AreEqual(
			First.ArtifactSetDigest, Second.ArtifactSetDigest));
		ASSERT_THAT(IsTrue(
			First.IsolatedTypedASTImportedFallbackDetail.StartsWith(
				TEXT("UnsupportedImportedRoute:"),
				ESearchCase::CaseSensitive)));
		ASSERT_THAT(AreEqual(
			First.IsolatedTypedASTImportedFallbackDetail,
			Second.IsolatedTypedASTImportedFallbackDetail));
		ASSERT_THAT(IsTrue(
			First.IsolatedTypedASTNonCloneableFallbackDetail.StartsWith(
				TEXT("NonCloneableEffectRejected: MissingTypedHIR:"),
				ESearchCase::CaseSensitive)));
		ASSERT_THAT(AreEqual(
			First.IsolatedTypedASTNonCloneableFallbackDetail,
			Second.IsolatedTypedASTNonCloneableFallbackDetail));
		ASSERT_THAT(AreEqual(
			FString(TEXT(
				"AS_Test_Differential_Bytecode_DifferentialScalarValue")),
			First.IsolatedBytecodeDifferentialSymbolPrefix));
		ASSERT_THAT(AreEqual(
			FString(TEXT(
				"AS_Test_Differential_TypedAST_DifferentialScalarValue")),
			First.IsolatedTypedASTDifferentialSymbolPrefix));
		ASSERT_THAT(AreEqual(
			First.IsolatedBytecodeDifferentialSymbolPrefix,
			Second.IsolatedBytecodeDifferentialSymbolPrefix));
		ASSERT_THAT(AreEqual(
			First.IsolatedTypedASTDifferentialSymbolPrefix,
			Second.IsolatedTypedASTDifferentialSymbolPrefix));
		ASSERT_THAT(IsFalse(
			First.IsolatedBytecodeDifferentialIdentity.FunctionKey.IsEmpty()));
		ASSERT_THAT(AreEqual(
			First.IsolatedBytecodeDifferentialIdentity.FunctionKey,
			First.IsolatedTypedASTDifferentialIdentity.FunctionKey));
		ASSERT_THAT(AreEqual(
			First.IsolatedBytecodeDifferentialIdentity.ExecutionHash,
			First.IsolatedTypedASTDifferentialIdentity.ExecutionHash));
		ASSERT_THAT(AreEqual(
			First.IsolatedBytecodeDifferentialIdentity.DebugHash,
			First.IsolatedTypedASTDifferentialIdentity.DebugHash));
		ASSERT_THAT(AreEqual(
			First.IsolatedBytecodeDifferentialIdentity.ArtifactProfile,
			First.IsolatedTypedASTDifferentialIdentity.ArtifactProfile));
		ASSERT_THAT(AreEqual(
			First.IsolatedBytecodeDifferentialIdentity.EntryAbiHash,
			First.IsolatedTypedASTDifferentialIdentity.EntryAbiHash));
		ASSERT_THAT(IsTrue(
			First.IsolatedBytecodeDifferentialIdentity
				== Second.IsolatedBytecodeDifferentialIdentity));
		ASSERT_THAT(IsTrue(
			First.IsolatedTypedASTDifferentialIdentity
				== Second.IsolatedTypedASTDifferentialIdentity));
		ASSERT_THAT(AreEqual(
			First.TypedASTNormalizedHIR, Second.TypedASTNormalizedHIR));
		ASSERT_THAT(AreEqual(
			First.TypedASTGeneratedCpp, Second.TypedASTGeneratedCpp));
	}

	TEST_METHOD(CrossProfileOutputHasDistinctIdentityAndVerifyReportsStale)
	{
		using namespace AngelscriptStaticJITAotGeneration;
		const FStaticJITAotGenerationResult Current =
			Run(EStaticJITAotGenerationMode::Verify);
		ASSERT_THAT(IsTrue(Current.bSuccess, Current.Error));

		const FStaticJITAotGenerationResult CrossProfile =
			RunForTesting(
				EStaticJITAotGenerationMode::Verify,
				EStaticJITAotGenerationTestVariant::RecursionOnlyProfile);
		ASSERT_THAT(IsFalse(CrossProfile.bSuccess));
		ASSERT_THAT(IsFalse(
			Current.ProviderGeneration == CrossProfile.ProviderGeneration));
		ASSERT_THAT(IsFalse(
			Current.ArtifactSetDigest == CrossProfile.ArtifactSetDigest));
		ASSERT_THAT(IsTrue(CrossProfile.Error.Contains(
			TEXT("Verify found stale output"), ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(CrossProfile.Error.Contains(
			TEXT("Owned generated bytes are stale."),
			ESearchCase::CaseSensitive), CrossProfile.Error));
	}

	TEST_METHOD(ExpectedEligibleTypedFallbackFailsVerification)
	{
		using namespace AngelscriptStaticJITAotGeneration;
		const FStaticJITAotGenerationResult Result =
			RunForTesting(
				EStaticJITAotGenerationMode::Verify,
				EStaticJITAotGenerationTestVariant::RequireCoverageWithoutCapability);
		ASSERT_THAT(IsFalse(Result.bSuccess));
		ASSERT_THAT(IsTrue(Result.Error.Contains(
			TEXT("did not use the production TypedAST backend"),
			ESearchCase::CaseSensitive)));
		ASSERT_THAT(IsTrue(Result.Error.Contains(
			TEXT("UnsupportedExecutionObservability"),
			ESearchCase::CaseSensitive)));
	}

	TEST_METHOD(EmitterVerificationFailureFallsBackAtomicallyAndFailsVerification)
	{
		using namespace AngelscriptStaticJITAotGeneration;
		const FStaticJITAotGenerationResult Result =
			RunForTesting(
				EStaticJITAotGenerationMode::Verify,
				EStaticJITAotGenerationTestVariant::
					AdvertiseUnsupportedEmitterCapability);
		const FStaticJITAotEmitterFailureFallbackObservation& Observation =
			Result.EmitterFailureFallback;

		ASSERT_THAT(IsFalse(Result.bSuccess,
			TEXT("Differential verification must reject an eligible TypedAST function that unexpectedly falls back")));
		ASSERT_THAT(IsTrue(Observation.bCaptured,
			TEXT("The production generator result must expose the exact fallback function")));
		ASSERT_THAT(IsTrue(Observation.bTypedEligibilityPassed,
			TEXT("The injected capability must fail in emission after TypedAST eligibility passed")));
		ASSERT_THAT(IsFalse(Observation.FunctionKey.IsEmpty(),
			TEXT("The fallback observation must retain the stable function identity")));
		ASSERT_THAT(AreEqual(FString(TEXT("bytecode")),
			Observation.FinalBackendId,
			TEXT("The production fallback chain must select BytecodeJIT")));
		ASSERT_THAT(IsTrue(Observation.bFinalFunctionEmitted,
			TEXT("BytecodeJIT fallback must still emit the function")));
		ASSERT_THAT(IsTrue(Observation.bBytecodeAttemptEmitted,
			TEXT("The backend-attempt trace must record the emitted BytecodeJIT attempt")));
		ASSERT_THAT(AreEqual(1, Observation.ProviderFunctionCountForKey,
			TEXT("The Provider must contain exactly one final registration for the stable FunctionKey")));
		ASSERT_THAT(IsFalse(Observation.bTypedBodyPresent,
			TEXT("An emitter failure must not leak a partial TypedAST body into the final module source")));
		ASSERT_THAT(IsTrue(Observation.TypedAttemptDetail.StartsWith(
			TEXT("EmitterFailure:"),
			ESearchCase::CaseSensitive), Observation.TypedAttemptDetail));
		ASSERT_THAT(IsTrue(Observation.TypedAttemptDetail.Contains(
			TEXT("UnsupportedInstrumentationCapabilities"),
			ESearchCase::CaseSensitive), Observation.TypedAttemptDetail));
		ASSERT_THAT(IsTrue(Result.Error.Contains(
			TEXT("did not use the production TypedAST backend"),
			ESearchCase::CaseSensitive), Result.Error));
		ASSERT_THAT(IsTrue(Result.Error.Contains(
			TEXT("EmitterFailure"), ESearchCase::CaseSensitive), Result.Error));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
