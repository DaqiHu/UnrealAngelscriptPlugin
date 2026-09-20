#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestForLoopGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace ForLoopGeneratorTest
{
struct FForLoopExpectedCell
{
	const TCHAR* CaseId = nullptr;
	bool bInit = false;
	bool bCondition = false;
	bool bIncrement = false;
	int32 Limit = 0;
	int32 Packed = 0;
	const TCHAR* EntryName = nullptr;
	const TCHAR* ForHeader = nullptr;
};

constexpr FForLoopExpectedCell ExpectedCells[] = {
	{TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ZERO"), true, true, true, 0, 1010,
		TEXT("EntryLangCfForClausesPresentPresentPresentZero"),
		TEXT("for (Index = InitializeIndex(InitCount); CountCondition(Index, Limit, ConditionCount); CountIncrement(Index, IncrementCount))")},
	{TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE"), true, true, true, 1, 1121,
		TEXT("EntryLangCfForClausesPresentPresentPresentOne"),
		TEXT("for (Index = InitializeIndex(InitCount); CountCondition(Index, Limit, ConditionCount); CountIncrement(Index, IncrementCount))")},
	{TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-MANY"), true, true, true, 4, 1454,
		TEXT("EntryLangCfForClausesPresentPresentPresentMany"),
		TEXT("for (Index = InitializeIndex(InitCount); CountCondition(Index, Limit, ConditionCount); CountIncrement(Index, IncrementCount))")},
	{TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-OMITTED-ZERO"), true, true, false, 0, 1010,
		TEXT("EntryLangCfForClausesPresentPresentOmittedZero"),
		TEXT("for (Index = InitializeIndex(InitCount); CountCondition(Index, Limit, ConditionCount); )")},
	{TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-OMITTED-ONE"), true, true, false, 1, 1120,
		TEXT("EntryLangCfForClausesPresentPresentOmittedOne"),
		TEXT("for (Index = InitializeIndex(InitCount); CountCondition(Index, Limit, ConditionCount); )")},
	{TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-OMITTED-MANY"), true, true, false, 4, 1450,
		TEXT("EntryLangCfForClausesPresentPresentOmittedMany"),
		TEXT("for (Index = InitializeIndex(InitCount); CountCondition(Index, Limit, ConditionCount); )")},
	{TEXT("LANG-CF-FOR-CLAUSES-PRESENT-OMITTED-PRESENT-ZERO"), true, false, true, 0, 1100,
		TEXT("EntryLangCfForClausesPresentOmittedPresentZero"),
		TEXT("for (Index = InitializeIndex(InitCount); ; CountIncrement(Index, IncrementCount))")},
	{TEXT("LANG-CF-FOR-CLAUSES-PRESENT-OMITTED-PRESENT-ONE"), true, false, true, 1, 1100,
		TEXT("EntryLangCfForClausesPresentOmittedPresentOne"),
		TEXT("for (Index = InitializeIndex(InitCount); ; CountIncrement(Index, IncrementCount))")},
	{TEXT("LANG-CF-FOR-CLAUSES-PRESENT-OMITTED-PRESENT-MANY"), true, false, true, 4, 1403,
		TEXT("EntryLangCfForClausesPresentOmittedPresentMany"),
		TEXT("for (Index = InitializeIndex(InitCount); ; CountIncrement(Index, IncrementCount))")},
	{TEXT("LANG-CF-FOR-CLAUSES-PRESENT-OMITTED-OMITTED-ZERO"), true, false, false, 0, 1100,
		TEXT("EntryLangCfForClausesPresentOmittedOmittedZero"),
		TEXT("for (Index = InitializeIndex(InitCount); ; )")},
	{TEXT("LANG-CF-FOR-CLAUSES-PRESENT-OMITTED-OMITTED-ONE"), true, false, false, 1, 1100,
		TEXT("EntryLangCfForClausesPresentOmittedOmittedOne"),
		TEXT("for (Index = InitializeIndex(InitCount); ; )")},
	{TEXT("LANG-CF-FOR-CLAUSES-PRESENT-OMITTED-OMITTED-MANY"), true, false, false, 4, 1400,
		TEXT("EntryLangCfForClausesPresentOmittedOmittedMany"),
		TEXT("for (Index = InitializeIndex(InitCount); ; )")},
	{TEXT("LANG-CF-FOR-CLAUSES-OMITTED-PRESENT-PRESENT-ZERO"), false, true, true, 0, 10,
		TEXT("EntryLangCfForClausesOmittedPresentPresentZero"),
		TEXT("for (; CountCondition(Index, Limit, ConditionCount); CountIncrement(Index, IncrementCount))")},
	{TEXT("LANG-CF-FOR-CLAUSES-OMITTED-PRESENT-PRESENT-ONE"), false, true, true, 1, 121,
		TEXT("EntryLangCfForClausesOmittedPresentPresentOne"),
		TEXT("for (; CountCondition(Index, Limit, ConditionCount); CountIncrement(Index, IncrementCount))")},
	{TEXT("LANG-CF-FOR-CLAUSES-OMITTED-PRESENT-PRESENT-MANY"), false, true, true, 4, 454,
		TEXT("EntryLangCfForClausesOmittedPresentPresentMany"),
		TEXT("for (; CountCondition(Index, Limit, ConditionCount); CountIncrement(Index, IncrementCount))")},
	{TEXT("LANG-CF-FOR-CLAUSES-OMITTED-PRESENT-OMITTED-ZERO"), false, true, false, 0, 10,
		TEXT("EntryLangCfForClausesOmittedPresentOmittedZero"),
		TEXT("for (; CountCondition(Index, Limit, ConditionCount); )")},
	{TEXT("LANG-CF-FOR-CLAUSES-OMITTED-PRESENT-OMITTED-ONE"), false, true, false, 1, 120,
		TEXT("EntryLangCfForClausesOmittedPresentOmittedOne"),
		TEXT("for (; CountCondition(Index, Limit, ConditionCount); )")},
	{TEXT("LANG-CF-FOR-CLAUSES-OMITTED-PRESENT-OMITTED-MANY"), false, true, false, 4, 450,
		TEXT("EntryLangCfForClausesOmittedPresentOmittedMany"),
		TEXT("for (; CountCondition(Index, Limit, ConditionCount); )")},
	{TEXT("LANG-CF-FOR-CLAUSES-OMITTED-OMITTED-PRESENT-ZERO"), false, false, true, 0, 100,
		TEXT("EntryLangCfForClausesOmittedOmittedPresentZero"),
		TEXT("for (; ; CountIncrement(Index, IncrementCount))")},
	{TEXT("LANG-CF-FOR-CLAUSES-OMITTED-OMITTED-PRESENT-ONE"), false, false, true, 1, 100,
		TEXT("EntryLangCfForClausesOmittedOmittedPresentOne"),
		TEXT("for (; ; CountIncrement(Index, IncrementCount))")},
	{TEXT("LANG-CF-FOR-CLAUSES-OMITTED-OMITTED-PRESENT-MANY"), false, false, true, 4, 403,
		TEXT("EntryLangCfForClausesOmittedOmittedPresentMany"),
		TEXT("for (; ; CountIncrement(Index, IncrementCount))")},
	{TEXT("LANG-CF-FOR-CLAUSES-OMITTED-OMITTED-OMITTED-ZERO"), false, false, false, 0, 100,
		TEXT("EntryLangCfForClausesOmittedOmittedOmittedZero"),
		TEXT("for (; ; )")},
	{TEXT("LANG-CF-FOR-CLAUSES-OMITTED-OMITTED-OMITTED-ONE"), false, false, false, 1, 100,
		TEXT("EntryLangCfForClausesOmittedOmittedOmittedOne"),
		TEXT("for (; ; )")},
	{TEXT("LANG-CF-FOR-CLAUSES-OMITTED-OMITTED-OMITTED-MANY"), false, false, false, 4, 400,
		TEXT("EntryLangCfForClausesOmittedOmittedOmittedMany"),
		TEXT("for (; ; )")},
};

constexpr const TCHAR* ProbeCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-OMITTED-PRESENT-ONE");

FString ExpectedDeclaration(const FForLoopExpectedCell& Cell)
{
	return FString::Printf(TEXT("int %s()"), Cell.EntryName);
}
}

