#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestOpOverloadAssignmentConsumerGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	OpOverloadAssignmentConsumerGenerator,
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
		TEXT("LANG-OP-OVERLOAD-ASSIGNMENT-CONSUMER-ASSIGNMENT_INT_PARAMETER-ASSIGNMENT");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-OP-NUMERIC-BINARY-INT-ADD-INT-ZERO");

	static const TCHAR* ScenarioToken(const int32 Index)
	{
		static const TCHAR* const kTokens[] = {
			TEXT("ASSIGNMENT_INT_PARAMETER"),
			TEXT("ASSIGNMENT_VALUE_PARAMETER"),
			TEXT("ASSIGNMENT_AMBIGUOUS_INT8"),
			TEXT("ASSIGNMENT_MISSING_ADD_ASSIGN")};
		return kTokens[Index];
	}

	static const TCHAR* ContextToken(const int32 Index)
	{
		static const TCHAR* const kTokens[] = {
			TEXT("ASSIGNMENT"), TEXT("RETURN"), TEXT("CONDITION"),
			TEXT("OVERLOAD_ARGUMENT"), TEXT("CHAIN"), TEXT("SWITCH_INDEX")};
		return kTokens[Index];
	}

	static int32 IndependentMarker(const int32 Scenario)
	{
		static const int32 kMarkers[] = {114, 115, 206, 307};
		return kMarkers[Scenario];
	}

	static FString IndependentTypeNeedle(const int32 Scenario)
	{
		static const TCHAR* const kNames[] = {
			TEXT("FOverloadedAssignmentIntParameter"),
			TEXT("FOverloadedAssignmentValueParameter"),
			TEXT("FOverloadedAssignmentAmbiguousInt8"),
			TEXT("FOverloadedAssignmentMissingAddAssign")};
		return kNames[Scenario];
	}

	static FString IndependentOperationNeedle(const int32 Scenario)
	{
		if (Scenario == 0)
		{
			return TEXT("Left = 6");
		}
		if (Scenario == 1)
		{
			return TEXT("Left = Right");
		}
		if (Scenario == 2)
		{
			return TEXT("Left = int8(6)");
		}
		return TEXT("Left += Right");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		TArray<FExpectedCell> Cells;
		Cells.SetNum(24);
		for (int32 Index = 0; Index < 24; ++Index)
		{
			const int32 Scenario = Index / 6;
			const int32 Context = Index % 6;
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-OP-OVERLOAD-ASSIGNMENT-CONSUMER-%s-%s"),
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
		using AngelscriptTest::Generate::EOpOverloadAssignmentConsumerScenario;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FOpOverloadAssignmentConsumerGenerator;
		using AngelscriptTest::Generate::FOpOverloadAssignmentConsumerParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FOpOverloadAssignmentConsumerGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-OP-OVERLOAD-ASSIGNMENT-CONSUMER")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangOpOverloadAssignmentConsumerAssignmentIntParameterAssignment")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("OpOverloadAssignmentConsumerGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("OpOverloadAssignmentConsumerGenerator dump path=%s cases=24 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(12, CaseCount));
		ASSERT_THAT(AreEqual(24, Cases.Num()));
		ASSERT_THAT(AreEqual(12, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/OpOverloadAssignmentConsumerGenerator/RepresentativeSources.as"));
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
				ASSERT_THAT(AreEqual(CaseSource, Generator.BuildCaseSource(Expected.CaseId)));
				ASSERT_THAT(IsTrue(CaseSource.Contains(TEXT("OP_OVERLOAD_OPERATION_CAUSE"))));
			}
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
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
		ASSERT_THAT(AreEqual(12, RejectCount));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("OP_OVERLOAD_OPERATION_CAUSE"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("struct FOverloadedAssignmentIntParameter"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("struct FOverloadedAssignmentValueParameter"))));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangOpOverloadAssignmentConsumerAssignmentIntParameterAssignment()"))));

		FOpOverloadAssignmentConsumerParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildScenarioSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FOpOverloadAssignmentConsumerParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildScenarioSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildScenarioSource(ProbeParams).IsEmpty()));
		FOpOverloadAssignmentConsumerParams RejectParams = DefaultParams;
		RejectParams.Scenario = EOpOverloadAssignmentConsumerScenario::AssignmentAmbiguousInt8;
		ASSERT_THAT(IsTrue(Generator.BuildScenarioSource(RejectParams).IsEmpty()));
		RejectParams.Scenario = static_cast<EOpOverloadAssignmentConsumerScenario>(4);
		ASSERT_THAT(IsTrue(Generator.BuildScenarioSource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(24, CountOccurrences(Dump, TEXT("// Case: LANG-OP-OVERLOAD-ASSIGNMENT-CONSUMER-"))));
		ASSERT_THAT(AreEqual(12, CountOccurrences(Dump, TEXT("// Reject module"))));
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
			TEXT("C:/no-such-root/OpOverloadAssignmentConsumerGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("OpOverloadAssignmentConsumerGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
