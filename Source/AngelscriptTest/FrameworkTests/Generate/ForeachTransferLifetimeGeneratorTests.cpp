#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestForeachTransferLifetimeGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ForeachTransferLifetimeGenerator,
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
		bool bFault = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-FE-TRANSFER-LIFETIME-COMPLETE-SINGLE-PRIMITIVE_VALUE");

	static int32 IndependentVisit(const TCHAR* Transfer, const int32 Limit)
	{
		if (FCString::Strcmp(Transfer, TEXT("RETURN")) == 0
			|| FCString::Strcmp(Transfer, TEXT("EXCEPTION")) == 0
			|| FCString::Strcmp(Transfer, TEXT("BREAK_FIRST")) == 0)
		{
			return Limit < 1 ? Limit : 1;
		}
		if (FCString::Strcmp(Transfer, TEXT("BREAK_MIDDLE")) == 0)
		{
			return Limit < 2 ? Limit : 2;
		}
		return Limit;
	}

	static int32 IndependentExpected(const TCHAR* Transfer, const TCHAR* Nesting)
	{
		if (FCString::Strcmp(Transfer, TEXT("EXCEPTION")) == 0)
		{
			return 0;
		}
		const int32 InnerLimit = FCString::Strcmp(Nesting, TEXT("NESTED_DISTINCT")) == 0 ? 2 : 3;
		const int32 InnerVisits = IndependentVisit(Transfer, InnerLimit);
		const bool bStops = FCString::Strcmp(Transfer, TEXT("RETURN")) == 0;
		if (FCString::Strcmp(Nesting, TEXT("SINGLE")) == 0)
		{
			return InnerVisits;
		}
		if (FCString::Strcmp(Nesting, TEXT("NESTED_SAME")) == 0
			|| FCString::Strcmp(Nesting, TEXT("NESTED_DISTINCT")) == 0)
		{
			const int32 Outer = bStops ? 1 : 3;
			return Outer * 100 + Outer * InnerVisits;
		}
		const int32 Outer = bStops ? 1 : 2;
		return Outer * 10 + Outer * InnerVisits;
	}

	static FString IndependentNeedle(const TCHAR* Nesting)
	{
		if (FCString::Strcmp(Nesting, TEXT("INSIDE_FOR")) == 0)
		{
			return TEXT("for (int OuterIndex = 0; OuterIndex < 2; ++OuterIndex)");
		}
		if (FCString::Strcmp(Nesting, TEXT("NESTED_DISTINCT")) == 0)
		{
			return TEXT("OtherRange");
		}
		if (FCString::Strcmp(Nesting, TEXT("NESTED_SAME")) == 0)
		{
			return TEXT("Trace += 100;");
		}
		return TEXT("foreach (");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kTransfers[] = {
			TEXT("COMPLETE"), TEXT("BREAK_FIRST"), TEXT("BREAK_MIDDLE"), TEXT("CONTINUE_FIRST"),
			TEXT("CONTINUE_MIDDLE"), TEXT("RETURN"), TEXT("EXCEPTION")};
		static const TCHAR* const kNestings[] = {
			TEXT("SINGLE"), TEXT("NESTED_SAME"), TEXT("NESTED_DISTINCT"), TEXT("INSIDE_FOR")};
		static const TCHAR* const kElements[] = {
			TEXT("PRIMITIVE_VALUE"), TEXT("VALUE_OBJECT_COPY"), TEXT("VALUE_OBJECT_CONST_REF")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(84);
		for (int32 Index = 0; Index < 84; ++Index)
		{
			const TCHAR* const Transfer = kTransfers[Index / 12];
			const TCHAR* const Nesting = kNestings[(Index / 3) % 4];
			const TCHAR* const Element = kElements[Index % 3];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-FE-TRANSFER-LIFETIME-%s-%s-%s"), Transfer, Nesting, Element);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Needle = IndependentNeedle(Nesting);
			Cell.bFault = FCString::Strcmp(Transfer, TEXT("EXCEPTION")) == 0;
			Cell.Expected = IndependentExpected(Transfer, Nesting);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EForeachLifetimeTransfer;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FForeachTransferLifetimeGenerator;
		using AngelscriptTest::Generate::FForeachTransferLifetimeParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FForeachTransferLifetimeGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-FE-TRANSFER-LIFETIME")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangFeTransferLifetimeCompleteSinglePrimitiveValue")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ForeachTransferLifetimeGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ForeachTransferLifetimeGenerator dump path=%s cases=84 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(84, CaseCount));
		ASSERT_THAT(AreEqual(84, Cases.Num()));

		int32 FaultCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			if (Expected.bFault)
			{
				++FaultCount;
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")), Cases[Index].ExpectedException));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
			}
			else
			{
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			}
		}
		ASSERT_THAT(AreEqual(12, FaultCount));

		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangFeTransferLifetimeCompleteSinglePrimitiveValue()"))));

		FForeachTransferLifetimeParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildTransferSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FForeachTransferLifetimeParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildTransferSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/ForeachTransferLifetimeGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
			if (Expected.bFault)
			{
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("1 / Zero"))));
			}
			ASSERT_THAT(AreEqual(1, CountOccurrences(Dump, FString::Printf(TEXT("// Case: %s\n"), *Expected.CaseId))));
			if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FForeachTransferLifetimeParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildTransferSource(InvalidName).IsEmpty()));
		FForeachTransferLifetimeParams InvalidEnum = DefaultParams;
		InvalidEnum.Transfer = static_cast<EForeachLifetimeTransfer>(7);
		ASSERT_THAT(IsTrue(Generator.BuildTransferSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-FE-STRUCTURAL-MUTATION-ONE-STABLE-PRIMITIVE_VALUE")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(84, CountOccurrences(Dump, TEXT("// Case: "))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/ForeachTransferLifetimeGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ForeachTransferLifetimeGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
