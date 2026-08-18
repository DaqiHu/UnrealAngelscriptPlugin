#include "StaticJIT/AngelscriptJITRefreshService.h"

#include "AngelscriptEngine.h"
#include "AngelscriptSubsystem.h"
#include "StaticJIT/AngelscriptJITProjectScaffold.h"

#include "Features/IModularFeatures.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

#if PLATFORM_WINDOWS
#include "ILiveCodingModule.h"
#endif

namespace AngelscriptEditor::StaticJIT
{
	namespace
	{
		const TCHAR* ProviderRegistrationCodeName(
			const EAngelscriptJITProviderRegistrationCode Code)
		{
			switch (Code)
			{
			case EAngelscriptJITProviderRegistrationCode::Registered:
				return TEXT("Registered");
			case EAngelscriptJITProviderRegistrationCode::Replaced:
				return TEXT("Replaced");
			case EAngelscriptJITProviderRegistrationCode::Unchanged:
				return TEXT("Unchanged");
			case EAngelscriptJITProviderRegistrationCode::InvalidOwner:
				return TEXT("InvalidOwner");
			case EAngelscriptJITProviderRegistrationCode::InvalidProvider:
				return TEXT("InvalidProvider");
			case EAngelscriptJITProviderRegistrationCode::OwnerProviderIdChanged:
				return TEXT("OwnerProviderIdChanged");
			case EAngelscriptJITProviderRegistrationCode::ProviderIdConflict:
				return TEXT("ProviderIdConflict");
			case EAngelscriptJITProviderRegistrationCode::AmbiguousGeneration:
				return TEXT("AmbiguousGeneration");
			case EAngelscriptJITProviderRegistrationCode::CodeLifetimeUnavailable:
				return TEXT("CodeLifetimeUnavailable");
			default:
				return TEXT("Unknown");
			}
		}

		const TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe> FindProvider(
			const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
				ESPMode::ThreadSafe>& Snapshot,
			const FAngelscriptJITProviderId& ProviderId)
		{
			if (!Snapshot.IsValid())
			{
				return {};
			}
			const TSharedPtr<const FAngelscriptJITProviderCatalog,
				ESPMode::ThreadSafe>* Found = Snapshot->Providers.FindByPredicate(
				[ProviderId](
					const TSharedPtr<const FAngelscriptJITProviderCatalog,
						ESPMode::ThreadSafe>& Candidate)
				{
					return Candidate.IsValid()
						&& Candidate->ProviderId == ProviderId;
				});
			return Found != nullptr ? *Found : nullptr;
		}

		class FLiveCodingPatchBackend final
			: public IAngelscriptJITPatchBackend
		{
		public:
			bool IsAvailable(FString& OutReason) const override
			{
#if PLATFORM_WINDOWS
				ILiveCodingModule* LiveCoding =
					FModuleManager::LoadModulePtr<ILiveCodingModule>(
						LIVE_CODING_MODULE_NAME);
				if (LiveCoding == nullptr)
				{
					OutReason = TEXT("The Live Coding module is unavailable.");
					return false;
				}
				if (!LiveCoding->HasStarted())
				{
					OutReason =
						TEXT("Live Coding has not been started for this Editor session.");
					return false;
				}
				if (!LiveCoding->IsEnabledForSession())
				{
					OutReason =
						TEXT("Live Coding is disabled for this Editor session.");
					return false;
				}
				OutReason.Reset();
				return true;
#else
				OutReason = TEXT("Live Coding is unsupported on this platform.");
				return false;
#endif
			}

			bool IsCompileInProgress() const override
			{
#if PLATFORM_WINDOWS
				const ILiveCodingModule* LiveCoding =
					FModuleManager::GetModulePtr<ILiveCodingModule>(
						LIVE_CODING_MODULE_NAME);
				return LiveCoding != nullptr && LiveCoding->IsCompiling();
#else
				return false;
#endif
			}

			bool RequestCompile(
				TFunction<void(FAngelscriptJITPatchCompletion)> Completion)
				override
			{
#if PLATFORM_WINDOWS
				ILiveCodingModule* LiveCoding =
					FModuleManager::GetModulePtr<ILiveCodingModule>(
						LIVE_CODING_MODULE_NAME);
				if (LiveCoding == nullptr || !Completion)
				{
					return false;
				}
				ELiveCodingCompileResult CompileResult =
					ELiveCodingCompileResult::Failure;
				LiveCoding->Compile(
					ELiveCodingCompileFlags::WaitForCompletion,
					&CompileResult);
				FAngelscriptJITPatchCompletion Result;
				switch (CompileResult)
				{
				case ELiveCodingCompileResult::Success:
					Result.Code = EAngelscriptJITPatchCompletionCode::Applied;
					Result.Detail = TEXT("Live Coding applied a patch.");
					break;
				case ELiveCodingCompileResult::NoChanges:
					Result.Code = EAngelscriptJITPatchCompletionCode::NoChanges;
					Result.Detail = TEXT("Live Coding reported no compiled changes.");
					break;
				case ELiveCodingCompileResult::Cancelled:
					Result.Code = EAngelscriptJITPatchCompletionCode::Cancelled;
					Result.Detail = TEXT("Live Coding compilation was cancelled.");
					break;
				case ELiveCodingCompileResult::Failure:
				case ELiveCodingCompileResult::CompileStillActive:
					Result.Code =
						EAngelscriptJITPatchCompletionCode::CompileFailed;
					Result.Detail = TEXT("Live Coding compilation failed.");
					break;
				case ELiveCodingCompileResult::NotStarted:
				default:
					Result.Code = EAngelscriptJITPatchCompletionCode::PatchFailed;
					Result.Detail =
						TEXT("Live Coding did not start or complete a patch.");
					break;
				}
				Completion(MoveTemp(Result));
				return true;
#else
				return false;
#endif
			}
		};

