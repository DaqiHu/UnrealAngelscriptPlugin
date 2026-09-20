#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestNestedTargetGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace NestedTargetGeneratorTest
{
struct FNestedExpectedCell
{
	FString CaseId;
	FString EntryName;
	FString ShapeNeedle;
	FString TransferNeedle;
	int32 Expected = 0;
};

constexpr const TCHAR* ProbeCaseId = TEXT("LANG-CF-NESTED-TARGETS-NESTED_LOOP-BREAK-INNER");

int32 IndependentTrace(const TCHAR* Nesting, const TCHAR* Transfer, const TCHAR* Target)
{
	int32 Trace = 0;
	const bool bInner = FCString::Strcmp(Target, TEXT("INNER")) == 0;
	for (int32 Outer = 0; Outer < 2; ++Outer)
	{
		Trace += 1;
		if (FCString::Strcmp(Nesting, TEXT("NESTED_LOOP")) == 0)
		{
			for (int32 Inner = 0; Inner < 2; ++Inner)
			{
				Trace += 10;
				if (bInner)
				{
					if (FCString::Strcmp(Transfer, TEXT("BREAK")) == 0)
					{
						break;
					}
					if (FCString::Strcmp(Transfer, TEXT("CONTINUE")) == 0)
					{
						continue;
					}
					return Trace;
				}
			}
		}
		else if (FCString::Strcmp(Nesting, TEXT("BRANCH_LOOP")) == 0)
		{
			Trace += 2;
			for (int32 Inner = 0; Inner < 2; ++Inner)
			{
				Trace += 10;
				if (bInner)
				{
					if (FCString::Strcmp(Transfer, TEXT("BREAK")) == 0)
					{
						break;
					}
					if (FCString::Strcmp(Transfer, TEXT("CONTINUE")) == 0)
					{
						continue;
					}
					return Trace;
				}
			}
		}
		else
		{
			for (int32 Middle = 0; Middle < 2; ++Middle)
			{
				Trace += 10;
				for (int32 Inner = 0; Inner < 2; ++Inner)
				{
					Trace += 100;
					if (bInner)
					{
						if (FCString::Strcmp(Transfer, TEXT("BREAK")) == 0)
						{
							break;
						}
						if (FCString::Strcmp(Transfer, TEXT("CONTINUE")) == 0)
						{
							continue;
						}
						return Trace;
					}
				}
			}
		}
		Trace += 1000;
		if (!bInner)
		{
			if (FCString::Strcmp(Transfer, TEXT("BREAK")) == 0)
			{
				break;
			}
			if (FCString::Strcmp(Transfer, TEXT("CONTINUE")) == 0)
			{
				continue;
			}
			return Trace;
		}
		Trace += 10000;
	}
	return Trace;
}

TArray<FNestedExpectedCell> BuildExpectedCells()
{
	static const TCHAR* const kNestings[] = {TEXT("NESTED_LOOP"), TEXT("BRANCH_LOOP"), TEXT("THREE_LEVEL")};
	static const TCHAR* const kTransfers[] = {TEXT("BREAK"), TEXT("CONTINUE"), TEXT("RETURN")};
	static const TCHAR* const kTargets[] = {TEXT("INNER"), TEXT("OUTER")};
	TArray<FNestedExpectedCell> Cells;
	Cells.SetNum(18);
	for (int32 Index = 0; Index < 18; ++Index)
	{
		const TCHAR* const Nesting = kNestings[Index / 6];
		const TCHAR* const Transfer = kTransfers[(Index / 2) % 3];
		const TCHAR* const Target = kTargets[Index % 2];
		FNestedExpectedCell& Cell = Cells[Index];
		Cell.CaseId = FString::Printf(TEXT("LANG-CF-NESTED-TARGETS-%s-%s-%s"), Nesting, Transfer, Target);
		Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
		if (FCString::Strcmp(Nesting, TEXT("BRANCH_LOOP")) == 0)
		{
			Cell.ShapeNeedle = TEXT("if (Outer >= 0)");
		}
		else if (FCString::Strcmp(Nesting, TEXT("THREE_LEVEL")) == 0)
		{
			Cell.ShapeNeedle = TEXT("for (int Middle = 0; Middle < 2; ++Middle)");
		}
		else
		{
			Cell.ShapeNeedle = TEXT("for (int Inner = 0; Inner < 2; ++Inner)");
		}
		if (FCString::Strcmp(Transfer, TEXT("RETURN")) == 0)
		{
			Cell.TransferNeedle = TEXT("return Trace;");
		}
		else if (FCString::Strcmp(Transfer, TEXT("CONTINUE")) == 0)
		{
			Cell.TransferNeedle = TEXT("continue;");
		}
		else
		{
			Cell.TransferNeedle = TEXT("break;");
		}
		Cell.Expected = IndependentTrace(Nesting, Transfer, Target);
	}
	return Cells;
}
}

TEST_CLASS_WITH_FLAGS(
	NestedTargetGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using NestedTargetGeneratorTest::BuildExpectedCells;
		using NestedTargetGeneratorTest::FNestedExpectedCell;
		using NestedTargetGeneratorTest::ProbeCaseId;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::ENestedTargetNesting;
		using AngelscriptTest::Generate::ENestedTargetSite;
		using AngelscriptTest::Generate::ENestedTargetTransfer;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FNestedTargetGenerator;
		using AngelscriptTest::Generate::FNestedTargetParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FNestedExpectedCell> ExpectedCells = BuildExpectedCells();
		FNestedTargetGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CF-NESTED-TARGETS")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangCfNestedTargetsNestedLoopBreakInner")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("NestedTargetGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("NestedTargetGenerator dump path=%s cases=18 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(18, CaseCount));
		ASSERT_THAT(AreEqual(18, Cases.Num()));
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FNestedExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
		}

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangCfNestedTargetsNestedLoopBreakInner()"))));

		FNestedTargetParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildNestedSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FNestedTargetParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildNestedSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/NestedTargetGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FNestedExpectedCell& Expected : ExpectedCells)
		{
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.ShapeNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.TransferNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("return Trace;"))));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FNestedTargetParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildNestedSource(InvalidName).IsEmpty()));
		FNestedTargetParams InvalidEnum = DefaultParams;
		InvalidEnum.Nesting = static_cast<ENestedTargetNesting>(3);
		ASSERT_THAT(IsTrue(Generator.BuildNestedSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(AreEqual(ExpectedCells[0].Expected, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(18, CountOccurrences(Dump, TEXT("// Case: LANG-CF-NESTED-TARGETS-"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/NestedTargetGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("NestedTargetGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
