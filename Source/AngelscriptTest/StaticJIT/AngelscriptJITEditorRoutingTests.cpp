#include "Cache/AngelscriptCacheDiagnostics.h"

#include "AngelscriptEngine.h"
#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Editor/AngelscriptPIETestUtils.h"
#include "Shared/AngelscriptTestEngineAcquisition.h"
#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestModuleBuilder.h"
#include "StaticJIT/AngelscriptJITGeneration.h"
#include "StaticJIT/AngelscriptJITProviderRegistry.h"
#include "StaticJIT/AngelscriptJITProviderRouter.h"
#include "StaticJIT/AngelscriptStaticJIT.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptJITEditorRoutingTests_Private
{
	class FScopedProjectRoot final
	{
	public:
		FScopedProjectRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptJITEditorRouting"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptJITEditorRouting/")));
			ScriptRoot = Root / TEXT("Script");
			CacheRoot = Root / TEXT("CacheV2");
			check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		}

		~FScopedProjectRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptJITEditorRouting/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		bool WriteSource(const FString& Source) const
		{
			return WriteSource(TEXT("EditorRouting.as"), Source);
		}

		bool WriteSource(
			const FString& RelativePath,
			const FString& Source) const
		{
			return FFileHelper::SaveStringToFile(
				Source, *(ScriptRoot / RelativePath));
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
			[&CanonicalDeclaration](const FAngelscriptFunctionRoute& Route)
			{
				return Route.CanonicalDeclaration == CanonicalDeclaration;
			});
	}

	static const FAngelscriptFunctionRoute* FindRouteByName(
		const FAngelscriptFunctionRouteSnapshot& Snapshot,
		const ANSICHAR* FunctionName)
	{
		return Snapshot.FunctionRoutes.FindByPredicate(
			[FunctionName](const FAngelscriptFunctionRoute& Route)
			{
				return Route.Function != nullptr
					&& FCStringAnsi::Strcmp(
						Route.Function->GetName(), FunctionName) == 0;
			});
	}

	static FAngelscriptHash256 MakeHash(const FStringView Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("jit-editor-routing-test"));
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	static void ProviderVmEntry(
		FScriptExecution&,
		asDWORD*,
		asQWORD* OutValue)
	{
		if (OutValue != nullptr)
		{
			*OutValue = 700;
		}
	}

	static asDWORD ProviderRawEntry(FScriptExecution&, void*)
	{
		return 700;
	}

	static void ProviderParmsEntry(FScriptExecution&, void*, void*)
	{
	}

	class FRouteProvider final : public IAngelscriptJITArtifactProvider
	{
	public:
		explicit FRouteProvider(
			const FAngelscriptFunctionRouteSnapshot& Routes,
			const TCHAR* ProviderSeed = TEXT("provider"))
		{
			View.ProviderId.Hash = MakeHash(ProviderSeed);
			View.ProviderName = TEXT("EditorRoutingProvider");
			View.OwnerModuleName = TEXT("AngelscriptTest");
			View.NativeEnvironmentFingerprint =
				FAngelscriptJITTargetProfiles::
					BuildCurrentNativeEnvironmentFingerprint(
						EAngelscriptJITTargetProfile::EditorDevelopment);

			for (const FAngelscriptFunctionRoute& Route : Routes.FunctionRoutes)
			{
				if (!Route.bHasVerifiedArtifactIdentity)
				{
					continue;
				}
				FAngelscriptJITArtifactEntry& Entry = Entries.AddDefaulted_GetRef();
				Entry.ModuleKey = Route.ModuleKey;
				Entry.FunctionKey = Route.Identity.FunctionKey;
				Entry.ExecutionHash = Route.Identity.Content.Execution;
				Entry.DebugHash = Route.Identity.Content.Debug;
				Entry.ArtifactProfile = Route.Identity.Profile;
				Entry.NativeEnvironmentFingerprint =
					View.NativeEnvironmentFingerprint;
				const bool bHasParmsEntry =
					FAngelscriptStaticJIT::CanGenerateParmsEntry(Route.Function);
				Entry.EntryAbiHash = FAngelscriptJITGeneration::BuildEntryAbiHash(
					Route.CanonicalDeclaration,
					true,
					true,
					bHasParmsEntry);
				Entry.EntryPoints.VMEntry = &ProviderVmEntry;
				Entry.EntryPoints.RawEntry =
					reinterpret_cast<asJITFunction_Raw>(&ProviderRawEntry);
				Entry.EntryPoints.ParmsEntry = bHasParmsEntry
					? &ProviderParmsEntry : nullptr;
			}
			Entries.Sort([](
				const FAngelscriptJITArtifactEntry& Left,
				const FAngelscriptJITArtifactEntry& Right)
			{
				if (!(Left.ModuleKey == Right.ModuleKey))
				{
					return Left.ModuleKey.Hash < Right.ModuleKey.Hash;
				}
				return Left.FunctionKey.Hash < Right.FunctionKey.Hash;
			});
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

#if WITH_EDITOR
	struct FPIERouteState final
	{
		TUniquePtr<FScopedProjectRoot> Project;
		TUniquePtr<FAngelscriptEngine> Engine;
		TUniquePtr<FRouteProvider> Provider;
		TStrongObjectPtr<UObject> PIEObject;
		FRequestPlaySessionParams RequestParams;
		UWorld* EditorWorld = nullptr;
		UClass* ScriptClass = nullptr;
		bool bProviderRegistered = false;
	};
#endif
}

TEST_CLASS_WITH_FLAGS(FAngelscriptJITEditorRoutingTests,
	"Angelscript.TestModule.StaticJIT.EditorRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(BodyReloadFallsBackOnlyChangedFunctionAndRebindsUnchangedFunction)
	{
		using namespace AngelscriptJITEditorRoutingTests_Private;
		FScopedProjectRoot Project;
		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT(R"AS(
int ChangedValue()
{
	return 10;
}

int UnchangedValue()
{
	return 20;
}
)AS"))));

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

		FRouteProvider Provider(*InitialRoutes);
		FAngelscriptJITProviderRegistry& Registry =
			FAngelscriptJITProviderRegistry::Get();
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&Provider, Provider).IsAccepted()));
		ON_SCOPE_EXIT
		{
			Registry.UnregisterOwner(&Provider);
		};
		const FAngelscriptJITProviderRouteRefreshResult InitialRefresh =
			FAngelscriptJITProviderRouter::Refresh(*Engine);
		ASSERT_THAT(IsTrue(InitialRefresh.IsApplied()));
		ASSERT_THAT(AreEqual(2u, InitialRefresh.PublishedNativeCount));

		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT(R"AS(
int ChangedValue()
{
	return 11;
}

int UnchangedValue()
{
	return 20;
}
)AS"))));
		const FAngelscriptCacheForceCleanApiResult Reload =
			ForceCleanAngelscriptCache(Engine.Get(), TEXT("EditorRouting"));
		ASSERT_THAT(IsTrue(Reload.IsSuccess()));

		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> ReloadedRoutes =
			Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(ReloadedRoutes.IsValid()));
		if (!ReloadedRoutes.IsValid())
		{
			return;
		}
		const FAngelscriptFunctionRoute* const Changed = FindRoute(
			*ReloadedRoutes, TEXT("int ChangedValue()"));
		const FAngelscriptFunctionRoute* const Unchanged = FindRoute(
			*ReloadedRoutes, TEXT("int UnchangedValue()"));
		ASSERT_THAT(IsNotNull(Changed));
		ASSERT_THAT(IsNotNull(Unchanged));
		if (Changed == nullptr || Unchanged == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			EAngelscriptFunctionExecutionRoute::Vm,
			Changed->SelectedExecutionRoute));
		ASSERT_THAT(IsNull(Changed->Function->GetJITBinding().VMEntry));
		ASSERT_THAT(IsNull(Changed->Function->GetJITBinding().RawEntry));
		ASSERT_THAT(IsNull(Changed->Function->GetJITBinding().ParmsEntry));
		ASSERT_THAT(AreEqual(
			EAngelscriptFunctionExecutionRoute::Native,
			Unchanged->SelectedExecutionRoute));
		ASSERT_THAT(IsTrue(Unchanged->Function->GetJITBinding().VMEntry
			== &ProviderVmEntry));
		ASSERT_THAT(IsTrue(Unchanged->Function->GetJITBinding().RawEntry
			== reinterpret_cast<asJITFunction_Raw>(&ProviderRawEntry)));
		ASSERT_THAT(IsTrue(Unchanged->Function->GetJITBinding().ParmsEntry
			== &ProviderParmsEntry));
	}

	TEST_METHOD(DebugOnlyReloadPreservesExactNativeBindings)
	{
		using namespace AngelscriptJITEditorRoutingTests_Private;
		FScopedProjectRoot Project;
		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT(R"AS(
int FirstValue()
{
	return 31;
}

int SecondValue()
{
	return 32;
}
)AS"))));

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
		const FAngelscriptFunctionRoute* const InitialFirst =
			FindRouteByName(*InitialRoutes, "FirstValue");
		ASSERT_THAT(IsNotNull(InitialFirst));
		if (InitialFirst == nullptr)
		{
			return;
		}
		const FAngelscriptFunctionArtifactIdentity InitialIdentity =
			InitialFirst->Identity;

		FRouteProvider Provider(*InitialRoutes);
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

		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT(R"AS(
// Move the debug sites without changing executable bytecode.


int FirstValue()
{
	return 31;
}

int SecondValue()
{
	return 32;
}
)AS"))));
		const FAngelscriptCacheForceCleanApiResult Reload =
			ForceCleanAngelscriptCache(Engine.Get(), TEXT("EditorRouting"));
		ASSERT_THAT(IsTrue(Reload.IsSuccess()));

		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> ReloadedRoutes =
			Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(ReloadedRoutes.IsValid()));
		if (!ReloadedRoutes.IsValid())
		{
			return;
		}
		const FAngelscriptFunctionRoute* const ReloadedFirst =
			FindRouteByName(*ReloadedRoutes, "FirstValue");
		const FAngelscriptFunctionRoute* const ReloadedSecond =
			FindRouteByName(*ReloadedRoutes, "SecondValue");
		ASSERT_THAT(IsNotNull(ReloadedFirst));
		ASSERT_THAT(IsNotNull(ReloadedSecond));
		if (ReloadedFirst == nullptr || ReloadedSecond == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ReloadedFirst->Identity.FunctionKey
			== InitialIdentity.FunctionKey));
		ASSERT_THAT(IsTrue(ReloadedFirst->Identity.Content.Execution
			== InitialIdentity.Content.Execution));
		ASSERT_THAT(IsFalse(ReloadedFirst->Identity.Content.Debug
			== InitialIdentity.Content.Debug));
		ASSERT_THAT(AreEqual(
			EAngelscriptFunctionExecutionRoute::Native,
			ReloadedFirst->SelectedExecutionRoute));
		ASSERT_THAT(AreEqual(
			EAngelscriptFunctionExecutionRoute::Native,
			ReloadedSecond->SelectedExecutionRoute));
		ASSERT_THAT(IsTrue(ReloadedFirst->Function->GetJITBinding().VMEntry
			== &ProviderVmEntry));
		ASSERT_THAT(IsTrue(ReloadedSecond->Function->GetJITBinding().VMEntry
			== &ProviderVmEntry));
	}

	TEST_METHOD(FailedReloadKeepsLastGoodNativePublication)
	{
		using namespace AngelscriptJITEditorRoutingTests_Private;
		FScopedProjectRoot Project;
		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT(R"AS(
int LastGoodValue()
{
	return 41;
}
)AS"))));

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

		FRouteProvider Provider(*InitialRoutes);
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
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> LastGoodRoutes =
			Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(LastGoodRoutes.IsValid()));
		const FAngelscriptFunctionRoute* const LastGood =
			LastGoodRoutes.IsValid()
				? FindRouteByName(*LastGoodRoutes, "LastGoodValue") : nullptr;
		ASSERT_THAT(IsNotNull(LastGood));
		if (LastGood == nullptr)
		{
			return;
		}
		asIScriptFunction* const LastGoodFunction = LastGood->Function;
		ASSERT_THAT(IsTrue(LastGoodFunction->GetJITBinding().VMEntry
			== &ProviderVmEntry));

		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT(R"AS(
int LastGoodValue(
)AS"))));
		TestRunner->AddExpectedErrorPlain(
			TEXT("EditorRouting.as:"),
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
			ForceCleanAngelscriptCache(Engine.Get(), TEXT("EditorRouting"));
		ASSERT_THAT(IsFalse(Reload.IsSuccess()));
		ASSERT_THAT(IsTrue(Engine->GetFunctionRouteSnapshot()
			== LastGoodRoutes));
		ASSERT_THAT(IsTrue(LastGoodFunction->GetJITBinding().VMEntry
			== &ProviderVmEntry));
	}

	TEST_METHOD(SignatureChangeAndDeletionDoNotRetainOldProviderEntries)
	{
		using namespace AngelscriptJITEditorRoutingTests_Private;
		FScopedProjectRoot Project;
		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT(R"AS(
int ChangedSignature()
{
	return 51;
}

int DeletedValue()
{
	return 52;
}

int StableValue()
{
	return 53;
}
)AS"))));

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
		const FAngelscriptFunctionRoute* const InitialChanged =
			FindRouteByName(*InitialRoutes, "ChangedSignature");
		const FAngelscriptFunctionRoute* const InitialDeleted =
			FindRouteByName(*InitialRoutes, "DeletedValue");
		ASSERT_THAT(IsNotNull(InitialChanged));
		ASSERT_THAT(IsNotNull(InitialDeleted));
		if (InitialChanged == nullptr || InitialDeleted == nullptr)
		{
			return;
		}
		const FAngelscriptStableFunctionKey OldChangedKey =
			InitialChanged->Identity.FunctionKey;
		const FAngelscriptStableFunctionKey DeletedKey =
			InitialDeleted->Identity.FunctionKey;

		FRouteProvider Provider(*InitialRoutes);
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

		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT(R"AS(
int ChangedSignature(int Input)
{
	return Input + 51;
}

int StableValue()
{
	return 53;
}
)AS"))));
		const FAngelscriptCacheForceCleanApiResult Reload =
			ForceCleanAngelscriptCache(Engine.Get(), TEXT("EditorRouting"));
		ASSERT_THAT(IsTrue(Reload.IsSuccess()));

		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> ReloadedRoutes =
			Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(ReloadedRoutes.IsValid()));
		if (!ReloadedRoutes.IsValid())
		{
			return;
		}
		const FAngelscriptFunctionRoute* const Changed =
			FindRouteByName(*ReloadedRoutes, "ChangedSignature");
		const FAngelscriptFunctionRoute* const Stable =
			FindRouteByName(*ReloadedRoutes, "StableValue");
		ASSERT_THAT(IsNotNull(Changed));
		ASSERT_THAT(IsNotNull(Stable));
		ASSERT_THAT(IsNull(FindRouteByName(
			*ReloadedRoutes, "DeletedValue")));
		ASSERT_THAT(IsFalse(ReloadedRoutes->FunctionRoutes.ContainsByPredicate(
			[&DeletedKey](const FAngelscriptFunctionRoute& Route)
			{
				return Route.Identity.FunctionKey == DeletedKey;
			})));
		if (Changed == nullptr || Stable == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsFalse(Changed->Identity.FunctionKey == OldChangedKey));
		ASSERT_THAT(AreEqual(
			EAngelscriptFunctionExecutionRoute::Vm,
			Changed->SelectedExecutionRoute));
		ASSERT_THAT(IsNull(Changed->Function->GetJITBinding().VMEntry));
		ASSERT_THAT(AreEqual(
			EAngelscriptFunctionExecutionRoute::Native,
			Stable->SelectedExecutionRoute));
		ASSERT_THAT(IsTrue(Stable->Function->GetJITBinding().VMEntry
			== &ProviderVmEntry));
	}

	TEST_METHOD(StructuralReloadPublishesRoutesBeforePostCompileConsumers)
	{
		using namespace AngelscriptJITEditorRoutingTests_Private;
		FScopedProjectRoot Project;
		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT(R"AS(
UCLASS(meta=(DisplayName="Initial Base"))
class UEditorRoutingStructuralBase : UObject
{
	UPROPERTY()
	int BaseValue;

	UFUNCTION(meta=(DisplayName="Initial Contract"))
	int ContractValue() final
	{
		return 61;
	}
}

UCLASS()
class UEditorRoutingStructuralChild : UEditorRoutingStructuralBase
{
	UFUNCTION()
	int ChildValue() final
	{
		return 62;
	}
}

int StableStructuralGlobal()
{
	return 63;
}
)AS"))));

		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		if (!Engine.IsValid())
		{
			return;
		}
		FAngelscriptEngineScope Scope(*Engine);
		Engine->InitialCompile();
		UClass* const InitialBase = FindGeneratedClass(
			Engine.Get(), TEXT("UEditorRoutingStructuralBase"));
		UClass* const InitialChild = FindGeneratedClass(
			Engine.Get(), TEXT("UEditorRoutingStructuralChild"));
		ASSERT_THAT(IsNotNull(InitialBase));
		ASSERT_THAT(IsNotNull(InitialChild));

		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> InitialRoutes =
			Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(InitialRoutes.IsValid()));
		if (!InitialRoutes.IsValid())
		{
			return;
		}
		FRouteProvider Provider(*InitialRoutes, TEXT("structural-provider"));
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

		bool bPostCompileObserved = false;
		bool bPostCompileObservedNewLayout = false;
		bool bPostCompileObservedChangedVm = false;
		bool bPostCompileObservedStableNative = false;
		const FDelegateHandle PostCompileHandle =
			Engine->GetPostCompileClassCollection().AddLambda(
				[&](const TArray<TSharedRef<FAngelscriptModuleDesc>>&)
				{
					bPostCompileObserved = true;
					UClass* const CurrentBase = FindGeneratedClass(
						Engine.Get(), TEXT("UEditorRoutingStructuralBase"));
					bPostCompileObservedNewLayout = CurrentBase != nullptr
						&& FindFProperty<FIntProperty>(
							CurrentBase, TEXT("AddedValue")) != nullptr;
					const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
						ESPMode::ThreadSafe> Routes =
						Engine->GetFunctionRouteSnapshot();
					if (Routes.IsValid())
					{
						const FAngelscriptFunctionRoute* const Changed =
							FindRouteByName(*Routes, "ContractValue");
						const FAngelscriptFunctionRoute* const Stable =
							FindRouteByName(*Routes, "StableStructuralGlobal");
						bPostCompileObservedChangedVm = Changed != nullptr
							&& Changed->SelectedExecutionRoute
								== EAngelscriptFunctionExecutionRoute::Vm
							&& Changed->Function->GetJITBinding().VMEntry == nullptr;
						bPostCompileObservedStableNative = Stable != nullptr
							&& Stable->SelectedExecutionRoute
								== EAngelscriptFunctionExecutionRoute::Native
							&& Stable->Function->GetJITBinding().VMEntry
								== &ProviderVmEntry;
					}
				});
		ON_SCOPE_EXIT
		{
			Engine->GetPostCompileClassCollection().Remove(PostCompileHandle);
		};

		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT(R"AS(
UCLASS(meta=(DisplayName="Reloaded Base"))
class UEditorRoutingStructuralBase : UObject
{
	UPROPERTY()
	int AddedValue;

	UPROPERTY()
	int BaseValue;

	UFUNCTION(BlueprintCallable, meta=(DisplayName="Reloaded Contract"))
	int ContractValue() final
	{
		return 64;
	}
}

UCLASS()
class UEditorRoutingStructuralChild : UEditorRoutingStructuralBase
{
	UFUNCTION()
	int ChildValue() final
	{
		return 62;
	}
}

int StableStructuralGlobal()
{
	return 63;
}
)AS"))));
		const FAngelscriptCacheForceCleanApiResult Reload =
			ForceCleanAngelscriptCache(Engine.Get(), TEXT("EditorRouting"));
		ASSERT_THAT(IsTrue(Reload.IsSuccess()));

		UClass* const ReloadedBase = FindGeneratedClass(
			Engine.Get(), TEXT("UEditorRoutingStructuralBase"));
		UClass* const ReloadedChild = FindGeneratedClass(
			Engine.Get(), TEXT("UEditorRoutingStructuralChild"));
		ASSERT_THAT(IsNotNull(ReloadedBase));
		ASSERT_THAT(IsNotNull(ReloadedChild));
		ASSERT_THAT(AreNotEqual(InitialBase, ReloadedBase));
		ASSERT_THAT(AreNotEqual(InitialChild, ReloadedChild));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(
			ReloadedBase, TEXT("AddedValue"))));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(
			ReloadedChild, TEXT("AddedValue"))));
		UFunction* const ReloadedContract = FindGeneratedFunction(
			ReloadedBase, TEXT("ContractValue"));
		ASSERT_THAT(IsNotNull(ReloadedContract));
		if (ReloadedContract != nullptr)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("Reloaded Contract")),
				ReloadedContract->GetMetaData(TEXT("DisplayName"))));
			ASSERT_THAT(IsTrue(ReloadedContract->HasAnyFunctionFlags(
				FUNC_BlueprintCallable)));
		}
		ASSERT_THAT(IsTrue(bPostCompileObserved));
		ASSERT_THAT(IsTrue(bPostCompileObservedNewLayout));
		ASSERT_THAT(IsTrue(bPostCompileObservedChangedVm));
		ASSERT_THAT(IsTrue(bPostCompileObservedStableNative));
	}

	TEST_METHOD(AutomaticImportReloadKeepsExactConsumerNative)
	{
		using namespace AngelscriptJITEditorRoutingTests_Private;
		FScopedProjectRoot Project;
		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("EditorRoutingImportProvider.as"), TEXT(R"AS(
int ImportedValue()
{
	return 71;
}
)AS"))));
		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("EditorRoutingImportConsumer.as"), TEXT(R"AS(
int ImportEntry()
{
	return ImportedValue();
}
)AS"))));

		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		if (!Engine.IsValid())
		{
			return;
		}
		FAngelscriptEngineScope Scope(*Engine);
		ASSERT_THAT(IsTrue(Engine->bUseAutomaticImportMethod));
		Engine->InitialCompile();
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> InitialRoutes =
			Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(InitialRoutes.IsValid()));
		if (!InitialRoutes.IsValid())
		{
			return;
		}
		const FAngelscriptFunctionRoute* const InitialImported =
			FindRouteByName(*InitialRoutes, "ImportedValue");
		const FAngelscriptFunctionRoute* const InitialConsumer =
			FindRouteByName(*InitialRoutes, "ImportEntry");
		ASSERT_THAT(IsNotNull(InitialImported));
		ASSERT_THAT(IsNotNull(InitialConsumer));
		if (InitialImported == nullptr || InitialConsumer == nullptr)
		{
			return;
		}
		const FAngelscriptFunctionArtifactIdentity InitialImportedIdentity =
			InitialImported->Identity;
		const FAngelscriptFunctionArtifactIdentity InitialConsumerIdentity =
			InitialConsumer->Identity;
		FRouteProvider Provider(*InitialRoutes, TEXT("import-provider"));
		FAngelscriptJITProviderRegistry& Registry =
			FAngelscriptJITProviderRegistry::Get();
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&Provider, Provider).IsAccepted()));
		ON_SCOPE_EXIT
		{
			Registry.UnregisterOwner(&Provider);
		};
		const FAngelscriptJITProviderRouteRefreshResult InitialRefresh =
			FAngelscriptJITProviderRouter::Refresh(*Engine);
		ASSERT_THAT(IsTrue(InitialRefresh.IsApplied()));
		ASSERT_THAT(AreEqual(2u, InitialRefresh.PublishedNativeCount));

		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("EditorRoutingImportProvider.as"), TEXT(R"AS(
int ImportedValue()
{
	return 72;
}
)AS"))));
		const FAngelscriptCacheForceCleanApiResult Reload =
			ForceCleanAngelscriptCache(
				Engine.Get(), TEXT("EditorRoutingImportProvider"));
		ASSERT_THAT(IsTrue(Reload.IsSuccess()));

		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> ReloadedRoutes =
			Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(ReloadedRoutes.IsValid()));
		if (!ReloadedRoutes.IsValid())
		{
			return;
		}
		const FAngelscriptFunctionRoute* const Imported =
			FindRouteByName(*ReloadedRoutes, "ImportedValue");
		const FAngelscriptFunctionRoute* const Consumer =
			FindRouteByName(*ReloadedRoutes, "ImportEntry");
		ASSERT_THAT(IsNotNull(Imported));
		ASSERT_THAT(IsNotNull(Consumer));
		if (Imported == nullptr || Consumer == nullptr)
		{
			return;
		}
		const FAngelscriptJITProviderRouteRefreshResult DiagnosticRefresh =
			FAngelscriptJITProviderRouter::Refresh(*Engine);
		const FAngelscriptJITProviderRouteRecord* const ImportedRecord =
			DiagnosticRefresh.Routes.FindByPredicate(
				[&InitialImportedIdentity](
					const FAngelscriptJITProviderRouteRecord& Record)
				{
					return Record.FunctionKey
						== InitialImportedIdentity.FunctionKey;
				});
		const FAngelscriptJITProviderRouteRecord* const ConsumerRecord =
			DiagnosticRefresh.Routes.FindByPredicate(
				[&InitialConsumerIdentity](
					const FAngelscriptJITProviderRouteRecord& Record)
				{
					return Record.FunctionKey
						== InitialConsumerIdentity.FunctionKey;
				});
		TestRunner->AddInfo(FString::Printf(
			TEXT("Automatic-import route refresh: %s; imported key=%d execution=%d debug=%d match=%d; consumer key=%d execution=%d debug=%d match=%d"),
			*DiagnosticRefresh.Detail,
			Imported->Identity.FunctionKey == InitialImportedIdentity.FunctionKey,
			Imported->Identity.Content.Execution
				== InitialImportedIdentity.Content.Execution,
			Imported->Identity.Content.Debug
				== InitialImportedIdentity.Content.Debug,
			ImportedRecord != nullptr
				? static_cast<int32>(ImportedRecord->MatchResult) : -1,
			Consumer->Identity.FunctionKey == InitialConsumerIdentity.FunctionKey,
			Consumer->Identity.Content.Execution
				== InitialConsumerIdentity.Content.Execution,
			Consumer->Identity.Content.Debug
				== InitialConsumerIdentity.Content.Debug,
			ConsumerRecord != nullptr
				? static_cast<int32>(ConsumerRecord->MatchResult) : -1));
		ASSERT_THAT(IsNotNull(ImportedRecord));
		ASSERT_THAT(IsNotNull(ConsumerRecord));
		if (ImportedRecord != nullptr)
		{
			ASSERT_THAT(AreEqual(
				EAngelscriptArtifactMatchResult::ContentMismatch,
				ImportedRecord->MatchResult));
		}
		if (ConsumerRecord != nullptr)
		{
			ASSERT_THAT(AreEqual(
				EAngelscriptArtifactMatchResult::Exact,
				ConsumerRecord->MatchResult));
		}
		ASSERT_THAT(IsTrue(
			Imported->Identity.FunctionKey
				== InitialImportedIdentity.FunctionKey));
		ASSERT_THAT(IsFalse(
			Imported->Identity.Content.Execution
				== InitialImportedIdentity.Content.Execution));
		ASSERT_THAT(IsTrue(
			Consumer->Identity.FunctionKey
				== InitialConsumerIdentity.FunctionKey));
		ASSERT_THAT(IsTrue(
			Consumer->Identity.Content.Execution
				== InitialConsumerIdentity.Content.Execution));
		ASSERT_THAT(IsTrue(
			Consumer->Identity.Content.Debug
				== InitialConsumerIdentity.Content.Debug));
		ASSERT_THAT(AreEqual(
			EAngelscriptFunctionExecutionRoute::Vm,
			Imported->SelectedExecutionRoute));
		ASSERT_THAT(IsNull(Imported->Function->GetJITBinding().VMEntry));
		ASSERT_THAT(AreEqual(
			EAngelscriptFunctionExecutionRoute::Native,
			Consumer->SelectedExecutionRoute));
		ASSERT_THAT(IsTrue(Consumer->Function->GetJITBinding().VMEntry
			== &ProviderVmEntry));

		const TSharedPtr<FAngelscriptModuleDesc> ConsumerModule =
			Engine->GetModule(TEXT("EditorRoutingImportConsumer"));
		ASSERT_THAT(IsTrue(ConsumerModule.IsValid()));
		if (ConsumerModule.IsValid())
		{
			ASSERT_THAT(AreEqual(
				0,
				static_cast<int32>(ConsumerModule->ScriptModule->
					GetImportedFunctionCount())));
		}
	}

	TEST_METHOD(AmbiguousExactEditorProvidersFallBackWithTypedReason)
	{
		using namespace AngelscriptJITEditorRoutingTests_Private;
		FScopedProjectRoot Project;
		ASSERT_THAT(IsTrue(Project.WriteSource(TEXT(R"AS(
int AmbiguousValue()
{
	return 81;
}
)AS"))));
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
		FRouteProvider First(*Routes, TEXT("ambiguous-provider-a"));
		FRouteProvider Second(*Routes, TEXT("ambiguous-provider-b"));
		FAngelscriptJITProviderRegistry& Registry =
			FAngelscriptJITProviderRegistry::Get();
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&First, First).IsAccepted()));
		ON_SCOPE_EXIT
		{
			Registry.UnregisterOwner(&First);
		};
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&Second, Second).IsAccepted()));
		ON_SCOPE_EXIT
		{
			Registry.UnregisterOwner(&Second);
		};

		const FAngelscriptJITProviderRouteRefreshResult Refresh =
			FAngelscriptJITProviderRouter::Refresh(*Engine);
		ASSERT_THAT(IsTrue(Refresh.IsApplied()));
		ASSERT_THAT(AreEqual(0u, Refresh.PublishedNativeCount));
		ASSERT_THAT(AreEqual(1u, Refresh.VmFallbackCount));
		ASSERT_THAT(AreEqual(1, Refresh.Routes.Num()));
		if (Refresh.Routes.Num() == 1)
		{
			ASSERT_THAT(AreEqual(
				EAngelscriptArtifactMatchResult::AmbiguousExactProvider,
				Refresh.Routes[0].MatchResult));
			ASSERT_THAT(IsFalse(Refresh.Routes[0].bBindingPublished));
		}
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> PublishedRoutes =
			Engine->GetFunctionRouteSnapshot();
		const FAngelscriptFunctionRoute* const Function =
			PublishedRoutes.IsValid()
				? FindRouteByName(*PublishedRoutes, "AmbiguousValue") : nullptr;
		ASSERT_THAT(IsNotNull(Function));
		if (Function != nullptr)
		{
			ASSERT_THAT(AreEqual(
				EAngelscriptFunctionExecutionRoute::Vm,
				Function->SelectedExecutionRoute));
			ASSERT_THAT(IsNull(Function->Function->GetJITBinding().VMEntry));
		}
	}

