#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestFnDefaultsGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	FnDefaultsGenerator,
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

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-FN-DEFAULTS-NONE-FINAL_ONE-GLOBAL");

	static int32 IndependentOptionalCount(const TCHAR* Pattern)
	{
		if (FCString::Strcmp(Pattern, TEXT("FINAL_ONE")) == 0)
		{
			return 1;
		}
		if (FCString::Strcmp(Pattern, TEXT("FINAL_MANY")) == 0)
		{
			return 2;
		}
		if (FCString::Strcmp(Pattern, TEXT("ALL_OPTIONAL")) == 0
			|| FCString::Strcmp(Pattern, TEXT("EXPLICIT_OVERRIDE")) == 0
			|| FCString::Strcmp(Pattern, TEXT("MIXED_OMITTED_PROVIDED")) == 0)
		{
			return 3;
		}
		return 0;
	}

	static int32 IndependentOmitted(const TCHAR* Omission)
	{
		if (FCString::Strcmp(Omission, TEXT("ONE")) == 0)
		{
			return 1;
		}
		if (FCString::Strcmp(Omission, TEXT("MANY")) == 0)
		{
			return 2;
		}
		return 0;
	}

	static bool IndependentReject(const TCHAR* Omission, const TCHAR* Pattern)
	{
		const int32 Omitted = IndependentOmitted(Omission);
		if (FCString::Strcmp(Pattern, TEXT("TYPE_INVALID")) == 0)
		{
			return false;
		}
		if (FCString::Strcmp(Pattern, TEXT("EARLIER_PARAMETER_REFERENCE_INVALID")) == 0)
		{
			return Omitted > 1;
		}
		if (FCString::Strcmp(Pattern, TEXT("NON_TRAILING_INVALID")) == 0)
		{
			return true;
		}
		return Omitted > IndependentOptionalCount(Pattern);
	}

	static int32 IndependentExpected(const TCHAR* Omission, const TCHAR* Pattern)
	{
		if (IndependentReject(Omission, Pattern))
		{
			return 0;
		}
		if (FCString::Strcmp(Pattern, TEXT("MIXED_OMITTED_PROVIDED")) == 0)
		{
			if (FCString::Strcmp(Omission, TEXT("NONE")) == 0)
			{
				return 456;
			}
			if (FCString::Strcmp(Omission, TEXT("ONE")) == 0)
			{
				return 426;
			}
			return 126;
		}
		if (FCString::Strcmp(Omission, TEXT("NONE")) == 0)
		{
			return 456;
		}
		if (FCString::Strcmp(Omission, TEXT("ONE")) == 0)
		{
			return 453;
		}
		return 423;
	}

	static FString IndependentNeedle(const TCHAR* Omission, const TCHAR* Pattern)
	{
		if (FCString::Strcmp(Pattern, TEXT("TYPE_INVALID")) == 0)
		{
			return TEXT("UnknownDefault");
		}
		if (FCString::Strcmp(Pattern, TEXT("EARLIER_PARAMETER_REFERENCE_INVALID")) == 0)
		{
			return TEXT("int B = A");
		}
		if (FCString::Strcmp(Pattern, TEXT("NON_TRAILING_INVALID")) == 0)
		{
			return TEXT("int A = 1, int B, int C = 3");
		}
		if (FCString::Strcmp(Pattern, TEXT("MIXED_OMITTED_PROVIDED")) == 0)
		{
			return FCString::Strcmp(Omission, TEXT("MANY")) == 0 ? TEXT("C: 6") : TEXT("A: 4");
		}
		if (FCString::Strcmp(Pattern, TEXT("FINAL_ONE")) == 0)
		{
			return TEXT("int C = 3");
		}
		return TEXT("A * 100 + B * 10 + C");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kOmissions[] = {TEXT("NONE"), TEXT("ONE"), TEXT("MANY")};
		static const TCHAR* const kPatterns[] = {
			TEXT("FINAL_ONE"), TEXT("FINAL_MANY"), TEXT("ALL_OPTIONAL"), TEXT("EXPLICIT_OVERRIDE"),
			TEXT("MIXED_OMITTED_PROVIDED"), TEXT("NON_TRAILING_INVALID"), TEXT("TYPE_INVALID"),
			TEXT("EARLIER_PARAMETER_REFERENCE_INVALID")};
		static const TCHAR* const kTargets[] = {TEXT("GLOBAL"), TEXT("NAMESPACE_GLOBAL"), TEXT("INSTANCE_METHOD")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(72);
		for (int32 Index = 0; Index < 72; ++Index)
		{
			const TCHAR* const Omission = kOmissions[Index / 24];
			const TCHAR* const Pattern = kPatterns[(Index / 3) % 8];
			const TCHAR* const Target = kTargets[Index % 3];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-FN-DEFAULTS-%s-%s-%s"), Omission, Pattern, Target);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Omission, Pattern);
			Cell.Expected = IndependentExpected(Omission, Pattern);
			Cell.Needle = IndependentNeedle(Omission, Pattern);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EFnDefaultsOmission;
		using AngelscriptTest::Generate::EFnDefaultsPattern;
		using AngelscriptTest::Generate::EFnDefaultsTarget;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FFnDefaultsGenerator;
		using AngelscriptTest::Generate::FFnDefaultsParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FFnDefaultsGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-FN-DEFAULTS")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangFnDefaultsNoneFinalOneGlobal")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("FnDefaultsGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("FnDefaultsGenerator dump path=%s cases=72 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(57, CaseCount));
		ASSERT_THAT(AreEqual(72, Cases.Num()));
		ASSERT_THAT(AreEqual(15, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/FnDefaultsGenerator/RepresentativeSources.as"));
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
				const FString RejectSource = Generator.BuildRejectSource(Expected.CaseId);
				ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.Needle)));
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
			}
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				ASSERT_THAT(IsTrue(Aggregate.Contains(Expected.Needle)));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				if (Representative.Contains(Expected.EntryName))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
		}
		ASSERT_THAT(AreEqual(15, RejectCount));

		FFnDefaultsParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildDefaultArgumentSource(DefaultParams).Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsTrue(Generator.BuildDefaultArgumentSource(DefaultParams).Contains(TEXT("4, 5, 6"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildDefaultArgumentSource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildDefaultArgumentSource(DefaultParams).IsEmpty()));
		DefaultParams.FunctionName.Reset();
		DefaultParams.Omission = EFnDefaultsOmission::Many;
		ASSERT_THAT(IsTrue(Generator.BuildDefaultArgumentSource(DefaultParams).IsEmpty()));
		DefaultParams.Pattern = static_cast<EFnDefaultsPattern>(8);
		ASSERT_THAT(IsTrue(Generator.BuildDefaultArgumentSource(DefaultParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(72, CountOccurrences(Dump, TEXT("// Case: LANG-FN-DEFAULTS-"))));
		ASSERT_THAT(AreEqual(15, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/FnDefaultsGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("FnDefaultsGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
