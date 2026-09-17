#include "CQTest.h"
#include "Framework/Catalog/AngelscriptTestCode.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

#include "Windows/WindowsHWrapper.h"

namespace
{
bool AdoptionBytesEqual(TConstArrayView<uint8> Actual, const ANSICHAR* Expected)
{
	const int32 ExpectedLength = FCStringAnsi::Strlen(Expected);
	return Actual.Num() == ExpectedLength
		&& (Actual.IsEmpty()
			|| FMemory::Memcmp(Actual.GetData(), Expected, ExpectedLength) == 0);
}

using FGetObservedCenter = const void* (*)();

FGetObservedCenter GetJitCenterProbe()
{
	const HMODULE JitModule = GetModuleHandleW(L"UnrealEditor-AngelscriptTestJIT.dll");
	return JitModule != nullptr
		? reinterpret_cast<FGetObservedCenter>(GetProcAddress(
			JitModule,
			"GetAngelscriptTestCodeCenterObservedByJIT"))
		: nullptr;
}
}

TEST_CLASS_WITH_FLAGS(
	Adoption,
	"Angelscript.UnitTest.Framework",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(TwoModulesOneCenter)
	{
		auto& Code = AngelscriptTest::FAngelscriptTestCode::GetInstance();
		ASSERT_THAT(IsTrue(Code.IsActivated()));
		ASSERT_THAT(IsTrue(Code.Get(TEXT("Language/Syntax/StructFields"), TEXT("fields-two")).IsSuccess()));
		ASSERT_THAT(IsTrue(Code.Get(TEXT("Fixture/Secondary"), TEXT("root")).IsSuccess()));
		ASSERT_THAT(IsFalse(Code.Get(TEXT("Language/Counter"), TEXT("root")).IsSuccess()));

		const FGetObservedCenter Probe = GetJitCenterProbe();
		ASSERT_THAT(IsNotNull(Probe));
		if (Probe != nullptr)
		{
			ASSERT_THAT(AreEqual(
				static_cast<const void*>(&Code),
				Probe()));
		}
	}

	TEST_METHOD(VersionReads)
	{
		auto& Code = AngelscriptTest::FAngelscriptTestCode::GetInstance();
		auto Changed = Code.Get(TEXT("Fixture/Secondary"), TEXT("changed"));
		auto Root = Code.Get(TEXT("Fixture/Secondary"), TEXT("root"));
		ASSERT_THAT(IsTrue(Changed.IsSuccess()));
		ASSERT_THAT(IsTrue(Root.IsSuccess()));
		if (Changed.IsSuccess() && Root.IsSuccess())
		{
			const auto ChangedSource = Changed.GetCase()->GetSource();
			const auto RootSource = Root.GetCase()->GetSource();
			ASSERT_THAT(IsTrue(AdoptionBytesEqual(
				RootSource.GetBytes(),
				"class Secondary { int Value = 1; }\n")));
			ASSERT_THAT(IsTrue(AdoptionBytesEqual(
				ChangedSource.GetBytes(),
				"class Secondary { int Value = 2; string Label = \"changed\"; }\n")));
			ASSERT_THAT(AreEqual(
				FString(TEXT("root")),
				Changed.GetCase()->GetVersionMeta().Parent.GetValue()));
		}

		auto StructFields = Code.Get(TEXT("Language/Syntax/StructFields"), TEXT("fields-two"));
		ASSERT_THAT(IsTrue(StructFields.IsSuccess()));
		ASSERT_THAT(IsFalse(Code.Get(TEXT("Language/Syntax/StructFields"), TEXT("root")).IsSuccess()));
		ASSERT_THAT(IsFalse(Code.Get(TEXT("Language/Counter"), TEXT("root")).IsSuccess()));
	}

	TEST_METHOD(AdoptionFieldsTwo)
	{
		auto& Code = AngelscriptTest::FAngelscriptTestCode::GetInstance();
		ASSERT_THAT(IsTrue(Code.Get(TEXT("Language/Syntax/StructFields"), TEXT("fields-two")).IsSuccess()));
		ASSERT_THAT(IsFalse(Code.Get(TEXT("Language/Syntax/StructFields"), TEXT("root")).IsSuccess()));
	}

	TEST_METHOD(ReplacementOnly)
	{
		ASSERT_THAT(IsTrue(WITH_ANGELSCRIPT_TESTS != 0));
		ASSERT_THAT(IsFalse(WITH_ANGELSCRIPT_UNITTESTS != 0));
		auto& Code = AngelscriptTest::FAngelscriptTestCode::GetInstance();
		ASSERT_THAT(IsTrue(Code.IsActivated()));
		ASSERT_THAT(AreEqual(0, Code.GetRegistrationErrors().Num()));
		ASSERT_THAT(IsTrue(Code.Get(TEXT("Language/Syntax/StructFields"), TEXT("fields-two")).IsSuccess()));
		ASSERT_THAT(IsTrue(Code.Get(TEXT("Fixture/Secondary"), TEXT("changed")).IsSuccess()));
		ASSERT_THAT(IsFalse(Code.Get(TEXT("Language/Counter"), TEXT("root")).IsSuccess()));
	}
};

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS
