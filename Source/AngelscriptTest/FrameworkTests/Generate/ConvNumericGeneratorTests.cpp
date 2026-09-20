#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestConvNumericGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include <cstdlib>

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	ConvNumericGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FTypeAxis
	{
		const TCHAR* Token = nullptr;
		const TCHAR* ScriptType = nullptr;
		const TCHAR* Suffix = nullptr;
		uint8 Width = 1;
		bool bSigned = true;
		bool bFloating = false;
	};

	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString FormNeedle;
		FString SourceNeedle;
		FString ExpectedNeedle;
		bool bLimited = false;
	};

	static const FTypeAxis* Types()
	{
		static const FTypeAxis kTypes[] = {
			{TEXT("INT8"), TEXT("int8"), TEXT("Int8"), 1, true, false},
			{TEXT("INT16"), TEXT("int16"), TEXT("Int16"), 2, true, false},
			{TEXT("INT"), TEXT("int"), TEXT("Int"), 4, true, false},
			{TEXT("INT64"), TEXT("int64"), TEXT("Int64"), 8, true, false},
			{TEXT("UINT8"), TEXT("uint8"), TEXT("Uint8"), 1, false, false},
			{TEXT("UINT16"), TEXT("uint16"), TEXT("Uint16"), 2, false, false},
			{TEXT("UINT"), TEXT("uint"), TEXT("Uint"), 4, false, false},
			{TEXT("UINT64"), TEXT("uint64"), TEXT("Uint64"), 8, false, false},
			{TEXT("FLOAT32"), TEXT("float"), TEXT("Float"), 4, true, true},
			{TEXT("FLOAT64"), TEXT("double"), TEXT("Double"), 8, true, true},
		};
		return kTypes;
	}

	static const TCHAR* IndependentLiteral(const FTypeAxis& Type, const TCHAR* Value)
	{
		if (FCString::Strcmp(Value, TEXT("ZERO")) == 0)
		{
			return Type.bFloating ? (Type.Width == 4 ? TEXT("0.0f") : TEXT("0.0")) : TEXT("0");
		}
		if (FCString::Strcmp(Value, TEXT("ONE")) == 0)
		{
			return Type.bFloating ? (Type.Width == 4 ? TEXT("1.0f") : TEXT("1.0")) : TEXT("1");
		}
		if (FCString::Strcmp(Value, TEXT("NEGATIVE")) == 0)
		{
			if (Type.bFloating || Type.bSigned)
			{
				return TEXT("-1");
			}
			if (Type.Width == 1)
			{
				return TEXT("255");
			}
			if (Type.Width == 2)
			{
				return TEXT("65535");
			}
			if (Type.Width == 4)
			{
				return TEXT("4294967295");
			}
			return TEXT("18446744073709551615");
		}
		if (FCString::Strcmp(Value, TEXT("FRACTIONAL")) == 0)
		{
			return Type.bFloating ? (Type.Width == 4 ? TEXT("1.5f") : TEXT("1.5")) : TEXT("1");
		}
		if (FCString::Strcmp(Value, TEXT("MIN")) == 0)
		{
			if (!Type.bFloating && !Type.bSigned)
			{
				return TEXT("0");
			}
			if (Type.bFloating)
			{
				return Type.Width == 4 ? TEXT("-3.402823466e+38f") : TEXT("-1.7976931348623157e+308");
			}
			if (Type.Width == 1)
			{
				return TEXT("-128");
			}
			if (Type.Width == 2)
			{
				return TEXT("-32768");
			}
			if (Type.Width == 4)
			{
				return TEXT("-2147483647 - 1");
			}
			return TEXT("-9223372036854775807 - 1");
		}
		if (FCString::Strcmp(Value, TEXT("NEAR_BOUNDARY")) == 0)
		{
			if (Type.bFloating)
			{
				return Type.Width == 4 ? TEXT("3.402823263e+38f") : TEXT("1.7976931348623155e+308");
			}
			if (!Type.bSigned)
			{
				if (Type.Width == 1)
				{
					return TEXT("254");
				}
				if (Type.Width == 2)
				{
					return TEXT("65534");
				}
				if (Type.Width == 4)
				{
					return TEXT("4294967294");
				}
				return TEXT("18446744073709551614");
			}
			if (Type.Width == 1)
			{
				return TEXT("126");
			}
			if (Type.Width == 2)
			{
				return TEXT("32766");
			}
			if (Type.Width == 4)
			{
				return TEXT("2147483646");
			}
			return TEXT("9223372036854775806");
		}
		if (Type.bFloating)
		{
			return Type.Width == 4 ? TEXT("3.402823466e+38f") : TEXT("1.7976931348623157e+308");
		}
		if (!Type.bSigned)
		{
			if (Type.Width == 1)
			{
				return TEXT("255");
			}
			if (Type.Width == 2)
			{
				return TEXT("65535");
			}
			if (Type.Width == 4)
			{
				return TEXT("4294967295");
			}
			return TEXT("18446744073709551615");
		}
		if (Type.Width == 1)
		{
			return TEXT("127");
		}
		if (Type.Width == 2)
		{
			return TEXT("32767");
		}
		if (Type.Width == 4)
		{
			return TEXT("2147483647");
		}
		return TEXT("9223372036854775807");
	}

	static bool IndependentLimited(const FTypeAxis& Source, const FTypeAxis& Target, const TCHAR* Value)
	{
		if (!Source.bFloating || Target.bFloating)
		{
			return false;
		}
		if (FCString::Strcmp(Value, TEXT("ZERO")) == 0
			|| FCString::Strcmp(Value, TEXT("ONE")) == 0
			|| FCString::Strcmp(Value, TEXT("FRACTIONAL")) == 0)
		{
			return false;
		}
		return !(FCString::Strcmp(Value, TEXT("NEGATIVE")) == 0 && Target.bSigned);
	}

	static const ANSICHAR* IndependentLiteralAnsi(const FTypeAxis& Type, const TCHAR* Value)
	{
		if (FCString::Strcmp(Value, TEXT("ZERO")) == 0)
		{
			return Type.bFloating ? (Type.Width == 4 ? "0.0f" : "0.0") : "0";
		}
		if (FCString::Strcmp(Value, TEXT("ONE")) == 0)
		{
			return Type.bFloating ? (Type.Width == 4 ? "1.0f" : "1.0") : "1";
		}
		if (FCString::Strcmp(Value, TEXT("NEGATIVE")) == 0)
		{
			if (Type.bFloating || Type.bSigned)
			{
				return "-1";
			}
			return Type.Width == 1 ? "255" : Type.Width == 2 ? "65535" : Type.Width == 4 ? "4294967295" : "18446744073709551615";
		}
		if (FCString::Strcmp(Value, TEXT("FRACTIONAL")) == 0)
		{
			return Type.bFloating ? (Type.Width == 4 ? "1.5f" : "1.5") : "1";
		}
		if (FCString::Strcmp(Value, TEXT("MIN")) == 0)
		{
			if (Type.bFloating)
			{
				return Type.Width == 4 ? "-3.402823466e+38f" : "-1.7976931348623157e+308";
			}
			if (!Type.bSigned)
			{
				return "0";
			}
			if (Type.Width == 1)
			{
				return "-128";
			}
			if (Type.Width == 2)
			{
				return "-32768";
			}
			if (Type.Width == 4)
			{
				return "-2147483647 - 1";
			}
			return "-9223372036854775807 - 1";
		}
		if (FCString::Strcmp(Value, TEXT("NEAR_BOUNDARY")) == 0)
		{
			if (Type.bFloating)
			{
				return Type.Width == 4 ? "3.402823263e+38f" : "1.7976931348623155e+308";
			}
			if (!Type.bSigned)
			{
				return Type.Width == 1 ? "254" : Type.Width == 2 ? "65534" : Type.Width == 4 ? "4294967294" : "18446744073709551614";
			}
			return Type.Width == 1 ? "126" : Type.Width == 2 ? "32766" : Type.Width == 4 ? "2147483646" : "9223372036854775806";
		}
		if (Type.bFloating)
		{
			return Type.Width == 4 ? "3.402823466e+38f" : "1.7976931348623157e+308";
		}
		if (!Type.bSigned)
		{
			return Type.Width == 1 ? "255" : Type.Width == 2 ? "65535" : Type.Width == 4 ? "4294967295" : "18446744073709551615";
		}
		return Type.Width == 1 ? "127" : Type.Width == 2 ? "32767" : Type.Width == 4 ? "2147483647" : "9223372036854775807";
	}

	static int64 IndependentSigned(const FTypeAxis& Type, const TCHAR* Value)
	{
		const uint32 BitCount = static_cast<uint32>(Type.Width) * 8u;
		if (FCString::Strcmp(Value, TEXT("ZERO")) == 0)
		{
			return 0;
		}
		if (FCString::Strcmp(Value, TEXT("ONE")) == 0 || FCString::Strcmp(Value, TEXT("FRACTIONAL")) == 0)
		{
			return 1;
		}
		if (FCString::Strcmp(Value, TEXT("NEGATIVE")) == 0)
		{
			return -1;
		}
		if (FCString::Strcmp(Value, TEXT("MIN")) == 0)
		{
			return BitCount == 64u ? MIN_int64 : -(int64(1) << (BitCount - 1u));
		}
		const int64 Maximum = BitCount == 64u ? MAX_int64 : ((int64(1) << (BitCount - 1u)) - 1);
		return FCString::Strcmp(Value, TEXT("NEAR_BOUNDARY")) == 0 ? Maximum - 1 : Maximum;
	}

	static uint64 IndependentUnsigned(const FTypeAxis& Type, const TCHAR* Value)
	{
		if (FCString::Strcmp(Value, TEXT("ZERO")) == 0 || FCString::Strcmp(Value, TEXT("MIN")) == 0)
		{
			return 0;
		}
		if (FCString::Strcmp(Value, TEXT("ONE")) == 0 || FCString::Strcmp(Value, TEXT("FRACTIONAL")) == 0)
		{
			return 1;
		}
		const uint32 BitCount = static_cast<uint32>(Type.Width) * 8u;
		const uint64 Maximum = BitCount == 64u ? MAX_uint64 : ((uint64(1) << BitCount) - 1u);
		return FCString::Strcmp(Value, TEXT("NEAR_BOUNDARY")) == 0 ? Maximum - 1 : Maximum;
	}

	static FString IndependentExpectedLiteral(const FTypeAxis& Source, const FTypeAxis& Target, const TCHAR* Value)
	{
		if (Target.bFloating)
		{
			const double Raw = Source.bFloating
				? (Source.Width == 4
					? static_cast<double>(static_cast<float>(std::strtod(IndependentLiteralAnsi(Source, Value), nullptr)))
					: std::strtod(IndependentLiteralAnsi(Source, Value), nullptr))
				: Source.bSigned
					? static_cast<double>(IndependentSigned(Source, Value))
					: static_cast<double>(IndependentUnsigned(Source, Value));
			if (Target.Width == 4)
			{
				return FString::Printf(TEXT("%.9gf"), static_cast<double>(static_cast<float>(Raw)));
			}
			return FString::Printf(TEXT("%.17g"), Raw);
		}
		if (IndependentLimited(Source, Target, Value))
		{
			return FString();
		}
		uint64 Bits = 0;
		if (Source.bFloating)
		{
			const double Parsed = Source.Width == 4
				? static_cast<double>(static_cast<float>(std::strtod(IndependentLiteralAnsi(Source, Value), nullptr)))
				: std::strtod(IndependentLiteralAnsi(Source, Value), nullptr);
			Bits = Target.bSigned ? static_cast<uint64>(static_cast<int64>(Parsed)) : static_cast<uint64>(Parsed);
		}
		else if (Source.bSigned)
		{
			Bits = static_cast<uint64>(IndependentSigned(Source, Value));
		}
		else
		{
			Bits = IndependentUnsigned(Source, Value);
		}
		const uint32 BitCount = static_cast<uint32>(Target.Width) * 8u;
		const uint64 Mask = BitCount == 64u ? MAX_uint64 : ((uint64(1) << BitCount) - 1u);
		Bits &= Mask;
		if (Target.bSigned)
		{
			const uint64 SignBit = BitCount == 64u ? (uint64(1) << 63u) : (uint64(1) << (BitCount - 1u));
			const int64 Expected = (Bits & SignBit) != 0
				? static_cast<int64>(Bits | ~Mask)
				: static_cast<int64>(Bits);
			return FString::Printf(TEXT("%lld"), static_cast<long long>(Expected));
		}
		return FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(Bits));
	}

	static FString IndependentFormNeedle(const TCHAR* Form, const FTypeAxis& Target)
	{
		if (FCString::Strcmp(Form, TEXT("ASSIGNMENT")) == 0)
		{
			return TEXT("Converted = SourceValue;");
		}
		if (FCString::Strcmp(Form, TEXT("INITIALIZER")) == 0)
		{
			return FString::Printf(TEXT("%s Converted = SourceValue;"), Target.ScriptType);
		}
		if (FCString::Strcmp(Form, TEXT("ARGUMENT")) == 0)
		{
			return FString::Printf(TEXT("Converted = PassNumeric%s(SourceValue);"), Target.Suffix);
		}
		if (FCString::Strcmp(Form, TEXT("RETURN")) == 0)
		{
			return FString::Printf(TEXT("Converted = ReturnNumeric%s(SourceValue);"), Target.Suffix);
		}
		if (FCString::Strcmp(Form, TEXT("PROMOTION")) == 0)
		{
			return FString::Printf(TEXT("Converted = SourceValue + %s(0);"), Target.ScriptType);
		}
		return FString::Printf(TEXT("Converted = %s(SourceValue);"), Target.ScriptType);
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kForms[] = {
			TEXT("ASSIGNMENT"), TEXT("INITIALIZER"), TEXT("ARGUMENT"),
			TEXT("RETURN"), TEXT("PROMOTION"), TEXT("EXPLICIT_CAST")};
		static const TCHAR* const kValues[] = {
			TEXT("ZERO"), TEXT("ONE"), TEXT("NEGATIVE"), TEXT("MIN"),
			TEXT("MAX"), TEXT("NEAR_BOUNDARY"), TEXT("FRACTIONAL")};
		const FTypeAxis* const kTypes = Types();
		TArray<FExpectedCell> Cells;
		Cells.SetNum(4200);
		for (int32 Index = 0; Index < 4200; ++Index)
		{
			const int32 SourceIndex = Index / 420;
			const int32 TargetIndex = (Index / 42) % 10;
			const int32 FormIndex = (Index / 7) % 6;
			const int32 ValueIndex = Index % 7;
			const FTypeAxis& Source = kTypes[SourceIndex];
			const FTypeAxis& Target = kTypes[TargetIndex];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-CONV-NUMERIC-%s-%s-%s-%s"),
				Source.Token,
				Target.Token,
				kForms[FormIndex],
				kValues[ValueIndex]);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.FormNeedle = IndependentFormNeedle(kForms[FormIndex], Target);
			Cell.SourceNeedle = FString::Printf(
				TEXT("%s SourceValue = %s;"),
				Source.ScriptType,
				IndependentLiteral(Source, kValues[ValueIndex]));
			Cell.bLimited = IndependentLimited(Source, Target, kValues[ValueIndex]);
			Cell.ExpectedNeedle = Cell.bLimited
				? FString()
				: IndependentExpectedLiteral(Source, Target, kValues[ValueIndex]);
		}
		return Cells;
	}

	static bool HasExtractableEntry(const FString& Source, const FString& EntryName)
	{
		const FString Header = FString::Printf(TEXT("int %s()"), *EntryName);
		return Source.StartsWith(Header) || Source.Contains(TEXT("\n") + Header);
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EConvNumericForm;
		using AngelscriptTest::Generate::EConvNumericType;
		using AngelscriptTest::Generate::EConvNumericValue;
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::FConvNumericGenerator;
		using AngelscriptTest::Generate::FConvNumericParams;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FConvNumericGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-CONV-NUMERIC")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangConvNumericInt8Int8AssignmentZero")),
			MakeCanonicalEntryName(TEXT("LANG-CONV-NUMERIC-INT8-INT8-ASSIGNMENT-ZERO"))));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile = AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("ConvNumericGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("ConvNumericGenerator dump path=%s cases=4200 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(4200, CaseCount));
		ASSERT_THAT(AreEqual(4200, Cases.Num()));
		ASSERT_THAT(AreEqual(4200, ExpectedCells.Num()));

		int32 LimitedCount = 0;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(1, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(1, Generator.GetExpected(Expected.CaseId)));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedException.IsEmpty()));
			ASSERT_THAT(IsTrue(Cases[Index].bLimitedObservation == Expected.bLimited));
			if (Expected.bLimited)
			{
				++LimitedCount;
				ASSERT_THAT(IsTrue(Cases[Index].ExecutionNotes.Contains(TEXT("not portable"))));
			}
		}
		ASSERT_THAT(IsTrue(LimitedCount > 0));

		const TCHAR* const ProbeCaseId = TEXT("LANG-CONV-NUMERIC-INT8-INT8-ASSIGNMENT-ZERO");
		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		const FString OwnedAgain = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(IsTrue(OwnedCase.Len() > 0));
		ASSERT_THAT(AreEqual(OwnedCase, OwnedAgain));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangConvNumericInt8Int8AssignmentZero()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangConvNumericInt8Int8AssignmentZero()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int8 PassNumericInt8(int8 Value)"))));

		FConvNumericParams DefaultParams;
		const FString DefaultSingle = Generator.BuildNumericConversionSource(DefaultParams);
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsFalse(DefaultSingle.Contains(TEXT("int EntryLang"))));

		FConvNumericParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		const FString ProbeSource = Generator.BuildNumericConversionSource(ProbeParams);
		ASSERT_THAT(IsTrue(ProbeSource.Contains(TEXT("int ProbeEntry()"))));
		ASSERT_THAT(IsFalse(ProbeSource.Contains(TEXT("int Entry()"))));

		const FString Representative = LoadGold(TEXT("LanguageGenerators/ConvNumericGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.SourceNeedle)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.FormNeedle)));
			if (Expected.bLimited)
			{
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("return 1;"))));
			}
			else
			{
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.ExpectedNeedle)));
				ASSERT_THAT(IsTrue(GeneratedBody.Contains(TEXT("return Converted == ExpectedValue ? 1 : 0;"))));
			}
			if (HasExtractableEntry(Representative, Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FConvNumericParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildNumericConversionSource(InvalidName).IsEmpty()));
		FConvNumericParams InvalidEnum = DefaultParams;
		InvalidEnum.Source = static_cast<EConvNumericType>(10);
		ASSERT_THAT(IsTrue(Generator.BuildNumericConversionSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("LANG-CF-CONDITION-IF-BOOL_LITERAL-TRUE")).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));
		ASSERT_THAT(AreEqual(1, Generator.GetExpected(ProbeCaseId)));
		bool bFoundProbe = false;
		for (const FGeneratedCaseInfo& Info : Cases)
		{
			if (Info.CaseId == ProbeCaseId)
			{
				bFoundProbe = true;
				ASSERT_THAT(IsTrue(Info.ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(1, Info.ExpectedReturn.Get(MIN_int32)));
			}
		}
		ASSERT_THAT(IsTrue(bFoundProbe));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(1, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(IsTrue(OutputFile.EndsWith(TEXT("GeneratedCases/ConvNumericGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\ConvNumericGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(4200, CountOccurrences(Dump, TEXT("// Case: LANG-CONV-NUMERIC-"))));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			ASSERT_THAT(IsTrue(Dump.Contains(FString::Printf(TEXT("// Case: %s"), *Expected.CaseId))));
			FString DumpBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Dump, Expected.EntryName, DumpBody)));
			FString AggregateBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, AggregateBody)));
			ASSERT_THAT(AreEqual(AggregateBody, DumpBody));
		}
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		ASSERT_THAT(IsTrue(AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("")).IsEmpty()));
		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/ConvNumericGenerator.as"), Dump, ProbeError)));
		ASSERT_THAT(IsTrue(ProbeError.Len() > 0));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("ConvNumericGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
