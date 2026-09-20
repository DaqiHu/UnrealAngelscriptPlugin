#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestConvAbiGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ConvAbiGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString TypeNeedle;
		FString OpNeedle;
		int32 Expected = 0;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-CONV-ABI-FLOAT-FLOAT32-ARGUMENT");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-CONDITION-IF-BOOL_LITERAL-TRUE");

	static int32 IndependentExpected(const TCHAR* Storage, const TCHAR* Direction)
	{
		if (FCString::Strcmp(Direction, TEXT("ARGUMENT")) != 0)
		{
			return 1;
		}
		return FCString::Strcmp(Storage, TEXT("FLOAT32")) == 0 ? 101 : 202;
	}

	static FString IndependentTypeNeedle(const TCHAR* Script)
	{
		return FCString::Strcmp(Script, TEXT("DOUBLE")) == 0
			? TEXT("double Value = double(3.25);")
			: TEXT("float Value = float(3.25);");
	}

	static FString IndependentOpNeedle(const TCHAR* Script, const TCHAR* Direction, const FString& Qualifier)
	{
		if (FCString::Strcmp(Direction, TEXT("ARGUMENT")) == 0)
		{
			return FString::Printf(TEXT("return CallAbi%s(Value);"), *Qualifier);
		}
		if (FCString::Strcmp(Direction, TEXT("RETURN")) == 0)
		{
			const TCHAR* const Type = FCString::Strcmp(Script, TEXT("DOUBLE")) == 0 ? TEXT("double") : TEXT("float");
			return FString::Printf(TEXT("%s Result = CallAbi%s();"), Type, *Qualifier);
		}
		return FString::Printf(TEXT("AbiValue%s = Input;"), *Qualifier);
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kScripts[] = {TEXT("FLOAT"), TEXT("DOUBLE")};
		static const TCHAR* const kStorages[] = {TEXT("FLOAT32"), TEXT("FLOAT64")};
		static const TCHAR* const kDirections[] = {TEXT("ARGUMENT"), TEXT("RETURN"), TEXT("PROPERTY")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(12);
		for (int32 Index = 0; Index < 12; ++Index)
		{
			const TCHAR* const Script = kScripts[Index / 6];
			const TCHAR* const Storage = kStorages[(Index / 3) % 2];
			const TCHAR* const Direction = kDirections[Index % 3];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-CONV-ABI-%s-%s-%s"), Script, Storage, Direction);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			const FString Qualifier = Cell.EntryName.Mid(5);
			Cell.Expected = IndependentExpected(Storage, Direction);
			if (FCString::Strcmp(Direction, TEXT("ARGUMENT")) == 0)
			{
				Cell.TypeNeedle = IndependentTypeNeedle(Script);
			}
			else if (FCString::Strcmp(Direction, TEXT("RETURN")) == 0)
			{
				Cell.TypeNeedle = FCString::Strcmp(Script, TEXT("DOUBLE")) == 0
					? TEXT("double Result = CallAbi")
					: TEXT("float Result = CallAbi");
			}
			else
			{
				Cell.TypeNeedle = FCString::Strcmp(Script, TEXT("DOUBLE")) == 0
					? TEXT("double Input = double(6.5);")
					: TEXT("float Input = float(6.5);");
			}
			Cell.OpNeedle = IndependentOpNeedle(Script, Direction, Qualifier);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EConvAbiDirection;
		using AngelscriptTest::Generate::EConvAbiNativeStorage;
		using AngelscriptTest::Generate::EConvAbiScriptDeclaration;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FConvAbiGenerator;
		using AngelscriptTest::Generate::FConvAbiParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FConvAbiGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CONV-ABI")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangConvAbiFloatFloat32Argument")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile = AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ConvAbiGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ConvAbiGenerator dump path=%s cases=12 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(12, CaseCount));
		ASSERT_THAT(AreEqual(12, Cases.Num()));
		ASSERT_THAT(AreEqual(12, ExpectedCells.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative = LoadGold(TEXT("LanguageGenerators/ConvAbiGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 RejectCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedException.IsEmpty()));
			ASSERT_THAT(IsTrue(Cases[Index].bLimitedObservation));
			ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("ExpectedBits"))));
			if (Cases[Index].Outcome == EGeneratedCaseOutcome::CompileReject)
			{
				++RejectCount;
			}
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.TypeNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.OpNeedle)));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}
		ASSERT_THAT(AreEqual(0, RejectCount));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangConvAbiFloatFloat32Argument()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangConvAbiFloatFloat32Argument()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));

		FConvAbiParams DefaultParams;
		const FString DefaultSingle = Generator.BuildAbiSource(DefaultParams);
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsFalse(DefaultSingle.Contains(TEXT("int EntryLang"))));
		FConvAbiParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildAbiSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(Generator.BuildAbiSource(ProbeParams).Contains(TEXT("int Entry()"))));

		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildAbiSource(ProbeParams).IsEmpty()));
		FConvAbiParams InvalidEnum = DefaultParams;
		InvalidEnum.Direction = static_cast<EConvAbiDirection>(3);
		ASSERT_THAT(IsTrue(Generator.BuildAbiSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(ExpectedCells[0].Expected, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(12, CountOccurrences(Dump, TEXT("// Case: LANG-CONV-ABI-"))));
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
			TEXT("C:/no-such-root/ConvAbiGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ConvAbiGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
