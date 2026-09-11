#include "StaticJIT/AngelscriptJITProviderMatcher.h"

#include "AngelscriptTestJITProbes.h"
#include "Async/ParallelFor.h"
#include "CQTest.h"
#include "Features/IModularFeatures.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"
#include "StaticJIT/StaticJITHeader.h"

#include "StartAngelscriptHeaders.h"
#include "as_thread.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptJITMultiProviderTests_Private
{
	static FAngelscriptHash256 MakeHash(const FStringView Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("jit-multi-provider-test"));
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	static void VmEntry(FScriptExecution&, asDWORD*, asQWORD*)
	{
	}

	class FProvider final : public IAngelscriptJITArtifactProvider
	{
	public:
		FString ProviderName;
		FString OwnerModuleName;
		TArray<FAngelscriptJITArtifactEntry> Entries;
		FAngelscriptJITProviderView View;

		FProvider(
			const FStringView ProviderSeed,
			const FStringView FunctionSeed,
			const int32 ScriptModuleCount = 1)
			: ProviderName(FString::Printf(TEXT("Provider-%.*s"),
				ProviderSeed.Len(), ProviderSeed.GetData()))
			, OwnerModuleName(TEXT("AngelscriptTest"))
		{
			View.ProviderId.Hash = MakeHash(ProviderSeed);
			View.ArtifactProfile.Hash = MakeHash(TEXT("profile"));
			View.NativeEnvironmentFingerprint = MakeHash(TEXT("environment"));
			Entries.SetNum(ScriptModuleCount);
			for (int32 Index = 0; Index < ScriptModuleCount; ++Index)
			{
				FAngelscriptJITArtifactEntry& Entry = Entries[Index];
				Entry.ModuleKey.Hash = MakeHash(FString::Printf(
					TEXT("module-%d"), Index));
				Entry.FunctionKey.Hash = MakeHash(FString::Printf(
					TEXT("%.*s-%d"), FunctionSeed.Len(),
					FunctionSeed.GetData(), Index));
				Entry.ExecutionHash = MakeHash(FString::Printf(
					TEXT("execution-%.*s-%d"), FunctionSeed.Len(),
					FunctionSeed.GetData(), Index));
				Entry.DebugHash = MakeHash(FString::Printf(
					TEXT("debug-%.*s-%d"), FunctionSeed.Len(),
					FunctionSeed.GetData(), Index));
				Entry.ArtifactProfile = View.ArtifactProfile;
				Entry.EntryAbiHash = MakeHash(FString::Printf(
					TEXT("abi-%.*s-%d"), FunctionSeed.Len(),
					FunctionSeed.GetData(), Index));
				Entry.NativeEnvironmentFingerprint =
					View.NativeEnvironmentFingerprint;
				Entry.EntryPoints.VMEntry = &VmEntry;
			}
			Refresh();
		}

		const FAngelscriptJITProviderView* GetAngelscriptJITProviderView()
			const override
		{
			return &View;
		}

		void Refresh()
		{
			Entries.Sort([](
				const FAngelscriptJITArtifactEntry& Left,
				const FAngelscriptJITArtifactEntry& Right)
			{
				if (Left.ModuleKey != Right.ModuleKey)
				{
					return Left.ModuleKey.Hash < Right.ModuleKey.Hash;
				}
				return Left.FunctionKey.Hash < Right.FunctionKey.Hash;
			});
			View.ProviderName = *ProviderName;
			View.OwnerModuleName = *OwnerModuleName;
			View.Entries = Entries.GetData();
			View.EntryCount = Entries.Num();
			View.ArtifactSetDigest =
				FAngelscriptJITProviderManifest::BuildArtifactSetDigest(Entries);
			View.ProviderGeneration =
				FAngelscriptJITProviderManifest::BuildProviderGeneration(View);
		}
	};

	static FAngelscriptJITFunctionMatchInput MakeInput(
		const FAngelscriptJITArtifactEntry& Entry)
	{
		FAngelscriptJITFunctionMatchInput Input;
		Input.ModuleKey = Entry.ModuleKey;
		Input.Identity.FunctionKey = Entry.FunctionKey;
		Input.Identity.Content.Execution = Entry.ExecutionHash;
		Input.Identity.Content.Debug = Entry.DebugHash;
		Input.Identity.Profile = Entry.ArtifactProfile;
		Input.EntryAbiHash = Entry.EntryAbiHash;
		Input.NativeEnvironmentFingerprint =
			Entry.NativeEnvironmentFingerprint;
		return Input;
	}

	static void RetargetEntry(
		FProvider& Provider,
		const int32 EntryIndex,
		const FStringView Seed)
	{
		FAngelscriptJITArtifactEntry& Entry = Provider.Entries[EntryIndex];
		Entry.FunctionKey.Hash = MakeHash(FString::Printf(
			TEXT("function-%.*s"), Seed.Len(), Seed.GetData()));
		Entry.ExecutionHash = MakeHash(FString::Printf(
			TEXT("execution-%.*s"), Seed.Len(), Seed.GetData()));
		Entry.DebugHash = MakeHash(FString::Printf(
			TEXT("debug-%.*s"), Seed.Len(), Seed.GetData()));
		Entry.EntryAbiHash = MakeHash(FString::Printf(
			TEXT("abi-%.*s"), Seed.Len(), Seed.GetData()));
		Provider.Refresh();
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptJITMultiProviderTests,
	"Angelscript.TestModule.StaticJIT.MultiProvider",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ConcurrentRegistrationPublishesOneDeterministicallySortedCatalog)
	{
		using namespace AngelscriptJITMultiProviderTests_Private;
		constexpr int32 ProviderCount = 24;
		FAngelscriptJITProviderRegistry Registry;
		TArray<TUniquePtr<FProvider>> Providers;
		Providers.Reserve(ProviderCount);
		for (int32 Index = 0; Index < ProviderCount; ++Index)
		{
			Providers.Add(MakeUnique<FProvider>(
				FString::Printf(TEXT("concurrent-%02d"), Index),
				FString::Printf(TEXT("function-%02d"), Index)));
		}
		TArray<EAngelscriptJITProviderRegistrationCode> Results;
		Results.SetNumUninitialized(ProviderCount);
		ParallelFor(ProviderCount, [&](const int32 Index)
		{
			Results[Index] = Registry.RegisterProvider(
				Providers[Index].Get(), *Providers[Index]).Code;
		});
		for (const EAngelscriptJITProviderRegistrationCode Result : Results)
		{
			ASSERT_THAT(IsTrue(
				Result == EAngelscriptJITProviderRegistrationCode::Registered));
		}

		const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> Snapshot = Registry.GetSnapshot();
		ASSERT_THAT(AreEqual(ProviderCount, Snapshot->Providers.Num()));
		for (int32 Index = 1; Index < Snapshot->Providers.Num(); ++Index)
		{
			ASSERT_THAT(IsTrue(Snapshot->Providers[Index - 1]->ProviderId
				< Snapshot->Providers[Index]->ProviderId));
		}
	}

	TEST_METHOD(OneProviderOwnsSeveralAngelScriptModulesWithoutCrossSelection)
	{
		using namespace AngelscriptJITMultiProviderTests_Private;
		FAngelscriptJITProviderRegistry Registry;
		FProvider Provider(TEXT("multi-module"), TEXT("entry"), 3);
		ASSERT_THAT(IsTrue(Registry.RegisterProvider(&Provider, Provider)
			.IsAccepted()));
		const TSharedRef<const FAngelscriptJITReferenceResolver,
			ESPMode::ThreadSafe> Resolver =
			FAngelscriptJITReferenceIndexBuilder(&Registry).Build();
		for (const FAngelscriptJITArtifactEntry& Entry : Provider.Entries)
		{
			const FAngelscriptJITFunctionMatchResult Result =
				FAngelscriptJITProviderMatcher::Match(
					*Registry.GetSnapshot(), MakeInput(Entry), *Resolver);
			ASSERT_THAT(IsTrue(Result.Result
				== EAngelscriptArtifactMatchResult::Exact));
			ASSERT_THAT(IsTrue(Result.Selection.IsSet()));
			ASSERT_THAT(IsTrue(Result.Selection->Provider->ProviderId
				== Provider.View.ProviderId));
			ASSERT_THAT(IsTrue(Result.Selection->Provider->Entries[
				Result.Selection->EntryIndex].ModuleKey == Entry.ModuleKey));
		}
	}

	TEST_METHOD(CrossProviderExactConflictFallsBackAndOwnerDepartureRecovers)
	{
		using namespace AngelscriptJITMultiProviderTests_Private;
		FAngelscriptJITProviderRegistry Registry;
		FProvider ProjectProvider(TEXT("project"), TEXT("shared"));
		FProvider PluginProvider(TEXT("plugin"), TEXT("shared"));
		ASSERT_THAT(IsTrue(Registry.RegisterProvider(
			&PluginProvider, PluginProvider).IsAccepted()));
		ASSERT_THAT(IsTrue(Registry.RegisterProvider(
			&ProjectProvider, ProjectProvider).IsAccepted()));
		const TSharedRef<const FAngelscriptJITReferenceResolver,
			ESPMode::ThreadSafe> Resolver =
			FAngelscriptJITReferenceIndexBuilder(&Registry).Build();
		const FAngelscriptJITFunctionMatchInput Input =
			MakeInput(ProjectProvider.Entries[0]);

		FAngelscriptJITFunctionMatchResult Result =
			FAngelscriptJITProviderMatcher::Match(
				*Registry.GetSnapshot(), Input, *Resolver);
		ASSERT_THAT(IsTrue(Result.Result
			== EAngelscriptArtifactMatchResult::AmbiguousExactProvider));
		ASSERT_THAT(IsFalse(Result.Selection.IsSet()));
		ASSERT_THAT(AreEqual(2, Result.Candidates.Num()));

		ASSERT_THAT(IsTrue(Registry.UnregisterOwner(&PluginProvider)));
		Result = FAngelscriptJITProviderMatcher::Match(
			*Registry.GetSnapshot(), Input, *Resolver);
		ASSERT_THAT(IsTrue(Result.Result
			== EAngelscriptArtifactMatchResult::Exact));
		ASSERT_THAT(IsTrue(Result.Selection.IsSet()));
		ASSERT_THAT(IsTrue(Result.Selection->Provider->ProviderId
			== ProjectProvider.View.ProviderId));
	}

	TEST_METHOD(NewGenerationDoesNotMutateRetainedOldSelection)
	{
		using namespace AngelscriptJITMultiProviderTests_Private;
		FAngelscriptJITProviderRegistry Registry;
		FProvider Provider(TEXT("refresh"), TEXT("entry"));
		ASSERT_THAT(IsTrue(Registry.RegisterProvider(&Provider, Provider)
			.IsAccepted()));
		const TSharedRef<const FAngelscriptJITReferenceResolver,
			ESPMode::ThreadSafe> Resolver =
			FAngelscriptJITReferenceIndexBuilder(&Registry).Build();
		const FAngelscriptJITFunctionMatchResult OldMatch =
			FAngelscriptJITProviderMatcher::Match(
				*Registry.GetSnapshot(), MakeInput(Provider.Entries[0]), *Resolver);
		ASSERT_THAT(IsTrue(OldMatch.Selection.IsSet()));
		const FAngelscriptJITProviderGeneration OldGeneration =
			OldMatch.Selection->Provider->ProviderGeneration;
		const FAngelscriptHash256 OldExecution =
			OldMatch.Selection->Provider->Entries[0].ExecutionHash;

		Provider.Entries[0].ExecutionHash = MakeHash(TEXT("refreshed-execution"));
		Provider.Refresh();
		ASSERT_THAT(IsTrue(Registry.RegisterProvider(&Provider, Provider).Code
			== EAngelscriptJITProviderRegistrationCode::Replaced));
		const FAngelscriptJITFunctionMatchResult NewMatch =
			FAngelscriptJITProviderMatcher::Match(
				*Registry.GetSnapshot(), MakeInput(Provider.Entries[0]), *Resolver);
		ASSERT_THAT(IsTrue(NewMatch.Selection.IsSet()));
		ASSERT_THAT(IsTrue(NewMatch.Selection->Provider->ProviderGeneration
			!= OldGeneration));
		ASSERT_THAT(IsTrue(OldMatch.Selection->Provider->ProviderGeneration
			== OldGeneration));
		ASSERT_THAT(IsTrue(OldMatch.Selection->Provider->Entries[0].ExecutionHash
			== OldExecution));
	}

	TEST_METHOD(ConflictScopeAndRecoveryAreIndependentOfRegistrationOrder)
	{
		using namespace AngelscriptJITMultiProviderTests_Private;
		for (int32 Order = 0; Order < 2; ++Order)
		{
			FAngelscriptJITProviderRegistry Registry;
			FProvider ProjectProvider(TEXT("project-matrix"), TEXT("shared"), 2);
			FProvider PluginProvider(TEXT("plugin-matrix"), TEXT("shared"), 2);
			FProvider UnrelatedProvider(TEXT("unrelated-matrix"), TEXT("other"));
			RetargetEntry(ProjectProvider, 1, TEXT("project-only"));
			RetargetEntry(PluginProvider, 1, TEXT("plugin-only"));

			if (Order == 0)
			{
				ASSERT_THAT(IsTrue(Registry.RegisterProvider(
					&ProjectProvider, ProjectProvider).IsAccepted()));
				ASSERT_THAT(IsTrue(Registry.RegisterProvider(
					&PluginProvider, PluginProvider).IsAccepted()));
				ASSERT_THAT(IsTrue(Registry.RegisterProvider(
					&UnrelatedProvider, UnrelatedProvider).IsAccepted()));
			}
			else
			{
				ASSERT_THAT(IsTrue(Registry.RegisterProvider(
					&UnrelatedProvider, UnrelatedProvider).IsAccepted()));
				ASSERT_THAT(IsTrue(Registry.RegisterProvider(
					&PluginProvider, PluginProvider).IsAccepted()));
				ASSERT_THAT(IsTrue(Registry.RegisterProvider(
					&ProjectProvider, ProjectProvider).IsAccepted()));
			}

			const TSharedRef<const FAngelscriptJITReferenceResolver,
				ESPMode::ThreadSafe> Resolver =
				FAngelscriptJITReferenceIndexBuilder(&Registry).Build();
			auto Match = [&Registry, &Resolver](
				const FAngelscriptJITArtifactEntry& Entry)
			{
				return FAngelscriptJITProviderMatcher::Match(
					*Registry.GetSnapshot(), MakeInput(Entry), *Resolver);
			};

			ASSERT_THAT(IsTrue(Match(ProjectProvider.Entries[0]).Result
				== EAngelscriptArtifactMatchResult::AmbiguousExactProvider));
			ASSERT_THAT(IsTrue(Match(ProjectProvider.Entries[1]).Result
				== EAngelscriptArtifactMatchResult::Exact));
			ASSERT_THAT(IsTrue(Match(PluginProvider.Entries[1]).Result
				== EAngelscriptArtifactMatchResult::Exact));
			ASSERT_THAT(IsTrue(Match(UnrelatedProvider.Entries[0]).Result
				== EAngelscriptArtifactMatchResult::Exact));

			ASSERT_THAT(IsTrue(Registry.UnregisterOwner(&PluginProvider)));
			ASSERT_THAT(IsTrue(Match(ProjectProvider.Entries[0]).Result
				== EAngelscriptArtifactMatchResult::Exact));
			ASSERT_THAT(IsTrue(Match(UnrelatedProvider.Entries[0]).Result
				== EAngelscriptArtifactMatchResult::Exact));
			ASSERT_THAT(IsTrue(Registry.RegisterProvider(
				&PluginProvider, PluginProvider).IsAccepted()));
			ASSERT_THAT(IsTrue(Match(ProjectProvider.Entries[0]).Result
				== EAngelscriptArtifactMatchResult::AmbiguousExactProvider));
			ASSERT_THAT(IsTrue(Match(UnrelatedProvider.Entries[0]).Result
				== EAngelscriptArtifactMatchResult::Exact));

			ASSERT_THAT(IsTrue(Registry.UnregisterOwner(&ProjectProvider)));
			const FAngelscriptJITFunctionMatchResult RecoveredPlugin =
				Match(PluginProvider.Entries[0]);
			ASSERT_THAT(IsTrue(RecoveredPlugin.Result
				== EAngelscriptArtifactMatchResult::Exact));
			ASSERT_THAT(IsTrue(RecoveredPlugin.Selection.IsSet()));
			ASSERT_THAT(IsTrue(RecoveredPlugin.Selection->Provider->ProviderId
				== PluginProvider.View.ProviderId));
			ASSERT_THAT(IsTrue(Match(UnrelatedProvider.Entries[0]).Result
				== EAngelscriptArtifactMatchResult::Exact));
		}
	}

	TEST_METHOD(TestCarrierDepartureRetainsOldCodeAndUnrelatedProviders)
	{
		using namespace AngelscriptJITMultiProviderTests_Private;
		const FName TestJITModuleName(TEXT("AngelscriptTestJIT"));
		FModuleManager& ModuleManager = FModuleManager::Get();
		ASSERT_THAT(IsTrue(ModuleManager.IsModuleLoaded(TestJITModuleName),
			TEXT("AngelscriptTest depends on the loaded test JIT carrier")));

		FProvider ProjectProvider(TEXT("real-carrier-project"),
			TEXT("project-entry"));
		FProvider PluginProvider(TEXT("real-carrier-plugin"),
			TEXT("plugin-entry"));
		IModularFeatures& Features = IModularFeatures::Get();
		bool bProjectRegistered = false;
		bool bPluginRegistered = false;
		bool bTestJITUnloaded = false;
		ON_SCOPE_EXIT
		{
			if (bTestJITUnloaded)
			{
				ModuleManager.LoadModule(TestJITModuleName);
			}
			if (bPluginRegistered)
			{
				Features.UnregisterModularFeature(
					IAngelscriptJITArtifactProvider::FeatureName(),
					&PluginProvider);
			}
			if (bProjectRegistered)
			{
				Features.UnregisterModularFeature(
					IAngelscriptJITArtifactProvider::FeatureName(),
					&ProjectProvider);
			}
		};

		Features.RegisterModularFeature(
			IAngelscriptJITArtifactProvider::FeatureName(), &ProjectProvider);
		bProjectRegistered = true;
		Features.RegisterModularFeature(
			IAngelscriptJITArtifactProvider::FeatureName(), &PluginProvider);
		bPluginRegistered = true;

		const FAngelscriptJITProviderId TestProviderId =
			AngelscriptTestJITProbes::GetStableProviderId();
		const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> BeforeUnload =
			FAngelscriptJITProviderRegistry::Get().GetSnapshot();
		auto FindProvider = [](const FAngelscriptJITProviderRegistrySnapshot& Snapshot,
			const FAngelscriptJITProviderId& ProviderId)
			-> TSharedPtr<const FAngelscriptJITProviderCatalog,
				ESPMode::ThreadSafe>
		{
			const TSharedPtr<const FAngelscriptJITProviderCatalog,
				ESPMode::ThreadSafe>* Found = Snapshot.Providers.FindByPredicate(
				[&ProviderId](const TSharedPtr<
					const FAngelscriptJITProviderCatalog,
					ESPMode::ThreadSafe>& Candidate)
				{
					return Candidate.IsValid()
						&& Candidate->ProviderId == ProviderId;
				});
			return Found != nullptr ? *Found : nullptr;
		};

		const TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe> OldTestCatalog =
			FindProvider(*BeforeUnload, TestProviderId);
		ASSERT_THAT(IsTrue(OldTestCatalog.IsValid(),
			TEXT("The real AngelscriptTestJIT provider must be registered")));
		ASSERT_THAT(IsTrue(FindProvider(
			*BeforeUnload, ProjectProvider.View.ProviderId).IsValid()));
		ASSERT_THAT(IsTrue(FindProvider(
			*BeforeUnload, PluginProvider.View.ProviderId).IsValid()));
		if (!OldTestCatalog.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(OldTestCatalog->Lifetime.IsValid()));
		ASSERT_THAT(IsTrue(OldTestCatalog->Lifetime->KeepsCodeMapped()));

		const FAngelscriptStableFunctionKey IndependentModuleFunctionKey{
			FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT(
				"6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17")))}};
		const FAngelscriptJITProviderCatalogEntry* IndependentEntry =
			OldTestCatalog->Entries.FindByPredicate(
				[&IndependentModuleFunctionKey](
					const FAngelscriptJITProviderCatalogEntry& Entry)
				{
					return Entry.FunctionKey == IndependentModuleFunctionKey;
				});
		ASSERT_THAT(IsNotNull(IndependentEntry,
			TEXT("The committed independent-module fixture entry must exist")));
		if (IndependentEntry == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsNotNull(
			reinterpret_cast<const void*>(IndependentEntry->EntryPoints.VMEntry)));

		asQWORD BeforeUnloadResult = 0;
		{
			FScriptExecution Execution(asCThreadManager::GetLocalData());
			IndependentEntry->EntryPoints.VMEntry(
				Execution, nullptr, &BeforeUnloadResult);
		}
		ASSERT_THAT(AreEqual(87, static_cast<int32>(BeforeUnloadResult),
			TEXT("The real generated entry must execute before module departure")));

		ASSERT_THAT(IsTrue(ModuleManager.UnloadModule(
			TestJITModuleName, false, true),
			TEXT("The test must exercise the explicit physical-unload path")));
		bTestJITUnloaded = true;
		ASSERT_THAT(IsFalse(ModuleManager.IsModuleLoaded(TestJITModuleName)));

		const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> AfterUnload =
			FAngelscriptJITProviderRegistry::Get().GetSnapshot();
		ASSERT_THAT(IsFalse(FindProvider(*AfterUnload, TestProviderId).IsValid(),
			TEXT("Departure must prevent new selection from TestJIT immediately")));
		ASSERT_THAT(IsTrue(FindProvider(
			*AfterUnload, ProjectProvider.View.ProviderId).IsValid(),
			TEXT("Project provider must survive TestJIT departure")));
		ASSERT_THAT(IsTrue(FindProvider(
			*AfterUnload, PluginProvider.View.ProviderId).IsValid(),
			TEXT("Plugin provider must survive TestJIT departure")));

		asQWORD RetiredGenerationResult = 0;
		{
			FScriptExecution Execution(asCThreadManager::GetLocalData());
			IndependentEntry->EntryPoints.VMEntry(
				Execution, nullptr, &RetiredGenerationResult);
		}
		ASSERT_THAT(AreEqual(87, static_cast<int32>(RetiredGenerationResult),
			TEXT("A retained old generation must keep its generated code callable")));

		ASSERT_THAT(IsNotNull(ModuleManager.LoadModule(TestJITModuleName),
			TEXT("The fixed carrier must recover after explicit reload")));
		bTestJITUnloaded = false;
		const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> AfterReload =
			FAngelscriptJITProviderRegistry::Get().GetSnapshot();
		const TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe> ReloadedTestCatalog =
			FindProvider(*AfterReload, TestProviderId);
		ASSERT_THAT(IsTrue(ReloadedTestCatalog.IsValid(),
			TEXT("Reload must publish the real TestJIT provider again")));
		ASSERT_THAT(IsTrue(ReloadedTestCatalog.IsValid()
			&& ReloadedTestCatalog->ProviderGeneration
				== OldTestCatalog->ProviderGeneration,
			TEXT("Unchanged generated bytes must recover the same generation")));
		ASSERT_THAT(IsTrue(FindProvider(
			*AfterReload, ProjectProvider.View.ProviderId).IsValid()));
		ASSERT_THAT(IsTrue(FindProvider(
			*AfterReload, PluginProvider.View.ProviderId).IsValid()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
