#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestOpPowerNegativeExponentGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	OpPowerNegativeExponentGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString BaseType;
		FString ExpType;
		FString ShapeNeedle;
		int32 Expected = 0;
		bool bReject = false;
		bool bLimited = false;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-OP-POWER-NEGATIVE-EXPONENT-INT-INT-CONSTANT-NEGATIVE_EXPONENT");
	static constexpr const TCHAR* ZeroCaseId =
		TEXT("LANG-OP-POWER-NEGATIVE-EXPONENT-INT-INT-CONSTANT-NEGATIVE_EXPONENT");

	static const TCHAR* TypeName(const int32 TypeIndex, const bool bExponent)
	{
		switch (TypeIndex)
		{
		case 0:
			return TEXT("int8");
		case 1:
			return TEXT("int16");
		case 2:
			return TEXT("int");
		case 3:
			return TEXT("int64");
		case 4:
			return bExponent ? TEXT("float32") : TEXT("uint8");
		case 5:
			return bExponent ? TEXT("float64") : TEXT("uint16");
		case 6:
			return TEXT("uint");
		case 7:
			return TEXT("uint64");
		case 8:
			return TEXT("float32");
		default:
			return TEXT("float64");
		}
	}

	static const TCHAR* TypeToken(const int32 TypeIndex, const bool bExponent)
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
			return bExponent ? TEXT("FLOAT32") : TEXT("UINT8");
		case 5:
			return bExponent ? TEXT("FLOAT64") : TEXT("UINT16");
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

	static bool IsFloat(const int32 TypeIndex, const bool bExponent)
	{
		return bExponent ? TypeIndex >= 4 : TypeIndex >= 8;
	}

	static bool IsUnsignedBase(const int32 Base)
	{
		return Base >= 4 && Base <= 7;
	}

	static bool IndependentIntegerResult(const int32 Base, const int32 Exp)
	{
		return !IsFloat(Base, false) && !IsFloat(Exp, true);
	}

	static bool IndependentReject(const int32 Base, const int32 Exp, const int32 Shape)
	{
		const bool bCompileTime = Shape == 0 || Shape == 2;
		if (bCompileTime && IsUnsignedBase(Base) && !IsFloat(Exp, true))
		{
			return true;
		}
		return !bCompileTime && IndependentIntegerResult(Base, Exp);
	}

	static bool IndependentLimited(const int32 Base, const int32 Exp)
	{
		return Base == 3 || Base == 7 || Base >= 8 || Exp == 3 || Exp >= 4;
	}

	static FString IndependentShapeNeedle(const int32 Shape, const TCHAR* BaseType, const TCHAR* ExpType)
	{
		if (Shape == 0)
		{
			return FString::Printf(TEXT("%s(2) ** %s(-2); // POWER_CAUSE"), BaseType, ExpType);
		}
		if (Shape == 3)
		{
			return TEXT("PowerBase_");
		}
		return TEXT("Base ** Exponent; // POWER_CAUSE");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kShapes[] = {
			TEXT("CONSTANT"), TEXT("MUTABLE_LVALUE"), TEXT("CONST_LVALUE"), TEXT("FUNCTION_RETURN")};
		TArray<FExpectedCell> Cells;
		Cells.SetNum(240);
		for (int32 Index = 0; Index < 240; ++Index)
		{
			const int32 Base = Index / 24;
			const int32 Exp = (Index / 4) % 6;
			const int32 Shape = Index % 4;
			FExpectedCell& Cell = Cells[Index];
			Cell.BaseType = TypeName(Base, false);
			Cell.ExpType = TypeName(Exp, true);
			Cell.CaseId = FString::Printf(
				TEXT("LANG-OP-POWER-NEGATIVE-EXPONENT-%s-%s-%s-NEGATIVE_EXPONENT"),
				TypeToken(Base, false),
				TypeToken(Exp, true),
				kShapes[Shape]);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Base, Exp, Shape);
			Cell.Expected = 0;
			Cell.bLimited = !Cell.bReject && IndependentLimited(Base, Exp);
			Cell.ShapeNeedle = IndependentShapeNeedle(Shape, TypeName(Base, false), TypeName(Exp, true));
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EOpPowerNegativeExponentBase;
		using AngelscriptTest::Generate::EOpPowerNegativeExponentExponent;
		using AngelscriptTest::Generate::EOpPowerNegativeExponentShape;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FOpPowerNegativeExponentGenerator;
		using AngelscriptTest::Generate::FOpPowerNegativeExponentParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FOpPowerNegativeExponentGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-OP-POWER-NEGATIVE-EXPONENT")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangOpPowerNegativeExponentIntIntConstantNegativeExponent")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("OpPowerNegativeExponentGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("OpPowerNegativeExponentGenerator dump path=%s cases=240 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(144, CaseCount));
		ASSERT_THAT(AreEqual(240, Cases.Num()));
		ASSERT_THAT(AreEqual(240, ExpectedCells.Num()));
		ASSERT_THAT(AreEqual(96, RejectIds.Num()));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/OpPowerNegativeExponentGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 RejectCount = 0;
		int32 ZeroCount = 0;
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
				ASSERT_THAT(IsTrue(RejectSource.Contains(TEXT("**"))));
				ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.ShapeNeedle)));
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
			}
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				ASSERT_THAT(AreEqual(Expected.bLimited, Cases[Index].bLimitedObservation));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("**"))));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.ShapeNeedle)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.BaseType)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.ExpType)));
				if (Expected.Expected == 0)
				{
					++ZeroCount;
				}
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
		ASSERT_THAT(AreEqual(96, RejectCount));
		ASSERT_THAT(IsTrue(ZeroCount > 0));

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangOpPowerNegativeExponentIntIntConstantNegativeExponent()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int EntryLangOpPowerNegativeExponentUintIntConstant"))));

		FOpPowerNegativeExponentParams DefaultParams;
		DefaultParams.Base = EOpPowerNegativeExponentBase::Int;
		DefaultParams.Exponent = EOpPowerNegativeExponentExponent::Int;
		ASSERT_THAT(IsTrue(Generator.BuildPowerSource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildPowerSource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(Generator.BuildPowerSource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildPowerSource(DefaultParams).IsEmpty()));
		FOpPowerNegativeExponentParams InvalidEnum;
		InvalidEnum.Base = static_cast<EOpPowerNegativeExponentBase>(10);
		ASSERT_THAT(IsTrue(Generator.BuildPowerSource(InvalidEnum).IsEmpty()));
		FOpPowerNegativeExponentParams RejectParams;
		RejectParams.Base = EOpPowerNegativeExponentBase::Uint;
		RejectParams.Exponent = EOpPowerNegativeExponentExponent::Int;
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
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(240, CountOccurrences(Dump, TEXT("// Case: LANG-OP-POWER-NEGATIVE-EXPONENT-"))));
		ASSERT_THAT(AreEqual(96, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/OpPowerNegativeExponentGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("OpPowerNegativeExponentGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
