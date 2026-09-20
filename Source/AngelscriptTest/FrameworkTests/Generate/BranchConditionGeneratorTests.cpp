#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestBranchConditionGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

namespace BranchConditionGeneratorTest
{
struct FBranchExpectedCell
{
	FString CaseId;
	FString EntryName;
	FString ConditionNeedle;
	FString FirstInit;
	FString SecondInit;
	int32 Packed = 0;
	bool bHasElse = false;
	bool bHasElseIf = false;
	bool bCrlf = false;
};

constexpr const TCHAR* ProbeCaseId = TEXT("LANG-CF-BRANCH-CONDITION-DEPTH-IF-VARIABLE-FIRST-LF");
constexpr const TCHAR* ZeroCaseId = TEXT("LANG-CF-BRANCH-CONDITION-DEPTH-IF-VARIABLE-SECOND-LF");

int32 IndependentPacked(const FString& Branch, const FString& Condition, const FString& Selection)
{
	const bool bFirst = Selection == TEXT("FIRST");
	const bool bSecond = Selection == TEXT("SECOND");
	int32 Marker = 0;
	if (Branch == TEXT("IF"))
	{
		Marker = bFirst ? 10 : 0;
	}
	else if (Branch == TEXT("IF_ELSE"))
	{
		Marker = bFirst ? 10 : 20;
	}
	else
	{
		Marker = bFirst ? 10 : (bSecond ? 20 : 30);
	}

	int32 Calls = 0;
	if (Condition == TEXT("SIDE_EFFECT"))
	{
		Calls = (Branch == TEXT("ELSE_IF_CHAIN") && !bFirst) ? 2 : 1;
	}
	return Marker * 100 + Calls;
}

FString IndependentConditionNeedle(const FString& Condition, const FString& Name)
{
	if (Condition == TEXT("COMPARISON"))
	{
		return Name + TEXT(" == true");
	}
	if (Condition == TEXT("LOGICAL"))
	{
		return Name + TEXT(" && true");
	}
	if (Condition == TEXT("NEGATED"))
	{
		return FString::Printf(TEXT("!(%s == false)"), *Name);
	}
	if (Condition == TEXT("SIDE_EFFECT"))
	{
		return FString::Printf(TEXT("EvaluateBranchCondition(%s)"), *Name);
	}
	return Name;
}

TArray<FBranchExpectedCell> BuildExpectedCells()
{
	static const TCHAR* const kBranches[] = {TEXT("IF"), TEXT("IF_ELSE"), TEXT("ELSE_IF_CHAIN")};
	static const TCHAR* const kConditions[] = {
		TEXT("VARIABLE"), TEXT("COMPARISON"), TEXT("LOGICAL"), TEXT("NEGATED"), TEXT("SIDE_EFFECT")};
	static const TCHAR* const kSelections[] = {TEXT("FIRST"), TEXT("SECOND"), TEXT("NONE")};
	static const TCHAR* const kLineEndings[] = {TEXT("LF"), TEXT("CRLF")};

	TArray<FBranchExpectedCell> Cells;
	Cells.SetNum(45);
	for (int32 Index = 0; Index < 45; ++Index)
	{
		const TCHAR* const Branch = kBranches[Index / 30];
		const TCHAR* const Condition = kConditions[(Index / 6) % 5];
		const TCHAR* const Selection = kSelections[(Index / 2) % 3];
		const TCHAR* const LineEnding = kLineEndings[Index % 2];
		FBranchExpectedCell& Cell = Cells[Index];
		Cell.CaseId = FString::Printf(
			TEXT("LANG-CF-BRANCH-CONDITION-DEPTH-%s-%s-%s-%s"),
			Branch,
			Condition,
			Selection,
			LineEnding);
		Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
		Cell.ConditionNeedle = IndependentConditionNeedle(Condition, TEXT("FirstCondition"));
		Cell.FirstInit = FCString::Strcmp(Selection, TEXT("FIRST")) == 0
			? TEXT("bool FirstCondition = true;")
			: TEXT("bool FirstCondition = false;");
		Cell.SecondInit = FCString::Strcmp(Selection, TEXT("SECOND")) == 0
			? TEXT("bool SecondCondition = true;")
			: TEXT("bool SecondCondition = false;");
		Cell.Packed = IndependentPacked(Branch, Condition, Selection);
		Cell.bHasElse = FCString::Strcmp(Branch, TEXT("IF")) != 0;
		Cell.bHasElseIf = FCString::Strcmp(Branch, TEXT("ELSE_IF_CHAIN")) == 0;
		Cell.bCrlf = FCString::Strcmp(LineEnding, TEXT("CRLF")) == 0;
	}
	return Cells;
}

FString ExpectedDeclaration(const FBranchExpectedCell& Cell)
{
	return FString::Printf(TEXT("int %s()"), *Cell.EntryName);
}
}

using AngelscriptTest::Generate::TestSupport::CountOccurrences;
using AngelscriptTest::Generate::TestSupport::ExtractFunction;
using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
using AngelscriptTest::Generate::TestSupport::LoadGold;

