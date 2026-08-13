#include "RuntimeJIT/AngelscriptRuntimeJITState.h"

#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "Async/Async.h"
#include "CQTest.h"
#include "Features/IModularFeatures.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptRuntimeJITStateTests,
	"Angelscript.TestModule.RuntimeJIT.Coordinator.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FAngelscriptHash256 MakeHash(const TCHAR* Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("runtime-jit-state-test"));
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	static FAngelscriptRuntimeJITBackendId BackendId()
	{
		FAngelscriptRuntimeJITBackendId Result;
		check(FAngelscriptRuntimeJITBackendId::TryParse(
			TEXT("fake-policy"), Result));
		return Result;
	}

	static void RuntimeVmEntry(FScriptExecution&, asDWORD*, asQWORD*)
	{
	}

	struct FBackendControl : TSharedFromThis<FBackendControl, ESPMode::ThreadSafe>
	{
		FBackendControl()
		{
			FirstCompileEntered =
				FPlatformProcess::GetSynchEventFromPool(true);
			ReleaseFirstCompile =
				FPlatformProcess::GetSynchEventFromPool(true);
			CompileStateChanged =
				FPlatformProcess::GetSynchEventFromPool(true);
		}

		~FBackendControl()
		{
			FPlatformProcess::ReturnSynchEventToPool(FirstCompileEntered);
			FPlatformProcess::ReturnSynchEventToPool(ReleaseFirstCompile);
			FPlatformProcess::ReturnSynchEventToPool(CompileStateChanged);
		}

		void SetOutcome(
			const EAngelscriptRuntimeJITCompileOutcome InOutcome)
		{
			Outcome.Store(static_cast<uint8>(InOutcome));
		}

		EAngelscriptRuntimeJITCompileOutcome GetOutcome() const
		{
			return static_cast<EAngelscriptRuntimeJITCompileOutcome>(
				Outcome.Load());
		}

		void ReleaseBlockedCompile()
		{
			ReleaseFirstCompile->Trigger();
		}

		TAtomic<uint8> Outcome{
			static_cast<uint8>(EAngelscriptRuntimeJITCompileOutcome::Compiled)};
		TAtomic<bool> bBlockFirstCompile{false};
		TAtomic<bool> bReleaseBlockedCompileOnCancel{true};
		FThreadSafeCounter CompileCount;
		FThreadSafeCounter FinishedCompileCount;
		FThreadSafeCounter CancelCount;
		FThreadSafeCounter LeaseReleaseCount;
		TAtomic<uint64> LastCancellationGeneration{0};
		FEvent* FirstCompileEntered = nullptr;
		FEvent* ReleaseFirstCompile = nullptr;
		FEvent* CompileStateChanged = nullptr;
	};

	struct FLeasePayload
	{
		explicit FLeasePayload(
			TSharedRef<FBackendControl, ESPMode::ThreadSafe> InControl)
			: Control(MoveTemp(InControl))
		{
		}

		TSharedRef<FBackendControl, ESPMode::ThreadSafe> Control;
	};

	static void ReleaseLease(void* UserData)
	{
		TUniquePtr<FLeasePayload> Payload(
			static_cast<FLeasePayload*>(UserData));
		Payload->Control->LeaseReleaseCount.Increment();
		Payload->Control->CompileStateChanged->Trigger();
	}

	class FFakeSession final : public IAngelscriptRuntimeJITBackendSession
	{
	public:
		explicit FFakeSession(
			TSharedRef<FBackendControl, ESPMode::ThreadSafe> InControl)
			: Control(MoveTemp(InControl))
		{
		}

		FAngelscriptRuntimeJITCompileResult Compile(
			const FAngelscriptRuntimeJITCompileSnapshotView& Snapshot) override
		{
			const int32 CompileIndex = Control->CompileCount.Increment();
			Control->CompileStateChanged->Trigger();
			if (CompileIndex == 1 && Control->bBlockFirstCompile.Load())
			{
				Control->FirstCompileEntered->Trigger();
				Control->ReleaseFirstCompile->Wait(5000);
			}

			FAngelscriptRuntimeJITCompileResult Result;
			Result.Outcome =
				Control->LastCancellationGeneration.Load()
					== Snapshot.Header.CancellationGeneration
				? EAngelscriptRuntimeJITCompileOutcome::Cancelled
				: Control->GetOutcome();
			Result.FunctionRevision = Snapshot.Header.FunctionRevision;
			switch (Result.Outcome)
			{
			case EAngelscriptRuntimeJITCompileOutcome::Compiled:
				Result.Reason = EAngelscriptRuntimeJITDiagnosticReason::None;
				Result.EntryAbiHash = Snapshot.Header.EntryAbiHash;
				Result.VMEntry = &RuntimeVmEntry;
				Result.CodeSize = 64;
				Result.CompileLatencyMicroseconds = 7;
				{
					FAngelscriptRuntimeJITCodeLeaseView Lease;
					Lease.UserData = new FLeasePayload(Control);
					Lease.Release = &ReleaseLease;
					Result.CodeLease =
						FAngelscriptRuntimeJITCodeLease::Adopt(Lease);
				}
				break;
			case EAngelscriptRuntimeJITCompileOutcome::Unsupported:
				Result.Reason =
					EAngelscriptRuntimeJITDiagnosticReason::UnsupportedOpcode;
				break;
			case EAngelscriptRuntimeJITCompileOutcome::Cancelled:
				Result.Reason = EAngelscriptRuntimeJITDiagnosticReason::Cancelled;
				break;
			case EAngelscriptRuntimeJITCompileOutcome::Stale:
				Result.Reason = EAngelscriptRuntimeJITDiagnosticReason::StaleRevision;
				break;
			case EAngelscriptRuntimeJITCompileOutcome::BackendFailure:
				Result.Reason = EAngelscriptRuntimeJITDiagnosticReason::BackendFailure;
				break;
			case EAngelscriptRuntimeJITCompileOutcome::InvalidInput:
				Result.Reason =
					EAngelscriptRuntimeJITDiagnosticReason::MalformedSnapshot;
				break;
			}
			Control->FinishedCompileCount.Increment();
			Control->CompileStateChanged->Trigger();
			return Result;
		}

		void Cancel(const uint64 CancellationGeneration) override
		{
			Control->LastCancellationGeneration.Store(CancellationGeneration);
			Control->CancelCount.Increment();
			if (Control->bReleaseBlockedCompileOnCancel.Load())
			{
				Control->ReleaseFirstCompile->Trigger();
			}
			Control->CompileStateChanged->Trigger();
		}

	private:
		TSharedRef<FBackendControl, ESPMode::ThreadSafe> Control;
	};

	class FFakeFactory final : public IAngelscriptRuntimeJITBackendFactory
	{
	public:
		FFakeFactory()
			: Control(MakeShared<FBackendControl, ESPMode::ThreadSafe>())
		{
			Metadata.BackendId = BackendId();
			Metadata.DisplayName = TEXT("Runtime JIT Policy Fake");
			Metadata.SupportedPlatforms =
				EAngelscriptRuntimeJITPlatform::Win64;
			Metadata.SupportedConfigurations =
				EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment;
		}

		const FAngelscriptRuntimeJITBackendFactoryMetadata*
			GetMetadata() const override
		{
			return &Metadata;
		}

		TUniquePtr<IAngelscriptRuntimeJITBackendSession> CreateSession(
			const FAngelscriptRuntimeJITBackendSessionConfig&) override
		{
			++CreateCount;
			return MakeUnique<FFakeSession>(Control);
		}

		FAngelscriptRuntimeJITBackendFactoryMetadata Metadata;
		TSharedRef<FBackendControl, ESPMode::ThreadSafe> Control;
		int32 CreateCount = 0;
	};

	class FScopedFactoryRegistration final
	{
	public:
		explicit FScopedFactoryRegistration(FFakeFactory& InFactory)
			: Factory(InFactory)
		{
			IModularFeatures::Get().RegisterModularFeature(
				IAngelscriptRuntimeJITBackendFactory::FeatureName(), &Factory);
		}

		~FScopedFactoryRegistration()
		{
			IModularFeatures::Get().UnregisterModularFeature(
				IAngelscriptRuntimeJITBackendFactory::FeatureName(), &Factory);
		}

	private:
		FFakeFactory& Factory;
	};

	static FAngelscriptRuntimeJITSelectionConfig MakeSelection(
		const EAngelscriptRuntimeJITCompilePolicy Policy,
		const uint64 Generation)
	{
		FAngelscriptRuntimeJITSelectionConfig Config;
		Config.BackendId = BackendId();
		Config.Platform = EAngelscriptRuntimeJITPlatform::Win64;
		Config.Configuration =
			EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment;
		Config.EngineNamespace = 707;
		Config.CompilePolicy = Policy;
		Config.BackendConfigurationGeneration = Generation;
		Config.CancellationGeneration = Generation;
		return Config;
	}

	static FAngelscriptRuntimeJITCompileSnapshot MakeOwnedSnapshot(
		const TCHAR* Revision,
		const uint64 PublicationOrdinal,
		const uint64 CancellationGeneration,
		const TCHAR* FunctionIdentity = TEXT("policy-function"),
		const uint32 EntryAbiRevision =
			FAngelscriptRuntimeJITBackendAbi::EntryAbiRevision)
	{
		FAngelscriptRuntimeJITCompileSnapshotHeader Header;
		Header.BackendId = BackendId();
		Header.Platform = EAngelscriptRuntimeJITPlatform::Win64;
		Header.Configuration =
			EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment;
		Header.PointerWidth = sizeof(void*);
		Header.Endianness = EAngelscriptRuntimeJITEndianness::Little;
		Header.CallingConvention =
			EAngelscriptRuntimeJITCallingConvention::AngelscriptVMEntry;
		Header.Invocation = EAngelscriptRuntimeJITInvocationKind::Global;
		Header.Receiver = EAngelscriptRuntimeJITReceiverKind::None;
		Header.FunctionProfile =
			EAngelscriptRuntimeJITFunctionProfile::ScalarWholeFunction;
		Header.EntryAbiRevision = EntryAbiRevision;
		Header.EngineNamespace = 707;
		Header.PublicationOrdinal = PublicationOrdinal;
		Header.CancellationGeneration = CancellationGeneration;
		Header.ModuleKey.Hash = MakeHash(TEXT("policy-module"));
		Header.FunctionKey.Hash = MakeHash(FunctionIdentity);
		Header.FunctionRevision = MakeHash(Revision);
		Header.EntryAbiHash = MakeHash(TEXT("entry-abi"));
		return FAngelscriptRuntimeJITCompileSnapshot::FromOwnedData(
			Header, {static_cast<asDWORD>(asBC_RET)}, {}, {0}, {}, {});
	}

	static bool WaitForCompileCount(
		const TSharedRef<FBackendControl, ESPMode::ThreadSafe>& Control,
		const int32 Expected)
	{
		const double Deadline = FPlatformTime::Seconds() + 5.0;
		while (Control->CompileCount.GetValue() < Expected
			&& FPlatformTime::Seconds() < Deadline)
		{
			Control->CompileStateChanged->Wait(50);
			Control->CompileStateChanged->Reset();
		}
		return Control->CompileCount.GetValue() >= Expected;
	}

	static bool WaitForFinishedCount(
		const TSharedRef<FBackendControl, ESPMode::ThreadSafe>& Control,
		const int32 Expected)
	{
		const double Deadline = FPlatformTime::Seconds() + 5.0;
		while (Control->FinishedCompileCount.GetValue() < Expected
			&& FPlatformTime::Seconds() < Deadline)
		{
			Control->CompileStateChanged->Wait(50);
			Control->CompileStateChanged->Reset();
		}
		return Control->FinishedCompileCount.GetValue() >= Expected;
	}

	static bool WaitForCancelCount(
		const TSharedRef<FBackendControl, ESPMode::ThreadSafe>& Control,
		const int32 Expected)
	{
		const double Deadline = FPlatformTime::Seconds() + 5.0;
		while (Control->CancelCount.GetValue() < Expected
			&& FPlatformTime::Seconds() < Deadline)
		{
			Control->CompileStateChanged->Wait(50);
			Control->CompileStateChanged->Reset();
		}
		return Control->CancelCount.GetValue() >= Expected;
	}

	static FAngelscriptFunctionRoute MakeRoute(
		asIScriptFunction& Function,
		const TCHAR* ExecutionRevision)
	{
		FAngelscriptFunctionRoute Route;
		Route.ModuleKey.Hash = MakeHash(TEXT("replacement-module"));
		Route.Identity.FunctionKey.Hash =
			MakeHash(TEXT("replacement-function"));
		Route.Identity.Content.Execution = MakeHash(ExecutionRevision);
		Route.Identity.Content.Debug = MakeHash(TEXT("replacement-debug"));
		Route.Identity.Profile.Hash = MakeHash(TEXT("replacement-profile"));
		Route.CanonicalDeclaration = UTF8_TO_TCHAR(
			Function.GetDeclaration(false, false, false));
		Route.Function = &Function;
		Route.NumericFunctionId = Function.GetId();
		Route.bHasVerifiedArtifactIdentity = true;
		return Route;
	}

	static FAngelscriptRuntimeJITCompileSnapshot CaptureReplacementSnapshot(
		asIScriptFunction& Function,
		const FAngelscriptFunctionRoute& Route,
		const uint64 PublicationOrdinal,
		const uint64 CancellationGeneration)
	{
		FAngelscriptRuntimeJITSnapshotCaptureInput Input;
		Input.Function = &Function;
		Input.Route = &Route;
		Input.BackendId = BackendId();
		Input.Platform = EAngelscriptRuntimeJITPlatform::Win64;
		Input.Configuration =
			EAngelscriptRuntimeJITBuildConfiguration::EditorDevelopment;
		Input.EngineNamespace = 707;
		Input.PublicationOrdinal = PublicationOrdinal;
		Input.CancellationGeneration = CancellationGeneration;
		FAngelscriptRuntimeJITCompileSnapshot Snapshot;
		const FAngelscriptRuntimeJITSnapshotCaptureResult Capture =
			FAngelscriptRuntimeJITCompileSnapshot::Capture(Input, Snapshot);
		check(Capture.IsCaptured());
		return Snapshot;
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(EagerSyncPublishesOnceAndCoalescesTheSameRevision)
	{
		FFakeFactory Factory;
		FScopedFactoryRegistration Registration(Factory);
		{
			FAngelscriptRuntimeJITRequestStateMachine Machine(
				MakeSelection(EAngelscriptRuntimeJITCompilePolicy::EagerSync, 1));
			const FAngelscriptRuntimeJITCompileSnapshot Snapshot =
				MakeOwnedSnapshot(TEXT("sync-v1"), 1, 1);
			const FAngelscriptRuntimeJITObserveResult First =
				Machine.ObserveFunction(Snapshot);
			ASSERT_THAT(IsTrue(First.Action ==
				EAngelscriptRuntimeJITRequestAction::Started));
			ASSERT_THAT(IsTrue(First.State ==
				EAngelscriptRuntimeJITFunctionState::Published));
			ASSERT_THAT(AreEqual(1, Factory.Control->CompileCount.GetValue()));

			FAngelscriptRuntimeJITPublishedCode Published;
			ASSERT_THAT(IsTrue(Machine.TryAcquirePublished(
				Snapshot.GetHeader().FunctionKey,
				Snapshot.GetHeader().FunctionRevision,
				Snapshot.GetHeader().PublicationOrdinal,
				Published)));
			ASSERT_THAT(IsTrue(Published.IsValid()));
			ASSERT_THAT(IsTrue(Published.VMEntry == &RuntimeVmEntry));

			const FAngelscriptRuntimeJITObserveResult Duplicate =
				Machine.ObserveFunction(Snapshot);
			ASSERT_THAT(IsTrue(Duplicate.Action ==
				EAngelscriptRuntimeJITRequestAction::AlreadyPublished));
			ASSERT_THAT(AreEqual(1, Factory.Control->CompileCount.GetValue(),
				TEXT("One revision and selection generation should compile once")));
		}
		ASSERT_THAT(AreEqual(1,
			Factory.Control->LeaseReleaseCount.GetValue(),
			TEXT("Published code should release exactly once with state teardown")));
	}

	TEST_METHOD(InvalidEntryAbiIsReportedAtTheObserveBoundary)
	{
		FFakeFactory Factory;
		FScopedFactoryRegistration Registration(Factory);
		FAngelscriptRuntimeJITRequestStateMachine Machine(
			MakeSelection(EAngelscriptRuntimeJITCompilePolicy::EagerSync, 1));
		const FAngelscriptRuntimeJITCompileSnapshot Snapshot =
			MakeOwnedSnapshot(
				TEXT("invalid-entry-abi"),
				1,
				1,
				TEXT("invalid-entry-abi-function"),
				FAngelscriptRuntimeJITBackendAbi::EntryAbiRevision + 1);

		const FAngelscriptRuntimeJITObserveResult Observed =
			Machine.ObserveFunction(Snapshot);
		ASSERT_THAT(IsTrue(Observed.Action ==
			EAngelscriptRuntimeJITRequestAction::Rejected));
		ASSERT_THAT(IsTrue(Observed.State ==
			EAngelscriptRuntimeJITFunctionState::InvalidInput));
		ASSERT_THAT(IsTrue(Observed.Reason ==
			EAngelscriptRuntimeJITDiagnosticReason::EntryAbiMismatch));
		ASSERT_THAT(AreEqual(0, Factory.Control->CompileCount.GetValue()));
	}

	TEST_METHOD(EagerSyncReportsItsOwnStaleReplacementResult)
	{
		FFakeFactory Factory;
		Factory.Metadata.CompileConcurrency =
			EAngelscriptRuntimeJITCompileConcurrency::ConcurrentPerSession;
		Factory.Control->bBlockFirstCompile.Store(true);
		FScopedFactoryRegistration Registration(Factory);
		FAngelscriptRuntimeJITRequestStateMachine Machine(
			MakeSelection(EAngelscriptRuntimeJITCompilePolicy::EagerSync, 1));
		const FAngelscriptRuntimeJITCompileSnapshot OldSnapshot =
			MakeOwnedSnapshot(TEXT("eager-sync-old"), 1, 1);
		const FAngelscriptRuntimeJITCompileSnapshot NewSnapshot =
			MakeOwnedSnapshot(TEXT("eager-sync-new"), 2, 1);

		TFuture<FAngelscriptRuntimeJITObserveResult> OldObserve = Async(
			EAsyncExecution::ThreadPool,
			[&Machine, OldSnapshot]() mutable
			{
				return Machine.ObserveFunction(OldSnapshot);
			});
		ASSERT_THAT(IsTrue(Factory.Control->FirstCompileEntered->Wait(5000)));

		const FAngelscriptRuntimeJITObserveResult NewObserved =
			Machine.ObserveFunction(NewSnapshot);
		Factory.Control->ReleaseBlockedCompile();
		const FAngelscriptRuntimeJITObserveResult OldObserved = OldObserve.Get();

		ASSERT_THAT(IsTrue(NewObserved.State ==
			EAngelscriptRuntimeJITFunctionState::Published));
		ASSERT_THAT(IsTrue(OldObserved.State ==
			EAngelscriptRuntimeJITFunctionState::Stale));
		ASSERT_THAT(IsTrue(OldObserved.Reason ==
			EAngelscriptRuntimeJITDiagnosticReason::StaleRevision));
		ASSERT_THAT(AreEqual(0, Factory.Control->CancelCount.GetValue()));
	}

	TEST_METHOD(TerminalOutcomesRetryOnlyAfterSelectionGenerationChanges)
	{
		FFakeFactory Factory;
		FScopedFactoryRegistration Registration(Factory);
		Factory.Control->SetOutcome(
			EAngelscriptRuntimeJITCompileOutcome::Unsupported);
		FAngelscriptRuntimeJITRequestStateMachine Machine(
			MakeSelection(EAngelscriptRuntimeJITCompilePolicy::EagerSync, 1));
		const FAngelscriptRuntimeJITCompileSnapshot UnsupportedSnapshot =
			MakeOwnedSnapshot(TEXT("terminal-v1"), 1, 1);
		Machine.ObserveFunction(UnsupportedSnapshot);
		ASSERT_THAT(AreEqual(1, Factory.Control->CompileCount.GetValue()));
		const FAngelscriptRuntimeJITObserveResult Memoized =
			Machine.ObserveFunction(UnsupportedSnapshot);
		ASSERT_THAT(IsTrue(Memoized.Action ==
			EAngelscriptRuntimeJITRequestAction::Memoized));
		ASSERT_THAT(AreEqual(1, Factory.Control->CompileCount.GetValue(),
			TEXT("Unsupported must be terminal for the unchanged generation")));

		Factory.Control->SetOutcome(
			EAngelscriptRuntimeJITCompileOutcome::Compiled);
		ASSERT_THAT(IsTrue(Machine.Reconfigure(
			MakeSelection(EAngelscriptRuntimeJITCompilePolicy::EagerSync, 2))));
		const FAngelscriptRuntimeJITCompileSnapshot RetriedSnapshot =
			MakeOwnedSnapshot(TEXT("terminal-v1"), 1, 2);
		const FAngelscriptRuntimeJITObserveResult Retried =
			Machine.ObserveFunction(RetriedSnapshot);
		ASSERT_THAT(IsTrue(Retried.State ==
			EAngelscriptRuntimeJITFunctionState::Published));
		ASSERT_THAT(AreEqual(2, Factory.Control->CompileCount.GetValue()));
		ASSERT_THAT(AreEqual(2, Factory.CreateCount,
			TEXT("A new backend/config generation owns a new Engine session")));
	}

	TEST_METHOD(UnavailableAndCancelledRequestsDoNotSpinRetry)
	{
		FFakeFactory Factory;
		FAngelscriptRuntimeJITRequestStateMachine Machine(
			MakeSelection(EAngelscriptRuntimeJITCompilePolicy::EagerSync, 1));
		const FAngelscriptRuntimeJITCompileSnapshot MissingSnapshot =
			MakeOwnedSnapshot(TEXT("unavailable-v1"), 1, 1);
		const FAngelscriptRuntimeJITObserveResult Missing =
			Machine.ObserveFunction(MissingSnapshot);
		ASSERT_THAT(IsTrue(Missing.State ==
			EAngelscriptRuntimeJITFunctionState::BackendUnavailable));
		ASSERT_THAT(IsTrue(Machine.ObserveFunction(MissingSnapshot).Action ==
			EAngelscriptRuntimeJITRequestAction::Memoized));

		FScopedFactoryRegistration Registration(Factory);
		Factory.Control->SetOutcome(
			EAngelscriptRuntimeJITCompileOutcome::Cancelled);
		ASSERT_THAT(IsTrue(Machine.Reconfigure(
			MakeSelection(EAngelscriptRuntimeJITCompilePolicy::EagerSync, 2))));
		const FAngelscriptRuntimeJITCompileSnapshot CancelledSnapshot =
			MakeOwnedSnapshot(TEXT("unavailable-v1"), 1, 2);
		Machine.ObserveFunction(CancelledSnapshot);
		ASSERT_THAT(AreEqual(1, Factory.Control->CompileCount.GetValue()));
		ASSERT_THAT(IsTrue(Machine.ObserveFunction(CancelledSnapshot).Action ==
			EAngelscriptRuntimeJITRequestAction::Memoized));
		ASSERT_THAT(AreEqual(1, Factory.Control->CompileCount.GetValue(),
			TEXT("Cancellation retries only after another generation change")));

		Factory.Control->SetOutcome(
			EAngelscriptRuntimeJITCompileOutcome::Compiled);
		ASSERT_THAT(IsTrue(Machine.Reconfigure(
			MakeSelection(EAngelscriptRuntimeJITCompilePolicy::EagerSync, 3))));
		Machine.ObserveFunction(
			MakeOwnedSnapshot(TEXT("unavailable-v1"), 1, 3));
		ASSERT_THAT(AreEqual(2, Factory.Control->CompileCount.GetValue()));
	}

	TEST_METHOD(EagerBackgroundKeepsVmUntilSafePointPublication)
	{
		FFakeFactory Factory;
		Factory.Control->bBlockFirstCompile.Store(true);
		FScopedFactoryRegistration Registration(Factory);
		{
			FAngelscriptRuntimeJITRequestStateMachine Machine(
				MakeSelection(
					EAngelscriptRuntimeJITCompilePolicy::EagerBackground, 1));
			const FAngelscriptRuntimeJITCompileSnapshot Snapshot =
				MakeOwnedSnapshot(TEXT("background-v1"), 1, 1);
			const FAngelscriptRuntimeJITObserveResult Started =
				Machine.ObserveFunction(Snapshot);
			ASSERT_THAT(IsTrue(Started.State ==
				EAngelscriptRuntimeJITFunctionState::Compiling));
			ASSERT_THAT(IsTrue(Factory.Control->FirstCompileEntered->Wait(5000)));

			FAngelscriptRuntimeJITPublishedCode Published;
			ASSERT_THAT(IsFalse(Machine.TryAcquirePublished(
				Snapshot.GetHeader().FunctionKey,
				Snapshot.GetHeader().FunctionRevision,
				Snapshot.GetHeader().PublicationOrdinal,
				Published),
				TEXT("Pending background compilation must retain VM")));
			ASSERT_THAT(IsTrue(Machine.ObserveFunction(Snapshot).Action ==
				EAngelscriptRuntimeJITRequestAction::Coalesced));
			ASSERT_THAT(AreEqual(1, Factory.Control->CompileCount.GetValue()));

			Factory.Control->ReleaseBlockedCompile();
			ASSERT_THAT(IsTrue(WaitForFinishedCount(Factory.Control, 1)));
			ASSERT_THAT(IsFalse(Machine.TryAcquirePublished(
				Snapshot.GetHeader().FunctionKey,
				Snapshot.GetHeader().FunctionRevision,
				Snapshot.GetHeader().PublicationOrdinal,
				Published),
				TEXT("Worker completion alone is not an Engine safe point")));
			const FAngelscriptRuntimeJITProcessResult Processed =
				Machine.ProcessPendingResults();
			ASSERT_THAT(AreEqual(1, Processed.PublishedCount));
			ASSERT_THAT(IsTrue(Machine.TryAcquirePublished(
				Snapshot.GetHeader().FunctionKey,
				Snapshot.GetHeader().FunctionRevision,
				Snapshot.GetHeader().PublicationOrdinal,
				Published)));
		}
		ASSERT_THAT(AreEqual(1,
			Factory.Control->LeaseReleaseCount.GetValue()));
	}

	TEST_METHOD(LazyFirstCallHasOneConcurrentClaimAndEveryCallerUsesVm)
	{
		FFakeFactory Factory;
		Factory.Control->bBlockFirstCompile.Store(true);
		FScopedFactoryRegistration Registration(Factory);
		{
			FAngelscriptRuntimeJITRequestStateMachine Machine(
				MakeSelection(
					EAngelscriptRuntimeJITCompilePolicy::LazyFirstCall, 1));
			const FAngelscriptRuntimeJITCompileSnapshot Snapshot =
				MakeOwnedSnapshot(TEXT("lazy-v1"), 1, 1);
			const FAngelscriptRuntimeJITObserveResult Registered =
				Machine.ObserveFunction(Snapshot);
			ASSERT_THAT(IsTrue(Registered.State ==
				EAngelscriptRuntimeJITFunctionState::WaitingForFirstCall));
			ASSERT_THAT(AreEqual(0, Factory.Control->CompileCount.GetValue()));

			TArray<TFuture<FAngelscriptRuntimeJITLazyCallResult>> Calls;
			for (int32 Index = 0; Index < 4; ++Index)
			{
				Calls.Add(Async(EAsyncExecution::ThreadPool,
					[&Machine, &Snapshot]()
					{
						return Machine.OnLazyFirstCall(
							Snapshot.GetHeader().FunctionKey,
							Snapshot.GetHeader().FunctionRevision,
							Snapshot.GetHeader().PublicationOrdinal);
					}));
			}
			ASSERT_THAT(IsTrue(Factory.Control->FirstCompileEntered->Wait(5000)));
			Factory.Control->ReleaseBlockedCompile();

			int32 ClaimedCount = 0;
			for (TFuture<FAngelscriptRuntimeJITLazyCallResult>& Call : Calls)
			{
				const FAngelscriptRuntimeJITLazyCallResult Result = Call.Get();
				ClaimedCount += Result.bCompileClaimed ? 1 : 0;
				ASSERT_THAT(IsTrue(Result.bUseVmForCurrentCall,
					TEXT("Every already-held lazy route completes through VM")));
			}
			ASSERT_THAT(AreEqual(1, ClaimedCount));
			ASSERT_THAT(AreEqual(1, Factory.Control->CompileCount.GetValue()));

			FAngelscriptRuntimeJITPublishedCode Published;
			ASSERT_THAT(IsFalse(Machine.TryAcquirePublished(
				Snapshot.GetHeader().FunctionKey,
				Snapshot.GetHeader().FunctionRevision,
				Snapshot.GetHeader().PublicationOrdinal,
				Published)));
			ASSERT_THAT(AreEqual(1,
				Machine.ProcessPendingResults().PublishedCount));
			ASSERT_THAT(IsTrue(Machine.TryAcquirePublished(
				Snapshot.GetHeader().FunctionKey,
				Snapshot.GetHeader().FunctionRevision,
				Snapshot.GetHeader().PublicationOrdinal,
				Published)));
		}
		ASSERT_THAT(AreEqual(1,
			Factory.Control->LeaseReleaseCount.GetValue()));
	}

	TEST_METHOD(ShutdownWaitsForClaimedLazyCompilation)
	{
		FFakeFactory Factory;
		Factory.Control->bBlockFirstCompile.Store(true);
		Factory.Control->bReleaseBlockedCompileOnCancel.Store(false);
		FScopedFactoryRegistration Registration(Factory);
		FAngelscriptRuntimeJITRequestStateMachine Machine(
			MakeSelection(
				EAngelscriptRuntimeJITCompilePolicy::LazyFirstCall, 1));
		const FAngelscriptRuntimeJITCompileSnapshot Snapshot =
			MakeOwnedSnapshot(TEXT("lazy-shutdown-v1"), 1, 1);
		Machine.ObserveFunction(Snapshot);

		TFuture<FAngelscriptRuntimeJITLazyCallResult> Call = Async(
			EAsyncExecution::ThreadPool,
			[&Machine, &Snapshot]()
			{
				return Machine.OnLazyFirstCall(
					Snapshot.GetHeader().FunctionKey,
					Snapshot.GetHeader().FunctionRevision,
					Snapshot.GetHeader().PublicationOrdinal);
			});
		ASSERT_THAT(IsTrue(Factory.Control->FirstCompileEntered->Wait(5000)));

		FEvent* const ShutdownReturned =
			FPlatformProcess::GetSynchEventFromPool(true);
		TFuture<void> Shutdown = Async(
			EAsyncExecution::ThreadPool,
			[&Machine, ShutdownReturned]()
			{
				Machine.Shutdown();
				ShutdownReturned->Trigger();
			});
		ASSERT_THAT(IsTrue(WaitForCancelCount(Factory.Control, 1)));
		const bool bShutdownReturnedBeforeCompile =
			ShutdownReturned->Wait(100);

		Factory.Control->ReleaseBlockedCompile();
		const FAngelscriptRuntimeJITLazyCallResult CallResult = Call.Get();
		Shutdown.Get();
		FPlatformProcess::ReturnSynchEventToPool(ShutdownReturned);

		ASSERT_THAT(IsTrue(CallResult.bCompileClaimed));
		ASSERT_THAT(IsFalse(bShutdownReturnedBeforeCompile,
			TEXT("Shutdown must wait until an already-claimed compile has "
				"returned and queued its terminal completion")));
		ASSERT_THAT(AreEqual(1,
			Factory.Control->FinishedCompileCount.GetValue()));
	}

	TEST_METHOD(ShutdownWaitsForAdmittedEagerSyncCompilation)
	{
		FFakeFactory Factory;
		Factory.Control->bBlockFirstCompile.Store(true);
		Factory.Control->bReleaseBlockedCompileOnCancel.Store(false);
		FScopedFactoryRegistration Registration(Factory);
		FAngelscriptRuntimeJITRequestStateMachine Machine(
			MakeSelection(EAngelscriptRuntimeJITCompilePolicy::EagerSync, 1));
		const FAngelscriptRuntimeJITCompileSnapshot Snapshot =
			MakeOwnedSnapshot(TEXT("eager-sync-shutdown"), 1, 1);

		TFuture<FAngelscriptRuntimeJITObserveResult> Observe = Async(
			EAsyncExecution::ThreadPool,
			[&Machine, Snapshot]() mutable
			{
				return Machine.ObserveFunction(Snapshot);
			});
		ASSERT_THAT(IsTrue(Factory.Control->FirstCompileEntered->Wait(5000)));

		FEvent* const ShutdownReturned =
			FPlatformProcess::GetSynchEventFromPool(true);
		TFuture<void> Shutdown = Async(
			EAsyncExecution::ThreadPool,
			[&Machine, ShutdownReturned]()
			{
				Machine.Shutdown();
				ShutdownReturned->Trigger();
			});
		ASSERT_THAT(IsTrue(WaitForCancelCount(Factory.Control, 1)));
		const bool bShutdownReturnedBeforeCompile =
			ShutdownReturned->Wait(100);

		Factory.Control->ReleaseBlockedCompile();
		const FAngelscriptRuntimeJITObserveResult Observed = Observe.Get();
		Shutdown.Get();
		FPlatformProcess::ReturnSynchEventToPool(ShutdownReturned);

		ASSERT_THAT(IsFalse(bShutdownReturnedBeforeCompile));
		ASSERT_THAT(IsTrue(Observed.State ==
			EAngelscriptRuntimeJITFunctionState::Stale));
		ASSERT_THAT(AreEqual(1,
			Factory.Control->FinishedCompileCount.GetValue()));
	}

	TEST_METHOD(RetiringOneFunctionDoesNotCancelAnotherFunction)
	{
		FFakeFactory Factory;
		Factory.Control->bBlockFirstCompile.Store(true);
		FScopedFactoryRegistration Registration(Factory);
		FAngelscriptRuntimeJITRequestStateMachine Machine(
			MakeSelection(
				EAngelscriptRuntimeJITCompilePolicy::EagerBackground, 1));
		const FAngelscriptRuntimeJITCompileSnapshot SnapshotA =
			MakeOwnedSnapshot(
				TEXT("retire-a"), 1, 1, TEXT("retire-function-a"));
		const FAngelscriptRuntimeJITCompileSnapshot SnapshotB =
			MakeOwnedSnapshot(
				TEXT("keep-b"), 1, 1, TEXT("keep-function-b"));

		Machine.ObserveFunction(SnapshotA);
		ASSERT_THAT(IsTrue(Factory.Control->FirstCompileEntered->Wait(5000)));
		Machine.ObserveFunction(SnapshotB);
		ASSERT_THAT(IsTrue(Machine.RetireFunction(
			SnapshotA.GetHeader().FunctionKey,
			SnapshotA.GetHeader().PublicationOrdinal)));
		ASSERT_THAT(AreEqual(0, Factory.Control->CancelCount.GetValue()));

		Factory.Control->ReleaseBlockedCompile();
		ASSERT_THAT(IsTrue(WaitForFinishedCount(Factory.Control, 2)));
		const FAngelscriptRuntimeJITProcessResult Processed =
			Machine.ProcessPendingResults();
		ASSERT_THAT(AreEqual(1, Processed.PublishedCount));
		ASSERT_THAT(AreEqual(1, Processed.StaleCount));

		FAngelscriptRuntimeJITPublishedCode Published;
		ASSERT_THAT(IsTrue(Machine.TryAcquirePublished(
			SnapshotB.GetHeader().FunctionKey,
			SnapshotB.GetHeader().FunctionRevision,
			SnapshotB.GetHeader().PublicationOrdinal,
			Published)));
	}

	TEST_METHOD(ModuleReplacementRejectsTheCompletedOldRevision)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FFakeFactory Factory;
		Factory.Control->bBlockFirstCompile.Store(true);
		FScopedFactoryRegistration Registration(Factory);
		FAngelscriptRuntimeJITRequestStateMachine Machine(
			MakeSelection(
				EAngelscriptRuntimeJITCompilePolicy::EagerBackground, 1));

		FAngelscriptRuntimeJITCompileSnapshot OldSnapshot;
		FAngelscriptHash256 OldRevision;
		{
			const FString V1Source = ASTEST_AS(R"AS(
				int ReplacedValue()
				{
					return 1;
				}
				)AS");
			FScopedAngelscriptModule V1Module(
				*TestRunner, Engine, TEXT("ASRuntimeJITReplacement"), V1Source);
			ASSERT_THAT(IsTrue(V1Module.IsValid()));
			asIScriptFunction* V1Function =
				V1Module.GetModule().GetFunctionByDecl("int ReplacedValue()");
			ASSERT_THAT(IsNotNull(V1Function));
			const FAngelscriptFunctionRoute V1Route =
				MakeRoute(*V1Function, TEXT("replacement-v1"));
			OldSnapshot = CaptureReplacementSnapshot(
				*V1Function, V1Route, 1, 1);
			OldRevision = OldSnapshot.GetHeader().FunctionRevision;
			Machine.ObserveFunction(OldSnapshot);
			ASSERT_THAT(IsTrue(Factory.Control->FirstCompileEntered->Wait(5000)));
		}

		const FString V2Source = ASTEST_AS(R"AS(
			int ReplacedValue()
			{
				return 2;
			}
			)AS");
		FScopedAngelscriptModule V2Module(
			*TestRunner, Engine, TEXT("ASRuntimeJITReplacement"), V2Source);
		ASSERT_THAT(IsTrue(V2Module.IsValid()));
		asIScriptFunction* V2Function =
			V2Module.GetModule().GetFunctionByDecl("int ReplacedValue()");
		ASSERT_THAT(IsNotNull(V2Function));
		const FAngelscriptFunctionRoute V2Route =
			MakeRoute(*V2Function, TEXT("replacement-v2"));
		const FAngelscriptRuntimeJITCompileSnapshot NewSnapshot =
			CaptureReplacementSnapshot(*V2Function, V2Route, 2, 1);
		ASSERT_THAT(IsFalse(OldRevision ==
			NewSnapshot.GetHeader().FunctionRevision));

		Machine.ObserveFunction(NewSnapshot);
		Factory.Control->ReleaseBlockedCompile();
		ASSERT_THAT(IsTrue(WaitForCompileCount(Factory.Control, 2)));
		ASSERT_THAT(IsTrue(WaitForFinishedCount(Factory.Control, 2)));
		const FAngelscriptRuntimeJITProcessResult Processed =
			Machine.ProcessPendingResults();
		ASSERT_THAT(AreEqual(1, Processed.PublishedCount));
		ASSERT_THAT(AreEqual(1, Processed.StaleCount));
		ASSERT_THAT(AreEqual(0, Factory.Control->CancelCount.GetValue(),
			TEXT("Replacing one function must not cancel its shared session "
				"generation")));

		FAngelscriptRuntimeJITPublishedCode Published;
		ASSERT_THAT(IsFalse(Machine.TryAcquirePublished(
			OldSnapshot.GetHeader().FunctionKey,
			OldRevision,
			OldSnapshot.GetHeader().PublicationOrdinal,
			Published),
			TEXT("The old revision must never attach to its replacement")));
		ASSERT_THAT(IsTrue(Machine.TryAcquirePublished(
			NewSnapshot.GetHeader().FunctionKey,
			NewSnapshot.GetHeader().FunctionRevision,
			NewSnapshot.GetHeader().PublicationOrdinal,
			Published)));
		ASSERT_THAT(AreEqual(1,
			Factory.Control->LeaseReleaseCount.GetValue(),
			TEXT("The stale old result lease should already be released")));

		ASSERT_THAT(IsTrue(Machine.RetireFunction(
			NewSnapshot.GetHeader().FunctionKey,
			NewSnapshot.GetHeader().PublicationOrdinal)));
		Published = {};
		ASSERT_THAT(AreEqual(2,
			Factory.Control->LeaseReleaseCount.GetValue(),
			TEXT("Retiring the current function releases current Runtime code")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
