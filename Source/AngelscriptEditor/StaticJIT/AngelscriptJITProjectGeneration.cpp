#include "StaticJIT/AngelscriptJITProjectGeneration.h"

#include "AngelscriptEngine.h"
#include "Artifacts/AngelscriptFunctionRoute.h"
#include "StaticJIT/AngelscriptJITProjectScaffold.h"
#include "StaticJIT/AngelscriptStaticJIT.h"
#include "StaticJIT/AngelscriptStaticJITGenerationSnapshot.h"
#include "StaticJIT/AngelscriptStaticJITGenerator.h"

#include "HAL/FileManager.h"
#include "GenericPlatform/GenericPlatformProperties.h"
#include "Misc/EngineVersion.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

namespace AngelscriptEditor::StaticJIT
{
	namespace
	{
		FString FullDirectory(const FStringView Directory)
		{
			FString Result(Directory);
			Result.TrimStartAndEndInline();
			if (Result.IsEmpty())
			{
				return {};
			}
			Result = FPaths::ConvertRelativePathToFull(Result);
			FPaths::NormalizeDirectoryName(Result);
			return Result;
		}

		bool IsSameOrNested(const FString& Candidate, const FString& Parent)
		{
			return Candidate == Parent
				|| FPaths::IsUnderDirectory(Candidate, Parent);
		}

		const TCHAR* DifferenceKindName(
			const EAngelscriptJITGeneratedFileDifference Kind)
		{
			switch (Kind)
			{
			case EAngelscriptJITGeneratedFileDifference::Missing:
				return TEXT("Missing");
			case EAngelscriptJITGeneratedFileDifference::ContentMismatch:
				return TEXT("ContentMismatch");
			case EAngelscriptJITGeneratedFileDifference::UnexpectedOwnedFile:
				return TEXT("UnexpectedOwnedFile");
			case EAngelscriptJITGeneratedFileDifference::OwnershipConflict:
				return TEXT("OwnershipConflict");
			case EAngelscriptJITGeneratedFileDifference::TargetProfileMismatch:
				return TEXT("TargetProfileMismatch");
			case EAngelscriptJITGeneratedFileDifference::ProviderIdMismatch:
				return TEXT("ProviderIdMismatch");
			case EAngelscriptJITGeneratedFileDifference::InvalidInventory:
				return TEXT("InvalidInventory");
			case EAngelscriptJITGeneratedFileDifference::InvalidExpectedOutput:
				return TEXT("InvalidExpectedOutput");
			case EAngelscriptJITGeneratedFileDifference::ReadFailure:
				return TEXT("ReadFailure");
			default:
				return TEXT("Unknown");
			}
		}

		FString OneLine(FString Value)
		{
			Value.ReplaceInline(TEXT("\r"), TEXT("\\r"));
			Value.ReplaceInline(TEXT("\n"), TEXT("\\n"));
			return Value;
		}

