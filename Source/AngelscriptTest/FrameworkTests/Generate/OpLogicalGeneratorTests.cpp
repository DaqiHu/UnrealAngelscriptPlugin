#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestOpLogicalGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	OpLogicalGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString Glyph;
		FString ContextNeedle;
		FString SourceNeedle;
		int32 Expected = 0;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-OP-LOGICAL-ASSIGNMENT-AND-BOOL_LITERAL-FALSE_FALSE");
	static constexpr const TCHAR* ZeroCaseId =
		TEXT("LANG-OP-LOGICAL-ASSIGNMENT-AND-BOOL_LITERAL-FALSE_FALSE");
	static constexpr const TCHAR* ForeignCaseId =
		TEXT("LANG-OP-LOGICAL-NOT-MUTABLE_LVALUE-FALSE");

	static bool IndependentLeft(const TCHAR* Truth)
	{
		return FCString::Strcmp(Truth, TEXT("TRUE_FALSE")) == 0
			|| FCString::Strcmp(Truth, TEXT("TRUE_TRUE")) == 0;
	}

	static bool IndependentRight(const TCHAR* Truth)
	{
		return FCString::Strcmp(Truth, TEXT("FALSE_TRUE")) == 0
			|| FCString::Strcmp(Truth, TEXT("TRUE_TRUE")) == 0;
	}

	static int32 IndependentExpected(const TCHAR* Operator, const TCHAR* Truth)
	{
		const bool bLeft = IndependentLeft(Truth);
		const bool bRight = IndependentRight(Truth);
		if (FCString::Strcmp(Operator, TEXT("AND")) == 0)
		{
			return (bLeft && bRight) ? 1 : 0;
		}
		if (FCString::Strcmp(Operator, TEXT("OR")) == 0)
		{
			return (bLeft || bRight) ? 1 : 0;
		}
		return (bLeft != bRight) ? 1 : 0;
	}

	static FString IndependentGlyph(const TCHAR* Operator)
	{
		if (FCString::Strcmp(Operator, TEXT("OR")) == 0)
		{
			return TEXT("||");
		}
		if (FCString::Strcmp(Operator, TEXT("XOR")) == 0)
		{
			return TEXT("^^");
		}
		return TEXT("&&");
	}

	static FString IndependentContextNeedle(const TCHAR* Context)
	{
		if (FCString::Strcmp(Context, TEXT("RETURN")) == 0)
		{
			return TEXT("return (");
		}
		if (FCString::Strcmp(Context, TEXT("CONDITION")) == 0)
		{
			return TEXT("if (");
		}
		if (FCString::Strcmp(Context, TEXT("ARGUMENT")) == 0)
		{
			return TEXT("ConsumeLogical(");
		}
		return TEXT("bool Result = ");
	}

	static FString IndependentSourceNeedle(const TCHAR* Source)
	{
		if (FCString::Strcmp(Source, TEXT("BOOL_LVALUE")) == 0)
		{
			return TEXT("bool LeftValue = ");
		}
		if (FCString::Strcmp(Source, TEXT("COMPARISON")) == 0)
		{
			return TEXT("int LeftNumber = ");
		}
		if (FCString::Strcmp(Source, TEXT("CONVERSION_OPERATOR")) == 0)
		{
			return TEXT("FLogicalConversionValue LeftValue(");
		}
		return TEXT("TraceLogicalOperand(1, ");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kContexts[] = {
			TEXT("ASSIGNMENT"), TEXT("RETURN"), TEXT("CONDITION"), TEXT("ARGUMENT")};
		static const TCHAR* const kOperators[] = {TEXT("AND"), TEXT("OR"), TEXT("XOR")};
		static const TCHAR* const kSources[] = {
			TEXT("BOOL_LITERAL"), TEXT("BOOL_LVALUE"), TEXT("COMPARISON"), TEXT("CONVERSION_OPERATOR")};
		static const TCHAR* const kTruths[] = {
			TEXT("FALSE_FALSE"), TEXT("FALSE_TRUE"), TEXT("TRUE_FALSE"), TEXT("TRUE_TRUE")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(192);
		for (int32 Index = 0; Index < 192; ++Index)
		{
			const TCHAR* const Context = kContexts[Index / 48];
			const TCHAR* const Operator = kOperators[(Index / 16) % 3];
			const TCHAR* const Source = kSources[(Index / 4) % 4];
			const TCHAR* const Truth = kTruths[Index % 4];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-OP-LOGICAL-%s-%s-%s-%s"), Context, Operator, Source, Truth);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Glyph = IndependentGlyph(Operator);
			Cell.ContextNeedle = IndependentContextNeedle(Context);
			Cell.SourceNeedle = IndependentSourceNeedle(Source);
			Cell.Expected = IndependentExpected(Operator, Truth);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EOpLogicalContext;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FOpLogicalGenerator;
		using AngelscriptTest::Generate::FOpLogicalParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FOpLogicalGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-OP-LOGICAL")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangOpLogicalAssignmentAndBoolLiteralFalseFalse")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("OpLogicalGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("OpLogicalGenerator dump path=%s cases=192 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(192, CaseCount));
		ASSERT_THAT(AreEqual(192, Cases.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

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
			if (Expected.Expected == 0)
			{
				++ZeroCount;
			}
		}
		ASSERT_THAT(IsTrue(ZeroCount > 0));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("struct FLogicalConversionValue"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("bool ConsumeLogical(bool Value)"))));

		FOpLogicalParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildLogicalSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FOpLogicalParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildLogicalSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/OpLogicalGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Glyph)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.ContextNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.SourceNeedle)));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FOpLogicalParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildLogicalSource(InvalidName).IsEmpty()));
		FOpLogicalParams InvalidEnum = DefaultParams;
		InvalidEnum.Context = static_cast<EOpLogicalContext>(4);
		ASSERT_THAT(IsTrue(Generator.BuildLogicalSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(ZeroCaseId)));
		bool bFoundZero = false;
		for (int32 Index = 0; Index < Cases.Num(); ++Index)
		{
			if (Cases[Index].CaseId == ZeroCaseId)
			{
				bFoundZero = true;
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			}
		}
		ASSERT_THAT(IsTrue(bFoundZero));

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
		ASSERT_THAT(AreEqual(192, CountOccurrences(Dump, TEXT("// Case: LANG-OP-LOGICAL-"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/OpLogicalGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("OpLogicalGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
