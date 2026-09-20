#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestInhClassRuleGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	InhClassRuleGenerator,
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
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-INH-CLASS-RULE-CONCRETE_INSTANTIATE-RUNTIME");

	static bool IndependentReject(const TCHAR* Scenario)
	{
		return FCString::Strcmp(Scenario, TEXT("CONCRETE_BASE_INHERIT")) != 0
			&& FCString::Strcmp(Scenario, TEXT("CONCRETE_INSTANTIATE")) != 0
			&& FCString::Strcmp(Scenario, TEXT("FINAL_METHOD_INHERIT")) != 0
			&& FCString::Strcmp(Scenario, TEXT("IMPLICIT_OVERRIDE_WITHOUT_KEYWORD")) != 0
			&& FCString::Strcmp(Scenario, TEXT("EXACT_OVERRIDE")) != 0
			&& FCString::Strcmp(Scenario, TEXT("DEEP_OVERRIDE")) != 0;
	}

	static int32 IndependentExpected(const TCHAR* Scenario)
	{
		if (FCString::Strcmp(Scenario, TEXT("CONCRETE_BASE_INHERIT")) == 0)
		{
			return 31;
		}
		if (FCString::Strcmp(Scenario, TEXT("CONCRETE_INSTANTIATE")) == 0)
		{
			return 41;
		}
		if (FCString::Strcmp(Scenario, TEXT("FINAL_METHOD_INHERIT")) == 0)
		{
			return 61;
		}
		if (FCString::Strcmp(Scenario, TEXT("IMPLICIT_OVERRIDE_WITHOUT_KEYWORD")) == 0)
		{
			return 122;
		}
		if (FCString::Strcmp(Scenario, TEXT("EXACT_OVERRIDE")) == 0)
		{
			return 132;
		}
		if (FCString::Strcmp(Scenario, TEXT("DEEP_OVERRIDE")) == 0)
		{
			return 163;
		}
		return 0;
	}

	static FString IndependentNeedle(const TCHAR* Scenario)
	{
		if (FCString::Strcmp(Scenario, TEXT("ABSTRACT_CLASS_KEYWORD_REJECTED")) == 0)
		{
			return TEXT("abstract class ");
		}
		if (FCString::Strcmp(Scenario, TEXT("FINAL_CLASS_KEYWORD_REJECTED")) == 0)
		{
			return TEXT("final class ");
		}
		if (FCString::Strcmp(Scenario, TEXT("CONCRETE_BASE_INHERIT")) == 0)
		{
			return TEXT("return 31;");
		}
		if (FCString::Strcmp(Scenario, TEXT("CONCRETE_INSTANTIATE")) == 0)
		{
			return TEXT("return 41;");
		}
		if (FCString::Strcmp(Scenario, TEXT("FINAL_METHOD_OVERRIDE")) == 0)
		{
			return TEXT("final");
		}
		if (FCString::Strcmp(Scenario, TEXT("FINAL_METHOD_INHERIT")) == 0)
		{
			return TEXT("return 61;");
		}
		if (FCString::Strcmp(Scenario, TEXT("INVALID_BASE_NAME")) == 0)
		{
			return TEXT("FMissingRuleBase");
		}
		if (FCString::Strcmp(Scenario, TEXT("INVALID_BASE_KIND")) == 0)
		{
			return TEXT("struct ");
		}
		if (FCString::Strcmp(Scenario, TEXT("DUPLICATE_BASE")) == 0)
		{
			return TEXT(", ");
		}
		if (FCString::Strcmp(Scenario, TEXT("INHERITANCE_CYCLE_DIRECT")) == 0)
		{
			return TEXT("return 101;");
		}
		if (FCString::Strcmp(Scenario, TEXT("INHERITANCE_CYCLE_INDIRECT")) == 0)
		{
			return TEXT("return 111;");
		}
		if (FCString::Strcmp(Scenario, TEXT("IMPLICIT_OVERRIDE_WITHOUT_KEYWORD")) == 0)
		{
			return TEXT("return 122;");
		}
		if (FCString::Strcmp(Scenario, TEXT("EXACT_OVERRIDE")) == 0)
		{
			return TEXT("return 132;");
		}
		if (FCString::Strcmp(Scenario, TEXT("OVERRIDE_WITHOUT_BASE")) == 0)
		{
			return TEXT("override");
		}
		if (FCString::Strcmp(Scenario, TEXT("RETURN_TYPE_MISMATCH")) == 0)
		{
			return TEXT("float Value() override");
		}
		return TEXT("return 163;");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kScenarios[] = {
			TEXT("ABSTRACT_CLASS_KEYWORD_REJECTED"), TEXT("FINAL_CLASS_KEYWORD_REJECTED"),
			TEXT("CONCRETE_BASE_INHERIT"), TEXT("CONCRETE_INSTANTIATE"), TEXT("FINAL_METHOD_OVERRIDE"),
			TEXT("FINAL_METHOD_INHERIT"), TEXT("INVALID_BASE_NAME"), TEXT("INVALID_BASE_KIND"),
			TEXT("DUPLICATE_BASE"), TEXT("INHERITANCE_CYCLE_DIRECT"), TEXT("INHERITANCE_CYCLE_INDIRECT"),
			TEXT("IMPLICIT_OVERRIDE_WITHOUT_KEYWORD"), TEXT("EXACT_OVERRIDE"), TEXT("OVERRIDE_WITHOUT_BASE"),
			TEXT("RETURN_TYPE_MISMATCH"), TEXT("DEEP_OVERRIDE")};
		static const TCHAR* const kObservations[] = {
			TEXT("COMPILE"), TEXT("DIAGNOSTIC"), TEXT("METADATA"), TEXT("RUNTIME")};
		TArray<FExpectedCell> Cells;
		Cells.SetNum(64);
		for (int32 Index = 0; Index < 64; ++Index)
		{
			const TCHAR* const Scenario = kScenarios[Index / 4];
			const TCHAR* const Observation = kObservations[Index % 4];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-INH-CLASS-RULE-%s-%s"), Scenario, Observation);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Scenario);
			Cell.Expected = Cell.bReject ? 0 : IndependentExpected(Scenario);
			Cell.Needle = IndependentNeedle(Scenario);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EInhClassRuleObservation;
		using AngelscriptTest::Generate::EInhClassRuleScenario;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FInhClassRuleGenerator;
		using AngelscriptTest::Generate::FInhClassRuleParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FInhClassRuleGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-INH-CLASS-RULE")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangInhClassRuleConcreteInstantiateRuntime")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("InhClassRuleGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("InhClassRuleGenerator dump path=%s cases=64 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(24, CaseCount));
		ASSERT_THAT(AreEqual(64, Cases.Num()));
		ASSERT_THAT(AreEqual(40, RejectIds.Num()));

		int32 RejectCount = 0;
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
			}
			else
			{
				ASSERT_THAT(AreEqual(
					FString::Printf(TEXT("int %s()"), *Expected.EntryName),
					Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
			}
		}
		ASSERT_THAT(AreEqual(40, RejectCount));

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangInhClassRuleConcreteInstantiateRuntime()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("abstract class"))));
		ASSERT_THAT(AreEqual(24, CountOccurrences(Aggregate, TEXT("class FRulePrimary"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));

		FInhClassRuleParams DefaultParams;
		DefaultParams.Scenario = EInhClassRuleScenario::ConcreteInstantiate;
		ASSERT_THAT(IsTrue(Generator.BuildInheritanceRuleSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FInhClassRuleParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildInheritanceRuleSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(Generator.BuildInheritanceRuleSource(ProbeParams).Contains(TEXT("int Entry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/InhClassRuleGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			const FString CaseSource = Expected.bReject
				? Generator.BuildRejectSource(Expected.CaseId)
				: Generator.BuildCaseSource(Expected.CaseId);
			ASSERT_THAT(IsTrue(CaseSource.Contains(Expected.Needle)));
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FInhClassRuleParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildInheritanceRuleSource(InvalidName).IsEmpty()));
		FInhClassRuleParams InvalidEnum = DefaultParams;
		InvalidEnum.Scenario = static_cast<EInhClassRuleScenario>(16);
		ASSERT_THAT(IsTrue(Generator.BuildInheritanceRuleSource(InvalidEnum).IsEmpty()));
		FInhClassRuleParams RejectParams = DefaultParams;
		RejectParams.Scenario = EInhClassRuleScenario::AbstractClassKeywordRejected;
		ASSERT_THAT(IsTrue(Generator.BuildInheritanceRuleSource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-INH-ACCESS-DEFAULT-FIELD-OWNER")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(64, CountOccurrences(Dump, TEXT("// Case: LANG-INH-CLASS-RULE-"))));
		ASSERT_THAT(AreEqual(40, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/InhClassRuleGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("InhClassRuleGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
