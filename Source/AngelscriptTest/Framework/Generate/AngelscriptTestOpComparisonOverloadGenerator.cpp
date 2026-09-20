#include "Framework/Generate/AngelscriptTestOpComparisonOverloadGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace OpComparisonOverloadGeneratorDetail
{
constexpr const TCHAR* HostNotes =
	TEXT("Needs BeginOverloadedComparisonValue, EndOverloadedComparisonValue, RecordComparisonOverload.");

struct FOverloadCell
{
	EOpComparisonOverloadOperator Operator = EOpComparisonOverloadOperator::Less;
	EOpComparisonOverloadRelation Relation = EOpComparisonOverloadRelation::Less;
	EOpComparisonOverloadOrder Order = EOpComparisonOverloadOrder::LeftRight;
	EOpComparisonOverloadReceiver Receiver = EOpComparisonOverloadReceiver::Mutable;
};

const TCHAR* OperatorToken(const EOpComparisonOverloadOperator Operator)
{
	switch (Operator)
	{
	case EOpComparisonOverloadOperator::Less:
		return TEXT("LESS");
	case EOpComparisonOverloadOperator::LessEqual:
		return TEXT("LESS_EQUAL");
	case EOpComparisonOverloadOperator::Greater:
		return TEXT("GREATER");
	case EOpComparisonOverloadOperator::GreaterEqual:
		return TEXT("GREATER_EQUAL");
	case EOpComparisonOverloadOperator::Equal:
		return TEXT("EQUAL");
	case EOpComparisonOverloadOperator::NotEqual:
		return TEXT("NOT_EQUAL");
	default:
		return nullptr;
	}
}

const TCHAR* RelationToken(const EOpComparisonOverloadRelation Relation)
{
	switch (Relation)
	{
	case EOpComparisonOverloadRelation::Less:
		return TEXT("LESS");
	case EOpComparisonOverloadRelation::Equal:
		return TEXT("EQUAL");
	case EOpComparisonOverloadRelation::Greater:
		return TEXT("GREATER");
	case EOpComparisonOverloadRelation::Unequal:
		return TEXT("UNEQUAL");
	default:
		return nullptr;
	}
}

const TCHAR* OrderToken(const EOpComparisonOverloadOrder Order)
{
	switch (Order)
	{
	case EOpComparisonOverloadOrder::LeftRight:
		return TEXT("LEFT_RIGHT");
	case EOpComparisonOverloadOrder::RightLeft:
		return TEXT("RIGHT_LEFT");
	default:
		return nullptr;
	}
}

const TCHAR* ReceiverToken(const EOpComparisonOverloadReceiver Receiver)
{
	switch (Receiver)
	{
	case EOpComparisonOverloadReceiver::Mutable:
		return TEXT("MUTABLE");
	case EOpComparisonOverloadReceiver::Const:
		return TEXT("CONST");
	default:
		return nullptr;
	}
}

const TCHAR* OperatorSymbol(const EOpComparisonOverloadOperator Operator)
{
	switch (Operator)
	{
	case EOpComparisonOverloadOperator::Less:
		return TEXT("<");
	case EOpComparisonOverloadOperator::LessEqual:
		return TEXT("<=");
	case EOpComparisonOverloadOperator::Greater:
		return TEXT(">");
	case EOpComparisonOverloadOperator::GreaterEqual:
		return TEXT(">=");
	case EOpComparisonOverloadOperator::Equal:
		return TEXT("==");
	case EOpComparisonOverloadOperator::NotEqual:
		return TEXT("!=");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FOpComparisonOverloadParams& Params)
{
	return OperatorToken(Params.Operator) != nullptr
		&& RelationToken(Params.Relation) != nullptr
		&& OrderToken(Params.Order) != nullptr
		&& ReceiverToken(Params.Receiver) != nullptr;
}

void RelationValues(const EOpComparisonOverloadRelation Relation, int32& OutLeft, int32& OutRight)
{
	switch (Relation)
	{
	case EOpComparisonOverloadRelation::Less:
		OutLeft = 1;
		OutRight = 2;
		break;
	case EOpComparisonOverloadRelation::Equal:
		OutLeft = 2;
		OutRight = 2;
		break;
	case EOpComparisonOverloadRelation::Greater:
		OutLeft = 3;
		OutRight = 2;
		break;
	case EOpComparisonOverloadRelation::Unequal:
		OutLeft = -4;
		OutRight = 5;
		break;
	default:
		OutLeft = 0;
		OutRight = 0;
		break;
	}
}

bool ExpectedComparison(
	const int32 Left, const int32 Right, const EOpComparisonOverloadOperator Operator)
{
	switch (Operator)
	{
	case EOpComparisonOverloadOperator::Less:
		return Left < Right;
	case EOpComparisonOverloadOperator::LessEqual:
		return Left <= Right;
	case EOpComparisonOverloadOperator::Greater:
		return Left > Right;
	case EOpComparisonOverloadOperator::GreaterEqual:
		return Left >= Right;
	case EOpComparisonOverloadOperator::Equal:
		return Left == Right;
	case EOpComparisonOverloadOperator::NotEqual:
		return Left != Right;
	default:
		return false;
	}
}

int32 ExpectedFromCell(const FOverloadCell& Cell)
{
	int32 Left = 0;
	int32 Right = 0;
	RelationValues(Cell.Relation, Left, Right);
	if (Cell.Order == EOpComparisonOverloadOrder::RightLeft)
	{
		const int32 Swap = Left;
		Left = Right;
		Right = Swap;
	}
	return ExpectedComparison(Left, Right, Cell.Operator) ? 1 : 0;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FOverloadCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-COMPARISON-OVERLOAD-%s-%s-%s-%s"),
		OperatorToken(Cell.Operator),
		RelationToken(Cell.Relation),
		OrderToken(Cell.Order),
		ReceiverToken(Cell.Receiver));
}

FString MakeEntryName(const FOverloadCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpComparisonOverloadParams ParamsFromCell(const FOverloadCell& Cell)
{
	FOpComparisonOverloadParams Params;
	Params.Operator = Cell.Operator;
	Params.Relation = Cell.Relation;
	Params.Order = Cell.Order;
	Params.Receiver = Cell.Receiver;
	return Params;
}

TArray<FOverloadCell> AllCells()
{
	TArray<FOverloadCell> Cells;
	Cells.SetNum(96);
	for (int32 Index = 0; Index < 96; ++Index)
	{
		FOverloadCell& Cell = Cells[Index];
		Cell.Operator = static_cast<EOpComparisonOverloadOperator>(Index / 16);
		Cell.Relation = static_cast<EOpComparisonOverloadRelation>((Index / 4) % 4);
		Cell.Order = static_cast<EOpComparisonOverloadOrder>((Index / 2) % 2);
		Cell.Receiver = static_cast<EOpComparisonOverloadReceiver>(Index % 2);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FOverloadCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FOverloadCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (MakeCaseId(Cells[Index]) == Needle)
		{
			OutCell = Cells[Index];
			return true;
		}
	}
	return false;
}

void AppendSharedHelpers(FString& Source)
{
	AppendLine(Source, TEXT("struct FOverloadedComparisonValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value;"));
	AppendLine(Source, TEXT("\tint ObjectId;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFOverloadedComparisonValue(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t\tObjectId = BeginOverloadedComparisonValue(Value);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\t~FOverloadedComparisonValue()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tEndOverloadedComparisonValue(ObjectId, Value);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint opCmp(const FOverloadedComparisonValue& Other)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tRecordComparisonOverload(101, Value, Other.Value);"));
	AppendLine(Source, TEXT("\t\tif (Value < Other.Value)"));
	AppendLine(Source, TEXT("\t\t{"));
	AppendLine(Source, TEXT("\t\t\treturn -1;"));
	AppendLine(Source, TEXT("\t\t}"));
	AppendLine(Source, TEXT("\t\tif (Value > Other.Value)"));
	AppendLine(Source, TEXT("\t\t{"));
	AppendLine(Source, TEXT("\t\t\treturn 1;"));
	AppendLine(Source, TEXT("\t\t}"));
	AppendLine(Source, TEXT("\t\treturn 0;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint opCmp(const FOverloadedComparisonValue& Other) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tRecordComparisonOverload(102, Value, Other.Value);"));
	AppendLine(Source, TEXT("\t\tif (Value < Other.Value)"));
	AppendLine(Source, TEXT("\t\t{"));
	AppendLine(Source, TEXT("\t\t\treturn -1;"));
	AppendLine(Source, TEXT("\t\t}"));
	AppendLine(Source, TEXT("\t\tif (Value > Other.Value)"));
	AppendLine(Source, TEXT("\t\t{"));
	AppendLine(Source, TEXT("\t\t\treturn 1;"));
	AppendLine(Source, TEXT("\t\t}"));
	AppendLine(Source, TEXT("\t\treturn 0;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tbool opEquals(const FOverloadedComparisonValue& Other)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tRecordComparisonOverload(201, Value, Other.Value);"));
	AppendLine(Source, TEXT("\t\treturn Value == Other.Value;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tbool opEquals(const FOverloadedComparisonValue& Other) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tRecordComparisonOverload(202, Value, Other.Value);"));
	AppendLine(Source, TEXT("\t\treturn Value == Other.Value;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendEntry(FString& Source, const FOpComparisonOverloadParams& Params, const FString& FunctionName)
{
	int32 Left = 0;
	int32 Right = 0;
	RelationValues(Params.Relation, Left, Right);
	const bool bLeftConst = Params.Receiver == EOpComparisonOverloadReceiver::Const
		&& Params.Order == EOpComparisonOverloadOrder::LeftRight;
	const bool bRightConst = Params.Receiver == EOpComparisonOverloadReceiver::Const
		&& Params.Order == EOpComparisonOverloadOrder::RightLeft;
	const TCHAR* const Symbol = OperatorSymbol(Params.Operator);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(
		Source,
		FString::Printf(
			TEXT("\t%sFOverloadedComparisonValue Left(%d);"),
			bLeftConst ? TEXT("const ") : TEXT(""),
			Left));
	AppendLine(
		Source,
		FString::Printf(
			TEXT("\t%sFOverloadedComparisonValue Right(%d);"),
			bRightConst ? TEXT("const ") : TEXT(""),
			Right));
	if (Params.Order == EOpComparisonOverloadOrder::LeftRight)
	{
		AppendLine(Source, FString::Printf(TEXT("\tconst bool Result = Left %s Right;"), Symbol));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\tconst bool Result = Right %s Left;"), Symbol));
	}
	AppendLine(Source, TEXT("\treturn Result ? 1 : 0;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FOpComparisonOverloadParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FOverloadCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	Info.ExecutionNotes = HostNotes;
	return Info;
}
}

FStringView FOpComparisonOverloadGenerator::GetProductId() const
{
	return TEXT("LANG-OP-COMPARISON-OVERLOAD");
}

TArray<FGeneratedCaseInfo> FOpComparisonOverloadGenerator::ListCases() const
{
	using namespace OpComparisonOverloadGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FOverloadCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		const FString EntryName = MakeEntryName(Cells[Index]);
		checkf(!EntryNames.Contains(EntryName), TEXT("OpComparisonOverload PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FOpComparisonOverloadGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace OpComparisonOverloadGeneratorDetail;
	FOverloadCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FOpComparisonOverloadGenerator::BuildDumpSource() const
{
	using namespace OpComparisonOverloadGeneratorDetail;
	const TArray<FOverloadCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-COMPARISON-OVERLOAD"));
	AppendLine(Source, TEXT("// Cases: 96 total, 96 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FOverloadCell& Cell = Cells[Index];
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FOpComparisonOverloadGenerator::BuildOverloadedComparisonSource(
	const FOpComparisonOverloadParams& Params) const
{
	using namespace OpComparisonOverloadGeneratorDetail;
	if (!IsValidParams(Params))
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

FString FOpComparisonOverloadGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace OpComparisonOverloadGeneratorDetail;
	const TArray<FOverloadCell> Cells = AllCells();
	OutCaseCount = Cells.Num();
	FString Source;
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

int32 FOpComparisonOverloadGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpComparisonOverloadGeneratorDetail;
	FOverloadCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}
}
