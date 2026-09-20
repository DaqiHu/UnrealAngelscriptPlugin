#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestSwitchGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	SwitchGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		int32 Expected = 0;
		bool bReject = false;
		bool bFault = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-CF-SWITCH-INT8-FIRST-BREAK");

	static int32 IndependentTraceSeed(const TCHAR* Case)
	{
		if (FCString::Strcmp(Case, TEXT("FIRST")) == 0)
		{
			return 11;
		}
		if (FCString::Strcmp(Case, TEXT("MIDDLE")) == 0)
		{
			return 12;
		}
		if (FCString::Strcmp(Case, TEXT("LAST")) == 0)
		{
			return 13;
		}
		if (FCString::Strcmp(Case, TEXT("DEFAULT")) == 0)
		{
			return 14;
		}
		if (FCString::Strcmp(Case, TEXT("NO_MATCH")) == 0)
		{
			return 15;
		}
		if (FCString::Strcmp(Case, TEXT("FALLTHROUGH")) == 0)
		{
			return 16;
		}
		return 17;
	}

	static bool IndependentReject(const TCHAR* Selector, const TCHAR* Case, const TCHAR* Exit)
	{
		if (FCString::Strcmp(Selector, TEXT("UNSUPPORTED")) == 0)
		{
			return true;
		}
		if (FCString::Strcmp(Case, TEXT("DUPLICATE")) == 0 || FCString::Strcmp(Case, TEXT("NON_CONSTANT")) == 0)
		{
			return true;
		}
		return (FCString::Strcmp(Case, TEXT("DEFAULT")) == 0 || FCString::Strcmp(Case, TEXT("NO_MATCH")) == 0)
			&& FCString::Strcmp(Exit, TEXT("FALLTHROUGH")) == 0;
	}

	static int32 IndependentExpected(const TCHAR* Selector, const TCHAR* Case, const TCHAR* Exit)
	{
		int32 Value = IndependentTraceSeed(Case) + (FCString::Strcmp(Case, TEXT("FALLTHROUGH")) == 0 ? 1 : 0);
		if (FCString::Strcmp(Selector, TEXT("BOUNDARY")) == 0)
		{
			Value += 6;
		}
		if (FCString::Strcmp(Exit, TEXT("FALLTHROUGH")) == 0)
		{
			Value += 100;
		}
		else if (FCString::Strcmp(Exit, TEXT("RETURN")) == 0)
		{
			Value += 200;
		}
		return Value;
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kSelectors[] = {
			TEXT("INT8"), TEXT("INT16"), TEXT("INT"), TEXT("INT64"),
			TEXT("UINT8"), TEXT("UINT16"), TEXT("UINT"), TEXT("UINT64"),
			TEXT("ENUM"), TEXT("TYPEDEF"), TEXT("BOUNDARY"), TEXT("UNSUPPORTED")};
		static const TCHAR* const kCases[] = {
			TEXT("FIRST"), TEXT("MIDDLE"), TEXT("LAST"), TEXT("DEFAULT"), TEXT("NO_MATCH"),
			TEXT("FALLTHROUGH"), TEXT("GROUPED"), TEXT("DUPLICATE"), TEXT("NON_CONSTANT")};
		static const TCHAR* const kExits[] = {TEXT("BREAK"), TEXT("FALLTHROUGH"), TEXT("RETURN"), TEXT("EXCEPTION")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(432);
		for (int32 Index = 0; Index < 432; ++Index)
		{
			const TCHAR* const Selector = kSelectors[Index / 36];
			const TCHAR* const Case = kCases[(Index / 4) % 9];
			const TCHAR* const Exit = kExits[Index % 4];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-CF-SWITCH-%s-%s-%s"), Selector, Case, Exit);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Selector, Case, Exit);
			Cell.bFault = !Cell.bReject && FCString::Strcmp(Exit, TEXT("EXCEPTION")) == 0;
			Cell.Expected = (Cell.bReject || Cell.bFault) ? 0 : IndependentExpected(Selector, Case, Exit);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::ESwitchCase;
		using AngelscriptTest::Generate::ESwitchExit;
		using AngelscriptTest::Generate::ESwitchSelector;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FSwitchGenerator;
		using AngelscriptTest::Generate::FSwitchParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FSwitchGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CF-SWITCH")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangCfSwitchInt8FirstBreak")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("SwitchGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("SwitchGenerator dump path=%s cases=432 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(286, CaseCount));
		ASSERT_THAT(AreEqual(432, Cases.Num()));
		ASSERT_THAT(AreEqual(146, RejectIds.Num()));

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
				ASSERT_THAT(IsTrue(RejectSource.Contains(TEXT("switch (Value)"))));
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
			}
			else if (Expected.bFault)
			{
				++FaultCount;
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")), Cases[Index].ExpectedException));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
			}
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
			}
		}
		ASSERT_THAT(AreEqual(146, RejectCount));
		ASSERT_THAT(AreEqual(77, FaultCount));

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangCfSwitchInt8FirstBreak()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int EntryLangCfSwitchUnsupported"))));

		FSwitchParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildSwitchSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FSwitchParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildSwitchSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/SwitchGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			if (Expected.bReject)
			{
				continue;
			}
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("switch (Value)"))));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FSwitchParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildSwitchSource(InvalidName).IsEmpty()));
		FSwitchParams InvalidEnum = DefaultParams;
		InvalidEnum.Selector = static_cast<ESwitchSelector>(12);
		ASSERT_THAT(IsTrue(Generator.BuildSwitchSource(InvalidEnum).IsEmpty()));
		FSwitchParams RejectParams = DefaultParams;
		RejectParams.Selector = ESwitchSelector::Unsupported;
		ASSERT_THAT(IsTrue(Generator.BuildSwitchSource(RejectParams).IsEmpty()));
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
		ASSERT_THAT(AreEqual(432, CountOccurrences(Dump, TEXT("// Case: LANG-CF-SWITCH-"))));
		ASSERT_THAT(AreEqual(146, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/SwitchGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("SwitchGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