		FString BuildVerificationReport(
			const FAngelscriptJITGenerationOutput& Output,
			const FAngelscriptJITGeneratedFileComparison& Comparison)
		{
			FString Report;
			Report += FString::Printf(TEXT("Status=%s\n"),
				Comparison.IsCurrent() ? TEXT("Current") : TEXT("Stale"));
			Report += FString::Printf(TEXT("ProviderId=%s\n"),
				*Output.ProviderId.Hash.ToHexString());
			Report += FString::Printf(TEXT("ProviderGeneration=%s\n"),
				*Output.ProviderGeneration.Hash.ToHexString());
			Report += FString::Printf(TEXT("Profile=%s\n"),
				FAngelscriptJITTargetProfiles::ToString(Output.TargetProfile));
			Report += FString::Printf(TEXT("ProviderAbiRevision=%u\n"),
				FAngelscriptJITProviderAbi::Revision);
			Report += FString::Printf(TEXT("ArtifactSetDigest=%s\n"),
				*Output.ArtifactSetDigest.ToHexString());
			Report += FString::Printf(TEXT("ArtifactProfile=%s\n"),
				*Output.ArtifactProfile.Hash.ToHexString());
			Report += FString::Printf(TEXT("NativeEnvironment=%s\n"),
				*Output.NativeEnvironmentFingerprint.ToHexString());
			Report += FString::Printf(TEXT("FunctionCount=%d\n"),
				Output.Functions.Num());
			Report += FString::Printf(TEXT("ModuleCount=%d\n"),
				Output.Modules.Num());
			for (const FAngelscriptJITGeneratedModule& Module : Output.Modules)
			{
				Report += FString::Printf(
					TEXT("Module StableModuleKey=%s CanonicalModuleName=%s VirtualSourcePath=%s Source=%s ContentHash=%s FunctionCount=%u\n"),
					*Module.ModuleKey.Hash.ToHexString(),
					*OneLine(Module.CanonicalModuleName),
					*OneLine(Module.VirtualSourcePath),
					*Module.SourceRelativePath,
					*Module.ContentHash.ToHexString(),
					Module.FunctionCount);
			}
			for (const FAngelscriptJITGeneratedFunction& Function :
				Output.Functions)
			{
				Report += FString::Printf(
					TEXT("Function StableFunctionKey=%s ModuleKey=%s Declaration=%s Source=%s:%u:%u ExecutionHash=%s DebugHash=%s EntryAbiHash=%s ModuleSource=%s Symbol=%s ReferenceCount=%d\n"),
					*Function.FunctionKey.Hash.ToHexString(),
					*Function.ModuleKey.Hash.ToHexString(),
					*OneLine(Function.CanonicalDeclaration),
					*OneLine(Function.SourceSection),
					Function.SourceLine,
					Function.SourceColumn,
					*Function.ExecutionHash.ToHexString(),
					*Function.DebugHash.ToHexString(),
					*Function.EntryAbiHash.ToHexString(),
					*Function.ModuleSourceRelativePath,
					*Function.SymbolPrefix,
					Function.ReferenceSlots.Num());
			}
			Report += FString::Printf(TEXT("DifferenceCount=%d\n"),
				Comparison.Differences.Num());
			for (const FAngelscriptJITGeneratedFileDifferenceRecord& Difference :
				Comparison.Differences)
			{
				Report += FString::Printf(
					TEXT("Difference Kind=%s Path=%s ExpectedHash=%s ActualHash=%s Detail=%s\n"),
					DifferenceKindName(Difference.Kind),
					*Difference.RelativePath,
					*Difference.ExpectedHash.ToHexString(),
					*Difference.ActualHash.ToHexString(),
					*OneLine(Difference.Detail));
			}
			return Report;
		}

		void PopulateModuleSourceSetChanges(
			const FAngelscriptJITGenerationOutput& Output,
			const FAngelscriptJITGeneratedFileComparison& Comparison,
			FAngelscriptJITProjectGenerationResult& OutResult)
		{
			for (const FAngelscriptJITGeneratedFileDifferenceRecord& Difference :
				Comparison.Differences)
			{
				if (Difference.Kind
					== EAngelscriptJITGeneratedFileDifference::Missing
					&& Output.Modules.ContainsByPredicate(
						[Path = Difference.RelativePath](
							const FAngelscriptJITGeneratedModule& Module)
						{
							return Module.SourceRelativePath == Path;
						}))
				{
					OutResult.AddedModuleSources.Add(Difference.RelativePath);
				}
				else if (Difference.Kind
						== EAngelscriptJITGeneratedFileDifference::UnexpectedOwnedFile
					&& Difference.RelativePath.EndsWith(TEXT(".jit.cpp")))
				{
					OutResult.RemovedModuleSources.Add(Difference.RelativePath);
				}
			}
			OutResult.AddedModuleSources.Sort();
			OutResult.RemovedModuleSources.Sort();
		}

		struct FLegacyProfileOutputRoot
		{
			FString Directory;
			FString DisplayRoot;
		};

