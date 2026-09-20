#include "Framework/Generate/AngelscriptTestOpOverloadIntegerConsumerGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace OpOverloadIntegerConsumerGeneratorDetail
{
constexpr int32 ScenarioCount = 20;
constexpr int32 ContextCount = 6;
constexpr int32 CellCount = ScenarioCount * ContextCount;
constexpr const TCHAR* LimitedNotes = TEXT("int32 Marker is a scenario identity, not full-width arithmetic proof.");

enum class EIntegerFamily : uint8
{
	Unary,
	Binary,
	Index,
	Call,
	Conversion,
};

enum class EIntegerOutcome : uint8
{
	Success,
	Ambiguous,
	Missing,
};

enum class EIntegerOperand : uint8
{
	None,
	Int,
	Int64Promotion,
	Value,
	AmbiguousInt8,
};

struct FIntegerScenario
{
	EOpOverloadIntegerConsumerScenario Scenario = EOpOverloadIntegerConsumerScenario::UnaryMember;
	EIntegerFamily Family = EIntegerFamily::Unary;
	EIntegerOutcome Outcome = EIntegerOutcome::Success;
	EIntegerOperand Operand = EIntegerOperand::None;
	bool bConstMethod = false;
	int32 Marker = 0;
	const TCHAR* Token = TEXT("UNARY_MEMBER");
	const TCHAR* TypeName = TEXT("FOverloadedIntegerUnaryMember");
};

struct FIntegerCell
{
	EOpOverloadIntegerConsumerScenario Scenario = EOpOverloadIntegerConsumerScenario::UnaryMember;
	EOpOverloadIntegerConsumerContext Context = EOpOverloadIntegerConsumerContext::Assignment;
};

const FIntegerScenario ScenarioTable[ScenarioCount] = {
	{EOpOverloadIntegerConsumerScenario::UnaryMember, EIntegerFamily::Unary, EIntegerOutcome::Success,
		EIntegerOperand::None, false, 101, TEXT("UNARY_MEMBER"), TEXT("FOverloadedIntegerUnaryMember")},
	{EOpOverloadIntegerConsumerScenario::UnaryConstMember, EIntegerFamily::Unary, EIntegerOutcome::Success,
		EIntegerOperand::None, true, 102, TEXT("UNARY_CONST_MEMBER"), TEXT("FOverloadedIntegerUnaryConstMember")},
	{EOpOverloadIntegerConsumerScenario::BinaryIntMember, EIntegerFamily::Binary, EIntegerOutcome::Success,
		EIntegerOperand::Int, false, 103, TEXT("BINARY_INT_MEMBER"), TEXT("FOverloadedIntegerBinaryIntMember")},
	{EOpOverloadIntegerConsumerScenario::BinaryIntConstMember, EIntegerFamily::Binary, EIntegerOutcome::Success,
		EIntegerOperand::Int, true, 104, TEXT("BINARY_INT_CONST_MEMBER"), TEXT("FOverloadedIntegerBinaryIntConstMember")},
	{EOpOverloadIntegerConsumerScenario::BinaryInt64Promotion, EIntegerFamily::Binary, EIntegerOutcome::Success,
		EIntegerOperand::Int64Promotion, true, 105, TEXT("BINARY_INT64_PROMOTION"), TEXT("FOverloadedIntegerBinaryInt64Promotion")},
	{EOpOverloadIntegerConsumerScenario::BinaryValueParameter, EIntegerFamily::Binary, EIntegerOutcome::Success,
		EIntegerOperand::Value, true, 106, TEXT("BINARY_VALUE_PARAMETER"), TEXT("FOverloadedIntegerBinaryValueParameter")},
	{EOpOverloadIntegerConsumerScenario::IndexIntParameter, EIntegerFamily::Index, EIntegerOutcome::Success,
		EIntegerOperand::Int, true, 109, TEXT("INDEX_INT_PARAMETER"), TEXT("FOverloadedIntegerIndexIntParameter")},
	{EOpOverloadIntegerConsumerScenario::IndexInt64Promotion, EIntegerFamily::Index, EIntegerOutcome::Success,
		EIntegerOperand::Int64Promotion, true, 110, TEXT("INDEX_INT64_PROMOTION"), TEXT("FOverloadedIntegerIndexInt64Promotion")},
	{EOpOverloadIntegerConsumerScenario::CallIntMember, EIntegerFamily::Call, EIntegerOutcome::Success,
		EIntegerOperand::Int, false, 111, TEXT("CALL_INT_MEMBER"), TEXT("FOverloadedIntegerCallIntMember")},
	{EOpOverloadIntegerConsumerScenario::CallInt64Promotion, EIntegerFamily::Call, EIntegerOutcome::Success,
		EIntegerOperand::Int64Promotion, true, 112, TEXT("CALL_INT64_PROMOTION"), TEXT("FOverloadedIntegerCallInt64Promotion")},
	{EOpOverloadIntegerConsumerScenario::ConversionIntTarget, EIntegerFamily::Conversion, EIntegerOutcome::Success,
		EIntegerOperand::None, true, 113, TEXT("CONVERSION_INT_TARGET"), TEXT("FOverloadedIntegerConversionIntTarget")},
	{EOpOverloadIntegerConsumerScenario::BinaryAmbiguousInt8, EIntegerFamily::Binary, EIntegerOutcome::Ambiguous,
		EIntegerOperand::AmbiguousInt8, true, 201, TEXT("BINARY_AMBIGUOUS_INT8"), TEXT("FOverloadedIntegerBinaryAmbiguousInt8")},
	{EOpOverloadIntegerConsumerScenario::IndexAmbiguousInt8, EIntegerFamily::Index, EIntegerOutcome::Ambiguous,
		EIntegerOperand::AmbiguousInt8, true, 203, TEXT("INDEX_AMBIGUOUS_INT8"), TEXT("FOverloadedIntegerIndexAmbiguousInt8")},
	{EOpOverloadIntegerConsumerScenario::CallAmbiguousInt8, EIntegerFamily::Call, EIntegerOutcome::Ambiguous,
		EIntegerOperand::AmbiguousInt8, true, 204, TEXT("CALL_AMBIGUOUS_INT8"), TEXT("FOverloadedIntegerCallAmbiguousInt8")},
	{EOpOverloadIntegerConsumerScenario::ConversionAmbiguousTarget, EIntegerFamily::Conversion, EIntegerOutcome::Ambiguous,
		EIntegerOperand::AmbiguousInt8, true, 205, TEXT("CONVERSION_AMBIGUOUS_TARGET"), TEXT("FOverloadedIntegerConversionAmbiguousTarget")},
	{EOpOverloadIntegerConsumerScenario::UnaryMissingComplement, EIntegerFamily::Unary, EIntegerOutcome::Missing,
		EIntegerOperand::None, true, 301, TEXT("UNARY_MISSING_COMPLEMENT"), TEXT("FOverloadedIntegerUnaryMissingComplement")},
	{EOpOverloadIntegerConsumerScenario::BinaryMissingSubtract, EIntegerFamily::Binary, EIntegerOutcome::Missing,
		EIntegerOperand::Value, true, 302, TEXT("BINARY_MISSING_SUBTRACT"), TEXT("FOverloadedIntegerBinaryMissingSubtract")},
	{EOpOverloadIntegerConsumerScenario::IndexMissingSecondArgument, EIntegerFamily::Index, EIntegerOutcome::Missing,
		EIntegerOperand::Int, true, 304, TEXT("INDEX_MISSING_SECOND_ARGUMENT"), TEXT("FOverloadedIntegerIndexMissingSecondArgument")},
	{EOpOverloadIntegerConsumerScenario::CallMissingSecondArgument, EIntegerFamily::Call, EIntegerOutcome::Missing,
		EIntegerOperand::Int, true, 305, TEXT("CALL_MISSING_SECOND_ARGUMENT"), TEXT("FOverloadedIntegerCallMissingSecondArgument")},
	{EOpOverloadIntegerConsumerScenario::ConversionMissingTarget, EIntegerFamily::Conversion, EIntegerOutcome::Missing,
		EIntegerOperand::None, true, 306, TEXT("CONVERSION_MISSING_TARGET"), TEXT("FOverloadedIntegerConversionMissingTarget")},
};

const FIntegerScenario& ScenarioInfo(const EOpOverloadIntegerConsumerScenario Scenario)
{
	const int32 Index = static_cast<int32>(Scenario);
	return ScenarioTable[Index >= 0 && Index < ScenarioCount ? Index : 0];
}

const TCHAR* ContextToken(const EOpOverloadIntegerConsumerContext Context)
{
	switch (Context)
	{
	case EOpOverloadIntegerConsumerContext::Assignment:
		return TEXT("ASSIGNMENT");
	case EOpOverloadIntegerConsumerContext::Return:
		return TEXT("RETURN");
	case EOpOverloadIntegerConsumerContext::Condition:
		return TEXT("CONDITION");
	case EOpOverloadIntegerConsumerContext::OverloadArgument:
		return TEXT("OVERLOAD_ARGUMENT");
	case EOpOverloadIntegerConsumerContext::Chain:
		return TEXT("CHAIN");
	case EOpOverloadIntegerConsumerContext::SwitchIndex:
		return TEXT("SWITCH_INDEX");
	default:
		return nullptr;
	}
}

const TCHAR* ScenarioToken(const EOpOverloadIntegerConsumerScenario Scenario)
{
	const int32 Index = static_cast<int32>(Scenario);
	return Index >= 0 && Index < ScenarioCount ? ScenarioTable[Index].Token : nullptr;
}

bool IsReject(const EOpOverloadIntegerConsumerScenario Scenario)
{
	return ScenarioInfo(Scenario).Outcome != EIntegerOutcome::Success;
}

bool IsValidParams(const FOpOverloadIntegerConsumerParams& Params)
{
	return ScenarioToken(Params.Scenario) != nullptr && ContextToken(Params.Context) != nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FIntegerCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-OVERLOAD-INTEGER-CONSUMER-%s-%s"),
		ScenarioToken(Cell.Scenario),
		ContextToken(Cell.Context));
}

