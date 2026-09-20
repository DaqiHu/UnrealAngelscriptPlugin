#include "Framework/Generate/AngelscriptTestOpOverloadAssignmentConsumerGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace OpOverloadAssignmentConsumerGeneratorDetail
{
constexpr int32 ScenarioCount = 4;
constexpr int32 ContextCount = 6;
constexpr int32 CellCount = ScenarioCount * ContextCount;
constexpr const TCHAR* LimitedNotes = TEXT("int32 Marker is a scenario identity, not full-width arithmetic proof.");

struct FAssignmentCell
{
	EOpOverloadAssignmentConsumerScenario Scenario = EOpOverloadAssignmentConsumerScenario::AssignmentIntParameter;
	EOpOverloadAssignmentConsumerContext Context = EOpOverloadAssignmentConsumerContext::Assignment;
};

const TCHAR* ScenarioToken(const EOpOverloadAssignmentConsumerScenario Scenario)
{
	switch (Scenario)
	{
	case EOpOverloadAssignmentConsumerScenario::AssignmentIntParameter:
		return TEXT("ASSIGNMENT_INT_PARAMETER");
	case EOpOverloadAssignmentConsumerScenario::AssignmentValueParameter:
		return TEXT("ASSIGNMENT_VALUE_PARAMETER");
	case EOpOverloadAssignmentConsumerScenario::AssignmentAmbiguousInt8:
		return TEXT("ASSIGNMENT_AMBIGUOUS_INT8");
	case EOpOverloadAssignmentConsumerScenario::AssignmentMissingAddAssign:
		return TEXT("ASSIGNMENT_MISSING_ADD_ASSIGN");
	default:
		return nullptr;
	}
}

const TCHAR* ContextToken(const EOpOverloadAssignmentConsumerContext Context)
{
	switch (Context)
	{
	case EOpOverloadAssignmentConsumerContext::Assignment:
		return TEXT("ASSIGNMENT");
	case EOpOverloadAssignmentConsumerContext::Return:
		return TEXT("RETURN");
	case EOpOverloadAssignmentConsumerContext::Condition:
		return TEXT("CONDITION");
	case EOpOverloadAssignmentConsumerContext::OverloadArgument:
		return TEXT("OVERLOAD_ARGUMENT");
	case EOpOverloadAssignmentConsumerContext::Chain:
		return TEXT("CHAIN");
	case EOpOverloadAssignmentConsumerContext::SwitchIndex:
		return TEXT("SWITCH_INDEX");
	default:
		return nullptr;
	}
}

const TCHAR* TypeName(const EOpOverloadAssignmentConsumerScenario Scenario)
{
	switch (Scenario)
	{
	case EOpOverloadAssignmentConsumerScenario::AssignmentIntParameter:
		return TEXT("FOverloadedAssignmentIntParameter");
	case EOpOverloadAssignmentConsumerScenario::AssignmentValueParameter:
		return TEXT("FOverloadedAssignmentValueParameter");
	case EOpOverloadAssignmentConsumerScenario::AssignmentAmbiguousInt8:
		return TEXT("FOverloadedAssignmentAmbiguousInt8");
	case EOpOverloadAssignmentConsumerScenario::AssignmentMissingAddAssign:
		return TEXT("FOverloadedAssignmentMissingAddAssign");
	default:
		return nullptr;
	}
}

int32 ScenarioMarker(const EOpOverloadAssignmentConsumerScenario Scenario)
{
	switch (Scenario)
	{
	case EOpOverloadAssignmentConsumerScenario::AssignmentIntParameter:
		return 114;
	case EOpOverloadAssignmentConsumerScenario::AssignmentValueParameter:
		return 115;
	case EOpOverloadAssignmentConsumerScenario::AssignmentAmbiguousInt8:
		return 206;
	case EOpOverloadAssignmentConsumerScenario::AssignmentMissingAddAssign:
		return 307;
	default:
		return 0;
	}
}

bool IsReject(const EOpOverloadAssignmentConsumerScenario Scenario)
{
	return Scenario >= EOpOverloadAssignmentConsumerScenario::AssignmentAmbiguousInt8;
}

bool IsValidParams(const FOpOverloadAssignmentConsumerParams& Params)
{
	return ScenarioToken(Params.Scenario) != nullptr && ContextToken(Params.Context) != nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FAssignmentCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-OVERLOAD-ASSIGNMENT-CONSUMER-%s-%s"),
		ScenarioToken(Cell.Scenario),
		ContextToken(Cell.Context));
}

