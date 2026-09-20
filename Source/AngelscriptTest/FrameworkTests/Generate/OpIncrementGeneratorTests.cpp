#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestOpIncrementGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	OpIncrementGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString TypeName;
		FString OperatorNeedle;
		int32 Expected = 0;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-OP-INCREMENT-LOCAL-BEFORE-PRE_INCREMENT-INT");
	static constexpr const TCHAR* ForeignCaseId =
		TEXT("LANG-OP-FAILURE-INVALID_SHIFT_COUNT-DIAGNOSTIC_OR_EXCEPTION-FRESH_MODULE");

	static bool IndependentIncrement(const TCHAR* Operator)
	{
		return FCString::Strcmp(Operator, TEXT("PRE_INCREMENT")) == 0
			|| FCString::Strcmp(Operator, TEXT("POST_INCREMENT")) == 0;
	}

	static bool IndependentPrefix(const TCHAR* Operator)
	{
		return FCString::Strcmp(Operator, TEXT("PRE_INCREMENT")) == 0
			|| FCString::Strcmp(Operator, TEXT("PRE_DECREMENT")) == 0;
	}

	static int32 IndependentExpected(const TCHAR* Observation, const TCHAR* Operator)
	{
		const int32 Initial = 12;
		const int32 Final = IndependentIncrement(Operator) ? 13 : 11;
		if (FCString::Strcmp(Observation, TEXT("BEFORE")) == 0)
		{
			return Initial;
		}
		if (FCString::Strcmp(Observation, TEXT("EXPRESSION_RESULT")) == 0)
		{
			return IndependentPrefix(Operator) ? Final : Initial;
		}
		return Final;
	}

	static FString IndependentType(const TCHAR* Type)
	{
		if (FCString::Strcmp(Type, TEXT("INT8")) == 0)
		{
			return TEXT("int8");
		}
		if (FCString::Strcmp(Type, TEXT("INT16")) == 0)
		{
			return TEXT("int16");
		}
		if (FCString::Strcmp(Type, TEXT("INT64")) == 0)
		{
			return TEXT("int64");
		}
		if (FCString::Strcmp(Type, TEXT("UINT8")) == 0)
		{
			return TEXT("uint8");
		}
		if (FCString::Strcmp(Type, TEXT("UINT16")) == 0)
		{
			return TEXT("uint16");
		}
		if (FCString::Strcmp(Type, TEXT("UINT")) == 0)
		{
			return TEXT("uint");
		}
		if (FCString::Strcmp(Type, TEXT("UINT64")) == 0)
		{
			return TEXT("uint64");
		}
		if (FCString::Strcmp(Type, TEXT("FLOAT32")) == 0)
		{
			return TEXT("float32");
		}
		if (FCString::Strcmp(Type, TEXT("FLOAT64")) == 0)
		{
			return TEXT("double");
		}
		return TEXT("int");
	}

	static FString IndependentOperatorNeedle(const TCHAR* Category, const TCHAR* Operator)
	{
		const bool bPrefix = IndependentPrefix(Operator);
		const bool bIncrement = IndependentIncrement(Operator);
		const TCHAR* const Token = bIncrement ? TEXT("++") : TEXT("--");
		if (FCString::Strcmp(Category, TEXT("FIELD")) == 0)
		{
			return bPrefix ? FString::Printf(TEXT("%sOwner.Value"), Token) : FString::Printf(TEXT("Owner.Value%s"), Token);
		}
		if (FCString::Strcmp(Category, TEXT("PROPERTY")) == 0)
		{
			return bPrefix ? FString::Printf(TEXT("%sUpdated"), Token) : FString::Printf(TEXT("Updated%s"), Token);
		}
		if (FCString::Strcmp(Category, TEXT("ALIAS")) == 0)
		{
			if (FCString::Strcmp(Operator, TEXT("PRE_INCREMENT")) == 0)
			{
				return TEXT("ApplyPreIncrementAlias_");
			}
			if (FCString::Strcmp(Operator, TEXT("POST_INCREMENT")) == 0)
			{
				return TEXT("ApplyPostIncrementAlias_");
			}
			if (FCString::Strcmp(Operator, TEXT("PRE_DECREMENT")) == 0)
			{
				return TEXT("ApplyPreDecrementAlias_");
			}
			return TEXT("ApplyPostDecrementAlias_");
		}
		return bPrefix ? FString::Printf(TEXT("%sValue"), Token) : FString::Printf(TEXT("Value%s"), Token);
	}

	static bool HasExtractableEntry(const FString& Source, const FString& EntryName)
	{
		const FString Header = FString::Printf(TEXT("int %s()"), *EntryName);
		return Source.StartsWith(Header) || Source.Contains(TEXT("\n") + Header);
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kCategories[] = {TEXT("LOCAL"), TEXT("FIELD"), TEXT("PROPERTY"), TEXT("ALIAS")};
		static const TCHAR* const kObservations[] = {TEXT("BEFORE"), TEXT("EXPRESSION_RESULT"), TEXT("AFTER")};
		static const TCHAR* const kOperators[] = {
			TEXT("PRE_INCREMENT"), TEXT("POST_INCREMENT"), TEXT("PRE_DECREMENT"), TEXT("POST_DECREMENT")};
		static const TCHAR* const kTypes[] = {
			TEXT("INT8"), TEXT("INT16"), TEXT("INT"), TEXT("INT64"), TEXT("UINT8"),
			TEXT("UINT16"), TEXT("UINT"), TEXT("UINT64"), TEXT("FLOAT32"), TEXT("FLOAT64")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(480);
		for (int32 Index = 0; Index < 480; ++Index)
		{
			const TCHAR* const Category = kCategories[Index / 120];
			const TCHAR* const Observation = kObservations[(Index / 40) % 3];
			const TCHAR* const Operator = kOperators[(Index / 10) % 4];
			const TCHAR* const Type = kTypes[Index % 10];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-OP-INCREMENT-%s-%s-%s-%s"), Category, Observation, Operator, Type);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.TypeName = IndependentType(Type);
			Cell.OperatorNeedle = IndependentOperatorNeedle(Category, Operator);
			Cell.Expected = IndependentExpected(Observation, Operator);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EOpIncrementCategory;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FOpIncrementGenerator;
		using AngelscriptTest::Generate::FOpIncrementParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FOpIncrementGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-OP-INCREMENT")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangOpIncrementLocalBeforePreIncrementInt")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("OpIncrementGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("OpIncrementGenerator dump path=%s cases=480 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(480, CaseCount));
		ASSERT_THAT(AreEqual(480, Cases.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
		}

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangOpIncrementLocalBeforePreIncrementInt()"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("struct FIncrementFieldOwner_int\n{"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int ApplyPreIncrementAlias_int(int& inout Value)"))));

		FOpIncrementParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildLegalSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FOpIncrementParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildLegalSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/OpIncrementGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(HasExtractableEntry(Aggregate, Expected.EntryName)));
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.TypeName + TEXT(" Input ="))));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.OperatorNeedle)));
			if (HasExtractableEntry(Representative, Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FOpIncrementParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildLegalSource(InvalidName).IsEmpty()));
		FOpIncrementParams InvalidEnum = DefaultParams;
		InvalidEnum.Category = static_cast<EOpIncrementCategory>(4);
		ASSERT_THAT(IsTrue(Generator.BuildLegalSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
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
		ASSERT_THAT(AreEqual(480, CountOccurrences(Dump, TEXT("// Case: LANG-OP-INCREMENT-"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/OpIncrementGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("OpIncrementGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
