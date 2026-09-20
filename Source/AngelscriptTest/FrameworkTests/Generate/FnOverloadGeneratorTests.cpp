#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestFnOverloadGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	FnOverloadGenerator,
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

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-FN-OVERLOAD-TYPE-EXACT");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static int32 IndependentMarker(const TCHAR* Discriminator, const TCHAR* Outcome)
	{
		const int32 Base =
			FCString::Strcmp(Discriminator, TEXT("TYPE")) == 0 ? 100
			: FCString::Strcmp(Discriminator, TEXT("ARITY")) == 0 ? 200
			: FCString::Strcmp(Discriminator, TEXT("CONST")) == 0 ? 300
			: FCString::Strcmp(Discriminator, TEXT("DIRECTION")) == 0 ? 400
			: FCString::Strcmp(Discriminator, TEXT("NAMESPACE")) == 0 ? 500
			: FCString::Strcmp(Discriminator, TEXT("DEFAULT")) == 0 ? 600
			: 700;
		return Base + (FCString::Strcmp(Outcome, TEXT("EXACT")) == 0 ? 1 : 2);
	}

	static FString IndependentNeedle(const TCHAR* Discriminator, const TCHAR* Outcome)
	{
		if (FCString::Strcmp(Discriminator, TEXT("TYPE")) == 0 || FCString::Strcmp(Discriminator, TEXT("CONVERSION")) == 0)
		{
			if (FCString::Strcmp(Outcome, TEXT("EXACT")) == 0)
			{
				return FCString::Strcmp(Discriminator, TEXT("CONVERSION")) == 0
					? TEXT("int(7)")
					: TEXT("(7);");
			}
			if (FCString::Strcmp(Outcome, TEXT("PROMOTION")) == 0)
			{
				return TEXT("double Value");
			}
			if (FCString::Strcmp(Outcome, TEXT("AMBIGUOUS")) == 0)
			{
				return TEXT("float Value");
			}
			return TEXT("(true);");
		}
		if (FCString::Strcmp(Discriminator, TEXT("ARITY")) == 0 || FCString::Strcmp(Discriminator, TEXT("DEFAULT")) == 0)
		{
			if (FCString::Strcmp(Outcome, TEXT("EXACT")) == 0)
			{
				return TEXT("(3, 4);");
			}
			if (FCString::Strcmp(Outcome, TEXT("PROMOTION")) == 0)
			{
				return TEXT("double Left, double Right");
			}
			if (FCString::Strcmp(Outcome, TEXT("AMBIGUOUS")) == 0)
			{
				return TEXT("int Extra = 2");
			}
			return FCString::Strcmp(Discriminator, TEXT("DEFAULT")) == 0
				? TEXT("();")
				: TEXT("(1, 2, 3);");
		}
		if (FCString::Strcmp(Discriminator, TEXT("CONST")) == 0)
		{
			return TEXT("Owner.Invoke();");
		}
		if (FCString::Strcmp(Discriminator, TEXT("DIRECTION")) == 0)
		{
			if (FCString::Strcmp(Outcome, TEXT("MISSING")) == 0)
			{
				return TEXT("int& out Value");
			}
			return TEXT("& in Value");
		}
		if (FCString::Strcmp(Outcome, TEXT("MISSING")) == 0)
		{
			return TEXT("Missing::Probe(7);");
		}
		return TEXT("namespace A");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kDiscriminators[] = {
			TEXT("TYPE"), TEXT("ARITY"), TEXT("CONST"), TEXT("DIRECTION"),
			TEXT("NAMESPACE"), TEXT("DEFAULT"), TEXT("CONVERSION")};
		static const TCHAR* const kOutcomes[] = {
			TEXT("EXACT"), TEXT("PROMOTION"), TEXT("AMBIGUOUS"), TEXT("MISSING")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(28);
		for (int32 Index = 0; Index < 28; ++Index)
		{
			const TCHAR* const Discriminator = kDiscriminators[Index / 4];
			const TCHAR* const Outcome = kOutcomes[Index % 4];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-FN-OVERLOAD-%s-%s"), Discriminator, Outcome);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = FCString::Strcmp(Outcome, TEXT("AMBIGUOUS")) == 0
				|| FCString::Strcmp(Outcome, TEXT("MISSING")) == 0;
			Cell.Expected = Cell.bReject ? 0 : IndependentMarker(Discriminator, Outcome);
			Cell.Needle = IndependentNeedle(Discriminator, Outcome);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EFnOverloadDiscriminator;
		using AngelscriptTest::Generate::EFnOverloadOutcome;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FFnOverloadGenerator;
		using AngelscriptTest::Generate::FFnOverloadParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FFnOverloadGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-FN-OVERLOAD")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangFnOverloadTypeExact")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("FnOverloadGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("FnOverloadGenerator dump path=%s cases=28 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(14, CaseCount));
		ASSERT_THAT(AreEqual(28, Cases.Num()));
		ASSERT_THAT(AreEqual(14, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangFnOverloadTypeExact()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int EntryLangFnOverloadTypeAmbiguous()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/FnOverloadGenerator/RepresentativeSources.as"));
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
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				const FString Isolated = Generator.BuildCaseSource(Expected.CaseId);
				ASSERT_THAT(IsTrue(Isolated.Contains(Expected.Needle)));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
		}
		ASSERT_THAT(AreEqual(14, RejectCount));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangFnOverloadTypeExact()"))));

		FFnOverloadParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildOverloadSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FFnOverloadParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildOverloadSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(Generator.BuildOverloadSource(ProbeParams).Contains(TEXT("int Entry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildOverloadSource(ProbeParams).IsEmpty()));
		FFnOverloadParams InvalidEnum = DefaultParams;
		InvalidEnum.Discriminator = static_cast<EFnOverloadDiscriminator>(7);
		ASSERT_THAT(IsTrue(Generator.BuildOverloadSource(InvalidEnum).IsEmpty()));
		FFnOverloadParams RejectParams = DefaultParams;
		RejectParams.Outcome = EFnOverloadOutcome::Ambiguous;
		ASSERT_THAT(IsTrue(Generator.BuildOverloadSource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(ExpectedCells[0].Expected, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(28, CountOccurrences(Dump, TEXT("// Case: LANG-FN-OVERLOAD-"))));
		ASSERT_THAT(AreEqual(14, CountOccurrences(Dump, TEXT("// Reject module"))));
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

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/FnOverloadGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("FnOverloadGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
