#include "Framework/Generate/AngelscriptTestOpComparisonReferenceGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace OpComparisonReferenceGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native FComparisonReferenceRoot fixtures.");

struct FReferenceCell
{
	EOpComparisonReferenceOperator Operator = EOpComparisonReferenceOperator::Equal;
	EOpComparisonReferenceRelation Relation = EOpComparisonReferenceRelation::SameNonNull;
	EOpComparisonReferenceOrder Order = EOpComparisonReferenceOrder::LeftRight;
};

const TCHAR* OperatorToken(const EOpComparisonReferenceOperator Operator)
{
	switch (Operator)
	{
	case EOpComparisonReferenceOperator::Less:
		return TEXT("LESS");
	case EOpComparisonReferenceOperator::LessEqual:
		return TEXT("LESS_EQUAL");
	case EOpComparisonReferenceOperator::Greater:
		return TEXT("GREATER");
	case EOpComparisonReferenceOperator::GreaterEqual:
		return TEXT("GREATER_EQUAL");
	case EOpComparisonReferenceOperator::Equal:
		return TEXT("EQUAL");
	case EOpComparisonReferenceOperator::NotEqual:
		return TEXT("NOT_EQUAL");
	default:
		return nullptr;
	}
}

const TCHAR* RelationToken(const EOpComparisonReferenceRelation Relation)
{
	switch (Relation)
	{
	case EOpComparisonReferenceRelation::SameNonNull:
		return TEXT("SAME_NON_NULL");
	case EOpComparisonReferenceRelation::DifferentNonNull:
		return TEXT("DIFFERENT_NON_NULL");
	case EOpComparisonReferenceRelation::LeftNull:
		return TEXT("LEFT_NULL");
	case EOpComparisonReferenceRelation::RightNull:
		return TEXT("RIGHT_NULL");
	case EOpComparisonReferenceRelation::BothNull:
		return TEXT("BOTH_NULL");
	case EOpComparisonReferenceRelation::DerivedBaseSame:
		return TEXT("DERIVED_BASE_SAME");
	case EOpComparisonReferenceRelation::SiblingDifferent:
		return TEXT("SIBLING_DIFFERENT");
	case EOpComparisonReferenceRelation::ConstSame:
		return TEXT("CONST_SAME");
	default:
		return nullptr;
	}
}

const TCHAR* OrderToken(const EOpComparisonReferenceOrder Order)
{
	switch (Order)
	{
	case EOpComparisonReferenceOrder::LeftRight:
		return TEXT("LEFT_RIGHT");
	case EOpComparisonReferenceOrder::RightLeft:
		return TEXT("RIGHT_LEFT");
	default:
		return nullptr;
	}
}

const TCHAR* OperatorGlyph(const EOpComparisonReferenceOperator Operator)
{
	switch (Operator)
	{
	case EOpComparisonReferenceOperator::Less:
		return TEXT("<");
	case EOpComparisonReferenceOperator::LessEqual:
		return TEXT("<=");
	case EOpComparisonReferenceOperator::Greater:
		return TEXT(">");
	case EOpComparisonReferenceOperator::GreaterEqual:
		return TEXT(">=");
	case EOpComparisonReferenceOperator::Equal:
		return TEXT("==");
	case EOpComparisonReferenceOperator::NotEqual:
		return TEXT("!=");
	default:
		return TEXT("==");
	}
}

bool IsValidParams(const FOpComparisonReferenceParams& Params)
{
	return OperatorToken(Params.Operator) != nullptr
		&& RelationToken(Params.Relation) != nullptr
		&& OrderToken(Params.Order) != nullptr;
}

bool IsOrdering(const EOpComparisonReferenceOperator Operator)
{
	return Operator == EOpComparisonReferenceOperator::Less
		|| Operator == EOpComparisonReferenceOperator::LessEqual
		|| Operator == EOpComparisonReferenceOperator::Greater
		|| Operator == EOpComparisonReferenceOperator::GreaterEqual;
}

bool RelationEqual(const EOpComparisonReferenceRelation Relation)
{
	return Relation == EOpComparisonReferenceRelation::SameNonNull
		|| Relation == EOpComparisonReferenceRelation::BothNull
		|| Relation == EOpComparisonReferenceRelation::DerivedBaseSame
		|| Relation == EOpComparisonReferenceRelation::ConstSame;
}

