#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestOpNumericBinaryGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	OpNumericBinaryGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString LeftNeedle;
		FString RightNeedle;
		FString OpNeedle;
		int32 Expected = 0;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-OP-NUMERIC-BINARY-INT-ADD-INT-ZERO");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-CONDITION-IF-BOOL-LITERAL-TRUE");
	static constexpr int32 TypeCount = 10;
	static constexpr int32 OperatorCount = 11;
	static constexpr int32 ValueCount = 5;
	static constexpr int32 CellCount = 5500;

	static const TCHAR* TypeToken(const int32 Index)
	{
		static const TCHAR* const kTokens[] = {
			TEXT("INT8"), TEXT("INT16"), TEXT("INT"), TEXT("INT64"),
			TEXT("UINT8"), TEXT("UINT16"), TEXT("UINT"), TEXT("UINT64"),
			TEXT("FLOAT32"), TEXT("FLOAT64")};
		return kTokens[Index];
	}

	static const TCHAR* TypeName(const int32 Index)
	{
		static const TCHAR* const kNames[] = {
			TEXT("int8"), TEXT("int16"), TEXT("int"), TEXT("int64"),
			TEXT("uint8"), TEXT("uint16"), TEXT("uint"), TEXT("uint64"),
			TEXT("float32"), TEXT("float64")};
		return kNames[Index];
	}

	static const TCHAR* OperatorToken(const int32 Index)
	{
		static const TCHAR* const kTokens[] = {
			TEXT("ADD"), TEXT("SUBTRACT"), TEXT("MULTIPLY"), TEXT("DIVIDE"), TEXT("MODULO"),
			TEXT("LESS"), TEXT("LESS_EQUAL"), TEXT("GREATER"), TEXT("GREATER_EQUAL"), TEXT("EQUAL"), TEXT("NOT_EQUAL")};
		return kTokens[Index];
	}

	static const TCHAR* OperatorText(const int32 Index)
	{
		static const TCHAR* const kText[] = {
			TEXT("+"), TEXT("-"), TEXT("*"), TEXT("/"), TEXT("%"),
			TEXT("<"), TEXT("<="), TEXT(">"), TEXT(">="), TEXT("=="), TEXT("!=")};
		return kText[Index];
	}

	static const TCHAR* ValueToken(const int32 Index)
	{
		static const TCHAR* const kTokens[] = {
			TEXT("ZERO"), TEXT("ONE"), TEXT("NEGATIVE"), TEXT("NEAR_MIN"), TEXT("NEAR_MAX")};
		return kTokens[Index];
	}

	static bool IsComparison(const int32 OperatorIndex)
	{
		return OperatorIndex >= 5;
	}

	static bool IsSigned(const int32 TypeIndex)
	{
		return TypeIndex <= 3;
	}

	static bool IsFloat32(const int32 TypeIndex)
	{
		return TypeIndex == 8;
	}

	static bool IsFloat64(const int32 TypeIndex)
	{
		return TypeIndex == 9;
	}

	static bool IsWide(const int32 TypeIndex)
	{
		return TypeIndex == 3 || TypeIndex == 7 || TypeIndex == 9;
	}

	static int32 IndependentMarker(const int32 Left, const int32 Operator, const int32 Right)
	{
		if (IsComparison(Operator))
		{
			return 107;
		}
		if (IsFloat64(Left) || IsFloat64(Right))
		{
			return 106;
		}
		if (IsFloat32(Left) || IsFloat32(Right))
		{
			return 105;
		}
		const bool bWide = IsWide(Left) || IsWide(Right);
		const bool bSigned = IsSigned(Left) || IsSigned(Right);
		if (bWide)
		{
			return bSigned ? 103 : 104;
		}
		return bSigned ? 101 : 102;
	}

	static const TCHAR* PartitionLiteral(const int32 TypeIndex, const int32 ValueIndex)
	{
		if (IsFloat32(TypeIndex) || IsFloat64(TypeIndex))
		{
			if (ValueIndex == 0)
			{
				return TEXT("0.0");
			}
			if (ValueIndex == 1)
			{
				return TEXT("1.0");
			}
			if (ValueIndex == 2)
			{
				return TEXT("-3.0");
			}
			return IsFloat32(TypeIndex)
				? (ValueIndex == 3 ? TEXT("-1.70141173e38") : TEXT("1.70141173e38"))
				: (ValueIndex == 3 ? TEXT("-8.988465674311579e307") : TEXT("8.988465674311579e307"));
		}
		if (IsSigned(TypeIndex))
		{
			if (ValueIndex == 0)
			{
				return TEXT("0");
			}
			if (ValueIndex == 1)
			{
				return TEXT("1");
			}
			if (ValueIndex == 2)
			{
				return TEXT("-3");
			}
			if (ValueIndex == 3)
			{
				if (TypeIndex == 0)
				{
					return TEXT("-127");
				}
				if (TypeIndex == 1)
				{
					return TEXT("-32767");
				}
				if (TypeIndex == 3)
				{
					return TEXT("-9223372036854775807");
				}
				return TEXT("-2147483647");
			}
			if (TypeIndex == 0)
			{
				return TEXT("126");
			}
			if (TypeIndex == 1)
			{
				return TEXT("32766");
			}
			if (TypeIndex == 3)
			{
				return TEXT("9223372036854775806");
			}
			return TEXT("2147483646");
		}
		if (ValueIndex == 0)
		{
			return TEXT("0");
		}
		if (ValueIndex == 1 || ValueIndex == 3)
		{
			return TEXT("1");
		}
		if (ValueIndex == 2)
		{
			if (TypeIndex == 4)
			{
				return TEXT("253");
			}
			if (TypeIndex == 5)
			{
				return TEXT("65533");
			}
			if (TypeIndex == 7)
			{
				return TEXT("18446744073709551613");
			}
			return TEXT("4294967293");
		}
		if (TypeIndex == 4)
		{
			return TEXT("254");
		}
		if (TypeIndex == 5)
		{
			return TEXT("65534");
		}
		if (TypeIndex == 7)
		{
			return TEXT("18446744073709551614");
		}
		return TEXT("4294967294");
	}

	static const TCHAR* RightLiteral(const int32 TypeIndex, const int32 OperatorIndex, const int32 ValueIndex)
	{
		if (IsComparison(OperatorIndex))
		{
			return PartitionLiteral(TypeIndex, ValueIndex);
		}
		const bool bNear = ValueIndex == 3 || ValueIndex == 4;
		const bool bFloat = IsFloat32(TypeIndex) || IsFloat64(TypeIndex);
		if (OperatorIndex == 2 || OperatorIndex == 3)
		{
			return bFloat ? (bNear ? TEXT("1.0") : TEXT("2.0")) : (bNear ? TEXT("1") : TEXT("2"));
		}
		if (OperatorIndex == 4)
		{
			return bFloat ? TEXT("2.0") : TEXT("2");
		}
		return bFloat ? TEXT("1.0") : TEXT("1");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		TArray<FExpectedCell> Cells;
		Cells.SetNum(CellCount);
		for (int32 Index = 0; Index < CellCount; ++Index)
		{
			const int32 Left = Index / (OperatorCount * TypeCount * ValueCount);
			const int32 Operator = (Index / (TypeCount * ValueCount)) % OperatorCount;
			const int32 Right = (Index / ValueCount) % TypeCount;
			const int32 Value = Index % ValueCount;
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-OP-NUMERIC-BINARY-%s-%s-%s-%s"),
				TypeToken(Left),
				OperatorToken(Operator),
				TypeToken(Right),
				ValueToken(Value));
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.LeftNeedle = FString::Printf(
				TEXT("%s Left = %s(%s);"),
				TypeName(Left),
				TypeName(Left),
				PartitionLiteral(Left, Value));
			Cell.RightNeedle = FString::Printf(
				TEXT("%s Right = %s(%s);"),
				TypeName(Right),
				TypeName(Right),
				RightLiteral(Right, Operator, Value));
			Cell.OpNeedle = FString::Printf(TEXT("ObserveNumericType(Left %s Right)"), OperatorText(Operator));
			Cell.Expected = IndependentMarker(Left, Operator, Right);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EOpNumericBinaryOperator;
		using AngelscriptTest::Generate::EOpNumericBinaryType;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FOpNumericBinaryGenerator;
		using AngelscriptTest::Generate::FOpNumericBinaryParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FOpNumericBinaryGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-OP-NUMERIC-BINARY")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangOpNumericBinaryIntAddIntZero")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("OpNumericBinaryGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("OpNumericBinaryGenerator dump path=%s cases=5500 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(5500, CaseCount));
		ASSERT_THAT(AreEqual(5500, Cases.Num()));
		ASSERT_THAT(AreEqual(5500, ExpectedCells.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/OpNumericBinaryGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 GoldMatches = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedException.IsEmpty()));
			ASSERT_THAT(IsTrue(Cases[Index].bLimitedObservation));
			ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("full-width"))));

			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.LeftNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.RightNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.OpNeedle)));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				++GoldMatches;
			}
		}
		ASSERT_THAT(IsTrue(GoldMatches > 0));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangOpNumericBinaryIntAddIntZero()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangOpNumericBinaryIntAddIntZero()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int ObserveNumericType(int Value)"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int ObserveNumericType(bool Value)"))));

		FOpNumericBinaryParams DefaultParams;
		const FString DefaultSingle = Generator.BuildNumericBinarySource(DefaultParams);
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsFalse(DefaultSingle.Contains(TEXT("int EntryLang"))));
		FOpNumericBinaryParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildNumericBinarySource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildNumericBinarySource(ProbeParams).IsEmpty()));
		FOpNumericBinaryParams InvalidEnum = DefaultParams;
		InvalidEnum.LeftType = static_cast<EOpNumericBinaryType>(10);
		ASSERT_THAT(IsTrue(Generator.BuildNumericBinarySource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(ExpectedCells[0].Expected, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(5500, CountOccurrences(Dump, TEXT("// Case: LANG-OP-NUMERIC-BINARY-"))));
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(IsTrue(Dump.Contains(FString::Printf(TEXT("// Case: %s"), *Expected.CaseId))));
			FString DumpBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Dump, Expected.EntryName, DumpBody)));
			FString AggregateBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, AggregateBody)));
			ASSERT_THAT(AreEqual(AggregateBody, DumpBody));
		}
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		ASSERT_THAT(IsTrue(AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("")).IsEmpty()));
		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/OpNumericBinaryGenerator.as"), Dump, ProbeError)));
		ASSERT_THAT(IsTrue(ProbeError.Len() > 0));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("OpNumericBinaryGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
