#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestSwitchPlacementGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	SwitchPlacementGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString Needle;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-CF-SWITCH-PLACEMENT-FUNCTION-CASE_OUTSIDE");

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kScopes[] = {TEXT("FUNCTION"), TEXT("BRANCH"), TEXT("LOOP"), TEXT("AFTER_SWITCH")};
		static const TCHAR* const kKinds[] = {
			TEXT("CASE_OUTSIDE"), TEXT("DEFAULT_OUTSIDE"), TEXT("DUPLICATE_DEFAULT"), TEXT("CASE_AFTER_DEFAULT")};
		static const TCHAR* const kNeedles[] = {
			TEXT("case 1:"), TEXT("default:"), TEXT("Value += 30;"), TEXT("Value += 60;")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(16);
		for (int32 Index = 0; Index < 16; ++Index)
		{
			const TCHAR* const Scope = kScopes[Index / 4];
			const TCHAR* const Kind = kKinds[Index % 4];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-CF-SWITCH-PLACEMENT-%s-%s"), Scope, Kind);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Needle = kNeedles[Index % 4];
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::ESwitchPlacementKind;
		using AngelscriptTest::Generate::ESwitchPlacementScope;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FSwitchPlacementGenerator;
		using AngelscriptTest::Generate::FSwitchPlacementParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FSwitchPlacementGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CF-SWITCH-PLACEMENT")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangCfSwitchPlacementFunctionCaseOutside")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("SwitchPlacementGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("SwitchPlacementGenerator dump path=%s cases=16 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(0, CaseCount));
		ASSERT_THAT(AreEqual(FString(), Aggregate));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(AreEqual(16, Cases.Num()));
		ASSERT_THAT(AreEqual(16, RejectIds.Num()));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/SwitchPlacementGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
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

		FSwitchPlacementParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildSwitchPlacementSource(DefaultParams).IsEmpty()));
		FSwitchPlacementParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildSwitchPlacementSource(ProbeParams).IsEmpty()));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildSwitchPlacementSource(ProbeParams).IsEmpty()));
		DefaultParams.Scope = static_cast<ESwitchPlacementScope>(4);
		ASSERT_THAT(IsTrue(Generator.BuildSwitchPlacementSource(DefaultParams).IsEmpty()));
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
		ASSERT_THAT(AreEqual(16, CountOccurrences(Dump, TEXT("// Case: LANG-CF-SWITCH-PLACEMENT-"))));
		ASSERT_THAT(AreEqual(16, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/SwitchPlacementGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("SwitchPlacementGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
