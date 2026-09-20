#include "CQTest.h"
#include "Framework/Generate/AngelscriptTestVarAssignTargetGenerator.h"
#include "FrameworkTests/Generate/GeneratedCaseExport.h"
#include "FrameworkTests/Generate/GeneratedCaseTestSupport.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_TESTS && WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(
	VarAssignTargetGenerator,
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

	static constexpr const TCHAR* ProbeCaseId = TEXT("LANG-VAR-ASSIGN-TARGET-SIMPLE-MUTABLE_LOCAL-INT8");
	static constexpr const TCHAR* ZeroCaseId = TEXT("LANG-VAR-ASSIGN-TARGET-SIMPLE-TEMPORARY-SCRIPT_VALUE");
	static constexpr const TCHAR* ForeignCaseId = TEXT("LANG-CF-FOR-CLAUSES-PRESENT-PRESENT-PRESENT-ONE");

	static bool IndependentIsRef(const TCHAR* Type)
	{
		return FCString::Strcmp(Type, TEXT("SCRIPT_REFERENCE")) == 0
			|| FCString::Strcmp(Type, TEXT("NATIVE_REFERENCE")) == 0;
	}

	static bool IndependentIsValueObject(const TCHAR* Type)
	{
		return FCString::Strcmp(Type, TEXT("SCRIPT_VALUE")) == 0
			|| FCString::Strcmp(Type, TEXT("NATIVE_VALUE")) == 0;
	}

	static bool IndependentIsObject(const TCHAR* Type)
	{
		return IndependentIsValueObject(Type) || IndependentIsRef(Type);
	}

	static bool IndependentSupportsCompound(const TCHAR* Type)
	{
		static const TCHAR* const kCompound[] = {
			TEXT("INT8"), TEXT("INT16"), TEXT("INT"), TEXT("INT64"), TEXT("UINT8"), TEXT("UINT16"),
			TEXT("UINT"), TEXT("UINT64"), TEXT("FLOAT32"), TEXT("FLOAT64"), TEXT("TYPEDEF")};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(kCompound); ++Index)
		{
			if (FCString::Strcmp(Type, kCompound[Index]) == 0)
			{
				return true;
			}
		}
		return false;
	}

	static bool IndependentWritable(const TCHAR* Target)
	{
		return FCString::Strcmp(Target, TEXT("MUTABLE_LOCAL")) == 0
			|| FCString::Strcmp(Target, TEXT("MUTABLE_FIELD")) == 0
			|| FCString::Strcmp(Target, TEXT("REFERENCE_ALIAS")) == 0;
	}

	static bool IndependentForkConstLocalRef(const TCHAR* Assignment, const TCHAR* Target, const TCHAR* Type)
	{
		return IndependentIsRef(Type)
			&& FCString::Strcmp(Target, TEXT("CONST_LOCAL")) == 0
			&& FCString::Strcmp(Assignment, TEXT("COMPOUND")) != 0;
	}

	static bool IndependentForkDiscarded(const TCHAR* Assignment, const TCHAR* Target, const TCHAR* Type)
	{
		const bool bAssign = FCString::Strcmp(Assignment, TEXT("SIMPLE")) == 0
			|| FCString::Strcmp(Assignment, TEXT("COPY_SOURCE")) == 0
			|| FCString::Strcmp(Assignment, TEXT("SELF_ASSIGNMENT")) == 0;
		const bool bTarget = FCString::Strcmp(Target, TEXT("TEMPORARY")) == 0
			|| FCString::Strcmp(Target, TEXT("EXPRESSION_RESULT")) == 0;
		return IndependentIsValueObject(Type) && bAssign && bTarget;
	}

	static bool IndependentReject(const TCHAR* Assignment, const TCHAR* Target, const TCHAR* Type)
	{
		if (IndependentForkConstLocalRef(Assignment, Target, Type)
			|| IndependentForkDiscarded(Assignment, Target, Type))
		{
			return false;
		}
		if (!IndependentWritable(Target))
		{
			return true;
		}
		if (FCString::Strcmp(Assignment, TEXT("COMPOUND")) == 0)
		{
			return !IndependentSupportsCompound(Type);
		}
		if (FCString::Strcmp(Assignment, TEXT("REFERENCE_REBIND")) == 0)
		{
			return !IndependentIsRef(Type);
		}
		return false;
	}

	static int32 IndependentInitial(const TCHAR* Type)
	{
		return FCString::Strcmp(Type, TEXT("BOOL")) == 0 || FCString::Strcmp(Type, TEXT("ENUM")) == 0 ? 0 : 11;
	}

	static int32 IndependentSource(const TCHAR* Type)
	{
		return FCString::Strcmp(Type, TEXT("BOOL")) == 0 || FCString::Strcmp(Type, TEXT("ENUM")) == 0 ? 1 : 29;
	}

	static int32 IndependentAlternate(const TCHAR* Type)
	{
		return FCString::Strcmp(Type, TEXT("BOOL")) == 0 || FCString::Strcmp(Type, TEXT("ENUM")) == 0 ? 0 : 37;
	}

	static int32 IndependentExpected(const TCHAR* Assignment, const TCHAR* Target, const TCHAR* Type)
	{
		if (IndependentForkDiscarded(Assignment, Target, Type))
		{
			return 0;
		}
		if (FCString::Strcmp(Assignment, TEXT("SELF_ASSIGNMENT")) == 0)
		{
			return IndependentInitial(Type);
		}
		if (FCString::Strcmp(Assignment, TEXT("COMPOUND")) == 0)
		{
			return IndependentInitial(Type) + IndependentSource(Type);
		}
		if (FCString::Strcmp(Assignment, TEXT("COPY_SOURCE")) == 0)
		{
			const int32 SourceAfter = FCString::Strcmp(Target, TEXT("REFERENCE_ALIAS")) == 0
				? IndependentSource(Type)
				: (IndependentIsObject(Type) ? IndependentAlternate(Type) : IndependentSource(Type));
			const int32 TargetAfter = IndependentIsRef(Type) ? SourceAfter : IndependentSource(Type);
			return TargetAfter * 100 + SourceAfter;
		}
		if (FCString::Strcmp(Assignment, TEXT("REFERENCE_REBIND")) == 0)
		{
			const int32 SharedAfter = IndependentAlternate(Type);
			return SharedAfter * 100 + SharedAfter;
		}
		return IndependentSource(Type);
	}

	static const TCHAR* IndependentScriptType(const TCHAR* Type)
	{
		if (FCString::Strcmp(Type, TEXT("INT8")) == 0) { return TEXT("int8"); }
		if (FCString::Strcmp(Type, TEXT("INT16")) == 0) { return TEXT("int16"); }
		if (FCString::Strcmp(Type, TEXT("INT")) == 0) { return TEXT("int"); }
		if (FCString::Strcmp(Type, TEXT("INT64")) == 0) { return TEXT("int64"); }
		if (FCString::Strcmp(Type, TEXT("UINT8")) == 0) { return TEXT("uint8"); }
		if (FCString::Strcmp(Type, TEXT("UINT16")) == 0) { return TEXT("uint16"); }
		if (FCString::Strcmp(Type, TEXT("UINT")) == 0) { return TEXT("uint"); }
		if (FCString::Strcmp(Type, TEXT("UINT64")) == 0) { return TEXT("uint64"); }
		if (FCString::Strcmp(Type, TEXT("FLOAT32")) == 0) { return TEXT("float"); }
		if (FCString::Strcmp(Type, TEXT("FLOAT64")) == 0) { return TEXT("double"); }
		if (FCString::Strcmp(Type, TEXT("BOOL")) == 0) { return TEXT("bool"); }
		if (FCString::Strcmp(Type, TEXT("ENUM")) == 0) { return TEXT("ENativeAssignmentEnum"); }
		if (FCString::Strcmp(Type, TEXT("TYPEDEF")) == 0) { return TEXT("NativeCaseAlias"); }
		if (FCString::Strcmp(Type, TEXT("SCRIPT_VALUE")) == 0) { return TEXT("FScriptCaseValue"); }
		if (FCString::Strcmp(Type, TEXT("NATIVE_VALUE")) == 0) { return TEXT("FNativeCaseValue"); }
		if (FCString::Strcmp(Type, TEXT("SCRIPT_REFERENCE")) == 0) { return TEXT("FScriptCaseReference"); }
		return TEXT("FNativeCaseReference");
	}

	static const TCHAR* IndependentTypeSuffix(const TCHAR* Type)
	{
		if (FCString::Strcmp(Type, TEXT("INT8")) == 0) { return TEXT("Int8"); }
		if (FCString::Strcmp(Type, TEXT("INT16")) == 0) { return TEXT("Int16"); }
		if (FCString::Strcmp(Type, TEXT("INT")) == 0) { return TEXT("Int"); }
		if (FCString::Strcmp(Type, TEXT("INT64")) == 0) { return TEXT("Int64"); }
		if (FCString::Strcmp(Type, TEXT("UINT8")) == 0) { return TEXT("Uint8"); }
		if (FCString::Strcmp(Type, TEXT("UINT16")) == 0) { return TEXT("Uint16"); }
		if (FCString::Strcmp(Type, TEXT("UINT")) == 0) { return TEXT("Uint"); }
		if (FCString::Strcmp(Type, TEXT("UINT64")) == 0) { return TEXT("Uint64"); }
		if (FCString::Strcmp(Type, TEXT("FLOAT32")) == 0) { return TEXT("Float32"); }
		if (FCString::Strcmp(Type, TEXT("FLOAT64")) == 0) { return TEXT("Float64"); }
		if (FCString::Strcmp(Type, TEXT("BOOL")) == 0) { return TEXT("Bool"); }
		if (FCString::Strcmp(Type, TEXT("ENUM")) == 0) { return TEXT("Enum"); }
		if (FCString::Strcmp(Type, TEXT("TYPEDEF")) == 0) { return TEXT("Typedef"); }
		if (FCString::Strcmp(Type, TEXT("SCRIPT_VALUE")) == 0) { return TEXT("ScriptValue"); }
		if (FCString::Strcmp(Type, TEXT("NATIVE_VALUE")) == 0) { return TEXT("NativeValue"); }
		if (FCString::Strcmp(Type, TEXT("SCRIPT_REFERENCE")) == 0) { return TEXT("ScriptReference"); }
		return TEXT("NativeReference");
	}

	static const TCHAR* IndependentKindSuffix(const TCHAR* Assignment)
	{
		if (FCString::Strcmp(Assignment, TEXT("COPY_SOURCE")) == 0) { return TEXT("CopySource"); }
		if (FCString::Strcmp(Assignment, TEXT("SELF_ASSIGNMENT")) == 0) { return TEXT("SelfAssignment"); }
		if (FCString::Strcmp(Assignment, TEXT("COMPOUND")) == 0) { return TEXT("Compound"); }
		if (FCString::Strcmp(Assignment, TEXT("REFERENCE_REBIND")) == 0) { return TEXT("ReferenceRebind"); }
		return TEXT("Simple");
	}

	static FString IndependentTypedValue(const TCHAR* Type, const int32 Value)
	{
		if (FCString::Strcmp(Type, TEXT("BOOL")) == 0)
		{
			return Value == 0 ? FString(TEXT("false")) : FString(TEXT("true"));
		}
		if (FCString::Strcmp(Type, TEXT("ENUM")) == 0)
		{
			return Value == 0 ? FString(TEXT("ENativeAssignmentEnum::Zero")) : FString(TEXT("ENativeAssignmentEnum::One"));
		}
		if (FCString::Strcmp(Type, TEXT("SCRIPT_REFERENCE")) == 0)
		{
			return FString::Printf(TEXT("FScriptCaseReference(%d)"), Value);
		}
		if (FCString::Strcmp(Type, TEXT("NATIVE_REFERENCE")) == 0)
		{
			return FString::Printf(TEXT("CreateNativeCaseReference(%d)"), Value);
		}
		return FString::Printf(TEXT("%s(%d)"), IndependentScriptType(Type), Value);
	}

	static FString IndependentNeedle(const TCHAR* Assignment, const TCHAR* Target, const TCHAR* Type)
	{
		if (IndependentForkDiscarded(Assignment, Target, Type))
		{
			return TEXT("return 0;");
		}
		if (FCString::Strcmp(Target, TEXT("REFERENCE_ALIAS")) == 0)
		{
			return FString::Printf(
				TEXT("ApplyAssignmentAlias%s%s("),
				IndependentKindSuffix(Assignment),
				IndependentTypeSuffix(Type));
		}
		if (IndependentReject(Assignment, Target, Type))
		{
			if (FCString::Strcmp(Target, TEXT("CONST_FIELD")) == 0)
			{
				return FString::Printf(TEXT("const FAssignmentHolder%s Holder"), IndependentTypeSuffix(Type));
			}
			if (FCString::Strcmp(Target, TEXT("CONST_LOCAL")) == 0)
			{
				return FString::Printf(TEXT("const %s Target"), IndependentScriptType(Type));
			}
			if (FCString::Strcmp(Target, TEXT("EXPRESSION_RESULT")) == 0)
			{
				return TEXT("(true ?");
			}
			if (FCString::Strcmp(Assignment, TEXT("REFERENCE_REBIND")) == 0)
			{
				return IndependentIsRef(Type) ? TEXT(" = Source;") : TEXT(" = nullptr;");
			}
			if (FCString::Strcmp(Assignment, TEXT("COMPOUND")) == 0)
			{
				return FString::Printf(TEXT(" += %s;"), *IndependentTypedValue(Type, IndependentSource(Type)));
			}
			return TEXT("return 0;");
		}
		if (FCString::Strcmp(Assignment, TEXT("SELF_ASSIGNMENT")) == 0
			&& FCString::Strcmp(Target, TEXT("MUTABLE_LOCAL")) == 0)
		{
			return TEXT("Target = Target;");
		}
		if (FCString::Strcmp(Assignment, TEXT("COMPOUND")) == 0
			&& FCString::Strcmp(Target, TEXT("MUTABLE_FIELD")) == 0)
		{
			return FString::Printf(
				TEXT("Holder.Target += %s;"),
				*IndependentTypedValue(Type, IndependentSource(Type)));
		}
		if (FCString::Strcmp(Assignment, TEXT("COPY_SOURCE")) == 0
			|| FCString::Strcmp(Assignment, TEXT("REFERENCE_REBIND")) == 0)
		{
			return TEXT("* 100 + ObserveAssignmentValue(Source)");
		}
		if (FCString::Strcmp(Target, TEXT("CONST_LOCAL")) == 0)
		{
			return FString::Printf(TEXT("const %s Target"), IndependentScriptType(Type));
		}
		if (FCString::Strcmp(Assignment, TEXT("SIMPLE")) == 0
			&& FCString::Strcmp(Target, TEXT("MUTABLE_LOCAL")) == 0
			&& FCString::Strcmp(Type, TEXT("INT8")) == 0)
		{
			return TEXT("Target = int8(29);");
		}
		return TEXT("return ObserveAssignmentValue(");
	}

	static TArray<FExpectedCell> BuildExpectedCells()
	{
		static const TCHAR* const kAssignments[] = {
			TEXT("SIMPLE"), TEXT("COPY_SOURCE"), TEXT("SELF_ASSIGNMENT"), TEXT("COMPOUND"), TEXT("REFERENCE_REBIND")};
		static const TCHAR* const kTargets[] = {
			TEXT("MUTABLE_LOCAL"), TEXT("CONST_LOCAL"), TEXT("MUTABLE_FIELD"), TEXT("CONST_FIELD"),
			TEXT("REFERENCE_ALIAS"), TEXT("TEMPORARY"), TEXT("EXPRESSION_RESULT")};
		static const TCHAR* const kTypes[] = {
			TEXT("INT8"), TEXT("INT16"), TEXT("INT"), TEXT("INT64"), TEXT("UINT8"), TEXT("UINT16"),
			TEXT("UINT"), TEXT("UINT64"), TEXT("FLOAT32"), TEXT("FLOAT64"), TEXT("BOOL"), TEXT("ENUM"),
			TEXT("TYPEDEF"), TEXT("SCRIPT_VALUE"), TEXT("NATIVE_VALUE"), TEXT("SCRIPT_REFERENCE"),
			TEXT("NATIVE_REFERENCE")};

		TArray<FExpectedCell> Cells;
		Cells.SetNum(595);
		for (int32 Index = 0; Index < 595; ++Index)
		{
			const TCHAR* const Assignment = kAssignments[Index / 119];
			const TCHAR* const Target = kTargets[(Index / 17) % 7];
			const TCHAR* const Type = kTypes[Index % 17];
			FExpectedCell& Cell = Cells[Index];
			Cell.CaseId = FString::Printf(TEXT("LANG-VAR-ASSIGN-TARGET-%s-%s-%s"), Assignment, Target, Type);
			Cell.EntryName = AngelscriptTest::Generate::MakeCanonicalEntryName(Cell.CaseId);
			Cell.bReject = IndependentReject(Assignment, Target, Type);
			Cell.Expected = Cell.bReject ? 0 : IndependentExpected(Assignment, Target, Type);
			Cell.Needle = IndependentNeedle(Assignment, Target, Type);
		}
		return Cells;
	}