		FAngelscriptJITSourceAuthoritySnapshot CaptureProjectAuthority(
			const FStringView InProjectFile)
		{
			UAngelscriptSubsystem* const Subsystem =
				UAngelscriptSubsystem::Get();
			FAngelscriptEngine* const PrimaryEngine =
				Subsystem != nullptr ? Subsystem->GetEngine() : nullptr;
			if (PrimaryEngine == nullptr)
			{
				FAngelscriptJITSourceAuthoritySnapshot Result;
				Result.Code =
					EAngelscriptJITSourceAuthorityCode::EngineUnavailable;
				Result.Detail =
					TEXT("The authoritative AngelScript Engine is unavailable.");
				return Result;
			}
			return FAngelscriptJITSourceAuthority::Capture(
				*PrimaryEngine, InProjectFile,
				EAngelscriptJITTargetProfile::EditorDevelopment);
		}

		FAngelscriptJITRefreshPreparation PrepareProject(
			const FStringView InProjectFile,
			const FAngelscriptJITSourceAuthoritySnapshot& Authority)
		{
			FAngelscriptJITRefreshPreparation Result;
			FString ProjectFile(InProjectFile);
			ProjectFile.TrimStartAndEndInline();
			if (ProjectFile.IsEmpty())
			{
				ProjectFile = FPaths::GetProjectFilePath();
			}
			ProjectFile = FPaths::ConvertRelativePathToFull(ProjectFile);
			FPaths::NormalizeFilename(ProjectFile);

			const FAngelscriptJITProjectScaffoldInspection Inspection =
				FAngelscriptJITProjectScaffold::Inspect(ProjectFile);
			if (!Inspection.IsCurrent())
			{
				Result.FailureCode =
					EAngelscriptJITRefreshCode::ScaffoldRequired;
				Result.Detail =
					TEXT("AngelscriptJIT project scaffold is missing or stale: ")
					+ Inspection.Summary();
				return Result;
			}

			if (!Authority.IsCurrent()
				|| !Authority.FrozenSourceProvider.IsValid())
			{
				Result.FailureCode =
					EAngelscriptJITRefreshCode::AuthoritativeEngineStale;
				Result.Detail =
					TEXT("The authoritative AngelScript source snapshot is unavailable.");
				FStaticJITDiagnostics::FPrePublicationFailureRecord& Failure =
					Result.PrePublicationFailure.Emplace();
				Failure.Category = FStaticJITDiagnostics::
					EPrePublicationFailureCategory::AuthoritativeEngineStale;
				Failure.AffectedBoundary = FStaticJITDiagnostics::
					EPrePublicationBoundary::PrimarySourceAuthority;
				Failure.Detail = Result.Detail;
				return Result;
			}

			FAngelscriptJITGenerationOutput RuntimeOutput;
			FString Error;
			if (!FAngelscriptJITProjectGeneration::BuildProjectArtifacts(
					ProjectFile,
					EAngelscriptJITTargetProfile::EditorDevelopment,
					Authority.FrozenSourceProvider,
					RuntimeOutput,
					&Error))
			{
				Result.FailureCode =
					EAngelscriptJITRefreshCode::GenerationFailed;
				Result.Detail = Error;
				return Result;
			}
			if (RuntimeOutput.ProviderId != Inspection.ProviderId
				|| RuntimeOutput.TargetProfile !=
					EAngelscriptJITTargetProfile::EditorDevelopment)
			{
				Result.FailureCode =
					EAngelscriptJITRefreshCode::GenerationFailed;
				Result.Detail =
					TEXT("Generated artifacts do not belong to this project Editor profile.");
				return Result;
			}

			if (!FAngelscriptJITProjectGeneration::PrepareProjectProfileOutput(
					RuntimeOutput, Result.GeneratedOutput, &Error))
			{
				Result.FailureCode =
					EAngelscriptJITRefreshCode::GenerationFailed;
				Result.Detail = Error;
				return Result;
			}
			const FString OutputDirectory =
				FAngelscriptJITProjectGeneration::GetProfileOutputDirectory(
					ProjectFile,
					EAngelscriptJITTargetProfile::EditorDevelopment);
			Result.Publication = FAngelscriptJITProjectGeneration::Generate(
				OutputDirectory, Result.GeneratedOutput);
			if (!Result.Publication.bSuccess)
			{
				Result.FailureCode =
					EAngelscriptJITRefreshCode::PublicationFailed;
				Result.Detail = Result.Publication.Error;
				return Result;
			}

			Result.bHostModuleLoaded =
				FModuleManager::Get().IsModuleLoaded(
					FAngelscriptJITProjectScaffold::ModuleName);
			Result.bSuccess = true;
			Result.Detail = TEXT("EditorDevelopment StaticJIT sources are current.");
			return Result;
		}

