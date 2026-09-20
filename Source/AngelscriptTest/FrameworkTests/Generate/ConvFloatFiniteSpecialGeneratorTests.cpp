#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestConvFloatFiniteSpecialGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ConvFloatFiniteSpecialGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString ValueNeedle;
		FString FormNeedle;
		FString ReturnNeedle;
		int32 Expected = 0;
		bool bLimited = false;
		bool bHost = false;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-CONV-FLOAT-FINITE-SPECIAL-FLOAT32-INT8-ASSIGNMENT-POSITIVE_ZERO");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CONV-ABI-FLOAT-FLOAT32-ARGUMENT");

	static uint64 IndependentBits(const TCHAR* Source, const TCHAR* Target, const TCHAR* Value)
	{
		if (FCString::Strcmp(Value, TEXT("POSITIVE_ZERO")) == 0)
		{
			return 0;
		}
		if (FCString::Strcmp(Value, TEXT("NEGATIVE_ZERO")) == 0)
		{
			if (FCString::Strcmp(Target, TEXT("FLOAT32")) == 0)
			{
				return 0x80000000ull;
			}
			if (FCString::Strcmp(Target, TEXT("FLOAT64")) == 0)
			{
				return 0x8000000000000000ull;
			}
			return 0;
		}
		if (FCString::Strcmp(Target, TEXT("FLOAT32")) == 0)
		{
			return FCString::Strcmp(Source, TEXT("FLOAT32")) == 0 ? 1ull : 0ull;
		}
		if (FCString::Strcmp(Target, TEXT("FLOAT64")) == 0)
		{
			return FCString::Strcmp(Source, TEXT("FLOAT32")) == 0 ? 0x36A0000000000000ull : 1ull;
		}
		return 0;
	}

	static bool IndependentLimited(const TCHAR* Target, const uint64 Bits)
	{
		return FCString::Strcmp(Target, TEXT("FLOAT64")) == 0 && Bits > static_cast<uint64>(MAX_int32);
	}

	static int32 IndependentExpected(const TCHAR* Target, const uint64 Bits, const bool bLimited)
	{
		if (bLimited)
		{
			return 0;
		}
		if (FCString::Strcmp(Target, TEXT("FLOAT32")) == 0)
		{
			return static_cast<int32>(static_cast<uint32>(Bits));
		}
		return static_cast<int32>(Bits);
	}

	static FString IndependentValueNeedle(const TCHAR* Source, const TCHAR* Value)
	{
		const TCHAR* const Type = FCString::Strcmp(Source, TEXT("FLOAT64")) == 0 ? TEXT("float") : TEXT("float32");
		if (FCString::Strcmp(Value, TEXT("POSITIVE_ZERO")) == 0)
		{
			return FString::Printf(TEXT("%s SourceValue = 0.0;"), Type);
		}
		if (FCString::Strcmp(Value, TEXT("NEGATIVE_ZERO")) == 0)
		{
			return FString::Printf(TEXT("%s SourceValue = -0.0;"), Type);
		}
		return FCString::Strcmp(Source, TEXT("FLOAT32")) == 0
			? TEXT("float32 SourceValue = 1.40129846e-45f;")
			: TEXT("float SourceValue = 4.9406564584124654e-324;");
	}

	static FString IndependentFormNeedle(const TCHAR* Form)
	{
		if (FCString::Strcmp(Form, TEXT("ASSIGNMENT")) == 0)
		{
			return TEXT("Converted = SourceValue;");
		}
		if (FCString::Strcmp(Form, TEXT("ARGUMENT")) == 0)
		{
			return TEXT("PassNumericBoundaryTarget");
		}
		if (FCString::Strcmp(Form, TEXT("RETURN")) == 0)
		{
			return TEXT("ReturnNumericBoundaryTarget");
		}
		return TEXT("(SourceValue);");
	}

	static FString IndependentReturnNeedle(const TCHAR* Target)
	{
		if (FCString::Strcmp(Target, TEXT("FLOAT32")) == 0)
		{
			return TEXT("return ObserveFloat32Bits(Converted);");
		}
		if (FCString::Strcmp(Target, TEXT("FLOAT64")) == 0)
		{
			return TEXT("return ObserveFloat64Bits(Converted);");
		}
		return TEXT("return int(Converted);");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kSources[] = {TEXT("FLOAT32"), TEXT("FLOAT64")};
		static const TCHAR* const kTargets[] = {
			TEXT("INT8"), TEXT("INT16"), TEXT("INT"), TEXT("INT64"), TEXT("UINT8"), TEXT("UINT16"), TEXT("UINT"),
			TEXT("UINT64"), TEXT("FLOAT32"), TEXT("FLOAT64")};
		static const TCHAR* const kForms[] = {
			TEXT("ASSIGNMENT"), TEXT("ARGUMENT"), TEXT("RETURN"), TEXT("EXPLICIT_CAST")};
		static const TCHAR* const kValues[] = {TEXT("POSITIVE_ZERO"), TEXT("NEGATIVE_ZERO"), TEXT("SUBNORMAL")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(240);
		for (int32 Index = 0; Index < 240; ++Index)
		{
			const TCHAR* const Source = kSources[Index / 120];
			const TCHAR* const Target = kTargets[(Index / 12) % 10];
			const TCHAR* const Form = kForms[(Index / 3) % 4];
			const TCHAR* const Value = kValues[Index % 3];
			const uint64 Bits = IndependentBits(Source, Target, Value);
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-CONV-FLOAT-FINITE-SPECIAL-%s-%s-%s-%s"), Source, Target, Form, Value);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bLimited = IndependentLimited(Target, Bits);
			Cell.bHost = !Cell.bLimited
				&& (FCString::Strcmp(Target, TEXT("FLOAT32")) == 0 || FCString::Strcmp(Target, TEXT("FLOAT64")) == 0);
			Cell.Expected = IndependentExpected(Target, Bits, Cell.bLimited);
			Cell.ValueNeedle = IndependentValueNeedle(Source, Value);
			Cell.FormNeedle = IndependentFormNeedle(Form);
			Cell.ReturnNeedle = IndependentReturnNeedle(Target);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EConvFloatFiniteSpecialForm;
		using AngelscriptTest::Generate::EConvFloatFiniteSpecialSource;
		using AngelscriptTest::Generate::EConvFloatFiniteSpecialTarget;
		using AngelscriptTest::Generate::EConvFloatFiniteSpecialValue;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FConvFloatFiniteSpecialGenerator;
		using AngelscriptTest::Generate::FConvFloatFiniteSpecialParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FConvFloatFiniteSpecialGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CONV-FLOAT-FINITE-SPECIAL")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangConvFloatFiniteSpecialFloat32Int8AssignmentPositiveZero")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ConvFloatFiniteSpecialGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ConvFloatFiniteSpecialGenerator dump path=%s cases=240 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(240, CaseCount));
		ASSERT_THAT(AreEqual(240, Cases.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/ConvFloatFiniteSpecialGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		bool bFoundZeroCase = false;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(
				FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			if (Expected.bLimited)
			{
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::SourceOnly));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(IsTrue(Cases[Index].bLimitedObservation));
			}
			else
			{
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(IsFalse(Cases[Index].bLimitedObservation));
				if (Expected.bHost)
				{
					ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
				}
				else
				{
					ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
				}
			}
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.ValueNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.FormNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.ReturnNeedle)));
			if (Expected.CaseId == ProbeCaseId)
			{
				bFoundZeroCase = true;
				ASSERT_THAT(AreEqual(0, Expected.Expected));
			}
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}
		ASSERT_THAT(IsTrue(bFoundZeroCase));

		FConvFloatFiniteSpecialParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildNumericBoundaryConversionSource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(
			Generator.BuildNumericBoundaryConversionSource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildNumericBoundaryConversionSource(DefaultParams).IsEmpty()));
		DefaultParams.FunctionName.Reset();
		DefaultParams.Form = static_cast<EConvFloatFiniteSpecialForm>(4);
		ASSERT_THAT(IsTrue(Generator.BuildNumericBoundaryConversionSource(DefaultParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

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
		ASSERT_THAT(AreEqual(240, CountOccurrences(Dump, TEXT("// Case: LANG-CONV-FLOAT-FINITE-SPECIAL-"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/ConvFloatFiniteSpecialGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ConvFloatFiniteSpecialGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
