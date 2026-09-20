#include "Framework/Generate/AngelscriptTestOpComparisonFloatGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace OpComparisonFloatGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs TraceFloat32Operand/TraceFloat64Operand host traces.");

enum class EFloatRelation : uint8
{
	Less,
	Equal,
	Greater,
	Unordered,
};

struct FFloatCell
{
	EOpComparisonFloatType Type = EOpComparisonFloatType::Float32;
	EOpComparisonFloatOperator Operator = EOpComparisonFloatOperator::Less;
	EOpComparisonFloatValue Value = EOpComparisonFloatValue::NegativeZero;
	EOpComparisonFloatOrder Order = EOpComparisonFloatOrder::LeftRight;
};

const TCHAR* TypeToken(const EOpComparisonFloatType Type)
{
	switch (Type)
	{
	case EOpComparisonFloatType::Float32:
		return TEXT("FLOAT32");
	case EOpComparisonFloatType::Float64:
		return TEXT("FLOAT64");
	default:
		return nullptr;
	}
}

const TCHAR* OperatorToken(const EOpComparisonFloatOperator Operator)
{
	switch (Operator)
	{
	case EOpComparisonFloatOperator::Less:
		return TEXT("LESS");
	case EOpComparisonFloatOperator::LessEqual:
		return TEXT("LESS_EQUAL");
	case EOpComparisonFloatOperator::Greater:
		return TEXT("GREATER");
	case EOpComparisonFloatOperator::GreaterEqual:
		return TEXT("GREATER_EQUAL");
	case EOpComparisonFloatOperator::Equal:
		return TEXT("EQUAL");
	case EOpComparisonFloatOperator::NotEqual:
		return TEXT("NOT_EQUAL");
	default:
		return nullptr;
	}
}

const TCHAR* ValueToken(const EOpComparisonFloatValue Value)
{
	switch (Value)
	{
	case EOpComparisonFloatValue::NegativeZero:
		return TEXT("NEGATIVE_ZERO");
	case EOpComparisonFloatValue::PositiveZero:
		return TEXT("POSITIVE_ZERO");
	case EOpComparisonFloatValue::NaN:
		return TEXT("NAN");
	case EOpComparisonFloatValue::PositiveInfinity:
		return TEXT("POSITIVE_INFINITY");
	case EOpComparisonFloatValue::NegativeInfinity:
		return TEXT("NEGATIVE_INFINITY");
	case EOpComparisonFloatValue::Minimum:
		return TEXT("MINIMUM");
	case EOpComparisonFloatValue::Maximum:
		return TEXT("MAXIMUM");
	case EOpComparisonFloatValue::EqualPair:
		return TEXT("EQUAL_PAIR");
	default:
		return nullptr;
	}
}

const TCHAR* OrderToken(const EOpComparisonFloatOrder Order)
{
	switch (Order)
	{
	case EOpComparisonFloatOrder::LeftRight:
		return TEXT("LEFT_RIGHT");
	case EOpComparisonFloatOrder::RightLeft:
		return TEXT("RIGHT_LEFT");
	default:
		return nullptr;
	}
}

const TCHAR* OperatorSymbol(const EOpComparisonFloatOperator Operator)
{
	switch (Operator)
	{
	case EOpComparisonFloatOperator::Less:
		return TEXT("<");
	case EOpComparisonFloatOperator::LessEqual:
		return TEXT("<=");
	case EOpComparisonFloatOperator::Greater:
		return TEXT(">");
	case EOpComparisonFloatOperator::GreaterEqual:
		return TEXT(">=");
	case EOpComparisonFloatOperator::Equal:
		return TEXT("==");
	case EOpComparisonFloatOperator::NotEqual:
		return TEXT("!=");
	default:
		return nullptr;
	}
}

const TCHAR* ScriptType(const EOpComparisonFloatType Type)
{
	return Type == EOpComparisonFloatType::Float32 ? TEXT("float32") : TEXT("float64");
}

const TCHAR* TraceName(const EOpComparisonFloatType Type)
{
	return Type == EOpComparisonFloatType::Float32 ? TEXT("TraceFloat32Operand") : TEXT("TraceFloat64Operand");
}

bool IsValidParams(const FOpComparisonFloatParams& Params)
{
	return TypeToken(Params.Type) != nullptr
		&& OperatorToken(Params.Operator) != nullptr
		&& ValueToken(Params.Value) != nullptr
		&& OrderToken(Params.Order) != nullptr;
}

