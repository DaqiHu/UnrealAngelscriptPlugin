#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestRefDirectionGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	RefDirectionGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString ParameterNeedle;
		FString ActionNeedle;
		int32 Expected = 1;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-REF-DIRECTION-VALUE-NON_NULL-SAME_TWO_NAMES");

	static FString IndependentParameterNeedle(const TCHAR* Direction)
	{
		if (FCString::Strcmp(Direction, TEXT("IN")) == 0)
		{
			return TEXT("FRefRoot & in Target");
		}
		if (FCString::Strcmp(Direction, TEXT("OUT")) == 0)
		{
			return TEXT("FRefRoot & out Target");
		}
		if (FCString::Strcmp(Direction, TEXT("INOUT")) == 0)
		{
			return TEXT("FRefRoot & inout Target");
		}
		return TEXT("FRefRoot Target");
	}

	static FString IndependentActionNeedle(const TCHAR* Direction, const TCHAR* NullState, const TCHAR* Relation)
	{
		if (FCString::Strcmp(NullState, TEXT("NULL_OUTPUT")) == 0)
		{
			return TEXT("= nullptr;");
		}
		if (FCString::Strcmp(NullState, TEXT("NULL_RETURN")) == 0)
		{
			return TEXT("ReturnNullReference()");
		}
		if (FCString::Strcmp(Relation, TEXT("OUT_REPLACEMENT")) == 0)
		{
			return TEXT("MakeRefRoot(71)");
		}
		if (FCString::Strcmp(Relation, TEXT("INOUT_MUTATION")) == 0)
		{
			return TEXT("MakeRefRoot(72)");
		}
		if (FCString::Strcmp(Relation, TEXT("SELF_ASSIGNMENT")) == 0)
		{
			return FCString::Strcmp(Direction, TEXT("IN")) == 0 ? TEXT("Working = Working;") : TEXT("Target = Target;");
		}
		if (FCString::Strcmp(Relation, TEXT("DISTINCT")) == 0)
		{
			return TEXT("SetValue(62)");
		}
		if (FCString::Strcmp(Relation, TEXT("BASE_DERIVED_VIEWS")) == 0)
		{
			return TEXT("SetValue(63)");
		}
		return TEXT("SetValue(61)");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kDirections[] = {TEXT("VALUE"), TEXT("IN"), TEXT("OUT"), TEXT("INOUT")};
		static const TCHAR* const kNulls[] = {TEXT("NON_NULL"), TEXT("NULL_INPUT"), TEXT("NULL_OUTPUT"), TEXT("NULL_RETURN")};
		static const TCHAR* const kRelations[] = {
			TEXT("SAME_TWO_NAMES"), TEXT("DISTINCT"), TEXT("SELF_ASSIGNMENT"),
			TEXT("BASE_DERIVED_VIEWS"), TEXT("OUT_REPLACEMENT"), TEXT("INOUT_MUTATION")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(96);
		for (int32 Index = 0; Index < 96; ++Index)
		{
			const TCHAR* const Direction = kDirections[Index / 24];
			const TCHAR* const NullState = kNulls[(Index / 6) % 4];
			const TCHAR* const Relation = kRelations[Index % 6];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-REF-DIRECTION-%s-%s-%s"), Direction, NullState, Relation);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.ParameterNeedle = IndependentParameterNeedle(Direction);
			Cell.ActionNeedle = IndependentActionNeedle(Direction, NullState, Relation);
			Cell.Expected = 1;
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::ERefDirectionKind;
		using AngelscriptTest::Generate::ERefDirectionNull;
		using AngelscriptTest::Generate::ERefDirectionRelation;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FRefDirectionGenerator;
		using AngelscriptTest::Generate::FRefDirectionParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FRefDirectionGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-REF-DIRECTION")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangRefDirectionValueNonNullSameTwoNames")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("RefDirectionGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("RefDirectionGenerator dump path=%s cases=96 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(96, CaseCount));
		ASSERT_THAT(AreEqual(96, Cases.Num()));
		ASSERT_THAT(AreEqual(96, ExpectedCells.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/RefDirectionGenerator/RepresentativeSources.as"));
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
			ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("CaptureDirectionSnapshot"))));
			const FString CaseSource = Generator.BuildCaseSource(Expected.CaseId);
			ASSERT_THAT(IsTrue(CaseSource.Contains(Expected.ParameterNeedle)));
			ASSERT_THAT(IsTrue(CaseSource.Contains(Expected.ActionNeedle)));
			ASSERT_THAT(IsTrue(CaseSource.Contains(TEXT("CaptureDirectionSnapshot"))));
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("return 1;"))));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
			if (Cases[Index].Outcome == EGeneratedCaseOutcome::CompileReject)
			{
				++RejectCount;
			}
		}
		ASSERT_THAT(AreEqual(0, RejectCount));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangRefDirectionValueNonNullSameTwoNames()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangRefDirectionValueNonNullSameTwoNames()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int ReferenceIdentity"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("FRefRoot ReturnNullReference"))));

		FRefDirectionParams DefaultParams;
		const FString DefaultSingle = Generator.BuildReferenceDirectionSource(DefaultParams);
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsFalse(DefaultSingle.Contains(TEXT("int EntryLang"))));
		FRefDirectionParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildReferenceDirectionSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildReferenceDirectionSource(ProbeParams).IsEmpty()));
		DefaultParams.Direction = static_cast<ERefDirectionKind>(4);
		ASSERT_THAT(IsTrue(Generator.BuildReferenceDirectionSource(DefaultParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(1, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(OutputFile.EndsWith(TEXT("GeneratedCases/RefDirectionGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\RefDirectionGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(96, CountOccurrences(Dump, TEXT("// Case: LANG-REF-DIRECTION-"))));
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
			TEXT("C:/no-such-root/RefDirectionGenerator.as"), Dump, ProbeError)));
		ASSERT_THAT(IsTrue(ProbeError.Len() > 0));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("RefDirectionGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
