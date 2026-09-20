#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestPropValueOpGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	PropValueOpGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString Needle;
		FString TypeNeedle;
		int32 Expected = 0;
		bool bReject = false;
		bool bHost = false;
		bool bLimited = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-PROP-VALUE-OP-DEFAULT_READ-MUTABLE-INT8");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static bool IndependentBoolOrEnum(const TCHAR* Type)
	{
		return FCString::Strcmp(Type, TEXT("BOOL")) == 0 || FCString::Strcmp(Type, TEXT("ENUM")) == 0;
	}

	static bool IndependentSupportsCompound(const TCHAR* Type)
	{
		return !IndependentBoolOrEnum(Type)
			&& FCString::Strcmp(Type, TEXT("SCRIPT_VALUE")) != 0
			&& FCString::Strcmp(Type, TEXT("NATIVE_VALUE")) != 0;
	}

	static bool IndependentReject(const TCHAR* Operation, const TCHAR* Receiver, const TCHAR* Type)
	{
		if (FCString::Strcmp(Receiver, TEXT("CONST")) == 0
			&& FCString::Strcmp(Operation, TEXT("DEFAULT_READ")) != 0
			&& FCString::Strcmp(Operation, TEXT("COPY")) != 0)
		{
			return true;
		}
		return FCString::Strcmp(Operation, TEXT("COMPOUND_WRITE")) == 0 && !IndependentSupportsCompound(Type);
	}

	static int32 IndependentCore(const TCHAR* Type, const TCHAR* Operation)
	{
		if (FCString::Strcmp(Operation, TEXT("DEFAULT_READ")) == 0)
		{
			return 0;
		}
		if (FCString::Strcmp(Operation, TEXT("COPY")) == 0)
		{
			return IndependentBoolOrEnum(Type) ? 1 : 37;
		}
		return IndependentBoolOrEnum(Type) ? 1 : 29;
	}

	static int32 IndependentMarker(const TCHAR* Receiver)
	{
		return (FCString::Strcmp(Receiver, TEXT("BASE_VIEW")) == 0
			|| FCString::Strcmp(Receiver, TEXT("DERIVED_VIEW")) == 0)
			? 22
			: 11;
	}

	static FString IndependentNeedle(const TCHAR* Operation)
	{
		if (FCString::Strcmp(Operation, TEXT("WRITE")) == 0)
		{
			return TEXT("Receiver.Stored = ");
		}
		if (FCString::Strcmp(Operation, TEXT("COMPOUND_WRITE")) == 0)
		{
			return TEXT("Receiver.Stored += ");
		}
		if (FCString::Strcmp(Operation, TEXT("COPY")) == 0)
		{
			return TEXT("CopyValue = Receiver.Stored;");
		}
		if (FCString::Strcmp(Operation, TEXT("REFERENCE_MUTATION")) == 0)
		{
			return TEXT("MutateStoredProperty_");
		}
		return TEXT("int CoreResult = ObserveStoredProperty_");
	}

	static FString IndependentScriptType(const TCHAR* Type)
	{
		if (FCString::Strcmp(Type, TEXT("INT8")) == 0)
		{
			return TEXT("int8");
		}
		if (FCString::Strcmp(Type, TEXT("INT16")) == 0)
		{
			return TEXT("int16");
		}
		if (FCString::Strcmp(Type, TEXT("INT64")) == 0)
		{
			return TEXT("int64");
		}
		if (FCString::Strcmp(Type, TEXT("UINT8")) == 0)
		{
			return TEXT("uint8");
		}
		if (FCString::Strcmp(Type, TEXT("UINT16")) == 0)
		{
			return TEXT("uint16");
		}
		if (FCString::Strcmp(Type, TEXT("UINT")) == 0)
		{
			return TEXT("uint");
		}
		if (FCString::Strcmp(Type, TEXT("UINT64")) == 0)
		{
			return TEXT("uint64");
		}
		if (FCString::Strcmp(Type, TEXT("FLOAT32")) == 0)
		{
			return TEXT("float");
		}
		if (FCString::Strcmp(Type, TEXT("FLOAT64")) == 0)
		{
			return TEXT("double");
		}
		if (FCString::Strcmp(Type, TEXT("BOOL")) == 0)
		{
			return TEXT("bool");
		}
		if (FCString::Strcmp(Type, TEXT("ENUM")) == 0)
		{
			return TEXT("ENativeCaseEnum");
		}
		if (FCString::Strcmp(Type, TEXT("TYPEDEF")) == 0)
		{
			return TEXT("NativeCaseAlias");
		}
		if (FCString::Strcmp(Type, TEXT("SCRIPT_VALUE")) == 0)
		{
			return TEXT("FScriptCaseValue");
		}
		if (FCString::Strcmp(Type, TEXT("NATIVE_VALUE")) == 0)
		{
			return TEXT("FNativeCaseValue");
		}
		return TEXT("int");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kOps[] = {
			TEXT("DEFAULT_READ"), TEXT("WRITE"), TEXT("COMPOUND_WRITE"), TEXT("COPY"), TEXT("REFERENCE_MUTATION")};
		static const TCHAR* const kReceivers[] = {
			TEXT("MUTABLE"), TEXT("CONST"), TEXT("BASE_VIEW"), TEXT("DERIVED_VIEW")};
		static const TCHAR* const kTypes[] = {
			TEXT("INT8"), TEXT("INT16"), TEXT("INT"), TEXT("INT64"), TEXT("UINT8"), TEXT("UINT16"),
			TEXT("UINT"), TEXT("UINT64"), TEXT("FLOAT32"), TEXT("FLOAT64"), TEXT("BOOL"), TEXT("ENUM"),
			TEXT("TYPEDEF"), TEXT("SCRIPT_VALUE"), TEXT("NATIVE_VALUE")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(300);
		for (int32 Index = 0; Index < 300; ++Index)
		{
			const TCHAR* const Operation = kOps[Index / 60];
			const TCHAR* const Receiver = kReceivers[(Index / 15) % 4];
			const TCHAR* const Type = kTypes[Index % 15];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-PROP-VALUE-OP-%s-%s-%s"), Operation, Receiver, Type);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Operation, Receiver, Type);
			Cell.Expected = Cell.bReject
				? 0
				: IndependentCore(Type, Operation) * 100 + IndependentMarker(Receiver);
			Cell.Needle = IndependentNeedle(Operation);
			Cell.TypeNeedle = FString::Printf(
				TEXT("ObserveStoredProperty_%s(%s Value)"),
				*Cell.EntryName,
				*IndependentScriptType(Type));
			Cell.bHost = FCString::Strcmp(Type, TEXT("TYPEDEF")) == 0
				|| FCString::Strcmp(Type, TEXT("NATIVE_VALUE")) == 0;
			Cell.bLimited = Cell.bHost
				|| FCString::Strcmp(Type, TEXT("FLOAT32")) == 0
				|| FCString::Strcmp(Type, TEXT("FLOAT64")) == 0
				|| FCString::Strcmp(Type, TEXT("INT64")) == 0
				|| FCString::Strcmp(Type, TEXT("UINT64")) == 0;
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EPropValueOpOperation;
		using AngelscriptTest::Generate::EPropValueOpReceiver;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FPropValueOpGenerator;
		using AngelscriptTest::Generate::FPropValueOpParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FPropValueOpGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-PROP-VALUE-OP")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangPropValueOpDefaultReadMutableInt8")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("PropValueOpGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("PropValueOpGenerator dump path=%s cases=300 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(243, CaseCount));
		ASSERT_THAT(AreEqual(300, Cases.Num()));
		ASSERT_THAT(AreEqual(57, RejectIds.Num()));

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
				ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.TypeNeedle)));
				FString RejectBody;
				ASSERT_THAT(IsTrue(ExtractFunction(RejectSource, Expected.EntryName, RejectBody)));
				ASSERT_THAT(IsTrue(RejectBody.Contains(Expected.Needle)));
			}
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == (Expected.bHost
					? EGeneratedCaseExecution::RequiresHostSetup
					: EGeneratedCaseExecution::Standalone)));
				ASSERT_THAT(AreEqual(Expected.bLimited, Cases[Index].bLimitedObservation));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
			}
		}
		ASSERT_THAT(AreEqual(57, RejectCount));

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangPropValueOpDefaultReadMutableInt8()"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("enum ENativeCaseEnum\n"))));

		FPropValueOpParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildStoredPropertySource(DefaultParams).Contains(TEXT("int Entry()"))));
		FPropValueOpParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildStoredPropertySource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/PropValueOpGenerator/RepresentativeSources.as"));
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
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("return CoreResult * 100 + Receiver.ReceiverMarker;"))));
			ASSERT_THAT(IsTrue(Aggregate.Contains(Expected.TypeNeedle)));
			if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FPropValueOpParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildStoredPropertySource(InvalidName).IsEmpty()));
		FPropValueOpParams InvalidEnum = DefaultParams;
		InvalidEnum.Operation = static_cast<EPropValueOpOperation>(5);
		ASSERT_THAT(IsTrue(Generator.BuildStoredPropertySource(InvalidEnum).IsEmpty()));
		FPropValueOpParams RejectParams = DefaultParams;
		RejectParams.Operation = EPropValueOpOperation::Write;
		RejectParams.Receiver = EPropValueOpReceiver::Const;
		ASSERT_THAT(IsTrue(Generator.BuildStoredPropertySource(RejectParams).IsEmpty()));
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
		ASSERT_THAT(AreEqual(300, CountOccurrences(Dump, TEXT("// Case: LANG-PROP-VALUE-OP-"))));
		ASSERT_THAT(AreEqual(57, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/PropValueOpGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("PropValueOpGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
