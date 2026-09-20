#include "Framework/Generate/AngelscriptTestOpIncrementTypeRejectionGenerator.h"

namespace AngelscriptTest::Generate
{
namespace OpIncrementTypeRejectionGeneratorDetail
{
struct FTypeRejectCell
{
	EOpIncrementTypeRejectionCategory Category = EOpIncrementTypeRejectionCategory::Local;
	EOpIncrementTypeRejectionOperator Operator = EOpIncrementTypeRejectionOperator::PreIncrement;
};

const TCHAR* CategoryToken(const EOpIncrementTypeRejectionCategory Category)
{
	switch (Category)
	{
	case EOpIncrementTypeRejectionCategory::Local:
		return TEXT("LOCAL");
	case EOpIncrementTypeRejectionCategory::Field:
		return TEXT("FIELD");
	case EOpIncrementTypeRejectionCategory::Property:
		return TEXT("PROPERTY");
	case EOpIncrementTypeRejectionCategory::Alias:
		return TEXT("ALIAS");
	default:
		return nullptr;
	}
}

const TCHAR* OperatorToken(const EOpIncrementTypeRejectionOperator Operator)
{
	switch (Operator)
	{
	case EOpIncrementTypeRejectionOperator::PreIncrement:
		return TEXT("PRE_INCREMENT");
	case EOpIncrementTypeRejectionOperator::PostIncrement:
		return TEXT("POST_INCREMENT");
	case EOpIncrementTypeRejectionOperator::PreDecrement:
		return TEXT("PRE_DECREMENT");
	case EOpIncrementTypeRejectionOperator::PostDecrement:
		return TEXT("POST_DECREMENT");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FOpIncrementTypeRejectionParams& Params)
{
	return CategoryToken(Params.Category) != nullptr && OperatorToken(Params.Operator) != nullptr;
}

FString OperatorExpression(const EOpIncrementTypeRejectionOperator Operator, const FString& Target)
{
	switch (Operator)
	{
	case EOpIncrementTypeRejectionOperator::PreIncrement:
		return TEXT("++") + Target;
	case EOpIncrementTypeRejectionOperator::PostIncrement:
		return Target + TEXT("++");
	case EOpIncrementTypeRejectionOperator::PreDecrement:
		return TEXT("--") + Target;
	case EOpIncrementTypeRejectionOperator::PostDecrement:
		return Target + TEXT("--");
	default:
		return Target;
	}
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FTypeRejectCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-INCREMENT-TYPE-REJECTION-%s-%s"),
		CategoryToken(Cell.Category),
		OperatorToken(Cell.Operator));
}

FString MakeEntryName(const FTypeRejectCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpIncrementTypeRejectionParams ParamsFromCell(const FTypeRejectCell& Cell)
{
	FOpIncrementTypeRejectionParams Params;
	Params.Category = Cell.Category;
	Params.Operator = Cell.Operator;
	return Params;
}

TArray<FTypeRejectCell> AllCells()
{
	TArray<FTypeRejectCell> Cells;
	Cells.SetNum(16);
	for (int32 Index = 0; Index < 16; ++Index)
	{
		FTypeRejectCell& Cell = Cells[Index];
		Cell.Category = static_cast<EOpIncrementTypeRejectionCategory>(Index / 4);
		Cell.Operator = static_cast<EOpIncrementTypeRejectionOperator>(Index % 4);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FTypeRejectCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FTypeRejectCell> Cells = AllCells();
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

void AppendEntry(FString& Source, const FOpIncrementTypeRejectionParams& Params, const FString& FunctionName)
{
	if (Params.Category == EOpIncrementTypeRejectionCategory::Field)
	{
		AppendLine(Source, TEXT("struct FRejectedBoolIncrementField"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tbool Value;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Category == EOpIncrementTypeRejectionCategory::Local)
	{
		AppendLine(Source, TEXT("\tbool Input = false;"));
		AppendLine(Source, TEXT("\tbool Value = Input;"));
		AppendLine(Source, FString::Printf(TEXT("\t%s; // INCREMENT_CAUSE"), *OperatorExpression(Params.Operator, TEXT("Value"))));
	}
	else if (Params.Category == EOpIncrementTypeRejectionCategory::Field)
	{
		AppendLine(Source, TEXT("\tbool Input = false;"));
		AppendLine(Source, TEXT("\tFRejectedBoolIncrementField Owner;"));
		AppendLine(Source, TEXT("\tOwner.Value = Input;"));
		AppendLine(Source, FString::Printf(
			TEXT("\t%s; // INCREMENT_CAUSE"),
			*OperatorExpression(Params.Operator, TEXT("Owner.Value"))));
	}
	else if (Params.Category == EOpIncrementTypeRejectionCategory::Property)
	{
		AppendLine(Source, TEXT("\tFIncrementProperty_bool Owner = MakeIncrementProperty_bool();"));
		AppendLine(Source, FString::Printf(
			TEXT("\t%s; // INCREMENT_CAUSE"),
			*OperatorExpression(Params.Operator, TEXT("Owner.Value"))));
	}
	else
	{
		AppendLine(Source, TEXT("\tbool Value = false;"));
		AppendLine(Source, FString::Printf(TEXT("\t%s; // INCREMENT_CAUSE"), *OperatorExpression(Params.Operator, TEXT("Value"))));
	}
	AppendLine(Source, TEXT("\treturn 0;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FOpIncrementTypeRejectionParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FTypeRejectCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.Outcome = EGeneratedCaseOutcome::CompileReject;
	Info.Execution = EGeneratedCaseExecution::SourceOnly;
	return Info;
}
}

FStringView FOpIncrementTypeRejectionGenerator::GetProductId() const
{
	return TEXT("LANG-OP-INCREMENT-TYPE-REJECTION");
}

TArray<FGeneratedCaseInfo> FOpIncrementTypeRejectionGenerator::ListCases() const
{
	using namespace OpIncrementTypeRejectionGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	const TArray<FTypeRejectCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		Cases.Add(MakeCaseInfo(Cells[Index]));
	}
	return Cases;
}

FString FOpIncrementTypeRejectionGenerator::BuildCaseSource(const FStringView CaseId) const
{
	return BuildRejectSource(CaseId);
}

FString FOpIncrementTypeRejectionGenerator::BuildDumpSource() const
{
	using namespace OpIncrementTypeRejectionGeneratorDetail;
	const TArray<FTypeRejectCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-INCREMENT-TYPE-REJECTION"));
	AppendLine(Source, TEXT("// Cases: 16 total, 0 normal, 16 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		AppendLine(Source);
		AppendLine(Source, TEXT("// Reject module"));
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, TEXT("// EntryDeclaration:"));
		AppendLine(Source, TEXT("// Outcome: CompileReject"));
		AppendLine(Source, TEXT("// Execution: SourceOnly"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

FString FOpIncrementTypeRejectionGenerator::BuildAllSource(int32& OutCaseCount) const
{
	OutCaseCount = 0;
	return FString();
}

FString FOpIncrementTypeRejectionGenerator::BuildBoolRejectionSource(
	const FOpIncrementTypeRejectionParams& Params) const
{
	using namespace OpIncrementTypeRejectionGeneratorDetail;
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

int32 FOpIncrementTypeRejectionGenerator::GetExpected(const FStringView CaseId) const
{
	(void)CaseId;
	return 0;
}

FString FOpIncrementTypeRejectionGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace OpIncrementTypeRejectionGeneratorDetail;
	FTypeRejectCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FOpIncrementTypeRejectionGenerator::ListRejectCaseIds() const
{
	using namespace OpIncrementTypeRejectionGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FTypeRejectCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		Ids.Add(MakeCaseId(Cells[Index]));
	}
	return Ids;
}
}
