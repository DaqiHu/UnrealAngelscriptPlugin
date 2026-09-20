#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestForeachProtocolGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ForeachProtocolGenerator,
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
		bool bFault = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-FE-PROTOCOL-COMPLETE-EXACT-SINGLE");

	static bool IndependentComplete(const TCHAR* Protocol)
	{
		return FCString::Strcmp(Protocol, TEXT("COMPLETE")) == 0
			|| FCString::Strcmp(Protocol, TEXT("OVERLOADED")) == 0
			|| FCString::Strcmp(Protocol, TEXT("THROWING")) == 0;
	}

	static bool IndependentResolved(const TCHAR* Resolution)
	{
		return FCString::Strcmp(Resolution, TEXT("MISSING")) != 0;
	}

	static int32 IndependentContribution(const TCHAR* Resolution)
	{
		if (FCString::Strcmp(Resolution, TEXT("CONVERSION")) == 0)
		{
			return 2;
		}
		if (FCString::Strcmp(Resolution, TEXT("CONST_OVERLOAD")) == 0)
		{
			return 3;
		}
		if (FCString::Strcmp(Resolution, TEXT("AMBIGUOUS")) == 0)
		{
			return 20;
		}
		return 1;
	}

	static int32 IndependentExpected(const TCHAR* Protocol, const TCHAR* Resolution, const TCHAR* Nesting)
	{
		if (!IndependentComplete(Protocol) || !IndependentResolved(Resolution))
		{
			return 0;
		}
		if (FCString::Strcmp(Protocol, TEXT("THROWING")) == 0
			&& FCString::Strcmp(Resolution, TEXT("AMBIGUOUS")) != 0)
		{
			return 0;
		}
		const int32 Contribution = IndependentContribution(Resolution);
		if (FCString::Strcmp(Nesting, TEXT("SINGLE")) == 0)
		{
			return 2 * Contribution;
		}
		if (FCString::Strcmp(Nesting, TEXT("INSIDE_FOR")) == 0)
		{
			return 4 * Contribution;
		}
		return 6 * Contribution;
	}

	static FString IndependentNeedle(const TCHAR* Nesting)
	{
		if (FCString::Strcmp(Nesting, TEXT("INSIDE_FOR")) == 0)
		{
			return TEXT("for (int OuterIndex = 0; OuterIndex < 2; ++OuterIndex)");
		}
		if (FCString::Strcmp(Nesting, TEXT("CONTAINS_FOR")) == 0)
		{
			return TEXT("for (int Index = 0; Index < 2; ++Index)");
		}
		if (FCString::Strcmp(Nesting, TEXT("DISTINCT_ITERABLE")) == 0)
		{
			return TEXT("OtherRange");
		}
		if (FCString::Strcmp(Nesting, TEXT("SAME_ITERABLE")) == 0)
		{
			return TEXT("OuterValue");
		}
		return TEXT("foreach (");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kProtocols[] = {
			TEXT("COMPLETE"), TEXT("OVERLOADED"), TEXT("MISSING_BEGIN"), TEXT("MISSING_NEXT"),
			TEXT("MISSING_VALUE"), TEXT("MISSING_END"), TEXT("WRONG_PARAMETER"), TEXT("WRONG_RETURN"),
			TEXT("INACCESSIBLE"), TEXT("THROWING")};
		static const TCHAR* const kResolutions[] = {
			TEXT("EXACT"), TEXT("CONVERSION"), TEXT("CONST_OVERLOAD"), TEXT("AMBIGUOUS"), TEXT("MISSING")};
		static const TCHAR* const kNestings[] = {
			TEXT("SINGLE"), TEXT("SAME_ITERABLE"), TEXT("DISTINCT_ITERABLE"), TEXT("INSIDE_FOR"),
			TEXT("CONTAINS_FOR")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(250);
		for (int32 Index = 0; Index < 250; ++Index)
		{
			const TCHAR* const Protocol = kProtocols[Index / 25];
			const TCHAR* const Resolution = kResolutions[(Index / 5) % 5];
			const TCHAR* const Nesting = kNestings[Index % 5];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-FE-PROTOCOL-%s-%s-%s"), Protocol, Resolution, Nesting);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Needle = IndependentNeedle(Nesting);
			Cell.bReject = !IndependentComplete(Protocol) || !IndependentResolved(Resolution);
			Cell.bFault = !Cell.bReject
				&& FCString::Strcmp(Protocol, TEXT("THROWING")) == 0
				&& FCString::Strcmp(Resolution, TEXT("AMBIGUOUS")) != 0;
			Cell.Expected = IndependentExpected(Protocol, Resolution, Nesting);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EForeachProtocol;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FForeachProtocolGenerator;
		using AngelscriptTest::Generate::FForeachProtocolParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FForeachProtocolGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-FE-PROTOCOL")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangFeProtocolCompleteExactSingle")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ForeachProtocolGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ForeachProtocolGenerator dump path=%s cases=250 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(60, CaseCount));
		ASSERT_THAT(AreEqual(250, Cases.Num()));
		ASSERT_THAT(AreEqual(190, RejectIds.Num()));

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
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::SourceOnly));
				ASSERT_THAT(IsTrue(RejectIds.Contains(Expected.CaseId)));
				ASSERT_THAT(AreEqual(Generator.BuildRejectSource(Expected.CaseId), Generator.BuildCaseSource(Expected.CaseId)));
			}
			else if (Expected.bFault)
			{
				++FaultCount;
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")), Cases[Index].ExpectedException));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
			}
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			}
		}
		ASSERT_THAT(AreEqual(190, RejectCount));
		ASSERT_THAT(AreEqual(15, FaultCount));

		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangFeProtocolCompleteExactSingle()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int EntryLangFeProtocolMissingBegin"))));

		FForeachProtocolParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildForeachProtocolSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FForeachProtocolParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildForeachProtocolSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/ForeachProtocolGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			const FString CaseSource = Expected.bReject
				? Generator.BuildRejectSource(Expected.CaseId)
				: Generator.BuildCaseSource(Expected.CaseId);
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
			if (Expected.bFault)
			{
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("1 / Zero"))));
			}
			ASSERT_THAT(AreEqual(1, CountOccurrences(Dump, FString::Printf(TEXT("// Case: %s\n"), *Expected.CaseId))));
			if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FForeachProtocolParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildForeachProtocolSource(InvalidName).IsEmpty()));
		FForeachProtocolParams InvalidEnum = DefaultParams;
		InvalidEnum.Protocol = static_cast<EForeachProtocol>(10);
		ASSERT_THAT(IsTrue(Generator.BuildForeachProtocolSource(InvalidEnum).IsEmpty()));
		FForeachProtocolParams RejectParams = DefaultParams;
		RejectParams.Protocol = EForeachProtocol::MissingBegin;
		ASSERT_THAT(IsTrue(Generator.BuildForeachProtocolSource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-DTOR-PARTIAL-INDEPENDENT-BEFORE_ROOT-NORMAL")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(250, CountOccurrences(Dump, TEXT("// Case: "))));
		ASSERT_THAT(AreEqual(190, CountOccurrences(Dump, TEXT("// Reject module\n"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/ForeachProtocolGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ForeachProtocolGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
