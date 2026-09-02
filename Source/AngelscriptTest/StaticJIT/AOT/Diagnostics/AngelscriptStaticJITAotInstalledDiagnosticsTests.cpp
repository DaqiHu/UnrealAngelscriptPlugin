#include "CQTest.h"

#include "StaticJIT/AngelscriptJITProviderDiagnostics.h"
#include "StaticJIT/AngelscriptJITProviderRegistry.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_WITH_STATIC_JIT_DIAGNOSTICS

namespace AngelscriptStaticJITAotInstalledDiagnosticsTests_Private
{
	FAngelscriptHash256 MakeHash(const TCHAR* Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("static-jit-installed-diagnostics-test"));
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	void VMEntry(FScriptExecution&, asDWORD*, asQWORD*)
	{
	}

	struct FProvider final : IAngelscriptJITArtifactProvider
	{
		FAngelscriptJITReferenceSlot Reference;
		FAngelscriptJITNativeCallSite NativeCallSite;
		FAngelscriptJITArtifactEntry Entry;
		const TCHAR* FallbackChain[2] = {TEXT("typed-ast"), TEXT("bytecode")};
		FAngelscriptJITProviderCanonicalFunctionDiagnostic Semantic;
		FAngelscriptJITProviderTypedCallDiagnostic Call;
		FAngelscriptJITProviderBackendAttemptDiagnostic Attempts[2];
		FAngelscriptJITProviderFunctionDiagnostic Function;
		FAngelscriptJITProviderDiagnostics Diagnostics;
		FAngelscriptJITProviderView View;

