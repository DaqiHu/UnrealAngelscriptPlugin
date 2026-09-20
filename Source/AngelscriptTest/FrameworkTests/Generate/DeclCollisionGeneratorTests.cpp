#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestDeclCollisionGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	DeclCollisionGenerator,
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

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-DECL-COLLISION-LEFT_THEN_RIGHT-SAME-FUNCTION_OVERLOAD");

	static bool IndependentProperty(const TCHAR* Pair)
	{
		return FCString::Strcmp(Pair, TEXT("METHOD_PROPERTY")) == 0
			|| FCString::Strcmp(Pair, TEXT("FIELD_PROPERTY")) == 0
			|| FCString::Strcmp(Pair, TEXT("PROPERTY_GET_SET")) == 0
			|| FCString::Strcmp(Pair, TEXT("PROPERTY_DUPLICATE_GET")) == 0
			|| FCString::Strcmp(Pair, TEXT("PROPERTY_DUPLICATE_SET")) == 0;
	}

	static bool IndependentLegalSameOwner(const TCHAR* Pair)
	{
		return FCString::Strcmp(Pair, TEXT("FUNCTION_OVERLOAD")) == 0
			|| FCString::Strcmp(Pair, TEXT("METHOD_OVERLOAD")) == 0;
	}

	static bool IndependentShouldCompile(const TCHAR* Relation, const TCHAR* Pair)
	{
		return !IndependentProperty(Pair)
			&& (FCString::Strcmp(Relation, TEXT("SAME")) != 0 || IndependentLegalSameOwner(Pair));
	}

	static FString IndependentNeedle(const TCHAR* Pair)
	{
		if (FCString::Strcmp(Pair, TEXT("PROPERTY_DUPLICATE_SET")) == 0)
		{
			return TEXT("\tset");
		}
		if (IndependentProperty(Pair))
		{
			return TEXT("\tget");
		}
		if (FCString::Strcmp(Pair, TEXT("FUNCTION_OVERLOAD")) == 0
			|| FCString::Strcmp(Pair, TEXT("FUNCTION_DUPLICATE")) == 0
			|| FCString::Strcmp(Pair, TEXT("FUNCTION_TYPE")) == 0
			|| FCString::Strcmp(Pair, TEXT("FUNCTION_ENUM")) == 0)
		{
			return TEXT("return 11;");
		}
		if (FCString::Strcmp(Pair, TEXT("TYPE_DUPLICATE")) == 0 || FCString::Strcmp(Pair, TEXT("TYPE_ENUM")) == 0)
		{
			return TEXT("int Value = 13;");
		}
		if (FCString::Strcmp(Pair, TEXT("ENUM_DUPLICATE")) == 0)
		{
			return TEXT("ClashValue = 14");
		}
		if (FCString::Strcmp(Pair, TEXT("FIELD_DUPLICATE")) == 0)
		{
			return TEXT("int Clash = 23;");
		}
		return TEXT("return 21;");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kOrders[] = {TEXT("LEFT_THEN_RIGHT"), TEXT("RIGHT_THEN_LEFT")};
		static const TCHAR* const kRelations[] = {TEXT("SAME"), TEXT("DIFFERENT"), TEXT("NESTED")};
		static const TCHAR* const kPairs[] = {
			TEXT("FUNCTION_OVERLOAD"), TEXT("FUNCTION_DUPLICATE"), TEXT("FUNCTION_TYPE"), TEXT("FUNCTION_ENUM"),
			TEXT("TYPE_DUPLICATE"), TEXT("TYPE_ENUM"), TEXT("ENUM_DUPLICATE"), TEXT("METHOD_OVERLOAD"),
			TEXT("METHOD_DUPLICATE"), TEXT("METHOD_FIELD"), TEXT("METHOD_PROPERTY"), TEXT("FIELD_DUPLICATE"),
			TEXT("FIELD_PROPERTY"), TEXT("PROPERTY_GET_SET"), TEXT("PROPERTY_DUPLICATE_GET"),
			TEXT("PROPERTY_DUPLICATE_SET")};
		TArray<FExpectedCell> Cells;
		Cells.SetNum(96);
		for (int32 Index = 0; Index < 96; ++Index)
		{
			const TCHAR* const Order = kOrders[Index / 48];
			const TCHAR* const Relation = kRelations[(Index / 16) % 3];
			const TCHAR* const Pair = kPairs[Index % 16];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-DECL-COLLISION-%s-%s-%s"), Order, Relation, Pair);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = !IndependentShouldCompile(Relation, Pair);
			Cell.Expected = Cell.bReject ? 0 : 71;
			Cell.Needle = IndependentNeedle(Pair);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EDeclCollisionOrder;
		using AngelscriptTest::Generate::EDeclCollisionPair;
		using AngelscriptTest::Generate::EDeclCollisionRelation;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FDeclCollisionGenerator;
		using AngelscriptTest::Generate::FDeclCollisionParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FDeclCollisionGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-DECL-COLLISION")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangDeclCollisionLeftThenRightSameFunctionOverload")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("DeclCollisionGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("DeclCollisionGenerator dump path=%s cases=96 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(48, CaseCount));
		ASSERT_THAT(AreEqual(96, Cases.Num()));
		ASSERT_THAT(AreEqual(48, RejectIds.Num()));

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
			}
			else
			{
				ASSERT_THAT(AreEqual(
					FString::Printf(TEXT("int %s()"), *Expected.EntryName),
					Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(71, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(71, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
			}
		}
		ASSERT_THAT(AreEqual(48, RejectCount));

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangDeclCollisionLeftThenRightSameFunctionOverload()"))));
		ASSERT_THAT(AreEqual(48, CountOccurrences(Aggregate, TEXT("return 71;"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("namespace CollisionLeftThenRightSameFunctionOverload"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));

		FDeclCollisionParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildCollisionSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FDeclCollisionParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildCollisionSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(Generator.BuildCollisionSource(ProbeParams).Contains(TEXT("int Entry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/DeclCollisionGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			const FString CaseSource = Expected.bReject
				? Generator.BuildRejectSource(Expected.CaseId)
				: Generator.BuildCaseSource(Expected.CaseId);
			ASSERT_THAT(IsTrue(CaseSource.Contains(Expected.Needle)));
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(CaseSource, Expected.EntryName, GeneratedBody)));
			if (Representative.Contains(Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FDeclCollisionParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildCollisionSource(InvalidName).IsEmpty()));
		FDeclCollisionParams InvalidEnum = DefaultParams;
		InvalidEnum.Pair = static_cast<EDeclCollisionPair>(16);
		ASSERT_THAT(IsTrue(Generator.BuildCollisionSource(InvalidEnum).IsEmpty()));
		FDeclCollisionParams RejectParams = DefaultParams;
		RejectParams.Pair = EDeclCollisionPair::FunctionDuplicate;
		ASSERT_THAT(IsTrue(Generator.BuildCollisionSource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-INH-ACCESS-DEFAULT-FIELD-OWNER")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(71, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(96, CountOccurrences(Dump, TEXT("// Case: LANG-DECL-COLLISION-"))));
		ASSERT_THAT(AreEqual(48, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/DeclCollisionGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("DeclCollisionGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
