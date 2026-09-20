#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestPropForkSemanticsGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	PropForkSemanticsGenerator,
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
		TEXT("LANG-PROP-FORK-SEMANTICS-DIRECT_METHOD-READ_WRITE_INDEXED");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kFamilies[] = {
			TEXT("SCRIPT_DECORATOR"), TEXT("SCRIPT_DECORATOR"), TEXT("SCRIPT_DECORATOR"), TEXT("SCRIPT_DECORATOR"),
			TEXT("NATIVE_REGISTRATION"), TEXT("NATIVE_REGISTRATION"), TEXT("NATIVE_REGISTRATION"),
			TEXT("NATIVE_REGISTRATION"),
			TEXT("AUTOMATIC_ACCESS"), TEXT("AUTOMATIC_ACCESS"), TEXT("AUTOMATIC_ACCESS"), TEXT("AUTOMATIC_ACCESS"),
			TEXT("DIRECT_METHOD")};
		static const TCHAR* const kVariants[] = {
			TEXT("GETTER"), TEXT("SETTER"), TEXT("INDEXED_GETTER"), TEXT("INDEXED_SETTER"),
			TEXT("GETTER"), TEXT("SETTER"), TEXT("INDEXED_GETTER"), TEXT("INDEXED_SETTER"),
			TEXT("READ"), TEXT("WRITE"), TEXT("INDEXED_READ"), TEXT("INDEXED_WRITE"),
			TEXT("READ_WRITE_INDEXED")};
		static const TCHAR* const kNeedles[] = {
			TEXT("int get_Value() property"),
			TEXT("void set_Value(int Value) property"),
			TEXT("int get_Value(int Index) property"),
			TEXT("void set_Value(int Index, int Value) property"),
			TEXT("int get_Value() const property"),
			TEXT("void set_Value(int Value) property"),
			TEXT("int get_Value(int Index) const property"),
			TEXT("void set_Value(int Index, int Value) property"),
			TEXT("return Receiver.Value;"),
			TEXT("Receiver.Value = 41;"),
			TEXT("return Receiver.Value[2];"),
			TEXT("Receiver.Value[2] = 41;"),
			TEXT("return Receiver.GetValue() + Receiver.GetIndexedValue(2);")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(13);
		for (int32 Index = 0; Index < 13; ++Index)
		{
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-PROP-FORK-SEMANTICS-%s-%s"), kFamilies[Index], kVariants[Index]);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.Needle = kNeedles[Index];
			Cell.bReject = FCString::Strcmp(kFamilies[Index], TEXT("DIRECT_METHOD")) != 0;
			Cell.Expected = Cell.bReject ? 0 : 88;
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EPropForkSemanticsFamily;
		using AngelscriptTest::Generate::EPropForkSemanticsVariant;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FPropForkSemanticsGenerator;
		using AngelscriptTest::Generate::FPropForkSemanticsParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FPropForkSemanticsGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-PROP-FORK-SEMANTICS")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangPropForkSemanticsDirectMethodReadWriteIndexed")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("PropForkSemanticsGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("PropForkSemanticsGenerator dump path=%s cases=13 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(1, CaseCount));
		ASSERT_THAT(AreEqual(13, Cases.Num()));
		ASSERT_THAT(AreEqual(12, RejectIds.Num()));

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
				if (Expected.Needle.Contains(TEXT("Receiver.Value")))
				{
					ASSERT_THAT(IsTrue(RejectBody.Contains(Expected.Needle)));
				}
			}
			else
			{
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(88, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(88, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::RequiresHostSetup));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
			}
		}
		ASSERT_THAT(AreEqual(12, RejectCount));

		ASSERT_THAT(AreEqual(Generator.BuildCaseSource(ProbeCaseId), Generator.BuildCaseSource(ProbeCaseId)));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangPropForkSemanticsDirectMethodReadWriteIndexed()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int EntryLangPropForkSemanticsScriptDecorator"))));

		FPropForkSemanticsParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildDirectMethodSource(DefaultParams).IsEmpty()));
		FPropForkSemanticsParams ProbeParams;
		ProbeParams.Family = EPropForkSemanticsFamily::DirectMethod;
		ProbeParams.Variant = EPropForkSemanticsVariant::ReadWriteIndexed;
		ASSERT_THAT(IsTrue(Generator.BuildDirectMethodSource(ProbeParams).Contains(TEXT("int Entry()"))));
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildDirectMethodSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/PropForkSemanticsGenerator/RepresentativeSources.as"));
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
			if (Representative.Contains(FString::Printf(TEXT("int %s()"), *Expected.EntryName)))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FPropForkSemanticsParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildDirectMethodSource(InvalidName).IsEmpty()));
		FPropForkSemanticsParams InvalidEnum = ProbeParams;
		InvalidEnum.Family = static_cast<EPropForkSemanticsFamily>(4);
		ASSERT_THAT(IsTrue(Generator.BuildDirectMethodSource(InvalidEnum).IsEmpty()));
		FPropForkSemanticsParams Mismatch = ProbeParams;
		Mismatch.Family = EPropForkSemanticsFamily::ScriptDecorator;
		Mismatch.Variant = EPropForkSemanticsVariant::ReadWriteIndexed;
		ASSERT_THAT(IsTrue(Generator.BuildDirectMethodSource(Mismatch).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(13, CountOccurrences(Dump, TEXT("// Case: LANG-PROP-FORK-SEMANTICS-"))));
		ASSERT_THAT(AreEqual(12, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/PropForkSemanticsGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("PropForkSemanticsGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
