#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestCtorTransferGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	CtorTransferGenerator,
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
		bool bLimited = false;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-CTOR-TRANSFER-SCRIPT_VALUE_LOCAL-COPY_DECLARATION_LOCAL-INITIAL_IDENTITY_VALUE");
	static constexpr const TCHAR* ZeroCaseId =
		TEXT("LANG-CTOR-TRANSFER-SCRIPT_VALUE_LOCAL-COPY_DECLARATION_LOCAL-TARGET_MUTATION_RELATION");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-CONDITION-IF-BOOL_LITERAL-TRUE");

	static bool IndependentComposite(const TCHAR* Source)
	{
		return FCString::Strcmp(Source, TEXT("SCRIPT_VALUE_LOCAL")) == 0
			|| FCString::Strcmp(Source, TEXT("SCRIPT_VALUE_TEMPORARY")) == 0
			|| FCString::Strcmp(Source, TEXT("SCRIPT_VALUE_RETURN")) == 0
			|| FCString::Strcmp(Source, TEXT("SCRIPT_REFERENCE_LOCAL")) == 0
			|| FCString::Strcmp(Source, TEXT("SCRIPT_REFERENCE_TEMPORARY")) == 0
			|| FCString::Strcmp(Source, TEXT("SCRIPT_REFERENCE_RETURN")) == 0
			|| FCString::Strcmp(Source, TEXT("DERIVED_REFERENCE_EXACT")) == 0
			|| FCString::Strcmp(Source, TEXT("DERIVED_REFERENCE_BASE_VIEW")) == 0;
	}

	static bool IndependentReference(const TCHAR* Source)
	{
		return FCString::Strstr(Source, TEXT("REFERENCE")) != nullptr;
	}

	static bool IndependentTemporary(const TCHAR* Source)
	{
		return FCString::Strstr(Source, TEXT("TEMPORARY")) != nullptr;
	}

	static int32 IndependentStateValue(const TCHAR* Source, const int32 Value)
	{
		return IndependentComposite(Source) ? Value * 10000 + (Value + 100) : Value;
	}

	static int32 IndependentExpected(const TCHAR* Source, const TCHAR* Observation)
	{
		if (FCString::Strcmp(Observation, TEXT("INITIAL_IDENTITY_VALUE")) == 0)
		{
			return IndependentStateValue(Source, 7);
		}
		if (FCString::Strcmp(Observation, TEXT("SOURCE_OR_TEMPORARY_STATE")) == 0)
		{
			return IndependentTemporary(Source) ? -1 : IndependentStateValue(Source, 23);
		}
		if (FCString::Strcmp(Observation, TEXT("TARGET_MUTATION_RELATION")) == 0)
		{
			if (IndependentTemporary(Source))
			{
				return 2;
			}
			return IndependentReference(Source) ? 1 : 0;
		}
		return 1;
	}

	static FString IndependentNeedle(const TCHAR* Source, const TCHAR* Workflow)
	{
		if (FCString::Strcmp(Workflow, TEXT("COPY_DECLARATION_LOCAL")) == 0)
		{
			return TEXT("Copied = Target;");
		}
		if (FCString::Strcmp(Workflow, TEXT("FIELD_CONSTRUCTOR_TRANSFER")) == 0)
		{
			return TEXT("Field = Target;");
		}
		if (FCString::Strcmp(Workflow, TEXT("ASSIGNMENT_LOCAL")) == 0)
		{
			return TEXT("Assigned = Target;");
		}
		if (FCString::Strcmp(Workflow, TEXT("FIELD_ASSIGNMENT_AFTER_DEFAULT")) == 0)
		{
			return TEXT("Field = Target;");
		}
		if (FCString::Strcmp(Workflow, TEXT("ARGUMENT_TRANSFER")) == 0)
		{
			return TEXT("int Auxiliary = Target.Value;");
		}
		if (FCString::Strcmp(Workflow, TEXT("SELF_ASSIGNMENT")) == 0)
		{
			return TEXT("Target = Target;");
		}
		if (FCString::Strcmp(Workflow, TEXT("CHAINED_ASSIGNMENT")) == 0)
		{
			return TEXT("Last = Mid = Target;");
		}
		if (FCString::Strcmp(Source, TEXT("NATIVE_VALUE_LOCAL")) == 0
			|| FCString::Strcmp(Source, TEXT("NATIVE_VALUE_TEMPORARY")) == 0
			|| FCString::Strcmp(Source, TEXT("NATIVE_VALUE_RETURN")) == 0)
		{
			return TEXT("FNativeCaseValue");
		}
		if (IndependentReference(Source) && FCString::Strstr(Source, TEXT("NATIVE")) != nullptr)
		{
			return TEXT("FNativeCaseReference");
		}
		if (FCString::Strstr(Source, TEXT("DERIVED")) != nullptr)
		{
			return TEXT("FTransferDerived");
		}
		if (IndependentReference(Source))
		{
			return TEXT("FTransferReference");
		}
		return TEXT("FTransferValue");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kSources[] = {
			TEXT("SCRIPT_VALUE_LOCAL"), TEXT("SCRIPT_VALUE_TEMPORARY"), TEXT("SCRIPT_VALUE_RETURN"),
			TEXT("NATIVE_VALUE_LOCAL"), TEXT("NATIVE_VALUE_TEMPORARY"), TEXT("NATIVE_VALUE_RETURN"),
			TEXT("SCRIPT_REFERENCE_LOCAL"), TEXT("SCRIPT_REFERENCE_TEMPORARY"), TEXT("SCRIPT_REFERENCE_RETURN"),
			TEXT("NATIVE_REFERENCE_LOCAL"), TEXT("NATIVE_REFERENCE_TEMPORARY"), TEXT("NATIVE_REFERENCE_RETURN"),
			TEXT("DERIVED_REFERENCE_EXACT"), TEXT("DERIVED_REFERENCE_BASE_VIEW")};
		static const TCHAR* const kWorkflows[] = {
			TEXT("COPY_DECLARATION_LOCAL"), TEXT("FIELD_CONSTRUCTOR_TRANSFER"), TEXT("ASSIGNMENT_LOCAL"),
			TEXT("FIELD_ASSIGNMENT_AFTER_DEFAULT"), TEXT("ARGUMENT_TRANSFER"), TEXT("RETURN_TRANSFER"),
			TEXT("SELF_ASSIGNMENT"), TEXT("CHAINED_ASSIGNMENT")};
		static const TCHAR* const kObservations[] = {
			TEXT("INITIAL_IDENTITY_VALUE"), TEXT("SOURCE_OR_TEMPORARY_STATE"),
			TEXT("TARGET_MUTATION_RELATION"), TEXT("LIFECYCLE_CLEANUP")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(448);
		for (int32 Index = 0; Index < 448; ++Index)
		{
			const TCHAR* const Source = kSources[Index / 32];
			const TCHAR* const Workflow = kWorkflows[(Index / 4) % 8];
			const TCHAR* const Observation = kObservations[Index % 4];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-CTOR-TRANSFER-%s-%s-%s"), Source, Workflow, Observation);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bLimited = FCString::Strcmp(Observation, TEXT("LIFECYCLE_CLEANUP")) == 0;
			Cell.Expected = IndependentExpected(Source, Observation);
			Cell.Needle = IndependentNeedle(Source, Workflow);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::ECtorTransferObservation;
		using AngelscriptTest::Generate::ECtorTransferSource;
		using AngelscriptTest::Generate::ECtorTransferWorkflow;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FCtorTransferGenerator;
		using AngelscriptTest::Generate::FCtorTransferParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FCtorTransferGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CTOR-TRANSFER")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangCtorTransferScriptValueLocalCopyDeclarationLocalInitialIdentityValue")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("CtorTransferGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("CtorTransferGenerator dump path=%s cases=448 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(448, CaseCount));
		ASSERT_THAT(AreEqual(448, Cases.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(
			TEXT("int EntryLangCtorTransferScriptValueLocalCopyDeclarationLocalInitialIdentityValue()"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("struct FTransferValue"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/CtorTransferGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

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
			ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("constructor-transfer"))));
			ASSERT_THAT(IsTrue(Cases[Index].bLimitedObservation == Expected.bLimited));
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
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
		ASSERT_THAT(IsTrue(ZeroCount > 0));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));

		FCtorTransferParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildConstructorTransferSource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildConstructorTransferSource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildConstructorTransferSource(DefaultParams).IsEmpty()));
		DefaultParams.FunctionName.Reset();
		DefaultParams.Source = static_cast<ECtorTransferSource>(14);
		ASSERT_THAT(IsTrue(Generator.BuildConstructorTransferSource(DefaultParams).IsEmpty()));
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
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(448, CountOccurrences(Dump, TEXT("// Case: LANG-CTOR-TRANSFER-"))));
		ASSERT_THAT(AreEqual(0, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/CtorTransferGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("CtorTransferGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
