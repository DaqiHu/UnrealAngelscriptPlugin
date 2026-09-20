#include "Framework/Generate/AngelscriptTestOpIncrementTargetRejectionGenerator.h"

namespace AngelscriptTest::Generate
{
namespace OpIncrementTargetRejectionGeneratorDetail
{
struct FTargetCell
{
	EOpIncrementTargetRejectionTarget Target = EOpIncrementTargetRejectionTarget::ConstInvalid;
	EOpIncrementTargetRejectionOperator Operator = EOpIncrementTargetRejectionOperator::PreIncrement;
	EOpIncrementTargetRejectionType Type = EOpIncrementTargetRejectionType::Int;
};

const TCHAR* TargetToken(const EOpIncrementTargetRejectionTarget Target)
{
	switch (Target)
	{
	case EOpIncrementTargetRejectionTarget::ConstInvalid:
		return TEXT("CONST_INVALID");
	case EOpIncrementTargetRejectionTarget::TemporaryInvalid:
		return TEXT("TEMPORARY_INVALID");
	default:
		return nullptr;
	}
}

const TCHAR* OperatorToken(const EOpIncrementTargetRejectionOperator Operator)
{
	switch (Operator)
	{
	case EOpIncrementTargetRejectionOperator::PreIncrement:
		return TEXT("PRE_INCREMENT");
	case EOpIncrementTargetRejectionOperator::PostIncrement:
		return TEXT("POST_INCREMENT");
	case EOpIncrementTargetRejectionOperator::PreDecrement:
		return TEXT("PRE_DECREMENT");
	case EOpIncrementTargetRejectionOperator::PostDecrement:
		return TEXT("POST_DECREMENT");
	default:
		return nullptr;
	}
}

const TCHAR* TypeToken(const EOpIncrementTargetRejectionType Type)
{
	switch (Type)
	{
	case EOpIncrementTargetRejectionType::Int8:
		return TEXT("INT8");
	case EOpIncrementTargetRejectionType::Int16:
		return TEXT("INT16");
	case EOpIncrementTargetRejectionType::Int:
		return TEXT("INT");
	case EOpIncrementTargetRejectionType::Int64:
		return TEXT("INT64");
	case EOpIncrementTargetRejectionType::Uint8:
		return TEXT("UINT8");
	case EOpIncrementTargetRejectionType::Uint16:
		return TEXT("UINT16");
	case EOpIncrementTargetRejectionType::Uint:
		return TEXT("UINT");
	case EOpIncrementTargetRejectionType::Uint64:
		return TEXT("UINT64");
	case EOpIncrementTargetRejectionType::Float32:
		return TEXT("FLOAT32");
	case EOpIncrementTargetRejectionType::Float64:
		return TEXT("FLOAT64");
	default:
		return nullptr;
	}
}

const TCHAR* ScriptType(const EOpIncrementTargetRejectionType Type)
{
	switch (Type)
	{
	case EOpIncrementTargetRejectionType::Int8:
		return TEXT("int8");
	case EOpIncrementTargetRejectionType::Int16:
		return TEXT("int16");
	case EOpIncrementTargetRejectionType::Int:
		return TEXT("int");
	case EOpIncrementTargetRejectionType::Int64:
		return TEXT("int64");
	case EOpIncrementTargetRejectionType::Uint8:
		return TEXT("uint8");
	case EOpIncrementTargetRejectionType::Uint16:
		return TEXT("uint16");
	case EOpIncrementTargetRejectionType::Uint:
		return TEXT("uint");
	case EOpIncrementTargetRejectionType::Uint64:
		return TEXT("uint64");
	case EOpIncrementTargetRejectionType::Float32:
		return TEXT("float32");
	case EOpIncrementTargetRejectionType::Float64:
		return TEXT("double");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FOpIncrementTargetRejectionParams& Params)
{
	return TargetToken(Params.Target) != nullptr
		&& OperatorToken(Params.Operator) != nullptr
		&& TypeToken(Params.Type) != nullptr;
}

FString OperatorExpression(const EOpIncrementTargetRejectionOperator Operator, const FString& Target)
{
	switch (Operator)
	{
	case EOpIncrementTargetRejectionOperator::PreIncrement:
		return TEXT("++") + Target;
	case EOpIncrementTargetRejectionOperator::PostIncrement:
		return Target + TEXT("++");
	case EOpIncrementTargetRejectionOperator::PreDecrement:
		return TEXT("--") + Target;
	case EOpIncrementTargetRejectionOperator::PostDecrement:
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

FString MakeCaseId(const FTargetCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-INCREMENT-TARGET-REJECTION-%s-%s-%s"),
		TargetToken(Cell.Target),
		OperatorToken(Cell.Operator),
		TypeToken(Cell.Type));
}

FString MakeEntryName(const FTargetCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpIncrementTargetRejectionParams ParamsFromCell(const FTargetCell& Cell)
{
	FOpIncrementTargetRejectionParams Params;
	Params.Target = Cell.Target;
	Params.Operator = Cell.Operator;
	Params.Type = Cell.Type;
	return Params;
}

TArray<FTargetCell> AllCells()
{
	TArray<FTargetCell> Cells;
	Cells.SetNum(80);
	for (int32 Index = 0; Index < 80; ++Index)
	{
		FTargetCell& Cell = Cells[Index];
		Cell.Target = static_cast<EOpIncrementTargetRejectionTarget>(Index / 40);
		Cell.Operator = static_cast<EOpIncrementTargetRejectionOperator>((Index / 10) % 4);
		Cell.Type = static_cast<EOpIncrementTargetRejectionType>(Index % 10);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FTargetCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FTargetCell> Cells = AllCells();
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

void AppendEntry(FString& Source, const FOpIncrementTargetRejectionParams& Params, const FString& FunctionName)
{
	const TCHAR* const TypeName = ScriptType(Params.Type);
	if (Params.Target == EOpIncrementTargetRejectionTarget::TemporaryInvalid)
	{
		AppendLine(Source, FString::Printf(TEXT("%s MakeRejectedIncrementTemporary(%s Input)"), TypeName, TypeName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tRecordIncrementRejectedProducer();"));
		AppendLine(Source, TEXT("\treturn Input;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Input = 12;"), TypeName));
	if (Params.Target == EOpIncrementTargetRejectionTarget::ConstInvalid)
	{
		AppendLine(Source, FString::Printf(TEXT("\tconst %s Value = Input;"), TypeName));
		AppendLine(Source, FString::Printf(TEXT("\t%s; // INCREMENT_CAUSE"), *OperatorExpression(Params.Operator, TEXT("Value"))));
	}
	else
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s; // INCREMENT_CAUSE"),
			*OperatorExpression(Params.Operator, TEXT("MakeRejectedIncrementTemporary(Input)"))));
	}
	AppendLine(Source, TEXT("\treturn 0;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FOpIncrementTargetRejectionParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FTargetCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.Outcome = EGeneratedCaseOutcome::CompileReject;
	Info.Execution = EGeneratedCaseExecution::SourceOnly;
	return Info;
}
}

FStringView FOpIncrementTargetRejectionGenerator::GetProductId() const
{
	return TEXT("LANG-OP-INCREMENT-TARGET-REJECTION");
}

TArray<FGeneratedCaseInfo> FOpIncrementTargetRejectionGenerator::ListCases() const
{
	using namespace OpIncrementTargetRejectionGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	const TArray<FTargetCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		Cases.Add(MakeCaseInfo(Cells[Index]));
	}
	return Cases;
}

FString FOpIncrementTargetRejectionGenerator::BuildCaseSource(const FStringView CaseId) const
{
	return BuildRejectSource(CaseId);
}

FString FOpIncrementTargetRejectionGenerator::BuildDumpSource() const
{
	using namespace OpIncrementTargetRejectionGeneratorDetail;
	const TArray<FTargetCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-INCREMENT-TARGET-REJECTION"));
	AppendLine(Source, TEXT("// Cases: 80 total, 0 normal, 80 reject, 0 fault"));
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

FString FOpIncrementTargetRejectionGenerator::BuildAllSource(int32& OutCaseCount) const
{
	OutCaseCount = 0;
	return FString();
}

FString FOpIncrementTargetRejectionGenerator::BuildTargetRejectionSource(
	const FOpIncrementTargetRejectionParams& Params) const
{
	using namespace OpIncrementTargetRejectionGeneratorDetail;
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

int32 FOpIncrementTargetRejectionGenerator::GetExpected(const FStringView CaseId) const
{
	(void)CaseId;
	return 0;
}

FString FOpIncrementTargetRejectionGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace OpIncrementTargetRejectionGeneratorDetail;
	FTargetCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FOpIncrementTargetRejectionGenerator::ListRejectCaseIds() const
{
	using namespace OpIncrementTargetRejectionGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FTargetCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		Ids.Add(MakeCaseId(Cells[Index]));
	}
	return Ids;
}
}
