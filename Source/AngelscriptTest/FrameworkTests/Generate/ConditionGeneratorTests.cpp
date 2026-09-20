#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestConditionGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace ConditionGeneratorTest
{
struct FConditionExpectedCell
{
	FString CaseId;
	FString EntryName;
	FString ConditionNeedle;
	FString StatementNeedle;
	int32 Packed = 0;
	bool bReject = false;
};

constexpr const TCHAR* ProbeCaseId = TEXT("LANG-CF-CONDITION-IF-BOOL_LITERAL-TRUE");
constexpr const TCHAR* CatalogExample = TEXT("LANG-CF-CONDITION-IF-BOOL_LITERAL");
constexpr const TCHAR* ZeroCaseId = TEXT("LANG-CF-CONDITION-IF-BOOL_LITERAL-FALSE");

int32 IndependentPacked(const TCHAR* Statement, const TCHAR* Condition, const TCHAR* Truth)
{
	if (FCString::Strcmp(Condition, TEXT("INVALID_TYPE")) == 0)
	{
		return 0;
	}
	const bool bTruth = FCString::Strcmp(Truth, TEXT("TRUE")) == 0;
	const int32 Trace = (FCString::Strcmp(Statement, TEXT("DO_WHILE")) == 0 || bTruth) ? 1 : 0;
	if (FCString::Strcmp(Condition, TEXT("SIDE_EFFECT_CALL")) != 0)
	{
		return Trace * 10;
	}
	const bool bRepeated = FCString::Strcmp(Statement, TEXT("WHILE")) == 0
		|| FCString::Strcmp(Statement, TEXT("FOR")) == 0;
	const int32 EvaluationCount = (bTruth && bRepeated) ? 2 : 1;
	return Trace * 10 + EvaluationCount;
}

FString IndependentConditionNeedle(const TCHAR* Condition, const bool bTruth)
{
	if (FCString::Strcmp(Condition, TEXT("BOOL_LITERAL")) == 0)
	{
		return bTruth ? TEXT("true") : TEXT("false");
	}
	if (FCString::Strcmp(Condition, TEXT("VARIABLE")) == 0)
	{
		return TEXT("ConditionValue");
	}
	if (FCString::Strcmp(Condition, TEXT("COMPARISON")) == 0)
	{
		return bTruth ? TEXT("2 > 1") : TEXT("2 < 1");
	}
	if (FCString::Strcmp(Condition, TEXT("LOGICAL")) == 0)
	{
		return bTruth ? TEXT("ConditionValue && true") : TEXT("ConditionValue && false");
	}
	if (FCString::Strcmp(Condition, TEXT("SIDE_EFFECT_CALL")) == 0)
	{
		return TEXT("EvaluateCondition(ConditionValue, EvaluationCount)");
	}
	if (FCString::Strcmp(Condition, TEXT("OVERLOADED_CONVERSION")) == 0)
	{
		return TEXT("FConditionValue(ConditionValue)");
	}
	return TEXT("FInvalidCondition()");
}

FString IndependentStatementNeedle(const TCHAR* Statement, const FString& ConditionNeedle)
{
	if (FCString::Strcmp(Statement, TEXT("IF")) == 0)
	{
		return FString::Printf(TEXT("if (%s)"), *ConditionNeedle);
	}
	if (FCString::Strcmp(Statement, TEXT("WHILE")) == 0)
	{
		return FString::Printf(TEXT("while ((%s) && Guard < 1)"), *ConditionNeedle);
	}
	if (FCString::Strcmp(Statement, TEXT("DO_WHILE")) == 0)
	{
		return FString::Printf(TEXT("while ((%s) && Guard < 1);"), *ConditionNeedle);
	}
	return FString::Printf(TEXT("for (; (%s) && Guard < 1; ++Guard)"), *ConditionNeedle);
}

TArray<FConditionExpectedCell> BuildExpectedCells()
{
	static const TCHAR* const kStatements[] = {TEXT("IF"), TEXT("WHILE"), TEXT("DO_WHILE"), TEXT("FOR")};
	static const TCHAR* const kConditions[] = {
		TEXT("BOOL_LITERAL"), TEXT("VARIABLE"), TEXT("COMPARISON"), TEXT("LOGICAL"),
		TEXT("SIDE_EFFECT_CALL"), TEXT("OVERLOADED_CONVERSION"), TEXT("INVALID_TYPE")};
	static const TCHAR* const kTruths[] = {TEXT("FALSE"), TEXT("TRUE")};

	TArray<FConditionExpectedCell> Cells;
	Cells.SetNum(56);
	for (int32 Index = 0; Index < 56; ++Index)
	{
		const TCHAR* const Statement = kStatements[Index / 14];
		const TCHAR* const Condition = kConditions[(Index / 2) % 7];
		const TCHAR* const Truth = kTruths[Index % 2];
		const bool bTruth = FCString::Strcmp(Truth, TEXT("TRUE")) == 0;
		FConditionExpectedCell& Cell = Cells[Index];
		Cell.CaseId = FString::Printf(TEXT("LANG-CF-CONDITION-%s-%s-%s"), Statement, Condition, Truth);
		Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
		Cell.ConditionNeedle = IndependentConditionNeedle(Condition, bTruth);
		Cell.StatementNeedle = IndependentStatementNeedle(Statement, Cell.ConditionNeedle);
		Cell.Packed = IndependentPacked(Statement, Condition, Truth);
		Cell.bReject = FCString::Strcmp(Condition, TEXT("INVALID_TYPE")) == 0;
	}
	return Cells;
}

FString ExpectedDeclaration(const FConditionExpectedCell& Cell)
{
	if (Cell.bReject)
	{
		return FString();
	}
	return FString::Printf(TEXT("int %s()"), *Cell.EntryName);
}
}