		TArray<FLegacyProfileOutputRoot> LegacyProfileOutputDirectories(
			const FStringView OutputDirectory,
			const EAngelscriptJITTargetProfile TargetProfile)
		{
			TArray<FLegacyProfileOutputRoot> Result;
			const FString OutputRoot = FullDirectory(OutputDirectory);
			if (OutputRoot.IsEmpty()
				|| FPaths::GetCleanFilename(OutputRoot)
					!= FAngelscriptJITTargetProfiles::ToString(TargetProfile))
			{
				return Result;
			}
			const FString GeneratedRoot = FPaths::GetPath(OutputRoot);
			if (FPaths::GetCleanFilename(GeneratedRoot) != TEXT("Generated"))
			{
				return Result;
			}
			const FString ModuleRoot = FPaths::GetPath(GeneratedRoot);
			const FString ProfileName =
				FAngelscriptJITTargetProfiles::ToString(TargetProfile);
			FLegacyProfileOutputRoot& Direct = Result.AddDefaulted_GetRef();
			Direct.Directory = FPaths::Combine(
				ModuleRoot, TEXT("Private/Generated"), ProfileName);
			Direct.DisplayRoot = FPaths::Combine(
				TEXT("Private/Generated"), ProfileName);
			FPaths::NormalizeDirectoryName(Direct.Directory);
			FPaths::NormalizeFilename(Direct.DisplayRoot);

			FLegacyProfileOutputRoot& Profiles = Result.AddDefaulted_GetRef();
			Profiles.Directory = FPaths::Combine(
				ModuleRoot, TEXT("Private/Generated/Profiles"), ProfileName);
			Profiles.DisplayRoot = FPaths::Combine(
				TEXT("Private/Generated/Profiles"), ProfileName);
			FPaths::NormalizeDirectoryName(Profiles.Directory);
			FPaths::NormalizeFilename(Profiles.DisplayRoot);
			return Result;
		}

		FString LegacyDisplayPath(
			const FLegacyProfileOutputRoot& LegacyRoot,
			const FStringView RelativePath)
		{
			FString Result = FPaths::Combine(
				LegacyRoot.DisplayRoot,
				FString(RelativePath));
			FPaths::NormalizeFilename(Result);
			return Result;
		}

		void RemoveEmptyLegacyPrivateWrappers(
			const TConstArrayView<FLegacyProfileOutputRoot> LegacyRoots)
		{
			if (LegacyRoots.IsEmpty())
			{
				return;
			}
			const FString PrivateGeneratedRoot = FPaths::GetPath(
				LegacyRoots[0].Directory);
			const FString PrivateRoot = FPaths::GetPath(PrivateGeneratedRoot);
			IFileManager::Get().DeleteDirectory(
				*FPaths::Combine(PrivateGeneratedRoot, TEXT("Profiles")),
				false, false);
			IFileManager::Get().DeleteDirectory(
				*PrivateGeneratedRoot, false, false);
			IFileManager::Get().DeleteDirectory(*PrivateRoot, false, false);
		}

		void AddLegacyDifference(
			FAngelscriptJITGeneratedFileComparison& Comparison,
			const EAngelscriptJITGeneratedFileDifference Kind,
			const FString& RelativePath,
			const FString& Detail)
		{
			FAngelscriptJITGeneratedFileDifferenceRecord& Difference =
				Comparison.Differences.AddDefaulted_GetRef();
			Difference.Kind = Kind;
			Difference.RelativePath = RelativePath;
			Difference.Detail = Detail;
		}
	}

	FString FAngelscriptJITProjectGeneration::GetProfileOutputDirectory(
		const FStringView ProjectFile,
		const EAngelscriptJITTargetProfile TargetProfile)
	{
		FString Descriptor(ProjectFile);
		Descriptor.TrimStartAndEndInline();
		if (Descriptor.IsEmpty()
			|| !FAngelscriptJITTargetProfiles::GetTraits(
				TargetProfile).IsSet())
		{
			return {};
		}
		Descriptor = FPaths::ConvertRelativePathToFull(Descriptor);
		FPaths::NormalizeFilename(Descriptor);
		FString Result = FPaths::Combine(
			FPaths::GetPath(Descriptor),
			TEXT("Source/AngelscriptJIT/Generated"),
			FAngelscriptJITTargetProfiles::ToString(TargetProfile));
		Result = FPaths::ConvertRelativePathToFull(Result);
		FPaths::NormalizeDirectoryName(Result);
		return Result;
	}

