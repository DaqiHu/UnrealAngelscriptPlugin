#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestDeclFailureRecoveryGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	DeclFailureRecoveryGenerator,
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

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-DECL-FAILURE-RECOVERY-UNBALANCED_CLASS-ENTRY_BODY-LF");

	static FString IndependentNeedle(const TCHAR* Form)
	{
		if (FCString::Strcmp(Form, TEXT("UNBALANCED_CLASS")) == 0)
		{
			return TEXT("class FBroken");
		}
		if (FCString::Strcmp(Form, TEXT("BAD_PARAMETER_LIST")) == 0)
		{
			return TEXT("int A,, int B");
		}
		if (FCString::Strcmp(Form, TEXT("UNCLOSED_FUNCTION_BODY")) == 0)
		{
			return TEXT("return 1;");
		}
		if (FCString::Strcmp(Form, TEXT("MISSING_TYPE_NAME")) == 0)
		{
			return TEXT("int;");
		}
		if (FCString::Strcmp(Form, TEXT("UNEXPECTED_HANDLE")) == 0)
		{
			return TEXT("int @Broken;");
		}
		return TEXT("MissingBase");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kForms[] = {
			TEXT("UNBALANCED_CLASS"), TEXT("BAD_PARAMETER_LIST"), TEXT("UNCLOSED_FUNCTION_BODY"),
			TEXT("MISSING_TYPE_NAME"), TEXT("UNEXPECTED_HANDLE"), TEXT("UNKNOWN_BASE")};
		static const TCHAR* const kPlacements[] = {
			TEXT("ENTRY_BODY"), TEXT("AFTER_VALID_FUNCTION"), TEXT("INSIDE_NAMESPACE")};
		TArray<FExpectedCell> Cells;
		Cells.SetNum(18);
		for (int32 Index = 0; Index < 18; ++Index)
		{
			const TCHAR* const Form = kForms[Index / 3];
			const TCHAR* const Placement = kPlacements[Index % 3];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-DECL-FAILURE-RECOVERY-%s-%s-LF"),
				Form,
				Placement);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Needle = IndependentNeedle(Form);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EDeclFailureRecoveryForm;
		using AngelscriptTest::Generate::EDeclFailureRecoveryLineEnding;
		using AngelscriptTest::Generate::EDeclFailureRecoveryPlacement;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FDeclFailureRecoveryGenerator;
		using AngelscriptTest::Generate::FDeclFailureRecoveryParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FDeclFailureRecoveryGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-DECL-FAILURE-RECOVERY")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangDeclFailureRecoveryUnbalancedClassEntryBodyLf")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("DeclFailureRecoveryGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("DeclFailureRecoveryGenerator dump path=%s cases=18 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(0, CaseCount));
		ASSERT_THAT(AreEqual(FString(), Aggregate));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(AreEqual(18, Cases.Num()));
		ASSERT_THAT(AreEqual(18, RejectIds.Num()));

		int32 RejectCount = 0;
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
			++RejectCount;
		}
		ASSERT_THAT(AreEqual(18, RejectCount));

		ASSERT_THAT(AreEqual(Generator.BuildRejectSource(ProbeCaseId), Generator.BuildRejectSource(ProbeCaseId)));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));

		FDeclFailureRecoveryParams DefaultParams;
		const FString DefaultInvalid = Generator.BuildInvalidSource(DefaultParams);
		ASSERT_THAT(IsTrue(DefaultInvalid.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsTrue(DefaultInvalid.Contains(TEXT("class FBroken"))));
		FDeclFailureRecoveryParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildInvalidSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(Generator.BuildInvalidSource(ProbeParams).Contains(TEXT("int Entry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/DeclFailureRecoveryGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			const FString CaseSource = Generator.BuildRejectSource(Expected.CaseId);
			ASSERT_THAT(IsTrue(CaseSource.Contains(Expected.Needle)));
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FDeclFailureRecoveryParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildInvalidSource(InvalidName).IsEmpty()));
		FDeclFailureRecoveryParams InvalidEnum = DefaultParams;
		InvalidEnum.Form = static_cast<EDeclFailureRecoveryForm>(6);
		ASSERT_THAT(IsTrue(Generator.BuildInvalidSource(InvalidEnum).IsEmpty()));
		FDeclFailureRecoveryParams CrlfParams = DefaultParams;
		CrlfParams.LineEnding = EDeclFailureRecoveryLineEnding::Crlf;
		ASSERT_THAT(IsTrue(Generator.BuildInvalidSource(CrlfParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-DECL-COLLISION-LEFT_THEN_RIGHT-SAME-FUNCTION_OVERLOAD")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(18, CountOccurrences(Dump, TEXT("// Case: LANG-DECL-FAILURE-RECOVERY-"))));
		ASSERT_THAT(AreEqual(18, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/DeclFailureRecoveryGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("DeclFailureRecoveryGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
