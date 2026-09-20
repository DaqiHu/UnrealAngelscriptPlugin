#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestExprPrecedenceGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ExprPrecedenceGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString ExpressionNeedle;
		int32 Expected = 0;
		bool bReject = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-EXPR-PRECEDENCE-UNPARENTHESIZED-MULTIPLICATIVE-ADDITIVE");
	static constexpr const TCHAR* ZeroCaseId = TEXT("LANG-EXPR-PRECEDENCE-RIGHT_PARENTHESIZED-MULTIPLICATIVE-ADDITIVE");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-CONDITION-IF-BOOL_LITERAL-TRUE");

	static const TCHAR* LevelToken(const int32 Level)
	{
		static const TCHAR* const kTokens[] = {
			TEXT("MULTIPLICATIVE"), TEXT("ADDITIVE"), TEXT("SHIFT"), TEXT("RELATIONAL"),
			TEXT("EQUALITY"), TEXT("BITWISE_AND"), TEXT("BITWISE_XOR"), TEXT("BITWISE_OR"),
			TEXT("LOGICAL_AND"), TEXT("LOGICAL_OR"), TEXT("CONDITIONAL"), TEXT("ASSIGNMENT")};
		return kTokens[Level];
	}

	static const TCHAR* LevelOp(const int32 Level)
	{
		static const TCHAR* const kOps[] = {
			TEXT("*"), TEXT("+"), TEXT("<<"), TEXT("<"), TEXT("=="), TEXT("&"),
			TEXT("^"), TEXT("|"), TEXT("&&"), TEXT("||"), TEXT("?"), TEXT("=")};
		return kOps[Level];
	}

	static bool IndependentContainsFork(const FString& Expression)
	{
		const bool bAssignment = Expression.Contains(TEXT(" = ")) || Expression.Contains(TEXT(" += "));
		const bool bCustomLogical =
			(Expression.Contains(TEXT("&&")) || Expression.Contains(TEXT("||"))) && !Expression.Contains(TEXT("Q"));
		const bool bConditionalWithComparison = Expression.Contains(TEXT("?"))
			&& (Expression.Contains(TEXT(" < ")) || Expression.Contains(TEXT(" >= "))
				|| Expression.Contains(TEXT(" == ")) || Expression.Contains(TEXT(" != ")));
		return bAssignment || bCustomLogical || bConditionalWithComparison;
	}

	static void IndependentForms(const int32 Left, const int32 Right, FString& Unparen, FString& LeftParen, FString& RightParen, bool& bUsesLeft)
	{
		const TCHAR* const L = LevelOp(Left);
		const TCHAR* const R = LevelOp(Right);
		bUsesLeft = false;
		if (Left <= 9 && Right <= 9)
		{
			Unparen = FString::Printf(TEXT("A %s B %s C"), L, R);
			LeftParen = FString::Printf(TEXT("(A %s B) %s C"), L, R);
			RightParen = FString::Printf(TEXT("A %s (B %s C)"), L, R);
			const bool bLeftBit = Left >= 5 && Left <= 7;
			const bool bRightBit = Right >= 5 && Right <= 7;
			const bool bLeftRel = Left == 3 || Left == 4;
			const bool bRightRel = Right == 3 || Right == 4;
			const bool bForkBit = (bLeftBit && bRightRel) || (bRightBit && bLeftRel);
			bUsesLeft = bForkBit ? !(Left <= Right) : (Left <= Right);
			return;
		}
		if (Left == 10 && Right == 10)
		{
			Unparen = TEXT("A ? B : C ? D : E");
			LeftParen = TEXT("(A ? B : C) ? D : E");
			RightParen = TEXT("A ? B : (C ? D : E)");
			return;
		}
		if (Left == 10 && Right == 11)
		{
			Unparen = TEXT("A ? B : C = D");
			LeftParen = TEXT("(A ? B : C) = D");
			RightParen = TEXT("A ? B : (C = D)");
			return;
		}
		if (Left == 11 && Right == 10)
		{
			Unparen = TEXT("A = B ? C : D");
			LeftParen = TEXT("(A = B) ? C : D");
			RightParen = TEXT("A = (B ? C : D)");
			return;
		}
		if (Left == 11 && Right == 11)
		{
			Unparen = TEXT("A = B = C");
			LeftParen = TEXT("(A = B) = C");
			RightParen = TEXT("A = (B = C)");
			return;
		}
		if (Left == 10)
		{
			Unparen = FString::Printf(TEXT("A ? B : C %s D"), R);
			LeftParen = FString::Printf(TEXT("(A ? B : C) %s D"), R);
			RightParen = FString::Printf(TEXT("A ? B : (C %s D)"), R);
			return;
		}
		if (Right == 10)
		{
			Unparen = FString::Printf(TEXT("A %s B ? C : D"), L);
			LeftParen = FString::Printf(TEXT("(A %s B) ? C : D"), L);
			RightParen = FString::Printf(TEXT("A %s (B ? C : D)"), L);
			bUsesLeft = true;
			return;
		}
		if (Left == 11)
		{
			Unparen = FString::Printf(TEXT("A = B %s C"), R);
			LeftParen = FString::Printf(TEXT("(A = B) %s C"), R);
			RightParen = FString::Printf(TEXT("A = (B %s C)"), R);
			return;
		}
		Unparen = FString::Printf(TEXT("A %s B ? C : D = E"), L);
		LeftParen = FString::Printf(TEXT("((A %s B) ? C : D) = E"), L);
		RightParen = FString::Printf(TEXT("(A %s B) ? C : (D = E)"), L);
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kGroupings[] = {
			TEXT("UNPARENTHESIZED"), TEXT("LEFT_PARENTHESIZED"), TEXT("RIGHT_PARENTHESIZED")};
		TArray<FExpectedCell> Cells;
		Cells.SetNum(432);
		for (int32 Index = 0; Index < 432; ++Index)
		{
			const int32 Grouping = Index / 144;
			const int32 Left = (Index / 12) % 12;
			const int32 Right = Index % 12;
			FString Unparen;
			FString LeftParen;
			FString RightParen;
			bool bUsesLeft = false;
			IndependentForms(Left, Right, Unparen, LeftParen, RightParen, bUsesLeft);
			const bool bReject = IndependentContainsFork(Unparen)
				|| IndependentContainsFork(LeftParen)
				|| IndependentContainsFork(RightParen);
			FString Selected = Unparen;
			bool bSelectedLeft = bUsesLeft;
			if (Grouping == 1)
			{
				Selected = LeftParen;
				bSelectedLeft = true;
			}
			else if (Grouping == 2)
			{
				Selected = RightParen;
				bSelectedLeft = false;
			}
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-EXPR-PRECEDENCE-%s-%s-%s"),
				kGroupings[Grouping],
				LevelToken(Left),
				LevelToken(Right));
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.ExpressionNeedle = Selected;
			Cell.bReject = bReject;
			Cell.Expected = bReject ? 0 : (bSelectedLeft ? 1 : 0);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EExprPrecedenceGrouping;
		using AngelscriptTest::Generate::EExprPrecedenceLevel;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FExprPrecedenceGenerator;
		using AngelscriptTest::Generate::FExprPrecedenceParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FExprPrecedenceGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-EXPR-PRECEDENCE")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangExprPrecedenceUnparenthesizedMultiplicativeAdditive")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ExprPrecedenceGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ExprPrecedenceGenerator dump path=%s cases=432 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(231, CaseCount));
		ASSERT_THAT(AreEqual(432, Cases.Num()));
		ASSERT_THAT(AreEqual(432, ExpectedCells.Num()));
		ASSERT_THAT(AreEqual(201, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/ExprPrecedenceGenerator/RepresentativeSources.as"));
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
				ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.ExpressionNeedle)));
				ASSERT_THAT(IsTrue(RejectSource.Contains(TEXT("return ObservePrecedence("))));
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
			}
			else
			{
				ASSERT_THAT(AreEqual(
					FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Cases[Index].bLimitedObservation));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.ExpressionNeedle)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("FPrecedenceProbe A(2, true);"))));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("return ObservePrecedence("))));
				if (Expected.Expected == 0)
				{
					++ZeroCount;
				}
			}
			if (Representative.Contains(Expected.EntryName))
			{
				const FString CaseSource = Expected.bReject
					? Generator.BuildRejectSource(Expected.CaseId)
					: Generator.BuildCaseSource(Expected.CaseId);
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}
		ASSERT_THAT(AreEqual(201, RejectCount));
		ASSERT_THAT(IsTrue(ZeroCount > 0));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangExprPrecedenceUnparenthesizedMultiplicativeAdditive()"))));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("A * B + C"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("struct FPrecedenceProbe"))));

		FExprPrecedenceParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildPrecedenceSource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildPrecedenceSource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(Generator.BuildPrecedenceSource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildPrecedenceSource(DefaultParams).IsEmpty()));
		DefaultParams.FunctionName.Reset();
		DefaultParams.Left = static_cast<EExprPrecedenceLevel>(12);
		ASSERT_THAT(IsTrue(Generator.BuildPrecedenceSource(DefaultParams).IsEmpty()));
		FExprPrecedenceParams RejectParams;
		RejectParams.Left = EExprPrecedenceLevel::LogicalAnd;
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

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(432, CountOccurrences(Dump, TEXT("// Case: LANG-EXPR-PRECEDENCE-"))));
		ASSERT_THAT(AreEqual(201, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/ExprPrecedenceGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ExprPrecedenceGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
