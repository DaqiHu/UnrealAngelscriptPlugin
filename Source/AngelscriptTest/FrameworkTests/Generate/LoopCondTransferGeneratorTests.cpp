#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestLoopCondTransferGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace LoopCondTransferGeneratorTest
{
struct FLoopCondExpectedCell
{
	FString CaseId;
	FString EntryName;
	FString HeaderNeedle;
	FString ConditionNeedle;
	int32 Expected = 0;
};

constexpr const TCHAR* ProbeCaseId = TEXT("LANG-CF-LOOP-COND-TRANSFER-DEPTH-WHILE-VARIABLE-ZERO-NONE");
constexpr const TCHAR* ZeroCaseId = TEXT("LANG-CF-LOOP-COND-TRANSFER-DEPTH-WHILE-VARIABLE-ZERO-NONE");

int32 IndependentLimit(const TCHAR* Count)
{
	if (FCString::Strcmp(Count, TEXT("ONE")) == 0)
	{
		return 1;
	}
	if (FCString::Strcmp(Count, TEXT("TWO")) == 0)
	{
		return 2;
	}
	return 0;
}

int32 IndependentBodyCalls(const TCHAR* Count, const TCHAR* Transfer)
{
	const int32 Limit = IndependentLimit(Count);
	if (FCString::Strcmp(Transfer, TEXT("BREAK")) == 0)
	{
		return FMath::Min(Limit, 1);
	}
	if (FCString::Strcmp(Transfer, TEXT("RETURN")) == 0)
	{
		return Limit > 0 ? 1 : 0;
	}
	return Limit;
}

int32 IndependentConditionCalls(const TCHAR* Loop, const TCHAR* Condition, const TCHAR* Count, const TCHAR* Transfer)
{
	if (FCString::Strcmp(Condition, TEXT("SIDE_EFFECT")) != 0)
	{
		return 0;
	}
	const int32 BodyCalls = IndependentBodyCalls(Count, Transfer);
	const bool bBreakOrReturn = FCString::Strcmp(Transfer, TEXT("BREAK")) == 0
		|| FCString::Strcmp(Transfer, TEXT("RETURN")) == 0;
	if (FCString::Strcmp(Loop, TEXT("DO_WHILE")) == 0)
	{
		return bBreakOrReturn ? 0 : BodyCalls;
	}
	return 1 + (bBreakOrReturn ? 0 : BodyCalls);
}

int32 IndependentExpected(const TCHAR* Loop, const TCHAR* Condition, const TCHAR* Count, const TCHAR* Transfer)
{
	const int32 BodyCalls = IndependentBodyCalls(Count, Transfer);
	const int32 ConditionCalls = IndependentConditionCalls(Loop, Condition, Count, Transfer);
	if (FCString::Strcmp(Transfer, TEXT("RETURN")) == 0 && IndependentLimit(Count) > 0)
	{
		return 1000 + BodyCalls * 100 + ConditionCalls;
	}
	return BodyCalls * 100 + ConditionCalls;
}

FString IndependentConditionNeedle(const TCHAR* Condition)
{
	if (FCString::Strcmp(Condition, TEXT("COMPARISON")) == 0)
	{
		return TEXT("Index < Limit");
	}
	if (FCString::Strcmp(Condition, TEXT("LOGICAL")) == 0)
	{
		return TEXT("(Index < Limit) && (Limit >= 0)");
	}
	if (FCString::Strcmp(Condition, TEXT("NEGATED")) == 0)
	{
		return TEXT("!(Index >= Limit)");
	}
	if (FCString::Strcmp(Condition, TEXT("SIDE_EFFECT")) == 0)
	{
		return TEXT("CheckCondition(Index, Limit)");
	}
	return TEXT("KeepGoing");
}

FString IndependentHeaderNeedle(const TCHAR* Loop, const FString& ConditionNeedle)
{
	if (FCString::Strcmp(Loop, TEXT("DO_WHILE")) == 0)
	{
		return TEXT("do");
	}
	if (FCString::Strcmp(Loop, TEXT("FOR")) == 0)
	{
		return FString::Printf(TEXT("for (; %s; ++Index)"), *ConditionNeedle);
	}
	return FString::Printf(TEXT("while (%s)"), *ConditionNeedle);
}

TArray<FLoopCondExpectedCell> BuildExpectedCells()
{
	static const TCHAR* const kLoops[] = {TEXT("WHILE"), TEXT("DO_WHILE"), TEXT("FOR")};
	static const TCHAR* const kConditions[] = {
		TEXT("VARIABLE"), TEXT("COMPARISON"), TEXT("LOGICAL"), TEXT("NEGATED"), TEXT("SIDE_EFFECT")};
	static const TCHAR* const kCounts[] = {TEXT("ZERO"), TEXT("ONE"), TEXT("TWO")};
	static const TCHAR* const kTransfers[] = {TEXT("NONE"), TEXT("BREAK"), TEXT("CONTINUE"), TEXT("RETURN")};

	TArray<FLoopCondExpectedCell> Cells;
	Cells.SetNum(180);
	for (int32 Index = 0; Index < 180; ++Index)
	{
		const TCHAR* const Loop = kLoops[Index / 60];
		const TCHAR* const Condition = kConditions[(Index / 12) % 5];
		const TCHAR* const Count = kCounts[(Index / 4) % 3];
		const TCHAR* const Transfer = kTransfers[Index % 4];
		FLoopCondExpectedCell& Cell = Cells[Index];
		Cell.CaseId = FString::Printf(
			TEXT("LANG-CF-LOOP-COND-TRANSFER-DEPTH-%s-%s-%s-%s"),
			Loop,
			Condition,
			Count,
			Transfer);
		Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
		Cell.ConditionNeedle = IndependentConditionNeedle(Condition);
		Cell.HeaderNeedle = IndependentHeaderNeedle(Loop, Cell.ConditionNeedle);
		Cell.Expected = IndependentExpected(Loop, Condition, Count, Transfer);
	}
	return Cells;
}

FString ExpectedDeclaration(const FLoopCondExpectedCell& Cell)
{
	return FString::Printf(TEXT("int %s()"), *Cell.EntryName);
}
}

