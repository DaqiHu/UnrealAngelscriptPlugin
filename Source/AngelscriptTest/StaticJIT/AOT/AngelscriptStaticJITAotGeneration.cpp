#include "StaticJIT/AOT/AngelscriptStaticJITAotGeneration.h"

#include "AngelscriptTestEngineAcquisition.h"
#include "AngelscriptTestJITProbes.h"
#include "Cache/AngelscriptCacheDiagnostics.h"
#include "Cache/AngelscriptCacheService.h"
#include "Compilation/AngelscriptCompilationEvents.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"
#include "StaticJIT/AngelscriptJITGeneratedFileStore.h"
#include "StaticJIT/AngelscriptJITProviderRegistry.h"
#include "StaticJIT/AngelscriptStaticJIT.h"
#include "StaticJIT/AngelscriptStaticJITGenerationSnapshot.h"
#include "StaticJIT/AngelscriptStaticJITGenerator.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptStaticJITAotGeneration
{
	namespace
	{
		static FString GetTestJITModuleRoot()
		{
			return FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectDir(),
				TEXT("Plugins/Angelscript/Source/AngelscriptTestJIT")));
		}

		static FString GetGeneratedProfileRoot()
		{
			return FPaths::Combine(
				GetTestJITModuleRoot(), TEXT("Generated/EditorDevelopment"));
		}

		static TArray<FString> GetLegacyGeneratedProfileRoots()
		{
			return {
				FPaths::Combine(GetTestJITModuleRoot(),
					TEXT("Private/Generated/EditorDevelopment")),
				FPaths::Combine(GetTestJITModuleRoot(),
					TEXT("Private/Generated/Profiles/EditorDevelopment")),
			};
		}

		static void RemoveEmptyLegacyPrivateWrappers()
		{
			const FString PrivateRoot = FPaths::Combine(
				GetTestJITModuleRoot(), TEXT("Private"));
			const FString GeneratedRoot = FPaths::Combine(
				PrivateRoot, TEXT("Generated"));
			IFileManager::Get().DeleteDirectory(
				*FPaths::Combine(GeneratedRoot, TEXT("Profiles")),
				false, false);
			IFileManager::Get().DeleteDirectory(
				*GeneratedRoot, false, false);
			IFileManager::Get().DeleteDirectory(
				*PrivateRoot, false, false);
		}

		static TUniquePtr<FAngelscriptEngine> CreateUncompiledFixtureEngine(
			const FString& FixtureRoot,
			FString& OutError,
			const bool bEnableCacheV2Persistence,
			const EAngelscriptEnginePurpose Purpose)
		{
			FAngelscriptEngineConfig Config;
			Config.Purpose = Purpose;
			Config.bIsEditor = true;
			Config.bDevelopmentMode = true;
			Config.bSkipThreadedInitialize = true;
			Config.bRunningCommandlet = true;
			Config.bIsUnattended = true;
			Config.CacheV2RootOverride = FPaths::Combine(
				FixtureRoot, TEXT("CacheV2"));
			Config.bDisableCacheV2Persistence =
				!bEnableCacheV2Persistence;
			// Automation invokes this synchronously on the game thread. Serial
			// preparation avoids starving TaskGraph writer workers while preserving
			// the exact same deterministic capture bytes and Store contract.
			Config.bForceSerialCacheV2Preparation = true;

			FAngelscriptEngineDependencies Dependencies =
				FAngelscriptEngineDependencies::CreateDefault();
			Dependencies.GetProjectDir = [FixtureRoot]()
			{
				return FixtureRoot;
			};
			Dependencies.GetEnabledPluginScriptRoots = []()
			{
				return TArray<FString>();
			};
			Dependencies.GetEnabledPluginScriptRootDescriptors = []()
			{
				return TArray<FAngelscriptPluginScriptRoot>();
			};

			TUniquePtr<FAngelscriptEngine> Engine =
				CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
			if (!Engine.IsValid())
			{
				OutError = TEXT(
					"Failed to create the isolated AngelscriptTestJIT fixture Engine.");
				return nullptr;
			}
			if (!AngelscriptStaticJITAotFixture::RegisterObjectLastNativeSurface(
				*Engine->GetScriptEngine()))
			{
				OutError = TEXT(
					"AngelscriptTestJIT fixture could not register its native probe surface.");
				return nullptr;
			}

			return Engine;
		}

		static int32 CountFrontendEvents(
			const TConstArrayView<FAngelscriptCompilationEvent> Events)
		{
			int32 Count = 0;
			for (const FAngelscriptCompilationEvent& Event : Events)
			{
				switch (Event.Type)
				{
				case EAngelscriptCompilationEventType::PreprocessProcessChunks:
				case EAngelscriptCompilationEventType::PreprocessPostProcessCode:
				case EAngelscriptCompilationEventType::CompileModuleParse:
				case EAngelscriptCompilationEventType::CompileModuleCompileCode:
					++Count;
					break;
				default:
					break;
				}
			}
			return Count;
		}

		static TUniquePtr<FAngelscriptEngine> CreateFixtureEngine(
			FString& OutFixtureRoot,
			FStaticJITAotGenerationResult& Result,
			const bool bEnableCacheV2Persistence,
			const EAngelscriptEnginePurpose Purpose)
		{
			OutFixtureRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptTestJITGeneration"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(OutFixtureRoot);
			if (!OutFixtureRoot.Contains(
				TEXT("/Saved/Automation/AngelscriptTestJITGeneration/")))
			{
				Result.Error = TEXT(
					"AngelscriptTestJIT fixture root escaped its isolated Saved directory.");
				return nullptr;
			}

			const FString ScriptRoot = FPaths::Combine(
				OutFixtureRoot, TEXT("Script"));
			if (!IFileManager::Get().MakeDirectory(*ScriptRoot, true)
				|| !FFileHelper::SaveStringToFile(
					AngelscriptStaticJITAotFixture::GetImportProviderScriptSource(),
					*FPaths::Combine(
						ScriptRoot,
						AngelscriptStaticJITAotFixture::
							GetImportProviderSourceFilename()))
				|| !FFileHelper::SaveStringToFile(
					AngelscriptStaticJITAotFixture::GetScriptSource(),
					*FPaths::Combine(
						ScriptRoot,
						AngelscriptStaticJITAotFixture::GetSourceFilename())))
			{
				Result.Error = TEXT(
					"AngelscriptTestJIT could not materialize its committed fixture sources.");
				return nullptr;
			}

			TUniquePtr<FAngelscriptEngine> Engine =
				CreateUncompiledFixtureEngine(
					OutFixtureRoot, Result.Error,
					bEnableCacheV2Persistence, Purpose);
			if (!Engine.IsValid())
			{
				return nullptr;
			}

			{
				FAngelscriptEngineScope EngineScope(*Engine);
				Engine->InitialCompile();
			}

			if (!Engine->GetModuleByModuleName(
					AngelscriptStaticJITAotFixture::
						GetImportProviderModuleName().ToString()).IsValid()
				|| !Engine->GetModuleByModuleName(
					AngelscriptStaticJITAotFixture::
						GetModuleName().ToString()).IsValid())
			{
				Result.Error = TEXT(
					"AngelscriptTestJIT isolated fixture source compilation failed.");
				return nullptr;
			}
			const TSharedPtr<FAngelscriptModuleDesc> PrimaryModule =
				Engine->GetModuleByModuleName(
					AngelscriptStaticJITAotFixture::GetModuleName().ToString());
			const asCModule* const PrimaryScriptModule =
				PrimaryModule.IsValid()
					? static_cast<const asCModule*>(PrimaryModule->ScriptModule)
					: nullptr;
			if (PrimaryModule.IsValid() && PrimaryScriptModule != nullptr)
			{
				UE_LOG(Angelscript, Verbose,
					TEXT("[TestJITGeneration] Primary capture shape: Classes=%d Enums=%d Delegates=%d ImportedModules=%d PostInitFunctions=%d ObjectTypes=%u ScriptEnums=%u Typedefs=%u ImportedFunctions=%u"),
					PrimaryModule->Classes.Num(), PrimaryModule->Enums.Num(),
					PrimaryModule->Delegates.Num(),
					PrimaryModule->ImportedModules.Num(),
					PrimaryModule->PostInitFunctions.Num(),
					PrimaryScriptModule->GetObjectTypeCount(),
					PrimaryScriptModule->GetEnumCount(),
					PrimaryScriptModule->GetTypedefCount(),
					PrimaryScriptModule->GetImportedFunctionCount());
			}
			return Engine;
		}

		static bool BuildExpectedOutput(
			FAngelscriptEngine& Engine,
			FAngelscriptJITGenerationOutput& OutOutput,
			FStaticJITAotGenerationResult& Result)
		{
#if !AS_CAN_GENERATE_JIT
			Result.Error = TEXT(
				"AngelscriptTestJIT generation is unavailable in this build.");
			return false;
#else
			TArray<asIScriptModule*> ScriptModules;
			const FName FixtureModuleNames[] = {
				AngelscriptStaticJITAotFixture::GetImportProviderModuleName(),
				AngelscriptStaticJITAotFixture::GetModuleName(),
			};
			for (const FName ModuleName : FixtureModuleNames)
			{
				const TSharedPtr<FAngelscriptModuleDesc> ModuleDesc =
					Engine.GetModuleByModuleName(ModuleName.ToString());
				if (!ModuleDesc.IsValid() || ModuleDesc->ScriptModule == nullptr)
				{
					Result.Error = FString::Printf(
						TEXT("AngelscriptTestJIT fixture module '%s' is unavailable after compile."),
						*ModuleName.ToString());
					return false;
				}
				ScriptModules.Add(ModuleDesc->ScriptModule);
			}

			const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
				ESPMode::ThreadSafe> GenerationSnapshot =
				Engine.GetStaticJITGenerationSnapshot();
			if (!GenerationSnapshot.IsValid()
				|| !GenerationSnapshot->IsComplete())
			{
				Result.Error = TEXT(
					"AngelscriptTestJIT fixture compile produced no complete generation snapshot.");
				return false;
			}

			FAngelscriptJITProviderGenerationSettings Settings;
			Settings.TargetProfile =
				EAngelscriptJITTargetProfile::EditorDevelopment;
			Settings.ProviderId =
				AngelscriptTestJITProbes::GetStableProviderId();
			Settings.ProviderName = TEXT("AngelscriptTestJIT Fixtures");
			Settings.OwnerModuleName = TEXT("AngelscriptTestJIT");
			Settings.ArtifactProfile = GenerationSnapshot->ArtifactProfile;
			Settings.NativeEnvironmentFingerprint =
				FAngelscriptJITTargetProfiles::
					BuildCurrentNativeEnvironmentFingerprint(
						Settings.TargetProfile);

			FAngelscriptStaticJITProviderGenerationRequest Request;
			Request.Settings = Settings;
			Request.CompiledSourceGraph = ScriptModules;
			Request.GenerationSnapshot = GenerationSnapshot;
			FAngelscriptStaticJITGeneratorOutput GeneratorOutput;
			FString GenerationError;
			if (!GenerateStaticJITProviderArtifacts(
				Request, GeneratorOutput, &GenerationError))
			{
				Result.Error = MoveTemp(GenerationError);
				return false;
			}
			if (!GeneratorOutput.bHasProviderOutput)
			{
				Result.Error = TEXT(
					"AngelscriptTestJIT generation produced no eligible Provider output.");
				return false;
			}
			OutOutput = MoveTemp(GeneratorOutput.ProviderOutput);
			Result.ProviderId = OutOutput.ProviderId.Hash.ToHexString();
			Result.ProviderGeneration =
				OutOutput.ProviderGeneration.Hash.ToHexString();
			for (const FAngelscriptJITGeneratedModule& Module : OutOutput.Modules)
			{
				Result.GeneratedModules.Add(FString::Printf(
					TEXT("%s [%s] -> %s (%u functions)"),
					*Module.CanonicalModuleName,
					*Module.ModuleKey.Hash.ToHexString(),
					*Module.SourceRelativePath,
					Module.FunctionCount));
			}
			for (const FAngelscriptJITGeneratedFunction& Function :
				OutOutput.Functions)
			{
				Result.GeneratedFunctionKeys.Add(
					Function.FunctionKey.Hash.ToHexString());
			}
			return true;
#endif
		}

		static FString DescribeDifferences(
			const FAngelscriptJITGeneratedFileComparison& Comparison)
		{
			TArray<FString> Lines;
			for (const FAngelscriptJITGeneratedFileDifferenceRecord& Difference :
				Comparison.Differences)
			{
				Lines.Add(FString::Printf(TEXT("%s: %s"),
					*Difference.RelativePath, *Difference.Detail));
			}
			return FString::Join(Lines, TEXT("; "));
		}
	}

	FStaticJITAotFixtureEngineSession::~FStaticJITAotFixtureEngineSession()
	{
		Engine.Reset();
		if (FixtureRoot.Contains(
			TEXT("/Saved/Automation/AngelscriptTestJITGeneration/")))
		{
			IFileManager::Get().DeleteDirectory(*FixtureRoot, false, true);
		}
	}

	FStaticJITAotCacheV2RestoreResult
	FStaticJITAotFixtureEngineSession::RestoreFreshEngineFromCacheV2()
	{
		FStaticJITAotCacheV2RestoreResult Result;
		const auto Fail = [&Result](const FString& Detail)
		{
			Result.Detail = Detail;
			return Result;
		};

		if (!IsValid())
		{
			return Fail(TEXT(
				"StaticJIT Cache V2 restore requires a valid source fixture Engine."));
		}

		const FAngelscriptCacheFlushApiResult Flush =
			FlushAngelscriptCacheToStore(Engine.Get(), 5.0);
		if (!Flush.IsSuccess() || !Flush.Flush.Current.IsSuccess())
		{
			return Fail(FString::Printf(
				TEXT("StaticJIT Cache V2 producer flush failed: ApiError=%u LifecycleError=%u Detail=%s"),
				static_cast<uint32>(Flush.Error),
				static_cast<uint32>(Flush.Flush.Error),
				*Flush.Detail));
		}
		const FAngelscriptHash256 ProducerGeneration =
			Flush.Flush.Current.GenerationId;
		int32 ExpectedProviderFunctionCount = 0;
		const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
			ESPMode::ThreadSafe> ProviderSnapshot =
				FAngelscriptJITProviderRegistry::Get().GetSnapshot();
		if (ProviderSnapshot.IsValid())
		{
			for (const TSharedPtr<const FAngelscriptJITProviderCatalog,
				ESPMode::ThreadSafe>& Provider : ProviderSnapshot->Providers)
			{
				if (Provider.IsValid()
					&& Provider->ProviderId
						== AngelscriptTestJITProbes::GetStableProviderId())
				{
					ExpectedProviderFunctionCount = Provider->Entries.Num();
					break;
				}
			}
		}
		if (ExpectedProviderFunctionCount <= 0)
		{
			return Fail(TEXT(
				"StaticJIT Cache V2 proof could not resolve the committed TestJIT Provider entry count."));
		}

		// The producer and all of its Engine-local function/reference objects are
		// gone before the consumer is allocated. Only source files and the
		// persisted Cache V2 generation remain below FixtureRoot.
		Engine.Reset();

		FString CreateError;
		TUniquePtr<FAngelscriptEngine> RestoredEngine =
			CreateUncompiledFixtureEngine(
				FixtureRoot, CreateError, true,
				EAngelscriptEnginePurpose::Runtime);
		if (!RestoredEngine.IsValid())
		{
			Error = CreateError;
			return Fail(CreateError);
		}
		FAngelscriptCacheService* const CacheService =
			RestoredEngine->GetCacheService();
		if (CacheService != nullptr)
		{
			// Preserve the complete two-module lookup sequence plus the subsequent
			// stable-route publication events so a late failure still names its
			// original function-level rejection.
			CacheService->ConfigureDecisionTrace(true, 512);
		}

		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle Listener =
			FAngelscriptCompilationEvents::RegisterListener(
				[&Events](const FAngelscriptCompilationEvent& Event)
				{
					Events.Add(Event);
				});
		{
			ON_SCOPE_EXIT
			{
				FAngelscriptCompilationEvents::UnregisterListener(Listener);
			};
			FAngelscriptEngineScope EngineScope(*RestoredEngine);
			RestoredEngine->InitialCompile();
		}
		Result.FrontendEventCount = CountFrontendEvents(Events);

		const TSharedPtr<FAngelscriptModuleDesc> ImportModule =
			RestoredEngine->GetModuleByModuleName(
				AngelscriptStaticJITAotFixture::
					GetImportProviderModuleName().ToString());
		const TSharedPtr<FAngelscriptModuleDesc> PrimaryModule =
			RestoredEngine->GetModuleByModuleName(
				AngelscriptStaticJITAotFixture::GetModuleName().ToString());
		const FAngelscriptCacheFunctionReuseSummary FunctionReuse =
			CacheService != nullptr
				? CacheService->CaptureFunctionReuseSummary()
				: FAngelscriptCacheFunctionReuseSummary{};
		Result.bConsumedPersistedCandidate = FunctionReuse.bPresent;
		Result.CandidateModuleCount = static_cast<int32>(
			FunctionReuse.CandidateModuleCount);
		Result.RestoredFunctionCount = static_cast<int32>(
			FunctionReuse.RestoredFunctionCount);
		Result.CompiledMissCount = static_cast<int32>(
			FunctionReuse.CompiledMissCount);
		Result.NotCacheableCount = static_cast<int32>(
			FunctionReuse.NotCacheableCount);

		const FAngelscriptCacheLifecyclePublications Publications =
			CacheService != nullptr
				? CacheService->GetLifecyclePublications()
				: FAngelscriptCacheLifecyclePublications{};
		const FAngelscriptHash256 CandidateGeneration =
			FunctionReuse.CandidateGenerationId;
		Result.GenerationId = CandidateGeneration.ToHexString();
		TArray<FString> DecisionParts;
		if (CacheService != nullptr)
		{
			const FAngelscriptCacheDecisionTraceSnapshot Trace =
				CacheService->CaptureDecisionTrace();
			for (const FAngelscriptCacheDecisionEvent& Event : Trace.Events)
			{
				const bool bStartupDecision =
					Event.Stage
						== EAngelscriptCacheDecisionStage::StartupSelection
					|| Event.Stage
						== EAngelscriptCacheDecisionStage::StartupRestore;
				const bool bFailedFunctionDecision =
					Event.Stage
						== EAngelscriptCacheDecisionStage::FunctionLookup
					&& Event.Outcome
						!= EAngelscriptCacheDecisionOutcome::Restored;
				if (!bStartupDecision && !bFailedFunctionDecision)
				{
					continue;
				}
				DecisionParts.Add(FString::Printf(
					TEXT("stage=%u outcome=%u domain=%u reason=%u expected=%s current=%s detail=%s"),
					static_cast<uint32>(Event.Stage),
					static_cast<uint32>(Event.Outcome),
					static_cast<uint32>(Event.ReasonDomain),
					Event.ReasonCode,
					Event.ExpectedCoordinate.IsSet()
						? *Event.ExpectedCoordinate->ToHexString() : TEXT("none"),
					Event.CurrentCoordinate.IsSet()
						? *Event.CurrentCoordinate->ToHexString() : TEXT("none"),
					*Event.Detail));
			}
		}
		const FString DecisionDetail = DecisionParts.IsEmpty()
			? TEXT("no startup or failed function decision events")
			: FString::Join(DecisionParts, TEXT(" | "));

		// The primary fixture deliberately contains reflected classes and many
		// functions. Cache V2's executable exact-start vertical currently accepts
		// only its narrow enum/global shape, so this production-shaped module uses
		// the normal typed hybrid path: select the persisted Generation, rebuild
		// current module/class authority, and restore every cacheable function body
		// into the fresh Engine's own function objects. Do not misreport that as an
		// exact whole-module bLoadedIncrementalCache restoration.
		if (!ImportModule.IsValid() || !PrimaryModule.IsValid()
			|| !Result.bConsumedPersistedCandidate
			|| Result.CandidateModuleCount != 2
			|| Result.RestoredFunctionCount != ExpectedProviderFunctionCount
			|| CandidateGeneration != ProducerGeneration
			|| !Publications.Current.IsValid()
			|| Publications.Current->bRestoredFromStore)
		{
			return Fail(FString::Printf(
				TEXT("StaticJIT Cache V2 fresh Engine reuse rejected: Producer=%s Candidate=%s Consumed=%d Modules=%d RestoredFunctions=%d/%d CompiledMisses=%d NotCacheable=%d ExactModules=%d FrontendEvents=%d Decisions=[%s]"),
				*ProducerGeneration.ToHexString(),
				*CandidateGeneration.ToHexString(),
				Result.bConsumedPersistedCandidate ? 1 : 0,
				Result.CandidateModuleCount,
				Result.RestoredFunctionCount,
				ExpectedProviderFunctionCount,
				Result.CompiledMissCount,
				Result.NotCacheableCount,
				Publications.Current.IsValid()
					&& Publications.Current->bRestoredFromStore ? 1 : 0,
				Result.FrontendEventCount,
				*DecisionDetail));
		}

		Engine = MoveTemp(RestoredEngine);
		Error.Reset();
		Result.bSuccess = true;
		Result.Detail = FString::Printf(
			TEXT("StaticJIT Cache V2 fresh Engine reused: Generation=%s Modules=%d RestoredFunctions=%d CompiledMisses=%d NotCacheable=%d FrontendEvents=%d"),
			*Result.GenerationId,
			Result.CandidateModuleCount,
			Result.RestoredFunctionCount,
			Result.CompiledMissCount,
			Result.NotCacheableCount,
			Result.FrontendEventCount);
		return Result;
	}

	TUniquePtr<FStaticJITAotFixtureEngineSession>
	CreateFixtureEngineSession(const bool bEnableCacheV2Persistence)
	{
		TUniquePtr<FStaticJITAotFixtureEngineSession> Session =
			MakeUnique<FStaticJITAotFixtureEngineSession>();
		FStaticJITAotGenerationResult CreationResult;
		Session->Engine = CreateFixtureEngine(
			Session->FixtureRoot, CreationResult,
			bEnableCacheV2Persistence,
			EAngelscriptEnginePurpose::Runtime);
		Session->Error = MoveTemp(CreationResult.Error);
		return Session;
	}

	FStaticJITAotGenerationResult Run(const EStaticJITAotGenerationMode Mode)
	{
		FStaticJITAotGenerationResult Result;
		TUniquePtr<FStaticJITAotFixtureEngineSession> Session =
			MakeUnique<FStaticJITAotFixtureEngineSession>();
		FStaticJITAotGenerationResult CreationResult;
		Session->Engine = CreateFixtureEngine(
			Session->FixtureRoot, CreationResult, false,
			EAngelscriptEnginePurpose::StaticJITGeneration);
		Session->Error = MoveTemp(CreationResult.Error);
		if (!Session.IsValid() || !Session->IsValid())
		{
			Result.Error = Session.IsValid()
				? Session->Error
				: TEXT("Failed to allocate the TestJIT fixture Engine session.");
			return Result;
		}

		FAngelscriptEngineScope EngineScope(*Session->Engine);
		FAngelscriptJITGenerationOutput Output;
		if (!BuildExpectedOutput(*Session->Engine, Output, Result))
		{
			return Result;
		}

		const FString OutputRoot = GetGeneratedProfileRoot();
		const TArray<FString> LegacyRoots = GetLegacyGeneratedProfileRoots();
		TArray<FAngelscriptJITOwnedOutputInspection> LegacyInspections;
		LegacyInspections.Reserve(LegacyRoots.Num());
		for (const FString& LegacyRoot : LegacyRoots)
		{
			const FAngelscriptJITOwnedOutputInspection Inspection =
				FAngelscriptJITGeneratedFileStore::InspectOwnedOutput(
					LegacyRoot, Output.TargetProfile, Output.ProviderId);
			LegacyInspections.Add(Inspection);
			if (!Inspection.bValid)
			{
				Result.Error = FString::Printf(
					TEXT("AngelscriptTestJIT legacy output '%s' cannot be migrated: %s"),
					*LegacyRoot, *Inspection.Error);
				return Result;
			}
		}
		if (Mode == EStaticJITAotGenerationMode::Generate)
		{
			const FAngelscriptJITGeneratedFilePublication Publication =
				FAngelscriptJITGeneratedFileStore::Publish(OutputRoot, Output);
			Result.bSuccess = Publication.bSuccess;
			Result.Error = Publication.Error;
			Result.WrittenFiles = Publication.WrittenFiles;
			Result.UnchangedFiles = Publication.UnchangedFiles;
			Result.RemovedFiles = Publication.RemovedFiles;
			for (int32 LegacyIndex = 0;
				Result.bSuccess && LegacyIndex < LegacyRoots.Num(); ++LegacyIndex)
			{
				if (!LegacyInspections[LegacyIndex].bExists)
				{
					continue;
				}
				const FAngelscriptJITOwnedOutputRetirement Retired =
					FAngelscriptJITGeneratedFileStore::RetireOwnedOutput(
						LegacyRoots[LegacyIndex], Output.TargetProfile,
						Output.ProviderId);
				Result.bSuccess = Retired.bSuccess;
				Result.Error = Retired.Error;
				Result.RemovedFiles.Append(Retired.RemovedFiles);
				Result.RemovedFiles.Sort();
			}
			if (Result.bSuccess)
			{
				RemoveEmptyLegacyPrivateWrappers();
			}
			return Result;
		}

		const FAngelscriptJITGeneratedFileComparison Comparison =
			FAngelscriptJITGeneratedFileStore::Compare(OutputRoot, Output);
		const bool bLegacyCurrent = LegacyInspections.ContainsByPredicate(
			[](const FAngelscriptJITOwnedOutputInspection& Inspection)
			{
				return !Inspection.bValid || Inspection.bExists;
			}) == false;
		Result.bSuccess = Comparison.IsCurrent() && bLegacyCurrent;
		if (!Result.bSuccess)
		{
			Result.Error = TEXT("AngelscriptTestJIT Verify found stale output: ")
				+ DescribeDifferences(Comparison);
			if (!bLegacyCurrent)
			{
				Result.Error +=
					TEXT("; legacy Private generated output remains or is invalid");
			}
		}
		return Result;
	}
}
