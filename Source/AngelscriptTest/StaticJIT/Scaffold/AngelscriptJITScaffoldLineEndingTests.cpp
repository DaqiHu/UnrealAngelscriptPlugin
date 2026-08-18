#include "CQTest.h"

#include "StaticJIT/AngelscriptJITProjectScaffold.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptJITScaffoldLineEndingTests,
	"Angelscript.TestModule.StaticJIT.Scaffold.LineEndings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ManagedScaffoldWithCrLfCanBeRefreshed)
	{
		using namespace AngelscriptEditor::StaticJIT;
		const FString Root = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/StaticJITScaffoldLineEndings"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
		IFileManager::Get().MakeDirectory(*Root, true);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		};

		const FString ProjectFile = FPaths::Combine(
			Root, TEXT("ScaffoldLineEndings.uproject"));
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			TEXT("{\n  \"FileVersion\": 3,\n  \"Modules\": []\n}\n"),
			*ProjectFile,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)));
		const FAngelscriptJITProjectScaffoldResult Initial =
			FAngelscriptJITProjectScaffold::Scaffold(ProjectFile);
		ASSERT_THAT(IsTrue(Initial.bSuccess, Initial.Error));

		const FString BuildRules = FPaths::Combine(
			Initial.ModuleDirectory, TEXT("AngelscriptJIT.Build.cs"));
		FString Content;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			Content, *BuildRules)));
		Content.ReplaceInline(TEXT("\n"), TEXT("\r\n"));
		Content += TEXT("// stale managed content\r\n");
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			Content,
			*BuildRules,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)));

		const FAngelscriptJITProjectScaffoldResult Refreshed =
			FAngelscriptJITProjectScaffold::Scaffold(ProjectFile);
		ASSERT_THAT(IsTrue(Refreshed.bSuccess, Refreshed.Error));
		ASSERT_THAT(IsTrue(Refreshed.WrittenFiles.Contains(
			TEXT("AngelscriptJIT.Build.cs"))));
		ASSERT_THAT(IsTrue(
			FAngelscriptJITProjectScaffold::Inspect(ProjectFile).IsCurrent()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
