#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestLiveLocalCleanupGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace LiveLocalCleanupGeneratorTest
{
struct FLiveLocalExpectedCell
{
	FString CaseId;
	FString EntryName;
	FString ShapeNeedle;
	FString LocalNeedle;
	FString ExitNeedle;
	int32 Expected = 0;
	bool bFault = false;
	bool bHasLevel2 = false;
	bool bHasLevel3 = false;
};

constexpr const TCHAR* ProbeCaseId = TEXT("LANG-CF-LIVE-LOCAL-CLEANUP-LOOP-NORMAL-ONE-ONE-LF");
constexpr const TCHAR* FaultCaseId = TEXT("LANG-CF-LIVE-LOCAL-CLEANUP-LOOP-EXCEPTION-ONE-ONE-LF");

struct FIndependentTrace
{
	int32 Value = 0;
	bool bReturned = false;
};

FIndependentTrace SimulateNestedScopes(const int32 CurrentLevel, const int32 Depth, const TCHAR* Exit)
{
	FIndependentTrace Result;
	Result.Value += (CurrentLevel + 1) * 100;
	if (CurrentLevel + 1 == Depth)
	{
		if (FCString::Strcmp(Exit, TEXT("RETURN")) == 0)
		{
			Result.Value += 13;
			Result.bReturned = true;
			return Result;
		}
		if (FCString::Strcmp(Exit, TEXT("BREAK")) == 0)
		{
			Result.Value += 7;
		}
		else if (FCString::Strcmp(Exit, TEXT("CONTINUE")) == 0)
		{
			Result.Value += 11;
		}
		else if (FCString::Strcmp(Exit, TEXT("EXCEPTION")) == 0)
		{
			Result.Value += 17;
			return Result;
		}
	}
	else
	{
		const FIndependentTrace Nested = SimulateNestedScopes(CurrentLevel + 1, Depth, Exit);
		Result.Value += Nested.Value;
		if (Nested.bReturned)
		{
			Result.bReturned = true;
			return Result;
		}
	}
	Result.Value += (CurrentLevel + 1) * 1000;
	return Result;
}

int32 IndependentExpected(const TCHAR* Exit, const int32 Depth)
{
	if (FCString::Strcmp(Exit, TEXT("EXCEPTION")) == 0)
	{
		return 0;
	}
	const FIndependentTrace Nested = SimulateNestedScopes(0, Depth, Exit);
	return Nested.bReturned ? Nested.Value : Nested.Value + 97;
}

FString IndependentShapeNeedle(const TCHAR* Scope)
{
	if (FCString::Strcmp(Scope, TEXT("BRANCH_LOOP")) == 0)
	{
		return TEXT("if (true)");
	}
	if (FCString::Strcmp(Scope, TEXT("NESTED_LOOP")) == 0)
	{
		return TEXT("for (int ShapeLoop = 0; ShapeLoop < 1; ++ShapeLoop)");
	}
	if (FCString::Strcmp(Scope, TEXT("SWITCH_LOOP")) == 0)
	{
		return TEXT("switch (0)");
	}
	return TEXT("for (int Level1 = 0; Level1 < 1; ++Level1)");
}

FString IndependentExitNeedle(const TCHAR* Exit)
{
	if (FCString::Strcmp(Exit, TEXT("BREAK")) == 0)
	{
		return TEXT("Trace += 7;");
	}
	if (FCString::Strcmp(Exit, TEXT("CONTINUE")) == 0)
	{
		return TEXT("Trace += 11;");
	}
	if (FCString::Strcmp(Exit, TEXT("RETURN")) == 0)
	{
		return TEXT("Trace += 13;");
	}
	if (FCString::Strcmp(Exit, TEXT("EXCEPTION")) == 0)
	{
		return TEXT("Trace += 1 / ExceptionDivisor;");
	}
	return FString();
}

TArray<FLiveLocalExpectedCell> BuildExpectedCells()
{
	static const TCHAR* const kScopes[] = {TEXT("LOOP"), TEXT("BRANCH_LOOP"), TEXT("NESTED_LOOP"), TEXT("SWITCH_LOOP")};
	static const TCHAR* const kExits[] = {TEXT("NORMAL"), TEXT("BREAK"), TEXT("CONTINUE"), TEXT("RETURN"), TEXT("EXCEPTION")};
	static const TCHAR* const kDepths[] = {TEXT("ONE"), TEXT("TWO"), TEXT("THREE")};
	static const TCHAR* const kCounts[] = {TEXT("ONE"), TEXT("TWO")};

	TArray<FLiveLocalExpectedCell> Cells;
	Cells.SetNum(120);
	for (int32 Index = 0; Index < 120; ++Index)
	{
		const TCHAR* const Scope = kScopes[Index / 30];
		const TCHAR* const Exit = kExits[(Index / 6) % 5];
		const TCHAR* const Depth = kDepths[(Index / 2) % 3];
		const TCHAR* const Count = kCounts[Index % 2];
		const int32 DepthValue = ((Index / 2) % 3) + 1;
		FLiveLocalExpectedCell& Cell = Cells[Index];
		Cell.CaseId = FString::Printf(
			TEXT("LANG-CF-LIVE-LOCAL-CLEANUP-%s-%s-%s-%s-LF"),
			Scope,
			Exit,
			Depth,
			Count);
		Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
		Cell.ShapeNeedle = IndependentShapeNeedle(Scope);
		Cell.LocalNeedle = FCString::Strcmp(Count, TEXT("TWO")) == 0
			? TEXT("FScopedLifetimeProbe Local_1_2(102);")
			: TEXT("FScopedLifetimeProbe Local_1_1(101);");
		Cell.ExitNeedle = IndependentExitNeedle(Exit);
		Cell.Expected = IndependentExpected(Exit, DepthValue);
		Cell.bFault = FCString::Strcmp(Exit, TEXT("EXCEPTION")) == 0;
		Cell.bHasLevel2 = DepthValue >= 2;
		Cell.bHasLevel3 = DepthValue >= 3;
	}
	return Cells;
}

FString ExpectedDeclaration(const FLiveLocalExpectedCell& Cell)
{
	return FString::Printf(TEXT("int %s()"), *Cell.EntryName);
}
}

