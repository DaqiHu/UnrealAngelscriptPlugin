#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestOpOverloadBooleanConsumerGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	OpOverloadBooleanConsumerGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString TypeNeedle;
		FString OperationNeedle;
		int32 Expected = 0;
		bool bReject = false;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-OP-OVERLOAD-BOOLEAN-CONSUMER-COMPARISON_INT_PARAMETER-ASSIGNMENT");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-OP-NUMERIC-BINARY-INT-ADD-INT-ZERO");

	static const TCHAR* ScenarioToken(const int32 Index)
	{
		static const TCHAR* const kTokens[] = {
			TEXT("COMPARISON_INT_PARAMETER"),
			TEXT("COMPARISON_VALUE_PARAMETER"),
			TEXT("COMPARISON_AMBIGUOUS_INT8"),
			TEXT("COMPARISON_MISSING_LESS")};
		return kTokens[Index];
	}

	static const TCHAR* ContextToken(const int32 Index)
	{
		static const TCHAR* const kTokens[] = {
			TEXT("ASSIGNMENT"), TEXT("RETURN"), TEXT("CONDITION"), TEXT("OVERLOAD_ARGUMENT"), TEXT("CHAIN")};
		return kTokens[Index];
	}

	static int32 IndependentMarker(const int32 Scenario)
	{
		static const int32 kMarkers[] = {107, 108, 202, 303};
		return kMarkers[Scenario];
	}

	static FString IndependentTypeNeedle(const int32 Scenario)
	{
		static const TCHAR* const kNames[] = {
			TEXT("FOverloadedBooleanComparisonIntParameter"),
			TEXT("FOverloadedBooleanComparisonValueParameter"),
			TEXT("FOverloadedBooleanComparisonAmbiguousInt8"),
			TEXT("FOverloadedBooleanComparisonMissingLess")};
		return kNames[Scenario];
	}

	static FString IndependentOperationNeedle(const int32 Scenario)
	{
		if (Scenario == 0)
		{
			return TEXT("Left == 6");
		}
		if (Scenario == 1)
		{
			return TEXT("Left == Right");
		}
		if (Scenario == 2)
		{
			return TEXT("Left == int8(6)");
		}
		return TEXT("Left < Right");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		TArray<FExpectedCell> Cells;
		Cells.SetNum(20);
		for (int32 Index = 0; Index < 20; ++Index)
		{
			const int32 Scenario = Index / 5;
			const int32 Context = Index % 5;
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-OP-OVERLOAD-BOOLEAN-CONSUMER-%s-%s"),
				ScenarioToken(Scenario),
				ContextToken(Context));
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.TypeNeedle = IndependentTypeNeedle(Scenario);
			Cell.OperationNeedle = IndependentOperationNeedle(Scenario);
			Cell.bReject = Scenario >= 2;
			Cell.Expected = Cell.bReject ? 0 : IndependentMarker(Scenario);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EOpOverloadBooleanConsumerScenario;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FOpOverloadBooleanConsumerGenerator;
		using AngelscriptTest::Generate::FOpOverloadBooleanConsumerParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FOpOverloadBooleanConsumerGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-OP-OVERLOAD-BOOLEAN-CONSUMER")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangOpOverloadBooleanConsumerComparisonIntParameterAssignment")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("OpOverloadBooleanConsumerGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("OpOverloadBooleanConsumerGenerator dump path=%s cases=20 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(10, CaseCount));
		ASSERT_THAT(AreEqual(20, Cases.Num()));
		ASSERT_THAT(AreEqual(10, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/OpOverloadBooleanConsumerGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 RejectCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			const FString CaseSource = Expected.bReject
				? Generator.BuildRejectSource(Expected.CaseId)
				: Generator.BuildCaseSource(Expected.CaseId);
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.TypeNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.OperationNeedle)));
			if (Expected.bReject)
			{
				++RejectCount;
				ASSERT_THAT(AreEqual(FString(), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::CompileReject));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::SourceOnly));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(RejectIds.Contains(Expected.CaseId)));
				ASSERT_THAT(IsTrue(CaseSource.Contains(TEXT("OP_OVERLOAD_OPERATION_CAUSE"))));
			}
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Cases[Index].bLimitedObservation));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
			}
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}
		ASSERT_THAT(AreEqual(10, RejectCount));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("bool ReturnBoolMarker"))));

		FOpOverloadBooleanConsumerParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildScenarioSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FOpOverloadBooleanConsumerParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildScenarioSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildScenarioSource(ProbeParams).IsEmpty()));
		FOpOverloadBooleanConsumerParams RejectParams = DefaultParams;
		RejectParams.Scenario = EOpOverloadBooleanConsumerScenario::ComparisonMissingLess;
		ASSERT_THAT(IsTrue(Generator.BuildScenarioSource(RejectParams).IsEmpty()));
		RejectParams.Scenario = static_cast<EOpOverloadBooleanConsumerScenario>(4);
		ASSERT_THAT(IsTrue(Generator.BuildScenarioSource(RejectParams).IsEmpty()));
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
		ASSERT_THAT(AreEqual(20, CountOccurrences(Dump, TEXT("// Case: LANG-OP-OVERLOAD-BOOLEAN-CONSUMER-"))));
		ASSERT_THAT(AreEqual(10, CountOccurrences(Dump, TEXT("// Reject module"))));
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

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/OpOverloadBooleanConsumerGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("OpOverloadBooleanConsumerGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
