#include "CQTest.h"

#include "StaticJIT/AngelscriptJITGeneratedFileStore.h"
#include "StaticJIT/AngelscriptJITProjectGeneration.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptJITProjectGenerationTests
{
	static FAngelscriptHash256 MakeHash(const FStringView Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("jit-project-generation-test"));
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	static FAngelscriptJITGenerationOutput MakeOutput(
		const EAngelscriptJITTargetProfile TargetProfile,
		const TCHAR* ExecutionIdentity = TEXT("Execution-A"),
		const bool bIncludeUnrelatedModule = true)
	{
		FAngelscriptJITGenerationRequest Request;
		Request.TargetProfile = TargetProfile;
		Request.ProviderId.Hash = MakeHash(TEXT("ProjectProvider"));
		Request.ProviderName = TEXT("ProjectProvider");
		Request.OwnerModuleName = TEXT("AngelscriptJIT");
		Request.ArtifactProfile.Hash = MakeHash(
			FAngelscriptJITTargetProfiles::ToString(TargetProfile));
		const TArray<FString> NativeInputs = {
			TEXT("Platform=Win64"),
			TEXT("Architecture=x64"),
			TEXT("Compiler=ProjectGenerationTest"),
		};
		Request.NativeEnvironmentFingerprint =
			FAngelscriptJITTargetProfiles::BuildNativeEnvironmentFingerprint(
				TargetProfile, NativeInputs);

		FAngelscriptJITGenerationModule& Module =
			Request.Modules.AddDefaulted_GetRef();
		Module.ModuleKey.Hash = MakeHash(TEXT("ProjectModule"));
		Module.CanonicalModuleName = TEXT("ProjectModule");
		Module.VirtualSourcePath = TEXT("/Angelscript/Game/ProjectModule.as");
		if (bIncludeUnrelatedModule)
		{
			FAngelscriptJITGenerationModule& UnrelatedModule =
				Request.Modules.AddDefaulted_GetRef();
			UnrelatedModule.ModuleKey.Hash = MakeHash(TEXT("UnrelatedModule"));
			UnrelatedModule.CanonicalModuleName = TEXT("UnrelatedModule");
			UnrelatedModule.VirtualSourcePath =
				TEXT("/Angelscript/Game/UnrelatedModule.as");
		}

		FAngelscriptJITGenerationFunction& Function =
			Request.Functions.AddDefaulted_GetRef();
		Function.ModuleKey = Request.Modules[0].ModuleKey;
		Function.FunctionKey.Hash = MakeHash(TEXT("ProjectFunction"));
		Function.ExecutionHash = MakeHash(ExecutionIdentity);
		Function.DebugHash = MakeHash(TEXT("ProjectDebug"));
		Function.CanonicalDeclaration = TEXT("int ProjectFunction()");
		Function.SourceSection = Module.VirtualSourcePath;
		Function.SourceLine = 1;
		Function.SourceColumn = 1;
		Function.EntryAbiHash =
			FAngelscriptJITGeneration::BuildEntryAbiHash(
				TEXT("int ProjectFunction()"), true, false, false);
		Function.bHasVMEntry = true;
		Function.ImplementationTemplate =
			TEXT("{VM_ENTRY_COMMENT}void {VM_SYMBOL}(FScriptExecution&, asDWORD*, asQWORD*) {}\n");
		if (bIncludeUnrelatedModule)
		{
			FAngelscriptJITGenerationFunction& UnrelatedFunction =
				Request.Functions.AddDefaulted_GetRef();
			UnrelatedFunction.ModuleKey = Request.Modules[1].ModuleKey;
			UnrelatedFunction.FunctionKey.Hash =
				MakeHash(TEXT("UnrelatedFunction"));
			UnrelatedFunction.ExecutionHash =
				MakeHash(TEXT("UnrelatedExecution"));
			UnrelatedFunction.DebugHash = MakeHash(TEXT("UnrelatedDebug"));
			UnrelatedFunction.CanonicalDeclaration =
				TEXT("int UnrelatedFunction()");
			UnrelatedFunction.SourceSection =
				Request.Modules[1].VirtualSourcePath;
			UnrelatedFunction.SourceLine = 1;
			UnrelatedFunction.SourceColumn = 1;
			UnrelatedFunction.EntryAbiHash =
				FAngelscriptJITGeneration::BuildEntryAbiHash(
					TEXT("int UnrelatedFunction()"), true, false, false);
			UnrelatedFunction.bHasVMEntry = true;
			UnrelatedFunction.ImplementationTemplate =
				TEXT("{VM_ENTRY_COMMENT}void {VM_SYMBOL}(FScriptExecution&, asDWORD*, asQWORD*) {}\n");
		}

		FAngelscriptJITGenerationOutput Output;
		FString Error;
		check(FAngelscriptJITGeneration::Generate(Request, Output, &Error));
		return Output;
	}

	static FString MakeTestRoot()
	{
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation"),
			TEXT("StaticJITProjectGeneration"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static const FAngelscriptJITGeneratedFile& FindFile(
		const FAngelscriptJITGenerationOutput& Output,
		const EAngelscriptJITGeneratedFileKind Kind)
	{
		const FAngelscriptJITGeneratedFile* File =
			Output.Files.FindByPredicate(
				[Kind](const FAngelscriptJITGeneratedFile& Candidate)
				{
					return Candidate.Kind == Kind;
				});
		check(File != nullptr);
		return *File;
	}

	static const FAngelscriptJITGeneratedFile& FindModuleSource(
		const FAngelscriptJITGenerationOutput& Output,
		const FStringView CanonicalModuleName)
	{
		const FAngelscriptJITGeneratedModule* Module =
			Output.Modules.FindByPredicate(
				[CanonicalModuleName](
					const FAngelscriptJITGeneratedModule& Candidate)
				{
					return Candidate.CanonicalModuleName == CanonicalModuleName;
				});
		check(Module != nullptr);
		const FAngelscriptJITGeneratedFile* File =
			Output.Files.FindByPredicate(
				[Path = Module->SourceRelativePath](
					const FAngelscriptJITGeneratedFile& Candidate)
				{
					return Candidate.RelativePath == Path;
				});
		check(File != nullptr);
		return *File;
	}

	static bool HasDifference(
		const FAngelscriptJITGeneratedFileComparison& Comparison,
		const EAngelscriptJITGeneratedFileDifference Kind)
	{
		return Comparison.Differences.ContainsByPredicate(
			[Kind](const FAngelscriptJITGeneratedFileDifferenceRecord& Value)
			{
				return Value.Kind == Kind;
			});
	}

	static const FAngelscriptJITGeneratedFileDifferenceRecord* FindDifference(
		const FAngelscriptJITGeneratedFileComparison& Comparison,
		const EAngelscriptJITGeneratedFileDifference Kind)
	{
		return Comparison.Differences.FindByPredicate(
			[Kind](const FAngelscriptJITGeneratedFileDifferenceRecord& Value)
			{
				return Value.Kind == Kind;
			});
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptJITProjectGenerationTests,
	"Angelscript.TestModule.StaticJIT.ProjectGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(GenerateAtomicallyReplacesOwnedChangesAndPreservesUnchangedFiles)
	{
		using namespace AngelscriptJITProjectGenerationTests;
		using namespace AngelscriptEditor::StaticJIT;
		const FString TestRoot = MakeTestRoot();
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
		};
		const FString OutputRoot = FPaths::Combine(TestRoot, TEXT("Generated"));
		const FAngelscriptJITGenerationOutput Baseline = MakeOutput(
			EAngelscriptJITTargetProfile::EditorDevelopment);

		const FAngelscriptJITProjectGenerationResult First =
			FAngelscriptJITProjectGeneration::Generate(OutputRoot, Baseline);
		ASSERT_THAT(IsTrue(First.bSuccess, First.Error));
		ASSERT_THAT(AreEqual(Baseline.Files.Num(),
			First.Publication.WrittenFiles.Num()));
		ASSERT_THAT(AreEqual(0, First.Publication.UnchangedFiles.Num()));
		ASSERT_THAT(AreEqual(2, First.AddedModuleSources.Num()));
		ASSERT_THAT(AreEqual(0, First.RemovedModuleSources.Num()));

		const FAngelscriptJITGeneratedFile& Manifest = FindFile(
			Baseline, EAngelscriptJITGeneratedFileKind::ProviderManifest);
		const FString ManifestPath = FPaths::Combine(
			OutputRoot, Manifest.RelativePath);
		const FDateTime FrozenTimestamp(2025, 1, 2, 3, 4, 5);
		IFileManager::Get().SetTimeStamp(*ManifestPath, FrozenTimestamp);

		const FAngelscriptJITProjectGenerationResult Second =
			FAngelscriptJITProjectGeneration::Generate(OutputRoot, Baseline);
		ASSERT_THAT(IsTrue(Second.bSuccess, Second.Error));
		ASSERT_THAT(AreEqual(0, Second.Publication.WrittenFiles.Num()));
		ASSERT_THAT(AreEqual(Baseline.Files.Num(),
			Second.Publication.UnchangedFiles.Num()));
		ASSERT_THAT(IsFalse(Second.HasModuleSourceSetChange()));
		ASSERT_THAT(IsTrue(IFileManager::Get().GetTimeStamp(*ManifestPath)
			== FrozenTimestamp));

		const FAngelscriptJITGenerationOutput Changed = MakeOutput(
			EAngelscriptJITTargetProfile::EditorDevelopment,
			TEXT("Execution-B"));
		const FString OldModuleSource = FindModuleSource(
			Baseline, TEXT("ProjectModule")).RelativePath;
		const FString NewModuleSource = FindModuleSource(
			Changed, TEXT("ProjectModule")).RelativePath;
		ASSERT_THAT(AreEqual(OldModuleSource, NewModuleSource));
		const FAngelscriptJITGeneratedFile& UnrelatedSource =
			FindModuleSource(Baseline, TEXT("UnrelatedModule"));
		const FString UnrelatedPath = FPaths::Combine(
			OutputRoot, UnrelatedSource.RelativePath);
		const FDateTime UnrelatedTimestamp(2025, 3, 4, 5, 6, 7);
		IFileManager::Get().SetTimeStamp(
			*UnrelatedPath, UnrelatedTimestamp);
		TArray<uint8> UnrelatedBytesBefore;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToArray(
			UnrelatedBytesBefore, *UnrelatedPath)));

		const FAngelscriptJITProjectGenerationResult Replaced =
			FAngelscriptJITProjectGeneration::Generate(OutputRoot, Changed);
		ASSERT_THAT(IsTrue(Replaced.bSuccess, Replaced.Error));
		ASSERT_THAT(IsTrue(Replaced.Publication.RemovedFiles.IsEmpty()));
		ASSERT_THAT(IsTrue(Replaced.Publication.WrittenFiles.Contains(
			NewModuleSource)));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
			*FPaths::Combine(OutputRoot, NewModuleSource))));
		TArray<uint8> UnrelatedBytesAfter;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToArray(
			UnrelatedBytesAfter, *UnrelatedPath)));
		ASSERT_THAT(IsTrue(UnrelatedBytesBefore == UnrelatedBytesAfter));
		ASSERT_THAT(IsTrue(IFileManager::Get().GetTimeStamp(*UnrelatedPath)
			== UnrelatedTimestamp));
		ASSERT_THAT(IsFalse(Replaced.HasModuleSourceSetChange()));

		const FAngelscriptJITGenerationOutput RemovedModuleOutput = MakeOutput(
			EAngelscriptJITTargetProfile::EditorDevelopment,
			TEXT("Execution-B"), false);
		const FAngelscriptJITProjectGenerationResult RemovedModuleResult =
			FAngelscriptJITProjectGeneration::Generate(
				OutputRoot, RemovedModuleOutput);
		ASSERT_THAT(IsTrue(RemovedModuleResult.bSuccess,
			RemovedModuleResult.Error));
		ASSERT_THAT(AreEqual(0,
			RemovedModuleResult.AddedModuleSources.Num()));
		ASSERT_THAT(AreEqual(1,
			RemovedModuleResult.RemovedModuleSources.Num()));
		ASSERT_THAT(AreEqual(UnrelatedSource.RelativePath,
			RemovedModuleResult.RemovedModuleSources[0]));
		ASSERT_THAT(IsFalse(IFileManager::Get().FileExists(*UnrelatedPath)));

		TArray<FString> TemporaryFiles;
		IFileManager::Get().FindFilesRecursive(
			TemporaryFiles, *OutputRoot, TEXT("*.asjit-tmp-*"), true, false);
		ASSERT_THAT(AreEqual(0, TemporaryFiles.Num()));
	}

	TEST_METHOD(CrlfCheckoutKeepsOwnedMarkerRecognizableForRegeneration)
	{
		using namespace AngelscriptJITProjectGenerationTests;
		const FString TestRoot = MakeTestRoot();
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
		};
		const FString OutputRoot = FPaths::Combine(TestRoot, TEXT("Generated"));
		const FAngelscriptJITGenerationOutput Output = MakeOutput(
			EAngelscriptJITTargetProfile::EditorDevelopment);
		ASSERT_THAT(IsTrue(
			FAngelscriptJITGeneratedFileStore::Publish(
				OutputRoot, Output).bSuccess));

		for (const FAngelscriptJITGeneratedFile& File : Output.Files)
		{
			FString CrlfContent = File.Content;
			CrlfContent.ReplaceInline(TEXT("\n"), TEXT("\r\n"),
				ESearchCase::CaseSensitive);
			ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
				CrlfContent,
				*FPaths::Combine(OutputRoot, File.RelativePath),
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)));
		}

		const FAngelscriptJITGeneratedFileComparison Comparison =
			FAngelscriptJITGeneratedFileStore::Compare(OutputRoot, Output);
		ASSERT_THAT(IsFalse(HasDifference(
			Comparison,
			EAngelscriptJITGeneratedFileDifference::OwnershipConflict)));
		ASSERT_THAT(IsTrue(HasDifference(
			Comparison,
			EAngelscriptJITGeneratedFileDifference::ContentMismatch)));
		const FAngelscriptJITGeneratedFilePublication Publication =
			FAngelscriptJITGeneratedFileStore::Publish(OutputRoot, Output);
		ASSERT_THAT(IsTrue(Publication.bSuccess, Publication.Error));
	}

	TEST_METHOD(GeneratePreflightsUserAndCrossProfileConflictsWithoutPartialWrites)
	{
		using namespace AngelscriptJITProjectGenerationTests;
		using namespace AngelscriptEditor::StaticJIT;
		const FString TestRoot = MakeTestRoot();
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
		};
		const FAngelscriptJITGenerationOutput EditorOutput = MakeOutput(
			EAngelscriptJITTargetProfile::EditorDevelopment);
		const FAngelscriptJITProjectGenerationResult EmptyRoot =
			FAngelscriptJITProjectGeneration::Generate(FStringView(), EditorOutput);
		ASSERT_THAT(IsFalse(EmptyRoot.bSuccess));
		ASSERT_THAT(IsTrue(HasDifference(EmptyRoot.Comparison,
			EAngelscriptJITGeneratedFileDifference::InvalidExpectedOutput)));

		const FString UserRoot = FPaths::Combine(TestRoot, TEXT("UserConflict"));
		const FString UserFile = FPaths::Combine(
			UserRoot, TEXT("Provider.generated.cpp"));
		ASSERT_THAT(IsTrue(IFileManager::Get().MakeDirectory(*UserRoot, true)));
		const FString UserOwnedContent =
			TEXT("// @angelscript-jit-owned revision=0 kind=provider-source\n")
			TEXT("// user-owned provider\n");
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			UserOwnedContent, *UserFile)));
		const FAngelscriptJITProjectGenerationResult UserConflict =
			FAngelscriptJITProjectGeneration::Generate(UserRoot, EditorOutput);
		ASSERT_THAT(IsFalse(UserConflict.bSuccess));
		ASSERT_THAT(IsTrue(HasDifference(UserConflict.Comparison,
			EAngelscriptJITGeneratedFileDifference::OwnershipConflict)));
		ASSERT_THAT(IsFalse(IFileManager::Get().FileExists(
			*FPaths::Combine(UserRoot,
				FindFile(EditorOutput,
					EAngelscriptJITGeneratedFileKind::ModuleSource).RelativePath))));
		FString UserContent;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			UserContent, *UserFile)));
		ASSERT_THAT(AreEqual(UserOwnedContent, UserContent));

		const FString ProfileRoot = FPaths::Combine(
			TestRoot, TEXT("ProfileConflict"));
		ASSERT_THAT(IsTrue(FAngelscriptJITProjectGeneration::Generate(
			ProfileRoot, EditorOutput).bSuccess));
		TArray<uint8> BeforeManifest;
		const FString ManifestPath = FPaths::Combine(
			ProfileRoot,
			FindFile(EditorOutput,
				EAngelscriptJITGeneratedFileKind::ProviderManifest).RelativePath);
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToArray(
			BeforeManifest, *ManifestPath)));

		const FAngelscriptJITProjectGenerationResult ProfileConflict =
			FAngelscriptJITProjectGeneration::Generate(
				ProfileRoot,
				MakeOutput(EAngelscriptJITTargetProfile::GameShipping));
		ASSERT_THAT(IsFalse(ProfileConflict.bSuccess));
		ASSERT_THAT(IsTrue(HasDifference(ProfileConflict.Comparison,
			EAngelscriptJITGeneratedFileDifference::TargetProfileMismatch)));
		TArray<uint8> AfterManifest;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToArray(
			AfterManifest, *ManifestPath)));
		ASSERT_THAT(IsTrue(BeforeManifest == AfterManifest));
	}

	TEST_METHOD(RevisionOneInventoryIsRemovalOnlyAndNeverDeletesUnownedLookalikes)
	{
		using namespace AngelscriptJITProjectGenerationTests;
		using namespace AngelscriptEditor::StaticJIT;
		const FString TestRoot = MakeTestRoot();
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
		};
		const FAngelscriptJITGenerationOutput Current = MakeOutput(
			EAngelscriptJITTargetProfile::EditorDevelopment);
		const FString LegacySlice =
			TEXT("Slices/legacy/function.jit.hpp");
		const FString LegacyBucket =
			TEXT("Buckets/JITBucket_00.generated.inl");
		auto SeedLegacyInventory = [&](const FString& Root,
			const FString& BucketContent)
		{
			const FString Inventory = FString::Printf(
				TEXT("{\n")
				TEXT("  \"ownershipRevision\": 1,\n")
				TEXT("  \"targetProfile\": \"EditorDevelopment\",\n")
				TEXT("  \"providerId\": \"%s\",\n")
				TEXT("  \"providerGeneration\": \"legacy\",\n")
				TEXT("  \"files\": [\n")
				TEXT("    \"Buckets/JITBucket_00.generated.inl\",\n")
				TEXT("    \"OwnedFiles.generated.json\",\n")
				TEXT("    \"Slices/legacy/function.jit.hpp\"\n")
				TEXT("  ]\n")
				TEXT("}\n"),
				*Current.ProviderId.Hash.ToHexString());
			const FString InventoryPath = FPaths::Combine(
				Root, TEXT("OwnedFiles.generated.json"));
			const FString SlicePath = FPaths::Combine(Root, LegacySlice);
			const FString BucketPath = FPaths::Combine(Root, LegacyBucket);
			IFileManager::Get().MakeDirectory(
				*FPaths::GetPath(SlicePath), true);
			IFileManager::Get().MakeDirectory(
				*FPaths::GetPath(BucketPath), true);
			check(FFileHelper::SaveStringToFile(
				Inventory, *InventoryPath,
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
			check(FFileHelper::SaveStringToFile(
				TEXT("// @angelscript-jit-owned revision=1 kind=function-slice\n")
				TEXT("#pragma once\n"),
				*SlicePath,
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
			check(FFileHelper::SaveStringToFile(
				BucketContent, *BucketPath,
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
		};

		const FString MigratedRoot = FPaths::Combine(
			TestRoot, TEXT("Migrated"));
		SeedLegacyInventory(MigratedRoot,
			TEXT("// @angelscript-jit-owned revision=1 kind=bucket\n"));
		const FAngelscriptJITProjectGenerationResult Migrated =
			FAngelscriptJITProjectGeneration::Generate(
				MigratedRoot, Current);
		ASSERT_THAT(IsTrue(Migrated.bSuccess, Migrated.Error));
		ASSERT_THAT(IsTrue(Migrated.Publication.RemovedFiles.Contains(
			LegacySlice)));
		ASSERT_THAT(IsTrue(Migrated.Publication.RemovedFiles.Contains(
			LegacyBucket)));
		ASSERT_THAT(IsFalse(IFileManager::Get().FileExists(
			*FPaths::Combine(MigratedRoot, LegacySlice))));
		ASSERT_THAT(IsFalse(IFileManager::Get().FileExists(
			*FPaths::Combine(MigratedRoot, LegacyBucket))));

		const FString ConflictRoot = FPaths::Combine(
			TestRoot, TEXT("Conflict"));
		const FString Lookalike = TEXT("// user-owned lookalike\n");
		SeedLegacyInventory(ConflictRoot, Lookalike);
		const FAngelscriptJITProjectGenerationResult Conflict =
			FAngelscriptJITProjectGeneration::Generate(
				ConflictRoot, Current);
		ASSERT_THAT(IsFalse(Conflict.bSuccess));
		ASSERT_THAT(IsTrue(HasDifference(Conflict.Comparison,
			EAngelscriptJITGeneratedFileDifference::OwnershipConflict)));
		FString LookalikeAfter;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			LookalikeAfter,
			*FPaths::Combine(ConflictRoot, LegacyBucket))));
		ASSERT_THAT(AreEqual(Lookalike, LookalikeAfter));
		ASSERT_THAT(IsFalse(IFileManager::Get().FileExists(
			*FPaths::Combine(ConflictRoot,
				Current.Modules[0].SourceRelativePath))));

		const FString ForgedRoot = FPaths::Combine(
			TestRoot, TEXT("ForgedLegacyPath"));
		const FString ForgedRelative = TEXT("Custom/Retained.cpp");
		const FString ForgedInventory = FString::Printf(
			TEXT("{\n")
			TEXT("  \"ownershipRevision\": 1,\n")
			TEXT("  \"targetProfile\": \"EditorDevelopment\",\n")
			TEXT("  \"providerId\": \"%s\",\n")
			TEXT("  \"providerGeneration\": \"legacy\",\n")
			TEXT("  \"files\": [\n")
			TEXT("    \"Custom/Retained.cpp\",\n")
			TEXT("    \"OwnedFiles.generated.json\"\n")
			TEXT("  ]\n")
			TEXT("}\n"),
			*Current.ProviderId.Hash.ToHexString());
		const FString ForgedPath = FPaths::Combine(
			ForgedRoot, ForgedRelative);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(ForgedPath), true);
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			ForgedInventory,
			*FPaths::Combine(ForgedRoot, TEXT("OwnedFiles.generated.json")),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)));
		const FString ForgedContent =
			TEXT("// @angelscript-jit-owned revision=1 kind=bucket\n")
			TEXT("// must not be removed outside the legacy bucket path\n");
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			ForgedContent, *ForgedPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)));
		const FAngelscriptJITProjectGenerationResult Forged =
			FAngelscriptJITProjectGeneration::Generate(ForgedRoot, Current);
		ASSERT_THAT(IsFalse(Forged.bSuccess));
		ASSERT_THAT(IsTrue(HasDifference(Forged.Comparison,
			EAngelscriptJITGeneratedFileDifference::OwnershipConflict)));
		FString ForgedAfter;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			ForgedAfter, *ForgedPath)));
		ASSERT_THAT(AreEqual(ForgedContent, ForgedAfter));
	}

	TEST_METHOD(ProfileRootMigrationReportsThenRemovesOnlyRevisionTwoOwnedFiles)
	{
		using namespace AngelscriptJITProjectGenerationTests;
		using namespace AngelscriptEditor::StaticJIT;
		const FString TestRoot = MakeTestRoot();
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
		};
		const FString ProjectFile = FPaths::Combine(
			TestRoot, TEXT("Fixture.uproject"));
		const FString NewRoot =
			FAngelscriptJITProjectGeneration::GetProfileOutputDirectory(
				ProjectFile,
				EAngelscriptJITTargetProfile::EditorDevelopment);
		const FString LegacyRoot = FPaths::Combine(
			TestRoot,
			TEXT("Source/AngelscriptJIT/Private/Generated/Profiles/EditorDevelopment"));
		const FString VerifyTemp = FPaths::Combine(TestRoot, TEXT("VerifyTemp"));
		const FAngelscriptJITGenerationOutput Current = MakeOutput(
			EAngelscriptJITTargetProfile::EditorDevelopment);
		const FAngelscriptJITGeneratedFilePublication Seeded =
			FAngelscriptJITGeneratedFileStore::Publish(LegacyRoot, Current);
		ASSERT_THAT(IsTrue(Seeded.bSuccess, Seeded.Error));
		const FString UserFile = FPaths::Combine(
			LegacyRoot, TEXT("Keep/UserNotes.txt"));
		ASSERT_THAT(IsTrue(IFileManager::Get().MakeDirectory(
			*FPaths::GetPath(UserFile), true)));
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			TEXT("keep\n"), *UserFile)));

		const FAngelscriptJITProjectGenerationResult Verified =
			FAngelscriptJITProjectGeneration::Verify(
				NewRoot, VerifyTemp, Current);
		ASSERT_THAT(IsFalse(Verified.bSuccess));
		ASSERT_THAT(IsTrue(HasDifference(Verified.Comparison,
			EAngelscriptJITGeneratedFileDifference::UnexpectedOwnedFile)));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(*UserFile)));

		const FAngelscriptJITProjectGenerationResult Migrated =
			FAngelscriptJITProjectGeneration::Generate(NewRoot, Current);
		ASSERT_THAT(IsTrue(Migrated.bSuccess, Migrated.Error));
		ASSERT_THAT(AreEqual(2, Migrated.RemovedModuleSources.Num()));
		for (const FString& OwnedFile : Current.OwnedFiles)
		{
			ASSERT_THAT(IsFalse(IFileManager::Get().FileExists(
				*FPaths::Combine(LegacyRoot, OwnedFile))));
			ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
				*FPaths::Combine(NewRoot, OwnedFile))));
		}
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(*UserFile)));
	}

	TEST_METHOD(PrivateProfileRootMigratesToRootGeneratedAndRemovesEmptyWrappers)
	{
		using namespace AngelscriptJITProjectGenerationTests;
		using namespace AngelscriptEditor::StaticJIT;
		const FString TestRoot = MakeTestRoot();
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
		};
		const FString ProjectFile = FPaths::Combine(
			TestRoot, TEXT("Fixture.uproject"));
		const FString NewRoot = FPaths::Combine(
			TestRoot,
			TEXT("Source/AngelscriptJIT/Generated/EditorDevelopment"));
		const FString LegacyRoot = FPaths::Combine(
			TestRoot,
			TEXT("Source/AngelscriptJIT/Private/Generated/EditorDevelopment"));
		const FString VerifyTemp = FPaths::Combine(TestRoot, TEXT("VerifyTemp"));
		const FAngelscriptJITGenerationOutput Current = MakeOutput(
			EAngelscriptJITTargetProfile::EditorDevelopment);
		ASSERT_THAT(IsTrue(FAngelscriptJITGeneratedFileStore::Publish(
			LegacyRoot, Current).bSuccess));
		ASSERT_THAT(AreEqual(
			NewRoot,
			FAngelscriptJITProjectGeneration::GetProfileOutputDirectory(
				ProjectFile,
				EAngelscriptJITTargetProfile::EditorDevelopment)));

		const FAngelscriptJITProjectGenerationResult Verified =
			FAngelscriptJITProjectGeneration::Verify(
				NewRoot, VerifyTemp, Current);
		ASSERT_THAT(IsFalse(Verified.bSuccess));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
			*FPaths::Combine(
				LegacyRoot, TEXT("OwnedFiles.generated.json")))));

		const FAngelscriptJITProjectGenerationResult Migrated =
			FAngelscriptJITProjectGeneration::Generate(NewRoot, Current);
		ASSERT_THAT(IsTrue(Migrated.bSuccess, Migrated.Error));
		for (const FString& OwnedFile : Current.OwnedFiles)
		{
			ASSERT_THAT(IsFalse(IFileManager::Get().FileExists(
				*FPaths::Combine(LegacyRoot, OwnedFile))));
			ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
				*FPaths::Combine(NewRoot, OwnedFile))));
		}
		ASSERT_THAT(IsFalse(IFileManager::Get().DirectoryExists(
			*FPaths::Combine(
				TestRoot, TEXT("Source/AngelscriptJIT/Private")))));
	}

	TEST_METHOD(ProfileRootMigrationRemovesTheEmptyProfilesWrapper)
	{
		using namespace AngelscriptJITProjectGenerationTests;
		using namespace AngelscriptEditor::StaticJIT;
		const FString TestRoot = MakeTestRoot();
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
		};
		const FString ProjectFile = FPaths::Combine(
			TestRoot, TEXT("Fixture.uproject"));
		const FString NewRoot =
			FAngelscriptJITProjectGeneration::GetProfileOutputDirectory(
				ProjectFile,
				EAngelscriptJITTargetProfile::EditorDevelopment);
		const FString LegacyProfilesRoot = FPaths::Combine(
			TestRoot,
			TEXT("Source/AngelscriptJIT/Private/Generated/Profiles"));
		const FString LegacyRoot = FPaths::Combine(
			LegacyProfilesRoot, TEXT("EditorDevelopment"));
		const FAngelscriptJITGenerationOutput Current = MakeOutput(
			EAngelscriptJITTargetProfile::EditorDevelopment);
		ASSERT_THAT(IsTrue(FAngelscriptJITGeneratedFileStore::Publish(
			LegacyRoot, Current).bSuccess));

		const FAngelscriptJITProjectGenerationResult Migrated =
			FAngelscriptJITProjectGeneration::Generate(NewRoot, Current);
		ASSERT_THAT(IsTrue(Migrated.bSuccess, Migrated.Error));
		ASSERT_THAT(IsFalse(IFileManager::Get().DirectoryExists(
			*LegacyProfilesRoot)));
	}

	TEST_METHOD(ProfileRootMigrationRefusesModifiedRevisionTwoOwnedFiles)
	{
		using namespace AngelscriptJITProjectGenerationTests;
		using namespace AngelscriptEditor::StaticJIT;
		const FString TestRoot = MakeTestRoot();
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
		};
		const FString ProjectFile = FPaths::Combine(
			TestRoot, TEXT("Fixture.uproject"));
		const FString NewRoot =
			FAngelscriptJITProjectGeneration::GetProfileOutputDirectory(
				ProjectFile,
				EAngelscriptJITTargetProfile::EditorDevelopment);
		const FString LegacyRoot = FPaths::Combine(
			TestRoot,
			TEXT("Source/AngelscriptJIT/Private/Generated/Profiles/EditorDevelopment"));
		const FAngelscriptJITGenerationOutput Current = MakeOutput(
			EAngelscriptJITTargetProfile::EditorDevelopment);
		ASSERT_THAT(IsTrue(FAngelscriptJITGeneratedFileStore::Publish(
			LegacyRoot, Current).bSuccess));
		const FString ModifiedOwnedPath = FPaths::Combine(
			LegacyRoot, Current.Modules[0].SourceRelativePath);
		const FString UserContent = TEXT("// user replaced this old generated file\n");
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			UserContent, *ModifiedOwnedPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)));

		const FAngelscriptJITProjectGenerationResult Rejected =
			FAngelscriptJITProjectGeneration::Generate(NewRoot, Current);
		ASSERT_THAT(IsFalse(Rejected.bSuccess));
		ASSERT_THAT(IsTrue(HasDifference(Rejected.Comparison,
			EAngelscriptJITGeneratedFileDifference::InvalidInventory)));
		ASSERT_THAT(IsFalse(IFileManager::Get().FileExists(
			*FPaths::Combine(NewRoot, TEXT("OwnedFiles.generated.json")))));
		FString PreservedContent;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			PreservedContent, *ModifiedOwnedPath)));
		ASSERT_THAT(AreEqual(UserContent, PreservedContent));
	}

	TEST_METHOD(VerifyUsesIsolatedTemporaryOutputAndNeverMutatesProjectFiles)
	{
		using namespace AngelscriptJITProjectGenerationTests;
		using namespace AngelscriptEditor::StaticJIT;
		const FString TestRoot = MakeTestRoot();
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
		};
		const FString OutputRoot = FPaths::Combine(TestRoot, TEXT("ProjectOutput"));
		const FString TempRoot = FPaths::Combine(TestRoot, TEXT("VerifyTemp"));
		const FAngelscriptJITGenerationOutput Baseline = MakeOutput(
			EAngelscriptJITTargetProfile::EditorDevelopment);
		ASSERT_THAT(IsTrue(FAngelscriptJITProjectGeneration::Generate(
			OutputRoot, Baseline).bSuccess));

		const FAngelscriptJITGeneratedFile& ProviderSource = FindFile(
			Baseline, EAngelscriptJITGeneratedFileKind::ProviderSource);
		const FString ProviderPath = FPaths::Combine(
			OutputRoot, ProviderSource.RelativePath);
		const FDateTime FrozenTimestamp(2025, 2, 3, 4, 5, 6);
		IFileManager::Get().SetTimeStamp(*ProviderPath, FrozenTimestamp);
		TArray<uint8> BeforeVerify;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToArray(
			BeforeVerify, *ProviderPath)));

		const FAngelscriptJITProjectGenerationResult Current =
			FAngelscriptJITProjectGeneration::Verify(
				OutputRoot, TempRoot, Baseline);
		ASSERT_THAT(IsTrue(Current.bSuccess, Current.Error));
		ASSERT_THAT(IsTrue(Current.Comparison.IsCurrent()));
		TArray<uint8> AfterVerify;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToArray(
			AfterVerify, *ProviderPath)));
		ASSERT_THAT(IsTrue(BeforeVerify == AfterVerify));
		ASSERT_THAT(IsTrue(IFileManager::Get().GetTimeStamp(*ProviderPath)
			== FrozenTimestamp));
		ASSERT_THAT(IsFalse(IFileManager::Get().DirectoryExists(
			*Current.VerificationDirectory)));

		FString StaleProvider = ProviderSource.Content
			+ TEXT("// stale but still owned\n");
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			StaleProvider, *ProviderPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)));
		const FAngelscriptJITProjectGenerationResult Stale =
			FAngelscriptJITProjectGeneration::Verify(
				OutputRoot, TempRoot, Baseline);
		ASSERT_THAT(IsFalse(Stale.bSuccess));
		const FAngelscriptJITGeneratedFileDifferenceRecord* StaleDifference =
			FindDifference(Stale.Comparison,
				EAngelscriptJITGeneratedFileDifference::ContentMismatch);
		ASSERT_THAT(IsNotNull(StaleDifference));
		ASSERT_THAT(IsFalse(StaleDifference->ExpectedHash.IsZero()));
		ASSERT_THAT(IsFalse(StaleDifference->ActualHash.IsZero()));
		ASSERT_THAT(IsFalse(StaleDifference->ExpectedHash
			== StaleDifference->ActualHash));
		FString StillStale;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			StillStale, *ProviderPath)));
		ASSERT_THAT(IsTrue(StillStale.Contains(TEXT("stale but still owned"))));

		ASSERT_THAT(IsTrue(FAngelscriptJITProjectGeneration::Generate(
			OutputRoot, Baseline).bSuccess));
		const FAngelscriptJITGenerationOutput Changed = MakeOutput(
			EAngelscriptJITTargetProfile::EditorDevelopment,
			TEXT("Execution-C"));
		const FAngelscriptJITProjectGenerationResult ChangedVerify =
			FAngelscriptJITProjectGeneration::Verify(
				OutputRoot, TempRoot, Changed);
		ASSERT_THAT(IsFalse(ChangedVerify.bSuccess));
		ASSERT_THAT(IsTrue(HasDifference(ChangedVerify.Comparison,
			EAngelscriptJITGeneratedFileDifference::ContentMismatch)));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
			*FPaths::Combine(
				OutputRoot,
				FindFile(Baseline,
					EAngelscriptJITGeneratedFileKind::ModuleSource).RelativePath))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