#if WITH_EDITOR
	TEST_METHOD(PIEExecutesTheEditorEnginesCurrentNativeBinding)
	{
		using namespace AngelscriptJITEditorRoutingTests_Private;
		ASSERT_THAT(IsFalse(AngelscriptPIETestUtils::IsPIEWorldAlive()));
		const TSharedRef<FPIERouteState> State = MakeShared<FPIERouteState>();
		State->Project = MakeUnique<FScopedProjectRoot>();
		TestCommandBuilder.CleanUpWith(
			TEXT("Release StaticJIT PIE routing fixture"), [State]()
			{
				State->PIEObject.Reset();
				AngelscriptPIETestUtils::EndPIE();
				if (State->bProviderRegistered && State->Provider.IsValid())
				{
					FAngelscriptJITProviderRegistry::Get().UnregisterOwner(
						State->Provider.Get());
					State->bProviderRegistered = false;
				}
				State->Engine.Reset();
				State->Provider.Reset();
				State->Project.Reset();
			});

		ASSERT_THAT(IsTrue(State->Project->WriteSource(TEXT(R"AS(
UCLASS()
class UEditorRoutingPIECarrier : UObject
{
	UFUNCTION()
	int GetValue() final
	{
		return 91;
	}
}
)AS"))));
		State->Engine = CreateEngine(*State->Project);
		ASSERT_THAT(IsNotNull(State->Engine.Get()));
		if (!State->Engine.IsValid())
		{
			return;
		}
		{
			FAngelscriptEngineScope Scope(*State->Engine);
			State->Engine->InitialCompile();
		}
		State->ScriptClass = FindGeneratedClass(
			State->Engine.Get(), TEXT("UEditorRoutingPIECarrier"));
		ASSERT_THAT(IsNotNull(State->ScriptClass));
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Routes =
			State->Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Routes.IsValid()));
		if (!Routes.IsValid() || State->ScriptClass == nullptr)
		{
			return;
		}
		State->Provider = MakeUnique<FRouteProvider>(
			*Routes, TEXT("pie-provider"));
		ASSERT_THAT(IsTrue(FAngelscriptJITProviderRegistry::Get().
			RegisterProvider(State->Provider.Get(), *State->Provider).
			IsAccepted()));
		State->bProviderRegistered = true;
		const FAngelscriptJITProviderRouteRefreshResult Refresh =
			FAngelscriptJITProviderRouter::Refresh(*State->Engine);
		ASSERT_THAT(IsTrue(Refresh.IsApplied()));
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> PublishedRoutes =
			State->Engine->GetFunctionRouteSnapshot();
		const FAngelscriptFunctionRoute* const GetValue =
			PublishedRoutes.IsValid()
				? FindRouteByName(*PublishedRoutes, "GetValue") : nullptr;
		ASSERT_THAT(IsNotNull(GetValue));
		if (GetValue != nullptr)
		{
			ASSERT_THAT(AreEqual(
				EAngelscriptFunctionExecutionRoute::Native,
				GetValue->SelectedExecutionRoute));
		}

		State->EditorWorld = AngelscriptPIETestUtils::CreateTransientEmptyMap(
			*TestRunner, TEXT("StaticJIT PIE routing"));
		ASSERT_THAT(IsNotNull(State->EditorWorld));
		ASSERT_THAT(IsTrue(AngelscriptPIETestUtils::BuildStandalonePIERequest(
			*TestRunner,
			AGameModeBase::StaticClass(),
			State->RequestParams)));
		if (TestRunner->HasAnyErrors())
		{
			return;
		}

		AddCommand(new FStartPIEForAutomationCommand(State->RequestParams));
		TestCommandBuilder
			.Until(TEXT("Wait for StaticJIT PIE routing world"), []()
			{
				return AngelscriptPIETestUtils::FindPIEWorld() != nullptr;
			}, FTimespan::FromSeconds(30.0))
			.Then(TEXT("Invoke exact Editor Native binding in PIE"),
				[this, State]()
				{
					UWorld* const PIEWorld =
						AngelscriptPIETestUtils::FindPIEWorld();
					ASSERT_THAT(IsNotNull(PIEWorld));
					if (PIEWorld == nullptr)
					{
						return;
					}
					ASSERT_THAT(IsTrue(PIEWorld->WorldType == EWorldType::PIE));
					State->PIEObject.Reset(NewObject<UObject>(
						PIEWorld, State->ScriptClass));
					ASSERT_THAT(IsNotNull(State->PIEObject.Get()));
					UFunction* const Function = FindGeneratedFunction(
						State->ScriptClass, TEXT("GetValue"));
					ASSERT_THAT(IsNotNull(Function));
					int32 Result = 0;
					FAngelscriptEngineScope Scope(*State->Engine);
					ASSERT_THAT(IsTrue(ExecuteGeneratedIntEventOnGameThread(
						State->Engine.Get(),
						State->PIEObject.Get(),
						Function,
						Result)));
					ASSERT_THAT(AreEqual(
						700,
						Result,
						TEXT("PIE reflected dispatch should consume the exact current Provider binding")));
				})
			.Then(TEXT("End StaticJIT PIE routing session"), [State]()
			{
				State->PIEObject.Reset();
				AngelscriptPIETestUtils::EndPIE();
			})
			.Until(TEXT("Wait for StaticJIT PIE routing shutdown"), []()
			{
				return !AngelscriptPIETestUtils::IsPIEWorldAlive();
			}, FTimespan::FromSeconds(30.0));
	}
#endif
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
