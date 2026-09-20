#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestStatementTransferGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace StatementTransferGeneratorTest
{
constexpr int32 StatementAxisCount = 8;
constexpr int32 CountAxisCount = 4;
constexpr int32 TransferAxisCount = 8;
constexpr int32 NestingAxisCount = 6;
constexpr int32 TotalCells = StatementAxisCount * CountAxisCount * TransferAxisCount * NestingAxisCount;
constexpr int32 FaultCells = StatementAxisCount * CountAxisCount * NestingAxisCount;
constexpr int32 NormalCells = TotalCells - FaultCells;

constexpr const TCHAR* CatalogExample = TEXT("LANG-CF-STATEMENT-COUNT-TRANSFER-IF-ZERO-NONE-NONE");

struct FStatementExpectedCell
{
	FString CaseId;
	FString EntryName;
	FString StatementNeedle;
	int32 ExpectedReturn = 0;
	bool bFault = false;
};

const TCHAR* const kStatements[StatementAxisCount] = {
	TEXT("IF"), TEXT("IF_ELSE"), TEXT("ELSE_IF"), TEXT("WHILE"),
	TEXT("DO_WHILE"), TEXT("FOR"), TEXT("SWITCH"), TEXT("NESTED_BLOCK")};
const TCHAR* const kCounts[CountAxisCount] = {
	TEXT("ZERO"), TEXT("ONE"), TEXT("TWO"), TEXT("MANY")};
const TCHAR* const kTransfers[TransferAxisCount] = {
	TEXT("NONE"), TEXT("BREAK_LOOP"), TEXT("BREAK_SWITCH"), TEXT("CONTINUE"),
	TEXT("EARLY_RETURN"), TEXT("NESTED_RETURN"), TEXT("FALLTHROUGH"), TEXT("EXCEPTION")};
const TCHAR* const kNestings[NestingAxisCount] = {
	TEXT("NONE"), TEXT("SAME_KIND"), TEXT("MIXED_LOOP"), TEXT("LOOP_SWITCH"),
	TEXT("BRANCH_LOOP"), TEXT("THREE_LEVEL")};

const int32 kStatementWeight[StatementAxisCount] = {1, 2, 3, 4, 5, 6, 7, 8};
const int32 kCountLimit[CountAxisCount] = {0, 1, 2, 4};
const int32 kNestingWeight[NestingAxisCount] = {0, 10, 20, 30, 40, 50};

const TCHAR* const kStatementNeedle[StatementAxisCount] = {
	TEXT("if (Index >= 0)"),
	TEXT("if ((Index & 1) == 0)"),
	TEXT("else if (Index >= 0)"),
	TEXT("Trace += 4;"),
	TEXT("Trace += 5;"),
	TEXT("Trace += 6;"),
	TEXT("switch (Index & 1)"),
	TEXT("Trace += 8;")};

int32 IndependentExpected(const int32 StatementIndex, const int32 CountIndex, const int32 TransferIndex, const int32 NestingIndex)
{
	if (TransferIndex == 7)
	{
		return 0; // EXCEPTION: fault, no integer expectation.
	}
	const int32 Limit = kCountLimit[CountIndex];
	if (Limit == 0)
	{
		return 0;
	}
	const int32 PerIteration = kStatementWeight[StatementIndex] + kNestingWeight[NestingIndex];
	switch (TransferIndex)
	{
	case 1: // BREAK_LOOP
		return PerIteration + 10;
	case 4: // EARLY_RETURN
		return PerIteration + 20;
	case 5: // NESTED_RETURN
		return PerIteration + 30;
	case 2: // BREAK_SWITCH
	case 6: // FALLTHROUGH
		return Limit * (PerIteration + 11);
	case 3: // CONTINUE
		return Limit * (PerIteration + 10);
	default: // NONE
		return Limit * (PerIteration + 1);
	}
}

TArray<FStatementExpectedCell> BuildExpectedCells()
{
	TArray<FStatementExpectedCell> Cells;
	Cells.SetNum(TotalCells);
	int32 Index = 0;
	for (int32 StatementIndex = 0; StatementIndex < StatementAxisCount; ++StatementIndex)
	{
		for (int32 CountIndex = 0; CountIndex < CountAxisCount; ++CountIndex)
		{
			for (int32 TransferIndex = 0; TransferIndex < TransferAxisCount; ++TransferIndex)
			{
				for (int32 NestingIndex = 0; NestingIndex < NestingAxisCount; ++NestingIndex)
				{
					FStatementExpectedCell& Cell = Cells[Index];
					Cell.CaseId = FString::Printf(
						TEXT("LANG-CF-STATEMENT-COUNT-TRANSFER-%s-%s-%s-%s"),
						kStatements[StatementIndex],
						kCounts[CountIndex],
						kTransfers[TransferIndex],
						kNestings[NestingIndex]);
					Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
					Cell.StatementNeedle = kStatementNeedle[StatementIndex];
					Cell.bFault = TransferIndex == 7;
					Cell.ExpectedReturn = IndependentExpected(StatementIndex, CountIndex, TransferIndex, NestingIndex);
					++Index;
				}
			}
		}
	}
	return Cells;
}
}

