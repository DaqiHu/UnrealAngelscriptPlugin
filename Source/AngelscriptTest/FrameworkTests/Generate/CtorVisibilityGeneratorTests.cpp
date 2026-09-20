#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestCtorVisibilityGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	CtorVisibilityGenerator,
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

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-CTOR-VISIBILITY-EXACT-OWNER-DEFAULT");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-CONDITION-IF-BOOL_LITERAL-TRUE");

	static bool IndependentDirect(const TCHAR* Selection)
	{
		return FCString::Strcmp(Selection, TEXT("EXACT")) == 0
			|| FCString::Strcmp(Selection, TEXT("PROMOTION")) == 0
			|| FCString::Strcmp(Selection, TEXT("EXPLICIT_CAST")) == 0;
	}

	static bool IndependentAccessible(const TCHAR* Visibility, const TCHAR* Site)
	{
		if (FCString::Strcmp(Visibility, TEXT("DEFAULT")) == 0)
		{
			return true;
		}
		if (FCString::Strcmp(Visibility, TEXT("PROTECTED")) == 0)
		{
			return FCString::Strcmp(Site, TEXT("OWNER")) == 0 || FCString::Strcmp(Site, TEXT("DERIVED")) == 0;
		}
		return FCString::Strcmp(Site, TEXT("OWNER")) == 0;
	}

	static bool IndependentReject(const TCHAR* Selection, const TCHAR* Site, const TCHAR* Visibility)
	{
		return !IndependentDirect(Selection) || !IndependentAccessible(Visibility, Site);
	}

	static FString IndependentNeedle(const TCHAR* Selection, const TCHAR* Site, const TCHAR* Visibility)
	{
		if (FCString::Strcmp(Selection, TEXT("PROMOTION")) == 0)
		{
			return TEXT("int8(7)");
		}
		if (FCString::Strcmp(Selection, TEXT("EXPLICIT_CAST")) == 0)
		{
			return TEXT("int64(int8(7))");
		}
		if (FCString::Strcmp(Selection, TEXT("IMPLICIT_REJECTED")) == 0)
		{
			return TEXT("ConsumeVisibilityTarget");
		}
		if (FCString::Strcmp(Selection, TEXT("AMBIGUOUS")) == 0)
		{
			return TEXT("float Extra = 1.0f");
		}
		if (FCString::Strcmp(Selection, TEXT("MISSING")) == 0)
		{
			return TEXT("FVisibilityNoMatch");
		}
		if (FCString::Strcmp(Site, TEXT("DERIVED")) == 0)
		{
			return TEXT("FVisibilityDerived");
		}
		if (FCString::Strcmp(Site, TEXT("UNRELATED")) == 0)
		{
			return TEXT("FVisibilityUnrelated");
		}
		if (FCString::Strcmp(Visibility, TEXT("PROTECTED")) == 0)
		{
			return TEXT("protected ");
		}
		if (FCString::Strcmp(Visibility, TEXT("PRIVATE")) == 0)
		{
			return TEXT("private ");
		}
		return TEXT("int(7)");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kSelections[] = {
			TEXT("EXACT"), TEXT("PROMOTION"), TEXT("EXPLICIT_CAST"),
			TEXT("IMPLICIT_REJECTED"), TEXT("AMBIGUOUS"), TEXT("MISSING")};
		static const TCHAR* const kSites[] = {
			TEXT("OWNER"), TEXT("DERIVED"), TEXT("UNRELATED"), TEXT("GLOBAL")};
		static const TCHAR* const kVisibilities[] = {
			TEXT("DEFAULT"), TEXT("PROTECTED"), TEXT("PRIVATE")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(72);
		for (int32 Index = 0; Index < 72; ++Index)
		{
			const TCHAR* const Selection = kSelections[Index / 12];
			const TCHAR* const Site = kSites[(Index / 3) % 4];
			const TCHAR* const Visibility = kVisibilities[Index % 3];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-CTOR-VISIBILITY-%s-%s-%s"), Selection, Site, Visibility);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Selection, Site, Visibility);
			Cell.Expected = Cell.bReject ? 0 : 7;
			Cell.Needle = IndependentNeedle(Selection, Site, Visibility);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::ECtorVisibilitySelection;
		using AngelscriptTest::Generate::ECtorVisibilitySite;
		using AngelscriptTest::Generate::ECtorVisibilityVisibility;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FCtorVisibilityGenerator;
		using AngelscriptTest::Generate::FCtorVisibilityParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FCtorVisibilityGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CTOR-VISIBILITY")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangCtorVisibilityExactOwnerDefault")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("CtorVisibilityGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("CtorVisibilityGenerator dump path=%s cases=72 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(21, CaseCount));
		ASSERT_THAT(AreEqual(72, Cases.Num()));
		ASSERT_THAT(AreEqual(51, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangCtorVisibilityExactOwnerDefault()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int EntryLangCtorVisibilityAmbiguousOwnerDefault()"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("void RecordConstructorVisibilitySelection(int Marker)"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/CtorVisibilityGenerator/RepresentativeSources.as"));
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
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(7, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(7, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedException.IsEmpty()));
				ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("constructor-visibility"))));
				ASSERT_THAT(IsFalse(Cases[Index].bLimitedObservation));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle) || Aggregate.Contains(Expected.Needle)));
				if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
		}
		ASSERT_THAT(AreEqual(51, RejectCount));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));

		FCtorVisibilityParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildConstructorVisibilitySource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildConstructorVisibilitySource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildConstructorVisibilitySource(DefaultParams).IsEmpty()));
		DefaultParams.FunctionName.Reset();
		DefaultParams.Selection = ECtorVisibilitySelection::Ambiguous;
		ASSERT_THAT(IsTrue(Generator.BuildConstructorVisibilitySource(DefaultParams).IsEmpty()));
		DefaultParams.Selection = ECtorVisibilitySelection::Exact;
		DefaultParams.Site = static_cast<ECtorVisibilitySite>(4);
		ASSERT_THAT(IsTrue(Generator.BuildConstructorVisibilitySource(DefaultParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(72, CountOccurrences(Dump, TEXT("// Case: LANG-CTOR-VISIBILITY-"))));
		ASSERT_THAT(AreEqual(51, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/CtorVisibilityGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("CtorVisibilityGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
