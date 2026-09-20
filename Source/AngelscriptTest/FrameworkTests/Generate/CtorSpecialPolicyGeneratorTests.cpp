#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestCtorSpecialPolicyGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	CtorSpecialPolicyGenerator,
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
		bool bLimited = false;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-CTOR-SPECIAL-POLICY-COMPILE-IMPLICIT_STRUCT_DEFAULT");
	static constexpr const TCHAR* ZeroCaseId =
		TEXT("LANG-CTOR-SPECIAL-POLICY-COMPILE-IMPLICIT_STRUCT_DEFAULT");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-CONDITION-IF-BOOL_LITERAL-TRUE");

	static bool IndependentReject(const TCHAR* Scenario)
	{
		return FCString::Strcmp(Scenario, TEXT("PARAMETER_SUPPRESSES_DEFAULT_OPTION_OFF")) == 0
			|| FCString::Strcmp(Scenario, TEXT("IMPLICIT_STRUCT_COPY")) == 0
			|| FCString::Strcmp(Scenario, TEXT("USER_DESTRUCTOR_COPY")) == 0
			|| FCString::Strcmp(Scenario, TEXT("MISSING_BASE_DEFAULT_OPTION_OFF")) == 0
			|| FCString::Strcmp(Scenario, TEXT("COPY_AFTER_USER_CONSTRUCTOR")) == 0;
	}

	static int32 IndependentExpected(const TCHAR* Scenario)
	{
		if (FCString::Strcmp(Scenario, TEXT("DECLARED_STRUCT_DEFAULT")) == 0)
		{
			return 12;
		}
		if (FCString::Strcmp(Scenario, TEXT("PARAMETER_PRESERVES_GENERATED_DEFAULT")) == 0)
		{
			return 13;
		}
		if (FCString::Strcmp(Scenario, TEXT("DECLARED_STRUCT_COPY")) == 0)
		{
			return 32;
		}
		if (FCString::Strcmp(Scenario, TEXT("IMPLICIT_STRUCT_ASSIGNMENT")) == 0)
		{
			return 34;
		}
		if (FCString::Strcmp(Scenario, TEXT("DECLARED_STRUCT_ASSIGNMENT")) == 0)
		{
			return 36;
		}
		if (FCString::Strcmp(Scenario, TEXT("CLASS_FACTORY_DEFAULT")) == 0)
		{
			return 20;
		}
		if (FCString::Strcmp(Scenario, TEXT("CLASS_PARAMETER_FACTORY")) == 0)
		{
			return 21;
		}
		if (FCString::Strcmp(Scenario, TEXT("DERIVED_GENERATED_DEFAULT")) == 0)
		{
			return 34;
		}
		if (FCString::Strcmp(Scenario, TEXT("DERIVED_EXPLICIT_SUPER")) == 0)
		{
			return 26;
		}
		if (FCString::Strcmp(Scenario, TEXT("ASSIGNMENT_SELF_STABILITY")) == 0)
		{
			return 84;
		}
		return 0;
	}

	static FString IndependentNeedle(const TCHAR* Scenario)
	{
		if (FCString::Strcmp(Scenario, TEXT("IMPLICIT_STRUCT_DEFAULT")) == 0)
		{
			return TEXT("RecordConstructorPolicyMarker(101)");
		}
		if (FCString::Strcmp(Scenario, TEXT("DECLARED_STRUCT_DEFAULT")) == 0)
		{
			return TEXT("Value = 12;");
		}
		if (FCString::Strcmp(Scenario, TEXT("PARAMETER_PRESERVES_GENERATED_DEFAULT")) == 0)
		{
			return TEXT("ParameterValue(13)");
		}
		if (FCString::Strcmp(Scenario, TEXT("PARAMETER_SUPPRESSES_DEFAULT_OPTION_OFF")) == 0)
		{
			return TEXT("FPolicyValuePARAMETERSUPPRESSESDEFAULTOPTIONOFF Value;");
		}
		if (FCString::Strcmp(Scenario, TEXT("IMPLICIT_STRUCT_COPY")) == 0)
		{
			return TEXT("Target(Source)");
		}
		if (FCString::Strcmp(Scenario, TEXT("DECLARED_STRUCT_COPY")) == 0)
		{
			return TEXT("Source.Value = 16;");
		}
		if (FCString::Strcmp(Scenario, TEXT("IMPLICIT_STRUCT_ASSIGNMENT")) == 0)
		{
			return TEXT("Source.Value = 17;");
		}
		if (FCString::Strcmp(Scenario, TEXT("DECLARED_STRUCT_ASSIGNMENT")) == 0)
		{
			return TEXT("Source.Value = 18;");
		}
		if (FCString::Strcmp(Scenario, TEXT("USER_DESTRUCTOR_COPY")) == 0)
		{
			return TEXT("~FPolicyValueUSERDESTRUCTORCOPY()");
		}
		if (FCString::Strcmp(Scenario, TEXT("CLASS_FACTORY_DEFAULT")) == 0)
		{
			return TEXT("FPolicyClassCLASSFACTORYDEFAULT()");
		}
		if (FCString::Strcmp(Scenario, TEXT("CLASS_PARAMETER_FACTORY")) == 0)
		{
			return TEXT("FPolicyClassCLASSPARAMETERFACTORY(21)");
		}
		if (FCString::Strcmp(Scenario, TEXT("DERIVED_GENERATED_DEFAULT")) == 0)
		{
			return TEXT("Value.BaseValue + Value.DerivedValue");
		}
		if (FCString::Strcmp(Scenario, TEXT("DERIVED_EXPLICIT_SUPER")) == 0)
		{
			return TEXT("super(13);");
		}
		if (FCString::Strcmp(Scenario, TEXT("MISSING_BASE_DEFAULT_OPTION_OFF")) == 0)
		{
			return TEXT("FPolicyBaseMISSINGBASEDEFAULTOPTIONOFF(int InValue)");
		}
		if (FCString::Strcmp(Scenario, TEXT("COPY_AFTER_USER_CONSTRUCTOR")) == 0)
		{
			return TEXT("FPolicyValueCOPYAFTERUSERCONSTRUCTOR Source(15)");
		}
		return TEXT("Value = Value;");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kObservations[] = {
			TEXT("COMPILE"), TEXT("METADATA"), TEXT("RUNTIME"), TEXT("LIFECYCLE")};
		static const TCHAR* const kScenarios[] = {
			TEXT("IMPLICIT_STRUCT_DEFAULT"),
			TEXT("DECLARED_STRUCT_DEFAULT"),
			TEXT("PARAMETER_PRESERVES_GENERATED_DEFAULT"),
			TEXT("PARAMETER_SUPPRESSES_DEFAULT_OPTION_OFF"),
			TEXT("IMPLICIT_STRUCT_COPY"),
			TEXT("DECLARED_STRUCT_COPY"),
			TEXT("IMPLICIT_STRUCT_ASSIGNMENT"),
			TEXT("DECLARED_STRUCT_ASSIGNMENT"),
			TEXT("USER_DESTRUCTOR_COPY"),
			TEXT("CLASS_FACTORY_DEFAULT"),
			TEXT("CLASS_PARAMETER_FACTORY"),
			TEXT("DERIVED_GENERATED_DEFAULT"),
			TEXT("DERIVED_EXPLICIT_SUPER"),
			TEXT("MISSING_BASE_DEFAULT_OPTION_OFF"),
			TEXT("COPY_AFTER_USER_CONSTRUCTOR"),
			TEXT("ASSIGNMENT_SELF_STABILITY"),
		};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(64);
		for (int32 Index = 0; Index < 64; ++Index)
		{
			const TCHAR* const Observation = kObservations[Index / 16];
			const TCHAR* const Scenario = kScenarios[Index % 16];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-CTOR-SPECIAL-POLICY-%s-%s"), Observation, Scenario);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Scenario);
			Cell.bLimited = FCString::Strcmp(Observation, TEXT("RUNTIME")) != 0;
			Cell.Expected = Cell.bReject ? 0 : IndependentExpected(Scenario);
			Cell.Needle = IndependentNeedle(Scenario);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::ECtorSpecialPolicyObservation;
		using AngelscriptTest::Generate::ECtorSpecialPolicyScenario;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FCtorSpecialPolicyGenerator;
		using AngelscriptTest::Generate::FCtorSpecialPolicyParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FCtorSpecialPolicyGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CTOR-SPECIAL-POLICY")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangCtorSpecialPolicyCompileImplicitStructDefault")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("CtorSpecialPolicyGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("CtorSpecialPolicyGenerator dump path=%s cases=64 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(44, CaseCount));
		ASSERT_THAT(AreEqual(64, Cases.Num()));
		ASSERT_THAT(AreEqual(20, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangCtorSpecialPolicyCompileImplicitStructDefault()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int EntryLangCtorSpecialPolicyCompileImplicitStructCopy()"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("void RecordConstructorPolicyMarker(int Marker)"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/CtorSpecialPolicyGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 RejectCount = 0;
		int32 ZeroCount = 0;
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
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(RejectSource, Expected.EntryName, GeneratedBody)));
				if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedException.IsEmpty()));
				ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("constructor-policy"))));
				ASSERT_THAT(IsTrue(Cases[Index].bLimitedObservation == Expected.bLimited));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle) || Aggregate.Contains(Expected.Needle)));
				if (Expected.Expected == 0)
				{
					++ZeroCount;
				}
				if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
		}
		ASSERT_THAT(AreEqual(20, RejectCount));
		ASSERT_THAT(AreEqual(4, ZeroCount));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));

		FCtorSpecialPolicyParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildConstructorPolicySource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildConstructorPolicySource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildConstructorPolicySource(DefaultParams).IsEmpty()));
		DefaultParams.FunctionName.Reset();
		DefaultParams.Scenario = ECtorSpecialPolicyScenario::ImplicitStructCopy;
		ASSERT_THAT(IsTrue(Generator.BuildConstructorPolicySource(DefaultParams).IsEmpty()));
		DefaultParams.Scenario = ECtorSpecialPolicyScenario::ImplicitStructDefault;
		DefaultParams.Observation = static_cast<ECtorSpecialPolicyObservation>(4);
		ASSERT_THAT(IsTrue(Generator.BuildConstructorPolicySource(DefaultParams).IsEmpty()));
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

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(64, CountOccurrences(Dump, TEXT("// Case: LANG-CTOR-SPECIAL-POLICY-"))));
		ASSERT_THAT(AreEqual(20, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/CtorSpecialPolicyGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("CtorSpecialPolicyGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
