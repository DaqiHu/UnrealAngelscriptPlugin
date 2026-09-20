#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestOpUnaryRejectionGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	OpUnaryRejectionGenerator,
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

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-OP-UNARY-REJECTION-MUTABLE_LVALUE-POSITIVE_BOOL");

	static const TCHAR* FailureNeedle(const int32 Failure)
	{
		switch (Failure)
		{
		case 0:
			return TEXT("+Value");
		case 1:
			return TEXT("-Value");
		case 2:
			return TEXT("~Value");
		case 3:
			return TEXT("!Value");
		case 4:
			return TEXT("!Value");
		case 5:
			return TEXT("!Value");
		case 6:
			return TEXT("~Value");
		default:
			return TEXT("~Value");
		}
	}

	static FString IndependentNeedle(const int32 Category, const int32 Failure)
	{
		if (Category == 2)
		{
			return FString::Printf(TEXT("%sMakeInvalidUnaryTemporary"), Failure < 3 && Failure != 2 && Failure != 0 && Failure != 1
				? TEXT("!")
				: (Failure == 0 ? TEXT("+") : (Failure == 1 ? TEXT("-") : (Failure == 2 || Failure >= 6 ? TEXT("~") : TEXT("!")))));
		}
		if (Category == 3)
		{
			return TEXT("Owner.Value");
		}
		if (Category == 4)
		{
			return Failure == 0 ? TEXT("+Input") : (Failure == 1 ? TEXT("-Input") : (Failure == 2 || Failure >= 6 ? TEXT("~Input") : TEXT("!Input")));
		}
		return FailureNeedle(Failure);
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kCategories[] = {
			TEXT("MUTABLE_LVALUE"), TEXT("CONST_LVALUE"), TEXT("TEMPORARY"), TEXT("FIELD"), TEXT("ALIAS")};
		static const TCHAR* const kFailures[] = {
			TEXT("POSITIVE_BOOL"), TEXT("NEGATIVE_BOOL"), TEXT("BIT_NOT_BOOL"), TEXT("LOGICAL_NOT_SIGNED"),
			TEXT("LOGICAL_NOT_UNSIGNED"), TEXT("LOGICAL_NOT_FLOAT"), TEXT("BIT_NOT_FLOAT32"), TEXT("BIT_NOT_FLOAT64")};
		TArray<FExpectedCell> Cells;
		Cells.SetNum(40);
		for (int32 Index = 0; Index < 40; ++Index)
		{
			const int32 Category = Index / 8;
			const int32 Failure = Index % 8;
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-OP-UNARY-REJECTION-%s-%s"),
				kCategories[Category],
				kFailures[Failure]);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Needle = IndependentNeedle(Category, Failure);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EOpUnaryRejectionCategory;
		using AngelscriptTest::Generate::EOpUnaryRejectionFailure;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FOpUnaryRejectionGenerator;
		using AngelscriptTest::Generate::FOpUnaryRejectionParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FOpUnaryRejectionGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-OP-UNARY-REJECTION")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangOpUnaryRejectionMutableLvaluePositiveBool")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 1;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("OpUnaryRejectionGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("OpUnaryRejectionGenerator dump path=%s cases=40 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(0, CaseCount));
		ASSERT_THAT(AreEqual(40, Cases.Num()));
		ASSERT_THAT(AreEqual(40, RejectIds.Num()));
		ASSERT_THAT(IsTrue(Aggregate.IsEmpty()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/OpUnaryRejectionGenerator/RepresentativeSources.as"));
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
			ASSERT_THAT(IsTrue(RejectSource.Contains(TEXT("UNARY_CAUSE"))));
			ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(RejectSource, Expected.EntryName, GeneratedBody)));
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		const FString OwnedReject = Generator.BuildRejectSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedReject, Generator.BuildRejectSource(ProbeCaseId)));

		FOpUnaryRejectionParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildUnaryFailureSource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildUnaryFailureSource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(Generator.BuildUnaryFailureSource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildUnaryFailureSource(DefaultParams).IsEmpty()));
		FOpUnaryRejectionParams InvalidEnum;
		InvalidEnum.Category = static_cast<EOpUnaryRejectionCategory>(5);
		ASSERT_THAT(IsTrue(Generator.BuildUnaryFailureSource(InvalidEnum).IsEmpty()));
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
		ASSERT_THAT(AreEqual(40, CountOccurrences(Dump, TEXT("// Case: LANG-OP-UNARY-REJECTION-"))));
		ASSERT_THAT(AreEqual(40, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/OpUnaryRejectionGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("OpUnaryRejectionGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
