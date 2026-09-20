#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestOpLogicalNotGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace OpLogicalNotGeneratorTest
{
struct FLogicalNotExpectedCell
{
	FString CaseId;
	FString EntryName;
	FString OperandNeedle;
	FString LiteralNeedle;
	int32 Expected = 0;
};

constexpr const TCHAR* ProbeCaseId = TEXT("LANG-OP-LOGICAL-NOT-MUTABLE_LVALUE-FALSE");
constexpr const TCHAR* ZeroCaseId = TEXT("LANG-OP-LOGICAL-NOT-MUTABLE_LVALUE-TRUE");

FString IndependentOperandNeedle(const TCHAR* Category)
{
	if (FCString::Strcmp(Category, TEXT("CONST_LVALUE")) == 0)
	{
		return TEXT("const bool Value = Input;");
	}
	if (FCString::Strcmp(Category, TEXT("TEMPORARY")) == 0)
	{
		return TEXT("!MakeLogicalNotTemporary(ObserveNotOperand(Input))");
	}
	if (FCString::Strcmp(Category, TEXT("FIELD")) == 0)
	{
		return TEXT("!ObserveNotOperand(Owner.Value)");
	}
	if (FCString::Strcmp(Category, TEXT("ALIAS")) == 0)
	{
		return TEXT("ApplyLogicalNotAlias(Value)");
	}
	return TEXT("!ObserveNotOperand(Value)");
}

TArray<FLogicalNotExpectedCell> BuildExpectedCells()
{
	static const TCHAR* const kCategories[] = {
		TEXT("MUTABLE_LVALUE"), TEXT("CONST_LVALUE"), TEXT("TEMPORARY"), TEXT("FIELD"), TEXT("ALIAS")};
	static const TCHAR* const kValues[] = {TEXT("FALSE"), TEXT("TRUE")};

	TArray<FLogicalNotExpectedCell> Cells;
	Cells.SetNum(10);
	for (int32 Index = 0; Index < 10; ++Index)
	{
		const TCHAR* const Category = kCategories[Index / 2];
		const TCHAR* const Value = kValues[Index % 2];
		const bool bTrue = FCString::Strcmp(Value, TEXT("TRUE")) == 0;
		FLogicalNotExpectedCell& Cell = Cells[Index];
		Cell.CaseId = FString::Printf(TEXT("LANG-OP-LOGICAL-NOT-%s-%s"), Category, Value);
		Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
		Cell.OperandNeedle = IndependentOperandNeedle(Category);
		Cell.LiteralNeedle = bTrue ? TEXT("bool Input = true;") : TEXT("bool Input = false;");
		Cell.Expected = bTrue ? 0 : 1;
	}
	return Cells;
}

FString ExpectedDeclaration(const FLogicalNotExpectedCell& Cell)
{
	return FString::Printf(TEXT("int %s()"), *Cell.EntryName);
}
}

TEST_CLASS_WITH_FLAGS(
	OpLogicalNotGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using OpLogicalNotGeneratorTest::BuildExpectedCells;
		using OpLogicalNotGeneratorTest::ExpectedDeclaration;
		using OpLogicalNotGeneratorTest::FLogicalNotExpectedCell;
		using OpLogicalNotGeneratorTest::ProbeCaseId;
		using OpLogicalNotGeneratorTest::ZeroCaseId;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EOpLogicalNotCategory;
		using AngelscriptTest::Generate::EOpLogicalNotValue;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FOpLogicalNotGenerator;
		using AngelscriptTest::Generate::FOpLogicalNotParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FLogicalNotExpectedCell> ExpectedCells = BuildExpectedCells();
		FOpLogicalNotGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-OP-LOGICAL-NOT")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangOpLogicalNotMutableLvalueFalse")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("OpLogicalNotGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("OpLogicalNotGenerator dump path=%s cases=10 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(10, CaseCount));
		ASSERT_THAT(AreEqual(10, Cases.Num()));
		ASSERT_THAT(AreEqual(10, ExpectedCells.Num()));

		int32 RejectCount = 0;
		int32 ZeroCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FLogicalNotExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(ExpectedDeclaration(Expected), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedException.IsEmpty()));
			ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("ObserveNotOperand"))));
			ASSERT_THAT(IsFalse(Cases[Index].bLimitedObservation));
			if (Expected.Expected == 0)
			{
				++ZeroCount;
			}
			if (Cases[Index].Outcome == EGeneratedCaseOutcome::CompileReject)
			{
				++RejectCount;
			}
		}
		ASSERT_THAT(AreEqual(0, RejectCount));
		ASSERT_THAT(AreEqual(5, ZeroCount));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		const FString OwnedAgain = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(IsTrue(OwnedCase.Len() > 0));
		ASSERT_THAT(AreEqual(OwnedCase, OwnedAgain));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangOpLogicalNotMutableLvalueFalse()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangOpLogicalNotMutableLvalueFalse()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("struct FLogicalNotOwner"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("bool MakeLogicalNotTemporary"))));

		FOpLogicalNotParams DefaultParams;
		const FString DefaultSingle = Generator.BuildLogicalNotSource(DefaultParams);
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsFalse(DefaultSingle.Contains(TEXT("int EntryLang"))));

		FOpLogicalNotParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		const FString ProbeSource = Generator.BuildLogicalNotSource(ProbeParams);
		ASSERT_THAT(IsTrue(ProbeSource.Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(ProbeSource.Contains(TEXT("int Entry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/OpLogicalNotGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FLogicalNotExpectedCell& Expected : ExpectedCells)
		{
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.LiteralNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.OperandNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("return Result ? 1 : 0;"))));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FOpLogicalNotParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildLogicalNotSource(InvalidName).IsEmpty()));
		FOpLogicalNotParams InvalidEnum = DefaultParams;
		InvalidEnum.Category = static_cast<EOpLogicalNotCategory>(5);
		ASSERT_THAT(IsTrue(Generator.BuildLogicalNotSource(InvalidEnum).IsEmpty()));
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
		ASSERT_THAT(AreEqual(ExpectedCells[0].Expected, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(OutputFile.EndsWith(TEXT("GeneratedCases/OpLogicalNotGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\OpLogicalNotGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(10, CountOccurrences(Dump, TEXT("// Case: LANG-OP-LOGICAL-NOT-"))));
		for (const FLogicalNotExpectedCell& Expected : ExpectedCells)
		{
			ASSERT_THAT(IsTrue(Dump.Contains(FString::Printf(TEXT("// Case: %s"), *Expected.CaseId))));
			FString DumpBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Dump, Expected.EntryName, DumpBody)));
			FString AggregateBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, AggregateBody)));
			ASSERT_THAT(AreEqual(AggregateBody, DumpBody));
		}
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		ASSERT_THAT(IsTrue(AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("")).IsEmpty()));
		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/OpLogicalNotGenerator.as"), Dump, ProbeError)));
		ASSERT_THAT(IsTrue(ProbeError.Len() > 0));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("OpLogicalNotGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
