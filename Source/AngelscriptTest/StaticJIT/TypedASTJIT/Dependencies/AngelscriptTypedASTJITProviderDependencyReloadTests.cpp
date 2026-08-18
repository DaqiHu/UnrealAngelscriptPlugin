#include "StaticJIT/TypedASTJIT/Dependencies/AngelscriptTypedASTJITProviderDependencyTestSupport.h"

#include "Async/Async.h"
#include "CQTest.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptTypedASTJITProviderDependencyTestSupport;

namespace AngelscriptTypedASTJITProviderDependencyReloadTests_Private
{
	static FEvent* BlockingEntryStarted = nullptr;
	static FEvent* BlockingEntryContinue = nullptr;
	static int32 BlockingNativeCalls = 0;

	static void BlockingProviderVmEntry(
		FScriptExecution&,
		asDWORD*,
		asQWORD* OutValue)
	{
		++BlockingNativeCalls;
		if (BlockingEntryStarted != nullptr)
		{
			BlockingEntryStarted->Trigger();
		}
		if (BlockingEntryContinue != nullptr)
		{
			BlockingEntryContinue->Wait(5000);
		}
		if (OutValue != nullptr)
		{
			*OutValue = 700;
		}
	}

	static const TCHAR* InitialScript = TEXT(R"AS(
int Helper()
{
	return 1;
}

int RootA()
{
	return Helper() + 10;
}

int RootB()
{
	return Helper() + 20;
}

int Unrelated()
{
	return 30;
}

int DynamicRouted()
{
	return Helper() + 40;
}
)AS");

	static const TCHAR* ReloadedHelperScript = TEXT(R"AS(
int Helper()
{
	return 100;
}

int RootA()
{
	return Helper() + 10;
}

int RootB()
{
	return Helper() + 20;
}

int Unrelated()
{
	return 30;
}

int DynamicRouted()
{
	return Helper() + 40;
}
)AS");

