#include "CQTest.h"

#include "StaticJIT/AngelscriptHIRDumpCommand.h"
#include "StaticJIT/AngelscriptHIRDumpCommandlet.h"
#include "StaticJIT/AngelscriptJITCommand.h"
#include "StaticJIT/AngelscriptJITCommandlet.h"
#include "StaticJIT/AngelscriptJITProjectGeneration.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITBackend.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT

namespace AngelscriptHIRDumpCommandletTests
{
	using namespace AngelscriptEditor::StaticJIT;

	static FString MakeProject(FString& OutRoot)
	{
		OutRoot = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/AngelscriptHIRDump"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
		check(IFileManager::Get().MakeDirectory(*OutRoot, true));
		const FString ProjectFile = FPaths::Combine(
			OutRoot, TEXT("HIRDumpFixture.uproject"));
		check(FFileHelper::SaveStringToFile(
			TEXT("{\n  \"FileVersion\": 3,\n  \"Modules\": []\n}\n"),
			*ProjectFile,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
		const FString ScriptRoot = FPaths::Combine(OutRoot, TEXT("Script"));
		check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		check(FFileHelper::SaveStringToFile(
			TEXT("int HIRDumpHelper(int Value)\n")
			TEXT("{\n")
			TEXT("\treturn Value + 1;\n")
			TEXT("}\n\n")
			TEXT("UFUNCTION()\n")
			TEXT("int HIRDumpEntry(int Value)\n")
			TEXT("{\n")
			TEXT("\treturn HIRDumpHelper(Value);\n")
			TEXT("}\n"),
			*FPaths::Combine(ScriptRoot, TEXT("HIRDumpFixture.as")),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
		return ProjectFile;
	}

	static FString ReadRequired(const FString& Path)
	{
		FString Content;
		check(FFileHelper::LoadFileToString(Content, *Path));
		return Content;
	}

	static const FAngelscriptHIRDumpFunctionResult* FindEntry(
		const FAngelscriptHIRDumpResult& Result)
	{
		return Result.SelectedFunctions.FindByPredicate(
			[](const FAngelscriptHIRDumpFunctionResult& Function)
			{
				return Function.CanonicalDeclaration.Contains(
					TEXT("HIRDumpEntry"));
			});
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptHIRDumpCommandletTests,
	"Angelscript.TestModule.StaticJIT.HIRDump.Commandlet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ParseRequiresConcreteProfileAndDefaultsToBoth)
	{
		using namespace AngelscriptEditor::StaticJIT;
		using namespace AngelscriptHIRDumpCommandletTests;
		FString Root;
		const FString ProjectFile = MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};

		FAngelscriptHIRDumpRequest Request;
		FString Error;
		ASSERT_THAT(IsFalse(FAngelscriptHIRDumpCommand::Parse(
			TEXT("Format=Both"), ProjectFile, Request, Error)));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("requires -Profile="))));
		ASSERT_THAT(IsFalse(FAngelscriptHIRDumpCommand::Parse(
			TEXT("Profile=All"), ProjectFile, Request, Error)));
		ASSERT_THAT(IsFalse(FAngelscriptHIRDumpCommand::Parse(
			TEXT("Profile=EditorDevelopment Format=Yaml"),
			ProjectFile, Request, Error)));

		ASSERT_THAT(IsTrue(FAngelscriptHIRDumpCommand::Parse(
			TEXT("Profile=EditorDevelopment"),
			ProjectFile, Request, Error), Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptHIRDumpRequestKind::DeveloperHIRDump,
			Request.RequestKind));
		ASSERT_THAT(AreEqual(
			EAngelscriptJITTargetProfile::EditorDevelopment,
			Request.TargetProfile));
		ASSERT_THAT(AreEqual(
			EAngelscriptHIRDumpFormat::Both, Request.Format));
		ASSERT_THAT(IsTrue(Request.OutputDirectory.EndsWith(
			TEXT("Saved/Angelscript/HIRDump/EditorDevelopment"))));

		FAngelscriptJITCommandRequest JITRequest;
		ASSERT_THAT(IsFalse(FAngelscriptJITCommand::Parse(
			TEXT("Mode=DumpHIR Profile=EditorDevelopment"),
			ProjectFile, JITRequest, Error),
			TEXT("HIR dumping must not become an AngelscriptJIT mode")));
	}

	TEST_METHOD(ParseNormalizesStableFiltersAndRejectsPartialKeys)
	{
		using namespace AngelscriptEditor::StaticJIT;
		using namespace AngelscriptHIRDumpCommandletTests;
		FString Root;
		const FString ProjectFile = MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};
		const FString UpperKey = FString::ChrN(64, TEXT('A'));
		const FString Output = FPaths::Combine(Root, TEXT("ExplicitOutput"));
		FAngelscriptHIRDumpRequest Request;
		FString Error;
		ASSERT_THAT(IsTrue(FAngelscriptHIRDumpCommand::Parse(
			FString::Printf(
				TEXT("Profile=GameShipping Format=Json Module=%s Function=%s Output=\"%s\""),
				*UpperKey, *UpperKey, *Output),
			ProjectFile, Request, Error), Error));
		ASSERT_THAT(AreEqual(FString::ChrN(64, TEXT('a')),
			Request.StableModuleFilter));
		ASSERT_THAT(AreEqual(FString::ChrN(64, TEXT('a')),
			Request.StableFunctionFilter));
		ASSERT_THAT(AreEqual(
			EAngelscriptHIRDumpFormat::Json, Request.Format));
		ASSERT_THAT(IsTrue(Request.OutputDirectory.EndsWith(
			TEXT("ExplicitOutput"))));
		ASSERT_THAT(IsFalse(FAngelscriptHIRDumpCommand::Parse(
			TEXT("Profile=EditorDevelopment Module=abc123"),
			ProjectFile, Request, Error)));
		ASSERT_THAT(IsFalse(FAngelscriptHIRDumpCommand::Parse(
			TEXT("Profile=EditorDevelopment Function=not-a-stable-key"),
			ProjectFile, Request, Error)));
	}

