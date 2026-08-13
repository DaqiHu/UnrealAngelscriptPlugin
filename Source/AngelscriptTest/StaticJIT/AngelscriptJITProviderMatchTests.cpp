#include "StaticJIT/AngelscriptJITProviderMatcher.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptJITProviderMatchTests_Private
{
	static FAngelscriptHash256 MakeHash(const TCHAR* Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("jit-provider-match-test"));
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	static void VmEntry(FScriptExecution&, asDWORD*, asQWORD*)
	{
	}

	class FResolver : public IAngelscriptJITReferenceResolver
	{
	public:
		EAngelscriptJITReferenceResolutionCode Code =
			EAngelscriptJITReferenceResolutionCode::Exact;

		FAngelscriptJITReferenceResolution Resolve(
			const FAngelscriptArtifactReference&) const override
		{
			return {Code,
				Code == EAngelscriptJITReferenceResolutionCode::Exact
					? this : nullptr};
		}
	};

	struct FFixture
	{
		TSharedPtr<FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe> Catalog;
		TSharedPtr<FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> Snapshot;
		FAngelscriptJITFunctionMatchInput Input;
		FResolver Resolver;

		FFixture()
		{
			Catalog = MakeShared<FAngelscriptJITProviderCatalog,
				ESPMode::ThreadSafe>();
			Catalog->ProviderId.Hash = MakeHash(TEXT("provider-a"));
			Catalog->ProviderGeneration.Hash = MakeHash(TEXT("generation-a"));
			Catalog->ArtifactSetDigest = MakeHash(TEXT("artifact-set"));
			Catalog->ArtifactProfile.Hash = MakeHash(TEXT("profile"));
			Catalog->NativeEnvironmentFingerprint = MakeHash(TEXT("environment"));
			Catalog->ProviderName = TEXT("Provider A");
			Catalog->OwnerModuleName = TEXT("ProviderAModule");

			FAngelscriptJITProviderCatalogEntry& Entry =
				Catalog->Entries.AddDefaulted_GetRef();
			Entry.ModuleKey.Hash = MakeHash(TEXT("module"));
			Entry.FunctionKey.Hash = MakeHash(TEXT("function"));
			Entry.ExecutionHash = MakeHash(TEXT("execution"));
			Entry.DebugHash = MakeHash(TEXT("debug"));
			Entry.ArtifactProfile = Catalog->ArtifactProfile;
			Entry.EntryAbiHash = MakeHash(TEXT("entry-abi"));
			Entry.NativeEnvironmentFingerprint =
				Catalog->NativeEnvironmentFingerprint;
			Entry.EntryPoints.VMEntry = &VmEntry;

			Snapshot = MakeShared<FAngelscriptJITProviderRegistrySnapshot,
				ESPMode::ThreadSafe>();
			Snapshot->PublicationOrdinal = 1;
			Snapshot->Providers.Add(Catalog);

			Input.ModuleKey = Entry.ModuleKey;
			Input.Identity.FunctionKey = Entry.FunctionKey;
			Input.Identity.Content.Execution = Entry.ExecutionHash;
			Input.Identity.Content.Debug = Entry.DebugHash;
			Input.Identity.Profile = Entry.ArtifactProfile;
			Input.EntryAbiHash = Entry.EntryAbiHash;
			Input.NativeEnvironmentFingerprint =
				Entry.NativeEnvironmentFingerprint;
			Input.CurrentArtifactSetDigest = Catalog->ArtifactSetDigest;
			Input.RequiredEntryPoints =
				EAngelscriptJITRequiredEntryPoints::VM;
		}

		FAngelscriptJITFunctionMatchResult Match() const
		{
			return FAngelscriptJITProviderMatcher::Match(
				*Snapshot, Input, Resolver);
		}

		FAngelscriptJITProviderCatalogEntry& Entry()
		{
			return Catalog->Entries[0];
		}
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptJITProviderMatchTests,
	"Angelscript.TestModule.StaticJIT.ProviderMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ExactMatchAndDebugPolicyAreTyped)
	{
		using namespace AngelscriptJITProviderMatchTests_Private;
		FFixture Fixture;
		FAngelscriptJITFunctionMatchResult Result = Fixture.Match();
		ASSERT_THAT(IsTrue(Result.Result
			== EAngelscriptArtifactMatchResult::Exact));
		ASSERT_THAT(IsTrue(Result.Selection.IsSet()));

		Fixture.Input.Identity.Content.Debug = MakeHash(TEXT("new-debug"));
		Result = Fixture.Match();
		ASSERT_THAT(IsTrue(Result.Result
			== EAngelscriptArtifactMatchResult::Exact));
		Fixture.Entry().Flags =
			EAngelscriptJITArtifactEntryFlags::RequireExactDebugIdentity;
		Result = Fixture.Match();
		ASSERT_THAT(IsTrue(Result.Result
			== EAngelscriptArtifactMatchResult::DebugMismatch));
	}

	TEST_METHOD(ContentProfileEnvironmentAbiAndEntryPointMismatchesAreDistinct)
	{
		using namespace AngelscriptJITProviderMatchTests_Private;
		FFixture Fixture;

		Fixture.Input.Identity.Content.Execution = MakeHash(TEXT("other-execution"));
		ASSERT_THAT(IsTrue(Fixture.Match().Result
			== EAngelscriptArtifactMatchResult::ContentMismatch));
		Fixture.Input.Identity.Content.Execution = Fixture.Entry().ExecutionHash;

		Fixture.Input.Identity.Profile.Hash = MakeHash(TEXT("other-profile"));
		ASSERT_THAT(IsTrue(Fixture.Match().Result
			== EAngelscriptArtifactMatchResult::ProfileMismatch));
		Fixture.Input.Identity.Profile = Fixture.Entry().ArtifactProfile;

		Fixture.Input.NativeEnvironmentFingerprint =
			MakeHash(TEXT("other-environment"));
		ASSERT_THAT(IsTrue(Fixture.Match().Result
			== EAngelscriptArtifactMatchResult::EnvironmentMismatch));
		Fixture.Input.NativeEnvironmentFingerprint =
			Fixture.Entry().NativeEnvironmentFingerprint;

		Fixture.Input.EntryAbiHash = MakeHash(TEXT("other-abi"));
		ASSERT_THAT(IsTrue(Fixture.Match().Result
			== EAngelscriptArtifactMatchResult::EntryAbiMismatch));
		Fixture.Input.EntryAbiHash = Fixture.Entry().EntryAbiHash;

		Fixture.Input.RequiredEntryPoints =
			EAngelscriptJITRequiredEntryPoints::Raw;
		ASSERT_THAT(IsTrue(Fixture.Match().Result
			== EAngelscriptArtifactMatchResult::MissingEntryPoint));
	}

	TEST_METHOD(ReferenceFailuresAreIsolatedAndOptionalMissingIsExplicit)
	{
		using namespace AngelscriptJITProviderMatchTests_Private;
		FFixture Fixture;
		FAngelscriptJITReferenceSlot Slot;
		Slot.Reference.Kind = EAngelscriptArtifactReferenceKind::ScriptFunction;
		Slot.Reference.StableKey = MakeHash(TEXT("callee"));
		Slot.Reference.ExpectedAbi = MakeHash(TEXT("callee-abi"));
		Fixture.Entry().ReferenceSlots.Add(Slot);

		Fixture.Resolver.Code = EAngelscriptJITReferenceResolutionCode::Missing;
		ASSERT_THAT(IsTrue(Fixture.Match().Result
			== EAngelscriptArtifactMatchResult::MissingReference));
		Fixture.Resolver.Code = EAngelscriptJITReferenceResolutionCode::Ambiguous;
		ASSERT_THAT(IsTrue(Fixture.Match().Result
			== EAngelscriptArtifactMatchResult::AmbiguousReference));
		Fixture.Resolver.Code = EAngelscriptJITReferenceResolutionCode::WrongKind;
		ASSERT_THAT(IsTrue(Fixture.Match().Result
			== EAngelscriptArtifactMatchResult::WrongReferenceKind));
		Fixture.Resolver.Code = EAngelscriptJITReferenceResolutionCode::AbiMismatch;
		ASSERT_THAT(IsTrue(Fixture.Match().Result
			== EAngelscriptArtifactMatchResult::ReferenceAbiMismatch));

		Fixture.Resolver.Code = EAngelscriptJITReferenceResolutionCode::Missing;
		Fixture.Entry().ReferenceSlots[0].Flags =
			EAngelscriptJITReferenceSlotFlags::Optional;
		const FAngelscriptJITFunctionMatchResult Optional = Fixture.Match();
		ASSERT_THAT(IsTrue(Optional.Result
			== EAngelscriptArtifactMatchResult::Exact));
		ASSERT_THAT(IsNull(Optional.Selection.GetValue()
			.ReferenceTable->Slots[0].Value));
	}

	TEST_METHOD(DuplicateAndCrossProviderExactCandidatesFailClosed)
	{
		using namespace AngelscriptJITProviderMatchTests_Private;
		FFixture Fixture;
		const FAngelscriptJITProviderCatalogEntry DuplicateEntry = Fixture.Entry();
		Fixture.Catalog->Entries.Add(DuplicateEntry);
		ASSERT_THAT(IsTrue(Fixture.Match().Result
			== EAngelscriptArtifactMatchResult::DuplicateProviderEntry));

		Fixture.Catalog->Entries.SetNum(1);
		TSharedPtr<FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe> Other =
			MakeShared<FAngelscriptJITProviderCatalog,
				ESPMode::ThreadSafe>(*Fixture.Catalog);
		Other->ProviderId.Hash = MakeHash(TEXT("provider-b"));
		Other->ProviderGeneration.Hash = MakeHash(TEXT("generation-b"));
		Fixture.Snapshot->Providers.Add(Other);
		const FAngelscriptJITFunctionMatchResult Ambiguous = Fixture.Match();
		ASSERT_THAT(IsTrue(Ambiguous.Result
			== EAngelscriptArtifactMatchResult::AmbiguousExactProvider));
		ASSERT_THAT(IsFalse(Ambiguous.Selection.IsSet()));
		ASSERT_THAT(AreEqual(2, Ambiguous.Candidates.Num()));
	}

	TEST_METHOD(CompleteArtifactSetAndStableModuleFunctionAreRequired)
	{
		using namespace AngelscriptJITProviderMatchTests_Private;
		FFixture Fixture;
		Fixture.Entry().Flags =
			EAngelscriptJITArtifactEntryFlags::ImmutableDirectCallSet;
		Fixture.Input.CurrentArtifactSetDigest = MakeHash(TEXT("stale-set"));
		ASSERT_THAT(IsTrue(Fixture.Match().Result
			== EAngelscriptArtifactMatchResult::ArtifactSetMismatch));

		Fixture.Input.CurrentArtifactSetDigest = Fixture.Catalog->ArtifactSetDigest;
		Fixture.Input.ModuleKey.Hash = MakeHash(TEXT("other-module"));
		ASSERT_THAT(IsTrue(Fixture.Match().Result
			== EAngelscriptArtifactMatchResult::MissingProviderEntry));
		Fixture.Input.ModuleKey = Fixture.Entry().ModuleKey;
		Fixture.Input.Identity.FunctionKey.Hash = MakeHash(TEXT("other-function"));
		ASSERT_THAT(IsTrue(Fixture.Match().Result
			== EAngelscriptArtifactMatchResult::MissingProviderEntry));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
