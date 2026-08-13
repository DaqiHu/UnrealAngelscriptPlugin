#include "StaticJIT/AngelscriptJITProjectScaffold.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace AngelscriptEditor::StaticJIT
{
	namespace
	{
		static constexpr TCHAR OwnershipPrefix[] =
			TEXT("// @angelscript-jit-scaffold revision=2 kind=");
		static constexpr TCHAR LegacyOwnershipPrefix[] =
			TEXT("// @angelscript-jit-scaffold revision=1 kind=");

		struct FScaffoldFile
		{
			FString RelativePath;
			FString Content;
		};

		struct FPendingWrite
		{
			FString DisplayPath;
			FString Destination;
			FString Temporary;
		};

		FString CanonicalPath(const FStringView Path)
		{
			FString Result(Path);
			Result.TrimStartAndEndInline();
			if (Result.IsEmpty())
			{
				return {};
			}
			Result = FPaths::ConvertRelativePathToFull(Result);
			FPaths::NormalizeFilename(Result);
			FPaths::CollapseRelativeDirectories(Result);
#if PLATFORM_WINDOWS
			Result.ToLowerInline();
#endif
			return Result;
		}

		FString FullProjectPath(const FStringView ProjectFile)
		{
			FString Result(ProjectFile);
			Result.TrimStartAndEndInline();
			if (Result.IsEmpty())
			{
				return {};
			}
			Result = FPaths::ConvertRelativePathToFull(Result);
			FPaths::NormalizeFilename(Result);
			FPaths::CollapseRelativeDirectories(Result);
			return Result;
		}

		FString ModuleDirectoryForProject(const FString& ProjectFile)
		{
			FString Result = FPaths::Combine(
				FPaths::GetPath(ProjectFile), TEXT("Source/AngelscriptJIT"));
			Result = FPaths::ConvertRelativePathToFull(Result);
			FPaths::NormalizeFilename(Result);
			return Result;
		}

		bool LoadProjectJson(
			const FString& ProjectFile,
			TSharedPtr<FJsonObject>& OutProject,
			FString& OutError)
		{
			FString Content;
			if (!FFileHelper::LoadFileToString(Content, *ProjectFile))
			{
				OutError = FString::Printf(
					TEXT("Could not read project descriptor '%s'."),
					*ProjectFile);
				return false;
			}
			const TSharedRef<TJsonReader<>> Reader =
				TJsonReaderFactory<>::Create(Content);
			if (!FJsonSerializer::Deserialize(Reader, OutProject)
				|| !OutProject.IsValid())
			{
				OutError = FString::Printf(
					TEXT("Project descriptor '%s' is not valid JSON."),
					*ProjectFile);
				return false;
			}
			return true;
		}

		bool FindModuleDescriptor(
			const TSharedPtr<FJsonObject>& Project,
			TSharedPtr<FJsonObject>& OutDescriptor,
			int32& OutCount)
		{
			OutDescriptor.Reset();
			OutCount = 0;
			const TArray<TSharedPtr<FJsonValue>>* Modules = nullptr;
			if (!Project->TryGetArrayField(TEXT("Modules"), Modules)
				|| Modules == nullptr)
			{
				return true;
			}
			for (const TSharedPtr<FJsonValue>& Value : *Modules)
			{
				const TSharedPtr<FJsonObject> Module = Value.IsValid()
					? Value->AsObject() : nullptr;
				FString Name;
				if (Module.IsValid()
					&& Module->TryGetStringField(TEXT("Name"), Name)
					&& Name == FAngelscriptJITProjectScaffold::ModuleName)
				{
					++OutCount;
					OutDescriptor = Module;
				}
			}
			return true;
		}

		bool IsExpectedModuleDescriptor(
			const TSharedPtr<FJsonObject>& Descriptor)
		{
			if (!Descriptor.IsValid())
			{
				return false;
			}
			FString Type;
			FString LoadingPhase;
			return Descriptor->TryGetStringField(TEXT("Type"), Type)
				&& Descriptor->TryGetStringField(
					TEXT("LoadingPhase"), LoadingPhase)
				&& Type == TEXT("Runtime")
				&& LoadingPhase == TEXT("PreDefault");
		}

		void AddModuleDescriptor(const TSharedPtr<FJsonObject>& Project)
		{
			TArray<TSharedPtr<FJsonValue>> Modules;
			const TArray<TSharedPtr<FJsonValue>>* ExistingModules = nullptr;
			if (Project->TryGetArrayField(TEXT("Modules"), ExistingModules)
				&& ExistingModules != nullptr)
			{
				Modules = *ExistingModules;
			}
			const TSharedRef<FJsonObject> Module = MakeShared<FJsonObject>();
			Module->SetStringField(TEXT("Name"),
				FAngelscriptJITProjectScaffold::ModuleName);
			Module->SetStringField(TEXT("Type"), TEXT("Runtime"));
			Module->SetStringField(TEXT("LoadingPhase"), TEXT("PreDefault"));
			Modules.Add(MakeShared<FJsonValueObject>(Module));
			Project->SetArrayField(TEXT("Modules"), MoveTemp(Modules));
		}

		FString SerializeProject(const TSharedPtr<FJsonObject>& Project)
		{
			FString Content;
			const TSharedRef<TJsonWriter<TCHAR,
				TPrettyJsonPrintPolicy<TCHAR>>> Writer =
				TJsonWriterFactory<TCHAR,
					TPrettyJsonPrintPolicy<TCHAR>>::Create(&Content);
			FJsonSerializer::Serialize(Project.ToSharedRef(), Writer);
			Content += TEXT("\n");
			return Content;
		}

		FString BuildRulesContent()
		{
			return FString(OwnershipPrefix)
				+ TEXT("build-rules\n")
				+ TEXT("using UnrealBuildTool;\n\n")
				+ TEXT("public class AngelscriptJIT : ModuleRules\n")
				+ TEXT("{\n")
				+ TEXT("\tpublic AngelscriptJIT(ReadOnlyTargetRules Target) : base(Target)\n")
				+ TEXT("\t{\n")
				+ TEXT("\t\tPCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;\n")
				+ TEXT("\t\tbUseUnity = false;\n")
				+ TEXT("\t\tif (Target.bBuildEditor)\n")
				+ TEXT("\t\t{\n")
				+ TEXT("\t\t\tPrivateDefinitions.Add(\"AS_ENABLE_EDITOR_JITTED_CODE=1\");\n")
				+ TEXT("\t\t}\n")
				+ TEXT("\t\tPrivateDependencyModuleNames.AddRange(new string[]\n")
				+ TEXT("\t\t{\n")
				+ TEXT("\t\t\t\"Core\",\n")
				+ TEXT("\t\t\t\"AngelscriptRuntime\",\n")
				+ TEXT("\t\t});\n")
				+ TEXT("\t}\n")
				+ TEXT("}\n");
		}

		FString ModuleSourceContent()
		{
			return FString(OwnershipPrefix)
				+ TEXT("module-source\n")
				+ TEXT("#include \"Modules/ModuleManager.h\"\n")
				+ TEXT("#include \"StaticJIT/AngelscriptJITProvider.h\"\n\n")
				+ TEXT("#include \"Generated/Provider.generated.h\"\n")
				+ TEXT("\n")
				+ TEXT("class FAngelscriptJITModule final\n")
				+ TEXT("\t: public IModuleInterface\n")
				+ TEXT("\t, public IAngelscriptJITArtifactProvider\n")
				+ TEXT("{\n")
				+ TEXT("public:\n")
				+ TEXT("\tvirtual void StartupModule() override\n")
				+ TEXT("\t{\n")
				+ TEXT("\t\tIModularFeatures::Get().RegisterModularFeature(\n")
				+ TEXT("\t\t\tIAngelscriptJITArtifactProvider::FeatureName(), this);\n")
				+ TEXT("\t\tbRegistered = true;\n")
				+ TEXT("\t}\n\n")
				+ TEXT("\tvirtual void ShutdownModule() override\n")
				+ TEXT("\t{\n")
				+ TEXT("\t\tif (bRegistered)\n")
				+ TEXT("\t\t{\n")
				+ TEXT("\t\t\tIModularFeatures::Get().UnregisterModularFeature(\n")
				+ TEXT("\t\t\t\tIAngelscriptJITArtifactProvider::FeatureName(), this);\n")
				+ TEXT("\t\t\tbRegistered = false;\n")
				+ TEXT("\t\t}\n")
				+ TEXT("\t}\n\n")
				+ TEXT("\tvirtual const FAngelscriptJITProviderView*\n")
				+ TEXT("\tGetAngelscriptJITProviderView() const override\n")
				+ TEXT("\t{\n")
				+ TEXT("\t\treturn GetCurrentGeneratedAngelscriptJITProviderView();\n")
				+ TEXT("\t}\n\n")
				+ TEXT("\tvirtual bool SupportsDynamicReloading() override\n")
				+ TEXT("\t{\n")
				+ TEXT("\t\treturn false;\n")
				+ TEXT("\t}\n\n")
				+ TEXT("private:\n")
				+ TEXT("\tbool bRegistered = false;\n")
				+ TEXT("};\n\n")
				+ TEXT("IMPLEMENT_MODULE(FAngelscriptJITModule, AngelscriptJIT)\n");
		}

		FString ProviderSelectorHeaderContent()
		{
			return FString(OwnershipPrefix)
				+ TEXT("provider-selector-header\n")
				+ TEXT("#pragma once\n\n")
				+ TEXT("#include \"StaticJIT/AngelscriptJITProvider.h\"\n\n")
				+ TEXT("const FAngelscriptJITProviderView* ")
				+ TEXT("GetCurrentGeneratedAngelscriptJITProviderView();\n");
		}

		FString ProviderSelectorSourceContent()
		{
			return FString(OwnershipPrefix)
				+ TEXT("provider-selector-source\n")
				+ TEXT("#include \"Provider.generated.h\"\n\n")
				+ TEXT("#if WITH_EDITOR && UE_BUILD_DEVELOPMENT && __has_include(\"EditorDevelopment/Provider.generated.inl\")\n")
				+ TEXT("#include \"EditorDevelopment/Provider.generated.inl\"\n")
				+ TEXT("#define ANGELSCRIPT_JIT_HAS_SELECTED_PROVIDER 1\n")
				+ TEXT("#elif !WITH_EDITOR && UE_BUILD_SHIPPING && __has_include(\"GameShipping/Provider.generated.inl\")\n")
				+ TEXT("#include \"GameShipping/Provider.generated.inl\"\n")
				+ TEXT("#define ANGELSCRIPT_JIT_HAS_SELECTED_PROVIDER 1\n")
				+ TEXT("#elif !WITH_EDITOR && UE_BUILD_DEVELOPMENT && __has_include(\"GameDevelopment/Provider.generated.inl\")\n")
				+ TEXT("#include \"GameDevelopment/Provider.generated.inl\"\n")
				+ TEXT("#define ANGELSCRIPT_JIT_HAS_SELECTED_PROVIDER 1\n")
				+ TEXT("#else\n")
				+ TEXT("#define ANGELSCRIPT_JIT_HAS_SELECTED_PROVIDER 0\n")
				+ TEXT("#endif\n\n")
				+ TEXT("const FAngelscriptJITProviderView*\n")
				+ TEXT("GetCurrentGeneratedAngelscriptJITProviderView()\n")
				+ TEXT("{\n")
				+ TEXT("#if ANGELSCRIPT_JIT_HAS_SELECTED_PROVIDER\n")
				+ TEXT("\tstatic thread_local FAngelscriptJITProviderView View;\n")
				+ TEXT("\tView = GetGeneratedAngelscriptJITProviderView();\n")
				+ TEXT("\treturn &View;\n")
				+ TEXT("#else\n")
				+ TEXT("\treturn nullptr;\n")
				+ TEXT("#endif\n")
				+ TEXT("}\n");
		}

		TArray<FScaffoldFile> BuildScaffoldFiles()
		{
			TArray<FScaffoldFile> Files;
			Files.Add({TEXT("AngelscriptJIT.Build.cs"), BuildRulesContent()});
			Files.Add({TEXT("AngelscriptJITModule.cpp"),
				ModuleSourceContent()});
			Files.Add({TEXT("Generated/Provider.generated.h"),
				ProviderSelectorHeaderContent()});
			Files.Add({TEXT("Generated/Provider.generated.cpp"),
				ProviderSelectorSourceContent()});
			return Files;
		}

		TArray<FScaffoldFile> BuildLegacyPrivateScaffoldFiles()
		{
			TArray<FScaffoldFile> Files;
			Files.Add({TEXT("Private/AngelscriptJITModule.cpp"),
				ModuleSourceContent()});
			Files.Add({TEXT("Private/Generated/Provider.generated.h"),
				ProviderSelectorHeaderContent()});
			Files.Add({TEXT("Private/Generated/Provider.generated.cpp"),
				ProviderSelectorSourceContent()});
			return Files;
		}

		bool LoadExactUtf8(const FString& Filename, FString& OutContent)
		{
			TArray<uint8> Bytes;
			if (!FFileHelper::LoadFileToArray(Bytes, *Filename))
			{
				return false;
			}
			if (Bytes.IsEmpty())
			{
				OutContent.Empty();
				return true;
			}
			FUTF8ToTCHAR Converted(
				reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
			OutContent = FString(Converted.Length(), Converted.Get());
			return true;
		}

		bool HasExpectedScaffoldMarker(
			const FString& Expected,
			const FString& Actual)
		{
			int32 MarkerEnd = INDEX_NONE;
			if (!Expected.StartsWith(OwnershipPrefix, ESearchCase::CaseSensitive)
				|| !Expected.FindChar(TEXT('\n'), MarkerEnd))
			{
				return false;
			}
			return Actual.StartsWith(
				Expected.Left(MarkerEnd + 1), ESearchCase::CaseSensitive);
		}

		bool IsLegacyBucketPath(const FStringView RelativePath)
		{
			return RelativePath.StartsWith(
				TEXT("Private/Generated/Buckets/JITBucket_"),
				ESearchCase::CaseSensitive);
		}

		bool HasLegacyBucketMarker(const FString& Content)
		{
			return Content.StartsWith(
				TEXT("// @angelscript-jit-scaffold revision=1 kind=bucket\n"),
					ESearchCase::CaseSensitive)
				|| Content.StartsWith(
					TEXT("// @angelscript-jit-owned revision=1 kind=bucket\n"),
					ESearchCase::CaseSensitive);
		}

		bool HasLegacyScaffoldMarker(const FString& Content)
		{
			return Content.StartsWith(
				LegacyOwnershipPrefix, ESearchCase::CaseSensitive);
		}

		bool StageWrite(
			const FString& Destination,
			const FString& DisplayPath,
			const FString& Content,
			TArray<FPendingWrite>& OutPending,
			FString& OutError)
		{
			const FString Parent = FPaths::GetPath(Destination);
			if (!IFileManager::Get().DirectoryExists(*Parent)
				&& !IFileManager::Get().MakeDirectory(*Parent, true))
			{
				OutError = FString::Printf(
					TEXT("Could not create scaffold directory '%s'."), *Parent);
				return false;
			}
			FPendingWrite& Pending = OutPending.AddDefaulted_GetRef();
			Pending.DisplayPath = DisplayPath;
			Pending.Destination = Destination;
			Pending.Temporary = Destination + TEXT(".asjit-scaffold-tmp-")
				+ FGuid::NewGuid().ToString(EGuidFormats::Digits);
			if (!FFileHelper::SaveStringToFile(Content, *Pending.Temporary,
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				OutError = FString::Printf(
					TEXT("Could not stage scaffold file '%s'."), *DisplayPath);
				return false;
			}
			return true;
		}

		void DeleteStaged(const TArray<FPendingWrite>& PendingWrites)
		{
			for (const FPendingWrite& Pending : PendingWrites)
			{
				IFileManager::Get().Delete(*Pending.Temporary, false, true);
			}
		}
	}

	FString FAngelscriptJITProjectScaffoldInspection::Summary() const
	{
		return Differences.IsEmpty()
			? TEXT("AngelscriptJIT project scaffold is current.")
			: FString::Join(Differences, TEXT("; "));
	}

	FAngelscriptJITProviderId
	FAngelscriptJITProjectScaffold::BuildProviderId(
		const FStringView ProjectFile)
	{
		const FString CanonicalProject = CanonicalPath(ProjectFile);
		if (CanonicalProject.IsEmpty())
		{
			return {};
		}
		const FString CanonicalScriptRoot = CanonicalPath(FPaths::Combine(
			FPaths::GetPath(CanonicalProject), TEXT("Script")));
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("jit-project-provider-v1"));
		Writer.WriteString(CanonicalProject);
		Writer.WriteString(CanonicalScriptRoot);
		Writer.WriteString(ModuleName);
		return {Writer.FinalizeHash()};
	}

	FAngelscriptJITProjectScaffoldInspection
	FAngelscriptJITProjectScaffold::Inspect(const FStringView ProjectFile)
	{
		FAngelscriptJITProjectScaffoldInspection Result;
		Result.ProjectFile = FullProjectPath(ProjectFile);
		Result.ModuleDirectory = ModuleDirectoryForProject(Result.ProjectFile);
		Result.ProviderId = BuildProviderId(Result.ProjectFile);
		if (Result.ProjectFile.IsEmpty()
			|| !IFileManager::Get().FileExists(*Result.ProjectFile))
		{
			Result.Differences.Add(TEXT("Project descriptor is missing."));
			return Result;
		}

		TSharedPtr<FJsonObject> Project;
		FString Error;
		if (!LoadProjectJson(Result.ProjectFile, Project, Error))
		{
			Result.Differences.Add(MoveTemp(Error));
			return Result;
		}
		TSharedPtr<FJsonObject> Descriptor;
		int32 DescriptorCount = 0;
		FindModuleDescriptor(Project, Descriptor, DescriptorCount);
		Result.bDescriptorCurrent = DescriptorCount == 1
			&& IsExpectedModuleDescriptor(Descriptor);
		Result.bTargetDiscoverable = Result.bDescriptorCurrent;
		if (!Result.bDescriptorCurrent)
		{
			Result.Differences.Add(
				TEXT("AngelscriptJIT must have one Runtime/PostDefault project descriptor."));
		}

		const TArray<FScaffoldFile> Files = BuildScaffoldFiles();
		for (const FScaffoldFile& File : Files)
		{
			const FString Filename = FPaths::Combine(
				Result.ModuleDirectory, File.RelativePath);
			FString Actual;
			const bool bLoaded = LoadExactUtf8(Filename, Actual);
			const bool bCurrent = bLoaded && Actual == File.Content;
			if (File.RelativePath == TEXT("AngelscriptJIT.Build.cs"))
			{
				Result.bRuntimeDependencyCurrent = bCurrent;
			}
			else if (File.RelativePath
				== TEXT("AngelscriptJITModule.cpp"))
			{
				Result.bModuleSourceCurrent = bCurrent;
			}
			if (!bCurrent)
			{
				Result.Differences.Add(FString::Printf(
					TEXT("Scaffold file is missing or stale: %s."),
					*File.RelativePath));
			}
		}

		TArray<FString> Buckets;
		IFileManager::Get().FindFiles(Buckets,
			*FPaths::Combine(Result.ModuleDirectory,
				TEXT("Private/Generated/Buckets/JITBucket_*.cpp")),
			true, false);
		TArray<FString> CurrentBuckets;
		IFileManager::Get().FindFiles(CurrentBuckets,
			*FPaths::Combine(Result.ModuleDirectory,
				TEXT("Generated/Buckets/JITBucket_*.cpp")),
			true, false);
		Buckets.Append(CurrentBuckets);
		bool bLegacyScaffoldRemains = false;
		for (const FScaffoldFile& Legacy : BuildLegacyPrivateScaffoldFiles())
		{
			if (IFileManager::Get().FileExists(*FPaths::Combine(
				Result.ModuleDirectory, Legacy.RelativePath)))
			{
				bLegacyScaffoldRemains = true;
				Result.Differences.Add(FString::Printf(
					TEXT("Legacy Private scaffold file remains: %s."),
					*Legacy.RelativePath));
			}
		}
		Result.bGeneratedLayoutCurrent = Buckets.IsEmpty()
			&& !bLegacyScaffoldRemains;
		if (!Result.bGeneratedLayoutCurrent)
		{
			Result.Differences.Add(FString::Printf(
				TEXT("Legacy fixed bucket translation units remain: %d."),
				Buckets.Num()));
		}
		return Result;
	}

	FAngelscriptJITProjectScaffoldResult
	FAngelscriptJITProjectScaffold::Scaffold(const FStringView ProjectFile)
	{
		FAngelscriptJITProjectScaffoldResult Result;
		Result.ProjectFile = FullProjectPath(ProjectFile);
		Result.ModuleDirectory = ModuleDirectoryForProject(Result.ProjectFile);
		Result.ProviderId = BuildProviderId(Result.ProjectFile);
		if (Result.ProjectFile.IsEmpty()
			|| !Result.ProviderId.IsValid()
			|| !FPaths::GetExtension(Result.ProjectFile, true).Equals(
				TEXT(".uproject"), ESearchCase::IgnoreCase))
		{
			Result.Error = TEXT("Scaffold requires one valid .uproject path.");
			return Result;
		}

		TSharedPtr<FJsonObject> Project;
		if (!LoadProjectJson(Result.ProjectFile, Project, Result.Error))
		{
			return Result;
		}
		TSharedPtr<FJsonObject> Descriptor;
		int32 DescriptorCount = 0;
		FindModuleDescriptor(Project, Descriptor, DescriptorCount);
		if (DescriptorCount > 1
			|| (DescriptorCount == 1
				&& !IsExpectedModuleDescriptor(Descriptor)))
		{
			Result.Error =
				TEXT("Reserved module AngelscriptJIT has conflicting project descriptor metadata; expected exactly Runtime/PostDefault.");
			return Result;
		}
		const bool bWriteDescriptor = DescriptorCount == 0;
		if (bWriteDescriptor)
		{
			AddModuleDescriptor(Project);
		}

		const TArray<FScaffoldFile> Files = BuildScaffoldFiles();
		TArray<const FScaffoldFile*> ChangedFiles;
		for (const FScaffoldFile& File : Files)
		{
			const FString Filename = FPaths::Combine(
				Result.ModuleDirectory, File.RelativePath);
			if (!IFileManager::Get().FileExists(*Filename))
			{
				ChangedFiles.Add(&File);
				continue;
			}
			FString Actual;
			if (!LoadExactUtf8(Filename, Actual))
			{
				Result.Error = FString::Printf(
					TEXT("Could not read existing scaffold path '%s'."),
					*Filename);
				return Result;
			}
			if (Actual == File.Content)
			{
				Result.UnchangedFiles.Add(File.RelativePath);
				continue;
			}
			if (!HasExpectedScaffoldMarker(File.Content, Actual)
				&& !HasLegacyScaffoldMarker(Actual))
			{
				Result.Error = FString::Printf(
					TEXT("Refusing to overwrite user-owned AngelscriptJIT path '%s'."),
					*Filename);
				return Result;
			}
			ChangedFiles.Add(&File);
		}

		TArray<FString> LegacyScaffoldPaths;
		for (const FScaffoldFile& Legacy : BuildLegacyPrivateScaffoldFiles())
		{
			const FString Filename = FPaths::Combine(
				Result.ModuleDirectory, Legacy.RelativePath);
			if (!IFileManager::Get().FileExists(*Filename))
			{
				continue;
			}
			FString Actual;
			if (!LoadExactUtf8(Filename, Actual))
			{
				Result.Error = FString::Printf(
					TEXT("Could not read legacy Private scaffold path '%s'."),
					*Filename);
				return Result;
			}
			if (Actual != Legacy.Content)
			{
				Result.Error = FString::Printf(
					TEXT("Refusing to migrate modified legacy Private scaffold path '%s'."),
					*Filename);
				return Result;
			}
			LegacyScaffoldPaths.Add(Legacy.RelativePath);
		}

		TArray<FString> LegacyBucketFiles;
		IFileManager::Get().FindFiles(
			LegacyBucketFiles,
			*FPaths::Combine(Result.ModuleDirectory,
				TEXT("Private/Generated/Buckets/JITBucket_*.cpp")),
			true, false);
		TArray<FString> LegacyBucketPaths;
		for (const FString& LegacyBucketFile : LegacyBucketFiles)
		{
			const FString RelativePath = FPaths::Combine(
				TEXT("Private/Generated/Buckets"), LegacyBucketFile);
			const FString Filename = FPaths::Combine(
				Result.ModuleDirectory, RelativePath);
			FString Actual;
			if (!IsLegacyBucketPath(RelativePath)
				|| !LoadExactUtf8(Filename, Actual)
				|| !HasLegacyBucketMarker(Actual))
			{
				Result.Error = FString::Printf(
					TEXT("Refusing to remove unrecognized legacy bucket path '%s'."),
					*Filename);
				return Result;
			}
			LegacyBucketPaths.Add(RelativePath);
		}

		TArray<FPendingWrite> PendingWrites;
		for (const FScaffoldFile* File : ChangedFiles)
		{
			if (!StageWrite(
				FPaths::Combine(Result.ModuleDirectory, File->RelativePath),
				File->RelativePath, File->Content, PendingWrites, Result.Error))
			{
				DeleteStaged(PendingWrites);
				return Result;
			}
		}
		if (bWriteDescriptor
			&& !StageWrite(Result.ProjectFile,
				FPaths::GetCleanFilename(Result.ProjectFile),
				SerializeProject(Project), PendingWrites, Result.Error))
		{
			DeleteStaged(PendingWrites);
			return Result;
		}

		for (const FPendingWrite& Pending : PendingWrites)
		{
			if (!IFileManager::Get().Move(*Pending.Destination,
				*Pending.Temporary, true, true))
			{
				Result.Error = FString::Printf(
					TEXT("Could not atomically publish scaffold file '%s'."),
					*Pending.DisplayPath);
				DeleteStaged(PendingWrites);
				return Result;
			}
			Result.WrittenFiles.Add(Pending.DisplayPath);
		}
		for (const FString& LegacyBucketPath : LegacyBucketPaths)
		{
			const FString Filename = FPaths::Combine(
				Result.ModuleDirectory, LegacyBucketPath);
			if (!IFileManager::Get().Delete(*Filename, false, true))
			{
				Result.Error = FString::Printf(
					TEXT("Could not remove legacy scaffold file '%s'."),
					*LegacyBucketPath);
				return Result;
			}
			Result.RemovedFiles.Add(LegacyBucketPath);
		}
		for (const FString& LegacyScaffoldPath : LegacyScaffoldPaths)
		{
			const FString Filename = FPaths::Combine(
				Result.ModuleDirectory, LegacyScaffoldPath);
			if (!IFileManager::Get().Delete(*Filename, false, true))
			{
				Result.Error = FString::Printf(
					TEXT("Could not remove legacy Private scaffold file '%s'."),
					*LegacyScaffoldPath);
				return Result;
			}
			Result.RemovedFiles.Add(LegacyScaffoldPath);
		}
		IFileManager::Get().DeleteDirectory(
			*FPaths::Combine(Result.ModuleDirectory,
				TEXT("Private/Generated/Buckets")), false, false);
		IFileManager::Get().DeleteDirectory(
			*FPaths::Combine(Result.ModuleDirectory,
				TEXT("Private/Generated")), false, false);
		IFileManager::Get().DeleteDirectory(
			*FPaths::Combine(Result.ModuleDirectory,
				TEXT("Private")), false, false);

		Result.WrittenFiles.Sort();
		Result.UnchangedFiles.Sort();
		Result.RemovedFiles.Sort();
		Result.bChanged = !Result.WrittenFiles.IsEmpty()
			|| !Result.RemovedFiles.IsEmpty();
		Result.bRequiresFullBuild = Result.bChanged;
		Result.bSuccess = true;
		return Result;
	}
}
