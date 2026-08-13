#include "StaticJIT/AngelscriptJITProviderRegistry.h"

#include "CQTest.h"
#include "Features/IModularFeatures.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptJITProviderRegistryTests_Private
{
	static FAngelscriptHash256 MakeHash(const TCHAR* Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("jit-provider-registry-test"));
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	static void VmEntry(FScriptExecution&, asDWORD*, asQWORD*)
	{
	}

	class FProvider : public IAngelscriptJITArtifactProvider
	{
	public:
		FString ProviderName;
		FString OwnerModuleName;
		TArray<FAngelscriptJITReferenceSlot> References;
		TArray<FAngelscriptJITArtifactEntry> Entries;
		FAngelscriptJITProviderView View;

		FProvider(
			const TCHAR* ProviderSeed,
			const TCHAR*,
			const TCHAR* ModuleSeed,
			const TCHAR* FunctionSeed)
			: ProviderName(FString::Printf(TEXT("Provider-%s"), ProviderSeed))
			, OwnerModuleName(TEXT("AngelscriptTest"))
		{
			References.SetNum(1);
			References[0].Reference.Kind =
				EAngelscriptArtifactReferenceKind::ScriptFunction;
			References[0].Reference.StableKey = MakeHash(TEXT("reference"));
			References[0].Reference.ExpectedAbi = MakeHash(TEXT("reference-abi"));

			Entries.SetNum(1);
			Entries[0].ModuleKey.Hash = MakeHash(ModuleSeed);
			Entries[0].FunctionKey.Hash = MakeHash(FunctionSeed);
			Entries[0].ExecutionHash = MakeHash(TEXT("execution"));
			Entries[0].DebugHash = MakeHash(TEXT("debug"));
			Entries[0].ArtifactProfile.Hash = MakeHash(TEXT("profile"));
			Entries[0].EntryAbiHash = MakeHash(TEXT("entry-abi"));
			Entries[0].NativeEnvironmentFingerprint =
				MakeHash(TEXT("environment"));
			Entries[0].EntryPoints.VMEntry = &VmEntry;
			Entries[0].ReferenceSlots = References.GetData();
			Entries[0].ReferenceSlotCount = References.Num();

			View.ProviderId.Hash = MakeHash(ProviderSeed);
			View.ArtifactProfile = Entries[0].ArtifactProfile;
			View.NativeEnvironmentFingerprint =
				Entries[0].NativeEnvironmentFingerprint;
			RefreshViewPointersAndDigest();
		}

		const FAngelscriptJITProviderView* GetAngelscriptJITProviderView()
			const override
		{
			return &View;
		}

		void RefreshViewPointersAndDigest()
		{
			Entries[0].ReferenceSlots = References.GetData();
			Entries[0].ReferenceSlotCount = References.Num();
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
}

TEST_CLASS_WITH_FLAGS(FAngelscriptJITProviderRegistryTests,
	"Angelscript.TestModule.StaticJIT.ProviderRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(RegistrationCopiesProviderOwnedMemory)
	{
		using namespace AngelscriptJITProviderRegistryTests_Private;
		FAngelscriptJITProviderRegistry Registry;
		FProvider Provider(TEXT("copy"), TEXT("generation-a"),
			TEXT("module"), TEXT("function"));
		const int Owner = 1;

		const FAngelscriptJITProviderRegistrationResult Result =
			Registry.RegisterProvider(&Owner, Provider);
		ASSERT_THAT(IsTrue(Result.IsAccepted()));
		const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> Snapshot = Registry.GetSnapshot();
		ASSERT_THAT(IsNotNull(Snapshot.Get()));
		ASSERT_THAT(AreEqual(1, Snapshot->Providers.Num()));

		Provider.ProviderName = TEXT("Mutated");
		Provider.References[0].Reference.StableKey = MakeHash(TEXT("mutated"));
		Provider.Entries[0].FunctionKey.Hash = MakeHash(TEXT("mutated-function"));
		Provider.RefreshViewPointersAndDigest();

		const FAngelscriptJITProviderCatalog& Copied = *Snapshot->Providers[0];
		ASSERT_THAT(AreEqual(FString(TEXT("Provider-copy")), Copied.ProviderName));
		ASSERT_THAT(IsTrue(Copied.Lifetime.IsValid()));
		ASSERT_THAT(IsTrue(Copied.Lifetime->GetProviderId()
			== Copied.ProviderId));
		ASSERT_THAT(IsTrue(Copied.Lifetime->GetProviderGeneration()
			== Copied.ProviderGeneration));
		ASSERT_THAT(IsTrue(Copied.Entries[0].FunctionKey
			!= Provider.Entries[0].FunctionKey));
		ASSERT_THAT(IsTrue(Copied.Entries[0].ReferenceSlots[0].Reference.StableKey
			!= Provider.References[0].Reference.StableKey));
	}

	TEST_METHOD(EnumerationIsDeterministicAndOwnerScoped)
	{
		using namespace AngelscriptJITProviderRegistryTests_Private;
		FAngelscriptJITProviderRegistry Registry;
		FProvider First(TEXT("first"), TEXT("generation-first"),
			TEXT("module-first"), TEXT("function-first"));
		FProvider Second(TEXT("second"), TEXT("generation-second"),
			TEXT("module-second"), TEXT("function-second"));
		const int FirstOwner = 1;
		const int SecondOwner = 2;

		ASSERT_THAT(IsTrue(Registry.RegisterProvider(&SecondOwner, Second)
			.IsAccepted()));
		const FAngelscriptJITProviderRegistrationResult FirstResult =
			Registry.RegisterProvider(&FirstOwner, First);
		ASSERT_THAT(AreEqual(
			static_cast<uint8>(EAngelscriptJITProviderRegistrationCode::Registered),
			static_cast<uint8>(FirstResult.Code)));
		ASSERT_THAT(AreEqual(
			static_cast<uint8>(EAngelscriptJITProviderValidationCode::Valid),
			static_cast<uint8>(FirstResult.Validation.Code)));
		TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> Snapshot = Registry.GetSnapshot();
		ASSERT_THAT(AreEqual(2, Snapshot->Providers.Num()));
		ASSERT_THAT(IsTrue(Snapshot->Providers[0]->ProviderId
			< Snapshot->Providers[1]->ProviderId));

		ASSERT_THAT(IsTrue(Registry.UnregisterOwner(&SecondOwner)));
		Snapshot = Registry.GetSnapshot();
		ASSERT_THAT(AreEqual(1, Snapshot->Providers.Num()));
		ASSERT_THAT(IsTrue(Snapshot->Providers[0]->ProviderId
			== First.View.ProviderId));
		ASSERT_THAT(IsFalse(Registry.UnregisterOwner(&SecondOwner)));
	}

	TEST_METHOD(NewGenerationReplacesAtomicallyAndOldSnapshotRemainsAlive)
	{
		using namespace AngelscriptJITProviderRegistryTests_Private;
		FAngelscriptJITProviderRegistry Registry;
		FProvider Provider(TEXT("replace"), TEXT("generation-a"),
			TEXT("module"), TEXT("function"));
		const int Owner = 1;
		ASSERT_THAT(IsTrue(Registry.RegisterProvider(&Owner, Provider)
			.Code == EAngelscriptJITProviderRegistrationCode::Registered));
		TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> OldSnapshot = Registry.GetSnapshot();
		TWeakPtr<const FAngelscriptJITProviderLifetime,
			ESPMode::ThreadSafe> WeakOldLifetime =
			OldSnapshot->Providers[0]->Lifetime;

		Provider.Entries[0].ExecutionHash = MakeHash(TEXT("execution-b"));
		Provider.RefreshViewPointersAndDigest();
		ASSERT_THAT(IsTrue(Registry.RegisterProvider(&Owner, Provider)
			.Code == EAngelscriptJITProviderRegistrationCode::Replaced));
		const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> NewSnapshot = Registry.GetSnapshot();

		ASSERT_THAT(IsTrue(NewSnapshot->PublicationOrdinal
			> OldSnapshot->PublicationOrdinal));
		ASSERT_THAT(IsTrue(OldSnapshot->Providers[0]->ProviderGeneration
			!= NewSnapshot->Providers[0]->ProviderGeneration));
		ASSERT_THAT(IsTrue(OldSnapshot->Providers[0]->Entries[0].ExecutionHash
			!= NewSnapshot->Providers[0]->Entries[0].ExecutionHash));
		ASSERT_THAT(IsTrue(WeakOldLifetime.IsValid()));
		ASSERT_THAT(IsTrue(OldSnapshot->Providers[0]->Lifetime
			!= NewSnapshot->Providers[0]->Lifetime));
		OldSnapshot.Reset();
		ASSERT_THAT(IsFalse(WeakOldLifetime.IsValid()));
	}

	TEST_METHOD(ProviderIdConflictRejectsAndRecoversAfterOwnerDeparture)
	{
		using namespace AngelscriptJITProviderRegistryTests_Private;
		FAngelscriptJITProviderRegistry Registry;
		FProvider First(TEXT("conflict"), TEXT("generation-a"),
			TEXT("module-a"), TEXT("function-a"));
		FProvider Second(TEXT("other"), TEXT("generation-b"),
			TEXT("module-b"), TEXT("function-b"));
		Second.View.ProviderId = First.View.ProviderId;
		Second.RefreshViewPointersAndDigest();
		const int FirstOwner = 1;
		const int SecondOwner = 2;

		const FAngelscriptJITProviderRegistrationResult FirstResult =
			Registry.RegisterProvider(&FirstOwner, First);
		ASSERT_THAT(AreEqual(
			static_cast<uint8>(EAngelscriptJITProviderRegistrationCode::Registered),
			static_cast<uint8>(FirstResult.Code)));
		ASSERT_THAT(AreEqual(
			static_cast<uint8>(EAngelscriptJITProviderValidationCode::Valid),
			static_cast<uint8>(FirstResult.Validation.Code)));
		ASSERT_THAT(IsTrue(Registry.RegisterProvider(&SecondOwner, Second).Code
			== EAngelscriptJITProviderRegistrationCode::ProviderIdConflict));
		ASSERT_THAT(AreEqual(1, Registry.GetSnapshot()->Providers.Num()));
		ASSERT_THAT(IsTrue(Registry.UnregisterOwner(&FirstOwner)));
		ASSERT_THAT(IsTrue(Registry.RegisterProvider(&SecondOwner, Second)
			.IsAccepted()));
		ASSERT_THAT(IsTrue(Registry.GetSnapshot()->Providers[0]->ProviderGeneration
			== Second.View.ProviderGeneration));
	}

	TEST_METHOD(NewGenerationCanTransferTheSameProviderIdentityToANewOwner)
	{
		using namespace AngelscriptJITProviderRegistryTests_Private;
		FAngelscriptJITProviderRegistry Registry;
		FProvider Original(TEXT("live-coding"), TEXT("generation-a"),
			TEXT("module"), TEXT("function"));
		FProvider Patched(TEXT("live-coding"), TEXT("generation-b"),
			TEXT("module"), TEXT("function"));
		Patched.Entries[0].ExecutionHash = MakeHash(TEXT("patched-execution"));
		Patched.RefreshViewPointersAndDigest();
		const int OriginalOwner = 1;
		const int PatchedOwner = 2;

		ASSERT_THAT(IsTrue(Registry.RegisterProvider(&OriginalOwner, Original).Code
			== EAngelscriptJITProviderRegistrationCode::Registered));
		ASSERT_THAT(IsTrue(Registry.RegisterProvider(&PatchedOwner, Patched).Code
			== EAngelscriptJITProviderRegistrationCode::Replaced));
		ASSERT_THAT(AreEqual(1, Registry.GetSnapshot()->Providers.Num()));
		ASSERT_THAT(IsTrue(
			Registry.GetSnapshot()->Providers[0]->ProviderGeneration
				== Patched.View.ProviderGeneration));
		ASSERT_THAT(IsFalse(Registry.UnregisterOwner(&OriginalOwner)));
		ASSERT_THAT(IsTrue(Registry.UnregisterOwner(&PatchedOwner)));
	}

	TEST_METHOD(SameGenerationDifferentManifestIsRejectedWithoutReplacement)
	{
		using namespace AngelscriptJITProviderRegistryTests_Private;
		FAngelscriptJITProviderRegistry Registry;
		FProvider Provider(TEXT("ambiguous"), TEXT("same-generation"),
			TEXT("module"), TEXT("function"));
		const int Owner = 1;
		ASSERT_THAT(IsTrue(Registry.RegisterProvider(&Owner, Provider)
			.IsAccepted()));
		const FAngelscriptHash256 PublishedDigest =
			Registry.GetSnapshot()->Providers[0]->ArtifactSetDigest;

		Provider.ProviderName = TEXT("Different diagnostic name, same content");
		Provider.RefreshViewPointersAndDigest();
		ASSERT_THAT(IsTrue(Registry.RegisterProvider(&Owner, Provider).Code
			== EAngelscriptJITProviderRegistrationCode::AmbiguousGeneration));
		ASSERT_THAT(IsTrue(Registry.GetSnapshot()->Providers[0].Get()
			->ArtifactSetDigest == PublishedDigest));
	}

	TEST_METHOD(ModularFeatureArrivalAndDepartureAreObserved)
	{
		using namespace AngelscriptJITProviderRegistryTests_Private;
		FAngelscriptJITProviderRegistry Registry;
		Registry.StartModularFeatureDiscovery();
		ON_SCOPE_EXIT
		{
			Registry.StopModularFeatureDiscovery();
		};
		const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> BaselineSnapshot = Registry.GetSnapshot();

		FProvider Provider(TEXT("feature"), TEXT("generation"),
			TEXT("module"), TEXT("function"));
		IModularFeatures::Get().RegisterModularFeature(
			IAngelscriptJITArtifactProvider::FeatureName(), &Provider);
		bool bFeatureRegistered = true;
		ON_SCOPE_EXIT
		{
			if (bFeatureRegistered)
			{
				IModularFeatures::Get().UnregisterModularFeature(
					IAngelscriptJITArtifactProvider::FeatureName(), &Provider);
			}
		};
		TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> RegisteredSnapshot = Registry.GetSnapshot();
		ASSERT_THAT(AreEqual(
			BaselineSnapshot->Providers.Num() + 1,
			RegisteredSnapshot->Providers.Num()));
		const TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe>* RegisteredProvider =
			RegisteredSnapshot->Providers.FindByPredicate(
				[&Provider](const TSharedPtr<
					const FAngelscriptJITProviderCatalog,
					ESPMode::ThreadSafe>& Candidate)
				{
					return Candidate.IsValid()
						&& Candidate->ProviderId == Provider.View.ProviderId;
				});
		ASSERT_THAT(IsNotNull(RegisteredProvider));
		if (RegisteredProvider == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			(*RegisteredProvider)->Lifetime->KeepsCodeMapped()));
		ASSERT_THAT(IsTrue(
			(*RegisteredProvider)->Lifetime
				->GetPinnedModuleFilenames().Num() > 0));
		ASSERT_THAT(AreEqual(
			FString(TEXT("AngelscriptTest")),
			(*RegisteredProvider)->Lifetime->GetOwnerModuleName()));
		TWeakPtr<const FAngelscriptJITProviderLifetime,
			ESPMode::ThreadSafe> WeakLifetime =
			(*RegisteredProvider)->Lifetime;

		IModularFeatures::Get().UnregisterModularFeature(
			IAngelscriptJITArtifactProvider::FeatureName(), &Provider);
		bFeatureRegistered = false;
		const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> AfterDeparture = Registry.GetSnapshot();
		ASSERT_THAT(AreEqual(
			BaselineSnapshot->Providers.Num(),
			AfterDeparture->Providers.Num()));
		for (const TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe>& BaselineProvider : BaselineSnapshot->Providers)
		{
			ASSERT_THAT(IsTrue(AfterDeparture->Providers.Contains(
				BaselineProvider)));
		}
		ASSERT_THAT(IsTrue(WeakLifetime.IsValid()));
		RegisteredSnapshot.Reset();
		ASSERT_THAT(IsFalse(WeakLifetime.IsValid()));
	}

	TEST_METHOD(RegistrationRejectsAnOwnerModuleThatCannotRetainCode)
	{
		using namespace AngelscriptJITProviderRegistryTests_Private;
		FAngelscriptJITProviderRegistry Registry;
		FProvider Provider(TEXT("missing-owner-module"), TEXT("generation"),
			TEXT("module"), TEXT("function"));
		Provider.OwnerModuleName = TEXT("DefinitelyNotALoadedUEModule");
		Provider.RefreshViewPointersAndDigest();
		const int Owner = 1;

		ASSERT_THAT(IsTrue(Registry.RegisterProvider(&Owner, Provider).Code
			== EAngelscriptJITProviderRegistrationCode::CodeLifetimeUnavailable));
		ASSERT_THAT(AreEqual(0, Registry.GetSnapshot()->Providers.Num()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
