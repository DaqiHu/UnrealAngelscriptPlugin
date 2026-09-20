#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestCtorParamSelectGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	CtorParamSelectGenerator,
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

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-CTOR-PARAM-SELECT-ONE-EXACT-INT8");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-CONDITION-IF-BOOL_LITERAL-TRUE");

	static bool IndependentSupportsPromotion(const TCHAR* Type)
	{
		return FCString::Strcmp(Type, TEXT("INT8")) == 0
			|| FCString::Strcmp(Type, TEXT("INT16")) == 0
			|| FCString::Strcmp(Type, TEXT("INT")) == 0
			|| FCString::Strcmp(Type, TEXT("UINT8")) == 0
			|| FCString::Strcmp(Type, TEXT("UINT16")) == 0
			|| FCString::Strcmp(Type, TEXT("UINT")) == 0
			|| FCString::Strcmp(Type, TEXT("FLOAT32")) == 0
			|| FCString::Strcmp(Type, TEXT("TYPEDEF")) == 0;
	}

	static bool IndependentReject(const TCHAR* Selection, const TCHAR* Type)
	{
		if (FCString::Strcmp(Selection, TEXT("AMBIGUOUS")) == 0
			|| FCString::Strcmp(Selection, TEXT("MISSING")) == 0)
		{
			return true;
		}
		return FCString::Strcmp(Selection, TEXT("PROMOTION")) == 0 && !IndependentSupportsPromotion(Type);
	}

	static int32 IndependentArityCount(const TCHAR* Arity)
	{
		if (FCString::Strcmp(Arity, TEXT("TWO")) == 0)
		{
			return 2;
		}
		if (FCString::Strcmp(Arity, TEXT("FIVE")) == 0)
		{
			return 5;
		}
		if (FCString::Strcmp(Arity, TEXT("SIXTEEN")) == 0)
		{
			return 16;
		}
		return 1;
	}

	static int32 IndependentExpected(const TCHAR* Arity, const TCHAR* Type)
	{
		const int32 Count = IndependentArityCount(Arity);
		int32 Result = 0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Result += (FCString::Strcmp(Type, TEXT("BOOL")) == 0) ? ((Index % 2 == 0) ? 1 : 0) : (Index + 1);
		}
		return Result;
	}

	static FString Compact(const TCHAR* Token)
	{
		FString Text(Token);
		Text.ReplaceInline(TEXT("_"), TEXT(""));
		return Text;
	}

	static FString IndependentNeedle(const TCHAR* Arity, const TCHAR* Selection, const TCHAR* Type)
	{
		return FString::Printf(
			TEXT("FConstructorProbe%s%s%s"),
			*Compact(Arity),
			*Compact(Selection),
			*Compact(Type));
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kArities[] = {TEXT("ONE"), TEXT("TWO"), TEXT("FIVE"), TEXT("SIXTEEN")};
		static const TCHAR* const kSelections[] = {
			TEXT("EXACT"), TEXT("PROMOTION"), TEXT("EXPLICIT_CONVERSION"), TEXT("AMBIGUOUS"), TEXT("MISSING")};
		static const TCHAR* const kTypes[] = {
			TEXT("INT8"), TEXT("INT16"), TEXT("INT"), TEXT("INT64"), TEXT("UINT8"), TEXT("UINT16"), TEXT("UINT"),
			TEXT("UINT64"), TEXT("FLOAT32"), TEXT("FLOAT64"), TEXT("BOOL"), TEXT("ENUM"), TEXT("TYPEDEF"),
			TEXT("SCRIPT_VALUE"), TEXT("NATIVE_VALUE")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(300);
		for (int32 Index = 0; Index < 300; ++Index)
		{
			const TCHAR* const Arity = kArities[Index / 75];
			const TCHAR* const Selection = kSelections[(Index / 15) % 5];
			const TCHAR* const Type = kTypes[Index % 15];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-CTOR-PARAM-SELECT-%s-%s-%s"), Arity, Selection, Type);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Selection, Type);
			Cell.Expected = Cell.bReject ? 0 : IndependentExpected(Arity, Type);
			Cell.Needle = IndependentNeedle(Arity, Selection, Type);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::ECtorParamSelectArity;
		using AngelscriptTest::Generate::ECtorParamSelectSelection;
		using AngelscriptTest::Generate::ECtorParamSelectType;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FCtorParamSelectGenerator;
		using AngelscriptTest::Generate::FCtorParamSelectParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FCtorParamSelectGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CTOR-PARAM-SELECT")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangCtorParamSelectOneExactInt8")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("CtorParamSelectGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("CtorParamSelectGenerator dump path=%s cases=300 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(152, CaseCount));
		ASSERT_THAT(AreEqual(300, Cases.Num()));
		ASSERT_THAT(AreEqual(148, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangCtorParamSelectOneExactInt8()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int EntryLangCtorParamSelectOneAmbiguousInt8()"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("typedef int NativeCaseAlias;"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/CtorParamSelectGenerator/RepresentativeSources.as"));
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
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::SourceOnly));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(RejectIds.Contains(Expected.CaseId)));
				const FString RejectSource = Generator.BuildRejectSource(Expected.CaseId);
				ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.Needle)));
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(RejectSource, Expected.EntryName, GeneratedBody)));
				if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedException.IsEmpty()));
				ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("constructor-parameter"))));
				ASSERT_THAT(IsFalse(Cases[Index].bLimitedObservation));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
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
		ASSERT_THAT(AreEqual(148, RejectCount));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));

		FCtorParamSelectParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildConstructorParameterSource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildConstructorParameterSource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildConstructorParameterSource(DefaultParams).IsEmpty()));
		DefaultParams.FunctionName.Reset();
		DefaultParams.Selection = ECtorParamSelectSelection::Ambiguous;
		ASSERT_THAT(IsTrue(Generator.BuildConstructorParameterSource(DefaultParams).IsEmpty()));
		DefaultParams.Selection = ECtorParamSelectSelection::Exact;
		DefaultParams.Type = static_cast<ECtorParamSelectType>(15);
		ASSERT_THAT(IsTrue(Generator.BuildConstructorParameterSource(DefaultParams).IsEmpty()));
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
		ASSERT_THAT(AreEqual(300, CountOccurrences(Dump, TEXT("// Case: LANG-CTOR-PARAM-SELECT-"))));
		ASSERT_THAT(AreEqual(148, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/CtorParamSelectGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("CtorParamSelectGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
