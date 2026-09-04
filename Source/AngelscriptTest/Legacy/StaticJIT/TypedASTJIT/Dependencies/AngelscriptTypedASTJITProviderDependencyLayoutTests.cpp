#include "CQTest.h"

#include "StaticJIT/AngelscriptJITProvider.h"

#include <type_traits>

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptTypedASTJITProviderDependencyLayoutTests_Private
{
	static FAngelscriptHash256 MakeHash(const TCHAR* Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("typed-ast-provider-dependency-layout"));
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	static void VmEntry(FScriptExecution&, asDWORD*, asQWORD*)
	{
	}

	static FAngelscriptArtifactReference MakeReference(
		const TCHAR* Key,
		const TCHAR* Abi,
		const EAngelscriptArtifactReferenceKind Kind =
			EAngelscriptArtifactReferenceKind::ScriptFunction)
	{
		FAngelscriptArtifactReference Reference;
		Reference.Kind = Kind;
		Reference.StableKey = MakeHash(Key);
		Reference.ExpectedAbi = MakeHash(Abi);
		return Reference;
	}

	static FAngelscriptArtifactSemanticDependency MakeDependency(
		const EAngelscriptArtifactSemanticDependencyKind Kind,
		const FAngelscriptArtifactReference& Target,
		const TOptional<FAngelscriptHash256>& Expected = {})
	{
		FAngelscriptArtifactSemanticDependency Dependency;
		Dependency.Kind = Kind;
		Dependency.Target = Target;
		Dependency.ExpectedContentOrValue = Expected;
		return Dependency;
	}

	struct FProviderFixture
	{
		TArray<FAngelscriptJITSemanticDependencyRow> Dependencies;
		TArray<FAngelscriptJITArtifactEntry> Entries;
		FAngelscriptJITProviderView View;

		FProviderFixture()
		{
			Entries.SetNum(1);
			FAngelscriptJITArtifactEntry& Entry = Entries[0];
			Entry.ModuleKey.Hash = MakeHash(TEXT("module"));
			Entry.FunctionKey.Hash = MakeHash(TEXT("root"));
			Entry.ExecutionHash = MakeHash(TEXT("root-execution"));
			Entry.DebugHash = MakeHash(TEXT("root-debug"));
			Entry.EntryAbiHash = MakeHash(TEXT("root-abi"));
			Entry.EntryPoints.VMEntry = &VmEntry;
			View.ProviderId.Hash = MakeHash(TEXT("provider"));
			View.ProviderName = TEXT("Dependency Layout Provider");
			View.OwnerModuleName = TEXT("LayoutModule");
			View.ArtifactProfile.Hash = MakeHash(TEXT("profile"));
			View.NativeEnvironmentFingerprint = MakeHash(TEXT("environment"));
			View.Entries = Entries.GetData();
			View.EntryCount = Entries.Num();
			Entry.ArtifactProfile = View.ArtifactProfile;
			Entry.NativeEnvironmentFingerprint =
				View.NativeEnvironmentFingerprint;
			RebuildIdentity();
		}

		void SetDependencies(
			const TArray<FAngelscriptArtifactSemanticDependency>& Rows)
		{
			Dependencies.Reset();
			for (int32 Index = 0; Index < Rows.Num(); ++Index)
			{
				FAngelscriptJITSemanticDependencyRow Row;
				FAngelscriptJITProviderManifest::FillSemanticDependencyRow(
					Rows[Index],
					static_cast<uint32>(Index),
					Row);
				Dependencies.Add(Row);
			}
			Entries[0].SemanticDependencyStartIndex = 0;
			Entries[0].SemanticDependencyCount =
				static_cast<uint32>(Dependencies.Num());
			View.SemanticDependencies = Dependencies.Num() > 0
				? Dependencies.GetData() : nullptr;
			View.SemanticDependencyCount =
				static_cast<uint32>(Dependencies.Num());
			RebuildIdentity();
		}

		void RebuildIdentity()
		{
			View.ArtifactSetDigest =
				FAngelscriptJITProviderManifest::BuildArtifactSetDigest(
					Entries,
					MakeArrayView(Dependencies));
			View.ProviderGeneration =
				FAngelscriptJITProviderManifest::BuildProviderGeneration(View);
		}
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITProviderDependencyLayoutTests,
	"Angelscript.TestModule.StaticJIT.TypedASTJIT.Dependencies.ProviderLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static_assert(std::is_standard_layout_v<
		FAngelscriptJITSemanticDependencyRow>);

public:
	TEST_METHOD(ProviderAbiRevisionRequiresSemanticDependencyTable)
	{
		using namespace AngelscriptTypedASTJITProviderDependencyLayoutTests_Private;
		FProviderFixture Fixture;

		ASSERT_THAT(AreEqual(8u, FAngelscriptJITProviderAbi::Revision));
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProviderManifest::Validate(Fixture.View).IsValid()));
		ASSERT_THAT(AreEqual(0u, Fixture.View.SemanticDependencyCount));
		ASSERT_THAT(AreEqual(0u,
			Fixture.Entries[0].SemanticDependencyCount));
	}

	TEST_METHOD(RejectsOldAbiRevisionAndRowOrViewSize)
	{
		using namespace AngelscriptTypedASTJITProviderDependencyLayoutTests_Private;
		FProviderFixture Fixture;

		FAngelscriptJITProviderView OldRevision = Fixture.View;
		OldRevision.AbiRevision = 7;
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProviderManifest::Validate(OldRevision).Code
				== EAngelscriptJITProviderValidationCode::
					UnsupportedAbiRevision));

		FAngelscriptJITProviderView InvalidView = Fixture.View;
		InvalidView.StructSize -= 1;
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProviderManifest::Validate(InvalidView).Code
				== EAngelscriptJITProviderValidationCode::InvalidViewSize));

		const FAngelscriptArtifactReference Helper =
			MakeReference(TEXT("helper"), TEXT("helper-abi"));
		Fixture.SetDependencies({
			MakeDependency(
				EAngelscriptArtifactSemanticDependencyKind::FunctionContent,
				Helper,
				MakeHash(TEXT("helper-exec"))),
		});
		Fixture.Dependencies[0].StructSize -= 1;
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
				== EAngelscriptJITProviderValidationCode::
					InvalidSemanticDependencySize));
	}

	TEST_METHOD(RejectsBoundsOverflowUnknownKindZeroKeyAndMissingContent)
	{
		using namespace AngelscriptTypedASTJITProviderDependencyLayoutTests_Private;
		const FAngelscriptArtifactReference Helper =
			MakeReference(TEXT("helper"), TEXT("helper-abi"));

		{
			FProviderFixture Fixture;
			Fixture.SetDependencies({
				MakeDependency(
					EAngelscriptArtifactSemanticDependencyKind::FunctionContent,
					Helper,
					MakeHash(TEXT("helper-exec"))),
			});
			Fixture.Entries[0].SemanticDependencyStartIndex = 4;
			Fixture.RebuildIdentity();
			ASSERT_THAT(IsTrue(
				FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
					== EAngelscriptJITProviderValidationCode::
						InvalidSemanticDependencyRange));
		}

		{
			FProviderFixture Fixture;
			Fixture.SetDependencies({
				MakeDependency(
					EAngelscriptArtifactSemanticDependencyKind::FunctionContent,
					Helper,
					MakeHash(TEXT("helper-exec"))),
			});
			Fixture.Dependencies[0].Kind =
				static_cast<EAngelscriptArtifactSemanticDependencyKind>(99);
			Fixture.RebuildIdentity();
			ASSERT_THAT(IsTrue(
				FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
					== EAngelscriptJITProviderValidationCode::
						InvalidSemanticDependencyKind));
		}

		{
			FProviderFixture Fixture;
			Fixture.SetDependencies({
				MakeDependency(
					EAngelscriptArtifactSemanticDependencyKind::FunctionContent,
					Helper,
					MakeHash(TEXT("helper-exec"))),
			});
			Fixture.Dependencies[0].TargetStableKey = {};
			Fixture.RebuildIdentity();
			ASSERT_THAT(IsTrue(
				FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
					== EAngelscriptJITProviderValidationCode::
						InvalidSemanticDependencyKey));
		}

		{
			FProviderFixture Fixture;
			FAngelscriptArtifactSemanticDependency MissingContent =
				MakeDependency(
					EAngelscriptArtifactSemanticDependencyKind::FunctionContent,
					Helper);
			const FAngelscriptJITSemanticDependencyCanonicalResult
				Canonical =
					FAngelscriptJITProviderManifest::
						CanonicalizeSemanticDependencies(
							MakeArrayView(&MissingContent, 1));
			ASSERT_THAT(IsFalse(Canonical.IsValid()));
			ASSERT_THAT(IsTrue(Canonical.Code
				== EAngelscriptJITSemanticDependencyCanonicalCode::
					MissingExpectedContentOrValue));
		}
	}

	TEST_METHOD(RejectsWrongAbiNonCanonicalOrderAndConflictingDuplicates)
	{
		using namespace AngelscriptTypedASTJITProviderDependencyLayoutTests_Private;
		const FAngelscriptArtifactReference Helper =
			MakeReference(TEXT("helper"), TEXT("helper-abi"));

		{
			FProviderFixture Fixture;
			Fixture.SetDependencies({
				MakeDependency(
					EAngelscriptArtifactSemanticDependencyKind::FunctionContent,
					Helper,
					MakeHash(TEXT("helper-exec"))),
			});
			Fixture.Dependencies[0].ExpectedAbi = {};
			Fixture.RebuildIdentity();
			ASSERT_THAT(IsTrue(
				FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
					== EAngelscriptJITProviderValidationCode::
						InvalidSemanticDependencyAbi));
		}

		{
			FProviderFixture Fixture;
			const FAngelscriptArtifactReference First =
				MakeReference(TEXT("first"), TEXT("first-abi"));
			const FAngelscriptArtifactReference Second =
				MakeReference(TEXT("second"), TEXT("second-abi"));
			Fixture.SetDependencies({
				MakeDependency(
					EAngelscriptArtifactSemanticDependencyKind::FunctionContent,
					Second,
					MakeHash(TEXT("second-exec"))),
				MakeDependency(
					EAngelscriptArtifactSemanticDependencyKind::Signature,
					First),
			});
			ASSERT_THAT(IsTrue(
				FAngelscriptJITProviderManifest::Validate(Fixture.View).Code
					== EAngelscriptJITProviderValidationCode::
						InvalidSemanticDependencyOrder));
		}

		{
			const FAngelscriptArtifactSemanticDependency First = MakeDependency(
				EAngelscriptArtifactSemanticDependencyKind::FunctionContent,
				Helper,
				MakeHash(TEXT("helper-exec-a")));
			const FAngelscriptArtifactSemanticDependency Second = MakeDependency(
				EAngelscriptArtifactSemanticDependencyKind::FunctionContent,
				Helper,
				MakeHash(TEXT("helper-exec-b")));
			FAngelscriptArtifactSemanticDependency Rows[] = {First, Second};
			const FAngelscriptJITSemanticDependencyCanonicalResult
				Canonical =
					FAngelscriptJITProviderManifest::
						CanonicalizeSemanticDependencies(
							MakeArrayView(Rows, 2));
			ASSERT_THAT(IsFalse(Canonical.IsValid()));
			ASSERT_THAT(IsTrue(Canonical.Code
				== EAngelscriptJITSemanticDependencyCanonicalCode::
					ConflictingDuplicate));
		}
	}

	TEST_METHOD(CanonicalizeDeduplicatesRepeatedCallsAndFlattensTransitiveContent)
	{
		using namespace AngelscriptTypedASTJITProviderDependencyLayoutTests_Private;
		const FAngelscriptArtifactReference HelperB =
			MakeReference(TEXT("helper-b"), TEXT("helper-b-abi"));
		const FAngelscriptArtifactReference HelperC =
			MakeReference(TEXT("helper-c"), TEXT("helper-c-abi"));
		const FAngelscriptHash256 HelperBContent = MakeHash(TEXT("b-exec"));
		const FAngelscriptHash256 HelperCContent = MakeHash(TEXT("c-exec"));

		const FAngelscriptArtifactSemanticDependency Discovered[] = {
			MakeDependency(
				EAngelscriptArtifactSemanticDependencyKind::FunctionContent,
				HelperC,
				HelperCContent),
			MakeDependency(
				EAngelscriptArtifactSemanticDependencyKind::Signature,
				HelperB),
			MakeDependency(
				EAngelscriptArtifactSemanticDependencyKind::FunctionContent,
				HelperB,
				HelperBContent),
			MakeDependency(
				EAngelscriptArtifactSemanticDependencyKind::FunctionContent,
				HelperB,
				HelperBContent),
			MakeDependency(
				EAngelscriptArtifactSemanticDependencyKind::FunctionContent,
				HelperC,
				HelperCContent),
		};
		const FAngelscriptJITSemanticDependencyCanonicalResult Forward =
			FAngelscriptJITProviderManifest::CanonicalizeSemanticDependencies(
				MakeArrayView(Discovered, UE_ARRAY_COUNT(Discovered)));
		ASSERT_THAT(IsTrue(Forward.IsValid(), TEXT("Forward canonicalize")));
		ASSERT_THAT(AreEqual(3, Forward.Rows.Num()));
		ASSERT_THAT(IsTrue(Forward.Rows[0].Kind
			== EAngelscriptArtifactSemanticDependencyKind::Signature));
		ASSERT_THAT(IsTrue(Forward.Rows[0].Target == HelperB));
		ASSERT_THAT(IsTrue(Forward.Rows.ContainsByPredicate(
			[&HelperB, &HelperBContent](
				const FAngelscriptArtifactSemanticDependency& Row)
			{
				return Row.Kind
						== EAngelscriptArtifactSemanticDependencyKind::
							FunctionContent
					&& Row.Target == HelperB
					&& Row.ExpectedContentOrValue == HelperBContent;
			})));
		ASSERT_THAT(IsTrue(Forward.Rows.ContainsByPredicate(
			[&HelperC, &HelperCContent](
				const FAngelscriptArtifactSemanticDependency& Row)
			{
				return Row.Kind
						== EAngelscriptArtifactSemanticDependencyKind::
							FunctionContent
					&& Row.Target == HelperC
					&& Row.ExpectedContentOrValue == HelperCContent;
			})));

		const FAngelscriptArtifactSemanticDependency Reversed[] = {
			MakeDependency(
				EAngelscriptArtifactSemanticDependencyKind::FunctionContent,
				HelperB,
				HelperBContent),
			MakeDependency(
				EAngelscriptArtifactSemanticDependencyKind::FunctionContent,
				HelperC,
				HelperCContent),
			MakeDependency(
				EAngelscriptArtifactSemanticDependencyKind::Signature,
				HelperB),
		};
		const FAngelscriptJITSemanticDependencyCanonicalResult Backward =
			FAngelscriptJITProviderManifest::CanonicalizeSemanticDependencies(
				MakeArrayView(Reversed, UE_ARRAY_COUNT(Reversed)));
		ASSERT_THAT(IsTrue(Backward.IsValid(), TEXT("Reverse canonicalize")));
		ASSERT_THAT(IsTrue(Forward.Rows == Backward.Rows));

		FProviderFixture First;
		First.SetDependencies(Forward.Rows);
		FProviderFixture Second;
		Second.SetDependencies(Backward.Rows);
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProviderManifest::Validate(First.View).IsValid()));
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProviderManifest::Validate(Second.View).IsValid()));
		ASSERT_THAT(IsTrue(
			First.View.ArtifactSetDigest == Second.View.ArtifactSetDigest));

		FProviderFixture Mutated = First;
		Mutated.Dependencies[2].ExpectedContentOrValue =
			MakeHash(TEXT("c-exec-changed"));
		Mutated.RebuildIdentity();
		ASSERT_THAT(IsTrue(
			First.View.ArtifactSetDigest != Mutated.View.ArtifactSetDigest));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
