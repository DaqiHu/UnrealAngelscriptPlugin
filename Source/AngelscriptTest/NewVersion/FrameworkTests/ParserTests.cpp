#include "CQTest.h"
#include "NewVersion/Framework/Source/AngelscriptTestSourceParseResult.h"
#include "NewVersion/Framework/Source/AngelscriptTestSourceParser.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest
{
struct FAngelscriptTestCodeBuildResultParserTestAccess
{
	static const TArray<FAngelscriptTestSourceCase>& GetCases(
		const FAngelscriptTestCodeBuildResult& Result)
	{
		return Result.GetCasesForAdmission();
	}
};
}

TEST_CLASS_WITH_FLAGS(
	Parser,
	"Angelscript.UnitTest.Framework",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static TConstArrayView<uint8> AsBytes(const ANSICHAR* Text)
	{
		return MakeArrayView(
			reinterpret_cast<const uint8*>(Text),
			FCStringAnsi::Strlen(Text));
	}

	static bool BytesEqual(TConstArrayView<uint8> Actual, TConstArrayView<uint8> Expected)
	{
		return Actual.Num() == Expected.Num()
			&& (Actual.IsEmpty() || FMemory::Memcmp(Actual.GetData(), Expected.GetData(), Actual.Num()) == 0);
	}

	static bool BytesEqual(TConstArrayView<uint8> Actual, const ANSICHAR* Expected)
	{
		return BytesEqual(Actual, AsBytes(Expected));
	}

	static const AngelscriptTest::FAngelscriptTestSourceCase* FindCase(
		const AngelscriptTest::FAngelscriptTestCodeBuildResult& Result,
		FStringView VersionTag)
	{
		return AngelscriptTest::FAngelscriptTestCodeBuildResultParserTestAccess::GetCases(Result)
			.FindByPredicate([VersionTag](const AngelscriptTest::FAngelscriptTestSourceCase& SourceCase)
			{
				return SourceCase.GetVersionMeta().Tag == VersionTag;
			});
	}

	static bool HasErrorCode(
		const TArray<AngelscriptTest::FAngelscriptTestError>& Errors,
		FStringView Code)
	{
		return Errors.ContainsByPredicate([Code](const AngelscriptTest::FAngelscriptTestError& Error)
		{
			return Error.Code == Code;
		});
	}

public:
	TEST_METHOD(FullContainer)
	{
		const ANSICHAR* Container =
			"/**\n"
			" * @version v1\n"
			" * @summary Counter source variants.\n"
			" * @topic Language\n"
			" */\n"
			"/**\n"
			" * @version add-step\n"
			" * @parent root\n"
			" * @summary Add a step.\n"
			" * @topic Fields\n"
			" */\n"
			"class Counter { int Step = 1; }\n"
			"/** @end */\n"
			"/**\n"
			" * @version root\n"
			" * @summary Define the initial counter.\n"
			" */\n"
			"class Counter { int Value = 0; }\n"
			"/** @end */\n";

		auto Result = AngelscriptTest::FAngelscriptTestSourceParser::Parse(
			TEXT("Language/Counter"), AsBytes(Container));
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		if (!Result.IsSuccess())
		{
			return;
		}

		const auto& Cases = AngelscriptTest::FAngelscriptTestCodeBuildResultParserTestAccess::GetCases(Result);
		ASSERT_THAT(AreEqual(2, Cases.Num()));
		ASSERT_THAT(IsNull(FindCase(Result, TEXT("v1"))));
		const auto* Root = FindCase(Result, TEXT("root"));
		const auto* Child = FindCase(Result, TEXT("add-step"));
		ASSERT_THAT(IsNotNull(Root));
		ASSERT_THAT(IsNotNull(Child));
		if (Root != nullptr && Child != nullptr)
		{
			ASSERT_THAT(AreEqual(FString(TEXT("v1")), Root->GetFileMeta().Version));
			ASSERT_THAT(AreEqual(FString(TEXT("Language/Counter")), Root->GetFileMeta().Tag));
			ASSERT_THAT(AreEqual(FString(TEXT("Counter source variants.")), Root->GetFileMeta().Summary));
			ASSERT_THAT(AreEqual(FString(TEXT("Define the initial counter.")), Root->GetVersionMeta().Summary));
			ASSERT_THAT(AreEqual(FString(TEXT("root")), Child->GetVersionMeta().Parent.GetValue()));
			const auto RootSource = Root->GetSource();
			const auto ChildSource = Child->GetSource();
			ASSERT_THAT(IsTrue(BytesEqual(RootSource.GetBytes(), "class Counter { int Value = 0; }\n")));
			ASSERT_THAT(IsTrue(BytesEqual(ChildSource.GetBytes(), "class Counter { int Step = 1; }\n")));
		}
	}

	TEST_METHOD(BytePositions)
	{
		const auto PointInput = AngelscriptTest::FAngelscriptTestSource::FromBytes(
			AsBytes("\xC3\xA9" "/** @point p */x"));
		auto PointResult = AngelscriptTest::FAngelscriptTestAnnotations::Parse(PointInput);
		ASSERT_THAT(IsTrue(PointResult.IsSuccess()));
		if (PointResult.IsSuccess())
		{
			const auto* Point = PointResult.GetSource()->GetAnnotations().GetPoint(TEXT("p"));
			ASSERT_THAT(IsNotNull(Point));
			ASSERT_THAT(AreEqual(2, Point != nullptr ? *Point : INDEX_NONE));
			ASSERT_THAT(IsTrue(BytesEqual(PointResult.GetSource()->GetBytes(), "\xC3\xA9" "x")));
			ASSERT_THAT(IsTrue(PointResult.GetSource()->GetOriginMap().MapToOriginalByteOffset(2) > 2));
		}

		const auto RangeInput = AngelscriptTest::FAngelscriptTestSource::FromBytes(
			AsBytes("a/** @range-begin r */bc/** @range-end r */d"));
		auto RangeResult = AngelscriptTest::FAngelscriptTestAnnotations::Parse(RangeInput);
		ASSERT_THAT(IsTrue(RangeResult.IsSuccess()));
		if (RangeResult.IsSuccess())
		{
			const auto* Range = RangeResult.GetSource()->GetAnnotations().GetRange(TEXT("r"));
			ASSERT_THAT(IsNotNull(Range));
			ASSERT_THAT(AreEqual(1, Range != nullptr ? Range->Begin : INDEX_NONE));
			ASSERT_THAT(AreEqual(3, Range != nullptr ? Range->End : INDEX_NONE));
			ASSERT_THAT(IsTrue(BytesEqual(RangeResult.GetSource()->GetBytes(), "abcd")));
		}

		auto BreakpointResult = AngelscriptTest::FAngelscriptTestAnnotations::Parse(
			AngelscriptTest::FAngelscriptTestSource::FromBytes(AsBytes("/** @breakpoint stop */x")));
		ASSERT_THAT(IsTrue(BreakpointResult.IsSuccess()));
		if (BreakpointResult.IsSuccess())
		{
			const auto* Breakpoint = BreakpointResult.GetSource()->GetAnnotations().GetBreakpoint(TEXT("stop"));
			ASSERT_THAT(IsNotNull(Breakpoint));
			ASSERT_THAT(AreEqual(0, Breakpoint != nullptr ? *Breakpoint : INDEX_NONE));
		}
	}

	TEST_METHOD(EscapeAndMalformedMarkers)
	{
		auto Escaped = AngelscriptTest::FAngelscriptTestAnnotations::Parse(
			AngelscriptTest::FAngelscriptTestSource::FromBytes(AsBytes("/** @@point p */")));
		ASSERT_THAT(IsTrue(Escaped.IsSuccess()));
		if (Escaped.IsSuccess())
		{
			ASSERT_THAT(IsTrue(BytesEqual(Escaped.GetSource()->GetBytes(), "/** @point p */")));
			ASSERT_THAT(AreEqual(0, Escaped.GetSource()->GetAnnotations().Num()));
		}

		struct FBadMarker
		{
			const ANSICHAR* Text;
			const TCHAR* ErrorCode;
		};
		for (const FBadMarker& Bad : {
			FBadMarker{"/** @point p *//** @point p */", TEXT("DuplicatePoint")},
			FBadMarker{"/** @range-begin r */", TEXT("MissingRangeEnd")},
			FBadMarker{"/** @range-begin a *//** @range-begin b *//** @range-end a *//** @range-end b */", TEXT("CrossingRange")},
			FBadMarker{"/** @cursor p */", TEXT("UnknownAnnotationDirective")}})
		{
			auto Result = AngelscriptTest::FAngelscriptTestAnnotations::Parse(
				AngelscriptTest::FAngelscriptTestSource::FromBytes(AsBytes(Bad.Text)));
			ASSERT_THAT(IsFalse(Result.IsSuccess()));
			ASSERT_THAT(IsNull(Result.GetSource()));
			ASSERT_THAT(IsTrue(HasErrorCode(Result.GetErrors(), Bad.ErrorCode)));
		}
	}

	TEST_METHOD(EmptyAndLineEndings)
	{
		const ANSICHAR* EmptyContainer =
			"/**\n * @version v1\n * @summary Empty source.\n */\n"
			"/**\n * @version root\n * @summary Empty root.\n */\n"
			"/** @end */\n";
		auto Empty = AngelscriptTest::FAngelscriptTestSourceParser::Parse(
			TEXT("Language/Empty"), AsBytes(EmptyContainer));
		ASSERT_THAT(IsTrue(Empty.IsSuccess()));
		if (Empty.IsSuccess())
		{
			const auto* Root = FindCase(Empty, TEXT("root"));
			ASSERT_THAT(IsNotNull(Root));
			if (Root != nullptr)
			{
				const auto Source = Root->GetSource();
				ASSERT_THAT(AreEqual(0, Source.GetBytes().Num()));
			}
		}

		TArray<uint8> CrLf = {0xef, 0xbb, 0xbf};
		const ANSICHAR* CrLfText =
			"/**\r\n * @version v1\r\n * @summary CRLF source.\r\n */\r\n"
			"/**\r\n * @version root\r\n * @summary Root.\r\n */\r\n"
			"a\r\nb\r\n/** @end */\r\n";
		CrLf.Append(AsBytes(CrLfText).GetData(), AsBytes(CrLfText).Num());
		auto Normalized = AngelscriptTest::FAngelscriptTestSourceParser::Parse(
			TEXT("Language/CrLf"), CrLf);
		ASSERT_THAT(IsTrue(Normalized.IsSuccess()));
		if (Normalized.IsSuccess())
		{
			const auto Source = FindCase(Normalized, TEXT("root"))->GetSource();
			ASSERT_THAT(IsTrue(BytesEqual(Source.GetBytes(), "a\nb\n")));
			ASSERT_THAT(AreEqual(FString(TEXT("Language/CrLf.as")), FString(Source.GetOriginMap().GetHostFile())));
			ASSERT_THAT(AreEqual(9, Source.GetOriginMap().GetHostLine()));
			ASSERT_THAT(AreEqual(2,
				Source.GetOriginMap().MapToOriginalByteOffset(2)
				- Source.GetOriginMap().MapToOriginalByteOffset(1)));
		}

		TArray<uint8> Invalid = { '/', '*', '*', '\n', ' ', '*', ' ', '@', 'v', 'e', 'r', 's', 'i', 'o', 'n', ' ', 'v', '1', '\n',
			' ', '*', ' ', '@', 's', 'u', 'm', 'm', 'a', 'r', 'y', ' ', 0xff, '\n', ' ', '*', '/', '\n' };
		auto InvalidResult = AngelscriptTest::FAngelscriptTestSourceParser::Parse(
			TEXT("Language/Invalid"), Invalid);
		ASSERT_THAT(IsFalse(InvalidResult.IsSuccess()));
		ASSERT_THAT(IsTrue(HasErrorCode(InvalidResult.GetErrors(), TEXT("InvalidUtf8"))));
	}

	TEST_METHOD(InlineParity)
	{
		const int32 InlineLine = __builtin_LINE() + 1;
		auto Inline = AngelscriptTest::FAngelscriptTestAnnotations::Parse(AS_TEST_SOURCE_EXACT(
			"\xC3\xA9" "/** @point p */x\n"));
		const ANSICHAR* Container =
			"/**\n * @version v1\n * @summary Parity.\n */\n"
			"/**\n * @version root\n * @summary Root.\n */\n"
			"\xC3\xA9" "/** @point p */x\n"
			"/** @end */\n";
		auto Parsed = AngelscriptTest::FAngelscriptTestSourceParser::Parse(
			TEXT("Language/Parity"), AsBytes(Container));

		ASSERT_THAT(IsTrue(Inline.IsSuccess()));
		ASSERT_THAT(IsTrue(Parsed.IsSuccess()));
		if (Inline.IsSuccess() && Parsed.IsSuccess())
		{
			const auto ParsedSource = FindCase(Parsed, TEXT("root"))->GetSource();
			ASSERT_THAT(IsTrue(BytesEqual(Inline.GetSource()->GetBytes(), ParsedSource.GetBytes())));
			ASSERT_THAT(AreEqual(
				*Inline.GetSource()->GetAnnotations().GetPoint(TEXT("p")),
				*ParsedSource.GetAnnotations().GetPoint(TEXT("p"))));
			ASSERT_THAT(AreEqual(InlineLine, Inline.GetSource()->GetOriginMap().GetHostLine()));
			ASSERT_THAT(IsFalse(Inline.GetSource()->GetOriginMap().GetHostFile()
				== ParsedSource.GetOriginMap().GetHostFile()));
		}
	}
};

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
