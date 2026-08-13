#include "AngelscriptEngine.h"
#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Shared/AngelscriptTestEngineAcquisition.h"
#include "StaticJIT/AngelscriptJITGeneration.h"
#include "StaticJIT/AngelscriptJITProviderRegistry.h"
#include "StaticJIT/AngelscriptJITProviderRouter.h"
#include "StaticJIT/AngelscriptStaticJIT.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptJITPackagedProviderTests_Private
{
	class FScopedProjectRoot final
	{
	public:
		explicit FScopedProjectRoot(const TCHAR* Leaf)
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptJITPackagedProvider"),
				Leaf,
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptJITPackagedProvider/")));
			ScriptRoot = Root / TEXT("Script");
			CacheRoot = Root / TEXT("CacheV2");
			check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		}

		~FScopedProjectRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptJITPackagedProvider/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		bool WriteSources(const bool bReverseCreationOrder = false) const
		{
			const auto WriteAlpha = [this]()
			{
				return FFileHelper::SaveStringToFile(TEXT(R"AS(
int PackagedAlpha()
{
	return 101;
}

int PackagedAlphaSecond()
{
	return 102;
}
)AS"), *(ScriptRoot / TEXT("PackagedAlpha.as")));
			};
			const auto WriteBeta = [this]()
			{
				return FFileHelper::SaveStringToFile(TEXT(R"AS(
int PackagedBeta()
{
	return 201;
}
)AS"), *(ScriptRoot / TEXT("PackagedBeta.as")));
			};
			return bReverseCreationOrder
				? WriteBeta() && WriteAlpha()
				: WriteAlpha() && WriteBeta();
		}

		FString Root;
		FString ScriptRoot;
		FString CacheRoot;
	};

	static FAngelscriptHash256 MakeHash(const FStringView Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("jit-packaged-provider-test"));
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
			*OutValue = 909;
		}
	}

	static asDWORD ProviderRawEntry(FScriptExecution&, void*)
	{
		return 909;
	}

	static void ProviderParmsEntry(FScriptExecution&, void*, void*)
	{
	}

	static TUniquePtr<FAngelscriptEngine> CreateEngine(
		const FScopedProjectRoot& Project,
		const EAngelscriptJITTargetProfile Profile)
	{
		FAngelscriptEngineConfig Config;
		Config.bIsEditor = false;
		Config.bDevelopmentMode = false;
		Config.bSkipThreadedInitialize = true;
		Config.bOverrideTargetPreprocessorFlags = true;
		Config.bTargetEditor = false;
		Config.bTargetEditorOnlyData = false;
		Config.bTargetRelease =
			Profile == EAngelscriptJITTargetProfile::GameShipping;
		Config.bTargetTest = false;
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

	class FRouteProvider final : public IAngelscriptJITArtifactProvider
	{
	public:
		FRouteProvider(
			const FAngelscriptFunctionRouteSnapshot& Routes,
			const EAngelscriptJITTargetProfile Profile,
			const TCHAR* ProviderSeed)
		{
			ProviderName = FString::Printf(
				TEXT("PackagedProvider-%s"), ProviderSeed);
			OwnerModuleName = TEXT("AngelscriptTest");
			View.ProviderId.Hash = MakeHash(ProviderSeed);
			View.NativeEnvironmentFingerprint =
				FAngelscriptJITTargetProfiles::
					BuildCurrentNativeEnvironmentFingerprint(Profile);

			for (const FAngelscriptFunctionRoute& Route : Routes.FunctionRoutes)
			{
				if (Route.Function == nullptr
					|| !Route.bHasVerifiedArtifactIdentity)
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
			RefreshView();
		}

		const FAngelscriptJITProviderView* GetAngelscriptJITProviderView()
			const override
		{
			return &View;
		}

		void RefreshView()
		{
			View.ProviderName = *ProviderName;
			View.OwnerModuleName = *OwnerModuleName;
			View.ArtifactProfile = Entries[0].ArtifactProfile;
			View.Entries = Entries.GetData();
			View.EntryCount = Entries.Num();
			View.ArtifactSetDigest =
				FAngelscriptJITProviderManifest::BuildArtifactSetDigest(Entries);
			View.ProviderGeneration =
				FAngelscriptJITProviderManifest::BuildProviderGeneration(View);
		}

		FString ProviderName;
		FString OwnerModuleName;
		TArray<FAngelscriptJITArtifactEntry> Entries;
		FAngelscriptJITProviderView View;
	};

	static FString BuildStableSelectionSignature(
		const FAngelscriptJITProviderRouteRefreshResult& Result)
	{
		TArray<FString> Records;
		for (const FAngelscriptJITProviderRouteRecord& Route : Result.Routes)
		{
			Records.Add(FString::Printf(
				TEXT("%s|%s|%s|%s|%u|%u"),
				*Route.ModuleKey.Hash.ToHexString(),
				*Route.FunctionKey.Hash.ToHexString(),
				*Route.ProviderId.Hash.ToHexString(),
				*Route.ProviderGeneration.Hash.ToHexString(),
				static_cast<uint32>(Route.MatchResult),
				Route.bBindingPublished ? 1u : 0u));
		}
		Records.Sort();
		return FString::Join(Records, TEXT("\n"));
	}

	static TArray<asIScriptFunction*> CaptureFunctionPointers(
		const FAngelscriptFunctionRouteSnapshot& Routes)
	{
		TArray<asIScriptFunction*> Result;
		for (const FAngelscriptFunctionRoute& Route : Routes.FunctionRoutes)
		{
			if (Route.Function != nullptr && Route.bHasVerifiedArtifactIdentity)
			{
				Result.Add(Route.Function);
			}
		}
		return Result;
	}

	static TArray<int32> CaptureFunctionIds(
		const FAngelscriptFunctionRouteSnapshot& Routes)
	{
		TArray<int32> Result;
		for (const FAngelscriptFunctionRoute& Route : Routes.FunctionRoutes)
		{
			if (Route.Function != nullptr && Route.bHasVerifiedArtifactIdentity)
			{
				Result.Add(Route.Function->GetId());
			}
		}
		return Result;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptJITPackagedProviderTests,
	"Angelscript.TestModule.StaticJIT.PackagedProvider",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(DevelopmentAndShippingSelectCompleteProviderThenUnloadToVm)
	{
		using namespace AngelscriptJITPackagedProviderTests_Private;
		for (const EAngelscriptJITTargetProfile Profile : {
			EAngelscriptJITTargetProfile::GameDevelopment,
			EAngelscriptJITTargetProfile::GameShipping})
		{
			FScopedProjectRoot Project(
				FAngelscriptJITTargetProfiles::ToString(Profile));
			ASSERT_THAT(IsTrue(Project.WriteSources()));
			TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project, Profile);
			ASSERT_THAT(IsNotNull(Engine.Get()));
			if (!Engine.IsValid())
			{
				continue;
			}
			FAngelscriptEngineScope Scope(*Engine);
			Engine->InitialCompile();
			const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
				ESPMode::ThreadSafe> Routes = Engine->GetFunctionRouteSnapshot();
			ASSERT_THAT(IsTrue(Routes.IsValid()));
			if (!Routes.IsValid())
			{
				continue;
			}

			FRouteProvider Provider(
				*Routes,
				Profile,
				Profile == EAngelscriptJITTargetProfile::GameShipping
					? TEXT("shipping-complete")
					: TEXT("development-complete"));
			FAngelscriptJITProviderRegistry& Registry =
				FAngelscriptJITProviderRegistry::Get();
			ASSERT_THAT(IsTrue(
				Registry.RegisterProvider(&Provider, Provider).IsAccepted()));
			const FAngelscriptJITProviderRouteRefreshResult Native =
				FAngelscriptJITProviderRouter::Refresh(*Engine);
			ASSERT_THAT(IsTrue(Native.IsApplied()));
			ASSERT_THAT(AreEqual(Profile, Native.TargetProfile));
			ASSERT_THAT(AreEqual(
				Native.VerifiedRouteCount, Native.ExactMatchCount));
			ASSERT_THAT(AreEqual(
				Native.VerifiedRouteCount, Native.PublishedNativeCount));
			ASSERT_THAT(AreEqual(0u, Native.VmFallbackCount));
			ASSERT_THAT(IsTrue(Registry.UnregisterOwner(&Provider)));

			const FAngelscriptJITProviderRouteRefreshResult Fallback =
				FAngelscriptJITProviderRouter::Refresh(*Engine);
			ASSERT_THAT(IsTrue(Fallback.IsApplied()));
			ASSERT_THAT(AreEqual(0u, Fallback.PublishedNativeCount));
			ASSERT_THAT(AreEqual(
				Fallback.VerifiedRouteCount, Fallback.VmFallbackCount));
		}
	}

	TEST_METHOD(StaleEntryFallsBackAloneAndProviderIdConflictRecovers)
	{
		using namespace AngelscriptJITPackagedProviderTests_Private;
		FScopedProjectRoot Project(TEXT("PartialAndConflict"));
		ASSERT_THAT(IsTrue(Project.WriteSources()));
		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(
			Project, EAngelscriptJITTargetProfile::GameDevelopment);
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

		FRouteProvider Current(
			*Routes,
			EAngelscriptJITTargetProfile::GameDevelopment,
			TEXT("conflicting-provider-id"));
		FRouteProvider Replacement(
			*Routes,
			EAngelscriptJITTargetProfile::GameDevelopment,
			TEXT("replacement-provider-id"));
		Replacement.View.ProviderId = Current.View.ProviderId;
		Replacement.Entries[0].DebugHash = MakeHash(TEXT("new-debug-generation"));
		Replacement.RefreshView();

		FAngelscriptJITProviderRegistry& Registry =
			FAngelscriptJITProviderRegistry::Get();
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&Current, Current).IsAccepted()));
		ON_SCOPE_EXIT
		{
			Registry.UnregisterOwner(&Current);
			Registry.UnregisterOwner(&Replacement);
		};
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&Replacement, Replacement).Code
				== EAngelscriptJITProviderRegistrationCode::ProviderIdConflict));
		ASSERT_THAT(IsTrue(Registry.UnregisterOwner(&Current)));
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&Replacement, Replacement).IsAccepted()));
		const FAngelscriptJITProviderRouteRefreshResult Recovered =
			FAngelscriptJITProviderRouter::Refresh(*Engine);
		ASSERT_THAT(AreEqual(
			Recovered.VerifiedRouteCount, Recovered.PublishedNativeCount));

		ASSERT_THAT(IsTrue(Registry.UnregisterOwner(&Replacement)));
		FRouteProvider Stale(
			*Routes,
			EAngelscriptJITTargetProfile::GameDevelopment,
			TEXT("one-stale-entry"));
		Stale.Entries[0].ExecutionHash = MakeHash(TEXT("stale-execution"));
		Stale.RefreshView();
		ASSERT_THAT(IsTrue(
			Registry.RegisterProvider(&Stale, Stale).IsAccepted()));
		ON_SCOPE_EXIT
		{
			Registry.UnregisterOwner(&Stale);
		};
		const FAngelscriptJITProviderRouteRefreshResult Partial =
			FAngelscriptJITProviderRouter::Refresh(*Engine);
		ASSERT_THAT(AreEqual(1u, Partial.VmFallbackCount));
		ASSERT_THAT(AreEqual(
			Partial.VerifiedRouteCount - 1u,
			Partial.PublishedNativeCount));
		ASSERT_THAT(AreEqual(
			1,
			Partial.Routes.FilterByPredicate([](
				const FAngelscriptJITProviderRouteRecord& Route)
			{
				return Route.MatchResult
					== EAngelscriptArtifactMatchResult::ContentMismatch;
			}).Num()));
	}

	TEST_METHOD(StableSelectionIgnoresTransientIdsPointersAndCreationOrder)
	{
		using namespace AngelscriptJITPackagedProviderTests_Private;
		FScopedProjectRoot FirstProject(TEXT("MultiStartA"));
		FScopedProjectRoot SecondProject(TEXT("MultiStartB"));
		ASSERT_THAT(IsTrue(FirstProject.WriteSources(false)));
		ASSERT_THAT(IsTrue(SecondProject.WriteSources(true)));

		TUniquePtr<FAngelscriptEngine> FirstEngine = CreateEngine(
			FirstProject, EAngelscriptJITTargetProfile::GameDevelopment);
		TUniquePtr<FAngelscriptEngine> SecondEngine = CreateEngine(
			SecondProject, EAngelscriptJITTargetProfile::GameDevelopment);
		ASSERT_THAT(IsNotNull(FirstEngine.Get()));
		ASSERT_THAT(IsNotNull(SecondEngine.Get()));
		if (!FirstEngine.IsValid() || !SecondEngine.IsValid())
		{
			return;
		}

		FString FirstSignature;
		FAngelscriptJITProviderGeneration FirstGeneration;
		TArray<asIScriptFunction*> FirstPointers;
		TArray<int32> FirstIds;
		{
			FAngelscriptEngineScope Scope(*FirstEngine);
			FirstEngine->InitialCompile();
			const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
				ESPMode::ThreadSafe> Routes =
				FirstEngine->GetFunctionRouteSnapshot();
			ASSERT_THAT(IsTrue(Routes.IsValid()));
			if (!Routes.IsValid())
			{
				return;
			}
			FirstPointers = CaptureFunctionPointers(*Routes);
			FirstIds = CaptureFunctionIds(*Routes);
			FRouteProvider Provider(
				*Routes,
				EAngelscriptJITTargetProfile::GameDevelopment,
				TEXT("multi-start-stable"));
			FirstGeneration = Provider.View.ProviderGeneration;
			FAngelscriptJITProviderRegistry& Registry =
				FAngelscriptJITProviderRegistry::Get();
			ASSERT_THAT(IsTrue(
				Registry.RegisterProvider(&Provider, Provider).IsAccepted()));
			FirstSignature = BuildStableSelectionSignature(
				FAngelscriptJITProviderRouter::Refresh(*FirstEngine));
			ASSERT_THAT(IsTrue(Registry.UnregisterOwner(&Provider)));
		}

		FString SecondSignature;
		FAngelscriptJITProviderGeneration SecondGeneration;
		TArray<asIScriptFunction*> SecondPointers;
		TArray<int32> SecondIds;
		{
			FAngelscriptEngineScope Scope(*SecondEngine);
			asIScriptModule* const Padding =
				SecondEngine->GetScriptEngine()->GetModule(
					"TransientFunctionIdPadding", asGM_ALWAYS_CREATE);
			ASSERT_THAT(IsNotNull(Padding));
			if (Padding != nullptr)
			{
				const ANSICHAR* PaddingSource =
					"int TransientPadding() { return 1; }";
				ASSERT_THAT(AreEqual(asSUCCESS,
					Padding->AddScriptSection(
						"TransientPadding.as", PaddingSource)));
				ASSERT_THAT(AreEqual(asSUCCESS, Padding->Build()));
			}
			SecondEngine->InitialCompile();
			const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
				ESPMode::ThreadSafe> Routes =
				SecondEngine->GetFunctionRouteSnapshot();
			ASSERT_THAT(IsTrue(Routes.IsValid()));
			if (!Routes.IsValid())
			{
				return;
			}
			SecondPointers = CaptureFunctionPointers(*Routes);
			SecondIds = CaptureFunctionIds(*Routes);
			FRouteProvider Provider(
				*Routes,
				EAngelscriptJITTargetProfile::GameDevelopment,
				TEXT("multi-start-stable"));
			SecondGeneration = Provider.View.ProviderGeneration;
			FAngelscriptJITProviderRegistry& Registry =
				FAngelscriptJITProviderRegistry::Get();
			ASSERT_THAT(IsTrue(
				Registry.RegisterProvider(&Provider, Provider).IsAccepted()));
			SecondSignature = BuildStableSelectionSignature(
				FAngelscriptJITProviderRouter::Refresh(*SecondEngine));
			ASSERT_THAT(IsTrue(Registry.UnregisterOwner(&Provider)));
		}

		ASSERT_THAT(AreEqual(FirstSignature, SecondSignature));
		ASSERT_THAT(IsTrue(FirstGeneration == SecondGeneration));
		ASSERT_THAT(AreEqual(FirstPointers.Num(), SecondPointers.Num()));
		ASSERT_THAT(AreEqual(FirstIds.Num(), SecondIds.Num()));
		ASSERT_THAT(IsFalse(FirstPointers == SecondPointers,
			TEXT("Two live Engines must expose different transient function pointers")));
		ASSERT_THAT(IsFalse(FirstIds == SecondIds,
			TEXT("Padding one Engine must shift transient numeric FunctionIds")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
