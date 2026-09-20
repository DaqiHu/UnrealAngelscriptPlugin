#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestConvOverloadGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ConvOverloadGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString SourceNeedle;
		FString ContextNeedle;
		int32 Expected = 0;
		bool bReject = false;
	};

	static int32 IndependentSourceMarker(const TCHAR* Conversion)
	{
		if (FCString::Strcmp(Conversion, TEXT("IDENTITY")) == 0)
		{
			return 11;
		}
		if (FCString::Strcmp(Conversion, TEXT("PROMOTION")) == 0)
		{
			return 12;
		}
		if (FCString::Strcmp(Conversion, TEXT("NARROWING")) == 0)
		{
			return 13;
		}
		if (FCString::Strcmp(Conversion, TEXT("CONSTRUCTOR")) == 0)
		{
			return 14;
		}
		if (FCString::Strcmp(Conversion, TEXT("OPERATOR")) == 0)
		{
			return 15;
		}
		return -1;
	}

	static int32 IndependentSelectedMarker(const TCHAR* Conversion, const TCHAR* Outcome)
	{
		if (FCString::Strcmp(Outcome, TEXT("EXACT")) == 0)
		{
			return IndependentSourceMarker(Conversion);
		}
		if (FCString::Strcmp(Conversion, TEXT("CONSTRUCTOR")) == 0)
		{
			return 114;
		}
		if (FCString::Strcmp(Conversion, TEXT("OPERATOR")) == 0)
		{
			return 215;
		}
		return IndependentSourceMarker(Conversion);
	}

	static int32 IndependentExpected(const TCHAR* Context, const TCHAR* Conversion, const TCHAR* Outcome)
	{
		const int32 Marker = IndependentSelectedMarker(Conversion, Outcome);
		if (FCString::Strcmp(Context, TEXT("OVERLOAD")) == 0)
		{
			return Marker + 100;
		}
		if (FCString::Strcmp(Context, TEXT("OPERATOR")) == 0)
		{
			return Marker + 7;
		}
		if (FCString::Strcmp(Context, TEXT("DEFAULT_ARGUMENT")) == 0)
		{
			return Marker + 5;
		}
		if (FCString::Strcmp(Context, TEXT("INDEX")) == 0)
		{
			return Marker + 1;
		}
		return Marker;
	}

	static FString IndependentSourceNeedle(const TCHAR* Conversion)
	{
		if (FCString::Strcmp(Conversion, TEXT("IDENTITY")) == 0)
		{
			return TEXT("int SourceValue = 11;");
		}
		if (FCString::Strcmp(Conversion, TEXT("PROMOTION")) == 0)
		{
			return TEXT("int8 SourceValue = 12;");
		}
		if (FCString::Strcmp(Conversion, TEXT("NARROWING")) == 0)
		{
			return TEXT("int64 SourceValue = 13;");
		}
		if (FCString::Strcmp(Conversion, TEXT("CONSTRUCTOR")) == 0)
		{
			return TEXT("int SourceValue = 14;");
		}
		if (FCString::Strcmp(Conversion, TEXT("OPERATOR")) == 0)
		{
			return TEXT("SourceValue(15);");
		}
		return TEXT("SourceValue = nullptr;");
	}

	static FString IndependentContextNeedle(const TCHAR* Context, const TCHAR* Outcome)
	{
		if (FCString::Strcmp(Outcome, TEXT("AMBIGUOUS")) == 0)
		{
			if (FCString::Strcmp(Context, TEXT("OVERLOAD")) == 0)
			{
				return TEXT("SelectAmbiguousResolution");
			}
			if (FCString::Strcmp(Context, TEXT("OPERATOR")) == 0)
			{
				return TEXT("Context + SourceValue;");
			}
			if (FCString::Strcmp(Context, TEXT("DEFAULT_ARGUMENT")) == 0)
			{
				return TEXT("DefaultAmbiguous");
			}
			if (FCString::Strcmp(Context, TEXT("PROPERTY")) == 0)
			{
				return TEXT("Holder.Value = SourceValue;");
			}
			if (FCString::Strcmp(Context, TEXT("INDEX")) == 0)
			{
				return TEXT("Index[SourceValue]");
			}
			return TEXT("SelectAmbiguousResolution");
		}
		if (FCString::Strcmp(Outcome, TEXT("REJECTED")) == 0)
		{
			if (FCString::Strcmp(Context, TEXT("OVERLOAD")) == 0)
			{
				return TEXT("SelectRejectedResolution");
			}
			if (FCString::Strcmp(Context, TEXT("OPERATOR")) == 0)
			{
				return TEXT("Context + SourceValue;");
			}
			if (FCString::Strcmp(Context, TEXT("DEFAULT_ARGUMENT")) == 0)
			{
				return TEXT("DefaultRejectedResolution");
			}
			if (FCString::Strcmp(Context, TEXT("PROPERTY")) == 0)
			{
				return TEXT("Holder.Value = SourceValue;");
			}
			if (FCString::Strcmp(Context, TEXT("INDEX")) == 0)
			{
				return TEXT("Index[SourceValue]");
			}
			return TEXT("TargetValue = true ? SourceValue : TargetValue;");
		}
		if (FCString::Strcmp(Context, TEXT("OVERLOAD")) == 0)
		{
			return TEXT("SelectResolution");
		}
		if (FCString::Strcmp(Context, TEXT("OPERATOR")) == 0)
		{
			return TEXT("Context + SourceValue;");
		}
		if (FCString::Strcmp(Context, TEXT("DEFAULT_ARGUMENT")) == 0)
		{
			return TEXT("DefaultResolution");
		}
		if (FCString::Strcmp(Context, TEXT("PROPERTY")) == 0)
		{
			return TEXT("Holder.Value = SourceValue;");
		}
		if (FCString::Strcmp(Context, TEXT("INDEX")) == 0)
		{
			return TEXT("Index[SourceValue]");
		}
		return TEXT("true ? SourceValue :");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kContexts[] = {
			TEXT("OVERLOAD"), TEXT("OPERATOR"), TEXT("DEFAULT_ARGUMENT"),
			TEXT("PROPERTY"), TEXT("INDEX"), TEXT("CONDITIONAL")};
		static const TCHAR* const kConversions[] = {
			TEXT("IDENTITY"), TEXT("PROMOTION"), TEXT("NARROWING"),
			TEXT("CONSTRUCTOR"), TEXT("OPERATOR"), TEXT("REFERENCE_CAST")};
		static const TCHAR* const kOutcomes[] = {
			TEXT("EXACT"), TEXT("SELECTED_CONVERSION"), TEXT("AMBIGUOUS"), TEXT("REJECTED")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(144);
		for (int32 Index = 0; Index < 144; ++Index)
		{
			const TCHAR* const Context = kContexts[Index / 24];
			const TCHAR* const Conversion = kConversions[(Index / 4) % 6];
			const TCHAR* const Outcome = kOutcomes[Index % 4];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-CONV-OVERLOAD-%s-%s-%s"), Context, Conversion, Outcome);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = FCString::Strcmp(Outcome, TEXT("AMBIGUOUS")) == 0
				|| FCString::Strcmp(Outcome, TEXT("REJECTED")) == 0;
			Cell.Expected = Cell.bReject ? 0 : IndependentExpected(Context, Conversion, Outcome);
			Cell.SourceNeedle = IndependentSourceNeedle(Conversion);
			Cell.ContextNeedle = IndependentContextNeedle(Context, Outcome);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EConvOverloadContext;
		using AngelscriptTest::Generate::EConvOverloadConversion;
		using AngelscriptTest::Generate::EConvOverloadOutcome;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FConvOverloadGenerator;
		using AngelscriptTest::Generate::FConvOverloadParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FConvOverloadGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CONV-OVERLOAD")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangConvOverloadOverloadIdentityExact")),
			MakeCanonicalEntryName(TEXT("LANG-CONV-OVERLOAD-OVERLOAD-IDENTITY-EXACT"))));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile = AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ConvOverloadGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ConvOverloadGenerator dump path=%s cases=144 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(72, CaseCount));
		ASSERT_THAT(AreEqual(144, Cases.Num()));
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
				ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.ContextNeedle)));
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
			}
		}
		ASSERT_THAT(AreEqual(72, RejectCount));

		const TCHAR* const ProbeCaseId = TEXT("LANG-CONV-OVERLOAD-OVERLOAD-IDENTITY-EXACT");
		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		const FString OwnedAgain = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(IsTrue(OwnedCase.Len() > 0));
		ASSERT_THAT(AreEqual(OwnedCase, OwnedAgain));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangConvOverloadOverloadIdentityExact()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangConvOverloadOverloadIdentityExact()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));

		FConvOverloadParams DefaultParams;
		const FString DefaultSingle = Generator.BuildResolutionSource(DefaultParams);
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsFalse(DefaultSingle.Contains(TEXT("int EntryLang"))));

		FConvOverloadParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		const FString ProbeSource = Generator.BuildResolutionSource(ProbeParams);
		ASSERT_THAT(IsTrue(ProbeSource.Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(ProbeSource.Contains(TEXT("int Entry()"))));

		const FString Representative = LoadGold(TEXT("LanguageGenerators/ConvOverloadGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			const FString CaseSource = Expected.bReject
				? Generator.BuildRejectSource(Expected.CaseId)
				: Generator.BuildCaseSource(Expected.CaseId);
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.SourceNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.ContextNeedle)));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FConvOverloadParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildResolutionSource(InvalidName).IsEmpty()));
		FConvOverloadParams InvalidEnum = DefaultParams;
		InvalidEnum.Context = static_cast<EConvOverloadContext>(6);
		ASSERT_THAT(IsTrue(Generator.BuildResolutionSource(InvalidEnum).IsEmpty()));
		FConvOverloadParams RejectParams = DefaultParams;
		RejectParams.Outcome = EConvOverloadOutcome::Ambiguous;
		ASSERT_THAT(IsTrue(Generator.BuildResolutionSource(RejectParams).IsEmpty()));
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

		ASSERT_THAT(IsTrue(OutputFile.EndsWith(TEXT("GeneratedCases/ConvOverloadGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\ConvOverloadGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(144, CountOccurrences(Dump, TEXT("// Case: LANG-CONV-OVERLOAD-"))));
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
			TEXT("C:/no-such-root/ConvOverloadGenerator.as"), Dump, ProbeError)));
		ASSERT_THAT(IsTrue(ProbeError.Len() > 0));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ConvOverloadGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
