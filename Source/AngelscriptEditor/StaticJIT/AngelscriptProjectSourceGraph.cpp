#include "StaticJIT/AngelscriptProjectSourceGraph.h"

#include "AngelscriptEngine.h"
#include "StaticJIT/AngelscriptStaticJITGenerationProfile.h"
#include "StaticJIT/AngelscriptStaticJITGenerationSnapshot.h"

#include "HAL/FileManager.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

namespace AngelscriptEditor::StaticJIT
{
	bool FAngelscriptProjectSourceGraph::Compile(
		const FAngelscriptProjectSourceGraphCompileRequest& Request,
		FAngelscriptProjectSourceGraphConsumer Consumer,
		FString* OutError)
	{
		auto Fail = [OutError](const FString& Message)
		{
			if (OutError != nullptr)
			{
				*OutError = Message;
			}
			return false;
		};

#if !AS_CAN_GENERATE_JIT
		return Fail(
			TEXT("Project source-graph compilation is unavailable in this host build."));
#else
		const TOptional<FAngelscriptJITTargetProfileTraits> Traits =
			FAngelscriptJITTargetProfiles::GetTraits(Request.TargetProfile);
		FString Descriptor = Request.ProjectFile;
		Descriptor.TrimStartAndEndInline();
		if (!Traits.IsSet() || Descriptor.IsEmpty() || !Consumer)
		{
			return Fail(
				TEXT("Project source-graph compilation requires one concrete target profile, project descriptor, and synchronous consumer."));
		}
		if (Request.CaptureProfile
			!= EAngelscriptStaticJITCaptureProfile::Bytecode
			&& Request.CaptureProfile
				!= EAngelscriptStaticJITCaptureProfile::VerifiedTypedHIR)
		{
			return Fail(TEXT("Project source-graph capture profile is invalid."));
		}
		Descriptor = FPaths::ConvertRelativePathToFull(Descriptor);
		FPaths::NormalizeFilename(Descriptor);
		if (!IFileManager::Get().FileExists(*Descriptor))
		{
			return Fail(FString::Printf(
				TEXT("Project descriptor does not exist: %s."), *Descriptor));
		}
		const FString ProjectDirectory = FPaths::GetPath(Descriptor);
		const FString ScriptRoot = FPaths::Combine(
			ProjectDirectory, TEXT("Script"));
		if (!IFileManager::Get().DirectoryExists(*ScriptRoot))
		{
			return Fail(FString::Printf(
				TEXT("Project Script root does not exist: %s."), *ScriptRoot));
		}

		const TCHAR* ScratchOwner = Request.RequestKind
			== EAngelscriptProjectSourceGraphRequestKind::DeveloperHIRDump
			? TEXT("HIRDump") : TEXT("JITGeneration");
		const FString TemporaryCacheRoot = FPaths::Combine(
			ProjectDirectory,
			TEXT("Saved/Angelscript"),
			ScratchOwner,
			TEXT("Scratch"),
			FAngelscriptJITTargetProfiles::ToString(Request.TargetProfile),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
		TUniquePtr<FAngelscriptEngine> Engine;
		ON_SCOPE_EXIT
		{
			Engine.Reset();
			IFileManager::Get().DeleteDirectory(
				*TemporaryCacheRoot, false, true);
		};

		FAngelscriptEngineConfig Config;
		Config.Purpose = EAngelscriptEnginePurpose::StaticJITGeneration;
		Config.bCaptureTypedSemanticIR = Request.CaptureProfile
			== EAngelscriptStaticJITCaptureProfile::VerifiedTypedHIR;
		// Artifact generation needs native forms and reviewed call descriptors.
		// HIR dump stays dump-only and must not collect that metadata.
		if (Request.RequestKind
			== EAngelscriptProjectSourceGraphRequestKind::StaticJITArtifact)
		{
			FAngelscriptStaticJITGenerationProfile GenerationProfile;
			FString ProfileError;
			const FAngelscriptStaticJITBackendId BackendId =
				Request.CaptureProfile
					== EAngelscriptStaticJITCaptureProfile::VerifiedTypedHIR
				? FAngelscriptStaticJITBackendId::TypedAST()
				: FAngelscriptStaticJITBackendId::Bytecode();
			if (!FAngelscriptStaticJITGenerationProfile::TryCreate(
					BackendId,
					Request.TargetProfile,
					GenerationProfile,
					&ProfileError))
			{
				return Fail(ProfileError.IsEmpty()
					? TEXT("StaticJITArtifact compile could not freeze a generation profile.")
					: ProfileError);
			}
			GenerationProfile.ApplyToEngineConfig(Config);
		}
		Config.bSkipThreadedInitialize = true;
		Config.bIsEditor = Traits->bEditor;
		Config.bRunningCommandlet = true;
		Config.bIsUnattended = true;
		Config.bSimulateCooked = Traits->bUseCookedBindings;
		Config.bForcePreprocessEditorCode = Traits->bIncludeEditorScripts;
		Config.bDevelopmentMode = Traits->bIncludeDevelopmentScripts;
		Config.bOverrideTargetPreprocessorFlags = true;
		Config.bTargetEditor = Traits->bEditor;
		Config.bTargetEditorOnlyData = Traits->bEditorOnlyData;
		Config.bTargetRelease = Traits->bRelease;
		Config.bTargetTest = Traits->bTest;
		Config.bDisableCacheV2Persistence = true;
		Config.CacheV2RootOverride = TemporaryCacheRoot;

		FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		Dependencies.GetProjectDir = [ProjectDirectory]()
		{
			return ProjectDirectory;
		};
		// A project-owned request compiles only its host Script/ source graph.
		// Enabled plugin providers remain independent registry entries.
		Dependencies.GetEnabledPluginScriptRoots = []()
		{
			return TArray<FString>();
		};
		Dependencies.GetEnabledPluginScriptRootDescriptors = []()
		{
			return TArray<FAngelscriptPluginScriptRoot>();
		};
		if (Request.SourceProviderOverride.IsValid())
		{
			Dependencies.SourceProvider = Request.SourceProviderOverride;
		}
		// A contained developer/generation request reports compile failure to its
		// caller. It must never terminate the Editor/commandlet process that owns
		// the request; the caller maps the failure to its own typed exit code.
		Dependencies.HandleStartupCompileFailureExit =
			[](const FAngelscriptStartupCompileFailureExitRequest&)
			{
			};

		Engine = FAngelscriptEngine::Create(Config, Dependencies);
		if (!Engine.IsValid() || !Engine->DidInitialCompileSucceed())
		{
			return Fail(FString::Printf(
				TEXT("Project scripts failed to compile for %s."),
				FAngelscriptJITTargetProfiles::ToString(
					Request.TargetProfile)));
		}
		FAngelscriptEngineScope EngineScope(*Engine);
		const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules =
			Engine->GetActiveModules();
		TArray<asIScriptModule*> ScriptModules;
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
		{
			if (!Module->bCompileError && Module->ScriptModule != nullptr)
			{
				ScriptModules.Add(reinterpret_cast<asIScriptModule*>(
					Module->ScriptModule));
			}
		}
		if (ScriptModules.IsEmpty())
		{
			return Fail(FString::Printf(
				TEXT("Project Script root compiled no modules for %s."),
				FAngelscriptJITTargetProfiles::ToString(
					Request.TargetProfile)));
		}

		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> Snapshot =
			Engine->GetStaticJITGenerationSnapshot();
		if (!Snapshot.IsValid() || !Snapshot->IsComplete())
		{
			return Fail(
				TEXT("Project compile produced no complete generation source-graph snapshot."));
		}
		if (Snapshot->CaptureProfile != Request.CaptureProfile)
		{
			return Fail(TEXT("Project source-graph capture profile mismatch."));
		}

		const FAngelscriptProjectSourceGraphCompileView View{
			*Engine,
			Descriptor,
			ProjectDirectory,
			ScriptModules,
			Snapshot.ToSharedRef(),
		};
		FString ConsumerError;
		if (!Consumer(View, &ConsumerError))
		{
			return Fail(ConsumerError.IsEmpty()
				? TEXT("Project source-graph consumer failed.")
				: ConsumerError);
		}
		if (OutError != nullptr)
		{
			OutError->Reset();
		}
		return true;
#endif
	}
}
