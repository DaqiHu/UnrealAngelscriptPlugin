#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestOpOverloadDuplicateDeclarationGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	OpOverloadDuplicateDeclarationGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString MethodNeedle;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-OP-OVERLOAD-DUPLICATE-DECLARATION-DUPLICATE-UNARY");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-OP-NUMERIC-BINARY-INT-ADD-INT-ZERO");

	static const TCHAR* FamilyToken(const int32 Index)
	{
		static const TCHAR* const kTokens[] = {
			TEXT("UNARY"), TEXT("BINARY"), TEXT("COMPARISON"), TEXT("INDEX"),
			TEXT("CALL"), TEXT("CONVERSION"), TEXT("ASSIGNMENT")};
		return kTokens[Index];
	}

	static const TCHAR* MethodNeedle(const int32 Index)
	{
		static const TCHAR* const kMethods[] = {
			TEXT("opNeg"), TEXT("opAdd"), TEXT("opEquals"), TEXT("opIndex"),
			TEXT("opCall"), TEXT("opImplConv"), TEXT("opAssign")};
		return kMethods[Index];
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		TArray<FExpectedCell> Cells;
		Cells.SetNum(7);
		for (int32 Index = 0; Index < 7; ++Index)
		{
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-OP-OVERLOAD-DUPLICATE-DECLARATION-DUPLICATE-%s"),
				FamilyToken(Index));
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.MethodNeedle = MethodNeedle(Index);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EOpOverloadDuplicateDeclarationFamily;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FOpOverloadDuplicateDeclarationGenerator;
		using AngelscriptTest::Generate::FOpOverloadDuplicateDeclarationParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FOpOverloadDuplicateDeclarationGenerator Generator;
		ASSERT_THAT(AreEqual(
			FString(TEXT("LANG-OP-OVERLOAD-DUPLICATE-DECLARATION")),
			FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangOpOverloadDuplicateDeclarationDuplicateUnary")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = -1;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("OpOverloadDuplicateDeclarationGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("OpOverloadDuplicateDeclarationGenerator dump path=%s cases=7 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(0, CaseCount));
		ASSERT_THAT(IsTrue(Aggregate.IsEmpty()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(AreEqual(7, Cases.Num()));
		ASSERT_THAT(AreEqual(7, RejectIds.Num()));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/OpOverloadDuplicateDeclarationGenerator/RepresentativeSources.as"));
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
			ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
			ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.MethodNeedle)));
			ASSERT_THAT(IsTrue(RejectSource.Contains(TEXT("OP_OVERLOAD_DUPLICATE_CAUSE"))));
			ASSERT_THAT(AreEqual(2, CountOccurrences(RejectSource, Expected.MethodNeedle)));
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(RejectSource, Expected.EntryName, GeneratedBody)));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		const FString OwnedReject = Generator.BuildRejectSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedReject, Generator.BuildRejectSource(ProbeCaseId)));

		FOpOverloadDuplicateDeclarationParams DefaultParams;
		const FString DefaultSingle = Generator.BuildDuplicateSource(DefaultParams);
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("opNeg"))));
		FOpOverloadDuplicateDeclarationParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildDuplicateSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildDuplicateSource(ProbeParams).IsEmpty()));
		ProbeParams.FunctionName.Reset();
		ProbeParams.Family = static_cast<EOpOverloadDuplicateDeclarationFamily>(7);
		ASSERT_THAT(IsTrue(Generator.BuildDuplicateSource(ProbeParams).IsEmpty()));
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
		ASSERT_THAT(AreEqual(7, CountOccurrences(Dump, TEXT("// Case: LANG-OP-OVERLOAD-DUPLICATE-DECLARATION-"))));
		ASSERT_THAT(AreEqual(7, CountOccurrences(Dump, TEXT("// Reject module"))));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			ASSERT_THAT(IsTrue(Dump.Contains(FString::Printf(TEXT("// Case: %s"), *Expected.CaseId))));
			FString DumpBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Dump, Expected.EntryName, DumpBody)));
		}
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/OpOverloadDuplicateDeclarationGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("OpOverloadDuplicateDeclarationGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
