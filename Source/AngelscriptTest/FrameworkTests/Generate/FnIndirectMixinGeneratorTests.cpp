#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestFnIndirectMixinGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	FnIndirectMixinGenerator,
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

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-FN-INDIRECT-MIXIN-DECLARATION_METADATA");

	static bool IndependentReject(const TCHAR* Scenario)
	{
		return FCString::Strcmp(Scenario, TEXT("NULL_OR_UNBOUND")) == 0
			|| FCString::Strcmp(Scenario, TEXT("INCOMPATIBLE_SIGNATURE")) == 0;
	}

	static FString IndependentNeedle(const TCHAR* Scenario)
	{
		if (FCString::Strcmp(Scenario, TEXT("NULL_OR_UNBOUND")) == 0)
		{
			return TEXT("MissingMixin");
		}
		if (FCString::Strcmp(Scenario, TEXT("INCOMPATIBLE_SIGNATURE")) == 0)
		{
			return TEXT("int Value = 40;");
		}
		if (FCString::Strcmp(Scenario, TEXT("COMPATIBLE_NESTED")) == 0)
		{
			return TEXT("InvokeMixin");
		}
		return TEXT("AddValue");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kScenarios[] = {
			TEXT("DECLARATION_METADATA"), TEXT("COMPATIBLE_DIRECT"), TEXT("COMPATIBLE_NESTED"),
			TEXT("NULL_OR_UNBOUND"), TEXT("INCOMPATIBLE_SIGNATURE"), TEXT("REBUILD_OR_REBIND")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(6);
		for (int32 Index = 0; Index < 6; ++Index)
		{
			const TCHAR* const Scenario = kScenarios[Index];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-FN-INDIRECT-MIXIN-%s"), Scenario);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Scenario);
			Cell.Expected = Cell.bReject ? 0 : 42;
			Cell.Needle = IndependentNeedle(Scenario);
			Cell.bLimited = FCString::Strcmp(Scenario, TEXT("REBUILD_OR_REBIND")) == 0;
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EFnIndirectMixinScenario;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FFnIndirectMixinGenerator;
		using AngelscriptTest::Generate::FFnIndirectMixinParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FFnIndirectMixinGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-FN-INDIRECT-MIXIN")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangFnIndirectMixinDeclarationMetadata")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("FnIndirectMixinGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("FnIndirectMixinGenerator dump path=%s cases=6 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(4, CaseCount));
		ASSERT_THAT(AreEqual(6, Cases.Num()));
		ASSERT_THAT(AreEqual(2, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/FnIndirectMixinGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

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
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(RejectSource, Expected.EntryName, GeneratedBody)));
				if (Representative.Contains(Expected.EntryName))
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
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(AreEqual(Expected.bLimited, Cases[Index].bLimitedObservation));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
				if (Representative.Contains(Expected.EntryName))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
		}
		ASSERT_THAT(AreEqual(2, RejectCount));

		FFnIndirectMixinParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildMixinSource(DefaultParams).Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsTrue(Generator.BuildMixinSource(DefaultParams).Contains(TEXT("Self.Value + Delta + 0"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildMixinSource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildMixinSource(DefaultParams).IsEmpty()));
		DefaultParams.FunctionName.Reset();
		DefaultParams.Scenario = EFnIndirectMixinScenario::NullOrUnbound;
		ASSERT_THAT(IsTrue(Generator.BuildMixinSource(DefaultParams).IsEmpty()));
		DefaultParams.Scenario = static_cast<EFnIndirectMixinScenario>(6);
		ASSERT_THAT(IsTrue(Generator.BuildMixinSource(DefaultParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(6, CountOccurrences(Dump, TEXT("// Case: LANG-FN-INDIRECT-MIXIN-"))));
		ASSERT_THAT(AreEqual(2, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/FnIndirectMixinGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("FnIndirectMixinGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
