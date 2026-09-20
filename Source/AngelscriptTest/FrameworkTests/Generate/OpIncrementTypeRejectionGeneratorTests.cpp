#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestOpIncrementTypeRejectionGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	OpIncrementTypeRejectionGenerator,
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

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-OP-INCREMENT-TYPE-REJECTION-LOCAL-PRE_INCREMENT");
	static constexpr const TCHAR* ForeignCaseId =
		TEXT("LANG-OP-INCREMENT-TARGET-REJECTION-CONST_INVALID-PRE_INCREMENT-INT");

	static FString IndependentNeedle(const TCHAR* Category, const TCHAR* Operator)
	{
		const bool bPrefix = FCString::Strcmp(Operator, TEXT("PRE_INCREMENT")) == 0
			|| FCString::Strcmp(Operator, TEXT("PRE_DECREMENT")) == 0;
		const TCHAR* const Token = (FCString::Strcmp(Operator, TEXT("PRE_INCREMENT")) == 0
			|| FCString::Strcmp(Operator, TEXT("POST_INCREMENT")) == 0)
			? TEXT("++")
			: TEXT("--");
		if (FCString::Strcmp(Category, TEXT("FIELD")) == 0)
		{
			return bPrefix ? FString::Printf(TEXT("%sOwner.Value; // INCREMENT_CAUSE"), Token)
				: FString::Printf(TEXT("Owner.Value%s; // INCREMENT_CAUSE"), Token);
		}
		if (FCString::Strcmp(Category, TEXT("PROPERTY")) == 0)
		{
			return TEXT("FIncrementProperty_bool Owner = MakeIncrementProperty_bool();");
		}
		return bPrefix ? FString::Printf(TEXT("%sValue; // INCREMENT_CAUSE"), Token)
			: FString::Printf(TEXT("Value%s; // INCREMENT_CAUSE"), Token);
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kCategories[] = {TEXT("LOCAL"), TEXT("FIELD"), TEXT("PROPERTY"), TEXT("ALIAS")};
		static const TCHAR* const kOperators[] = {
			TEXT("PRE_INCREMENT"), TEXT("POST_INCREMENT"), TEXT("PRE_DECREMENT"), TEXT("POST_DECREMENT")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(16);
		for (int32 Index = 0; Index < 16; ++Index)
		{
			const TCHAR* const Category = kCategories[Index / 4];
			const TCHAR* const Operator = kOperators[Index % 4];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-OP-INCREMENT-TYPE-REJECTION-%s-%s"), Category, Operator);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Needle = IndependentNeedle(Category, Operator);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EOpIncrementTypeRejectionCategory;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FOpIncrementTypeRejectionGenerator;
		using AngelscriptTest::Generate::FOpIncrementTypeRejectionParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FOpIncrementTypeRejectionGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-OP-INCREMENT-TYPE-REJECTION")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangOpIncrementTypeRejectionLocalPreIncrement")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("OpIncrementTypeRejectionGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("OpIncrementTypeRejectionGenerator dump path=%s cases=16 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(0, CaseCount));
		ASSERT_THAT(AreEqual(FString(), Aggregate));
		ASSERT_THAT(AreEqual(16, Cases.Num()));
		ASSERT_THAT(AreEqual(16, RejectIds.Num()));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/OpIncrementTypeRejectionGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(FString(), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::CompileReject));
			ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
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

		FOpIncrementTypeRejectionParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildBoolRejectionSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FOpIncrementTypeRejectionParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildBoolRejectionSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildBoolRejectionSource(ProbeParams).IsEmpty()));
		DefaultParams.Category = static_cast<EOpIncrementTypeRejectionCategory>(4);
		ASSERT_THAT(IsTrue(Generator.BuildBoolRejectionSource(DefaultParams).IsEmpty()));
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
		ASSERT_THAT(AreEqual(16, CountOccurrences(Dump, TEXT("// Case: LANG-OP-INCREMENT-TYPE-REJECTION-"))));
		ASSERT_THAT(AreEqual(16, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/OpIncrementTypeRejectionGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("OpIncrementTypeRejectionGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