TEST_CLASS_WITH_FLAGS(
	LoopCondTransferGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using LoopCondTransferGeneratorTest::BuildExpectedCells;
		using LoopCondTransferGeneratorTest::ExpectedDeclaration;
		using LoopCondTransferGeneratorTest::FLoopCondExpectedCell;
		using LoopCondTransferGeneratorTest::ProbeCaseId;
		using LoopCondTransferGeneratorTest::ZeroCaseId;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::ELoopCondCount;
		using AngelscriptTest::Generate::ELoopCondExpr;
		using AngelscriptTest::Generate::ELoopCondLoop;
		using AngelscriptTest::Generate::ELoopCondTransfer;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FLoopCondTransferGenerator;
		using AngelscriptTest::Generate::FLoopCondTransferParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FLoopCondExpectedCell> ExpectedCells = BuildExpectedCells();
		FLoopCondTransferGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CF-LOOP-COND-TRANSFER-DEPTH")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangCfLoopCondTransferDepthWhileVariableZeroNone")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("LoopCondTransferGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("LoopCondTransferGenerator dump path=%s cases=180 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(180, CaseCount));
		ASSERT_THAT(AreEqual(180, Cases.Num()));
		ASSERT_THAT(AreEqual(180, ExpectedCells.Num()));

		int32 RejectCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FLoopCondExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(ExpectedDeclaration(Expected), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedException.IsEmpty()));
			ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("GetConditionCalls"))));
			ASSERT_THAT(IsFalse(Cases[Index].bLimitedObservation));
			if (Cases[Index].Outcome == EGeneratedCaseOutcome::CompileReject)
			{
				++RejectCount;
			}
		}
		ASSERT_THAT(AreEqual(0, RejectCount));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		const FString OwnedAgain = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(IsTrue(OwnedCase.Len() > 0));
		ASSERT_THAT(AreEqual(OwnedCase, OwnedAgain));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangCfLoopCondTransferDepthWhileVariableZeroNone()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangCfLoopCondTransferDepthWhileVariableZeroNone()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));

		FLoopCondTransferParams DefaultParams;
		const FString DefaultSingle = Generator.BuildLoopCondSource(DefaultParams);
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsFalse(DefaultSingle.Contains(TEXT("int EntryLang"))));

		FLoopCondTransferParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		const FString ProbeSource = Generator.BuildLoopCondSource(ProbeParams);
		ASSERT_THAT(IsTrue(ProbeSource.Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(ProbeSource.Contains(TEXT("int Entry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/LoopCondTransferGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FLoopCondExpectedCell& Expected : ExpectedCells)
		{
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.HeaderNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.ConditionNeedle)));
			ASSERT_THAT(IsTrue(
				GeneratedBody.Contains(TEXT("return BodyCalls * 100 + GetConditionCalls();"))
				|| GeneratedBody.Contains(TEXT("return 1000 + BodyCalls * 100 + GetConditionCalls();"))
				|| GeneratedBody.Contains(TEXT("return 0;"))));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FLoopCondTransferParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildLoopCondSource(InvalidName).IsEmpty()));
		FLoopCondTransferParams InvalidEnum = DefaultParams;
		InvalidEnum.Loop = static_cast<ELoopCondLoop>(3);
		ASSERT_THAT(IsTrue(Generator.BuildLoopCondSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE")).IsEmpty()));
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
		ASSERT_THAT(AreEqual(ExpectedCells[0].Expected, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(OutputFile.EndsWith(TEXT("GeneratedCases/LoopCondTransferGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\LoopCondTransferGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(180, CountOccurrences(Dump, TEXT("// Case: LANG-CF-LOOP-COND-TRANSFER-DEPTH-"))));
		for (const FLoopCondExpectedCell& Expected : ExpectedCells)
		{
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
			TEXT("C:/no-such-root/LoopCondTransferGenerator.as"), Dump, ProbeError)));
		ASSERT_THAT(IsTrue(ProbeError.Len() > 0));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("LoopCondTransferGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
