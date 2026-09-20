#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestConvBoolContextGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ConvBoolContextGenerator,
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

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-CONV-BOOL-CONTEXT-BOOL-IF-ZERO");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CONV-ABI-FLOAT-FLOAT32-ARGUMENT");

	static bool IndependentReject(const TCHAR* Source)
	{
		return FCString::Strcmp(Source, TEXT("BOOL")) != 0;
	}

	static int32 IndependentExpected(const TCHAR* Value)
	{
		return FCString::Strcmp(Value, TEXT("ZERO")) == 0 ? 0 : 1;
	}

	static FString IndependentNeedle(const TCHAR* Context)
	{
		if (FCString::Strcmp(Context, TEXT("IF")) == 0)
		{
			return TEXT("if (SourceValue)");
		}
		if (FCString::Strcmp(Context, TEXT("WHILE")) == 0)
		{
			return TEXT("while (SourceValue)");
		}
		if (FCString::Strcmp(Context, TEXT("TERNARY_CONDITION")) == 0)
		{
			return TEXT("return SourceValue ? 1 : 0;");
		}
		if (FCString::Strcmp(Context, TEXT("LOGICAL_AND")) == 0)
		{
			return TEXT("return SourceValue && true ? 1 : 0;");
		}
		if (FCString::Strcmp(Context, TEXT("LOGICAL_OR")) == 0)
		{
			return TEXT("return SourceValue || false ? 1 : 0;");
		}
		return TEXT("return SourceValue ^^ false ? 1 : 0;");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kSources[] = {
			TEXT("INT8"), TEXT("INT16"), TEXT("INT"), TEXT("INT64"), TEXT("UINT8"), TEXT("UINT16"),
			TEXT("UINT"), TEXT("UINT64"), TEXT("FLOAT32"), TEXT("FLOAT64"), TEXT("BOOL"), TEXT("ENUM"),
			TEXT("TYPEDEF")};
		static const TCHAR* const kContexts[] = {
			TEXT("IF"), TEXT("WHILE"), TEXT("TERNARY_CONDITION"), TEXT("LOGICAL_AND"), TEXT("LOGICAL_OR"),
			TEXT("LOGICAL_XOR")};
		static const TCHAR* const kValues[] = {TEXT("ZERO"), TEXT("ONE"), TEXT("NEGATIVE")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(234);
		for (int32 Index = 0; Index < 234; ++Index)
		{
			const TCHAR* const Source = kSources[Index / 18];
			const TCHAR* const Context = kContexts[(Index / 3) % 6];
			const TCHAR* const Value = kValues[Index % 3];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-CONV-BOOL-CONTEXT-%s-%s-%s"), Source, Context, Value);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Source);
			Cell.Expected = Cell.bReject ? 0 : IndependentExpected(Value);
			Cell.Needle = IndependentNeedle(Context);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EConvBoolContextContext;
		using AngelscriptTest::Generate::EConvBoolContextSource;
		using AngelscriptTest::Generate::EConvBoolContextValue;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FConvBoolContextGenerator;
		using AngelscriptTest::Generate::FConvBoolContextParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FConvBoolContextGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CONV-BOOL-CONTEXT")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangConvBoolContextBoolIfZero")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ConvBoolContextGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ConvBoolContextGenerator dump path=%s cases=234 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(18, CaseCount));
		ASSERT_THAT(AreEqual(234, Cases.Num()));
		ASSERT_THAT(AreEqual(216, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/ConvBoolContextGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 RejectCount = 0;
		bool bFoundZeroCase = false;
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
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(RejectSource, Expected.EntryName, GeneratedBody)));
				if (Representative.Contains(Expected.EntryName))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
			else
			{
				ASSERT_THAT(AreEqual(
					FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("bool SourceValue ="))));
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
		}
		ASSERT_THAT(AreEqual(216, RejectCount));
		ASSERT_THAT(IsTrue(bFoundZeroCase));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(ProbeCaseId)));

		FConvBoolContextParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildBoolConversionSource(DefaultParams).IsEmpty()));
		FConvBoolContextParams BoolParams;
		BoolParams.Source = EConvBoolContextSource::Bool;
		ASSERT_THAT(IsTrue(Generator.BuildBoolConversionSource(BoolParams).Contains(TEXT("int Entry()"))));
		BoolParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildBoolConversionSource(BoolParams).Contains(TEXT("int ProbeEntry()"))));
		BoolParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildBoolConversionSource(BoolParams).IsEmpty()));
		BoolParams.FunctionName.Reset();
		BoolParams.Context = static_cast<EConvBoolContextContext>(6);
		ASSERT_THAT(IsTrue(Generator.BuildBoolConversionSource(BoolParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(234, CountOccurrences(Dump, TEXT("// Case: LANG-CONV-BOOL-CONTEXT-"))));
		ASSERT_THAT(AreEqual(216, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/ConvBoolContextGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ConvBoolContextGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
