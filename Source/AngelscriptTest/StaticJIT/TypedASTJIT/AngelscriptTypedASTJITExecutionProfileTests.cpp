#include "CQTest.h"

#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITExecutionProfile.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITExecutionProfileTests,
	"Angelscript.TestModule.StaticJIT.Eligibility.TypedASTJIT.ExecutionProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using ECapability = EAngelscriptTypedASTJITDirectCapability;

	static FAngelscriptHash256 RepeatedByteHash(const uint8 Value)
	{
		FBlake3Hash::ByteArray Bytes{};
		for (uint8& Byte : Bytes)
		{
			Byte = Value;
		}
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	static FAngelscriptTypedASTJITGenerationCapabilityProfile MakeProfile(
		const ECapability Capabilities)
	{
		FAngelscriptTypedASTJITGenerationCapabilityProfile Profile;
		FString Error;
		check(BuildAngelscriptTypedASTJITGenerationCapabilityProfile(
			Capabilities, Profile, &Error));
		check(Error.IsEmpty());
		return Profile;
	}

	static FAngelscriptTypedASTJITExecutionRequirementsSnapshot MakeRequirements(
		const FAngelscriptTypedASTJITExecutionRequirementsInput& Input)
	{
		FAngelscriptTypedASTJITExecutionRequirementsSnapshot Snapshot;
		FString Error;
		check(BuildAngelscriptTypedASTJITExecutionRequirementsSnapshot(
			Input, Snapshot, &Error));
		check(Error.IsEmpty());
		return Snapshot;
	}

public:
	TEST_METHOD(PositionOnlyProfileDoesNotClaimDebuggerOrControlParity)
	{
		const FAngelscriptTypedASTJITGenerationCapabilityProfile Profile =
			MakeProfile(ECapability::FramePosition);
		FAngelscriptTypedASTJITExecutionRequirementsInput Input;
		Input.bRequireFramePosition = true;
		const FAngelscriptTypedASTJITExecutionRequirementsSnapshot Requirements =
			MakeRequirements(Input);

		ASSERT_THAT(IsTrue(Profile.IsValid()));
		ASSERT_THAT(IsTrue(Requirements.IsValid()));
		ASSERT_THAT(AreEqual(ECapability::FramePosition,
			Profile.AvailableCapabilities));
		ASSERT_THAT(AreEqual(ECapability::FramePosition,
			Requirements.RequiredCapabilities));
		const FAngelscriptTypedASTJITExecutionRouteEligibility Route =
			EvaluateAngelscriptTypedASTJITExecutionRoute(Profile, Requirements);
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITExecutionRoute::TypedASTJIT, Route.Route));
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITFallbackReason::None, Route.Reason));
		ASSERT_THAT(AreEqual(FString(TEXT("ExecutionCapabilitiesSatisfied")),
			Route.StableDetail));
		ASSERT_THAT(AreEqual(FString(TEXT("FramePosition")),
			LexToString(Profile.AvailableCapabilities)));
	}

	TEST_METHOD(UnavailableRequirementsSelectVmWithStableTypedReasons)
	{
		struct FCase
		{
			const TCHAR* Name;
			FAngelscriptTypedASTJITExecutionRequirementsInput Input;
			ECapability ExpectedRequired;
			EAngelscriptTypedASTJITFallbackReason ExpectedReason;
			const TCHAR* ExpectedDetail;
		};

		TArray<FCase> Cases;
		{
			FCase& Case = Cases.AddDefaulted_GetRef();
			Case.Name = TEXT("Breakpoint");
			Case.Input.bBreakpointActive = true;
			Case.ExpectedRequired =
				ECapability::LineCallback | ECapability::DebuggerStep;
			Case.ExpectedReason =
				EAngelscriptTypedASTJITFallbackReason::UnsupportedExecutionObservability;
			Case.ExpectedDetail = TEXT("DebuggerStepUnavailable");
		}
		{
			FCase& Case = Cases.AddDefaulted_GetRef();
			Case.Name = TEXT("Step");
			Case.Input.bStepActive = true;
			Case.ExpectedRequired =
				ECapability::LineCallback | ECapability::DebuggerStep;
			Case.ExpectedReason =
				EAngelscriptTypedASTJITFallbackReason::UnsupportedExecutionObservability;
			Case.ExpectedDetail = TEXT("DebuggerStepUnavailable");
		}
		{
			FCase& Case = Cases.AddDefaulted_GetRef();
			Case.Name = TEXT("Locals");
			Case.Input.bDebuggerLocalsRequired = true;
			Case.ExpectedRequired = ECapability::DebuggerLocals;
			Case.ExpectedReason =
				EAngelscriptTypedASTJITFallbackReason::UnsupportedExecutionObservability;
			Case.ExpectedDetail = TEXT("DebuggerLocalsUnavailable");
		}
		{
			FCase& Case = Cases.AddDefaulted_GetRef();
			Case.Name = TEXT("Coverage");
			Case.Input.bCoverageRecording = true;
			Case.ExpectedRequired =
				ECapability::LineCallback | ECapability::Coverage;
			Case.ExpectedReason =
				EAngelscriptTypedASTJITFallbackReason::UnsupportedExecutionObservability;
			Case.ExpectedDetail = TEXT("CoverageHookUnavailable");
		}
		{
			FCase& Case = Cases.AddDefaulted_GetRef();
			Case.Name = TEXT("LoopTimeout");
			Case.Input.bLoopTimeoutRequired = true;
			Case.ExpectedRequired = ECapability::LoopTimeout;
			Case.ExpectedReason =
				EAngelscriptTypedASTJITFallbackReason::UnsupportedExecutionControl;
			Case.ExpectedDetail = TEXT("LoopTimeoutSafepointUnavailable");
		}
		{
			FCase& Case = Cases.AddDefaulted_GetRef();
			Case.Name = TEXT("AbortSuspend");
			Case.Input.bAbortOrSuspendPollingRequired = true;
			Case.ExpectedRequired = ECapability::AbortSuspend;
			Case.ExpectedReason =
				EAngelscriptTypedASTJITFallbackReason::UnsupportedExecutionControl;
			Case.ExpectedDetail = TEXT("AbortSuspendSafepointUnavailable");
		}
		{
			FCase& Case = Cases.AddDefaulted_GetRef();
			Case.Name = TEXT("Recursion");
			Case.Input.bRecursionBudgetRequired = true;
			Case.ExpectedRequired = ECapability::RecursionBudget;
			Case.ExpectedReason =
				EAngelscriptTypedASTJITFallbackReason::UnsupportedExecutionControl;
			Case.ExpectedDetail = TEXT("RecursionGuardUnavailable");
		}

		const FAngelscriptTypedASTJITGenerationCapabilityProfile EmptyProfile =
			MakeProfile(ECapability::None);
		for (const FCase& Case : Cases)
		{
			const FAngelscriptTypedASTJITExecutionRequirementsSnapshot Requirements =
				MakeRequirements(Case.Input);
			ASSERT_THAT(AreEqual(Case.ExpectedRequired,
				Requirements.RequiredCapabilities));
			const FAngelscriptTypedASTJITExecutionRouteEligibility Route =
				EvaluateAngelscriptTypedASTJITExecutionRoute(
					EmptyProfile, Requirements);
			ASSERT_THAT(AreEqual(
				EAngelscriptTypedASTJITExecutionRoute::VM, Route.Route));
			ASSERT_THAT(AreEqual(Case.ExpectedReason, Route.Reason));
			ASSERT_THAT(AreEqual(FString(Case.ExpectedDetail),
				Route.StableDetail));
			ASSERT_THAT(AreEqual(Case.ExpectedRequired,
				Route.MissingCapabilities));
		}
	}

	TEST_METHOD(CompleteProfileAcceptsTheNormalizedRequirementMatrix)
	{
		const ECapability AllCapabilities =
			ECapability::FramePosition
			| ECapability::LineCallback
			| ECapability::DebuggerStep
			| ECapability::DebuggerLocals
			| ECapability::Coverage
			| ECapability::LoopTimeout
			| ECapability::AbortSuspend
			| ECapability::RecursionBudget;
		const FAngelscriptTypedASTJITGenerationCapabilityProfile Profile =
			MakeProfile(AllCapabilities);
		FAngelscriptTypedASTJITExecutionRequirementsInput Input;
		Input.bRequireFramePosition = true;
		Input.bBreakpointActive = true;
		Input.bStepActive = true;
		Input.bDebuggerLocalsRequired = true;
		Input.bCoverageRecording = true;
		Input.bLoopTimeoutRequired = true;
		Input.bAbortOrSuspendPollingRequired = true;
		Input.bRecursionBudgetRequired = true;
		const FAngelscriptTypedASTJITExecutionRequirementsSnapshot Requirements =
			MakeRequirements(Input);

		ASSERT_THAT(AreEqual(AllCapabilities,
			Requirements.RequiredCapabilities));
		const FAngelscriptTypedASTJITExecutionRouteEligibility Route =
			EvaluateAngelscriptTypedASTJITExecutionRoute(Profile, Requirements);
		ASSERT_THAT(AreEqual(
			EAngelscriptTypedASTJITExecutionRoute::TypedASTJIT, Route.Route));
		ASSERT_THAT(AreEqual(ECapability::None, Route.MissingCapabilities));
	}

	TEST_METHOD(CapabilityProfileAndRequirementsParticipateInStableIdentity)
	{
		const FAngelscriptTypedASTJITGenerationCapabilityProfile PositionOnly =
			MakeProfile(ECapability::FramePosition);
		const FAngelscriptTypedASTJITGenerationCapabilityProfile Instrumented =
			MakeProfile(ECapability::FramePosition
				| ECapability::LineCallback | ECapability::Coverage);
		const FAngelscriptTypedASTJITGenerationCapabilityProfile Rebuilt =
			MakeProfile(ECapability::FramePosition);
		ASSERT_THAT(IsTrue(PositionOnly.IdentityHash == Rebuilt.IdentityHash));
		ASSERT_THAT(IsFalse(PositionOnly.IdentityHash == Instrumented.IdentityHash));

		FAngelscriptTypedASTJITExecutionRequirementsInput EmptyInput;
		FAngelscriptTypedASTJITExecutionRequirementsInput CoverageInput;
		CoverageInput.bCoverageRecording = true;
		const FAngelscriptTypedASTJITExecutionRequirementsSnapshot Empty =
			MakeRequirements(EmptyInput);
		const FAngelscriptTypedASTJITExecutionRequirementsSnapshot Coverage =
			MakeRequirements(CoverageInput);
		ASSERT_THAT(IsFalse(Empty.IdentityHash == Coverage.IdentityHash));

		const FAngelscriptHash256 SourceContent = RepeatedByteHash(0x42);
		const FAngelscriptHash256 PositionContent =
			BuildAngelscriptTypedASTJITProfiledContentIdentity(
				SourceContent, PositionOnly);
		const FAngelscriptHash256 InstrumentedContent =
			BuildAngelscriptTypedASTJITProfiledContentIdentity(
				SourceContent, Instrumented);
		ASSERT_THAT(IsFalse(PositionContent.IsZero()));
		ASSERT_THAT(IsFalse(PositionContent == InstrumentedContent));
		ASSERT_THAT(IsTrue(PositionContent ==
			BuildAngelscriptTypedASTJITProfiledContentIdentity(
				SourceContent, Rebuilt)));
	}

	TEST_METHOD(UnknownCapabilityBitsFailClosedWithoutAnIdentity)
	{
		FAngelscriptTypedASTJITGenerationCapabilityProfile Profile;
		FString Error;
		ASSERT_THAT(IsFalse(
			BuildAngelscriptTypedASTJITGenerationCapabilityProfile(
				static_cast<ECapability>(1u << 15), Profile, &Error)));
		ASSERT_THAT(IsFalse(Profile.IsValid()));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("UnknownCapabilityBits"))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