int32 ExpectedFromCell(const FReferenceCell& Cell)
{
	if (IsOrdering(Cell.Operator))
	{
		return 0;
	}
	const bool bEqual = RelationEqual(Cell.Relation);
	if (Cell.Operator == EOpComparisonReferenceOperator::Equal)
	{
		return bEqual ? 1 : 0;
	}
	return bEqual ? 0 : 1;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FReferenceCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-COMPARISON-REFERENCE-%s-%s-%s"),
		OperatorToken(Cell.Operator),
		RelationToken(Cell.Relation),
		OrderToken(Cell.Order));
}

FString MakeEntryName(const FReferenceCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpComparisonReferenceParams ParamsFromCell(const FReferenceCell& Cell)
{
	FOpComparisonReferenceParams Params;
	Params.Operator = Cell.Operator;
	Params.Relation = Cell.Relation;
	Params.Order = Cell.Order;
	return Params;
}

TArray<FReferenceCell> AllCells()
{
	TArray<FReferenceCell> Cells;
	Cells.SetNum(96);
	for (int32 Index = 0; Index < 96; ++Index)
	{
		FReferenceCell& Cell = Cells[Index];
		Cell.Operator = static_cast<EOpComparisonReferenceOperator>(Index / 16);
		Cell.Relation = static_cast<EOpComparisonReferenceRelation>((Index / 2) % 8);
		Cell.Order = static_cast<EOpComparisonReferenceOrder>(Index % 2);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FReferenceCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FReferenceCell> Cells = AllCells();
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
	AppendLine(Source, TEXT("FComparisonReferenceRoot TraceReferenceOperand(int Marker, FComparisonReferenceRoot Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tRecordReferenceOperand(Marker, Value == nullptr ? 0 : Value.GetIdentity());"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("const FComparisonReferenceRoot TraceConstReferenceOperand(int Marker, const FComparisonReferenceRoot Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tRecordReferenceOperand(Marker, Value == nullptr ? 0 : Value.GetIdentity());"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
}

void AppendSetup(FString& Source, const EOpComparisonReferenceRelation Relation)
{
	switch (Relation)
	{
	case EOpComparisonReferenceRelation::SameNonNull:
		AppendLine(Source, TEXT("\tFComparisonReferenceRoot Left = MakeComparisonRoot();"));
		AppendLine(Source, TEXT("\tFComparisonReferenceRoot Right = Left;"));
		break;
	case EOpComparisonReferenceRelation::DifferentNonNull:
		AppendLine(Source, TEXT("\tFComparisonReferenceRoot Left = MakeComparisonRoot();"));
		AppendLine(Source, TEXT("\tFComparisonReferenceRoot Right = MakeComparisonRoot();"));
		break;
	case EOpComparisonReferenceRelation::LeftNull:
		AppendLine(Source, TEXT("\tFComparisonReferenceRoot Left = nullptr;"));
		AppendLine(Source, TEXT("\tFComparisonReferenceRoot Right = MakeComparisonRoot();"));
		break;
	case EOpComparisonReferenceRelation::RightNull:
		AppendLine(Source, TEXT("\tFComparisonReferenceRoot Left = MakeComparisonRoot();"));
		AppendLine(Source, TEXT("\tFComparisonReferenceRoot Right = nullptr;"));
		break;
	case EOpComparisonReferenceRelation::BothNull:
		AppendLine(Source, TEXT("\tFComparisonReferenceRoot Left = nullptr;"));
		AppendLine(Source, TEXT("\tFComparisonReferenceRoot Right = nullptr;"));
		break;
	case EOpComparisonReferenceRelation::DerivedBaseSame:
		AppendLine(Source, TEXT("\tFComparisonReferenceRoot Left = MakeComparisonDerivedAsRoot();"));
		AppendLine(Source, TEXT("\tFComparisonReferenceRoot Right = Left;"));
		break;
	case EOpComparisonReferenceRelation::SiblingDifferent:
		AppendLine(Source, TEXT("\tFComparisonReferenceRoot Left = MakeComparisonDerivedAsRoot();"));
		AppendLine(Source, TEXT("\tFComparisonReferenceRoot Right = MakeComparisonSiblingAsRoot();"));
		break;
	case EOpComparisonReferenceRelation::ConstSame:
		AppendLine(Source, TEXT("\tFComparisonReferenceRoot Mutable = MakeComparisonRoot();"));
		AppendLine(Source, TEXT("\tconst FComparisonReferenceRoot Left = Mutable;"));
		AppendLine(Source, TEXT("\tconst FComparisonReferenceRoot Right = Mutable;"));
		break;
	default:
		break;
	}
}

void AppendEntry(FString& Source, const FOpComparisonReferenceParams& Params, const FString& FunctionName)
{
	const TCHAR* const TraceName = Params.Relation == EOpComparisonReferenceRelation::ConstSame
		? TEXT("TraceConstReferenceOperand")
		: TEXT("TraceReferenceOperand");
	const TCHAR* const Glyph = OperatorGlyph(Params.Operator);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendSetup(Source, Params.Relation);
	if (Params.Order == EOpComparisonReferenceOrder::LeftRight)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\treturn (%s(1, Left) %s %s(2, Right)) ? 1 : 0; // REFERENCE_COMPARE_CAUSE"),
			TraceName,
			Glyph,
			TraceName));
	}
	else
	{
		AppendLine(Source, FString::Printf(
			TEXT("\treturn (%s(2, Right) %s %s(1, Left)) ? 1 : 0; // REFERENCE_COMPARE_CAUSE"),
			TraceName,
			Glyph,
			TraceName));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FOpComparisonReferenceParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FReferenceCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsOrdering(Cell.Operator))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	Info.ExecutionNotes = HostNotes;
	return Info;
}
}

FStringView FOpComparisonReferenceGenerator::GetProductId() const
{
	return TEXT("LANG-OP-COMPARISON-REFERENCE");
}

TArray<FGeneratedCaseInfo> FOpComparisonReferenceGenerator::ListCases() const
{
	using namespace OpComparisonReferenceGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FReferenceCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cells[Index]);
			checkf(!EntryNames.Contains(EntryName), TEXT("OpComparisonReference PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FOpComparisonReferenceGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace OpComparisonReferenceGeneratorDetail;
	FReferenceCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FOpComparisonReferenceGenerator::BuildDumpSource() const
{
	using namespace OpComparisonReferenceGeneratorDetail;
	const TArray<FReferenceCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-COMPARISON-REFERENCE"));
	AppendLine(Source, TEXT("// Cases: 96 total, 32 normal, 64 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	bool bNeedBlank = true;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsOrdering(Cells[Index].Operator))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (bNeedBlank)
		{
			AppendLine(Source);
		}
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		bNeedBlank = true;
	}
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (!IsOrdering(Cells[Index].Operator))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		AppendLine(Source);
		AppendLine(Source, TEXT("// Reject module"));
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, TEXT("// EntryDeclaration:"));
		AppendLine(Source, TEXT("// Outcome: CompileReject"));
		AppendLine(Source, TEXT("// Execution: SourceOnly"));
		AppendLine(Source);
		AppendSharedHelpers(Source);
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

FString FOpComparisonReferenceGenerator::BuildReferenceComparisonSource(const FOpComparisonReferenceParams& Params) const
{
	using namespace OpComparisonReferenceGeneratorDetail;
	if (!IsValidParams(Params) || IsOrdering(Params.Operator))
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

FString FOpComparisonReferenceGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace OpComparisonReferenceGeneratorDetail;
	const TArray<FReferenceCell> Cells = AllCells();
	FString Source;
	AppendSharedHelpers(Source);
	int32 Emitted = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsOrdering(Cells[Index].Operator))
		{
			continue;
		}
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

int32 FOpComparisonReferenceGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpComparisonReferenceGeneratorDetail;
	FReferenceCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsOrdering(Cell.Operator))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}

FString FOpComparisonReferenceGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace OpComparisonReferenceGeneratorDetail;
	FReferenceCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsOrdering(Cell.Operator))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FOpComparisonReferenceGenerator::ListRejectCaseIds() const
{
	using namespace OpComparisonReferenceGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FReferenceCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsOrdering(Cells[Index].Operator))
		{
			Ids.Add(MakeCaseId(Cells[Index]));
		}
	}
	return Ids;
}
}