		FProvider()
		{
			Reference.Reference.Kind =
				EAngelscriptArtifactReferenceKind::EnvironmentSymbol;
			Reference.Reference.StableKey = MakeHash(TEXT("PrintTarget"));
			Reference.Reference.ExpectedAbi = MakeHash(TEXT("PrintAbi"));

			NativeCallSite.Route = EAngelscriptJITNativeCallSiteRoute::Bridge;
			NativeCallSite.CanonicalDeclaration = TEXT("void Print(string Message)");
			NativeCallSite.EmittedCppCallee =
				TEXT("AngelscriptTypedASTJIT::InvokeBound<void, FString>");
			NativeCallSite.RuntimeDllCore =
				TEXT("AngelscriptTypedASTJIT::InvokeBoundViaVM");
			NativeCallSite.MetadataRowSymbol = TEXT("ASJIT_Call_Print_4a92d812");
			NativeCallSite.RegisteredTarget = TEXT("FAngelscriptBinds::Print");
			NativeCallSite.StableTargetKey = Reference.Reference.StableKey;
			NativeCallSite.ExpectedAbi = Reference.Reference.ExpectedAbi;
			NativeCallSite.ReferenceSlotIndex = 0;

			Entry.ModuleKey.Hash = MakeHash(TEXT("Module"));
			Entry.FunctionKey.Hash = MakeHash(TEXT("Function"));
			Entry.ExecutionHash = MakeHash(TEXT("Execution"));
			Entry.DebugHash = MakeHash(TEXT("Debug"));
			Entry.EntryAbiHash = MakeHash(TEXT("EntryAbi"));
			Entry.EntryPoints.VMEntry = &VMEntry;
			Entry.ReferenceSlots = &Reference;
			Entry.ReferenceSlotCount = 1;
			Entry.NativeCallSites = &NativeCallSite;
			Entry.NativeCallSiteCount = 1;

			Semantic.FunctionKey = Entry.FunctionKey;
			Semantic.Role = TEXT("UFunctionRoot");
			Semantic.InvocationKind = TEXT("Global");
			Semantic.ReceiverKind = TEXT("None");
			Semantic.CanonicalASTState = TEXT("Verified");
			Semantic.CleanupPlanState = TEXT("VerifiedEmpty");
			Semantic.LifetimeProtocolRevision = 1;
			Semantic.LifetimeStableKey = MakeHash(TEXT("LifetimeStable"));
			Semantic.LifetimeABIKey = MakeHash(TEXT("LifetimeABI"));
			Semantic.bNativeObjectFrameABIAvailable = 1;
			Semantic.bCleanupPlanCoversAllTransfers = 1;
			Semantic.EligibilityReason = TEXT("None");
			Semantic.EligibilityDetail = TEXT("eligible");
			Semantic.EligibilitySource.Section =
				TEXT("/Angelscript/Game/Diagnostics.as");
			Semantic.EligibilitySource.Row = 5;
			Semantic.EligibilitySource.Column = 1;

			Call.CallerFunctionKey = Entry.FunctionKey;
			Call.CalleeFunctionKey.Hash = MakeHash(TEXT("PrintFunction"));
			Call.ExpressionIndex = 4;
			Call.SourceFlags =
				EAngelscriptJITProviderDiagnosticSourceFlags::HasAuthoredSource;
			Call.ProcessedSource.Section =
				TEXT("/Angelscript/Game/Diagnostics.processed.as");
			Call.ProcessedSource.Row = 10;
			Call.ProcessedSource.Column = 7;
			Call.AuthoredSource.Section =
				TEXT("/Angelscript/Game/Diagnostics.as");
			Call.AuthoredSource.Row = 8;
			Call.AuthoredSource.Column = 3;
			Call.GeneratedSource.Section = TEXT("");
			Call.TargetKind = TEXT("SystemFunction");
			Call.Disposition = TEXT("Bridge");
			Call.CalleeEligibilityReason = TEXT("None");
			Call.CalleeEligibilityDetail = TEXT("bridge eligible");
			Call.CalleeEligibilitySource.Section =
				Call.ProcessedSource.Section;
			Call.NativeLinkageValidation = TEXT("Valid");
			Call.NativeLinkageValidationDetail = TEXT("");
			Call.CanonicalDeclaration =
				NativeCallSite.CanonicalDeclaration;
			Call.EmittedRoute = NativeCallSite.Route;
			Call.EmittedCppCallee = NativeCallSite.EmittedCppCallee;
			Call.RuntimeDllCore = NativeCallSite.RuntimeDllCore;
			Call.MetadataRowSymbol = NativeCallSite.MetadataRowSymbol;
			Call.RegisteredTarget = NativeCallSite.RegisteredTarget;
			Call.DirectCppSymbol = TEXT("");
			Call.StableTargetKey = NativeCallSite.StableTargetKey;
			Call.ExpectedAbi = NativeCallSite.ExpectedAbi;
			Call.ReferenceSlotIndex = 0;

			Attempts[0].BackendId = TEXT("typed-ast");
			Attempts[0].Disposition = TEXT("Emitted");
			Attempts[0].Detail = TEXT("typed root emitted");
			Attempts[0].Flags =
				EAngelscriptJITProviderDiagnosticAttemptFlags::HasTypedDiagnostic
				| EAngelscriptJITProviderDiagnosticAttemptFlags::HasSemanticFunction
				| EAngelscriptJITProviderDiagnosticAttemptFlags::HasCallClosure;
			Attempts[0].FallbackReason = TEXT("None");
			Attempts[0].TypedDetail = TEXT("eligible");
			Attempts[0].ProcessedSource.Section =
				TEXT("/Angelscript/Game/Diagnostics.processed.as");
			Attempts[0].PrimarySemanticFunctionIndex = 0;
			Attempts[0].SemanticFunctionStart = 0;
			Attempts[0].SemanticFunctionCount = 1;
			Attempts[0].CallStart = 0;
			Attempts[0].CallCount = 1;
			Attempts[0].RequiredCapabilities =
				EAngelscriptJITExecutionCapability::FramePosition;
			Attempts[0].AvailableCapabilities =
				EAngelscriptJITExecutionCapability::FramePosition;
			Attempts[0].ExecutionRequirementsIdentity =
				MakeHash(TEXT("Requirements"));
			Attempts[0].AvailableCapabilityProfileIdentity =
				MakeHash(TEXT("Capabilities"));
			Attempts[0].ProfileKind = TEXT("PositionOnly");
			Attempts[0].ExceptionControlState = TEXT("DirectExecutionState");
			Attempts[0].ExceptionPayloadState = TEXT("DirectFailureRecord");
			Attempts[0].bRecursionGuardAvailable = 1;

			Attempts[1].BackendId = TEXT("bytecode");
			Attempts[1].Disposition = TEXT("Unsupported");
			Attempts[1].Detail = TEXT("not attempted after Typed emission");
			Attempts[1].FallbackReason = TEXT("");
			Attempts[1].TypedDetail = TEXT("");
			Attempts[1].ProcessedSource.Section = TEXT("");
			Attempts[1].PrimarySemanticFunctionIndex = INDEX_NONE;
			Attempts[1].SemanticFunctionStart = 1;
			Attempts[1].CallStart = 1;

			Function.FunctionKey = Entry.FunctionKey;
			Function.ActualBackendId = TEXT("typed-ast");
			Function.Disposition = TEXT("Emitted");
			Function.Detail = TEXT("provider entry emitted");
			Function.BackendAttemptStart = 0;
			Function.BackendAttemptCount = 2;

			Diagnostics.RequestKind = TEXT("StaticJITArtifact");
			Diagnostics.RequestedBackendId = TEXT("typed-ast");
			Diagnostics.CaptureState = TEXT("VerifiedCanonicalAST");
			Diagnostics.FallbackChain = FallbackChain;
			Diagnostics.FallbackChainCount = UE_ARRAY_COUNT(FallbackChain);
			Diagnostics.SourceCompilePassCount = 1;
			Diagnostics.CompiledModuleCount = 1;
			Diagnostics.CompiledFunctionCount = 1;
			Diagnostics.EmitModuleCount = 1;
			Diagnostics.bHasVerifiedCanonicalAST = 1;
			Diagnostics.TaskError = TEXT("");
			Diagnostics.Functions = &Function;
			Diagnostics.FunctionCount = 1;
			Diagnostics.BackendAttempts = Attempts;
			Diagnostics.BackendAttemptCount = UE_ARRAY_COUNT(Attempts);
			Diagnostics.SemanticFunctions = &Semantic;
			Diagnostics.SemanticFunctionCount = 1;
			Diagnostics.Calls = &Call;
			Diagnostics.CallCount = 1;

			View.ProviderId.Hash = MakeHash(TEXT("Provider"));
			View.ProviderName = TEXT("Installed Diagnostics Provider");
			View.OwnerModuleName = TEXT("AngelscriptTest");
			View.ArtifactProfile.Hash = MakeHash(TEXT("Profile"));
			View.NativeEnvironmentFingerprint = MakeHash(TEXT("Environment"));
			Entry.ArtifactProfile = View.ArtifactProfile;
			Entry.NativeEnvironmentFingerprint =
				View.NativeEnvironmentFingerprint;
			View.Entries = &Entry;
			View.EntryCount = 1;
			View.Diagnostics = &Diagnostics;
			RebuildIdentity();
		}

