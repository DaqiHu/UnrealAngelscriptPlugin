#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestCtorBoundaryGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	CtorBoundaryGenerator,
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
		bool bLimited = false;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-CTOR-BOUNDARY-DUPLICATE_DEFAULT_SIGNATURE-COMPILE_OR_EXECUTION_STATE");
	static constexpr const TCHAR* ZeroCaseId =
		TEXT("LANG-CTOR-BOUNDARY-DUPLICATE_PARAMETER_SIGNATURE-COMPILE_OR_EXECUTION_STATE");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-CONDITION-IF-BOOL_LITERAL-TRUE");

	static bool IndependentReject(const TCHAR* Scenario)
	{
		return FCString::Strcmp(Scenario, TEXT("MISMATCHED_CONSTRUCTOR_NAME")) == 0
			|| FCString::Strcmp(Scenario, TEXT("CONSTRUCTOR_VALUE_RETURN")) == 0
			|| FCString::Strcmp(Scenario, TEXT("SUPER_OUTSIDE_CONSTRUCTOR")) == 0
			|| FCString::Strcmp(Scenario, TEXT("REPEATED_SUPER")) == 0
			|| FCString::Strcmp(Scenario, TEXT("MISSING_BASE_ARGUMENT")) == 0
			|| FCString::Strcmp(Scenario, TEXT("AMBIGUOUS_BASE_ARGUMENT")) == 0
			|| FCString::Strcmp(Scenario, TEXT("INACCESSIBLE_BASE")) == 0
			|| FCString::Strcmp(Scenario, TEXT("RECURSIVE_VALUE_FIELD_DIRECT")) == 0
			|| FCString::Strcmp(Scenario, TEXT("RECURSIVE_VALUE_FIELD_INDIRECT")) == 0
			|| FCString::Strcmp(Scenario, TEXT("MUTABLE_REFERENCE_GLOBAL")) == 0;
	}

	static bool IndependentFault(const TCHAR* Scenario)
	{
		return FCString::Strcmp(Scenario, TEXT("RECURSIVE_CONSTRUCTOR_DIRECT")) == 0
			|| FCString::Strcmp(Scenario, TEXT("RECURSIVE_CONSTRUCTOR_INDIRECT")) == 0;
	}

	static bool IndependentLimited(const TCHAR* Scenario, const TCHAR* Observation)
	{
		if (FCString::Strcmp(Scenario, TEXT("DUPLICATE_PARAMETER_SIGNATURE")) == 0)
		{
			return true;
		}
		if (FCString::Strcmp(Scenario, TEXT("MODULE_DISCARD_CONST_VALUE_GLOBAL")) == 0
			|| FCString::Strcmp(Scenario, TEXT("ENGINE_SHUTDOWN_CONST_VALUE_GLOBAL")) == 0)
		{
			return FCString::Strcmp(Observation, TEXT("COMPILE_OR_EXECUTION_STATE")) != 0;
		}
		return FCString::Strcmp(Observation, TEXT("COMPILE_OR_EXECUTION_STATE")) != 0;
	}

	static int32 IndependentExpected(const TCHAR* Scenario)
	{
		if (FCString::Strcmp(Scenario, TEXT("DUPLICATE_DEFAULT_SIGNATURE")) == 0)
		{
			return 42;
		}
		if (FCString::Strcmp(Scenario, TEXT("SUPER_AFTER_STATEMENT")) == 0)
		{
			return 7;
		}
		if (FCString::Strcmp(Scenario, TEXT("MODULE_DISCARD_CONST_VALUE_GLOBAL")) == 0
			|| FCString::Strcmp(Scenario, TEXT("ENGINE_SHUTDOWN_CONST_VALUE_GLOBAL")) == 0)
		{
			return 71;
		}
		return 0;
	}

	static FString IndependentNeedle(const TCHAR* Scenario)
	{
		if (FCString::Strcmp(Scenario, TEXT("DUPLICATE_DEFAULT_SIGNATURE")) == 0)
		{
			return TEXT("RecordConstructorBoundaryMarker(42)");
		}
		if (FCString::Strcmp(Scenario, TEXT("DUPLICATE_PARAMETER_SIGNATURE")) == 0)
		{
			return TEXT("FBoundaryValueDUPLICATEPARAMETERSIGNATURE(int OtherValue)");
		}
		if (FCString::Strcmp(Scenario, TEXT("MISMATCHED_CONSTRUCTOR_NAME")) == 0)
		{
			return TEXT("FBoundaryOther()");
		}
		if (FCString::Strcmp(Scenario, TEXT("CONSTRUCTOR_VALUE_RETURN")) == 0)
		{
			return TEXT("return 1;");
		}
		if (FCString::Strcmp(Scenario, TEXT("SUPER_OUTSIDE_CONSTRUCTOR")) == 0)
		{
			return TEXT("void InvokeSuperOutsideConstructor()");
		}
		if (FCString::Strcmp(Scenario, TEXT("SUPER_AFTER_STATEMENT")) == 0)
		{
			return TEXT("super(7);");
		}
		if (FCString::Strcmp(Scenario, TEXT("REPEATED_SUPER")) == 0)
		{
			return TEXT("\t\tsuper();\n\t\tsuper();");
		}
		if (FCString::Strcmp(Scenario, TEXT("MISSING_BASE_ARGUMENT")) == 0)
		{
			return TEXT("FBoundaryBaseMISSINGBASEARGUMENT(int Value)");
		}
		if (FCString::Strcmp(Scenario, TEXT("AMBIGUOUS_BASE_ARGUMENT")) == 0)
		{
			return TEXT("super(1);");
		}
		if (FCString::Strcmp(Scenario, TEXT("INACCESSIBLE_BASE")) == 0)
		{
			return TEXT("private FBoundaryBaseINACCESSIBLEBASE()");
		}
		if (FCString::Strcmp(Scenario, TEXT("RECURSIVE_VALUE_FIELD_DIRECT")) == 0)
		{
			return TEXT("FBoundaryRecursiveRECURSIVEVALUEFIELDDIRECT Nested;");
		}
		if (FCString::Strcmp(Scenario, TEXT("RECURSIVE_VALUE_FIELD_INDIRECT")) == 0)
		{
			return TEXT("FBoundaryRecursiveARECURSIVEVALUEFIELDINDIRECT");
		}
		if (FCString::Strcmp(Scenario, TEXT("RECURSIVE_CONSTRUCTOR_DIRECT")) == 0)
		{
			return TEXT("FBoundaryRecursiveRECURSIVECONSTRUCTORDIRECT Next = FBoundaryRecursiveRECURSIVECONSTRUCTORDIRECT()");
		}
		if (FCString::Strcmp(Scenario, TEXT("RECURSIVE_CONSTRUCTOR_INDIRECT")) == 0)
		{
			return TEXT("FBoundaryRecursiveBRECURSIVECONSTRUCTORINDIRECT Next");
		}
		if (FCString::Strcmp(Scenario, TEXT("MUTABLE_REFERENCE_GLOBAL")) == 0)
		{
			return TEXT("FBoundaryRetainedMUTABLEREFERENCEGLOBAL RetainedGlobal");
		}
		return TEXT("const FBoundaryRetained");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kScenarios[] = {
			TEXT("DUPLICATE_DEFAULT_SIGNATURE"),
			TEXT("DUPLICATE_PARAMETER_SIGNATURE"),
			TEXT("MISMATCHED_CONSTRUCTOR_NAME"),
			TEXT("CONSTRUCTOR_VALUE_RETURN"),
			TEXT("SUPER_OUTSIDE_CONSTRUCTOR"),
			TEXT("SUPER_AFTER_STATEMENT"),
			TEXT("REPEATED_SUPER"),
			TEXT("MISSING_BASE_ARGUMENT"),
			TEXT("AMBIGUOUS_BASE_ARGUMENT"),
			TEXT("INACCESSIBLE_BASE"),
			TEXT("RECURSIVE_VALUE_FIELD_DIRECT"),
			TEXT("RECURSIVE_VALUE_FIELD_INDIRECT"),
			TEXT("RECURSIVE_CONSTRUCTOR_DIRECT"),
			TEXT("RECURSIVE_CONSTRUCTOR_INDIRECT"),
			TEXT("MUTABLE_REFERENCE_GLOBAL"),
			TEXT("MODULE_DISCARD_CONST_VALUE_GLOBAL"),
			TEXT("ENGINE_SHUTDOWN_CONST_VALUE_GLOBAL"),
		};
		static const TCHAR* const kObservations[] = {
			TEXT("COMPILE_OR_EXECUTION_STATE"),
			TEXT("DIAGNOSTIC_OR_METADATA"),
			TEXT("LIFECYCLE_CLEANUP"),
			TEXT("RECOVERY_OR_TEARDOWN"),
		};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(68);
		for (int32 Index = 0; Index < 68; ++Index)
		{
			const TCHAR* const Scenario = kScenarios[Index / 4];
			const TCHAR* const Observation = kObservations[Index % 4];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-CTOR-BOUNDARY-%s-%s"), Scenario, Observation);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Scenario);
			Cell.bFault = IndependentFault(Scenario);
			Cell.bLimited = IndependentLimited(Scenario, Observation);
			Cell.Expected = (Cell.bReject || Cell.bFault) ? 0 : IndependentExpected(Scenario);
			Cell.Needle = IndependentNeedle(Scenario);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::ECtorBoundaryObservation;
		using AngelscriptTest::Generate::ECtorBoundaryScenario;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FCtorBoundaryGenerator;
		using AngelscriptTest::Generate::FCtorBoundaryParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FCtorBoundaryGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CTOR-BOUNDARY")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangCtorBoundaryDuplicateDefaultSignatureCompileOrExecutionState")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("CtorBoundaryGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("CtorBoundaryGenerator dump path=%s cases=68 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(28, CaseCount));
		ASSERT_THAT(AreEqual(68, Cases.Num()));
		ASSERT_THAT(AreEqual(40, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangCtorBoundaryDuplicateDefaultSignatureCompileOrExecutionState()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int EntryLangCtorBoundaryMismatchedConstructorNameCompileOrExecutionState()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangCtorBoundaryRecursiveConstructorDirectCompileOrExecutionState()"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("void RecordConstructorBoundaryMarker(int Marker)"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/CtorBoundaryGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 RejectCount = 0;
		int32 FaultCount = 0;
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
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedException.IsEmpty()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(RejectIds.Contains(Expected.CaseId)));
				const FString RejectSource = Generator.BuildRejectSource(Expected.CaseId);
				ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.Needle)));
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(RejectSource, Expected.EntryName, GeneratedBody)));
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
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(FString(TEXT("Stack overflow")), Cases[Index].ExpectedException));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("constructor-boundary"))));
				ASSERT_THAT(IsTrue(Cases[Index].bLimitedObservation == Expected.bLimited));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(Aggregate.Contains(Expected.Needle)));
				if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedException.IsEmpty()));
				ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("constructor-boundary"))));
				ASSERT_THAT(IsTrue(Cases[Index].bLimitedObservation == Expected.bLimited));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(Aggregate.Contains(Expected.Needle)));
				if (Expected.Expected == 0)
				{
					++ZeroCount;
				}
				if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
		}
		ASSERT_THAT(AreEqual(40, RejectCount));
		ASSERT_THAT(AreEqual(8, FaultCount));
		ASSERT_THAT(AreEqual(4, ZeroCount));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangCtorBoundaryDuplicateDefaultSignatureCompileOrExecutionState()"))));

		FCtorBoundaryParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildConstructorBoundarySource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildConstructorBoundarySource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildConstructorBoundarySource(DefaultParams).IsEmpty()));
		DefaultParams.FunctionName.Reset();
		DefaultParams.Scenario = ECtorBoundaryScenario::MismatchedConstructorName;
		ASSERT_THAT(IsTrue(Generator.BuildConstructorBoundarySource(DefaultParams).IsEmpty()));
		DefaultParams.Scenario = ECtorBoundaryScenario::DuplicateDefaultSignature;
		DefaultParams.Observation = static_cast<ECtorBoundaryObservation>(4);
		ASSERT_THAT(IsTrue(Generator.BuildConstructorBoundarySource(DefaultParams).IsEmpty()));
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
		ASSERT_THAT(AreEqual(42, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(OutputFile.EndsWith(TEXT("GeneratedCases/CtorBoundaryGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\CtorBoundaryGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(68, CountOccurrences(Dump, TEXT("// Case: LANG-CTOR-BOUNDARY-"))));
		ASSERT_THAT(AreEqual(40, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/CtorBoundaryGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("CtorBoundaryGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