	static void AttachSharedHelperDependencies(
		FDependencyProvider& Provider,
		const FAngelscriptFunctionRouteSnapshot& Routes)
	{
		const FAngelscriptFunctionRoute* const RootA =
			FindRoute(Routes, TEXT("int RootA()"));
		const FAngelscriptFunctionRoute* const RootB =
			FindRoute(Routes, TEXT("int RootB()"));
		const FAngelscriptFunctionRoute* const Dynamic =
			FindRoute(Routes, TEXT("int DynamicRouted()"));
		const FAngelscriptFunctionRoute* const Helper =
			FindRoute(Routes, TEXT("int Helper()"));
		check(RootA != nullptr && RootB != nullptr && Dynamic != nullptr
			&& Helper != nullptr);
		Provider.SetEntryDependencies(
			RootA->Identity.FunctionKey, {MakeFunctionContent(*Helper)});
		Provider.SetEntryDependencies(
			RootB->Identity.FunctionKey, {MakeFunctionContent(*Helper)});
		Provider.SetEntryDependencies(
			Dynamic->Identity.FunctionKey, {MakeSignature(*Helper)});
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITProviderDependencyReloadTests,
	"Angelscript.TestModule.StaticJIT.TypedASTJIT.Dependencies.ProviderReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(IsolatedEnginesDoNotShareReverseIndex)
	{
		using namespace AngelscriptTypedASTJITProviderDependencyReloadTests_Private;
		FScopedProjectRoot ProjectA(TEXT("TypedASTJITDependencyReload"));
		FScopedProjectRoot ProjectB(TEXT("TypedASTJITDependencyReload"));
		ASSERT_THAT(IsTrue(ProjectA.WriteSource(InitialScript)));
		ASSERT_THAT(IsTrue(ProjectB.WriteSource(InitialScript)));
		TUniquePtr<FAngelscriptEngine> EngineA = CreateEngine(ProjectA);
		TUniquePtr<FAngelscriptEngine> EngineB = CreateEngine(ProjectB);
		ASSERT_THAT(IsNotNull(EngineA.Get()));
		ASSERT_THAT(IsNotNull(EngineB.Get()));
		if (!EngineA.IsValid() || !EngineB.IsValid())
		{
			return;
		}
		FAngelscriptEngineScope ScopeA(*EngineA);
		FAngelscriptEngineScope ScopeB(*EngineB);
		EngineA->InitialCompile();
		EngineB->InitialCompile();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> RoutesA = EngineA->GetFunctionRouteSnapshot();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> RoutesB = EngineB->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(RoutesA.IsValid()));
		ASSERT_THAT(IsTrue(RoutesB.IsValid()));
		if (!RoutesA.IsValid() || !RoutesB.IsValid())
		{
			return;
		}

		FDependencyProvider Provider(*RoutesA, TEXT("reload-isolated"));
		AttachSharedHelperDependencies(Provider, *RoutesA);
		FAngelscriptJITProviderRegistry& Registry =
			FAngelscriptJITProviderRegistry::Get();
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&Provider, Provider).IsAccepted()));
		ON_SCOPE_EXIT
		{
			Registry.UnregisterOwner(&Provider);
		};
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProviderRouter::Refresh(*EngineA).IsApplied()));
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProviderRouter::Refresh(*EngineB).IsApplied()));

		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> AdoptedA = EngineA->GetFunctionRouteSnapshot();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> AdoptedB = EngineB->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(AdoptedA.IsValid() && AdoptedB.IsValid()));
		if (!AdoptedA.IsValid() || !AdoptedB.IsValid())
		{
			return;
		}
		const FAngelscriptFunctionRoute* const AdoptedRootA =
			FindRoute(*AdoptedA, TEXT("int RootA()"));
		const FAngelscriptFunctionRoute* const AdoptedRootB =
			FindRoute(*AdoptedB, TEXT("int RootA()"));
		ASSERT_THAT(IsNotNull(AdoptedRootA));
		ASSERT_THAT(IsNotNull(AdoptedRootB));
		if (AdoptedRootA == nullptr || AdoptedRootB == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(IsNativeProviderBinding(*AdoptedRootA)));
		ASSERT_THAT(IsTrue(IsNativeProviderBinding(*AdoptedRootB)));

		ASSERT_THAT(IsTrue(ProjectA.WriteSource(ReloadedHelperScript)));
		ASSERT_THAT(IsTrue(
			ForceCleanAngelscriptCache(EngineA.Get(), ProjectA.ModuleName())
				.IsSuccess()));

		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> ReloadedA = EngineA->GetFunctionRouteSnapshot();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> UnchangedB = EngineB->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(ReloadedA.IsValid() && UnchangedB.IsValid()));
		if (!ReloadedA.IsValid() || !UnchangedB.IsValid())
		{
			return;
		}
		const FAngelscriptFunctionRoute* const ReloadedRootA =
			FindRoute(*ReloadedA, TEXT("int RootA()"));
		const FAngelscriptFunctionRoute* const IsolatedRootB =
			FindRoute(*UnchangedB, TEXT("int RootA()"));
		ASSERT_THAT(IsNotNull(ReloadedRootA));
		ASSERT_THAT(IsNotNull(IsolatedRootB));
		if (ReloadedRootA == nullptr || IsolatedRootB == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			EAngelscriptFunctionExecutionRoute::Vm,
			ReloadedRootA->SelectedExecutionRoute));
		ASSERT_THAT(IsTrue(IsNativeProviderBinding(*IsolatedRootB)));
	}

	TEST_METHOD(HelperOnlyHotReloadWithdrawsDependentsAndKeepsUnrelatedRoutes)
	{
		using namespace AngelscriptTypedASTJITProviderDependencyReloadTests_Private;
		FScopedProjectRoot Project(TEXT("TypedASTJITDependencyReload"));
		ASSERT_THAT(IsTrue(Project.WriteSource(InitialScript)));
		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		if (!Engine.IsValid())
		{
			return;
		}
		FAngelscriptEngineScope Scope(*Engine);
		Engine->InitialCompile();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Routes = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Routes.IsValid()));
		if (!Routes.IsValid())
		{
			return;
		}

		FDependencyProvider Provider(*Routes, TEXT("reload-helper"));
		AttachSharedHelperDependencies(Provider, *Routes);
		FAngelscriptJITProviderRegistry& Registry =
			FAngelscriptJITProviderRegistry::Get();
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&Provider, Provider).IsAccepted()));
		ON_SCOPE_EXIT
		{
			Registry.UnregisterOwner(&Provider);
		};
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProviderRouter::Refresh(*Engine).IsApplied()));

		ASSERT_THAT(IsTrue(Project.WriteSource(ReloadedHelperScript)));
		ASSERT_THAT(IsTrue(
			ForceCleanAngelscriptCache(Engine.Get(), Project.ModuleName())
				.IsSuccess()));

		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Reloaded = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Reloaded.IsValid()));
		if (!Reloaded.IsValid())
		{
			return;
		}
		const FAngelscriptFunctionRoute* const RootA =
			FindRoute(*Reloaded, TEXT("int RootA()"));
		const FAngelscriptFunctionRoute* const RootB =
			FindRoute(*Reloaded, TEXT("int RootB()"));
		const FAngelscriptFunctionRoute* const Unrelated =
			FindRoute(*Reloaded, TEXT("int Unrelated()"));
		const FAngelscriptFunctionRoute* const Dynamic =
			FindRoute(*Reloaded, TEXT("int DynamicRouted()"));
		ASSERT_THAT(IsNotNull(RootA));
		ASSERT_THAT(IsNotNull(RootB));
		ASSERT_THAT(IsNotNull(Unrelated));
		ASSERT_THAT(IsNotNull(Dynamic));
		if (RootA == nullptr || RootB == nullptr || Unrelated == nullptr
			|| Dynamic == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			EAngelscriptFunctionExecutionRoute::Vm,
			RootA->SelectedExecutionRoute));
		ASSERT_THAT(AreEqual(
			EAngelscriptFunctionExecutionRoute::Vm,
			RootB->SelectedExecutionRoute));
		ASSERT_THAT(IsTrue(IsNativeProviderBinding(*Unrelated)));
		ASSERT_THAT(IsTrue(IsNativeProviderBinding(*Dynamic)));
		ASSERT_THAT(IsNull(RootA->Function->GetJITBinding().VMEntry));
		ASSERT_THAT(IsNull(RootB->Function->GetJITBinding().VMEntry));
	}

	TEST_METHOD(SubsequentCallsUseVmAfterStaleWithdrawal)
	{
		using namespace AngelscriptTypedASTJITProviderDependencyReloadTests_Private;
		FScopedProjectRoot Project(TEXT("TypedASTJITDependencyReload"));
		ASSERT_THAT(IsTrue(Project.WriteSource(InitialScript)));
		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		if (!Engine.IsValid())
		{
			return;
		}
		FAngelscriptEngineScope Scope(*Engine);
		Engine->InitialCompile();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Routes = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Routes.IsValid()));
		if (!Routes.IsValid())
		{
			return;
		}

		FDependencyProvider Provider(*Routes, TEXT("reload-subsequent"));
		AttachSharedHelperDependencies(Provider, *Routes);
		FAngelscriptJITProviderRegistry& Registry =
			FAngelscriptJITProviderRegistry::Get();
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&Provider, Provider).IsAccepted()));
		ON_SCOPE_EXIT
		{
			Registry.UnregisterOwner(&Provider);
		};
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProviderRouter::Refresh(*Engine).IsApplied()));

		int32 NativeValue = 0;
		ASSERT_THAT(IsTrue(ExecuteInt(
			*Engine, Project.ModuleName(), TEXT("int RootA()"), NativeValue)));
		ASSERT_THAT(AreEqual(700, NativeValue));

		ASSERT_THAT(IsTrue(Project.WriteSource(ReloadedHelperScript)));
		ASSERT_THAT(IsTrue(
			ForceCleanAngelscriptCache(Engine.Get(), Project.ModuleName())
				.IsSuccess()));

		int32 VmValue = 0;
		ASSERT_THAT(IsTrue(ExecuteInt(
			*Engine, Project.ModuleName(), TEXT("int RootA()"), VmValue)));
		ASSERT_THAT(AreEqual(110, VmValue));
		int32 UnrelatedValue = 0;
		ASSERT_THAT(IsTrue(ExecuteInt(
			*Engine,
			Project.ModuleName(),
			TEXT("int Unrelated()"),
			UnrelatedValue)));
		ASSERT_THAT(AreEqual(700, UnrelatedValue));
	}

	TEST_METHOD(InFlightLeasedCallCompletesWithoutForcedUnload)
	{
		using namespace AngelscriptTypedASTJITProviderDependencyReloadTests_Private;
		FScopedProjectRoot Project(TEXT("TypedASTJITDependencyReload"));
		ASSERT_THAT(IsTrue(Project.WriteSource(InitialScript)));
		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		if (!Engine.IsValid())
		{
			return;
		}
		FAngelscriptEngineScope Scope(*Engine);
		Engine->InitialCompile();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Routes = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Routes.IsValid()));
		if (!Routes.IsValid())
		{
			return;
		}
		const FAngelscriptFunctionRoute* const Root =
			FindRoute(*Routes, TEXT("int RootA()"));
		const FAngelscriptFunctionRoute* const Helper =
			FindRoute(*Routes, TEXT("int Helper()"));
		ASSERT_THAT(IsNotNull(Root));
		ASSERT_THAT(IsNotNull(Helper));
		if (Root == nullptr || Helper == nullptr)
		{
			return;
		}

		FDependencyProvider Provider(*Routes, TEXT("reload-lease"));
		AttachSharedHelperDependencies(Provider, *Routes);
		const int32 RootEntryIndex =
			Provider.FindEntryIndex(Root->Identity.FunctionKey);
		ASSERT_THAT(AreNotEqual(INDEX_NONE, RootEntryIndex));
		Provider.Entries[RootEntryIndex].EntryPoints.VMEntry =
			&BlockingProviderVmEntry;
		Provider.Rebuild();
		FAngelscriptJITProviderRegistry& Registry =
			FAngelscriptJITProviderRegistry::Get();
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&Provider, Provider).IsAccepted()));
		ON_SCOPE_EXIT
		{
			Registry.UnregisterOwner(&Provider);
		};
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProviderRouter::Refresh(*Engine).IsApplied()));

		const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> AdoptedSnapshot = Registry.GetSnapshot();
		ASSERT_THAT(IsTrue(AdoptedSnapshot.IsValid()));
		TWeakPtr<const FAngelscriptJITProviderCatalog, ESPMode::ThreadSafe>
			WeakCatalog;
		TWeakPtr<const FAngelscriptJITProviderLifetime, ESPMode::ThreadSafe>
			WeakLifetime;
		if (AdoptedSnapshot.IsValid() && AdoptedSnapshot->Providers.Num() > 0)
		{
			WeakCatalog = AdoptedSnapshot->Providers[0];
			WeakLifetime = AdoptedSnapshot->Providers[0]->Lifetime;
		}
		ASSERT_THAT(IsTrue(WeakCatalog.IsValid()));
		ASSERT_THAT(IsTrue(WeakLifetime.IsValid()));

		asIScriptFunction* const Function = Root->Function;
		ASSERT_THAT(IsNotNull(Function));
		if (Function == nullptr)
		{
			return;
		}
		asIScriptContext* const Context =
			Engine->GetScriptEngine()->CreateContext();
		ASSERT_THAT(IsNotNull(Context));
		if (Context == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Function)));

		BlockingNativeCalls = 0;
		BlockingEntryStarted = FPlatformProcess::GetSynchEventFromPool(true);
		BlockingEntryContinue = FPlatformProcess::GetSynchEventFromPool(true);
		ON_SCOPE_EXIT
		{
			if (BlockingEntryContinue != nullptr)
			{
				BlockingEntryContinue->Trigger();
			}
			FPlatformProcess::ReturnSynchEventToPool(BlockingEntryStarted);
			FPlatformProcess::ReturnSynchEventToPool(BlockingEntryContinue);
			BlockingEntryStarted = nullptr;
			BlockingEntryContinue = nullptr;
		};

		TFuture<TPair<int32, asDWORD>> Execution = Async(
			EAsyncExecution::Thread,
			[Context]()
			{
				const int32 Result = Context->Execute();
				const asDWORD Value = Context->GetReturnDWord();
				Context->Release();
				return TPair<int32, asDWORD>(Result, Value);
			});
		const bool bStarted = BlockingEntryStarted->Wait(5000);
		ASSERT_THAT(IsTrue(bStarted));
		if (!bStarted)
		{
			BlockingEntryContinue->Trigger();
			Execution.Wait();
			return;
		}

		FAngelscriptArtifactSemanticDependency Stale =
			MakeFunctionContent(*Helper);
		Stale.ExpectedContentOrValue = MakeHash(TEXT("stale-helper-execution"));
		Registry.UnregisterOwner(&Provider);
		FDependencyProvider StaleProvider(*Routes, TEXT("reload-lease-stale"));
		StaleProvider.SetEntryDependencies(Root->Identity.FunctionKey, {Stale});
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&StaleProvider, StaleProvider)
				.IsAccepted()));
		ON_SCOPE_EXIT
		{
			Registry.UnregisterOwner(&StaleProvider);
		};
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProviderRouter::Refresh(*Engine).IsApplied()));
		ASSERT_THAT(IsTrue(WeakCatalog.IsValid()));
		ASSERT_THAT(IsTrue(WeakLifetime.IsValid()));
		ASSERT_THAT(IsTrue(Registry.GetSnapshot().IsValid()));
		ASSERT_THAT(AreEqual(1, BlockingNativeCalls));

		BlockingEntryContinue->Trigger();
		const TPair<int32, asDWORD> ExecutionResult = Execution.Get();
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ExecutionResult.Key));
		ASSERT_THAT(AreEqual(700u, ExecutionResult.Value));

		int32 Subsequent = 0;
		ASSERT_THAT(IsTrue(ExecuteInt(
			*Engine, Project.ModuleName(), TEXT("int RootA()"), Subsequent)));
		ASSERT_THAT(AreEqual(710, Subsequent));
		ASSERT_THAT(AreEqual(1, BlockingNativeCalls));
	}

	TEST_METHOD(MatchingProviderRestoresNativeAfterWithdrawal)
	{
		using namespace AngelscriptTypedASTJITProviderDependencyReloadTests_Private;
		FScopedProjectRoot Project(TEXT("TypedASTJITDependencyReload"));
		ASSERT_THAT(IsTrue(Project.WriteSource(InitialScript)));
		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		if (!Engine.IsValid())
		{
			return;
		}
		FAngelscriptEngineScope Scope(*Engine);
		Engine->InitialCompile();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> InitialRoutes =
			Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(InitialRoutes.IsValid()));
		if (!InitialRoutes.IsValid())
		{
			return;
		}

		FDependencyProvider InitialProvider(*InitialRoutes, TEXT("reload-restore-initial"));
		AttachSharedHelperDependencies(InitialProvider, *InitialRoutes);
		FAngelscriptJITProviderRegistry& Registry =
			FAngelscriptJITProviderRegistry::Get();
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&InitialProvider, InitialProvider)
				.IsAccepted()));
		ON_SCOPE_EXIT
		{
			Registry.UnregisterOwner(&InitialProvider);
		};
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProviderRouter::Refresh(*Engine).IsApplied()));

		ASSERT_THAT(IsTrue(Project.WriteSource(ReloadedHelperScript)));
		ASSERT_THAT(IsTrue(
			ForceCleanAngelscriptCache(Engine.Get(), Project.ModuleName())
				.IsSuccess()));
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> ReloadedRoutes =
			Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(ReloadedRoutes.IsValid()));
		if (!ReloadedRoutes.IsValid())
		{
			return;
		}
		const FAngelscriptFunctionRoute* const Withdrawn =
			FindRoute(*ReloadedRoutes, TEXT("int RootA()"));
		ASSERT_THAT(IsNotNull(Withdrawn));
		if (Withdrawn == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			EAngelscriptFunctionExecutionRoute::Vm,
			Withdrawn->SelectedExecutionRoute));

		Registry.UnregisterOwner(&InitialProvider);
		FDependencyProvider RestoredProvider(
			*ReloadedRoutes, TEXT("reload-restore-new"));
		AttachSharedHelperDependencies(RestoredProvider, *ReloadedRoutes);
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&RestoredProvider, RestoredProvider)
				.IsAccepted()));
		ON_SCOPE_EXIT
		{
			Registry.UnregisterOwner(&RestoredProvider);
		};
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProviderRouter::Refresh(*Engine).IsApplied()));
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> RestoredRoutes =
			Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(RestoredRoutes.IsValid()));
		if (!RestoredRoutes.IsValid())
		{
			return;
		}
		const FAngelscriptFunctionRoute* const Restored =
			FindRoute(*RestoredRoutes, TEXT("int RootA()"));
		ASSERT_THAT(IsNotNull(Restored));
		if (Restored == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(IsNativeProviderBinding(*Restored)));
		int32 RestoredValue = 0;
		ASSERT_THAT(IsTrue(ExecuteInt(
			*Engine,
			Project.ModuleName(),
			TEXT("int RootA()"),
			RestoredValue)));
		ASSERT_THAT(AreEqual(700, RestoredValue));
	}
};

#endif