		void RebuildIdentity()
		{
			Diagnostics.DiagnosticDigest =
				FAngelscriptJITProviderManifest::BuildDiagnosticsDigest(
					Diagnostics);
			View.ArtifactSetDigest =
				FAngelscriptJITProviderManifest::BuildArtifactSetDigest(
					MakeArrayView(&Entry, 1));
			View.ProviderGeneration =
				FAngelscriptJITProviderManifest::BuildProviderGeneration(View);
		}

		const FAngelscriptJITProviderView* GetAngelscriptJITProviderView()
			const override
		{
			return &View;
		}
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotInstalledDiagnosticTests,
	"Angelscript.TestModule.StaticJIT.AOT.Diagnostics.Installed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(UnavailableCanonicalFactsRequireAnAbsentLifetimeSummary)
	{
		using namespace
			AngelscriptStaticJITAotInstalledDiagnosticsTests_Private;
		FProvider Provider;
		Provider.Semantic.CanonicalASTState = TEXT("Missing");
		Provider.Semantic.CleanupPlanState = TEXT("Unverified");
		Provider.Semantic.LifetimeProtocolRevision = 0;
		Provider.Semantic.LifetimeStableKey = {};
		Provider.Semantic.LifetimeABIKey = {};
		Provider.Semantic.bNativeObjectFrameABIAvailable = 0;
		Provider.Semantic.bCleanupPlanCoversAllTransfers = 0;
		Provider.RebuildIdentity();
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProviderManifest::Validate(Provider.View).IsValid(),
			TEXT("a non-verified Canonical diagnostic must not invent a lifetime summary")));

