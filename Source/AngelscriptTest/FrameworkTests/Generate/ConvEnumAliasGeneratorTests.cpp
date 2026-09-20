#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestConvEnumAliasGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ConvEnumAliasGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString FormNeedle;
		FString LiteralNeedle;
		bool bReject = false;
	};

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-CONV-ENUM-ALIAS-ENUM-ENUM-ASSIGNMENT-ZERO");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CONV-BOOL-CONTEXT-BOOL-IF-ZERO");

	static bool IndependentReject(const TCHAR* Source, const TCHAR* Target, const TCHAR* Form)
	{
		const bool bAlias = FString(Source).StartsWith(TEXT("ALIAS_"));
		if (bAlias && FCString::Strcmp(Target, TEXT("ENUM")) == 0
			&& FCString::Strcmp(Form, TEXT("EXPLICIT_CAST")) != 0)
		{
			return true;
		}
		return FCString::Strcmp(Source, TEXT("ENUM")) == 0
			&& FCString::Strcmp(Target, TEXT("ENUM")) == 0
			&& FCString::Strcmp(Form, TEXT("PROMOTION")) == 0;
	}

	static int64 IndependentDeclared(const TCHAR* Value)
	{
		if (FCString::Strcmp(Value, TEXT("ZERO")) == 0)
		{
			return 0;
		}
		if (FCString::Strcmp(Value, TEXT("ONE")) == 0)
		{
			return 1;
		}
		if (FCString::Strcmp(Value, TEXT("NEGATIVE")) == 0)
		{
			return -1;
		}
		return FCString::Strcmp(Value, TEXT("NEAR_MIN")) == 0 ? -126 : 126;
	}

	static int64 IndependentSourceValue(const TCHAR* Source, const TCHAR* Value)
	{
		const int64 Declared = IndependentDeclared(Value);
		if (FCString::Strcmp(Source, TEXT("ALIAS_UINT")) == 0)
		{
			return static_cast<int64>(static_cast<uint32>(Declared));
		}
		if (FCString::Strcmp(Source, TEXT("ALIAS_UINT64")) == 0)
		{
			return static_cast<int64>(static_cast<uint64>(Declared));
		}
		return Declared;
	}

	static uint64 IndependentNormalizeUnsigned(const int64 Value, const uint8 Width)
	{
		const uint64 Mask = Width == 8 ? MAX_uint64 : ((uint64(1) << (Width * 8)) - 1);
		return static_cast<uint64>(Value) & Mask;
	}

	static int64 IndependentNormalizeSigned(const int64 Value, const uint8 Width)
	{
		const uint64 UnsignedValue = IndependentNormalizeUnsigned(Value, Width);
		if (Width == 8)
		{
			return static_cast<int64>(UnsignedValue);
		}
		const uint64 SignBit = uint64(1) << (Width * 8 - 1);
		if ((UnsignedValue & SignBit) == 0)
		{
			return static_cast<int64>(UnsignedValue);
		}
		return static_cast<int64>(UnsignedValue | ~((uint64(1) << (Width * 8)) - 1));
	}

	static FString IndependentLiteral(const TCHAR* Source, const TCHAR* Target, const TCHAR* Value)
	{
		if (FCString::Strcmp(Target, TEXT("ENUM")) == 0)
		{
			if (FCString::Strcmp(Value, TEXT("ZERO")) == 0)
			{
				return TEXT("EConversionEnum::Zero");
			}
			if (FCString::Strcmp(Value, TEXT("ONE")) == 0)
			{
				return TEXT("EConversionEnum::One");
			}
			if (FCString::Strcmp(Value, TEXT("NEGATIVE")) == 0)
			{
				return TEXT("EConversionEnum::Negative");
			}
			return FCString::Strcmp(Value, TEXT("NEAR_MIN")) == 0
				? TEXT("EConversionEnum::NearMinimum")
				: TEXT("EConversionEnum::NearMaximum");
		}
		const int64 SourceValue = IndependentSourceValue(Source, Value);
		if (FCString::Strcmp(Target, TEXT("FLOAT64")) == 0)
		{
			const double Floating = FCString::Strcmp(Source, TEXT("ALIAS_UINT64")) == 0
				? static_cast<double>(static_cast<uint64>(SourceValue))
				: static_cast<double>(SourceValue);
			return FString::Printf(TEXT("%.17g"), Floating);
		}
		if (FCString::Strcmp(Target, TEXT("UINT")) == 0)
		{
			return LexToString(IndependentNormalizeUnsigned(SourceValue, 4));
		}
		if (FCString::Strcmp(Target, TEXT("UINT64")) == 0)
		{
			return LexToString(IndependentNormalizeUnsigned(SourceValue, 8));
		}
		if (FCString::Strcmp(Target, TEXT("INT8")) == 0)
		{
			return LexToString(IndependentNormalizeSigned(SourceValue, 1));
		}
		if (FCString::Strcmp(Target, TEXT("INT64")) == 0)
		{
			return LexToString(IndependentNormalizeSigned(SourceValue, 8));
		}
		return LexToString(IndependentNormalizeSigned(SourceValue, 4));
	}

	static FString IndependentFormNeedle(const TCHAR* Form, const TCHAR* Target)
	{
		if (FCString::Strcmp(Form, TEXT("ASSIGNMENT")) == 0)
		{
			return TEXT("Converted = SourceValue;");
		}
		if (FCString::Strcmp(Form, TEXT("INITIALIZER")) == 0)
		{
			return TEXT("Converted = SourceValue;");
		}
		if (FCString::Strcmp(Form, TEXT("ARGUMENT")) == 0)
		{
			return TEXT("PassEnumAliasTarget");
		}
		if (FCString::Strcmp(Form, TEXT("RETURN")) == 0)
		{
			return TEXT("ReturnEnumAliasTarget");
		}
		if (FCString::Strcmp(Form, TEXT("PROMOTION")) == 0)
		{
			return FString::Printf(TEXT("SourceValue + %s(0)"),
				FCString::Strcmp(Target, TEXT("ENUM")) == 0 ? TEXT("EConversionEnum")
				: FCString::Strcmp(Target, TEXT("FLOAT64")) == 0 ? TEXT("float")
				: FCString::Strcmp(Target, TEXT("INT8")) == 0 ? TEXT("int8")
				: FCString::Strcmp(Target, TEXT("INT64")) == 0 ? TEXT("int64")
				: FCString::Strcmp(Target, TEXT("UINT")) == 0 ? TEXT("uint")
				: FCString::Strcmp(Target, TEXT("UINT64")) == 0 ? TEXT("uint64")
				: TEXT("int"));
		}
		return TEXT("(SourceValue);");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kSources[] = {
			TEXT("ENUM"), TEXT("ALIAS_INT8"), TEXT("ALIAS_INT"), TEXT("ALIAS_INT64"), TEXT("ALIAS_UINT"),
			TEXT("ALIAS_UINT64")};
		static const TCHAR* const kTargets[] = {
			TEXT("ENUM"), TEXT("INT8"), TEXT("INT"), TEXT("INT64"), TEXT("UINT"), TEXT("UINT64"), TEXT("FLOAT64")};
		static const TCHAR* const kForms[] = {
			TEXT("ASSIGNMENT"), TEXT("INITIALIZER"), TEXT("ARGUMENT"), TEXT("RETURN"), TEXT("PROMOTION"),
			TEXT("EXPLICIT_CAST")};
		static const TCHAR* const kValues[] = {
			TEXT("ZERO"), TEXT("ONE"), TEXT("NEGATIVE"), TEXT("NEAR_MIN"), TEXT("NEAR_MAX")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(1260);
		for (int32 Index = 0; Index < 1260; ++Index)
		{
			const TCHAR* const Source = kSources[Index / 210];
			const TCHAR* const Target = kTargets[(Index / 30) % 7];
			const TCHAR* const Form = kForms[(Index / 5) % 6];
			const TCHAR* const Value = kValues[Index % 5];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-CONV-ENUM-ALIAS-%s-%s-%s-%s"), Source, Target, Form, Value);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Source, Target, Form);
			Cell.FormNeedle = IndependentFormNeedle(Form, Target);
			Cell.LiteralNeedle = IndependentLiteral(Source, Target, Value);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EConvEnumAliasForm;
		using AngelscriptTest::Generate::EConvEnumAliasSource;
		using AngelscriptTest::Generate::EConvEnumAliasTarget;
		using AngelscriptTest::Generate::EConvEnumAliasValue;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FConvEnumAliasGenerator;
		using AngelscriptTest::Generate::FConvEnumAliasParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FConvEnumAliasGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CONV-ENUM-ALIAS")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangConvEnumAliasEnumEnumAssignmentZero")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ConvEnumAliasGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ConvEnumAliasGenerator dump path=%s cases=1260 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(1130, CaseCount));
		ASSERT_THAT(AreEqual(1260, Cases.Num()));
		ASSERT_THAT(AreEqual(130, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("enum EConversionEnum"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/ConvEnumAliasGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

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
				ASSERT_THAT(IsFalse(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(RejectIds.Contains(Expected.CaseId)));
				const FString RejectSource = Generator.BuildRejectSource(Expected.CaseId);
				ASSERT_THAT(IsTrue(RejectSource.Contains(Expected.FormNeedle)));
				ASSERT_THAT(AreEqual(RejectSource, Generator.BuildCaseSource(Expected.CaseId)));
				if (Representative.Contains(Expected.EntryName))
				{
					FString GeneratedBody;
					ASSERT_THAT(IsTrue(ExtractFunction(RejectSource, Expected.EntryName, GeneratedBody)));
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
			else
			{
				ASSERT_THAT(AreEqual(
					FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(1, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(1, Generator.GetExpected(Expected.CaseId)));
				FString GeneratedBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.FormNeedle)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.LiteralNeedle)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("ActualValue == ExpectedValue ? 1 : 0;"))));
				if (Representative.Contains(Expected.EntryName))
				{
					FString GoldBody;
					ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
					ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
				}
			}
		}
		ASSERT_THAT(AreEqual(130, RejectCount));

		FConvEnumAliasParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildEnumAliasConversionSource(DefaultParams).Contains(TEXT("int Entry()"))));
		DefaultParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildEnumAliasConversionSource(DefaultParams).Contains(TEXT("int ProbeEntry()"))));
		DefaultParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildEnumAliasConversionSource(DefaultParams).IsEmpty()));
		FConvEnumAliasParams RejectParams;
		RejectParams.Source = EConvEnumAliasSource::AliasInt8;
		ASSERT_THAT(IsTrue(Generator.BuildEnumAliasConversionSource(RejectParams).IsEmpty()));
		FConvEnumAliasParams InvalidEnum = DefaultParams;
		InvalidEnum.FunctionName.Reset();
		InvalidEnum.Form = static_cast<EConvEnumAliasForm>(6);
		ASSERT_THAT(IsTrue(Generator.BuildEnumAliasConversionSource(InvalidEnum).IsEmpty()));
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
		ASSERT_THAT(AreEqual(1260, CountOccurrences(Dump, TEXT("// Case: LANG-CONV-ENUM-ALIAS-"))));
		ASSERT_THAT(AreEqual(130, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/ConvEnumAliasGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ConvEnumAliasGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
