#include "Framework/Generate/AngelscriptTestOpOverloadBooleanConsumerGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace OpOverloadBooleanConsumerGeneratorDetail
{
constexpr int32 ScenarioCount = 4;
constexpr int32 ContextCount = 5;
constexpr int32 CellCount = ScenarioCount * ContextCount;
constexpr const TCHAR* LimitedNotes = TEXT("int32 Marker is a scenario identity, not full-width arithmetic proof.");

struct FBooleanCell
{
	EOpOverloadBooleanConsumerScenario Scenario = EOpOverloadBooleanConsumerScenario::ComparisonIntParameter;
	EOpOverloadBooleanConsumerContext Context = EOpOverloadBooleanConsumerContext::Assignment;
};

const TCHAR* ScenarioToken(const EOpOverloadBooleanConsumerScenario Scenario)
{
	switch (Scenario)
	{
	case EOpOverloadBooleanConsumerScenario::ComparisonIntParameter:
		return TEXT("COMPARISON_INT_PARAMETER");
	case EOpOverloadBooleanConsumerScenario::ComparisonValueParameter:
		return TEXT("COMPARISON_VALUE_PARAMETER");
	case EOpOverloadBooleanConsumerScenario::ComparisonAmbiguousInt8:
		return TEXT("COMPARISON_AMBIGUOUS_INT8");
	case EOpOverloadBooleanConsumerScenario::ComparisonMissingLess:
		return TEXT("COMPARISON_MISSING_LESS");
	default:
		return nullptr;
	}
}

const TCHAR* ContextToken(const EOpOverloadBooleanConsumerContext Context)
{
	switch (Context)
	{
	case EOpOverloadBooleanConsumerContext::Assignment:
		return TEXT("ASSIGNMENT");
	case EOpOverloadBooleanConsumerContext::Return:
		return TEXT("RETURN");
	case EOpOverloadBooleanConsumerContext::Condition:
		return TEXT("CONDITION");
	case EOpOverloadBooleanConsumerContext::OverloadArgument:
		return TEXT("OVERLOAD_ARGUMENT");
	case EOpOverloadBooleanConsumerContext::Chain:
		return TEXT("CHAIN");
	default:
		return nullptr;
	}
}

const TCHAR* TypeName(const EOpOverloadBooleanConsumerScenario Scenario)
{
	switch (Scenario)
	{
	case EOpOverloadBooleanConsumerScenario::ComparisonIntParameter:
		return TEXT("FOverloadedBooleanComparisonIntParameter");
	case EOpOverloadBooleanConsumerScenario::ComparisonValueParameter:
		return TEXT("FOverloadedBooleanComparisonValueParameter");
	case EOpOverloadBooleanConsumerScenario::ComparisonAmbiguousInt8:
		return TEXT("FOverloadedBooleanComparisonAmbiguousInt8");
	case EOpOverloadBooleanConsumerScenario::ComparisonMissingLess:
		return TEXT("FOverloadedBooleanComparisonMissingLess");
	default:
		return nullptr;
	}
}

int32 ScenarioMarker(const EOpOverloadBooleanConsumerScenario Scenario)
{
	switch (Scenario)
	{
	case EOpOverloadBooleanConsumerScenario::ComparisonIntParameter:
		return 107;
	case EOpOverloadBooleanConsumerScenario::ComparisonValueParameter:
		return 108;
	case EOpOverloadBooleanConsumerScenario::ComparisonAmbiguousInt8:
		return 202;
	case EOpOverloadBooleanConsumerScenario::ComparisonMissingLess:
		return 303;
	default:
		return 0;
	}
}

bool IsReject(const EOpOverloadBooleanConsumerScenario Scenario)
{
	return Scenario >= EOpOverloadBooleanConsumerScenario::ComparisonAmbiguousInt8;
}

bool IsValidParams(const FOpOverloadBooleanConsumerParams& Params)
{
	return ScenarioToken(Params.Scenario) != nullptr && ContextToken(Params.Context) != nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FBooleanCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-OVERLOAD-BOOLEAN-CONSUMER-%s-%s"),
		ScenarioToken(Cell.Scenario),
		ContextToken(Cell.Context));
}

