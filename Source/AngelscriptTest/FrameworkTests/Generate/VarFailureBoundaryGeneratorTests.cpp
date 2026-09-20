#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestVarFailureBoundaryGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	VarFailureBoundaryGenerator,
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
		bool bFault = false;
		bool bUninit = false;
		bool bDiscard = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-VAR-FAILURE-BOUNDARY-FRESH_MODULE-LONG_IDENTIFIER");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static bool IndependentReject(const TCHAR* Scenario)
	{
		return FCString::Strcmp(Scenario, TEXT("USE_BEFORE_DECLARATION")) == 0
			|| FCString::Strcmp(Scenario, TEXT("USE_AFTER_SCOPE")) == 0
			|| FCString::Strcmp(Scenario, TEXT("DUPLICATE_SAME_SCOPE")) == 0
			|| FCString::Strcmp(Scenario, TEXT("INCOMPATIBLE_INITIALIZER")) == 0
			|| FCString::Strcmp(Scenario, TEXT("MUTABLE_GLOBAL")) == 0
			|| FCString::Strcmp(Scenario, TEXT("REFERENCE_GLOBAL")) == 0;
	}

	static bool IndependentFault(const TCHAR* Scenario)
	{
		return FCString::Strcmp(Scenario, TEXT("INITIALIZER_EXCEPTION")) == 0
			|| FCString::Strcmp(Scenario, TEXT("FAILED_INITIALIZER_ATOMICITY")) == 0;
	}

	static int32 IndependentExpected(const TCHAR* Scenario)
	{
		if (FCString::Strcmp(Scenario, TEXT("LONG_IDENTIFIER")) == 0
			|| FCString::Strcmp(Scenario, TEXT("MODULE_DISCARD")) == 0)
		{
			return 61;
		}
		if (FCString::Strcmp(Scenario, TEXT("MANY_LOCALS_SUPPORTED")) == 0
			|| FCString::Strcmp(Scenario, TEXT("STACK_FRAME_PRESSURE")) == 0)
		{
			return 128;
		}
		if (FCString::Strcmp(Scenario, TEXT("MANY_LOCALS_BOUNDARY")) == 0)
		{
			return 2048;
		}
		return 0;
	}

	static FString IndependentNeedle(const TCHAR* Scenario)
	{
		if (FCString::Strcmp(Scenario, TEXT("USE_BEFORE_DECLARATION")) == 0)
		{
			return TEXT("int Captured = Value;");
		}
		if (FCString::Strcmp(Scenario, TEXT("USE_AFTER_SCOPE")) == 0)
		{
			return TEXT("return ScopedValue;");
		}
		if (FCString::Strcmp(Scenario, TEXT("DUPLICATE_SAME_SCOPE")) == 0)
		{
			return TEXT("int DuplicateValue = 31;");
		}
		if (FCString::Strcmp(Scenario, TEXT("INCOMPATIBLE_INITIALIZER")) == 0)
		{
			return TEXT("FNativeCaseValue(37)");
		}
		if (FCString::Strcmp(Scenario, TEXT("MUTABLE_GLOBAL")) == 0)
		{
			return TEXT("MutableBoundaryGlobal");
		}
		if (FCString::Strcmp(Scenario, TEXT("REFERENCE_GLOBAL")) == 0)
		{
			return TEXT("ReferenceBoundaryGlobal");
		}
		if (FCString::Strcmp(Scenario, TEXT("UNINITIALIZED_READ")) == 0)
		{
			return TEXT("int Value;");
		}
		if (FCString::Strcmp(Scenario, TEXT("INITIALIZER_EXCEPTION")) == 0)
		{
			return TEXT("41 / Zero");
		}
		if (FCString::Strcmp(Scenario, TEXT("FAILED_INITIALIZER_ATOMICITY")) == 0)
		{
			return TEXT("1 / Zero");
		}
		if (FCString::Strcmp(Scenario, TEXT("LONG_IDENTIFIER")) == 0)
		{
			return TEXT("LongVariable_");
		}
		if (FCString::Strcmp(Scenario, TEXT("MANY_LOCALS_SUPPORTED")) == 0)
		{
			return TEXT("Local0127");
		}
		if (FCString::Strcmp(Scenario, TEXT("MANY_LOCALS_BOUNDARY")) == 0)
		{
			return TEXT("Local2047");
		}
		if (FCString::Strcmp(Scenario, TEXT("STACK_FRAME_PRESSURE")) == 0)
		{
			return TEXT("PressureValue127");
		}
		return TEXT("FDiscardVariableOwner");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kRecoveries[] = {TEXT("FRESH_MODULE"), TEXT("SAME_MODULE_OR_CONTEXT")};
		static const TCHAR* const kScenarios[] = {
			TEXT("USE_BEFORE_DECLARATION"), TEXT("USE_AFTER_SCOPE"), TEXT("DUPLICATE_SAME_SCOPE"),
			TEXT("INCOMPATIBLE_INITIALIZER"), TEXT("MUTABLE_GLOBAL"), TEXT("REFERENCE_GLOBAL"),
			TEXT("UNINITIALIZED_READ"), TEXT("INITIALIZER_EXCEPTION"), TEXT("FAILED_INITIALIZER_ATOMICITY"),
			TEXT("LONG_IDENTIFIER"), TEXT("MANY_LOCALS_SUPPORTED"), TEXT("MANY_LOCALS_BOUNDARY"),
			TEXT("STACK_FRAME_PRESSURE"), TEXT("MODULE_DISCARD")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(28);
		for (int32 Index = 0; Index < 28; ++Index)
		{
			const TCHAR* const Recovery = kRecoveries[Index / 14];
			const TCHAR* const Scenario = kScenarios[Index % 14];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-VAR-FAILURE-BOUNDARY-%s-%s"), Recovery, Scenario);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Scenario);
			Cell.bFault = IndependentFault(Scenario);
			Cell.bUninit = FCString::Strcmp(Scenario, TEXT("UNINITIALIZED_READ")) == 0;
			Cell.bDiscard = FCString::Strcmp(Scenario, TEXT("MODULE_DISCARD")) == 0;
			Cell.Expected = Cell.bReject || Cell.bFault || Cell.bUninit ? 0 : IndependentExpected(Scenario);
			Cell.Needle = IndependentNeedle(Scenario);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EVarFailureRecovery;
		using AngelscriptTest::Generate::EVarFailureScenario;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FVarFailureBoundaryGenerator;
		using AngelscriptTest::Generate::FVarFailureBoundaryParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FVarFailureBoundaryGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-VAR-FAILURE-BOUNDARY")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangVarFailureBoundaryFreshModuleLongIdentifier")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("VarFailureBoundaryGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(
			LogTemp,
			Display,
			TEXT("VarFailureBoundaryGenerator dump path=%s cases=28 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(16, CaseCount));
		ASSERT_THAT(AreEqual(28, Cases.Num()));
		ASSERT_THAT(AreEqual(12, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/VarFailureBoundaryGenerator/RepresentativeSources.as"));
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
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(RejectSource, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
				if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
			else if (Expected.bFault)
			{
				++FaultCount;
				ASSERT_THAT(AreEqual(
					FString::Printf(TEXT("int %s()"), *Expected.EntryName),
					Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
				ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")), Cases[Index].ExpectedException));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
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
			else if (Expected.bUninit)
			{
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::SourceOnly));
				ASSERT_THAT(IsTrue(Cases[Index].bLimitedObservation));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
			}
			else
			{
				ASSERT_THAT(AreEqual(
					FString::Printf(TEXT("int %s()"), *Expected.EntryName),
					Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				if (Expected.bDiscard)
				{
					ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::SourceOnly));
					ASSERT_THAT(IsTrue(Cases[Index].bLimitedObservation));
				}
				else
				{
					ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
				}
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
		}
		ASSERT_THAT(AreEqual(12, RejectCount));
		ASSERT_THAT(AreEqual(4, FaultCount));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("class FDiscardVariableOwner\n"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));

		FVarFailureBoundaryParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildBoundarySource(DefaultParams).IsEmpty()));
		FVarFailureBoundaryParams ProbeParams;
		ProbeParams.Scenario = EVarFailureScenario::LongIdentifier;
		ASSERT_THAT(IsTrue(Generator.BuildBoundarySource(ProbeParams).Contains(TEXT("int Entry()"))));
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildBoundarySource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildBoundarySource(ProbeParams).IsEmpty()));
		FVarFailureBoundaryParams InvalidEnum;
		InvalidEnum.Scenario = static_cast<EVarFailureScenario>(14);
		ASSERT_THAT(IsTrue(Generator.BuildBoundarySource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[9].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[9].CaseId, Relisted[9].CaseId));
		ASSERT_THAT(IsTrue(Relisted[9].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(61, Relisted[9].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(AreEqual(28, CountOccurrences(Dump, TEXT("// Case: LANG-VAR-FAILURE-BOUNDARY-"))));
		ASSERT_THAT(AreEqual(12, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(IsTrue(
			OutputFile.EndsWith(TEXT("GeneratedCases/VarFailureBoundaryGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\VarFailureBoundaryGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/VarFailureBoundaryGenerator.as"),
			Dump,
			ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("VarFailureBoundaryGenerator.as")),
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
