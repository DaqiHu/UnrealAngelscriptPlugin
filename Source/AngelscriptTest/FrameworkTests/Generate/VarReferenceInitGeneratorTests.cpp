#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestVarReferenceInitGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	VarReferenceInitGenerator,
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

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-VAR-REFERENCE-INIT-EXPLICIT_TYPE-CONSTRUCTED_LOCAL-SCRIPT_REFERENCE-IDENTITY");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static bool IndependentReject(const TCHAR* Declaration, const TCHAR* Source, const TCHAR* Use)
	{
		if (FCString::Strcmp(Source, TEXT("NULL")) == 0 && FCString::Strcmp(Declaration, TEXT("AUTO")) == 0)
		{
			return true;
		}
		return FCString::Strcmp(Declaration, TEXT("CONST_VIEW")) == 0
			&& FCString::Strcmp(Use, TEXT("MUTATION")) == 0;
	}

	static bool IndependentFault(const TCHAR* Declaration, const TCHAR* Source, const TCHAR* Use)
	{
		return FCString::Strcmp(Source, TEXT("NULL")) == 0
			&& FCString::Strcmp(Declaration, TEXT("EXPLICIT_TYPE")) == 0
			&& FCString::Strcmp(Use, TEXT("MUTATION")) == 0;
	}

	static FString IndependentNeedle(
		const TCHAR* Declaration,
		const TCHAR* Source,
		const TCHAR* Type,
		const TCHAR* Use,
		const FString& EntryName)
	{
		if (FCString::Strcmp(Source, TEXT("PARAMETER")) == 0)
		{
			return FString::Printf(TEXT("Exercise%s("), *EntryName);
		}
		if (FCString::Strcmp(Source, TEXT("FIELD")) == 0)
		{
			return FCString::Strcmp(Type, TEXT("SCRIPT_REFERENCE")) == 0
				? TEXT("FVariableReferenceHolderScript Holder")
				: TEXT("FVariableReferenceHolderNative Holder");
		}
		if (FCString::Strcmp(Source, TEXT("FUNCTION_RETURN")) == 0)
		{
			return TEXT("PassVariableReference(Original)");
		}
		if (FCString::Strcmp(Declaration, TEXT("AUTO")) == 0)
		{
			return TEXT("auto Variable =");
		}
		if (FCString::Strcmp(Declaration, TEXT("CONST_VIEW")) == 0)
		{
			return FCString::Strcmp(Type, TEXT("SCRIPT_REFERENCE")) == 0
				? TEXT("const FScriptCaseReference Variable")
				: TEXT("const FNativeCaseReference Variable");
		}
		if (FCString::Strcmp(Use, TEXT("MUTATION")) == 0)
		{
			return TEXT("Variable.Value += 5;");
		}
		if (FCString::Strcmp(Use, TEXT("ARGUMENT")) == 0)
		{
			return TEXT("ObserveVariableReference(Variable, Original)");
		}
		if (FCString::Strcmp(Use, TEXT("RETURN")) == 0)
		{
			return TEXT("Returned == Original");
		}
		if (FCString::Strcmp(Use, TEXT("NULL_COMPARE")) == 0)
		{
			return TEXT("Variable == nullptr");
		}
		if (FCString::Strcmp(Source, TEXT("NULL")) == 0)
		{
			return TEXT("Original = nullptr;");
		}
		return TEXT("Variable == Original ? 1 : -1");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kDeclarations[] = {TEXT("EXPLICIT_TYPE"), TEXT("AUTO"), TEXT("CONST_VIEW")};
		static const TCHAR* const kSources[] = {
			TEXT("CONSTRUCTED_LOCAL"), TEXT("PARAMETER"), TEXT("FUNCTION_RETURN"), TEXT("FIELD"), TEXT("NULL")};
		static const TCHAR* const kTypes[] = {TEXT("SCRIPT_REFERENCE"), TEXT("NATIVE_REFERENCE")};
		static const TCHAR* const kUses[] = {
			TEXT("IDENTITY"), TEXT("MUTATION"), TEXT("ARGUMENT"), TEXT("RETURN"), TEXT("NULL_COMPARE")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(150);
		for (int32 Index = 0; Index < 150; ++Index)
		{
			const TCHAR* const Declaration = kDeclarations[Index / 50];
			const TCHAR* const Source = kSources[(Index / 10) % 5];
			const TCHAR* const Type = kTypes[(Index / 5) % 2];
			const TCHAR* const Use = kUses[Index % 5];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-VAR-REFERENCE-INIT-%s-%s-%s-%s"),
				Declaration,
				Source,
				Type,
				Use);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Declaration, Source, Use);
			Cell.bFault = IndependentFault(Declaration, Source, Use);
			Cell.Expected = Cell.bReject || Cell.bFault ? 0 : 1;
			Cell.Needle = IndependentNeedle(Declaration, Source, Type, Use, Cell.EntryName);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EVarRefDecl;
		using AngelscriptTest::Generate::EVarRefSource;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FVarReferenceInitGenerator;
		using AngelscriptTest::Generate::FVarReferenceInitParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FVarReferenceInitGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-VAR-REFERENCE-INIT")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangVarReferenceInitExplicitTypeConstructedLocalScriptReferenceIdentity")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("VarReferenceInitGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(
			LogTemp,
			Display,
			TEXT("VarReferenceInitGenerator dump path=%s cases=150 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(130, CaseCount));
		ASSERT_THAT(AreEqual(150, Cases.Num()));
		ASSERT_THAT(AreEqual(20, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/VarReferenceInitGenerator/RepresentativeSources.as"));
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
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
				ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.Needle)));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(RejectSource, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
				FString GoldBody;
				if (ExtractFunction(Representative, Expected.EntryName, GoldBody))
				{
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
			else if (Expected.bFault)
			{
				++FaultCount;
				ASSERT_THAT(AreEqual(
					FString::Printf(TEXT("int %s()"), *Expected.EntryName),
					Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::RuntimeException));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
				ASSERT_THAT(AreEqual(FString(TEXT("Null pointer access")), Cases[Index].ExpectedException));
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
				FString GoldBody;
				if (ExtractFunction(Representative, Expected.EntryName, GoldBody))
				{
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
			else
			{
				ASSERT_THAT(AreEqual(
					FString::Printf(TEXT("int %s()"), *Expected.EntryName),
					Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(1, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(1, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.Needle)));
				FString GoldBody;
				if (ExtractFunction(Representative, Expected.EntryName, GoldBody))
				{
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
		}
		ASSERT_THAT(AreEqual(20, RejectCount));
		ASSERT_THAT(AreEqual(2, FaultCount));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("class FVariableReferenceHolderScript\n"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));

		FVarReferenceInitParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildReferenceInitializationSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FVarReferenceInitParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildReferenceInitializationSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildReferenceInitializationSource(ProbeParams).IsEmpty()));
		FVarReferenceInitParams InvalidEnum = DefaultParams;
		InvalidEnum.Declaration = static_cast<EVarRefDecl>(3);
		ASSERT_THAT(IsTrue(Generator.BuildReferenceInitializationSource(InvalidEnum).IsEmpty()));
		FVarReferenceInitParams RejectParams = DefaultParams;
		RejectParams.Source = EVarRefSource::Null;
		RejectParams.Declaration = EVarRefDecl::Auto;
		ASSERT_THAT(IsTrue(Generator.BuildReferenceInitializationSource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));
		ASSERT_THAT(IsTrue(AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("")).IsEmpty()));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(AreEqual(1, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(AreEqual(150, CountOccurrences(Dump, TEXT("// Case: LANG-VAR-REFERENCE-INIT-"))));
		ASSERT_THAT(AreEqual(20, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(IsTrue(
			OutputFile.EndsWith(TEXT("GeneratedCases/VarReferenceInitGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\VarReferenceInitGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/VarReferenceInitGenerator.as"),
			Dump,
			ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("VarReferenceInitGenerator.as")),
			Dump,
			FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
