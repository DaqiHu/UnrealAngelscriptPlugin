#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/AngelscriptJITProjectGeneration.h"
#include "StaticJIT/AngelscriptJITProviderRegistry.h"
#include "StaticJIT/AngelscriptJITProviderRouter.h"

namespace AngelscriptEditor::StaticJIT
{
	enum class EAngelscriptJITPatchCompletionCode : uint8
	{
		Applied = 0,
		NoChanges,
		CompileFailed,
		PatchFailed,
		Cancelled,
	};

	struct FAngelscriptJITPatchCompletion
	{
		EAngelscriptJITPatchCompletionCode Code =
			EAngelscriptJITPatchCompletionCode::PatchFailed;
		FString Detail;
	};

	/** Testable boundary over the Editor-only Live Coding module. */
	class ANGELSCRIPTEDITOR_API IAngelscriptJITPatchBackend
	{
	public:
		IAngelscriptJITPatchBackend();
		virtual ~IAngelscriptJITPatchBackend();
		virtual bool IsAvailable(FString& OutReason) const = 0;
		virtual bool IsCompileInProgress() const = 0;
		virtual bool RequestCompile(
			TFunction<void(FAngelscriptJITPatchCompletion)> Completion) = 0;
	};

	enum class EAngelscriptJITRefreshCode : uint8
	{
		Idle = 0,
		WaitingForPatch,
		Succeeded,
		AlreadyCurrent,
		AlreadyInProgress,
		EngineUnavailable,
		ScriptCompileFailed,
		ScaffoldRequired,
		GenerationFailed,
		PublicationFailed,
		MissingFullBuild,
		RequiresFullBuild,
		LiveCodingUnavailable,
		LiveCodingCompileInProgress,
		LiveCodingRequestFailed,
		LiveCodingNoChanges,
		LiveCodingCompileFailed,
		LiveCodingPatchFailed,
		LiveCodingCancelled,
		ProviderMissing,
		StaleProviderGeneration,
		UnexpectedProviderGeneration,
		IncompatibleProvider,
		RouteRefreshFailed,
	};

	struct FAngelscriptJITRefreshPreparation
	{
		bool bSuccess = false;
		EAngelscriptJITRefreshCode FailureCode =
			EAngelscriptJITRefreshCode::GenerationFailed;
		FString Detail;
		bool bHostModuleLoaded = false;
		FAngelscriptJITGenerationOutput GeneratedOutput;
		FAngelscriptJITProjectGenerationResult Publication;
	};

	struct FAngelscriptJITObservedProvider
	{
		bool bFound = false;
		FAngelscriptJITProviderValidationResult Validation;
		FAngelscriptJITProviderId ProviderId;
		FAngelscriptJITProviderGeneration ProviderGeneration;
		FAngelscriptHash256 ArtifactSetDigest;
		FAngelscriptArtifactProfileKey ArtifactProfile;
		FAngelscriptHash256 NativeEnvironmentFingerprint;
	};

	struct FAngelscriptJITRefreshResult
	{
		EAngelscriptJITRefreshCode Code = EAngelscriptJITRefreshCode::Idle;
		FString Detail;
		FAngelscriptJITProviderId ProviderId;
		FAngelscriptJITProviderGeneration PreviousGeneration;
		FAngelscriptJITProviderGeneration ExpectedGeneration;
		FAngelscriptHash256 ExpectedArtifactSetDigest;
		TArray<FString> AddedModuleSources;
		TArray<FString> RemovedModuleSources;
		FAngelscriptJITProviderRegistrationResult ProviderRegistration;
		FAngelscriptJITProviderRouteRefreshResult RouteRefresh;

		bool IsSuccess() const
		{
			return Code == EAngelscriptJITRefreshCode::Succeeded
				|| Code == EAngelscriptJITRefreshCode::AlreadyCurrent;
		}

		bool IsInProgress() const
		{
			return Code == EAngelscriptJITRefreshCode::WaitingForPatch;
		}
	};

	using FAngelscriptJITRefreshPrepare = TFunction<
		FAngelscriptJITRefreshPreparation(FStringView)>;
	using FAngelscriptJITRefreshProviderSnapshot = TFunction<
		TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe>()>;
	using FAngelscriptJITRefreshProviderObservation = TFunction<
		FAngelscriptJITObservedProvider(
			const FAngelscriptJITProviderId&)>;
	using FAngelscriptJITRefreshProviderRegistration = TFunction<
		FAngelscriptJITProviderRegistrationResult(
			const FAngelscriptJITProviderId&)>;
	using FAngelscriptJITRefreshRoutePublication = TFunction<
		FAngelscriptJITProviderRouteRefreshResult()>;

	struct FAngelscriptJITRefreshDependencies
	{
		FAngelscriptJITRefreshPrepare Prepare;
		FAngelscriptJITRefreshProviderSnapshot GetProviderSnapshot;
		FAngelscriptJITRefreshProviderObservation ObserveProvider;
		FAngelscriptJITRefreshProviderRegistration RefreshProvider;
		FAngelscriptJITRefreshRoutePublication RefreshRoutes;
	};

	/**
	 * Explicit Generate/Refresh state machine. The service owns no generated or
	 * Provider memory; every completion re-validates copied Registry state before
	 * publishing Engine-local bindings.
	 */
	class ANGELSCRIPTEDITOR_API FAngelscriptJITRefreshService final
		: public TSharedFromThis<FAngelscriptJITRefreshService>
	{
	public:
		static TSharedRef<FAngelscriptJITRefreshService> CreateDefault();

		FAngelscriptJITRefreshService(
			TSharedRef<IAngelscriptJITPatchBackend> InPatchBackend,
			FAngelscriptJITRefreshDependencies InDependencies);
		~FAngelscriptJITRefreshService();

		bool Start(FStringView ProjectFile = {});
		const FAngelscriptJITRefreshResult& GetLastResult() const
		{
			return LastResult;
		}

	private:
		void CompletePatch(FAngelscriptJITPatchCompletion Completion);
		void Finish(EAngelscriptJITRefreshCode Code, FString Detail);

		TSharedRef<IAngelscriptJITPatchBackend> PatchBackend;
		FAngelscriptJITRefreshDependencies Dependencies;
		FAngelscriptJITRefreshResult LastResult;
		FAngelscriptJITGenerationOutput ExpectedOutput;
		bool bRequestActive = false;
	};
}
