#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestOpUnaryGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	OpUnaryGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString OperatorNeedle;
		FString OperandNeedle;
		int32 Expected = 0;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-OP-UNARY-MUTABLE_LVALUE-POSITIVE_INT-ZERO");

	static const TCHAR* OperationToken(const int32 Operation)
	{
		static const TCHAR* const kTokens[] = {
			TEXT("POSITIVE_INT8"), TEXT("POSITIVE_INT16"), TEXT("POSITIVE_INT"), TEXT("POSITIVE_INT64"),
			TEXT("POSITIVE_UINT8"), TEXT("POSITIVE_UINT16"), TEXT("POSITIVE_UINT"), TEXT("POSITIVE_UINT64"),
			TEXT("POSITIVE_FLOAT32"), TEXT("POSITIVE_FLOAT64"),
			TEXT("NEGATIVE_INT8"), TEXT("NEGATIVE_INT16"), TEXT("NEGATIVE_INT"), TEXT("NEGATIVE_INT64"),
			TEXT("NEGATIVE_UINT8"), TEXT("NEGATIVE_UINT16"), TEXT("NEGATIVE_UINT"), TEXT("NEGATIVE_UINT64"),
			TEXT("NEGATIVE_FLOAT32"), TEXT("NEGATIVE_FLOAT64"),
			TEXT("BIT_NOT_INT8"), TEXT("BIT_NOT_INT16"), TEXT("BIT_NOT_INT"), TEXT("BIT_NOT_INT64"),
			TEXT("BIT_NOT_UINT8"), TEXT("BIT_NOT_UINT16"), TEXT("BIT_NOT_UINT"), TEXT("BIT_NOT_UINT64")};
		return kTokens[Operation];
	}

	static const TCHAR* OperatorText(const int32 Operation)
	{
		if (Operation < 10)
		{
			return TEXT("+");
		}
		if (Operation < 20)
		{
			return TEXT("-");
		}
		return TEXT("~");
	}

	static int32 SourceType(const int32 Operation)
	{
		if (Operation < 10)
		{
			return Operation;
		}
		if (Operation < 20)
		{
			return Operation - 10;
		}
		return Operation - 20;
	}

	static int32 ResultType(const int32 Operation)
	{
		const int32 Source = SourceType(Operation);
		if (Operation >= 20 || Source <= 3 || Source >= 8)
		{
			return Source;
		}
		return Source - 4;
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kCategories[] = {
			TEXT("MUTABLE_LVALUE"), TEXT("CONST_LVALUE"), TEXT("TEMPORARY"), TEXT("FIELD"), TEXT("ALIAS")};
		static const TCHAR* const kValues[] = {
			TEXT("ZERO"), TEXT("ONE"), TEXT("NEGATIVE"), TEXT("NEAR_MIN"), TEXT("NEAR_MAX")};
		TArray<FExpectedCell> Cells;
		Cells.SetNum(700);
		for (int32 Index = 0; Index < 700; ++Index)
		{
			const int32 Category = Index / 140;
			const int32 Operation = (Index / 5) % 28;
			const int32 Value = Index % 5;
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-OP-UNARY-%s-%s-%s"),
				kCategories[Category],
				OperationToken(Operation),
				kValues[Value]);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Expected = 201 + ResultType(Operation);
			Cell.OperatorNeedle = OperatorText(Operation);
			if (Category == 0)
			{
				Cell.OperandNeedle = FString::Printf(TEXT("ObserveUnaryNumericType(%sValue)"), OperatorText(Operation));
			}
			else if (Category == 1)
			{
				Cell.OperandNeedle = TEXT("const ");
			}
			else if (Category == 2)
			{
				Cell.OperandNeedle = TEXT("MakeUnaryTemporary");
			}
			else if (Category == 3)
			{
				Cell.OperandNeedle = FString::Printf(TEXT("%sOwner.Value"), OperatorText(Operation));
			}
			else
			{
				Cell.OperandNeedle = TEXT("ObserveUnaryAliasType");
				Cell.OperatorNeedle = FString::Printf(TEXT("_%s(Value)"), OperationToken(Operation));
			}
		}
		return Cells;
	}

	static bool HasExtractableEntry(const FString& Source, const FString& EntryName)
	{
		const FString Header = FString::Printf(TEXT("int %s()"), *EntryName);
		return Source.StartsWith(Header) || Source.Contains(TEXT("\n") + Header);
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EOpUnaryCategory;
		using AngelscriptTest::Generate::EOpUnaryOperation;
		using AngelscriptTest::Generate::EOpUnaryValue;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FOpUnaryGenerator;
		using AngelscriptTest::Generate::FOpUnaryParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FOpUnaryGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-OP-UNARY")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangOpUnaryMutableLvaluePositiveIntZero")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("OpUnaryGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("OpUnaryGenerator dump path=%s cases=700 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(700, CaseCount));
		ASSERT_THAT(AreEqual(700, Cases.Num()));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/OpUnaryGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			ASSERT_THAT(IsTrue(Cases[Index].bLimitedObservation));
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.OperatorNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.OperandNeedle)));
			if (HasExtractableEntry(Representative, Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int ObserveUnaryNumericType(int8 Value)"))));

		FOpUnaryParams DefaultParams;
		DefaultParams.Operation = EOpUnaryOperation::PositiveInt;
		ASSERT_THAT(IsTrue(Generator.BuildUnarySource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildUnarySource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildUnarySource(DefaultParams).IsEmpty()));
		FOpUnaryParams InvalidEnum;
		InvalidEnum.Category = static_cast<EOpUnaryCategory>(5);
		ASSERT_THAT(IsTrue(Generator.BuildUnarySource(InvalidEnum).IsEmpty()));
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
		ASSERT_THAT(AreEqual(700, CountOccurrences(Dump, TEXT("// Case: LANG-OP-UNARY-"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/OpUnaryGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("OpUnaryGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