FString MakeEntryName(const FAssignmentCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpOverloadAssignmentConsumerParams ParamsFromCell(const FAssignmentCell& Cell)
{
	FOpOverloadAssignmentConsumerParams Params;
	Params.Scenario = Cell.Scenario;
	Params.Context = Cell.Context;
	return Params;
}

FAssignmentCell CellFromIndex(const int32 Index)
{
	FAssignmentCell Cell;
	Cell.Scenario = static_cast<EOpOverloadAssignmentConsumerScenario>(Index / ContextCount);
	Cell.Context = static_cast<EOpOverloadAssignmentConsumerContext>(Index % ContextCount);
	return Cell;
}

bool TryFindCell(const FStringView CaseId, FAssignmentCell& OutCell)
{
	const FString Needle(CaseId);
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FAssignmentCell Cell = CellFromIndex(Index);
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FString ArgumentExpression(const EOpOverloadAssignmentConsumerScenario Scenario)
{
	if (Scenario == EOpOverloadAssignmentConsumerScenario::AssignmentValueParameter
		|| Scenario == EOpOverloadAssignmentConsumerScenario::AssignmentMissingAddAssign)
	{
		return TEXT("Right");
	}
	if (Scenario == EOpOverloadAssignmentConsumerScenario::AssignmentAmbiguousInt8)
	{
		return TEXT("int8(6)");
	}
	return TEXT("6");
}

FString OperationExpression(const EOpOverloadAssignmentConsumerScenario Scenario)
{
	if (Scenario == EOpOverloadAssignmentConsumerScenario::AssignmentMissingAddAssign)
	{
		return TEXT("Left += Right");
	}
	return FString::Printf(TEXT("Left = %s"), *ArgumentExpression(Scenario));
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

void AppendScenarioType(FString& Source, const EOpOverloadAssignmentConsumerScenario Scenario)
{
	const TCHAR* const Name = TypeName(Scenario);
	AppendTypeHeader(Source, Name);
	const int32 Marker = ScenarioMarker(Scenario);
	if (Scenario == EOpOverloadAssignmentConsumerScenario::AssignmentIntParameter)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s& opAssign(int Other)"), Name));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\tValue = %d;"), Marker));
		AppendLine(Source, TEXT("\t\treturn this;"));
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Scenario == EOpOverloadAssignmentConsumerScenario::AssignmentValueParameter)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s& opAssign(const %s& Other)"), Name, Name));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\tValue = %d;"), Marker));
		AppendLine(Source, TEXT("\t\treturn this;"));
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Scenario == EOpOverloadAssignmentConsumerScenario::AssignmentAmbiguousInt8)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s& opAssign(int64 Other)"), Name));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\tValue = %d;"), Marker));
		AppendLine(Source, TEXT("\t\treturn this;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\t%s& opAssign(uint64 Other) // OP_OVERLOAD_OPERATION_CAUSE"), Name));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\tValue = %d;"), Marker + 1));
		AppendLine(Source, TEXT("\t\treturn this;"));
		AppendLine(Source, TEXT("\t}"));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s& opAssign(const %s& Other)"), Name, Name));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\tValue = %d;"), Marker));
		AppendLine(Source, TEXT("\t\treturn this;"));
		AppendLine(Source, TEXT("\t}"));
	}
	AppendLine(Source, TEXT("}"));
}

void AppendValueHelpers(FString& Source, const EOpOverloadAssignmentConsumerScenario Scenario)
{
	const TCHAR* const Name = TypeName(Scenario);
	AppendLine(Source, FString::Printf(TEXT("int ReturnValueMarker(const %s& Value)"), Name));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value.Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("int ConsumeValueMarker(const %s& Value)"), Name));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value.Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("%s ChainValueMarker(const %s& Value)"), Name, Name));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
}

void AppendContext(
	FString& Source,
	const FOpOverloadAssignmentConsumerParams& Params,
	const FString& Expression)
{
	const int32 Marker = ScenarioMarker(Params.Scenario);
	const TCHAR* const Cause = IsReject(Params.Scenario) ? TEXT(" // OP_OVERLOAD_OPERATION_CAUSE") : TEXT("");
	switch (Params.Context)
	{
	case EOpOverloadAssignmentConsumerContext::Assignment:
		AppendLine(Source, FString::Printf(
			TEXT("\t%s Result = %s;%s"),
			TypeName(Params.Scenario),
			*Expression,
			Cause));
		AppendLine(Source, TEXT("\treturn Result.Value;"));
		break;
	case EOpOverloadAssignmentConsumerContext::Return:
		AppendLine(Source, FString::Printf(TEXT("\treturn ReturnValueMarker(%s);%s"), *Expression, Cause));
		break;
	case EOpOverloadAssignmentConsumerContext::Condition:
		AppendLine(Source, FString::Printf(TEXT("\tif ((%s).Value == %d)%s"), *Expression, Marker, Cause));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\treturn %d;"), Marker));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn -1;"));
		break;
	case EOpOverloadAssignmentConsumerContext::OverloadArgument:
		AppendLine(Source, FString::Printf(TEXT("\treturn ConsumeValueMarker(%s);%s"), *Expression, Cause));
		break;
	case EOpOverloadAssignmentConsumerContext::Chain:
		AppendLine(Source, FString::Printf(TEXT("\treturn ChainValueMarker(%s).Value;%s"), *Expression, Cause));
		break;
	case EOpOverloadAssignmentConsumerContext::SwitchIndex:
		AppendLine(Source, FString::Printf(TEXT("\tswitch ((%s).Value)%s"), *Expression, Cause));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\tcase %d:"), Marker));
		AppendLine(Source, FString::Printf(TEXT("\t\treturn %d;"), Marker));
		AppendLine(Source, TEXT("\tdefault:"));
		AppendLine(Source, TEXT("\t\treturn -1;"));
		AppendLine(Source, TEXT("\t}"));
		break;
	}
}

