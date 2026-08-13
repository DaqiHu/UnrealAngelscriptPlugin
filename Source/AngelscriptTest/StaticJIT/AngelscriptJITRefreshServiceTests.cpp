#include "CQTest.h"

#include "StaticJIT/AngelscriptJITRefreshService.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptJITRefreshServiceTests_Private
{
	using namespace AngelscriptEditor::StaticJIT;

	FAngelscriptHash256 MakeHash(const FStringView Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(TEXT("jit-refresh-test"));
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	FAngelscriptJITGenerationOutput MakeOutput(const FStringView Generation)
	{
		FAngelscriptJITGenerationOutput Output;
		Output.TargetProfile =
			EAngelscriptJITTargetProfile::EditorDevelopment;
		Output.ProviderId.Hash = MakeHash(TEXT("project-provider"));
		Output.ProviderGeneration.Hash = MakeHash(Generation);
		Output.ArtifactSetDigest = MakeHash(
			FString::Printf(TEXT("artifact-%.*s"),
				Generation.Len(), Generation.GetData()));
		Output.ArtifactProfile.Hash = MakeHash(TEXT("editor-profile"));
		Output.NativeEnvironmentFingerprint = MakeHash(TEXT("editor-env"));
		return Output;
	}

	TSharedRef<const FAngelscriptJITProviderCatalog, ESPMode::ThreadSafe>
	MakeCatalog(const FAngelscriptJITGenerationOutput& Output)
	{
		TSharedRef<FAngelscriptJITProviderCatalog, ESPMode::ThreadSafe> Catalog =
			MakeShared<FAngelscriptJITProviderCatalog, ESPMode::ThreadSafe>();
		Catalog->ProviderId = Output.ProviderId;
		Catalog->ProviderGeneration = Output.ProviderGeneration;
		Catalog->ArtifactSetDigest = Output.ArtifactSetDigest;
		Catalog->ArtifactProfile = Output.ArtifactProfile;
		Catalog->NativeEnvironmentFingerprint =
			Output.NativeEnvironmentFingerprint;
		Catalog->ProviderName = TEXT("Project JIT");
		Catalog->OwnerModuleName = TEXT("AngelscriptJIT");
		return Catalog;
	}

	FAngelscriptJITObservedProvider ObserveCatalog(
		const TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe>& Catalog)
	{
		FAngelscriptJITObservedProvider Result;
		if (!Catalog.IsValid())
		{
			Result.Validation.Code =
				EAngelscriptJITProviderValidationCode::MissingProviderView;
			return Result;
		}
		Result.bFound = true;
		Result.Validation.Code = EAngelscriptJITProviderValidationCode::Valid;
		Result.ProviderId = Catalog->ProviderId;
		Result.ProviderGeneration = Catalog->ProviderGeneration;
		Result.ArtifactSetDigest = Catalog->ArtifactSetDigest;
		Result.ArtifactProfile = Catalog->ArtifactProfile;
		Result.NativeEnvironmentFingerprint =
			Catalog->NativeEnvironmentFingerprint;
		return Result;
	}

	class FFakePatchBackend final : public IAngelscriptJITPatchBackend
	{
	public:
		bool bAvailable = true;
		bool bCompiling = false;
		bool bAcceptRequest = true;
		int32 RequestCount = 0;
		FString UnavailableReason = TEXT("Live Coding unavailable");
		TFunction<void(FAngelscriptJITPatchCompletion)> Pending;

		bool IsAvailable(FString& OutReason) const override
		{
			OutReason = bAvailable ? FString() : UnavailableReason;
			return bAvailable;
		}

		bool IsCompileInProgress() const override
		{
			return bCompiling;
		}

		bool RequestCompile(
			TFunction<void(FAngelscriptJITPatchCompletion)> Completion)
			override
		{
			++RequestCount;
			if (!bAcceptRequest)
			{
				return false;
			}
			Pending = MoveTemp(Completion);
			return true;
		}

		void Complete(
			const EAngelscriptJITPatchCompletionCode Code,
			const FStringView Detail = {})
		{
			TFunction<void(FAngelscriptJITPatchCompletion)> Callback =
				MoveTemp(Pending);
			check(Callback);
			Callback({Code, FString(Detail)});
		}
	};

	struct FFixture
	{
		TSharedRef<FFakePatchBackend> Backend = MakeShared<FFakePatchBackend>();
		FAngelscriptJITRefreshPreparation Preparation;
		TSharedPtr<FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> Snapshot =
			MakeShared<FAngelscriptJITProviderRegistrySnapshot,
				ESPMode::ThreadSafe>();
		TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe> PatchedCatalog;
		FAngelscriptJITProviderRegistrationResult Registration;
		FAngelscriptJITProviderRouteRefreshResult RouteRefresh;
		int32 PrepareCount = 0;
		int32 RegisterCount = 0;
		int32 RouteRefreshCount = 0;
		TSharedRef<FAngelscriptJITRefreshService> Service;

		FFixture()
			: Service(MakeService())
		{
		}

		TSharedRef<FAngelscriptJITRefreshService> MakeService()
		{
			Preparation.bSuccess = true;
			Preparation.bHostModuleLoaded = true;
			Preparation.GeneratedOutput = MakeOutput(TEXT("new"));
			Preparation.Publication.bSuccess = true;
			Registration.Code =
				EAngelscriptJITProviderRegistrationCode::Replaced;
			RouteRefresh.Code =
				EAngelscriptJITProviderRouteRefreshCode::Applied;
			RouteRefresh.PublishedNativeCount = 1;

			FAngelscriptJITRefreshDependencies Dependencies;
			Dependencies.Prepare = [this](FStringView)
			{
				++PrepareCount;
				return Preparation;
			};
			Dependencies.GetProviderSnapshot = [this]()
			{
				return StaticCastSharedPtr<
					const FAngelscriptJITProviderRegistrySnapshot>(Snapshot);
			};
			Dependencies.ObserveProvider = [this](
				const FAngelscriptJITProviderId&)
			{
				return ObserveCatalog(PatchedCatalog);
			};
			Dependencies.RefreshProvider = [this](
				const FAngelscriptJITProviderId&)
			{
				++RegisterCount;
				if (PatchedCatalog.IsValid())
				{
					Snapshot->Providers = {PatchedCatalog};
					++Snapshot->PublicationOrdinal;
				}
				return Registration;
			};
			Dependencies.RefreshRoutes = [this]()
			{
				++RouteRefreshCount;
				return RouteRefresh;
			};
			return MakeShared<FAngelscriptJITRefreshService>(
				Backend, MoveTemp(Dependencies));
		}

		void SetPrior(const FStringView Generation = TEXT("old"))
		{
			Snapshot->Providers = {MakeCatalog(MakeOutput(Generation))};
		}

		void SetExpectedPatch()
		{
			PatchedCatalog = MakeCatalog(Preparation.GeneratedOutput);
		}
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptJITRefreshServiceTests,
	"Angelscript.TestModule.StaticJIT.RefreshService",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(StartsIdleAndRejectsOverlappingRequests)
	{
		using namespace AngelscriptJITRefreshServiceTests_Private;
		FFixture Fixture;
		Fixture.SetPrior();
		ASSERT_THAT(IsTrue(Fixture.Service->GetLastResult().Code
			== EAngelscriptJITRefreshCode::Idle));
		ASSERT_THAT(IsTrue(Fixture.Service->Start(TEXT("Project.uproject"))));
		ASSERT_THAT(IsTrue(Fixture.Service->GetLastResult().Code
			== EAngelscriptJITRefreshCode::WaitingForPatch));
		ASSERT_THAT(IsFalse(Fixture.Service->Start(TEXT("Project.uproject"))));
		ASSERT_THAT(IsTrue(Fixture.Service->GetLastResult().Code
			== EAngelscriptJITRefreshCode::AlreadyInProgress));
		ASSERT_THAT(AreEqual(1, Fixture.Backend->RequestCount));
	}

	TEST_METHOD(PreparationAndFullBuildGatesNeverInvokeLiveCoding)
	{
		using namespace AngelscriptJITRefreshServiceTests_Private;
		const TArray<EAngelscriptJITRefreshCode> Failures = {
			EAngelscriptJITRefreshCode::EngineUnavailable,
			EAngelscriptJITRefreshCode::ScriptCompileFailed,
			EAngelscriptJITRefreshCode::ScaffoldRequired,
			EAngelscriptJITRefreshCode::GenerationFailed,
			EAngelscriptJITRefreshCode::PublicationFailed,
		};
		for (const EAngelscriptJITRefreshCode Failure : Failures)
		{
			FFixture Fixture;
			Fixture.Preparation.bSuccess = false;
			Fixture.Preparation.FailureCode = Failure;
			Fixture.Preparation.Detail = TEXT("expected failure");
			ASSERT_THAT(IsFalse(Fixture.Service->Start()));
			ASSERT_THAT(IsTrue(
				Fixture.Service->GetLastResult().Code == Failure));
			ASSERT_THAT(AreEqual(0, Fixture.Backend->RequestCount));
		}

		FFixture MissingBuild;
		MissingBuild.Preparation.bHostModuleLoaded = false;
		ASSERT_THAT(IsFalse(MissingBuild.Service->Start()));
		ASSERT_THAT(IsTrue(MissingBuild.Service->GetLastResult().Code
			== EAngelscriptJITRefreshCode::MissingFullBuild));
		ASSERT_THAT(AreEqual(0, MissingBuild.Backend->RequestCount));

		FFixture SourceSet;
		SourceSet.Preparation.Publication.AddedModuleSources = {
			TEXT("Tests/New.11111111.EditorDevelopment.jit.cpp")};
		SourceSet.Preparation.Publication.RemovedModuleSources = {
			TEXT("Tests/Old.22222222.EditorDevelopment.jit.cpp")};
		ASSERT_THAT(IsFalse(SourceSet.Service->Start()));
		ASSERT_THAT(IsTrue(SourceSet.Service->GetLastResult().Code
			== EAngelscriptJITRefreshCode::RequiresFullBuild));
		ASSERT_THAT(AreEqual(0, SourceSet.Backend->RequestCount));
	}

	TEST_METHOD(LiveCodingAvailabilityAndRequestFailuresAreTyped)
	{
		using namespace AngelscriptJITRefreshServiceTests_Private;
		FFixture Unavailable;
		Unavailable.SetPrior();
		Unavailable.Backend->bAvailable = false;
		ASSERT_THAT(IsFalse(Unavailable.Service->Start()));
		ASSERT_THAT(IsTrue(Unavailable.Service->GetLastResult().Code
			== EAngelscriptJITRefreshCode::LiveCodingUnavailable));

		FFixture Active;
		Active.SetPrior();
		Active.Backend->bCompiling = true;
		ASSERT_THAT(IsFalse(Active.Service->Start()));
		ASSERT_THAT(IsTrue(Active.Service->GetLastResult().Code
			== EAngelscriptJITRefreshCode::LiveCodingCompileInProgress));

		FFixture Rejected;
		Rejected.SetPrior();
		Rejected.Backend->bAcceptRequest = false;
		ASSERT_THAT(IsFalse(Rejected.Service->Start()));
		ASSERT_THAT(IsTrue(Rejected.Service->GetLastResult().Code
			== EAngelscriptJITRefreshCode::LiveCodingRequestFailed));
	}

	TEST_METHOD(PatchFailuresAndProviderValidationRetainRoutes)
	{
		using namespace AngelscriptJITRefreshServiceTests_Private;
		const TArray<TPair<EAngelscriptJITPatchCompletionCode,
			EAngelscriptJITRefreshCode>> PatchFailures = {
			{EAngelscriptJITPatchCompletionCode::NoChanges,
				EAngelscriptJITRefreshCode::LiveCodingNoChanges},
			{EAngelscriptJITPatchCompletionCode::CompileFailed,
				EAngelscriptJITRefreshCode::LiveCodingCompileFailed},
			{EAngelscriptJITPatchCompletionCode::PatchFailed,
				EAngelscriptJITRefreshCode::LiveCodingPatchFailed},
			{EAngelscriptJITPatchCompletionCode::Cancelled,
				EAngelscriptJITRefreshCode::LiveCodingCancelled},
		};
		for (const auto& Pair : PatchFailures)
		{
			FFixture Fixture;
			Fixture.SetPrior();
			ASSERT_THAT(IsTrue(Fixture.Service->Start()));
			Fixture.Backend->Complete(Pair.Key, TEXT("expected patch failure"));
			ASSERT_THAT(IsTrue(
				Fixture.Service->GetLastResult().Code == Pair.Value));
			ASSERT_THAT(AreEqual(0, Fixture.RegisterCount));
		ASSERT_THAT(AreEqual(0, Fixture.RouteRefreshCount));
		}

		FFixture Missing;
		Missing.SetPrior();
		ASSERT_THAT(IsTrue(Missing.Service->Start()));
		Missing.Backend->Complete(
			EAngelscriptJITPatchCompletionCode::Applied);
		ASSERT_THAT(IsTrue(Missing.Service->GetLastResult().Code
			== EAngelscriptJITRefreshCode::ProviderMissing));
		ASSERT_THAT(AreEqual(0, Missing.RegisterCount));
		ASSERT_THAT(AreEqual(0, Missing.RouteRefreshCount));

		FFixture Stale;
		Stale.SetPrior();
		Stale.PatchedCatalog = Stale.Snapshot->Providers[0];
		ASSERT_THAT(IsTrue(Stale.Service->Start()));
		Stale.Backend->Complete(EAngelscriptJITPatchCompletionCode::Applied);
		ASSERT_THAT(IsTrue(Stale.Service->GetLastResult().Code
			== EAngelscriptJITRefreshCode::StaleProviderGeneration));
		ASSERT_THAT(AreEqual(0, Stale.RegisterCount));
		ASSERT_THAT(AreEqual(0, Stale.RouteRefreshCount));

		FFixture Unexpected;
		Unexpected.SetPrior();
		Unexpected.PatchedCatalog = MakeCatalog(MakeOutput(TEXT("other")));
		ASSERT_THAT(IsTrue(Unexpected.Service->Start()));
		Unexpected.Backend->Complete(EAngelscriptJITPatchCompletionCode::Applied);
		ASSERT_THAT(IsTrue(Unexpected.Service->GetLastResult().Code
			== EAngelscriptJITRefreshCode::UnexpectedProviderGeneration));
		ASSERT_THAT(AreEqual(0, Unexpected.RegisterCount));

		FFixture Incompatible;
		Incompatible.SetPrior();
		Incompatible.SetExpectedPatch();
		TSharedPtr<FAngelscriptJITProviderCatalog, ESPMode::ThreadSafe>
			MutableCatalog = ConstCastSharedPtr<FAngelscriptJITProviderCatalog>(
				Incompatible.PatchedCatalog);
		MutableCatalog->ArtifactSetDigest = MakeHash(TEXT("wrong-digest"));
		ASSERT_THAT(IsTrue(Incompatible.Service->Start()));
		Incompatible.Backend->Complete(
			EAngelscriptJITPatchCompletionCode::Applied);
		ASSERT_THAT(IsTrue(Incompatible.Service->GetLastResult().Code
			== EAngelscriptJITRefreshCode::IncompatibleProvider));
		ASSERT_THAT(AreEqual(0, Incompatible.RegisterCount));

		FFixture RegistrationFailure;
		RegistrationFailure.SetPrior();
		RegistrationFailure.SetExpectedPatch();
		RegistrationFailure.Registration.Code =
			EAngelscriptJITProviderRegistrationCode::CodeLifetimeUnavailable;
		ASSERT_THAT(IsTrue(RegistrationFailure.Service->Start()));
		RegistrationFailure.Backend->Complete(
			EAngelscriptJITPatchCompletionCode::Applied);
		ASSERT_THAT(IsTrue(
			RegistrationFailure.Service->GetLastResult().Code
				== EAngelscriptJITRefreshCode::IncompatibleProvider));
		ASSERT_THAT(IsTrue(
			RegistrationFailure.Service->GetLastResult().Detail.Contains(
				TEXT("CodeLifetimeUnavailable"))));
		ASSERT_THAT(AreEqual(1, RegistrationFailure.RegisterCount));
		ASSERT_THAT(AreEqual(0, RegistrationFailure.RouteRefreshCount));
	}

	TEST_METHOD(ValidNewGenerationRefreshesRoutesAndAlreadyCurrentSkipsPatch)
	{
		using namespace AngelscriptJITRefreshServiceTests_Private;
		FFixture Success;
		Success.SetPrior();
		Success.SetExpectedPatch();
		ASSERT_THAT(IsTrue(Success.Service->Start()));
		Success.Backend->Complete(EAngelscriptJITPatchCompletionCode::Applied);
		ASSERT_THAT(IsTrue(Success.Service->GetLastResult().Code
			== EAngelscriptJITRefreshCode::Succeeded));
		ASSERT_THAT(AreEqual(1, Success.RegisterCount));
		ASSERT_THAT(AreEqual(1, Success.RouteRefreshCount));
		ASSERT_THAT(AreEqual(1u,
			Success.Service->GetLastResult().RouteRefresh.PublishedNativeCount));

		FFixture RouteFailure;
		RouteFailure.SetPrior();
		RouteFailure.SetExpectedPatch();
		RouteFailure.RouteRefresh.Code =
			EAngelscriptJITProviderRouteRefreshCode::RoutePublicationFailed;
		ASSERT_THAT(IsTrue(RouteFailure.Service->Start()));
		RouteFailure.Backend->Complete(
			EAngelscriptJITPatchCompletionCode::Applied);
		ASSERT_THAT(IsTrue(RouteFailure.Service->GetLastResult().Code
			== EAngelscriptJITRefreshCode::RouteRefreshFailed));

		FFixture Current;
		Current.SetPrior(TEXT("new"));
		ASSERT_THAT(IsTrue(Current.Service->Start()));
		ASSERT_THAT(IsTrue(Current.Service->GetLastResult().Code
			== EAngelscriptJITRefreshCode::AlreadyCurrent));
		ASSERT_THAT(AreEqual(0, Current.Backend->RequestCount));
		ASSERT_THAT(AreEqual(1, Current.RouteRefreshCount));
	}
};

#endif