TEST_CLASS_WITH_FLAGS(
	StatementTransferGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using StatementTransferGeneratorTest::BuildExpectedCells;
		using StatementTransferGeneratorTest::CatalogExample;
		using StatementTransferGeneratorTest::FaultCells;
		using StatementTransferGeneratorTest::FStatementExpectedCell;
		using StatementTransferGeneratorTest::NormalCells;
		using StatementTransferGeneratorTest::TotalCells;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EStatementCount;
		using AngelscriptTest::Generate::EStatementKind;
		using AngelscriptTest::Generate::EStatementNesting;
		using AngelscriptTest::Generate::EStatementTransfer;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FStatementTransferGenerator;
		using AngelscriptTest::Generate::FStatementTransferParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FStatementExpectedCell> ExpectedCells = BuildExpectedCells();
		FStatementTransferGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CF-STATEMENT-COUNT-TRANSFER")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangCfStatementCountTransferIfZeroNoneNone")),
			MakeCanonicalEntryName(CatalogExample)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile = AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("StatementTransferGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("StatementTransferGenerator dump path=%s cases=%d wrote=%s error=%s"),
			*OutputFile,
			TotalCells,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(TotalCells, CaseCount));
		ASSERT_THAT(AreEqual(TotalCells, Cases.Num()));
		ASSERT_THAT(AreEqual(TotalCells, ExpectedCells.Num()));

		// Complete ordered cell partition, category membership, and typed expectations.
		int32 FaultCount = 0;
		int32 NormalCount = 0;
		bool bFoundCatalogExample = false;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FStatementExpectedCell& Expected = ExpectedCells[Index];
			const FGeneratedCaseInfo& Info = Cases[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Info.CaseId));
			ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Info.EntryDeclaration));
			ASSERT_THAT(IsTrue(Info.Execution == EGeneratedCaseExecution::Standalone));
			ASSERT_THAT(IsFalse(Info.bLimitedObservation));
			if (Expected.CaseId == CatalogExample)
			{
				bFoundCatalogExample = true;
			}
			if (Expected.bFault)
			{
				++FaultCount;
				ASSERT_THAT(IsTrue(Info.Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(IsFalse(Info.ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")), Info.ExpectedException));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
			}
			else
			{
				++NormalCount;
				ASSERT_THAT(IsTrue(Info.Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Info.ExpectedReturn.IsSet()));
				ASSERT_THAT(IsTrue(Info.ExpectedException.IsEmpty()));
				ASSERT_THAT(AreEqual(Expected.ExpectedReturn, Info.ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.ExpectedReturn, Generator.GetExpected(Expected.CaseId)));
			}
		}
		ASSERT_THAT(AreEqual(FaultCells, FaultCount));
		ASSERT_THAT(AreEqual(NormalCells, NormalCount));
		ASSERT_THAT(IsTrue(bFoundCatalogExample));

		// Byte-stable aggregate and dump.
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));
		ASSERT_THAT(AreEqual(TotalCells, CountOccurrences(Dump, TEXT("// Case: LANG-CF-STATEMENT-COUNT-TRANSFER-"))));
		ASSERT_THAT(AreEqual(0, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(FaultCells, CountOccurrences(Dump, TEXT("// Outcome: RuntimeException"))));
		ASSERT_THAT(AreEqual(FaultCells, CountOccurrences(Dump, TEXT("// ExpectedException: Divide by zero"))));
		ASSERT_THAT(AreEqual(NormalCells, CountOccurrences(Dump, TEXT("// Outcome: ReturnValue"))));

		// Every cell: owned source construction and aggregate/dump body agreement.
		const FString Representative = LoadGold(TEXT("LanguageGenerators/StatementTransferGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		int32 GoldMatches = 0;
		for (const FStatementExpectedCell& Expected : ExpectedCells)
		{
			const FString CaseSource = Generator.BuildCaseSource(Expected.CaseId);
			ASSERT_THAT(IsTrue(CaseSource.Len() > 0));
			ASSERT_THAT(AreEqual(CaseSource, Generator.BuildCaseSource(Expected.CaseId)));

			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.StatementNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("return Trace;"))));
			if (Expected.bFault)
			{
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("Trace += 1 / ExceptionDivisor;"))));
			}

			FString AggregateBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, AggregateBody)));
			ASSERT_THAT(AreEqual(GeneratedBody, AggregateBody));
			FString DumpBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Dump, Expected.EntryName, DumpBody)));
			ASSERT_THAT(AreEqual(AggregateBody, DumpBody));

			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				++GoldMatches;
			}
		}
		ASSERT_THAT(AreEqual(23, GoldMatches));

		// Distinct transfer and nesting operation text (typed operands, not just an integer).
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(
			TEXT("LANG-CF-STATEMENT-COUNT-TRANSFER-IF-ONE-EARLY_RETURN-NONE")).Contains(TEXT("return Trace + 20;"))));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(
			TEXT("LANG-CF-STATEMENT-COUNT-TRANSFER-IF-ONE-NESTED_RETURN-NONE")).Contains(TEXT("return Trace + 30;"))));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(
			TEXT("LANG-CF-STATEMENT-COUNT-TRANSFER-IF-ONE-CONTINUE-NONE")).Contains(TEXT("continue;"))));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(
			TEXT("LANG-CF-STATEMENT-COUNT-TRANSFER-IF-ONE-FALLTHROUGH-NONE")).Contains(TEXT("case 1:"))));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(
			TEXT("LANG-CF-STATEMENT-COUNT-TRANSFER-IF-ONE-NONE-SAME_KIND")).Contains(TEXT("for (int SameKind = 0; SameKind < 1; ++SameKind)"))));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(
			TEXT("LANG-CF-STATEMENT-COUNT-TRANSFER-IF-ONE-NONE-MIXED_LOOP")).Contains(TEXT("int MixedLoop = 0;"))));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(
			TEXT("LANG-CF-STATEMENT-COUNT-TRANSFER-IF-ONE-NONE-BRANCH_LOOP")).Contains(TEXT("for (int BranchLoop = 0; BranchLoop < 1; ++BranchLoop)"))));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(
			TEXT("LANG-CF-STATEMENT-COUNT-TRANSFER-IF-ONE-NONE-THREE_LEVEL")).Contains(TEXT("int ThreeLevel = 0;"))));

		// Identity, owned source, and FunctionName behavior.
		const FString OwnedCatalog = Generator.BuildCaseSource(CatalogExample);
		ASSERT_THAT(IsTrue(OwnedCatalog.Contains(TEXT("int EntryLangCfStatementCountTransferIfZeroNoneNone()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangCfStatementCountTransferIfZeroNoneNone()"))));

		FStatementTransferParams DefaultParams;
		const FString DefaultSingle = Generator.BuildStatementTransferSource(DefaultParams);
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsFalse(DefaultSingle.Contains(TEXT("int EntryLang"))));

		FStatementTransferParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		const FString ProbeSource = Generator.BuildStatementTransferSource(ProbeParams);
		ASSERT_THAT(IsTrue(ProbeSource.Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(ProbeSource.Contains(TEXT("int Entry()"))));

		FStatementTransferParams ExceptionParams;
		ExceptionParams.Transfer = EStatementTransfer::Exception;
		ExceptionParams.Count = EStatementCount::One;
		const FString ExceptionSingle = Generator.BuildStatementTransferSource(ExceptionParams);
		ASSERT_THAT(IsTrue(ExceptionSingle.Contains(TEXT("Trace += 1 / ExceptionDivisor;"))));

		// Invalid inputs return empty with no partial module.
		FStatementTransferParams InvalidName = DefaultParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildStatementTransferSource(InvalidName).IsEmpty()));
		FStatementTransferParams InvalidEnum = DefaultParams;
		InvalidEnum.Statement = static_cast<EStatementKind>(StatementTransferGeneratorTest::StatementAxisCount);
		ASSERT_THAT(IsTrue(Generator.BuildStatementTransferSource(InvalidEnum).IsEmpty()));
		FStatementTransferParams InvalidTransfer = DefaultParams;
		InvalidTransfer.Transfer = static_cast<EStatementTransfer>(StatementTransferGeneratorTest::TransferAxisCount);
		ASSERT_THAT(IsTrue(Generator.BuildStatementTransferSource(InvalidTransfer).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		// A real expected zero (ZERO count, non-fault) stays a populated optional.
		const FString ZeroCaseId = TEXT("LANG-CF-STATEMENT-COUNT-TRANSFER-IF-ZERO-NONE-NONE");
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

		// Isolation: mutating a returned list does not affect a later enumeration.
		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(ExpectedCells[0].ExpectedReturn, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		// Retained failure artifact and write-error probes.
		ASSERT_THAT(IsTrue(OutputFile.EndsWith(TEXT("GeneratedCases/StatementTransferGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\StatementTransferGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));

		ASSERT_THAT(IsTrue(AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("")).IsEmpty()));
		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/StatementTransferGenerator.as"), Dump, ProbeError)));
		ASSERT_THAT(IsTrue(ProbeError.Len() > 0));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("StatementTransferGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
