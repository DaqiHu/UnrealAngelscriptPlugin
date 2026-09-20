#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestExprAssociativityGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ExprAssociativityGenerator,
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
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-EXPR-ASSOCIATIVITY-UNPARENTHESIZED-MULTIPLICATIVE-REPEATED_OPERATOR");
	static constexpr const TCHAR* ZeroCaseId =
		TEXT("LANG-EXPR-ASSOCIATIVITY-RIGHT_PARENTHESIZED-MULTIPLICATIVE-REPEATED_OPERATOR");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-EXPR-CHAIN-INITIALIZER-TWO-CALL_MEMBER-VALID");

	static bool IndependentReject(const TCHAR* Level, const TCHAR* Sequence)
	{
		if (FCString::Strcmp(Level, TEXT("ASSIGNMENT")) == 0)
		{
			return true;
		}
		if (FCString::Strcmp(Sequence, TEXT("REPEATED_OPERATOR")) != 0)
		{
			return false;
		}
		return FCString::Strcmp(Level, TEXT("LOGICAL_AND")) == 0
			|| FCString::Strcmp(Level, TEXT("LOGICAL_OR")) == 0;
	}

	static bool IndependentUsesLeft(const TCHAR* Grouping, const TCHAR* Level)
	{
		if (FCString::Strcmp(Grouping, TEXT("LEFT_PARENTHESIZED")) == 0)
		{
			return true;
		}
		if (FCString::Strcmp(Grouping, TEXT("RIGHT_PARENTHESIZED")) == 0)
		{
			return false;
		}
		return FCString::Strcmp(Level, TEXT("CONDITIONAL")) != 0
			&& FCString::Strcmp(Level, TEXT("ASSIGNMENT")) != 0;
	}

	static FString IndependentNeedle(const TCHAR* Grouping, const TCHAR* Level, const TCHAR* Sequence)
	{
		const bool bMixed = FCString::Strcmp(Sequence, TEXT("MIXED_SAME_LEVEL")) == 0;
		const bool bLeft = FCString::Strcmp(Grouping, TEXT("LEFT_PARENTHESIZED")) == 0;
		const bool bRight = FCString::Strcmp(Grouping, TEXT("RIGHT_PARENTHESIZED")) == 0;
		if (FCString::Strcmp(Level, TEXT("CONDITIONAL")) == 0)
		{
			if (bMixed)
			{
				if (bLeft)
				{
					return TEXT("(A ? B : C) ? D : E");
				}
				return bRight ? TEXT("A ? (B ? C : D) : E") : TEXT("A ? B ? C : D : E");
			}
			if (bLeft)
			{
				return TEXT("(A ? B : C) ? D : E");
			}
			return bRight ? TEXT("A ? B : (C ? D : E)") : TEXT("A ? B : C ? D : E");
		}
		if (FCString::Strcmp(Level, TEXT("ASSIGNMENT")) == 0)
		{
			const TCHAR* const First = bMixed ? TEXT("+=") : TEXT("=");
			if (bLeft)
			{
				return FString::Printf(TEXT("(A %s B) = C"), First);
			}
			return bRight
				? FString::Printf(TEXT("A %s (B = C)"), First)
				: FString::Printf(TEXT("A %s B = C"), First);
		}

		const TCHAR* Primary = TEXT("*");
		const TCHAR* Secondary = TEXT("/");
		bool bDistinct = true;
		if (FCString::Strcmp(Level, TEXT("ADDITIVE")) == 0)
		{
			Primary = TEXT("+");
			Secondary = TEXT("-");
		}
		else if (FCString::Strcmp(Level, TEXT("SHIFT")) == 0)
		{
			Primary = TEXT("<<");
			Secondary = TEXT(">>");
		}
		else if (FCString::Strcmp(Level, TEXT("RELATIONAL")) == 0)
		{
			Primary = TEXT("<");
			Secondary = TEXT(">=");
		}
		else if (FCString::Strcmp(Level, TEXT("EQUALITY")) == 0)
		{
			Primary = TEXT("==");
			Secondary = TEXT("!=");
		}
		else if (FCString::Strcmp(Level, TEXT("BITWISE_AND")) == 0)
		{
			Primary = TEXT("&");
			Secondary = TEXT("&");
			bDistinct = false;
		}
		else if (FCString::Strcmp(Level, TEXT("BITWISE_XOR")) == 0)
		{
			Primary = TEXT("^");
			Secondary = TEXT("^");
			bDistinct = false;
		}
		else if (FCString::Strcmp(Level, TEXT("BITWISE_OR")) == 0)
		{
			Primary = TEXT("|");
			Secondary = TEXT("|");
			bDistinct = false;
		}
		else if (FCString::Strcmp(Level, TEXT("LOGICAL_AND")) == 0)
		{
			Primary = TEXT("&&");
			Secondary = TEXT("&&");
			bDistinct = false;
		}
		else if (FCString::Strcmp(Level, TEXT("LOGICAL_OR")) == 0)
		{
			Primary = TEXT("||");
			Secondary = TEXT("||");
			bDistinct = false;
		}

		const TCHAR* const RightToken = bMixed ? Secondary : Primary;
		const TCHAR* const Middle = bMixed && !bDistinct ? TEXT("Q") : TEXT("B");
		if (bLeft)
		{
			return FString::Printf(TEXT("(A %s %s) %s C"), Primary, Middle, RightToken);
		}
		if (bRight)
		{
			return FString::Printf(TEXT("A %s (%s %s C)"), Primary, Middle, RightToken);
		}
		return FString::Printf(TEXT("A %s %s %s C"), Primary, Middle, RightToken);
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kGroupings[] = {
			TEXT("UNPARENTHESIZED"), TEXT("LEFT_PARENTHESIZED"), TEXT("RIGHT_PARENTHESIZED")};
		static const TCHAR* const kLevels[] = {
			TEXT("MULTIPLICATIVE"), TEXT("ADDITIVE"), TEXT("SHIFT"), TEXT("RELATIONAL"),
			TEXT("EQUALITY"), TEXT("BITWISE_AND"), TEXT("BITWISE_XOR"), TEXT("BITWISE_OR"),
			TEXT("LOGICAL_AND"), TEXT("LOGICAL_OR"), TEXT("CONDITIONAL"), TEXT("ASSIGNMENT")};
		static const TCHAR* const kSequences[] = {TEXT("REPEATED_OPERATOR"), TEXT("MIXED_SAME_LEVEL")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(72);
		for (int32 Index = 0; Index < 72; ++Index)
		{
			const TCHAR* const Grouping = kGroupings[Index / 24];
			const TCHAR* const Level = kLevels[(Index / 2) % 12];
			const TCHAR* const Sequence = kSequences[Index % 2];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-EXPR-ASSOCIATIVITY-%s-%s-%s"), Grouping, Level, Sequence);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Level, Sequence);
			Cell.Expected = Cell.bReject ? 0 : (IndependentUsesLeft(Grouping, Level) ? 1 : 0);
			Cell.Needle = IndependentNeedle(Grouping, Level, Sequence);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EExprAssociativityGrouping;
		using AngelscriptTest::Generate::EExprAssociativityLevel;
		using AngelscriptTest::Generate::EExprAssociativitySequence;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FExprAssociativityGenerator;
		using AngelscriptTest::Generate::FExprAssociativityParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FExprAssociativityGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-EXPR-ASSOCIATIVITY")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangExprAssociativityUnparenthesizedMultiplicativeRepeatedOperator")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ExprAssociativityGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ExprAssociativityGenerator dump path=%s cases=72 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(60, CaseCount));
		ASSERT_THAT(AreEqual(72, Cases.Num()));
		ASSERT_THAT(AreEqual(72, ExpectedCells.Num()));
		ASSERT_THAT(AreEqual(12, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/ExprAssociativityGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 RejectCount = 0;
		int32 ZeroCount = 0;
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
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(RejectSource, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
				if (Representative.Contains(Expected.EntryName))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
			else
			{
				ASSERT_THAT(AreEqual(
					FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Cases[Index].bLimitedObservation));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("FPrecedenceProbe A(2, true);"))));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("return ObservePrecedence("))));
				if (Expected.Expected == 0)
				{
					++ZeroCount;
				}
				if (Representative.Contains(Expected.EntryName))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
		}
		ASSERT_THAT(AreEqual(12, RejectCount));
		ASSERT_THAT(IsTrue(ZeroCount > 0));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangExprAssociativityUnparenthesizedMultiplicativeRepeatedOperator()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangExprAssociativityUnparenthesizedMultiplicativeRepeatedOperator()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("struct FPrecedenceProbe"))));
		ASSERT_THAT(AreEqual(13, CountOccurrences(Dump, TEXT("struct FPrecedenceProbe"))));

		FExprAssociativityParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildPrecedenceSource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildPrecedenceSource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(Generator.BuildPrecedenceSource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildPrecedenceSource(DefaultParams).IsEmpty()));
		FExprAssociativityParams InvalidEnum;
		InvalidEnum.Level = static_cast<EExprAssociativityLevel>(12);
		ASSERT_THAT(IsTrue(Generator.BuildPrecedenceSource(InvalidEnum).IsEmpty()));
		FExprAssociativityParams RejectParams;
		RejectParams.Level = EExprAssociativityLevel::Assignment;
		ASSERT_THAT(IsTrue(Generator.BuildPrecedenceSource(RejectParams).IsEmpty()));
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
		ASSERT_THAT(AreEqual(ExpectedCells[0].Expected, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(OutputFile.EndsWith(TEXT("GeneratedCases/ExprAssociativityGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\ExprAssociativityGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(72, CountOccurrences(Dump, TEXT("// Case: LANG-EXPR-ASSOCIATIVITY-"))));
		ASSERT_THAT(AreEqual(12, CountOccurrences(Dump, TEXT("// Reject module"))));
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(IsTrue(Dump.Contains(FString::Printf(TEXT("// Case: %s"), *Expected.CaseId))));
			if (!Expected.bReject)
			{
				FString DumpBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Dump, Expected.EntryName, DumpBody)));
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
			TEXT("C:/no-such-root/ExprAssociativityGenerator.as"), Dump, ProbeError)));
		ASSERT_THAT(IsTrue(ProbeError.Len() > 0));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ExprAssociativityGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
