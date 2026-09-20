#include "CQTest.h"
#include "Framework/Catalog/AngelscriptTestCode.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	UnrealFixtureCorpus,
	"Angelscript.UnitTest.Framework",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(CorpusHasCastingAndWorldActor)
	{
		auto& Code = AngelscriptTest::FAngelscriptTestCode::GetInstance();
		TArray<AngelscriptTest::FAngelscriptTestFileMeta> Files;
		const TArray<FString> UnrealTopic = {FString(TEXT("Unreal"))};
		const auto FileStatus = Code.FindFiles(UnrealTopic, Files);
		ASSERT_THAT(IsTrue(FileStatus.IsSuccess()));
		if (!FileStatus.IsSuccess())
		{
			return;
		}

		const bool bHasCasting = Files.ContainsByPredicate(
			[](const AngelscriptTest::FAngelscriptTestFileMeta& File)
			{
				return File.Tag == TEXT("Unreal/Casting");
			});
		const bool bHasWorldActor = Files.ContainsByPredicate(
			[](const AngelscriptTest::FAngelscriptTestFileMeta& File)
			{
				return File.Tag == TEXT("Unreal/World/Actor");
			});
		const bool bHasLanguageCast = Files.ContainsByPredicate(
			[](const AngelscriptTest::FAngelscriptTestFileMeta& File)
			{
				return File.Tag == TEXT("Language/Casting/ClassHandleCast");
			});
		ASSERT_THAT(IsTrue(bHasCasting));
		ASSERT_THAT(IsTrue(bHasWorldActor));
		ASSERT_THAT(IsFalse(bHasLanguageCast));
	}

	TEST_METHOD(LanguageTopicOmitsUnreal)
	{
		auto& Code = AngelscriptTest::FAngelscriptTestCode::GetInstance();
		TArray<AngelscriptTest::FAngelscriptTestFileMeta> Files;
		const TArray<FString> LanguageTopic = {FString(TEXT("Language"))};
		const auto FileStatus = Code.FindFiles(LanguageTopic, Files);
		ASSERT_THAT(IsTrue(FileStatus.IsSuccess()));
		if (!FileStatus.IsSuccess())
		{
			return;
		}

		const bool bHasUnrealTag = Files.ContainsByPredicate(
			[](const AngelscriptTest::FAngelscriptTestFileMeta& File)
			{
				return File.Tag.StartsWith(TEXT("Unreal/"));
			});
		ASSERT_THAT(IsFalse(bHasUnrealTag));
	}
};

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
