#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestInhAccessGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	InhAccessGenerator,
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

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-INH-ACCESS-DEFAULT-FIELD-OWNER");

	static bool IndependentShouldCompile(const TCHAR* Access, const TCHAR* Member, const TCHAR* Site)
	{
		const bool bPrivate = FCString::Strcmp(Access, TEXT("PRIVATE")) == 0;
		const bool bProtected = FCString::Strcmp(Access, TEXT("PROTECTED")) == 0;
		const bool bCtor = FCString::Strcmp(Member, TEXT("CONSTRUCTOR")) == 0;
		if (bCtor && bPrivate && FCString::Strcmp(Site, TEXT("OWNER")) == 0)
		{
			return false;
		}
		if (FCString::Strcmp(Site, TEXT("OWNER")) == 0)
		{
			return true;
		}
		if (bCtor && bProtected
			&& (FCString::Strcmp(Site, TEXT("UNRELATED")) == 0 || FCString::Strcmp(Site, TEXT("GLOBAL")) == 0))
		{
			return true;
		}
		if (FCString::Strcmp(Site, TEXT("DIRECT_DERIVED")) == 0 || FCString::Strcmp(Site, TEXT("DEEP_DERIVED")) == 0)
		{
			return !bPrivate;
		}
		return !bPrivate && !bProtected;
	}

	static int32 IndependentExpected(const TCHAR* Member, const TCHAR* Site)
	{
		if (FCString::Strcmp(Member, TEXT("FIELD")) == 0)
		{
			return 41;
		}
		if (FCString::Strcmp(Member, TEXT("METHOD")) == 0)
		{
			return 42;
		}
		if (FCString::Strcmp(Member, TEXT("GETTER_SETTER_METHOD")) == 0)
		{
			return 43;
		}
		if (FCString::Strcmp(Site, TEXT("UNRELATED")) == 0)
		{
			return 46;
		}
		if (FCString::Strcmp(Site, TEXT("DIRECT_DERIVED")) == 0 || FCString::Strcmp(Site, TEXT("DEEP_DERIVED")) == 0)
		{
			return 48;
		}
		return 47;
	}

	static FString IndependentNeedle(const TCHAR* Member, const TCHAR* Site)
	{
		if (FCString::Strcmp(Member, TEXT("FIELD")) == 0)
		{
			return TEXT("int Value = 41;");
		}
		if (FCString::Strcmp(Member, TEXT("METHOD")) == 0)
		{
			return TEXT("return 42;");
		}
		if (FCString::Strcmp(Member, TEXT("GETTER_SETTER_METHOD")) == 0)
		{
			return TEXT("SetValue(73);");
		}
		if (FCString::Strcmp(Site, TEXT("UNRELATED")) == 0)
		{
			return TEXT("return 46;");
		}
		if (FCString::Strcmp(Site, TEXT("DIRECT_DERIVED")) == 0 || FCString::Strcmp(Site, TEXT("DEEP_DERIVED")) == 0)
		{
			return TEXT("return 48;");
		}
		return TEXT("return 47;");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kAccess[] = {TEXT("DEFAULT"), TEXT("PROTECTED"), TEXT("PRIVATE")};
		static const TCHAR* const kMembers[] = {
			TEXT("FIELD"), TEXT("METHOD"), TEXT("GETTER_SETTER_METHOD"), TEXT("CONSTRUCTOR")};
		static const TCHAR* const kSites[] = {
			TEXT("OWNER"), TEXT("DIRECT_DERIVED"), TEXT("DEEP_DERIVED"), TEXT("UNRELATED"), TEXT("GLOBAL")};
		TArray<FExpectedCell> Cells;
		Cells.SetNum(60);
		for (int32 Index = 0; Index < 60; ++Index)
		{
			const TCHAR* const Access = kAccess[Index / 20];
			const TCHAR* const Member = kMembers[(Index / 5) % 4];
			const TCHAR* const Site = kSites[Index % 5];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-INH-ACCESS-%s-%s-%s"), Access, Member, Site);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = !IndependentShouldCompile(Access, Member, Site);
			Cell.Expected = Cell.bReject ? 0 : IndependentExpected(Member, Site);
			Cell.Needle = IndependentNeedle(Member, Site);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EInhAccessKind;
		using AngelscriptTest::Generate::EInhAccessMember;
		using AngelscriptTest::Generate::EInhAccessSite;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FInhAccessGenerator;
		using AngelscriptTest::Generate::FInhAccessParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FInhAccessGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-INH-ACCESS")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangInhAccessDefaultFieldOwner")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("InhAccessGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("InhAccessGenerator dump path=%s cases=60 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(37, CaseCount));
		ASSERT_THAT(AreEqual(60, Cases.Num()));
		ASSERT_THAT(AreEqual(23, RejectIds.Num()));

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
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
			}
		}
		ASSERT_THAT(AreEqual(23, RejectCount));

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangInhAccessDefaultFieldOwner()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int EntryLangInhAccessPrivateFieldDirect"))));
		ASSERT_THAT(AreEqual(37, CountOccurrences(Aggregate, TEXT("class FAccessBase"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("FAccessBaseDefaultFieldOwner"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("FAccessBaseDefaultFieldGlobal"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));

		FInhAccessParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildInheritanceAccessSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FInhAccessParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildInheritanceAccessSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(Generator.BuildInheritanceAccessSource(ProbeParams).Contains(TEXT("int Entry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/InhAccessGenerator/RepresentativeSources.as"));
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

		FInhAccessParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildInheritanceAccessSource(InvalidName).IsEmpty()));
		FInhAccessParams InvalidEnum = DefaultParams;
		InvalidEnum.Access = static_cast<EInhAccessKind>(3);
		ASSERT_THAT(IsTrue(Generator.BuildInheritanceAccessSource(InvalidEnum).IsEmpty()));
		FInhAccessParams RejectParams = DefaultParams;
		RejectParams.Access = EInhAccessKind::Private;
		RejectParams.Site = EInhAccessSite::DirectDerived;
		ASSERT_THAT(IsTrue(Generator.BuildInheritanceAccessSource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-INH-CAST-MUTABLE-EXACT-ASSIGN")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(41, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(60, CountOccurrences(Dump, TEXT("// Case: LANG-INH-ACCESS-"))));
		ASSERT_THAT(AreEqual(23, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/InhAccessGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("InhAccessGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
