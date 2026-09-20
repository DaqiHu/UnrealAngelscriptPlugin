#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestConvValueObjectGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ConvValueObjectGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString FormNeedle;
		int32 Expected = 0;
		bool bReject = false;
	};

	static bool IndependentExplicitForm(const TCHAR* Form)
	{
		return FCString::Strcmp(Form, TEXT("EXPLICIT_CAST")) == 0
			|| FCString::Strcmp(Form, TEXT("DIRECT_CONSTRUCTOR")) == 0;
	}

	static bool IndependentImplicitPath(const TCHAR* Availability, const TCHAR* Target)
	{
		if (FCString::Strcmp(Target, TEXT("SAME_VALUE")) == 0)
		{
			return true;
		}
		if (FCString::Strcmp(Target, TEXT("OTHER_VALUE")) == 0)
		{
			return FCString::Strcmp(Availability, TEXT("IMPLICIT_CONSTRUCTOR")) == 0
				|| FCString::Strcmp(Availability, TEXT("IMPLICIT_OPERATOR")) == 0
				|| FCString::Strcmp(Availability, TEXT("CONSTRUCTOR_AND_OPERATOR")) == 0;
		}
		return FCString::Strcmp(Availability, TEXT("IMPLICIT_OPERATOR")) == 0
			|| FCString::Strcmp(Availability, TEXT("CONSTRUCTOR_AND_OPERATOR")) == 0;
	}

	static bool IndependentExplicitPath(const TCHAR* Availability, const TCHAR* Target)
	{
		if (FCString::Strcmp(Target, TEXT("SAME_VALUE")) == 0)
		{
			return true;
		}
		if (FCString::Strcmp(Target, TEXT("OTHER_VALUE")) == 0)
		{
			return FCString::Strcmp(Availability, TEXT("NONE")) != 0;
		}
		return FCString::Strcmp(Availability, TEXT("IMPLICIT_OPERATOR")) == 0
			|| FCString::Strcmp(Availability, TEXT("EXPLICIT_OPERATOR")) == 0
			|| FCString::Strcmp(Availability, TEXT("CONSTRUCTOR_AND_OPERATOR")) == 0;
	}

	static bool IndependentCompile(const TCHAR* Availability, const TCHAR* Target, const TCHAR* Form, const TCHAR* Outcome)
	{
		if (FCString::Strcmp(Outcome, TEXT("AMBIGUOUS")) == 0 || FCString::Strcmp(Outcome, TEXT("REJECTED")) == 0)
		{
			return false;
		}
		return IndependentExplicitForm(Form)
			? IndependentExplicitPath(Availability, Target)
			: IndependentImplicitPath(Availability, Target);
	}

	static int32 IndependentExpected(const TCHAR* Availability, const TCHAR* Target)
	{
		if (FCString::Strcmp(Target, TEXT("SAME_VALUE")) == 0)
		{
			return 7;
		}
		if (FCString::Strcmp(Target, TEXT("OTHER_VALUE")) == 0)
		{
			if (FCString::Strcmp(Availability, TEXT("IMPLICIT_CONSTRUCTOR")) == 0
				|| FCString::Strcmp(Availability, TEXT("EXPLICIT_CONSTRUCTOR")) == 0
				|| FCString::Strcmp(Availability, TEXT("CONSTRUCTOR_AND_OPERATOR")) == 0)
			{
				return 107;
			}
			return FCString::Strcmp(Availability, TEXT("IMPLICIT_OPERATOR")) == 0 ? 207 : 307;
		}
		if (FCString::Strcmp(Target, TEXT("BOOL")) == 0)
		{
			return FCString::Strcmp(Availability, TEXT("EXPLICIT_OPERATOR")) == 0 ? 0 : 1;
		}
		return FCString::Strcmp(Availability, TEXT("EXPLICIT_OPERATOR")) == 0 ? 307 : 207;
	}

	static FString IndependentFormNeedle(const TCHAR* Form, const TCHAR* Outcome)
	{
		if (FCString::Strcmp(Outcome, TEXT("AMBIGUOUS")) == 0)
		{
			return TEXT("SelectValueAmbiguity");
		}
		if (FCString::Strcmp(Outcome, TEXT("REJECTED")) == 0)
		{
			return TEXT("RejectValueConversion");
		}
		if (FCString::Strcmp(Form, TEXT("ASSIGNMENT")) == 0)
		{
			return TEXT("TargetValue = SourceValue;");
		}
		if (FCString::Strcmp(Form, TEXT("INITIALIZER")) == 0)
		{
			return TEXT("TargetValue = SourceValue;");
		}
		if (FCString::Strcmp(Form, TEXT("ARGUMENT")) == 0)
		{
			return TEXT("ObserveValueConversion");
		}
		if (FCString::Strcmp(Form, TEXT("RETURN")) == 0)
		{
			return TEXT("ReturnValueConversionTarget");
		}
		if (FCString::Strcmp(Form, TEXT("EXPLICIT_CAST")) == 0)
		{
			return TEXT("TargetValue = ");
		}
		return TEXT("TargetValue(SourceValue);");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kAvailability[] = {
			TEXT("IMPLICIT_CONSTRUCTOR"), TEXT("EXPLICIT_CONSTRUCTOR"), TEXT("IMPLICIT_OPERATOR"),
			TEXT("EXPLICIT_OPERATOR"), TEXT("CONSTRUCTOR_AND_OPERATOR"), TEXT("NONE")};
		static const TCHAR* const kTargets[] = {
			TEXT("SAME_VALUE"), TEXT("OTHER_VALUE"), TEXT("INT"), TEXT("FLOAT64"), TEXT("BOOL")};
		static const TCHAR* const kForms[] = {
			TEXT("ASSIGNMENT"), TEXT("INITIALIZER"), TEXT("ARGUMENT"),
			TEXT("RETURN"), TEXT("EXPLICIT_CAST"), TEXT("DIRECT_CONSTRUCTOR")};
		static const TCHAR* const kOutcomes[] = {
			TEXT("DIRECT"), TEXT("SELECTED"), TEXT("AMBIGUOUS"), TEXT("REJECTED")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(720);
		for (int32 Index = 0; Index < 720; ++Index)
		{
			const TCHAR* const Availability = kAvailability[Index / 120];
			const TCHAR* const Target = kTargets[(Index / 24) % 5];
			const TCHAR* const Form = kForms[(Index / 4) % 6];
			const TCHAR* const Outcome = kOutcomes[Index % 4];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-CONV-VALUE-OBJECT-%s-%s-%s-%s"),
				Availability,
				Target,
				Form,
				Outcome);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = !IndependentCompile(Availability, Target, Form, Outcome);
			Cell.Expected = Cell.bReject ? 0 : IndependentExpected(Availability, Target);
			Cell.FormNeedle = IndependentFormNeedle(Form, Outcome);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EConvValueObjectAvailability;
		using AngelscriptTest::Generate::EConvValueObjectForm;
		using AngelscriptTest::Generate::EConvValueObjectOutcome;
		using AngelscriptTest::Generate::EConvValueObjectTarget;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FConvValueObjectGenerator;
		using AngelscriptTest::Generate::FConvValueObjectParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FConvValueObjectGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CONV-VALUE-OBJECT")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangConvValueObjectImplicitConstructorSameValueAssignmentDirect")),
			MakeCanonicalEntryName(TEXT("LANG-CONV-VALUE-OBJECT-IMPLICIT_CONSTRUCTOR-SAME_VALUE-ASSIGNMENT-DIRECT"))));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile = AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ConvValueObjectGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ConvValueObjectGenerator dump path=%s cases=720 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(200, CaseCount));
		ASSERT_THAT(AreEqual(720, Cases.Num()));
		ASSERT_THAT(AreEqual(520, RejectIds.Num()));

		int32 RejectCount = 0;
		const TCHAR* const ZeroCaseId = TEXT("LANG-CONV-VALUE-OBJECT-EXPLICIT_OPERATOR-BOOL-EXPLICIT_CAST-DIRECT");
		bool bFoundZero = false;
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
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				if (Expected.CaseId == ZeroCaseId)
				{
					bFoundZero = true;
					ASSERT_THAT(AreEqual(0, Expected.Expected));
				}
			}
		}
		ASSERT_THAT(AreEqual(520, RejectCount));
		ASSERT_THAT(IsTrue(bFoundZero));

		const TCHAR* const ProbeCaseId = TEXT("LANG-CONV-VALUE-OBJECT-IMPLICIT_CONSTRUCTOR-SAME_VALUE-ASSIGNMENT-DIRECT");
		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		const FString OwnedAgain = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(IsTrue(OwnedCase.Len() > 0));
		ASSERT_THAT(AreEqual(OwnedCase, OwnedAgain));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangConvValueObjectImplicitConstructorSameValueAssignmentDirect()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangConvValueObjectImplicitConstructorSameValueAssignmentDirect()"))));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("SourceValue(7);"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));

		FConvValueObjectParams DefaultParams;
		const FString DefaultSingle = Generator.BuildValueObjectConversionSource(DefaultParams);
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsFalse(DefaultSingle.Contains(TEXT("int EntryLang"))));

		FConvValueObjectParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		const FString ProbeSource = Generator.BuildValueObjectConversionSource(ProbeParams);
		ASSERT_THAT(IsTrue(ProbeSource.Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(ProbeSource.Contains(TEXT("int Entry()"))));

		const FString Representative = LoadGold(TEXT("LanguageGenerators/ConvValueObjectGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			const FString CaseSource = Expected.bReject
				? Generator.BuildRejectSource(Expected.CaseId)
				: Generator.BuildCaseSource(Expected.CaseId);
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("SourceValue(7);"))));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.FormNeedle)));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FConvValueObjectParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildValueObjectConversionSource(InvalidName).IsEmpty()));
		FConvValueObjectParams InvalidEnum = DefaultParams;
		InvalidEnum.Availability = static_cast<EConvValueObjectAvailability>(6);
		ASSERT_THAT(IsTrue(Generator.BuildValueObjectConversionSource(InvalidEnum).IsEmpty()));
		FConvValueObjectParams RejectParams = DefaultParams;
		RejectParams.Outcome = EConvValueObjectOutcome::Rejected;
		ASSERT_THAT(IsTrue(Generator.BuildValueObjectConversionSource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-CONV-NUMERIC-INT8-INT8-ASSIGNMENT-ZERO")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildRejectSource(ProbeCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(ZeroCaseId)));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(ExpectedCells[0].Expected, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(OutputFile.EndsWith(TEXT("GeneratedCases/ConvValueObjectGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\ConvValueObjectGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(720, CountOccurrences(Dump, TEXT("// Case: LANG-CONV-VALUE-OBJECT-"))));
		ASSERT_THAT(AreEqual(520, CountOccurrences(Dump, TEXT("// Reject module"))));
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
			TEXT("C:/no-such-root/ConvValueObjectGenerator.as"), Dump, ProbeError)));
		ASSERT_THAT(IsTrue(ProbeError.Len() > 0));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ConvValueObjectGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
