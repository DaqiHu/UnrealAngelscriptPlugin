#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestRefLifetimeGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	RefLifetimeGenerator,
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

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-REF-LIFETIME-IDENTITY-OWNER_LIVE-NONE");
	static constexpr const TCHAR* ZeroCaseId = TEXT("LANG-REF-LIFETIME-IDENTITY-OWNER_LIVE-NONE");

	static bool IndependentReject(const TCHAR* Owner)
	{
		return FCString::Strcmp(Owner, TEXT("MODULE_RETAINED")) == 0
			|| FCString::Strcmp(Owner, TEXT("MODULE_DISCARDED")) == 0;
	}

	static int32 IndependentExpected(const TCHAR* Owner, const TCHAR* Reference)
	{
		if (IndependentReject(Owner))
		{
			return 0;
		}
		if (FCString::Strcmp(Owner, TEXT("GC_CYCLE")) == 0)
		{
			return 1;
		}
		return FCString::Strcmp(Reference, TEXT("NONE")) == 0 ? 0 : 1;
	}

	static FString IndependentNeedle(const TCHAR* Owner, const TCHAR* Reference)
	{
		if (IndependentReject(Owner))
		{
			return TEXT("FRefRoot GReferenceOwner;");
		}
		if (FCString::Strcmp(Owner, TEXT("GC_CYCLE")) == 0)
		{
			return TEXT("return 1;");
		}
		if (FCString::Strcmp(Owner, TEXT("RETURNED_ALIAS")) == 0)
		{
			return TEXT("FRefRoot Returned = Primary;");
		}
		if (FCString::Strcmp(Reference, TEXT("NONE")) == 0)
		{
			return TEXT("FRefRoot Primary = nullptr;");
		}
		if (FCString::Strcmp(Reference, TEXT("MULTIPLE_ALIASES")) == 0)
		{
			return TEXT("FRefRoot AliasA = Primary;");
		}
		if (FCString::Strcmp(Reference, TEXT("CYCLE")) == 0)
		{
			return TEXT("Primary.Link(Primary);");
		}
		return TEXT("FRefRoot Primary = MakeRefRoot(41);");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kObservations[] = {
			TEXT("IDENTITY"), TEXT("REFCOUNT"), TEXT("WEAK_FLAG"), TEXT("DESTRUCTION"), TEXT("GC_STATS")};
		static const TCHAR* const kOwners[] = {
			TEXT("OWNER_LIVE"), TEXT("SCOPE_EXIT"), TEXT("RETURNED_ALIAS"), TEXT("MODULE_RETAINED"),
			TEXT("MODULE_DISCARDED"), TEXT("CONTEXT_RETAINED"), TEXT("CONTEXT_RELEASED"), TEXT("GC_CYCLE")};
		static const TCHAR* const kReferences[] = {
			TEXT("NONE"), TEXT("ONE_ALIAS"), TEXT("MULTIPLE_ALIASES"), TEXT("CYCLE"), TEXT("WEAK_FLAG")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(200);
		for (int32 Index = 0; Index < 200; ++Index)
		{
			const TCHAR* const Observation = kObservations[Index / 40];
			const TCHAR* const Owner = kOwners[(Index / 5) % 8];
			const TCHAR* const Reference = kReferences[Index % 5];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-REF-LIFETIME-%s-%s-%s"), Observation, Owner, Reference);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Owner);
			Cell.Expected = IndependentExpected(Owner, Reference);
			Cell.Needle = IndependentNeedle(Owner, Reference);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::ERefLifetimeObservation;
		using AngelscriptTest::Generate::ERefLifetimeOwner;
		using AngelscriptTest::Generate::ERefLifetimeReference;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FRefLifetimeGenerator;
		using AngelscriptTest::Generate::FRefLifetimeParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FRefLifetimeGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-REF-LIFETIME")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangRefLifetimeIdentityOwnerLiveNone")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("RefLifetimeGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("RefLifetimeGenerator dump path=%s cases=200 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(150, CaseCount));
		ASSERT_THAT(AreEqual(200, Cases.Num()));
		ASSERT_THAT(AreEqual(50, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/RefLifetimeGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 RejectCount = 0;
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
				ASSERT_THAT(IsTrue(Cases[Index].bLimitedObservation));
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
				if (Expected.Expected == 0)
				{
					++ZeroCount;
				}
			}
		}
		ASSERT_THAT(AreEqual(50, RejectCount));
		ASSERT_THAT(IsTrue(ZeroCount > 0));

		FRefLifetimeParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildReferenceLifetimeSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FRefLifetimeParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildReferenceLifetimeSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildReferenceLifetimeSource(ProbeParams).IsEmpty()));
		FRefLifetimeParams RejectParams;
		RejectParams.Owner = ERefLifetimeOwner::ModuleRetained;
		ASSERT_THAT(IsTrue(Generator.BuildReferenceLifetimeSource(RejectParams).IsEmpty()));
		DefaultParams.Owner = static_cast<ERefLifetimeOwner>(8);
		ASSERT_THAT(IsTrue(Generator.BuildReferenceLifetimeSource(DefaultParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE")).IsEmpty()));
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

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(200, CountOccurrences(Dump, TEXT("// Case: LANG-REF-LIFETIME-"))));
		ASSERT_THAT(AreEqual(50, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/RefLifetimeGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("RefLifetimeGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
