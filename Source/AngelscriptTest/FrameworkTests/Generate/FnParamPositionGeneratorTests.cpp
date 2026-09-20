#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestFnParamPositionGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	FnParamPositionGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString Needle;
		bool bHost = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-FN-PARAM-POSITION-VALUE-FIRST-INT8");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static FString IndependentNeedle(const TCHAR* Direction, const TCHAR* Position, const TCHAR* Type)
	{
		if (FCString::Strcmp(Type, TEXT("NATIVE_VALUE")) == 0)
		{
			return TEXT("FNativeCaseValue");
		}
		if (FCString::Strcmp(Type, TEXT("BOOL")) == 0
			&& FCString::Strcmp(Direction, TEXT("INOUT")) == 0)
		{
			return TEXT("Target == false");
		}
		if (FCString::Strcmp(Position, TEXT("FIRST")) == 0)
		{
			return TEXT("Target, 202, 303");
		}
		if (FCString::Strcmp(Position, TEXT("MIDDLE")) == 0)
		{
			return TEXT("101, Target, 303");
		}
		return TEXT("101, 202, Target");
	}

	static bool HasExtractableEntry(const FString& Source, const FString& EntryName)
	{
		const FString Header = FString::Printf(TEXT("int %s()"), *EntryName);
		return Source.StartsWith(Header) || Source.Contains(TEXT("\n") + Header);
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kDirections[] = {TEXT("VALUE"), TEXT("IN"), TEXT("OUT"), TEXT("INOUT")};
		static const TCHAR* const kPositions[] = {TEXT("FIRST"), TEXT("MIDDLE"), TEXT("LAST")};
		static const TCHAR* const kTypes[] = {
			TEXT("INT8"), TEXT("INT16"), TEXT("INT"), TEXT("INT64"),
			TEXT("UINT8"), TEXT("UINT16"), TEXT("UINT"), TEXT("UINT64"),
			TEXT("FLOAT32"), TEXT("FLOAT64"), TEXT("BOOL"), TEXT("ENUM"),
			TEXT("TYPEDEF"), TEXT("SCRIPT_VALUE"), TEXT("NATIVE_VALUE")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(180);
		for (int32 Index = 0; Index < 180; ++Index)
		{
			const TCHAR* const Direction = kDirections[Index / 45];
			const TCHAR* const Position = kPositions[(Index / 15) % 3];
			const TCHAR* const Type = kTypes[Index % 15];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-FN-PARAM-POSITION-%s-%s-%s"), Direction, Position, Type);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Needle = IndependentNeedle(Direction, Position, Type);
			Cell.bHost = FCString::Strcmp(Type, TEXT("TYPEDEF")) == 0
				|| FCString::Strcmp(Type, TEXT("NATIVE_VALUE")) == 0;
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EFnParamPositionDirection;
		using AngelscriptTest::Generate::EFnParamPositionPosition;
		using AngelscriptTest::Generate::EFnParamPositionType;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FFnParamPositionGenerator;
		using AngelscriptTest::Generate::FFnParamPositionParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FFnParamPositionGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-FN-PARAM-POSITION")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangFnParamPositionValueFirstInt8")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("FnParamPositionGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("FnParamPositionGenerator dump path=%s cases=180 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(180, CaseCount));
		ASSERT_THAT(AreEqual(180, Cases.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("enum ENativeCaseEnum"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/FnParamPositionGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(1, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(1, Generator.GetExpected(Expected.CaseId)));
			ASSERT_THAT(IsTrue(Cases[Index].Execution == (Expected.bHost
				? EGeneratedCaseExecution::RequiresHostSetup
				: EGeneratedCaseExecution::Standalone)));
			const FString Isolated = Generator.BuildCaseSource(Expected.CaseId);
			ASSERT_THAT(IsTrue(Isolated.Contains(Expected.Needle)));
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
			if (HasExtractableEntry(Representative, Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FFnParamPositionParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildParameterPositionSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FFnParamPositionParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildParameterPositionSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildParameterPositionSource(ProbeParams).IsEmpty()));
		FFnParamPositionParams InvalidEnum = DefaultParams;
		InvalidEnum.Position = static_cast<EFnParamPositionPosition>(3);
		ASSERT_THAT(IsTrue(Generator.BuildParameterPositionSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(180, CountOccurrences(Dump, TEXT("// Case: LANG-FN-PARAM-POSITION-"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/FnParamPositionGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("FnParamPositionGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
