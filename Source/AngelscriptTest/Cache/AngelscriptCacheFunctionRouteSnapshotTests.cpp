#include "Cache/AngelscriptCacheRestore.h"
#include "Cache/AngelscriptCacheDiagnostics.h"
#include "Cache/AngelscriptCacheEnvironment.h"
#include "Cache/AngelscriptCacheService.h"

#include "AngelscriptEngine.h"
#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Shared/AngelscriptTestEngineAcquisition.h"

#include "angelscript.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheFunctionRouteSnapshotTests_Private
{
	class FScopedProjectRoot final
	{
	public:
		FScopedProjectRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheFunctionRoutes"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheFunctionRoutes/")));
			ScriptRoot = Root / TEXT("Script");
			CacheRoot = Root / TEXT("CacheV2");
			check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		}

		~FScopedProjectRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheFunctionRoutes/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		bool WriteSource(const FString& Source) const
		{
			return FFileHelper::SaveStringToFile(
				Source, *(ScriptRoot / TEXT("FunctionRoute.as")));
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

	static bool ExecuteInt(
		FAngelscriptEngine& Engine,
		const FString& Declaration,
		int32& OutValue)
	{
		const TSharedPtr<FAngelscriptModuleDesc> Module =
			Engine.GetModuleByModuleName(TEXT("FunctionRoute"));
		if (!Module.IsValid() || Module->ScriptModule == nullptr)
		{
			return false;
		}
		asIScriptFunction* Function = Module->ScriptModule->GetFunctionByDecl(
			TCHAR_TO_UTF8(*Declaration));
		if (Function == nullptr)
		{
			return false;
		}
		asIScriptContext* Context = Engine.GetScriptEngine()->CreateContext();
		if (Context == nullptr)
		{
			return false;
		}
		const bool bSuccess = Context->Prepare(Function) >= 0
			&& Context->Execute() == asEXECUTION_FINISHED;
		if (bSuccess)
		{
			OutValue = static_cast<int32>(Context->GetReturnDWord());
		}
		Context->Release();
		return bSuccess;
	}

	static bool ConsumeOneFunctionId(FAngelscriptEngine& Engine)
	{
		asIScriptModule* const PaddingModule =
			Engine.GetScriptEngine()->GetModule(
				"NeutralRouteFunctionIdPadding", asGM_ALWAYS_CREATE);
		if (PaddingModule == nullptr)
		{
			return false;
		}
		static constexpr const char* PaddingSource =
			"int ConsumeFunctionId() { return 1; }";
		return PaddingModule->AddScriptSection(
				"NeutralRouteFunctionIdPadding", PaddingSource) >= 0
			&& PaddingModule->Build() >= 0;
	}

	static bool FindEnvironmentFunctionReference(
		FAngelscriptEngine& Engine,
		FAngelscriptArtifactReference& OutReference,
		asCScriptFunction*& OutFunction)
	{
		OutReference = {};
		OutFunction = nullptr;
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.GetScriptEngine());
		for (asUINT Index = 0;
			Index < ScriptEngine->scriptFunctions.GetLength(); ++Index)
		{
			asCScriptFunction* const Function =
				ScriptEngine->scriptFunctions[Index];
			if (Function != nullptr && Function->module == nullptr
				&& FAngelscriptCacheEnvironmentIdentity::
					TryBuildFunctionReference(*Function, OutReference))
			{
				OutFunction = Function;
				return true;
			}
		}
		return false;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptFunctionRouteSnapshotTests,
	"Angelscript.TestModule.Cache.FunctionRouteSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(NormalCompilePublishesStableVmRoute)
	{
		using namespace AngelscriptCacheFunctionRouteSnapshotTests_Private;
		FScopedProjectRoot Project;
		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT(R"AS(
enum EFunctionRouteState
{
	Ready = 1,
}

int ReadFunctionRouteValue()
{
	return 601;
}
)AS"))));

		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope Scope(*Engine);
		Engine->GetCacheService()->ConfigureDecisionTrace(true, 64);
		Engine->InitialCompile();

		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Snapshot = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Snapshot.IsValid()));
		ASSERT_THAT(IsTrue(Snapshot->Generation.IsValid()));
		const FAngelscriptFunctionRoute* Route = FindRoute(
			*Snapshot, TEXT("int ReadFunctionRouteValue()"));
		ASSERT_THAT(IsNotNull(Route));
		ASSERT_THAT(IsFalse(Route->Identity.FunctionKey.Hash.IsZero()));
		ASSERT_THAT(IsTrue(Route->bHasVerifiedArtifactIdentity));
		ASSERT_THAT(IsFalse(Route->Identity.Content.Execution.IsZero()));
		ASSERT_THAT(IsFalse(Route->Identity.Content.Debug.IsZero()));
		ASSERT_THAT(IsFalse(Route->Identity.Profile.Hash.IsZero()));
		ASSERT_THAT(AreEqual(
			EAngelscriptFunctionExecutionRoute::Vm,
			Route->SelectedExecutionRoute));
		ASSERT_THAT(IsNotNull(Route->Function));
		ASSERT_THAT(AreEqual(
			Route->Function->GetId(), Route->NumericFunctionId));
		ASSERT_THAT(IsTrue(Route->Function->GetEngine()
			== Engine->GetScriptEngine()));

		const FAngelscriptCacheDecisionTraceSnapshot Trace =
			Engine->GetCacheService()->CaptureDecisionTrace();
		const FAngelscriptCacheDecisionEvent* RouteEvent =
			Trace.Events.FindByPredicate(
				[&Route](const FAngelscriptCacheDecisionEvent& Event)
				{
					return Event.Stage
							== EAngelscriptCacheDecisionStage::StableRoute
						&& Event.FunctionKey.IsSet()
						&& Event.FunctionKey.GetValue()
							== Route->Identity.FunctionKey;
				});
		ASSERT_THAT(IsNotNull(RouteEvent));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDecisionOutcome::Published,
			RouteEvent->Outcome));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDecisionReasonDomain::StableRoute,
			RouteEvent->ReasonDomain));
		ASSERT_THAT(AreEqual(
			static_cast<uint32>(EAngelscriptFunctionExecutionRoute::Vm),
			RouteEvent->ReasonCode));
		ASSERT_THAT(IsTrue(RouteEvent->CurrentCoordinate.IsSet()));
		ASSERT_THAT(IsTrue(RouteEvent->CurrentCoordinate.GetValue()
			== Route->Identity.Content.Execution));
		ASSERT_THAT(IsTrue(RouteEvent->Profile.Hash
			== Route->Identity.Profile.Hash));
	}

	TEST_METHOD(BodyReloadPublishesNewSnapshotWithStableKey)
	{
		using namespace AngelscriptCacheFunctionRouteSnapshotTests_Private;
		FScopedProjectRoot Project;
		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT(R"AS(
enum EFunctionRouteState
{
	Ready = 1,
}

int ReadFunctionRouteValue()
{
	return 601;
}
)AS"))));

		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope Scope(*Engine);
		Engine->InitialCompile();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Before = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Before.IsValid()));
		const FAngelscriptFunctionRoute* BeforeRoute = FindRoute(
			*Before, TEXT("int ReadFunctionRouteValue()"));
		ASSERT_THAT(IsNotNull(BeforeRoute));
		const FAngelscriptStableFunctionKey StableKey =
			BeforeRoute->Identity.FunctionKey;
		ASSERT_THAT(IsTrue(BeforeRoute->bHasVerifiedArtifactIdentity));
		const FAngelscriptFunctionContentHash BeforeContent =
			BeforeRoute->Identity.Content;
		const FAngelscriptArtifactProfileKey BeforeProfile =
			BeforeRoute->Identity.Profile;
		const int32 BeforeFunctionId = BeforeRoute->NumericFunctionId;

		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT(R"AS(
enum EFunctionRouteState
{
	Ready = 1,
}

int ReadFunctionRouteValue()
{
	return 602;
}
)AS"))));
		const FAngelscriptCacheForceCleanApiResult Reload =
			ForceCleanAngelscriptCache(Engine.Get(), TEXT("FunctionRoute"));
		ASSERT_THAT(IsTrue(Reload.IsSuccess()));

		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> After = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(After.IsValid()));
		ASSERT_THAT(IsTrue(After != Before));
		ASSERT_THAT(IsTrue(
			Before->Generation < After->Generation));
		const FAngelscriptFunctionRoute* AfterRoute = FindRoute(
			*After, TEXT("int ReadFunctionRouteValue()"));
		ASSERT_THAT(IsNotNull(AfterRoute));
		ASSERT_THAT(IsTrue(AfterRoute->Identity.FunctionKey == StableKey));
		ASSERT_THAT(IsTrue(AfterRoute->bHasVerifiedArtifactIdentity));
		ASSERT_THAT(IsFalse(AfterRoute->Identity.Content.Execution
			== BeforeContent.Execution));
		ASSERT_THAT(IsTrue(AfterRoute->Identity.Profile.Hash
			== BeforeProfile.Hash));
		ASSERT_THAT(IsNotNull(AfterRoute->Function));
		ASSERT_THAT(AreEqual(
			AfterRoute->Function->GetId(), AfterRoute->NumericFunctionId));
		ASSERT_THAT(IsTrue(AfterRoute->Function->GetEngine()
			== Engine->GetScriptEngine()));

		// A held old publication remains a readable value object. Its transient
		// function pointer is deliberately not dereferenced after module swap.
		ASSERT_THAT(AreEqual(
			BeforeFunctionId, BeforeRoute->NumericFunctionId));
		ASSERT_THAT(IsTrue(BeforeRoute->Identity.FunctionKey == StableKey));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteInt(
			*Engine, TEXT("int ReadFunctionRouteValue()"), Value)));
		ASSERT_THAT(AreEqual(602, Value));
	}

	TEST_METHOD(FailedReloadKeepsLastGoodSnapshot)
	{
		using namespace AngelscriptCacheFunctionRouteSnapshotTests_Private;
		FScopedProjectRoot Project;
		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT(R"AS(
int ReadFunctionRouteValue()
{
	return 603;
}
)AS"))));

		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope Scope(*Engine);
		Engine->InitialCompile();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Before = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Before.IsValid()));

		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT(R"AS(
int ReadFunctionRouteValue(
)AS"))));
		TestRunner->AddExpectedErrorPlain(
			TEXT("FunctionRoute.as:"),
			EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("Expected data type"),
			EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("Instead found '<end of file>'"),
			EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("Hot reload failed due to script compile errors"),
			EAutomationExpectedErrorFlags::Contains, 1);
		const FAngelscriptCacheForceCleanApiResult Reload =
			ForceCleanAngelscriptCache(Engine.Get(), TEXT("FunctionRoute"));
		ASSERT_THAT(IsFalse(Reload.IsSuccess()));

		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> After = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(After == Before));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(ExecuteInt(
			*Engine, TEXT("int ReadFunctionRouteValue()"), Value)));
		ASSERT_THAT(AreEqual(603, Value));
	}

	TEST_METHOD(TwoEnginesOwnIndependentFunctionIdRoutes)
	{
		using namespace AngelscriptCacheFunctionRouteSnapshotTests_Private;
		FScopedProjectRoot ProjectA;
		FScopedProjectRoot ProjectB;
		const FString Source = TEXT(R"AS(
int ReadFunctionRouteValue()
{
	return 604;
}
)AS");
		ASSERT_THAT(IsTrue(ProjectA.WriteSource(Source)));
		ASSERT_THAT(IsTrue(ProjectB.WriteSource(Source)));

		TUniquePtr<FAngelscriptEngine> EngineA = CreateEngine(ProjectA);
		TUniquePtr<FAngelscriptEngine> EngineB = CreateEngine(ProjectB);
		ASSERT_THAT(IsNotNull(EngineA.Get()));
		ASSERT_THAT(IsNotNull(EngineB.Get()));
		{
			FAngelscriptEngineScope ScopeA(*EngineA);
			EngineA->InitialCompile();
		}
		{
			FAngelscriptEngineScope ScopeB(*EngineB);
			EngineB->InitialCompile();
		}

		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> SnapshotA =
			EngineA->GetFunctionRouteSnapshot();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> SnapshotB =
			EngineB->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(SnapshotA.IsValid()));
		ASSERT_THAT(IsTrue(SnapshotB.IsValid()));
		ASSERT_THAT(IsTrue(SnapshotA != SnapshotB));
		const FAngelscriptFunctionRoute* RouteA = FindRoute(
			*SnapshotA, TEXT("int ReadFunctionRouteValue()"));
		const FAngelscriptFunctionRoute* RouteB = FindRoute(
			*SnapshotB, TEXT("int ReadFunctionRouteValue()"));
		ASSERT_THAT(IsNotNull(RouteA));
		ASSERT_THAT(IsNotNull(RouteB));
		ASSERT_THAT(IsTrue(
			RouteA->Identity.FunctionKey == RouteB->Identity.FunctionKey));
		ASSERT_THAT(IsTrue(RouteA->Function != RouteB->Function));
		ASSERT_THAT(IsTrue(RouteA->Function->GetEngine()
			== EngineA->GetScriptEngine()));
		ASSERT_THAT(IsTrue(RouteB->Function->GetEngine()
			== EngineB->GetScriptEngine()));
		ASSERT_THAT(IsTrue(EngineA->GetScriptEngine()->GetFunctionById(
			RouteA->NumericFunctionId) == RouteA->Function));
		ASSERT_THAT(IsTrue(EngineB->GetScriptEngine()->GetFunctionById(
			RouteB->NumericFunctionId) == RouteB->Function));
	}

	TEST_METHOD(ReorderedFunctionIdsKeepIdentityAndReferenceResolutionEngineLocal)
	{
		using namespace AngelscriptCacheFunctionRouteSnapshotTests_Private;
		FScopedProjectRoot ProjectA;
		FScopedProjectRoot ProjectB;
		const FString Source = TEXT(R"AS(
int ReadFunctionRouteValue()
{
	return 605;
}
)AS");
		ASSERT_THAT(IsTrue(ProjectA.WriteSource(Source)));
		ASSERT_THAT(IsTrue(ProjectB.WriteSource(Source)));

		TUniquePtr<FAngelscriptEngine> EngineA = CreateEngine(ProjectA);
		TUniquePtr<FAngelscriptEngine> EngineB = CreateEngine(ProjectB);
		ASSERT_THAT(IsNotNull(EngineA.Get()));
		ASSERT_THAT(IsNotNull(EngineB.Get()));
		ASSERT_THAT(IsTrue(ConsumeOneFunctionId(*EngineB),
			TEXT("The second Engine should consume one transient FunctionId first")));
		{
			FAngelscriptEngineScope ScopeA(*EngineA);
			EngineA->InitialCompile();
		}
		{
			FAngelscriptEngineScope ScopeB(*EngineB);
			EngineB->InitialCompile();
		}

		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> SnapshotA = EngineA->GetFunctionRouteSnapshot();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> SnapshotB = EngineB->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(SnapshotA.IsValid()));
		ASSERT_THAT(IsTrue(SnapshotB.IsValid()));
		const FAngelscriptFunctionRoute* const RouteA = FindRoute(
			*SnapshotA, TEXT("int ReadFunctionRouteValue()"));
		const FAngelscriptFunctionRoute* const RouteB = FindRoute(
			*SnapshotB, TEXT("int ReadFunctionRouteValue()"));
		ASSERT_THAT(IsNotNull(RouteA));
		ASSERT_THAT(IsNotNull(RouteB));
		ASSERT_THAT(IsTrue(
			RouteA->Identity.FunctionKey == RouteB->Identity.FunctionKey));
		ASSERT_THAT(IsTrue(
			RouteA->Identity.Content.Execution
				== RouteB->Identity.Content.Execution));
		ASSERT_THAT(IsTrue(
			RouteA->Identity.Profile.Hash == RouteB->Identity.Profile.Hash));
		ASSERT_THAT(IsTrue(
			RouteA->NumericFunctionId != RouteB->NumericFunctionId,
			TEXT("Stable identity should survive intentionally reordered FunctionIds")));
		ASSERT_THAT(IsTrue(RouteA->Function != RouteB->Function));
		ASSERT_THAT(IsTrue(EngineA->GetScriptEngine()->GetFunctionById(
			RouteA->NumericFunctionId) == RouteA->Function));
		ASSERT_THAT(IsTrue(EngineB->GetScriptEngine()->GetFunctionById(
			RouteB->NumericFunctionId) == RouteB->Function));

		FAngelscriptArtifactReference EnvironmentReference;
		asCScriptFunction* SourceEnvironmentFunction = nullptr;
		ASSERT_THAT(IsTrue(FindEnvironmentFunctionReference(
			*EngineA, EnvironmentReference, SourceEnvironmentFunction)));
		FAngelscriptCacheEngineEnvironmentResolver ResolverA(
			*static_cast<asCScriptEngine*>(EngineA->GetScriptEngine()));
		FAngelscriptCacheEngineEnvironmentResolver ResolverB(
			*static_cast<asCScriptEngine*>(EngineB->GetScriptEngine()));
		asCScriptFunction* const ResolvedA =
			ResolverA.ResolveFunctionReference(EnvironmentReference);
		asCScriptFunction* const ResolvedB =
			ResolverB.ResolveFunctionReference(EnvironmentReference);
		ASSERT_THAT(IsNotNull(ResolvedA));
		ASSERT_THAT(IsNotNull(ResolvedB));
		ASSERT_THAT(IsTrue(ResolvedA == SourceEnvironmentFunction));
		ASSERT_THAT(IsTrue(ResolvedA != ResolvedB,
			TEXT("One stable reference descriptor should resolve to Engine-local objects")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
