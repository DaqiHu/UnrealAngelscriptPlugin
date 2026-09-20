#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestOpComparisonReferenceGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	OpComparisonReferenceGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString Glyph;
		FString RelationNeedle;
		int32 Expected = 0;
		bool bReject = false;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-OP-COMPARISON-REFERENCE-EQUAL-SAME_NON_NULL-LEFT_RIGHT");
	static constexpr const TCHAR* ZeroCaseId =
		TEXT("LANG-OP-COMPARISON-REFERENCE-EQUAL-DIFFERENT_NON_NULL-LEFT_RIGHT");
	static constexpr const TCHAR* ForeignCaseId =
		TEXT("LANG-OP-LOGICAL-NOT-MUTABLE_LVALUE-FALSE");

	static bool IndependentOrdering(const TCHAR* Operator)
	{
		return FCString::Strcmp(Operator, TEXT("LESS")) == 0
			|| FCString::Strcmp(Operator, TEXT("LESS_EQUAL")) == 0
			|| FCString::Strcmp(Operator, TEXT("GREATER")) == 0
			|| FCString::Strcmp(Operator, TEXT("GREATER_EQUAL")) == 0;
	}

	static bool IndependentEqual(const TCHAR* Relation)
	{
		return FCString::Strcmp(Relation, TEXT("SAME_NON_NULL")) == 0
			|| FCString::Strcmp(Relation, TEXT("BOTH_NULL")) == 0
			|| FCString::Strcmp(Relation, TEXT("DERIVED_BASE_SAME")) == 0
			|| FCString::Strcmp(Relation, TEXT("CONST_SAME")) == 0;
	}

	static FString IndependentGlyph(const TCHAR* Operator)
	{
		if (FCString::Strcmp(Operator, TEXT("LESS")) == 0)
		{
			return TEXT("<");
		}
		if (FCString::Strcmp(Operator, TEXT("LESS_EQUAL")) == 0)
		{
			return TEXT("<=");
		}
		if (FCString::Strcmp(Operator, TEXT("GREATER")) == 0)
		{
			return TEXT(">");
		}
		if (FCString::Strcmp(Operator, TEXT("GREATER_EQUAL")) == 0)
		{
			return TEXT(">=");
		}
		if (FCString::Strcmp(Operator, TEXT("NOT_EQUAL")) == 0)
		{
			return TEXT("!=");
		}
		return TEXT("==");
	}

	static FString IndependentRelationNeedle(const TCHAR* Relation)
	{
		if (FCString::Strcmp(Relation, TEXT("SAME_NON_NULL")) == 0)
		{
			return TEXT("FComparisonReferenceRoot Right = Left;");
		}
		if (FCString::Strcmp(Relation, TEXT("DIFFERENT_NON_NULL")) == 0)
		{
			return TEXT("FComparisonReferenceRoot Right = MakeComparisonRoot();");
		}
		if (FCString::Strcmp(Relation, TEXT("LEFT_NULL")) == 0)
		{
			return TEXT("FComparisonReferenceRoot Left = nullptr;");
		}
		if (FCString::Strcmp(Relation, TEXT("RIGHT_NULL")) == 0)
		{
			return TEXT("FComparisonReferenceRoot Right = nullptr;");
		}
		if (FCString::Strcmp(Relation, TEXT("BOTH_NULL")) == 0)
		{
			return TEXT("FComparisonReferenceRoot Left = nullptr;");
		}
		if (FCString::Strcmp(Relation, TEXT("DERIVED_BASE_SAME")) == 0)
		{
			return TEXT("MakeComparisonDerivedAsRoot()");
		}
		if (FCString::Strcmp(Relation, TEXT("SIBLING_DIFFERENT")) == 0)
		{
			return TEXT("MakeComparisonSiblingAsRoot()");
		}
		return TEXT("const FComparisonReferenceRoot Left = Mutable;");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kOperators[] = {
			TEXT("LESS"), TEXT("LESS_EQUAL"), TEXT("GREATER"), TEXT("GREATER_EQUAL"), TEXT("EQUAL"), TEXT("NOT_EQUAL")};
		static const TCHAR* const kRelations[] = {
			TEXT("SAME_NON_NULL"), TEXT("DIFFERENT_NON_NULL"), TEXT("LEFT_NULL"), TEXT("RIGHT_NULL"),
			TEXT("BOTH_NULL"), TEXT("DERIVED_BASE_SAME"), TEXT("SIBLING_DIFFERENT"), TEXT("CONST_SAME")};
		static const TCHAR* const kOrders[] = {TEXT("LEFT_RIGHT"), TEXT("RIGHT_LEFT")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(96);
		for (int32 Index = 0; Index < 96; ++Index)
		{
			const TCHAR* const Operator = kOperators[Index / 16];
			const TCHAR* const Relation = kRelations[(Index / 2) % 8];
			const TCHAR* const Order = kOrders[Index % 2];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-OP-COMPARISON-REFERENCE-%s-%s-%s"), Operator, Relation, Order);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Glyph = IndependentGlyph(Operator);
			Cell.RelationNeedle = IndependentRelationNeedle(Relation);
			Cell.bReject = IndependentOrdering(Operator);
			if (Cell.bReject)
			{
				Cell.Expected = 0;
			}
			else if (FCString::Strcmp(Operator, TEXT("EQUAL")) == 0)
			{
				Cell.Expected = IndependentEqual(Relation) ? 1 : 0;
			}
			else
			{
				Cell.Expected = IndependentEqual(Relation) ? 0 : 1;
			}
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EOpComparisonReferenceOperator;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FOpComparisonReferenceGenerator;
		using AngelscriptTest::Generate::FOpComparisonReferenceParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FOpComparisonReferenceGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-OP-COMPARISON-REFERENCE")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangOpComparisonReferenceEqualSameNonNullLeftRight")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("OpComparisonReferenceGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("OpComparisonReferenceGenerator dump path=%s cases=96 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(32, CaseCount));
		ASSERT_THAT(AreEqual(96, Cases.Num()));
		ASSERT_THAT(AreEqual(96, ExpectedCells.Num()));
		ASSERT_THAT(AreEqual(64, RejectIds.Num()));

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
				ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.Glyph)));
				ASSERT_THAT(IsTrue(RejectSource.Contains(TEXT("REFERENCE_COMPARE_CAUSE"))));
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
			}
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				if (Expected.Expected == 0)
				{
					++ZeroCount;
				}
			}
		}
		ASSERT_THAT(AreEqual(64, RejectCount));
		ASSERT_THAT(IsTrue(ZeroCount > 0));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangOpComparisonReferenceEqualSameNonNullLeftRight()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangOpComparisonReferenceEqualSameNonNullLeftRight()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int EntryLangOpComparisonReferenceLess"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("FComparisonReferenceRoot TraceReferenceOperand"))));

		FOpComparisonReferenceParams DefaultParams;
		const FString DefaultSingle = Generator.BuildReferenceComparisonSource(DefaultParams);
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Entry()"))));
		FOpComparisonReferenceParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildReferenceComparisonSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(Generator.BuildReferenceComparisonSource(ProbeParams).Contains(TEXT("int Entry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/OpComparisonReferenceGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			const FString CaseSource = Expected.bReject
				? Generator.BuildRejectSource(Expected.CaseId)
				: Generator.BuildCaseSource(Expected.CaseId);
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Glyph)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.RelationNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("REFERENCE_COMPARE_CAUSE"))));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FOpComparisonReferenceParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildReferenceComparisonSource(InvalidName).IsEmpty()));
		FOpComparisonReferenceParams InvalidEnum = DefaultParams;
		InvalidEnum.Operator = static_cast<EOpComparisonReferenceOperator>(6);
		ASSERT_THAT(IsTrue(Generator.BuildReferenceComparisonSource(InvalidEnum).IsEmpty()));
		FOpComparisonReferenceParams RejectParams = DefaultParams;
		RejectParams.Operator = EOpComparisonReferenceOperator::Less;
		ASSERT_THAT(IsTrue(Generator.BuildReferenceComparisonSource(RejectParams).IsEmpty()));
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
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(96, CountOccurrences(Dump, TEXT("// Case: LANG-OP-COMPARISON-REFERENCE-"))));
		ASSERT_THAT(AreEqual(64, CountOccurrences(Dump, TEXT("// Reject module"))));
		for (const FExpectedCell& Expected : ExpectedCells)
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
			TEXT("C:/no-such-root/OpComparisonReferenceGenerator.as"), Dump, ProbeError)));
		ASSERT_THAT(IsTrue(ProbeError.Len() > 0));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("OpComparisonReferenceGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
