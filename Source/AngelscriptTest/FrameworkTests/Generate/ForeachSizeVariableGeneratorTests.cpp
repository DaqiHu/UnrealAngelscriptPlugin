#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestForeachSizeVariableGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ForeachSizeVariableGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		int32 Expected = 0;
		bool bReject = false;
		bool bFault = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-FE-SIZE-VARIABLE-EMPTY-PRIMITIVE-VALUE-COMPLETE");
	static constexpr const TCHAR* ZeroCaseId = TEXT("LANG-FE-SIZE-VARIABLE-EMPTY-PRIMITIVE-VALUE-COMPLETE");

	static int32 IndependentCount(const TCHAR* Size)
	{
		if (FCString::Strcmp(Size, TEXT("EMPTY")) == 0)
		{
			return 0;
		}
		if (FCString::Strcmp(Size, TEXT("ONE")) == 0)
		{
			return 1;
		}
		if (FCString::Strcmp(Size, TEXT("TWO")) == 0)
		{
			return 2;
		}
		return 4;
	}

	static bool IndependentReject(const TCHAR* Element, const TCHAR* Variable)
	{
		return FCString::Strcmp(Variable, TEXT("INCOMPATIBLE")) == 0
			|| FCString::Strcmp(Element, TEXT("REFERENCE_OBJECT")) == 0
			|| (FCString::Strcmp(Element, TEXT("CONST_ELEMENT")) == 0
				&& FCString::Strcmp(Variable, TEXT("MUTABLE_REFERENCE")) == 0);
	}

	static int32 IndependentExpected(const TCHAR* Size, const TCHAR* Variable, const TCHAR* Transfer)
	{
		const int32 Count = IndependentCount(Size);
		const int32 Contribution = FCString::Strcmp(Variable, TEXT("MUTABLE_REFERENCE")) == 0 ? 2 : 1;
		if (Count == 0)
		{
			return 0;
		}
		if (FCString::Strcmp(Transfer, TEXT("EXCEPTION")) == 0)
		{
			return 0;
		}
		if (FCString::Strcmp(Transfer, TEXT("RETURN")) == 0)
		{
			return Contribution;
		}
		if (FCString::Strcmp(Transfer, TEXT("BREAK_FIRST")) == 0)
		{
			return 0;
		}
		if (FCString::Strcmp(Transfer, TEXT("BREAK_MIDDLE")) == 0)
		{
			return Contribution;
		}
		if (FCString::Strcmp(Transfer, TEXT("BREAK_LAST")) == 0
			|| FCString::Strcmp(Transfer, TEXT("CONTINUE_FIRST")) == 0)
		{
			return (Count - 1) * Contribution;
		}
		if (FCString::Strcmp(Transfer, TEXT("CONTINUE_MIDDLE")) == 0)
		{
			return (Count == 1 ? 1 : Count - 1) * Contribution;
		}
		return Count * Contribution;
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kSizes[] = {TEXT("EMPTY"), TEXT("ONE"), TEXT("TWO"), TEXT("MANY")};
		static const TCHAR* const kElements[] = {
			TEXT("PRIMITIVE"), TEXT("VALUE_OBJECT"), TEXT("REFERENCE_OBJECT"), TEXT("CONST_ELEMENT")};
		static const TCHAR* const kVariables[] = {
			TEXT("VALUE"), TEXT("AUTO"), TEXT("MUTABLE_REFERENCE"), TEXT("CONST_REFERENCE"), TEXT("INCOMPATIBLE")};
		static const TCHAR* const kTransfers[] = {
			TEXT("COMPLETE"), TEXT("BREAK_FIRST"), TEXT("BREAK_MIDDLE"), TEXT("BREAK_LAST"),
			TEXT("CONTINUE_FIRST"), TEXT("CONTINUE_MIDDLE"), TEXT("RETURN"), TEXT("EXCEPTION")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(640);
		for (int32 Index = 0; Index < 640; ++Index)
		{
			const TCHAR* const Size = kSizes[Index / 160];
			const TCHAR* const Element = kElements[(Index / 40) % 4];
			const TCHAR* const Variable = kVariables[(Index / 8) % 5];
			const TCHAR* const Transfer = kTransfers[Index % 8];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-FE-SIZE-VARIABLE-%s-%s-%s-%s"), Size, Element, Variable, Transfer);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Element, Variable);
			Cell.bFault = !Cell.bReject
				&& FCString::Strcmp(Transfer, TEXT("EXCEPTION")) == 0
				&& IndependentCount(Size) > 0;
			Cell.Expected = Cell.bReject ? 0 : IndependentExpected(Size, Variable, Transfer);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EForeachElement;
		using AngelscriptTest::Generate::EForeachVariable;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FForeachSizeVariableGenerator;
		using AngelscriptTest::Generate::FForeachSizeVariableParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FForeachSizeVariableGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-FE-SIZE-VARIABLE")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangFeSizeVariableEmptyPrimitiveValueComplete")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ForeachSizeVariableGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ForeachSizeVariableGenerator dump path=%s cases=640 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(352, CaseCount));
		ASSERT_THAT(AreEqual(640, Cases.Num()));
		ASSERT_THAT(AreEqual(288, RejectIds.Num()));

		int32 RejectCount = 0;
		int32 FaultCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			if (Expected.bReject)
			{
				++RejectCount;
				ASSERT_THAT(AreEqual(FString(), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::CompileReject));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::SourceOnly));
				ASSERT_THAT(IsTrue(RejectIds.Contains(Expected.CaseId)));
			}
			else if (Expected.bFault)
			{
				++FaultCount;
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")), Cases[Index].ExpectedException));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
			}
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			}
		}
		ASSERT_THAT(AreEqual(288, RejectCount));
		ASSERT_THAT(AreEqual(33, FaultCount));

		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangFeSizeVariableEmptyPrimitiveValueComplete()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int EntryLangFeSizeVariableEmptyPrimitiveIncompatible"))));

		FForeachSizeVariableParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildForeachIterationSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FForeachSizeVariableParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildForeachIterationSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/ForeachSizeVariableGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			const FString CaseSource = Expected.bReject
				? Generator.BuildRejectSource(Expected.CaseId)
				: Generator.BuildCaseSource(Expected.CaseId);
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("foreach ("))));
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

		FForeachSizeVariableParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildForeachIterationSource(InvalidName).IsEmpty()));
		FForeachSizeVariableParams InvalidEnum = DefaultParams;
		InvalidEnum.Element = static_cast<EForeachElement>(4);
		ASSERT_THAT(IsTrue(Generator.BuildForeachIterationSource(InvalidEnum).IsEmpty()));
		FForeachSizeVariableParams RejectParams = DefaultParams;
		RejectParams.Variable = EForeachVariable::Incompatible;
		ASSERT_THAT(IsTrue(Generator.BuildForeachIterationSource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-FE-PROTOCOL-COMPLETE-EXACT-SINGLE")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(ZeroCaseId)));
		bool bFoundZero = false;
		for (const FGeneratedCaseInfo& Info : Cases)
		{
			if (Info.CaseId == ZeroCaseId)
			{
				bFoundZero = true;
				ASSERT_THAT(IsTrue(Info.ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Info.ExpectedReturn.Get(MIN_int32)));
			}
		}
		ASSERT_THAT(IsTrue(bFoundZero));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(640, CountOccurrences(Dump, TEXT("// Case: "))));
		ASSERT_THAT(AreEqual(288, CountOccurrences(Dump, TEXT("// Reject module\n"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/ForeachSizeVariableGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ForeachSizeVariableGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
