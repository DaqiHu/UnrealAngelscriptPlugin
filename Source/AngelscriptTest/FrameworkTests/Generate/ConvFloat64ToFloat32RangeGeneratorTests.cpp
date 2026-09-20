#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestConvFloat64ToFloat32RangeGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ConvFloat64ToFloat32RangeGenerator,
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
		int32 Expected = 0;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-CONV-FLOAT64-TO-FLOAT32-RANGE-ASSIGNMENT-ABOVE_TARGET_MAX");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CONV-ABI-FLOAT-FLOAT32-ARGUMENT");

	static int32 IndependentExpected(const TCHAR* Boundary)
	{
		return FCString::Strcmp(Boundary, TEXT("ABOVE_TARGET_MAX")) == 0
			? static_cast<int32>(0x7F800000u)
			: static_cast<int32>(0xFF800000u);
	}

	static FString IndependentValueNeedle(const TCHAR* Boundary)
	{
		return FCString::Strcmp(Boundary, TEXT("ABOVE_TARGET_MAX")) == 0
			? TEXT("float SourceValue = 6.805646932e+38;")
			: TEXT("float SourceValue = -6.805646932e+38;");
	}

	static FString IndependentFormNeedle(const TCHAR* Form)
	{
		if (FCString::Strcmp(Form, TEXT("ASSIGNMENT")) == 0)
		{
			return TEXT("Converted = SourceValue;");
		}
		if (FCString::Strcmp(Form, TEXT("ARGUMENT")) == 0)
		{
			return TEXT("PassNumericBoundaryTargetFloat32(SourceValue);");
		}
		if (FCString::Strcmp(Form, TEXT("RETURN")) == 0)
		{
			return TEXT("ReturnNumericBoundaryTargetFloat64Float32(SourceValue);");
		}
		return TEXT("float32 Converted = float32(SourceValue);");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kForms[] = {
			TEXT("ASSIGNMENT"), TEXT("ARGUMENT"), TEXT("RETURN"), TEXT("EXPLICIT_CAST")};
		static const TCHAR* const kBoundaries[] = {TEXT("ABOVE_TARGET_MAX"), TEXT("BELOW_TARGET_MIN")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(8);
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const TCHAR* const Form = kForms[Index / 2];
			const TCHAR* const Boundary = kBoundaries[Index % 2];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-CONV-FLOAT64-TO-FLOAT32-RANGE-%s-%s"), Form, Boundary);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Expected = IndependentExpected(Boundary);
			Cell.ValueNeedle = IndependentValueNeedle(Boundary);
			Cell.FormNeedle = IndependentFormNeedle(Form);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EConvFloat64ToFloat32RangeBoundaryDirection;
		using AngelscriptTest::Generate::EConvFloat64ToFloat32RangeForm;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FConvFloat64ToFloat32RangeGenerator;
		using AngelscriptTest::Generate::FConvFloat64ToFloat32RangeParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FConvFloat64ToFloat32RangeGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CONV-FLOAT64-TO-FLOAT32-RANGE")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangConvFloat64ToFloat32RangeAssignmentAboveTargetMax")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ConvFloat64ToFloat32RangeGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ConvFloat64ToFloat32RangeGenerator dump path=%s cases=8 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(8, CaseCount));
		ASSERT_THAT(AreEqual(8, Cases.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/ConvFloat64ToFloat32RangeGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(
				FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("ObserveFloat32Bits"))));
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.ValueNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.FormNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("return ObserveFloat32Bits(Converted);"))));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FConvFloat64ToFloat32RangeParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildNumericBoundaryConversionSource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(
			Generator.BuildNumericBoundaryConversionSource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildNumericBoundaryConversionSource(DefaultParams).IsEmpty()));
		DefaultParams.FunctionName.Reset();
		DefaultParams.Form = static_cast<EConvFloat64ToFloat32RangeForm>(4);
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
		ASSERT_THAT(AreEqual(8, CountOccurrences(Dump, TEXT("// Case: LANG-CONV-FLOAT64-TO-FLOAT32-RANGE-"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/ConvFloat64ToFloat32RangeGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ConvFloat64ToFloat32RangeGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
