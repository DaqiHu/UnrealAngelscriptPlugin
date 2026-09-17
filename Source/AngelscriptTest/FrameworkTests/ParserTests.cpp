#include "CQTest.h"
#include "Framework/Source/AngelscriptTestSource.h"
#include "Framework/Source/AngelscriptTestSourceParseResult.h"
#include "Framework/Source/AngelscriptTestSourceParser.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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

	static const AngelscriptTest::FAngelscriptTestError* FindError(
		const TArray<AngelscriptTest::FAngelscriptTestError>& Errors,
		FStringView Code)
	{
		return Errors.FindByPredicate([Code](const AngelscriptTest::FAngelscriptTestError& Error)
		{
			return Error.Code == Code;
		});
	}

	static FString ConformancePath(const TCHAR* FileName)
	{
		return FPaths::ConvertRelativePathToFull(
			FPaths::ProjectDir()
			/ TEXT("AngelscriptTestCode/CodeGenTool/tests/fixtures/conformance")
			/ FileName);
	}

	static bool LoadConformanceBytes(const TCHAR* FileName, TArray<uint8>& OutBytes)
	{
		if (!FFileHelper::LoadFileToArray(OutBytes, *ConformancePath(FileName)))
		{
			return false;
		}
		if (OutBytes.Num() > 0 && OutBytes.Last() == 0)
		{
			OutBytes.Pop(EAllowShrinking::No);
		}
		return true;
	}

	static bool OriginMapEquals(
		const AngelscriptTest::FAngelscriptTestSource& Source,
		TConstArrayView<int32> Expected)
	{
		if (Source.GetBytes().Num() + 1 != Expected.Num())
		{
			return false;
		}
		for (int32 Offset = 0; Offset < Expected.Num(); ++Offset)
		{
			if (Source.GetOriginMap().MapToOriginalByteOffset(Offset) != Expected[Offset])
			{
				return false;
			}
		}
		return true;
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

	TEST_METHOD(ConformancePositive)
	{
		TArray<uint8> BomCrLf;
		ASSERT_THAT(IsTrue(LoadConformanceBytes(TEXT("bom-crlf.as"), BomCrLf)));
		auto BomResult = AngelscriptTest::FAngelscriptTestSourceParser::Parse(
			TEXT("conformance/bom-crlf"), BomCrLf);
		ASSERT_THAT(IsTrue(BomResult.IsSuccess()));
		const auto* BomRoot = FindCase(BomResult, TEXT("root"));
		ASSERT_THAT(IsNotNull(BomRoot));
		if (BomRoot != nullptr)
		{
			ASSERT_THAT(AreEqual(FString(TEXT("v1")), BomRoot->GetFileMeta().Version));
			ASSERT_THAT(AreEqual(FString(TEXT("conformance/bom-crlf")), BomRoot->GetFileMeta().Tag));
			ASSERT_THAT(AreEqual(FString(TEXT("BOM and CRLF container.")), BomRoot->GetFileMeta().Summary));
			ASSERT_THAT(AreEqual(FString(TEXT("Root body.")), BomRoot->GetVersionMeta().Summary));
			const auto Source = BomRoot->GetSource();
			ASSERT_THAT(IsTrue(BytesEqual(Source.GetBytes(), "int X = 1;\n")));
			ASSERT_THAT(AreEqual(9, Source.GetOriginMap().GetHostLine()));
			constexpr int32 BomMap[] = {118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 130};
			ASSERT_THAT(IsTrue(OriginMapEquals(Source, BomMap)));
		}

		TArray<uint8> Unicode;
		ASSERT_THAT(IsTrue(LoadConformanceBytes(TEXT("unicode.as"), Unicode)));
		auto UnicodeResult = AngelscriptTest::FAngelscriptTestSourceParser::Parse(
			TEXT("conformance/unicode"), Unicode);
		ASSERT_THAT(IsTrue(UnicodeResult.IsSuccess()));
		const auto* UnicodeRoot = FindCase(UnicodeResult, TEXT("root"));
		ASSERT_THAT(IsNotNull(UnicodeRoot));
		if (UnicodeRoot != nullptr)
		{
			const auto Source = UnicodeRoot->GetSource();
			const TConstArrayView<uint8> Bytes = Source.GetBytes();
			ASSERT_THAT(AreEqual(19, Bytes.Num()));
			ASSERT_THAT(AreEqual(0xce, static_cast<int32>(Bytes[12])));
			ASSERT_THAT(AreEqual(0xb1, static_cast<int32>(Bytes[13])));
			ASSERT_THAT(AreEqual(0xce, static_cast<int32>(Bytes[14])));
			ASSERT_THAT(AreEqual(0xb2, static_cast<int32>(Bytes[15])));
			const int32* AfterAlpha = Source.GetAnnotations().GetPoint(TEXT("after-alpha"));
			ASSERT_THAT(IsNotNull(AfterAlpha));
			ASSERT_THAT(AreEqual(14, AfterAlpha != nullptr ? *AfterAlpha : INDEX_NONE));
			ASSERT_THAT(AreEqual(109, Source.GetOriginMap().MapToOriginalByteOffset(12)));
			ASSERT_THAT(AreEqual(110, Source.GetOriginMap().MapToOriginalByteOffset(13)));
			ASSERT_THAT(AreEqual(136, Source.GetOriginMap().MapToOriginalByteOffset(14)));
			ASSERT_THAT(AreEqual(141, Source.GetOriginMap().MapToOriginalByteOffset(Bytes.Num())));
		}

		TArray<uint8> Annotations;
		ASSERT_THAT(IsTrue(LoadConformanceBytes(TEXT("annotations.as"), Annotations)));
		auto AnnotationResult = AngelscriptTest::FAngelscriptTestSourceParser::Parse(
			TEXT("conformance/annotations"), Annotations);
		ASSERT_THAT(IsTrue(AnnotationResult.IsSuccess()));
		const auto* AnnotationRoot = FindCase(AnnotationResult, TEXT("root"));
		ASSERT_THAT(IsNotNull(AnnotationRoot));
		if (AnnotationRoot != nullptr)
		{
			const auto Source = AnnotationRoot->GetSource();
			ASSERT_THAT(IsTrue(BytesEqual(Source.GetBytes(), "abxy/** @point literal */\n")));
			const int32* SitePoint = Source.GetAnnotations().GetPoint(TEXT("site"));
			const int32* SiteBreakpoint = Source.GetAnnotations().GetBreakpoint(TEXT("site"));
			ASSERT_THAT(IsNotNull(SitePoint));
			ASSERT_THAT(IsNotNull(SiteBreakpoint));
			ASSERT_THAT(AreEqual(1, SitePoint != nullptr ? *SitePoint : INDEX_NONE));
			ASSERT_THAT(AreEqual(2, SiteBreakpoint != nullptr ? *SiteBreakpoint : INDEX_NONE));
			const auto* Inner = Source.GetAnnotations().GetRange(TEXT("inner"));
			const auto* Outer = Source.GetAnnotations().GetRange(TEXT("outer"));
			ASSERT_THAT(IsNotNull(Inner));
			ASSERT_THAT(IsNotNull(Outer));
			if (Inner != nullptr && Outer != nullptr)
			{
				ASSERT_THAT(AreEqual(2, Inner->Begin));
				ASSERT_THAT(AreEqual(3, Inner->End));
				ASSERT_THAT(AreEqual(2, Outer->Begin));
				ASSERT_THAT(AreEqual(4, Outer->End));
			}
			ASSERT_THAT(AreEqual(97, Source.GetOriginMap().MapToOriginalByteOffset(0)));
			ASSERT_THAT(AreEqual(261, Source.GetOriginMap().MapToOriginalByteOffset(Source.GetBytes().Num())));
		}

		TArray<uint8> Branch;
		ASSERT_THAT(IsTrue(LoadConformanceBytes(TEXT("branch-trailing-lf.as"), Branch)));
		auto BranchResult = AngelscriptTest::FAngelscriptTestSourceParser::Parse(
			TEXT("conformance/branch-trailing-lf"), Branch);
		ASSERT_THAT(IsTrue(BranchResult.IsSuccess()));
		const auto* Child = FindCase(BranchResult, TEXT("child"));
		const auto* Root = FindCase(BranchResult, TEXT("root"));
		ASSERT_THAT(IsNotNull(Child));
		ASSERT_THAT(IsNotNull(Root));
		if (Child != nullptr && Root != nullptr)
		{
			ASSERT_THAT(AreEqual(FString(TEXT("root")), Child->GetVersionMeta().Parent.GetValue()));
			const auto ChildSource = Child->GetSource();
			const auto RootSource = Root->GetSource();
			ASSERT_THAT(IsTrue(BytesEqual(ChildSource.GetBytes(), "class Child {}\n")));
			ASSERT_THAT(IsTrue(BytesEqual(RootSource.GetBytes(), "class Root {}\n")));
			ASSERT_THAT(AreEqual(149, ChildSource.GetOriginMap().MapToOriginalByteOffset(ChildSource.GetBytes().Num())));
			ASSERT_THAT(AreEqual(225, RootSource.GetOriginMap().MapToOriginalByteOffset(RootSource.GetBytes().Num())));
			const auto& Cases = AngelscriptTest::FAngelscriptTestCodeBuildResultParserTestAccess::GetCases(BranchResult);
			ASSERT_THAT(AreEqual(2, Cases.Num()));
			ASSERT_THAT(AreEqual(FString(TEXT("child")), Cases[0].GetVersionMeta().Tag));
			ASSERT_THAT(AreEqual(FString(TEXT("root")), Cases[1].GetVersionMeta().Tag));
		}
	}

	TEST_METHOD(ConformanceNegative)
	{
		struct FNegativeRow
		{
			const TCHAR* FileName;
			const TCHAR* FileTag;
			const TCHAR* Code;
			int32 SourceLine = 0;
			int32 ByteOffset = 0;
		};
		for (const FNegativeRow& Row : {
			FNegativeRow{TEXT("unknown-directive.as"), TEXT("conformance/unknown-directive"), TEXT("UnknownMetadataDirective"), 4, 53},
			FNegativeRow{TEXT("missing-summary.as"), TEXT("conformance/missing-summary"), TEXT("MissingFileSummary"), 1, 0},
			FNegativeRow{TEXT("duplicate-version.as"), TEXT("conformance/duplicate-version"), TEXT("DuplicateVersionTag"), 17, 216},
			FNegativeRow{TEXT("missing-parent.as"), TEXT("conformance/missing-parent"), TEXT("MissingParent"), 10, 122},
			FNegativeRow{TEXT("cycle.as"), TEXT("conformance/cycle"), TEXT("VersionCycle"), 1, 0},
			FNegativeRow{TEXT("duplicate-point.as"), TEXT("conformance/duplicate-point"), TEXT("DuplicatePoint"), 9, 110},
			FNegativeRow{TEXT("missing-range-end.as"), TEXT("conformance/missing-range-end"), TEXT("MissingRangeEnd"), 9, 120},
			FNegativeRow{TEXT("crossing-range.as"), TEXT("conformance/crossing-range"), TEXT("CrossingRange"), 9, 137}})
		{
			TArray<uint8> Bytes;
			ASSERT_THAT(IsTrue(LoadConformanceBytes(Row.FileName, Bytes)));
			auto Result = AngelscriptTest::FAngelscriptTestSourceParser::Parse(Row.FileTag, Bytes);
			ASSERT_THAT(IsFalse(Result.IsSuccess()));
			const auto* Error = FindError(Result.GetErrors(), Row.Code);
			ASSERT_THAT(IsNotNull(Error));
			if (Error != nullptr)
			{
				ASSERT_THAT(AreEqual(Row.SourceLine, Error->SourceLine));
				ASSERT_THAT(AreEqual(Row.ByteOffset, Error->ByteOffset));
			}
		}
	}

	TEST_METHOD(ConformanceMessageProseIsNotOracle)
	{
		TArray<uint8> Bytes;
		ASSERT_THAT(IsTrue(LoadConformanceBytes(TEXT("unknown-directive.as"), Bytes)));
		auto Result = AngelscriptTest::FAngelscriptTestSourceParser::Parse(
			TEXT("conformance/unknown-directive"), Bytes);
		const auto* Error = FindError(Result.GetErrors(), TEXT("UnknownMetadataDirective"));
		ASSERT_THAT(IsNotNull(Error));
		if (Error != nullptr)
		{
			ASSERT_THAT(AreEqual(4, Error->SourceLine));
			ASSERT_THAT(AreEqual(53, Error->ByteOffset));
			ASSERT_THAT(IsFalse(Error->Message.IsEmpty()));
			ASSERT_THAT(IsFalse(Error->Message == TEXT("UnknownMetadataDirective")));
		}
	}

	TEST_METHOD(TwoParentlessBeginCasesAdmit)
	{
		const ANSICHAR* Container =
			"/**\n"
			" * @version v1\n"
			" * @summary Two parentless theme cases.\n"
			" */\n"
			"/**\n"
			" * @begin alpha\n"
			" * @summary First parentless case.\n"
			" */\n"
			"int Alpha = 1;\n"
			"/** @end */\n"
			"/**\n"
			" * @begin beta\n"
			" * @summary Second parentless case.\n"
			" */\n"
			"int Beta = 2;\n"
			"/** @end */\n";

		auto Result = AngelscriptTest::FAngelscriptTestSourceParser::Parse(
			TEXT("Language/Theme"), AsBytes(Container));
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		if (!Result.IsSuccess())
		{
			return;
		}

		const auto* Alpha = FindCase(Result, TEXT("alpha"));
		const auto* Beta = FindCase(Result, TEXT("beta"));
		ASSERT_THAT(IsNotNull(Alpha));
		ASSERT_THAT(IsNotNull(Beta));
		if (Alpha != nullptr && Beta != nullptr)
		{
			ASSERT_THAT(IsFalse(Alpha->GetVersionMeta().Parent.IsSet()));
			ASSERT_THAT(IsFalse(Beta->GetVersionMeta().Parent.IsSet()));
			const auto AlphaSource = Alpha->GetSource();
			const auto BetaSource = Beta->GetSource();
			ASSERT_THAT(IsTrue(BytesEqual(AlphaSource.GetBytes(), "int Alpha = 1;\n")));
			ASSERT_THAT(IsTrue(BytesEqual(BetaSource.GetBytes(), "int Beta = 2;\n")));
		}
	}

	TEST_METHOD(MissingParentStillFails)
	{
		const ANSICHAR* Container =
			"/**\n"
			" * @version v1\n"
			" * @summary Missing parent still fails.\n"
			" */\n"
			"/**\n"
			" * @begin child\n"
			" * @parent missing\n"
			" * @summary Names a parent that is not in this file.\n"
			" */\n"
			"int Child = 1;\n"
			"/** @end */\n";

		auto Result = AngelscriptTest::FAngelscriptTestSourceParser::Parse(
			TEXT("Language/MissingParent"), AsBytes(Container));
		ASSERT_THAT(IsFalse(Result.IsSuccess()));
		ASSERT_THAT(IsTrue(HasErrorCode(Result.GetErrors(), TEXT("MissingParent"))));
	}

	TEST_METHOD(StarRootStillParsesUntilAuthorsMove)
	{
		const ANSICHAR* Container =
			"/**\n"
			" * @version v1\n"
			" * @summary Legacy star container still admits.\n"
			" */\n"
			"/**\n"
			" * @version root\n"
			" * @summary Privileged-looking root remains ordinary source.\n"
			" */\n"
			"int Root = 0;\n"
			"/** @end */\n"
			"/**\n"
			" * @version child\n"
			" * @parent root\n"
			" * @summary Child still names the legacy root.\n"
			" */\n"
			"int Child = 1;\n"
			"/** @end */\n";

		auto Result = AngelscriptTest::FAngelscriptTestSourceParser::Parse(
			TEXT("Language/StarRoot"), AsBytes(Container));
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		if (!Result.IsSuccess())
		{
			return;
		}

		const auto* Root = FindCase(Result, TEXT("root"));
		const auto* Child = FindCase(Result, TEXT("child"));
		ASSERT_THAT(IsNotNull(Root));
		ASSERT_THAT(IsNotNull(Child));
		if (Root != nullptr && Child != nullptr)
		{
			ASSERT_THAT(IsFalse(Root->GetVersionMeta().Parent.IsSet()));
			ASSERT_THAT(AreEqual(FString(TEXT("root")), Child->GetVersionMeta().Parent.GetValue()));
			const auto RootSource = Root->GetSource();
			const auto ChildSource = Child->GetSource();
			ASSERT_THAT(IsTrue(BytesEqual(RootSource.GetBytes(), "int Root = 0;\n")));
			ASSERT_THAT(IsTrue(BytesEqual(ChildSource.GetBytes(), "int Child = 1;\n")));
		}
	}
};

#endif // WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS
