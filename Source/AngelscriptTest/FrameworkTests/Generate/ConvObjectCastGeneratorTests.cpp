#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestConvObjectCastGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ConvObjectCastGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString FactoryNeedle;
		FString FormNeedle;
		int32 Expected = 0;
		bool bReject = false;
	};

	static bool IndependentImplicit(const TCHAR* Source, const TCHAR* Target)
	{
		return FCString::Strcmp(Source, Target) == 0 || FCString::Strcmp(Target, TEXT("BASE")) == 0;
	}

	static bool IndependentCompile(const TCHAR* Source, const TCHAR* Target, const TCHAR* Form)
	{
		if (FCString::Strcmp(Form, TEXT("EXPLICIT_CAST")) == 0)
		{
			return IndependentImplicit(Source, Target)
				|| (FCString::Strcmp(Source, TEXT("BASE")) == 0 && FCString::Strcmp(Target, TEXT("BASE")) != 0);
		}
		return IndependentImplicit(Source, Target);
	}

	static bool IndependentVisible(const TCHAR* Kind, const TCHAR* View)
	{
		if (FCString::Strcmp(Kind, TEXT("NULL")) == 0)
		{
			return false;
		}
		if (FCString::Strcmp(View, TEXT("BASE")) == 0)
		{
			return true;
		}
		return (FCString::Strcmp(Kind, TEXT("DERIVED")) == 0 && FCString::Strcmp(View, TEXT("DERIVED")) == 0)
			|| (FCString::Strcmp(Kind, TEXT("UNRELATED")) == 0 && FCString::Strcmp(View, TEXT("UNRELATED")) == 0);
	}

	static int32 IndependentExpected(const TCHAR* Kind, const TCHAR* Source, const TCHAR* Target)
	{
		if (!IndependentVisible(Kind, Source) || !IndependentVisible(Kind, Target))
		{
			return -1;
		}
		if (FCString::Strcmp(Kind, TEXT("BASE")) == 0)
		{
			return 1001;
		}
		if (FCString::Strcmp(Kind, TEXT("DERIVED")) == 0)
		{
			return 2001;
		}
		return 3001;
	}

	static FString IndependentFactory(const TCHAR* Kind)
	{
		if (FCString::Strcmp(Kind, TEXT("BASE")) == 0)
		{
			return TEXT("MakeObjectCastBase()");
		}
		if (FCString::Strcmp(Kind, TEXT("DERIVED")) == 0)
		{
			return TEXT("MakeObjectCastDerived()");
		}
		if (FCString::Strcmp(Kind, TEXT("UNRELATED")) == 0)
		{
			return TEXT("MakeObjectCastUnrelated()");
		}
		return TEXT("MakeObjectCastNull()");
	}

	static FString IndependentFormNeedle(const TCHAR* Form, const TCHAR* TargetType, const TCHAR* TargetSuffix)
	{
		if (FCString::Strcmp(Form, TEXT("ASSIGNMENT")) == 0)
		{
			return TEXT("TargetValue = SourceValue;");
		}
		if (FCString::Strcmp(Form, TEXT("INITIALIZER")) == 0)
		{
			return FString::Printf(TEXT("%s TargetValue = SourceValue;"), TargetType);
		}
		if (FCString::Strcmp(Form, TEXT("ARGUMENT")) == 0)
		{
			return TEXT("return ObserveObjectCastTarget(");
		}
		if (FCString::Strcmp(Form, TEXT("RETURN")) == 0)
		{
			return FString::Printf(TEXT("TargetValue = ReturnObjectCast%s(SourceValue);"), TargetSuffix);
		}
		return FString::Printf(TEXT("TargetValue = cast<%s>(SourceValue);"), TargetType);
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kKinds[] = {TEXT("BASE"), TEXT("DERIVED"), TEXT("UNRELATED"), TEXT("NULL")};
		static const TCHAR* const kViews[] = {TEXT("BASE"), TEXT("DERIVED"), TEXT("UNRELATED")};
		static const TCHAR* const kForms[] = {
			TEXT("ASSIGNMENT"), TEXT("INITIALIZER"), TEXT("ARGUMENT"), TEXT("RETURN"), TEXT("EXPLICIT_CAST")};
		static const TCHAR* const kViewTypes[] = {TEXT("FObjectCastBase"), TEXT("FObjectCastDerived"), TEXT("FObjectCastUnrelated")};
		static const TCHAR* const kSuffixes[] = {TEXT("Base"), TEXT("Derived"), TEXT("Unrelated")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(180);
		for (int32 Index = 0; Index < 180; ++Index)
		{
			const int32 KindIndex = Index / 45;
			const int32 SourceIndex = (Index / 15) % 3;
			const int32 TargetIndex = (Index / 5) % 3;
			const int32 FormIndex = Index % 5;
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-CONV-OBJECT-CAST-%s-%s-%s-%s"),
				kKinds[KindIndex],
				kViews[SourceIndex],
				kViews[TargetIndex],
				kForms[FormIndex]);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = !IndependentCompile(kViews[SourceIndex], kViews[TargetIndex], kForms[FormIndex]);
			Cell.Expected = Cell.bReject
				? 0
				: IndependentExpected(kKinds[KindIndex], kViews[SourceIndex], kViews[TargetIndex]);
			Cell.FactoryNeedle = IndependentFactory(kKinds[KindIndex]);
			Cell.FormNeedle = IndependentFormNeedle(kForms[FormIndex], kViewTypes[TargetIndex], kSuffixes[TargetIndex]);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EConvObjectCastForm;
		using AngelscriptTest::Generate::EConvObjectCastKind;
		using AngelscriptTest::Generate::EConvObjectCastView;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FConvObjectCastGenerator;
		using AngelscriptTest::Generate::FConvObjectCastParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FConvObjectCastGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CONV-OBJECT-CAST")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangConvObjectCastBaseBaseBaseAssignment")),
			MakeCanonicalEntryName(TEXT("LANG-CONV-OBJECT-CAST-BASE-BASE-BASE-ASSIGNMENT"))));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile = AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ConvObjectCastGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ConvObjectCastGenerator dump path=%s cases=180 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(108, CaseCount));
		ASSERT_THAT(AreEqual(180, Cases.Num()));
		ASSERT_THAT(AreEqual(72, RejectIds.Num()));

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
				ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.FormNeedle)));
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
			}
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("MakeObjectCast"))));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
			}
		}
		ASSERT_THAT(AreEqual(72, RejectCount));

		const TCHAR* const ProbeCaseId = TEXT("LANG-CONV-OBJECT-CAST-BASE-BASE-BASE-ASSIGNMENT");
		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		const FString OwnedAgain = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(IsTrue(OwnedCase.Len() > 0));
		ASSERT_THAT(AreEqual(OwnedCase, OwnedAgain));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangConvObjectCastBaseBaseBaseAssignment()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangConvObjectCastBaseBaseBaseAssignment()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int ObserveObjectCastTarget(FObjectCastBase Value)"))));

		FConvObjectCastParams DefaultParams;
		const FString DefaultSingle = Generator.BuildObjectCastSource(DefaultParams);
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsFalse(DefaultSingle.Contains(TEXT("int EntryLang"))));

		FConvObjectCastParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		const FString ProbeSource = Generator.BuildObjectCastSource(ProbeParams);
		ASSERT_THAT(IsTrue(ProbeSource.Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(ProbeSource.Contains(TEXT("int Entry()"))));

		const FString Representative = LoadGold(TEXT("LanguageGenerators/ConvObjectCastGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			const FString CaseSource = Expected.bReject
				? Generator.BuildRejectSource(Expected.CaseId)
				: Generator.BuildCaseSource(Expected.CaseId);
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.FactoryNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.FormNeedle)));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FConvObjectCastParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildObjectCastSource(InvalidName).IsEmpty()));
		FConvObjectCastParams InvalidEnum = DefaultParams;
		InvalidEnum.Kind = static_cast<EConvObjectCastKind>(4);
		ASSERT_THAT(IsTrue(Generator.BuildObjectCastSource(InvalidEnum).IsEmpty()));
		FConvObjectCastParams RejectParams = DefaultParams;
		RejectParams.Source = EConvObjectCastView::Base;
		RejectParams.Target = EConvObjectCastView::Derived;
		ASSERT_THAT(IsTrue(Generator.BuildObjectCastSource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-CONV-NUMERIC-INT8-INT8-ASSIGNMENT-ZERO")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildRejectSource(ProbeCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(ExpectedCells[0].Expected, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(OutputFile.EndsWith(TEXT("GeneratedCases/ConvObjectCastGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\ConvObjectCastGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(180, CountOccurrences(Dump, TEXT("// Case: LANG-CONV-OBJECT-CAST-"))));
		ASSERT_THAT(AreEqual(72, CountOccurrences(Dump, TEXT("// Reject module"))));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			ASSERT_THAT(IsTrue(Dump.Contains(FString::Printf(TEXT("// Case: %s"), *Expected.CaseId))));
			FString DumpBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Dump, Expected.EntryName, DumpBody)));
			if (!Expected.bReject)
			{
				FString AggregateBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, AggregateBody)));
				ASSERT_THAT(AreEqual(AggregateBody, DumpBody));
			}
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
			TEXT("C:/no-such-root/ConvObjectCastGenerator.as"), Dump, ProbeError)));
		ASSERT_THAT(IsTrue(ProbeError.Len() > 0));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ConvObjectCastGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
