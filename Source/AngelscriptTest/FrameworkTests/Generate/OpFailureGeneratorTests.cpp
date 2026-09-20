#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestOpFailureGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	OpFailureGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	enum class EKind : uint8
	{
		Normal,
		Reject,
		Fault,
	};

	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString Needle;
		FString Exception;
		int32 Expected = 0;
		EKind Kind = EKind::Fault;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-OP-FAILURE-INVALID_SHIFT_COUNT-DIAGNOSTIC_OR_EXCEPTION-FRESH_MODULE");
	static constexpr const TCHAR* ForeignCaseId =
		TEXT("LANG-OP-COMPARISON-REFERENCE-EQUAL-SAME_NON_NULL-LEFT_RIGHT");

	static EKind IndependentKind(const TCHAR* Family)
	{
		if (FCString::Strcmp(Family, TEXT("UNSUPPORTED_OPERAND")) == 0
			|| FCString::Strcmp(Family, TEXT("INVALID_LVALUE")) == 0
			|| FCString::Strcmp(Family, TEXT("CONST_MUTATION")) == 0
			|| FCString::Strcmp(Family, TEXT("MISSING_OVERLOAD")) == 0
			|| FCString::Strcmp(Family, TEXT("AMBIGUOUS_OVERLOAD")) == 0
			|| FCString::Strcmp(Family, TEXT("INVALID_SIGNATURE")) == 0
			|| FCString::Strcmp(Family, TEXT("DUPLICATE_OPERATOR")) == 0)
		{
			return EKind::Reject;
		}
		if (FCString::Strcmp(Family, TEXT("INVALID_SHIFT_COUNT")) == 0)
		{
			return EKind::Normal;
		}
		return EKind::Fault;
	}

	static FString IndependentException(const TCHAR* Family)
	{
		if (FCString::Strcmp(Family, TEXT("DIVIDE_ZERO")) == 0
			|| FCString::Strcmp(Family, TEXT("MODULO_ZERO")) == 0)
		{
			return TEXT("Divide by zero");
		}
		if (FCString::Strcmp(Family, TEXT("SIGNED_OVERFLOW")) == 0
			|| FCString::Strcmp(Family, TEXT("SIGNED_REMAINDER_OVERFLOW")) == 0)
		{
			return TEXT("Overflow in integer division");
		}
		if (FCString::Strcmp(Family, TEXT("POWER_OVERFLOW")) == 0)
		{
			return TEXT("Overflow in exponent operation");
		}
		if (FCString::Strcmp(Family, TEXT("NULL_RECEIVER")) == 0)
		{
			return TEXT("Null pointer access");
		}
		if (FCString::Strcmp(Family, TEXT("LEFT_OPERAND_EXCEPTION")) == 0)
		{
			return TEXT("Operator left operand exception");
		}
		if (FCString::Strcmp(Family, TEXT("RIGHT_OPERAND_EXCEPTION")) == 0)
		{
			return TEXT("Operator right operand exception");
		}
		if (FCString::Strcmp(Family, TEXT("ASSIGNMENT_EXCEPTION")) == 0)
		{
			return TEXT("Operator assignment exception");
		}
		return FString();
	}

	static FString IndependentNeedle(const TCHAR* Family)
	{
		if (FCString::Strcmp(Family, TEXT("UNSUPPORTED_OPERAND")) == 0)
		{
			return TEXT("return Left + Right; // OP_CAUSE");
		}
		if (FCString::Strcmp(Family, TEXT("DIVIDE_ZERO")) == 0)
		{
			return TEXT("Numerator / Divisor");
		}
		if (FCString::Strcmp(Family, TEXT("MODULO_ZERO")) == 0)
		{
			return TEXT("Dividend % Divisor");
		}
		if (FCString::Strcmp(Family, TEXT("INVALID_SHIFT_COUNT")) == 0)
		{
			return TEXT("Value << Count");
		}
		if (FCString::Strcmp(Family, TEXT("SIGNED_OVERFLOW")) == 0)
		{
			return TEXT("Numerator / Divisor");
		}
		if (FCString::Strcmp(Family, TEXT("SIGNED_REMAINDER_OVERFLOW")) == 0)
		{
			return TEXT("Dividend % Divisor");
		}
		if (FCString::Strcmp(Family, TEXT("POWER_OVERFLOW")) == 0)
		{
			return TEXT("Base ** Exponent");
		}
		if (FCString::Strcmp(Family, TEXT("INVALID_LVALUE")) == 0)
		{
			return TEXT("(Value + 1) = 3");
		}
		if (FCString::Strcmp(Family, TEXT("CONST_MUTATION")) == 0)
		{
			return TEXT("Value += 1; // OP_CAUSE");
		}
		if (FCString::Strcmp(Family, TEXT("MISSING_OVERLOAD")) == 0)
		{
			return TEXT("Left - Right");
		}
		if (FCString::Strcmp(Family, TEXT("AMBIGUOUS_OVERLOAD")) == 0)
		{
			return TEXT("ResolveAmbiguousOperator(null)");
		}
		if (FCString::Strcmp(Family, TEXT("INVALID_SIGNATURE")) == 0)
		{
			return TEXT("Left + Right");
		}
		if (FCString::Strcmp(Family, TEXT("DUPLICATE_OPERATOR")) == 0)
		{
			return TEXT("int opAdd(const FDuplicateOperator& Other) const // OP_CAUSE");
		}
		if (FCString::Strcmp(Family, TEXT("NULL_RECEIVER")) == 0)
		{
			return TEXT("FNativeCaseReference Receiver = nullptr;");
		}
		if (FCString::Strcmp(Family, TEXT("LEFT_OPERAND_EXCEPTION")) == 0)
		{
			return TEXT("ThrowOperatorStage(1)");
		}
		if (FCString::Strcmp(Family, TEXT("RIGHT_OPERAND_EXCEPTION")) == 0)
		{
			return TEXT("ThrowOperatorStage(2)");
		}
		return TEXT("ThrowOperatorStage(3); // ASSIGNMENT_CAUSE");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kFamilies[] = {
			TEXT("UNSUPPORTED_OPERAND"), TEXT("DIVIDE_ZERO"), TEXT("MODULO_ZERO"), TEXT("INVALID_SHIFT_COUNT"),
			TEXT("SIGNED_OVERFLOW"), TEXT("SIGNED_REMAINDER_OVERFLOW"), TEXT("POWER_OVERFLOW"), TEXT("INVALID_LVALUE"),
			TEXT("CONST_MUTATION"), TEXT("MISSING_OVERLOAD"), TEXT("AMBIGUOUS_OVERLOAD"), TEXT("INVALID_SIGNATURE"),
			TEXT("DUPLICATE_OPERATOR"), TEXT("NULL_RECEIVER"), TEXT("LEFT_OPERAND_EXCEPTION"),
			TEXT("RIGHT_OPERAND_EXCEPTION"), TEXT("ASSIGNMENT_EXCEPTION")};
		static const TCHAR* const kObservations[] = {
			TEXT("DIAGNOSTIC_OR_EXCEPTION"), TEXT("CLEANUP"), TEXT("RECOVERY_RESULT")};
		static const TCHAR* const kRecoveries[] = {TEXT("FRESH_MODULE"), TEXT("SAME_MODULE_OR_CONTEXT")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(102);
		for (int32 Index = 0; Index < 102; ++Index)
		{
			const TCHAR* const Family = kFamilies[Index / 6];
			const TCHAR* const Observation = kObservations[(Index / 2) % 3];
			const TCHAR* const Recovery = kRecoveries[Index % 2];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-OP-FAILURE-%s-%s-%s"), Family, Observation, Recovery);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Needle = IndependentNeedle(Family);
			Cell.Exception = IndependentException(Family);
			Cell.Kind = IndependentKind(Family);
			Cell.Expected = Cell.Kind == EKind::Normal ? static_cast<int32>(1u << 31) : 0;
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EOpFailureFamily;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FOpFailureGenerator;
		using AngelscriptTest::Generate::FOpFailureParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FOpFailureGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-OP-FAILURE")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangOpFailureInvalidShiftCountDiagnosticOrExceptionFreshModule")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("OpFailureGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("OpFailureGenerator dump path=%s cases=102 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(60, CaseCount));
		ASSERT_THAT(AreEqual(102, Cases.Num()));
		ASSERT_THAT(AreEqual(42, RejectIds.Num()));

		int32 RejectCount = 0;
		int32 FaultCount = 0;
		int32 NormalCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			if (Expected.Kind == EKind::Reject)
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
			else if (Expected.Kind == EKind::Fault)
			{
				++FaultCount;
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(AreEqual(Expected.Exception, Cases[Index].ExpectedException));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
			}
			else
			{
				++NormalCount;
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			}
		}
		ASSERT_THAT(AreEqual(42, RejectCount));
		ASSERT_THAT(AreEqual(54, FaultCount));
		ASSERT_THAT(AreEqual(6, NormalCount));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangOpFailureInvalidShiftCountDiagnosticOrExceptionFreshModule()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("Left + Right"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("struct FThrowingAssignment"))));

		FOpFailureParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildFailureSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FOpFailureParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildFailureSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/OpFailureGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			const FString CaseSource = Expected.Kind == EKind::Reject
				? Generator.BuildRejectSource(Expected.CaseId)
				: Generator.BuildCaseSource(Expected.CaseId);
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle) || CaseSource.Contains(Expected.Needle)));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FOpFailureParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildFailureSource(InvalidName).IsEmpty()));
		FOpFailureParams InvalidEnum = DefaultParams;
		InvalidEnum.Family = static_cast<EOpFailureFamily>(17);
		ASSERT_THAT(IsTrue(Generator.BuildFailureSource(InvalidEnum).IsEmpty()));
		FOpFailureParams RejectParams = DefaultParams;
		RejectParams.Family = EOpFailureFamily::UnsupportedOperand;
		ASSERT_THAT(IsTrue(Generator.BuildFailureSource(RejectParams).IsEmpty()));
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
		ASSERT_THAT(AreEqual(102, CountOccurrences(Dump, TEXT("// Case: LANG-OP-FAILURE-"))));
		ASSERT_THAT(AreEqual(42, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/OpFailureGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("OpFailureGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
