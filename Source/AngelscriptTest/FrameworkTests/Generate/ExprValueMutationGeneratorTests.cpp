#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestExprValueMutationGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ExprValueMutationGenerator,
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

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-EXPR-VALUE-MUTATION-MUTABLE_LVALUE-ASSIGN-LOCAL");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-CONDITION-IF-BOOL_LITERAL-TRUE");

	static const TCHAR* CategoryToken(const int32 Category)
	{
		static const TCHAR* const kTokens[] = {
			TEXT("MUTABLE_LVALUE"), TEXT("CONST_LVALUE"), TEXT("TEMPORARY"), TEXT("REFERENCE_ALIAS"),
			TEXT("FIELD"), TEXT("PROPERTY"), TEXT("INVALID_NON_LVALUE")};
		return kTokens[Category];
	}

	static const TCHAR* MutationToken(const int32 Mutation)
	{
		static const TCHAR* const kTokens[] = {
			TEXT("ASSIGN"), TEXT("COMPOUND_ASSIGN"), TEXT("PREFIX_INCREMENT"),
			TEXT("POSTFIX_INCREMENT"), TEXT("OUT_ARGUMENT")};
		return kTokens[Mutation];
	}

	static const TCHAR* PlacementToken(const int32 Placement)
	{
		static const TCHAR* const kTokens[] = {TEXT("LOCAL"), TEXT("GLOBAL"), TEXT("MEMBER"), TEXT("INDEXED")};
		return kTokens[Placement];
	}

	static bool IndependentWritable(const int32 Category)
	{
		return Category == 0 || Category == 3 || Category == 4;
	}

	static int32 IndependentExpected(const int32 Mutation)
	{
		static const int32 kResult[] = {20, 15, 11, 10, 31};
		static const int32 kAfter[] = {20, 15, 11, 11, 31};
		return kResult[Mutation] * 10000 + kAfter[Mutation] * 100 + kAfter[Mutation];
	}

	static FString IndependentNeedle(const int32 Category, const int32 Mutation, const int32 Placement)
	{
		if (Mutation == 0)
		{
			return TEXT("= NextMutationOperand(20);");
		}
		if (Mutation == 1)
		{
			return TEXT("+= NextMutationOperand(5);");
		}
		if (Mutation == 2)
		{
			return TEXT("\t++");
		}
		if (Mutation == 3)
		{
			return TEXT("++;");
		}
		if (Mutation == 4)
		{
			return TEXT("WriteMutationOut(");
		}
		if (Category == 2)
		{
			return TEXT("MakeMutationTemporary(");
		}
		if (Category == 6)
		{
			return TEXT("ReadMutationNonLValue(");
		}
		if (Category == 5)
		{
			return TEXT("GetPropertyTarget()");
		}
		if (Placement == 1)
		{
			return TEXT("GlobalTarget_");
		}
		if (Category == 3)
		{
			return Placement == 3 ? TEXT("IndexedAlias(0)") : TEXT("BindMutationAlias");
		}
		return TEXT("LocalTarget");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		TArray<FExpectedCell> Cells;
		Cells.SetNum(140);
		for (int32 Index = 0; Index < 140; ++Index)
		{
			const int32 Category = Index / 20;
			const int32 Mutation = (Index / 4) % 5;
			const int32 Placement = Index % 4;
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-EXPR-VALUE-MUTATION-%s-%s-%s"),
				CategoryToken(Category),
				MutationToken(Mutation),
				PlacementToken(Placement));
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Needle = IndependentNeedle(Category, Mutation, Placement);
			Cell.bReject = !IndependentWritable(Category);
			Cell.Expected = Cell.bReject ? 0 : IndependentExpected(Mutation);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EExprValueMutationCategory;
		using AngelscriptTest::Generate::EExprValueMutationMutation;
		using AngelscriptTest::Generate::EExprValueMutationPlacement;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FExprValueMutationGenerator;
		using AngelscriptTest::Generate::FExprValueMutationParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FExprValueMutationGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-EXPR-VALUE-MUTATION")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangExprValueMutationMutableLvalueAssignLocal")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ExprValueMutationGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ExprValueMutationGenerator dump path=%s cases=140 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(60, CaseCount));
		ASSERT_THAT(AreEqual(140, Cases.Num()));
		ASSERT_THAT(AreEqual(80, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/ExprValueMutationGenerator/RepresentativeSources.as"));
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
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(RejectIds.Contains(Expected.CaseId)));
				const FString RejectSource = Generator.BuildRejectSource(Expected.CaseId);
				ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.Needle)));
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
			}
			else
			{
				ASSERT_THAT(AreEqual(
					FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("return Result * 10000 + After * 100 + AliasAfter;"))));
			}
			if (Representative.Contains(Expected.EntryName))
			{
				const FString CaseSource = Expected.bReject
					? Generator.BuildRejectSource(Expected.CaseId)
					: Generator.BuildCaseSource(Expected.CaseId);
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}
		ASSERT_THAT(AreEqual(80, RejectCount));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("class FMutationOwner"))));

		FExprValueMutationParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildExpressionValueCategorySource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildExpressionValueCategorySource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildExpressionValueCategorySource(DefaultParams).IsEmpty()));
		DefaultParams.FunctionName.Reset();
		DefaultParams.Category = static_cast<EExprValueMutationCategory>(7);
		ASSERT_THAT(IsTrue(Generator.BuildExpressionValueCategorySource(DefaultParams).IsEmpty()));
		FExprValueMutationParams RejectParams;
		RejectParams.Category = EExprValueMutationCategory::ConstLValue;
		ASSERT_THAT(IsTrue(Generator.BuildExpressionValueCategorySource(RejectParams).IsEmpty()));
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
		ASSERT_THAT(AreEqual(140, CountOccurrences(Dump, TEXT("// Case: LANG-EXPR-VALUE-MUTATION-"))));
		ASSERT_THAT(AreEqual(80, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/ExprValueMutationGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ExprValueMutationGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