FString MakeEntryName(const FIntegerCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpOverloadIntegerConsumerParams ParamsFromCell(const FIntegerCell& Cell)
{
	FOpOverloadIntegerConsumerParams Params;
	Params.Scenario = Cell.Scenario;
	Params.Context = Cell.Context;
	return Params;
}

FIntegerCell CellFromIndex(const int32 Index)
{
	FIntegerCell Cell;
	Cell.Scenario = static_cast<EOpOverloadIntegerConsumerScenario>(Index / ContextCount);
	Cell.Context = static_cast<EOpOverloadIntegerConsumerContext>(Index % ContextCount);
	return Cell;
}

bool TryFindCell(const FStringView CaseId, FIntegerCell& OutCell)
{
	const FString Needle(CaseId);
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FIntegerCell Cell = CellFromIndex(Index);
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

const TCHAR* MethodName(const EIntegerFamily Family)
{
	switch (Family)
	{
	case EIntegerFamily::Unary:
		return TEXT("opNeg");
	case EIntegerFamily::Binary:
		return TEXT("opAdd");
	case EIntegerFamily::Index:
		return TEXT("opIndex");
	case EIntegerFamily::Call:
		return TEXT("opCall");
	case EIntegerFamily::Conversion:
		return TEXT("opImplConv");
	default:
		return TEXT("opNeg");
	}
}

FString ParameterDecl(const EIntegerOperand Operand, const TCHAR* TypeName)
{
	switch (Operand)
	{
	case EIntegerOperand::Int:
		return TEXT("int Other");
	case EIntegerOperand::Int64Promotion:
		return TEXT("int64 Other");
	case EIntegerOperand::Value:
		return FString::Printf(TEXT("const %s& Other"), TypeName);
	default:
		return FString();
	}
}

FString ArgumentExpression(const EIntegerOperand Operand)
{
	switch (Operand)
	{
	case EIntegerOperand::Value:
		return TEXT("Right");
	case EIntegerOperand::Int64Promotion:
	case EIntegerOperand::AmbiguousInt8:
		return TEXT("int8(6)");
	case EIntegerOperand::Int:
		return TEXT("6");
	default:
		return FString();
	}
}

FString SuccessExpression(const FIntegerScenario& Info)
{
	const FString Argument = ArgumentExpression(Info.Operand);
	switch (Info.Family)
	{
	case EIntegerFamily::Unary:
		return TEXT("-Left");
	case EIntegerFamily::Binary:
		return FString::Printf(TEXT("Left + %s"), *Argument);
	case EIntegerFamily::Index:
		return FString::Printf(TEXT("Left[%s]"), *Argument);
	case EIntegerFamily::Call:
		return FString::Printf(TEXT("Left(%s)"), *Argument);
	case EIntegerFamily::Conversion:
		return TEXT("int(Left)");
	default:
		return TEXT("0");
	}
}

FString AmbiguousExpression(const FIntegerScenario& Info)
{
	if (Info.Family == EIntegerFamily::Conversion)
	{
		return TEXT("ConsumeAmbiguous(Left)");
	}
	const FString Argument = ArgumentExpression(EIntegerOperand::AmbiguousInt8);
	switch (Info.Family)
	{
	case EIntegerFamily::Binary:
		return FString::Printf(TEXT("Left + %s"), *Argument);
	case EIntegerFamily::Index:
		return FString::Printf(TEXT("Left[%s]"), *Argument);
	case EIntegerFamily::Call:
		return FString::Printf(TEXT("Left(%s)"), *Argument);
	default:
		return TEXT("0");
	}
}

FString MissingExpression(const FIntegerScenario& Info)
{
	switch (Info.Family)
	{
	case EIntegerFamily::Unary:
		return TEXT("~Left");
	case EIntegerFamily::Binary:
		return TEXT("Left - Right");
	case EIntegerFamily::Index:
		return TEXT("Left[int8(1), int8(2)]");
	case EIntegerFamily::Call:
		return TEXT("Left(1, 2)");
	case EIntegerFamily::Conversion:
		return TEXT("ConsumeMissingConversion(Left)");
	default:
		return TEXT("0");
	}
}

FString OperationExpression(const FIntegerScenario& Info)
{
	switch (Info.Outcome)
	{
	case EIntegerOutcome::Success:
		return SuccessExpression(Info);
	case EIntegerOutcome::Ambiguous:
		return AmbiguousExpression(Info);
	case EIntegerOutcome::Missing:
		return MissingExpression(Info);
	default:
		return TEXT("0");
	}
}

void AppendTypeHeader(FString& Source, const TCHAR* Name)
{
	AppendLine(Source, FString::Printf(TEXT("struct %s"), Name));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value;"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("\t%s(int InValue)"), Name));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
}

void AppendOperatorMethod(
	FString& Source,
	const TCHAR* TypeName,
	const EIntegerFamily Family,
	const FString& Parameter,
	const bool bConstMethod,
	const int32 Marker,
	const TCHAR* CauseSuffix)
{
	const TCHAR* const ConstSuffix = bConstMethod ? TEXT(" const") : TEXT("");
	const FString Signature = Parameter.IsEmpty()
		? FString::Printf(TEXT("\tint %s()%s%s"), MethodName(Family), ConstSuffix, CauseSuffix)
		: FString::Printf(TEXT("\tint %s(%s)%s%s"), MethodName(Family), *Parameter, ConstSuffix, CauseSuffix);
	AppendLine(Source, Signature);
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, FString::Printf(TEXT("\t\treturn %d;"), Marker));
	AppendLine(Source, TEXT("\t}"));
	(void)TypeName;
}

void AppendScenarioType(FString& Source, const FIntegerScenario& Info)
{
	AppendTypeHeader(Source, Info.TypeName);
	if (Info.Outcome == EIntegerOutcome::Success)
	{
		AppendOperatorMethod(
			Source,
			Info.TypeName,
			Info.Family,
			ParameterDecl(Info.Operand, Info.TypeName),
			Info.bConstMethod,
			Info.Marker,
			TEXT(""));
	}
	else if (Info.Outcome == EIntegerOutcome::Ambiguous)
	{
		if (Info.Family == EIntegerFamily::Conversion)
		{
			AppendLine(Source, TEXT("\tint8 opImplConv() const"));
			AppendLine(Source, TEXT("\t{"));
			AppendLine(Source, TEXT("\t\treturn int8(6);"));
			AppendLine(Source, TEXT("\t}"));
		}
		else
		{
			AppendOperatorMethod(Source, Info.TypeName, Info.Family, TEXT("int64 Other"), Info.bConstMethod, Info.Marker, TEXT(""));
			AppendLine(Source);
			AppendOperatorMethod(
				Source,
				Info.TypeName,
				Info.Family,
				TEXT("uint64 Other"),
				Info.bConstMethod,
				Info.Marker + 1,
				TEXT(" // OP_OVERLOAD_OPERATION_CAUSE"));
		}
	}
	else if (Info.Family != EIntegerFamily::Conversion)
	{
		const EIntegerOperand Available = (Info.Family == EIntegerFamily::Binary)
			? EIntegerOperand::Value
			: EIntegerOperand::Int;
		AppendOperatorMethod(
			Source,
			Info.TypeName,
			Info.Family,
			ParameterDecl(Available, Info.TypeName),
			Info.bConstMethod,
			Info.Marker,
			TEXT(""));
	}
	AppendLine(Source, TEXT("}"));
}

void AppendIntHelpers(FString& Source)
{
	AppendLine(Source, TEXT("int ReturnIntMarker(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ConsumeIntMarker(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ChainIntMarker(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
}

void AppendAmbiguousHelpers(FString& Source)
{
	AppendLine(Source, TEXT("int ConsumeAmbiguous(int64 Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ConsumeAmbiguous(uint64 Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
}

void AppendMissingConversionHelper(FString& Source)
{
	AppendLine(Source, TEXT("struct FOverloadedOperatorTarget"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ConsumeMissingConversion(FOverloadedOperatorTarget Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value.Value;"));
	AppendLine(Source, TEXT("}"));
}

void AppendContext(
	FString& Source,
	const FOpOverloadIntegerConsumerParams& Params,
	const FString& Expression)
{
	const FIntegerScenario& Info = ScenarioInfo(Params.Scenario);
	const TCHAR* const Cause = Info.Outcome != EIntegerOutcome::Success ? TEXT(" // OP_OVERLOAD_OPERATION_CAUSE") : TEXT("");
	switch (Params.Context)
	{
	case EOpOverloadIntegerConsumerContext::Assignment:
		AppendLine(Source, FString::Printf(TEXT("\tint Result = %s;%s"), *Expression, Cause));
		AppendLine(Source, TEXT("\treturn Result;"));
		break;
	case EOpOverloadIntegerConsumerContext::Return:
		AppendLine(Source, FString::Printf(TEXT("\treturn ReturnIntMarker(%s);%s"), *Expression, Cause));
		break;
	case EOpOverloadIntegerConsumerContext::Condition:
		AppendLine(Source, FString::Printf(TEXT("\tif (%s == %d)%s"), *Expression, Info.Marker, Cause));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\treturn %d;"), Info.Marker));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn -1;"));
		break;
	case EOpOverloadIntegerConsumerContext::OverloadArgument:
		AppendLine(Source, FString::Printf(TEXT("\treturn ConsumeIntMarker(%s);%s"), *Expression, Cause));
		break;
	case EOpOverloadIntegerConsumerContext::Chain:
		AppendLine(Source, FString::Printf(TEXT("\treturn ChainIntMarker(%s + 0);%s"), *Expression, Cause));
		break;
	case EOpOverloadIntegerConsumerContext::SwitchIndex:
		AppendLine(Source, FString::Printf(TEXT("\tint Index = %s;%s"), *Expression, Cause));
		AppendLine(Source, TEXT("\tswitch (Index)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\tcase %d:"), Info.Marker));
		AppendLine(Source, FString::Printf(TEXT("\t\treturn %d;"), Info.Marker));
		AppendLine(Source, TEXT("\tdefault:"));
		AppendLine(Source, TEXT("\t\treturn -1;"));
		AppendLine(Source, TEXT("\t}"));
		break;
	}
}

void AppendEntry(FString& Source, const FOpOverloadIntegerConsumerParams& Params, const FString& FunctionName)
{
	const FIntegerScenario& Info = ScenarioInfo(Params.Scenario);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Left(5);"), Info.TypeName));
	AppendLine(Source, FString::Printf(TEXT("\t%s Right(6);"), Info.TypeName));
	AppendContext(Source, Params, OperationExpression(Info));
	AppendLine(Source, TEXT("}"));
}

void AppendOwnedModule(FString& Source, const FOpOverloadIntegerConsumerParams& Params, const FString& FunctionName)
{
	const FIntegerScenario& Info = ScenarioInfo(Params.Scenario);
	AppendScenarioType(Source, Info);
	AppendLine(Source);
	AppendIntHelpers(Source);
	if (Info.Outcome == EIntegerOutcome::Ambiguous && Info.Family == EIntegerFamily::Conversion)
	{
		AppendLine(Source);
		AppendAmbiguousHelpers(Source);
	}
	if (Info.Outcome == EIntegerOutcome::Missing && Info.Family == EIntegerFamily::Conversion)
	{
		AppendLine(Source);
		AppendMissingConversionHelper(Source);
	}
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
}

void AppendSuccessTypesAndHelpers(FString& Source)
{
	for (int32 Index = 0; Index < ScenarioCount; ++Index)
	{
		if (ScenarioTable[Index].Outcome != EIntegerOutcome::Success)
		{
			continue;
		}
		AppendScenarioType(Source, ScenarioTable[Index]);
		AppendLine(Source);
	}
	AppendIntHelpers(Source);
}

FString BuildOwnedSource(const FOpOverloadIntegerConsumerParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOwnedModule(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FIntegerCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Scenario))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
	}
	else
	{
		Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.Execution = EGeneratedCaseExecution::Standalone;
		Info.ExpectedReturn = ScenarioInfo(Cell.Scenario).Marker;
		Info.bLimitedObservation = true;
		Info.ExecutionNotes = LimitedNotes;
	}
	return Info;
}
}

FStringView FOpOverloadIntegerConsumerGenerator::GetProductId() const
{
	return TEXT("LANG-OP-OVERLOAD-INTEGER-CONSUMER");
}

TArray<FGeneratedCaseInfo> FOpOverloadIntegerConsumerGenerator::ListCases() const
{
	using namespace OpOverloadIntegerConsumerGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FIntegerCell Cell = CellFromIndex(Index);
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("OpOverloadIntegerConsumer PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FOpOverloadIntegerConsumerGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace OpOverloadIntegerConsumerGeneratorDetail;
	FIntegerCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FOpOverloadIntegerConsumerGenerator::BuildDumpSource() const
{
	using namespace OpOverloadIntegerConsumerGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-OVERLOAD-INTEGER-CONSUMER"));
	AppendLine(Source, TEXT("// Cases: 120 total, 66 normal, 54 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSuccessTypesAndHelpers(Source);
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FIntegerCell Cell = CellFromIndex(Index);
		if (IsReject(Cell.Scenario))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source, TEXT("// LimitedObservation: true"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FIntegerCell Cell = CellFromIndex(Index);
		if (!IsReject(Cell.Scenario))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, TEXT("// Reject module"));
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, TEXT("// EntryDeclaration:"));
		AppendLine(Source, TEXT("// Outcome: CompileReject"));
		AppendLine(Source, TEXT("// Execution: SourceOnly"));
		AppendLine(Source);
		AppendOwnedModule(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FOpOverloadIntegerConsumerGenerator::BuildScenarioSource(const FOpOverloadIntegerConsumerParams& Params) const
{
	using namespace OpOverloadIntegerConsumerGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Scenario))
	{
		return FString();
	}
	if (!Params.FunctionName.IsEmpty() && !IsValidGeneratedFunctionName(Params.FunctionName))
	{
		return FString();
	}
	const FString FunctionName = Params.FunctionName.IsEmpty() ? FString(TEXT("Entry")) : Params.FunctionName;
	return BuildOwnedSource(Params, FunctionName);
}

FString FOpOverloadIntegerConsumerGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace OpOverloadIntegerConsumerGeneratorDetail;
	FString Source;
	AppendSuccessTypesAndHelpers(Source);
	int32 Emitted = 0;
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FIntegerCell Cell = CellFromIndex(Index);
		if (IsReject(Cell.Scenario))
		{
			continue;
		}
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

int32 FOpOverloadIntegerConsumerGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpOverloadIntegerConsumerGeneratorDetail;
	FIntegerCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Scenario))
	{
		return 0;
	}
	return ScenarioInfo(Cell.Scenario).Marker;
}

FString FOpOverloadIntegerConsumerGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace OpOverloadIntegerConsumerGeneratorDetail;
	FIntegerCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Scenario))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FOpOverloadIntegerConsumerGenerator::ListRejectCaseIds() const
{
	using namespace OpOverloadIntegerConsumerGeneratorDetail;
	TArray<FString> Ids;
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FIntegerCell Cell = CellFromIndex(Index);
		if (IsReject(Cell.Scenario))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
