#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestOpIntegralBitwiseGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	OpIntegralBitwiseGenerator,
	"Angelscript.UnitTest.Framework.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FExpectedCell
	{
		FString CaseId;
		FString EntryName;
		FString OperatorNeedle;
		FString TypeName;
		int32 Expected = 0;
	};

	static constexpr const TCHAR* ProbeCaseId =
		TEXT("LANG-OP-INTEGRAL-BITWISE-MUTABLE_LVALUE-BIT_AND-ZERO-INT");
	static constexpr const TCHAR* ZeroCaseId =
		TEXT("LANG-OP-INTEGRAL-BITWISE-MUTABLE_LVALUE-BIT_AND-ZERO-INT");
	static constexpr const TCHAR* ForeignCaseId =
		TEXT("LANG-OP-INCREMENT-LOCAL-BEFORE-PRE_INCREMENT-INT");

	static int32 IndependentWidth(const TCHAR* Type)
	{
		if (FCString::Strcmp(Type, TEXT("INT8")) == 0 || FCString::Strcmp(Type, TEXT("UINT8")) == 0)
		{
			return 8;
		}
		if (FCString::Strcmp(Type, TEXT("INT16")) == 0 || FCString::Strcmp(Type, TEXT("UINT16")) == 0)
		{
			return 16;
		}
		if (FCString::Strcmp(Type, TEXT("INT64")) == 0 || FCString::Strcmp(Type, TEXT("UINT64")) == 0)
		{
			return 64;
		}
		return 32;
	}

	static bool IndependentSigned(const TCHAR* Type)
	{
		return FCString::Strcmp(Type, TEXT("INT8")) == 0
			|| FCString::Strcmp(Type, TEXT("INT16")) == 0
			|| FCString::Strcmp(Type, TEXT("INT")) == 0
			|| FCString::Strcmp(Type, TEXT("INT64")) == 0;
	}

	static uint64 IndependentMask(const int32 Width)
	{
		return Width == 64 ? MAX_uint64 : (uint64{1} << Width) - 1;
	}

	static uint64 IndependentSourceBits(const TCHAR* Type)
	{
		switch (IndependentWidth(Type))
		{
		case 8:
			return 0xA5ull;
		case 16:
			return 0xA55Aull;
		case 32:
			return 0xA55AA55Aull;
		default:
			return 0xA55AA55AA55AA55Aull;
		}
	}

	static uint64 IndependentPromoted(const TCHAR* Type)
	{
		const int32 SourceWidth = IndependentWidth(Type);
		const int32 OutputWidth = SourceWidth == 64 ? 64 : 32;
		const uint64 Bits = IndependentSourceBits(Type) & IndependentMask(SourceWidth);
		if (!IndependentSigned(Type) || SourceWidth == OutputWidth
			|| (Bits & (uint64{1} << (SourceWidth - 1))) == 0)
		{
			return Bits & IndependentMask(OutputWidth);
		}
		return (Bits | ~IndependentMask(SourceWidth)) & IndependentMask(OutputWidth);
	}

	static int32 IndependentRight(const TCHAR* Type, const TCHAR* Right)
	{
		const int32 Width = IndependentWidth(Type);
		if (FCString::Strcmp(Right, TEXT("ONE")) == 0)
		{
			return 1;
		}
		if (FCString::Strcmp(Right, TEXT("SOURCE_WIDTH_MINUS_ONE")) == 0)
		{
			return Width - 1;
		}
		if (FCString::Strcmp(Right, TEXT("SOURCE_WIDTH")) == 0)
		{
			return Width;
		}
		if (FCString::Strcmp(Right, TEXT("SOURCE_WIDTH_PLUS_ONE")) == 0)
		{
			return Width + 1;
		}
		if (FCString::Strcmp(Right, TEXT("LARGE")) == 0)
		{
			return 100000;
		}
		if (FCString::Strcmp(Right, TEXT("NEGATIVE")) == 0)
		{
			return -1;
		}
		return 0;
	}

	static uint64 IndependentArithmeticShift(const uint64 Bits, const int32 Count, const int32 Width)
	{
		const int32 Effective = Count & (Width - 1);
		const uint64 Mask = IndependentMask(Width);
		const uint64 Value = Bits & Mask;
		if (Effective == 0)
		{
			return Value;
		}
		const uint64 Shifted = Value >> Effective;
		if ((Value & (uint64{1} << (Width - 1))) == 0)
		{
			return Shifted;
		}
		const uint64 Fill = Mask ^ ((uint64{1} << (Width - Effective)) - 1);
		return (Shifted | Fill) & Mask;
	}

	static int32 IndependentExpected(const TCHAR* Type, const TCHAR* Operator, const TCHAR* Right)
	{
		const int32 Width = IndependentWidth(Type) == 64 ? 64 : 32;
		const uint64 Mask = IndependentMask(Width);
		const uint64 Left = IndependentPromoted(Type);
		const int32 RightSigned = IndependentRight(Type, Right);
		const uint64 RightBits = static_cast<uint64>(static_cast<int64>(RightSigned)) & Mask;
		const int32 Count = RightSigned & (Width - 1);
		uint64 Bits = 0;
		if (FCString::Strcmp(Operator, TEXT("BIT_AND")) == 0)
		{
			Bits = Left & RightBits;
		}
		else if (FCString::Strcmp(Operator, TEXT("BIT_OR")) == 0)
		{
			Bits = Left | RightBits;
		}
		else if (FCString::Strcmp(Operator, TEXT("BIT_XOR")) == 0)
		{
			Bits = Left ^ RightBits;
		}
		else if (FCString::Strcmp(Operator, TEXT("SHIFT_LEFT")) == 0)
		{
			Bits = (Left << Count) & Mask;
		}
		else if (FCString::Strcmp(Operator, TEXT("SHIFT_RIGHT_LOGICAL")) == 0)
		{
			Bits = Left >> Count;
		}
		else
		{
			Bits = IndependentArithmeticShift(Left, Count, Width);
		}
		return static_cast<int32>(Bits & 0xFFFFFFFFull);
	}

	static FString IndependentOperatorNeedle(const TCHAR* Category, const TCHAR* Operator)
	{
		if (FCString::Strcmp(Category, TEXT("ALIAS")) == 0)
		{
			if (FCString::Strcmp(Operator, TEXT("BIT_OR")) == 0)
			{
				return TEXT("ApplyBitOrAlias_");
			}
			if (FCString::Strcmp(Operator, TEXT("BIT_XOR")) == 0)
			{
				return TEXT("ApplyBitXorAlias_");
			}
			if (FCString::Strcmp(Operator, TEXT("SHIFT_LEFT")) == 0)
			{
				return TEXT("ApplyShiftLeftAlias_");
			}
			if (FCString::Strcmp(Operator, TEXT("SHIFT_RIGHT_LOGICAL")) == 0)
			{
				return TEXT("ApplyShiftRightLogicalAlias_");
			}
			if (FCString::Strcmp(Operator, TEXT("SHIFT_RIGHT_ARITHMETIC")) == 0)
			{
				return TEXT("ApplyShiftRightArithmeticAlias_");
			}
			return TEXT("ApplyBitAndAlias_");
		}
		if (FCString::Strcmp(Operator, TEXT("BIT_OR")) == 0)
		{
			return TEXT("|");
		}
		if (FCString::Strcmp(Operator, TEXT("BIT_XOR")) == 0)
		{
			return TEXT("^");
		}
		if (FCString::Strcmp(Operator, TEXT("SHIFT_LEFT")) == 0)
		{
			return TEXT("<<");
		}
		if (FCString::Strcmp(Operator, TEXT("SHIFT_RIGHT_LOGICAL")) == 0)
		{
			return TEXT(">>");
		}
		if (FCString::Strcmp(Operator, TEXT("SHIFT_RIGHT_ARITHMETIC")) == 0)
		{
			return TEXT(">>>");
		}
		return TEXT("&");
	}

	static FString IndependentType(const TCHAR* Type)
	{
		if (FCString::Strcmp(Type, TEXT("INT8")) == 0)
		{
			return TEXT("int8");
		}
		if (FCString::Strcmp(Type, TEXT("INT16")) == 0)
		{
			return TEXT("int16");
		}
		if (FCString::Strcmp(Type, TEXT("INT64")) == 0)
		{
			return TEXT("int64");
		}
		if (FCString::Strcmp(Type, TEXT("UINT8")) == 0)
		{
			return TEXT("uint8");
		}
		if (FCString::Strcmp(Type, TEXT("UINT16")) == 0)
		{
			return TEXT("uint16");
		}
		if (FCString::Strcmp(Type, TEXT("UINT")) == 0)
		{
			return TEXT("uint");
		}
		if (FCString::Strcmp(Type, TEXT("UINT64")) == 0)
		{
			return TEXT("uint64");
		}
		return TEXT("int");
	}

	static bool HasExtractableEntry(const FString& Source, const FString& EntryName)
	{
		const FString Header = FString::Printf(TEXT("int %s()"), *EntryName);
		return Source.StartsWith(Header) || Source.Contains(TEXT("\n") + Header);
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kCategories[] = {
			TEXT("MUTABLE_LVALUE"), TEXT("CONST_LVALUE"), TEXT("TEMPORARY"), TEXT("FIELD"), TEXT("ALIAS")};
		static const TCHAR* const kOperators[] = {
			TEXT("BIT_AND"), TEXT("BIT_OR"), TEXT("BIT_XOR"),
			TEXT("SHIFT_LEFT"), TEXT("SHIFT_RIGHT_LOGICAL"), TEXT("SHIFT_RIGHT_ARITHMETIC")};
		static const TCHAR* const kRights[] = {
			TEXT("ZERO"), TEXT("ONE"), TEXT("SOURCE_WIDTH_MINUS_ONE"), TEXT("SOURCE_WIDTH"),
			TEXT("SOURCE_WIDTH_PLUS_ONE"), TEXT("LARGE"), TEXT("NEGATIVE")};
		static const TCHAR* const kTypes[] = {
			TEXT("INT8"), TEXT("INT16"), TEXT("INT"), TEXT("INT64"),
			TEXT("UINT8"), TEXT("UINT16"), TEXT("UINT"), TEXT("UINT64")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(1680);
		for (int32 Index = 0; Index < 1680; ++Index)
		{
			const TCHAR* const Category = kCategories[Index / 336];
			const TCHAR* const Operator = kOperators[(Index / 56) % 6];
			const TCHAR* const Right = kRights[(Index / 8) % 7];
			const TCHAR* const Type = kTypes[Index % 8];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(
				TEXT("LANG-OP-INTEGRAL-BITWISE-%s-%s-%s-%s"), Category, Operator, Right, Type);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.OperatorNeedle = IndependentOperatorNeedle(Category, Operator);
			Cell.TypeName = IndependentType(Type);
			Cell.Expected = IndependentExpected(Type, Operator, Right);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EOpIntegralBitwiseCategory;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FOpIntegralBitwiseGenerator;
		using AngelscriptTest::Generate::FOpIntegralBitwiseParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FOpIntegralBitwiseGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-OP-INTEGRAL-BITWISE")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangOpIntegralBitwiseMutableLvalueBitAndZeroInt")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("OpIntegralBitwiseGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(LogTemp, Display, TEXT("OpIntegralBitwiseGenerator dump path=%s cases=1680 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		ASSERT_THAT(AreEqual(1680, CaseCount));
		ASSERT_THAT(AreEqual(1680, Cases.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		bool bFoundZero = false;
		for (int32 Index = 0; Index < ExpectedCells.Num(); ++Index)
		{
			const FExpectedCell& Expected = ExpectedCells[Index];
			ASSERT_THAT(AreEqual(Expected.CaseId, Cases[Index].CaseId));
			ASSERT_THAT(AreEqual(FString::Printf(TEXT("int %s()"), *Expected.EntryName), Cases[Index].EntryDeclaration));
			ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
			ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
			ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
			if (Expected.CaseId == ZeroCaseId)
			{
				bFoundZero = true;
				ASSERT_THAT(AreEqual(0, Expected.Expected));
			}
		}
		ASSERT_THAT(IsTrue(bFoundZero));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("int ObserveBitwiseType(int Value)"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("struct FBitwiseFieldOwner_int\n{"))));

		FOpIntegralBitwiseParams DefaultParams;
		ASSERT_THAT(IsTrue(Generator.BuildBitwiseSource(DefaultParams).Contains(TEXT("int Entry()"))));
		FOpIntegralBitwiseParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildBitwiseSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/OpIntegralBitwiseGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));
		for (const FExpectedCell& Expected : ExpectedCells)
		{
			FString GeneratedBody;
			ASSERT_THAT(IsTrue(ExtractFunction(Aggregate, Expected.EntryName, GeneratedBody)));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.TypeName + TEXT(" Input ="))));
			ASSERT_THAT(IsTrue(GeneratedBody.Contains(Expected.OperatorNeedle)));
			if (HasExtractableEntry(Representative, Expected.EntryName))
			{
				FString GoldBody;
				ASSERT_THAT(IsTrue(ExtractFunction(Representative, Expected.EntryName, GoldBody)));
				ASSERT_THAT(AreEqual(GoldBody, GeneratedBody));
			}
		}

		FOpIntegralBitwiseParams InvalidName = ProbeParams;
		InvalidName.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildBitwiseSource(InvalidName).IsEmpty()));
		FOpIntegralBitwiseParams InvalidEnum = DefaultParams;
		InvalidEnum.Category = static_cast<EOpIntegralBitwiseCategory>(5);
		ASSERT_THAT(IsTrue(Generator.BuildBitwiseSource(InvalidEnum).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(ZeroCaseId)));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));

		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(1680, CountOccurrences(Dump, TEXT("// Case: LANG-OP-INTEGRAL-BITWISE-"))));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/OpIntegralBitwiseGenerator.as"), Dump, ProbeError)));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("OpIntegralBitwiseGenerator.as")), Dump, FileParentError)));
		FString SiblingRead;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(SiblingRead, *Sibling)));
		ASSERT_THAT(AreEqual(FString(TEXT("keep\n")), SiblingRead));
		IFileManager::Get().Delete(*Sibling);
		IFileManager::Get().Delete(*BlockingFile);
	}
};

#endif
