#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestFnValueLifecycleGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	FnValueLifecycleGenerator,
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
		FString Exception;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-FN-VALUE-LIFECYCLE-NONE-ZERO-VALUE_ARGUMENT");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static FString IndependentNeedle(const TCHAR* Failure, const TCHAR* Initialized, const TCHAR* Transfer)
	{
		if (FCString::Strcmp(Initialized, TEXT("MANY")) == 0)
		{
			return TEXT("Initialized3(103)");
		}
		if (FCString::Strcmp(Initialized, TEXT("ONE")) == 0)
		{
			return TEXT("Initialized0(100)");
		}
		if (FCString::Strcmp(Failure, TEXT("ARGUMENT_EVALUATION")) == 0)
		{
			return TEXT("1 / Zero");
		}
		if (FCString::Strcmp(Failure, TEXT("BODY")) == 0)
		{
			return FCString::Strcmp(Transfer, TEXT("VALUE_ARGUMENT")) == 0
				? TEXT("ConsumeValueArgument(FNativeCaseValue(41), 0)")
				: TEXT("ProduceReturnValue(41, 0, false)");
		}
		if (FCString::Strcmp(Failure, TEXT("RETURN_CONSTRUCTION")) == 0)
		{
			return FCString::Strcmp(Transfer, TEXT("VALUE_ARGUMENT")) == 0
				? TEXT("ProduceArgumentValue(41, true)")
				: TEXT("ProduceReturnValue(41, 1, true)");
		}
		return FCString::Strcmp(Transfer, TEXT("VALUE_ARGUMENT")) == 0
			? TEXT("ProduceArgumentValue(41, false)")
			: TEXT("ProduceReturnValue(41, 1, false)");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kFailures[] = {
			TEXT("NONE"), TEXT("ARGUMENT_EVALUATION"), TEXT("BODY"), TEXT("RETURN_CONSTRUCTION")};
		static const TCHAR* const kInitialized[] = {TEXT("ZERO"), TEXT("ONE"), TEXT("MANY")};
		static const TCHAR* const kTransfers[] = {TEXT("VALUE_ARGUMENT"), TEXT("VALUE_RETURN")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(24);
		for (int32 Index = 0; Index < 24; ++Index)
		{
			const TCHAR* const Failure = kFailures[Index / 6];
			const TCHAR* const Initialized = kInitialized[(Index / 2) % 3];
			const TCHAR* const Transfer = kTransfers[Index % 2];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-FN-VALUE-LIFECYCLE-%s-%s-%s"), Failure, Initialized, Transfer);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bFault = FCString::Strcmp(Failure, TEXT("NONE")) != 0;
			Cell.Expected = Cell.bFault ? 0 : 41;
			Cell.Exception = FCString::Strcmp(Failure, TEXT("RETURN_CONSTRUCTION")) == 0
				? TEXT("Native case value copy construction fault")
				: TEXT("Divide by zero");
			Cell.Needle = IndependentNeedle(Failure, Initialized, Transfer);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EFnValueLifecycleFailure;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FFnValueLifecycleGenerator;
		using AngelscriptTest::Generate::FFnValueLifecycleParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FFnValueLifecycleGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-FN-VALUE-LIFECYCLE")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangFnValueLifecycleNoneZeroValueArgument")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("FnValueLifecycleGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("FnValueLifecycleGenerator dump path=%s cases=24 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(24, CaseCount));
		ASSERT_THAT(AreEqual(24, Cases.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int CleanAfterValueLifecycle()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/FnValueLifecycleGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 FaultCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
			ASSERT_THAT(IsTrue(Cases[Index].bLimitedObservation));
			ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("FNativeCaseValue"))));
			if (Expected.bFault)
			{
				++FaultCount;
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(AreEqual(Expected.Exception, Cases[Index].ExpectedException));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
			}
			else
			{
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(41, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(41, Generator.GetExpected(Expected.CaseId)));
			}
			const FString Isolated = Generator.BuildCaseSource(Expected.CaseId);
			ASSERT_THAT(IsTrue(Isolated.Contains(Expected.Needle)));
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
		ASSERT_THAT(AreEqual(18, FaultCount));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangFnValueLifecycleNoneZeroValueArgument()"))));

		FFnValueLifecycleParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FFnValueLifecycleParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(Generator.BuildSource(ProbeParams).Contains(TEXT("int Entry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildSource(ProbeParams).IsEmpty()));
		FFnValueLifecycleParams InvalidEnum = DefaultParams;
		InvalidEnum.Failure = static_cast<EFnValueLifecycleFailure>(4);
		ASSERT_THAT(IsTrue(Generator.BuildSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(41, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(24, CountOccurrences(Dump, TEXT("// Case: LANG-FN-VALUE-LIFECYCLE-"))));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
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

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/FnValueLifecycleGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("FnValueLifecycleGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
