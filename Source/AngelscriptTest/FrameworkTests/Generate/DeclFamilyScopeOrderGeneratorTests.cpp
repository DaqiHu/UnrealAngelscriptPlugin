#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestDeclFamilyScopeOrderGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	DeclFamilyScopeOrderGenerator,
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

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-DECL-FAMILY-SCOPE-ORDER-FUNCTION-GLOBAL-BEFORE_USE");

	static FString IndependentNeedle(const TCHAR* Family)
	{
		if (FCString::Strcmp(Family, TEXT("FUNCTION")) == 0)
		{
			return TEXT("FamilyPublishedFunction");
		}
		if (FCString::Strcmp(Family, TEXT("METHOD")) == 0)
		{
			return TEXT("FFamilyMethod");
		}
		if (FCString::Strcmp(Family, TEXT("CLASS")) == 0)
		{
			return TEXT("FFamilyClass");
		}
		if (FCString::Strcmp(Family, TEXT("STRUCT")) == 0)
		{
			return TEXT("FFamilyStruct");
		}
		if (FCString::Strcmp(Family, TEXT("FIELD")) == 0)
		{
			return TEXT("FFamilyField");
		}
		if (FCString::Strcmp(Family, TEXT("CONSTRUCTOR")) == 0)
		{
			return TEXT("FFamilyConstructor");
		}
		if (FCString::Strcmp(Family, TEXT("DESTRUCTOR")) == 0)
		{
			return TEXT("~FFamilyDestructor");
		}
		if (FCString::Strcmp(Family, TEXT("NAMESPACE")) == 0)
		{
			return TEXT("FamilyNamespace");
		}
		if (FCString::Strcmp(Family, TEXT("ENUM")) == 0)
		{
			return TEXT("EFamilyPublished");
		}
		return TEXT("typedef int FamilyPublishedAlias");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kFamilies[] = {
			TEXT("FUNCTION"), TEXT("METHOD"), TEXT("CLASS"), TEXT("STRUCT"), TEXT("FIELD"),
			TEXT("CONSTRUCTOR"), TEXT("DESTRUCTOR"), TEXT("NAMESPACE"), TEXT("ENUM"), TEXT("TYPEDEF")};
		static const TCHAR* const kScopes[] = {
			TEXT("GLOBAL"), TEXT("NAMESPACE"), TEXT("NESTED_NAMESPACE"), TEXT("MEMBER"), TEXT("MULTIPLE_SECTIONS")};
		static const TCHAR* const kOrderings[] = {
			TEXT("BEFORE_USE"), TEXT("FORWARD_USE"), TEXT("SAME_SECTION"), TEXT("LATER_SECTION"),
			TEXT("REVERSED_SECTIONS"), TEXT("REBUILD")};
		TArray<FExpectedCell> Cells;
		Cells.SetNum(300);
		for (int32 Index = 0; Index < 300; ++Index)
		{
			const int32 FamilyIndex = Index / 30;
			const int32 ScopeIndex = (Index / 6) % 5;
			const int32 OrderingIndex = Index % 6;
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-DECL-FAMILY-SCOPE-ORDER-%s-%s-%s"),
				kFamilies[FamilyIndex],
				kScopes[ScopeIndex],
				kOrderings[OrderingIndex]);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Expected = (100 + FamilyIndex) + (200 + ScopeIndex) + (300 + OrderingIndex);
			Cell.Needle = IndependentNeedle(kFamilies[FamilyIndex]);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EDeclFamilyScopeOrderFamily;
		using AngelscriptTest::Generate::EDeclFamilyScopeOrderOrdering;
		using AngelscriptTest::Generate::EDeclFamilyScopeOrderScope;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FDeclFamilyScopeOrderGenerator;
		using AngelscriptTest::Generate::FDeclFamilyScopeOrderParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FDeclFamilyScopeOrderGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-DECL-FAMILY-SCOPE-ORDER")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangDeclFamilyScopeOrderFunctionGlobalBeforeUse")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("DeclFamilyScopeOrderGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("DeclFamilyScopeOrderGenerator dump path=%s cases=300 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(300, CaseCount));
		ASSERT_THAT(AreEqual(300, Cases.Num()));

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
		ASSERT_THAT(AreEqual(600, Generator.GetExpected(ProbeCaseId)));

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangDeclFamilyScopeOrderFunctionGlobalBeforeUse()"))));
		ASSERT_THAT(AreEqual(300, CountOccurrences(Aggregate, TEXT("int FamilyWitness"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("typedef int FamilyPublishedAlias"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("BeginNativeScriptLifecycle"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));

		FDeclFamilyScopeOrderParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildDeclarationSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FDeclFamilyScopeOrderParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildDeclarationSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(Generator.BuildDeclarationSource(ProbeParams).Contains(TEXT("int Entry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/DeclFamilyScopeOrderGenerator/RepresentativeSources.as"));
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

		FDeclFamilyScopeOrderParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildDeclarationSource(InvalidName).IsEmpty()));
		FDeclFamilyScopeOrderParams InvalidEnum = DefaultParams;
		InvalidEnum.Family = static_cast<EDeclFamilyScopeOrderFamily>(15);
		ASSERT_THAT(IsTrue(Generator.BuildDeclarationSource(InvalidEnum).IsEmpty()));
		FDeclFamilyScopeOrderParams FilteredParams = DefaultParams;
		FilteredParams.Family = EDeclFamilyScopeOrderFamily::Funcdef;
		ASSERT_THAT(IsTrue(Generator.BuildDeclarationSource(FilteredParams).IsEmpty()));
		FilteredParams.Family = EDeclFamilyScopeOrderFamily::Function;
		FilteredParams.Scope = EDeclFamilyScopeOrderScope::ImportedModule;
		ASSERT_THAT(IsTrue(Generator.BuildDeclarationSource(FilteredParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-INH-ACCESS-DEFAULT-FIELD-OWNER")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(600, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(300, CountOccurrences(Dump, TEXT("// Case: LANG-DECL-FAMILY-SCOPE-ORDER-"))));
		ASSERT_THAT(AreEqual(0, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/DeclFamilyScopeOrderGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("DeclFamilyScopeOrderGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
