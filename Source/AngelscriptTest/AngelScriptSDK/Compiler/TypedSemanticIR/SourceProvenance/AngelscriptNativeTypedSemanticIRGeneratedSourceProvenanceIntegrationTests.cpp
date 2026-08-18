#include "CQTest.h"

#include "StaticJIT/AngelscriptHIRDumpCommand.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS && AS_CAN_GENERATE_JIT

namespace AngelscriptTypedSemanticIRGeneratedSourceProvenanceIntegrationTests
{
	using namespace AngelscriptEditor::StaticJIT;

	FString MakeGeneratedSourceProject(FString& OutRoot)
	{
		OutRoot = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/TypedSemanticIRSourceProvenance"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
		check(IFileManager::Get().MakeDirectory(*OutRoot, true));
		const FString ProjectFile = FPaths::Combine(
			OutRoot, TEXT("SourceProvenanceFixture.uproject"));
		check(FFileHelper::SaveStringToFile(
			TEXT("{\n  \"FileVersion\": 3,\n  \"Modules\": []\n}\n"),
			*ProjectFile,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
		const FString ScriptRoot = FPaths::Combine(OutRoot, TEXT("Script"));
		check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		check(FFileHelper::SaveStringToFile(
			TEXT("// 非 ASCII 前缀验证来源范围使用 UTF-8 字节。\n")
			TEXT("UCLASS()\n")
			TEXT("class UGeneratedProvenanceSubsystem : UEngineSubsystem\n")
			TEXT("{\n")
			TEXT("}\n\n")
			TEXT("asset GeneratedProvenanceAsset of UObject\n")
			TEXT("{\n")
			TEXT("}\n"),
			*FPaths::Combine(ScriptRoot, TEXT("GeneratedProvenance.as")),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
		return ProjectFile;
	}

	FString ReadRequired(const FString& Path)
	{
		FString Content;
		check(FFileHelper::LoadFileToString(Content, *Path));
		return Content;
	}

	FString BuildProvenanceFailureExcerpt(const FString& Dump)
	{
		TArray<FString> Lines;
		Dump.ParseIntoArrayLines(Lines, false);
		FString Excerpt;
		for (const FString& Line : Lines)
		{
			if (Line.StartsWith(TEXT("Declaration="))
				|| Line.StartsWith(TEXT("CaptureState="))
				|| Line.Contains(TEXT("processed="))
				|| Line.Contains(TEXT("authored="))
				|| Line.Contains(TEXT("generated="))
				|| Line.Contains(TEXT("GeneratedProvenanceAsset")))
			{
				Excerpt += Line;
				Excerpt += TEXT("\n");
			}
		}
		return Excerpt;
	}
}

TEST_CLASS_WITH_FLAGS(
	FAngelscriptNativeTypedSemanticIRGeneratedSourceProvenanceIntegrationTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR.SourceProvenance.Integration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(AssetAndSubsystemOriginsReachTheNormalizedHIRDump)
	{
		using namespace AngelscriptEditor::StaticJIT;
		using namespace
			AngelscriptTypedSemanticIRGeneratedSourceProvenanceIntegrationTests;

		FString Root;
		const FString ProjectFile = MakeGeneratedSourceProject(Root);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};

		FAngelscriptHIRDumpRequest Request;
		FString Error;
		ASSERT_THAT(IsTrue(FAngelscriptHIRDumpCommand::Parse(
			TEXT("Profile=EditorDevelopment Format=Text"),
			ProjectFile,
			Request,
			Error), Error));
		const FAngelscriptHIRDumpResult Result =
			FAngelscriptHIRDumpCommand::Execute(Request);
		ASSERT_THAT(IsTrue(Result.IsSuccess(), Result.Error));
		if (!Result.IsSuccess())
		{
			return;
		}

		const FString Dump = ReadRequired(Result.TextOutputPath);
		const FString ProvenanceExcerpt = BuildProvenanceFailureExcerpt(Dump);
		ASSERT_THAT(IsTrue(Dump.Contains(TEXT("processed="))));
		ASSERT_THAT(IsTrue(Dump.Contains(TEXT("generated=LiteralAsset")),
			ProvenanceExcerpt));
		ASSERT_THAT(IsTrue(Dump.Contains(
			TEXT("generator=LiteralAsset:GeneratedProvenanceAsset"))));
		ASSERT_THAT(IsTrue(Dump.Contains(
			TEXT("generated=SubsystemAccessor"))));
		ASSERT_THAT(IsTrue(Dump.Contains(
			TEXT("generator=SubsystemAccessor:UGeneratedProvenanceSubsystem"))));
		ASSERT_THAT(IsTrue(Dump.Contains(TEXT("anchor="))));
	}
};

#endif
