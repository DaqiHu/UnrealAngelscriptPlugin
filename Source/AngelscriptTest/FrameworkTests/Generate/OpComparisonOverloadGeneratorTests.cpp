#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestOpComparisonOverloadGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	OpComparisonOverloadGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString OperatorNeedle;
		FString LeftNeedle;
		FString RightNeedle;
		int32 Expected = 0;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-OP-COMPARISON-OVERLOAD-LESS-LESS-LEFT_RIGHT-MUTABLE");
	static constexpr const TCHAR* ZeroCaseId =
		TEXT("LANG-OP-COMPARISON-OVERLOAD-LESS-EQUAL-LEFT_RIGHT-MUTABLE");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-OP-ASSIGNMENT-LOCAL-ASSIGN_INT");

	static void IndependentValues(const TCHAR* Relation, int32& OutLeft, int32& OutRight)
	{
		if (FCString::Strcmp(Relation, TEXT("LESS")) == 0)
		{
			OutLeft = 1;
			OutRight = 2;
			return;
		}
		if (FCString::Strcmp(Relation, TEXT("EQUAL")) == 0)
		{
			OutLeft = 2;
			OutRight = 2;
			return;
		}
		if (FCString::Strcmp(Relation, TEXT("GREATER")) == 0)
		{
			OutLeft = 3;
			OutRight = 2;
			return;
		}
		OutLeft = -4;
		OutRight = 5;
	}

	static int32 IndependentExpected(const TCHAR* Operator, const TCHAR* Relation, const TCHAR* Order)
	{
		int32 Left = 0;
		int32 Right = 0;
		IndependentValues(Relation, Left, Right);
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
			return TEXT(" < ");
		}
		if (FCString::Strcmp(Operator, TEXT("LESS_EQUAL")) == 0)
		{
			return TEXT(" <= ");
		}
		if (FCString::Strcmp(Operator, TEXT("GREATER")) == 0)
		{
			return TEXT(" > ");
		}
		if (FCString::Strcmp(Operator, TEXT("GREATER_EQUAL")) == 0)
		{
			return TEXT(" >= ");
		}
		if (FCString::Strcmp(Operator, TEXT("EQUAL")) == 0)
		{
			return TEXT(" == ");
		}
		return TEXT(" != ");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kOperators[] = {
			TEXT("LESS"), TEXT("LESS_EQUAL"), TEXT("GREATER"), TEXT("GREATER_EQUAL"), TEXT("EQUAL"), TEXT("NOT_EQUAL")};
		static const TCHAR* const kRelations[] = {TEXT("LESS"), TEXT("EQUAL"), TEXT("GREATER"), TEXT("UNEQUAL")};
		static const TCHAR* const kOrders[] = {TEXT("LEFT_RIGHT"), TEXT("RIGHT_LEFT")};
		static const TCHAR* const kReceivers[] = {TEXT("MUTABLE"), TEXT("CONST")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(96);
		for (int32 Index = 0; Index < 96; ++Index)
		{
			const TCHAR* const Operator = kOperators[Index / 16];
			const TCHAR* const Relation = kRelations[(Index / 4) % 4];
			const TCHAR* const Order = kOrders[(Index / 2) % 2];
			const TCHAR* const Receiver = kReceivers[Index % 2];
			int32 Left = 0;
			int32 Right = 0;
			IndependentValues(Relation, Left, Right);
			const bool bLeftConst = FCString::Strcmp(Receiver, TEXT("CONST")) == 0
				&& FCString::Strcmp(Order, TEXT("LEFT_RIGHT")) == 0;
			const bool bRightConst = FCString::Strcmp(Receiver, TEXT("CONST")) == 0
				&& FCString::Strcmp(Order, TEXT("RIGHT_LEFT")) == 0;
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-OP-COMPARISON-OVERLOAD-%s-%s-%s-%s"), Operator, Relation, Order, Receiver);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.OperatorNeedle = IndependentOperatorNeedle(Operator);
			Cell.LeftNeedle = FString::Printf(
				TEXT("%sFOverloadedComparisonValue Left(%d);"),
				bLeftConst ? TEXT("const ") : TEXT(""),
				Left);
			Cell.RightNeedle = FString::Printf(
				TEXT("%sFOverloadedComparisonValue Right(%d);"),
				bRightConst ? TEXT("const ") : TEXT(""),
				Right);
			Cell.Expected = IndependentExpected(Operator, Relation, Order);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EOpComparisonOverloadOperator;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FOpComparisonOverloadGenerator;
		using AngelscriptTest::Generate::FOpComparisonOverloadParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FOpComparisonOverloadGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-OP-COMPARISON-OVERLOAD")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangOpComparisonOverloadLessLessLeftRightMutable")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("OpComparisonOverloadGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("OpComparisonOverloadGenerator dump path=%s cases=96 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(96, CaseCount));
		ASSERT_THAT(AreEqual(96, Cases.Num()));
		ASSERT_THAT(AreEqual(96, ExpectedCells.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("struct FOverloadedComparisonValue"))));

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
			LoadGold(TEXT("LanguageGenerators/OpComparisonOverloadGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.OperatorNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.LeftNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.RightNeedle)));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FOpComparisonOverloadParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildOverloadedComparisonSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FOpComparisonOverloadParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildOverloadedComparisonSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildOverloadedComparisonSource(ProbeParams).IsEmpty()));
		FOpComparisonOverloadParams InvalidEnum = DefaultParams;
		InvalidEnum.Operator = static_cast<EOpComparisonOverloadOperator>(6);
		ASSERT_THAT(IsTrue(Generator.BuildOverloadedComparisonSource(InvalidEnum).IsEmpty()));
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
		ASSERT_THAT(AreEqual(96, CountOccurrences(Dump, TEXT("// Case: LANG-OP-COMPARISON-OVERLOAD-"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/OpComparisonOverloadGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("OpComparisonOverloadGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
