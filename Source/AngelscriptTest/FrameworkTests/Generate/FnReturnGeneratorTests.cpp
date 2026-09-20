#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestFnReturnGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	FnReturnGenerator,
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
		bool bFault = false;
		bool bReject = false;
		bool bHost = false;
		bool bLimited = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-FN-RETURN-DIRECT-VOID");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static int32 IndependentExpected(const TCHAR* Type)
	{
		if (FCString::Strcmp(Type, TEXT("BOOL")) == 0 || FCString::Strcmp(Type, TEXT("ENUM")) == 0)
		{
			return 1;
		}
		if (FCString::Strcmp(Type, TEXT("VOID")) == 0
			|| FCString::Strcmp(Type, TEXT("FLOAT32")) == 0
			|| FCString::Strcmp(Type, TEXT("FLOAT64")) == 0
			|| FCString::Strcmp(Type, TEXT("NULL_REFERENCE")) == 0)
		{
			return 0;
		}
		return 7;
	}

	static FString IndependentNeedle(const TCHAR* Path, const TCHAR* Type)
	{
		if (FCString::Strcmp(Path, TEXT("IF_ELSE")) == 0)
		{
			return TEXT("bool bTakeFirst = true;");
		}
		if (FCString::Strcmp(Path, TEXT("SWITCH")) == 0)
		{
			return TEXT("switch (Selector)");
		}
		if (FCString::Strcmp(Path, TEXT("EARLY")) == 0)
		{
			return TEXT("if (true)");
		}
		if (FCString::Strcmp(Path, TEXT("RECURSIVE_BASE")) == 0)
		{
			return TEXT("_Recursive(int Depth)");
		}
		if (FCString::Strcmp(Path, TEXT("EXCEPTION")) == 0)
		{
			return TEXT("int Crash = 1 / Zero;");
		}
		if (FCString::Strcmp(Path, TEXT("MISSING_INVALID")) == 0)
		{
			return TEXT("int Marker = 1;");
		}
		if (FCString::Strcmp(Path, TEXT("INCOMPATIBLE_INVALID")) == 0)
		{
			if (FCString::Strcmp(Type, TEXT("VOID")) == 0)
			{
				return TEXT("return 1;");
			}
			if (FCString::Strcmp(Type, TEXT("SCRIPT_VALUE")) == 0
				|| FCString::Strcmp(Type, TEXT("SCRIPT_REFERENCE")) == 0
				|| FCString::Strcmp(Type, TEXT("NULL_REFERENCE")) == 0)
			{
				return TEXT("return 7;");
			}
			return TEXT("return FScriptCaseValue(7);");
		}
		if (FCString::Strcmp(Type, TEXT("VOID")) == 0)
		{
			return TEXT("return;");
		}
		if (FCString::Strcmp(Type, TEXT("FLOAT32")) == 0)
		{
			return TEXT("return 7.25f;");
		}
		if (FCString::Strcmp(Type, TEXT("FLOAT64")) == 0)
		{
			return TEXT("return 7.5;");
		}
		if (FCString::Strcmp(Type, TEXT("INT8")) == 0)
		{
			return TEXT("return int8(7);");
		}
		if (FCString::Strcmp(Type, TEXT("INT16")) == 0)
		{
			return TEXT("return int16(7);");
		}
		if (FCString::Strcmp(Type, TEXT("INT64")) == 0)
		{
			return TEXT("return int64(7);");
		}
		if (FCString::Strcmp(Type, TEXT("UINT8")) == 0)
		{
			return TEXT("return uint8(7);");
		}
		if (FCString::Strcmp(Type, TEXT("UINT16")) == 0)
		{
			return TEXT("return uint16(7);");
		}
		if (FCString::Strcmp(Type, TEXT("UINT")) == 0)
		{
			return TEXT("return uint(7);");
		}
		if (FCString::Strcmp(Type, TEXT("UINT64")) == 0)
		{
			return TEXT("return uint64(7);");
		}
		if (FCString::Strcmp(Type, TEXT("BOOL")) == 0)
		{
			return TEXT("return true;");
		}
		if (FCString::Strcmp(Type, TEXT("ENUM")) == 0)
		{
			return TEXT("ENativeCaseEnum::One");
		}
		if (FCString::Strcmp(Type, TEXT("TYPEDEF")) == 0)
		{
			return TEXT("NativeCaseAlias(7)");
		}
		if (FCString::Strcmp(Type, TEXT("SCRIPT_VALUE")) == 0)
		{
			return TEXT("FScriptCaseValue(7)");
		}
		if (FCString::Strcmp(Type, TEXT("NULL_REFERENCE")) == 0)
		{
			return TEXT("return nullptr;");
		}
		if (FCString::Strcmp(Type, TEXT("SCRIPT_REFERENCE")) == 0)
		{
			return TEXT("FScriptCaseReference()");
		}
		return TEXT("return 7;");
	}

	static bool HasExtractableEntry(const FString& Source, const FString& EntryName)
	{
		const FString Header = FString::Printf(TEXT("int %s()"), *EntryName);
		return Source.StartsWith(Header) || Source.Contains(TEXT("\n") + Header);
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kPaths[] = {
			TEXT("DIRECT"), TEXT("IF_ELSE"), TEXT("SWITCH"), TEXT("EARLY"),
			TEXT("RECURSIVE_BASE"), TEXT("EXCEPTION"), TEXT("MISSING_INVALID"), TEXT("INCOMPATIBLE_INVALID")};
		static const TCHAR* const kTypes[] = {
			TEXT("VOID"), TEXT("INT8"), TEXT("INT16"), TEXT("INT"), TEXT("INT64"),
			TEXT("UINT8"), TEXT("UINT16"), TEXT("UINT"), TEXT("UINT64"),
			TEXT("FLOAT32"), TEXT("FLOAT64"), TEXT("BOOL"), TEXT("ENUM"),
			TEXT("TYPEDEF"), TEXT("SCRIPT_VALUE"), TEXT("SCRIPT_REFERENCE"), TEXT("NULL_REFERENCE")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(136);
		for (int32 Index = 0; Index < 136; ++Index)
		{
			const TCHAR* const Path = kPaths[Index / 17];
			const TCHAR* const Type = kTypes[Index % 17];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-FN-RETURN-%s-%s"), Path, Type);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = FCString::Strcmp(Path, TEXT("INCOMPATIBLE_INVALID")) == 0
				|| (FCString::Strcmp(Path, TEXT("MISSING_INVALID")) == 0 && FCString::Strcmp(Type, TEXT("VOID")) != 0);
			Cell.bFault = !Cell.bReject && FCString::Strcmp(Path, TEXT("EXCEPTION")) == 0;
			Cell.Expected = (Cell.bReject || Cell.bFault) ? 0 : IndependentExpected(Type);
			Cell.Needle = IndependentNeedle(Path, Type);
			Cell.bHost = FCString::Strcmp(Type, TEXT("TYPEDEF")) == 0;
			Cell.bLimited = FCString::Strcmp(Type, TEXT("FLOAT32")) == 0
				|| FCString::Strcmp(Type, TEXT("FLOAT64")) == 0;
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EFnReturnPath;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FFnReturnGenerator;
		using AngelscriptTest::Generate::FFnReturnParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FFnReturnGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-FN-RETURN")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangFnReturnDirectVoid")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("FnReturnGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("FnReturnGenerator dump path=%s cases=136 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(103, CaseCount));
		ASSERT_THAT(AreEqual(136, Cases.Num()));
		ASSERT_THAT(AreEqual(33, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("enum ENativeCaseEnum"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/FnReturnGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

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
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(RejectSource, Expected.EntryName, GeneratedBody)));
				if (HasExtractableEntry(Representative, Expected.EntryName))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				if (Expected.bFault)
				{
					++FaultCount;
					ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
					ASSERT_THAT(AreEqual(FString(TEXT("Divide by zero")), Cases[Index].ExpectedException));
					ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
					ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				}
				else
				{
					ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
					ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
					ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
					ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
					ASSERT_THAT(AreEqual(Expected.bLimited, Cases[Index].bLimitedObservation));
				}
				ASSERT_THAT(IsTrue(Cases[Index].Execution == (Expected.bHost
					? EGeneratedCaseExecution::RequiresHostSetup
					: EGeneratedCaseExecution::Standalone)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				const FString Isolated = Generator.BuildCaseSource(Expected.CaseId);
				ASSERT_THAT(IsTrue(Isolated.Contains(Expected.Needle)));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				if (HasExtractableEntry(Representative, Expected.EntryName))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
		}
		ASSERT_THAT(AreEqual(33, RejectCount));
		ASSERT_THAT(AreEqual(17, FaultCount));
		ASSERT_THAT(IsTrue(Cases[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(0, Cases[0].ExpectedReturn.Get(MIN_int32)));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangFnReturnDirectVoid()"))));

		FFnReturnParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildReturnSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FFnReturnParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildReturnSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(Generator.BuildReturnSource(ProbeParams).Contains(TEXT("int Entry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildReturnSource(ProbeParams).IsEmpty()));
		FFnReturnParams InvalidEnum = DefaultParams;
		InvalidEnum.Path = static_cast<EFnReturnPath>(8);
		ASSERT_THAT(IsTrue(Generator.BuildReturnSource(InvalidEnum).IsEmpty()));
		FFnReturnParams RejectParams = DefaultParams;
		RejectParams.Path = EFnReturnPath::IncompatibleInvalid;
		ASSERT_THAT(IsTrue(Generator.BuildReturnSource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildRejectSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(0, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(136, CountOccurrences(Dump, TEXT("// Case: LANG-FN-RETURN-"))));
		ASSERT_THAT(AreEqual(33, CountOccurrences(Dump, TEXT("// Reject module"))));
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
			TEXT("C:/no-such-root/FnReturnGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("FnReturnGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
