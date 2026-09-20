#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestRefFailureGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	RefFailureGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString Needle;
		bool bReject = false;
		bool bFault = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-REF-FAILURE-NULL_MEMBER_ACCESS-FRESH_MODULE");

	static FString IndependentNeedle(const TCHAR* Failure)
	{
		if (FCString::Strcmp(Failure, TEXT("EXPLICIT_HANDLE")) == 0)
		{
			return TEXT("FRefRoot@ Explicit");
		}
		if (FCString::Strcmp(Failure, TEXT("CONST_REMOVAL")) == 0)
		{
			return TEXT("const FRefRoot ReadOnly");
		}
		if (FCString::Strcmp(Failure, TEXT("TEMPORARY_OUT")) == 0)
		{
			return TEXT("WriteTemporaryOut(MakeRefRoot(4))");
		}
		if (FCString::Strcmp(Failure, TEXT("EXPIRED_LOCAL_RETURN")) == 0)
		{
			return TEXT("ReturnExpiredLocal()");
		}
		if (FCString::Strcmp(Failure, TEXT("UNRELATED_ASSIGNMENT")) == 0)
		{
			return TEXT("FRefUnrelated Other");
		}
		if (FCString::Strcmp(Failure, TEXT("UNRELATED_CAST")) == 0)
		{
			return TEXT("cast<FRefDerived>(Other)");
		}
		if (FCString::Strcmp(Failure, TEXT("NULL_MEMBER_ACCESS")) == 0)
		{
			return TEXT("Missing.GetValue()");
		}
		if (FCString::Strcmp(Failure, TEXT("STALE_MODULE_OBJECT")) == 0)
		{
			return TEXT("FModuleOwnedReference Value");
		}
		if (FCString::Strcmp(Failure, TEXT("AMBIGUOUS_OVERLOAD")) == 0)
		{
			return TEXT("SelectAmbiguous(nullptr)");
		}
		return TEXT("MutateDerived(Root)");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kFailures[] = {
			TEXT("EXPLICIT_HANDLE"), TEXT("CONST_REMOVAL"), TEXT("TEMPORARY_OUT"), TEXT("EXPIRED_LOCAL_RETURN"),
			TEXT("UNRELATED_ASSIGNMENT"), TEXT("UNRELATED_CAST"), TEXT("NULL_MEMBER_ACCESS"),
			TEXT("STALE_MODULE_OBJECT"), TEXT("AMBIGUOUS_OVERLOAD"), TEXT("INCOMPATIBLE_INOUT")};
		static const TCHAR* const kRecoveries[] = {TEXT("FRESH_MODULE"), TEXT("SAME_MODULE_OR_CONTEXT")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(20);
		for (int32 Index = 0; Index < 20; ++Index)
		{
			const TCHAR* const Failure = kFailures[Index / 2];
			const TCHAR* const Recovery = kRecoveries[Index % 2];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-REF-FAILURE-%s-%s"), Failure, Recovery);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Needle = IndependentNeedle(Failure);
			Cell.bFault = FCString::Strcmp(Failure, TEXT("NULL_MEMBER_ACCESS")) == 0;
			Cell.bReject = !Cell.bFault;
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::ERefFailureKind;
		using AngelscriptTest::Generate::ERefFailureRecovery;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FRefFailureGenerator;
		using AngelscriptTest::Generate::FRefFailureParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FRefFailureGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-REF-FAILURE")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangRefFailureNullMemberAccessFreshModule")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("RefFailureGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("RefFailureGenerator dump path=%s cases=20 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(2, CaseCount));
		ASSERT_THAT(AreEqual(20, Cases.Num()));
		ASSERT_THAT(AreEqual(18, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int RecoverReferenceFailure"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/RefFailureGenerator/RepresentativeSources.as"));
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
				if (Representative.Contains(Expected.EntryName))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
			else
			{
				++FaultCount;
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(AreEqual(FString(TEXT("Null pointer access")), Cases[Index].ExpectedException));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
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
		ASSERT_THAT(AreEqual(18, RejectCount));
		ASSERT_THAT(AreEqual(2, FaultCount));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int RecoverReferenceFailure()"))));

		FRefFailureParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildReferenceFailureSource(DefaultParams).IsEmpty()));
		FRefFailureParams FaultParams;
		FaultParams.Failure = ERefFailureKind::NullMemberAccess;
		ASSERT_THAT(IsTrue(Generator.BuildReferenceFailureSource(FaultParams).Contains(TEXT("int Entry()"))));
		FaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildReferenceFailureSource(FaultParams).Contains(TEXT("int ProbeEntry()"))));
		FaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildReferenceFailureSource(FaultParams).IsEmpty()));
		FaultParams.FunctionName.Reset();
		FaultParams.Failure = static_cast<ERefFailureKind>(10);
		ASSERT_THAT(IsTrue(Generator.BuildReferenceFailureSource(FaultParams).IsEmpty()));
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
		ASSERT_THAT(AreEqual(20, CountOccurrences(Dump, TEXT("// Case: LANG-REF-FAILURE-"))));
		ASSERT_THAT(AreEqual(18, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/RefFailureGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("RefFailureGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
