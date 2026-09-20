#include "Framework/Generate/AngelscriptTestOpAssignmentTargetRejectionGenerator.h"

namespace AngelscriptTest::Generate
{
namespace OpAssignmentTargetRejectionGeneratorDetail
{
struct FTargetRejectionCell
{
	EOpAssignmentTargetRejectionTarget Target = EOpAssignmentTargetRejectionTarget::ConstInvalid;
	EOpAssignmentTargetRejectionOperation Operation = EOpAssignmentTargetRejectionOperation::Assign;
	EOpAssignmentTargetRejectionType Type = EOpAssignmentTargetRejectionType::Int;
};

const TCHAR* TargetToken(const EOpAssignmentTargetRejectionTarget Target)
{
	switch (Target)
	{
	case EOpAssignmentTargetRejectionTarget::ConstInvalid:
		return TEXT("CONST_INVALID");
	case EOpAssignmentTargetRejectionTarget::TemporaryInvalid:
		return TEXT("TEMPORARY_INVALID");
	default:
		return nullptr;
	}
}

const TCHAR* OperationToken(const EOpAssignmentTargetRejectionOperation Operation)
{
	switch (Operation)
	{
	case EOpAssignmentTargetRejectionOperation::Assign:
		return TEXT("ASSIGN");
	case EOpAssignmentTargetRejectionOperation::AddAssign:
		return TEXT("ADD_ASSIGN");
	case EOpAssignmentTargetRejectionOperation::SubtractAssign:
		return TEXT("SUBTRACT_ASSIGN");
	case EOpAssignmentTargetRejectionOperation::MultiplyAssign:
		return TEXT("MULTIPLY_ASSIGN");
	case EOpAssignmentTargetRejectionOperation::DivideAssign:
		return TEXT("DIVIDE_ASSIGN");
	case EOpAssignmentTargetRejectionOperation::ModuloAssign:
		return TEXT("MODULO_ASSIGN");
	case EOpAssignmentTargetRejectionOperation::PowerAssign:
		return TEXT("POWER_ASSIGN");
	case EOpAssignmentTargetRejectionOperation::AndAssign:
		return TEXT("AND_ASSIGN");
	case EOpAssignmentTargetRejectionOperation::OrAssign:
		return TEXT("OR_ASSIGN");
	case EOpAssignmentTargetRejectionOperation::XorAssign:
		return TEXT("XOR_ASSIGN");
	case EOpAssignmentTargetRejectionOperation::ShiftLeftAssign:
		return TEXT("SHIFT_LEFT_ASSIGN");
	case EOpAssignmentTargetRejectionOperation::ShiftRightLogicalAssign:
		return TEXT("SHIFT_RIGHT_LOGICAL_ASSIGN");
	case EOpAssignmentTargetRejectionOperation::ShiftRightArithmeticAssign:
		return TEXT("SHIFT_RIGHT_ARITHMETIC_ASSIGN");
	default:
		return nullptr;
	}
}

const TCHAR* TypeToken(const EOpAssignmentTargetRejectionType Type)
{
	switch (Type)
	{
	case EOpAssignmentTargetRejectionType::Int8:
		return TEXT("INT8");
	case EOpAssignmentTargetRejectionType::Int16:
		return TEXT("INT16");
	case EOpAssignmentTargetRejectionType::Int:
		return TEXT("INT");
	case EOpAssignmentTargetRejectionType::Int64:
		return TEXT("INT64");
	case EOpAssignmentTargetRejectionType::Uint8:
		return TEXT("UINT8");
	case EOpAssignmentTargetRejectionType::Uint16:
		return TEXT("UINT16");
	case EOpAssignmentTargetRejectionType::Uint:
		return TEXT("UINT");
	case EOpAssignmentTargetRejectionType::Uint64:
		return TEXT("UINT64");
	case EOpAssignmentTargetRejectionType::Float32:
		return TEXT("FLOAT32");
	case EOpAssignmentTargetRejectionType::Float64:
		return TEXT("FLOAT64");
	case EOpAssignmentTargetRejectionType::Bool:
		return TEXT("BOOL");
	default:
		return nullptr;
	}
}

const TCHAR* TypeCatalogName(const EOpAssignmentTargetRejectionType Type)
{
	switch (Type)
	{
	case EOpAssignmentTargetRejectionType::Int8:
		return TEXT("int8");
	case EOpAssignmentTargetRejectionType::Int16:
		return TEXT("int16");
	case EOpAssignmentTargetRejectionType::Int:
		return TEXT("int");
	case EOpAssignmentTargetRejectionType::Int64:
		return TEXT("int64");
	case EOpAssignmentTargetRejectionType::Uint8:
		return TEXT("uint8");
	case EOpAssignmentTargetRejectionType::Uint16:
		return TEXT("uint16");
	case EOpAssignmentTargetRejectionType::Uint:
		return TEXT("uint");
	case EOpAssignmentTargetRejectionType::Uint64:
		return TEXT("uint64");
	case EOpAssignmentTargetRejectionType::Float32:
		return TEXT("float32");
	case EOpAssignmentTargetRejectionType::Float64:
		return TEXT("float64");
	case EOpAssignmentTargetRejectionType::Bool:
		return TEXT("bool");
	default:
		return nullptr;
	}
}

const TCHAR* ScriptType(const EOpAssignmentTargetRejectionType Type)
{
	if (Type == EOpAssignmentTargetRejectionType::Float64)
	{
		return TEXT("double");
	}
	return TypeCatalogName(Type);
}

const TCHAR* OperationSymbol(const EOpAssignmentTargetRejectionOperation Operation)
{
	switch (Operation)
	{
	case EOpAssignmentTargetRejectionOperation::Assign:
		return TEXT("=");
	case EOpAssignmentTargetRejectionOperation::AddAssign:
		return TEXT("+=");
	case EOpAssignmentTargetRejectionOperation::SubtractAssign:
		return TEXT("-=");
	case EOpAssignmentTargetRejectionOperation::MultiplyAssign:
		return TEXT("*=");
	case EOpAssignmentTargetRejectionOperation::DivideAssign:
		return TEXT("/=");
	case EOpAssignmentTargetRejectionOperation::ModuloAssign:
		return TEXT("%=");
	case EOpAssignmentTargetRejectionOperation::PowerAssign:
		return TEXT("**=");
	case EOpAssignmentTargetRejectionOperation::AndAssign:
		return TEXT("&=");
	case EOpAssignmentTargetRejectionOperation::OrAssign:
		return TEXT("|=");
	case EOpAssignmentTargetRejectionOperation::XorAssign:
		return TEXT("^=");
	case EOpAssignmentTargetRejectionOperation::ShiftLeftAssign:
		return TEXT("<<=");
	case EOpAssignmentTargetRejectionOperation::ShiftRightLogicalAssign:
		return TEXT(">>=");
	case EOpAssignmentTargetRejectionOperation::ShiftRightArithmeticAssign:
		return TEXT(">>>=");
	default:
		return nullptr;
	}
}

const TCHAR* InputLiteral(const EOpAssignmentTargetRejectionType Type)
{
	if (Type == EOpAssignmentTargetRejectionType::Bool)
	{
		return TEXT("false");
	}
	if (Type == EOpAssignmentTargetRejectionType::Float32)
	{
		return TEXT("4.0f");
	}
	if (Type == EOpAssignmentTargetRejectionType::Float64)
	{
		return TEXT("4.0");
	}
	return TEXT("4");
}

const TCHAR* SourceLiteral(const EOpAssignmentTargetRejectionType Type)
{
	if (Type == EOpAssignmentTargetRejectionType::Bool)
	{
		return TEXT("true");
	}
	if (Type == EOpAssignmentTargetRejectionType::Float32)
	{
		return TEXT("2.0f");
	}
	if (Type == EOpAssignmentTargetRejectionType::Float64)
	{
		return TEXT("2.0");
	}
	return TEXT("2");
}

bool IsIntegralType(const EOpAssignmentTargetRejectionType Type)
{
	return Type <= EOpAssignmentTargetRejectionType::Uint64;
}

bool IsNumericType(const EOpAssignmentTargetRejectionType Type)
{
	return Type <= EOpAssignmentTargetRejectionType::Float64;
}

bool SupportsOperation(
	const EOpAssignmentTargetRejectionOperation Operation, const EOpAssignmentTargetRejectionType Type)
{
	switch (Operation)
	{
	case EOpAssignmentTargetRejectionOperation::Assign:
		return true;
	case EOpAssignmentTargetRejectionOperation::AddAssign:
	case EOpAssignmentTargetRejectionOperation::SubtractAssign:
	case EOpAssignmentTargetRejectionOperation::MultiplyAssign:
	case EOpAssignmentTargetRejectionOperation::DivideAssign:
	case EOpAssignmentTargetRejectionOperation::ModuloAssign:
		return IsNumericType(Type);
	case EOpAssignmentTargetRejectionOperation::PowerAssign:
		return Type == EOpAssignmentTargetRejectionType::Float32
			|| Type == EOpAssignmentTargetRejectionType::Float64;
	case EOpAssignmentTargetRejectionOperation::AndAssign:
	case EOpAssignmentTargetRejectionOperation::OrAssign:
	case EOpAssignmentTargetRejectionOperation::XorAssign:
	case EOpAssignmentTargetRejectionOperation::ShiftLeftAssign:
	case EOpAssignmentTargetRejectionOperation::ShiftRightLogicalAssign:
	case EOpAssignmentTargetRejectionOperation::ShiftRightArithmeticAssign:
		return IsIntegralType(Type);
	default:
		return false;
	}
}

bool IsValidParams(const FOpAssignmentTargetRejectionParams& Params)
{
	return TargetToken(Params.Target) != nullptr
		&& OperationToken(Params.Operation) != nullptr
		&& TypeToken(Params.Type) != nullptr
		&& SupportsOperation(Params.Operation, Params.Type);
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FTargetRejectionCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-ASSIGNMENT-TARGET-REJECTION-%s-%s_%s"),
		TargetToken(Cell.Target),
		OperationToken(Cell.Operation),
		TypeToken(Cell.Type));
}

FString MakeEntryName(const FTargetRejectionCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpAssignmentTargetRejectionParams ParamsFromCell(const FTargetRejectionCell& Cell)
{
	FOpAssignmentTargetRejectionParams Params;
	Params.Target = Cell.Target;
	Params.Operation = Cell.Operation;
	Params.Type = Cell.Type;
	return Params;
}

TArray<FTargetRejectionCell> AllCells()
{
	TArray<FTargetRejectionCell> Cells;
	Cells.SetNum(222);
	int32 Write = 0;
	for (int32 TargetIndex = 0; TargetIndex < 2; ++TargetIndex)
	{
		for (int32 OperationIndex = 0; OperationIndex < 13; ++OperationIndex)
		{
			for (int32 TypeIndex = 0; TypeIndex < 11; ++TypeIndex)
			{
				const EOpAssignmentTargetRejectionOperation Operation =
					static_cast<EOpAssignmentTargetRejectionOperation>(OperationIndex);
				const EOpAssignmentTargetRejectionType Type =
					static_cast<EOpAssignmentTargetRejectionType>(TypeIndex);
				if (!SupportsOperation(Operation, Type))
				{
					continue;
				}
				FTargetRejectionCell& Cell = Cells[Write];
				Cell.Target = static_cast<EOpAssignmentTargetRejectionTarget>(TargetIndex);
				Cell.Operation = Operation;
				Cell.Type = Type;
				++Write;
			}
		}
	}
	checkf(Write == 222, TEXT("OpAssignmentTargetRejection AllCells must retain 222 reject cells"));
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FTargetRejectionCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FTargetRejectionCell> Cells = AllCells();
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

void AppendTemporaryHelper(FString& Source, const EOpAssignmentTargetRejectionType Type)
{
	const TCHAR* const TypeName = ScriptType(Type);
	AppendLine(
		Source,
		FString::Printf(TEXT("%s MakeRejectedAssignmentTemporary(%s Input)"), TypeName, TypeName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tRecordAssignmentTemporaryProducer();"));
	AppendLine(Source, TEXT("\treturn Input;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendEntry(
	FString& Source, const FOpAssignmentTargetRejectionParams& Params, const FString& FunctionName)
{
	const TCHAR* const TypeName = ScriptType(Params.Type);
	const TCHAR* const Symbol = OperationSymbol(Params.Operation);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Input = %s;"), TypeName, InputLiteral(Params.Type)));
	AppendLine(Source, FString::Printf(TEXT("\t%s Source = %s;"), TypeName, SourceLiteral(Params.Type)));
	if (Params.Target == EOpAssignmentTargetRejectionTarget::ConstInvalid)
	{
		AppendLine(Source, FString::Printf(TEXT("\tconst %s Value = Input;"), TypeName));
		AppendLine(
			Source,
			FString::Printf(TEXT("\tValue %s Source; // ASSIGNMENT_CAUSE"), Symbol));
	}
	else
	{
		AppendLine(
			Source,
			FString::Printf(
				TEXT("\tMakeRejectedAssignmentTemporary(Input) %s Source; // ASSIGNMENT_CAUSE"),
				Symbol));
	}
	AppendLine(Source, TEXT("\treturn 0;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FOpAssignmentTargetRejectionParams& Params, const FString& FunctionName)
{
	FString Source;
	if (Params.Target == EOpAssignmentTargetRejectionTarget::TemporaryInvalid)
	{
		AppendTemporaryHelper(Source, Params.Type);
	}
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FTargetRejectionCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.Outcome = EGeneratedCaseOutcome::CompileReject;
	Info.Execution = EGeneratedCaseExecution::SourceOnly;
	return Info;
}
}

FStringView FOpAssignmentTargetRejectionGenerator::GetProductId() const
{
	return TEXT("LANG-OP-ASSIGNMENT-TARGET-REJECTION");
}

TArray<FGeneratedCaseInfo> FOpAssignmentTargetRejectionGenerator::ListCases() const
{
	using namespace OpAssignmentTargetRejectionGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	const TArray<FTargetRejectionCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		Cases.Add(MakeCaseInfo(Cells[Index]));
	}
	return Cases;
}

FString FOpAssignmentTargetRejectionGenerator::BuildCaseSource(const FStringView CaseId) const
{
	return BuildRejectSource(CaseId);
}

FString FOpAssignmentTargetRejectionGenerator::BuildDumpSource() const
{
	using namespace OpAssignmentTargetRejectionGeneratorDetail;
	const TArray<FTargetRejectionCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-ASSIGNMENT-TARGET-REJECTION"));
	AppendLine(Source, TEXT("// Cases: 222 total, 0 normal, 222 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FTargetRejectionCell& Cell = Cells[Index];
		AppendLine(Source);
		AppendLine(Source, TEXT("// Reject module"));
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *MakeCaseId(Cell)));
		AppendLine(Source, TEXT("// EntryDeclaration:"));
		AppendLine(Source, TEXT("// Outcome: CompileReject"));
		AppendLine(Source, TEXT("// Execution: SourceOnly"));
		AppendLine(Source);
		if (Cell.Target == EOpAssignmentTargetRejectionTarget::TemporaryInvalid)
		{
			AppendTemporaryHelper(Source, Cell.Type);
		}
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FOpAssignmentTargetRejectionGenerator::BuildAllSource(int32& OutCaseCount) const
{
	OutCaseCount = 0;
	return FString();
}

FString FOpAssignmentTargetRejectionGenerator::BuildTargetRejectionSource(
	const FOpAssignmentTargetRejectionParams& Params) const
{
	using namespace OpAssignmentTargetRejectionGeneratorDetail;
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

int32 FOpAssignmentTargetRejectionGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpAssignmentTargetRejectionGeneratorDetail;
	FTargetRejectionCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return 0;
}

FString FOpAssignmentTargetRejectionGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace OpAssignmentTargetRejectionGeneratorDetail;
	FTargetRejectionCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FOpAssignmentTargetRejectionGenerator::ListRejectCaseIds() const
{
	using namespace OpAssignmentTargetRejectionGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FTargetRejectionCell> Cells = AllCells();
	Ids.SetNum(Cells.Num());
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		Ids[Index] = MakeCaseId(Cells[Index]);
	}
	return Ids;
}
}
