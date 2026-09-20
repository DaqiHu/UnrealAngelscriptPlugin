#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestConvFailureGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ConvFailureGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	enum class ECellKind : uint8
	{
		Normal,
		Fault,
		Reject,
	};

	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString Needle;
		FString Exception;
		ECellKind Kind = ECellKind::Reject;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-CONV-FAILURE-AMBIGUOUS_CONSTRUCTOR-FRESH_MODULE");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CONV-ABI-FLOAT-FLOAT32-ARGUMENT");

	static ECellKind IndependentKind(const TCHAR* Failure)
	{
		if (FCString::Strcmp(Failure, TEXT("AMBIGUOUS_CONSTRUCTOR")) == 0
			|| FCString::Strcmp(Failure, TEXT("AMBIGUOUS_OPERATOR")) == 0)
		{
			return ECellKind::Normal;
		}
		if (FCString::Strcmp(Failure, TEXT("NULL_VALUE_TARGET")) == 0
			|| FCString::Strcmp(Failure, TEXT("CONVERSION_EXCEPTION")) == 0
			|| FCString::Strcmp(Failure, TEXT("CONSTRUCTOR_EXCEPTION")) == 0)
		{
			return ECellKind::Fault;
		}
		return ECellKind::Reject;
	}

	static FString IndependentException(const TCHAR* Failure)
	{
		if (FCString::Strcmp(Failure, TEXT("NULL_VALUE_TARGET")) == 0)
		{
			return TEXT("Null pointer access");
		}
		if (FCString::Strcmp(Failure, TEXT("CONVERSION_EXCEPTION")) == 0
			|| FCString::Strcmp(Failure, TEXT("CONSTRUCTOR_EXCEPTION")) == 0)
		{
			return TEXT("Divide by zero");
		}
		return FString();
	}

	static FString IndependentNeedle(const TCHAR* Failure)
	{
		if (FCString::Strcmp(Failure, TEXT("IMPLICIT_NARROWING")) == 0)
		{
			return TEXT("RequireFailureEnum(SourceValue); // CONVERSION_CAUSE");
		}
		if (FCString::Strcmp(Failure, TEXT("NUMERIC_TO_ENUM")) == 0)
		{
			return TEXT("int SourceValue = 1;");
		}
		if (FCString::Strcmp(Failure, TEXT("UNRELATED_REFERENCE")) == 0)
		{
			return TEXT("RequireUnrelatedReference(SourceValue); // CONVERSION_CAUSE");
		}
		if (FCString::Strcmp(Failure, TEXT("BAD_DOWNCAST")) == 0)
		{
			return TEXT("RequireDerivedReference(SourceValue); // CONVERSION_CAUSE");
		}
		if (FCString::Strcmp(Failure, TEXT("NULL_VALUE_TARGET")) == 0)
		{
			return TEXT("return Target.Value; // CONVERSION_CAUSE");
		}
		if (FCString::Strcmp(Failure, TEXT("AMBIGUOUS_CONSTRUCTOR")) == 0)
		{
			return TEXT("SelectAmbiguousConstructor(int8(1)); // CONVERSION_CAUSE");
		}
		if (FCString::Strcmp(Failure, TEXT("AMBIGUOUS_OPERATOR")) == 0)
		{
			return TEXT("SourceValue + int8(1); // CONVERSION_CAUSE");
		}
		if (FCString::Strcmp(Failure, TEXT("EXPLICIT_ONLY_IMPLICIT_USE")) == 0)
		{
			return TEXT("int ConvertedValue = SourceValue; // CONVERSION_CAUSE");
		}
		if (FCString::Strcmp(Failure, TEXT("CONVERSION_EXCEPTION")) == 0)
		{
			return TEXT("FThrowingConversion SourceValue(9);");
		}
		if (FCString::Strcmp(Failure, TEXT("CONSTRUCTOR_EXCEPTION")) == 0)
		{
			return TEXT("FThrowingConversionConstructor SourceValue(9); // CONVERSION_CAUSE");
		}
		if (FCString::Strcmp(Failure, TEXT("ABI_MISMATCH")) == 0)
		{
			return TEXT("FConditionalConversionValue ConvertedValue = SourceValue; // CONVERSION_CAUSE");
		}
		return TEXT("bUseNumber ? 1 : FConditionalConversionValue(); // CONVERSION_CAUSE");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kFailures[] = {
			TEXT("IMPLICIT_NARROWING"), TEXT("NUMERIC_TO_ENUM"), TEXT("UNRELATED_REFERENCE"), TEXT("BAD_DOWNCAST"),
			TEXT("NULL_VALUE_TARGET"), TEXT("AMBIGUOUS_CONSTRUCTOR"), TEXT("AMBIGUOUS_OPERATOR"),
			TEXT("EXPLICIT_ONLY_IMPLICIT_USE"), TEXT("CONVERSION_EXCEPTION"), TEXT("CONSTRUCTOR_EXCEPTION"),
			TEXT("ABI_MISMATCH"), TEXT("CONDITIONAL_NO_COMMON_TYPE")};
		static const TCHAR* const kRecoveries[] = {TEXT("FRESH_MODULE"), TEXT("SAME_MODULE_OR_CONTEXT")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(24);
		for (int32 Index = 0; Index < 24; ++Index)
		{
			const TCHAR* const Failure = kFailures[Index / 2];
			const TCHAR* const Recovery = kRecoveries[Index % 2];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-CONV-FAILURE-%s-%s"), Failure, Recovery);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Kind = IndependentKind(Failure);
			Cell.Exception = IndependentException(Failure);
			Cell.Needle = IndependentNeedle(Failure);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EConvFailureKind;
		using AngelscriptTest::Generate::EConvFailureRecovery;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FConvFailureGenerator;
		using AngelscriptTest::Generate::FConvFailureParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FConvFailureGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CONV-FAILURE")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangConvFailureAmbiguousConstructorFreshModule")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ConvFailureGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ConvFailureGenerator dump path=%s cases=24 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(10, CaseCount));
		ASSERT_THAT(AreEqual(24, Cases.Num()));
		ASSERT_THAT(AreEqual(14, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangConvFailureAmbiguousConstructorFreshModule()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int EntryLangConvFailureImplicitNarrowingFreshModule()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/ConvFailureGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 RejectCount = 0;
		int32 FaultCount = 0;
		int32 NormalCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			if (Expected.Kind == ECellKind::Reject)
			{
				++RejectCount;
				ASSERT_THAT(AreEqual(FString(), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::CompileReject));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedException.IsEmpty()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				const FString RejectSource = Generator.BuildRejectSource(Expected.CaseId);
				ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.Needle)));
				ASSERT_THAT(IsTrue(RejectSource.Contains(TEXT("CONVERSION_CAUSE"))));
				if (Representative.Contains(Expected.EntryName))
				{
					FString GeneratedBody;
					ASSERT_THAT(IsTrue(ExtractFunction(RejectSource, Expected.EntryName, GeneratedBody)));
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
			else if (Expected.Kind == ECellKind::Fault)
			{
				++FaultCount;
				ASSERT_THAT(AreEqual(
					FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Exception, Cases[Index].ExpectedException));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
				if (Representative.Contains(Expected.EntryName))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
			else
			{
				++NormalCount;
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(1, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(1, Generator.GetExpected(Expected.CaseId)));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
				if (Representative.Contains(Expected.EntryName))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
		}
		ASSERT_THAT(AreEqual(14, RejectCount));
		ASSERT_THAT(AreEqual(6, FaultCount));
		ASSERT_THAT(AreEqual(4, NormalCount));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));

		FConvFailureParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildFailureSource(DefaultParams).IsEmpty()));
		FConvFailureParams NormalParams;
		NormalParams.Failure = EConvFailureKind::AmbiguousConstructor;
		ASSERT_THAT(IsTrue(Generator.BuildFailureSource(NormalParams).Contains(TEXT("int Entry()"))));
		NormalParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildFailureSource(NormalParams).Contains(TEXT("int ProbeEntry()"))));
		NormalParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildFailureSource(NormalParams).IsEmpty()));
		NormalParams.FunctionName.Reset();
		NormalParams.Failure = static_cast<EConvFailureKind>(12);
		ASSERT_THAT(IsTrue(Generator.BuildFailureSource(NormalParams).IsEmpty()));
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
		ASSERT_THAT(AreEqual(24, CountOccurrences(Dump, TEXT("// Case: LANG-CONV-FAILURE-"))));
		ASSERT_THAT(AreEqual(14, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/ConvFailureGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ConvFailureGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
