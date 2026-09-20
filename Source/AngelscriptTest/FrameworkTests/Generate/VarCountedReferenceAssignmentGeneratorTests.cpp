#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestVarCountedReferenceAssignmentGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	VarCountedReferenceAssignmentGenerator,
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
		bool bSaveLoad = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-VAR-COUNTED-REFERENCE-ASSIGNMENT-FACTORY_LOCAL");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static int32 IndependentExpected(const TCHAR* Scenario)
	{
		if (FCString::Strcmp(Scenario, TEXT("FACTORY_LOCAL")) == 0
			|| FCString::Strcmp(Scenario, TEXT("SAVE_LOAD")) == 0)
		{
			return 410;
		}
		if (FCString::Strcmp(Scenario, TEXT("OVERWRITE")) == 0)
		{
			return 421;
		}
		if (FCString::Strcmp(Scenario, TEXT("NULL_ASSIGNMENT")) == 0)
		{
			return 1;
		}
		if (FCString::Strcmp(Scenario, TEXT("PARAMETER_RETURN")) == 0
			|| FCString::Strcmp(Scenario, TEXT("PARAMETER_RETURN_SAVE_LOAD")) == 0)
		{
			return 440;
		}
		return 0;
	}

	static FString IndependentNeedle(const TCHAR* Scenario)
	{
		if (FCString::Strcmp(Scenario, TEXT("FACTORY_LOCAL")) == 0
			|| FCString::Strcmp(Scenario, TEXT("SAVE_LOAD")) == 0)
		{
			return TEXT("CreateNativeCaseReference(410)");
		}
		if (FCString::Strcmp(Scenario, TEXT("OVERWRITE")) == 0)
		{
			return TEXT("CreateNativeCaseReference(421)");
		}
		if (FCString::Strcmp(Scenario, TEXT("NULL_ASSIGNMENT")) == 0)
		{
			return TEXT("Value = nullptr;");
		}
		if (FCString::Strcmp(Scenario, TEXT("PARAMETER_RETURN")) == 0
			|| FCString::Strcmp(Scenario, TEXT("PARAMETER_RETURN_SAVE_LOAD")) == 0)
		{
			return TEXT("PassCountedReference(Value)");
		}
		return TEXT("RaiseCountedReferenceException()");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kScenarios[] = {
			TEXT("FACTORY_LOCAL"), TEXT("OVERWRITE"), TEXT("NULL_ASSIGNMENT"), TEXT("PARAMETER_RETURN"),
			TEXT("EXCEPTION_FRAME"), TEXT("SAVE_LOAD"), TEXT("PARAMETER_RETURN_SAVE_LOAD")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(7);
		for (int32 Index = 0; Index < 7; ++Index)
		{
			const TCHAR* const Scenario = kScenarios[Index];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-VAR-COUNTED-REFERENCE-ASSIGNMENT-%s"), Scenario);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Needle = IndependentNeedle(Scenario);
			Cell.bFault = FCString::Strcmp(Scenario, TEXT("EXCEPTION_FRAME")) == 0;
			Cell.bSaveLoad = FCString::Strcmp(Scenario, TEXT("SAVE_LOAD")) == 0
				|| FCString::Strcmp(Scenario, TEXT("PARAMETER_RETURN_SAVE_LOAD")) == 0;
			Cell.Expected = Cell.bFault ? 0 : IndependentExpected(Scenario);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EVarReferenceAssignmentScenario;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FVarCountedReferenceAssignmentGenerator;
		using AngelscriptTest::Generate::FVarCountedReferenceAssignmentParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FVarCountedReferenceAssignmentGenerator Generator;
		ASSERT_THAT(AreEqual(
			FString(TEXT("LANG-VAR-COUNTED-REFERENCE-ASSIGNMENT")),
			FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangVarCountedReferenceAssignmentFactoryLocal")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("VarCountedReferenceAssignmentGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(
			LogTemp,
			Display,
			TEXT("VarCountedReferenceAssignmentGenerator dump path=%s cases=7 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(7, CaseCount));
		ASSERT_THAT(AreEqual(7, Cases.Num()));
		ASSERT_THAT(AreEqual(7, ExpectedCells.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		int32 FaultCount = 0;
		int32 SaveLoadCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(
				FString::Printf(TEXT("int %s()"), *Expected.EntryName),
				Cases[Index].EntryDeclaration));
			if (Expected.bFault)
			{
				++FaultCount;
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
				ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")), Cases[Index].ExpectedException));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
			}
			else if (Expected.bSaveLoad)
			{
				++SaveLoadCount;
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::SourceOnly));
				ASSERT_THAT(IsTrue(Cases[Index].bLimitedObservation));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			}
			else
			{
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedException.IsEmpty()));
			}
		}
		ASSERT_THAT(AreEqual(1, FaultCount));
		ASSERT_THAT(AreEqual(2, SaveLoadCount));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		const FString OwnedAgain = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(IsTrue(OwnedCase.Len() > 0));
		ASSERT_THAT(AreEqual(OwnedCase, OwnedAgain));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangVarCountedReferenceAssignmentFactoryLocal()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangVarCountedReferenceAssignmentFactoryLocal()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("class FNativeCaseReference\n"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("FNativeCaseReference CreateNativeCaseReference("))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int RaiseCountedReferenceException()"))));

		FVarCountedReferenceAssignmentParams DefaultParams;
		const FString DefaultSingle = Generator.BuildReferenceAssignmentSource(DefaultParams);
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsFalse(DefaultSingle.Contains(TEXT("int EntryLang"))));

		FVarCountedReferenceAssignmentParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		const FString ProbeSource = Generator.BuildReferenceAssignmentSource(ProbeParams);
		ASSERT_THAT(IsTrue(ProbeSource.Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(ProbeSource.Contains(TEXT("int Entry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/VarCountedReferenceAssignmentGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
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

		FVarCountedReferenceAssignmentParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildReferenceAssignmentSource(InvalidName).IsEmpty()));
		FVarCountedReferenceAssignmentParams InvalidEnum = DefaultParams;
		InvalidEnum.Scenario = static_cast<EVarReferenceAssignmentScenario>(7);
		ASSERT_THAT(IsTrue(Generator.BuildReferenceAssignmentSource(InvalidEnum).IsEmpty()));
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

		ASSERT_THAT(IsTrue(
			OutputFile.EndsWith(TEXT("GeneratedCases/VarCountedReferenceAssignmentGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\VarCountedReferenceAssignmentGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(7, CountOccurrences(Dump, TEXT("// Case: LANG-VAR-COUNTED-REFERENCE-ASSIGNMENT-"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Dump, TEXT("// Outcome: RuntimeException"))));
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(IsTrue(Dump.Contains(FString::Printf(TEXT("// Case: %s"), *Expected.CaseId))));
			FString DumpBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Dump, Expected.EntryName, DumpBody)));
			FString AggregateBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, AggregateBody)));
			ASSERT_THAT(AreEqual(AggregateBody, DumpBody));
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
			TEXT("C:/no-such-root/VarCountedReferenceAssignmentGenerator.as"),
			Dump,
			ProbeError)));
		ASSERT_THAT(IsTrue(ProbeError.Len() > 0));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("VarCountedReferenceAssignmentGenerator.as")),
			Dump,
			FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
