#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestExprSourceBoundaryGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ExprSourceBoundaryGenerator,
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
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-EXPR-SOURCE-BOUNDARY-FIRST-INITIALIZER-PARENTHESES_ONE");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-CONDITION-IF-BOOL_LITERAL-TRUE");

	static const TCHAR* BuildToken(const int32 Build)
	{
		static const TCHAR* const kTokens[] = {TEXT("FIRST"), TEXT("SAME_REBUILD"), TEXT("CHANGED_REBUILD")};
		return kTokens[Build];
	}

	static const TCHAR* ContextToken(const int32 Context)
	{
		static const TCHAR* const kTokens[] = {
			TEXT("INITIALIZER"), TEXT("ARGUMENT"), TEXT("RETURN"), TEXT("CONDITION"), TEXT("INDEX")};
		return kTokens[Context];
	}

	static const TCHAR* ScenarioToken(const int32 Scenario)
	{
		static const TCHAR* const kTokens[] = {
			TEXT("PARENTHESES_ONE"), TEXT("PARENTHESES_EIGHT"), TEXT("PARENTHESES_SIXTY_FOUR"),
			TEXT("CHAIN_ONE"), TEXT("CHAIN_EIGHT"), TEXT("CHAIN_THIRTY_TWO"),
			TEXT("ARGUMENTS_ZERO"), TEXT("ARGUMENTS_ONE"), TEXT("ARGUMENTS_MANY"),
			TEXT("NUMERIC_MINIMUM"), TEXT("NUMERIC_MAXIMUM"), TEXT("WHITESPACE"),
			TEXT("COMMENTS"), TEXT("MULTILINE")};
		return kTokens[Scenario];
	}

	static int32 IndependentSeed(const int32 Build)
	{
		return Build == 2 ? 58 : 41;
	}

	static int32 IndependentExpected(const int32 Build, const int32 Scenario)
	{
		const int32 Seed = IndependentSeed(Build);
		if (Scenario == 3)
		{
			return Seed + 1;
		}
		if (Scenario == 4)
		{
			return Seed + 8;
		}
		if (Scenario == 5)
		{
			return Seed + 32;
		}
		if (Scenario == 8)
		{
			return Seed + 120;
		}
		if (Scenario == 9)
		{
			return Build == 2 ? MIN_int32 + 17 : MIN_int32;
		}
		if (Scenario == 10)
		{
			return Build == 2 ? MAX_int32 - 17 : MAX_int32;
		}
		if (Scenario >= 11)
		{
			return Seed + 3;
		}
		return Seed;
	}

	static FString IndependentNeedle(const int32 Build, const int32 Scenario)
	{
		const int32 Seed = IndependentSeed(Build);
		if (Scenario == 0)
		{
			return FString::Printf(TEXT("(int64(%d))"), Seed);
		}
		if (Scenario == 1)
		{
			return TEXT("((((((((");
		}
		if (Scenario == 2)
		{
			return TEXT("int64(");
		}
		if (Scenario == 3)
		{
			return FString::Printf(TEXT("MakeBoundaryChain(%d).Step().Value"), Seed);
		}
		if (Scenario == 6)
		{
			return Build == 2 ? TEXT("BoundaryArgumentsZeroChanged()") : TEXT("BoundaryArgumentsZero()");
		}
		if (Scenario == 7)
		{
			return FString::Printf(TEXT("BoundaryArgumentsOne(%d)"), Seed);
		}
		if (Scenario == 8)
		{
			return FString::Printf(TEXT("BoundaryArgumentsMany(%d"), Seed);
		}
		if (Scenario == 9)
		{
			return Build == 2 ? TEXT("int64(-2147483647 - 1) + 17") : TEXT("int64(-2147483647 - 1)");
		}
		if (Scenario == 10)
		{
			return Build == 2 ? TEXT("int64(2147483647) - 17") : TEXT("int64(2147483647)");
		}
		if (Scenario == 11)
		{
			return FString::Printf(TEXT("int64(  %d\t+\t3  )"), Seed);
		}
		if (Scenario == 12)
		{
			return FString::Printf(TEXT("int64(%d /* seed */ + /* delta */ 3)"), Seed);
		}
		if (Scenario == 13)
		{
			return FString::Printf(TEXT("%d"), Seed);
		}
		return FString::Printf(TEXT("MakeBoundaryChain(%d)"), Seed);
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		TArray<FExpectedCell> Cells;
		Cells.SetNum(210);
		for (int32 Index = 0; Index < 210; ++Index)
		{
			const int32 Build = Index / 70;
			const int32 Context = (Index / 14) % 5;
			const int32 Scenario = Index % 14;
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-EXPR-SOURCE-BOUNDARY-%s-%s-%s"),
				BuildToken(Build),
				ContextToken(Context),
				ScenarioToken(Scenario));
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Needle = IndependentNeedle(Build, Scenario);
			Cell.Expected = IndependentExpected(Build, Scenario);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EExprSourceBoundaryBuild;
		using AngelscriptTest::Generate::EExprSourceBoundaryContext;
		using AngelscriptTest::Generate::EExprSourceBoundaryScenario;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::FExprSourceBoundaryGenerator;
		using AngelscriptTest::Generate::FExprSourceBoundaryParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FExprSourceBoundaryGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-EXPR-SOURCE-BOUNDARY")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangExprSourceBoundaryFirstInitializerParenthesesOne")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ExprSourceBoundaryGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ExprSourceBoundaryGenerator dump path=%s cases=210 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(210, CaseCount));
		ASSERT_THAT(AreEqual(210, Cases.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/ExprSourceBoundaryGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(
				FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			ASSERT_THAT(IsTrue(Cases[Index].bLimitedObservation));
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			const FString CaseSource = Generator.BuildCaseSource(Expected.CaseId);
			ASSERT_THAT(IsTrue(CaseSource.Contains(Expected.Needle)));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
			if (Expected.CaseId.Contains(TEXT("SAME_REBUILD")) || Expected.CaseId.Contains(TEXT("CHANGED_REBUILD")))
			{
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::SourceOnly));
			}
		}
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("struct FBoundaryChain"))));

		FExprSourceBoundaryParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildExpressionBoundarySource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildExpressionBoundarySource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildExpressionBoundarySource(DefaultParams).IsEmpty()));
		DefaultParams.FunctionName.Reset();
		DefaultParams.Build = static_cast<EExprSourceBoundaryBuild>(3);
		ASSERT_THAT(IsTrue(Generator.BuildExpressionBoundarySource(DefaultParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(210, CountOccurrences(Dump, TEXT("// Case: LANG-EXPR-SOURCE-BOUNDARY-"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/ExprSourceBoundaryGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ExprSourceBoundaryGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
