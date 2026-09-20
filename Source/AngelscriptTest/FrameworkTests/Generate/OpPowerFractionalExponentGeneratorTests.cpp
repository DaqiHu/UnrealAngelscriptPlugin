#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestOpPowerFractionalExponentGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	OpPowerFractionalExponentGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString ShapeNeedle;
		FString BaseNeedle;
		FString ExponentNeedle;
		int32 Expected = 0;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-OP-POWER-FRACTIONAL-EXPONENT-INT-FLOAT32-CONSTANT-FRACTIONAL_EXPONENT");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-OP-NUMERIC-BINARY-INT-ADD-INT-ZERO");

	static const TCHAR* BaseToken(const int32 Index)
	{
		static const TCHAR* const kTokens[] = {
			TEXT("INT8"), TEXT("INT16"), TEXT("INT"), TEXT("INT64"),
			TEXT("UINT8"), TEXT("UINT16"), TEXT("UINT"), TEXT("UINT64"),
			TEXT("FLOAT32"), TEXT("FLOAT64")};
		return kTokens[Index];
	}

	static const TCHAR* BaseName(const int32 Index)
	{
		static const TCHAR* const kNames[] = {
			TEXT("int8"), TEXT("int16"), TEXT("int"), TEXT("int64"),
			TEXT("uint8"), TEXT("uint16"), TEXT("uint"), TEXT("uint64"),
			TEXT("float32"), TEXT("float64")};
		return kNames[Index];
	}

	static const TCHAR* ExponentToken(const int32 Index)
	{
		return Index == 0 ? TEXT("FLOAT32") : TEXT("FLOAT64");
	}

	static const TCHAR* ExponentName(const int32 Index)
	{
		return Index == 0 ? TEXT("float32") : TEXT("float64");
	}

	static const TCHAR* ShapeToken(const int32 Index)
	{
		static const TCHAR* const kTokens[] = {
			TEXT("CONSTANT"), TEXT("MUTABLE_LVALUE"), TEXT("CONST_LVALUE"), TEXT("FUNCTION_RETURN")};
		return kTokens[Index];
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		TArray<FExpectedCell> Cells;
		Cells.SetNum(80);
		for (int32 Index = 0; Index < 80; ++Index)
		{
			const int32 Base = Index / 8;
			const int32 Exponent = (Index / 4) % 2;
			const int32 Shape = Index % 4;
			const bool bFloatBase = Base >= 8;
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-OP-POWER-FRACTIONAL-EXPONENT-%s-%s-%s-FRACTIONAL_EXPONENT"),
				BaseToken(Base),
				ExponentToken(Exponent),
				ShapeToken(Shape));
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.BaseNeedle = FString::Printf(TEXT("%s(%s)"), BaseName(Base), bFloatBase ? TEXT("4.0") : TEXT("4"));
			Cell.ExponentNeedle = FString::Printf(TEXT("%s(0.5)"), ExponentName(Exponent));
			if (Shape == 0)
			{
				Cell.ShapeNeedle = TEXT(" ** ");
			}
			else if (Shape == 1)
			{
				Cell.ShapeNeedle = FString::Printf(TEXT("%s Base = "), BaseName(Base));
			}
			else if (Shape == 2)
			{
				Cell.ShapeNeedle = FString::Printf(TEXT("const %s Base = "), BaseName(Base));
			}
			else
			{
				Cell.ShapeNeedle = FString::Printf(TEXT("PowerBaseValue%s%s()"), BaseToken(Base), ExponentToken(Exponent));
			}
			Cell.Expected = (Base == 9 || Exponent == 1) ? 106 : 105;
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EOpPowerFractionalExponentBase;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FOpPowerFractionalExponentGenerator;
		using AngelscriptTest::Generate::FOpPowerFractionalExponentParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FOpPowerFractionalExponentGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-OP-POWER-FRACTIONAL-EXPONENT")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangOpPowerFractionalExponentIntFloat32ConstantFractionalExponent")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("OpPowerFractionalExponentGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("OpPowerFractionalExponentGenerator dump path=%s cases=80 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(80, CaseCount));
		ASSERT_THAT(AreEqual(80, Cases.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/OpPowerFractionalExponentGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			ASSERT_THAT(IsTrue(Cases[Index].bLimitedObservation));
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.ShapeNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("**"))));
			if (Index % 4 != 3)
			{
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.BaseNeedle)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.ExponentNeedle)));
			}
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int ObserveNumericType(float32 Value)"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int ObserveNumericType(float64 Value)"))));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangOpPowerFractionalExponentIntFloat32ConstantFractionalExponent()"))));

		FOpPowerFractionalExponentParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildPowerSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FOpPowerFractionalExponentParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildPowerSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildPowerSource(ProbeParams).IsEmpty()));
		FOpPowerFractionalExponentParams InvalidEnum = DefaultParams;
		InvalidEnum.Base = static_cast<EOpPowerFractionalExponentBase>(10);
		ASSERT_THAT(IsTrue(Generator.BuildPowerSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(80, CountOccurrences(Dump, TEXT("// Case: LANG-OP-POWER-FRACTIONAL-EXPONENT-"))));
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
			TEXT("C:/no-such-root/OpPowerFractionalExponentGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("OpPowerFractionalExponentGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
