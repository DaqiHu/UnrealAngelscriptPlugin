#include "StaticJIT/AngelscriptJITProvider.h"

#include "CQTest.h"

#include <type_traits>

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptJITProviderAbiTests_Private
{
	static FAngelscriptHash256 MakeHash(const TCHAR* Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(TEXT("jit-provider-abi-test"));
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	static void VmEntry(FScriptExecution&, asDWORD*, asQWORD*)
	{
	}

	static void RawEntry()
	{
	}

	static void ParmsEntry(FScriptExecution&, void*, void*)
	{
	}

	struct FProviderFixture
	{
		TArray<FAngelscriptJITReferenceSlot> References;
		TArray<FAngelscriptJITArtifactEntry> Entries;
		FAngelscriptJITProviderView View;

		FProviderFixture()
		{
			References.SetNum(2);
			References[0].SlotIndex = 0;
			References[0].Reference.Kind =
				EAngelscriptArtifactReferenceKind::ScriptFunction;
			References[0].Reference.StableKey = MakeHash(TEXT("callee"));
			References[0].Reference.ExpectedAbi = MakeHash(TEXT("callee-abi"));
			References[1].SlotIndex = 1;
			References[1].Flags = EAngelscriptJITReferenceSlotFlags::Optional;
			References[1].Reference.Kind =
				EAngelscriptArtifactReferenceKind::StringLiteral;
			References[1].Reference.StableKey = MakeHash(TEXT("literal"));
			References[1].Reference.ExpectedAbi = MakeHash(TEXT("literal-abi"));

			Entries.SetNum(2);
			InitializeEntry(Entries[0], TEXT("module-a"), TEXT("function-a"));
			InitializeEntry(Entries[1], TEXT("module-b"), TEXT("function-b"));
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
			Entries[0].ReferenceSlots = References.GetData();
			Entries[0].ReferenceSlotCount = References.Num();

			View.ProviderId.Hash = MakeHash(TEXT("provider"));
			View.ProviderName = TEXT("Synthetic Provider");
			View.OwnerModuleName = TEXT("SyntheticModule");
			View.ArtifactProfile.Hash = MakeHash(TEXT("profile"));
			View.NativeEnvironmentFingerprint = MakeHash(TEXT("environment"));
			View.Entries = Entries.GetData();
			View.EntryCount = Entries.Num();
			for (FAngelscriptJITArtifactEntry& Entry : Entries)
			{
				Entry.ArtifactProfile = View.ArtifactProfile;
				Entry.NativeEnvironmentFingerprint =
					View.NativeEnvironmentFingerprint;
			}
			View.ArtifactSetDigest =
				FAngelscriptJITProviderManifest::BuildArtifactSetDigest(Entries);
			View.ProviderGeneration =
				FAngelscriptJITProviderManifest::BuildProviderGeneration(View);
		}

		static void InitializeEntry(
			FAngelscriptJITArtifactEntry& Entry,
			const TCHAR* ModuleSeed,
			const TCHAR* FunctionSeed)
		{
			Entry.ModuleKey.Hash = MakeHash(ModuleSeed);
			Entry.FunctionKey.Hash = MakeHash(FunctionSeed);
			const FString ExecutionSeed =
				FString::Printf(TEXT("%s-execution"), FunctionSeed);
			const FString DebugSeed =
				FString::Printf(TEXT("%s-debug"), FunctionSeed);
			const FString AbiSeed =
				FString::Printf(TEXT("%s-abi"), FunctionSeed);
			Entry.ExecutionHash = MakeHash(*ExecutionSeed);
			Entry.DebugHash = MakeHash(*DebugSeed);
			Entry.EntryAbiHash = MakeHash(*AbiSeed);
			Entry.EntryPoints.VMEntry = &VmEntry;
			Entry.EntryPoints.RawEntry = &RawEntry;
			Entry.EntryPoints.ParmsEntry = &ParmsEntry;
		}
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptJITProviderAbiTests,
	"Angelscript.TestModule.StaticJIT.ProviderAbi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static_assert(std::is_standard_layout_v<FAngelscriptJITProviderId>);
	static_assert(std::is_standard_layout_v<FAngelscriptJITProviderGeneration>);
	static_assert(std::is_standard_layout_v<FAngelscriptJITEntryPoints>);
	static_assert(std::is_standard_layout_v<FAngelscriptJITReferenceSlot>);
	static_assert(std::is_standard_layout_v<FAngelscriptJITArtifactEntry>);
	static_assert(std::is_standard_layout_v<FAngelscriptJITProviderView>);
	static_assert(sizeof(FAngelscriptJITProviderId) == 32);
	static_assert(sizeof(FAngelscriptJITProviderGeneration) == 32);
	static_assert(sizeof(FAngelscriptJITEntryPoints) == sizeof(void*) * 3);

public:
	TEST_METHOD(ProviderAbiRevisionIsCurrent)
	{
		using namespace AngelscriptJITProviderAbiTests_Private;
		FProviderFixture Fixture;

		ASSERT_THAT(AreEqual(2u, FAngelscriptJITProviderAbi::Revision));
		ASSERT_THAT(IsTrue(FAngelscriptJITProviderManifest::Validate(Fixture.View).IsValid()));
	}

	TEST_METHOD(CurrentViewWithSeveralScriptModulesValidates)
	{
		using namespace AngelscriptJITProviderAbiTests_Private;
		FProviderFixture Fixture;
		const FAngelscriptJITProviderValidationResult Result =
			FAngelscriptJITProviderManifest::Validate(Fixture.View);
		ASSERT_THAT(IsTrue(Result.IsValid()));
		ASSERT_THAT(AreEqual(2, Fixture.View.EntryCount));
		ASSERT_THAT(IsTrue(Fixture.Entries[0].ModuleKey
			!= Fixture.Entries[1].ModuleKey));
	}

	TEST_METHOD(RejectsInvalidViewRevisionSizeCountAndPointers)
	{
		using namespace AngelscriptJITProviderAbiTests_Private;
		FProviderFixture Fixture;

		FAngelscriptJITProviderView Invalid = Fixture.View;
		Invalid.StructSize -= 1;
		ASSERT_THAT(IsTrue(FAngelscriptJITProviderManifest::Validate(Invalid).Code
			== EAngelscriptJITProviderValidationCode::InvalidViewSize));

		Invalid = Fixture.View;
		Invalid.AbiRevision += 1;
		ASSERT_THAT(IsTrue(FAngelscriptJITProviderManifest::Validate(Invalid).Code
			== EAngelscriptJITProviderValidationCode::UnsupportedAbiRevision));

		Invalid = Fixture.View;
		Invalid.EntryCount = FAngelscriptJITProviderAbi::MaximumEntryCount + 1;
		ASSERT_THAT(IsTrue(FAngelscriptJITProviderManifest::Validate(Invalid).Code
			== EAngelscriptJITProviderValidationCode::InvalidEntryCount));

		Invalid = Fixture.View;
		Invalid.Entries = nullptr;
		ASSERT_THAT(IsTrue(FAngelscriptJITProviderManifest::Validate(Invalid).Code
			== EAngelscriptJITProviderValidationCode::MissingEntryTable));
	}

	TEST_METHOD(RejectsMalformedEntriesReferencesAndOrder)
	{
		using namespace AngelscriptJITProviderAbiTests_Private;
		FProviderFixture Fixture;

		Fixture.Entries[0].StructSize -= 1;
		ASSERT_THAT(IsTrue(FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
			== EAngelscriptJITProviderValidationCode::InvalidEntrySize));
		Fixture.Entries[0].StructSize = sizeof(FAngelscriptJITArtifactEntry);

		Swap(Fixture.Entries[0], Fixture.Entries[1]);
		ASSERT_THAT(IsTrue(FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
			== EAngelscriptJITProviderValidationCode::UnsortedEntries));
		Swap(Fixture.Entries[0], Fixture.Entries[1]);

		Fixture.Entries[0].ReferenceSlots = nullptr;
		ASSERT_THAT(IsTrue(FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
			== EAngelscriptJITProviderValidationCode::MissingReferenceTable));
		Fixture.Entries[0].ReferenceSlots = Fixture.References.GetData();

		Fixture.References[1].SlotIndex = 7;
		ASSERT_THAT(IsTrue(FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
			== EAngelscriptJITProviderValidationCode::InvalidReferenceOrder));
	}

	TEST_METHOD(RejectsIncompleteEntryAndInconsistentArtifactSet)
	{
		using namespace AngelscriptJITProviderAbiTests_Private;
		FProviderFixture Fixture;

		Fixture.Entries[0].EntryPoints = {};
		ASSERT_THAT(IsTrue(FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
			== EAngelscriptJITProviderValidationCode::MissingEntryPoints));

		Fixture.Entries[0].EntryPoints.VMEntry = &VmEntry;
		Fixture.View.ArtifactSetDigest = MakeHash(TEXT("wrong-artifact-set"));
		ASSERT_THAT(IsTrue(FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
			== EAngelscriptJITProviderValidationCode::ArtifactSetDigestMismatch));
	}

	TEST_METHOD(RejectsProviderGenerationThatIsNotDerivedFromManifestContent)
	{
		using namespace AngelscriptJITProviderAbiTests_Private;
		FProviderFixture Fixture;
		Fixture.View.ProviderGeneration.Hash = MakeHash(TEXT("invented-generation"));
		ASSERT_THAT(IsTrue(FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
			== EAngelscriptJITProviderValidationCode::ProviderGenerationDigestMismatch));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
