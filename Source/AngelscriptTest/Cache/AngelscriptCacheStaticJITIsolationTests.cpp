#include "Cache/AngelscriptCacheRestore.h"
#include "Cache/AngelscriptCacheService.h"

#include "AngelscriptEngine.h"
#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Shared/AngelscriptTestEngineAcquisition.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheStaticJITIsolationTests_Private
{
	class FScopedProjectRoot final
	{
	public:
		FScopedProjectRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheStaticJITIsolation"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheStaticJITIsolation/")));
			ScriptRoot = Root / TEXT("Script");
			CacheRoot = Root / TEXT("CacheV2");
			check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		}

		~FScopedProjectRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheStaticJITIsolation/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		bool WriteSource() const
		{
			return FFileHelper::SaveStringToFile(TEXT(R"AS(
enum EStaticJITIsolationState
{
	Ready = 1,
}

int ReadStaticJITIsolationA()
{
	return 701;
}

int ReadStaticJITIsolationB()
{
	return 702;
}
)AS"), *(ScriptRoot / TEXT("StaticJITIsolation.as")));
		}

		FString Root;
		FString ScriptRoot;
		FString CacheRoot;
	};

	static TUniquePtr<FAngelscriptEngine> CreateEngine(
		const FScopedProjectRoot& Project)
	{
		FAngelscriptEngineConfig Config;
		Config.bIsEditor = true;
		Config.bDevelopmentMode = true;
		Config.bSkipThreadedInitialize = true;
		Config.CacheV2RootOverride = Project.CacheRoot;

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
		return CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
	}

	static const FAngelscriptFunctionRoute* FindRoute(
		const FAngelscriptFunctionRouteSnapshot& Snapshot,
		const FString& CanonicalDeclaration)
	{
		return Snapshot.FunctionRoutes.FindByPredicate(
			[&CanonicalDeclaration](
				const FAngelscriptFunctionRoute& Route)
			{
				return Route.CanonicalDeclaration == CanonicalDeclaration;
			});
	}

	static void SyntheticVmEntry(
		FScriptExecution&,
		asDWORD*,
		asQWORD*)
	{
	}

	static void SyntheticParmsEntry(FScriptExecution&, void*, void*)
	{
	}

	static void SyntheticRawEntry()
	{
	}

	class FScopedSyntheticJitCompiler final : public asIJITCompiler
	{
	public:
		explicit FScopedSyntheticJitCompiler(FAngelscriptEngine& Engine)
			: ScriptEngine(Engine.GetScriptEngine())
		{
			check(ScriptEngine != nullptr);
			check(ScriptEngine->SetJITCompiler(this) == asSUCCESS);
		}

		~FScopedSyntheticJitCompiler() override
		{
			check(ScriptEngine->SetJITCompiler(nullptr) == asSUCCESS);
		}

		void OnFunctionReady(asIScriptFunction*) override
		{
		}

		void ReleaseFunctionBinding(
			asIScriptFunction*,
			const asSJITFunctionBinding&) override
		{
		}

	private:
		asIScriptEngine* ScriptEngine = nullptr;
	};

	static void SetSyntheticNativeEntries(
		const FAngelscriptFunctionRoute& Route,
		const bool bNative)
	{
		check(Route.Function != nullptr);
		asCScriptFunction* Function =
			static_cast<asCScriptFunction*>(Route.Function);
		asSJITFunctionBinding Binding;
		if (bNative)
		{
			Binding.VMEntry = &SyntheticVmEntry;
			Binding.RawEntry = &SyntheticRawEntry;
			Binding.ParmsEntry = &SyntheticParmsEntry;
		}
		check(Function->SetJITBinding(Binding) == asSUCCESS);
	}

	class FScopedSyntheticEntryCleanup final
	{
	public:
		void Add(const FAngelscriptFunctionRoute& Route)
		{
			check(Route.Function != nullptr);
			Functions.AddUnique(
				static_cast<asCScriptFunction*>(Route.Function));
		}

		~FScopedSyntheticEntryCleanup()
		{
			for (asCScriptFunction* Function : Functions)
			{
				if (Function != nullptr)
				{
					check(Function->SetJITBinding(asSJITFunctionBinding()) == asSUCCESS);
				}
			}
		}

	private:
		TArray<asCScriptFunction*> Functions;
	};

	static bool IsLifecycleUnchanged(
		const FAngelscriptCacheLifecyclePublications& Before,
		const FAngelscriptCacheLifecyclePublications& After)
	{
		return After.Current == Before.Current
			&& After.PendingColdStart == Before.PendingColdStart
			&& After.LatestSuccessful == Before.LatestSuccessful;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheStaticJITIsolationTests,
	"Angelscript.TestModule.Cache.StaticJITIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ProviderArrivalAndDepartureRepublishOnlyRoutes)
	{
		using namespace AngelscriptCacheStaticJITIsolationTests_Private;
		FScopedProjectRoot Project;
		ASSERT_THAT(IsTrue(Project.WriteSource()));

		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope Scope(*Engine);
		Engine->InitialCompile();
		FAngelscriptCacheService* Service = Engine->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		FScopedSyntheticJitCompiler SyntheticCompiler(*Engine);
		const FAngelscriptCacheLifecyclePublications LifecycleBefore =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(LifecycleBefore.Current.IsValid()));

		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Initial = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Initial.IsValid()));
		const FAngelscriptFunctionRoute* InitialA = FindRoute(
			*Initial, TEXT("int ReadStaticJITIsolationA()"));
		const FAngelscriptFunctionRoute* InitialB = FindRoute(
			*Initial, TEXT("int ReadStaticJITIsolationB()"));
		ASSERT_THAT(IsNotNull(InitialA));
		ASSERT_THAT(IsNotNull(InitialB));
		FScopedSyntheticEntryCleanup SyntheticCleanup;
		SyntheticCleanup.Add(*InitialA);
		SyntheticCleanup.Add(*InitialB);
		ASSERT_THAT(AreEqual(EAngelscriptFunctionExecutionRoute::Vm,
			InitialA->SelectedExecutionRoute));
		ASSERT_THAT(AreEqual(EAngelscriptFunctionExecutionRoute::Vm,
			InitialB->SelectedExecutionRoute));

		SetSyntheticNativeEntries(*InitialA, true);
		ASSERT_THAT(IsTrue(
			Engine->RefreshFunctionRouteSnapshotAfterStaticJITChange(true)));
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Native = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Native.IsValid()));
		ASSERT_THAT(IsTrue(Native != Initial));
		ASSERT_THAT(AreEqual(1u, Native->NativeRouteCount));
		ASSERT_THAT(AreEqual(1u, Native->VmRouteCount));
		const FAngelscriptFunctionRoute* NativeA = FindRoute(
			*Native, TEXT("int ReadStaticJITIsolationA()"));
		ASSERT_THAT(IsNotNull(NativeA));
		ASSERT_THAT(IsTrue(
			NativeA->Identity.FunctionKey == InitialA->Identity.FunctionKey));
		ASSERT_THAT(IsTrue(IsLifecycleUnchanged(
			LifecycleBefore, Service->GetLifecyclePublications())));
		TestRunner->AddInfo(FString::Printf(
			TEXT("[CacheV2][StaticJITIsolation] Outcome=ExactNative RouteOrdinal=%llu VM=%u Native=%u CurrentTx=%llu"),
			static_cast<unsigned long long>(Native->Generation.Value),
			Native->VmRouteCount,
			Native->NativeRouteCount,
			static_cast<unsigned long long>(
				LifecycleBefore.Current->TransactionOrdinal)));

		SetSyntheticNativeEntries(*NativeA, false);
		ASSERT_THAT(IsTrue(
			Engine->RefreshFunctionRouteSnapshotAfterStaticJITChange(true)));
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Removed = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Removed.IsValid()));
		ASSERT_THAT(IsTrue(Removed != Native));
		ASSERT_THAT(AreEqual(0u, Removed->NativeRouteCount));
		ASSERT_THAT(AreEqual(2u, Removed->VmRouteCount));
		ASSERT_THAT(IsTrue(IsLifecycleUnchanged(
			LifecycleBefore, Service->GetLifecyclePublications())));
		TestRunner->AddInfo(FString::Printf(
			TEXT("[CacheV2][StaticJITIsolation] Outcome=ProviderDeparture RouteOrdinal=%llu VM=%u Native=%u CurrentTx=%llu"),
			static_cast<unsigned long long>(Removed->Generation.Value),
			Removed->VmRouteCount,
			Removed->NativeRouteCount,
			static_cast<unsigned long long>(
				LifecycleBefore.Current->TransactionOrdinal)));
	}

	TEST_METHOD(PerFunctionMissOutcomesKeepUnrelatedNativeAndCacheCurrent)
	{
		using namespace AngelscriptCacheStaticJITIsolationTests_Private;
		FScopedProjectRoot Project;
		ASSERT_THAT(IsTrue(Project.WriteSource()));

		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope Scope(*Engine);
		Engine->InitialCompile();
		FAngelscriptCacheService* Service = Engine->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		FScopedSyntheticJitCompiler SyntheticCompiler(*Engine);
		const FAngelscriptCacheLifecyclePublications LifecycleBefore =
			Service->GetLifecyclePublications();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Initial = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Initial.IsValid()));
		const FAngelscriptFunctionRoute* InitialA = FindRoute(
			*Initial, TEXT("int ReadStaticJITIsolationA()"));
		const FAngelscriptFunctionRoute* InitialB = FindRoute(
			*Initial, TEXT("int ReadStaticJITIsolationB()"));
		ASSERT_THAT(IsNotNull(InitialA));
		ASSERT_THAT(IsNotNull(InitialB));
		FScopedSyntheticEntryCleanup SyntheticCleanup;
		SyntheticCleanup.Add(*InitialA);
		SyntheticCleanup.Add(*InitialB);

		const TCHAR* const InjectedMissOutcomes[] =
		{
			TEXT("ProviderAbsent"),
			TEXT("ContentMismatch"),
			TEXT("ProfileMismatch"),
			TEXT("EntryAbiMismatch"),
			TEXT("ProviderGenerationMismatch"),
		};
		for (const TCHAR* InjectedOutcome : InjectedMissOutcomes)
		{
			const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
				ESPMode::ThreadSafe> Before =
				Engine->GetFunctionRouteSnapshot();
			ASSERT_THAT(IsTrue(Before.IsValid()));
			const FAngelscriptFunctionRoute* RouteA = FindRoute(
				*Before, TEXT("int ReadStaticJITIsolationA()"));
			const FAngelscriptFunctionRoute* RouteB = FindRoute(
				*Before, TEXT("int ReadStaticJITIsolationB()"));
			ASSERT_THAT(IsNotNull(RouteA));
			ASSERT_THAT(IsNotNull(RouteB));

			// The sibling matcher has already rejected A for the named reason and
			// accepted B. This Cache test injects that selected result; it does not
			// duplicate provider ABI/catalog matching.
			SetSyntheticNativeEntries(*RouteA, false);
			SetSyntheticNativeEntries(*RouteB, true);
			ASSERT_THAT(IsTrue(
				Engine->RefreshFunctionRouteSnapshotAfterStaticJITChange(true)));

			const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
				ESPMode::ThreadSafe> After =
				Engine->GetFunctionRouteSnapshot();
			ASSERT_THAT(IsTrue(After.IsValid()));
			ASSERT_THAT(IsTrue(After != Before));
			ASSERT_THAT(AreEqual(1u, After->NativeRouteCount));
			ASSERT_THAT(AreEqual(1u, After->VmRouteCount));
			const FAngelscriptFunctionRoute* AfterA = FindRoute(
				*After, TEXT("int ReadStaticJITIsolationA()"));
			const FAngelscriptFunctionRoute* AfterB = FindRoute(
				*After, TEXT("int ReadStaticJITIsolationB()"));
			ASSERT_THAT(IsNotNull(AfterA));
			ASSERT_THAT(IsNotNull(AfterB));
			ASSERT_THAT(AreEqual(
				EAngelscriptFunctionExecutionRoute::Vm,
				AfterA->SelectedExecutionRoute));
			ASSERT_THAT(AreEqual(
				EAngelscriptFunctionExecutionRoute::Native,
				AfterB->SelectedExecutionRoute));
			ASSERT_THAT(IsTrue(IsLifecycleUnchanged(
				LifecycleBefore, Service->GetLifecyclePublications())));
			TestRunner->AddInfo(FString::Printf(
				TEXT("[CacheV2][StaticJITIsolation] Outcome=%s RouteOrdinal=%llu VM=%u Native=%u CurrentTx=%llu"),
				InjectedOutcome,
				static_cast<unsigned long long>(After->Generation.Value),
				After->VmRouteCount,
				After->NativeRouteCount,
				static_cast<unsigned long long>(
					LifecycleBefore.Current->TransactionOrdinal)));
		}

		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Final = Engine->GetFunctionRouteSnapshot();
		const FAngelscriptFunctionRoute* FinalB = FindRoute(
			*Final, TEXT("int ReadStaticJITIsolationB()"));
		ASSERT_THAT(IsNotNull(FinalB));
		SetSyntheticNativeEntries(*FinalB, false);
	}

	TEST_METHOD(FailedLiveCodingOutcomePublishesNothing)
	{
		using namespace AngelscriptCacheStaticJITIsolationTests_Private;
		FScopedProjectRoot Project;
		ASSERT_THAT(IsTrue(Project.WriteSource()));

		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope Scope(*Engine);
		Engine->InitialCompile();
		FAngelscriptCacheService* Service = Engine->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		const FAngelscriptCacheLifecyclePublications LifecycleBefore =
			Service->GetLifecyclePublications();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> RoutesBefore =
			Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(RoutesBefore.IsValid()));

		// Generation/compile/patch/provider validation failed upstream. The seam
		// receives a rejected result and must retain both immutable authorities.
		ASSERT_THAT(IsFalse(
			Engine->RefreshFunctionRouteSnapshotAfterStaticJITChange(false)));
		ASSERT_THAT(IsTrue(
			Engine->GetFunctionRouteSnapshot() == RoutesBefore));
		ASSERT_THAT(IsTrue(IsLifecycleUnchanged(
			LifecycleBefore, Service->GetLifecyclePublications())));
		TestRunner->AddInfo(FString::Printf(
			TEXT("[CacheV2][StaticJITIsolation] Outcome=LiveCodingFailure RouteOrdinal=%llu SnapshotRetained=1 CurrentTx=%llu"),
			static_cast<unsigned long long>(RoutesBefore->Generation.Value),
			static_cast<unsigned long long>(
				LifecycleBefore.Current->TransactionOrdinal)));
	}
};

#endif
