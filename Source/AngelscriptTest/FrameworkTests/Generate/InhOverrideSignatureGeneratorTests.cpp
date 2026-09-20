#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestInhOverrideSignatureGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	InhOverrideSignatureGenerator,
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
		bool bReject = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-INH-OVERRIDE-SIGNATURE-PARAMETER_TYPE-EXACT-BASE");

	static int32 IndependentExpected(const TCHAR* Dimension, const TCHAR* Variant, const TCHAR* View)
	{
		if (FCString::Strcmp(Variant, TEXT("INCOMPATIBLE")) == 0)
		{
			return 0;
		}
		if (FCString::Strcmp(View, TEXT("EXPLICIT_BASE")) == 0)
		{
			return 102;
		}
		if (FCString::Strcmp(View, TEXT("BASE")) == 0)
		{
			return FCString::Strcmp(Variant, TEXT("EXACT")) == 0 ? 203 : 102;
		}
		if (FCString::Strcmp(Variant, TEXT("COMPATIBLE_OVERLOAD")) == 0)
		{
			if (FCString::Strcmp(Dimension, TEXT("PARAMETER_COUNT")) == 0)
			{
				return 205;
			}
			if (FCString::Strcmp(Dimension, TEXT("NAME_HIDING")) == 0)
			{
				return 204;
			}
		}
		return 203;
	}

	static FString IndependentNeedle(const TCHAR* Dimension, const TCHAR* Variant)
	{
		if (FCString::Strcmp(Variant, TEXT("INCOMPATIBLE")) == 0)
		{
			if (FCString::Strcmp(Dimension, TEXT("NAME_HIDING")) == 0)
			{
				return TEXT("ResolveRenamed");
			}
			if (FCString::Strcmp(Dimension, TEXT("RETURN_TYPE")) == 0)
			{
				return TEXT("float Resolve");
			}
			if (FCString::Strcmp(Dimension, TEXT("VISIBILITY")) == 0)
			{
				return TEXT("private ");
			}
			return TEXT("override");
		}
		if (FCString::Strcmp(Variant, TEXT("COMPATIBLE_OVERLOAD")) == 0)
		{
			if (FCString::Strcmp(Dimension, TEXT("PARAMETER_COUNT")) == 0)
			{
				return TEXT("Resolve(1, 2)");
			}
			if (FCString::Strcmp(Dimension, TEXT("NAME_HIDING")) == 0)
			{
				return TEXT("Resolve(2.5f)");
			}
			if (FCString::Strcmp(Dimension, TEXT("RETURN_TYPE")) == 0)
			{
				return TEXT("202.0f + float(Value)");
			}
			if (FCString::Strcmp(Dimension, TEXT("CONSTNESS")) == 0)
			{
				return TEXT("int Resolve(int Value)");
			}
			return TEXT("Resolve(1.5f)");
		}
		return TEXT("101 + Value");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kDimensions[] = {
			TEXT("PARAMETER_TYPE"), TEXT("PARAMETER_COUNT"), TEXT("RETURN_TYPE"),
			TEXT("CONSTNESS"), TEXT("VISIBILITY"), TEXT("NAME_HIDING")};
		static const TCHAR* const kVariants[] = {TEXT("EXACT"), TEXT("COMPATIBLE_OVERLOAD"), TEXT("INCOMPATIBLE")};
		static const TCHAR* const kViews[] = {TEXT("BASE"), TEXT("DERIVED"), TEXT("EXPLICIT_BASE")};
		TArray<FExpectedCell> Cells;
		Cells.SetNum(54);
		for (int32 Index = 0; Index < 54; ++Index)
		{
			const TCHAR* const Dimension = kDimensions[Index / 9];
			const TCHAR* const Variant = kVariants[(Index / 3) % 3];
			const TCHAR* const View = kViews[Index % 3];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-INH-OVERRIDE-SIGNATURE-%s-%s-%s"),
				Dimension,
				Variant,
				View);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = FCString::Strcmp(Variant, TEXT("INCOMPATIBLE")) == 0;
			Cell.Expected = IndependentExpected(Dimension, Variant, View);
			Cell.Needle = IndependentNeedle(Dimension, Variant);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EInhOverrideSignatureDimension;
		using AngelscriptTest::Generate::EInhOverrideSignatureVariant;
		using AngelscriptTest::Generate::EInhOverrideSignatureView;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FInhOverrideSignatureGenerator;
		using AngelscriptTest::Generate::FInhOverrideSignatureParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FInhOverrideSignatureGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-INH-OVERRIDE-SIGNATURE")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangInhOverrideSignatureParameterTypeExactBase")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("InhOverrideSignatureGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("InhOverrideSignatureGenerator dump path=%s cases=54 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(36, CaseCount));
		ASSERT_THAT(AreEqual(54, Cases.Num()));
		ASSERT_THAT(AreEqual(18, RejectIds.Num()));

		int32 RejectCount = 0;
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
				ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.Needle)));
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
			}
			else
			{
				ASSERT_THAT(AreEqual(
					FString::Printf(TEXT("int %s()"), *Expected.EntryName),
					Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
			}
		}
		ASSERT_THAT(AreEqual(18, RejectCount));

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangInhOverrideSignatureParameterTypeExactBase()"))));
		ASSERT_THAT(AreEqual(36, CountOccurrences(Aggregate, TEXT("class FSignatureBase"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));

		FInhOverrideSignatureParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildOverrideSignatureSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FInhOverrideSignatureParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildOverrideSignatureSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(Generator.BuildOverrideSignatureSource(ProbeParams).Contains(TEXT("int Entry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/InhOverrideSignatureGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			const FString CaseSource = Expected.bReject
				? Generator.BuildRejectSource(Expected.CaseId)
				: Generator.BuildCaseSource(Expected.CaseId);
			ASSERT_THAT(IsTrue(CaseSource.Contains(Expected.Needle)));
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FInhOverrideSignatureParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildOverrideSignatureSource(InvalidName).IsEmpty()));
		FInhOverrideSignatureParams InvalidEnum = DefaultParams;
		InvalidEnum.Dimension = static_cast<EInhOverrideSignatureDimension>(6);
		ASSERT_THAT(IsTrue(Generator.BuildOverrideSignatureSource(InvalidEnum).IsEmpty()));
		FInhOverrideSignatureParams RejectParams = DefaultParams;
		RejectParams.Variant = EInhOverrideSignatureVariant::Incompatible;
		ASSERT_THAT(IsTrue(Generator.BuildOverrideSignatureSource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-INH-ACCESS-DEFAULT-FIELD-OWNER")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(203, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(54, CountOccurrences(Dump, TEXT("// Case: LANG-INH-OVERRIDE-SIGNATURE-"))));
		ASSERT_THAT(AreEqual(18, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/InhOverrideSignatureGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("InhOverrideSignatureGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
