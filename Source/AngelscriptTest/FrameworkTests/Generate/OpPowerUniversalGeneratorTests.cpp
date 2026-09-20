#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestOpPowerUniversalGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	OpPowerUniversalGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString OperandNeedle;
		int32 Expected = 0;
		bool bReject = false;
		bool bFault = false;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-OP-POWER-UNIVERSAL-INT-INT-CONSTANT-ZERO_EXPONENT");
	static constexpr const TCHAR* ZeroCaseId =
		TEXT("LANG-OP-POWER-UNIVERSAL-INT8-UINT8-CONST_LVALUE-OVERFLOW");

	static const TCHAR* TypeToken(const int32 TypeIndex)
	{
		switch (TypeIndex)
		{
		case 0:
			return TEXT("INT8");
		case 1:
			return TEXT("INT16");
		case 2:
			return TEXT("INT");
		case 3:
			return TEXT("INT64");
		case 4:
			return TEXT("UINT8");
		case 5:
			return TEXT("UINT16");
		case 6:
			return TEXT("UINT");
		case 7:
			return TEXT("UINT64");
		case 8:
			return TEXT("FLOAT32");
		default:
			return TEXT("FLOAT64");
		}
	}

	static bool IsFloat(const int32 TypeIndex)
	{
		return TypeIndex >= 8;
	}

	static bool IsUnsigned(const int32 TypeIndex)
	{
		return TypeIndex >= 4 && TypeIndex <= 7;
	}

	static bool IsInteger(const int32 TypeIndex)
	{
		return TypeIndex <= 7;
	}

	static bool IndependentIntegerResult(const int32 Base, const int32 Exp)
	{
		return !IsFloat(Base) && !IsFloat(Exp);
	}

	static bool IndependentMixedSigns(const int32 Base, const int32 Exp)
	{
		return IsInteger(Base) && IsInteger(Exp) && (IsUnsigned(Base) != IsUnsigned(Exp));
	}

	static bool IndependentReject(const int32 Base, const int32 Exp, const int32 Shape, const int32 Scenario)
	{
		const bool bCompileTime = Shape == 0 || Shape == 2;
		if (Scenario == 3 && bCompileTime)
		{
			return Shape == 0 || !IndependentMixedSigns(Base, Exp);
		}
		return !bCompileTime && IndependentIntegerResult(Base, Exp);
	}

	static bool IndependentFault(const int32 Base, const int32 Exp, const int32 Shape, const int32 Scenario)
	{
		const bool bCompileTime = Shape == 0 || Shape == 2;
		return Scenario == 3 && !bCompileTime && !IndependentIntegerResult(Base, Exp);
	}

	static int32 IndependentExpected(const int32 Base, const int32 Exp, const int32 Shape, const int32 Scenario)
	{
		if (IndependentReject(Base, Exp, Shape, Scenario) || IndependentFault(Base, Exp, Shape, Scenario))
		{
			return 0;
		}
		if (Scenario == 3 && Shape == 2 && IndependentMixedSigns(Base, Exp))
		{
			return 0;
		}
		return Scenario == 1 ? 2 : 1;
	}

	static FString IndependentNeedle(const int32 Shape, const int32 Scenario)
	{
		if (Shape == 3)
		{
			return TEXT("PowerBase_");
		}
		if (Shape == 0)
		{
			return Scenario == 1 ? TEXT(" ** ") : TEXT(" ** ");
		}
		return TEXT("Base ** Exponent; // POWER_CAUSE");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kShapes[] = {
			TEXT("CONSTANT"), TEXT("MUTABLE_LVALUE"), TEXT("CONST_LVALUE"), TEXT("FUNCTION_RETURN")};
		static const TCHAR* const kScenarios[] = {
			TEXT("ZERO_EXPONENT"), TEXT("ONE_EXPONENT"), TEXT("NEAR_LIMIT"), TEXT("OVERFLOW")};
		TArray<FExpectedCell> Cells;
		Cells.SetNum(1600);
		for (int32 Index = 0; Index < 1600; ++Index)
		{
			const int32 Base = Index / 160;
			const int32 Exp = (Index / 16) % 10;
			const int32 Shape = (Index / 4) % 4;
			const int32 Scenario = Index % 4;
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-OP-POWER-UNIVERSAL-%s-%s-%s-%s"),
				TypeToken(Base),
				TypeToken(Exp),
				kShapes[Shape],
				kScenarios[Scenario]);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Base, Exp, Shape, Scenario);
			Cell.bFault = IndependentFault(Base, Exp, Shape, Scenario);
			Cell.Expected = IndependentExpected(Base, Exp, Shape, Scenario);
			Cell.OperandNeedle = IndependentNeedle(Shape, Scenario);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EOpPowerUniversalBase;
		using AngelscriptTest::Generate::EOpPowerUniversalExponent;
		using AngelscriptTest::Generate::EOpPowerUniversalScenario;
		using AngelscriptTest::Generate::EOpPowerUniversalShape;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FOpPowerUniversalGenerator;
		using AngelscriptTest::Generate::FOpPowerUniversalParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FOpPowerUniversalGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-OP-POWER-UNIVERSAL")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangOpPowerUniversalIntIntConstantZeroExponent")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("OpPowerUniversalGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("OpPowerUniversalGenerator dump path=%s cases=1600 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(920, CaseCount));
		ASSERT_THAT(AreEqual(1600, Cases.Num()));
		ASSERT_THAT(AreEqual(680, RejectIds.Num()));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/OpPowerUniversalGenerator/RepresentativeSources.as"));
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
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(RejectIds.Contains(Expected.CaseId)));
				const FString RejectSource = Generator.BuildRejectSource(Expected.CaseId);
				ASSERT_THAT(IsTrue(RejectSource.Contains(TEXT("**"))));
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
			}
			else if (Expected.bFault)
			{
				++FaultCount;
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(AreEqual(FString(TEXT("Overflow in exponent operation")), Cases[Index].ExpectedException));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("**"))));
			}
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("**"))));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.OperandNeedle)));
			}
			if (Representative.Contains(Expected.EntryName))
			{
				const FString CaseSource = Expected.bReject
					? Generator.BuildRejectSource(Expected.CaseId)
					: Generator.BuildCaseSource(Expected.CaseId);
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}
		ASSERT_THAT(AreEqual(680, RejectCount));
		ASSERT_THAT(AreEqual(72, FaultCount));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangOpPowerUniversalIntIntConstantZeroExponent()"))));

		FOpPowerUniversalParams DefaultParams;
		DefaultParams.Base = EOpPowerUniversalBase::Int;
		DefaultParams.Exponent = EOpPowerUniversalExponent::Int;
		ASSERT_THAT(IsTrue(Generator.BuildPowerSource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildPowerSource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildPowerSource(DefaultParams).IsEmpty()));
		FOpPowerUniversalParams InvalidEnum;
		InvalidEnum.Base = static_cast<EOpPowerUniversalBase>(10);
		ASSERT_THAT(IsTrue(Generator.BuildPowerSource(InvalidEnum).IsEmpty()));
		FOpPowerUniversalParams RejectParams;
		RejectParams.Scenario = EOpPowerUniversalScenario::Overflow;
		ASSERT_THAT(IsTrue(Generator.BuildPowerSource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(ZeroCaseId)));
		bool bFoundZeroCase = false;
		for (int32 Index = 0; Index < Cases.Num(); ++Index)
		{
			if (Cases[Index].CaseId == ZeroCaseId)
			{
				bFoundZeroCase = true;
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Cases[Index].ExpectedReturn.Get(MIN_int32)));
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
		ASSERT_THAT(AreEqual(1600, CountOccurrences(Dump, TEXT("// Case: LANG-OP-POWER-UNIVERSAL-"))));
		ASSERT_THAT(AreEqual(680, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/OpPowerUniversalGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("OpPowerUniversalGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