EFloatRelation RelationFromValue(const EOpComparisonFloatValue Value)
{
	switch (Value)
	{
	case EOpComparisonFloatValue::NegativeZero:
	case EOpComparisonFloatValue::PositiveZero:
	case EOpComparisonFloatValue::EqualPair:
		return EFloatRelation::Equal;
	case EOpComparisonFloatValue::NaN:
		return EFloatRelation::Unordered;
	case EOpComparisonFloatValue::PositiveInfinity:
	case EOpComparisonFloatValue::Maximum:
		return EFloatRelation::Greater;
	case EOpComparisonFloatValue::NegativeInfinity:
	case EOpComparisonFloatValue::Minimum:
		return EFloatRelation::Less;
	default:
		return EFloatRelation::Equal;
	}
}

EFloatRelation SwapRelation(const EFloatRelation Relation)
{
	if (Relation == EFloatRelation::Less)
	{
		return EFloatRelation::Greater;
	}
	if (Relation == EFloatRelation::Greater)
	{
		return EFloatRelation::Less;
	}
	return Relation;
}

bool ExpectedComparison(const EFloatRelation Relation, const EOpComparisonFloatOperator Operator)
{
	if (Relation == EFloatRelation::Unordered)
	{
		return Operator == EOpComparisonFloatOperator::Greater
			|| Operator == EOpComparisonFloatOperator::GreaterEqual
			|| Operator == EOpComparisonFloatOperator::NotEqual;
	}
	switch (Operator)
	{
	case EOpComparisonFloatOperator::Less:
		return Relation == EFloatRelation::Less;
	case EOpComparisonFloatOperator::LessEqual:
		return Relation == EFloatRelation::Less || Relation == EFloatRelation::Equal;
	case EOpComparisonFloatOperator::Greater:
		return Relation == EFloatRelation::Greater;
	case EOpComparisonFloatOperator::GreaterEqual:
		return Relation == EFloatRelation::Greater || Relation == EFloatRelation::Equal;
	case EOpComparisonFloatOperator::Equal:
		return Relation == EFloatRelation::Equal;
	case EOpComparisonFloatOperator::NotEqual:
		return Relation != EFloatRelation::Equal;
	default:
		return false;
	}
}

int32 ExpectedFromCell(const FFloatCell& Cell)
{
	EFloatRelation Relation = RelationFromValue(Cell.Value);
	if (Cell.Order == EOpComparisonFloatOrder::RightLeft)
	{
		Relation = SwapRelation(Relation);
	}
	return ExpectedComparison(Relation, Cell.Operator) ? 1 : 0;
}

void ValueLiterals(
	const EOpComparisonFloatType Type,
	const EOpComparisonFloatValue Value,
	FString& OutLeft,
	FString& OutRight)
{
	const bool b32 = Type == EOpComparisonFloatType::Float32;
	const TCHAR* const Zero = b32 ? TEXT("0.0f") : TEXT("0.0");
	const TCHAR* const One = b32 ? TEXT("1.0f") : TEXT("1.0");
	const TCHAR* const NegOne = b32 ? TEXT("-1.0f") : TEXT("-1.0");
	const TCHAR* const Max = b32 ? TEXT("3.402823466e+38f") : TEXT("1.7976931348623157e+308");
	const TCHAR* const Min = b32 ? TEXT("-3.402823466e+38f") : TEXT("-1.7976931348623157e+308");
	const TCHAR* const Pair = b32 ? TEXT("13.25f") : TEXT("13.25");
	switch (Value)
	{
	case EOpComparisonFloatValue::NegativeZero:
		OutLeft = b32 ? TEXT("-0.0f") : TEXT("-0.0");
		OutRight = Zero;
		break;
	case EOpComparisonFloatValue::PositiveZero:
		OutLeft = Zero;
		OutRight = b32 ? TEXT("-0.0f") : TEXT("-0.0");
		break;
	case EOpComparisonFloatValue::NaN:
		OutLeft = b32 ? TEXT("0.0f / 0.0f") : TEXT("0.0 / 0.0");
		OutRight = One;
		break;
	case EOpComparisonFloatValue::PositiveInfinity:
		OutLeft = b32 ? TEXT("1.0f / 0.0f") : TEXT("1.0 / 0.0");
		OutRight = Max;
		break;
	case EOpComparisonFloatValue::NegativeInfinity:
		OutLeft = b32 ? TEXT("-1.0f / 0.0f") : TEXT("-1.0 / 0.0");
		OutRight = Min;
		break;
	case EOpComparisonFloatValue::Minimum:
		OutLeft = Min;
		OutRight = NegOne;
		break;
	case EOpComparisonFloatValue::Maximum:
		OutLeft = Max;
		OutRight = One;
		break;
	case EOpComparisonFloatValue::EqualPair:
		OutLeft = Pair;
		OutRight = Pair;
		break;
	default:
		OutLeft = Zero;
		OutRight = Zero;
		break;
	}
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FFloatCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-COMPARISON-FLOAT-%s-%s-%s-%s"),
		TypeToken(Cell.Type),
		OperatorToken(Cell.Operator),
		ValueToken(Cell.Value),
		OrderToken(Cell.Order));
}

