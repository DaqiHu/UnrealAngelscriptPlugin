#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestFnTypedDefaultsGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	FnTypedDefaultsGenerator,
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

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-FN-TYPED-DEFAULTS-ONE-HOMOGENEOUS_INT-GLOBAL-OMIT0");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static bool IndependentIsBool(const TCHAR* Type, const int32 Index)
	{
		if (FCString::Strcmp(Type, TEXT("HOMOGENEOUS_BOOL")) == 0)
		{
			return true;
		}
		if (FCString::Strcmp(Type, TEXT("ALTERNATING_INT_BOOL")) == 0)
		{
			return (Index % 2) == 1;
		}
		if (FCString::Strcmp(Type, TEXT("ALTERNATING_BOOL_INT")) == 0)
		{
			return (Index % 2) == 0;
		}
		return false;
	}

	static int32 IndependentDefault(const TCHAR* Type, const int32 Index)
	{
		if (IndependentIsBool(Type, Index))
		{
			return Index == 0 || Index == 2 ? 1 : 0;
		}
		static const int32 IntDefaults[] = {100, 12, 16, -7};
		return IntDefaults[Index];
	}

	static int32 IndependentExplicit(const TCHAR* Type, const int32 Index)
	{
		if (IndependentIsBool(Type, Index))
		{
			return Index % 2 == 1 ? 1 : 0;
		}
		return Index + 1;
	}

	static int32 IndependentExpected(const TCHAR* Type, const int32 Count, const int32 Omitted)
	{
		int32 Result = 0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Result += Index >= Count - Omitted
				? IndependentDefault(Type, Index)
				: IndependentExplicit(Type, Index);
		}
		return Result;
	}

	static FString IndependentNeedle(const TCHAR* Type, const TCHAR* Target, const int32 Count, const int32 Omitted)
	{
		if (FCString::Strcmp(Target, TEXT("NAMESPACE_GLOBAL")) == 0)
		{
			return TEXT("namespace N");
		}
		if (FCString::Strcmp(Target, TEXT("INSTANCE_METHOD")) == 0)
		{
			return TEXT("struct FOwner");
		}
		if (FCString::Strcmp(Type, TEXT("HOMOGENEOUS_BOOL")) == 0)
		{
			return TEXT("P0 = true");
		}
		if (FCString::Strcmp(Type, TEXT("ALTERNATING_BOOL_INT")) == 0)
		{
			return TEXT("P0 = true");
		}
		if (FCString::Strcmp(Type, TEXT("ALTERNATING_INT_BOOL")) == 0 && Count > 1)
		{
			return TEXT("P1 = false");
		}
		if (Count == 4)
		{
			return TEXT("P3 = -7");
		}
		if (Omitted == Count)
		{
			return TEXT("();");
		}
		return TEXT("P0 = 100");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kArities[] = {TEXT("ONE"), TEXT("TWO"), TEXT("THREE"), TEXT("FOUR")};
		static const TCHAR* const kTypes[] = {
			TEXT("HOMOGENEOUS_INT"), TEXT("HOMOGENEOUS_BOOL"),
			TEXT("ALTERNATING_INT_BOOL"), TEXT("ALTERNATING_BOOL_INT")};
		static const TCHAR* const kTargets[] = {TEXT("GLOBAL"), TEXT("NAMESPACE_GLOBAL"), TEXT("INSTANCE_METHOD")};

		TArray<FExpectedCell> Cells;
		Cells.Reserve(168);
		for (int32 ArityIndex = 0; ArityIndex < 4; ++ArityIndex)
		{
			const int32 Count = ArityIndex + 1;
			for (int32 TypeIndex = 0; TypeIndex < 4; ++TypeIndex)
			{
				for (int32 TargetIndex = 0; TargetIndex < 3; ++TargetIndex)
				{
					for (int32 Omitted = 0; Omitted <= Count; ++Omitted)
					{
						FExpectedCell Cell;
						Cell.CaseId = FString::Printf(
							TEXT("LANG-FN-TYPED-DEFAULTS-%s-%s-%s-OMIT%d"),
							kArities[ArityIndex],
							kTypes[TypeIndex],
							kTargets[TargetIndex],
							Omitted);
						Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
						Cell.Expected = IndependentExpected(kTypes[TypeIndex], Count, Omitted);
						Cell.Needle = IndependentNeedle(kTypes[TypeIndex], kTargets[TargetIndex], Count, Omitted);
						Cells.Add(MoveTemp(Cell));
					}
				}
			}
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EFnTypedDefaultsTarget;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FFnTypedDefaultsGenerator;
		using AngelscriptTest::Generate::FFnTypedDefaultsParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FFnTypedDefaultsGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-FN-TYPED-DEFAULTS")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangFnTypedDefaultsOneHomogeneousIntGlobalOmit0")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("FnTypedDefaultsGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("FnTypedDefaultsGenerator dump path=%s cases=168 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(168, ExpectedCells.Num()));
		ASSERT_THAT(AreEqual(168, CaseCount));
		ASSERT_THAT(AreEqual(168, Cases.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/FnTypedDefaultsGenerator/RepresentativeSources.as"));
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
			ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
			const FString Isolated = Generator.BuildCaseSource(Expected.CaseId);
			ASSERT_THAT(IsTrue(Isolated.Contains(Expected.Needle)));
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangFnTypedDefaultsOneHomogeneousIntGlobalOmit0()"))));

		FFnTypedDefaultsParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FFnTypedDefaultsParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(Generator.BuildSource(ProbeParams).Contains(TEXT("int Entry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildSource(ProbeParams).IsEmpty()));
		FFnTypedDefaultsParams InvalidEnum = DefaultParams;
		InvalidEnum.Target = static_cast<EFnTypedDefaultsTarget>(3);
		ASSERT_THAT(IsTrue(Generator.BuildSource(InvalidEnum).IsEmpty()));
		FFnTypedDefaultsParams InvalidOmit = DefaultParams;
		InvalidOmit.OmittedCount = 5;
		ASSERT_THAT(IsTrue(Generator.BuildSource(InvalidOmit).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(ExpectedCells[0].Expected, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(168, CountOccurrences(Dump, TEXT("// Case: LANG-FN-TYPED-DEFAULTS-"))));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			ASSERT_THAT(IsTrue(Dump.Contains(FString::Printf(TEXT("// Case: %s"), *Expected.CaseId))));
			FString DumpBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Dump, Expected.EntryName, DumpBody)));
			FString AggregateBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, AggregateBody)));
			ASSERT_THAT(AreEqual(AggregateBody, DumpBody));
		}
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/FnTypedDefaultsGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("FnTypedDefaultsGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
