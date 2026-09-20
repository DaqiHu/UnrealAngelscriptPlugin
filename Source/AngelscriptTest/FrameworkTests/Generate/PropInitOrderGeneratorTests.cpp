#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestPropInitOrderGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	PropInitOrderGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString StageNeedle;
		FString ReturnNeedle;
		FString TypeNeedle;
		int32 Expected = 0;
		bool bLimited = false;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-PROP-INIT-ORDER-BASE_ENTRY-BASE_FIRST-DEFAULT_VALUE-INT8");
	static constexpr const TCHAR* ZeroCaseId = ProbeCaseId;
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static int32 IndependentExpected(const TCHAR* Source)
	{
		return (FCString::Strcmp(Source, TEXT("DEFAULT_VALUE")) == 0
			|| FCString::Strcmp(Source, TEXT("DERIVED_REASSIGNMENT")) == 0)
			? 0
			: 1;
	}

	static int32 IndependentStage(const TCHAR* Observation)
	{
		if (FCString::Strcmp(Observation, TEXT("BASE_ENTRY")) == 0)
		{
			return 0;
		}
		if (FCString::Strcmp(Observation, TEXT("BASE_EXIT")) == 0)
		{
			return 1;
		}
		if (FCString::Strcmp(Observation, TEXT("DERIVED_ENTRY")) == 0)
		{
			return 2;
		}
		return 3;
	}

	static bool IndependentLimited(const TCHAR* Type)
	{
		return FCString::Strcmp(Type, TEXT("FLOAT32")) == 0
			|| FCString::Strcmp(Type, TEXT("FLOAT64")) == 0
			|| FCString::Strcmp(Type, TEXT("INT64")) == 0
			|| FCString::Strcmp(Type, TEXT("UINT64")) == 0
			|| FCString::Strcmp(Type, TEXT("NATIVE_VALUE")) == 0
			|| FCString::Strcmp(Type, TEXT("TYPEDEF")) == 0;
	}

	static FString IndependentScriptType(const TCHAR* Type)
	{
		if (FCString::Strcmp(Type, TEXT("INT8")) == 0)
		{
			return TEXT("int8");
		}
		if (FCString::Strcmp(Type, TEXT("INT16")) == 0)
		{
			return TEXT("int16");
		}
		if (FCString::Strcmp(Type, TEXT("INT64")) == 0)
		{
			return TEXT("int64");
		}
		if (FCString::Strcmp(Type, TEXT("UINT8")) == 0)
		{
			return TEXT("uint8");
		}
		if (FCString::Strcmp(Type, TEXT("UINT16")) == 0)
		{
			return TEXT("uint16");
		}
		if (FCString::Strcmp(Type, TEXT("UINT")) == 0)
		{
			return TEXT("uint");
		}
		if (FCString::Strcmp(Type, TEXT("UINT64")) == 0)
		{
			return TEXT("uint64");
		}
		if (FCString::Strcmp(Type, TEXT("FLOAT32")) == 0)
		{
			return TEXT("float");
		}
		if (FCString::Strcmp(Type, TEXT("FLOAT64")) == 0)
		{
			return TEXT("double");
		}
		if (FCString::Strcmp(Type, TEXT("BOOL")) == 0)
		{
			return TEXT("bool");
		}
		if (FCString::Strcmp(Type, TEXT("ENUM")) == 0)
		{
			return TEXT("ENativeCaseEnum");
		}
		if (FCString::Strcmp(Type, TEXT("TYPEDEF")) == 0)
		{
			return TEXT("NativeCaseAlias");
		}
		if (FCString::Strcmp(Type, TEXT("SCRIPT_VALUE")) == 0)
		{
			return TEXT("FScriptCaseValue");
		}
		if (FCString::Strcmp(Type, TEXT("NATIVE_VALUE")) == 0)
		{
			return TEXT("FNativeCaseValue");
		}
		return TEXT("int");
	}

	static FString IndependentTypeNeedle(const TCHAR* Type)
	{
		return FString::Printf(TEXT("\t%s Observed"), *IndependentScriptType(Type));
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kObservations[] = {
			TEXT("BASE_ENTRY"), TEXT("BASE_EXIT"), TEXT("DERIVED_ENTRY"), TEXT("DERIVED_EXIT")};
		static const TCHAR* const kPositions[] = {
			TEXT("BASE_FIRST"), TEXT("BASE_MIDDLE"), TEXT("BASE_LAST"), TEXT("DERIVED_FIRST"), TEXT("DERIVED_LAST")};
		static const TCHAR* const kSources[] = {
			TEXT("DEFAULT_VALUE"), TEXT("DECLARATION_INITIALIZER"), TEXT("OWNER_LITERAL_ASSIGNMENT"),
			TEXT("OWNER_SOURCE_ASSIGNMENT"), TEXT("DERIVED_REASSIGNMENT")};
		static const TCHAR* const kTypes[] = {
			TEXT("INT8"), TEXT("INT16"), TEXT("INT"), TEXT("INT64"), TEXT("UINT8"), TEXT("UINT16"),
			TEXT("UINT"), TEXT("UINT64"), TEXT("FLOAT32"), TEXT("FLOAT64"), TEXT("BOOL"), TEXT("ENUM"),
			TEXT("TYPEDEF"), TEXT("SCRIPT_VALUE"), TEXT("NATIVE_VALUE")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(1500);
		for (int32 Index = 0; Index < 1500; ++Index)
		{
			const TCHAR* const Observation = kObservations[Index / 375];
			const TCHAR* const Position = kPositions[(Index / 75) % 5];
			const TCHAR* const Source = kSources[(Index / 15) % 5];
			const TCHAR* const Type = kTypes[Index % 15];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-PROP-INIT-ORDER-%s-%s-%s-%s"), Observation, Position, Source, Type);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Expected = IndependentExpected(Source);
			Cell.StageNeedle = FString::Printf(TEXT("const int ObservationStage = %d;"), IndependentStage(Observation));
			Cell.ReturnNeedle = FString::Printf(TEXT("return %d;"), Cell.Expected);
			Cell.TypeNeedle = IndependentTypeNeedle(Type);
			Cell.bLimited = IndependentLimited(Type);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EPropInitOrderObservation;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FPropInitOrderGenerator;
		using AngelscriptTest::Generate::FPropInitOrderParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FPropInitOrderGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-PROP-INIT-ORDER")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangPropInitOrderBaseEntryBaseFirstDefaultValueInt8")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("PropInitOrderGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("PropInitOrderGenerator dump path=%s cases=1500 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(1500, CaseCount));
		ASSERT_THAT(AreEqual(1500, Cases.Num()));

		bool bFoundZero = false;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
			ASSERT_THAT(AreEqual(Expected.bLimited, Cases[Index].bLimitedObservation));
			if (Expected.CaseId == ZeroCaseId)
			{
				bFoundZero = true;
				ASSERT_THAT(AreEqual(0, Expected.Expected));
			}
		}
		ASSERT_THAT(IsTrue(bFoundZero));

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangPropInitOrderBaseEntryBaseFirstDefaultValueInt8()"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int RecordPropertyInitializationMarker(int Marker)"))));

		FPropInitOrderParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildPropertyInitializationSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FPropInitOrderParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildPropertyInitializationSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/PropInitOrderGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.StageNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.ReturnNeedle)));
			ASSERT_THAT(IsTrue(Aggregate.Contains(FString::Printf(
				TEXT("class FPropertyInitializationDerived_%s :"),
				*Expected.EntryName))));
			ASSERT_THAT(IsTrue(Aggregate.Contains(FString::Printf(
				TEXT("FPropertyInitializationDerived_%s();"),
				*Expected.EntryName))));
			ASSERT_THAT(IsTrue(Generator.BuildCaseSource(Expected.CaseId).Contains(Expected.TypeNeedle)));
			if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FPropInitOrderParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildPropertyInitializationSource(InvalidName).IsEmpty()));
		FPropInitOrderParams InvalidEnum = DefaultParams;
		InvalidEnum.Observation = static_cast<EPropInitOrderObservation>(4);
		ASSERT_THAT(IsTrue(Generator.BuildPropertyInitializationSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(ZeroCaseId)));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(0, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(1500, CountOccurrences(Dump, TEXT("// Case: LANG-PROP-INIT-ORDER-"))));
		ASSERT_THAT(AreEqual(0, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/PropInitOrderGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("PropInitOrderGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
