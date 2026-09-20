#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestInhCastGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	InhCastGenerator,
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
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-INH-CAST-MUTABLE-EXACT-ASSIGN");
	static constexpr const TCHAR* ZeroCaseId = TEXT("LANG-INH-CAST-MUTABLE-SIBLING-IDENTITY_COMPARE");

	static bool IndependentSuccess(const TCHAR* Relation)
	{
		return FCString::Strcmp(Relation, TEXT("EXACT")) == 0
			|| FCString::Strcmp(Relation, TEXT("UPCAST")) == 0
			|| FCString::Strcmp(Relation, TEXT("DOWNCAST_SUCCESS")) == 0;
	}

	static int32 IndependentExpected(const TCHAR* Relation, const TCHAR* Use)
	{
		if (FCString::Strcmp(Use, TEXT("IDENTITY_COMPARE")) == 0)
		{
			return (IndependentSuccess(Relation) || FCString::Strcmp(Relation, TEXT("NULL")) == 0) ? 1 : 0;
		}
		return IndependentSuccess(Relation) ? 2 : -1;
	}

	static FString IndependentNeedle(const TCHAR* Relation)
	{
		return FCString::Strcmp(Relation, TEXT("UPCAST")) == 0
			? FString(TEXT("cast<FCastRoot>"))
			: FString(TEXT("cast<FCastDerived>"));
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kConstness[] = {TEXT("MUTABLE"), TEXT("CONST")};
		static const TCHAR* const kRelations[] = {
			TEXT("EXACT"), TEXT("UPCAST"), TEXT("DOWNCAST_SUCCESS"), TEXT("DOWNCAST_FAILURE"),
			TEXT("SIBLING"), TEXT("NULL")};
		static const TCHAR* const kUses[] = {
			TEXT("ASSIGN"), TEXT("ARGUMENT"), TEXT("RETURN"), TEXT("IDENTITY_COMPARE"), TEXT("MEMBER_CALL")};
		TArray<FExpectedCell> Cells;
		Cells.SetNum(60);
		for (int32 Index = 0; Index < 60; ++Index)
		{
			const TCHAR* const Constness = kConstness[Index / 30];
			const TCHAR* const Relation = kRelations[(Index / 5) % 6];
			const TCHAR* const Use = kUses[Index % 5];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-INH-CAST-%s-%s-%s"), Constness, Relation, Use);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Expected = IndependentExpected(Relation, Use);
			Cell.Needle = IndependentNeedle(Relation);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EInhCastConstness;
		using AngelscriptTest::Generate::EInhCastRelation;
		using AngelscriptTest::Generate::EInhCastUse;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FInhCastGenerator;
		using AngelscriptTest::Generate::FInhCastParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FInhCastGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-INH-CAST")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangInhCastMutableExactAssign")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("InhCastGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("InhCastGenerator dump path=%s cases=60 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(60, CaseCount));
		ASSERT_THAT(AreEqual(60, Cases.Num()));

		int32 RejectCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(
				FString::Printf(TEXT("int %s()"), *Expected.EntryName),
				Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			if (Cases[Index].Outcome == EGeneratedCaseOutcome::CompileReject)
			{
				++RejectCount;
			}
		}
		ASSERT_THAT(AreEqual(0, RejectCount));

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangInhCastMutableExactAssign()"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("class FCastRoot"))));
		ASSERT_THAT(AreEqual(60, CountOccurrences(Aggregate, TEXT("int ObserveCast"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));

		FInhCastParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildInheritanceCastSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FInhCastParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildInheritanceCastSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(Generator.BuildInheritanceCastSource(ProbeParams).Contains(TEXT("int Entry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/InhCastGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			const FString CaseSource = Generator.BuildCaseSource(Expected.CaseId);
			ASSERT_THAT(IsTrue(
				CaseSource.Contains(Expected.Needle),
				*FString::Printf(TEXT("%s missing %s sourceLen=%d"), *Expected.CaseId, *Expected.Needle, CaseSource.Len())));
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FInhCastParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildInheritanceCastSource(InvalidName).IsEmpty()));
		FInhCastParams InvalidEnum = DefaultParams;
		InvalidEnum.Relation = static_cast<EInhCastRelation>(6);
		ASSERT_THAT(IsTrue(Generator.BuildInheritanceCastSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-INH-ACCESS-DEFAULT-FIELD-OWNER")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(ZeroCaseId)));
		bool bFoundZeroCase = false;
		for (int32 Index = 0; Index < Cases.Num(); ++Index)
		{
			if (Cases[Index].CaseId == ZeroCaseId)
			{
				bFoundZeroCase = true;
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			}
		}
		ASSERT_THAT(IsTrue(bFoundZeroCase));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(2, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(60, CountOccurrences(Dump, TEXT("// Case: LANG-INH-CAST-"))));
		ASSERT_THAT(AreEqual(0, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/InhCastGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("InhCastGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
