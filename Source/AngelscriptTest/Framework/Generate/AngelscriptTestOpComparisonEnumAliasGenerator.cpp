#include "Framework/Generate/AngelscriptTestOpComparisonEnumAliasGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace OpComparisonEnumAliasGeneratorDetail
{
constexpr const TCHAR* HostNotes =
	TEXT("Needs RecordComparisonIntegerOperand and ComparisonAlias host typedef.");

struct FEnumAliasCell
{
	EOpComparisonEnumAliasFamily Family = EOpComparisonEnumAliasFamily::Enum;
	EOpComparisonEnumAliasOperator Operator = EOpComparisonEnumAliasOperator::Less;
	EOpComparisonEnumAliasPair Pair = EOpComparisonEnumAliasPair::EqualZero;
	EOpComparisonEnumAliasOrder Order = EOpComparisonEnumAliasOrder::LeftRight;
};

const TCHAR* FamilyToken(const EOpComparisonEnumAliasFamily Family)
{
	switch (Family)
	{
	case EOpComparisonEnumAliasFamily::Enum:
		return TEXT("ENUM");
	case EOpComparisonEnumAliasFamily::Alias:
		return TEXT("ALIAS");
	default:
		return nullptr;
	}
}

const TCHAR* OperatorToken(const EOpComparisonEnumAliasOperator Operator)
{
	switch (Operator)
	{
	case EOpComparisonEnumAliasOperator::Less:
		return TEXT("LESS");
	case EOpComparisonEnumAliasOperator::LessEqual:
		return TEXT("LESS_EQUAL");
	case EOpComparisonEnumAliasOperator::Greater:
		return TEXT("GREATER");
	case EOpComparisonEnumAliasOperator::GreaterEqual:
		return TEXT("GREATER_EQUAL");
	case EOpComparisonEnumAliasOperator::Equal:
		return TEXT("EQUAL");
	case EOpComparisonEnumAliasOperator::NotEqual:
		return TEXT("NOT_EQUAL");
	default:
		return nullptr;
	}
}

const TCHAR* PairToken(const EOpComparisonEnumAliasPair Pair)
{
	switch (Pair)
	{
	case EOpComparisonEnumAliasPair::EqualZero:
		return TEXT("EQUAL_ZERO");
	case EOpComparisonEnumAliasPair::EqualNamed:
		return TEXT("EQUAL_NAMED");
	case EOpComparisonEnumAliasPair::BelowAdjacent:
		return TEXT("BELOW_ADJACENT");
	case EOpComparisonEnumAliasPair::AboveAdjacent:
		return TEXT("ABOVE_ADJACENT");
	case EOpComparisonEnumAliasPair::MinimumBoundary:
		return TEXT("MINIMUM_BOUNDARY");
	case EOpComparisonEnumAliasPair::MaximumBoundary:
		return TEXT("MAXIMUM_BOUNDARY");
	default:
		return nullptr;
	}
}

const TCHAR* OrderToken(const EOpComparisonEnumAliasOrder Order)
{
	switch (Order)
	{
	case EOpComparisonEnumAliasOrder::LeftRight:
		return TEXT("LEFT_RIGHT");
	case EOpComparisonEnumAliasOrder::RightLeft:
		return TEXT("RIGHT_LEFT");
	default:
		return nullptr;
	}
}

const TCHAR* OperatorSymbol(const EOpComparisonEnumAliasOperator Operator)
{
	switch (Operator)
	{
	case EOpComparisonEnumAliasOperator::Less:
		return TEXT("<");
	case EOpComparisonEnumAliasOperator::LessEqual:
		return TEXT("<=");
	case EOpComparisonEnumAliasOperator::Greater:
		return TEXT(">");
	case EOpComparisonEnumAliasOperator::GreaterEqual:
		return TEXT(">=");
	case EOpComparisonEnumAliasOperator::Equal:
		return TEXT("==");
	case EOpComparisonEnumAliasOperator::NotEqual:
		return TEXT("!=");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FOpComparisonEnumAliasParams& Params)
{
	return FamilyToken(Params.Family) != nullptr
		&& OperatorToken(Params.Operator) != nullptr
		&& PairToken(Params.Pair) != nullptr
		&& OrderToken(Params.Order) != nullptr;
}

void IntegralValues(const EOpComparisonEnumAliasPair Pair, int32& OutLeft, int32& OutRight)
{
	switch (Pair)
	{
	case EOpComparisonEnumAliasPair::EqualZero:
		OutLeft = 0;
		OutRight = 0;
		break;
	case EOpComparisonEnumAliasPair::EqualNamed:
		OutLeft = 1;
		OutRight = 1;
		break;
	case EOpComparisonEnumAliasPair::BelowAdjacent:
		OutLeft = -1;
		OutRight = 0;
		break;
	case EOpComparisonEnumAliasPair::AboveAdjacent:
		OutLeft = 1;
		OutRight = 0;
		break;
	case EOpComparisonEnumAliasPair::MinimumBoundary:
		OutLeft = -128;
		OutRight = -1;
		break;
	case EOpComparisonEnumAliasPair::MaximumBoundary:
		OutLeft = 127;
		OutRight = 1;
		break;
	default:
		OutLeft = 0;
		OutRight = 0;
		break;
	}
}

bool ExpectedComparison(
	const int32 Left, const int32 Right, const EOpComparisonEnumAliasOperator Operator)
{
	switch (Operator)
	{
	case EOpComparisonEnumAliasOperator::Less:
		return Left < Right;
	case EOpComparisonEnumAliasOperator::LessEqual:
		return Left <= Right;
	case EOpComparisonEnumAliasOperator::Greater:
		return Left > Right;
	case EOpComparisonEnumAliasOperator::GreaterEqual:
		return Left >= Right;
	case EOpComparisonEnumAliasOperator::Equal:
		return Left == Right;
	case EOpComparisonEnumAliasOperator::NotEqual:
		return Left != Right;
	default:
		return false;
	}
}

int32 ExpectedFromCell(const FEnumAliasCell& Cell)
{
	int32 Left = 0;
	int32 Right = 0;
	IntegralValues(Cell.Pair, Left, Right);
	if (Cell.Order == EOpComparisonEnumAliasOrder::RightLeft)
	{
		const int32 Swap = Left;
		Left = Right;
		Right = Swap;
	}
	return ExpectedComparison(Left, Right, Cell.Operator) ? 1 : 0;
}

const TCHAR* EnumLiteral(const int32 Value)
{
	switch (Value)
	{
	case -128:
		return TEXT("EComparisonEnum::Minimum");
	case -1:
		return TEXT("EComparisonEnum::MinusOne");
	case 0:
		return TEXT("EComparisonEnum::Zero");
	case 1:
		return TEXT("EComparisonEnum::One");
	case 127:
		return TEXT("EComparisonEnum::Maximum");
	default:
		return TEXT("EComparisonEnum::Zero");
	}
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FEnumAliasCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-COMPARISON-ENUM-ALIAS-%s-%s-%s-%s"),
		FamilyToken(Cell.Family),
		OperatorToken(Cell.Operator),
		PairToken(Cell.Pair),
		OrderToken(Cell.Order));
}

FString MakeEntryName(const FEnumAliasCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpComparisonEnumAliasParams ParamsFromCell(const FEnumAliasCell& Cell)
{
	FOpComparisonEnumAliasParams Params;
	Params.Family = Cell.Family;
	Params.Operator = Cell.Operator;
	Params.Pair = Cell.Pair;
	Params.Order = Cell.Order;
	return Params;
}

TArray<FEnumAliasCell> AllCells()
{
	TArray<FEnumAliasCell> Cells;
	Cells.SetNum(144);
	for (int32 Index = 0; Index < 144; ++Index)
	{
		FEnumAliasCell& Cell = Cells[Index];
		Cell.Family = static_cast<EOpComparisonEnumAliasFamily>(Index / 72);
		Cell.Operator = static_cast<EOpComparisonEnumAliasOperator>((Index / 12) % 6);
		Cell.Pair = static_cast<EOpComparisonEnumAliasPair>((Index / 2) % 6);
		Cell.Order = static_cast<EOpComparisonEnumAliasOrder>(Index % 2);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FEnumAliasCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FEnumAliasCell> Cells = AllCells();
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

void AppendEnumDeclaration(FString& Source)
{
	AppendLine(Source, TEXT("enum EComparisonEnum"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tMinimum = -128,"));
	AppendLine(Source, TEXT("\tMinusOne = -1,"));
	AppendLine(Source, TEXT("\tZero = 0,"));
	AppendLine(Source, TEXT("\tOne = 1,"));
	AppendLine(Source, TEXT("\tMaximum = 127"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendTraceHelpers(FString& Source)
{
	AppendLine(Source, TEXT("EComparisonEnum TraceEnumOperand(int Marker, EComparisonEnum Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tRecordComparisonIntegerOperand(Marker, int(Value));"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("ComparisonAlias TraceAliasOperand(int Marker, ComparisonAlias Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tRecordComparisonIntegerOperand(Marker, int(Value));"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendSharedHelpers(FString& Source)
{
	AppendEnumDeclaration(Source);
	AppendLine(Source, TEXT("// ComparisonAlias is registered through the raw SDK before build."));
	AppendLine(Source);
	AppendTraceHelpers(Source);
}

void AppendEntry(FString& Source, const FOpComparisonEnumAliasParams& Params, const FString& FunctionName)
{
	int32 Left = 0;
	int32 Right = 0;
	IntegralValues(Params.Pair, Left, Right);
	const TCHAR* const Symbol = OperatorSymbol(Params.Operator);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Family == EOpComparisonEnumAliasFamily::Enum)
	{
		AppendLine(Source, FString::Printf(TEXT("\tEComparisonEnum Left = %s;"), EnumLiteral(Left)));
		AppendLine(Source, FString::Printf(TEXT("\tEComparisonEnum Right = %s;"), EnumLiteral(Right)));
		if (Params.Order == EOpComparisonEnumAliasOrder::LeftRight)
		{
			AppendLine(
				Source,
				FString::Printf(
					TEXT("\tconst bool Result = TraceEnumOperand(1, Left) %s TraceEnumOperand(2, Right);"),
					Symbol));
		}
		else
		{
			AppendLine(
				Source,
				FString::Printf(
					TEXT("\tconst bool Result = TraceEnumOperand(2, Right) %s TraceEnumOperand(1, Left);"),
					Symbol));
		}
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\tComparisonAlias Left = ComparisonAlias(%d);"), Left));
		AppendLine(Source, FString::Printf(TEXT("\tComparisonAlias Right = ComparisonAlias(%d);"), Right));
		if (Params.Order == EOpComparisonEnumAliasOrder::LeftRight)
		{
			AppendLine(
				Source,
				FString::Printf(
					TEXT("\tconst bool Result = TraceAliasOperand(1, Left) %s TraceAliasOperand(2, Right);"),
					Symbol));
		}
		else
		{
			AppendLine(
				Source,
				FString::Printf(
					TEXT("\tconst bool Result = TraceAliasOperand(2, Right) %s TraceAliasOperand(1, Left);"),
					Symbol));
		}
	}
	AppendLine(Source, TEXT("\treturn Result ? 1 : 0;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FOpComparisonEnumAliasParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FEnumAliasCell& Cell)
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

FStringView FOpComparisonEnumAliasGenerator::GetProductId() const
{
	return TEXT("LANG-OP-COMPARISON-ENUM-ALIAS");
}

TArray<FGeneratedCaseInfo> FOpComparisonEnumAliasGenerator::ListCases() const
{
	using namespace OpComparisonEnumAliasGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FEnumAliasCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		const FString EntryName = MakeEntryName(Cells[Index]);
		checkf(!EntryNames.Contains(EntryName), TEXT("OpComparisonEnumAlias PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FOpComparisonEnumAliasGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace OpComparisonEnumAliasGeneratorDetail;
	FEnumAliasCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FOpComparisonEnumAliasGenerator::BuildDumpSource() const
{
	using namespace OpComparisonEnumAliasGeneratorDetail;
	const TArray<FEnumAliasCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-COMPARISON-ENUM-ALIAS"));
	AppendLine(Source, TEXT("// Cases: 144 total, 144 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FEnumAliasCell& Cell = Cells[Index];
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

FString FOpComparisonEnumAliasGenerator::BuildIntegralComparisonSource(
	const FOpComparisonEnumAliasParams& Params) const
{
	using namespace OpComparisonEnumAliasGeneratorDetail;
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

FString FOpComparisonEnumAliasGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace OpComparisonEnumAliasGeneratorDetail;
	const TArray<FEnumAliasCell> Cells = AllCells();
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

int32 FOpComparisonEnumAliasGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpComparisonEnumAliasGeneratorDetail;
	FEnumAliasCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}
}
