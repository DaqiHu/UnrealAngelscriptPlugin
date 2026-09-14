#include "CQTest.h"
#include "NewVersion/Framework/Source/AngelscriptTestSource.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	Source,
	"Angelscript.UnitTest.Framework",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool BytesEqual(TConstArrayView<uint8> Actual, TConstArrayView<uint8> Expected)
	{
		return Actual.Num() == Expected.Num()
			&& (Actual.IsEmpty() || FMemory::Memcmp(Actual.GetData(), Expected.GetData(), Actual.Num()) == 0);
	}

public:
	TEST_METHOD(ExactBytesOutliveInput)
	{
		TArray<uint8> Temporary = {0x61, 0x00, 0xff};
		AngelscriptTest::FAngelscriptTestSource Source =
			AngelscriptTest::FAngelscriptTestSource::FromBytes(Temporary);
		Temporary.Reset();

		const uint8 Expected[] = {0x61, 0x00, 0xff};
		ASSERT_THAT(IsTrue(BytesEqual(Source.GetBytes(), MakeArrayView(Expected))));
		ASSERT_THAT(AreEqual(0, Source.GetOriginMap().MapToOriginalByteOffset(0)));
		ASSERT_THAT(AreEqual(3, Source.GetOriginMap().MapToOriginalByteOffset(3)));

		const auto ExactLiteral = AS_TEST_SOURCE_EXACT("\n x\n");
		const ANSICHAR ExpectedLiteral[] = "\n x\n";
		ASSERT_THAT(IsTrue(BytesEqual(
			ExactLiteral.GetBytes(),
			MakeArrayView(
				reinterpret_cast<const uint8*>(ExpectedLiteral),
				static_cast<int32>(sizeof(ExpectedLiteral) - 1)))));
		ASSERT_THAT(AreEqual(0, ExactLiteral.GetOriginMap().MapToOriginalByteOffset(0)));
		ASSERT_THAT(AreEqual(4, ExactLiteral.GetOriginMap().MapToOriginalByteOffset(4)));
	}

	TEST_METHOD(EnvelopeAndIndent)
	{
		const int32 ExpectedHostLine = __builtin_LINE() + 1;
		const auto Source = AS_TEST_SOURCE("\n    alpha\n\n    beta\n");
		const ANSICHAR Expected[] = "alpha\n\nbeta";

		ASSERT_THAT(IsTrue(BytesEqual(
			Source.GetBytes(),
			MakeArrayView(reinterpret_cast<const uint8*>(Expected), static_cast<int32>(sizeof(Expected) - 1)))));
		ASSERT_THAT(AreEqual(5, Source.GetOriginMap().MapToOriginalByteOffset(0)));
		ASSERT_THAT(AreEqual(10, Source.GetOriginMap().MapToOriginalByteOffset(5)));
		ASSERT_THAT(AreEqual(11, Source.GetOriginMap().MapToOriginalByteOffset(6)));
		ASSERT_THAT(AreEqual(16, Source.GetOriginMap().MapToOriginalByteOffset(7)));
		ASSERT_THAT(AreEqual(20, Source.GetOriginMap().MapToOriginalByteOffset(11)));
		ASSERT_THAT(IsFalse(Source.GetOriginMap().GetHostFile().IsEmpty()));
		ASSERT_THAT(AreEqual(ExpectedHostLine, Source.GetOriginMap().GetHostLine()));
	}

	TEST_METHOD(NoImplicitRegistration)
	{
		const auto Source = AS_TEST_SOURCE("\n");

		ASSERT_THAT(AreEqual(0, Source.GetBytes().Num()));
		ASSERT_THAT(AreEqual(0, Source.GetAnnotations().Num()));
		ASSERT_THAT(IsFalse(Source.GetOriginMap().GetHostFile().IsEmpty()));
		ASSERT_THAT(IsTrue(Source.GetOriginMap().GetHostLine() > 0));
	}

	TEST_METHOD(MixedIndentUsesExactCommonPrefix)
	{
		const auto Source = AS_TEST_SOURCE("\n \tfirst\n \tsecond\n \t\tthird\n");
		const ANSICHAR Expected[] = "first\nsecond\n\tthird";

		ASSERT_THAT(IsTrue(BytesEqual(
			Source.GetBytes(),
			MakeArrayView(reinterpret_cast<const uint8*>(Expected), static_cast<int32>(sizeof(Expected) - 1)))));
	}
};

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