	bool FAngelscriptJITProjectGeneration::PrepareProjectProfileOutput(
		const FAngelscriptJITGenerationOutput& RuntimeOutput,
		FAngelscriptJITGenerationOutput& OutProjectOutput,
		FString* OutError)
	{
		OutProjectOutput = RuntimeOutput;
		for (FAngelscriptJITGeneratedFile& File : OutProjectOutput.Files)
		{
			if (File.Kind
				== EAngelscriptJITGeneratedFileKind::ProviderSource)
			{
				File.RelativePath = TEXT("Provider.generated.inl");
			}
		}
		return FAngelscriptJITGeneration::RefreshOwnedFileInventory(
			OutProjectOutput, OutError);
	}

	bool FAngelscriptJITProjectGeneration::BuildProjectArtifacts(
		const FStringView ProjectFile,
		const EAngelscriptJITTargetProfile TargetProfile,
		FAngelscriptJITGenerationOutput& OutGeneratedOutput,
		FString* OutError)
	{
		OutGeneratedOutput = {};
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
			TEXT("Project StaticJIT generation is unavailable in this host build."));
#else
		const TOptional<FAngelscriptJITTargetProfileTraits> Traits =
			FAngelscriptJITTargetProfiles::GetTraits(TargetProfile);
		FString Descriptor(ProjectFile);
		Descriptor.TrimStartAndEndInline();
		if (!Traits.IsSet() || Descriptor.IsEmpty())
		{
			return Fail(
				TEXT("Project StaticJIT generation requires one concrete target profile and project descriptor."));
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

		const FString TemporaryCacheRoot = FPaths::Combine(
			ProjectDirectory,
			TEXT("Saved/Angelscript/JITGeneration"),
			FAngelscriptJITTargetProfiles::ToString(TargetProfile),
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
		// A project Provider owns only the host Script/ domain. Enabled plugin
		// Providers remain separate Registry entries.
		Dependencies.GetEnabledPluginScriptRoots = []()
		{
			return TArray<FString>();
		};
		Dependencies.GetEnabledPluginScriptRootDescriptors = []()
		{
			return TArray<FAngelscriptPluginScriptRoot>();
		};

		Engine = FAngelscriptEngine::Create(Config, Dependencies);
		if (!Engine.IsValid())
		{
			return Fail(FString::Printf(
				TEXT("Project scripts failed to compile for %s."),
				FAngelscriptJITTargetProfiles::ToString(TargetProfile)));
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
				FAngelscriptJITTargetProfiles::ToString(TargetProfile)));
		}

		const TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
			ESPMode::ThreadSafe> GenerationSnapshot =
			Engine->GetStaticJITGenerationSnapshot();
		if (!GenerationSnapshot.IsValid()
			|| !GenerationSnapshot->IsComplete())
		{
			return Fail(
				TEXT("Project compile produced no complete StaticJIT generation snapshot."));
		}

		FAngelscriptJITProviderGenerationSettings Settings;
		Settings.TargetProfile = TargetProfile;
		Settings.ProviderId =
			FAngelscriptJITProjectScaffold::BuildProviderId(Descriptor);
		Settings.ProviderName = FPaths::GetBaseFilename(Descriptor)
			+ TEXT(" StaticJIT");
		Settings.OwnerModuleName = FAngelscriptJITProjectScaffold::ModuleName;
		Settings.ArtifactProfile = GenerationSnapshot->ArtifactProfile;
		Settings.NativeEnvironmentFingerprint =
			FAngelscriptJITTargetProfiles::
				BuildCurrentNativeEnvironmentFingerprint(TargetProfile);