TEST_CLASS_WITH_FLAGS(
	LiveLocalCleanupGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using LiveLocalCleanupGeneratorTest::BuildExpectedCells;
		using LiveLocalCleanupGeneratorTest::ExpectedDeclaration;
		using LiveLocalCleanupGeneratorTest::FaultCaseId;
		using LiveLocalCleanupGeneratorTest::FLiveLocalExpectedCell;
		using LiveLocalCleanupGeneratorTest::ProbeCaseId;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::ELiveLocalCount;
		using AngelscriptTest::Generate::ELiveLocalDepth;
		using AngelscriptTest::Generate::ELiveLocalExit;
		using AngelscriptTest::Generate::ELiveLocalLineEnding;
		using AngelscriptTest::Generate::ELiveLocalScope;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FLiveLocalCleanupGenerator;
		using AngelscriptTest::Generate::FLiveLocalCleanupParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FLiveLocalExpectedCell> ExpectedCells = BuildExpectedCells();
		FLiveLocalCleanupGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CF-LIVE-LOCAL-CLEANUP")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangCfLiveLocalCleanupLoopNormalOneOneLf")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("LiveLocalCleanupGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("LiveLocalCleanupGenerator dump path=%s cases=120 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(120, CaseCount));
		ASSERT_THAT(AreEqual(120, Cases.Num()));
		ASSERT_THAT(AreEqual(120, ExpectedCells.Num()));

		int32 FaultCount = 0;
		int32 RejectCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FLiveLocalExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(ExpectedDeclaration(Expected), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
			ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("BeginNativeScriptLifecycle"))));
			ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("EndNativeScriptLifecycle"))));
			ASSERT_THAT(IsFalse(Cases[Index].bLimitedObservation));
			if (Expected.bFault)
			{
				++FaultCount;
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")), Cases[Index].ExpectedException));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
			}
			else
			{
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedException.IsEmpty()));
			}
			if (Cases[Index].Outcome == EGeneratedCaseOutcome::CompileReject)
			{
				++RejectCount;
			}
		}
		ASSERT_THAT(AreEqual(24, FaultCount));
		ASSERT_THAT(AreEqual(0, RejectCount));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		const FString OwnedAgain = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(IsTrue(OwnedCase.Len() > 0));
		ASSERT_THAT(AreEqual(OwnedCase, OwnedAgain));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangCfLiveLocalCleanupLoopNormalOneOneLf()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangCfLiveLocalCleanupLoopNormalOneOneLf()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));
		ASSERT_THAT(IsFalse(OwnedCase.Contains(TEXT("\r"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("struct FScopedLifetimeProbe"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int Recovery()"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Dump, TEXT("struct FScopedLifetimeProbe"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Dump, TEXT("int Recovery()"))));

		FLiveLocalCleanupParams DefaultParams;
		const FString DefaultSingle = Generator.BuildLiveLocalSource(DefaultParams);
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsFalse(DefaultSingle.Contains(TEXT("int EntryLang"))));
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Recovery()"))));

		FLiveLocalCleanupParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		const FString ProbeSource = Generator.BuildLiveLocalSource(ProbeParams);
		ASSERT_THAT(IsTrue(ProbeSource.Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(ProbeSource.Contains(TEXT("int Entry()"))));

		FLiveLocalCleanupParams CrlfParams = DefaultParams;
		CrlfParams.LineEnding = ELiveLocalLineEnding::Crlf;
		const FString CrlfSource = Generator.BuildLiveLocalSource(CrlfParams);
		ASSERT_THAT(IsTrue(CrlfSource.Contains(TEXT("\r\n"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/LiveLocalCleanupGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FLiveLocalExpectedCell& Expected : ExpectedCells)
		{
			const FString CaseSource = Generator.BuildCaseSource(Expected.CaseId);
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.ShapeNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.LocalNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("return Trace + 97;"))));
			if (Expected.ExitNeedle.Len() > 0)
			{
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.ExitNeedle)));
			}
			else
			{
				ASSERT_THAT(IsFalse(GeneratedBody.Contains(TEXT("Trace += 7;"))));
				ASSERT_THAT(IsFalse(GeneratedBody.Contains(TEXT("Trace += 11;"))));
				ASSERT_THAT(IsFalse(GeneratedBody.Contains(TEXT("Trace += 13;"))));
				ASSERT_THAT(IsFalse(GeneratedBody.Contains(TEXT("ExceptionDivisor"))));
			}
			if (Expected.bHasLevel2)
			{
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("for (int Level2 = 0; Level2 < 1; ++Level2)"))));
			}
			else
			{
				ASSERT_THAT(IsFalse(GeneratedBody.Contains(TEXT("for (int Level2 = 0;"))));
			}
			if (Expected.bHasLevel3)
			{
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("for (int Level3 = 0; Level3 < 1; ++Level3)"))));
			}
			else
			{
				ASSERT_THAT(IsFalse(GeneratedBody.Contains(TEXT("for (int Level3 = 0;"))));
			}
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FLiveLocalCleanupParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildLiveLocalSource(InvalidName).IsEmpty()));
		FLiveLocalCleanupParams InvalidScope = DefaultParams;
		InvalidScope.Scope = static_cast<ELiveLocalScope>(4);
		ASSERT_THAT(IsTrue(Generator.BuildLiveLocalSource(InvalidScope).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(FaultCaseId)));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(ExpectedCells[0].Expected, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(OutputFile.EndsWith(TEXT("GeneratedCases/LiveLocalCleanupGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\LiveLocalCleanupGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(120, CountOccurrences(Dump, TEXT("// Case: LANG-CF-LIVE-LOCAL-CLEANUP-"))));
		ASSERT_THAT(AreEqual(24, CountOccurrences(Dump, TEXT("// Outcome: RuntimeException"))));
		for (const FLiveLocalExpectedCell& Expected : ExpectedCells)
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
			TEXT("C:/no-such-root/LiveLocalCleanupGenerator.as"), Dump, ProbeError)));
		ASSERT_THAT(IsTrue(ProbeError.Len() > 0));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("LiveLocalCleanupGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
