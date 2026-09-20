#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestExprPrimaryContextGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ExprPrimaryContextGenerator,
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

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-EXPR-PRIMARY-CONTEXT-INITIALIZER-INT_LITERAL");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-CONDITION-IF-BOOL_LITERAL-TRUE");

	static const TCHAR* ContextToken(const int32 Context)
	{
		static const TCHAR* const kTokens[] = {
			TEXT("INITIALIZER"), TEXT("ASSIGNMENT_RHS"), TEXT("ASSIGNMENT_LHS"), TEXT("ARGUMENT"),
			TEXT("RETURN"), TEXT("CONDITION"), TEXT("LOOP_CLAUSE"), TEXT("SWITCH_SELECTOR"),
			TEXT("INDEX"), TEXT("PROPERTY_ACCESSOR")};
		return kTokens[Context];
	}

	static const TCHAR* PrimaryToken(const int32 Primary)
	{
		static const TCHAR* const kTokens[] = {
			TEXT("INT_LITERAL"), TEXT("BOOL_LITERAL"), TEXT("ENUM_LITERAL"), TEXT("NULL_LITERAL"),
			TEXT("LOCAL_IDENTIFIER"), TEXT("CONST_IDENTIFIER"), TEXT("REFERENCE_IDENTIFIER"),
			TEXT("SCOPED_CONSTANT"), TEXT("SCOPED_ENUM"), TEXT("PARENTHESIZED_SCALAR"),
			TEXT("PARENTHESIZED_LVALUE"), TEXT("GLOBAL_FUNCTION_CALL"), TEXT("METHOD_CALL"),
			TEXT("VALUE_CONSTRUCTOR"), TEXT("REFERENCE_CONSTRUCTOR"), TEXT("MEMBER_FIELD"),
			TEXT("VIRTUAL_PROPERTY"), TEXT("INDEXED_PROPERTY"), TEXT("EXPLICIT_NUMERIC_CAST"),
			TEXT("OBJECT_CAST"), TEXT("BASE_CAST"), TEXT("DERIVED_CAST")};
		return kTokens[Primary];
	}

	static const TCHAR* PrimaryExpression(const int32 Primary)
	{
		static const TCHAR* const kExprs[] = {
			TEXT("41"), TEXT("true"), TEXT("EPrimaryValue::FortyOne"), TEXT("nullptr"),
			TEXT("LocalValue"), TEXT("ConstValue"), TEXT("Object"),
			TEXT("PrimaryScope::ScopedValue"), TEXT("PrimaryScope::ScopedFortyOne"),
			TEXT("(LocalValue + 0)"), TEXT("(LocalValue)"), TEXT("MakePrimaryInt()"),
			TEXT("Object.GetValue()"), TEXT("FPrimaryValue(41)"), TEXT("MakeRefRoot(41)"),
			TEXT("ValueObject.Value"), TEXT("Object.GetVirtualValue()"), TEXT("Object.GetItem(0)"),
			TEXT("int(41.75f)"), TEXT("RootObject.opCast()"), TEXT("DerivedObject.opImplCast()"),
			TEXT("BaseView.opCast()")};
		return kExprs[Primary];
	}

	static bool IndependentAssignable(const int32 Primary)
	{
		return Primary == 4 || Primary == 6 || Primary == 10 || Primary == 15;
	}

	static bool IndependentIntegral(const int32 Primary)
	{
		return !(Primary == 1 || Primary == 3 || Primary == 6 || Primary == 13 || Primary == 14
			|| Primary == 19 || Primary == 20 || Primary == 21);
	}

	static bool IndependentIntFamily(const int32 Primary)
	{
		return Primary == 0 || Primary == 4 || Primary == 5 || Primary == 7 || Primary == 9
			|| Primary == 10 || Primary == 11 || Primary == 12 || Primary == 15 || Primary == 16
			|| Primary == 17 || Primary == 18;
	}

	static bool IndependentReject(const int32 Context, const int32 Primary)
	{
		if (Context == 2)
		{
			return !IndependentAssignable(Primary);
		}
		if (Context == 7)
		{
			return !IndependentIntegral(Primary);
		}
		if (Context == 8 || Context == 9)
		{
			return !IndependentIntFamily(Primary);
		}
		return false;
	}

	static FString IndependentNeedle(const int32 Context, const int32 Primary)
	{
		const TCHAR* const Expression = PrimaryExpression(Primary);
		if (Context == 0 || Context == 1)
		{
			return FString::Printf(TEXT("Result = %s;"), Expression);
		}
		if (Context == 2)
		{
			if (Primary == 3 || Primary == 13)
			{
				return TEXT("1 = 2;");
			}
			return FString::Printf(TEXT("%s = "), Expression);
		}
		if (Context == 3)
		{
			return FString::Printf(TEXT("ObservePrimary(%s)"), Expression);
		}
		if (Context == 4)
		{
			return FString::Printf(TEXT("return %s;"), Expression);
		}
		if (Context == 5)
		{
			return FString::Printf(TEXT("if (ObservePrimary(%s) == 1)"), Expression);
		}
		if (Context == 6)
		{
			return FString::Printf(TEXT("ObservePrimary(%s) == 1"), Expression);
		}
		if (Context == 7)
		{
			return FString::Printf(TEXT("switch (%s)"), Expression);
		}
		if (Context == 8)
		{
			return FString::Printf(TEXT("GetItem(%s)"), Expression);
		}
		return FString::Printf(TEXT("SetVirtualValue(%s)"), Expression);
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		TArray<FExpectedCell> Cells;
		Cells.SetNum(220);
		for (int32 Index = 0; Index < 220; ++Index)
		{
			const int32 Context = Index / 22;
			const int32 Primary = Index % 22;
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-EXPR-PRIMARY-CONTEXT-%s-%s"),
				ContextToken(Context),
				PrimaryToken(Primary));
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Needle = IndependentNeedle(Context, Primary);
			Cell.bReject = IndependentReject(Context, Primary);
			Cell.Expected = Cell.bReject ? 0 : 1;
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EExprPrimaryContext;
		using AngelscriptTest::Generate::EExprPrimaryKind;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FExprPrimaryContextGenerator;
		using AngelscriptTest::Generate::FExprPrimaryContextParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FExprPrimaryContextGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-EXPR-PRIMARY-CONTEXT")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangExprPrimaryContextInitializerIntLiteral")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ExprPrimaryContextGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ExprPrimaryContextGenerator dump path=%s cases=220 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(174, CaseCount));
		ASSERT_THAT(AreEqual(220, Cases.Num()));
		ASSERT_THAT(AreEqual(46, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/ExprPrimaryContextGenerator/RepresentativeSources.as"));
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
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
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
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(1, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(1, Generator.GetExpected(Expected.CaseId)));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				const FString CaseSource = Generator.BuildCaseSource(Expected.CaseId);
				ASSERT_THAT(IsTrue(CaseSource.Contains(Expected.Needle)));
				if (Expected.CaseId.Contains(TEXT("-RETURN-")) == false)
				{
					ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
				}
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
		ASSERT_THAT(AreEqual(46, RejectCount));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("enum EPrimaryValue"))));

		FExprPrimaryContextParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildPrimaryExpressionSource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildPrimaryExpressionSource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildPrimaryExpressionSource(DefaultParams).IsEmpty()));
		DefaultParams.FunctionName.Reset();
		DefaultParams.Primary = static_cast<EExprPrimaryKind>(22);
		ASSERT_THAT(IsTrue(Generator.BuildPrimaryExpressionSource(DefaultParams).IsEmpty()));
		FExprPrimaryContextParams RejectParams;
		RejectParams.Context = EExprPrimaryContext::AssignmentLhs;
		RejectParams.Primary = EExprPrimaryKind::IntLiteral;
		ASSERT_THAT(IsTrue(Generator.BuildPrimaryExpressionSource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(220, CountOccurrences(Dump, TEXT("// Case: LANG-EXPR-PRIMARY-CONTEXT-"))));
		ASSERT_THAT(AreEqual(46, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/ExprPrimaryContextGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ExprPrimaryContextGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
