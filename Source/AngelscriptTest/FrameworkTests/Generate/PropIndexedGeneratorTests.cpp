#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestPropIndexedGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	PropIndexedGenerator,
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
		int32 Marker = 0;
		bool bReject = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-PROP-INDEXED-SAME_TYPE-INT8-READ-MUTABLE");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static bool IndependentFirstMatches(const TCHAR* Candidate, const TCHAR* IndexType)
	{
		if (FCString::Strcmp(Candidate, TEXT("UNRELATED")) == 0)
		{
			return false;
		}
		if (FCString::Strcmp(Candidate, TEXT("SAME_TYPE")) == 0
			|| FCString::Strcmp(Candidate, TEXT("COMPETING_PRIMARY_FIRST")) == 0)
		{
			return true;
		}
		return FCString::Strcmp(IndexType, TEXT("BOOL")) != 0 && FCString::Strcmp(IndexType, TEXT("ENUM")) != 0;
	}

	static bool IndependentCompeting(const TCHAR* Candidate)
	{
		return FCString::Strcmp(Candidate, TEXT("COMPETING_PRIMARY_FIRST")) == 0
			|| FCString::Strcmp(Candidate, TEXT("COMPETING_SECONDARY_FIRST")) == 0;
	}

	static bool IndependentReject(const TCHAR* Candidate, const TCHAR* IndexType, const TCHAR* Operation, const TCHAR* Receiver)
	{
		if (FCString::Strcmp(Operation, TEXT("COMPOUND")) == 0)
		{
			return true;
		}
		if (FCString::Strcmp(Operation, TEXT("READ")) == 0 && IndependentCompeting(Candidate))
		{
			return true;
		}
		if (!IndependentFirstMatches(Candidate, IndexType))
		{
			return true;
		}
		return FCString::Strcmp(Operation, TEXT("WRITE")) == 0 && FCString::Strcmp(Receiver, TEXT("CONST")) == 0;
	}

	static int32 IndependentMarker(const TCHAR* Candidate)
	{
		if (FCString::Strcmp(Candidate, TEXT("SAME_TYPE")) == 0)
		{
			return 101;
		}
		if (FCString::Strcmp(Candidate, TEXT("ADJACENT_NUMERIC")) == 0)
		{
			return 201;
		}
		if (FCString::Strcmp(Candidate, TEXT("CROSS_FAMILY")) == 0)
		{
			return 301;
		}
		if (FCString::Strcmp(Candidate, TEXT("COMPETING_PRIMARY_FIRST")) == 0)
		{
			return 401;
		}
		if (FCString::Strcmp(Candidate, TEXT("COMPETING_SECONDARY_FIRST")) == 0)
		{
			return 402;
		}
		return 0;
	}

	static int32 IndependentArgument(const TCHAR* IndexType)
	{
		return FCString::Strcmp(IndexType, TEXT("BOOL")) == 0 ? 1 : 3;
	}

	static int32 IndependentExpected(const TCHAR* Candidate, const TCHAR* IndexType, const TCHAR* Operation)
	{
		const int32 Marker = IndependentMarker(Candidate);
		const int32 Argument = IndependentArgument(IndexType);
		return FCString::Strcmp(Operation, TEXT("READ")) == 0
			? Marker * 100 + Argument
			: Marker * 10000 + Argument * 100 + 73;
	}

	static FString IndependentTypeNeedle(const TCHAR* IndexType)
	{
		if (FCString::Strcmp(IndexType, TEXT("INT8")) == 0)
		{
			return TEXT("\tint8 Index = 3;");
		}
		if (FCString::Strcmp(IndexType, TEXT("INT16")) == 0)
		{
			return TEXT("\tint16 Index = 3;");
		}
		if (FCString::Strcmp(IndexType, TEXT("INT64")) == 0)
		{
			return TEXT("\tint64 Index = 3;");
		}
		if (FCString::Strcmp(IndexType, TEXT("UINT8")) == 0)
		{
			return TEXT("\tuint8 Index = 3;");
		}
		if (FCString::Strcmp(IndexType, TEXT("UINT16")) == 0)
		{
			return TEXT("\tuint16 Index = 3;");
		}
		if (FCString::Strcmp(IndexType, TEXT("UINT")) == 0)
		{
			return TEXT("\tuint Index = 3;");
		}
		if (FCString::Strcmp(IndexType, TEXT("UINT64")) == 0)
		{
			return TEXT("\tuint64 Index = 3;");
		}
		if (FCString::Strcmp(IndexType, TEXT("FLOAT32")) == 0)
		{
			return TEXT("\tfloat Index = 3.0f;");
		}
		if (FCString::Strcmp(IndexType, TEXT("FLOAT64")) == 0)
		{
			return TEXT("\tdouble Index = 3.0;");
		}
		if (FCString::Strcmp(IndexType, TEXT("BOOL")) == 0)
		{
			return TEXT("\tbool Index = true;");
		}
		if (FCString::Strcmp(IndexType, TEXT("ENUM")) == 0)
		{
			return TEXT("\tERegisteredIndex Index = ERegisteredIndex::Three;");
		}
		if (FCString::Strcmp(IndexType, TEXT("TYPEDEF")) == 0)
		{
			return TEXT("\tRegisteredIndexAlias Index = 3;");
		}
		return TEXT("\tint Index = 3;");
	}

	static FString IndependentNeedle(const TCHAR* Operation)
	{
		if (FCString::Strcmp(Operation, TEXT("READ")) == 0)
		{
			return TEXT("return Receiver.Value[Index];");
		}
		if (FCString::Strcmp(Operation, TEXT("WRITE")) == 0)
		{
			return TEXT("Receiver.Value[Index] = 73;");
		}
		return TEXT("Receiver.Value[Index] += 5;");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kCandidates[] = {
			TEXT("SAME_TYPE"), TEXT("ADJACENT_NUMERIC"), TEXT("CROSS_FAMILY"),
			TEXT("COMPETING_PRIMARY_FIRST"), TEXT("COMPETING_SECONDARY_FIRST"), TEXT("UNRELATED")};
		static const TCHAR* const kTypes[] = {
			TEXT("INT8"), TEXT("INT16"), TEXT("INT"), TEXT("INT64"), TEXT("UINT8"), TEXT("UINT16"),
			TEXT("UINT"), TEXT("UINT64"), TEXT("FLOAT32"), TEXT("FLOAT64"), TEXT("BOOL"), TEXT("ENUM"),
			TEXT("TYPEDEF")};
		static const TCHAR* const kOps[] = {TEXT("READ"), TEXT("WRITE"), TEXT("COMPOUND")};
		static const TCHAR* const kReceivers[] = {TEXT("MUTABLE"), TEXT("CONST")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(468);
		for (int32 Index = 0; Index < 468; ++Index)
		{
			const TCHAR* const Candidate = kCandidates[Index / 78];
			const TCHAR* const Type = kTypes[(Index / 6) % 13];
			const TCHAR* const Operation = kOps[(Index / 2) % 3];
			const TCHAR* const Receiver = kReceivers[Index % 2];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-PROP-INDEXED-%s-%s-%s-%s"), Candidate, Type, Operation, Receiver);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Candidate, Type, Operation, Receiver);
			Cell.Expected = Cell.bReject ? 0 : IndependentExpected(Candidate, Type, Operation);
			Cell.Marker = IndependentMarker(Candidate);
			Cell.Needle = IndependentNeedle(Operation);
			Cell.TypeNeedle = IndependentTypeNeedle(Type);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EPropIndexedCandidateSet;
		using AngelscriptTest::Generate::EPropIndexedOperation;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FPropIndexedGenerator;
		using AngelscriptTest::Generate::FPropIndexedParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FPropIndexedGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-PROP-INDEXED")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangPropIndexedSameTypeInt8ReadMutable")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("PropIndexedGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("PropIndexedGenerator dump path=%s cases=468 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(129, CaseCount));
		ASSERT_THAT(AreEqual(468, Cases.Num()));
		ASSERT_THAT(AreEqual(339, RejectIds.Num()));

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
				FString RejectBody;
				ASSERT_THAT(IsTrue(ExtractFunction(RejectSource, Expected.EntryName, RejectBody)));
				ASSERT_THAT(IsTrue(RejectBody.Contains(Expected.Needle)));
				ASSERT_THAT(IsTrue(RejectBody.Contains(Expected.TypeNeedle)));
			}
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
			}
		}
		ASSERT_THAT(AreEqual(339, RejectCount));

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangPropIndexedSameTypeInt8ReadMutable()"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("enum ERegisteredIndex\n"))));

		FPropIndexedParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildIndexedPropertySource(DefaultParams).Contains(TEXT("int Entry()"))));
		FPropIndexedParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildIndexedPropertySource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/PropIndexedGenerator/RepresentativeSources.as"));
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
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.TypeNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(
				FString::Printf(TEXT("const int CandidateMarker = %d;"), Expected.Marker))));
			if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FPropIndexedParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildIndexedPropertySource(InvalidName).IsEmpty()));
		FPropIndexedParams InvalidEnum = DefaultParams;
		InvalidEnum.CandidateSet = static_cast<EPropIndexedCandidateSet>(6);
		ASSERT_THAT(IsTrue(Generator.BuildIndexedPropertySource(InvalidEnum).IsEmpty()));
		FPropIndexedParams RejectParams = DefaultParams;
		RejectParams.Operation = EPropIndexedOperation::Compound;
		ASSERT_THAT(IsTrue(Generator.BuildIndexedPropertySource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(ExpectedCells[0].Expected, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(468, CountOccurrences(Dump, TEXT("// Case: LANG-PROP-INDEXED-"))));
		ASSERT_THAT(AreEqual(339, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/PropIndexedGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("PropIndexedGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
