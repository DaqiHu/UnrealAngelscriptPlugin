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
		TArray<FAngelscriptJITNativeCallSite> NativeCallSites;
		TArray<FAngelscriptJITArtifactEntry> Entries;
		FAngelscriptJITProviderView View;

		FProviderFixture()
		{
			References.SetNum(2);
			References[0].SlotIndex = 0;
			References[0].Reference.Kind =
				EAngelscriptArtifactReferenceKind::EnvironmentSymbol;
			References[0].Reference.StableKey = MakeHash(TEXT("callee"));
			References[0].Reference.ExpectedAbi = MakeHash(TEXT("callee-abi"));
			References[1].SlotIndex = 1;
			References[1].Flags = EAngelscriptJITReferenceSlotFlags::Optional;
			References[1].Reference.Kind =
				EAngelscriptArtifactReferenceKind::StringLiteral;
			References[1].Reference.StableKey = MakeHash(TEXT("literal"));
			References[1].Reference.ExpectedAbi = MakeHash(TEXT("literal-abi"));

			NativeCallSites.SetNum(2);
			NativeCallSites[0].CallSiteIndex = 0;
			NativeCallSites[0].Route =
				EAngelscriptJITNativeCallSiteRoute::CurrentNativeBinding;
			NativeCallSites[0].CanonicalDeclaration =
				TEXT("int DaysInMonth(int, int)");
			NativeCallSites[0].EmittedCppCallee =
				TEXT("AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32, int32>");
			NativeCallSites[0].RuntimeDllCore =
				TEXT("AngelscriptTypedASTJIT::ResolveBoundNative");
			NativeCallSites[0].MetadataRowSymbol =
				TEXT("ASJIT_Call_DaysInMonth_deadbeef");
			NativeCallSites[0].RegisteredTarget =
				TEXT("FDateTime::DaysInMonth");
			NativeCallSites[0].StableTargetKey =
				References[0].Reference.StableKey;
			NativeCallSites[0].ExpectedAbi =
				References[0].Reference.ExpectedAbi;
			NativeCallSites[0].ReferenceSlotIndex = 0;
			NativeCallSites[1].CallSiteIndex = 1;
			NativeCallSites[1].Route =
				EAngelscriptJITNativeCallSiteRoute::Bridge;
			NativeCallSites[1].CanonicalDeclaration =
				TEXT("int ProviderPrivateAdd(int, int)");
			NativeCallSites[1].EmittedCppCallee = TEXT(
				"AngelscriptTypedASTJIT::InvokeBound<int32, int32, int32>");
			NativeCallSites[1].RuntimeDllCore =
				TEXT("AngelscriptTypedASTJIT::InvokeBoundViaVM");
			NativeCallSites[1].MetadataRowSymbol =
				TEXT("ASJIT_Call_ProviderPrivateAdd_deadbeef");
			NativeCallSites[1].RegisteredTarget =
				TEXT("FPrivateProvider::Add");
			NativeCallSites[1].StableTargetKey =
				References[0].Reference.StableKey;
			NativeCallSites[1].ExpectedAbi =
				References[0].Reference.ExpectedAbi;
			NativeCallSites[1].ReferenceSlotIndex = 0;

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
			Entries[0].NativeCallSites = NativeCallSites.GetData();
			Entries[0].NativeCallSiteCount = NativeCallSites.Num();

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
			RebuildIdentity();
		}

		void RebuildIdentity()
		{
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
	static_assert(std::is_standard_layout_v<FAngelscriptJITNativeCallSite>);
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

		ASSERT_THAT(AreEqual(8u, FAngelscriptJITProviderAbi::Revision));
		ASSERT_THAT(IsTrue(FAngelscriptJITProviderManifest::Validate(Fixture.View).IsValid()));
	}

	TEST_METHOD(ExecutionCapabilityProfileParticipatesInArtifactIdentity)
	{
		using namespace AngelscriptJITProviderAbiTests_Private;
		FProviderFixture Fixture;
		const FAngelscriptHash256 UnprofiledDigest =
			Fixture.View.ArtifactSetDigest;

		FAngelscriptJITArtifactEntry& Entry = Fixture.Entries[0];
		Entry.Flags |=
			EAngelscriptJITArtifactEntryFlags::HasExecutionCapabilityProfile;
		Entry.AvailableExecutionCapabilities =
			EAngelscriptJITExecutionCapability::FramePosition
			| EAngelscriptJITExecutionCapability::RecursionBudget;
		Entry.ExecutionCapabilityProfileHash =
			MakeHash(TEXT("position-recursion-profile"));
		Fixture.RebuildIdentity();

		ASSERT_THAT(IsTrue(
			FAngelscriptJITProviderManifest::Validate(Fixture.View).IsValid()));
		ASSERT_THAT(IsTrue(Fixture.View.ArtifactSetDigest != UnprofiledDigest));
		const FAngelscriptHash256 FirstProfileDigest =
			Fixture.View.ArtifactSetDigest;

		Entry.ExecutionCapabilityProfileHash =
			MakeHash(TEXT("different-position-recursion-profile"));
		Fixture.RebuildIdentity();
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProviderManifest::Validate(Fixture.View).IsValid()));
		ASSERT_THAT(IsTrue(
			Fixture.View.ArtifactSetDigest != FirstProfileDigest));
		const FAngelscriptHash256 SecondProfileDigest =
			Fixture.View.ArtifactSetDigest;

		Entry.RuntimeExecutionRequirementTriggers =
			EAngelscriptJITExecutionCapability::LoopTimeout;
		Fixture.RebuildIdentity();
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProviderManifest::Validate(Fixture.View).IsValid()));
		ASSERT_THAT(IsTrue(
			Fixture.View.ArtifactSetDigest != SecondProfileDigest));
	}

	TEST_METHOD(RejectsMalformedExecutionCapabilityProfiles)
	{
		using namespace AngelscriptJITProviderAbiTests_Private;

		{
			FProviderFixture Fixture;
			Fixture.Entries[0].Flags |=
				EAngelscriptJITArtifactEntryFlags::HasExecutionCapabilityProfile;
			Fixture.Entries[0].AvailableExecutionCapabilities =
				EAngelscriptJITExecutionCapability::FramePosition;
			ASSERT_THAT(IsTrue(
				FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
				== EAngelscriptJITProviderValidationCode::InvalidExecutionCapabilityProfile));
		}

		{
			FProviderFixture Fixture;
			Fixture.Entries[0].AvailableExecutionCapabilities =
				EAngelscriptJITExecutionCapability::FramePosition;
			ASSERT_THAT(IsTrue(
				FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
				== EAngelscriptJITProviderValidationCode::InvalidExecutionCapabilities));
		}

		{
			FProviderFixture Fixture;
			Fixture.Entries[0].ExecutionCapabilityProfileHash =
				MakeHash(TEXT("profile-without-flag"));
			ASSERT_THAT(IsTrue(
				FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
				== EAngelscriptJITProviderValidationCode::InvalidExecutionCapabilityProfile));
		}

		{
			FProviderFixture Fixture;
			Fixture.Entries[0].Flags |=
				EAngelscriptJITArtifactEntryFlags::HasExecutionCapabilityProfile;
			Fixture.Entries[0].AvailableExecutionCapabilities =
				static_cast<EAngelscriptJITExecutionCapability>(1u << 31);
			Fixture.Entries[0].ExecutionCapabilityProfileHash =
				MakeHash(TEXT("unknown-capability-profile"));
			ASSERT_THAT(IsTrue(
				FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
				== EAngelscriptJITProviderValidationCode::InvalidExecutionCapabilities));
		}

		{
			FProviderFixture Fixture;
			Fixture.Entries[0].RuntimeExecutionRequirementTriggers =
				EAngelscriptJITExecutionCapability::LoopTimeout;
			ASSERT_THAT(IsTrue(
				FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
					== EAngelscriptJITProviderValidationCode::
						InvalidExecutionRequirementTriggers));
		}

		{
			FProviderFixture Fixture;
			Fixture.Entries[0].Flags |=
				EAngelscriptJITArtifactEntryFlags::HasExecutionCapabilityProfile;
			Fixture.Entries[0].ExecutionCapabilityProfileHash =
				MakeHash(TEXT("trigger-profile"));
			Fixture.Entries[0].RuntimeExecutionRequirementTriggers =
				static_cast<EAngelscriptJITExecutionCapability>(1u << 31);
			ASSERT_THAT(IsTrue(
				FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
					== EAngelscriptJITProviderValidationCode::
						InvalidExecutionRequirementTriggers));
		}
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
		Fixture.References[1].SlotIndex = 1;

		Fixture.NativeCallSites[0].CallSiteIndex = 7;
		ASSERT_THAT(IsTrue(FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
			== EAngelscriptJITProviderValidationCode::InvalidNativeCallSiteOrder));
		Fixture.NativeCallSites[0].CallSiteIndex = 0;

		Fixture.NativeCallSites[0].CanonicalDeclaration = nullptr;
		ASSERT_THAT(IsTrue(FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
			== EAngelscriptJITProviderValidationCode::MissingNativeCallSiteDeclaration));
		Fixture.NativeCallSites[0].CanonicalDeclaration =
			TEXT("int DaysInMonth(int, int)");

		Fixture.NativeCallSites[1].ReferenceSlotIndex = 7;
		ASSERT_THAT(IsTrue(FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
			== EAngelscriptJITProviderValidationCode::InvalidBridgeNativeCallSite));
		Fixture.NativeCallSites[1].ReferenceSlotIndex = 0;

		Fixture.NativeCallSites[0].RuntimeDllCore =
			TEXT("AngelscriptTypedASTJIT::InvokeBoundViaVM");
		ASSERT_THAT(IsTrue(FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
			== EAngelscriptJITProviderValidationCode::InvalidCurrentNativeBindingCallSite));
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
