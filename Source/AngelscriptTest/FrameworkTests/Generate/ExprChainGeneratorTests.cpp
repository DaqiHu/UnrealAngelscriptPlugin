#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestExprChainGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ExprChainGenerator,
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
		bool bReject = false;
		bool bFault = false;
		FString ExceptionText;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-EXPR-CHAIN-INITIALIZER-TWO-CALL_MEMBER-VALID");
	static constexpr const TCHAR* ForeignCaseId =
		TEXT("LANG-EXPR-ASSOCIATIVITY-UNPARENTHESIZED-MULTIPLICATIVE-REPEATED_OPERATOR");

	static int32 IndependentDepth(const TCHAR* Depth)
	{
		if (FCString::Strcmp(Depth, TEXT("TWO")) == 0)
		{
			return 2;
		}
		if (FCString::Strcmp(Depth, TEXT("THREE")) == 0)
		{
			return 3;
		}
		if (FCString::Strcmp(Depth, TEXT("EIGHT")) == 0)
		{
			return 8;
		}
		return 32;
	}

	static int32 IndependentOpValue(const TCHAR* Shape, const int32 StageIndex)
	{
		int32 PatternCount = 2;
		if (FCString::Strcmp(Shape, TEXT("CALL_INDEX_CAST")) == 0
			|| FCString::Strcmp(Shape, TEXT("MEMBER_CALL_INDEX")) == 0)
		{
			PatternCount = 3;
		}
		else if (FCString::Strcmp(Shape, TEXT("CAST_CALL_MEMBER_INDEX")) == 0)
		{
			PatternCount = 4;
		}
		const int32 PatternIndex = StageIndex % PatternCount;
		if (FCString::Strcmp(Shape, TEXT("CALL_MEMBER")) == 0)
		{
			return PatternIndex == 0 ? 1 : 3;
		}
		if (FCString::Strcmp(Shape, TEXT("MEMBER_CALL")) == 0)
		{
			return PatternIndex == 0 ? 3 : 1;
		}
		if (FCString::Strcmp(Shape, TEXT("INDEX_MEMBER")) == 0)
		{
			return PatternIndex == 0 ? 5 : 3;
		}
		if (FCString::Strcmp(Shape, TEXT("MEMBER_INDEX")) == 0)
		{
			return PatternIndex == 0 ? 3 : 5;
		}
		if (FCString::Strcmp(Shape, TEXT("CAST_MEMBER")) == 0)
		{
			return PatternIndex == 0 ? 7 : 3;
		}
		if (FCString::Strcmp(Shape, TEXT("CALL_INDEX_CAST")) == 0)
		{
			return PatternIndex == 0 ? 1 : (PatternIndex == 1 ? 5 : 7);
		}
		if (FCString::Strcmp(Shape, TEXT("MEMBER_CALL_INDEX")) == 0)
		{
			return PatternIndex == 0 ? 3 : (PatternIndex == 1 ? 1 : 5);
		}
		if (PatternIndex == 0)
		{
			return 7;
		}
		if (PatternIndex == 1)
		{
			return 1;
		}
		return PatternIndex == 2 ? 3 : 5;
	}

	static int32 IndependentExpected(const TCHAR* Context, const TCHAR* Depth, const TCHAR* Shape)
	{
		if (FCString::Strcmp(Context, TEXT("CONDITION")) == 0)
		{
			return 1;
		}
		int32 Value = 100;
		const int32 Count = IndependentDepth(Depth);
		for (int32 Stage = 0; Stage < Count; ++Stage)
		{
			Value += IndependentOpValue(Shape, Stage);
		}
		return Value;
	}

	static FString IndependentNeedle(const TCHAR* Context, const TCHAR* Shape, const TCHAR* State)
	{
		if (FCString::Strcmp(State, TEXT("INVALID_INTERMEDIATE")) == 0)
		{
			return TEXT(".BreakToInt()");
		}
		if (FCString::Strcmp(State, TEXT("MISSING_TERMINAL")) == 0)
		{
			return TEXT(".MissingTerminal");
		}
		if (FCString::Strcmp(Context, TEXT("ARGUMENT")) == 0)
		{
			return TEXT("ObserveChain(");
		}
		if (FCString::Strcmp(Context, TEXT("RETURN")) == 0)
		{
			return TEXT("ProduceChainValue_");
		}
		if (FCString::Strcmp(Context, TEXT("CONDITION")) == 0)
		{
			return TEXT("if (");
		}
		if (FCString::Strcmp(Context, TEXT("ASSIGNMENT")) == 0)
		{
			return TEXT("Result = ");
		}
		if (FCString::Strcmp(Shape, TEXT("CALL_MEMBER")) == 0)
		{
			return TEXT(".StepCall()");
		}
		if (FCString::Strcmp(Shape, TEXT("MEMBER_CALL")) == 0)
		{
			return TEXT(".GetMember()");
		}
		if (FCString::Strcmp(Shape, TEXT("CALL_INDEX_CAST")) == 0)
		{
			return TEXT(".StepCall()");
		}
		if (FCString::Strcmp(Shape, TEXT("CAST_MEMBER")) == 0
			|| FCString::Strcmp(Shape, TEXT("CAST_CALL_MEMBER_INDEX")) == 0)
		{
			return TEXT(".opCast()");
		}
		return TEXT("MakeChain()");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kContexts[] = {
			TEXT("INITIALIZER"), TEXT("ARGUMENT"), TEXT("RETURN"), TEXT("CONDITION"), TEXT("ASSIGNMENT")};
		static const TCHAR* const kDepths[] = {TEXT("TWO"), TEXT("THREE"), TEXT("EIGHT"), TEXT("DEEP_BOUNDARY")};
		static const TCHAR* const kShapes[] = {
			TEXT("CALL_MEMBER"), TEXT("MEMBER_CALL"), TEXT("INDEX_MEMBER"), TEXT("MEMBER_INDEX"),
			TEXT("CAST_MEMBER"), TEXT("CALL_INDEX_CAST"), TEXT("MEMBER_CALL_INDEX"), TEXT("CAST_CALL_MEMBER_INDEX")};
		static const TCHAR* const kStates[] = {
			TEXT("VALID"), TEXT("NULL_RECEIVER"), TEXT("INVALID_INTERMEDIATE"),
			TEXT("EXCEPTION_INTERMEDIATE"), TEXT("MISSING_TERMINAL")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(800);
		for (int32 Index = 0; Index < 800; ++Index)
		{
			const TCHAR* const Context = kContexts[Index / 160];
			const TCHAR* const Depth = kDepths[(Index / 40) % 4];
			const TCHAR* const Shape = kShapes[(Index / 5) % 8];
			const TCHAR* const State = kStates[Index % 5];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-EXPR-CHAIN-%s-%s-%s-%s"), Context, Depth, Shape, State);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = FCString::Strcmp(State, TEXT("INVALID_INTERMEDIATE")) == 0
				|| FCString::Strcmp(State, TEXT("MISSING_TERMINAL")) == 0;
			Cell.bFault = FCString::Strcmp(State, TEXT("NULL_RECEIVER")) == 0
				|| FCString::Strcmp(State, TEXT("EXCEPTION_INTERMEDIATE")) == 0;
			Cell.Expected = (Cell.bReject || Cell.bFault) ? 0 : IndependentExpected(Context, Depth, Shape);
			Cell.Needle = IndependentNeedle(Context, Shape, State);
			if (FCString::Strcmp(State, TEXT("NULL_RECEIVER")) == 0)
			{
				Cell.ExceptionText = TEXT("Null pointer access");
			}
			else if (FCString::Strcmp(State, TEXT("EXCEPTION_INTERMEDIATE")) == 0)
			{
				Cell.ExceptionText = TEXT("Expression chain intermediate exception");
			}
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EExprChainContext;
		using AngelscriptTest::Generate::EExprChainState;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FExprChainGenerator;
		using AngelscriptTest::Generate::FExprChainParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FExprChainGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-EXPR-CHAIN")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangExprChainInitializerTwoCallMemberValid")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ExprChainGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ExprChainGenerator dump path=%s cases=800 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(480, CaseCount));
		ASSERT_THAT(AreEqual(800, Cases.Num()));
		ASSERT_THAT(AreEqual(800, ExpectedCells.Num()));
		ASSERT_THAT(AreEqual(320, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/ExprChainGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 RejectCount = 0;
		int32 FaultCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			if (Expected.bReject)
			{
				++RejectCount;
				ASSERT_THAT(AreEqual(FString(), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::CompileReject));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::SourceOnly));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(RejectIds.Contains(Expected.CaseId)));
				const FString RejectSource = Generator.BuildRejectSource(Expected.CaseId);
				ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.Needle)));
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
				if (Representative.Contains(Expected.EntryName))
				{
					FString GeneratedBody;
					ASSERT_THAT(IsTrue(ExtractFunction(RejectSource, Expected.EntryName, GeneratedBody)));
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
			else if (Expected.bFault)
			{
				++FaultCount;
				ASSERT_THAT(AreEqual(
					FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(AreEqual(Expected.ExceptionText, Cases[Index].ExpectedException));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
			}
			else
			{
				ASSERT_THAT(AreEqual(
					FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
				ASSERT_THAT(IsTrue(
					GeneratedBody.Contains(TEXT("MakeChain()"))
					|| GeneratedBody.Contains(TEXT("ProduceChainValue_"))));
				if (Representative.Contains(Expected.EntryName))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
		}
		ASSERT_THAT(AreEqual(320, RejectCount));
		ASSERT_THAT(AreEqual(320, FaultCount));

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangExprChainInitializerTwoCallMemberValid()"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int ObserveChain(int Value)"))));

		FExprChainParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildExpressionChainSource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildExpressionChainSource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildExpressionChainSource(DefaultParams).IsEmpty()));
		FExprChainParams InvalidEnum;
		InvalidEnum.Context = static_cast<EExprChainContext>(5);
		ASSERT_THAT(IsTrue(Generator.BuildExpressionChainSource(InvalidEnum).IsEmpty()));
		FExprChainParams RejectParams;
		RejectParams.State = EExprChainState::MissingTerminal;
		ASSERT_THAT(IsTrue(Generator.BuildExpressionChainSource(RejectParams).IsEmpty()));
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
		ASSERT_THAT(AreEqual(800, CountOccurrences(Dump, TEXT("// Case: LANG-EXPR-CHAIN-"))));
		ASSERT_THAT(AreEqual(320, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/ExprChainGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ExprChainGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
