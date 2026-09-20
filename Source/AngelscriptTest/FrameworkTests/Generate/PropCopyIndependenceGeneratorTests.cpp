#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestPropCopyIndependenceGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	PropCopyIndependenceGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString TransferNeedle;
		FString MutationNeedle;
		FString TypeNeedle;
		int32 Expected = 0;
		bool bReference = false;
		bool bLimited = false;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-PROP-COPY-INDEPENDENCE-SOURCE_AFTER_TRANSFER-COPY_CONSTRUCT-INT8-EXACT");
	static constexpr const TCHAR* ZeroCaseId =
		TEXT("LANG-PROP-COPY-INDEPENDENCE-SOURCE_AFTER_TRANSFER-SELF_ASSIGN-BOOL-EXACT");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static bool IndependentBoolOrEnum(const TCHAR* Type)
	{
		return FCString::Strcmp(Type, TEXT("BOOL")) == 0 || FCString::Strcmp(Type, TEXT("ENUM")) == 0;
	}

	static bool IndependentReference(const TCHAR* Type)
	{
		return FCString::Strcmp(Type, TEXT("SCRIPT_REFERENCE")) == 0
			|| FCString::Strcmp(Type, TEXT("NATIVE_REFERENCE")) == 0;
	}

	static bool IndependentLimited(const TCHAR* Type)
	{
		return IndependentReference(Type)
			|| FCString::Strcmp(Type, TEXT("FLOAT32")) == 0
			|| FCString::Strcmp(Type, TEXT("FLOAT64")) == 0
			|| FCString::Strcmp(Type, TEXT("INT64")) == 0
			|| FCString::Strcmp(Type, TEXT("UINT64")) == 0
			|| FCString::Strcmp(Type, TEXT("NATIVE_VALUE")) == 0;
	}

	static int32 IndependentInitialSource(const TCHAR* Type)
	{
		return IndependentBoolOrEnum(Type) ? 1 : 11;
	}

	static int32 IndependentInitialTarget(const TCHAR* Type)
	{
		return IndependentBoolOrEnum(Type) ? 0 : 22;
	}

	static int32 IndependentMutated(const TCHAR* Type, const int32 Before)
	{
		return IndependentBoolOrEnum(Type) ? 1 - Before : 37;
	}

	static int32 IndependentExpected(const TCHAR* Mutation, const TCHAR* Transfer, const TCHAR* Type)
	{
		const int32 SourceInitial = IndependentInitialSource(Type);
		const int32 TargetInitial = IndependentInitialTarget(Type);
		const bool bSelf = FCString::Strcmp(Transfer, TEXT("SELF_ASSIGN")) == 0;
		const bool bSourceAfter = FCString::Strcmp(Mutation, TEXT("SOURCE_AFTER_TRANSFER")) == 0;
		int32 ExpectedSource = SourceInitial;
		int32 ExpectedTarget = TargetInitial;
		if (bSelf)
		{
			ExpectedSource = bSourceAfter ? IndependentMutated(Type, SourceInitial) : SourceInitial;
			ExpectedTarget = bSourceAfter ? TargetInitial : IndependentMutated(Type, TargetInitial);
		}
		else if (IndependentReference(Type))
		{
			ExpectedSource = IndependentMutated(Type, SourceInitial);
			ExpectedTarget = IndependentMutated(Type, SourceInitial);
		}
		else if (bSourceAfter)
		{
			ExpectedSource = IndependentMutated(Type, SourceInitial);
			ExpectedTarget = SourceInitial;
		}
		else
		{
			ExpectedSource = SourceInitial;
			ExpectedTarget = IndependentMutated(Type, SourceInitial);
		}
		return ExpectedSource * 1000 + ExpectedTarget;
	}

	static FString IndependentTransferNeedle(const TCHAR* Mutation, const TCHAR* Transfer)
	{
		const bool bNested = FCString::Strcmp(Mutation, TEXT("NESTED_MEMBER")) == 0;
		if (FCString::Strcmp(Transfer, TEXT("COPY_CONSTRUCT")) == 0)
		{
			return bNested
				? TEXT("TransferTarget = Receiver.NestedSource;")
				: TEXT("TransferTarget = Receiver.Source;");
		}
		if (FCString::Strcmp(Transfer, TEXT("ASSIGN")) == 0)
		{
			return bNested
				? TEXT("Receiver.NestedTarget = Receiver.NestedSource;")
				: TEXT("Receiver.Target = Receiver.Source;");
		}
		if (bNested)
		{
			return TEXT("Receiver.NestedTarget = Receiver.NestedTarget;");
		}
		return FCString::Strcmp(Mutation, TEXT("SOURCE_AFTER_TRANSFER")) == 0
			? TEXT("Receiver.Source = Receiver.Source;")
			: TEXT("Receiver.Target = Receiver.Target;");
	}

	static FString IndependentMutationNeedle(const TCHAR* Mutation)
	{
		if (FCString::Strcmp(Mutation, TEXT("SOURCE_AFTER_TRANSFER")) == 0)
		{
			return TEXT("Receiver.Source = ");
		}
		if (FCString::Strcmp(Mutation, TEXT("TARGET_AFTER_TRANSFER")) == 0)
		{
			return TEXT("Receiver.Target = ");
		}
		return TEXT("Receiver.NestedSource.Stored = ");
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
		if (FCString::Strcmp(Type, TEXT("SCRIPT_REFERENCE")) == 0)
		{
			return TEXT("FScriptCaseReference");
		}
		if (FCString::Strcmp(Type, TEXT("NATIVE_REFERENCE")) == 0)
		{
			return TEXT("FNativeCaseReference");
		}
		return TEXT("int");
	}

	static FString IndependentTypeNeedle(const TCHAR* Mutation, const TCHAR* Type)
	{
		const int32 Before = FCString::Strcmp(Mutation, TEXT("TARGET_AFTER_TRANSFER")) == 0
			? IndependentInitialTarget(Type)
			: IndependentInitialSource(Type);
		const int32 After = IndependentMutated(Type, Before);
		if (FCString::Strcmp(Type, TEXT("BOOL")) == 0)
		{
			return After == 0 ? TEXT("= false;") : TEXT("= true;");
		}
		if (FCString::Strcmp(Type, TEXT("ENUM")) == 0)
		{
			return After == 0 ? TEXT("= ENativeCaseEnum::Zero;") : TEXT("= ENativeCaseEnum::One;");
		}
		if (FCString::Strcmp(Type, TEXT("NATIVE_REFERENCE")) == 0)
		{
			return FString::Printf(TEXT("= CreateNativeCaseReference(%d);"), After);
		}
		return FString::Printf(TEXT("= %s(%d);"), *IndependentScriptType(Type), After);
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kMutations[] = {
			TEXT("SOURCE_AFTER_TRANSFER"), TEXT("TARGET_AFTER_TRANSFER"), TEXT("NESTED_MEMBER")};
		static const TCHAR* const kTransfers[] = {
			TEXT("COPY_CONSTRUCT"), TEXT("ASSIGN"), TEXT("SELF_ASSIGN")};
		static const TCHAR* const kTypes[] = {
			TEXT("INT8"), TEXT("INT16"), TEXT("INT"), TEXT("INT64"), TEXT("UINT8"), TEXT("UINT16"),
			TEXT("UINT"), TEXT("UINT64"), TEXT("FLOAT32"), TEXT("FLOAT64"), TEXT("BOOL"), TEXT("ENUM"),
			TEXT("TYPEDEF"), TEXT("SCRIPT_VALUE"), TEXT("NATIVE_VALUE"), TEXT("SCRIPT_REFERENCE"),
			TEXT("NATIVE_REFERENCE")};
		static const TCHAR* const kViews[] = {TEXT("EXACT"), TEXT("BASE"), TEXT("DERIVED")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(459);
		for (int32 Index = 0; Index < 459; ++Index)
		{
			const TCHAR* const Mutation = kMutations[Index / 153];
			const TCHAR* const Transfer = kTransfers[(Index / 51) % 3];
			const TCHAR* const Type = kTypes[(Index / 3) % 17];
			const TCHAR* const View = kViews[Index % 3];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-PROP-COPY-INDEPENDENCE-%s-%s-%s-%s"), Mutation, Transfer, Type, View);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Expected = IndependentExpected(Mutation, Transfer, Type);
			Cell.TransferNeedle = IndependentTransferNeedle(Mutation, Transfer);
			Cell.MutationNeedle = IndependentMutationNeedle(Mutation);
			Cell.TypeNeedle = IndependentTypeNeedle(Mutation, Type);
			Cell.bReference = IndependentReference(Type);
			Cell.bLimited = IndependentLimited(Type);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EPropCopyIndependenceMutation;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FPropCopyIndependenceGenerator;
		using AngelscriptTest::Generate::FPropCopyIndependenceParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FPropCopyIndependenceGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-PROP-COPY-INDEPENDENCE")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangPropCopyIndependenceSourceAfterTransferCopyConstructInt8Exact")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("PropCopyIndependenceGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("PropCopyIndependenceGenerator dump path=%s cases=459 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(459, CaseCount));
		ASSERT_THAT(AreEqual(459, Cases.Num()));

		bool bFoundZero = false;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			ASSERT_THAT(IsTrue(Cases[Index].Execution == (Expected.bReference
				? EGeneratedCaseExecution::SourceOnly
				: EGeneratedCaseExecution::RequiresHostSetup)));
			ASSERT_THAT(AreEqual(Expected.bLimited, Cases[Index].bLimitedObservation));
			if (Expected.CaseId == ZeroCaseId)
			{
				bFoundZero = true;
				ASSERT_THAT(AreEqual(0, Expected.Expected));
			}
		}
		ASSERT_THAT(IsTrue(bFoundZero));

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangPropCopyIndependenceSourceAfterTransferCopyConstructInt8Exact()"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("enum ENativeCaseEnum\n"))));

		FPropCopyIndependenceParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildPropertyCopySource(DefaultParams).Contains(TEXT("int Entry()"))));
		FPropCopyIndependenceParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildPropertyCopySource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/PropCopyIndependenceGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.TransferNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.MutationNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.TypeNeedle)));
			if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FPropCopyIndependenceParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildPropertyCopySource(InvalidName).IsEmpty()));
		FPropCopyIndependenceParams InvalidEnum = DefaultParams;
		InvalidEnum.Mutation = static_cast<EPropCopyIndependenceMutation>(3);
		ASSERT_THAT(IsTrue(Generator.BuildPropertyCopySource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(ZeroCaseId)));

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
		ASSERT_THAT(AreEqual(459, CountOccurrences(Dump, TEXT("// Case: LANG-PROP-COPY-INDEPENDENCE-"))));
		ASSERT_THAT(AreEqual(0, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/PropCopyIndependenceGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("PropCopyIndependenceGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
