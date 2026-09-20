#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestFnArityTypeStressGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	FnArityTypeStressGenerator,
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
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-FN-ARITY-TYPE-STRESS-ZERO-HOMOGENEOUS_INT-GLOBAL");

	static int32 IndependentCount(const TCHAR* Arity)
	{
		if (FCString::Strcmp(Arity, TEXT("ONE")) == 0)
		{
			return 1;
		}
		if (FCString::Strcmp(Arity, TEXT("TWO")) == 0)
		{
			return 2;
		}
		if (FCString::Strcmp(Arity, TEXT("FOUR")) == 0)
		{
			return 4;
		}
		if (FCString::Strcmp(Arity, TEXT("EIGHT")) == 0)
		{
			return 8;
		}
		if (FCString::Strcmp(Arity, TEXT("SIXTEEN")) == 0)
		{
			return 16;
		}
		if (FCString::Strcmp(Arity, TEXT("THIRTY_TWO")) == 0)
		{
			return 32;
		}
		if (FCString::Strcmp(Arity, TEXT("SIXTY_FOUR")) == 0)
		{
			return 64;
		}
		return 0;
	}

	static bool IndependentIsBool(const TCHAR* Pattern, const int32 Index)
	{
		if (FCString::Strcmp(Pattern, TEXT("HOMOGENEOUS_BOOL")) == 0)
		{
			return true;
		}
		if (FCString::Strcmp(Pattern, TEXT("ALTERNATING_INT_BOOL")) == 0)
		{
			return (Index % 2) == 1;
		}
		if (FCString::Strcmp(Pattern, TEXT("ALTERNATING_BOOL_INT")) == 0)
		{
			return (Index % 2) == 0;
		}
		return false;
	}

	static int32 IndependentExpected(const TCHAR* Arity, const TCHAR* Pattern)
	{
		const int32 Count = IndependentCount(Arity);
		if (Count == 0)
		{
			return 42;
		}
		int32 Result = 0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Result += IndependentIsBool(Pattern, Index) ? ((Index % 2) == 0 ? 1 : 0) : (Index + 1);
		}
		return Result;
	}

	static FString IndependentNeedle(const TCHAR* Arity, const TCHAR* Pattern, const TCHAR* Target)
	{
		if (FCString::Strcmp(Arity, TEXT("ZERO")) == 0)
		{
			return TEXT("return 42;");
		}
		if (FCString::Strcmp(Pattern, TEXT("HOMOGENEOUS_BOOL")) == 0)
		{
			return TEXT("bool P0");
		}
		if (FCString::Strcmp(Target, TEXT("NAMESPACE_GLOBAL")) == 0)
		{
			return TEXT("namespace N");
		}
		if (FCString::Strcmp(Target, TEXT("INSTANCE_METHOD")) == 0)
		{
			return TEXT("struct FOwner");
		}
		return TEXT("int P0");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kArities[] = {
			TEXT("ZERO"), TEXT("ONE"), TEXT("TWO"), TEXT("FOUR"),
			TEXT("EIGHT"), TEXT("SIXTEEN"), TEXT("THIRTY_TWO"), TEXT("SIXTY_FOUR")};
		static const TCHAR* const kPatterns[] = {
			TEXT("HOMOGENEOUS_INT"), TEXT("HOMOGENEOUS_BOOL"),
			TEXT("ALTERNATING_INT_BOOL"), TEXT("ALTERNATING_BOOL_INT")};
		static const TCHAR* const kTargets[] = {TEXT("GLOBAL"), TEXT("NAMESPACE_GLOBAL"), TEXT("INSTANCE_METHOD")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(96);
		for (int32 Index = 0; Index < 96; ++Index)
		{
			const TCHAR* const Arity = kArities[Index / 12];
			const TCHAR* const Pattern = kPatterns[(Index / 3) % 4];
			const TCHAR* const Target = kTargets[Index % 3];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-FN-ARITY-TYPE-STRESS-%s-%s-%s"), Arity, Pattern, Target);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Expected = IndependentExpected(Arity, Pattern);
			Cell.Needle = IndependentNeedle(Arity, Pattern, Target);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EFnArityTypeStressArity;
		using AngelscriptTest::Generate::EFnArityTypeStressPattern;
		using AngelscriptTest::Generate::EFnArityTypeStressTarget;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FFnArityTypeStressGenerator;
		using AngelscriptTest::Generate::FFnArityTypeStressParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FFnArityTypeStressGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-FN-ARITY-TYPE-STRESS")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangFnArityTypeStressZeroHomogeneousIntGlobal")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("FnArityTypeStressGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("FnArityTypeStressGenerator dump path=%s cases=96 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(96, CaseCount));
		ASSERT_THAT(AreEqual(96, Cases.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/FnArityTypeStressGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			ASSERT_THAT(IsTrue(Aggregate.Contains(Expected.Needle)));
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FFnArityTypeStressParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildSource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildSource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildSource(DefaultParams).IsEmpty()));
		DefaultParams.FunctionName.Reset();
		DefaultParams.Arity = static_cast<EFnArityTypeStressArity>(8);
		ASSERT_THAT(IsTrue(Generator.BuildSource(DefaultParams).IsEmpty()));
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
		ASSERT_THAT(AreEqual(96, CountOccurrences(Dump, TEXT("// Case: LANG-FN-ARITY-TYPE-STRESS-"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/FnArityTypeStressGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("FnArityTypeStressGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
