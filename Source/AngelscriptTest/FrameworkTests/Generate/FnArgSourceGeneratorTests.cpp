#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestFnArgSourceGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	FnArgSourceGenerator,
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
		bool bHost = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-FN-ARG-SOURCE-VALUE-LITERAL");

	static bool IndependentReject(const TCHAR* Direction, const TCHAR* Source)
	{
		if (FCString::Strcmp(Source, TEXT("DERIVED_VIEW")) == 0)
		{
			return FCString::Strcmp(Direction, TEXT("VALUE")) != 0;
		}
		if (FCString::Strcmp(Direction, TEXT("VALUE")) == 0 || FCString::Strcmp(Direction, TEXT("IN")) == 0)
		{
			return false;
		}
		return FCString::Strcmp(Source, TEXT("LOCAL_LVALUE")) != 0
			&& FCString::Strcmp(Source, TEXT("FIELD")) != 0
			&& FCString::Strcmp(Source, TEXT("BASE_VIEW")) != 0;
	}

	static bool IndependentFault(const TCHAR* Source)
	{
		return FCString::Strcmp(Source, TEXT("BASE_VIEW")) == 0
			|| FCString::Strcmp(Source, TEXT("DERIVED_VIEW")) == 0;
	}

	static int32 IndependentExpected(const TCHAR* Direction, const TCHAR* Source)
	{
		if (IndependentReject(Direction, Source) || IndependentFault(Source))
		{
			return 0;
		}
		if (FCString::Strcmp(Direction, TEXT("OUT")) == 0 || FCString::Strcmp(Direction, TEXT("INOUT")) == 0)
		{
			return 18;
		}
		return FCString::Strcmp(Source, TEXT("NULL")) == 0 ? 1 : 7;
	}

	static FString IndependentNeedle(const TCHAR* Source)
	{
		if (FCString::Strcmp(Source, TEXT("LITERAL")) == 0)
		{
			return TEXT("(7)");
		}
		if (FCString::Strcmp(Source, TEXT("LOCAL_LVALUE")) == 0)
		{
			return TEXT("int Value = 7;");
		}
		if (FCString::Strcmp(Source, TEXT("CONST_LOCAL")) == 0)
		{
			return TEXT("const int Value = 7;");
		}
		if (FCString::Strcmp(Source, TEXT("GLOBAL_CONST")) == 0)
		{
			return TEXT("GlobalConstValue");
		}
		if (FCString::Strcmp(Source, TEXT("FIELD")) == 0)
		{
			return TEXT("Holder.Value");
		}
		if (FCString::Strcmp(Source, TEXT("FUNCTION_RETURN")) == 0)
		{
			return TEXT("ProvideValue()");
		}
		if (FCString::Strcmp(Source, TEXT("ARITHMETIC_EXPRESSION")) == 0)
		{
			return TEXT("3 + 4");
		}
		if (FCString::Strcmp(Source, TEXT("CONDITIONAL_EXPRESSION")) == 0)
		{
			return TEXT("true ? 7 : 8");
		}
		if (FCString::Strcmp(Source, TEXT("NULL")) == 0)
		{
			return TEXT("FNativeCaseReference");
		}
		if (FCString::Strcmp(Source, TEXT("BASE_VIEW")) == 0)
		{
			return TEXT("FBase Value;");
		}
		return TEXT("FDerived Value;");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kDirections[] = {TEXT("VALUE"), TEXT("IN"), TEXT("OUT"), TEXT("INOUT")};
		static const TCHAR* const kSources[] = {
			TEXT("LITERAL"), TEXT("LOCAL_LVALUE"), TEXT("CONST_LOCAL"), TEXT("GLOBAL_CONST"),
			TEXT("FIELD"), TEXT("FUNCTION_RETURN"), TEXT("ARITHMETIC_EXPRESSION"),
			TEXT("CONDITIONAL_EXPRESSION"), TEXT("NULL"), TEXT("BASE_VIEW"), TEXT("DERIVED_VIEW")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(44);
		for (int32 Index = 0; Index < 44; ++Index)
		{
			const TCHAR* const Direction = kDirections[Index / 11];
			const TCHAR* const Source = kSources[Index % 11];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-FN-ARG-SOURCE-%s-%s"), Direction, Source);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Direction, Source);
			Cell.bFault = !Cell.bReject && IndependentFault(Source);
			Cell.bHost = !Cell.bReject && !Cell.bFault && FCString::Strcmp(Source, TEXT("NULL")) == 0;
			Cell.Expected = IndependentExpected(Direction, Source);
			Cell.Needle = IndependentNeedle(Source);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EFnArgSourceDirection;
		using AngelscriptTest::Generate::EFnArgSourceSource;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FFnArgSourceGenerator;
		using AngelscriptTest::Generate::FFnArgSourceParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FFnArgSourceGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-FN-ARG-SOURCE")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangFnArgSourceValueLiteral")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("FnArgSourceGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("FnArgSourceGenerator dump path=%s cases=44 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(27, CaseCount));
		ASSERT_THAT(AreEqual(44, Cases.Num()));
		ASSERT_THAT(AreEqual(17, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("const int GlobalConstValue = 7;"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("class FDerived : FBase"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/FnArgSourceGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

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
			}
			else if (Expected.bFault)
			{
				++FaultCount;
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(AreEqual(FString(TEXT("Null pointer access")), Cases[Index].ExpectedException));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
			}
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				if (Expected.bHost)
				{
					ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
					ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("FNativeCaseReference"))));
				}
				else
				{
					ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
				}
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
		ASSERT_THAT(AreEqual(17, RejectCount));
		ASSERT_THAT(AreEqual(5, FaultCount));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangFnArgSourceValueLiteral()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangFnArgSourceValueLiteral()"))));

		FFnArgSourceParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildArgumentSource(DefaultParams).Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsTrue(Generator.BuildArgumentSource(DefaultParams).Contains(TEXT("return Probe(7);"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildArgumentSource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildArgumentSource(DefaultParams).IsEmpty()));
		DefaultParams.FunctionName.Reset();
		DefaultParams.Direction = EFnArgSourceDirection::Out;
		ASSERT_THAT(IsTrue(Generator.BuildArgumentSource(DefaultParams).IsEmpty()));
		DefaultParams.Direction = static_cast<EFnArgSourceDirection>(4);
		ASSERT_THAT(IsTrue(Generator.BuildArgumentSource(DefaultParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(7, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(44, CountOccurrences(Dump, TEXT("// Case: LANG-FN-ARG-SOURCE-"))));
		ASSERT_THAT(AreEqual(17, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/FnArgSourceGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("FnArgSourceGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
