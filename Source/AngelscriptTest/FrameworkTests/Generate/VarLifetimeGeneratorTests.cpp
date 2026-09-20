#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestVarLifetimeGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	VarLifetimeGenerator,
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
		bool bFault = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-VAR-LIFETIME-BLOCK_END-ONE-LOCAL_VALUE");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static int32 IndependentExpected(const TCHAR* Exit)
	{
		if (FCString::Strcmp(Exit, TEXT("BLOCK_END")) == 0)
		{
			return 71;
		}
		if (FCString::Strcmp(Exit, TEXT("RETURN")) == 0)
		{
			return 72;
		}
		if (FCString::Strcmp(Exit, TEXT("BREAK")) == 0)
		{
			return 73;
		}
		if (FCString::Strcmp(Exit, TEXT("CONTINUE")) == 0)
		{
			return 74;
		}
		return 0;
	}

	static FString IndependentNeedle(const TCHAR* Exit, const TCHAR* Nesting, const TCHAR* Owner)
	{
		if (FCString::Strcmp(Nesting, TEXT("NESTED_CALL")) == 0)
		{
			return TEXT("NestedEntryLangVarLifetime");
		}
		if (FCString::Strcmp(Exit, TEXT("EXCEPTION")) == 0)
		{
			return TEXT("RaiseVariableLifetimeException");
		}
		if (FCString::Strcmp(Nesting, TEXT("LOOP")) == 0)
		{
			return TEXT("OwnerIteration");
		}
		if (FCString::Strcmp(Exit, TEXT("BREAK")) == 0)
		{
			return TEXT("break;");
		}
		if (FCString::Strcmp(Exit, TEXT("CONTINUE")) == 0)
		{
			return TEXT("continue;");
		}
		if (FCString::Strcmp(Exit, TEXT("RETURN")) == 0)
		{
			return TEXT("return 72 + Trace - Trace;");
		}
		if (FCString::Strcmp(Owner, TEXT("FIELD")) == 0)
		{
			return TEXT("FFieldLifetimeOwner");
		}
		if (FCString::Strcmp(Owner, TEXT("REFERENCE")) == 0)
		{
			return TEXT("CreateNativeCaseReference");
		}
		if (FCString::Strcmp(Owner, TEXT("NESTED_VALUE")) == 0)
		{
			return TEXT("FNestedLifetimeValue");
		}
		return TEXT("local_valueOwner0");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kExits[] = {
			TEXT("BLOCK_END"), TEXT("RETURN"), TEXT("BREAK"), TEXT("CONTINUE"), TEXT("EXCEPTION")};
		static const TCHAR* const kNestings[] = {
			TEXT("ONE"), TEXT("SEQUENTIAL"), TEXT("NESTED_SCOPES"), TEXT("LOOP"), TEXT("NESTED_CALL")};
		static const TCHAR* const kOwners[] = {
			TEXT("LOCAL_VALUE"), TEXT("NESTED_VALUE"), TEXT("FIELD"), TEXT("REFERENCE")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(100);
		for (int32 Index = 0; Index < 100; ++Index)
		{
			const TCHAR* const Exit = kExits[Index / 20];
			const TCHAR* const Nesting = kNestings[(Index / 4) % 5];
			const TCHAR* const Owner = kOwners[Index % 4];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-VAR-LIFETIME-%s-%s-%s"), Exit, Nesting, Owner);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bFault = FCString::Strcmp(Exit, TEXT("EXCEPTION")) == 0;
			Cell.Expected = Cell.bFault ? 0 : IndependentExpected(Exit);
			Cell.Needle = IndependentNeedle(Exit, Nesting, Owner);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EVarLifetimeExit;
		using AngelscriptTest::Generate::EVarLifetimeNesting;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FVarLifetimeGenerator;
		using AngelscriptTest::Generate::FVarLifetimeParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FVarLifetimeGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-VAR-LIFETIME")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangVarLifetimeBlockEndOneLocalValue")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("VarLifetimeGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(
			LogTemp,
			Display,
			TEXT("VarLifetimeGenerator dump path=%s cases=100 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(100, CaseCount));
		ASSERT_THAT(AreEqual(100, Cases.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/VarLifetimeGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 FaultCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(
				FString::Printf(TEXT("int %s()"), *Expected.EntryName),
				Cases[Index].EntryDeclaration));
			if (Expected.bFault)
			{
				++FaultCount;
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")), Cases[Index].ExpectedException));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
			}
			else
			{
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			}
			ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
			if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}
		ASSERT_THAT(AreEqual(20, FaultCount));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int RaiseVariableLifetimeException()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));

		FVarLifetimeParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildVariableLifetimeSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FVarLifetimeParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildVariableLifetimeSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildVariableLifetimeSource(ProbeParams).IsEmpty()));
		FVarLifetimeParams InvalidEnum = DefaultParams;
		InvalidEnum.Exit = static_cast<EVarLifetimeExit>(5);
		ASSERT_THAT(IsTrue(Generator.BuildVariableLifetimeSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(AreEqual(71, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(AreEqual(100, CountOccurrences(Dump, TEXT("// Case: LANG-VAR-LIFETIME-"))));
		ASSERT_THAT(IsTrue(
			OutputFile.EndsWith(TEXT("GeneratedCases/VarLifetimeGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\VarLifetimeGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/VarLifetimeGenerator.as"),
			Dump,
			ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("VarLifetimeGenerator.as")),
			Dump,
			FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
