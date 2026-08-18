#include "CQTest.h"

#include "StaticJIT/AngelscriptJITProjectScaffold.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptJITProjectScaffoldTests
{
	static FString MakeTestRoot()
	{
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation"),
			TEXT("StaticJITProjectScaffold"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static FString WriteProject(
		const FString& Root,
		const FString& ProjectName,
		const FString& AdditionalModule = FString())
	{
		IFileManager::Get().MakeDirectory(*Root, true);
		const FString ProjectFile = FPaths::Combine(
			Root, ProjectName + TEXT(".uproject"));
		FString Modules =
			TEXT("    {\n")
			TEXT("      \"Name\": \"HostRuntime\",\n")
			TEXT("      \"Type\": \"Runtime\",\n")
			TEXT("      \"LoadingPhase\": \"Default\"\n")
			TEXT("    }");
		if (!AdditionalModule.IsEmpty())
		{
			Modules += TEXT(",\n") + AdditionalModule;
		}
		const FString Descriptor = FString::Printf(
			TEXT("{\n")
			TEXT("  \"FileVersion\": 3,\n")
			TEXT("  \"CustomUserField\": \"preserve-me\",\n")
			TEXT("  \"Modules\": [\n%s\n  ]\n")
			TEXT("}\n"),
			*Modules);
		check(FFileHelper::SaveStringToFile(
			Descriptor, *ProjectFile,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
		return ProjectFile;
	}

	static TSharedPtr<FJsonObject> ReadProject(const FString& ProjectFile)
	{
		FString Content;
		check(FFileHelper::LoadFileToString(Content, *ProjectFile));
		TSharedPtr<FJsonObject> Json;
		const TSharedRef<TJsonReader<>> Reader =
			TJsonReaderFactory<>::Create(Content);
		check(FJsonSerializer::Deserialize(Reader, Json));
		return Json;
	}

	static int32 CountModule(
		const TSharedPtr<FJsonObject>& Project,
		const FStringView Name,
		FString* OutType = nullptr,
		FString* OutLoadingPhase = nullptr)
	{
		const TArray<TSharedPtr<FJsonValue>>* Modules = nullptr;
		if (!Project->TryGetArrayField(TEXT("Modules"), Modules)
			|| Modules == nullptr)
		{
			return 0;
		}
		int32 Count = 0;
		for (const TSharedPtr<FJsonValue>& Value : *Modules)
		{
			const TSharedPtr<FJsonObject> Module = Value->AsObject();
			FString ModuleName;
			if (Module.IsValid()
				&& Module->TryGetStringField(TEXT("Name"), ModuleName)
				&& ModuleName == Name)
			{
				++Count;
				if (OutType != nullptr)
				{
					Module->TryGetStringField(TEXT("Type"), *OutType);
				}
				if (OutLoadingPhase != nullptr)
				{
					Module->TryGetStringField(
						TEXT("LoadingPhase"), *OutLoadingPhase);
				}
			}
		}
		return Count;
	}

	static bool MaterializeRevisionTwoPrivateScaffold(
		const FString& ModuleDirectory)
	{
		struct FScaffoldMove
		{
			const TCHAR* Current;
			const TCHAR* Legacy;
		};
		const FScaffoldMove Moves[] = {
			{TEXT("AngelscriptJITModule.cpp"),
				TEXT("Private/AngelscriptJITModule.cpp")},
			{TEXT("Generated/Provider.generated.h"),
				TEXT("Private/Generated/Provider.generated.h")},
			{TEXT("Generated/Provider.generated.cpp"),
				TEXT("Private/Generated/Provider.generated.cpp")},
		};
		for (const FScaffoldMove& Move : Moves)
		{
			const FString Current = FPaths::Combine(
				ModuleDirectory, Move.Current);
			const FString Legacy = FPaths::Combine(
				ModuleDirectory, Move.Legacy);
			if (IFileManager::Get().FileExists(*Legacy))
			{
				continue;
			}
			if (!IFileManager::Get().FileExists(*Current)
				|| !IFileManager::Get().MakeDirectory(
					*FPaths::GetPath(Legacy), true)
				|| !IFileManager::Get().Move(
					*Legacy, *Current, true, true))
			{
				return false;
			}
		}
		return true;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptJITProjectScaffoldTests,
	"Angelscript.TestModule.StaticJIT.ProjectScaffold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(FixedModuleIsRuntimePreDefaultAndTargetDiscoverable)
	{
		using namespace AngelscriptEditor::StaticJIT;
		using namespace AngelscriptJITProjectScaffoldTests;
		const FString Root = MakeTestRoot();
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};
		const FString ProjectFile = WriteProject(Root, TEXT("FixtureProject"));

		const FAngelscriptJITProjectScaffoldResult Result =
			FAngelscriptJITProjectScaffold::Scaffold(ProjectFile);
		ASSERT_THAT(IsTrue(Result.bSuccess, Result.Error));
		ASSERT_THAT(IsTrue(Result.bChanged));
		ASSERT_THAT(IsTrue(Result.bRequiresFullBuild));
		ASSERT_THAT(AreEqual(
			FPaths::Combine(Root, TEXT("Source/AngelscriptJIT")),
			Result.ModuleDirectory));
		ASSERT_THAT(IsTrue(Result.ProviderId.IsValid()));

		const TSharedPtr<FJsonObject> Project = ReadProject(ProjectFile);
		FString Type;
		FString LoadingPhase;
		ASSERT_THAT(AreEqual(1, CountModule(
			Project, TEXT("AngelscriptJIT"), &Type, &LoadingPhase)));
		ASSERT_THAT(AreEqual(FString(TEXT("Runtime")), Type));
		ASSERT_THAT(AreEqual(FString(TEXT("PreDefault")), LoadingPhase));
		ASSERT_THAT(AreEqual(
			FString(TEXT("preserve-me")),
			Project->GetStringField(TEXT("CustomUserField"))));

		FString BuildRules;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			BuildRules,
			*FPaths::Combine(Result.ModuleDirectory,
				TEXT("AngelscriptJIT.Build.cs")))));
		ASSERT_THAT(IsTrue(BuildRules.Contains(
			TEXT("PublicDependencyModuleNames")),
			TEXT("Generated direct-call includes must be backed by public module dependencies")));
		ASSERT_THAT(IsFalse(BuildRules.Contains(
			TEXT("PrivateDependencyModuleNames"))));
		ASSERT_THAT(IsTrue(BuildRules.Contains(TEXT("\"AngelscriptRuntime\""))));
		ASSERT_THAT(IsTrue(BuildRules.Contains(
			TEXT("AS_ENABLE_EDITOR_JITTED_CODE=1")),
			TEXT("The carrier must enable generated entry bodies only in its private Editor compile")));
		ASSERT_THAT(IsFalse(BuildRules.Contains(TEXT("AngelscriptTest"))));

		FString ProviderSelector;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			ProviderSelector,
			*FPaths::Combine(Result.ModuleDirectory,
				TEXT("Generated/Provider.generated.cpp")))));
		ASSERT_THAT(IsTrue(ProviderSelector.Contains(
			TEXT("#if WITH_EDITOR && UE_BUILD_DEVELOPMENT"))));
		ASSERT_THAT(IsTrue(ProviderSelector.Contains(
			TEXT("#elif !WITH_EDITOR && UE_BUILD_DEVELOPMENT"))));
		ASSERT_THAT(IsTrue(ProviderSelector.Contains(
			TEXT("\"EditorDevelopment/Provider.generated.inl\""))));
		ASSERT_THAT(IsTrue(ProviderSelector.Contains(
			TEXT("\"GameDevelopment/Provider.generated.inl\""))));
		ASSERT_THAT(IsTrue(ProviderSelector.Contains(
			TEXT("\"GameShipping/Provider.generated.inl\""))));
		ASSERT_THAT(IsFalse(ProviderSelector.Contains(TEXT("Profiles/"))));
		ASSERT_THAT(IsTrue(ProviderSelector.Contains(
			TEXT("thread_local FAngelscriptJITProviderView View")),
			TEXT("The selector must own per-thread scratch storage for the pointer-returning Provider ABI")));
		ASSERT_THAT(IsTrue(ProviderSelector.Contains(
			TEXT("View = GetGeneratedAngelscriptJITProviderView();")),
			TEXT("Every observation must repopulate the view from the currently patched generated accessor")));

		FString ModuleSource;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			ModuleSource,
			*FPaths::Combine(Result.ModuleDirectory,
				TEXT("AngelscriptJITModule.cpp")))));
		ASSERT_THAT(IsTrue(ModuleSource.Contains(
			TEXT("RegisterModularFeature"))));
		ASSERT_THAT(IsTrue(ModuleSource.Contains(
			TEXT("bool bRegistered = false;"))));
		ASSERT_THAT(IsTrue(ModuleSource.Contains(
			TEXT("if (bRegistered)"))));
		ASSERT_THAT(IsFalse(ModuleSource.Contains(
			TEXT("IsModularFeatureAvailable")),
			TEXT("Carrier shutdown must track its own feature instead of querying whether any provider exists")));
		ASSERT_THAT(IsTrue(ModuleSource.Contains(
			TEXT("IMPLEMENT_MODULE(FAngelscriptJITModule, AngelscriptJIT)"))));
		ASSERT_THAT(IsTrue(ModuleSource.Contains(
			TEXT("virtual bool SupportsDynamicReloading() override"))));
		ASSERT_THAT(IsTrue(ModuleSource.Contains(TEXT("return false;"))));

		TArray<FString> BucketFiles;
		IFileManager::Get().FindFiles(
			BucketFiles,
			*FPaths::Combine(Result.ModuleDirectory,
				TEXT("Generated/Buckets/JITBucket_*.cpp")),
			true, false);
		ASSERT_THAT(AreEqual(0, BucketFiles.Num()));
		ASSERT_THAT(IsFalse(IFileManager::Get().DirectoryExists(
			*FPaths::Combine(Result.ModuleDirectory, TEXT("Private")))));

		const FAngelscriptJITProjectScaffoldInspection Inspection =
			FAngelscriptJITProjectScaffold::Inspect(ProjectFile);
		ASSERT_THAT(IsTrue(Inspection.IsCurrent(), Inspection.Summary()));
		ASSERT_THAT(IsTrue(Inspection.bTargetDiscoverable));
	}

	TEST_METHOD(ProviderIdentityUsesCanonicalProjectAndScriptOwnershipDomain)
	{
		using namespace AngelscriptEditor::StaticJIT;
		using namespace AngelscriptJITProjectScaffoldTests;
		const FString Root = MakeTestRoot();
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};
		const FString ProjectA = WriteProject(
			FPaths::Combine(Root, TEXT("A")), TEXT("SharedName"));
		const FString ProjectB = WriteProject(
			FPaths::Combine(Root, TEXT("B")), TEXT("SharedName"));
		const FString EquivalentA = FPaths::Combine(
			FPaths::GetPath(ProjectA), TEXT("Nested/../SharedName.uproject"));

		const FAngelscriptJITProviderId ProviderA =
			FAngelscriptJITProjectScaffold::BuildProviderId(ProjectA);
		const FAngelscriptJITProviderId EquivalentProviderA =
			FAngelscriptJITProjectScaffold::BuildProviderId(EquivalentA);
		const FAngelscriptJITProviderId ProviderB =
			FAngelscriptJITProjectScaffold::BuildProviderId(ProjectB);
		ASSERT_THAT(IsTrue(ProviderA.IsValid()));
		ASSERT_THAT(IsTrue(ProviderA == EquivalentProviderA));
		ASSERT_THAT(IsFalse(ProviderA == ProviderB));
	}

	TEST_METHOD(ReservedDescriptorAndUserPathConflictsRefuseAllWrites)
	{
		using namespace AngelscriptEditor::StaticJIT;
		using namespace AngelscriptJITProjectScaffoldTests;
		const FString Root = MakeTestRoot();
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};
		const FString ConflictingModule =
			TEXT("    {\n")
			TEXT("      \"Name\": \"AngelscriptJIT\",\n")
			TEXT("      \"Type\": \"Editor\",\n")
			TEXT("      \"LoadingPhase\": \"Default\"\n")
			TEXT("    }");
		const FString DescriptorProject = WriteProject(
			FPaths::Combine(Root, TEXT("Descriptor")),
			TEXT("DescriptorConflict"), ConflictingModule);
		TArray<uint8> DescriptorBefore;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToArray(
			DescriptorBefore, *DescriptorProject)));
		const FAngelscriptJITProjectScaffoldResult DescriptorConflict =
			FAngelscriptJITProjectScaffold::Scaffold(DescriptorProject);
		ASSERT_THAT(IsFalse(DescriptorConflict.bSuccess));
		ASSERT_THAT(IsTrue(DescriptorConflict.Error.Contains(
			TEXT("AngelscriptJIT"))));
		ASSERT_THAT(IsFalse(IFileManager::Get().DirectoryExists(
			*FPaths::Combine(FPaths::GetPath(DescriptorProject),
				TEXT("Source/AngelscriptJIT")))));
		TArray<uint8> DescriptorAfter;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToArray(
			DescriptorAfter, *DescriptorProject)));
		ASSERT_THAT(IsTrue(DescriptorBefore == DescriptorAfter));

		const FString PathProject = WriteProject(
			FPaths::Combine(Root, TEXT("Path")), TEXT("PathConflict"));
		const FString UserBuildRules = FPaths::Combine(
			FPaths::GetPath(PathProject),
			TEXT("Source/AngelscriptJIT/AngelscriptJIT.Build.cs"));
		ASSERT_THAT(IsTrue(IFileManager::Get().MakeDirectory(
			*FPaths::GetPath(UserBuildRules), true)));
		const FString UserContent =
			TEXT("// @angelscript-jit-scaffold revision=0 kind=build-rules\n")
			TEXT("// user-owned module\n");
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			UserContent, *UserBuildRules)));
		TArray<uint8> ProjectBefore;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToArray(
			ProjectBefore, *PathProject)));
		const FAngelscriptJITProjectScaffoldResult PathConflict =
			FAngelscriptJITProjectScaffold::Scaffold(PathProject);
		ASSERT_THAT(IsFalse(PathConflict.bSuccess));
		FString UserAfter;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			UserAfter, *UserBuildRules)));
		ASSERT_THAT(AreEqual(UserContent, UserAfter));
		TArray<uint8> ProjectAfter;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToArray(
			ProjectAfter, *PathProject)));
		ASSERT_THAT(IsTrue(ProjectBefore == ProjectAfter));
	}

	TEST_METHOD(SecondScaffoldIsIdempotentAndPreservesTimestampsAndUserFiles)
	{
		using namespace AngelscriptEditor::StaticJIT;
		using namespace AngelscriptJITProjectScaffoldTests;
		const FString Root = MakeTestRoot();
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};
		const FString ProjectFile = WriteProject(Root, TEXT("Idempotent"));
		const FAngelscriptJITProjectScaffoldResult First =
			FAngelscriptJITProjectScaffold::Scaffold(ProjectFile);
		ASSERT_THAT(IsTrue(First.bSuccess, First.Error));
		const FString BuildRules = FPaths::Combine(
			First.ModuleDirectory, TEXT("AngelscriptJIT.Build.cs"));
		const FString ProviderSelector = FPaths::Combine(
			First.ModuleDirectory,
			TEXT("Generated/Provider.generated.cpp"));
		const FString UserFile = FPaths::Combine(
			First.ModuleDirectory, TEXT("UserExtension.cpp"));
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			TEXT("// keep me\n"), *UserFile)));
		const FDateTime FrozenTimestamp(2025, 3, 4, 5, 6, 7);
		IFileManager::Get().SetTimeStamp(*ProjectFile, FrozenTimestamp);
		IFileManager::Get().SetTimeStamp(*BuildRules, FrozenTimestamp);
		IFileManager::Get().SetTimeStamp(*ProviderSelector, FrozenTimestamp);

		const FAngelscriptJITProjectScaffoldResult Second =
			FAngelscriptJITProjectScaffold::Scaffold(ProjectFile);
		ASSERT_THAT(IsTrue(Second.bSuccess, Second.Error));
		ASSERT_THAT(IsFalse(Second.bChanged));
		ASSERT_THAT(IsFalse(Second.bRequiresFullBuild));
		ASSERT_THAT(AreEqual(0, Second.WrittenFiles.Num()));
		ASSERT_THAT(AreEqual(0, Second.RemovedFiles.Num()));
		ASSERT_THAT(IsTrue(IFileManager::Get().GetTimeStamp(*ProjectFile)
			== FrozenTimestamp));
		ASSERT_THAT(IsTrue(IFileManager::Get().GetTimeStamp(*BuildRules)
			== FrozenTimestamp));
		ASSERT_THAT(IsTrue(IFileManager::Get().GetTimeStamp(*ProviderSelector)
			== FrozenTimestamp));
		FString UserContent;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			UserContent, *UserFile)));
		ASSERT_THAT(AreEqual(FString(TEXT("// keep me\n")), UserContent));
		const FAngelscriptJITProjectScaffoldInspection Inspection =
			FAngelscriptJITProjectScaffold::Inspect(ProjectFile);
		ASSERT_THAT(IsTrue(Inspection.IsCurrent(), Inspection.Summary()));
	}

	TEST_METHOD(RevisionTwoPrivateScaffoldMigratesToTheModuleRoot)
	{
		using namespace AngelscriptEditor::StaticJIT;
		using namespace AngelscriptJITProjectScaffoldTests;
		const FString Root = MakeTestRoot();
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};
		const FString ProjectFile = WriteProject(Root, TEXT("PrivateMigration"));
		const FAngelscriptJITProjectScaffoldResult Seed =
			FAngelscriptJITProjectScaffold::Scaffold(ProjectFile);
		ASSERT_THAT(IsTrue(Seed.bSuccess, Seed.Error));
		ASSERT_THAT(IsTrue(MaterializeRevisionTwoPrivateScaffold(
			Seed.ModuleDirectory)));

		const FAngelscriptJITProjectScaffoldResult Migrated =
			FAngelscriptJITProjectScaffold::Scaffold(ProjectFile);
		ASSERT_THAT(IsTrue(Migrated.bSuccess, Migrated.Error));
		ASSERT_THAT(IsTrue(Migrated.bChanged));
		ASSERT_THAT(IsTrue(Migrated.bRequiresFullBuild));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
			*FPaths::Combine(Seed.ModuleDirectory,
				TEXT("AngelscriptJITModule.cpp")))));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
			*FPaths::Combine(Seed.ModuleDirectory,
				TEXT("Generated/Provider.generated.h")))));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
			*FPaths::Combine(Seed.ModuleDirectory,
				TEXT("Generated/Provider.generated.cpp")))));
		ASSERT_THAT(IsFalse(IFileManager::Get().DirectoryExists(
			*FPaths::Combine(Seed.ModuleDirectory, TEXT("Private")))));
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProjectScaffold::Inspect(ProjectFile).IsCurrent()));
	}

	TEST_METHOD(ModifiedRevisionTwoPrivateScaffoldBlocksMigrationWithoutWrites)
	{
		using namespace AngelscriptEditor::StaticJIT;
		using namespace AngelscriptJITProjectScaffoldTests;
		const FString Root = MakeTestRoot();
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};
		const FString ProjectFile = WriteProject(Root, TEXT("PrivateConflict"));
		const FAngelscriptJITProjectScaffoldResult Seed =
			FAngelscriptJITProjectScaffold::Scaffold(ProjectFile);
		ASSERT_THAT(IsTrue(Seed.bSuccess, Seed.Error));
		ASSERT_THAT(IsTrue(MaterializeRevisionTwoPrivateScaffold(
			Seed.ModuleDirectory)));
		const FString LegacyModule = FPaths::Combine(
			Seed.ModuleDirectory, TEXT("Private/AngelscriptJITModule.cpp"));
		FString Modified;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			Modified, *LegacyModule)));
		Modified += TEXT("// user modification\n");
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			Modified, *LegacyModule,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)));

		const FAngelscriptJITProjectScaffoldResult Rejected =
			FAngelscriptJITProjectScaffold::Scaffold(ProjectFile);
		ASSERT_THAT(IsFalse(Rejected.bSuccess));
		ASSERT_THAT(IsTrue(Rejected.Error.Contains(TEXT("legacy"))));
		ASSERT_THAT(IsFalse(IFileManager::Get().FileExists(
			*FPaths::Combine(Seed.ModuleDirectory,
				TEXT("AngelscriptJITModule.cpp")))));
		FString Preserved;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			Preserved, *LegacyModule)));
		ASSERT_THAT(AreEqual(Modified, Preserved));
	}

	TEST_METHOD(RecognizedRevisionOneBucketsAreRemovedDuringUpgrade)
	{
		using namespace AngelscriptEditor::StaticJIT;
		using namespace AngelscriptJITProjectScaffoldTests;
		const FString Root = MakeTestRoot();
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};
		const FString ProjectFile = WriteProject(Root, TEXT("LegacyBuckets"));
		const FAngelscriptJITProjectScaffoldResult First =
			FAngelscriptJITProjectScaffold::Scaffold(ProjectFile);
		ASSERT_THAT(IsTrue(First.bSuccess, First.Error));

		const FString LegacyBucketRelative =
			TEXT("Private/Generated/Buckets/JITBucket_17.cpp");
		const FString LegacyBucket = FPaths::Combine(
			First.ModuleDirectory, LegacyBucketRelative);
		ASSERT_THAT(IsTrue(IFileManager::Get().MakeDirectory(
			*FPaths::GetPath(LegacyBucket), true)));
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			TEXT("// @angelscript-jit-scaffold revision=1 kind=bucket\n")
			TEXT("// legacy fixed bucket\n"),
			*LegacyBucket,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)));

		const FAngelscriptJITProjectScaffoldResult Upgrade =
			FAngelscriptJITProjectScaffold::Scaffold(ProjectFile);
		ASSERT_THAT(IsTrue(Upgrade.bSuccess, Upgrade.Error));
		ASSERT_THAT(IsTrue(Upgrade.bChanged));
		ASSERT_THAT(IsTrue(Upgrade.bRequiresFullBuild));
		ASSERT_THAT(AreEqual(0, Upgrade.WrittenFiles.Num()));
		ASSERT_THAT(AreEqual(1, Upgrade.RemovedFiles.Num()));
		ASSERT_THAT(AreEqual(LegacyBucketRelative, Upgrade.RemovedFiles[0]));
		ASSERT_THAT(IsFalse(IFileManager::Get().FileExists(*LegacyBucket)));
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProjectScaffold::Inspect(ProjectFile).IsCurrent()));
	}

	TEST_METHOD(UnownedLegacyBucketLookalikeBlocksUpgradeWithoutWrites)
	{
		using namespace AngelscriptEditor::StaticJIT;
		using namespace AngelscriptJITProjectScaffoldTests;
		const FString Root = MakeTestRoot();
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};
		const FString ProjectFile = WriteProject(Root, TEXT("UserBucket"));
		const FAngelscriptJITProjectScaffoldResult First =
			FAngelscriptJITProjectScaffold::Scaffold(ProjectFile);
		ASSERT_THAT(IsTrue(First.bSuccess, First.Error));

		const FString UserBucket = FPaths::Combine(
			First.ModuleDirectory,
			TEXT("Private/Generated/Buckets/JITBucket_17.cpp"));
		ASSERT_THAT(IsTrue(IFileManager::Get().MakeDirectory(
			*FPaths::GetPath(UserBucket), true)));
		const FString UserContent = TEXT("// user-owned bucket lookalike\n");
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			UserContent, *UserBucket)));
		const FString BuildRules = FPaths::Combine(
			First.ModuleDirectory, TEXT("AngelscriptJIT.Build.cs"));
		TArray<uint8> BuildRulesBefore;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToArray(
			BuildRulesBefore, *BuildRules)));

		const FAngelscriptJITProjectScaffoldResult Upgrade =
			FAngelscriptJITProjectScaffold::Scaffold(ProjectFile);
		ASSERT_THAT(IsFalse(Upgrade.bSuccess));
		ASSERT_THAT(IsTrue(Upgrade.Error.Contains(TEXT("unrecognized"))));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(*UserBucket)));
		FString UserContentAfter;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			UserContentAfter, *UserBucket)));
		ASSERT_THAT(AreEqual(UserContent, UserContentAfter));
		TArray<uint8> BuildRulesAfter;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToArray(
			BuildRulesAfter, *BuildRules)));
		ASSERT_THAT(IsTrue(BuildRulesBefore == BuildRulesAfter));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