TEST_CLASS_WITH_FLAGS(
	BranchConditionGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using BranchConditionGeneratorTest::BuildExpectedCells;
		using BranchConditionGeneratorTest::ExpectedDeclaration;
		using BranchConditionGeneratorTest::FBranchExpectedCell;
		using BranchConditionGeneratorTest::ProbeCaseId;
		using BranchConditionGeneratorTest::ZeroCaseId;
		using AngelscriptTest::Generate::EBranchConditionBranch;
		using AngelscriptTest::Generate::EBranchConditionKind;
		using AngelscriptTest::Generate::EBranchConditionLineEnding;
		using AngelscriptTest::Generate::EBranchConditionSelection;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FBranchConditionGenerator;
		using AngelscriptTest::Generate::FBranchConditionParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;

		const TArray<FBranchExpectedCell> ExpectedCells = BuildExpectedCells();
		FBranchConditionGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CF-BRANCH-CONDITION-DEPTH")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangCfBranchConditionDepthIfVariableFirstLf")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("BranchConditionGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("BranchConditionGenerator dump path=%s cases=45 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(45, CaseCount));
		ASSERT_THAT(AreEqual(45, Cases.Num()));
		ASSERT_THAT(AreEqual(45, ExpectedCells.Num()));

		int32 RejectCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FBranchExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(ExpectedDeclaration(Expected), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Packed, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedException.IsEmpty()));
			ASSERT_THAT(IsFalse(Cases[Index].bLimitedObservation));
			ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("EvaluateBranchCondition"))));
			ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("ReadBranchConditionCalls"))));
			ASSERT_THAT(AreEqual(Expected.Packed, Generator.GetExpected(Expected.CaseId)));
			if (Cases[Index].Outcome == EGeneratedCaseOutcome::CompileReject)
			{
				++RejectCount;
			}
		}
		ASSERT_THAT(AreEqual(0, RejectCount));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		const FString OwnedAgain = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(IsTrue(OwnedCase.Len() > 0));
		ASSERT_THAT(AreEqual(OwnedCase, OwnedAgain));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangCfBranchConditionDepthIfVariableFirstLf()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangCfBranchConditionDepthIfVariableFirstLf()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));
		ASSERT_THAT(IsFalse(OwnedCase.Contains(TEXT("\r"))));

		FBranchConditionParams DefaultParams;
		DefaultParams.Branch = EBranchConditionBranch::If;
		DefaultParams.Condition = EBranchConditionKind::Variable;
		DefaultParams.Selection = EBranchConditionSelection::First;
		DefaultParams.LineEnding = EBranchConditionLineEnding::Lf;
		const FString DefaultSingle = Generator.BuildBranchSource(DefaultParams);
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsFalse(DefaultSingle.Contains(TEXT("int EntryLang"))));

		FBranchConditionParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		const FString ProbeSource = Generator.BuildBranchSource(ProbeParams);
		ASSERT_THAT(IsTrue(ProbeSource.Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(ProbeSource.Contains(TEXT("int Entry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/BranchConditionGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FBranchExpectedCell& Expected : ExpectedCells)
		{
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.FirstInit)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.SecondInit)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.ConditionNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(
				TEXT("return BranchMarker * 100 + ReadBranchConditionCalls();"))));
			if (Expected.bHasElseIf)
			{
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("else if ("))));
			}
			else
			{
				ASSERT_THAT(IsFalse(GeneratedBody.Contains(TEXT("else if ("))));
			}
			if (Expected.bHasElse)
			{
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("\telse"))));
			}
			else
			{
				ASSERT_THAT(IsFalse(GeneratedBody.Contains(TEXT("\telse"))));
			}
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}

			const FString CaseSource = Generator.BuildCaseSource(Expected.CaseId);
			if (Expected.bCrlf)
			{
				ASSERT_THAT(IsTrue(CaseSource.Contains(TEXT("\r\n"))));
			}
			else
			{
				ASSERT_THAT(IsFalse(CaseSource.Contains(TEXT("\r"))));
			}
		}

		FBranchConditionParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildBranchSource(InvalidName).IsEmpty()));
		FBranchConditionParams InvalidBranch = DefaultParams;
		InvalidBranch.Branch = static_cast<EBranchConditionBranch>(4);
		ASSERT_THAT(IsTrue(Generator.BuildBranchSource(InvalidBranch).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(ZeroCaseId)));
		bool bFoundZeroCase = false;
		for (const FGeneratedCaseInfo& Info : Cases)
		{
			if (Info.CaseId == ZeroCaseId)
			{
				bFoundZeroCase = true;
				ASSERT_THAT(IsTrue(Info.ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Info.ExpectedReturn.Get(MIN_int32)));
			}
		}
		ASSERT_THAT(IsTrue(bFoundZeroCase));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		ASSERT_THAT(IsTrue(Retained.Num() > 0));
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(ExpectedCells[0].Packed, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(OutputFile.EndsWith(TEXT("GeneratedCases/BranchConditionGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\BranchConditionGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(45, CountOccurrences(Dump, TEXT("// Case: LANG-CF-BRANCH-CONDITION-DEPTH-"))));
		for (const FBranchExpectedCell& Expected : ExpectedCells)
		{
			ASSERT_THAT(IsTrue(Dump.Contains(FString::Printf(TEXT("// Case: %s"), *Expected.CaseId))));
			ASSERT_THAT(IsTrue(Dump.Contains(ExpectedDeclaration(Expected))));
			FString DumpBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Dump, Expected.EntryName, DumpBody)));
			FString AggregateBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, AggregateBody)));
			ASSERT_THAT(AreEqual(AggregateBody, DumpBody));
		}
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsTrue(Dump.Contains(TEXT("\r\n"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));
		ASSERT_THAT(IsTrue(Dump.Contains(TEXT("\n{\n")) || Dump.Contains(TEXT("\r\n{\r\n"))));

		ASSERT_THAT(IsTrue(AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("")).IsEmpty()));
		ASSERT_THAT(IsTrue(AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("../Hack")).IsEmpty()));
		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/BranchConditionGenerator.as"), Dump, ProbeError)));
		ASSERT_THAT(IsTrue(ProbeError.Len() > 0));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		const FString NestedUnderFile = FPaths::Combine(BlockingFile, TEXT("BranchConditionGenerator.as"));
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
