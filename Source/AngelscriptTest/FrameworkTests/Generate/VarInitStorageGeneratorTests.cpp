#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestVarInitStorageGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	VarInitStorageGenerator,
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

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-VAR-INIT-STORAGE-DEFAULT-LOCAL-INT8");
	static constexpr const TCHAR* ZeroCaseId = TEXT("LANG-VAR-INIT-STORAGE-DEFAULT-LOCAL-INT8");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static bool IndependentReject(const TCHAR* Initializer, const TCHAR* Storage, const TCHAR* Type)
	{
		if (FCString::Strcmp(Initializer, TEXT("DEFAULT")) == 0 && FCString::Strcmp(Storage, TEXT("AUTO")) == 0)
		{
			return true;
		}
		return FCString::Strcmp(Storage, TEXT("CONST_GLOBAL")) == 0
			&& FCString::Strcmp(Type, TEXT("NATIVE_VALUE")) == 0;
	}

	static int32 IndependentExpected(const TCHAR* Initializer, const TCHAR* Type)
	{
		if (FCString::Strcmp(Initializer, TEXT("DEFAULT")) == 0)
		{
			return 0;
		}
		const bool bObject = FCString::Strcmp(Type, TEXT("SCRIPT_VALUE")) == 0
			|| FCString::Strcmp(Type, TEXT("NATIVE_VALUE")) == 0;
		if (FCString::Strcmp(Initializer, TEXT("EXPRESSION")) == 0
			&& FCString::Strcmp(Type, TEXT("BOOL")) != 0
			&& FCString::Strcmp(Type, TEXT("ENUM")) != 0
			&& !bObject)
		{
			return 2;
		}
		return 1;
	}

	static FString IndependentNeedle(const TCHAR* Initializer, const TCHAR* Storage, const TCHAR* Type)
	{
		if (IndependentReject(Initializer, Storage, Type)
			&& FCString::Strcmp(Storage, TEXT("AUTO")) == 0)
		{
			return TEXT("auto VariableValue;");
		}
		if (IndependentReject(Initializer, Storage, Type))
		{
			return TEXT("NativeValue.Value");
		}
		if (FCString::Strcmp(Storage, TEXT("CONST_GLOBAL")) == 0)
		{
			return TEXT("VariableValue");
		}
		if (FCString::Strcmp(Storage, TEXT("FIELD_LINKAGE")) == 0)
		{
			return TEXT("FVariableHolder");
		}
		if (FCString::Strcmp(Storage, TEXT("LOOP_INITIALIZER")) == 0)
		{
			return TEXT("for (");
		}
		if (FCString::Strcmp(Storage, TEXT("BRANCH_LOCAL")) == 0)
		{
			return TEXT("if (true)");
		}
		if (FCString::Strcmp(Storage, TEXT("AUTO")) == 0)
		{
			return TEXT("auto VariableValue =");
		}
		if (FCString::Strcmp(Initializer, TEXT("DEFAULT")) == 0)
		{
			return TEXT("VariableValue;");
		}
		if (FCString::Strcmp(Initializer, TEXT("EXPRESSION")) == 0
			&& FCString::Strcmp(Type, TEXT("INT")) == 0)
		{
			return TEXT("1 + 1");
		}
		if (FCString::Strcmp(Initializer, TEXT("COPY")) == 0)
		{
			return TEXT("VariableSource");
		}
		if (FCString::Strcmp(Initializer, TEXT("FUNCTION_RETURN")) == 0)
		{
			return TEXT("MakeVariableValue");
		}
		if (FCString::Strcmp(Initializer, TEXT("CONDITIONAL")) == 0)
		{
			return TEXT("true ?");
		}
		if (FCString::Strcmp(Initializer, TEXT("LITERAL")) == 0 && FCString::Strcmp(Type, TEXT("INT8")) == 0)
		{
			return TEXT("int8 VariableValue = 1;");
		}
		return TEXT("VariableValue");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kInitializers[] = {
			TEXT("DEFAULT"), TEXT("LITERAL"), TEXT("EXPRESSION"), TEXT("COPY"),
			TEXT("CONSTRUCTOR"), TEXT("FUNCTION_RETURN"), TEXT("CONDITIONAL")};
		static const TCHAR* const kStorages[] = {
			TEXT("LOCAL"), TEXT("CONST_LOCAL"), TEXT("AUTO"), TEXT("LOOP_INITIALIZER"),
			TEXT("BRANCH_LOCAL"), TEXT("CONST_GLOBAL"), TEXT("FIELD_LINKAGE")};
		static const TCHAR* const kTypes[] = {
			TEXT("INT8"), TEXT("INT16"), TEXT("INT"), TEXT("INT64"), TEXT("UINT8"), TEXT("UINT16"),
			TEXT("UINT"), TEXT("UINT64"), TEXT("FLOAT32"), TEXT("FLOAT64"), TEXT("BOOL"), TEXT("ENUM"),
			TEXT("TYPEDEF"), TEXT("SCRIPT_VALUE"), TEXT("NATIVE_VALUE")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(735);
		for (int32 Index = 0; Index < 735; ++Index)
		{
			const TCHAR* const Initializer = kInitializers[Index / 105];
			const TCHAR* const Storage = kStorages[(Index / 15) % 7];
			const TCHAR* const Type = kTypes[Index % 15];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-VAR-INIT-STORAGE-%s-%s-%s"), Initializer, Storage, Type);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Initializer, Storage, Type);
			Cell.Expected = Cell.bReject ? 0 : IndependentExpected(Initializer, Type);
			Cell.Needle = IndependentNeedle(Initializer, Storage, Type);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EVarInitKind;
		using AngelscriptTest::Generate::EVarInitStorage;
		using AngelscriptTest::Generate::EVarInitType;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FVarInitStorageGenerator;
		using AngelscriptTest::Generate::FVarInitStorageParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FVarInitStorageGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-VAR-INIT-STORAGE")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangVarInitStorageDefaultLocalInt8")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("VarInitStorageGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(
			LogTemp,
			Display,
			TEXT("VarInitStorageGenerator dump path=%s cases=735 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(713, CaseCount));
		ASSERT_THAT(AreEqual(735, Cases.Num()));
		ASSERT_THAT(AreEqual(22, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/VarInitStorageGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 RejectCount = 0;
		int32 ZeroCount = 0;
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
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(RejectSource, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
				if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
			else
			{
				ASSERT_THAT(AreEqual(
					FString::Printf(TEXT("int %s()"), *Expected.EntryName),
					Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				if (Expected.Expected == 0)
				{
					++ZeroCount;
				}
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
				if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
		}
		ASSERT_THAT(AreEqual(22, RejectCount));
		ASSERT_THAT(IsTrue(ZeroCount > 0));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("enum ENativeCaseEnum\n"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangVarInitStorageDefaultLocalInt8()"))));

		FVarInitStorageParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildVariableSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FVarInitStorageParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildVariableSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildVariableSource(ProbeParams).IsEmpty()));
		FVarInitStorageParams InvalidEnum = DefaultParams;
		InvalidEnum.Initializer = static_cast<EVarInitKind>(7);
		ASSERT_THAT(IsTrue(Generator.BuildVariableSource(InvalidEnum).IsEmpty()));
		FVarInitStorageParams RejectParams = DefaultParams;
		RejectParams.Storage = EVarInitStorage::Auto;
		ASSERT_THAT(IsTrue(Generator.BuildVariableSource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(ZeroCaseId)));
		bool bFoundZeroCase = false;
		for (int32 Index = 0; Index < Cases.Num(); ++Index)
		{
			if (Cases[Index].CaseId == ZeroCaseId)
			{
				bFoundZeroCase = true;
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			}
		}
		ASSERT_THAT(IsTrue(bFoundZeroCase));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(0, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(AreEqual(735, CountOccurrences(Dump, TEXT("// Case: LANG-VAR-INIT-STORAGE-"))));
		ASSERT_THAT(AreEqual(22, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(IsTrue(
			OutputFile.EndsWith(TEXT("GeneratedCases/VarInitStorageGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\VarInitStorageGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/VarInitStorageGenerator.as"),
			Dump,
			ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("VarInitStorageGenerator.as")),
			Dump,
			FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