FString MakeEntryName(const FBooleanCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpOverloadBooleanConsumerParams ParamsFromCell(const FBooleanCell& Cell)
{
	FOpOverloadBooleanConsumerParams Params;
	Params.Scenario = Cell.Scenario;
	Params.Context = Cell.Context;
	return Params;
}

FBooleanCell CellFromIndex(const int32 Index)
{
	FBooleanCell Cell;
	Cell.Scenario = static_cast<EOpOverloadBooleanConsumerScenario>(Index / ContextCount);
	Cell.Context = static_cast<EOpOverloadBooleanConsumerContext>(Index % ContextCount);
	return Cell;
}

bool TryFindCell(const FStringView CaseId, FBooleanCell& OutCell)
{
	const FString Needle(CaseId);
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FBooleanCell Cell = CellFromIndex(Index);
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FString ArgumentExpression(const EOpOverloadBooleanConsumerScenario Scenario)
{
	if (Scenario == EOpOverloadBooleanConsumerScenario::ComparisonValueParameter
		|| Scenario == EOpOverloadBooleanConsumerScenario::ComparisonMissingLess)
	{
		return TEXT("Right");
	}
	if (Scenario == EOpOverloadBooleanConsumerScenario::ComparisonAmbiguousInt8)
	{
		return TEXT("int8(6)");
	}
	return TEXT("6");
}

FString OperationExpression(const EOpOverloadBooleanConsumerScenario Scenario)
{
	if (Scenario == EOpOverloadBooleanConsumerScenario::ComparisonMissingLess)
	{
		return TEXT("Left < Right");
	}
	return FString::Printf(TEXT("Left == %s"), *ArgumentExpression(Scenario));
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

void AppendScenarioType(FString& Source, const EOpOverloadBooleanConsumerScenario Scenario)
{
	const TCHAR* const Name = TypeName(Scenario);
	AppendTypeHeader(Source, Name);
	if (Scenario == EOpOverloadBooleanConsumerScenario::ComparisonIntParameter)
	{
		AppendLine(Source, TEXT("\tbool opEquals(int Other) const"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn true;"));
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Scenario == EOpOverloadBooleanConsumerScenario::ComparisonValueParameter)
	{
		AppendLine(Source, FString::Printf(TEXT("\tbool opEquals(const %s& Other) const"), Name));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn true;"));
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Scenario == EOpOverloadBooleanConsumerScenario::ComparisonAmbiguousInt8)
	{
		AppendLine(Source, TEXT("\tbool opEquals(int64 Other) const"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn true;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\tbool opEquals(uint64 Other) const // OP_OVERLOAD_OPERATION_CAUSE"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn true;"));
		AppendLine(Source, TEXT("\t}"));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\tbool opEquals(const %s& Other) const"), Name));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn true;"));
		AppendLine(Source, TEXT("\t}"));
	}
	AppendLine(Source, TEXT("}"));
}

void AppendBoolHelpers(FString& Source)
{
	AppendLine(Source, TEXT("bool ReturnBoolMarker(bool Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("bool ConsumeBoolMarker(bool Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("bool ChainBoolMarker(bool Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
}

void AppendContext(
	FString& Source,
	const FOpOverloadBooleanConsumerParams& Params,
	const FString& Expression)
{
	const int32 Marker = ScenarioMarker(Params.Scenario);
	const TCHAR* const Cause = IsReject(Params.Scenario) ? TEXT(" // OP_OVERLOAD_OPERATION_CAUSE") : TEXT("");
	switch (Params.Context)
	{
	case EOpOverloadBooleanConsumerContext::Assignment:
		AppendLine(Source, FString::Printf(TEXT("\tbool Result = %s;%s"), *Expression, Cause));
		AppendLine(Source, FString::Printf(TEXT("\treturn Result ? %d : -1;"), Marker));
		break;
	case EOpOverloadBooleanConsumerContext::Return:
		AppendLine(Source, FString::Printf(
			TEXT("\treturn ReturnBoolMarker(%s) ? %d : -1;%s"),
			*Expression,
			Marker,
			Cause));
		break;
	case EOpOverloadBooleanConsumerContext::Condition:
		AppendLine(Source, FString::Printf(TEXT("\tif (%s)%s"), *Expression, Cause));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\treturn %d;"), Marker));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn -1;"));
		break;
	case EOpOverloadBooleanConsumerContext::OverloadArgument:
		AppendLine(Source, FString::Printf(
			TEXT("\treturn ConsumeBoolMarker(%s) ? %d : -1;%s"),
			*Expression,
			Marker,
			Cause));
		break;
	case EOpOverloadBooleanConsumerContext::Chain:
		AppendLine(Source, FString::Printf(
			TEXT("\treturn ChainBoolMarker(%s && true) ? %d : -1;%s"),
			*Expression,
			Marker,
			Cause));
		break;
	}
}

void AppendEntry(FString& Source, const FOpOverloadBooleanConsumerParams& Params, const FString& FunctionName)
{
	const TCHAR* const Name = TypeName(Params.Scenario);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Left(5);"), Name));
	AppendLine(Source, FString::Printf(TEXT("\t%s Right(6);"), Name));
	AppendContext(Source, Params, OperationExpression(Params.Scenario));
	AppendLine(Source, TEXT("}"));
}

void AppendOwnedModule(FString& Source, const FOpOverloadBooleanConsumerParams& Params, const FString& FunctionName)
{
	AppendScenarioType(Source, Params.Scenario);
	AppendLine(Source);
	AppendBoolHelpers(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
}

void AppendSuccessTypesAndHelpers(FString& Source)
{
	AppendScenarioType(Source, EOpOverloadBooleanConsumerScenario::ComparisonIntParameter);
	AppendLine(Source);
	AppendScenarioType(Source, EOpOverloadBooleanConsumerScenario::ComparisonValueParameter);
	AppendLine(Source);
	AppendBoolHelpers(Source);
}

FString BuildOwnedSource(const FOpOverloadBooleanConsumerParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOwnedModule(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FBooleanCell& Cell)
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
		Info.ExpectedReturn = ScenarioMarker(Cell.Scenario);
		Info.bLimitedObservation = true;
		Info.ExecutionNotes = LimitedNotes;
	}
	return Info;
}
}

FStringView FOpOverloadBooleanConsumerGenerator::GetProductId() const
{
	return TEXT("LANG-OP-OVERLOAD-BOOLEAN-CONSUMER");
}

TArray<FGeneratedCaseInfo> FOpOverloadBooleanConsumerGenerator::ListCases() const
{
	using namespace OpOverloadBooleanConsumerGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FBooleanCell Cell = CellFromIndex(Index);
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("OpOverloadBooleanConsumer PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FOpOverloadBooleanConsumerGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace OpOverloadBooleanConsumerGeneratorDetail;
	FBooleanCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FOpOverloadBooleanConsumerGenerator::BuildDumpSource() const
{
	using namespace OpOverloadBooleanConsumerGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-OVERLOAD-BOOLEAN-CONSUMER"));
	AppendLine(Source, TEXT("// Cases: 20 total, 10 normal, 10 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSuccessTypesAndHelpers(Source);
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FBooleanCell Cell = CellFromIndex(Index);
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
		const FBooleanCell Cell = CellFromIndex(Index);
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

FString FOpOverloadBooleanConsumerGenerator::BuildScenarioSource(const FOpOverloadBooleanConsumerParams& Params) const
{
	using namespace OpOverloadBooleanConsumerGeneratorDetail;
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

FString FOpOverloadBooleanConsumerGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace OpOverloadBooleanConsumerGeneratorDetail;
	FString Source;
	AppendSuccessTypesAndHelpers(Source);
	int32 Emitted = 0;
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FBooleanCell Cell = CellFromIndex(Index);
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

int32 FOpOverloadBooleanConsumerGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpOverloadBooleanConsumerGeneratorDetail;
	FBooleanCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Scenario))
	{
		return 0;
	}
	return ScenarioMarker(Cell.Scenario);
}

FString FOpOverloadBooleanConsumerGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace OpOverloadBooleanConsumerGeneratorDetail;
	FBooleanCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Scenario))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FOpOverloadBooleanConsumerGenerator::ListRejectCaseIds() const
{
	using namespace OpOverloadBooleanConsumerGeneratorDetail;
	TArray<FString> Ids;
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FBooleanCell Cell = CellFromIndex(Index);
		if (IsReject(Cell.Scenario))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