		FAngelscriptJITProviderRegistrationResult RefreshProjectProvider(
			const FAngelscriptJITProviderId& ProviderId)
		{
			FAngelscriptJITProviderRegistrationResult Result;
			Result.Code =
				EAngelscriptJITProviderRegistrationCode::InvalidProvider;
			const TArray<IAngelscriptJITArtifactProvider*> Providers =
				IModularFeatures::Get().GetModularFeatureImplementations<
					IAngelscriptJITArtifactProvider>(
						IAngelscriptJITArtifactProvider::FeatureName());
			for (IAngelscriptJITArtifactProvider* Provider : Providers)
			{
				const FAngelscriptJITProviderView* View = Provider != nullptr
					? Provider->GetAngelscriptJITProviderView()
					: nullptr;
				if (View != nullptr && View->ProviderId == ProviderId)
				{
					return FAngelscriptJITProviderRegistry::Get().RegisterProvider(
						Provider, *Provider);
				}
			}
			Result.Validation.Code =
				EAngelscriptJITProviderValidationCode::MissingProviderView;
			return Result;
		}

		FAngelscriptJITObservedProvider ObserveProjectProvider(
			const FAngelscriptJITProviderId& ProviderId)
		{
			FAngelscriptJITObservedProvider Result;
			const TArray<IAngelscriptJITArtifactProvider*> Providers =
				IModularFeatures::Get().GetModularFeatureImplementations<
					IAngelscriptJITArtifactProvider>(
						IAngelscriptJITArtifactProvider::FeatureName());
			for (IAngelscriptJITArtifactProvider* Provider : Providers)
			{
				const FAngelscriptJITProviderView* View = Provider != nullptr
					? Provider->GetAngelscriptJITProviderView()
					: nullptr;
				if (View == nullptr || View->ProviderId != ProviderId)
				{
					continue;
				}
				Result.bFound = true;
				Result.Validation =
					FAngelscriptJITProviderManifest::Validate(*View);
				if (Result.Validation.IsValid())
				{
					Result.ProviderId = View->ProviderId;
					Result.ProviderGeneration = View->ProviderGeneration;
					Result.ArtifactSetDigest = View->ArtifactSetDigest;
					Result.ArtifactProfile = View->ArtifactProfile;
					Result.NativeEnvironmentFingerprint =
						View->NativeEnvironmentFingerprint;
				}
				return Result;
			}
			Result.Validation.Code =
				EAngelscriptJITProviderValidationCode::MissingProviderView;
			return Result;
		}

