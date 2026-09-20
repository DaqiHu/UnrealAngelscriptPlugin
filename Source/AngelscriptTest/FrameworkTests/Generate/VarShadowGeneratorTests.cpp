#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestVarShadowGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	VarShadowGenerator,
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

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-VAR-SHADOW-NONE-FUNCTION_AFTER_DECLARATION");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static bool IndependentIsParam(const TCHAR* Relation)
	{
		return FCString::Strcmp(Relation, TEXT("PARAMETER_GLOBAL")) == 0
			|| FCString::Strcmp(Relation, TEXT("PARAMETER_MEMBER")) == 0;
	}

	static bool IndependentIsBefore(const TCHAR* Path)
	{
		return FCString::Strcmp(Path, TEXT("FUNCTION_BEFORE_DECLARATION")) == 0
			|| FCString::Strcmp(Path, TEXT("NESTED_BLOCK_BEFORE")) == 0
			|| FCString::Strcmp(Path, TEXT("IF_BRANCH_BEFORE")) == 0
			|| FCString::Strcmp(Path, TEXT("SWITCH_CASE_BEFORE")) == 0
			|| FCString::Strcmp(Path, TEXT("WHILE_BEFORE")) == 0
			|| FCString::Strcmp(Path, TEXT("NESTED_CALL_CALLER_BEFORE")) == 0;
	}

	static bool IndependentIsInner(const TCHAR* Path)
	{
		return FCString::Strcmp(Path, TEXT("NESTED_BLOCK_INSIDE")) == 0
			|| FCString::Strcmp(Path, TEXT("IF_BRANCH_INSIDE")) == 0
			|| FCString::Strcmp(Path, TEXT("SWITCH_CASE_INSIDE")) == 0
			|| FCString::Strcmp(Path, TEXT("FOR_INITIALIZER")) == 0
			|| FCString::Strcmp(Path, TEXT("FOR_BODY")) == 0
			|| FCString::Strcmp(Path, TEXT("WHILE_BODY")) == 0
			|| FCString::Strcmp(Path, TEXT("FUNCTION_SIBLING_CALL")) == 0
			|| FCString::Strcmp(Path, TEXT("NESTED_CALL_CALLEE")) == 0;
	}

	static bool IndependentReject(const TCHAR* Relation, const TCHAR* Path)
	{
		if (FCString::Strcmp(Path, TEXT("AFTER_OWNER")) == 0)
		{
			return !IndependentIsParam(Relation);
		}
		return IndependentIsBefore(Path) && !IndependentIsParam(Relation);
	}

	static int32 IndependentExpected(const TCHAR* Relation, const TCHAR* Path)
	{
		if (FCString::Strcmp(Path, TEXT("AFTER_OWNER")) == 0 && IndependentIsParam(Relation))
		{
			return 5;
		}
		if (IndependentIsParam(Relation))
		{
			return 11;
		}
		if (IndependentIsInner(Path))
		{
			if (FCString::Strcmp(Relation, TEXT("INNER_OUTER")) == 0)
			{
				return 22;
			}
			if (FCString::Strcmp(Relation, TEXT("SIBLING")) == 0)
			{
				return 21;
			}
		}
		return 11;
	}

	static FString IndependentPathSuffix(const TCHAR* Path)
	{
		return AngelscriptTest::Generate::MakeCanonicalEntryName(FString(Path)).Mid(5);
	}

	static FString IndependentNeedle(const TCHAR* Relation, const TCHAR* Path, const FString& EntryName)
	{
		if (FCString::Strcmp(Relation, TEXT("PARAMETER_MEMBER")) == 0)
		{
			if (FCString::Strcmp(Path, TEXT("AFTER_OWNER")) == 0)
			{
				return TEXT("Owner.ReadAfterOwner()");
			}
			return FString::Printf(TEXT("FScopeOwner%s Owner"), *IndependentPathSuffix(Path));
		}
		if (FCString::Strcmp(Path, TEXT("AFTER_OWNER")) == 0)
		{
			return TEXT("return ReadAfterOwner();");
		}
		if (FCString::Strcmp(Relation, TEXT("PARAMETER_GLOBAL")) == 0)
		{
			return FString::Printf(TEXT("return ScopeOwner%s(11);"), *EntryName);
		}
		return FString::Printf(TEXT("return ScopeOwner%s();"), *EntryName);
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kRelations[] = {
			TEXT("NONE"), TEXT("INNER_OUTER"), TEXT("PARAMETER_GLOBAL"), TEXT("PARAMETER_MEMBER"), TEXT("SIBLING")};
		static const TCHAR* const kPaths[] = {
			TEXT("FUNCTION_BEFORE_DECLARATION"), TEXT("FUNCTION_AFTER_DECLARATION"), TEXT("FUNCTION_SIBLING_CALL"),
			TEXT("NESTED_BLOCK_BEFORE"), TEXT("NESTED_BLOCK_INSIDE"), TEXT("NESTED_BLOCK_AFTER"),
			TEXT("IF_BRANCH_BEFORE"), TEXT("IF_BRANCH_INSIDE"), TEXT("IF_BRANCH_AFTER"),
			TEXT("SWITCH_CASE_BEFORE"), TEXT("SWITCH_CASE_INSIDE"), TEXT("SWITCH_CASE_AFTER"),
			TEXT("FOR_INITIALIZER"), TEXT("FOR_BODY"), TEXT("FOR_AFTER"),
			TEXT("WHILE_BEFORE"), TEXT("WHILE_BODY"), TEXT("WHILE_AFTER"),
			TEXT("NESTED_CALL_CALLER_BEFORE"), TEXT("NESTED_CALL_CALLEE"), TEXT("NESTED_CALL_CALLER_AFTER"),
			TEXT("AFTER_OWNER")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(110);
		for (int32 Index = 0; Index < 110; ++Index)
		{
			const TCHAR* const Relation = kRelations[Index / 22];
			const TCHAR* const Path = kPaths[Index % 22];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-VAR-SHADOW-%s-%s"), Relation, Path);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Relation, Path);
			Cell.Expected = Cell.bReject ? 0 : IndependentExpected(Relation, Path);
			Cell.Needle = IndependentNeedle(Relation, Path, Cell.EntryName);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EVarShadowPath;
		using AngelscriptTest::Generate::EVarShadowRelation;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FVarShadowGenerator;
		using AngelscriptTest::Generate::FVarShadowParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FVarShadowGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-VAR-SHADOW")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangVarShadowNoneFunctionAfterDeclaration")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("VarShadowGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(
			LogTemp,
			Display,
			TEXT("VarShadowGenerator dump path=%s cases=110 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(89, CaseCount));
		ASSERT_THAT(AreEqual(110, Cases.Num()));
		ASSERT_THAT(AreEqual(21, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("foreach"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/VarShadowGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

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
				ASSERT_THAT(IsTrue(RejectIds.Contains(Expected.CaseId)));
				const FString RejectSource = Generator.BuildRejectSource(Expected.CaseId);
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(RejectSource, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
				if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
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
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
				if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
		}
		ASSERT_THAT(AreEqual(21, RejectCount));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));

		FVarShadowParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildScopeSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FVarShadowParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildScopeSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildScopeSource(ProbeParams).IsEmpty()));
		FVarShadowParams InvalidEnum = DefaultParams;
		InvalidEnum.Relation = static_cast<EVarShadowRelation>(5);
		ASSERT_THAT(IsTrue(Generator.BuildScopeSource(InvalidEnum).IsEmpty()));
		FVarShadowParams RejectParams = DefaultParams;
		RejectParams.Path = EVarShadowPath::FunctionBeforeDeclaration;
		ASSERT_THAT(IsTrue(Generator.BuildScopeSource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[1].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[1].CaseId, Relisted[1].CaseId));
		ASSERT_THAT(AreEqual(11, Relisted[1].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(AreEqual(110, CountOccurrences(Dump, TEXT("// Case: LANG-VAR-SHADOW-"))));
		ASSERT_THAT(AreEqual(21, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(IsTrue(
			OutputFile.EndsWith(TEXT("GeneratedCases/VarShadowGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\VarShadowGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/VarShadowGenerator.as"),
			Dump,
			ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("VarShadowGenerator.as")),
			Dump,
			FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