FString MakeEntryName(const FFloatCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpComparisonFloatParams ParamsFromCell(const FFloatCell& Cell)
{
	FOpComparisonFloatParams Params;
	Params.Type = Cell.Type;
	Params.Operator = Cell.Operator;
	Params.Value = Cell.Value;
	Params.Order = Cell.Order;
	return Params;
}

TArray<FFloatCell> AllCells()
{
	TArray<FFloatCell> Cells;
	Cells.SetNum(192);
	for (int32 Index = 0; Index < 192; ++Index)
	{
		FFloatCell& Cell = Cells[Index];
		Cell.Type = static_cast<EOpComparisonFloatType>(Index / 96);
		Cell.Operator = static_cast<EOpComparisonFloatOperator>((Index / 16) % 6);
		Cell.Value = static_cast<EOpComparisonFloatValue>((Index / 2) % 8);
		Cell.Order = static_cast<EOpComparisonFloatOrder>(Index % 2);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FFloatCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FFloatCell> Cells = AllCells();
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

void AppendEntry(FString& Source, const FOpComparisonFloatParams& Params, const FString& FunctionName)
{
	FString LeftLiteral;
	FString RightLiteral;
	ValueLiterals(Params.Type, Params.Value, LeftLiteral, RightLiteral);
	const TCHAR* const TypeName = ScriptType(Params.Type);
	const TCHAR* const Trace = TraceName(Params.Type);
	const TCHAR* const Symbol = OperatorSymbol(Params.Operator);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Left = %s;"), TypeName, *LeftLiteral));
	AppendLine(Source, FString::Printf(TEXT("\t%s Right = %s;"), TypeName, *RightLiteral));
	if (Params.Order == EOpComparisonFloatOrder::LeftRight)
	{
		AppendLine(
			Source,
			FString::Printf(TEXT("\tconst bool Result = %s(1, Left) %s %s(2, Right);"), Trace, Symbol, Trace));
	}
	else
	{
		AppendLine(
			Source,
			FString::Printf(TEXT("\tconst bool Result = %s(2, Right) %s %s(1, Left);"), Trace, Symbol, Trace));
	}
	AppendLine(Source, TEXT("\treturn Result ? 1 : 0;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FOpComparisonFloatParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FFloatCell& Cell)
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

FStringView FOpComparisonFloatGenerator::GetProductId() const
{
	return TEXT("LANG-OP-COMPARISON-FLOAT");
}

TArray<FGeneratedCaseInfo> FOpComparisonFloatGenerator::ListCases() const
{
	using namespace OpComparisonFloatGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FFloatCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		const FString EntryName = MakeEntryName(Cells[Index]);
		checkf(!EntryNames.Contains(EntryName), TEXT("OpComparisonFloat PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FOpComparisonFloatGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace OpComparisonFloatGeneratorDetail;
	FFloatCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FOpComparisonFloatGenerator::BuildDumpSource() const
{
	using namespace OpComparisonFloatGeneratorDetail;
	const TArray<FFloatCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-COMPARISON-FLOAT"));
	AppendLine(Source, TEXT("// Cases: 192 total, 192 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FFloatCell& Cell = Cells[Index];
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

FString FOpComparisonFloatGenerator::BuildFloatComparisonSource(const FOpComparisonFloatParams& Params) const
{
	using namespace OpComparisonFloatGeneratorDetail;
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

FString FOpComparisonFloatGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace OpComparisonFloatGeneratorDetail;
	const TArray<FFloatCell> Cells = AllCells();
	OutCaseCount = Cells.Num();
	FString Source;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (Index > 0)
		{
			AppendLine(Source);
		}
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

int32 FOpComparisonFloatGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpComparisonFloatGeneratorDetail;
	FFloatCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}
}