		FAngelscriptJITRefreshDependencies DefaultDependencies()
		{
			FAngelscriptJITRefreshDependencies Result;
			Result.CaptureAuthority = &CaptureProjectAuthority;
			Result.Prepare = &PrepareProject;
			Result.GetProviderSnapshot = []()
			{
				return FAngelscriptJITProviderRegistry::Get().GetSnapshot();
			};
			Result.ObserveProvider = &ObserveProjectProvider;
			Result.RefreshProvider = &RefreshProjectProvider;
			Result.RefreshRoutes = []()
			{
				UAngelscriptSubsystem* const Subsystem =
					UAngelscriptSubsystem::Get();
				FAngelscriptEngine* const PrimaryEngine =
					Subsystem != nullptr ? Subsystem->GetEngine() : nullptr;
				if (PrimaryEngine == nullptr)
				{
					FAngelscriptJITProviderRouteRefreshResult Failed;
					Failed.Code =
						EAngelscriptJITProviderRouteRefreshCode::InvalidEngine;
					Failed.Detail = TEXT("The AngelScript Engine is unavailable.");
					return Failed;
				}
				return FAngelscriptJITProviderRouter::Refresh(
					*PrimaryEngine,
					EAngelscriptJITTargetProfile::EditorDevelopment);
			};
			return Result;
		}
	}

	IAngelscriptJITPatchBackend::IAngelscriptJITPatchBackend() = default;
	IAngelscriptJITPatchBackend::~IAngelscriptJITPatchBackend() = default;

	TSharedRef<FAngelscriptJITRefreshService>
	FAngelscriptJITRefreshService::CreateDefault()
	{
		return MakeShared<FAngelscriptJITRefreshService>(
			MakeShared<FLiveCodingPatchBackend>(), DefaultDependencies());
	}

	FAngelscriptJITRefreshService::FAngelscriptJITRefreshService(
		TSharedRef<IAngelscriptJITPatchBackend> InPatchBackend,
		FAngelscriptJITRefreshDependencies InDependencies)
		: PatchBackend(MoveTemp(InPatchBackend))
		, Dependencies(MoveTemp(InDependencies))
	{
	}

	FAngelscriptJITRefreshService::~FAngelscriptJITRefreshService() = default;

