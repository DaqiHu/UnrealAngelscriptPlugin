#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestPropFailureGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	PropFailureGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString Needle;
		FString Exception;
		bool bReject = false;
		bool bNull = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-PROP-FAILURE-NULL_RECEIVER-DIRECT-FRESH_MODULE");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static bool IndependentReject(const TCHAR* Failure)
	{
		return FCString::Strcmp(Failure, TEXT("REMOVED_PROPERTY_DECORATOR")) == 0
			|| FCString::Strcmp(Failure, TEXT("REMOVED_VIRTUAL_PROPERTY")) == 0
			|| FCString::Strcmp(Failure, TEXT("MISSING_GETTER")) == 0
			|| FCString::Strcmp(Failure, TEXT("MISSING_SETTER")) == 0
			|| FCString::Strcmp(Failure, TEXT("REGISTERED_MISMATCHED_TYPES")) == 0
			|| FCString::Strcmp(Failure, TEXT("REGISTERED_DUPLICATE_GETTER")) == 0
			|| FCString::Strcmp(Failure, TEXT("REGISTERED_DUPLICATE_SETTER")) == 0
			|| FCString::Strcmp(Failure, TEXT("INACCESSIBLE_FIELD")) == 0
			|| FCString::Strcmp(Failure, TEXT("COMPOUND_VALUE_RECEIVER")) == 0;
	}

	static bool IndependentDivide(const TCHAR* Failure)
	{
		return FCString::Strcmp(Failure, TEXT("RECURSIVE_GETTER")) == 0
			|| FCString::Strcmp(Failure, TEXT("RECURSIVE_SETTER")) == 0
			|| FCString::Strcmp(Failure, TEXT("THROWING_GETTER")) == 0
			|| FCString::Strcmp(Failure, TEXT("THROWING_SETTER")) == 0;
	}

	static FString IndependentNeedle(const TCHAR* Failure, const TCHAR* Probe)
	{
		if (FCString::Strcmp(Failure, TEXT("REMOVED_PROPERTY_DECORATOR")) == 0)
		{
			return TEXT("int GetValue() property");
		}
		if (FCString::Strcmp(Failure, TEXT("REMOVED_VIRTUAL_PROPERTY")) == 0)
		{
			return TEXT("\t\tget");
		}
		if (FCString::Strcmp(Failure, TEXT("INACCESSIBLE_FIELD")) == 0)
		{
			return TEXT("private int Hidden = 7;");
		}
		if (FCString::Strcmp(Failure, TEXT("COMPOUND_VALUE_RECEIVER")) == 0)
		{
			return TEXT("Receiver.Access += 5;");
		}
		if (FCString::Strcmp(Failure, TEXT("NULL_RECEIVER")) == 0)
		{
			return TEXT("FPropertyFailure Receiver = nullptr;");
		}
		if (IndependentDivide(Failure))
		{
			return FCString::Strcmp(Probe, TEXT("ALTERNATE_PATH")) == 0
				? TEXT("return InvokePropertyFailure(Receiver);")
				: TEXT("return 1 / Zero;");
		}
		return TEXT("Receiver.Value");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kFailures[] = {
			TEXT("REMOVED_PROPERTY_DECORATOR"), TEXT("REMOVED_VIRTUAL_PROPERTY"), TEXT("MISSING_GETTER"),
			TEXT("MISSING_SETTER"), TEXT("REGISTERED_MISMATCHED_TYPES"), TEXT("REGISTERED_DUPLICATE_GETTER"),
			TEXT("REGISTERED_DUPLICATE_SETTER"), TEXT("RECURSIVE_GETTER"), TEXT("RECURSIVE_SETTER"),
			TEXT("THROWING_GETTER"), TEXT("THROWING_SETTER"), TEXT("NULL_RECEIVER"),
			TEXT("INACCESSIBLE_FIELD"), TEXT("COMPOUND_VALUE_RECEIVER")};
		static const TCHAR* const kProbes[] = {TEXT("DIRECT"), TEXT("ALTERNATE_PATH")};
		static const TCHAR* const kRecoveries[] = {TEXT("FRESH_MODULE"), TEXT("SAME_MODULE_OR_CONTEXT")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(56);
		for (int32 Index = 0; Index < 56; ++Index)
		{
			const TCHAR* const Failure = kFailures[Index / 4];
			const TCHAR* const Probe = kProbes[(Index / 2) % 2];
			const TCHAR* const Recovery = kRecoveries[Index % 2];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-PROP-FAILURE-%s-%s-%s"), Failure, Probe, Recovery);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Failure);
			Cell.bNull = FCString::Strcmp(Failure, TEXT("NULL_RECEIVER")) == 0;
			Cell.Needle = IndependentNeedle(Failure, Probe);
			Cell.Exception = Cell.bNull ? TEXT("Null pointer access") : TEXT("Divide by zero");
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EPropFailureKind;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FPropFailureGenerator;
		using AngelscriptTest::Generate::FPropFailureParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FPropFailureGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-PROP-FAILURE")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangPropFailureNullReceiverDirectFreshModule")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("PropFailureGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("PropFailureGenerator dump path=%s cases=56 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(20, CaseCount));
		ASSERT_THAT(AreEqual(56, Cases.Num()));
		ASSERT_THAT(AreEqual(36, RejectIds.Num()));

		int32 RejectCount = 0;
		int32 FaultCount = 0;
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
				ASSERT_THAT(IsTrue(RejectSource.Contains(TEXT("// Recovery:"))));
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
			}
			else
			{
				++FaultCount;
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(AreEqual(Expected.Exception, Cases[Index].ExpectedException));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
			}
		}
		ASSERT_THAT(AreEqual(36, RejectCount));
		ASSERT_THAT(AreEqual(20, FaultCount));

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangPropFailureNullReceiverDirectFreshModule()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int EntryLangPropFailureRemovedPropertyDecorator"))));

		FPropFailureParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildPropertyFailureSource(DefaultParams).IsEmpty()));
		FPropFailureParams ProbeParams;
		ProbeParams.Failure = EPropFailureKind::NullReceiver;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildPropertyFailureSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		FPropFailureParams EmptyName = ProbeParams;
		EmptyName.FunctionName.Reset();
		ASSERT_THAT(IsTrue(Generator.BuildPropertyFailureSource(EmptyName).Contains(TEXT("int Entry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/PropFailureGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			if (Expected.bReject)
			{
				continue;
			}
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
			if (!Expected.bNull)
			{
				ASSERT_THAT(IsTrue(Generator.BuildCaseSource(Expected.CaseId).Contains(TEXT("1 / Zero"))));
			}
			if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FPropFailureParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildPropertyFailureSource(InvalidName).IsEmpty()));
		FPropFailureParams InvalidEnum = ProbeParams;
		InvalidEnum.Failure = static_cast<EPropFailureKind>(14);
		ASSERT_THAT(IsTrue(Generator.BuildPropertyFailureSource(InvalidEnum).IsEmpty()));
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
		ASSERT_THAT(AreEqual(56, CountOccurrences(Dump, TEXT("// Case: LANG-PROP-FAILURE-"))));
		ASSERT_THAT(AreEqual(36, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/PropFailureGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("PropFailureGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
