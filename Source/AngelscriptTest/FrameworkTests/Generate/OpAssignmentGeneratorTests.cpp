#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestOpAssignmentGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	OpAssignmentGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString OperatorNeedle;
		FString LiteralNeedle;
		FString CategoryNeedle;
		int32 Expected = 0;
		bool bLimited = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-OP-ASSIGNMENT-LOCAL-ASSIGN_INT");
	static constexpr const TCHAR* ZeroCaseId = TEXT("LANG-OP-ASSIGNMENT-LOCAL-MODULO_ASSIGN_INT");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

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

	static int32 IndependentExpected(const TCHAR* Operation, const TCHAR* Type)
	{
		if (FCString::Strcmp(Type, TEXT("BOOL")) == 0)
		{
			return 1;
		}
		if (FCString::Strcmp(Operation, TEXT("ASSIGN")) == 0
			|| FCString::Strcmp(Operation, TEXT("SUBTRACT_ASSIGN")) == 0
			|| FCString::Strcmp(Operation, TEXT("DIVIDE_ASSIGN")) == 0)
		{
			return 2;
		}
		if (FCString::Strcmp(Operation, TEXT("ADD_ASSIGN")) == 0
			|| FCString::Strcmp(Operation, TEXT("OR_ASSIGN")) == 0
			|| FCString::Strcmp(Operation, TEXT("XOR_ASSIGN")) == 0)
		{
			return 6;
		}
		if (FCString::Strcmp(Operation, TEXT("MULTIPLY_ASSIGN")) == 0)
		{
			return 8;
		}
		if (FCString::Strcmp(Operation, TEXT("MODULO_ASSIGN")) == 0
			|| FCString::Strcmp(Operation, TEXT("AND_ASSIGN")) == 0)
		{
			return 0;
		}
		if (FCString::Strcmp(Operation, TEXT("POWER_ASSIGN")) == 0
			|| FCString::Strcmp(Operation, TEXT("SHIFT_LEFT_ASSIGN")) == 0)
		{
			return 16;
		}
		return 1;
	}

	static FString IndependentOperatorNeedle(const TCHAR* Operation)
	{
		if (FCString::Strcmp(Operation, TEXT("ASSIGN")) == 0)
		{
			return TEXT("= Source");
		}
		if (FCString::Strcmp(Operation, TEXT("ADD_ASSIGN")) == 0)
		{
			return TEXT("+= Source");
		}
		if (FCString::Strcmp(Operation, TEXT("SUBTRACT_ASSIGN")) == 0)
		{
			return TEXT("-= Source");
		}
		if (FCString::Strcmp(Operation, TEXT("MULTIPLY_ASSIGN")) == 0)
		{
			return TEXT("*= Source");
		}
		if (FCString::Strcmp(Operation, TEXT("DIVIDE_ASSIGN")) == 0)
		{
			return TEXT("/= Source");
		}
		if (FCString::Strcmp(Operation, TEXT("MODULO_ASSIGN")) == 0)
		{
			return TEXT("%= Source");
		}
		if (FCString::Strcmp(Operation, TEXT("POWER_ASSIGN")) == 0)
		{
			return TEXT("**= Source");
		}
		if (FCString::Strcmp(Operation, TEXT("AND_ASSIGN")) == 0)
		{
			return TEXT("&= Source");
		}
		if (FCString::Strcmp(Operation, TEXT("OR_ASSIGN")) == 0)
		{
			return TEXT("|= Source");
		}
		if (FCString::Strcmp(Operation, TEXT("XOR_ASSIGN")) == 0)
		{
			return TEXT("^= Source");
		}
		if (FCString::Strcmp(Operation, TEXT("SHIFT_LEFT_ASSIGN")) == 0)
		{
			return TEXT("<<= Source");
		}
		if (FCString::Strcmp(Operation, TEXT("SHIFT_RIGHT_LOGICAL_ASSIGN")) == 0)
		{
			return TEXT(">>= Source");
		}
		return TEXT(">>>= Source");
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

	static FString IndependentAliasSuffix(const TCHAR* Operation)
	{
		if (FCString::Strcmp(Operation, TEXT("ASSIGN")) == 0)
		{
			return TEXT("Assign");
		}
		if (FCString::Strcmp(Operation, TEXT("ADD_ASSIGN")) == 0)
		{
			return TEXT("AddAssign");
		}
		if (FCString::Strcmp(Operation, TEXT("SUBTRACT_ASSIGN")) == 0)
		{
			return TEXT("SubtractAssign");
		}
		if (FCString::Strcmp(Operation, TEXT("MULTIPLY_ASSIGN")) == 0)
		{
			return TEXT("MultiplyAssign");
		}
		if (FCString::Strcmp(Operation, TEXT("DIVIDE_ASSIGN")) == 0)
		{
			return TEXT("DivideAssign");
		}
		if (FCString::Strcmp(Operation, TEXT("MODULO_ASSIGN")) == 0)
		{
			return TEXT("ModuloAssign");
		}
		if (FCString::Strcmp(Operation, TEXT("POWER_ASSIGN")) == 0)
		{
			return TEXT("PowerAssign");
		}
		if (FCString::Strcmp(Operation, TEXT("AND_ASSIGN")) == 0)
		{
			return TEXT("AndAssign");
		}
		if (FCString::Strcmp(Operation, TEXT("OR_ASSIGN")) == 0)
		{
			return TEXT("OrAssign");
		}
		if (FCString::Strcmp(Operation, TEXT("XOR_ASSIGN")) == 0)
		{
			return TEXT("XorAssign");
		}
		if (FCString::Strcmp(Operation, TEXT("SHIFT_LEFT_ASSIGN")) == 0)
		{
			return TEXT("ShiftLeftAssign");
		}
		if (FCString::Strcmp(Operation, TEXT("SHIFT_RIGHT_LOGICAL_ASSIGN")) == 0)
		{
			return TEXT("ShiftRightLogicalAssign");
		}
		return TEXT("ShiftRightArithmeticAssign");
	}

	static FString IndependentCategoryNeedle(const TCHAR* Category, const TCHAR* Operation, const TCHAR* Type)
	{
		if (FCString::Strcmp(Category, TEXT("FIELD")) == 0)
		{
			return TEXT("Owner.Value");
		}
		if (FCString::Strcmp(Category, TEXT("PROPERTY")) == 0)
		{
			return TEXT("MakeAssignmentProperty_");
		}
		if (FCString::Strcmp(Category, TEXT("ALIAS")) == 0)
		{
			return FString::Printf(TEXT("Apply%sAlias_"), *IndependentAliasSuffix(Operation));
		}
		(void)Type;
		return TEXT("Value = Input;");
	}

	static bool HasExtractableEntry(const FString& Source, const FString& EntryName)
	{
		const FString Header = FString::Printf(TEXT("int %s()"), *EntryName);
		return Source.StartsWith(Header) || Source.Contains(TEXT("\n") + Header);
	}

	static bool IndependentLimited(const TCHAR* Type)
	{
		return FCString::Strcmp(Type, TEXT("INT64")) == 0
			|| FCString::Strcmp(Type, TEXT("UINT64")) == 0
			|| FCString::Strcmp(Type, TEXT("FLOAT32")) == 0
			|| FCString::Strcmp(Type, TEXT("FLOAT64")) == 0;
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
		Cells.SetNum(444);
		int32 Write = 0;
		for (int32 CategoryIndex = 0; CategoryIndex < 4; ++CategoryIndex)
		{
			for (int32 OperationIndex = 0; OperationIndex < 13; ++OperationIndex)
			{
				for (int32 TypeIndex = 0; TypeIndex < 11; ++TypeIndex)
				{
					const TCHAR* const Operation = kOperations[OperationIndex];
					const TCHAR* const Type = kTypes[TypeIndex];
					if (!IndependentSupports(Operation, Type))
					{
						continue;
					}
					const TCHAR* const Category = kCategories[CategoryIndex];
					FExpectedCell& Cell = Cells[Write];
					Cell.CaseId = FString::Printf(TEXT("LANG-OP-ASSIGNMENT-%s-%s_%s"), Category, Operation, Type);
					Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
					Cell.OperatorNeedle = IndependentOperatorNeedle(Operation);
					Cell.LiteralNeedle = IndependentLiteralNeedle(Type);
					Cell.CategoryNeedle = IndependentCategoryNeedle(Category, Operation, Type);
					Cell.Expected = IndependentExpected(Operation, Type);
					Cell.bLimited = IndependentLimited(Type);
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
		using AngelscriptTest::Generate::EOpAssignmentCategory;
		using AngelscriptTest::Generate::EOpAssignmentOperation;
		using AngelscriptTest::Generate::EOpAssignmentType;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FOpAssignmentGenerator;
		using AngelscriptTest::Generate::FOpAssignmentParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FOpAssignmentGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-OP-ASSIGNMENT")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangOpAssignmentLocalAssignInt")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("OpAssignmentGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("OpAssignmentGenerator dump path=%s cases=444 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(444, CaseCount));
		ASSERT_THAT(AreEqual(444, Cases.Num()));
		ASSERT_THAT(AreEqual(444, ExpectedCells.Num()));

		int32 RejectCount = 0;
		int32 ZeroCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedException.IsEmpty()));
			ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("ObserveAssignment_"))));
			ASSERT_THAT(AreEqual(Expected.bLimited, Cases[Index].bLimitedObservation));
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
		ASSERT_THAT(IsTrue(ZeroCount > 0));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		const FString OwnedAgain = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(IsTrue(OwnedCase.Len() > 0));
		ASSERT_THAT(AreEqual(OwnedCase, OwnedAgain));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangOpAssignmentLocalAssignInt()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangOpAssignmentLocalAssignInt()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("struct FAssignmentFieldOwner_int\n{"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int ApplyAssignAlias_int(int& inout Value, int Source)"))));

		FOpAssignmentParams DefaultParams;
		const FString DefaultSingle = Generator.BuildLegalSource(DefaultParams);
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsFalse(DefaultSingle.Contains(TEXT("int EntryLang"))));

		FOpAssignmentParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		const FString ProbeSource = Generator.BuildLegalSource(ProbeParams);
		ASSERT_THAT(IsTrue(ProbeSource.Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(ProbeSource.Contains(TEXT("int Entry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/OpAssignmentGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			const FString CaseSource = Generator.BuildCaseSource(Expected.CaseId);
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.LiteralNeedle)));
			ASSERT_THAT(IsTrue(CaseSource.Contains(Expected.OperatorNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.CategoryNeedle)));
			if (Expected.CaseId.Contains(TEXT("BOOL")))
			{
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("return Result ? 1 : 0;"))));
			}
			else
			{
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("return int(Result);"))));
			}
			if (HasExtractableEntry(Representative, Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FOpAssignmentParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildLegalSource(InvalidName).IsEmpty()));
		FOpAssignmentParams InvalidEnum = DefaultParams;
		InvalidEnum.Category = static_cast<EOpAssignmentCategory>(4);
		ASSERT_THAT(IsTrue(Generator.BuildLegalSource(InvalidEnum).IsEmpty()));
		FOpAssignmentParams RejectType = DefaultParams;
		RejectType.Operation = EOpAssignmentOperation::AddAssign;
		RejectType.Type = EOpAssignmentType::Bool;
		ASSERT_THAT(IsTrue(Generator.BuildLegalSource(RejectType).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
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

		ASSERT_THAT(IsTrue(OutputFile.EndsWith(TEXT("GeneratedCases/OpAssignmentGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\OpAssignmentGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(444, CountOccurrences(Dump, TEXT("// Case: LANG-OP-ASSIGNMENT-"))));
		for (const FExpectedCell& Expected : ExpectedCells)
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
			TEXT("C:/no-such-root/OpAssignmentGenerator.as"), Dump, ProbeError)));
		ASSERT_THAT(IsTrue(ProbeError.Len() > 0));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("OpAssignmentGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
