#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestCtorKindCallGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	CtorKindCallGenerator,
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

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-CTOR-KIND-CALL-LOCAL-IMPLICIT_DEFAULT-SCRIPT_VALUE");
	static constexpr const TCHAR* ZeroCaseId =
		TEXT("LANG-CTOR-KIND-CALL-LOCAL-IMPLICIT_DEFAULT-SCRIPT_VALUE");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-CONDITION-IF-BOOL_LITERAL-TRUE");

	static bool IndependentReject(const TCHAR* Call, const TCHAR* Kind, const TCHAR* Object)
	{
		if (FCString::Strcmp(Call, TEXT("BASE_CALL")) != 0)
		{
			return false;
		}
		return FCString::Strcmp(Object, TEXT("DERIVED")) != 0
			|| FCString::Strcmp(Kind, TEXT("IMPLICIT_DEFAULT")) == 0;
	}

	static bool IndependentReference(const TCHAR* Object)
	{
		return FCString::Strcmp(Object, TEXT("SCRIPT_REFERENCE")) == 0
			|| FCString::Strcmp(Object, TEXT("BASE")) == 0
			|| FCString::Strcmp(Object, TEXT("DERIVED")) == 0
			|| FCString::Strcmp(Object, TEXT("NATIVE_REFERENCE")) == 0;
	}

	static int32 IndependentExpected(const TCHAR* Call, const TCHAR* Kind, const TCHAR* Object)
	{
		const int32 BaseValue =
			FCString::Strcmp(Kind, TEXT("IMPLICIT_DEFAULT")) == 0
				|| (FCString::Strcmp(Kind, TEXT("DECLARED_DEFAULT")) == 0
					&& FCString::Strcmp(Object, TEXT("NATIVE_VALUE")) == 0)
			? 0
			: 7;
		const bool bHasTransfer = FCString::Strcmp(Kind, TEXT("COPY")) == 0
			|| FCString::Strcmp(Call, TEXT("COPY_DECLARATION")) == 0
			|| FCString::Strcmp(Call, TEXT("ASSIGNMENT")) == 0;
		const bool bAliases = IndependentReference(Object) && FCString::Strcmp(Call, TEXT("BASE_CALL")) != 0;
		return bAliases && bHasTransfer ? 9 : BaseValue;
	}

	static FString IndependentNeedle(const TCHAR* Call, const TCHAR* Kind, const TCHAR* Object)
	{
		if (FCString::Strcmp(Call, TEXT("BASE_CALL")) == 0)
		{
			return TEXT("super();");
		}
		if (FCString::Strcmp(Call, TEXT("COPY_DECLARATION")) == 0)
		{
			return TEXT("Copied = Selected;");
		}
		if (FCString::Strcmp(Call, TEXT("ASSIGNMENT")) == 0)
		{
			return TEXT("Assigned = Selected;");
		}
		if (FCString::Strcmp(Call, TEXT("TEMPORARY")) == 0 || FCString::Strcmp(Call, TEXT("RETURN")) == 0)
		{
			if (FCString::Strcmp(Object, TEXT("NATIVE_REFERENCE")) == 0)
			{
				return TEXT("MakeNativeReference");
			}
			if (FCString::Strcmp(Kind, TEXT("PARAMETERIZED")) == 0)
			{
				return TEXT("(7)");
			}
			if (FCString::Strcmp(Kind, TEXT("OVERLOADED")) == 0)
			{
				return TEXT("(3, 4)");
			}
			if (FCString::Strcmp(Kind, TEXT("CONVERSION")) == 0)
			{
				return TEXT("int64(int8(7))");
			}
			return TEXT("RecordConstructorCallRoute(");
		}
		if (FCString::Strcmp(Object, TEXT("NATIVE_VALUE")) == 0)
		{
			return TEXT("FNativeCaseValue");
		}
		if (FCString::Strcmp(Object, TEXT("NATIVE_REFERENCE")) == 0)
		{
			return TEXT("FNativeCaseReference");
		}
		return TEXT("FKindObject");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kCalls[] = {
			TEXT("LOCAL"), TEXT("TEMPORARY"), TEXT("FIELD"), TEXT("RETURN"),
			TEXT("ARGUMENT"), TEXT("BASE_CALL"), TEXT("COPY_DECLARATION"), TEXT("ASSIGNMENT")};
		static const TCHAR* const kKinds[] = {
			TEXT("IMPLICIT_DEFAULT"), TEXT("DECLARED_DEFAULT"), TEXT("PARAMETERIZED"),
			TEXT("OVERLOADED"), TEXT("COPY"), TEXT("CONVERSION")};
		static const TCHAR* const kObjects[] = {
			TEXT("SCRIPT_VALUE"), TEXT("SCRIPT_REFERENCE"), TEXT("BASE"),
			TEXT("DERIVED"), TEXT("NATIVE_VALUE"), TEXT("NATIVE_REFERENCE")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(288);
		for (int32 Index = 0; Index < 288; ++Index)
		{
			const TCHAR* const Call = kCalls[Index / 36];
			const TCHAR* const Kind = kKinds[(Index / 6) % 6];
			const TCHAR* const Object = kObjects[Index % 6];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-CTOR-KIND-CALL-%s-%s-%s"), Call, Kind, Object);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Call, Kind, Object);
			Cell.Expected = Cell.bReject ? 0 : IndependentExpected(Call, Kind, Object);
			Cell.Needle = IndependentNeedle(Call, Kind, Object);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::ECtorKindCallCall;
		using AngelscriptTest::Generate::ECtorKindCallKind;
		using AngelscriptTest::Generate::ECtorKindCallObject;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FCtorKindCallGenerator;
		using AngelscriptTest::Generate::FCtorKindCallParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FCtorKindCallGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CTOR-KIND-CALL")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangCtorKindCallLocalImplicitDefaultScriptValue")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("CtorKindCallGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("CtorKindCallGenerator dump path=%s cases=288 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(257, CaseCount));
		ASSERT_THAT(AreEqual(288, Cases.Num()));
		ASSERT_THAT(AreEqual(31, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangCtorKindCallLocalImplicitDefaultScriptValue()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int EntryLangCtorKindCallBaseCallImplicitDefaultScriptValue()"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("void RecordConstructorKindSelected(int Marker)"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/CtorKindCallGenerator/RepresentativeSources.as"));
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
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedException.IsEmpty()));
				ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("constructor-selection"))));
				ASSERT_THAT(IsFalse(Cases[Index].bLimitedObservation));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle) || Aggregate.Contains(Expected.Needle)));
				if (Expected.Expected == 0)
				{
					++ZeroCount;
				}
				if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
		}
		ASSERT_THAT(AreEqual(31, RejectCount));
		ASSERT_THAT(IsTrue(ZeroCount > 0));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(AreEqual(OwnedCase, Generator.BuildCaseSource(ProbeCaseId)));

		FCtorKindCallParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildConstructorSelectionSource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildConstructorSelectionSource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildConstructorSelectionSource(DefaultParams).IsEmpty()));
		DefaultParams.FunctionName.Reset();
		DefaultParams.Call = ECtorKindCallCall::BaseCall;
		ASSERT_THAT(IsTrue(Generator.BuildConstructorSelectionSource(DefaultParams).IsEmpty()));
		DefaultParams.Call = ECtorKindCallCall::Local;
		DefaultParams.Kind = static_cast<ECtorKindCallKind>(6);
		ASSERT_THAT(IsTrue(Generator.BuildConstructorSelectionSource(DefaultParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(ZeroCaseId)));
		bool bFoundZeroCase = false;
		for (const FGeneratedCaseInfo& Info : Cases)
		{
			if (Info.CaseId == ZeroCaseId)
			{
				bFoundZeroCase = true;
				ASSERT_THAT(IsTrue(Info.ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Info.ExpectedReturn.Get(MIN_int32)));
			}
		}
		ASSERT_THAT(IsTrue(bFoundZeroCase));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(288, CountOccurrences(Dump, TEXT("// Case: LANG-CTOR-KIND-CALL-"))));
		ASSERT_THAT(AreEqual(31, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/CtorKindCallGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("CtorKindCallGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
