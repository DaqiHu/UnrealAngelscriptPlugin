#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestExprEvalOrderGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ExprEvalOrderGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString Needle;
		int32 Expected = 0;
		bool bFault = false;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-EXPR-EVAL-ORDER-BINARY-TWO-COMPLETE-SINGLE_LINE");
	static constexpr const TCHAR* ForeignCaseId =
		TEXT("LANG-EXPR-LAZY-EVALUATION-LOGICAL_AND-VALUE-FALSE-SINGLE_LINE");

	static int32 IndependentCount(const TCHAR* Count)
	{
		if (FCString::Strcmp(Count, TEXT("TWO")) == 0)
		{
			return 2;
		}
		if (FCString::Strcmp(Count, TEXT("THREE")) == 0)
		{
			return 3;
		}
		return 8;
	}

	static int32 IndependentExpected(const TCHAR* Composition, const TCHAR* Count)
	{
		const int32 N = IndependentCount(Count);
		const int32 Sum = N * (N + 1) / 2;
		return FCString::Strcmp(Composition, TEXT("COMPOUND_ASSIGNMENT")) == 0 ? 10 + Sum : Sum;
	}

	static FString IndependentNeedle(const TCHAR* Composition, const TCHAR* Outcome, const TCHAR* Shape)
	{
		if (FCString::Strcmp(Outcome, TEXT("EXCEPTION_MIDDLE")) == 0
			&& FCString::Strcmp(Shape, TEXT("SINGLE_LINE")) == 0)
		{
			return TEXT("RecordEagerStage");
		}
		if (FCString::Strcmp(Shape, TEXT("COMMENTS")) == 0)
		{
			return TEXT("/* stage_1 */");
		}
		if (FCString::Strcmp(Shape, TEXT("NESTED_PARENTHESES")) == 0)
		{
			return TEXT("(((");
		}
		if (FCString::Strcmp(Composition, TEXT("ASSIGNMENT")) == 0)
		{
			return TEXT("Target");
		}
		if (FCString::Strcmp(Composition, TEXT("COMPOUND_ASSIGNMENT")) == 0)
		{
			return TEXT("+=");
		}
		if (FCString::Strcmp(Composition, TEXT("CALL_ARGUMENTS")) == 0)
		{
			return TEXT("CollectEager(");
		}
		if (FCString::Strcmp(Composition, TEXT("CONSTRUCTOR_ARGUMENTS")) == 0)
		{
			return TEXT("FEagerConstructed");
		}
		if (FCString::Strcmp(Composition, TEXT("INDEX_ARGUMENTS")) == 0)
		{
			return TEXT("Indexer[");
		}
		if (FCString::Strcmp(Composition, TEXT("CALL_CHAIN")) == 0)
		{
			return TEXT(".Step(");
		}
		if (FCString::Strcmp(Composition, TEXT("MEMBER_INDEX_CHAIN")) == 0)
		{
			return TEXT("[0]");
		}
		if (FCString::Strcmp(Composition, TEXT("NESTED_CAST")) == 0)
		{
			return TEXT("double(");
		}
		return TEXT("RecordEagerStage(1, 1, ");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kCompositions[] = {
			TEXT("BINARY"), TEXT("ASSIGNMENT"), TEXT("COMPOUND_ASSIGNMENT"), TEXT("CALL_ARGUMENTS"),
			TEXT("CONSTRUCTOR_ARGUMENTS"), TEXT("INDEX_ARGUMENTS"), TEXT("CALL_CHAIN"),
			TEXT("MEMBER_INDEX_CHAIN"), TEXT("NESTED_CAST")};
		static const TCHAR* const kCounts[] = {TEXT("TWO"), TEXT("THREE"), TEXT("EIGHT")};
		static const TCHAR* const kOutcomes[] = {
			TEXT("COMPLETE"), TEXT("EXCEPTION_FIRST"), TEXT("EXCEPTION_MIDDLE"), TEXT("EXCEPTION_LAST")};
		static const TCHAR* const kShapes[] = {
			TEXT("SINGLE_LINE"), TEXT("WHITESPACE"), TEXT("COMMENTS"), TEXT("MULTILINE"),
			TEXT("NESTED_PARENTHESES")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(540);
		for (int32 Index = 0; Index < 540; ++Index)
		{
			const TCHAR* const Composition = kCompositions[Index / 60];
			const TCHAR* const Count = kCounts[(Index / 20) % 3];
			const TCHAR* const Outcome = kOutcomes[(Index / 5) % 4];
			const TCHAR* const Shape = kShapes[Index % 5];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-EXPR-EVAL-ORDER-%s-%s-%s-%s"), Composition, Count, Outcome, Shape);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bFault = FCString::Strcmp(Outcome, TEXT("COMPLETE")) != 0;
			Cell.Expected = Cell.bFault ? 0 : IndependentExpected(Composition, Count);
			Cell.Needle = IndependentNeedle(Composition, Outcome, Shape);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EExprEvalOrderComposition;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FExprEvalOrderGenerator;
		using AngelscriptTest::Generate::FExprEvalOrderParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FExprEvalOrderGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-EXPR-EVAL-ORDER")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangExprEvalOrderBinaryTwoCompleteSingleLine")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ExprEvalOrderGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ExprEvalOrderGenerator dump path=%s cases=540 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(540, CaseCount));
		ASSERT_THAT(AreEqual(540, Cases.Num()));
		ASSERT_THAT(AreEqual(540, ExpectedCells.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/ExprEvalOrderGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 FaultCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(
				FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			if (Expected.bFault)
			{
				++FaultCount;
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")), Cases[Index].ExpectedException));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
			}
			else
			{
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			}
			ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}
		ASSERT_THAT(AreEqual(405, FaultCount));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int RecordEagerStage("))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("struct FEagerChain"))));

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		FExprEvalOrderParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildEagerEvaluationSource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildEagerEvaluationSource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildEagerEvaluationSource(DefaultParams).IsEmpty()));
		FExprEvalOrderParams InvalidEnum;
		InvalidEnum.Composition = static_cast<EExprEvalOrderComposition>(9);
		ASSERT_THAT(IsTrue(Generator.BuildEagerEvaluationSource(InvalidEnum).IsEmpty()));
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
		ASSERT_THAT(AreEqual(540, CountOccurrences(Dump, TEXT("// Case: LANG-EXPR-EVAL-ORDER-"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/ExprEvalOrderGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ExprEvalOrderGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
