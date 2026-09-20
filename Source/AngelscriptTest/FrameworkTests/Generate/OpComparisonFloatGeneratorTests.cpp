#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestOpComparisonFloatGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	OpComparisonFloatGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString OperatorNeedle;
		FString TypeNeedle;
		FString ValueNeedle;
		int32 Expected = 0;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-OP-COMPARISON-FLOAT-FLOAT32-LESS-NEGATIVE_ZERO-LEFT_RIGHT");
	static constexpr const TCHAR* ZeroCaseId =
		TEXT("LANG-OP-COMPARISON-FLOAT-FLOAT32-LESS-NEGATIVE_ZERO-LEFT_RIGHT");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-OP-ASSIGNMENT-LOCAL-ASSIGN_INT");

	static int32 IndependentRelation(const TCHAR* Value)
	{
		if (FCString::Strcmp(Value, TEXT("NAN")) == 0)
		{
			return 3;
		}
		if (FCString::Strcmp(Value, TEXT("POSITIVE_INFINITY")) == 0
			|| FCString::Strcmp(Value, TEXT("MAXIMUM")) == 0)
		{
			return 2;
		}
		if (FCString::Strcmp(Value, TEXT("NEGATIVE_INFINITY")) == 0
			|| FCString::Strcmp(Value, TEXT("MINIMUM")) == 0)
		{
			return 0;
		}
		return 1;
	}

	static int32 IndependentExpected(const TCHAR* Operator, const TCHAR* Value, const TCHAR* Order)
	{
		int32 Relation = IndependentRelation(Value);
		if (FCString::Strcmp(Order, TEXT("RIGHT_LEFT")) == 0)
		{
			if (Relation == 0)
			{
				Relation = 2;
			}
			else if (Relation == 2)
			{
				Relation = 0;
			}
		}
		if (Relation == 3)
		{
			return (FCString::Strcmp(Operator, TEXT("GREATER")) == 0
				|| FCString::Strcmp(Operator, TEXT("GREATER_EQUAL")) == 0
				|| FCString::Strcmp(Operator, TEXT("NOT_EQUAL")) == 0)
				? 1
				: 0;
		}
		if (FCString::Strcmp(Operator, TEXT("LESS")) == 0)
		{
			return Relation == 0 ? 1 : 0;
		}
		if (FCString::Strcmp(Operator, TEXT("LESS_EQUAL")) == 0)
		{
			return Relation <= 1 ? 1 : 0;
		}
		if (FCString::Strcmp(Operator, TEXT("GREATER")) == 0)
		{
			return Relation == 2 ? 1 : 0;
		}
		if (FCString::Strcmp(Operator, TEXT("GREATER_EQUAL")) == 0)
		{
			return Relation >= 1 ? 1 : 0;
		}
		if (FCString::Strcmp(Operator, TEXT("EQUAL")) == 0)
		{
			return Relation == 1 ? 1 : 0;
		}
		return Relation != 1 ? 1 : 0;
	}

	static FString IndependentOperatorNeedle(const TCHAR* Operator)
	{
		if (FCString::Strcmp(Operator, TEXT("LESS")) == 0)
		{
			return TEXT(") < ");
		}
		if (FCString::Strcmp(Operator, TEXT("LESS_EQUAL")) == 0)
		{
			return TEXT(") <= ");
		}
		if (FCString::Strcmp(Operator, TEXT("GREATER")) == 0)
		{
			return TEXT(") > ");
		}
		if (FCString::Strcmp(Operator, TEXT("GREATER_EQUAL")) == 0)
		{
			return TEXT(") >= ");
		}
		if (FCString::Strcmp(Operator, TEXT("EQUAL")) == 0)
		{
			return TEXT(") == ");
		}
		return TEXT(") != ");
	}

	static FString IndependentValueNeedle(const TCHAR* Type, const TCHAR* Value)
	{
		const bool b32 = FCString::Strcmp(Type, TEXT("FLOAT32")) == 0;
		if (FCString::Strcmp(Value, TEXT("NEGATIVE_ZERO")) == 0)
		{
			return b32 ? TEXT("-0.0f") : TEXT("-0.0");
		}
		if (FCString::Strcmp(Value, TEXT("NAN")) == 0)
		{
			return b32 ? TEXT("0.0f / 0.0f") : TEXT("0.0 / 0.0");
		}
		if (FCString::Strcmp(Value, TEXT("POSITIVE_INFINITY")) == 0)
		{
			return b32 ? TEXT("1.0f / 0.0f") : TEXT("1.0 / 0.0");
		}
		if (FCString::Strcmp(Value, TEXT("NEGATIVE_INFINITY")) == 0)
		{
			return b32 ? TEXT("-1.0f / 0.0f") : TEXT("-1.0 / 0.0");
		}
		if (FCString::Strcmp(Value, TEXT("MINIMUM")) == 0)
		{
			return b32 ? TEXT("-3.402823466e+38f") : TEXT("-1.7976931348623157e+308");
		}
		if (FCString::Strcmp(Value, TEXT("MAXIMUM")) == 0)
		{
			return b32 ? TEXT("3.402823466e+38f") : TEXT("1.7976931348623157e+308");
		}
		if (FCString::Strcmp(Value, TEXT("EQUAL_PAIR")) == 0)
		{
			return b32 ? TEXT("13.25f") : TEXT("13.25");
		}
		return b32 ? TEXT("0.0f") : TEXT("0.0");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kTypes[] = {TEXT("FLOAT32"), TEXT("FLOAT64")};
		static const TCHAR* const kOperators[] = {
			TEXT("LESS"), TEXT("LESS_EQUAL"), TEXT("GREATER"), TEXT("GREATER_EQUAL"), TEXT("EQUAL"), TEXT("NOT_EQUAL")};
		static const TCHAR* const kValues[] = {
			TEXT("NEGATIVE_ZERO"),
			TEXT("POSITIVE_ZERO"),
			TEXT("NAN"),
			TEXT("POSITIVE_INFINITY"),
			TEXT("NEGATIVE_INFINITY"),
			TEXT("MINIMUM"),
			TEXT("MAXIMUM"),
			TEXT("EQUAL_PAIR")};
		static const TCHAR* const kOrders[] = {TEXT("LEFT_RIGHT"), TEXT("RIGHT_LEFT")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(192);
		for (int32 Index = 0; Index < 192; ++Index)
		{
			const TCHAR* const Type = kTypes[Index / 96];
			const TCHAR* const Operator = kOperators[(Index / 16) % 6];
			const TCHAR* const Value = kValues[(Index / 2) % 8];
			const TCHAR* const Order = kOrders[Index % 2];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-OP-COMPARISON-FLOAT-%s-%s-%s-%s"), Type, Operator, Value, Order);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.OperatorNeedle = IndependentOperatorNeedle(Operator);
			Cell.TypeNeedle = FCString::Strcmp(Type, TEXT("FLOAT32")) == 0 ? TEXT("float32") : TEXT("float64");
			Cell.ValueNeedle = IndependentValueNeedle(Type, Value);
			Cell.Expected = IndependentExpected(Operator, Value, Order);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EOpComparisonFloatType;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FOpComparisonFloatGenerator;
		using AngelscriptTest::Generate::FOpComparisonFloatParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FOpComparisonFloatGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-OP-COMPARISON-FLOAT")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangOpComparisonFloatFloat32LessNegativeZeroLeftRight")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("OpComparisonFloatGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("OpComparisonFloatGenerator dump path=%s cases=192 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(192, CaseCount));
		ASSERT_THAT(AreEqual(192, Cases.Num()));
		ASSERT_THAT(AreEqual(192, ExpectedCells.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		int32 RejectCount = 0;
		int32 ZeroCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			if (Expected.Expected == 0)
			{
				++ZeroCount;
			}
			if (Cases[Index].Outcome == EGeneratedCaseOutcome::CompileReject)
			{
				++RejectCount;
			}
		}
		ASSERT_THAT(AreEqual(0, RejectCount));
		ASSERT_THAT(IsTrue(ZeroCount > 0));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/OpComparisonFloatGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.OperatorNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.TypeNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.ValueNeedle)));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FOpComparisonFloatParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildFloatComparisonSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FOpComparisonFloatParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildFloatComparisonSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildFloatComparisonSource(ProbeParams).IsEmpty()));
		FOpComparisonFloatParams InvalidEnum = DefaultParams;
		InvalidEnum.Type = static_cast<EOpComparisonFloatType>(2);
		ASSERT_THAT(IsTrue(Generator.BuildFloatComparisonSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
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

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(192, CountOccurrences(Dump, TEXT("// Case: LANG-OP-COMPARISON-FLOAT-"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/OpComparisonFloatGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("OpComparisonFloatGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
