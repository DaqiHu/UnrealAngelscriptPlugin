#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestRefResolutionGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	RefResolutionGenerator,
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

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-REF-RESOLUTION-MUTABLE_EXACT-DIRECT-MUTABLE_LVALUE");

	static bool IndependentReject(const TCHAR* Candidate, const TCHAR* Site, const TCHAR* Source)
	{
		if (FCString::Strcmp(Candidate, TEXT("MISSING_CANDIDATE")) == 0
			|| FCString::Strcmp(Candidate, TEXT("INCOMPATIBLE_CANDIDATE")) == 0)
		{
			return true;
		}
		if (FCString::Strcmp(Candidate, TEXT("MUTABLE_EXACT")) == 0 && FCString::Strcmp(Source, TEXT("CONST_LVALUE")) == 0)
		{
			return true;
		}
		if (FCString::Strcmp(Candidate, TEXT("CONST_PAIR")) == 0 && FCString::Strcmp(Source, TEXT("CONST_LVALUE")) != 0)
		{
			return true;
		}
		return FCString::Strcmp(Candidate, TEXT("NULL_PAIR")) == 0
			&& FCString::Strcmp(Source, TEXT("NULL")) == 0
			&& FCString::Strcmp(Site, TEXT("DIRECT")) == 0;
	}

	static int32 IndependentExpected(const TCHAR* Candidate, const TCHAR* Source)
	{
		if (FCString::Strcmp(Candidate, TEXT("RETURN_COVARIANCE")) == 0)
		{
			return 1;
		}
		if (FCString::Strcmp(Candidate, TEXT("MUTABLE_EXACT")) == 0)
		{
			return 101;
		}
		if (FCString::Strcmp(Candidate, TEXT("CONST_PAIR")) == 0)
		{
			return FCString::Strcmp(Source, TEXT("CONST_LVALUE")) == 0 ? 202 : 201;
		}
		if (FCString::Strcmp(Candidate, TEXT("VALUE_VS_IN")) == 0)
		{
			return FCString::Strcmp(Source, TEXT("CONST_LVALUE")) == 0 ? 302 : 301;
		}
		if (FCString::Strcmp(Candidate, TEXT("BASE_VS_DERIVED")) == 0)
		{
			return FCString::Strcmp(Source, TEXT("DERIVED_VIEW")) == 0 ? 402 : 401;
		}
		if (FCString::Strcmp(Candidate, TEXT("NULL_PAIR")) == 0)
		{
			return 601;
		}
		if (FCString::Strcmp(Candidate, TEXT("NUMERIC_CONVERSION")) == 0)
		{
			return 701;
		}
		if (FCString::Strcmp(Candidate, TEXT("COMPETING_CONVERSIONS")) == 0)
		{
			return 802;
		}
		return 0;
	}

	static FString IndependentNeedle(const TCHAR* Candidate, const TCHAR* Site, const TCHAR* Source)
	{
		if (FCString::Strcmp(Site, TEXT("HELPER")) == 0)
		{
			return TEXT("InvokeThroughHelper");
		}
		if (FCString::Strcmp(Candidate, TEXT("NUMERIC_CONVERSION")) == 0
			|| FCString::Strcmp(Candidate, TEXT("COMPETING_CONVERSIONS")) == 0)
		{
			return FCString::Strcmp(Source, TEXT("NULL")) == 0 ? TEXT("SelectReference(0)") : TEXT(".GetValue())");
		}
		if (FCString::Strcmp(Source, TEXT("MUTABLE_LVALUE")) == 0)
		{
			return TEXT("MakeRefRoot(31)");
		}
		if (FCString::Strcmp(Source, TEXT("CONST_LVALUE")) == 0)
		{
			return TEXT("const FRefRoot Source = MakeRefRoot(32);");
		}
		if (FCString::Strcmp(Source, TEXT("TEMPORARY")) == 0)
		{
			return TEXT("MakeRefRoot(33)");
		}
		if (FCString::Strcmp(Source, TEXT("FIELD")) == 0)
		{
			return TEXT("Owner.Field");
		}
		if (FCString::Strcmp(Source, TEXT("PARAMETER")) == 0)
		{
			return TEXT("MakeRefRoot(35)");
		}
		if (FCString::Strcmp(Source, TEXT("BASE_VIEW")) == 0)
		{
			return TEXT("MakeRefDerivedAsRoot(36)");
		}
		if (FCString::Strcmp(Source, TEXT("DERIVED_VIEW")) == 0)
		{
			return TEXT("MakeRefDerived(37)");
		}
		return TEXT("FRefRoot Source = nullptr;");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kCandidates[] = {
			TEXT("MUTABLE_EXACT"), TEXT("CONST_PAIR"), TEXT("VALUE_VS_IN"), TEXT("BASE_VS_DERIVED"),
			TEXT("RETURN_COVARIANCE"), TEXT("NULL_PAIR"), TEXT("NUMERIC_CONVERSION"), TEXT("COMPETING_CONVERSIONS"),
			TEXT("MISSING_CANDIDATE"), TEXT("INCOMPATIBLE_CANDIDATE")};
		static const TCHAR* const kSites[] = {TEXT("DIRECT"), TEXT("HELPER")};
		static const TCHAR* const kSources[] = {
			TEXT("MUTABLE_LVALUE"), TEXT("CONST_LVALUE"), TEXT("TEMPORARY"), TEXT("FIELD"),
			TEXT("PARAMETER"), TEXT("BASE_VIEW"), TEXT("DERIVED_VIEW"), TEXT("NULL")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(160);
		for (int32 Index = 0; Index < 160; ++Index)
		{
			const TCHAR* const Candidate = kCandidates[Index / 16];
			const TCHAR* const Site = kSites[(Index / 8) % 2];
			const TCHAR* const Source = kSources[Index % 8];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-REF-RESOLUTION-%s-%s-%s"), Candidate, Site, Source);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Candidate, Site, Source);
			Cell.Expected = Cell.bReject ? 0 : IndependentExpected(Candidate, Source);
			Cell.Needle = IndependentNeedle(Candidate, Site, Source);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::ERefResolutionCandidate;
		using AngelscriptTest::Generate::ERefResolutionSite;
		using AngelscriptTest::Generate::ERefResolutionSource;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FRefResolutionGenerator;
		using AngelscriptTest::Generate::FRefResolutionParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FRefResolutionGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-REF-RESOLUTION")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangRefResolutionMutableExactDirectMutableLvalue")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("RefResolutionGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("RefResolutionGenerator dump path=%s cases=160 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(111, CaseCount));
		ASSERT_THAT(AreEqual(160, Cases.Num()));
		ASSERT_THAT(AreEqual(49, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("struct FResolutionOwner"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/RefResolutionGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 RejectCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			const FString CaseSource = Generator.BuildCaseSource(Expected.CaseId);
			ASSERT_THAT(IsTrue(CaseSource.Contains(Expected.Needle)));
			if (Expected.bReject)
			{
				++RejectCount;
				ASSERT_THAT(AreEqual(FString(), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::CompileReject));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::SourceOnly));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(RejectIds.Contains(Expected.CaseId)));
				ASSERT_THAT(AreEqual(CaseSource, Generator.BuildRejectSource(Expected.CaseId)));
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
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}
		ASSERT_THAT(AreEqual(49, RejectCount));

		FRefResolutionParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildReferenceResolutionSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FRefResolutionParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildReferenceResolutionSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildReferenceResolutionSource(ProbeParams).IsEmpty()));
		FRefResolutionParams RejectParams;
		RejectParams.Candidate = ERefResolutionCandidate::MissingCandidate;
		ASSERT_THAT(IsTrue(Generator.BuildReferenceResolutionSource(RejectParams).IsEmpty()));
		DefaultParams.Candidate = static_cast<ERefResolutionCandidate>(10);
		ASSERT_THAT(IsTrue(Generator.BuildReferenceResolutionSource(DefaultParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(160, CountOccurrences(Dump, TEXT("// Case: LANG-REF-RESOLUTION-"))));
		ASSERT_THAT(AreEqual(49, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/RefResolutionGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("RefResolutionGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