void AppendEntry(FString& Source, const FOpOverloadAssignmentConsumerParams& Params, const FString& FunctionName)
{
	const TCHAR* const Name = TypeName(Params.Scenario);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Left(5);"), Name));
	AppendLine(Source, FString::Printf(TEXT("\t%s Right(6);"), Name));
	if (!IsReject(Params.Scenario))
	{
		AppendLine(Source, FString::Printf(TEXT("\tLeft = %s;"), *ArgumentExpression(Params.Scenario)));
		AppendContext(Source, Params, TEXT("Left"));
	}
	else
	{
		AppendContext(Source, Params, OperationExpression(Params.Scenario));
	}
	AppendLine(Source, TEXT("}"));
}

void AppendOwnedModule(FString& Source, const FOpOverloadAssignmentConsumerParams& Params, const FString& FunctionName)
{
	AppendScenarioType(Source, Params.Scenario);
	AppendLine(Source);
	AppendValueHelpers(Source, Params.Scenario);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
}

void AppendSuccessTypesAndHelpers(FString& Source)
{
	AppendScenarioType(Source, EOpOverloadAssignmentConsumerScenario::AssignmentIntParameter);
	AppendLine(Source);
	AppendScenarioType(Source, EOpOverloadAssignmentConsumerScenario::AssignmentValueParameter);
	AppendLine(Source);
	AppendValueHelpers(Source, EOpOverloadAssignmentConsumerScenario::AssignmentIntParameter);
	AppendLine(Source);
	AppendValueHelpers(Source, EOpOverloadAssignmentConsumerScenario::AssignmentValueParameter);
}

FString BuildOwnedSource(const FOpOverloadAssignmentConsumerParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOwnedModule(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FAssignmentCell& Cell)
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

FStringView FOpOverloadAssignmentConsumerGenerator::GetProductId() const
{
	return TEXT("LANG-OP-OVERLOAD-ASSIGNMENT-CONSUMER");
}

TArray<FGeneratedCaseInfo> FOpOverloadAssignmentConsumerGenerator::ListCases() const
{
	using namespace OpOverloadAssignmentConsumerGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FAssignmentCell Cell = CellFromIndex(Index);
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("OpOverloadAssignmentConsumer PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FOpOverloadAssignmentConsumerGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace OpOverloadAssignmentConsumerGeneratorDetail;
	FAssignmentCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FOpOverloadAssignmentConsumerGenerator::BuildDumpSource() const
{
	using namespace OpOverloadAssignmentConsumerGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-OVERLOAD-ASSIGNMENT-CONSUMER"));
	AppendLine(Source, TEXT("// Cases: 24 total, 12 normal, 12 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSuccessTypesAndHelpers(Source);
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FAssignmentCell Cell = CellFromIndex(Index);
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
		const FAssignmentCell Cell = CellFromIndex(Index);
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

FString FOpOverloadAssignmentConsumerGenerator::BuildScenarioSource(const FOpOverloadAssignmentConsumerParams& Params) const
{
	using namespace OpOverloadAssignmentConsumerGeneratorDetail;
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

FString FOpOverloadAssignmentConsumerGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace OpOverloadAssignmentConsumerGeneratorDetail;
	FString Source;
	AppendSuccessTypesAndHelpers(Source);
	int32 Emitted = 0;
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FAssignmentCell Cell = CellFromIndex(Index);
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

int32 FOpOverloadAssignmentConsumerGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpOverloadAssignmentConsumerGeneratorDetail;
	FAssignmentCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Scenario))
	{
		return 0;
	}
	return ScenarioMarker(Cell.Scenario);
}

FString FOpOverloadAssignmentConsumerGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace OpOverloadAssignmentConsumerGeneratorDetail;
	FAssignmentCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Scenario))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FOpOverloadAssignmentConsumerGenerator::ListRejectCaseIds() const
{
	using namespace OpOverloadAssignmentConsumerGeneratorDetail;
	TArray<FString> Ids;
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FAssignmentCell Cell = CellFromIndex(Index);
		if (IsReject(Cell.Scenario))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