	bool FAngelscriptJITRefreshService::Start(const FStringView ProjectFile)
	{
		if (bRequestActive)
		{
			LastResult.Code =
				EAngelscriptJITRefreshCode::AlreadyInProgress;
			LastResult.Detail =
				TEXT("A Generate/Refresh AngelScript JIT request is already active.");
			return false;
		}
		if (!Dependencies.CaptureAuthority
			|| !Dependencies.Prepare
			|| !Dependencies.GetProviderSnapshot
			|| !Dependencies.ObserveProvider
			|| !Dependencies.RefreshProvider
			|| !Dependencies.RefreshRoutes)
		{
			LastResult = {};
			Finish(EAngelscriptJITRefreshCode::GenerationFailed,
				TEXT("StaticJIT refresh dependencies are incomplete."));
			return false;
		}

		LastResult = {};
		ExpectedOutput = {};
		bRequestActive = true;
		const FAngelscriptJITSourceAuthoritySnapshot Authority =
			Dependencies.CaptureAuthority(ProjectFile);
		if (!Authority.IsCurrent())
		{
			FString Detail = Authority.Detail
				+ TEXT(" Run the existing AngelScript Hot Reload/recompile flow, then retry Generate/Refresh.");
			FStaticJITDiagnostics::FPrePublicationFailureRecord& Failure =
				LastResult.PrePublicationFailure.Emplace();
			Failure.Category = FStaticJITDiagnostics::
				EPrePublicationFailureCategory::AuthoritativeEngineStale;
			Failure.AffectedBoundary = FStaticJITDiagnostics::
				EPrePublicationBoundary::PrimarySourceAuthority;
			Failure.Detail = Detail;
			Finish(EAngelscriptJITRefreshCode::AuthoritativeEngineStale,
				MoveTemp(Detail));
			return false;
		}
		const FAngelscriptJITRefreshPreparation Preparation =
			Dependencies.Prepare(ProjectFile, Authority);
		if (!Preparation.bSuccess)
		{
			LastResult.PrePublicationFailure =
				Preparation.PrePublicationFailure;
			Finish(Preparation.FailureCode, Preparation.Detail);
			return false;
		}

		ExpectedOutput = Preparation.GeneratedOutput;
		LastResult.ProviderId = ExpectedOutput.ProviderId;
		LastResult.ExpectedGeneration = ExpectedOutput.ProviderGeneration;
		LastResult.ExpectedArtifactSetDigest =
			ExpectedOutput.ArtifactSetDigest;
		LastResult.AddedModuleSources =
			Preparation.Publication.AddedModuleSources;
		LastResult.RemovedModuleSources =
			Preparation.Publication.RemovedModuleSources;
		if (Preparation.Publication.HasModuleSourceSetChange())
		{
			Finish(EAngelscriptJITRefreshCode::RequiresFullBuild,
				TEXT("The generated AS-module .jit.cpp source set changed; run one normal full build before using Live Coding."));
			return false;
		}
		if (!Preparation.bHostModuleLoaded)
		{
			Finish(EAngelscriptJITRefreshCode::MissingFullBuild,
				TEXT("AngelscriptJIT is not loaded in the active Editor target; run one normal full build and restart the Editor."));
			return false;
		}

		const TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe> Previous = FindProvider(
				Dependencies.GetProviderSnapshot(), ExpectedOutput.ProviderId);
		if (!Previous.IsValid())
		{
			Finish(EAngelscriptJITRefreshCode::MissingFullBuild,
				TEXT("The active AngelscriptJIT module has no baseline Provider; run one normal full build and restart the Editor."));
			return false;
		}
		LastResult.PreviousGeneration = Previous->ProviderGeneration;
		if (Previous->ProviderGeneration == ExpectedOutput.ProviderGeneration)
		{
			LastResult.RouteRefresh = Dependencies.RefreshRoutes();
			if (!LastResult.RouteRefresh.IsApplied())
			{
				Finish(EAngelscriptJITRefreshCode::RouteRefreshFailed,
					LastResult.RouteRefresh.Detail);
				return false;
			}
			Finish(EAngelscriptJITRefreshCode::AlreadyCurrent,
				TEXT("The generated Provider is already current; routes were refreshed without Live Coding."));
			return true;
		}

		if (PatchBackend->IsCompileInProgress())
		{
			Finish(EAngelscriptJITRefreshCode::LiveCodingCompileInProgress,
				TEXT("Live Coding is already compiling another patch."));
			return false;
		}
		FString UnavailableReason;
		if (!PatchBackend->IsAvailable(UnavailableReason))
		{
			Finish(EAngelscriptJITRefreshCode::LiveCodingUnavailable,
				MoveTemp(UnavailableReason));
			return false;
		}

		LastResult.Code = EAngelscriptJITRefreshCode::WaitingForPatch;
		LastResult.Detail = TEXT("Generated sources are current; waiting for the Live Coding patch.");
		const TWeakPtr<FAngelscriptJITRefreshService> WeakThis = AsShared();
		if (!PatchBackend->RequestCompile(
			[WeakThis](FAngelscriptJITPatchCompletion Completion)
			{
				if (const TSharedPtr<FAngelscriptJITRefreshService> Self =
					WeakThis.Pin())
				{
					Self->CompletePatch(MoveTemp(Completion));
				}
			}))
		{
			Finish(EAngelscriptJITRefreshCode::LiveCodingRequestFailed,
				TEXT("Live Coding rejected the compile request."));
			return false;
		}
		return true;
	}

