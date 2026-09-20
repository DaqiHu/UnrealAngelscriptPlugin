#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestOpAssignmentTypeRejectionGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	OpAssignmentTypeRejectionGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString OperatorNeedle;
		FString CategoryNeedle;
		FString LiteralNeedle;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-OP-ASSIGNMENT-TYPE-REJECTION-LOCAL-ADD_ASSIGN_BOOL");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-OP-ASSIGNMENT-LOCAL-ASSIGN_INT");

	static bool IndependentSupports(const TCHAR* Operation, const TCHAR* Type)
	{
		const bool bBool = FCString::Strcmp(Type, TEXT("BOOL")) == 0;
		const bool bFloat = FCString::Strcmp(Type, TEXT("FLOAT32")) == 0
			|| FCString::Strcmp(Type, TEXT("FLOAT64")) == 0;
		if (FCString::Strcmp(Operation, TEXT("ASSIGN")) == 0)
		{
			return true;
		}
		if (FCString::Strcmp(Operation, TEXT("ADD_ASSIGN")) == 0
			|| FCString::Strcmp(Operation, TEXT("SUBTRACT_ASSIGN")) == 0
			|| FCString::Strcmp(Operation, TEXT("MULTIPLY_ASSIGN")) == 0
			|| FCString::Strcmp(Operation, TEXT("DIVIDE_ASSIGN")) == 0
			|| FCString::Strcmp(Operation, TEXT("MODULO_ASSIGN")) == 0)
		{
			return !bBool;
		}
		if (FCString::Strcmp(Operation, TEXT("POWER_ASSIGN")) == 0)
		{
			return bFloat;
		}
		return !bBool && !bFloat;
	}

	static FString IndependentOperatorNeedle(const TCHAR* Operation)
	{
		if (FCString::Strcmp(Operation, TEXT("ADD_ASSIGN")) == 0)
		{
			return TEXT("+= Source; // ASSIGNMENT_CAUSE");
		}
		if (FCString::Strcmp(Operation, TEXT("SUBTRACT_ASSIGN")) == 0)
		{
			return TEXT("-= Source; // ASSIGNMENT_CAUSE");
		}
		if (FCString::Strcmp(Operation, TEXT("MULTIPLY_ASSIGN")) == 0)
		{
			return TEXT("*= Source; // ASSIGNMENT_CAUSE");
		}
		if (FCString::Strcmp(Operation, TEXT("DIVIDE_ASSIGN")) == 0)
		{
			return TEXT("/= Source; // ASSIGNMENT_CAUSE");
		}
		if (FCString::Strcmp(Operation, TEXT("MODULO_ASSIGN")) == 0)
		{
			return TEXT("%= Source; // ASSIGNMENT_CAUSE");
		}
		if (FCString::Strcmp(Operation, TEXT("POWER_ASSIGN")) == 0)
		{
			return TEXT("**= Source; // ASSIGNMENT_CAUSE");
		}
		if (FCString::Strcmp(Operation, TEXT("AND_ASSIGN")) == 0)
		{
			return TEXT("&= Source; // ASSIGNMENT_CAUSE");
		}
		if (FCString::Strcmp(Operation, TEXT("OR_ASSIGN")) == 0)
		{
			return TEXT("|= Source; // ASSIGNMENT_CAUSE");
		}
		if (FCString::Strcmp(Operation, TEXT("XOR_ASSIGN")) == 0)
		{
			return TEXT("^= Source; // ASSIGNMENT_CAUSE");
		}
		if (FCString::Strcmp(Operation, TEXT("SHIFT_LEFT_ASSIGN")) == 0)
		{
			return TEXT("<<= Source; // ASSIGNMENT_CAUSE");
		}
		if (FCString::Strcmp(Operation, TEXT("SHIFT_RIGHT_LOGICAL_ASSIGN")) == 0)
		{
			return TEXT(">>= Source; // ASSIGNMENT_CAUSE");
		}
		return TEXT(">>>= Source; // ASSIGNMENT_CAUSE");
	}

	static FString IndependentLiteralNeedle(const TCHAR* Type)
	{
		if (FCString::Strcmp(Type, TEXT("BOOL")) == 0)
		{
			return TEXT("bool Input = false;");
		}
		if (FCString::Strcmp(Type, TEXT("FLOAT32")) == 0)
		{
			return TEXT("float32 Input = 4.0f;");
		}
		if (FCString::Strcmp(Type, TEXT("FLOAT64")) == 0)
		{
			return TEXT("double Input = 4.0;");
		}
		if (FCString::Strcmp(Type, TEXT("INT8")) == 0)
		{
			return TEXT("int8 Input = 4;");
		}
		if (FCString::Strcmp(Type, TEXT("INT16")) == 0)
		{
			return TEXT("int16 Input = 4;");
		}
		if (FCString::Strcmp(Type, TEXT("INT64")) == 0)
		{
			return TEXT("int64 Input = 4;");
		}
		if (FCString::Strcmp(Type, TEXT("UINT8")) == 0)
		{
			return TEXT("uint8 Input = 4;");
		}
		if (FCString::Strcmp(Type, TEXT("UINT16")) == 0)
		{
			return TEXT("uint16 Input = 4;");
		}
		if (FCString::Strcmp(Type, TEXT("UINT")) == 0)
		{
			return TEXT("uint Input = 4;");
		}
		if (FCString::Strcmp(Type, TEXT("UINT64")) == 0)
		{
			return TEXT("uint64 Input = 4;");
		}
		return TEXT("int Input = 4;");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kCategories[] = {
			TEXT("LOCAL"), TEXT("FIELD"), TEXT("PROPERTY"), TEXT("ALIAS")};
		static const TCHAR* const kOperations[] = {
			TEXT("ASSIGN"),
			TEXT("ADD_ASSIGN"),
			TEXT("SUBTRACT_ASSIGN"),
			TEXT("MULTIPLY_ASSIGN"),
			TEXT("DIVIDE_ASSIGN"),
			TEXT("MODULO_ASSIGN"),
			TEXT("POWER_ASSIGN"),
			TEXT("AND_ASSIGN"),
			TEXT("OR_ASSIGN"),
			TEXT("XOR_ASSIGN"),
			TEXT("SHIFT_LEFT_ASSIGN"),
			TEXT("SHIFT_RIGHT_LOGICAL_ASSIGN"),
			TEXT("SHIFT_RIGHT_ARITHMETIC_ASSIGN")};
		static const TCHAR* const kTypes[] = {
			TEXT("INT8"),
			TEXT("INT16"),
			TEXT("INT"),
			TEXT("INT64"),
			TEXT("UINT8"),
			TEXT("UINT16"),
			TEXT("UINT"),
			TEXT("UINT64"),
			TEXT("FLOAT32"),
			TEXT("FLOAT64"),
			TEXT("BOOL")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(128);
		int32 Write = 0;
		for (int32 CategoryIndex = 0; CategoryIndex < 4; ++CategoryIndex)
		{
			for (int32 OperationIndex = 0; OperationIndex < 13; ++OperationIndex)
			{
				for (int32 TypeIndex = 0; TypeIndex < 11; ++TypeIndex)
				{
					const TCHAR* const Operation = kOperations[OperationIndex];
					const TCHAR* const Type = kTypes[TypeIndex];
					if (IndependentSupports(Operation, Type))
					{
						continue;
					}
					const TCHAR* const Category = kCategories[CategoryIndex];
					FExpectedCell& Cell = Cells[Write];
					Cell.CaseId = FString::Printf(
						TEXT("LANG-OP-ASSIGNMENT-TYPE-REJECTION-%s-%s_%s"), Category, Operation, Type);
					Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
					Cell.OperatorNeedle = IndependentOperatorNeedle(Operation);
					if (FCString::Strcmp(Category, TEXT("FIELD")) == 0)
					{
						Cell.CategoryNeedle = TEXT("FRejectedAssignmentField");
					}
					else if (FCString::Strcmp(Category, TEXT("PROPERTY")) == 0)
					{
						Cell.CategoryNeedle = TEXT("MakeAssignmentProperty_");
					}
					else
					{
						Cell.CategoryNeedle = TEXT("Value = Input;");
					}
					Cell.LiteralNeedle = IndependentLiteralNeedle(Type);
					++Write;
				}
			}
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EOpAssignmentTypeRejectionCategory;
		using AngelscriptTest::Generate::EOpAssignmentTypeRejectionOperation;
		using AngelscriptTest::Generate::EOpAssignmentTypeRejectionType;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FOpAssignmentTypeRejectionGenerator;
		using AngelscriptTest::Generate::FOpAssignmentTypeRejectionParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FOpAssignmentTypeRejectionGenerator Generator;
		ASSERT_THAT(AreEqual(
			FString(TEXT("LANG-OP-ASSIGNMENT-TYPE-REJECTION")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangOpAssignmentTypeRejectionLocalAddAssignBool")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile = AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(
			TEXT("OpAssignmentTypeRejectionGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(
			LogTemp,
			Display,
			TEXT("OpAssignmentTypeRejectionGenerator dump path=%s cases=128 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(0, CaseCount));
		ASSERT_THAT(AreEqual(128, Cases.Num()));
		ASSERT_THAT(AreEqual(128, ExpectedCells.Num()));
		ASSERT_THAT(AreEqual(128, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.IsEmpty()));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/OpAssignmentTypeRejectionGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 RejectCount = 0;
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
			ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.OperatorNeedle)));
			ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.CategoryNeedle)));
			ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.LiteralNeedle)));
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(RejectSource, Expected.EntryName, GeneratedBody)));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
			++RejectCount;
		}
		ASSERT_THAT(AreEqual(128, RejectCount));

		FOpAssignmentTypeRejectionParams DefaultParams;
		const FString DefaultSingle = Generator.BuildTypeRejectionSource(DefaultParams);
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Entry()"))));
		FOpAssignmentTypeRejectionParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildTypeRejectionSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildTypeRejectionSource(ProbeParams).IsEmpty()));
		FOpAssignmentTypeRejectionParams InvalidEnum = DefaultParams;
		InvalidEnum.Category = static_cast<EOpAssignmentTypeRejectionCategory>(4);
		ASSERT_THAT(IsTrue(Generator.BuildTypeRejectionSource(InvalidEnum).IsEmpty()));
		FOpAssignmentTypeRejectionParams LegalPair = DefaultParams;
		LegalPair.Operation = EOpAssignmentTypeRejectionOperation::Assign;
		LegalPair.Type = EOpAssignmentTypeRejectionType::Int;
		ASSERT_THAT(IsTrue(Generator.BuildTypeRejectionSource(LegalPair).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(ProbeCaseId)));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(128, CountOccurrences(Dump, TEXT("// Case: LANG-OP-ASSIGNMENT-TYPE-REJECTION-"))));
		ASSERT_THAT(AreEqual(128, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/OpAssignmentTypeRejectionGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("OpAssignmentTypeRejectionGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
