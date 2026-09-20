#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestLoopDepthGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace LoopDepthGeneratorTest
{
struct FLoopDepthExpectedCell
{
	FString CaseId;
	FString EntryName;
	FString LoopHeaderNeedle;
	int32 Packed = 0;
	int32 Limit = 0;
};

constexpr const TCHAR* ProbeCaseId = TEXT("LANG-CF-LOOP-DEPTH-WHILE-ZERO-NONE");
constexpr const TCHAR* ZeroCaseId = TEXT("LANG-CF-LOOP-DEPTH-DO_WHILE-ZERO-NONE");
constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-CONDITION-IF-BOOL_LITERAL-TRUE");

int32 CountLimit(const TCHAR* Count)
{
	if (FCString::Strcmp(Count, TEXT("ONE")) == 0)
	{
		return 1;
	}
	if (FCString::Strcmp(Count, TEXT("TWO")) == 0)
	{
		return 2;
	}
	if (FCString::Strcmp(Count, TEXT("MANY")) == 0)
	{
		return 4;
	}
	return 0;
}

int32 IndependentPacked(const TCHAR* Loop, const TCHAR* Count, const TCHAR* Transfer)
{
	const int32 Limit = CountLimit(Count);
	const bool bDoWhile = FCString::Strcmp(Loop, TEXT("DO_WHILE")) == 0;
	int32 BodyCount = 0;
	int32 ConditionCount = 0;
	int32 IncrementCount = 0;
	if (Limit == 0)
	{
		ConditionCount = bDoWhile ? 0 : 1;
	}
	else if (FCString::Strcmp(Transfer, TEXT("BREAK")) == 0 || FCString::Strcmp(Transfer, TEXT("RETURN")) == 0)
	{
		BodyCount = 1;
		ConditionCount = bDoWhile ? 0 : 1;
	}
	else
	{
		BodyCount = Limit;
		ConditionCount = bDoWhile ? Limit : Limit + 1;
		IncrementCount = Limit;
	}
	return BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

FString IndependentLoopHeaderNeedle(const TCHAR* Loop)
{
	if (FCString::Strcmp(Loop, TEXT("WHILE")) == 0)
	{
		return TEXT("while (CountCondition(Index, Limit, ConditionCount))");
	}
	if (FCString::Strcmp(Loop, TEXT("DO_WHILE")) == 0)
	{
		return TEXT("while (CountCondition(Index, Limit, ConditionCount));");
	}
	return TEXT("for (Index = 0; CountCondition(Index, Limit, ConditionCount); CountIncrement(Index, IncrementCount))");
}

TArray<FLoopDepthExpectedCell> BuildExpectedCells()
{
	static const TCHAR* const kLoops[] = {TEXT("WHILE"), TEXT("DO_WHILE"), TEXT("FOR")};
	static const TCHAR* const kCounts[] = {TEXT("ZERO"), TEXT("ONE"), TEXT("TWO"), TEXT("MANY")};
	static const TCHAR* const kTransfers[] = {TEXT("NONE"), TEXT("BREAK"), TEXT("CONTINUE"), TEXT("RETURN")};

	TArray<FLoopDepthExpectedCell> Cells;
	Cells.SetNum(48);
	for (int32 Index = 0; Index < 48; ++Index)
	{
		const TCHAR* const Loop = kLoops[Index / 16];
		const TCHAR* const Count = kCounts[(Index / 4) % 4];
		const TCHAR* const Transfer = kTransfers[Index % 4];
		FLoopDepthExpectedCell& Cell = Cells[Index];
		Cell.CaseId = FString::Printf(TEXT("LANG-CF-LOOP-DEPTH-%s-%s-%s"), Loop, Count, Transfer);
		Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
		Cell.LoopHeaderNeedle = IndependentLoopHeaderNeedle(Loop);
		Cell.Packed = IndependentPacked(Loop, Count, Transfer);
		Cell.Limit = CountLimit(Count);
	}
	return Cells;
}
}

TEST_CLASS_WITH_FLAGS(
	LoopDepthGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using LoopDepthGeneratorTest::BuildExpectedCells;
		using LoopDepthGeneratorTest::FLoopDepthExpectedCell;
		using LoopDepthGeneratorTest::ForeignCaseId;
		using LoopDepthGeneratorTest::ProbeCaseId;
		using LoopDepthGeneratorTest::ZeroCaseId;
		using AngelscriptTest::Generate::ELoopDepthCount;
		using AngelscriptTest::Generate::ELoopDepthLoop;
		using AngelscriptTest::Generate::ELoopDepthTransfer;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FLoopDepthGenerator;
		using AngelscriptTest::Generate::FLoopDepthParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FLoopDepthExpectedCell> ExpectedCells = BuildExpectedCells();
		FLoopDepthGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CF-LOOP-DEPTH")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangCfLoopDepthWhileZeroNone")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile = AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("LoopDepthGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("LoopDepthGenerator dump path=%s cases=48 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(48, CaseCount));
		ASSERT_THAT(AreEqual(48, Cases.Num()));
		ASSERT_THAT(AreEqual(48, ExpectedCells.Num()));

		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FLoopDepthExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(
				FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Packed, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(Expected.Packed, Generator.GetExpected(Expected.CaseId)));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedException.IsEmpty()));
			ASSERT_THAT(IsFalse(Cases[Index].bLimitedObservation));
		}

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		const FString OwnedAgain = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(IsTrue(OwnedCase.Len() > 0));
		ASSERT_THAT(AreEqual(OwnedCase, OwnedAgain));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangCfLoopDepthWhileZeroNone()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangCfLoopDepthWhileZeroNone()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));

		FLoopDepthParams DefaultParams;
		const FString DefaultSingle = Generator.BuildLoopSource(DefaultParams);
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("bool CountCondition(int Index, int Limit, int&inout Counter)"))));
		ASSERT_THAT(IsFalse(DefaultSingle.Contains(TEXT("int EntryLang"))));

		FLoopDepthParams ProbeParams;
		ProbeParams.Loop = ELoopDepthLoop::For;
		ProbeParams.Count = ELoopDepthCount::Two;
		ProbeParams.Transfer = ELoopDepthTransfer::None;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		const FString ProbeSource = Generator.BuildLoopSource(ProbeParams);
		ASSERT_THAT(IsTrue(ProbeSource.Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(ProbeSource.Contains(TEXT("int Entry()"))));

		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("bool CountCondition"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int CountIncrement"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Dump, TEXT("bool CountCondition"))));

		const FString Representative = LoadGold(TEXT("LanguageGenerators/LoopDepthGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		int32 GoldMatches = 0;
		for (const FLoopDepthExpectedCell& Expected : ExpectedCells)
		{
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(FString::Printf(TEXT("int Limit = %d;"), Expected.Limit))));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.LoopHeaderNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(
				TEXT("return BodyCount * 100 + ConditionCount * 10 + IncrementCount;"))));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				++GoldMatches;
			}
		}
		ASSERT_THAT(AreEqual(11, GoldMatches));

		FLoopDepthParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildLoopSource(InvalidName).IsEmpty()));
		FLoopDepthParams InvalidEnum = DefaultParams;
		InvalidEnum.Loop = static_cast<ELoopDepthLoop>(3);
		ASSERT_THAT(IsTrue(Generator.BuildLoopSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

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
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(ZeroCaseId)));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(ExpectedCells[0].Packed, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(OutputFile.EndsWith(TEXT("GeneratedCases/LoopDepthGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\LoopDepthGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(48, CountOccurrences(Dump, TEXT("// Case: LANG-CF-LOOP-DEPTH-"))));
		for (const FLoopDepthExpectedCell& Expected : ExpectedCells)
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
			TEXT("C:/no-such-root/LoopDepthGenerator.as"), Dump, ProbeError)));
		ASSERT_THAT(IsTrue(ProbeError.Len() > 0));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("LoopDepthGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
