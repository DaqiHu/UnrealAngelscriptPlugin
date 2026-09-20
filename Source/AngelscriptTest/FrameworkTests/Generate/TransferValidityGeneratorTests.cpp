#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestTransferValidityGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	TransferValidityGenerator,
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
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-CF-TRANSFER-VALIDITY-FUNCTION-BREAK");

	static bool IndependentReject(const TCHAR* Placement, const TCHAR* Transfer)
	{
		if (FCString::Strcmp(Placement, TEXT("FUNCTION")) == 0 || FCString::Strcmp(Placement, TEXT("BRANCH")) == 0)
		{
			return true;
		}
		return FCString::Strcmp(Placement, TEXT("SWITCH")) == 0 && FCString::Strcmp(Transfer, TEXT("CONTINUE")) == 0;
	}

	static int32 IndependentExpected(const TCHAR* Placement, const TCHAR* Transfer)
	{
		if (FCString::Strcmp(Placement, TEXT("LOOP")) == 0 && FCString::Strcmp(Transfer, TEXT("CONTINUE")) == 0)
		{
			return 5;
		}
		return 3;
	}

	static FString IndependentNeedle(const TCHAR* Placement, const TCHAR* Transfer)
	{
		if (FCString::Strcmp(Placement, TEXT("FUNCTION")) == 0)
		{
			return FCString::Strcmp(Transfer, TEXT("CONTINUE")) == 0 ? TEXT("\tcontinue;") : TEXT("\tbreak;");
		}
		if (FCString::Strcmp(Placement, TEXT("BRANCH")) == 0)
		{
			return TEXT("if (true)");
		}
		if (FCString::Strcmp(Placement, TEXT("SWITCH")) == 0)
		{
			return TEXT("switch (1)");
		}
		return TEXT("for (int Index = 0; Index < 2; ++Index)");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kPlacements[] = {TEXT("FUNCTION"), TEXT("BRANCH"), TEXT("SWITCH"), TEXT("LOOP")};
		static const TCHAR* const kTransfers[] = {TEXT("BREAK"), TEXT("CONTINUE")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(8);
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const TCHAR* const Placement = kPlacements[Index / 2];
			const TCHAR* const Transfer = kTransfers[Index % 2];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-CF-TRANSFER-VALIDITY-%s-%s"), Placement, Transfer);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Placement, Transfer);
			Cell.Expected = Cell.bReject ? 0 : IndependentExpected(Placement, Transfer);
			Cell.Needle = IndependentNeedle(Placement, Transfer);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::ETransferValidityPlacement;
		using AngelscriptTest::Generate::ETransferValidityTransfer;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FTransferValidityGenerator;
		using AngelscriptTest::Generate::FTransferValidityParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FTransferValidityGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CF-TRANSFER-VALIDITY")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangCfTransferValidityFunctionBreak")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("TransferValidityGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("TransferValidityGenerator dump path=%s cases=8 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(3, CaseCount));
		ASSERT_THAT(AreEqual(8, Cases.Num()));
		ASSERT_THAT(AreEqual(5, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangCfTransferValiditySwitchBreak()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int EntryLangCfTransferValidityFunctionBreak()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/TransferValidityGenerator/RepresentativeSources.as"));
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
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::SourceOnly));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(RejectIds.Contains(Expected.CaseId)));
				const FString RejectSource = Generator.BuildRejectSource(Expected.CaseId);
				ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.Needle)));
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(RejectSource, Expected.EntryName, GeneratedBody)));
				if (Representative.Contains(Expected.EntryName))
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
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
				if (Representative.Contains(Expected.EntryName))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
		}
		ASSERT_THAT(AreEqual(5, RejectCount));

		const FString OwnedReject = Generator.BuildRejectSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedReject, Generator.BuildRejectSource(ProbeCaseId)));

		FTransferValidityParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildValiditySource(DefaultParams).IsEmpty()));
		FTransferValidityParams LoopBreak;
		LoopBreak.Placement = ETransferValidityPlacement::Loop;
		ASSERT_THAT(IsTrue(Generator.BuildValiditySource(LoopBreak).Contains(TEXT("int Entry()"))));
		LoopBreak.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildValiditySource(LoopBreak).Contains(TEXT("int ProbeEntry()"))));
		LoopBreak.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildValiditySource(LoopBreak).IsEmpty()));
		LoopBreak.FunctionName.Reset();
		LoopBreak.Placement = static_cast<ETransferValidityPlacement>(4);
		ASSERT_THAT(IsTrue(Generator.BuildValiditySource(LoopBreak).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(8, CountOccurrences(Dump, TEXT("// Case: LANG-CF-TRANSFER-VALIDITY-"))));
		ASSERT_THAT(AreEqual(5, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/TransferValidityGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("TransferValidityGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
