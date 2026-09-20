#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestFnSignatureShapeGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	FnSignatureShapeGenerator,
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

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-FN-SIGNATURE-SHAPE-GLOBAL-ALL_VALUE-HOMOGENEOUS_INT-ONE");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static int32 IndependentArity(const TCHAR* Arity)
	{
		if (FCString::Strcmp(Arity, TEXT("TWO")) == 0)
		{
			return 2;
		}
		if (FCString::Strcmp(Arity, TEXT("THREE")) == 0)
		{
			return 3;
		}
		if (FCString::Strcmp(Arity, TEXT("FOUR")) == 0)
		{
			return 4;
		}
		return 1;
	}

	static bool IndependentWrites(const TCHAR* Direction)
	{
		return FCString::Strcmp(Direction, TEXT("ALL_OUT")) == 0
			|| FCString::Strcmp(Direction, TEXT("ALTERNATING_INOUT_OUT")) == 0;
	}

	static bool IndependentReads(const TCHAR* Direction, const int32 Index)
	{
		if (FCString::Strcmp(Direction, TEXT("ALL_VALUE")) == 0
			|| FCString::Strcmp(Direction, TEXT("ALL_IN")) == 0)
		{
			return true;
		}
		return FCString::Strcmp(Direction, TEXT("ALTERNATING_INOUT_OUT")) == 0 && (Index % 2) == 0;
	}

	static int32 IndependentExpected(const TCHAR* Direction, const TCHAR* Arity)
	{
		int32 Result = 0;
		const int32 Count = IndependentArity(Arity);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (!IndependentWrites(Direction))
			{
				Result += Index + 3;
			}
			else if (IndependentReads(Direction, Index))
			{
				Result += Index + 8;
			}
			else
			{
				Result += 100 + Index;
			}
		}
		return (Result * 1000) + 1;
	}

	static FString IndependentNeedle(const TCHAR* Target, const TCHAR* Direction, const TCHAR* Type, const TCHAR* Arity)
	{
		if (FCString::Strcmp(Target, TEXT("NAMESPACE_GLOBAL")) == 0)
		{
			return TEXT("namespace N");
		}
		if (FCString::Strcmp(Target, TEXT("INSTANCE_METHOD")) == 0)
		{
			return TEXT("struct FOwner");
		}
		if (FCString::Strcmp(Direction, TEXT("ALL_IN")) == 0)
		{
			return TEXT("& in");
		}
		if (FCString::Strcmp(Direction, TEXT("ALL_OUT")) == 0)
		{
			return TEXT("& out");
		}
		if (FCString::Strcmp(Direction, TEXT("ALTERNATING_INOUT_OUT")) == 0)
		{
			return TEXT("& inout");
		}
		if (FCString::Strcmp(Type, TEXT("HOMOGENEOUS_FLOAT")) == 0
			|| FCString::Strcmp(Type, TEXT("ALTERNATING_FLOAT_INT")) == 0)
		{
			return TEXT("float P0");
		}
		if (FCString::Strcmp(Arity, TEXT("FOUR")) == 0)
		{
			return TEXT("P3");
		}
		return TEXT("int P0");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kTargets[] = {TEXT("GLOBAL"), TEXT("NAMESPACE_GLOBAL"), TEXT("INSTANCE_METHOD")};
		static const TCHAR* const kDirections[] = {
			TEXT("ALL_VALUE"), TEXT("ALL_IN"), TEXT("ALL_OUT"), TEXT("ALTERNATING_INOUT_OUT")};
		static const TCHAR* const kTypes[] = {
			TEXT("HOMOGENEOUS_INT"), TEXT("HOMOGENEOUS_FLOAT"),
			TEXT("ALTERNATING_INT_FLOAT"), TEXT("ALTERNATING_FLOAT_INT")};
		static const TCHAR* const kArities[] = {TEXT("ONE"), TEXT("TWO"), TEXT("THREE"), TEXT("FOUR")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(192);
		for (int32 Index = 0; Index < 192; ++Index)
		{
			const TCHAR* const Target = kTargets[Index / 64];
			const TCHAR* const Direction = kDirections[(Index / 16) % 4];
			const TCHAR* const Type = kTypes[(Index / 4) % 4];
			const TCHAR* const Arity = kArities[Index % 4];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-FN-SIGNATURE-SHAPE-%s-%s-%s-%s"), Target, Direction, Type, Arity);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Expected = IndependentExpected(Direction, Arity);
			Cell.Needle = IndependentNeedle(Target, Direction, Type, Arity);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EFnSignatureShapeTarget;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FFnSignatureShapeGenerator;
		using AngelscriptTest::Generate::FFnSignatureShapeParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FFnSignatureShapeGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-FN-SIGNATURE-SHAPE")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangFnSignatureShapeGlobalAllValueHomogeneousIntOne")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("FnSignatureShapeGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("FnSignatureShapeGenerator dump path=%s cases=192 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(192, CaseCount));
		ASSERT_THAT(AreEqual(192, Cases.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/FnSignatureShapeGenerator/RepresentativeSources.as"));
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
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangFnSignatureShapeGlobalAllValueHomogeneousIntOne()"))));

		FFnSignatureShapeParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FFnSignatureShapeParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(Generator.BuildSource(ProbeParams).Contains(TEXT("int Entry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildSource(ProbeParams).IsEmpty()));
		FFnSignatureShapeParams InvalidEnum = DefaultParams;
		InvalidEnum.Target = static_cast<EFnSignatureShapeTarget>(3);
		ASSERT_THAT(IsTrue(Generator.BuildSource(InvalidEnum).IsEmpty()));
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
		ASSERT_THAT(AreEqual(192, CountOccurrences(Dump, TEXT("// Case: LANG-FN-SIGNATURE-SHAPE-"))));
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
			TEXT("C:/no-such-root/FnSignatureShapeGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("FnSignatureShapeGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