		FAngelscriptStaticJITProviderGenerationRequest Request;
		Request.Settings = Settings;
		Request.CompiledSourceGraph = ScriptModules;
		Request.GenerationSnapshot = GenerationSnapshot;
		FAngelscriptStaticJITGeneratorOutput GeneratorOutput;
		FString GenerationError;
		if (!GenerateStaticJITProviderArtifacts(
			Request, GeneratorOutput, &GenerationError))
		{
			return Fail(GenerationError);
		}
		if (!GeneratorOutput.bHasProviderOutput)
		{
			return Fail(
				TEXT("Project StaticJIT generation produced no eligible Provider output."));
		}
		OutGeneratedOutput = MoveTemp(GeneratorOutput.ProviderOutput);
		if (OutError != nullptr)
		{
			OutError->Reset();
		}
		return true;
#endif
	}

	FAngelscriptJITProjectGenerationResult
	FAngelscriptJITProjectGeneration::Generate(
		const FStringView OutputDirectory,
		const FAngelscriptJITGenerationOutput& GeneratedOutput)
	{
		FAngelscriptJITProjectGenerationResult Result;
		const TArray<FLegacyProfileOutputRoot> LegacyRoots =
			LegacyProfileOutputDirectories(
			OutputDirectory, GeneratedOutput.TargetProfile);
		TArray<FAngelscriptJITOwnedOutputInspection> LegacyInspections;
		LegacyInspections.Reserve(LegacyRoots.Num());
		for (const FLegacyProfileOutputRoot& LegacyRoot : LegacyRoots)
		{
			const FAngelscriptJITOwnedOutputInspection Inspection =
				FAngelscriptJITGeneratedFileStore::InspectOwnedOutput(
					LegacyRoot.Directory, GeneratedOutput.TargetProfile,
					GeneratedOutput.ProviderId);
			LegacyInspections.Add(Inspection);
			if (!Inspection.bValid)
			{
				Result.Comparison = FAngelscriptJITGeneratedFileStore::Compare(
					OutputDirectory, GeneratedOutput);
				AddLegacyDifference(Result.Comparison,
					EAngelscriptJITGeneratedFileDifference::InvalidInventory,
					LegacyDisplayPath(LegacyRoot,
						TEXT("OwnedFiles.generated.json")),
					Inspection.Error);
				Result.Error = FString::Printf(
					TEXT("Refusing StaticJIT profile migration from '%s': %s"),
					*LegacyRoot.DisplayRoot, *Inspection.Error);
				return Result;
			}
		}
		Result.Publication = FAngelscriptJITGeneratedFileStore::Publish(
			OutputDirectory, GeneratedOutput);
		Result.Comparison = Result.Publication.Comparison;
		Result.AddedModuleSources = Result.Publication.AddedModuleSources;
		Result.RemovedModuleSources = Result.Publication.RemovedModuleSources;
		Result.Error = Result.Publication.Error;
		Result.bSuccess = Result.Publication.bSuccess;
		for (int32 LegacyIndex = 0;
			Result.bSuccess && LegacyIndex < LegacyRoots.Num(); ++LegacyIndex)
		{
			if (!LegacyInspections[LegacyIndex].bExists)
			{
				continue;
			}
			const FLegacyProfileOutputRoot& LegacyRoot =
				LegacyRoots[LegacyIndex];
			const FAngelscriptJITOwnedOutputRetirement Retired =
				FAngelscriptJITGeneratedFileStore::RetireOwnedOutput(
					LegacyRoot.Directory, GeneratedOutput.TargetProfile,
					GeneratedOutput.ProviderId);
			if (!Retired.bSuccess)
			{
				Result.bSuccess = false;
				Result.Error = FString::Printf(
					TEXT("Generated the new StaticJIT profile but could not retire legacy '%s': %s"),
					*LegacyRoot.DisplayRoot, *Retired.Error);
				return Result;
			}
			for (const FString& RelativePath : Retired.RemovedModuleSources)
			{
				Result.RemovedModuleSources.Add(
					LegacyDisplayPath(LegacyRoot,
						RelativePath));
			}
		}
		if (Result.bSuccess)
		{
			Result.RemovedModuleSources.Sort();
			RemoveEmptyLegacyPrivateWrappers(LegacyRoots);
		}
		return Result;
	}

	FAngelscriptJITProjectGenerationResult
	FAngelscriptJITProjectGeneration::Verify(
		const FStringView OutputDirectory,
		const FStringView TemporaryRootDirectory,
		const FAngelscriptJITGenerationOutput& GeneratedOutput)
	{
		FAngelscriptJITProjectGenerationResult Result;
		const FString OutputRoot = FullDirectory(OutputDirectory);
		const FString TemporaryRoot = FullDirectory(TemporaryRootDirectory);
		if (TemporaryRoot.IsEmpty()
			|| OutputRoot.IsEmpty()
			|| IsSameOrNested(TemporaryRoot, OutputRoot)
			|| IsSameOrNested(OutputRoot, TemporaryRoot))
		{
			Result.Error =
				TEXT("StaticJIT Verify requires separate output and temporary roots.");
			return Result;
		}

		Result.VerificationDirectory = FPaths::Combine(
			TemporaryRoot,
			TEXT("AngelscriptJITVerify"),
			GeneratedOutput.ProviderId.Hash.ToHexString(),
			FAngelscriptJITTargetProfiles::ToString(
				GeneratedOutput.TargetProfile),
			GeneratedOutput.ProviderGeneration.Hash.ToHexString(),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
		FPaths::NormalizeDirectoryName(Result.VerificationDirectory);
		if (!FPaths::IsUnderDirectory(
			Result.VerificationDirectory, TemporaryRoot))
		{
			Result.Error = TEXT("StaticJIT Verify temporary path escaped its root.");
			return Result;
		}

		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(
				*Result.VerificationDirectory, false, true);
		};

		const FAngelscriptJITGeneratedFilePublication Staged =
			FAngelscriptJITGeneratedFileStore::Publish(
				Result.VerificationDirectory, GeneratedOutput);
		if (!Staged.bSuccess)
		{
			Result.Error = FString::Printf(
				TEXT("StaticJIT Verify could not materialize isolated expected output: %s"),
				*Staged.Error);
			return Result;
		}
		const FAngelscriptJITGeneratedFileComparison StagedComparison =
			FAngelscriptJITGeneratedFileStore::Compare(
				Result.VerificationDirectory, GeneratedOutput);
		if (!StagedComparison.IsCurrent())
		{
			Result.Error =
				TEXT("StaticJIT Verify isolated expected output was not deterministic.");
			return Result;
		}

		Result.Comparison = FAngelscriptJITGeneratedFileStore::Compare(
			OutputRoot, GeneratedOutput);
		PopulateModuleSourceSetChanges(
			GeneratedOutput, Result.Comparison, Result);
		const TArray<FLegacyProfileOutputRoot> LegacyRoots =
			LegacyProfileOutputDirectories(
				OutputRoot, GeneratedOutput.TargetProfile);
		for (const FLegacyProfileOutputRoot& LegacyRoot : LegacyRoots)
		{
			const FAngelscriptJITOwnedOutputInspection LegacyInspection =
				FAngelscriptJITGeneratedFileStore::InspectOwnedOutput(
					LegacyRoot.Directory, GeneratedOutput.TargetProfile,
					GeneratedOutput.ProviderId);
			if (!LegacyInspection.bValid)
			{
				AddLegacyDifference(Result.Comparison,
					EAngelscriptJITGeneratedFileDifference::InvalidInventory,
					LegacyDisplayPath(LegacyRoot,
						TEXT("OwnedFiles.generated.json")),
					LegacyInspection.Error);
			}
			else if (LegacyInspection.bExists)
			{
				for (const FString& RelativePath : LegacyInspection.OwnedFiles)
				{
					AddLegacyDifference(Result.Comparison,
						EAngelscriptJITGeneratedFileDifference::UnexpectedOwnedFile,
						LegacyDisplayPath(LegacyRoot,
							RelativePath),
						TEXT("Legacy Private generated output remains and requires Generate migration."));
				}
				for (const FString& RelativePath : LegacyInspection.ModuleSources)
				{
					Result.RemovedModuleSources.Add(
						LegacyDisplayPath(LegacyRoot,
							RelativePath));
				}
			}
		}
		Result.RemovedModuleSources.Sort();
		Result.bSuccess = Result.Comparison.IsCurrent();
		Result.Report = BuildVerificationReport(
			GeneratedOutput, Result.Comparison);
		if (!Result.bSuccess)
		{
			Result.Error = Result.Report;
		}
		return Result;
	}
}
