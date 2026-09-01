#include "CQTest.h"

#include "Core/Artifacts/AngelscriptArtifactIdentity.h"
#include "StaticJIT/AngelscriptStaticJITGenerator.h"
#include "StaticJIT/StaticJITDiagnostics.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_WITH_STATIC_JIT_DIAGNOSTICS \
	&& AS_CAN_GENERATE_JIT

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotGenerationDiagnosticTests,
	"Angelscript.TestModule.StaticJIT.AOT.Diagnostics.Generation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ArtifactRequestPreservesBackendSelectionAndAttempts)
	{
		FAngelscriptArtifactCanonicalWriter KeyWriter(
			TEXT("static-jit-generation-diagnostics-test"));
		KeyWriter.WriteString(TEXT("FallbackFunction"));

		FAngelscriptStaticJITGeneratorOutput GeneratorOutput;
		GeneratorOutput.Diagnostics.RequestedBackendId =
			FAngelscriptStaticJITBackendId::TypedAST();
		GeneratorOutput.Diagnostics.CaptureProfile =
			EAngelscriptStaticJITCaptureProfile::VerifiedCanonicalAST;
		GeneratorOutput.Diagnostics.FallbackChain = {
			FAngelscriptStaticJITBackendId::TypedAST(),
			FAngelscriptStaticJITBackendId::Bytecode(),
		};
		GeneratorOutput.Diagnostics.CompiledSourceGraph.SourceCompilePassCount = 1;
		GeneratorOutput.Diagnostics.CompiledSourceGraph.ModuleCount = 1;
		GeneratorOutput.Diagnostics.CompiledSourceGraph.FunctionCount = 1;
		GeneratorOutput.Diagnostics.CompiledSourceGraph.bHasVerifiedCanonicalAST = true;

		FAngelscriptStaticJITFunctionResult& Function =
			GeneratorOutput.Functions.AddDefaulted_GetRef();
		Function.FunctionKey.Hash = KeyWriter.FinalizeHash();
		Function.Disposition =
			EAngelscriptStaticJITFunctionDisposition::Emitted;
		Function.ActualBackendId =
			FAngelscriptStaticJITBackendId::Bytecode();
		Function.Detail = TEXT("Bytecode fallback emitted.");

		FAngelscriptStaticJITBackendAttemptDiagnostic& TypedAttempt =
			Function.BackendAttempts.AddDefaulted_GetRef();
		TypedAttempt.BackendId = FAngelscriptStaticJITBackendId::TypedAST();
		TypedAttempt.Disposition =
			EAngelscriptStaticJITFunctionDisposition::Unsupported;
		TypedAttempt.Detail = TEXT("MissingCanonicalAST: no sealed Canonical AST.");

		FAngelscriptStaticJITBackendAttemptDiagnostic& BytecodeAttempt =
			Function.BackendAttempts.AddDefaulted_GetRef();
		BytecodeAttempt.BackendId =
			FAngelscriptStaticJITBackendId::Bytecode();
		BytecodeAttempt.Disposition =
			EAngelscriptStaticJITFunctionDisposition::Emitted;
		BytecodeAttempt.Detail = Function.Detail;

		FAngelscriptJITGenerationFunction EmittedFunction;
		EmittedFunction.FunctionKey = Function.FunctionKey;
		EmittedFunction.bHasVMEntry = true;
		EmittedFunction.bHasRawEntry = true;
		EmittedFunction.bHasParmsEntry = false;
		Function.EmittedFunction = MoveTemp(EmittedFunction);

		FStaticJITDiagnostics::FSnapshot Snapshot;
		Snapshot.ArtifactRequest =
			FStaticJITDiagnostics::BuildArtifactRequestRecord(GeneratorOutput);
		const FStaticJITDiagnostics::FJsonResult Result =
			FStaticJITDiagnostics::Serialize(Snapshot);

		ASSERT_THAT(IsTrue(Result.IsSuccess(), Result.Detail));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"schemaVersion\":9"))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"requestKind\":\"StaticJITArtifact\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"requestedBackend\":\"typed-ast\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"captureState\":\"VerifiedCanonicalAST\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"actualBackend\":\"bytecode\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"hasVMEntry\":true"))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"hasRawEntry\":true"))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"hasParmsEntry\":false"))));

		const int32 TypedAttemptOffset = Result.Json.Find(
			TEXT("\"backend\":\"typed-ast\""));
		const int32 BytecodeAttemptOffset = Result.Json.Find(
			TEXT("\"backend\":\"bytecode\""),
			ESearchCase::CaseSensitive, ESearchDir::FromStart,
			TypedAttemptOffset + 1);
		ASSERT_THAT(IsTrue(TypedAttemptOffset != INDEX_NONE));
		ASSERT_THAT(IsTrue(BytecodeAttemptOffset > TypedAttemptOffset));
		ASSERT_THAT(IsFalse(Result.Json.Contains(TEXT("0x"),
			ESearchCase::IgnoreCase)));
	}

	TEST_METHOD(TypedFallbackReasonAndSourceAreStructuredForEveryReason)
	{
		FAngelscriptStaticJITGeneratorOutput GeneratorOutput;
		GeneratorOutput.Diagnostics.RequestedBackendId =
			FAngelscriptStaticJITBackendId::TypedAST();
		GeneratorOutput.Diagnostics.CaptureProfile =
			EAngelscriptStaticJITCaptureProfile::VerifiedCanonicalAST;

		const uint8 LastReason = static_cast<uint8>(
			EAngelscriptTypedASTJITFallbackReason::BackendUnavailable);
		for (uint8 ReasonIndex = 0; ReasonIndex <= LastReason; ++ReasonIndex)
		{
			const EAngelscriptTypedASTJITFallbackReason Reason =
				static_cast<EAngelscriptTypedASTJITFallbackReason>(ReasonIndex);
			FAngelscriptArtifactCanonicalWriter KeyWriter(
				TEXT("static-jit-typed-fallback-diagnostics-test"));
			KeyWriter.WriteUInt8(ReasonIndex);

			FAngelscriptStaticJITFunctionResult& Function =
				GeneratorOutput.Functions.AddDefaulted_GetRef();
			Function.FunctionKey.Hash = KeyWriter.FinalizeHash();
			Function.Disposition =
				EAngelscriptStaticJITFunctionDisposition::Emitted;
			Function.ActualBackendId =
				FAngelscriptStaticJITBackendId::Bytecode();
			Function.Detail = TEXT("opaque-final-detail");

			FAngelscriptStaticJITBackendAttemptDiagnostic& Attempt =
				Function.BackendAttempts.AddDefaulted_GetRef();
			Attempt.BackendId =
				FAngelscriptStaticJITBackendId::TypedAST();
			Attempt.Disposition = Reason
				== EAngelscriptTypedASTJITFallbackReason::None
				? EAngelscriptStaticJITFunctionDisposition::Emitted
				: EAngelscriptStaticJITFunctionDisposition::Unsupported;
			Attempt.Detail = TEXT(
				"opaque-attempt-detail-with-no-reason-token");
			Attempt.TypedDiagnostic.Emplace();
			Attempt.TypedDiagnostic->FallbackReason = Reason;
			Attempt.TypedDiagnostic->Detail = TEXT("structured-detail");
			Attempt.TypedDiagnostic->ProcessedSource.Section =
				TEXT("Script/Diagnostics/TypedFallback.as");
			Attempt.TypedDiagnostic->ProcessedSource.Row = ReasonIndex + 1;
			Attempt.TypedDiagnostic->ProcessedSource.Column = 7;
		}

		FStaticJITDiagnostics::FSnapshot Snapshot;
		Snapshot.ArtifactRequest =
			FStaticJITDiagnostics::BuildArtifactRequestRecord(GeneratorOutput);
		const FStaticJITDiagnostics::FJsonResult Result =
			FStaticJITDiagnostics::Serialize(Snapshot);
		ASSERT_THAT(IsTrue(Result.IsSuccess(), Result.Detail));
		ASSERT_THAT(IsFalse(Result.Json.Contains(
			TEXT("opaque-attempt-detail-with-no-reason-token ->"))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"processedSource\":{\"section\":\"Script/Diagnostics/TypedFallback.as\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(TEXT("\"column\":7"))));

		for (uint8 ReasonIndex = 0; ReasonIndex <= LastReason; ++ReasonIndex)
		{
			const EAngelscriptTypedASTJITFallbackReason Reason =
				static_cast<EAngelscriptTypedASTJITFallbackReason>(ReasonIndex);
			ASSERT_THAT(IsTrue(Result.Json.Contains(FString::Printf(
				TEXT("\"fallbackReason\":\"%s\""),
				LexToString(Reason)))));
		}
	}

	TEST_METHOD(TypedRootAndHelperFactsRemainStructured)
	{
		FAngelscriptArtifactCanonicalWriter RootKeyWriter(
			TEXT("static-jit-typed-root-diagnostics-test"));
		RootKeyWriter.WriteString(TEXT("Root"));
		FAngelscriptArtifactCanonicalWriter HelperKeyWriter(
			TEXT("static-jit-typed-root-diagnostics-test"));
		HelperKeyWriter.WriteString(TEXT("Helper"));

		FAngelscriptStaticJITGeneratorOutput GeneratorOutput;
		GeneratorOutput.Diagnostics.RequestedBackendId =
			FAngelscriptStaticJITBackendId::TypedAST();
		GeneratorOutput.Diagnostics.CaptureProfile =
			EAngelscriptStaticJITCaptureProfile::VerifiedCanonicalAST;

		FAngelscriptStaticJITFunctionResult& Function =
			GeneratorOutput.Functions.AddDefaulted_GetRef();
		Function.FunctionKey.Hash = RootKeyWriter.FinalizeHash();
		Function.ActualBackendId =
			FAngelscriptStaticJITBackendId::TypedAST();
		Function.Disposition =
			EAngelscriptStaticJITFunctionDisposition::Emitted;
		Function.TypedDiagnostic.Emplace();
		Function.TypedDiagnostic->FallbackReason =
			EAngelscriptTypedASTJITFallbackReason::None;
		Function.TypedDiagnostic->SemanticFunction.Emplace();
		FAngelscriptStaticJITCanonicalFunctionDiagnostic& RootFacts =
			Function.TypedDiagnostic->SemanticFunction.GetValue();
		RootFacts.FunctionKey = Function.FunctionKey;
		RootFacts.Role =
			EAngelscriptStaticJITTypedClosureFunctionRole::UFunctionRoot;
		RootFacts.RawTraitBits = 0x01020304u;
		RootFacts.UnknownTraitBits = 0x80000000u;
		RootFacts.InvocationKind =
			EAngelscriptTypedASTJITInvocationKind::InstanceMethod;
		RootFacts.ReceiverKind =
			EAngelscriptTypedASTJITReceiverKind::NativeObjectThis;
		RootFacts.ReceiverParameterIndex = INDEX_NONE;
		RootFacts.CanonicalASTState =
			EAngelscriptStaticJITCanonicalASTState::Verified;
		RootFacts.CleanupPlanState =
			EAngelscriptTypedASTJITCleanupPlanState::VerifiedEmpty;
		RootFacts.LifetimeSummary.ProtocolRevision = 1;
		RootFacts.LifetimeSummary.StableKey = RootFacts.FunctionKey.Hash;
		RootFacts.LifetimeSummary.ABIKey = RootFacts.FunctionKey.Hash;
		RootFacts.LifetimeSummary.bNativeObjectFrameABIAvailable = true;
		RootFacts.bCleanupPlanCoversAllTransfers = true;

		Function.TypedDiagnostic->CallClosure.Emplace();
		FAngelscriptStaticJITTypedCallClosureDiagnostic& Closure =
			Function.TypedDiagnostic->CallClosure.GetValue();
		Closure.bEligible = true;
		Closure.RequiredCapabilities =
			EAngelscriptTypedASTJITDirectCapability::FramePosition
			| EAngelscriptTypedASTJITDirectCapability::RecursionBudget;
		Closure.AvailableCapabilities = Closure.RequiredCapabilities;
		Closure.ProfileKind =
			EAngelscriptStaticJITTypedExecutionProfileKind::PositionOnly;
		Closure.ExceptionControlState =
			EAngelscriptStaticJITExceptionControlState::DirectExecutionState;
		Closure.ExceptionPayloadState =
			EAngelscriptStaticJITExceptionPayloadState::DirectFailureRecord;
		Closure.bRecursionGuardAvailable = true;
		Closure.Functions.Add(RootFacts);

		FAngelscriptStaticJITCanonicalFunctionDiagnostic& HelperFacts =
			Closure.Functions.AddDefaulted_GetRef();
		HelperFacts.FunctionKey.Hash = HelperKeyWriter.FinalizeHash();
		HelperFacts.Role = EAngelscriptStaticJITTypedClosureFunctionRole::
			InternalSemanticHelper;
		HelperFacts.InvocationKind =
			EAngelscriptTypedASTJITInvocationKind::Global;
		HelperFacts.ReceiverKind = EAngelscriptTypedASTJITReceiverKind::None;
		HelperFacts.CanonicalASTState =
			EAngelscriptStaticJITCanonicalASTState::Verified;
		HelperFacts.CleanupPlanState =
			EAngelscriptTypedASTJITCleanupPlanState::VerifiedEmpty;
		HelperFacts.LifetimeSummary.ProtocolRevision = 1;
		HelperFacts.LifetimeSummary.StableKey = HelperFacts.FunctionKey.Hash;
		HelperFacts.LifetimeSummary.ABIKey = HelperFacts.FunctionKey.Hash;
		HelperFacts.LifetimeSummary.bNativeObjectFrameABIAvailable = true;
		HelperFacts.bCleanupPlanCoversAllTransfers = true;
		HelperFacts.bRecursiveComponent = true;

		FStaticJITDiagnostics::FSnapshot Snapshot;
		Snapshot.ArtifactRequest =
			FStaticJITDiagnostics::BuildArtifactRequestRecord(GeneratorOutput);
		const FStaticJITDiagnostics::FJsonResult Result =
			FStaticJITDiagnostics::Serialize(Snapshot);

		ASSERT_THAT(IsTrue(Result.IsSuccess(), Result.Detail));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"role\":\"UFunctionRoot\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"rawTraitBits\":\"0x01020304\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"unknownTraitBits\":\"0x80000000\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"invocationKind\":\"InstanceMethod\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"receiverKind\":\"NativeObjectThis\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"canonicalASTState\":\"Verified\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"cleanupPlanState\":\"VerifiedEmpty\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"lifetimeProtocolRevision\":1"))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"nativeObjectFrameABIAvailable\":true"))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"requiredCapabilities\":\"FramePosition|RecursionBudget\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"availableCapabilities\":\"FramePosition|RecursionBudget\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"profileKind\":\"PositionOnly\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"exceptionControlState\":\"DirectExecutionState\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"exceptionPayloadState\":\"DirectFailureRecord\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"role\":\"InternalSemanticHelper\""))));
		ASSERT_THAT(IsTrue(Result.Json.Contains(
			TEXT("\"recursiveComponent\":true"))));

		Closure.ExceptionPayloadState =
			EAngelscriptStaticJITExceptionPayloadState::
				DirectFailureRecordAndNestedBridgeAdoption;
		Snapshot.ArtifactRequest =
			FStaticJITDiagnostics::BuildArtifactRequestRecord(GeneratorOutput);
		const FStaticJITDiagnostics::FJsonResult MixedPayloadResult =
			FStaticJITDiagnostics::Serialize(Snapshot);
		ASSERT_THAT(IsTrue(
			MixedPayloadResult.IsSuccess(), MixedPayloadResult.Detail));
		ASSERT_THAT(IsTrue(MixedPayloadResult.Json.Contains(TEXT(
			"\"exceptionPayloadState\":\"DirectFailureRecordAndNestedBridgeAdoption\""))));
	}

	TEST_METHOD(PrePublicationFailuresUseStableCategoryAndBoundary)
	{
		FStaticJITDiagnostics::FSnapshot StaleSnapshot;
		FStaticJITDiagnostics::FPrePublicationFailureRecord& Stale =
			StaleSnapshot.PrePublicationFailure.Emplace();
		Stale.Category = FStaticJITDiagnostics::
			EPrePublicationFailureCategory::AuthoritativeEngineStale;
		Stale.AffectedBoundary = FStaticJITDiagnostics::
			EPrePublicationBoundary::PrimarySourceAuthority;
		Stale.Detail = TEXT(
			"Run the established Hot Reload/recompile flow, then retry.");

		const FStaticJITDiagnostics::FJsonResult StaleJson =
			FStaticJITDiagnostics::Serialize(StaleSnapshot);
		ASSERT_THAT(IsTrue(StaleJson.IsSuccess(), StaleJson.Detail));
		ASSERT_THAT(IsTrue(StaleJson.Json.Contains(
			TEXT("\"schemaVersion\":9"))));
		ASSERT_THAT(IsTrue(StaleJson.Json.Contains(
			TEXT("\"category\":\"AuthoritativeEngineStale\""))));
		ASSERT_THAT(IsTrue(StaleJson.Json.Contains(
			TEXT("\"affectedBoundary\":\"PrimarySourceAuthority\""))));
		ASSERT_THAT(IsTrue(StaleJson.Json.Contains(
			TEXT("\"generationEngineCreated\":false"))));
		ASSERT_THAT(IsTrue(StaleJson.Json.Contains(
			TEXT("\"providerPackagingStarted\":false"))));
		ASSERT_THAT(IsTrue(StaleJson.Json.Contains(
			TEXT("\"providerPublished\":false"))));
		ASSERT_THAT(IsFalse(StaleJson.Json.Contains(
			TEXT("\"providers\":[{"))));

		FStaticJITDiagnostics::FSnapshot ContainmentSnapshot;
		FStaticJITDiagnostics::FPrePublicationFailureRecord& Containment =
			ContainmentSnapshot.PrePublicationFailure.Emplace();
		Containment.Category = FStaticJITDiagnostics::
			EPrePublicationFailureCategory::GenerationContainmentFailure;
		Containment.AffectedBoundary = FStaticJITDiagnostics::
			EPrePublicationBoundary::ProviderRegistry;
		Containment.Detail = TEXT(
			"Generation changed the primary Provider Registry snapshot.");
		Containment.bGenerationEngineCreated = true;

		const FStaticJITDiagnostics::FJsonResult ContainmentJson =
			FStaticJITDiagnostics::Serialize(ContainmentSnapshot);
		ASSERT_THAT(IsTrue(ContainmentJson.IsSuccess(),
			ContainmentJson.Detail));
		ASSERT_THAT(IsTrue(ContainmentJson.Json.Contains(
			TEXT("\"category\":\"GenerationContainmentFailure\""))));
		ASSERT_THAT(IsTrue(ContainmentJson.Json.Contains(
			TEXT("\"affectedBoundary\":\"ProviderRegistry\""))));
		ASSERT_THAT(IsTrue(ContainmentJson.Json.Contains(
			TEXT("\"generationEngineCreated\":true"))));
		ASSERT_THAT(IsTrue(ContainmentJson.Json.Contains(
			TEXT("\"providerPackagingStarted\":false"))));
		ASSERT_THAT(IsTrue(ContainmentJson.Json.Contains(
			TEXT("\"providerPublished\":false"))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS && AS_WITH_STATIC_JIT_DIAGNOSTICS
	// && AS_CAN_GENERATE_JIT