using AngelscriptTest::Generate::TestSupport::CountOccurrences;
using AngelscriptTest::Generate::TestSupport::ExtractFunction;
using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
using AngelscriptTest::Generate::TestSupport::LoadGold;

TEST_CLASS_WITH_FLAGS(
	ForLoopGenerator,
	"Angelscript.UnitTest.Framework",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using ForLoopGeneratorTest::ExpectedCells;
		using ForLoopGeneratorTest::ExpectedDeclaration;
		using ForLoopGeneratorTest::FForLoopExpectedCell;
		using ForLoopGeneratorTest::ProbeCaseId;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FForLoopGenerator;
		using AngelscriptTest::Generate::FForLoopParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;

		FForLoopGenerator ForLoop;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CF-FOR-CLAUSES")), FString(ForLoop.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangCfForClausesPresentOmittedPresentOne")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = ForLoop.BuildAllSource(CaseCount);
		const FString RepeatAggregate = ForLoop.BuildAllSource(CaseCount);
		const FString Dump = ForLoop.BuildDumpSource();
		const FString OutputFile = AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ForLoopGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ForLoopGenerator dump path=%s cases=24 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = ForLoop.ListCases();
		ASSERT_THAT(AreEqual(24, CaseCount));
		ASSERT_THAT(AreEqual(24, Cases.Num()));
		ASSERT_THAT(AreEqual(UE_ARRAY_COUNT(ExpectedCells), Cases.Num()));

		int32 RejectCount = 0;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedCells); ++Index)
		{
			const FForLoopExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(FString(Expected.CaseId), Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(ExpectedDeclaration(Expected), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Packed, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedException.IsEmpty()));
			ASSERT_THAT(IsFalse(Cases[Index].bLimitedObservation));
			ASSERT_THAT(AreEqual(Expected.Packed, ForLoop.GetExpected(Expected.CaseId)));
			if (Cases[Index].Outcome == EGeneratedCaseOutcome::CompileReject)
			{
				++RejectCount;
			}
		}
		ASSERT_THAT(AreEqual(0, RejectCount));

		const FString OwnedCase = ForLoop.BuildCaseSource(ProbeCaseId);
		const FString OwnedAgain = ForLoop.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(IsTrue(OwnedCase.Len() > 0));
		ASSERT_THAT(AreEqual(OwnedCase, OwnedAgain));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangCfForClausesPresentOmittedPresentOne()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangCfForClausesPresentOmittedPresentOne()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));

		const FString DefaultSingle = ForLoop.BuildForSource({
			.bInit = true,
			.bCondition = false,
			.bIncrement = true,
			.Limit = 1,
		});
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int InitializeIndex(int&inout InitCount)"))));
		ASSERT_THAT(IsFalse(DefaultSingle.Contains(TEXT("int EntryLang"))));

		FForLoopParams ProbeParams;
		ProbeParams.bInit = true;
		ProbeParams.bCondition = false;
		ProbeParams.bIncrement = true;
		ProbeParams.Limit = 1;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		const FString ProbeSource = ForLoop.BuildForSource(ProbeParams);
		ASSERT_THAT(IsTrue(ProbeSource.Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(ProbeSource.Contains(TEXT("int Entry()"))));

		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int InitializeIndex"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Dump, TEXT("int InitializeIndex"))));
		const FString Gold = LoadGold(TEXT("ForLoopAllSource.as"));
		ASSERT_THAT(IsTrue(Gold.Len() > 0));
		ASSERT_THAT(AreEqual(Gold, Aggregate));

		const FString Representative = LoadGold(TEXT("LanguageGenerators/ForLoopGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FForLoopExpectedCell& Expected : ExpectedCells)
		{
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(FString::Printf(TEXT("int Limit = %d;"), Expected.Limit))));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.ForHeader)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(
				TEXT("return InitCount * 1000 + BodyCount * 100 + ConditionCount * 10 + IncrementCount;"))));
			if (!Expected.bCondition)
			{
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("if (Index >= Limit - 1)"))));
			}
			else
			{
				ASSERT_THAT(IsFalse(GeneratedBody.Contains(TEXT("if (Index >= Limit - 1)"))));
			}
			if (!Expected.bIncrement)
			{
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("++Index;"))));
			}
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FForLoopParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(ForLoop.BuildForSource(InvalidName).IsEmpty()));
		ASSERT_THAT(IsTrue(ForLoop.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(ForLoop.BuildCaseSource(TEXT("LANG-CF-CONDITION-IF-BOOL_LITERAL")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, ForLoop.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = ForLoop.ListCases();
		ASSERT_THAT(IsTrue(Retained.Num() > 0));
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = ForLoop.ListCases();
		ASSERT_THAT(AreEqual(FString(ExpectedCells[0].CaseId), Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(ExpectedCells[0].Packed, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(OutputFile.EndsWith(TEXT("GeneratedCases/ForLoopGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\ForLoopGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(24, CountOccurrences(Dump, TEXT("// Case: LANG-CF-FOR-CLAUSES-"))));
		for (const FForLoopExpectedCell& Expected : ExpectedCells)
		{
			ASSERT_THAT(IsTrue(Dump.Contains(FString::Printf(TEXT("// Case: %s"), Expected.CaseId))));
			ASSERT_THAT(IsTrue(Dump.Contains(ExpectedDeclaration(Expected))));
			FString DumpBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Dump, Expected.EntryName, DumpBody)));
			FString AggregateBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, AggregateBody)));
			ASSERT_THAT(AreEqual(AggregateBody, DumpBody));
		}
		ASSERT_THAT(AreEqual(Dump, ForLoop.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));
		ASSERT_THAT(IsTrue(Dump.Contains(TEXT("\n{\n"))));
		ASSERT_THAT(IsTrue(Dump.Contains(TEXT("\n\tint Limit = 1;\n"))));

		ASSERT_THAT(IsTrue(AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("")).IsEmpty()));
		ASSERT_THAT(IsTrue(AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("../Hack")).IsEmpty()));
		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(TEXT("C:/no-such-root/ForLoopGenerator.as"), Dump, ProbeError)));
		ASSERT_THAT(IsTrue(ProbeError.Len() > 0));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		const FString NestedUnderFile = FPaths::Combine(BlockingFile, TEXT("ForLoopGenerator.as"));
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(NestedUnderFile, Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
