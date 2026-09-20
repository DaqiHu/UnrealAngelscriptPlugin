#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestOpComparisonEnumAliasGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	OpComparisonEnumAliasGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString OperatorNeedle;
		FString OperandNeedle;
		int32 Expected = 0;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-OP-COMPARISON-ENUM-ALIAS-ENUM-LESS-EQUAL_ZERO-LEFT_RIGHT");
	static constexpr const TCHAR* ZeroCaseId =
		TEXT("LANG-OP-COMPARISON-ENUM-ALIAS-ENUM-LESS-EQUAL_ZERO-LEFT_RIGHT");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-OP-ASSIGNMENT-LOCAL-ASSIGN_INT");

	static void IndependentValues(const TCHAR* Pair, int32& OutLeft, int32& OutRight)
	{
		if (FCString::Strcmp(Pair, TEXT("EQUAL_ZERO")) == 0)
		{
			OutLeft = 0;
			OutRight = 0;
			return;
		}
		if (FCString::Strcmp(Pair, TEXT("EQUAL_NAMED")) == 0)
		{
			OutLeft = 1;
			OutRight = 1;
			return;
		}
		if (FCString::Strcmp(Pair, TEXT("BELOW_ADJACENT")) == 0)
		{
			OutLeft = -1;
			OutRight = 0;
			return;
		}
		if (FCString::Strcmp(Pair, TEXT("ABOVE_ADJACENT")) == 0)
		{
			OutLeft = 1;
			OutRight = 0;
			return;
		}
		if (FCString::Strcmp(Pair, TEXT("MINIMUM_BOUNDARY")) == 0)
		{
			OutLeft = -128;
			OutRight = -1;
			return;
		}
		OutLeft = 127;
		OutRight = 1;
	}

	static int32 IndependentExpected(const TCHAR* Operator, const TCHAR* Pair, const TCHAR* Order)
	{
		int32 Left = 0;
		int32 Right = 0;
		IndependentValues(Pair, Left, Right);
		if (FCString::Strcmp(Order, TEXT("RIGHT_LEFT")) == 0)
		{
			const int32 Swap = Left;
			Left = Right;
			Right = Swap;
		}
		if (FCString::Strcmp(Operator, TEXT("LESS")) == 0)
		{
			return Left < Right ? 1 : 0;
		}
		if (FCString::Strcmp(Operator, TEXT("LESS_EQUAL")) == 0)
		{
			return Left <= Right ? 1 : 0;
		}
		if (FCString::Strcmp(Operator, TEXT("GREATER")) == 0)
		{
			return Left > Right ? 1 : 0;
		}
		if (FCString::Strcmp(Operator, TEXT("GREATER_EQUAL")) == 0)
		{
			return Left >= Right ? 1 : 0;
		}
		if (FCString::Strcmp(Operator, TEXT("EQUAL")) == 0)
		{
			return Left == Right ? 1 : 0;
		}
		return Left != Right ? 1 : 0;
	}

	static FString IndependentOperatorNeedle(const TCHAR* Operator)
	{
		if (FCString::Strcmp(Operator, TEXT("LESS")) == 0)
		{
			return TEXT(") < ");
		}
		if (FCString::Strcmp(Operator, TEXT("LESS_EQUAL")) == 0)
		{
			return TEXT(") <= ");
		}
		if (FCString::Strcmp(Operator, TEXT("GREATER")) == 0)
		{
			return TEXT(") > ");
		}
		if (FCString::Strcmp(Operator, TEXT("GREATER_EQUAL")) == 0)
		{
			return TEXT(") >= ");
		}
		if (FCString::Strcmp(Operator, TEXT("EQUAL")) == 0)
		{
			return TEXT(") == ");
		}
		return TEXT(") != ");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kFamilies[] = {TEXT("ENUM"), TEXT("ALIAS")};
		static const TCHAR* const kOperators[] = {
			TEXT("LESS"), TEXT("LESS_EQUAL"), TEXT("GREATER"), TEXT("GREATER_EQUAL"), TEXT("EQUAL"), TEXT("NOT_EQUAL")};
		static const TCHAR* const kPairs[] = {
			TEXT("EQUAL_ZERO"),
			TEXT("EQUAL_NAMED"),
			TEXT("BELOW_ADJACENT"),
			TEXT("ABOVE_ADJACENT"),
			TEXT("MINIMUM_BOUNDARY"),
			TEXT("MAXIMUM_BOUNDARY")};
		static const TCHAR* const kOrders[] = {TEXT("LEFT_RIGHT"), TEXT("RIGHT_LEFT")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(144);
		for (int32 Index = 0; Index < 144; ++Index)
		{
			const TCHAR* const Family = kFamilies[Index / 72];
			const TCHAR* const Operator = kOperators[(Index / 12) % 6];
			const TCHAR* const Pair = kPairs[(Index / 2) % 6];
			const TCHAR* const Order = kOrders[Index % 2];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-OP-COMPARISON-ENUM-ALIAS-%s-%s-%s-%s"), Family, Operator, Pair, Order);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.OperatorNeedle = IndependentOperatorNeedle(Operator);
			Cell.OperandNeedle = FCString::Strcmp(Family, TEXT("ENUM")) == 0
				? TEXT("TraceEnumOperand")
				: TEXT("TraceAliasOperand");
			Cell.Expected = IndependentExpected(Operator, Pair, Order);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EOpComparisonEnumAliasFamily;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FOpComparisonEnumAliasGenerator;
		using AngelscriptTest::Generate::FOpComparisonEnumAliasParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FOpComparisonEnumAliasGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-OP-COMPARISON-ENUM-ALIAS")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangOpComparisonEnumAliasEnumLessEqualZeroLeftRight")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("OpComparisonEnumAliasGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("OpComparisonEnumAliasGenerator dump path=%s cases=144 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(144, CaseCount));
		ASSERT_THAT(AreEqual(144, Cases.Num()));
		ASSERT_THAT(AreEqual(144, ExpectedCells.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("enum EComparisonEnum"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("TraceEnumOperand(int Marker"))));

		int32 RejectCount = 0;
		int32 ZeroCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			ASSERT_THAT(IsFalse(Cases[Index].bLimitedObservation));
			if (Expected.Expected == 0)
			{
				++ZeroCount;
			}
			if (Cases[Index].Outcome == EGeneratedCaseOutcome::CompileReject)
			{
				++RejectCount;
			}
		}
		ASSERT_THAT(AreEqual(0, RejectCount));
		ASSERT_THAT(IsTrue(ZeroCount > 0));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/OpComparisonEnumAliasGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.OperatorNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.OperandNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("return Result ? 1 : 0;"))));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FOpComparisonEnumAliasParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildIntegralComparisonSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FOpComparisonEnumAliasParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildIntegralComparisonSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildIntegralComparisonSource(ProbeParams).IsEmpty()));
		FOpComparisonEnumAliasParams InvalidEnum = DefaultParams;
		InvalidEnum.Family = static_cast<EOpComparisonEnumAliasFamily>(2);
		ASSERT_THAT(IsTrue(Generator.BuildIntegralComparisonSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(ZeroCaseId)));
		bool bFoundZeroCase = false;
		for (const FGeneratedCaseInfo& Info : Cases)
		{
			if (Info.CaseId == ZeroCaseId)
			{
				bFoundZeroCase = true;
				ASSERT_THAT(IsTrue(Info.ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Info.ExpectedReturn.Get(MIN_int32)));
			}
		}
		ASSERT_THAT(IsTrue(bFoundZeroCase));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(144, CountOccurrences(Dump, TEXT("// Case: LANG-OP-COMPARISON-ENUM-ALIAS-"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/OpComparisonEnumAliasGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("OpComparisonEnumAliasGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
