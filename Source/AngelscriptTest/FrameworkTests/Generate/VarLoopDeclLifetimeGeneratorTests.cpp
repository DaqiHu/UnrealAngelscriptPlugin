#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestVarLoopDeclLifetimeGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	VarLoopDeclLifetimeGenerator,
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

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-VAR-LOOP-DECL-LIFETIME-NORMAL-ZERO-FOR_INITIALIZER-SCRIPT_VALUE");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static int32 IndependentCount(const TCHAR* Iteration)
	{
		if (FCString::Strcmp(Iteration, TEXT("ONE")) == 0)
		{
			return 1;
		}
		if (FCString::Strcmp(Iteration, TEXT("THREE")) == 0)
		{
			return 3;
		}
		if (FCString::Strcmp(Iteration, TEXT("EIGHT")) == 0)
		{
			return 8;
		}
		return 0;
	}

	static int32 IndependentReturnBase(const TCHAR* Exit)
	{
		if (FCString::Strcmp(Exit, TEXT("NORMAL")) == 0)
		{
			return 7000;
		}
		if (FCString::Strcmp(Exit, TEXT("BREAK")) == 0)
		{
			return 7100;
		}
		if (FCString::Strcmp(Exit, TEXT("CONTINUE")) == 0)
		{
			return 7200;
		}
		if (FCString::Strcmp(Exit, TEXT("RETURN")) == 0)
		{
			return 7300;
		}
		return 7400;
	}

	static int32 IndependentBodyCount(const TCHAR* Exit, const TCHAR* Iteration, const TCHAR* Placement)
	{
		const int32 Count = IndependentCount(Iteration);
		const int32 FullCount = FCString::Strcmp(Placement, TEXT("DO_BODY")) == 0 && Count < 1 ? 1 : Count;
		if (FullCount > 0
			&& (FCString::Strcmp(Exit, TEXT("BREAK")) == 0
				|| FCString::Strcmp(Exit, TEXT("RETURN")) == 0
				|| FCString::Strcmp(Exit, TEXT("EXCEPTION")) == 0))
		{
			return 1;
		}
		return FullCount;
	}

	static FString IndependentNeedle(const TCHAR* Exit, const TCHAR* Placement, const TCHAR* Type)
	{
		if (FCString::Strcmp(Exit, TEXT("EXCEPTION")) == 0)
		{
			return TEXT("RaiseVariableLoopException");
		}
		if (FCString::Strcmp(Placement, TEXT("WHILE_BODY")) == 0)
		{
			return TEXT("while (Index < Limit)");
		}
		if (FCString::Strcmp(Placement, TEXT("DO_BODY")) == 0)
		{
			return TEXT("\tdo");
		}
		if (FCString::Strcmp(Placement, TEXT("FOR_INITIALIZER")) == 0)
		{
			return FCString::Strcmp(Type, TEXT("SCRIPT_VALUE")) == 0
				? TEXT("for (FScriptLoopValue Owner(100);")
				: TEXT("for (FNativeCaseValue Owner(100);");
		}
		if (FCString::Strcmp(Placement, TEXT("NESTED_BODY")) == 0)
		{
			return TEXT("++BodyCount;");
		}
		if (FCString::Strcmp(Exit, TEXT("BREAK")) == 0)
		{
			return TEXT("break;");
		}
		if (FCString::Strcmp(Exit, TEXT("RETURN")) == 0)
		{
			return TEXT("return 7300 + BodyCount;");
		}
		return TEXT("AdvanceVariableLoop");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kExits[] = {
			TEXT("NORMAL"), TEXT("BREAK"), TEXT("CONTINUE"), TEXT("RETURN"), TEXT("EXCEPTION")};
		static const TCHAR* const kIterations[] = {TEXT("ZERO"), TEXT("ONE"), TEXT("THREE"), TEXT("EIGHT")};
		static const TCHAR* const kPlacements[] = {
			TEXT("FOR_INITIALIZER"), TEXT("FOR_BODY"), TEXT("WHILE_BODY"), TEXT("DO_BODY"), TEXT("NESTED_BODY")};
		static const TCHAR* const kTypes[] = {TEXT("SCRIPT_VALUE"), TEXT("NATIVE_VALUE")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(200);
		for (int32 Index = 0; Index < 200; ++Index)
		{
			const TCHAR* const Exit = kExits[Index / 40];
			const TCHAR* const Iteration = kIterations[(Index / 10) % 4];
			const TCHAR* const Placement = kPlacements[(Index / 2) % 5];
			const TCHAR* const Type = kTypes[Index % 2];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-VAR-LOOP-DECL-LIFETIME-%s-%s-%s-%s"),
				Exit,
				Iteration,
				Placement,
				Type);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			const int32 BodyCount = IndependentBodyCount(Exit, Iteration, Placement);
			Cell.bFault = FCString::Strcmp(Exit, TEXT("EXCEPTION")) == 0 && BodyCount > 0;
			Cell.Expected = Cell.bFault ? 0 : IndependentReturnBase(Exit) + BodyCount;
			Cell.Needle = IndependentNeedle(Exit, Placement, Type);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EVarLoopExit;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FVarLoopDeclLifetimeGenerator;
		using AngelscriptTest::Generate::FVarLoopDeclLifetimeParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FVarLoopDeclLifetimeGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-VAR-LOOP-DECL-LIFETIME")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangVarLoopDeclLifetimeNormalZeroForInitializerScriptValue")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("VarLoopDeclLifetimeGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(
			LogTemp,
			Display,
			TEXT("VarLoopDeclLifetimeGenerator dump path=%s cases=200 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(200, CaseCount));
		ASSERT_THAT(AreEqual(200, Cases.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/VarLoopDeclLifetimeGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 FaultCount = 0;
		int32 ExceptionZeroNonDo = 0;
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
				if (Expected.Expected == 7400)
				{
					++ExceptionZeroNonDo;
				}
			}
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
		ASSERT_THAT(AreEqual(32, FaultCount));
		ASSERT_THAT(AreEqual(8, ExceptionZeroNonDo));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int RaiseVariableLoopException()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));

		FVarLoopDeclLifetimeParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildVariableLoopSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FVarLoopDeclLifetimeParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildVariableLoopSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildVariableLoopSource(ProbeParams).IsEmpty()));
		FVarLoopDeclLifetimeParams InvalidEnum = DefaultParams;
		InvalidEnum.Exit = static_cast<EVarLoopExit>(5);
		ASSERT_THAT(IsTrue(Generator.BuildVariableLoopSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(AreEqual(7000, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(AreEqual(200, CountOccurrences(Dump, TEXT("// Case: LANG-VAR-LOOP-DECL-LIFETIME-"))));
		ASSERT_THAT(IsTrue(
			OutputFile.EndsWith(TEXT("GeneratedCases/VarLoopDeclLifetimeGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\VarLoopDeclLifetimeGenerator.as"))));
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
			TEXT("C:/no-such-root/VarLoopDeclLifetimeGenerator.as"),
			Dump,
			ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("VarLoopDeclLifetimeGenerator.as")),
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
