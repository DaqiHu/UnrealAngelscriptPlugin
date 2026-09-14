#include "CQTest.h"
#include "NewVersion/Framework/Catalog/AngelscriptTestCode.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace
{
bool GeneratedBytesEqual(TConstArrayView<uint8> Actual, const ANSICHAR* Expected)
{
	const int32 ExpectedLength = FCStringAnsi::Strlen(Expected);
	return Actual.Num() == ExpectedLength
		&& (Actual.IsEmpty()
			|| FMemory::Memcmp(Actual.GetData(), Expected, ExpectedLength) == 0);
}

constexpr ANSICHAR GeneratedCounterRoot[] =
	"class Counter\n"
	"{\n"
	"    int Value = 0;\n"
	"}\n";

constexpr ANSICHAR GeneratedCounterAddStep[] =
	"class Counter\n"
	"{\n"
	"    int Value = 0;\n"
	"    int Step = 1;\n"
	"}\n";
}

TEST_CLASS_WITH_FLAGS(
	GeneratedSources,
	"Angelscript.UnitTest.Framework",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(GeneratedFixture)
	{
		auto& Code = AngelscriptTest::FAngelscriptTestCode::GetInstance();
		ASSERT_THAT(IsTrue(Code.IsActivated()));
		ASSERT_THAT(AreEqual(0, Code.GetRegistrationErrors().Num()));

		auto Root = Code.Get(TEXT("Language/Counter"), TEXT("root"));
		auto AddStep = Code.Get(TEXT("Language/Counter"), TEXT("add-step"));
		ASSERT_THAT(IsTrue(Root.IsSuccess()));
		ASSERT_THAT(IsTrue(AddStep.IsSuccess()));
		if (!Root.IsSuccess() || !AddStep.IsSuccess())
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("v1")), Root.GetCase()->GetFileMeta().Version));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Language/Counter")),
			Root.GetCase()->GetFileMeta().Tag));
		const auto RootSource = Root.GetCase()->GetSource();
		const auto AddStepSource = AddStep.GetCase()->GetSource();
		ASSERT_THAT(IsTrue(GeneratedBytesEqual(
			RootSource.GetBytes(), GeneratedCounterRoot)));
		ASSERT_THAT(IsTrue(GeneratedBytesEqual(
			AddStepSource.GetBytes(), GeneratedCounterAddStep)));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Language/Counter.as")),
			FString(RootSource.GetOriginMap().GetHostFile())));
	}
};

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
