#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestFnRecursionGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	FnRecursionGenerator,
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
		bool bReject = false;
		bool bHost = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-FN-RECURSION-ZERO-RETURN-PRIMITIVE");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static int32 IndependentDepth(const TCHAR* Depth, const TCHAR* Outcome)
	{
		if (FCString::Strcmp(Depth, TEXT("ONE")) == 0)
		{
			return 1;
		}
		if (FCString::Strcmp(Depth, TEXT("EIGHT")) == 0)
		{
			return 8;
		}
		if (FCString::Strcmp(Depth, TEXT("CONFIGURED_LIMIT")) == 0)
		{
			return FCString::Strcmp(Outcome, TEXT("EXCEPTION")) == 0 ? 13 : 11;
		}
		return 0;
	}

	static FString IndependentNeedle(const TCHAR* Depth, const TCHAR* Outcome, const TCHAR* Type)
	{
		if (FCString::Strcmp(Depth, TEXT("EIGHT")) == 0 || FCString::Strcmp(Depth, TEXT("CONFIGURED_LIMIT")) == 0)
		{
			return TEXT("int RecurseB");
		}
		if (FCString::Strcmp(Outcome, TEXT("EXCEPTION")) == 0)
		{
			return TEXT("return 1 / Zero;");
		}
		if (FCString::Strcmp(Type, TEXT("VALUE_OBJECT")) == 0)
		{
			return TEXT("return Value.Value;");
		}
		if (FCString::Strcmp(Type, TEXT("REFERENCE_OBJECT")) == 0)
		{
			return TEXT("return Accumulator;");
		}
		return TEXT("return Value;");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kDepths[] = {TEXT("ZERO"), TEXT("ONE"), TEXT("EIGHT"), TEXT("CONFIGURED_LIMIT")};
		static const TCHAR* const kOutcomes[] = {TEXT("RETURN"), TEXT("EXCEPTION")};
		static const TCHAR* const kTypes[] = {TEXT("PRIMITIVE"), TEXT("VALUE_OBJECT"), TEXT("REFERENCE_OBJECT")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(24);
		for (int32 Index = 0; Index < 24; ++Index)
		{
			const TCHAR* const Depth = kDepths[Index / 6];
			const TCHAR* const Outcome = kOutcomes[(Index / 3) % 2];
			const TCHAR* const Type = kTypes[Index % 3];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-FN-RECURSION-%s-%s-%s"), Depth, Outcome, Type);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = FCString::Strcmp(Depth, TEXT("EIGHT")) == 0
				|| FCString::Strcmp(Depth, TEXT("CONFIGURED_LIMIT")) == 0;
			Cell.bFault = !Cell.bReject && FCString::Strcmp(Outcome, TEXT("EXCEPTION")) == 0;
			Cell.Expected = (Cell.bReject || Cell.bFault) ? 0 : 1 + IndependentDepth(Depth, Outcome);
			Cell.Needle = IndependentNeedle(Depth, Outcome, Type);
			Cell.bHost = FCString::Strcmp(Type, TEXT("PRIMITIVE")) != 0;
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EFnRecursionDepth;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FFnRecursionGenerator;
		using AngelscriptTest::Generate::FFnRecursionParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FFnRecursionGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-FN-RECURSION")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangFnRecursionZeroReturnPrimitive")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("FnRecursionGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("FnRecursionGenerator dump path=%s cases=24 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(12, CaseCount));
		ASSERT_THAT(AreEqual(24, Cases.Num()));
		ASSERT_THAT(AreEqual(12, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int CleanAfterRecursion()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/FnRecursionGenerator/RepresentativeSources.as"));
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
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				if (Expected.bFault)
				{
					++FaultCount;
					ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
					ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")), Cases[Index].ExpectedException));
					ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
					ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				}
				else
				{
					ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
					ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
					ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				}
				ASSERT_THAT(IsTrue(Cases[Index].Execution == (Expected.bHost
					? EGeneratedCaseExecution::RequiresHostSetup
					: EGeneratedCaseExecution::Standalone)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				const FString Isolated = Generator.BuildCaseSource(Expected.CaseId);
				ASSERT_THAT(IsTrue(Isolated.Contains(Expected.Needle)));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
		}
		ASSERT_THAT(AreEqual(12, RejectCount));
		ASSERT_THAT(AreEqual(6, FaultCount));

		FFnRecursionParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildRecursionSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FFnRecursionParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildRecursionSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(Generator.BuildRecursionSource(ProbeParams).Contains(TEXT("int Entry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildRecursionSource(ProbeParams).IsEmpty()));
		FFnRecursionParams InvalidEnum = DefaultParams;
		InvalidEnum.Depth = static_cast<EFnRecursionDepth>(4);
		ASSERT_THAT(IsTrue(Generator.BuildRecursionSource(InvalidEnum).IsEmpty()));
		FFnRecursionParams RejectParams = DefaultParams;
		RejectParams.Depth = EFnRecursionDepth::Eight;
		ASSERT_THAT(IsTrue(Generator.BuildRecursionSource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildRejectSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(1, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(24, CountOccurrences(Dump, TEXT("// Case: LANG-FN-RECURSION-"))));
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
			TEXT("C:/no-such-root/FnRecursionGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("FnRecursionGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