	void FAngelscriptJITRefreshService::CompletePatch(
		FAngelscriptJITPatchCompletion Completion)
	{
		if (!bRequestActive)
		{
			return;
		}
		switch (Completion.Code)
		{
		case EAngelscriptJITPatchCompletionCode::NoChanges:
			Finish(EAngelscriptJITRefreshCode::LiveCodingNoChanges,
				MoveTemp(Completion.Detail));
			return;
		case EAngelscriptJITPatchCompletionCode::CompileFailed:
			Finish(EAngelscriptJITRefreshCode::LiveCodingCompileFailed,
				MoveTemp(Completion.Detail));
			return;
		case EAngelscriptJITPatchCompletionCode::PatchFailed:
			Finish(EAngelscriptJITRefreshCode::LiveCodingPatchFailed,
				MoveTemp(Completion.Detail));
			return;
		case EAngelscriptJITPatchCompletionCode::Cancelled:
			Finish(EAngelscriptJITRefreshCode::LiveCodingCancelled,
				MoveTemp(Completion.Detail));
			return;
		case EAngelscriptJITPatchCompletionCode::Applied:
			break;
		default:
			Finish(EAngelscriptJITRefreshCode::LiveCodingPatchFailed,
				TEXT("Live Coding returned an unknown completion result."));
			return;
		}

		const FAngelscriptJITObservedProvider Observed =
			Dependencies.ObserveProvider(ExpectedOutput.ProviderId);
		if (!Observed.bFound)
		{
			Finish(EAngelscriptJITRefreshCode::ProviderMissing,
				TEXT("The expected project Provider is missing after the patch."));
			return;
		}
		if (!Observed.Validation.IsValid())
		{
			Finish(EAngelscriptJITRefreshCode::IncompatibleProvider,
				TEXT("The patched project Provider failed ABI/manifest validation before Registry publication."));
			return;
		}
		if (Observed.ProviderGeneration != ExpectedOutput.ProviderGeneration)
		{
			const bool bStale = Observed.ProviderGeneration
				== LastResult.PreviousGeneration;
			Finish(bStale
					? EAngelscriptJITRefreshCode::StaleProviderGeneration
					: EAngelscriptJITRefreshCode::UnexpectedProviderGeneration,
				bStale
					? TEXT("Live Coding completed, but the Provider still exposes the previous generation.")
					: TEXT("Live Coding exposed a Provider generation other than the one generated by this request."));
			return;
		}
		if (Observed.ArtifactSetDigest != ExpectedOutput.ArtifactSetDigest
			|| Observed.ArtifactProfile.Hash
				!= ExpectedOutput.ArtifactProfile.Hash
			|| Observed.NativeEnvironmentFingerprint !=
				ExpectedOutput.NativeEnvironmentFingerprint)
		{
			Finish(EAngelscriptJITRefreshCode::IncompatibleProvider,
				TEXT("The patched Provider generation has an incompatible artifact set, profile, or native environment."));
			return;
		}

		LastResult.ProviderRegistration =
			Dependencies.RefreshProvider(ExpectedOutput.ProviderId);
		if (!LastResult.ProviderRegistration.IsAccepted())
		{
			Finish(EAngelscriptJITRefreshCode::IncompatibleProvider,
				FString::Printf(
					TEXT("The validated patched project Provider could not be registered: %s (validation=%u)."),
					ProviderRegistrationCodeName(
						LastResult.ProviderRegistration.Code),
					static_cast<uint8>(
						LastResult.ProviderRegistration.Validation.Code)));
			return;
		}
		const TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe> Current = FindProvider(
				Dependencies.GetProviderSnapshot(), ExpectedOutput.ProviderId);
		if (!Current.IsValid()
			|| Current->ProviderGeneration != ExpectedOutput.ProviderGeneration
			|| Current->ArtifactSetDigest != ExpectedOutput.ArtifactSetDigest)
		{
			Finish(EAngelscriptJITRefreshCode::IncompatibleProvider,
				TEXT("Registry publication did not retain the validated expected Provider generation."));
			return;
		}

		LastResult.RouteRefresh = Dependencies.RefreshRoutes();
		if (!LastResult.RouteRefresh.IsApplied())
		{
			Finish(EAngelscriptJITRefreshCode::RouteRefreshFailed,
				LastResult.RouteRefresh.Detail);
			return;
		}
		Finish(EAngelscriptJITRefreshCode::Succeeded,
			FString::Printf(
				TEXT("Live Coding published Provider generation %s; Native=%u VM=%u."),
				*ExpectedOutput.ProviderGeneration.Hash.ToHexString(),
				LastResult.RouteRefresh.PublishedNativeCount,
				LastResult.RouteRefresh.VmFallbackCount));
	}

	void FAngelscriptJITRefreshService::Finish(
		const EAngelscriptJITRefreshCode Code,
		FString Detail)
	{
		LastResult.Code = Code;
		LastResult.Detail = MoveTemp(Detail);
		bRequestActive = false;
	}
}
