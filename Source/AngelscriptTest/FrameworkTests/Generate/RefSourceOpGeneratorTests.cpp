#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestRefSourceOpGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	RefSourceOpGenerator,
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
		bool bFault = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-REF-SOURCE-OP-INITIALIZE-MUTABLE-NEW_LOCAL");

	static bool IndependentReject(const TCHAR* Operation, const TCHAR* Qualifier)
	{
		if (FCString::Strcmp(Qualifier, TEXT("CONST_REMOVAL_INVALID")) == 0)
		{
			return true;
		}
		return FCString::Strcmp(Operation, TEXT("ALIAS_MUTATION")) == 0
			&& FCString::Strcmp(Qualifier, TEXT("MUTABLE")) != 0;
	}

	static bool IndependentFault(const TCHAR* Operation, const TCHAR* Qualifier, const TCHAR* Source)
	{
		if (IndependentReject(Operation, Qualifier))
		{
			return false;
		}
		if (FCString::Strcmp(Source, TEXT("NULL")) != 0)
		{
			return false;
		}
		return FCString::Strcmp(Operation, TEXT("MEMBER_ACCESS")) == 0
			|| FCString::Strcmp(Operation, TEXT("CAST")) == 0
			|| FCString::Strcmp(Operation, TEXT("ALIAS_MUTATION")) == 0;
	}

	static FString IndependentNeedle(const TCHAR* Operation, const TCHAR* Qualifier, const TCHAR* Source)
	{
		if (FCString::Strcmp(Qualifier, TEXT("CONST_REMOVAL_INVALID")) == 0)
		{
			return TEXT("RequireMutable(Source)");
		}
		if (FCString::Strcmp(Qualifier, TEXT("CONST_INPUT")) == 0)
		{
			return TEXT("& in Source");
		}
		if (FCString::Strcmp(Operation, TEXT("INITIALIZE")) == 0)
		{
			return TEXT("Alias = Source;");
		}
		if (FCString::Strcmp(Operation, TEXT("ASSIGN")) == 0)
		{
			return TEXT("Alias = Source;");
		}
		if (FCString::Strcmp(Operation, TEXT("PASS")) == 0)
		{
			return TEXT("ObserveReference(Source)");
		}
		if (FCString::Strcmp(Operation, TEXT("RETURN")) == 0)
		{
			return FCString::Strcmp(Qualifier, TEXT("MUTABLE")) == 0
				? TEXT("ReturnReference(Source)")
				: TEXT("ReturnConstReference(Source)");
		}
		if (FCString::Strcmp(Operation, TEXT("IDENTITY")) == 0)
		{
			return TEXT("Source == Alias");
		}
		if (FCString::Strcmp(Operation, TEXT("NULL_COMPARE")) == 0)
		{
			return TEXT("nullptr");
		}
		if (FCString::Strcmp(Operation, TEXT("CAST")) == 0)
		{
			return FCString::Strcmp(Source, TEXT("DERIVED_VIEW")) == 0 ? TEXT("opImplCast()") : TEXT("opCast()");
		}
		if (FCString::Strcmp(Operation, TEXT("MEMBER_ACCESS")) == 0)
		{
			return TEXT("Source.GetValue()");
		}
		if (FCString::Strcmp(Operation, TEXT("ALIAS_MUTATION")) == 0)
		{
			return TEXT("Alias.SetValue(77)");
		}
		return TEXT("MakeRefRoot(11)");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kOperations[] = {
			TEXT("INITIALIZE"), TEXT("ASSIGN"), TEXT("PASS"), TEXT("RETURN"), TEXT("IDENTITY"),
			TEXT("NULL_COMPARE"), TEXT("CAST"), TEXT("MEMBER_ACCESS"), TEXT("ALIAS_MUTATION")};
		static const TCHAR* const kQualifiers[] = {
			TEXT("MUTABLE"), TEXT("CONST_OBJECT"), TEXT("CONST_INPUT"), TEXT("CONST_REMOVAL_INVALID")};
		static const TCHAR* const kSources[] = {
			TEXT("NEW_LOCAL"), TEXT("FIELD"), TEXT("PARAMETER"), TEXT("RETURN"),
			TEXT("BASE_VIEW"), TEXT("DERIVED_VIEW"), TEXT("NATIVE_OBJECT"), TEXT("NULL")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(288);
		for (int32 Index = 0; Index < 288; ++Index)
		{
			const TCHAR* const Operation = kOperations[Index / 32];
			const TCHAR* const Qualifier = kQualifiers[(Index / 8) % 4];
			const TCHAR* const Source = kSources[Index % 8];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-REF-SOURCE-OP-%s-%s-%s"), Operation, Qualifier, Source);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Operation, Qualifier);
			Cell.bFault = IndependentFault(Operation, Qualifier, Source);
			Cell.Expected = (Cell.bReject || Cell.bFault) ? 0 : 1;
			Cell.Needle = IndependentNeedle(Operation, Qualifier, Source);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::ERefSourceOpOperation;
		using AngelscriptTest::Generate::ERefSourceOpQualifier;
		using AngelscriptTest::Generate::ERefSourceOpSource;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FRefSourceOpGenerator;
		using AngelscriptTest::Generate::FRefSourceOpParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FRefSourceOpGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-REF-SOURCE-OP")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangRefSourceOpInitializeMutableNewLocal")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("RefSourceOpGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("RefSourceOpGenerator dump path=%s cases=288 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(200, CaseCount));
		ASSERT_THAT(AreEqual(288, Cases.Num()));
		ASSERT_THAT(AreEqual(88, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int ObserveReference"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("struct FReferenceFieldOwner"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("FRefRoot ProvideReference"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/RefSourceOpGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 RejectCount = 0;
		int32 FaultCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			const FString CaseSource = Generator.BuildCaseSource(Expected.CaseId);
			ASSERT_THAT(IsTrue(CaseSource.Contains(Expected.Needle)));
			if (Expected.bReject)
			{
				++RejectCount;
				ASSERT_THAT(AreEqual(FString(), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::CompileReject));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::SourceOnly));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(RejectIds.Contains(Expected.CaseId)));
				ASSERT_THAT(AreEqual(CaseSource, Generator.BuildRejectSource(Expected.CaseId)));
			}
			else if (Expected.bFault)
			{
				++FaultCount;
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(AreEqual(FString(TEXT("Null pointer access")), Cases[Index].ExpectedException));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
			}
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(1, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(1, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
			}
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}
		ASSERT_THAT(AreEqual(88, RejectCount));
		ASSERT_THAT(AreEqual(7, FaultCount));

		FRefSourceOpParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildReferenceIdentitySource(DefaultParams).Contains(TEXT("int Entry()"))));
		FRefSourceOpParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildReferenceIdentitySource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildReferenceIdentitySource(ProbeParams).IsEmpty()));
		FRefSourceOpParams RejectParams;
		RejectParams.Qualifier = ERefSourceOpQualifier::ConstRemovalInvalid;
		ASSERT_THAT(IsTrue(Generator.BuildReferenceIdentitySource(RejectParams).IsEmpty()));
		DefaultParams.Operation = static_cast<ERefSourceOpOperation>(9);
		ASSERT_THAT(IsTrue(Generator.BuildReferenceIdentitySource(DefaultParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(288, CountOccurrences(Dump, TEXT("// Case: LANG-REF-SOURCE-OP-"))));
		ASSERT_THAT(AreEqual(88, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/RefSourceOpGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("RefSourceOpGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