TEST_CLASS_WITH_FLAGS(
	ConditionGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using ConditionGeneratorTest::BuildExpectedCells;
		using ConditionGeneratorTest::CatalogExample;
		using ConditionGeneratorTest::ExpectedDeclaration;
		using ConditionGeneratorTest::FConditionExpectedCell;
		using ConditionGeneratorTest::ProbeCaseId;
		using ConditionGeneratorTest::ZeroCaseId;
		using AngelscriptTest::Generate::EConditionExpr;
		using AngelscriptTest::Generate::EConditionStatement;
		using AngelscriptTest::Generate::EConditionTruth;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FConditionGenerator;
		using AngelscriptTest::Generate::FConditionParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FConditionExpectedCell> ExpectedCells = BuildExpectedCells();
		FConditionGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CF-CONDITION")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangCfConditionIfBoolLiteralTrue")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile = AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ConditionGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ConditionGenerator dump path=%s cases=56 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(48, CaseCount));
		ASSERT_THAT(AreEqual(56, Cases.Num()));
		ASSERT_THAT(AreEqual(56, ExpectedCells.Num()));
		ASSERT_THAT(AreEqual(8, RejectIds.Num()));

		int32 RejectCount = 0;
		int32 PrefixMatches = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FConditionExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(ExpectedDeclaration(Expected), Cases[Index].EntryDeclaration));
			if (Expected.CaseId.StartsWith(CatalogExample))
			{
				++PrefixMatches;
			}
			if (Expected.bReject)
			{
				++RejectCount;
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::CompileReject));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::SourceOnly));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedException.IsEmpty()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(RejectIds.Contains(Expected.CaseId)));
				const FString RejectSource = Generator.BuildRejectSource(Expected.CaseId);
				ASSERT_THAT(IsTrue(RejectSource.Contains(TEXT("FInvalidCondition()"))));
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
			}
			else
			{
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Packed, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Packed, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
			}
			ASSERT_THAT(IsFalse(Cases[Index].bLimitedObservation));
		}
		ASSERT_THAT(AreEqual(8, RejectCount));
		ASSERT_THAT(AreEqual(2, PrefixMatches));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		const FString OwnedAgain = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(IsTrue(OwnedCase.Len() > 0));
		ASSERT_THAT(AreEqual(OwnedCase, OwnedAgain));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangCfConditionIfBoolLiteralTrue()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangCfConditionIfBoolLiteralTrue()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("FInvalidCondition()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));

		FConditionParams DefaultParams;
		const FString DefaultSingle = Generator.BuildConditionSource(DefaultParams);
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsFalse(DefaultSingle.Contains(TEXT("int EntryLang"))));

		FConditionParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		const FString ProbeSource = Generator.BuildConditionSource(ProbeParams);
		ASSERT_THAT(IsTrue(ProbeSource.Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(ProbeSource.Contains(TEXT("int Entry()"))));

		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("struct FConditionValue"))));
		ASSERT_THAT(AreEqual(9, CountOccurrences(Dump, TEXT("class FInvalidCondition"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("bool EvaluateCondition"))));

		const FString Representative = LoadGold(TEXT("LanguageGenerators/ConditionGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FConditionExpectedCell& Expected : ExpectedCells)
		{
			const FString CaseSource = Expected.bReject
				? Generator.BuildRejectSource(Expected.CaseId)
				: Generator.BuildCaseSource(Expected.CaseId);
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.StatementNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("return Trace * 10 + EvaluationCount;"))));
			if (Expected.bReject)
			{
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("FInvalidCondition()"))));
			}
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FConditionParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildConditionSource(InvalidName).IsEmpty()));
		FConditionParams InvalidEnum = DefaultParams;
		InvalidEnum.Statement = static_cast<EConditionStatement>(4);
		ASSERT_THAT(IsTrue(Generator.BuildConditionSource(InvalidEnum).IsEmpty()));
		FConditionParams RejectParams = DefaultParams;
		RejectParams.Condition = EConditionExpr::InvalidType;
		ASSERT_THAT(IsTrue(Generator.BuildConditionSource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildRejectSource(ProbeCaseId).IsEmpty()));
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
		ASSERT_THAT(AreEqual(ExpectedCells[0].Packed, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(OutputFile.EndsWith(TEXT("GeneratedCases/ConditionGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\ConditionGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(56, CountOccurrences(Dump, TEXT("// Case: LANG-CF-CONDITION-"))));
		ASSERT_THAT(AreEqual(8, CountOccurrences(Dump, TEXT("// Reject module"))));
		for (const FConditionExpectedCell& Expected : ExpectedCells)
		{
			ASSERT_THAT(IsTrue(Dump.Contains(FString::Printf(TEXT("// Case: %s"), *Expected.CaseId))));
			FString DumpBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Dump, Expected.EntryName, DumpBody)));
			if (!Expected.bReject)
			{
				FString AggregateBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, AggregateBody)));
				ASSERT_THAT(AreEqual(AggregateBody, DumpBody));
			}
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
			TEXT("C:/no-such-root/ConditionGenerator.as"), Dump, ProbeError)));
		ASSERT_THAT(IsTrue(ProbeError.Len() > 0));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ConditionGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
