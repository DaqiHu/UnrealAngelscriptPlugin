#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestPropAccessorGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	PropAccessorGenerator,
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
		bool bReject = false;
		bool bFault = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-PROP-ACCESSOR-GETTER_READ_MUTABLE-SINGLE_LINE");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static bool IndependentReject(const TCHAR* Scenario)
	{
		return FCString::Strcmp(Scenario, TEXT("GETTER_NONCONST_READ_CONST_REJECTED")) == 0
			|| FCString::Strcmp(Scenario, TEXT("GETTER_WRITE_MISSING")) == 0
			|| FCString::Strcmp(Scenario, TEXT("GETTER_COMPOUND_MISSING_SET")) == 0
			|| FCString::Strcmp(Scenario, TEXT("SETTER_WRITE_CONST_REJECTED")) == 0
			|| FCString::Strcmp(Scenario, TEXT("SETTER_READ_MISSING")) == 0
			|| FCString::Strcmp(Scenario, TEXT("SETTER_COMPOUND_MISSING_GET")) == 0
			|| FCString::Strcmp(Scenario, TEXT("BOTH_COMPOUND_CONST_REJECTED")) == 0;
	}

	static bool IndependentFault(const TCHAR* Scenario)
	{
		return FCString::Strcmp(Scenario, TEXT("THROWING_GETTER")) == 0
			|| FCString::Strcmp(Scenario, TEXT("THROWING_SETTER")) == 0;
	}

	static bool IndependentWrite(const TCHAR* Scenario)
	{
		return FCString::Strcmp(Scenario, TEXT("GETTER_WRITE_MISSING")) == 0
			|| FCString::Strcmp(Scenario, TEXT("SETTER_WRITE_MUTABLE")) == 0
			|| FCString::Strcmp(Scenario, TEXT("SETTER_WRITE_CONST_REJECTED")) == 0
			|| FCString::Strcmp(Scenario, TEXT("BOTH_WRITE_MUTABLE")) == 0
			|| FCString::Strcmp(Scenario, TEXT("RECURSIVE_SETTER")) == 0
			|| FCString::Strcmp(Scenario, TEXT("THROWING_SETTER")) == 0;
	}

	static bool IndependentCompound(const TCHAR* Scenario)
	{
		return FCString::Strcmp(Scenario, TEXT("GETTER_COMPOUND_MISSING_SET")) == 0
			|| FCString::Strcmp(Scenario, TEXT("SETTER_COMPOUND_MISSING_GET")) == 0
			|| FCString::Strcmp(Scenario, TEXT("BOTH_COMPOUND_MUTABLE")) == 0
			|| FCString::Strcmp(Scenario, TEXT("BOTH_COMPOUND_CONST_REJECTED")) == 0;
	}

	static int32 IndependentExpected(const TCHAR* Scenario)
	{
		if (IndependentReject(Scenario) || IndependentFault(Scenario))
		{
			return 0;
		}
		if (FCString::Strcmp(Scenario, TEXT("BOTH_COMPOUND_MUTABLE")) == 0)
		{
			return 36;
		}
		return IndependentWrite(Scenario) ? 73 : 31;
	}

	static FString IndependentNeedle(const TCHAR* Scenario, const TCHAR* Shape)
	{
		if (FCString::Strcmp(Shape, TEXT("HELPER_CALL")) == 0)
		{
			return TEXT("return InvokeRegisteredProperty_");
		}
		if (IndependentFault(Scenario))
		{
			return TEXT("return 1 / Zero;");
		}
		if (IndependentWrite(Scenario))
		{
			return FCString::Strcmp(Shape, TEXT("PARENTHESIZED")) == 0
				? TEXT("((Receiver.Value)) = 73;")
				: TEXT("Receiver.Value =");
		}
		if (IndependentCompound(Scenario))
		{
			return FCString::Strcmp(Shape, TEXT("PARENTHESIZED")) == 0
				? TEXT("((Receiver.Value)) += 5;")
				: TEXT("Receiver.Value +=");
		}
		if (FCString::Strcmp(Shape, TEXT("PARENTHESIZED")) == 0)
		{
			return TEXT("return (((Receiver.Value)));");
		}
		if (FCString::Strcmp(Shape, TEXT("MULTILINE")) == 0)
		{
			return TEXT("Receiver.Value;");
		}
		return TEXT("return Receiver.Value;");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kScenarios[] = {
			TEXT("GETTER_READ_MUTABLE"), TEXT("GETTER_READ_CONST"), TEXT("GETTER_NONCONST_READ_MUTABLE"),
			TEXT("GETTER_NONCONST_READ_CONST_REJECTED"), TEXT("GETTER_WRITE_MISSING"),
			TEXT("GETTER_COMPOUND_MISSING_SET"), TEXT("SETTER_WRITE_MUTABLE"),
			TEXT("SETTER_WRITE_CONST_REJECTED"), TEXT("SETTER_READ_MISSING"),
			TEXT("SETTER_COMPOUND_MISSING_GET"), TEXT("BOTH_READ_MUTABLE"), TEXT("BOTH_WRITE_MUTABLE"),
			TEXT("BOTH_COMPOUND_MUTABLE"), TEXT("BOTH_COMPOUND_CONST_REJECTED"), TEXT("RECURSIVE_GETTER"),
			TEXT("RECURSIVE_SETTER"), TEXT("THROWING_GETTER"), TEXT("THROWING_SETTER")};
		static const TCHAR* const kShapes[] = {
			TEXT("SINGLE_LINE"), TEXT("MULTILINE"), TEXT("PARENTHESIZED"), TEXT("HELPER_CALL")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(72);
		for (int32 Index = 0; Index < 72; ++Index)
		{
			const TCHAR* const Scenario = kScenarios[Index / 4];
			const TCHAR* const Shape = kShapes[Index % 4];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-PROP-ACCESSOR-%s-%s"), Scenario, Shape);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Scenario);
			Cell.bFault = !Cell.bReject && IndependentFault(Scenario);
			Cell.Expected = IndependentExpected(Scenario);
			Cell.Needle = IndependentNeedle(Scenario, Shape);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EPropAccessorScenario;
		using AngelscriptTest::Generate::EPropAccessorSourceShape;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FPropAccessorGenerator;
		using AngelscriptTest::Generate::FPropAccessorParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FPropAccessorGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-PROP-ACCESSOR")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangPropAccessorGetterReadMutableSingleLine")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("PropAccessorGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("PropAccessorGenerator dump path=%s cases=72 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(44, CaseCount));
		ASSERT_THAT(AreEqual(72, Cases.Num()));
		ASSERT_THAT(AreEqual(28, RejectIds.Num()));

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
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(RejectIds.Contains(Expected.CaseId)));
				const FString RejectSource = Generator.BuildRejectSource(Expected.CaseId);
				ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.Needle)));
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
				FString RejectBody;
				ASSERT_THAT(IsTrue(ExtractFunction(RejectSource, Expected.EntryName, RejectBody)));
				ASSERT_THAT(IsTrue(RejectBody.Contains(Expected.Needle)));
			}
			else if (Expected.bFault)
			{
				++FaultCount;
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")), Cases[Index].ExpectedException));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
				ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("CreateRegisteredProperty"))));
			}
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
			}
		}
		ASSERT_THAT(AreEqual(28, RejectCount));
		ASSERT_THAT(AreEqual(8, FaultCount));

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangPropAccessorGetterReadMutableSingleLine()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int EntryLangPropAccessorGetterNonconstReadConstRejected"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("class FRegisteredProperty\n"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("FRegisteredProperty CreateRegisteredProperty(int Value)"))));

		FPropAccessorParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildRegisteredPropertySource(DefaultParams).Contains(TEXT("int Entry()"))));
		FPropAccessorParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildRegisteredPropertySource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/PropAccessorGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			if (Expected.bReject)
			{
				continue;
			}
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("CreateRegisteredProperty(31)"))));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
			if (Expected.bFault)
			{
				ASSERT_THAT(IsTrue(Generator.BuildCaseSource(Expected.CaseId).Contains(TEXT("return 1 / Zero;"))));
			}
			if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FPropAccessorParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildRegisteredPropertySource(InvalidName).IsEmpty()));
		FPropAccessorParams InvalidEnum = DefaultParams;
		InvalidEnum.Scenario = static_cast<EPropAccessorScenario>(18);
		ASSERT_THAT(IsTrue(Generator.BuildRegisteredPropertySource(InvalidEnum).IsEmpty()));
		FPropAccessorParams RejectParams = DefaultParams;
		RejectParams.Scenario = EPropAccessorScenario::GetterWriteMissing;
		ASSERT_THAT(IsTrue(Generator.BuildRegisteredPropertySource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(ExpectedCells[0].Expected, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(72, CountOccurrences(Dump, TEXT("// Case: LANG-PROP-ACCESSOR-"))));
		ASSERT_THAT(AreEqual(28, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/PropAccessorGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("PropAccessorGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
