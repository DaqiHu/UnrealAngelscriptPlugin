#include "CQTest.h"
#include "Framework/Catalog/AngelscriptTestCode.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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

// Independently authored clean literals for Language/Syntax/StructFields.
constexpr ANSICHAR GeneratedStructFieldsRoot[] =
	"struct FStructFields\n"
	"{\n"
	"\tint X = 0;\n"
	"\tfloat Y = 0.0f;\n"
	"}\n";

constexpr ANSICHAR GeneratedStructFieldsAddField[] =
	"struct FStructFields\n"
	"{\n"
	"\tint X = 0;\n"
	"\tfloat Y = 0.0f;\n"
	"\tint Z = 1;\n"
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

		auto FieldsTwo = Code.Get(TEXT("Language/Syntax/StructFields"), TEXT("fields-two"));
		auto AddField = Code.Get(TEXT("Language/Syntax/StructFields"), TEXT("add-field"));
		ASSERT_THAT(IsTrue(FieldsTwo.IsSuccess()));
		ASSERT_THAT(IsTrue(AddField.IsSuccess()));
		if (!FieldsTwo.IsSuccess() || !AddField.IsSuccess())
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("v1")), FieldsTwo.GetCase()->GetFileMeta().Version));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Language/Syntax/StructFields")),
			FieldsTwo.GetCase()->GetFileMeta().Tag));
		const auto FieldsTwoSource = FieldsTwo.GetCase()->GetSource();
		const auto AddFieldSource = AddField.GetCase()->GetSource();
		ASSERT_THAT(IsTrue(GeneratedBytesEqual(
			FieldsTwoSource.GetBytes(), GeneratedStructFieldsRoot)));
		ASSERT_THAT(IsTrue(GeneratedBytesEqual(
			AddFieldSource.GetBytes(), GeneratedStructFieldsAddField)));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Language/Syntax/StructFields.as")),
			FString(FieldsTwoSource.GetOriginMap().GetHostFile())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Struct field declaration, in-class initializers, and annotated insertion points.")),
			FieldsTwo.GetCase()->GetFileMeta().Summary));
		ASSERT_THAT(AreEqual(2, FieldsTwo.GetCase()->GetFileMeta().Topics.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("Language")), FieldsTwo.GetCase()->GetFileMeta().Topics[0]));
		ASSERT_THAT(AreEqual(FString(TEXT("Syntax")), FieldsTwo.GetCase()->GetFileMeta().Topics[1]));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Two-field struct with integer and float in-class initializers.")),
			FieldsTwo.GetCase()->GetVersionMeta().Summary));
		ASSERT_THAT(AreEqual(1, FieldsTwo.GetCase()->GetVersionMeta().Topics.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("Syntax")), FieldsTwo.GetCase()->GetVersionMeta().Topics[0]));
		const int32* InitialValue = FieldsTwoSource.GetAnnotations().GetPoint(TEXT("initial-value"));
		ASSERT_THAT(IsNotNull(InitialValue));
		ASSERT_THAT(AreEqual(32, InitialValue != nullptr ? *InitialValue : INDEX_NONE));
		ASSERT_THAT(AreEqual(460, FieldsTwoSource.GetOriginMap().MapToOriginalByteOffset(32)));
	}

	TEST_METHOD(GeneratedChildRemainsDirectlyAddressable)
	{
		auto& Code = AngelscriptTest::FAngelscriptTestCode::GetInstance();
		auto AddFieldFirst = Code.Get(TEXT("Language/Syntax/StructFields"), TEXT("add-field"));
		auto FieldsTwo = Code.Get(TEXT("Language/Syntax/StructFields"), TEXT("fields-two"));
		auto AddFieldAgain = Code.Get(TEXT("Language/Syntax/StructFields"), TEXT("add-field"));
		ASSERT_THAT(IsTrue(AddFieldFirst.IsSuccess()));
		ASSERT_THAT(IsTrue(FieldsTwo.IsSuccess()));
		ASSERT_THAT(IsTrue(AddFieldAgain.IsSuccess()));
		if (!AddFieldFirst.IsSuccess() || !AddFieldAgain.IsSuccess())
		{
			return;
		}

		const auto FirstSource = AddFieldFirst.GetCase()->GetSource();
		const auto AgainSource = AddFieldAgain.GetCase()->GetSource();
		ASSERT_THAT(IsTrue(GeneratedBytesEqual(FirstSource.GetBytes(), GeneratedStructFieldsAddField)));
		ASSERT_THAT(IsTrue(GeneratedBytesEqual(AgainSource.GetBytes(), GeneratedStructFieldsAddField)));
		ASSERT_THAT(AreEqual(FString(TEXT("fields-two")), AddFieldFirst.GetCase()->GetVersionMeta().Parent.GetValue()));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Insert a third annotated integer field after the first two members.")),
			AddFieldFirst.GetCase()->GetVersionMeta().Summary));
		ASSERT_THAT(AreEqual(1, AddFieldFirst.GetCase()->GetVersionMeta().Topics.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("Syntax")), AddFieldFirst.GetCase()->GetVersionMeta().Topics[0]));
		const int32* BeforeAdd = FirstSource.GetAnnotations().GetBreakpoint(TEXT("before-add"));
		const auto* Delta = FirstSource.GetAnnotations().GetRange(TEXT("delta"));
		ASSERT_THAT(IsNotNull(BeforeAdd));
		ASSERT_THAT(IsNotNull(Delta));
		ASSERT_THAT(AreEqual(53, BeforeAdd != nullptr ? *BeforeAdd : INDEX_NONE));
		ASSERT_THAT(AreEqual(61, Delta != nullptr ? Delta->Begin : INDEX_NONE));
		ASSERT_THAT(AreEqual(62, Delta != nullptr ? Delta->End : INDEX_NONE));
		ASSERT_THAT(AreEqual(723, FirstSource.GetOriginMap().MapToOriginalByteOffset(53)));
		ASSERT_THAT(AreEqual(756, FirstSource.GetOriginMap().MapToOriginalByteOffset(61)));
		for (int32 Offset = 0; Offset <= FirstSource.GetBytes().Num(); ++Offset)
		{
			ASSERT_THAT(AreEqual(
				FirstSource.GetOriginMap().MapToOriginalByteOffset(Offset),
				AgainSource.GetOriginMap().MapToOriginalByteOffset(Offset)));
		}
	}

	TEST_METHOD(RegistrationAndCaseMetadataHaveOneTruth)
	{
		auto& Code = AngelscriptTest::FAngelscriptTestCode::GetInstance();
		auto FieldsTwo = Code.Get(TEXT("Language/Syntax/StructFields"), TEXT("fields-two"));
		auto AddField = Code.Get(TEXT("Language/Syntax/StructFields"), TEXT("add-field"));
		ASSERT_THAT(IsTrue(FieldsTwo.IsSuccess()));
		ASSERT_THAT(IsTrue(AddField.IsSuccess()));
		if (!FieldsTwo.IsSuccess() || !AddField.IsSuccess())
		{
			return;
		}

		const auto& FieldsTwoMeta = FieldsTwo.GetCase()->GetFileMeta();
		const auto& ChildMeta = AddField.GetCase()->GetFileMeta();
		ASSERT_THAT(AreEqual(FieldsTwoMeta.Version, ChildMeta.Version));
		ASSERT_THAT(AreEqual(FieldsTwoMeta.Tag, ChildMeta.Tag));
		ASSERT_THAT(AreEqual(FieldsTwoMeta.Summary, ChildMeta.Summary));
		ASSERT_THAT(AreEqual(FieldsTwoMeta.Topics.Num(), ChildMeta.Topics.Num()));
		ASSERT_THAT(AreEqual(FString(TEXT("v1")), FieldsTwoMeta.Version));
		ASSERT_THAT(AreEqual(FString(TEXT("Language/Syntax/StructFields")), FieldsTwoMeta.Tag));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Struct field declaration, in-class initializers, and annotated insertion points.")),
			FieldsTwoMeta.Summary));
		ASSERT_THAT(IsFalse(FieldsTwoMeta.Summary.Contains(TEXT("Generated from"))));
		ASSERT_THAT(IsTrue(&FieldsTwoMeta == &ChildMeta));
	}

	TEST_METHOD(GeneratedActivationHasNoParserDependency)
	{
		FString GeneratedPath = FPaths::ConvertRelativePathToFull(
			FPaths::ProjectDir()
			/ TEXT("Plugins/Angelscript/Source/AngelscriptTest/TestCode/Generated/Language/Syntax/StructFields.generated.cpp"));
		FString Text;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(Text, *GeneratedPath)));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("format=v2"))));
		ASSERT_THAT(IsTrue(Text.Contains(TEXT("GRegistration_Language_Syntax_StructFields"))));
		ASSERT_THAT(IsFalse(Text.Contains(TEXT("AngelscriptTestSourceParser.h"))));
		ASSERT_THAT(IsFalse(Text.Contains(TEXT("FAngelscriptTestSourceParser::Parse"))));
		ASSERT_THAT(IsFalse(Text.Contains(TEXT("constexpr uint8 GSource"))));

		auto& Code = AngelscriptTest::FAngelscriptTestCode::GetInstance();
		ASSERT_THAT(IsTrue(Code.Get(TEXT("Language/Syntax/StructFields"), TEXT("fields-two")).IsSuccess()));
		ASSERT_THAT(IsFalse(Code.Get(TEXT("Language/Counter"), TEXT("root")).IsSuccess()));
	}
};

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
