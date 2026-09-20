#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestDtorPartialGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	DtorPartialGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		int32 Expected = 0;
		bool bFault = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-DTOR-PARTIAL-INDEPENDENT-BEFORE_ROOT-NORMAL");
	static constexpr const TCHAR* ZeroCaseId = TEXT("LANG-DTOR-PARTIAL-INDEPENDENT-BEFORE_ROOT-NORMAL");

	static int32 IndependentBoundary(const TCHAR* Boundary)
	{
		if (FCString::Strcmp(Boundary, TEXT("BEFORE_ROOT")) == 0)
		{
			return 0;
		}
		if (FCString::Strcmp(Boundary, TEXT("ROOT_STARTED")) == 0)
		{
			return 1;
		}
		if (FCString::Strcmp(Boundary, TEXT("FIRST_OWNED")) == 0)
		{
			return 101;
		}
		if (FCString::Strcmp(Boundary, TEXT("MIDDLE_OWNED")) == 0)
		{
			return 303;
		}
		return 606;
	}

	static int32 IndependentExpected(const TCHAR* Topology, const TCHAR* Boundary, const TCHAR* Exit)
	{
		if (FCString::Strcmp(Exit, TEXT("EXCEPTION")) == 0)
		{
			return 0;
		}
		const int32 Base = IndependentBoundary(Boundary);
		const bool bDuplicate = FCString::Strcmp(Boundary, TEXT("COMPLETE")) == 0
			&& (FCString::Strcmp(Topology, TEXT("COPY_TRANSFER")) == 0
				|| FCString::Strcmp(Topology, TEXT("ASSIGNMENT_TRANSFER")) == 0
				|| FCString::Strcmp(Topology, TEXT("REFERENCE_ALIAS")) == 0);
		return bDuplicate ? Base * 2 : Base;
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kTopologies[] = {
			TEXT("INDEPENDENT"), TEXT("NESTED_MEMBER"), TEXT("BASE_DERIVED"), TEXT("COPY_TRANSFER"),
			TEXT("ASSIGNMENT_TRANSFER"), TEXT("SELF_ASSIGNMENT"), TEXT("REFERENCE_ALIAS")};
		static const TCHAR* const kBoundaries[] = {
			TEXT("BEFORE_ROOT"), TEXT("ROOT_STARTED"), TEXT("FIRST_OWNED"), TEXT("MIDDLE_OWNED"),
			TEXT("ALL_OWNED"), TEXT("COMPLETE")};
		static const TCHAR* const kExits[] = {TEXT("NORMAL"), TEXT("EXCEPTION"), TEXT("ABORT"), TEXT("UNPREPARE")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(168);
		for (int32 Index = 0; Index < 168; ++Index)
		{
			const TCHAR* const Topology = kTopologies[Index / 24];
			const TCHAR* const Boundary = kBoundaries[(Index / 4) % 6];
			const TCHAR* const Exit = kExits[Index % 4];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-DTOR-PARTIAL-%s-%s-%s"), Topology, Boundary, Exit);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bFault = FCString::Strcmp(Exit, TEXT("EXCEPTION")) == 0;
			Cell.Expected = IndependentExpected(Topology, Boundary, Exit);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EDtorPartialTopology;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FDtorPartialGenerator;
		using AngelscriptTest::Generate::FDtorPartialParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FDtorPartialGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-DTOR-PARTIAL")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangDtorPartialIndependentBeforeRootNormal")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("DtorPartialGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("DtorPartialGenerator dump path=%s cases=168 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(168, CaseCount));
		ASSERT_THAT(AreEqual(168, Cases.Num()));

		int32 FaultCount = 0;
		int32 ZeroCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			if (Expected.bFault)
			{
				++FaultCount;
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")), Cases[Index].ExpectedException));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
			}
			else
			{
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				if (Expected.Expected == 0)
				{
					++ZeroCount;
				}
			}
		}
		ASSERT_THAT(AreEqual(42, FaultCount));
		ASSERT_THAT(IsTrue(ZeroCount > 0));

		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangDtorPartialIndependentBeforeRootNormal()"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("class FPartialNestedOwner\n{"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int RunDestructorPartialRecovery()"))));

		FDtorPartialParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildDestructorPartialSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FDtorPartialParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildDestructorPartialSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/DtorPartialGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("ReachDestructorPartialBoundary"))));
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

		FDtorPartialParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildDestructorPartialSource(InvalidName).IsEmpty()));
		FDtorPartialParams InvalidEnum = DefaultParams;
		InvalidEnum.Topology = static_cast<EDtorPartialTopology>(7);
		ASSERT_THAT(IsTrue(Generator.BuildDestructorPartialSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-DTOR-DECLARATION-IMPLICIT_SCRIPT_VALUE-COMPILE")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(ZeroCaseId)));
		bool bFoundZero = false;
		for (const FGeneratedCaseInfo& Info : Cases)
		{
			if (Info.CaseId == ZeroCaseId)
			{
				bFoundZero = true;
				ASSERT_THAT(IsTrue(Info.ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Info.ExpectedReturn.Get(MIN_int32)));
			}
		}
		ASSERT_THAT(IsTrue(bFoundZero));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(168, CountOccurrences(Dump, TEXT("// Case: "))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/DtorPartialGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("DtorPartialGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
