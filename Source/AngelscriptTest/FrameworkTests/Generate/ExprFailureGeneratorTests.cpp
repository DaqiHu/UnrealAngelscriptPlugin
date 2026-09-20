#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestExprFailureGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ExprFailureGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString Needle;
		bool bReject = false;
		FString ExceptionText;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-EXPR-FAILURE-INITIALIZER-DIVIDE_ZERO-FRESH_MODULE");
	static constexpr const TCHAR* ForeignCaseId =
		TEXT("LANG-EXPR-CHAIN-INITIALIZER-TWO-CALL_MEMBER-VALID");

	static bool IndependentReject(const TCHAR* Failure)
	{
		return FCString::Strcmp(Failure, TEXT("INVALID_LVALUE")) == 0
			|| FCString::Strcmp(Failure, TEXT("MISSING_DELIMITER")) == 0
			|| FCString::Strcmp(Failure, TEXT("MALFORMED_TERNARY")) == 0
			|| FCString::Strcmp(Failure, TEXT("MISSING_SYMBOL")) == 0
			|| FCString::Strcmp(Failure, TEXT("AMBIGUOUS_SYMBOL")) == 0
			|| FCString::Strcmp(Failure, TEXT("INACCESSIBLE_MEMBER")) == 0
			|| FCString::Strcmp(Failure, TEXT("MISSING_MEMBER")) == 0;
	}

	static FString IndependentException(const TCHAR* Failure)
	{
		if (FCString::Strcmp(Failure, TEXT("DIVIDE_ZERO")) == 0)
		{
			return TEXT("Divide by zero");
		}
		if (FCString::Strcmp(Failure, TEXT("INDEX_OUT_OF_RANGE")) == 0)
		{
			return TEXT("Expression index out of range");
		}
		if (FCString::Strcmp(Failure, TEXT("NULL_ACCESS")) == 0)
		{
			return TEXT("Null pointer access");
		}
		if (FCString::Strcmp(Failure, TEXT("EXCEPTION_LEFT")) == 0)
		{
			return TEXT("Expression left operand exception");
		}
		if (FCString::Strcmp(Failure, TEXT("EXCEPTION_RIGHT")) == 0)
		{
			return TEXT("Expression right operand exception");
		}
		return FString();
	}

	static FString IndependentNeedle(const TCHAR* Context, const TCHAR* Failure)
	{
		if (FCString::Strcmp(Context, TEXT("RETURN")) == 0)
		{
			return TEXT("ProduceFailure_");
		}
		if (FCString::Strcmp(Failure, TEXT("INVALID_LVALUE")) == 0)
		{
			return TEXT("((1 + 2) = 3)");
		}
		if (FCString::Strcmp(Failure, TEXT("MISSING_DELIMITER")) == 0)
		{
			return TEXT("RecordFailureStage(1]");
		}
		if (FCString::Strcmp(Failure, TEXT("MALFORMED_TERNARY")) == 0)
		{
			return TEXT("(true ? RecordFailureStage(1))");
		}
		if (FCString::Strcmp(Failure, TEXT("MISSING_SYMBOL")) == 0)
		{
			return TEXT("MissingFailureSymbol");
		}
		if (FCString::Strcmp(Failure, TEXT("AMBIGUOUS_SYMBOL")) == 0)
		{
			return TEXT("ResolveAmbiguousFailure(nullptr)");
		}
		if (FCString::Strcmp(Failure, TEXT("INACCESSIBLE_MEMBER")) == 0)
		{
			return TEXT("HiddenOwner.HiddenFailureMember");
		}
		if (FCString::Strcmp(Failure, TEXT("MISSING_MEMBER")) == 0)
		{
			return TEXT("MissingOwner.MissingFailureMember");
		}
		if (FCString::Strcmp(Failure, TEXT("DIVIDE_ZERO")) == 0)
		{
			return TEXT("RecordFailureStage(1) / RecordFailureStage(0)");
		}
		if (FCString::Strcmp(Failure, TEXT("INDEX_OUT_OF_RANGE")) == 0)
		{
			return TEXT("FailureIndex[RecordFailureStage(9)]");
		}
		if (FCString::Strcmp(Failure, TEXT("NULL_ACCESS")) == 0)
		{
			return TEXT("NullFailure.GetValue()");
		}
		if (FCString::Strcmp(Failure, TEXT("EXCEPTION_LEFT")) == 0)
		{
			return TEXT("ThrowFailureStage(1)");
		}
		if (FCString::Strcmp(Failure, TEXT("EXCEPTION_RIGHT")) == 0)
		{
			return TEXT("ThrowFailureStage(2)");
		}
		if (FCString::Strcmp(Context, TEXT("LOOP_CLAUSE")) == 0)
		{
			return TEXT("for (;");
		}
		return TEXT("RecordFailureStage");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kContexts[] = {
			TEXT("INITIALIZER"), TEXT("ASSIGNMENT"), TEXT("ARGUMENT"), TEXT("RETURN"),
			TEXT("CONDITION"), TEXT("LOOP_CLAUSE"), TEXT("SWITCH_SELECTOR"), TEXT("INDEX")};
		static const TCHAR* const kFailures[] = {
			TEXT("INVALID_LVALUE"), TEXT("MISSING_DELIMITER"), TEXT("MALFORMED_TERNARY"),
			TEXT("MISSING_SYMBOL"), TEXT("AMBIGUOUS_SYMBOL"), TEXT("INACCESSIBLE_MEMBER"),
			TEXT("MISSING_MEMBER"), TEXT("DIVIDE_ZERO"), TEXT("INDEX_OUT_OF_RANGE"),
			TEXT("NULL_ACCESS"), TEXT("EXCEPTION_LEFT"), TEXT("EXCEPTION_RIGHT")};
		static const TCHAR* const kRecoveries[] = {TEXT("FRESH_MODULE"), TEXT("REBUILD_OR_CONTEXT_REUSE")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(192);
		for (int32 Index = 0; Index < 192; ++Index)
		{
			const TCHAR* const Context = kContexts[Index / 24];
			const TCHAR* const Failure = kFailures[(Index / 2) % 12];
			const TCHAR* const Recovery = kRecoveries[Index % 2];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-EXPR-FAILURE-%s-%s-%s"), Context, Failure, Recovery);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Failure);
			Cell.ExceptionText = IndependentException(Failure);
			Cell.Needle = IndependentNeedle(Context, Failure);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EExprFailureContext;
		using AngelscriptTest::Generate::EExprFailureKind;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FExprFailureGenerator;
		using AngelscriptTest::Generate::FExprFailureParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FExprFailureGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-EXPR-FAILURE")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangExprFailureInitializerDivideZeroFreshModule")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ExprFailureGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ExprFailureGenerator dump path=%s cases=192 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(80, CaseCount));
		ASSERT_THAT(AreEqual(192, Cases.Num()));
		ASSERT_THAT(AreEqual(192, ExpectedCells.Num()));
		ASSERT_THAT(AreEqual(112, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/ExprFailureGenerator/RepresentativeSources.as"));
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
			else
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
				if (Representative.Contains(Expected.EntryName))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
		}
		ASSERT_THAT(AreEqual(112, RejectCount));
		ASSERT_THAT(AreEqual(80, FaultCount));

		const FString OwnedFault = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedFault, Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(IsTrue(OwnedFault.Contains(TEXT("int EntryLangExprFailureInitializerDivideZeroFreshModule()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangExprFailureInitializerDivideZeroFreshModule()"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int RecordFailureStage(int Marker)"))));

		FExprFailureParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildExpressionFailureSource(DefaultParams).IsEmpty()));
		FExprFailureParams DivideZero;
		DivideZero.Failure = EExprFailureKind::DivideZero;
		ASSERT_THAT(IsTrue(Generator.BuildExpressionFailureSource(DivideZero).Contains(TEXT("int Entry()"))));
		DivideZero.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildExpressionFailureSource(DivideZero).Contains(TEXT("int ProbeEntry()"))));
		DivideZero.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildExpressionFailureSource(DivideZero).IsEmpty()));
		FExprFailureParams InvalidEnum;
		InvalidEnum.Context = static_cast<EExprFailureContext>(8);
		ASSERT_THAT(IsTrue(Generator.BuildExpressionFailureSource(InvalidEnum).IsEmpty()));
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
		ASSERT_THAT(AreEqual(192, CountOccurrences(Dump, TEXT("// Case: LANG-EXPR-FAILURE-"))));
		ASSERT_THAT(AreEqual(112, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/ExprFailureGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ExprFailureGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
