#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestForeachStructuralMutationGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ForeachStructuralMutationGenerator,
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

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-FE-STRUCTURAL-MUTATION-ONE-STABLE-PRIMITIVE_VALUE");

	static int32 IndependentSize(const TCHAR* Size)
	{
		if (FCString::Strcmp(Size, TEXT("ONE")) == 0)
		{
			return 1;
		}
		if (FCString::Strcmp(Size, TEXT("TWO")) == 0)
		{
			return 2;
		}
		return 4;
	}

	static int32 IndependentExpected(const TCHAR* Size, const TCHAR* Mutation)
	{
		const int32 Count = IndependentSize(Size);
		if (FCString::Strcmp(Mutation, TEXT("STABLE")) == 0)
		{
			return Count;
		}
		if (FCString::Strcmp(Mutation, TEXT("SHRINK_FIRST")) == 0
			|| FCString::Strcmp(Mutation, TEXT("CLEAR_AFTER_FIRST")) == 0)
		{
			return Count < 1 ? Count : 1;
		}
		return Count < 2 ? Count : 2;
	}

	static FString IndependentNeedle(const TCHAR* Mutation)
	{
		if (FCString::Strcmp(Mutation, TEXT("SHRINK_FIRST")) == 0)
		{
			return TEXT("Range.Native.Count = 1;");
		}
		if (FCString::Strcmp(Mutation, TEXT("SHRINK_MIDDLE")) == 0)
		{
			return TEXT("Range.Native.Count = 2;");
		}
		if (FCString::Strcmp(Mutation, TEXT("CLEAR_AFTER_FIRST")) == 0)
		{
			return TEXT("Range.Native.Count = 0;");
		}
		return TEXT("Trace += 1;");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kSizes[] = {TEXT("ONE"), TEXT("TWO"), TEXT("MANY")};
		static const TCHAR* const kMutations[] = {
			TEXT("STABLE"), TEXT("SHRINK_FIRST"), TEXT("SHRINK_MIDDLE"), TEXT("CLEAR_AFTER_FIRST")};
		static const TCHAR* const kElements[] = {
			TEXT("PRIMITIVE_VALUE"), TEXT("VALUE_OBJECT_COPY"), TEXT("VALUE_OBJECT_CONST_REF")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(36);
		for (int32 Index = 0; Index < 36; ++Index)
		{
			const TCHAR* const Size = kSizes[Index / 12];
			const TCHAR* const Mutation = kMutations[(Index / 3) % 4];
			const TCHAR* const Element = kElements[Index % 3];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-FE-STRUCTURAL-MUTATION-%s-%s-%s"), Size, Mutation, Element);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Needle = IndependentNeedle(Mutation);
			Cell.Expected = IndependentExpected(Size, Mutation);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EForeachMutationSize;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FForeachStructuralMutationGenerator;
		using AngelscriptTest::Generate::FForeachStructuralMutationParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FForeachStructuralMutationGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-FE-STRUCTURAL-MUTATION")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangFeStructuralMutationOneStablePrimitiveValue")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ForeachStructuralMutationGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ForeachStructuralMutationGenerator dump path=%s cases=36 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(36, CaseCount));
		ASSERT_THAT(AreEqual(36, Cases.Num()));

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
		}

		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangFeStructuralMutationOneStablePrimitiveValue()"))));

		FForeachStructuralMutationParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildStructuralMutationSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FForeachStructuralMutationParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildStructuralMutationSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/ForeachStructuralMutationGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
			ASSERT_THAT(AreEqual(1, CountOccurrences(Dump, FString::Printf(TEXT("// Case: %s\n"), *Expected.CaseId))));
			if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FForeachStructuralMutationParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildStructuralMutationSource(InvalidName).IsEmpty()));
		FForeachStructuralMutationParams InvalidEnum = DefaultParams;
		InvalidEnum.Size = static_cast<EForeachMutationSize>(3);
		ASSERT_THAT(IsTrue(Generator.BuildStructuralMutationSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-FE-TRANSFER-LIFETIME-COMPLETE-SINGLE-PRIMITIVE_VALUE")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(36, CountOccurrences(Dump, TEXT("// Case: "))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/ForeachStructuralMutationGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ForeachStructuralMutationGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
