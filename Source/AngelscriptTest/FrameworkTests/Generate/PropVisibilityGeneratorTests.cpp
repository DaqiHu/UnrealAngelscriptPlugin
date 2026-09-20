#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestPropVisibilityGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	PropVisibilityGenerator,
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
		bool bHost = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-PROP-VISIBILITY-OWNER_METHOD-READ-DEFAULT");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static bool IndependentOwner(const TCHAR* Path)
	{
		return FCString::Strcmp(Path, TEXT("OWNER_METHOD")) == 0
			|| FCString::Strcmp(Path, TEXT("OWNER_CONSTRUCTOR")) == 0
			|| FCString::Strcmp(Path, TEXT("OWNER_DESTRUCTOR")) == 0
			|| FCString::Strcmp(Path, TEXT("ACCESSOR_BODY")) == 0;
	}

	static bool IndependentDerived(const TCHAR* Path)
	{
		return FCString::Strcmp(Path, TEXT("DIRECT_DERIVED")) == 0
			|| FCString::Strcmp(Path, TEXT("DEEP_DERIVED")) == 0
			|| FCString::Strcmp(Path, TEXT("BASE_VIEW_FROM_DERIVED")) == 0;
	}

	static bool IndependentReject(const TCHAR* Path, const TCHAR* Visibility)
	{
		if (IndependentOwner(Path))
		{
			return false;
		}
		if (IndependentDerived(Path))
		{
			return FCString::Strcmp(Visibility, TEXT("PRIVATE")) == 0;
		}
		return FCString::Strcmp(Visibility, TEXT("DEFAULT")) != 0;
	}

	static FString IndependentNeedle(const TCHAR* Path, const TCHAR* Operation)
	{
		if (FCString::Strcmp(Path, TEXT("OWNER_METHOD")) == 0)
		{
			return TEXT("return Receiver.ProbeOwnerMethod();");
		}
		if (FCString::Strcmp(Path, TEXT("OWNER_CONSTRUCTOR")) == 0)
		{
			return TEXT("return Receiver.ConstructorObservation;");
		}
		if (FCString::Strcmp(Path, TEXT("OWNER_DESTRUCTOR")) == 0)
		{
			return TEXT("return ReadPropertyVisibilityDestructor();");
		}
		if (FCString::Strcmp(Path, TEXT("ACCESSOR_BODY")) == 0)
		{
			return FCString::Strcmp(Operation, TEXT("READ")) == 0
				? TEXT("return Receiver.ExplicitReadAccessor();")
				: TEXT("Receiver.ExplicitWriteAccessor();");
		}
		if (FCString::Strcmp(Path, TEXT("DIRECT_DERIVED")) == 0)
		{
			return TEXT("return Receiver.ProbeDirectDerived();");
		}
		if (FCString::Strcmp(Path, TEXT("DEEP_DERIVED")) == 0)
		{
			return TEXT("return Receiver.ProbeDeepDerived();");
		}
		if (FCString::Strcmp(Path, TEXT("UNRELATED_TYPE")) == 0)
		{
			return TEXT("return Observer.ProbeUnrelated(Receiver);");
		}
		if (FCString::Strcmp(Path, TEXT("BASE_VIEW_FROM_DERIVED")) == 0)
		{
			return TEXT("return Receiver.ProbeBaseView(BaseView);");
		}
		return FCString::Strcmp(Operation, TEXT("READ")) == 0
			? TEXT("int Observed = Receiver.VisibleValue;")
			: TEXT("Receiver.VisibleValue = 67;");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kPaths[] = {
			TEXT("OWNER_METHOD"), TEXT("OWNER_CONSTRUCTOR"), TEXT("OWNER_DESTRUCTOR"), TEXT("ACCESSOR_BODY"),
			TEXT("DIRECT_DERIVED"), TEXT("DEEP_DERIVED"), TEXT("UNRELATED_TYPE"), TEXT("GLOBAL_SAME_MODULE"),
			TEXT("BASE_VIEW_FROM_DERIVED"), TEXT("DERIVED_VIEW_FROM_GLOBAL")};
		static const TCHAR* const kOps[] = {TEXT("READ"), TEXT("WRITE")};
		static const TCHAR* const kVis[] = {TEXT("DEFAULT"), TEXT("PRIVATE"), TEXT("PROTECTED")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(60);
		for (int32 Index = 0; Index < 60; ++Index)
		{
			const TCHAR* const Path = kPaths[Index / 6];
			const TCHAR* const Operation = kOps[(Index / 3) % 2];
			const TCHAR* const Visibility = kVis[Index % 3];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-PROP-VISIBILITY-%s-%s-%s"), Path, Operation, Visibility);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Path, Visibility);
			Cell.Expected = Cell.bReject ? 0 : (FCString::Strcmp(Operation, TEXT("READ")) == 0 ? 41 : 67);
			Cell.Needle = IndependentNeedle(Path, Operation);
			Cell.bHost = FCString::Strcmp(Path, TEXT("OWNER_DESTRUCTOR")) == 0;
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EPropVisibilityAccessPath;
		using AngelscriptTest::Generate::EPropVisibilityVisibility;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FPropVisibilityGenerator;
		using AngelscriptTest::Generate::FPropVisibilityParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FPropVisibilityGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-PROP-VISIBILITY")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangPropVisibilityOwnerMethodReadDefault")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("PropVisibilityGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("PropVisibilityGenerator dump path=%s cases=60 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(42, CaseCount));
		ASSERT_THAT(AreEqual(60, Cases.Num()));
		ASSERT_THAT(AreEqual(18, RejectIds.Num()));

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
			}
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == (Expected.bHost
					? EGeneratedCaseExecution::RequiresHostSetup
					: EGeneratedCaseExecution::Standalone)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
			}
		}
		ASSERT_THAT(AreEqual(18, RejectCount));

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangPropVisibilityOwnerMethodReadDefault()"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int VisibilityDestructorObservation = -1;"))));

		FPropVisibilityParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildPropertyVisibilitySource(DefaultParams).Contains(TEXT("int Entry()"))));
		FPropVisibilityParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildPropertyVisibilitySource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/PropVisibilityGenerator/RepresentativeSources.as"));
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
			ASSERT_THAT(IsTrue(Aggregate.Contains(FString::Printf(TEXT("class FVisibilityOwner_%s\n"), *Expected.EntryName))));
			if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FPropVisibilityParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildPropertyVisibilitySource(InvalidName).IsEmpty()));
		FPropVisibilityParams InvalidEnum = DefaultParams;
		InvalidEnum.AccessPath = static_cast<EPropVisibilityAccessPath>(10);
		ASSERT_THAT(IsTrue(Generator.BuildPropertyVisibilitySource(InvalidEnum).IsEmpty()));
		FPropVisibilityParams RejectParams = DefaultParams;
		RejectParams.AccessPath = EPropVisibilityAccessPath::UnrelatedType;
		RejectParams.Visibility = EPropVisibilityVisibility::Private;
		ASSERT_THAT(IsTrue(Generator.BuildPropertyVisibilitySource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
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
		ASSERT_THAT(AreEqual(60, CountOccurrences(Dump, TEXT("// Case: LANG-PROP-VISIBILITY-"))));
		ASSERT_THAT(AreEqual(18, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/PropVisibilityGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("PropVisibilityGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
