#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestExprLazyEvaluationGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ExprLazyEvaluationGenerator,
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
		TEXT("LANG-EXPR-LAZY-EVALUATION-LOGICAL_AND-VALUE-FALSE-SINGLE_LINE");
	static constexpr const TCHAR* ZeroCaseId =
		TEXT("LANG-EXPR-LAZY-EVALUATION-LOGICAL_AND-VALUE-FALSE-SINGLE_LINE");
	static constexpr const TCHAR* ForeignCaseId =
		TEXT("LANG-EXPR-ASSOCIATIVITY-UNPARENTHESIZED-MULTIPLICATIVE-REPEATED_OPERATOR");

	static bool IndependentGuardedSelected(const TCHAR* Form, const TCHAR* Selector)
	{
		const bool bTrue = FCString::Strcmp(Selector, TEXT("TRUE")) == 0;
		if (FCString::Strcmp(Form, TEXT("LOGICAL_AND")) == 0)
		{
			return bTrue;
		}
		if (FCString::Strcmp(Form, TEXT("LOGICAL_OR")) == 0)
		{
			return !bTrue;
		}
		return bTrue;
	}

	static int32 IndependentExpected(const TCHAR* Form, const TCHAR* Selector)
	{
		const bool bTrue = FCString::Strcmp(Selector, TEXT("TRUE")) == 0;
		if (FCString::Strcmp(Form, TEXT("LOGICAL_AND")) == 0)
		{
			return bTrue ? 1 : 0;
		}
		if (FCString::Strcmp(Form, TEXT("LOGICAL_OR")) == 0)
		{
			return 1;
		}
		return bTrue ? 41 : 23;
	}

	static FString IndependentNeedle(const TCHAR* Form, const TCHAR* Outcome, const TCHAR* Shape)
	{
		if (FCString::Strcmp(Shape, TEXT("MULTILINE")) == 0)
		{
			return FCString::Strcmp(Form, TEXT("LOGICAL_OR")) == 0 ? TEXT("\t\t||") : (
				FCString::Strcmp(Form, TEXT("LOGICAL_AND")) == 0 ? TEXT("\t\t&&") : TEXT("\t\t?"));
		}
		if (FCString::Strcmp(Outcome, TEXT("EXCEPTION")) == 0)
		{
			return FCString::Strcmp(Form, TEXT("CONDITIONAL")) == 0
				? TEXT("RaiseGuardedInt()")
				: TEXT("RaiseGuardedBool()");
		}
		if (FCString::Strcmp(Outcome, TEXT("SIDE_EFFECT")) == 0)
		{
			return FCString::Strcmp(Form, TEXT("CONDITIONAL")) == 0
				? TEXT("RecordExpressionInt(2, 41)")
				: TEXT("RecordExpressionBool(2, true)");
		}
		if (FCString::Strcmp(Shape, TEXT("COMMENTS")) == 0)
		{
			return TEXT("/* selector */");
		}
		if (FCString::Strcmp(Shape, TEXT("PARENTHESIZED")) == 0)
		{
			return FCString::Strcmp(Form, TEXT("CONDITIONAL")) == 0 ? TEXT(") ? (") : TEXT("((");
		}
		if (FCString::Strcmp(Form, TEXT("LOGICAL_AND")) == 0)
		{
			return TEXT(" && ");
		}
		if (FCString::Strcmp(Form, TEXT("LOGICAL_OR")) == 0)
		{
			return TEXT(" || ");
		}
		return TEXT("RecordExpressionInt(3, 23)");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kForms[] = {TEXT("LOGICAL_AND"), TEXT("LOGICAL_OR"), TEXT("CONDITIONAL")};
		static const TCHAR* const kOutcomes[] = {TEXT("VALUE"), TEXT("SIDE_EFFECT"), TEXT("EXCEPTION")};
		static const TCHAR* const kSelectors[] = {TEXT("FALSE"), TEXT("TRUE")};
		static const TCHAR* const kShapes[] = {
			TEXT("SINGLE_LINE"), TEXT("COMMENTS"), TEXT("MULTILINE"), TEXT("PARENTHESIZED")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(72);
		for (int32 Index = 0; Index < 72; ++Index)
		{
			const TCHAR* const Form = kForms[Index / 24];
			const TCHAR* const Outcome = kOutcomes[(Index / 8) % 3];
			const TCHAR* const Selector = kSelectors[(Index / 4) % 2];
			const TCHAR* const Shape = kShapes[Index % 4];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-EXPR-LAZY-EVALUATION-%s-%s-%s-%s"), Form, Outcome, Selector, Shape);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bFault = FCString::Strcmp(Outcome, TEXT("EXCEPTION")) == 0
				&& IndependentGuardedSelected(Form, Selector);
			Cell.Expected = Cell.bFault ? 0 : IndependentExpected(Form, Selector);
			Cell.Needle = IndependentNeedle(Form, Outcome, Shape);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EExprLazyForm;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FExprLazyEvaluationGenerator;
		using AngelscriptTest::Generate::FExprLazyEvaluationParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FExprLazyEvaluationGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-EXPR-LAZY-EVALUATION")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangExprLazyEvaluationLogicalAndValueFalseSingleLine")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ExprLazyEvaluationGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ExprLazyEvaluationGenerator dump path=%s cases=72 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(72, CaseCount));
		ASSERT_THAT(AreEqual(72, Cases.Num()));
		ASSERT_THAT(AreEqual(72, ExpectedCells.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/ExprLazyEvaluationGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 FaultCount = 0;
		int32 ZeroCount = 0;
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
				if (Expected.Expected == 0)
				{
					++ZeroCount;
				}
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
		ASSERT_THAT(AreEqual(12, FaultCount));
		ASSERT_THAT(IsTrue(ZeroCount > 0));

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangExprLazyEvaluationLogicalAndValueFalseSingleLine()"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("bool RaiseGuardedBool()"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int RaiseGuardedInt()"))));

		FExprLazyEvaluationParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildLazyEvaluationSource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildLazyEvaluationSource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildLazyEvaluationSource(DefaultParams).IsEmpty()));
		FExprLazyEvaluationParams InvalidEnum;
		InvalidEnum.Form = static_cast<EExprLazyForm>(3);
		ASSERT_THAT(IsTrue(Generator.BuildLazyEvaluationSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(ZeroCaseId)));

		bool bFoundZeroCase = false;
		for (int32 Index = 0; Index < Cases.Num(); ++Index)
		{
			if (Cases[Index].CaseId == ZeroCaseId)
			{
				bFoundZeroCase = true;
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Cases[Index].ExpectedReturn.Get(MIN_int32)));
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
		ASSERT_THAT(AreEqual(72, CountOccurrences(Dump, TEXT("// Case: LANG-EXPR-LAZY-EVALUATION-"))));
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			FString DumpBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Dump, ExpectedCells[Index].EntryName, DumpBody)));
			FString AggregateBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, ExpectedCells[Index].EntryName, AggregateBody)));
			ASSERT_THAT(AreEqual(AggregateBody, DumpBody));
		}
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/ExprLazyEvaluationGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ExprLazyEvaluationGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
