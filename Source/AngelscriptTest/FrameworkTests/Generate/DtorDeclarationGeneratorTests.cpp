#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestDtorDeclarationGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	DtorDeclarationGenerator,
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

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-DTOR-DECLARATION-IMPLICIT_SCRIPT_VALUE-COMPILE");

	static int32 IndependentExpected(const TCHAR* Scenario)
	{
		if (FCString::Strcmp(Scenario, TEXT("IMPLICIT_SCRIPT_VALUE")) == 0)
		{
			return 41;
		}
		if (FCString::Strcmp(Scenario, TEXT("DECLARED_SCRIPT_VALUE")) == 0)
		{
			return 42;
		}
		if (FCString::Strcmp(Scenario, TEXT("IMPLICIT_SCRIPT_REFERENCE")) == 0)
		{
			return 43;
		}
		if (FCString::Strcmp(Scenario, TEXT("DECLARED_SCRIPT_REFERENCE")) == 0)
		{
			return 44;
		}
		if (FCString::Strcmp(Scenario, TEXT("NATIVE_VALUE")) == 0)
		{
			return 45;
		}
		if (FCString::Strcmp(Scenario, TEXT("NATIVE_REFERENCE")) == 0)
		{
			return 46;
		}
		if (FCString::Strcmp(Scenario, TEXT("EMPTY_DESTRUCTOR")) == 0)
		{
			return 47;
		}
		if (FCString::Strcmp(Scenario, TEXT("FIELD_DESTRUCTOR")) == 0)
		{
			return 48;
		}
		if (FCString::Strcmp(Scenario, TEXT("BASE_DERIVED_DESTRUCTOR")) == 0)
		{
			return 49;
		}
		if (FCString::Strcmp(Scenario, TEXT("PRIVATE_DESTRUCTOR")) == 0)
		{
			return 50;
		}
		return 0;
	}

	static FString IndependentNeedle(const TCHAR* Scenario)
	{
		if (FCString::Strcmp(Scenario, TEXT("IMPLICIT_SCRIPT_VALUE")) == 0)
		{
			return TEXT("FImplicitValue Object;");
		}
		if (FCString::Strcmp(Scenario, TEXT("DECLARED_SCRIPT_VALUE")) == 0)
		{
			return TEXT("FDeclaredValue Object;");
		}
		if (FCString::Strcmp(Scenario, TEXT("IMPLICIT_SCRIPT_REFERENCE")) == 0)
		{
			return TEXT("FImplicitReference Object = FImplicitReference();");
		}
		if (FCString::Strcmp(Scenario, TEXT("DECLARED_SCRIPT_REFERENCE")) == 0)
		{
			return TEXT("FDeclaredReference Object = FDeclaredReference();");
		}
		if (FCString::Strcmp(Scenario, TEXT("NATIVE_VALUE")) == 0)
		{
			return TEXT("FNativeCaseValue Object(45);");
		}
		if (FCString::Strcmp(Scenario, TEXT("NATIVE_REFERENCE")) == 0)
		{
			return TEXT("CreateNativeCaseReference(46)");
		}
		if (FCString::Strcmp(Scenario, TEXT("EMPTY_DESTRUCTOR")) == 0)
		{
			return TEXT("FEmptyDestructor Object;");
		}
		if (FCString::Strcmp(Scenario, TEXT("FIELD_DESTRUCTOR")) == 0)
		{
			return TEXT("FFieldOwner Object;");
		}
		if (FCString::Strcmp(Scenario, TEXT("BASE_DERIVED_DESTRUCTOR")) == 0)
		{
			return TEXT("FDestructorDerived Object = FDestructorDerived();");
		}
		if (FCString::Strcmp(Scenario, TEXT("PRIVATE_DESTRUCTOR")) == 0)
		{
			return TEXT("FPrivateDestructor Object = FPrivateDestructor();");
		}
		if (FCString::Strcmp(Scenario, TEXT("THROWING_DESTRUCTOR")) == 0)
		{
			return TEXT("FThrowingDestructor Object = FThrowingDestructor();");
		}
		if (FCString::Strcmp(Scenario, TEXT("THROWING_DERIVED_MEMBERS_BASE")) == 0)
		{
			return TEXT("FThrowingDerived Object = FThrowingDerived();");
		}
		return TEXT("FMalformedDestructor");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kScenarios[] = {
			TEXT("IMPLICIT_SCRIPT_VALUE"), TEXT("DECLARED_SCRIPT_VALUE"), TEXT("IMPLICIT_SCRIPT_REFERENCE"),
			TEXT("DECLARED_SCRIPT_REFERENCE"), TEXT("NATIVE_VALUE"), TEXT("NATIVE_REFERENCE"),
			TEXT("EMPTY_DESTRUCTOR"), TEXT("FIELD_DESTRUCTOR"), TEXT("BASE_DERIVED_DESTRUCTOR"),
			TEXT("PRIVATE_DESTRUCTOR"), TEXT("THROWING_DESTRUCTOR"), TEXT("THROWING_DERIVED_MEMBERS_BASE"),
			TEXT("MALFORMED_DESTRUCTOR")};
		static const TCHAR* const kObservations[] = {
			TEXT("COMPILE"), TEXT("METADATA"), TEXT("RUNTIME"), TEXT("CLEANUP")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(52);
		for (int32 Index = 0; Index < 52; ++Index)
		{
			const TCHAR* const Scenario = kScenarios[Index / 4];
			const TCHAR* const Observation = kObservations[Index % 4];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-DTOR-DECLARATION-%s-%s"), Scenario, Observation);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Needle = IndependentNeedle(Scenario);
			Cell.bReject = FCString::Strcmp(Scenario, TEXT("MALFORMED_DESTRUCTOR")) == 0;
			Cell.bFault = !Cell.bReject
				&& (FCString::Strcmp(Scenario, TEXT("THROWING_DESTRUCTOR")) == 0
					|| FCString::Strcmp(Scenario, TEXT("THROWING_DERIVED_MEMBERS_BASE")) == 0);
			Cell.Expected = (Cell.bReject || Cell.bFault) ? 0 : IndependentExpected(Scenario);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EDtorDeclarationScenario;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FDtorDeclarationGenerator;
		using AngelscriptTest::Generate::FDtorDeclarationParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FDtorDeclarationGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-DTOR-DECLARATION")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangDtorDeclarationImplicitScriptValueCompile")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("DtorDeclarationGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("DtorDeclarationGenerator dump path=%s cases=52 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(48, CaseCount));
		ASSERT_THAT(AreEqual(52, Cases.Num()));
		ASSERT_THAT(AreEqual(4, RejectIds.Num()));

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
				ASSERT_THAT(IsTrue(RejectIds.Contains(Expected.CaseId)));
				const FString RejectSource = Generator.BuildRejectSource(Expected.CaseId);
				ASSERT_THAT(IsTrue(RejectSource.Contains(TEXT("~FMalformedDestructor(int InvalidParameter)"))));
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
			}
			else if (Expected.bFault)
			{
				++FaultCount;
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
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
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
			}
		}
		ASSERT_THAT(AreEqual(4, RejectCount));
		ASSERT_THAT(AreEqual(8, FaultCount));

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangDtorDeclarationImplicitScriptValueCompile()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int EntryLangDtorDeclarationMalformed"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("struct FImplicitValue\n{"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int RunDestructorDeclarationRecovery()"))));

		FDtorDeclarationParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildDestructorDeclarationSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FDtorDeclarationParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildDestructorDeclarationSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(Generator.BuildDestructorDeclarationSource(ProbeParams).Contains(TEXT("int Entry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/DtorDeclarationGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			const FString CaseSource = Expected.bReject
				? Generator.BuildRejectSource(Expected.CaseId)
				: Generator.BuildCaseSource(Expected.CaseId);
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
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

		FDtorDeclarationParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildDestructorDeclarationSource(InvalidName).IsEmpty()));
		FDtorDeclarationParams InvalidEnum = DefaultParams;
		InvalidEnum.Scenario = static_cast<EDtorDeclarationScenario>(13);
		ASSERT_THAT(IsTrue(Generator.BuildDestructorDeclarationSource(InvalidEnum).IsEmpty()));
		FDtorDeclarationParams RejectParams = DefaultParams;
		RejectParams.Scenario = EDtorDeclarationScenario::MalformedDestructor;
		ASSERT_THAT(IsTrue(Generator.BuildDestructorDeclarationSource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-FE-PROTOCOL-COMPLETE-EXACT-SINGLE")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(41, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(52, CountOccurrences(Dump, TEXT("// Case: "))));
		ASSERT_THAT(AreEqual(4, CountOccurrences(Dump, TEXT("// Reject module\n"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/DtorDeclarationGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("DtorDeclarationGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
