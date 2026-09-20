#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestOpResultContextGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	OpResultContextGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString FamilyNeedle;
		int32 Expected = 0;
		bool bReject = false;
		bool bAmbiguous = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-OP-RESULT-CONTEXT-UNARY-ASSIGNMENT-EXACT");

	static const TCHAR* FamilyNeedle(const int32 Family)
	{
		switch (Family)
		{
		case 0:
			return TEXT("-TraceInt(EvaluationCount, 5)");
		case 1:
			return TEXT("TraceInt(EvaluationCount, 2) + TraceInt(EvaluationCount, 3)");
		case 2:
			return TEXT("TraceFloat(EvaluationCount, 2.0f) ** TraceFloat(EvaluationCount, 3.0f)");
		case 3:
			return TEXT("TraceInt(EvaluationCount, 6) & TraceInt(EvaluationCount, 3)");
		case 4:
			return TEXT("TraceInt(EvaluationCount, 1) << TraceInt(EvaluationCount, 3)");
		case 5:
			return TEXT("TraceInt(EvaluationCount, 3) < TraceInt(EvaluationCount, 4)");
		case 6:
			return TEXT("TraceBool(EvaluationCount, true) && TraceBool(EvaluationCount, true)");
		case 7:
			return TEXT("Assigned = TraceInt(EvaluationCount, 5)");
		case 8:
			return TEXT("++Incremented");
		default:
			return TEXT("FContextValue(TraceInt(EvaluationCount, 2)) + FContextValue(TraceInt(EvaluationCount, 3))");
		}
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kFamilies[] = {
			TEXT("UNARY"), TEXT("ARITHMETIC"), TEXT("POWER"), TEXT("BITWISE"), TEXT("SHIFT"),
			TEXT("COMPARISON"), TEXT("LOGICAL"), TEXT("ASSIGNMENT"), TEXT("INCREMENT"), TEXT("OVERLOADED")};
		static const TCHAR* const kContexts[] = {
			TEXT("ASSIGNMENT"), TEXT("RETURN"), TEXT("CONDITION"), TEXT("OVERLOAD_ARGUMENT"),
			TEXT("CHAIN"), TEXT("SWITCH_OR_INDEX")};
		static const TCHAR* const kOutcomes[] = {
			TEXT("EXACT"), TEXT("CONVERTED"), TEXT("AMBIGUOUS"), TEXT("REJECTED")};
		TArray<FExpectedCell> Cells;
		Cells.SetNum(240);
		for (int32 Index = 0; Index < 240; ++Index)
		{
			const int32 Family = Index / 24;
			const int32 Context = (Index / 4) % 6;
			const int32 Outcome = Index % 4;
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-OP-RESULT-CONTEXT-%s-%s-%s"),
				kFamilies[Family],
				kContexts[Context],
				kOutcomes[Outcome]);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.FamilyNeedle = FamilyNeedle(Family);
			Cell.bReject = Outcome == 3;
			Cell.bAmbiguous = Outcome == 2;
			Cell.Expected = (Cell.bReject || Cell.bAmbiguous) ? 0 : 1;
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EOpResultContextContext;
		using AngelscriptTest::Generate::EOpResultContextFamily;
		using AngelscriptTest::Generate::EOpResultContextOutcome;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FOpResultContextGenerator;
		using AngelscriptTest::Generate::FOpResultContextParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FOpResultContextGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-OP-RESULT-CONTEXT")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangOpResultContextUnaryAssignmentExact")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("OpResultContextGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("OpResultContextGenerator dump path=%s cases=240 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(180, CaseCount));
		ASSERT_THAT(AreEqual(240, Cases.Num()));
		ASSERT_THAT(AreEqual(60, RejectIds.Num()));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/OpResultContextGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 RejectCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			if (Expected.bReject)
			{
				++RejectCount;
				ASSERT_THAT(AreEqual(FString(), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::CompileReject));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(RejectIds.Contains(Expected.CaseId)));
				const FString RejectSource = Generator.BuildRejectSource(Expected.CaseId);
				ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.FamilyNeedle)));
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
			}
			else if (Expected.bAmbiguous)
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::SourceOnly));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				const FString CaseSource = Generator.BuildCaseSource(Expected.CaseId);
				ASSERT_THAT(IsTrue(CaseSource.Contains(TEXT("SelectAmbiguous"))));
				ASSERT_THAT(IsTrue(CaseSource.Contains(Expected.FamilyNeedle)));
			}
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(1, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(1, Generator.GetExpected(Expected.CaseId)));
				const FString CaseSource = Generator.BuildCaseSource(Expected.CaseId);
				ASSERT_THAT(IsTrue(CaseSource.Contains(Expected.FamilyNeedle)));
			}
			if (Representative.Contains(Expected.EntryName))
			{
				const FString CaseSource = Expected.bReject
					? Generator.BuildRejectSource(Expected.CaseId)
					: Generator.BuildCaseSource(Expected.CaseId);
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}
		ASSERT_THAT(AreEqual(60, RejectCount));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("struct FContextValue"))));

		FOpResultContextParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildSource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildSource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildSource(DefaultParams).IsEmpty()));
		FOpResultContextParams InvalidEnum;
		InvalidEnum.Family = static_cast<EOpResultContextFamily>(10);
		ASSERT_THAT(IsTrue(Generator.BuildSource(InvalidEnum).IsEmpty()));
		FOpResultContextParams RejectParams;
		RejectParams.Outcome = EOpResultContextOutcome::Rejected;
		ASSERT_THAT(IsTrue(Generator.BuildSource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE")).IsEmpty()));
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
		ASSERT_THAT(AreEqual(240, CountOccurrences(Dump, TEXT("// Case: LANG-OP-RESULT-CONTEXT-"))));
		ASSERT_THAT(AreEqual(60, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/OpResultContextGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("OpResultContextGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
