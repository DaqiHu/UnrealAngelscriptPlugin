#include "AngelscriptEngine.h"
#include "AngelscriptTestEngineAcquisition.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"
#include "Async/Async.h"
#include "CQTest.h"
#if WITH_AS_DEBUGSERVER
#include "Debugging/AngelscriptDebugServer.h"
#endif
#if WITH_AS_COVERAGE
#include "Extension/CodeCoverage/AngelscriptCodeCoverage.h"
#include "Testing/AngelscriptTestSettings.h"
#include "UObject/UObjectGlobals.h"
#endif
#include "Features/IModularFeatures.h"
#include "HAL/Event.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "JIT/AngelscriptRuntimeJITBackend.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RuntimeJIT/AngelscriptJITCoordinator.h"
#include "StaticJIT/AngelscriptJITGeneration.h"
#include "StaticJIT/AngelscriptJITProviderRegistry.h"
#include "StaticJIT/AngelscriptJITProviderRouter.h"
#include "StaticJIT/BytecodeJIT/AngelscriptBytecodeJIT.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT

TEST_CLASS_WITH_FLAGS(FAngelscriptJITCoordinatorRoutingTests,
	"Angelscript.TestModule.RuntimeJIT.Coordinator.Routing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static FEvent* BlockingRuntimeEntryStarted = nullptr;
	inline static FEvent* BlockingRuntimeEntryContinue = nullptr;
	inline static FThreadSafeCounter BlockingRuntimeEntryBudget;
	inline static FThreadSafeCounter StaticVmEntryCount;

	static FAngelscriptRuntimeJITBackendId BackendId()
	{
		FAngelscriptRuntimeJITBackendId Result;
		check(FAngelscriptRuntimeJITBackendId::TryParse(
			TEXT("fake-coordinator"), Result));
		return Result;
	}

	static void RuntimeVmEntry(FScriptExecution&, asDWORD*, asQWORD*)
	{
		FEvent* const Started = BlockingRuntimeEntryStarted;
		FEvent* const Continue = BlockingRuntimeEntryContinue;
		if (Started != nullptr && Continue != nullptr
			&& BlockingRuntimeEntryBudget.Decrement() >= 0)
		{
			Started->Trigger();
			Continue->Wait();
		}
	}

	static void StaticVmEntry(FScriptExecution&, asDWORD*, asQWORD*)
	{
		StaticVmEntryCount.Increment();
	}

	static void StaticRawEntry()
	{
	}

	static void StaticParmsEntry(FScriptExecution&, void*, void*)
	{
	}

	static FAngelscriptHash256 MakeHash(const TCHAR* Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("jit-coordinator-routing-test"));
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	struct FControl : TSharedFromThis<FControl, ESPMode::ThreadSafe>
	{
		FThreadSafeCounter CompileCount;
		FThreadSafeCounter HelperTokenCompileCount;
		FThreadSafeCounter SessionCreateCount;
		FThreadSafeCounter SessionDestroyCount;
		FThreadSafeCounter LeaseReleaseCount;
		TAtomic<bool> bRejectHelperTokens { false };
		FCriticalSection Mutex;
		TArray<uint64> EngineNamespaces;
	};

	struct FLeasePayload
	{
		explicit FLeasePayload(
			TSharedRef<FControl, ESPMode::ThreadSafe> InControl)
			: Control(MoveTemp(InControl))
		{
		}

		TSharedRef<FControl, ESPMode::ThreadSafe> Control;
	};

	static void ReleaseLease(void* UserData)
	{
		TUniquePtr<FLeasePayload> Payload(
			static_cast<FLeasePayload*>(UserData));
		Payload->Control->LeaseReleaseCount.Increment();
	}

	class FSession final : public IAngelscriptRuntimeJITBackendSession
	{
	public:
		explicit FSession(
			TSharedRef<FControl, ESPMode::ThreadSafe> InControl)
			: Control(MoveTemp(InControl))
		{
		}

		~FSession() override
		{
			Control->SessionDestroyCount.Increment();
		}

		FAngelscriptRuntimeJITCompileResult Compile(
			const FAngelscriptRuntimeJITCompileSnapshotView& Snapshot) override
		{
			Control->CompileCount.Increment();
			FAngelscriptRuntimeJITCompileResult Result;
			Result.FunctionRevision = Snapshot.Header.FunctionRevision;
			Result.EntryAbiHash = Snapshot.Header.EntryAbiHash;
			if (Snapshot.HelperTokenCount != 0)
			{
				Control->HelperTokenCompileCount.Increment();
				if (Control->bRejectHelperTokens.Load())
				{
					Result.Outcome =
						EAngelscriptRuntimeJITCompileOutcome::Unsupported;
					Result.Reason =
						EAngelscriptRuntimeJITDiagnosticReason::UnsupportedOpcode;
					return Result;
				}
			}
			Result.Outcome = EAngelscriptRuntimeJITCompileOutcome::Compiled;
			Result.Reason = EAngelscriptRuntimeJITDiagnosticReason::None;
			Result.VMEntry = &RuntimeVmEntry;
			Result.CodeSize = 32;
			Result.CompileLatencyMicroseconds = 5;
			FAngelscriptRuntimeJITCodeLeaseView Lease;
			Lease.UserData = new FLeasePayload(Control);
			Lease.Release = &ReleaseLease;
			Result.CodeLease =
				FAngelscriptRuntimeJITCodeLease::Adopt(Lease);
			return Result;
		}

		void Cancel(uint64) override
		{
		}

	private:
		TSharedRef<FControl, ESPMode::ThreadSafe> Control;
	};

	class FFactory final : public IAngelscriptRuntimeJITBackendFactory
	{
	public:
		FFactory()
			: Control(MakeShared<FControl, ESPMode::ThreadSafe>())
		{
			Metadata.BackendId = BackendId();
			Metadata.DisplayName = TEXT("Coordinator Routing Fake");
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
			const FAngelscriptRuntimeJITBackendSessionConfig& Config) override
		{
			Control->SessionCreateCount.Increment();
			{
				FScopeLock Lock(&Control->Mutex);
				Control->EngineNamespaces.Add(Config.EngineNamespace);
			}
			return MakeUnique<FSession>(Control);
		}

		FAngelscriptRuntimeJITBackendFactoryMetadata Metadata;
		TSharedRef<FControl, ESPMode::ThreadSafe> Control;
	};

	class FScopedFactoryRegistration final
	{
	public:
		explicit FScopedFactoryRegistration(FFactory& InFactory)
			: Factory(InFactory)
		{
			IModularFeatures::Get().RegisterModularFeature(
				IAngelscriptRuntimeJITBackendFactory::FeatureName(), &Factory);
		}

		~FScopedFactoryRegistration()
		{
			Unregister();
		}

		void Unregister()
		{
			if (!bRegistered)
			{
				return;
			}
			IModularFeatures::Get().UnregisterModularFeature(
				IAngelscriptRuntimeJITBackendFactory::FeatureName(), &Factory);
			bRegistered = false;
		}

	private:
		FFactory& Factory;
		bool bRegistered = true;
	};

	class FExactRouteProvider final
		: public IAngelscriptJITArtifactProvider
	{
	public:
		FExactRouteProvider(
			const FAngelscriptFunctionRouteSnapshot& Routes,
			const EAngelscriptJITTargetProfile TargetProfile)
		{
			View.ProviderId.Hash = MakeHash(TEXT("exact-aot-provider"));
			View.ProviderName = TEXT("Coordinator Exact AOT Provider");
			View.OwnerModuleName = TEXT("AngelscriptTest");
			View.NativeEnvironmentFingerprint =
				FAngelscriptJITTargetProfiles::
					BuildCurrentNativeEnvironmentFingerprint(TargetProfile);
			for (const FAngelscriptFunctionRoute& Route : Routes.FunctionRoutes)
			{
				if (!Route.bHasVerifiedArtifactIdentity)
				{
					continue;
				}
				FAngelscriptJITArtifactEntry& Entry =
					Entries.AddDefaulted_GetRef();
				Entry.ModuleKey = Route.ModuleKey;
				Entry.FunctionKey = Route.Identity.FunctionKey;
				Entry.ExecutionHash = Route.Identity.Content.Execution;
				Entry.DebugHash = Route.Identity.Content.Debug;
				Entry.ArtifactProfile = Route.Identity.Profile;
				Entry.EntryAbiHash =
					FAngelscriptJITGeneration::BuildEntryAbiHash(
						Route.CanonicalDeclaration,
						true,
						true,
						FAngelscriptBytecodeJIT::CanGenerateParmsEntry(
							Route.Function));
				Entry.NativeEnvironmentFingerprint =
					View.NativeEnvironmentFingerprint;
				Entry.EntryPoints.VMEntry = &StaticVmEntry;
				Entry.EntryPoints.RawEntry = &StaticRawEntry;
				Entry.EntryPoints.ParmsEntry =
					FAngelscriptBytecodeJIT::CanGenerateParmsEntry(
						Route.Function)
					? &StaticParmsEntry
					: nullptr;
			}
			check(!Entries.IsEmpty());
			View.ArtifactProfile = Entries[0].ArtifactProfile;
			View.Entries = Entries.GetData();
			View.EntryCount = Entries.Num();
			View.ArtifactSetDigest =
				FAngelscriptJITProviderManifest::BuildArtifactSetDigest(Entries);
			View.ProviderGeneration =
				FAngelscriptJITProviderManifest::BuildProviderGeneration(View);
		}

		const FAngelscriptJITProviderView* GetAngelscriptJITProviderView()
			const override
		{
			return &View;
		}

		TArray<FAngelscriptJITArtifactEntry> Entries;
		FAngelscriptJITProviderView View;
	};

	class FScopedProjectRoot final
	{
	public:
		FScopedProjectRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptJITCoordinatorRouting"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptJITCoordinatorRouting/")));
			ScriptRoot = Root / TEXT("Script");
			CacheRoot = Root / TEXT("CacheV2");
			check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		}

		~FScopedProjectRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptJITCoordinatorRouting/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		bool WriteSourceText(const FString& Source) const
		{
			return FFileHelper::SaveStringToFile(
				Source, *(ScriptRoot / TEXT("RuntimeCoordinator.as")));
		}

		bool WriteSource() const
		{
			return WriteSourceText(ASTEST_AS(R"AS(
				int AddOne(int Value)
				{
					return Value + 1;
				}
			)AS"));
		}

		FString Root;
		FString ScriptRoot;
		FString CacheRoot;
	};

	static FAngelscriptEngineConfig MakeRuntimeConfig(
		const FScopedProjectRoot& Project,
		const EAngelscriptRuntimeJITCompilePolicy Policy =
			EAngelscriptRuntimeJITCompilePolicy::EagerSync,
		const EAngelscriptJITExecutionMode Mode =
			EAngelscriptJITExecutionMode::RuntimeOnly)
	{
		FAngelscriptEngineConfig Config;
		Config.bIsEditor = true;
		Config.bDevelopmentMode = true;
		Config.bSkipThreadedInitialize = true;
		Config.CacheV2RootOverride = Project.CacheRoot;
		Config.JITExecutionMode = Mode;
		Config.RuntimeJITBackend = BackendId();
		Config.RuntimeJITCompilePolicy = Policy;
		return Config;
	}

	static FAngelscriptEngineDependencies MakeDependencies(
		const FScopedProjectRoot& Project)
	{
		FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		Dependencies.GetProjectDir = [&Project]()
		{
			return Project.Root;
		};
		Dependencies.GetEnabledPluginScriptRoots = []()
		{
			return TArray<FString>();
		};
		Dependencies.GetEnabledPluginScriptRootDescriptors = []()
		{
			return TArray<FAngelscriptPluginScriptRoot>();
		};
		return Dependencies;
	}

	static bool ExecuteAddOne(
		FAngelscriptEngine& Engine,
		asIScriptFunction& Function,
		const int32 Value,
		int32& OutResult)
	{
		asIScriptContext* Context = Engine.GetScriptEngine()->CreateContext();
		if (Context == nullptr)
		{
			return false;
		}
		const bool bSucceeded = Context->Prepare(&Function) >= 0
			&& Context->SetArgDWord(0, static_cast<asDWORD>(Value)) >= 0
			&& Context->Execute() == asEXECUTION_FINISHED;
		if (bSucceeded)
		{
			OutResult = static_cast<int32>(Context->GetReturnDWord());
		}
		Context->Release();
		return bSucceeded;
	}