		Provider.Semantic.CanonicalASTState = TEXT("Verified");
		Provider.RebuildIdentity();
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProviderManifest::Validate(Provider.View).IsValid(),
			TEXT("Canonical AST verification is independent from lifetime protocol authentication")));

		Provider.Semantic.CleanupPlanState = TEXT("VerifiedEmpty");
		Provider.RebuildIdentity();
		const FAngelscriptJITProviderValidationResult MissingAuthenticatedSummary =
			FAngelscriptJITProviderManifest::Validate(Provider.View);
		ASSERT_THAT(IsTrue(MissingAuthenticatedSummary.Code
			== EAngelscriptJITProviderValidationCode::InvalidDiagnosticString));
		ASSERT_THAT(AreEqual(0, MissingAuthenticatedSummary.DiagnosticSemanticIndex));

		Provider.Semantic.CleanupPlanState = TEXT("Unverified");

		Provider.Semantic.LifetimeProtocolRevision = 1;
		Provider.RebuildIdentity();
		const FAngelscriptJITProviderValidationResult PartialSummary =
			FAngelscriptJITProviderManifest::Validate(Provider.View);
		ASSERT_THAT(IsTrue(PartialSummary.Code
			== EAngelscriptJITProviderValidationCode::InvalidDiagnosticString));
		ASSERT_THAT(AreEqual(0, PartialSummary.DiagnosticSemanticIndex));
	}

	TEST_METHOD(AuthenticatedPartialConstructionDoesNotRequireLexicalExitPlans)
	{
		using namespace
			AngelscriptStaticJITAotInstalledDiagnosticsTests_Private;
		FProvider Provider;
		Provider.Semantic.CleanupPlanState = TEXT("PartialConstruction");
		Provider.Semantic.LifetimeActionFlags = 1u;
		Provider.Semantic.LifetimeSupportedExitMask = 1u << 5;
		Provider.Semantic.LifetimeRecordCount = 2u;
		Provider.Semantic.LifetimeExitPlanFlags = 0u;
		Provider.Semantic.LifetimeExitPlanCount = 0u;
		Provider.Semantic.bNativeObjectFrameABIAvailable = 0u;
		Provider.Semantic.bHasExceptionCleanup = 1u;
		Provider.RebuildIdentity();

		const FAngelscriptJITProviderValidationResult PartialConstruction =
			FAngelscriptJITProviderManifest::Validate(Provider.View);
		ASSERT_THAT(IsTrue(PartialConstruction.IsValid(),
			TEXT("authenticated constructor commits are not lexical transfer cleanup plans")));

		Provider.Semantic.CleanupPlanState = TEXT("NonEmpty");
		Provider.RebuildIdentity();
		const FAngelscriptJITProviderValidationResult OrdinaryLifetimeWithoutPlan =
			FAngelscriptJITProviderManifest::Validate(Provider.View);
		ASSERT_THAT(IsTrue(OrdinaryLifetimeWithoutPlan.Code
			== EAngelscriptJITProviderValidationCode::InvalidDiagnosticString,
			TEXT("ordinary non-empty lifetime summaries must retain an authenticated exit plan")));
		ASSERT_THAT(AreEqual(
			0, OrdinaryLifetimeWithoutPlan.DiagnosticSemanticIndex));
	}

	TEST_METHOD(FullExpressionLifetimeDiagnosticRequiresAuthenticatedPlanShape)
	{
		using namespace
			AngelscriptStaticJITAotInstalledDiagnosticsTests_Private;
		FProvider Provider;
		Provider.Semantic.CleanupPlanState = TEXT("ScriptDestructor");
		Provider.Semantic.LifetimeActionFlags = 1u;
		Provider.Semantic.LifetimeExitPlanFlags = 1u << 3;
		Provider.Semantic.LifetimeSupportedExitMask = (1u << 0) | (1u << 5);
		Provider.Semantic.LifetimeRecordCount = 1u;
		Provider.Semantic.LifetimeExitPlanCount = 1u;
		Provider.Semantic.bNativeObjectFrameABIAvailable = 0u;
		Provider.Semantic.bHasExceptionCleanup = 1u;
		Provider.RebuildIdentity();

		const FAngelscriptJITProviderValidationResult FullExpression =
			FAngelscriptJITProviderManifest::Validate(Provider.View);
		ASSERT_THAT(IsTrue(FullExpression.IsValid(),
			TEXT("one authenticated full-expression temporary must be a valid provider diagnostic")));
		const FAngelscriptHash256 ArtifactSet = Provider.View.ArtifactSetDigest;
		const FAngelscriptJITProviderGeneration ProviderGeneration =
			Provider.View.ProviderGeneration;
		FAngelscriptJITProviderRegistry Registry;
		const FAngelscriptJITProviderRegistrationResult Registered =
			Registry.RegisterProvider(&Provider, Provider);
		ASSERT_THAT(IsTrue(Registered.Code
			== EAngelscriptJITProviderRegistrationCode::Registered));
		const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> Snapshot = Registry.GetSnapshot();
		ASSERT_THAT(AreEqual(1, Snapshot->Providers.Num()));
		ASSERT_THAT(IsTrue(Snapshot->Providers[0]->Diagnostics.IsSet()));
		ASSERT_THAT(AreEqual(1u << 3, Snapshot->Providers[0]->Diagnostics
			->SemanticFunctions[0].LifetimeExitPlanFlags,
			TEXT("registry snapshot must deep-copy the full-expression plan bit")));
		ASSERT_THAT(IsTrue(Provider.View.ArtifactSetDigest == ArtifactSet));
		ASSERT_THAT(IsTrue(
			Provider.View.ProviderGeneration == ProviderGeneration));
		ASSERT_THAT(IsTrue(Registry.UnregisterOwner(&Provider)));

		Provider.Diagnostics.SchemaRevision =
			FAngelscriptJITProviderDiagnosticsAbi::SchemaRevision - 1u;
		Provider.RebuildIdentity();
		const FAngelscriptJITProviderValidationResult OldSchema =
			FAngelscriptJITProviderManifest::Validate(Provider.View);
		ASSERT_THAT(IsTrue(OldSchema.Code
			== EAngelscriptJITProviderValidationCode::UnsupportedDiagnosticSchema,
			TEXT("the previous diagnostics schema must fail closed")));
		Provider.Diagnostics.SchemaRevision =
			FAngelscriptJITProviderDiagnosticsAbi::SchemaRevision;

		Provider.Semantic.LifetimeExitPlanFlags = (1u << 3) | (1u << 4);
		Provider.RebuildIdentity();
		const FAngelscriptJITProviderValidationResult UnknownPlanBit =
			FAngelscriptJITProviderManifest::Validate(Provider.View);
		ASSERT_THAT(IsTrue(UnknownPlanBit.Code
			== EAngelscriptJITProviderValidationCode::InvalidDiagnosticString));
		ASSERT_THAT(AreEqual(0, UnknownPlanBit.DiagnosticSemanticIndex));

		Provider.Semantic.LifetimeExitPlanFlags = 1u << 3;
		Provider.Semantic.LifetimeExitPlanCount = 0u;
		Provider.RebuildIdentity();
		const FAngelscriptJITProviderValidationResult MissingPlan =
			FAngelscriptJITProviderManifest::Validate(Provider.View);
		ASSERT_THAT(IsTrue(MissingPlan.Code
			== EAngelscriptJITProviderValidationCode::InvalidDiagnosticString));
		ASSERT_THAT(AreEqual(0, MissingPlan.DiagnosticSemanticIndex));

		Provider.Semantic.LifetimeExitPlanCount = 1u;
		Provider.Semantic.LifetimeRecordCount = 0u;
		Provider.RebuildIdentity();
		const FAngelscriptJITProviderValidationResult MissingRecord =
			FAngelscriptJITProviderManifest::Validate(Provider.View);
		ASSERT_THAT(IsTrue(MissingRecord.Code
			== EAngelscriptJITProviderValidationCode::InvalidDiagnosticString));
		ASSERT_THAT(AreEqual(0, MissingRecord.DiagnosticSemanticIndex));

		Provider.Semantic.LifetimeRecordCount = 1u;
		Provider.Semantic.CleanupPlanState = TEXT("Unverified");
		Provider.RebuildIdentity();
		const FAngelscriptJITProviderValidationResult UnverifiedSummary =
			FAngelscriptJITProviderManifest::Validate(Provider.View);
		ASSERT_THAT(IsTrue(UnverifiedSummary.Code
			== EAngelscriptJITProviderValidationCode::InvalidDiagnosticString));
		ASSERT_THAT(AreEqual(0, UnverifiedSummary.DiagnosticSemanticIndex));

		Provider.Semantic.CleanupPlanState = TEXT("ScriptDestructor");
		Provider.Semantic.LifetimeActionFlags = 1u << 1;
		Provider.RebuildIdentity();
		const FAngelscriptJITProviderValidationResult WrongActionFamily =
			FAngelscriptJITProviderManifest::Validate(Provider.View);
		ASSERT_THAT(IsTrue(WrongActionFamily.Code
			== EAngelscriptJITProviderValidationCode::InvalidDiagnosticString,
			TEXT("a full-expression temporary cannot forge a release action")));
		ASSERT_THAT(AreEqual(0, WrongActionFamily.DiagnosticSemanticIndex));

		Provider.Semantic.LifetimeActionFlags = 1u;
		Provider.Semantic.LifetimeSupportedExitMask = 1u << 0;
		Provider.RebuildIdentity();
		const FAngelscriptJITProviderValidationResult WrongExitMask =
			FAngelscriptJITProviderManifest::Validate(Provider.View);
		ASSERT_THAT(IsTrue(WrongExitMask.Code
			== EAngelscriptJITProviderValidationCode::InvalidDiagnosticString,
			TEXT("a standalone full-expression temporary requires normal and exception exits")));
		ASSERT_THAT(AreEqual(0, WrongExitMask.DiagnosticSemanticIndex));

		Provider.Semantic.LifetimeSupportedExitMask =
			(1u << 0) | (1u << 5);
		Provider.Semantic.bNativeObjectFrameABIAvailable = 1u;
		Provider.RebuildIdentity();
		const FAngelscriptJITProviderValidationResult ForgedNativeFrame =
			FAngelscriptJITProviderManifest::Validate(Provider.View);
		ASSERT_THAT(IsTrue(ForgedNativeFrame.Code
			== EAngelscriptJITProviderValidationCode::InvalidDiagnosticString,
			TEXT("schema 3 must not claim native execution for this unsupported frame family")));
		ASSERT_THAT(AreEqual(0, ForgedNativeFrame.DiagnosticSemanticIndex));
	}

	TEST_METHOD(SystemCallUsesStableTargetIdentityWithoutScriptFunctionKey)
	{
		using namespace
			AngelscriptStaticJITAotInstalledDiagnosticsTests_Private;
		FProvider Provider;
		Provider.Call.CalleeFunctionKey = {};
		Provider.RebuildIdentity();

		ASSERT_THAT(IsTrue(
			FAngelscriptJITProviderManifest::Validate(Provider.View).IsValid()));

		Provider.Call.TargetKind = TEXT("ScriptFunction");
		Provider.RebuildIdentity();
		const FAngelscriptJITProviderValidationResult ScriptWithoutKey =
			FAngelscriptJITProviderManifest::Validate(Provider.View);
		ASSERT_THAT(IsTrue(ScriptWithoutKey.Code
			== EAngelscriptJITProviderValidationCode::InvalidDiagnosticString));
		ASSERT_THAT(AreEqual(0, ScriptWithoutKey.DiagnosticCallIndex));
	}

	TEST_METHOD(ProviderDiagnosticsValidateDeepCopyAndDoNotChangeArtifactIdentity)
	{
		using namespace
			AngelscriptStaticJITAotInstalledDiagnosticsTests_Private;
		FProvider Provider;

		ASSERT_THAT(AreEqual(8u, FAngelscriptJITProviderAbi::Revision));
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProviderManifest::Validate(Provider.View).IsValid()));
		const FAngelscriptHash256 ArtifactSet = Provider.View.ArtifactSetDigest;
		const FAngelscriptJITProviderGeneration ProviderGeneration =
			Provider.View.ProviderGeneration;

		FAngelscriptJITProviderRegistry Registry;
		const FAngelscriptJITProviderRegistrationResult First =
			Registry.RegisterProvider(&Provider, Provider);
		ASSERT_THAT(IsTrue(First.Code
			== EAngelscriptJITProviderRegistrationCode::Registered));
		TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> Snapshot = Registry.GetSnapshot();
		ASSERT_THAT(AreEqual(1, Snapshot->Providers.Num()));
		ASSERT_THAT(IsTrue(Snapshot->Providers[0]->Diagnostics.IsSet()));
		ASSERT_THAT(AreEqual(FString(TEXT("typed-ast")),
			Snapshot->Providers[0]->Diagnostics->RequestedBackendId));
		ASSERT_THAT(AreEqual(FString(TEXT("Bridge")),
			Snapshot->Providers[0]->Diagnostics->Calls[0].Disposition));
		ASSERT_THAT(AreEqual(1u, Snapshot->Providers[0]->Diagnostics
			->SemanticFunctions[0].LifetimeProtocolRevision));
		ASSERT_THAT(IsTrue(Snapshot->Providers[0]->Diagnostics
			->SemanticFunctions[0].LifetimeStableKey
				== Provider.Semantic.LifetimeStableKey));
		ASSERT_THAT(IsTrue(Snapshot->Providers[0]->Diagnostics
			->SemanticFunctions[0].LifetimeABIKey
				== Provider.Semantic.LifetimeABIKey));

		Provider.Diagnostics.RequestedBackendId = TEXT("bytecode");
		Provider.Diagnostics.DiagnosticDigest =
			FAngelscriptJITProviderManifest::BuildDiagnosticsDigest(
				Provider.Diagnostics);
		ASSERT_THAT(IsTrue(Provider.View.ArtifactSetDigest == ArtifactSet));
		ASSERT_THAT(IsTrue(
			Provider.View.ProviderGeneration == ProviderGeneration));
		ASSERT_THAT(AreEqual(FString(TEXT("typed-ast")),
			Snapshot->Providers[0]->Diagnostics->RequestedBackendId));

		const FAngelscriptJITProviderRegistrationResult Updated =
			Registry.RegisterProvider(&Provider, Provider);
		ASSERT_THAT(IsTrue(Updated.Code
			== EAngelscriptJITProviderRegistrationCode::Replaced));
		Snapshot = Registry.GetSnapshot();
		ASSERT_THAT(AreEqual(FString(TEXT("bytecode")),
			Snapshot->Providers[0]->Diagnostics->RequestedBackendId));
		ASSERT_THAT(IsTrue(Registry.UnregisterOwner(&Provider)));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS && AS_WITH_STATIC_JIT_DIAGNOSTICS
