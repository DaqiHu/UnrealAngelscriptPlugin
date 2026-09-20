#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestDtorOwnerExitGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	DtorOwnerExitGenerator,
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
		bool bFault = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-DTOR-OWNER-EXIT-LOCAL_BLOCK_END-ONE-EVENT_ORDER");

	static bool IndependentFault(const TCHAR* Scenario)
	{
		return FCString::Strcmp(Scenario, TEXT("LOCAL_EXCEPTION")) == 0
			|| FCString::Strcmp(Scenario, TEXT("NESTED_LOCAL_EXCEPTION")) == 0
			|| FCString::Strcmp(Scenario, TEXT("FIELD_EXCEPTION")) == 0
			|| FCString::Strcmp(Scenario, TEXT("BASE_DERIVED_EXCEPTION")) == 0
			|| FCString::Strcmp(Scenario, TEXT("TEMPORARY_EXCEPTION")) == 0
			|| FCString::Strcmp(Scenario, TEXT("RETURNED_VALUE_EXCEPTION")) == 0
			|| FCString::Strcmp(Scenario, TEXT("ARGUMENT_COPY_EXCEPTION")) == 0
			|| FCString::Strcmp(Scenario, TEXT("REFERENCE_EXCEPTION")) == 0;
	}

	static FString IndependentNeedle(const TCHAR* Scenario, const TCHAR* Nesting, const int32 Route)
	{
		if (FCString::Strcmp(Scenario, TEXT("MODULE_DISCARD_GLOBAL")) == 0
			|| FCString::Strcmp(Scenario, TEXT("ENGINE_SHUTDOWN_GLOBAL")) == 0)
		{
			return FString::Printf(TEXT("return %d;"), Route);
		}
		if (FCString::Strcmp(Nesting, TEXT("NESTED_CALLS")) == 0)
		{
			return TEXT("Call1()");
		}
		if (FCString::Strcmp(Nesting, TEXT("RECURSION")) == 0)
		{
			return TEXT("Recursive(");
		}
		if (FCString::Strcmp(Scenario, TEXT("ARGUMENT_COPY_RETURN")) == 0
			|| FCString::Strcmp(Scenario, TEXT("ARGUMENT_COPY_ABORT")) == 0)
		{
			return TEXT("ConsumeExitValueByValue(");
		}
		if (IndependentFault(Scenario))
		{
			return TEXT("1 / Zero");
		}
		return FString::Printf(TEXT("ReachDestructorExitTerminal(%d)"), Route);
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kScenarios[] = {
			TEXT("LOCAL_BLOCK_END"), TEXT("LOCAL_RETURN"), TEXT("LOCAL_EARLY_RETURN"), TEXT("LOCAL_BREAK"),
			TEXT("LOCAL_CONTINUE"), TEXT("LOCAL_SWITCH_EXIT"), TEXT("LOCAL_EXCEPTION"), TEXT("LOCAL_ABORT"),
			TEXT("LOCAL_UNPREPARE"), TEXT("NESTED_LOCAL_BLOCK_END"), TEXT("NESTED_LOCAL_RETURN"),
			TEXT("NESTED_LOCAL_EXCEPTION"), TEXT("FIELD_BLOCK_END"), TEXT("FIELD_RETURN"), TEXT("FIELD_EXCEPTION"),
			TEXT("FIELD_ABORT"), TEXT("BASE_DERIVED_BLOCK_END"), TEXT("BASE_DERIVED_RETURN"),
			TEXT("BASE_DERIVED_EXCEPTION"), TEXT("BASE_DERIVED_ABORT"), TEXT("TEMPORARY_STATEMENT_END"),
			TEXT("TEMPORARY_EARLY_RETURN"), TEXT("TEMPORARY_EXCEPTION"), TEXT("RETURNED_VALUE_CONSUME"),
			TEXT("RETURNED_VALUE_DISCARD"), TEXT("RETURNED_VALUE_EXCEPTION"), TEXT("ARGUMENT_COPY_RETURN"),
			TEXT("ARGUMENT_COPY_EXCEPTION"), TEXT("ARGUMENT_COPY_ABORT"), TEXT("REFERENCE_SCOPE_END"),
			TEXT("REFERENCE_ALIAS_SCOPE_END"), TEXT("REFERENCE_RETURN"), TEXT("REFERENCE_EXCEPTION"),
			TEXT("REFERENCE_ABORT"), TEXT("REFERENCE_UNPREPARE"), TEXT("MODULE_DISCARD_GLOBAL"),
			TEXT("ENGINE_SHUTDOWN_GLOBAL")};
		static const TCHAR* const kNestings[] = {
			TEXT("ONE"), TEXT("SEQUENTIAL"), TEXT("NESTED_SCOPES"), TEXT("NESTED_CALLS"), TEXT("LOOP"),
			TEXT("RECURSION")};
		static const TCHAR* const kObservations[] = {
			TEXT("EVENT_ORDER"), TEXT("OWNERSHIP_ONCE"), TEXT("TERMINAL_STATE_RECOVERY")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(666);
		for (int32 Index = 0; Index < 666; ++Index)
		{
			const TCHAR* const Scenario = kScenarios[Index / 18];
			const TCHAR* const Nesting = kNestings[(Index / 3) % 6];
			const TCHAR* const Observation = kObservations[Index % 3];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-DTOR-OWNER-EXIT-%s-%s-%s"), Scenario, Nesting, Observation);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bFault = IndependentFault(Scenario);
			const int32 Route = (Index / 18) + 1;
			Cell.Expected = Cell.bFault ? 0 : Route;
			Cell.Needle = IndependentNeedle(Scenario, Nesting, Route);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EDtorOwnerExitScenario;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FDtorOwnerExitGenerator;
		using AngelscriptTest::Generate::FDtorOwnerExitParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FDtorOwnerExitGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-DTOR-OWNER-EXIT")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangDtorOwnerExitLocalBlockEndOneEventOrder")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("DtorOwnerExitGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("DtorOwnerExitGenerator dump path=%s cases=666 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(666, CaseCount));
		ASSERT_THAT(AreEqual(666, Cases.Num()));

		int32 FaultCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			if (Expected.bFault)
			{
				++FaultCount;
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")), Cases[Index].ExpectedException));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
			}
			else
			{
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			}
		}
		ASSERT_THAT(AreEqual(144, FaultCount));

		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangDtorOwnerExitLocalBlockEndOneEventOrder()"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("struct FExitValue\n{"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int ObserveExitValue(const FExitValue& Value)"))));

		FDtorOwnerExitParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildDestructorExitSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FDtorOwnerExitParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildDestructorExitSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(Generator.BuildDestructorExitSource(ProbeParams).Contains(TEXT("int Entry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/DtorOwnerExitGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody), *Expected.CaseId));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle), *Expected.Needle));
			if (Expected.bFault)
			{
				ASSERT_THAT(IsTrue(
					GeneratedBody.Contains(TEXT("1 / Zero"))
					|| Generator.BuildCaseSource(Expected.CaseId).Contains(TEXT("1 / Zero"))));
			}
			ASSERT_THAT(AreEqual(1, CountOccurrences(Dump, FString::Printf(TEXT("// Case: %s\n"), *Expected.CaseId))));
			if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FDtorOwnerExitParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildDestructorExitSource(InvalidName).IsEmpty()));
		FDtorOwnerExitParams InvalidEnum = DefaultParams;
		InvalidEnum.Scenario = static_cast<EDtorOwnerExitScenario>(37);
		ASSERT_THAT(IsTrue(Generator.BuildDestructorExitSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-DTOR-DECLARATION-IMPLICIT_SCRIPT_VALUE-COMPILE")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(AreEqual(1, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(666, CountOccurrences(Dump, TEXT("// Case: "))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/DtorOwnerExitGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("DtorOwnerExitGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
