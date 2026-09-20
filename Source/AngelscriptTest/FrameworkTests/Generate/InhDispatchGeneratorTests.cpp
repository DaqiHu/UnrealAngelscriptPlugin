#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestInhDispatchGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	InhDispatchGenerator,
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
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-INH-DISPATCH-BASE-FIELD-DERIVED_OBJECT-DIRECT");

	static int32 IndependentLevel(const TCHAR* Depth)
	{
		if (FCString::Strcmp(Depth, TEXT("TWO_LEVELS")) == 0)
		{
			return 2;
		}
		if (FCString::Strcmp(Depth, TEXT("THREE_LEVELS")) == 0)
		{
			return 3;
		}
		if (FCString::Strcmp(Depth, TEXT("DEEP")) == 0)
		{
			return 8;
		}
		return 1;
	}

	static bool IndependentExplicitBase(const TCHAR* View, const TCHAR* Invocation)
	{
		return FCString::Strcmp(View, TEXT("EXPLICIT_BASE")) == 0
			|| FCString::Strcmp(Invocation, TEXT("EXPLICIT_BASE")) == 0;
	}

	static int32 IndependentExpected(const TCHAR* Depth, const TCHAR* Member, const TCHAR* View, const TCHAR* Invocation)
	{
		const int32 Level = IndependentLevel(Depth);
		if (FCString::Strcmp(Member, TEXT("NONVIRTUAL_METHOD")) == 0)
		{
			return 100;
		}
		if (IndependentExplicitBase(View, Invocation))
		{
			if (FCString::Strcmp(Member, TEXT("FIELD")) == 0)
			{
				return 701;
			}
			if (FCString::Strcmp(Member, TEXT("SETTER_METHOD")) == 0)
			{
				return 107;
			}
			return 100;
		}
		if (FCString::Strcmp(Member, TEXT("FIELD")) == 0)
		{
			return FCString::Strcmp(Invocation, TEXT("VIRTUAL_ROUTE")) == 0 ? 701 + Level * 10 : 701;
		}
		if (FCString::Strcmp(Member, TEXT("SETTER_METHOD")) == 0)
		{
			return 100 + Level * 100 + 7;
		}
		return 100 + Level * 100;
	}

	static FString IndependentNeedle(const TCHAR* Member)
	{
		if (FCString::Strcmp(Member, TEXT("FIELD")) == 0)
		{
			return TEXT("DispatchField");
		}
		if (FCString::Strcmp(Member, TEXT("SETTER_METHOD")) == 0)
		{
			return TEXT("SetValue(7)");
		}
		if (FCString::Strcmp(Member, TEXT("NONVIRTUAL_METHOD")) == 0)
		{
			return TEXT("StableValue");
		}
		if (FCString::Strcmp(Member, TEXT("GETTER_METHOD")) == 0)
		{
			return TEXT("GetValue");
		}
		if (FCString::Strcmp(Member, TEXT("OVERRIDE")) == 0)
		{
			return TEXT("OverrideValue");
		}
		return TEXT("VirtualValue");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kDepths[] = {TEXT("BASE"), TEXT("TWO_LEVELS"), TEXT("THREE_LEVELS"), TEXT("DEEP")};
		static const TCHAR* const kMembers[] = {
			TEXT("FIELD"), TEXT("NONVIRTUAL_METHOD"), TEXT("VIRTUAL_METHOD"), TEXT("OVERRIDE"),
			TEXT("GETTER_METHOD"), TEXT("SETTER_METHOD")};
		static const TCHAR* const kViews[] = {
			TEXT("DERIVED_OBJECT"), TEXT("BASE_VIEW"), TEXT("EXPLICIT_BASE"), TEXT("OWNER"), TEXT("DERIVED_IMPL")};
		static const TCHAR* const kInvocations[] = {TEXT("DIRECT"), TEXT("VIRTUAL_ROUTE"), TEXT("EXPLICIT_BASE")};
		TArray<FExpectedCell> Cells;
		Cells.SetNum(360);
		for (int32 Index = 0; Index < 360; ++Index)
		{
			const TCHAR* const Depth = kDepths[Index / 90];
			const TCHAR* const Member = kMembers[(Index / 15) % 6];
			const TCHAR* const View = kViews[(Index / 3) % 5];
			const TCHAR* const Invocation = kInvocations[Index % 3];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-INH-DISPATCH-%s-%s-%s-%s"),
				Depth,
				Member,
				View,
				Invocation);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Expected = IndependentExpected(Depth, Member, View, Invocation);
			Cell.Needle = IndependentNeedle(Member);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EInhDispatchDepth;
		using AngelscriptTest::Generate::EInhDispatchInvocation;
		using AngelscriptTest::Generate::EInhDispatchMember;
		using AngelscriptTest::Generate::EInhDispatchView;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FInhDispatchGenerator;
		using AngelscriptTest::Generate::FInhDispatchParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FInhDispatchGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-INH-DISPATCH")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangInhDispatchBaseFieldDerivedObjectDirect")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("InhDispatchGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("InhDispatchGenerator dump path=%s cases=360 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(360, CaseCount));
		ASSERT_THAT(AreEqual(360, Cases.Num()));

		int32 RejectCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(
				FString::Printf(TEXT("int %s()"), *Expected.EntryName),
				Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			if (Cases[Index].Outcome == EGeneratedCaseOutcome::CompileReject)
			{
				++RejectCount;
			}
		}
		ASSERT_THAT(AreEqual(0, RejectCount));

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangInhDispatchBaseFieldDerivedObjectDirect()"))));
		ASSERT_THAT(AreEqual(360, CountOccurrences(Aggregate, TEXT("class FDispatchPrimary"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));

		FInhDispatchParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildInheritanceDispatchSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FInhDispatchParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildInheritanceDispatchSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(Generator.BuildInheritanceDispatchSource(ProbeParams).Contains(TEXT("int Entry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/InhDispatchGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			const FString CaseSource = Generator.BuildCaseSource(Expected.CaseId);
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

		FInhDispatchParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildInheritanceDispatchSource(InvalidName).IsEmpty()));
		FInhDispatchParams InvalidEnum = DefaultParams;
		InvalidEnum.Depth = static_cast<EInhDispatchDepth>(4);
		ASSERT_THAT(IsTrue(Generator.BuildInheritanceDispatchSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-INH-ACCESS-DEFAULT-FIELD-OWNER")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(701, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(360, CountOccurrences(Dump, TEXT("// Case: LANG-INH-DISPATCH-"))));
		ASSERT_THAT(AreEqual(0, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/InhDispatchGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("InhDispatchGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
