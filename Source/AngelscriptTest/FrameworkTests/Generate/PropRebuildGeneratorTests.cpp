#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestPropRebuildGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	PropRebuildGenerator,
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
		bool bSourceOnly = false;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-PROP-REBUILD-METADATA-REBUILD-STORED_SAME_SOURCE");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static bool IndependentIndexed(const TCHAR* Scenario)
	{
		return FCString::Strcmp(Scenario, TEXT("REGISTERED_INDEXED_SAME_SOURCE")) == 0
			|| FCString::Strcmp(Scenario, TEXT("REGISTERED_INDEXED_VALUE")) == 0
			|| FCString::Strcmp(Scenario, TEXT("REGISTERED_INDEXED_CONSTNESS")) == 0
			|| FCString::Strcmp(Scenario, TEXT("REGISTERED_INDEXED_INDEX_TYPE")) == 0
			|| FCString::Strcmp(Scenario, TEXT("REGISTERED_INDEXED_OVERLOAD_SET")) == 0;
	}

	static int32 IndependentExpected(const TCHAR* Scenario)
	{
		if (FCString::Strcmp(Scenario, TEXT("REGISTERED_SETTER_PRESENCE")) == 0)
		{
			return 73;
		}
		const bool bChanged =
			FCString::Strcmp(Scenario, TEXT("STORED_VALUE")) == 0
			|| FCString::Strcmp(Scenario, TEXT("STORED_FIELD_TYPE")) == 0
			|| FCString::Strcmp(Scenario, TEXT("STORED_FIELD_ORDER")) == 0
			|| FCString::Strcmp(Scenario, TEXT("STORED_INHERITANCE")) == 0
			|| FCString::Strcmp(Scenario, TEXT("REGISTERED_READ_VALUE")) == 0
			|| FCString::Strcmp(Scenario, TEXT("REGISTERED_INDEXED_VALUE")) == 0;
		const int32 BaseValue = bChanged ? 29 : 11;
		return IndependentIndexed(Scenario) ? BaseValue + 1 : BaseValue;
	}

	static FString IndependentNeedle(const TCHAR* Scenario)
	{
		if (FCString::Strcmp(Scenario, TEXT("REGISTERED_SETTER_PRESENCE")) == 0)
		{
			return TEXT("Receiver.Value = 73;");
		}
		if (FCString::Strcmp(Scenario, TEXT("REGISTERED_INDEXED_INDEX_TYPE")) == 0)
		{
			return TEXT("\tuint Index = uint(1);");
		}
		if (IndependentIndexed(Scenario))
		{
			return TEXT("\tint Index = int(1);");
		}
		if (FCString::Strncmp(Scenario, TEXT("STORED_"), 7) == 0)
		{
			return TEXT("return int(Receiver.Stored);");
		}
		return TEXT("return Receiver.Value;");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kObservations[] = {
			TEXT("METADATA"), TEXT("RUNTIME"), TEXT("OLD_HANDLE_CLEANUP")};
		static const TCHAR* const kPaths[] = {TEXT("REBUILD"), TEXT("SAVE_LOAD")};
		static const TCHAR* const kScenarios[] = {
			TEXT("STORED_SAME_SOURCE"), TEXT("STORED_VALUE"), TEXT("STORED_FIELD_TYPE"),
			TEXT("STORED_FIELD_ORDER"), TEXT("STORED_INHERITANCE"), TEXT("REGISTERED_SAME_SOURCE"),
			TEXT("REGISTERED_READ_VALUE"), TEXT("REGISTERED_GETTER_PRESENCE"),
			TEXT("REGISTERED_SETTER_PRESENCE"), TEXT("REGISTERED_CONSTNESS"),
			TEXT("REGISTERED_INDEXED_SAME_SOURCE"), TEXT("REGISTERED_INDEXED_VALUE"),
			TEXT("REGISTERED_INDEXED_CONSTNESS"), TEXT("REGISTERED_INDEXED_INDEX_TYPE"),
			TEXT("REGISTERED_INDEXED_OVERLOAD_SET")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(90);
		for (int32 Index = 0; Index < 90; ++Index)
		{
			const TCHAR* const Observation = kObservations[Index / 30];
			const TCHAR* const Path = kPaths[(Index / 15) % 2];
			const TCHAR* const Scenario = kScenarios[Index % 15];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-PROP-REBUILD-%s-%s-%s"), Observation, Path, Scenario);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Expected = IndependentExpected(Scenario);
			Cell.Needle = IndependentNeedle(Scenario);
			Cell.bSourceOnly = FCString::Strcmp(Observation, TEXT("RUNTIME")) != 0;
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EPropRebuildObservation;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FPropRebuildGenerator;
		using AngelscriptTest::Generate::FPropRebuildParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FPropRebuildGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-PROP-REBUILD")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangPropRebuildMetadataRebuildStoredSameSource")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("PropRebuildGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("PropRebuildGenerator dump path=%s cases=90 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(90, CaseCount));
		ASSERT_THAT(AreEqual(90, Cases.Num()));

		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			ASSERT_THAT(IsTrue(Cases[Index].bLimitedObservation));
			ASSERT_THAT(IsTrue(Cases[Index].Execution == (Expected.bSourceOnly
				? EGeneratedCaseExecution::SourceOnly
				: EGeneratedCaseExecution::Standalone)));
			ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("Second-version"))));
		}

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangPropRebuildMetadataRebuildStoredSameSource()"))));

		FPropRebuildParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildScenarioSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FPropRebuildParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildScenarioSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/PropRebuildGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
			if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FPropRebuildParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildScenarioSource(InvalidName).IsEmpty()));
		FPropRebuildParams InvalidEnum = DefaultParams;
		InvalidEnum.Observation = static_cast<EPropRebuildObservation>(3);
		ASSERT_THAT(IsTrue(Generator.BuildScenarioSource(InvalidEnum).IsEmpty()));
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
		ASSERT_THAT(AreEqual(90, CountOccurrences(Dump, TEXT("// Case: LANG-PROP-REBUILD-"))));
		ASSERT_THAT(AreEqual(0, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/PropRebuildGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("PropRebuildGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