	TEST_METHOD(ExecuteProducesDeterministicPointerFreeTextAndJsonWithoutProvider)
	{
		using namespace AngelscriptEditor::StaticJIT;
		using namespace AngelscriptHIRDumpCommandletTests;
		FString Root;
		const FString ProjectFile = MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};

		FAngelscriptHIRDumpRequest Request;
		FString Error;
		ASSERT_THAT(IsTrue(FAngelscriptHIRDumpCommand::Parse(
			TEXT("Profile=EditorDevelopment"),
			ProjectFile, Request, Error), Error));
		FAngelscriptTypedASTJIT::ResetTaskObservationForTesting();
		const FAngelscriptHIRDumpResult First =
			FAngelscriptHIRDumpCommand::Execute(Request);
		ASSERT_THAT(IsTrue(First.IsSuccess(), First.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptHIRDumpRequestKind::DeveloperHIRDump,
			First.RequestKind));
		ASSERT_THAT(AreEqual(1u, First.SourceCompilePassCount));
		ASSERT_THAT(IsTrue(First.CompleteModuleCount > 0));
		ASSERT_THAT(IsTrue(First.CompleteFunctionCount >= 2));
		ASSERT_THAT(IsTrue(First.CapturedFunctionCount >= 2));
		ASSERT_THAT(AreEqual(0, First.InvalidFunctionCount));
		ASSERT_THAT(IsFalse(First.CompleteGraphIdentity.IsZero()));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
			*First.TextOutputPath)));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
			*First.JsonOutputPath)));

		const FString FirstText = ReadRequired(First.TextOutputPath);
		const FString FirstJson = ReadRequired(First.JsonOutputPath);
		ASSERT_THAT(IsTrue(FirstText.Contains(
			TEXT("RequestKind=DeveloperHIRDump"))));
		ASSERT_THAT(IsTrue(FirstJson.Contains(
			TEXT("\"requestKind\": \"DeveloperHIRDump\""))));
		ASSERT_THAT(IsTrue(FirstText.Contains(TEXT("HIRDumpEntry"))));
		ASSERT_THAT(IsTrue(FirstJson.Contains(TEXT("HIRDumpHelper"))));
		ASSERT_THAT(IsFalse(FirstText.Contains(TEXT("functionId=")),
			TEXT("Engine-local call IDs must be normalized to stable targets")));
		ASSERT_THAT(IsFalse(FirstJson.Contains(TEXT("functionId="))));
		ASSERT_THAT(IsFalse(FirstText.Contains(TEXT("0x")),
			TEXT("Pointer-shaped values are forbidden in a dump")));
		ASSERT_THAT(IsFalse(FirstJson.Contains(TEXT("0x"))));
		TArray<FString> CppFiles;
		IFileManager::Get().FindFilesRecursive(
			CppFiles, *First.OutputDirectory, TEXT("*.cpp"), true, false);
		ASSERT_THAT(AreEqual(0, CppFiles.Num()));
		const FAngelscriptTypedASTJITTaskObservation NoBackend =
			FAngelscriptTypedASTJIT::GetTaskObservationForTesting();
		ASSERT_THAT(AreEqual(0, NoBackend.CreatedInstanceCount));
		ASSERT_THAT(AreEqual(0, NoBackend.GenerateInvocationCount));

		const FAngelscriptHIRDumpResult Second =
			FAngelscriptHIRDumpCommand::Execute(Request);
		ASSERT_THAT(IsTrue(Second.IsSuccess(), Second.Error));
		ASSERT_THAT(AreEqual(
			First.CompleteGraphIdentity.ToHexString(),
			Second.CompleteGraphIdentity.ToHexString()));
		ASSERT_THAT(AreEqual(FirstText,
			ReadRequired(Second.TextOutputPath)));
		ASSERT_THAT(AreEqual(FirstJson,
			ReadRequired(Second.JsonOutputPath)));
	}

	TEST_METHOD(FiltersRestrictOnlyOutputAndStaticJITIgnoresDumpFiles)
	{
		using namespace AngelscriptEditor::StaticJIT;
		using namespace AngelscriptHIRDumpCommandletTests;
		FString Root;
		const FString ProjectFile = MakeProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};

		FAngelscriptHIRDumpRequest CompleteRequest;
		FString Error;
		ASSERT_THAT(IsTrue(FAngelscriptHIRDumpCommand::Parse(
			TEXT("Profile=EditorDevelopment"), ProjectFile,
			CompleteRequest, Error), Error));
		const FAngelscriptHIRDumpResult Complete =
			FAngelscriptHIRDumpCommand::Execute(CompleteRequest);
		ASSERT_THAT(IsTrue(Complete.IsSuccess(), Complete.Error));
		const FAngelscriptHIRDumpFunctionResult* Entry = FindEntry(Complete);
		ASSERT_THAT(IsNotNull(Entry));
		if (Entry == nullptr)
		{
			return;
		}

		const FString FilterRoot = FPaths::Combine(Root, TEXT("Filtered"));
		FAngelscriptHIRDumpRequest FilteredRequest;
		ASSERT_THAT(IsTrue(FAngelscriptHIRDumpCommand::Parse(
			FString::Printf(
				TEXT("Profile=EditorDevelopment Format=Text Module=%s Function=%s Output=\"%s\""),
				*Entry->ModuleKey.Hash.ToHexString(),
				*Entry->FunctionKey.Hash.ToHexString(),
				*FilterRoot),
			ProjectFile, FilteredRequest, Error), Error));
		const FAngelscriptHIRDumpResult Filtered =
			FAngelscriptHIRDumpCommand::Execute(FilteredRequest);
		ASSERT_THAT(IsTrue(Filtered.IsSuccess(), Filtered.Error));
		ASSERT_THAT(AreEqual(Complete.CompleteModuleCount,
			Filtered.CompleteModuleCount));
		ASSERT_THAT(AreEqual(Complete.CompleteFunctionCount,
			Filtered.CompleteFunctionCount));
		ASSERT_THAT(AreEqual(1, Filtered.SelectedFunctions.Num()));
		ASSERT_THAT(AreEqual(1u, Filtered.SourceCompilePassCount));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
			*Filtered.TextOutputPath)));
		ASSERT_THAT(IsTrue(Filtered.JsonOutputPath.IsEmpty()));
		const FString FilteredText = ReadRequired(Filtered.TextOutputPath);
		ASSERT_THAT(IsTrue(FilteredText.Contains(TEXT("HIRDumpEntry"))));
		ASSERT_THAT(IsFalse(FilteredText.Contains(
			TEXT("function HIRDumpHelper"))));

		FAngelscriptTypedASTJIT::ResetTaskObservationForTesting();
		FAngelscriptJITGenerationOutput BaselineStaticOutput;
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProjectGeneration::BuildProjectArtifacts(
				ProjectFile,
				EAngelscriptJITTargetProfile::EditorDevelopment,
				BaselineStaticOutput, &Error), Error));
		const FAngelscriptTypedASTJITTaskObservation BaselineObservation =
			FAngelscriptTypedASTJIT::GetTaskObservationForTesting();
		ASSERT_THAT(AreEqual(1, BaselineObservation.GenerateInvocationCount));
		ASSERT_THAT(IsTrue(
			BaselineObservation.bLastTaskUsedOnlySameCompilationHIR));

		const FString OriginalText = ReadRequired(Complete.TextOutputPath);
		const FString OriginalJson = ReadRequired(Complete.JsonOutputPath);
		const FString ScriptRoot = FPaths::Combine(Root, TEXT("Script"));
		const FString StaleTextPath = FPaths::Combine(
			ScriptRoot, TEXT("StaleSnapshot.hir.txt"));
		const FString EditedJsonPath = FPaths::Combine(
			ScriptRoot, TEXT("EditedSnapshot.hir.json"));
		const FString MaliciousTextPath = FPaths::Combine(
			ScriptRoot, TEXT("MaliciousSnapshot.hir.txt"));
		const FString MissingJsonPath = FPaths::Combine(
			ScriptRoot, TEXT("MissingSnapshot.hir.json"));
		FString EditedJson = OriginalJson;
		EditedJson.ReplaceInline(
			TEXT("HIRDumpEntry"), TEXT("PersistedEditMustNotWin"));
		const FString MaliciousText =
			TEXT("RequestKind=StaticJITArtifact\n")
			TEXT("ProviderId=ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff\n")
			TEXT("normalizedHIR=void Injected() { *((int*)0x1) = 7; }\n");
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			OriginalText, *StaleTextPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)));
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			EditedJson, *EditedJsonPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)));
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			MaliciousText, *MaliciousTextPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)));
		ASSERT_THAT(IsFalse(IFileManager::Get().FileExists(*MissingJsonPath)));

		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			TEXT("deliberately corrupt persisted HIR that must be ignored\n"),
			*Complete.TextOutputPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)));
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			MaliciousText,
			*Complete.JsonOutputPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)));
		const FString CorruptText = ReadRequired(Complete.TextOutputPath);
		const FString CorruptJson = ReadRequired(Complete.JsonOutputPath);
		FAngelscriptTypedASTJIT::ResetTaskObservationForTesting();
		FAngelscriptJITGenerationOutput StaticOutput;
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProjectGeneration::BuildProjectArtifacts(
				ProjectFile,
				EAngelscriptJITTargetProfile::EditorDevelopment,
				StaticOutput, &Error), Error));
		const FAngelscriptTypedASTJITTaskObservation StaticObservation =
			FAngelscriptTypedASTJIT::GetTaskObservationForTesting();
		ASSERT_THAT(AreEqual(1, StaticObservation.GenerateInvocationCount));
		ASSERT_THAT(IsTrue(
			StaticObservation.bLastTaskUsedOnlySameCompilationHIR));
		ASSERT_THAT(IsTrue(StaticOutput.Files.ContainsByPredicate(
			[](const FAngelscriptJITGeneratedFile& File)
			{
				return File.RelativePath.EndsWith(TEXT(".cpp"));
			})));
		ASSERT_THAT(AreEqual(CorruptText,
			ReadRequired(Complete.TextOutputPath),
			TEXT("StaticJIT must neither read back nor rewrite HIR dumps")));
		ASSERT_THAT(AreEqual(CorruptJson,
			ReadRequired(Complete.JsonOutputPath),
			TEXT("StaticJIT must neither read back nor rewrite JSON HIR dumps")));
		ASSERT_THAT(AreEqual(OriginalText, ReadRequired(StaleTextPath)));
		ASSERT_THAT(AreEqual(EditedJson, ReadRequired(EditedJsonPath)));
		ASSERT_THAT(AreEqual(MaliciousText, ReadRequired(MaliciousTextPath)));
		ASSERT_THAT(IsFalse(IFileManager::Get().FileExists(*MissingJsonPath)));

		ASSERT_THAT(AreEqual(
			BaselineStaticOutput.ProviderId.Hash.ToHexString(),
			StaticOutput.ProviderId.Hash.ToHexString(),
			TEXT("Persisted HIR must not change Provider identity")));
		ASSERT_THAT(AreEqual(
			BaselineStaticOutput.ProviderGeneration.Hash.ToHexString(),
			StaticOutput.ProviderGeneration.Hash.ToHexString(),
			TEXT("Persisted HIR must not change Provider generation")));
		ASSERT_THAT(AreEqual(
			BaselineStaticOutput.ArtifactSetDigest.ToHexString(),
			StaticOutput.ArtifactSetDigest.ToHexString(),
			TEXT("Persisted HIR must not change the artifact-set identity")));
		ASSERT_THAT(AreEqual(
			BaselineStaticOutput.Files.Num(), StaticOutput.Files.Num()));
		for (int32 FileIndex = 0;
			FileIndex < BaselineStaticOutput.Files.Num()
				&& FileIndex < StaticOutput.Files.Num();
			++FileIndex)
		{
			const FAngelscriptJITGeneratedFile& BaselineFile =
				BaselineStaticOutput.Files[FileIndex];
			const FAngelscriptJITGeneratedFile& CurrentFile =
				StaticOutput.Files[FileIndex];
			ASSERT_THAT(AreEqual(BaselineFile.RelativePath,
				CurrentFile.RelativePath));
			ASSERT_THAT(AreEqual(BaselineFile.Kind, CurrentFile.Kind));
			ASSERT_THAT(AreEqual(BaselineFile.Content, CurrentFile.Content,
				TEXT("Persisted HIR must not change generated C++ or Provider metadata")));
		}
		ASSERT_THAT(IsFalse(StaticOutput.Files.ContainsByPredicate(
			[](const FAngelscriptJITGeneratedFile& File)
			{
				return File.RelativePath.EndsWith(TEXT(".hir.txt"))
					|| File.RelativePath.EndsWith(TEXT(".hir.json"));
			})));
	}

	TEST_METHOD(UObjectCommandletIsDedicatedAndRejectsMissingProfile)
	{
		using namespace AngelscriptEditor::StaticJIT;
		UAngelscriptHIRDumpCommandlet* Commandlet =
			NewObject<UAngelscriptHIRDumpCommandlet>();
		ASSERT_THAT(IsNotNull(Commandlet));
		ASSERT_THAT(IsFalse(Commandlet->IsA<UAngelscriptJITCommandlet>()));
		TestRunner->AddExpectedErrorPlain(
			TEXT("AngelscriptHIRDump requires -Profile=EditorDevelopment|GameDevelopment|GameShipping."),
			EAutomationExpectedErrorFlags::Exact,
			1);
		ASSERT_THAT(AreEqual(
			static_cast<int32>(
				EAngelscriptHIRDumpExitCode::InvalidArguments),
			Commandlet->Main(TEXT("Format=Both"))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT
