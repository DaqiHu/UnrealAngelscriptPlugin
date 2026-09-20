#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestConvNonfinitePreconversionGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ConvNonfinitePreconversionGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString TriggerNeedle;
		FString FormNeedle;
		FString SourceTypeNeedle;
		FString TargetTypeNeedle;
	};

	static FString IndependentTrigger(const TCHAR* Value)
	{
		if (FCString::Strcmp(Value, TEXT("POSITIVE_INFINITY")) == 0)
		{
			return TEXT("SourceValue = Unit / Zero;");
		}
		if (FCString::Strcmp(Value, TEXT("NEGATIVE_INFINITY")) == 0)
		{
			return TEXT("SourceValue = -Unit / Zero;");
		}
		return TEXT("SourceValue = Zero / Zero;");
	}

	static FString IndependentFormNeedle(const TCHAR* Form, const TCHAR* TargetType, const TCHAR* Suffix)
	{
		if (FCString::Strcmp(Form, TEXT("ASSIGNMENT")) == 0)
		{
			return TEXT("Converted = SourceValue;");
		}
		if (FCString::Strcmp(Form, TEXT("ARGUMENT")) == 0)
		{
			return FString::Printf(TEXT("Converted = PassNumeric%s(SourceValue);"), Suffix);
		}
		if (FCString::Strcmp(Form, TEXT("RETURN")) == 0)
		{
			return FString::Printf(TEXT("Converted = ReturnNumeric%s(SourceValue);"), Suffix);
		}
		return FString::Printf(TEXT("Converted = %s(SourceValue);"), TargetType);
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kSources[] = {TEXT("FLOAT32"), TEXT("FLOAT64")};
		static const TCHAR* const kTargets[] = {
			TEXT("INT8"), TEXT("INT16"), TEXT("INT"), TEXT("INT64"),
			TEXT("UINT8"), TEXT("UINT16"), TEXT("UINT"), TEXT("UINT64"),
			TEXT("FLOAT32"), TEXT("FLOAT64")};
		static const TCHAR* const kForms[] = {
			TEXT("ASSIGNMENT"), TEXT("ARGUMENT"), TEXT("RETURN"), TEXT("EXPLICIT_CAST")};
		static const TCHAR* const kValues[] = {
			TEXT("POSITIVE_INFINITY"), TEXT("NEGATIVE_INFINITY"), TEXT("NAN")};
		static const TCHAR* const kSourceTypes[] = {TEXT("float"), TEXT("double")};
		static const TCHAR* const kTargetTypes[] = {
			TEXT("int8"), TEXT("int16"), TEXT("int"), TEXT("int64"),
			TEXT("uint8"), TEXT("uint16"), TEXT("uint"), TEXT("uint64"),
			TEXT("float"), TEXT("double")};
		static const TCHAR* const kSuffixes[] = {
			TEXT("Int8"), TEXT("Int16"), TEXT("Int"), TEXT("Int64"),
			TEXT("Uint8"), TEXT("Uint16"), TEXT("Uint"), TEXT("Uint64"),
			TEXT("Float"), TEXT("Double")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(240);
		for (int32 Index = 0; Index < 240; ++Index)
		{
			const int32 SourceIndex = Index / 120;
			const int32 TargetIndex = (Index / 12) % 10;
			const int32 FormIndex = (Index / 3) % 4;
			const int32 ValueIndex = Index % 3;
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-CONV-NONFINITE-PRECONVERSION-%s-%s-%s-%s"),
				kSources[SourceIndex],
				kTargets[TargetIndex],
				kForms[FormIndex],
				kValues[ValueIndex]);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.TriggerNeedle = IndependentTrigger(kValues[ValueIndex]);
			Cell.FormNeedle = IndependentFormNeedle(kForms[FormIndex], kTargetTypes[TargetIndex], kSuffixes[TargetIndex]);
			Cell.SourceTypeNeedle = FString::Printf(TEXT("%s SourceValue"), kSourceTypes[SourceIndex]);
			Cell.TargetTypeNeedle = FString::Printf(TEXT("%s Converted"), kTargetTypes[TargetIndex]);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EConvNonfiniteForm;
		using AngelscriptTest::Generate::EConvNonfiniteSource;
		using AngelscriptTest::Generate::EConvNonfiniteTarget;
		using AngelscriptTest::Generate::EConvNonfiniteValue;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FConvNonfinitePreconversionGenerator;
		using AngelscriptTest::Generate::FConvNonfinitePreconversionParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FConvNonfinitePreconversionGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CONV-NONFINITE-PRECONVERSION")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangConvNonfinitePreconversionFloat32Int8AssignmentPositiveInfinity")),
			MakeCanonicalEntryName(TEXT("LANG-CONV-NONFINITE-PRECONVERSION-FLOAT32-INT8-ASSIGNMENT-POSITIVE_INFINITY"))));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile = AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(
			TEXT("ConvNonfinitePreconversionGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ConvNonfinitePreconversionGenerator dump path=%s cases=240 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(240, CaseCount));
		ASSERT_THAT(AreEqual(240, Cases.Num()));
		ASSERT_THAT(AreEqual(240, ExpectedCells.Num()));

		int32 FaultCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
			ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
			ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")), Cases[Index].ExpectedException));
			ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
			ASSERT_THAT(IsFalse(Cases[Index].bLimitedObservation));
			++FaultCount;
		}
		ASSERT_THAT(AreEqual(240, FaultCount));

		const TCHAR* const ProbeCaseId = TEXT("LANG-CONV-NONFINITE-PRECONVERSION-FLOAT32-INT8-ASSIGNMENT-POSITIVE_INFINITY");
		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		const FString OwnedAgain = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(IsTrue(OwnedCase.Len() > 0));
		ASSERT_THAT(AreEqual(OwnedCase, OwnedAgain));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangConvNonfinitePreconversionFloat32Int8AssignmentPositiveInfinity()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangConvNonfinitePreconversionFloat32Int8AssignmentPositiveInfinity()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int8 PassNumericInt8(int8 Value)"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("float PassNumericFloat(float Value)"))));

		FConvNonfinitePreconversionParams DefaultParams;
		const FString DefaultSingle = Generator.BuildNumericBoundaryConversionSource(DefaultParams);
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsFalse(DefaultSingle.Contains(TEXT("int EntryLang"))));

		FConvNonfinitePreconversionParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		const FString ProbeSource = Generator.BuildNumericBoundaryConversionSource(ProbeParams);
		ASSERT_THAT(IsTrue(ProbeSource.Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(ProbeSource.Contains(TEXT("int Entry()"))));

		const FString Representative = LoadGold(
			TEXT("LanguageGenerators/ConvNonfinitePreconversionGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.TriggerNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.FormNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.SourceTypeNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.TargetTypeNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("return 0;"))));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FConvNonfinitePreconversionParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildNumericBoundaryConversionSource(InvalidName).IsEmpty()));
		FConvNonfinitePreconversionParams InvalidEnum = DefaultParams;
		InvalidEnum.Source = static_cast<EConvNonfiniteSource>(2);
		ASSERT_THAT(IsTrue(Generator.BuildNumericBoundaryConversionSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-CONV-NUMERIC-INT8-INT8-ASSIGNMENT-ZERO")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(ProbeCaseId)));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedException.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")), Relisted[0].ExpectedException));
		ASSERT_THAT(IsFalse(Relisted[0].ExpectedReturn.IsSet()));

		ASSERT_THAT(IsTrue(OutputFile.EndsWith(TEXT("GeneratedCases/ConvNonfinitePreconversionGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\ConvNonfinitePreconversionGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(240, CountOccurrences(Dump, TEXT("// Case: LANG-CONV-NONFINITE-PRECONVERSION-"))));
		ASSERT_THAT(AreEqual(0, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(240, CountOccurrences(Dump, TEXT("// Outcome: RuntimeException"))));
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

		ASSERT_THAT(IsTrue(AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("")).IsEmpty()));
		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/ConvNonfinitePreconversionGenerator.as"), Dump, ProbeError)));
		ASSERT_THAT(IsTrue(ProbeError.Len() > 0));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ConvNonfinitePreconversionGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