public:
	TEST_METHOD(GeneratesAndExportsAllCases)
	{
		using AngelscriptTest::Generate::EGeneratedCaseExecution;
		using AngelscriptTest::Generate::EGeneratedCaseOutcome;
		using AngelscriptTest::Generate::EVarAssignKind;
		using AngelscriptTest::Generate::EVarAssignTarget;
		using AngelscriptTest::Generate::EVarAssignType;
		using AngelscriptTest::Generate::FGeneratedCaseInfo;
		using AngelscriptTest::Generate::FVarAssignTargetGenerator;
		using AngelscriptTest::Generate::FVarAssignTargetParams;
		using AngelscriptTest::Generate::MakeCanonicalEntryName;
		using AngelscriptTest::Generate::TestSupport::CountOccurrences;
		using AngelscriptTest::Generate::TestSupport::ExtractFunction;
		using AngelscriptTest::Generate::TestSupport::HasTrailingWhitespace;
		using AngelscriptTest::Generate::TestSupport::LoadGold;

		const TArray<FExpectedCell> ExpectedCells = BuildExpectedCells();
		FVarAssignTargetGenerator Generator;
		ASSERT_THAT(AreEqual(FString(TEXT("LANG-VAR-ASSIGN-TARGET")), FString(Generator.GetProductId())));
		ASSERT_THAT(AreEqual(
			FString(TEXT("EntryLangVarAssignTargetSimpleMutableLocalInt8")),
			MakeCanonicalEntryName(ProbeCaseId)));

		int32 CaseCount = 0;
		const FString Aggregate = Generator.BuildAllSource(CaseCount);
		const FString RepeatAggregate = Generator.BuildAllSource(CaseCount);
		const FString Dump = Generator.BuildDumpSource();
		const FString OutputFile =
			AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("VarAssignTargetGenerator"));
		FString WriteError;
		const bool bWroteDump = AngelscriptTest::FGeneratedCaseExport::WriteDump(OutputFile, Dump, WriteError);
		UE_LOG(
			LogTemp,
			Display,
			TEXT("VarAssignTargetGenerator dump path=%s cases=595 wrote=%s error=%s"),
			*OutputFile,
			bWroteDump ? TEXT("true") : TEXT("false"),
			*WriteError);

		TArray<FGeneratedCaseInfo> Cases = Generator.ListCases();
		const TArray<FString> RejectIds = Generator.ListRejectCaseIds();
		ASSERT_THAT(AreEqual(212, CaseCount));
		ASSERT_THAT(AreEqual(595, Cases.Num()));
		ASSERT_THAT(AreEqual(383, RejectIds.Num()));
		ASSERT_THAT(AreEqual(Aggregate, RepeatAggregate));

		const FString Representative =
			LoadGold(TEXT("LanguageGenerators/VarAssignTargetGenerator/RepresentativeSources.as"));
		ASSERT_THAT(IsTrue(Representative.Len() > 0));

		int32 RejectCount = 0;
		int32 ZeroCount = 0;
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
			else
			{
				ASSERT_THAT(AreEqual(
					FString::Printf(TEXT("int %s()"), *Expected.EntryName),
					Cases[Index].EntryDeclaration));
				ASSERT_THAT(IsTrue(Cases[Index].Outcome == EGeneratedCaseOutcome::ReturnValue));
				ASSERT_THAT(IsTrue(Cases[Index].Execution == EGeneratedCaseExecution::Standalone));
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(Expected.Expected, Cases[Index].ExpectedReturn.Get(MIN_int32)));
				ASSERT_THAT(AreEqual(Expected.Expected, Generator.GetExpected(Expected.CaseId)));
				ASSERT_THAT(IsTrue(Generator.BuildRejectSource(Expected.CaseId).IsEmpty()));
				if (Expected.Expected == 0)
				{
					++ZeroCount;
				}
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
		ASSERT_THAT(AreEqual(383, RejectCount));
		ASSERT_THAT(IsTrue(ZeroCount > 0));

		const FString OwnedCase = Generator.BuildCaseSource(ProbeCaseId);
		const FString OwnedAgain = Generator.BuildCaseSource(ProbeCaseId);
		ASSERT_THAT(IsTrue(OwnedCase.Len() > 0));
		ASSERT_THAT(AreEqual(OwnedCase, OwnedAgain));
		ASSERT_THAT(IsTrue(OwnedCase.Contains(TEXT("int EntryLangVarAssignTargetSimpleMutableLocalInt8()"))));
		ASSERT_THAT(IsTrue(Aggregate.Contains(TEXT("int EntryLangVarAssignTargetSimpleMutableLocalInt8()"))));
		ASSERT_THAT(IsFalse(Aggregate.Contains(TEXT("int Entry_"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("enum ENativeAssignmentEnum\n"))));
		ASSERT_THAT(AreEqual(1, CountOccurrences(Aggregate, TEXT("class FAssignmentHolderInt8\n"))));

		FVarAssignTargetParams DefaultParams;
		const FString DefaultSingle = Generator.BuildAssignmentSource(DefaultParams);
		ASSERT_THAT(IsTrue(DefaultSingle.Contains(TEXT("int Entry()"))));
		ASSERT_THAT(IsFalse(DefaultSingle.Contains(TEXT("int EntryLang"))));
		FVarAssignTargetParams ProbeParams = DefaultParams;
		ProbeParams.FunctionName = TEXT("ProbeEntry");
		ASSERT_THAT(IsTrue(Generator.BuildAssignmentSource(ProbeParams).Contains(TEXT("int ProbeEntry()"))));
		ProbeParams.FunctionName = TEXT("bad-name");
		ASSERT_THAT(IsTrue(Generator.BuildAssignmentSource(ProbeParams).IsEmpty()));
		FVarAssignTargetParams InvalidEnum = DefaultParams;
		InvalidEnum.Assignment = static_cast<EVarAssignKind>(5);
		ASSERT_THAT(IsTrue(Generator.BuildAssignmentSource(InvalidEnum).IsEmpty()));
		FVarAssignTargetParams RejectParams = DefaultParams;
		RejectParams.Target = EVarAssignTarget::ConstField;
		ASSERT_THAT(IsTrue(Generator.BuildAssignmentSource(RejectParams).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(TEXT("UNKNOWN-CASE")).IsEmpty()));
		ASSERT_THAT(IsTrue(Generator.BuildCaseSource(ForeignCaseId).IsEmpty()));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(TEXT("UNKNOWN-CASE"))));
		ASSERT_THAT(AreEqual(0, Generator.GetExpected(ZeroCaseId)));
		bool bFoundZeroCase = false;
		for (int32 Index = 0; Index < Cases.Num(); ++Index)
		{
			if (Cases[Index].CaseId == ZeroCaseId)
			{
				bFoundZeroCase = true;
				ASSERT_THAT(IsTrue(Cases[Index].ExpectedReturn.IsSet()));
				ASSERT_THAT(AreEqual(0, Cases[Index].ExpectedReturn.Get(MIN_int32)));
			}
		}
		ASSERT_THAT(IsTrue(bFoundZeroCase));

		TArray<FGeneratedCaseInfo> Retained = Generator.ListCases();
		Retained[0].CaseId = TEXT("MUTATED");
		Retained[0].ExpectedReturn.Reset();
		const TArray<FGeneratedCaseInfo> Relisted = Generator.ListCases();
		ASSERT_THAT(AreEqual(ExpectedCells[0].CaseId, Relisted[0].CaseId));
		ASSERT_THAT(IsTrue(Relisted[0].ExpectedReturn.IsSet()));
		ASSERT_THAT(AreEqual(ExpectedCells[0].Expected, Relisted[0].ExpectedReturn.Get(MIN_int32)));

		ASSERT_THAT(AreEqual(595, CountOccurrences(Dump, TEXT("// Case: LANG-VAR-ASSIGN-TARGET-"))));
		ASSERT_THAT(AreEqual(383, CountOccurrences(Dump, TEXT("// Reject module"))));
		ASSERT_THAT(IsTrue(
			OutputFile.EndsWith(TEXT("GeneratedCases/VarAssignTargetGenerator.as"))
			|| OutputFile.EndsWith(TEXT("GeneratedCases\\VarAssignTargetGenerator.as"))));
		ASSERT_THAT(IsTrue(bWroteDump, *WriteError));
		FString ReadBack;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(ReadBack, *OutputFile)));
		ASSERT_THAT(AreEqual(Dump, ReadBack));
		ASSERT_THAT(AreEqual(Dump, Generator.BuildDumpSource()));
		ASSERT_THAT(IsTrue(Dump.EndsWith(TEXT("\n"))));
		ASSERT_THAT(IsFalse(Dump.Contains(TEXT("\r"))));
		ASSERT_THAT(IsFalse(HasTrailingWhitespace(Dump)));

		ASSERT_THAT(IsTrue(AngelscriptTest::FGeneratedCaseExport::ResolveOutputFile(TEXT("")).IsEmpty()));
		const FString Sibling = FPaths::Combine(FPaths::GetPath(OutputFile), TEXT("SiblingProbe.as"));
		FFileHelper::SaveStringToFile(TEXT("keep\n"), *Sibling, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString ProbeError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			TEXT("C:/no-such-root/VarAssignTargetGenerator.as"),
			Dump,
			ProbeError)));
		ASSERT_THAT(IsTrue(ProbeError.Len() > 0));
		const FString BlockingFile = OutputFile + TEXT(".block");
		FFileHelper::SaveStringToFile(TEXT("file\n"), *BlockingFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FString FileParentError;
		ASSERT_THAT(IsFalse(AngelscriptTest::FGeneratedCaseExport::WriteDump(
			FPaths::Combine(BlockingFile, TEXT("VarAssignTargetGenerator.as")),
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