public:
	TEST_METHOD(ShutdownWaitsForActiveRuntimeBindingReader)
	{
		FFactory Factory;
		FScopedFactoryRegistration Registration(Factory);
		FScopedProjectRoot Project;
		ASSERT_THAT(IsTrue(Project.WriteSource()));
		TUniquePtr<FAngelscriptEngine> Engine =
			CreateScriptScanFreeFullEngineForTesting(
				MakeRuntimeConfig(Project),
				MakeDependencies(Project));
		ASSERT_THAT(IsNotNull(Engine.Get()));

		asIScriptFunction* Function = nullptr;
		{
			FAngelscriptEngineScope Scope(*Engine);
			Engine->InitialCompile();
			const TSharedPtr<FAngelscriptModuleDesc> Module =
				Engine->GetModuleByModuleName(TEXT("RuntimeCoordinator"));
			ASSERT_THAT(IsTrue(Module.IsValid()));
			ASSERT_THAT(IsNotNull(Module->ScriptModule));
			Function =
				Module->ScriptModule->GetFunctionByDecl("int AddOne(int)");
			ASSERT_THAT(IsNotNull(Function));
			ASSERT_THAT(IsTrue(
				Function->GetJITBinding().VMEntry == &RuntimeVmEntry));
		}

		BlockingRuntimeEntryStarted =
			FPlatformProcess::GetSynchEventFromPool(true);
		BlockingRuntimeEntryContinue =
			FPlatformProcess::GetSynchEventFromPool(true);
		BlockingRuntimeEntryBudget.Reset();
		BlockingRuntimeEntryBudget.Increment();
		FEvent* const ShutdownStarted =
			FPlatformProcess::GetSynchEventFromPool(true);
		FEvent* const ShutdownReturned =
			FPlatformProcess::GetSynchEventFromPool(true);
		TFuture<int32> Execution;
		TFuture<void> Shutdown;
		ON_SCOPE_EXIT
		{
			BlockingRuntimeEntryContinue->Trigger();
			if (Execution.IsValid())
			{
				Execution.Wait();
			}
			if (Shutdown.IsValid())
			{
				Shutdown.Wait();
			}
			FPlatformProcess::ReturnSynchEventToPool(
				BlockingRuntimeEntryStarted);
			FPlatformProcess::ReturnSynchEventToPool(
				BlockingRuntimeEntryContinue);
			FPlatformProcess::ReturnSynchEventToPool(ShutdownStarted);
			FPlatformProcess::ReturnSynchEventToPool(ShutdownReturned);
			BlockingRuntimeEntryStarted = nullptr;
			BlockingRuntimeEntryContinue = nullptr;
			BlockingRuntimeEntryBudget.Reset();
		};

		asIScriptContext* const Context =
			Engine->GetScriptEngine()->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(0, 41)));
		Execution = Async(EAsyncExecution::Thread, [Context]()
		{
			const int32 Result = Context->Execute();
			Context->Release();
			return Result;
		});
		ASSERT_THAT(IsTrue(BlockingRuntimeEntryStarted->Wait(5000)));
		Registration.Unregister();
		ASSERT_THAT(AreEqual(0,
			Factory.Control->LeaseReleaseCount.GetValue()));
		ASSERT_THAT(AreEqual(0,
			Factory.Control->SessionDestroyCount.GetValue()));

		FAngelscriptJITCoordinator* const Coordinator =
			Engine->GetJITCoordinator();
		ASSERT_THAT(IsNotNull(Coordinator));
		Shutdown = Async(EAsyncExecution::Thread,
			[Coordinator, ShutdownStarted, ShutdownReturned]()
			{
				ShutdownStarted->Trigger();
				Coordinator->Shutdown();
				ShutdownReturned->Trigger();
			});
		ASSERT_THAT(IsTrue(ShutdownStarted->Wait(5000)));

		const double DetachDeadline = FPlatformTime::Seconds() + 5.0;
		while (Engine->GetScriptEngine()->GetJITCompiler() != nullptr
			&& FPlatformTime::Seconds() < DetachDeadline)
		{
			FPlatformProcess::SleepNoStats(0.001f);
		}
		ASSERT_THAT(IsNull(Engine->GetScriptEngine()->GetJITCompiler()));
		ASSERT_THAT(IsFalse(ShutdownReturned->Wait(50)));
		ASSERT_THAT(AreEqual(0,
			Factory.Control->LeaseReleaseCount.GetValue()));
		ASSERT_THAT(AreEqual(0,
			Factory.Control->SessionDestroyCount.GetValue()));

		int32 VmResult = 0;
		ASSERT_THAT(IsTrue(ExecuteAddOne(
			*Engine, *Function, 41, VmResult)));
		ASSERT_THAT(AreEqual(42, VmResult));

		BlockingRuntimeEntryContinue->Trigger();
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, Execution.Get()));
		ASSERT_THAT(IsTrue(ShutdownReturned->Wait(5000)));
		Shutdown.Wait();
		ASSERT_THAT(AreEqual(1,
			Factory.Control->LeaseReleaseCount.GetValue()));
		ASSERT_THAT(AreEqual(1,
			Factory.Control->SessionDestroyCount.GetValue()));

		Engine.Reset();
	}

	TEST_METHOD(ActiveRuntimeCallSurvivesAotAndRuntimeReplacement)
	{
		FFactory Factory;
		FScopedFactoryRegistration Registration(Factory);
		FScopedProjectRoot Project;
		ASSERT_THAT(IsTrue(Project.WriteSource()));
		TUniquePtr<FAngelscriptEngine> Engine =
			CreateScriptScanFreeFullEngineForTesting(
				MakeRuntimeConfig(
					Project,
					EAngelscriptRuntimeJITCompilePolicy::EagerSync,
					EAngelscriptJITExecutionMode::Auto),
				MakeDependencies(Project));
		ASSERT_THAT(IsNotNull(Engine.Get()));

		asIScriptFunction* Function = nullptr;
		TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Routes;
		{
			FAngelscriptEngineScope Scope(*Engine);
			Engine->InitialCompile();
			Routes = Engine->GetFunctionRouteSnapshot();
			ASSERT_THAT(IsTrue(Routes.IsValid()));
			const TSharedPtr<FAngelscriptModuleDesc> Module =
				Engine->GetModuleByModuleName(TEXT("RuntimeCoordinator"));
			ASSERT_THAT(IsTrue(Module.IsValid()));
			ASSERT_THAT(IsNotNull(Module->ScriptModule));
			Function =
				Module->ScriptModule->GetFunctionByDecl("int AddOne(int)");
			ASSERT_THAT(IsNotNull(Function));
			ASSERT_THAT(IsTrue(
				Function->GetJITBinding().VMEntry == &RuntimeVmEntry));
		}
		const asSJITFunctionBinding FirstRuntimeBinding =
			Function->GetJITBinding();

		BlockingRuntimeEntryStarted =
			FPlatformProcess::GetSynchEventFromPool(true);
		BlockingRuntimeEntryContinue =
			FPlatformProcess::GetSynchEventFromPool(true);
		BlockingRuntimeEntryBudget.Reset();
		BlockingRuntimeEntryBudget.Increment();
		StaticVmEntryCount.Reset();
		TFuture<int32> OldExecution;
		ON_SCOPE_EXIT
		{
			BlockingRuntimeEntryContinue->Trigger();
			if (OldExecution.IsValid())
			{
				OldExecution.Wait();
			}
			FPlatformProcess::ReturnSynchEventToPool(
				BlockingRuntimeEntryStarted);
			FPlatformProcess::ReturnSynchEventToPool(
				BlockingRuntimeEntryContinue);
			BlockingRuntimeEntryStarted = nullptr;
			BlockingRuntimeEntryContinue = nullptr;
			BlockingRuntimeEntryBudget.Reset();
			StaticVmEntryCount.Reset();
		};

		asIScriptContext* const Context =
			Engine->GetScriptEngine()->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Function)));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(0, 41)));
		OldExecution = Async(EAsyncExecution::Thread, [Context]()
		{
			const int32 Result = Context->Execute();
			Context->Release();
			return Result;
		});
		ASSERT_THAT(IsTrue(BlockingRuntimeEntryStarted->Wait(5000)));

		FExactRouteProvider Provider(
			*Routes,
			FAngelscriptJITProviderRouter::ResolveTargetProfile(
				Engine->GetRuntimeConfig()));
		FAngelscriptJITProviderRegistry& Registry =
			FAngelscriptJITProviderRegistry::Get();
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&Provider, Provider).IsAccepted()));
		bool bProviderRegistered = true;
		ON_SCOPE_EXIT
		{
			if (bProviderRegistered)
			{
				Registry.UnregisterOwner(&Provider);
			}
		};

		{
			FAngelscriptEngineScope Scope(*Engine);
			const FAngelscriptJITProviderRouteRefreshResult Refresh =
				FAngelscriptJITProviderRouter::Refresh(*Engine);
			ASSERT_THAT(IsTrue(Refresh.IsApplied(), Refresh.Detail));
		}
		const asSJITFunctionBinding AotBinding = Function->GetJITBinding();
		ASSERT_THAT(IsTrue(AotBinding.VMEntry == &StaticVmEntry));
		ASSERT_THAT(IsTrue(AotBinding.RawEntry == &StaticRawEntry));
		ASSERT_THAT(IsTrue(AotBinding.UserData
			!= FirstRuntimeBinding.UserData));
		ASSERT_THAT(AreEqual(0,
			Factory.Control->LeaseReleaseCount.GetValue()));

		int32 IgnoredAotResult = 0;
		ASSERT_THAT(IsTrue(ExecuteAddOne(
			*Engine, *Function, 41, IgnoredAotResult)));
		ASSERT_THAT(AreEqual(1, StaticVmEntryCount.GetValue()));

		ASSERT_THAT(IsTrue(Registry.UnregisterOwner(&Provider)));
		bProviderRegistered = false;
		{
			FAngelscriptEngineScope Scope(*Engine);
			const FAngelscriptJITProviderRouteRefreshResult Refresh =
				FAngelscriptJITProviderRouter::Refresh(*Engine);
			ASSERT_THAT(IsTrue(Refresh.IsApplied(), Refresh.Detail));
		}
		const asSJITFunctionBinding SecondRuntimeBinding =
			Function->GetJITBinding();
		ASSERT_THAT(IsTrue(
			SecondRuntimeBinding.VMEntry == &RuntimeVmEntry));
		ASSERT_THAT(IsTrue(SecondRuntimeBinding.UserData
			!= FirstRuntimeBinding.UserData));
		ASSERT_THAT(AreEqual(2,
			Factory.Control->CompileCount.GetValue()));
		ASSERT_THAT(AreEqual(0,
			Factory.Control->LeaseReleaseCount.GetValue()));

		int32 IgnoredRuntimeResult = 0;
		ASSERT_THAT(IsTrue(ExecuteAddOne(
			*Engine, *Function, 41, IgnoredRuntimeResult)));

		BlockingRuntimeEntryContinue->Trigger();
		ASSERT_THAT(AreEqual(
			asEXECUTION_FINISHED, OldExecution.Get()));
		ASSERT_THAT(AreEqual(1,
			Factory.Control->LeaseReleaseCount.GetValue()));
		ASSERT_THAT(AreEqual(0,
			Factory.Control->SessionDestroyCount.GetValue()));

		Engine->GetJITCoordinator()->Shutdown();
		ASSERT_THAT(AreEqual(2,
			Factory.Control->LeaseReleaseCount.GetValue()));
		ASSERT_THAT(AreEqual(1,
			Factory.Control->SessionDestroyCount.GetValue()));
		Engine.Reset();
	}

	TEST_METHOD(TwoEnginesOwnIsolatedRuntimeSessionsBindingsAndTeardown)
	{
		FFactory Factory;
		FScopedFactoryRegistration Registration(Factory);
		FScopedProjectRoot FirstProject;
		FScopedProjectRoot SecondProject;
		ASSERT_THAT(IsTrue(FirstProject.WriteSource()));
		ASSERT_THAT(IsTrue(SecondProject.WriteSource()));
		TUniquePtr<FAngelscriptEngine> First =
			CreateScriptScanFreeFullEngineForTesting(
				MakeRuntimeConfig(FirstProject),
				MakeDependencies(FirstProject));
		TUniquePtr<FAngelscriptEngine> Second =
			CreateScriptScanFreeFullEngineForTesting(
				MakeRuntimeConfig(SecondProject),
				MakeDependencies(SecondProject));
		ASSERT_THAT(IsNotNull(First.Get()));
		ASSERT_THAT(IsNotNull(Second.Get()));
		ASSERT_THAT(IsTrue(First->GetJITCoordinator()
			!= Second->GetJITCoordinator()));
		ASSERT_THAT(AreEqual(2,
			Factory.Control->SessionCreateCount.GetValue()));
		{
			FScopeLock Lock(&Factory.Control->Mutex);
			ASSERT_THAT(AreEqual(2, Factory.Control->EngineNamespaces.Num()));
			ASSERT_THAT(IsTrue(Factory.Control->EngineNamespaces[0] != 0));
			ASSERT_THAT(IsTrue(Factory.Control->EngineNamespaces[0]
				!= Factory.Control->EngineNamespaces[1]));
		}

		asIScriptFunction* FirstFunction = nullptr;
		{
			FAngelscriptEngineScope Scope(*First);
			First->InitialCompile();
			const TSharedPtr<FAngelscriptModuleDesc> Module =
				First->GetModuleByModuleName(TEXT("RuntimeCoordinator"));
			ASSERT_THAT(IsTrue(Module.IsValid()));
			ASSERT_THAT(IsNotNull(Module->ScriptModule));
			FirstFunction =
				Module->ScriptModule->GetFunctionByDecl("int AddOne(int)");
			ASSERT_THAT(IsNotNull(FirstFunction));
			const asSJITFunctionBinding Binding =
				FirstFunction->GetJITBinding();
			ASSERT_THAT(IsTrue(Binding.VMEntry == &RuntimeVmEntry));
			ASSERT_THAT(IsNull(Binding.RawEntry));
			ASSERT_THAT(IsNull(Binding.ParmsEntry));
			ASSERT_THAT(IsNotNull(Binding.UserData));
		}

		asIScriptFunction* SecondFunction = nullptr;
		asSJITFunctionBinding SecondBinding;
		{
			FAngelscriptEngineScope Scope(*Second);
			Second->InitialCompile();
			const TSharedPtr<FAngelscriptModuleDesc> Module =
				Second->GetModuleByModuleName(TEXT("RuntimeCoordinator"));
			ASSERT_THAT(IsTrue(Module.IsValid()));
			ASSERT_THAT(IsNotNull(Module->ScriptModule));
			SecondFunction =
				Module->ScriptModule->GetFunctionByDecl("int AddOne(int)");
			ASSERT_THAT(IsNotNull(SecondFunction));
			SecondBinding = SecondFunction->GetJITBinding();
			ASSERT_THAT(IsTrue(SecondBinding.VMEntry == &RuntimeVmEntry));
			ASSERT_THAT(IsNull(SecondBinding.RawEntry));
			ASSERT_THAT(IsNull(SecondBinding.ParmsEntry));
			ASSERT_THAT(IsNotNull(SecondBinding.UserData));
		}

		ASSERT_THAT(AreEqual(2, Factory.Control->CompileCount.GetValue()));
		{
			FAngelscriptEngineScope Scope(*First);
			ASSERT_THAT(IsTrue(First->DiscardModule(
				TEXT("RuntimeCoordinator"))));
		}
		First.Reset();
		ASSERT_THAT(AreEqual(1,
			Factory.Control->SessionDestroyCount.GetValue()));
		ASSERT_THAT(IsTrue(Second->GetScriptEngine()->GetJITCompiler()
			== Second->GetJITCoordinator()));
		ASSERT_THAT(IsTrue(SecondFunction->GetJITBinding().VMEntry
			== SecondBinding.VMEntry));

		{
			FAngelscriptEngineScope Scope(*Second);
			ASSERT_THAT(IsTrue(Second->DiscardModule(
				TEXT("RuntimeCoordinator"))));
		}
		Second.Reset();
		ASSERT_THAT(AreEqual(2,
			Factory.Control->SessionDestroyCount.GetValue()));
		ASSERT_THAT(AreEqual(2,
			Factory.Control->LeaseReleaseCount.GetValue()));
	}

	TEST_METHOD(LazyFirstCallKeepsCurrentInvocationOnVmThenPublishes)
	{
		FFactory Factory;
		FScopedFactoryRegistration Registration(Factory);
		FScopedProjectRoot Project;
		ASSERT_THAT(IsTrue(Project.WriteSource()));
		TUniquePtr<FAngelscriptEngine> Engine =
			CreateScriptScanFreeFullEngineForTesting(
				MakeRuntimeConfig(
					Project,
					EAngelscriptRuntimeJITCompilePolicy::LazyFirstCall),
				MakeDependencies(Project));
		ASSERT_THAT(IsNotNull(Engine.Get()));

		asIScriptFunction* Function = nullptr;
		{
			FAngelscriptEngineScope Scope(*Engine);
			Engine->InitialCompile();
			const TSharedPtr<FAngelscriptModuleDesc> Module =
				Engine->GetModuleByModuleName(TEXT("RuntimeCoordinator"));
			ASSERT_THAT(IsTrue(Module.IsValid()));
			ASSERT_THAT(IsNotNull(Module->ScriptModule));
			Function =
				Module->ScriptModule->GetFunctionByDecl("int AddOne(int)");
			ASSERT_THAT(IsNotNull(Function));
			ASSERT_THAT(AreEqual(0,
				Factory.Control->CompileCount.GetValue()));
			ASSERT_THAT(IsNull(Function->GetJITBinding().VMEntry));

			int32 Result = 0;
			ASSERT_THAT(IsTrue(ExecuteAddOne(
				*Engine, *Function, 41, Result)));
			ASSERT_THAT(AreEqual(42, Result));
			ASSERT_THAT(AreEqual(1,
				Factory.Control->CompileCount.GetValue()));
			ASSERT_THAT(IsNull(Function->GetJITBinding().VMEntry));

			ASSERT_THAT(IsTrue(
				Engine->GetJITCoordinator()->ProcessRuntimeSafePoint(*Engine)));
			const asSJITFunctionBinding Published =
				Function->GetJITBinding();
			ASSERT_THAT(IsTrue(Published.VMEntry == &RuntimeVmEntry));
			ASSERT_THAT(IsNull(Published.RawEntry));
			ASSERT_THAT(IsNull(Published.ParmsEntry));
			ASSERT_THAT(IsNotNull(Published.UserData));
			ASSERT_THAT(IsTrue(Engine->DiscardModule(
				TEXT("RuntimeCoordinator"))));
		}
		Engine.Reset();
		ASSERT_THAT(AreEqual(1,
			Factory.Control->SessionDestroyCount.GetValue()));
		ASSERT_THAT(AreEqual(1,
			Factory.Control->LeaseReleaseCount.GetValue()));
	}

	TEST_METHOD(AutoFallsBackToRuntimeAfterAotMissAndVmOnlyStaysVm)
	{
		FFactory Factory;
		FScopedFactoryRegistration Registration(Factory);
		FScopedProjectRoot AutoProject;
		FScopedProjectRoot VmProject;
		ASSERT_THAT(IsTrue(AutoProject.WriteSource()));
		ASSERT_THAT(IsTrue(VmProject.WriteSource()));
		TUniquePtr<FAngelscriptEngine> AutoEngine =
			CreateScriptScanFreeFullEngineForTesting(
				MakeRuntimeConfig(
					AutoProject,
					EAngelscriptRuntimeJITCompilePolicy::EagerSync,
					EAngelscriptJITExecutionMode::Auto),
				MakeDependencies(AutoProject));
		TUniquePtr<FAngelscriptEngine> VmEngine =
			CreateScriptScanFreeFullEngineForTesting(
				MakeRuntimeConfig(
					VmProject,
					EAngelscriptRuntimeJITCompilePolicy::EagerSync,
					EAngelscriptJITExecutionMode::VMOnly),
				MakeDependencies(VmProject));
		ASSERT_THAT(IsNotNull(AutoEngine.Get()));
		ASSERT_THAT(IsNotNull(VmEngine.Get()));
		ASSERT_THAT(AreEqual(1,
			Factory.Control->SessionCreateCount.GetValue()));

		{
			FAngelscriptEngineScope Scope(*AutoEngine);
			AutoEngine->InitialCompile();
			const TSharedPtr<FAngelscriptModuleDesc> Module =
				AutoEngine->GetModuleByModuleName(TEXT("RuntimeCoordinator"));
			ASSERT_THAT(IsTrue(Module.IsValid()));
			ASSERT_THAT(IsNotNull(Module->ScriptModule));
			asIScriptFunction* Function =
				Module->ScriptModule->GetFunctionByDecl("int AddOne(int)");
			ASSERT_THAT(IsNotNull(Function));
			const asSJITFunctionBinding Binding = Function->GetJITBinding();
			ASSERT_THAT(IsTrue(Binding.VMEntry == &RuntimeVmEntry));
			ASSERT_THAT(IsNull(Binding.RawEntry));
			ASSERT_THAT(IsNull(Binding.ParmsEntry));
		}

		{
			FAngelscriptEngineScope Scope(*VmEngine);
			VmEngine->InitialCompile();
			const TSharedPtr<FAngelscriptModuleDesc> Module =
				VmEngine->GetModuleByModuleName(TEXT("RuntimeCoordinator"));
			ASSERT_THAT(IsTrue(Module.IsValid()));
			ASSERT_THAT(IsNotNull(Module->ScriptModule));
			asIScriptFunction* Function =
				Module->ScriptModule->GetFunctionByDecl("int AddOne(int)");
			ASSERT_THAT(IsNotNull(Function));
			const asSJITFunctionBinding Binding = Function->GetJITBinding();
			ASSERT_THAT(IsNull(Binding.VMEntry));
			ASSERT_THAT(IsNull(Binding.RawEntry));
			ASSERT_THAT(IsNull(Binding.ParmsEntry));
		}
		ASSERT_THAT(AreEqual(1, Factory.Control->CompileCount.GetValue()));

		{
			FAngelscriptEngineScope Scope(*AutoEngine);
			ASSERT_THAT(IsTrue(AutoEngine->DiscardModule(
				TEXT("RuntimeCoordinator"))));
		}
		AutoEngine.Reset();
		{
			FAngelscriptEngineScope Scope(*VmEngine);
			ASSERT_THAT(IsTrue(VmEngine->DiscardModule(
				TEXT("RuntimeCoordinator"))));
		}
		VmEngine.Reset();
		ASSERT_THAT(AreEqual(1,
			Factory.Control->SessionDestroyCount.GetValue()));
		ASSERT_THAT(AreEqual(1,
			Factory.Control->LeaseReleaseCount.GetValue()));
	}

	TEST_METHOD(AutoKeepsExactAotAheadOfRuntimeThenFallsBackAfterRetirement)
	{
		FFactory Factory;
		FScopedFactoryRegistration Registration(Factory);
		FScopedProjectRoot ProducerProject;
		FScopedProjectRoot ConsumerProject;
		ASSERT_THAT(IsTrue(ProducerProject.WriteSource()));
		ASSERT_THAT(IsTrue(ConsumerProject.WriteSource()));

		TUniquePtr<FAngelscriptEngine> Producer =
			CreateScriptScanFreeFullEngineForTesting(
				MakeRuntimeConfig(
					ProducerProject,
					EAngelscriptRuntimeJITCompilePolicy::EagerSync,
					EAngelscriptJITExecutionMode::VMOnly),
				MakeDependencies(ProducerProject));
		ASSERT_THAT(IsNotNull(Producer.Get()));
		TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> ProducerRoutes;
		{
			FAngelscriptEngineScope Scope(*Producer);
			Producer->InitialCompile();
			ProducerRoutes = Producer->GetFunctionRouteSnapshot();
			ASSERT_THAT(IsTrue(ProducerRoutes.IsValid()));
		}
		ASSERT_THAT(IsTrue(ProducerRoutes.IsValid()));
		FExactRouteProvider Provider(
			*ProducerRoutes,
			FAngelscriptJITProviderRouter::ResolveTargetProfile(
				Producer->GetRuntimeConfig()));
		FAngelscriptJITProviderRegistry& Registry =
			FAngelscriptJITProviderRegistry::Get();
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&Provider, Provider).IsAccepted()));
		bool bProviderRegistered = true;
		ON_SCOPE_EXIT
		{
			if (bProviderRegistered)
			{
				Registry.UnregisterOwner(&Provider);
			}
		};

		TUniquePtr<FAngelscriptEngine> Consumer =
			CreateScriptScanFreeFullEngineForTesting(
				MakeRuntimeConfig(
					ConsumerProject,
					EAngelscriptRuntimeJITCompilePolicy::EagerSync,
					EAngelscriptJITExecutionMode::Auto),
				MakeDependencies(ConsumerProject));
		ASSERT_THAT(IsNotNull(Consumer.Get()));
		asIScriptFunction* Function = nullptr;
		{
			FAngelscriptEngineScope Scope(*Consumer);
			Consumer->InitialCompile();
			const TSharedPtr<FAngelscriptModuleDesc> Module =
				Consumer->GetModuleByModuleName(TEXT("RuntimeCoordinator"));
			ASSERT_THAT(IsTrue(Module.IsValid()));
			ASSERT_THAT(IsNotNull(Module->ScriptModule));
			Function =
				Module->ScriptModule->GetFunctionByDecl("int AddOne(int)");
			ASSERT_THAT(IsNotNull(Function));
			const asSJITFunctionBinding AotBinding =
				Function->GetJITBinding();
			ASSERT_THAT(IsTrue(AotBinding.VMEntry == &StaticVmEntry));
			ASSERT_THAT(IsTrue(AotBinding.RawEntry == &StaticRawEntry));
			ASSERT_THAT(AreEqual(0,
				Factory.Control->CompileCount.GetValue()));

			ASSERT_THAT(IsTrue(Registry.UnregisterOwner(&Provider)));
			bProviderRegistered = false;
			const FAngelscriptJITProviderRouteRefreshResult Refresh =
				FAngelscriptJITProviderRouter::Refresh(*Consumer);
			ASSERT_THAT(IsTrue(Refresh.IsApplied(), Refresh.Detail));
			const asSJITFunctionBinding RuntimeBinding =
				Function->GetJITBinding();
			ASSERT_THAT(IsTrue(RuntimeBinding.VMEntry == &RuntimeVmEntry));
			ASSERT_THAT(IsNull(RuntimeBinding.RawEntry));
			ASSERT_THAT(IsNull(RuntimeBinding.ParmsEntry));
			ASSERT_THAT(AreEqual(1,
				Factory.Control->CompileCount.GetValue()));
			ASSERT_THAT(IsTrue(Consumer->DiscardModule(
				TEXT("RuntimeCoordinator"))));
		}
		Consumer.Reset();
		{
			FAngelscriptEngineScope Scope(*Producer);
			ASSERT_THAT(IsTrue(Producer->DiscardModule(
				TEXT("RuntimeCoordinator"))));
		}
		Producer.Reset();
		ASSERT_THAT(AreEqual(1,
			Factory.Control->SessionDestroyCount.GetValue()));
		ASSERT_THAT(AreEqual(1,
			Factory.Control->LeaseReleaseCount.GetValue()));
	}

	TEST_METHOD(InvalidRuntimeConfigKeepsAotAndUnknownModeFailsClosedToVm)
	{
		FFactory Factory;
		FScopedFactoryRegistration Registration(Factory);
		FScopedProjectRoot ProducerProject;
		FScopedProjectRoot InvalidRuntimeProject;
		FScopedProjectRoot InvalidModeProject;
		ASSERT_THAT(IsTrue(ProducerProject.WriteSource()));
		ASSERT_THAT(IsTrue(InvalidRuntimeProject.WriteSource()));
		ASSERT_THAT(IsTrue(InvalidModeProject.WriteSource()));

		TUniquePtr<FAngelscriptEngine> Producer =
			CreateScriptScanFreeFullEngineForTesting(
				MakeRuntimeConfig(
					ProducerProject,
					EAngelscriptRuntimeJITCompilePolicy::EagerSync,
					EAngelscriptJITExecutionMode::VMOnly),
				MakeDependencies(ProducerProject));
		ASSERT_THAT(IsNotNull(Producer.Get()));
		TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> ProducerRoutes;
		{
			FAngelscriptEngineScope Scope(*Producer);
			Producer->InitialCompile();
			ProducerRoutes = Producer->GetFunctionRouteSnapshot();
			ASSERT_THAT(IsTrue(ProducerRoutes.IsValid()));
		}
		FExactRouteProvider Provider(
			*ProducerRoutes,
			FAngelscriptJITProviderRouter::ResolveTargetProfile(
				Producer->GetRuntimeConfig()));
		FAngelscriptJITProviderRegistry& Registry =
			FAngelscriptJITProviderRegistry::Get();
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&Provider, Provider).IsAccepted()));
		ON_SCOPE_EXIT
		{
			Registry.UnregisterOwner(&Provider);
		};

		FAngelscriptEngineConfig InvalidRuntimeConfig = MakeRuntimeConfig(
			InvalidRuntimeProject,
			static_cast<EAngelscriptRuntimeJITCompilePolicy>(255),
			EAngelscriptJITExecutionMode::Auto);
		InvalidRuntimeConfig.bRuntimeJITConfigurationValid = false;
		InvalidRuntimeConfig.JITConfigurationError =
			TEXT("Unknown Runtime JIT compile policy 'eventually'");
		TUniquePtr<FAngelscriptEngine> InvalidRuntimeEngine =
			CreateScriptScanFreeFullEngineForTesting(
				InvalidRuntimeConfig,
				MakeDependencies(InvalidRuntimeProject));
		ASSERT_THAT(IsNotNull(InvalidRuntimeEngine.Get()));
		{
			FAngelscriptEngineScope Scope(*InvalidRuntimeEngine);
			InvalidRuntimeEngine->InitialCompile();
			const TSharedPtr<FAngelscriptModuleDesc> Module =
				InvalidRuntimeEngine->GetModuleByModuleName(
					TEXT("RuntimeCoordinator"));
			ASSERT_THAT(IsTrue(Module.IsValid()));
			asIScriptFunction* Function =
				Module->ScriptModule->GetFunctionByDecl("int AddOne(int)");
			ASSERT_THAT(IsNotNull(Function));
			const asSJITFunctionBinding Binding = Function->GetJITBinding();
			ASSERT_THAT(IsTrue(Binding.VMEntry == &StaticVmEntry));
			ASSERT_THAT(IsTrue(Binding.RawEntry == &StaticRawEntry));
		}

		FAngelscriptEngineConfig InvalidModeConfig = MakeRuntimeConfig(
			InvalidModeProject,
			EAngelscriptRuntimeJITCompilePolicy::EagerSync,
			static_cast<EAngelscriptJITExecutionMode>(255));
		TUniquePtr<FAngelscriptEngine> InvalidModeEngine =
			CreateScriptScanFreeFullEngineForTesting(
				InvalidModeConfig,
				MakeDependencies(InvalidModeProject));
		ASSERT_THAT(IsNotNull(InvalidModeEngine.Get()));
		{
			FAngelscriptEngineScope Scope(*InvalidModeEngine);
			InvalidModeEngine->InitialCompile();
			const TSharedPtr<FAngelscriptModuleDesc> Module =
				InvalidModeEngine->GetModuleByModuleName(
					TEXT("RuntimeCoordinator"));
			ASSERT_THAT(IsTrue(Module.IsValid()));
			asIScriptFunction* Function =
				Module->ScriptModule->GetFunctionByDecl("int AddOne(int)");
			ASSERT_THAT(IsNotNull(Function));
			const asSJITFunctionBinding Binding = Function->GetJITBinding();
			ASSERT_THAT(IsNull(Binding.VMEntry));
			ASSERT_THAT(IsNull(Binding.RawEntry));
			ASSERT_THAT(IsNull(Binding.ParmsEntry));
		}

		ASSERT_THAT(AreEqual(0,
			Factory.Control->SessionCreateCount.GetValue()));
		ASSERT_THAT(AreEqual(0,
			Factory.Control->CompileCount.GetValue()));

		{
			FAngelscriptEngineScope Scope(*InvalidModeEngine);
			ASSERT_THAT(IsTrue(InvalidModeEngine->DiscardModule(
				TEXT("RuntimeCoordinator"))));
		}
		InvalidModeEngine.Reset();
		{
			FAngelscriptEngineScope Scope(*InvalidRuntimeEngine);
			ASSERT_THAT(IsTrue(InvalidRuntimeEngine->DiscardModule(
				TEXT("RuntimeCoordinator"))));
		}
		InvalidRuntimeEngine.Reset();
		{
			FAngelscriptEngineScope Scope(*Producer);
			ASSERT_THAT(IsTrue(Producer->DiscardModule(
				TEXT("RuntimeCoordinator"))));
		}
		Producer.Reset();
	}

	TEST_METHOD(ForcedModesIgnoreTheUnrequestedNativeTier)
	{
		FFactory Factory;
		FScopedFactoryRegistration Registration(Factory);
		FScopedProjectRoot ProducerProject;
		FScopedProjectRoot StaticProject;
		FScopedProjectRoot RuntimeProject;
		ASSERT_THAT(IsTrue(ProducerProject.WriteSource()));
		ASSERT_THAT(IsTrue(StaticProject.WriteSource()));
		ASSERT_THAT(IsTrue(RuntimeProject.WriteSource()));

		TUniquePtr<FAngelscriptEngine> Producer =
			CreateScriptScanFreeFullEngineForTesting(
				MakeRuntimeConfig(
					ProducerProject,
					EAngelscriptRuntimeJITCompilePolicy::EagerSync,
					EAngelscriptJITExecutionMode::VMOnly),
				MakeDependencies(ProducerProject));
		ASSERT_THAT(IsNotNull(Producer.Get()));
		TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> ProducerRoutes;
		{
			FAngelscriptEngineScope Scope(*Producer);
			Producer->InitialCompile();
			ProducerRoutes = Producer->GetFunctionRouteSnapshot();
			ASSERT_THAT(IsTrue(ProducerRoutes.IsValid()));
		}
		FExactRouteProvider Provider(
			*ProducerRoutes,
			FAngelscriptJITProviderRouter::ResolveTargetProfile(
				Producer->GetRuntimeConfig()));
		FAngelscriptJITProviderRegistry& Registry =
			FAngelscriptJITProviderRegistry::Get();
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&Provider, Provider).IsAccepted()));
		ON_SCOPE_EXIT
		{
			Registry.UnregisterOwner(&Provider);
		};

		TUniquePtr<FAngelscriptEngine> StaticEngine =
			CreateScriptScanFreeFullEngineForTesting(
				MakeRuntimeConfig(
					StaticProject,
					EAngelscriptRuntimeJITCompilePolicy::EagerSync,
					EAngelscriptJITExecutionMode::StaticAOTOnly),
				MakeDependencies(StaticProject));
		ASSERT_THAT(IsNotNull(StaticEngine.Get()));

		{
			FAngelscriptEngineScope Scope(*StaticEngine);
			StaticEngine->InitialCompile();
			const TSharedPtr<FAngelscriptModuleDesc> Module =
				StaticEngine->GetModuleByModuleName(TEXT("RuntimeCoordinator"));
			ASSERT_THAT(IsTrue(Module.IsValid()));
			asIScriptFunction* Function =
				Module->ScriptModule->GetFunctionByDecl("int AddOne(int)");
			ASSERT_THAT(IsNotNull(Function));
			const asSJITFunctionBinding Binding = Function->GetJITBinding();
			ASSERT_THAT(IsTrue(Binding.VMEntry == &StaticVmEntry));
			ASSERT_THAT(IsTrue(Binding.RawEntry == &StaticRawEntry));
		}
		ASSERT_THAT(AreEqual(0,
			Factory.Control->SessionCreateCount.GetValue()));

		TUniquePtr<FAngelscriptEngine> RuntimeEngine =
			CreateScriptScanFreeFullEngineForTesting(
				MakeRuntimeConfig(
					RuntimeProject,
					EAngelscriptRuntimeJITCompilePolicy::EagerSync,
					EAngelscriptJITExecutionMode::RuntimeOnly),
				MakeDependencies(RuntimeProject));
		ASSERT_THAT(IsNotNull(RuntimeEngine.Get()));
		{
			FAngelscriptEngineScope Scope(*RuntimeEngine);
			RuntimeEngine->InitialCompile();
			const TSharedPtr<FAngelscriptModuleDesc> Module =
				RuntimeEngine->GetModuleByModuleName(TEXT("RuntimeCoordinator"));
			ASSERT_THAT(IsTrue(Module.IsValid()));
			asIScriptFunction* Function =
				Module->ScriptModule->GetFunctionByDecl("int AddOne(int)");
			ASSERT_THAT(IsNotNull(Function));
			const asSJITFunctionBinding Binding = Function->GetJITBinding();
			ASSERT_THAT(IsTrue(Binding.VMEntry == &RuntimeVmEntry));
			ASSERT_THAT(IsNull(Binding.RawEntry));
			ASSERT_THAT(IsNull(Binding.ParmsEntry));
		}
		ASSERT_THAT(AreEqual(1,
			Factory.Control->SessionCreateCount.GetValue()));
		ASSERT_THAT(AreEqual(1,
			Factory.Control->CompileCount.GetValue()));

		{
			FAngelscriptEngineScope Scope(*RuntimeEngine);
			ASSERT_THAT(IsTrue(RuntimeEngine->DiscardModule(
				TEXT("RuntimeCoordinator"))));
		}
		RuntimeEngine.Reset();
		{
			FAngelscriptEngineScope Scope(*StaticEngine);
			ASSERT_THAT(IsTrue(StaticEngine->DiscardModule(
				TEXT("RuntimeCoordinator"))));
		}
		StaticEngine.Reset();
		{
			FAngelscriptEngineScope Scope(*Producer);
			ASSERT_THAT(IsTrue(Producer->DiscardModule(
				TEXT("RuntimeCoordinator"))));
		}
		Producer.Reset();
		ASSERT_THAT(AreEqual(1,
			Factory.Control->SessionDestroyCount.GetValue()));
		ASSERT_THAT(AreEqual(1,
			Factory.Control->LeaseReleaseCount.GetValue()));
	}

	TEST_METHOD(RuntimePublicationDoesNotMutateTheAotProviderRegistry)
	{
		FFactory Factory;
		FScopedFactoryRegistration Registration(Factory);
		FScopedProjectRoot Project;
		ASSERT_THAT(IsTrue(Project.WriteSource()));

		FAngelscriptJITProviderRegistry& Registry =
			FAngelscriptJITProviderRegistry::Get();
		const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> Before = Registry.GetSnapshot();
		ASSERT_THAT(IsTrue(Before.IsValid()));

		TUniquePtr<FAngelscriptEngine> Engine =
			CreateScriptScanFreeFullEngineForTesting(
				MakeRuntimeConfig(Project), MakeDependencies(Project));
		ASSERT_THAT(IsNotNull(Engine.Get()));
		{
			FAngelscriptEngineScope Scope(*Engine);
			Engine->InitialCompile();
			const TSharedPtr<FAngelscriptModuleDesc> Module =
				Engine->GetModuleByModuleName(TEXT("RuntimeCoordinator"));
			ASSERT_THAT(IsTrue(Module.IsValid()));
			asIScriptFunction* Function =
				Module->ScriptModule->GetFunctionByDecl("int AddOne(int)");
			ASSERT_THAT(IsNotNull(Function));
			ASSERT_THAT(IsTrue(
				Function->GetJITBinding().VMEntry == &RuntimeVmEntry));

			const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
				ESPMode::ThreadSafe> AfterPublication = Registry.GetSnapshot();
			ASSERT_THAT(IsTrue(AfterPublication.IsValid()));
			ASSERT_THAT(AreEqual(
				Before->PublicationOrdinal,
				AfterPublication->PublicationOrdinal,
				TEXT("Engine-local Runtime publication must not publish an AOT Provider generation")));
			ASSERT_THAT(AreEqual(
				Before->Providers.Num(),
				AfterPublication->Providers.Num(),
				TEXT("Engine-local Runtime publication must not add an AOT Provider")));
			ASSERT_THAT(IsTrue(Engine->DiscardModule(
				TEXT("RuntimeCoordinator"))));
		}
		Engine.Reset();

		const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> AfterTeardown = Registry.GetSnapshot();
		ASSERT_THAT(IsTrue(AfterTeardown.IsValid()));
		ASSERT_THAT(AreEqual(
			Before->PublicationOrdinal,
			AfterTeardown->PublicationOrdinal,
			TEXT("Runtime Engine teardown must not retire or republish AOT Provider state")));
	}

	TEST_METHOD(WholeFunctionEligibilityFallsBackWithoutPartialNativeEntries)
	{
		FFactory Factory;
		Factory.Control->bRejectHelperTokens.Store(true);
		FScopedFactoryRegistration Registration(Factory);
		FScopedProjectRoot Project;
		ASSERT_THAT(IsTrue(Project.WriteSourceText(ASTEST_AS(R"AS(
			int ScalarEligible(int Value)
			{
				return Value + 1;
			}

			int UsesSystemCall(int Value)
			{
				return Math::Abs(Value);
			}
			int UsesSuspendBoundary(int Value)
			{
				int Sum = 0;
				for (int Index = 0; Index < Value; ++Index)
				{
					Sum += Index;
				}
				return Sum;
			}
			)AS"))));

		TUniquePtr<FAngelscriptEngine> Engine =
			CreateScriptScanFreeFullEngineForTesting(
				MakeRuntimeConfig(Project), MakeDependencies(Project));
		ASSERT_THAT(IsNotNull(Engine.Get()));
		{
			FAngelscriptEngineScope Scope(*Engine);
			Engine->InitialCompile();
			const TSharedPtr<FAngelscriptModuleDesc> Module =
				Engine->GetModuleByModuleName(TEXT("RuntimeCoordinator"));
			ASSERT_THAT(IsTrue(Module.IsValid()));
			ASSERT_THAT(IsNotNull(Module->ScriptModule));

			asIScriptFunction* Scalar = Module->ScriptModule->GetFunctionByDecl(
				"int ScalarEligible(int)");
			asIScriptFunction* Call = Module->ScriptModule->GetFunctionByDecl(
				"int UsesSystemCall(int)");
			asIScriptFunction* Suspend = Module->ScriptModule->GetFunctionByDecl(
				"int UsesSuspendBoundary(int)");
			ASSERT_THAT(IsNotNull(Scalar));
			ASSERT_THAT(IsNotNull(Call));
			ASSERT_THAT(IsNotNull(Suspend));
			ASSERT_THAT(IsTrue(
				Scalar->GetJITBinding().VMEntry == &RuntimeVmEntry));
			for (asIScriptFunction* Unsupported : {Call, Suspend})
			{
				const asSJITFunctionBinding Binding =
					Unsupported->GetJITBinding();
				ASSERT_THAT(IsNull(Binding.VMEntry));
				ASSERT_THAT(IsNull(Binding.RawEntry));
				ASSERT_THAT(IsNull(Binding.ParmsEntry));
			}
			ASSERT_THAT(AreEqual(1,
				Factory.Control->HelperTokenCompileCount.GetValue(),
				TEXT("The call-bearing function should reach the backend once as one whole-function request")));

			int32 Result = 0;
			ASSERT_THAT(IsTrue(ExecuteAddOne(
				*Engine, *Call, -41, Result)));
			ASSERT_THAT(AreEqual(41, Result,
				TEXT("An unsupported call-bearing function must execute completely through VM")));
			ASSERT_THAT(IsTrue(ExecuteAddOne(
				*Engine, *Suspend, 3, Result)));
			ASSERT_THAT(AreEqual(3, Result,
				TEXT("A suspend-boundary function must execute completely through VM")));

			const int32 CompileCount = Factory.Control->CompileCount.GetValue();
			ASSERT_THAT(IsTrue(
				Engine->GetJITCoordinator()->ProcessRuntimeSafePoint(*Engine)));
			ASSERT_THAT(AreEqual(
				CompileCount,
				Factory.Control->CompileCount.GetValue(),
				TEXT("Unchanged whole-function fallback decisions must be memoized")));
			ASSERT_THAT(IsTrue(Engine->DiscardModule(
				TEXT("RuntimeCoordinator"))));
		}
		Engine.Reset();
	}

#if WITH_AS_DEBUGSERVER
	TEST_METHOD(ActiveDebuggerSuppressesRuntimeUntilBytecodeVisibilityIsReleased)
	{
		FFactory Factory;
		FScopedFactoryRegistration Registration(Factory);
		FScopedProjectRoot Project;
		ASSERT_THAT(IsTrue(Project.WriteSource()));
		TUniquePtr<FAngelscriptEngine> Engine =
			CreateScriptScanFreeFullEngineForTesting(
				MakeRuntimeConfig(Project), MakeDependencies(Project));
		ASSERT_THAT(IsNotNull(Engine.Get()));
		if (Engine->DebugServer == nullptr)
		{
			Engine->DebugServer = new FAngelscriptDebugServer(Engine.Get(), 0);
		}
		Engine->DebugServer->bIsDebugging = true;
		Engine->UpdateLineCallbackState();

		{
			FAngelscriptEngineScope Scope(*Engine);
			Engine->InitialCompile();
			const TSharedPtr<FAngelscriptModuleDesc> Module =
				Engine->GetModuleByModuleName(TEXT("RuntimeCoordinator"));
			ASSERT_THAT(IsTrue(Module.IsValid()));
			asIScriptFunction* Function =
				Module->ScriptModule->GetFunctionByDecl("int AddOne(int)");
			ASSERT_THAT(IsNotNull(Function));
			ASSERT_THAT(IsNull(Function->GetJITBinding().VMEntry));
			ASSERT_THAT(AreEqual(0,
				Factory.Control->CompileCount.GetValue(),
				TEXT("Debugger bytecode visibility must suppress Runtime compilation")));

			int32 Result = 0;
			ASSERT_THAT(IsTrue(ExecuteAddOne(
				*Engine, *Function, 41, Result)));
			ASSERT_THAT(AreEqual(42, Result));

			Engine->DebugServer->bIsDebugging = false;
			Engine->UpdateLineCallbackState();
			ASSERT_THAT(IsTrue(
				Engine->GetJITCoordinator()->ProcessRuntimeSafePoint(*Engine)));
			ASSERT_THAT(IsTrue(
				Function->GetJITBinding().VMEntry == &RuntimeVmEntry));
			ASSERT_THAT(AreEqual(1,
				Factory.Control->CompileCount.GetValue()));
			ASSERT_THAT(IsTrue(Engine->DiscardModule(
				TEXT("RuntimeCoordinator"))));
		}
		Engine.Reset();
	}
#endif

#if WITH_AS_COVERAGE
	TEST_METHOD(CodeCoverageSuppressesRuntimeButPreservesExactAotSelection)
	{
		UAngelscriptTestSettings* Settings =
			GetMutableDefault<UAngelscriptTestSettings>();
		const bool bWasCoverageEnabled = Settings->bEnableCodeCoverage;
		Settings->bEnableCodeCoverage = true;
		ON_SCOPE_EXIT
		{
			Settings->bEnableCodeCoverage = bWasCoverageEnabled;
		};

		FFactory Factory;
		FScopedFactoryRegistration Registration(Factory);
		FScopedProjectRoot RuntimeProject;
		ASSERT_THAT(IsTrue(RuntimeProject.WriteSource()));
		TUniquePtr<FAngelscriptEngine> RuntimeEngine =
			CreateScriptScanFreeFullEngineForTesting(
				MakeRuntimeConfig(RuntimeProject),
				MakeDependencies(RuntimeProject));
		ASSERT_THAT(IsNotNull(RuntimeEngine.Get()));
		{
			FAngelscriptEngineScope Scope(*RuntimeEngine);
			RuntimeEngine->InitialCompile();
			ASSERT_THAT(IsNotNull(
				FAngelscriptCodeCoverageExtension::GetForEngine(
					*RuntimeEngine)));
			const TSharedPtr<FAngelscriptModuleDesc> Module =
				RuntimeEngine->GetModuleByModuleName(TEXT("RuntimeCoordinator"));
			ASSERT_THAT(IsTrue(Module.IsValid()));
			asIScriptFunction* Function =
				Module->ScriptModule->GetFunctionByDecl("int AddOne(int)");
			ASSERT_THAT(IsNotNull(Function));
			ASSERT_THAT(IsNull(Function->GetJITBinding().VMEntry));
			ASSERT_THAT(AreEqual(0,
				Factory.Control->CompileCount.GetValue(),
				TEXT("Coverage bytecode visibility must suppress Runtime compilation")));
			ASSERT_THAT(IsTrue(RuntimeEngine->DiscardModule(
				TEXT("RuntimeCoordinator"))));
		}
		RuntimeEngine.Reset();

		FScopedProjectRoot ProducerProject;
		FScopedProjectRoot ConsumerProject;
		ASSERT_THAT(IsTrue(ProducerProject.WriteSource()));
		ASSERT_THAT(IsTrue(ConsumerProject.WriteSource()));
		TUniquePtr<FAngelscriptEngine> Producer =
			CreateScriptScanFreeFullEngineForTesting(
				MakeRuntimeConfig(
					ProducerProject,
					EAngelscriptRuntimeJITCompilePolicy::EagerSync,
					EAngelscriptJITExecutionMode::VMOnly),
				MakeDependencies(ProducerProject));
		ASSERT_THAT(IsNotNull(Producer.Get()));
		TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> ProducerRoutes;
		{
			FAngelscriptEngineScope Scope(*Producer);
			Producer->InitialCompile();
			ProducerRoutes = Producer->GetFunctionRouteSnapshot();
			ASSERT_THAT(IsTrue(ProducerRoutes.IsValid()));
		}

		FExactRouteProvider Provider(
			*ProducerRoutes,
			FAngelscriptJITProviderRouter::ResolveTargetProfile(
				Producer->GetRuntimeConfig()));
		FAngelscriptJITProviderRegistry& Registry =
			FAngelscriptJITProviderRegistry::Get();
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&Provider, Provider).IsAccepted()));
		ON_SCOPE_EXIT
		{
			Registry.UnregisterOwner(&Provider);
		};

		TUniquePtr<FAngelscriptEngine> Consumer =
			CreateScriptScanFreeFullEngineForTesting(
				MakeRuntimeConfig(
					ConsumerProject,
					EAngelscriptRuntimeJITCompilePolicy::EagerSync,
					EAngelscriptJITExecutionMode::Auto),
				MakeDependencies(ConsumerProject));
		ASSERT_THAT(IsNotNull(Consumer.Get()));
		{
			FAngelscriptEngineScope Scope(*Consumer);
			Consumer->InitialCompile();
			ASSERT_THAT(IsNotNull(
				FAngelscriptCodeCoverageExtension::GetForEngine(*Consumer)));
			const TSharedPtr<FAngelscriptModuleDesc> Module =
				Consumer->GetModuleByModuleName(TEXT("RuntimeCoordinator"));
			ASSERT_THAT(IsTrue(Module.IsValid()));
			asIScriptFunction* Function =
				Module->ScriptModule->GetFunctionByDecl("int AddOne(int)");
			ASSERT_THAT(IsNotNull(Function));
			const asSJITFunctionBinding Binding = Function->GetJITBinding();
			ASSERT_THAT(IsTrue(Binding.VMEntry == &StaticVmEntry));
			ASSERT_THAT(IsTrue(Binding.RawEntry == &StaticRawEntry));
			ASSERT_THAT(AreEqual(0,
				Factory.Control->CompileCount.GetValue(),
				TEXT("The Runtime-specific coverage gate must not redefine exact Static AOT policy")));
			ASSERT_THAT(IsTrue(Consumer->DiscardModule(
				TEXT("RuntimeCoordinator"))));
		}
		Consumer.Reset();
		{
			FAngelscriptEngineScope Scope(*Producer);
			ASSERT_THAT(IsTrue(Producer->DiscardModule(
				TEXT("RuntimeCoordinator"))));
		}
		Producer.Reset();
	}
#endif
};

#endif // WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT
