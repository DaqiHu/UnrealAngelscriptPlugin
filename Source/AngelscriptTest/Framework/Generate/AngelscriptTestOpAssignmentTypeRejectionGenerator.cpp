#include "Framework/Generate/AngelscriptTestOpAssignmentTypeRejectionGenerator.h"

namespace AngelscriptTest::Generate
{
namespace OpAssignmentTypeRejectionGeneratorDetail
{
struct FTypeRejectionCell
{
	EOpAssignmentTypeRejectionCategory Category = EOpAssignmentTypeRejectionCategory::Local;
	EOpAssignmentTypeRejectionOperation Operation = EOpAssignmentTypeRejectionOperation::AddAssign;
	EOpAssignmentTypeRejectionType Type = EOpAssignmentTypeRejectionType::Bool;
};

const TCHAR* CategoryToken(const EOpAssignmentTypeRejectionCategory Category)
{
	switch (Category)
	{
	case EOpAssignmentTypeRejectionCategory::Local:
		return TEXT("LOCAL");
	case EOpAssignmentTypeRejectionCategory::Field:
		return TEXT("FIELD");
	case EOpAssignmentTypeRejectionCategory::Property:
		return TEXT("PROPERTY");
	case EOpAssignmentTypeRejectionCategory::Alias:
		return TEXT("ALIAS");
	default:
		return nullptr;
	}
}

const TCHAR* OperationToken(const EOpAssignmentTypeRejectionOperation Operation)
{
	switch (Operation)
	{
	case EOpAssignmentTypeRejectionOperation::Assign:
		return TEXT("ASSIGN");
	case EOpAssignmentTypeRejectionOperation::AddAssign:
		return TEXT("ADD_ASSIGN");
	case EOpAssignmentTypeRejectionOperation::SubtractAssign:
		return TEXT("SUBTRACT_ASSIGN");
	case EOpAssignmentTypeRejectionOperation::MultiplyAssign:
		return TEXT("MULTIPLY_ASSIGN");
	case EOpAssignmentTypeRejectionOperation::DivideAssign:
		return TEXT("DIVIDE_ASSIGN");
	case EOpAssignmentTypeRejectionOperation::ModuloAssign:
		return TEXT("MODULO_ASSIGN");
	case EOpAssignmentTypeRejectionOperation::PowerAssign:
		return TEXT("POWER_ASSIGN");
	case EOpAssignmentTypeRejectionOperation::AndAssign:
		return TEXT("AND_ASSIGN");
	case EOpAssignmentTypeRejectionOperation::OrAssign:
		return TEXT("OR_ASSIGN");
	case EOpAssignmentTypeRejectionOperation::XorAssign:
		return TEXT("XOR_ASSIGN");
	case EOpAssignmentTypeRejectionOperation::ShiftLeftAssign:
		return TEXT("SHIFT_LEFT_ASSIGN");
	case EOpAssignmentTypeRejectionOperation::ShiftRightLogicalAssign:
		return TEXT("SHIFT_RIGHT_LOGICAL_ASSIGN");
	case EOpAssignmentTypeRejectionOperation::ShiftRightArithmeticAssign:
		return TEXT("SHIFT_RIGHT_ARITHMETIC_ASSIGN");
	default:
		return nullptr;
	}
}

const TCHAR* TypeToken(const EOpAssignmentTypeRejectionType Type)
{
	switch (Type)
	{
	case EOpAssignmentTypeRejectionType::Int8:
		return TEXT("INT8");
	case EOpAssignmentTypeRejectionType::Int16:
		return TEXT("INT16");
	case EOpAssignmentTypeRejectionType::Int:
		return TEXT("INT");
	case EOpAssignmentTypeRejectionType::Int64:
		return TEXT("INT64");
	case EOpAssignmentTypeRejectionType::Uint8:
		return TEXT("UINT8");
	case EOpAssignmentTypeRejectionType::Uint16:
		return TEXT("UINT16");
	case EOpAssignmentTypeRejectionType::Uint:
		return TEXT("UINT");
	case EOpAssignmentTypeRejectionType::Uint64:
		return TEXT("UINT64");
	case EOpAssignmentTypeRejectionType::Float32:
		return TEXT("FLOAT32");
	case EOpAssignmentTypeRejectionType::Float64:
		return TEXT("FLOAT64");
	case EOpAssignmentTypeRejectionType::Bool:
		return TEXT("BOOL");
	default:
		return nullptr;
	}
}

const TCHAR* TypeCatalogName(const EOpAssignmentTypeRejectionType Type)
{
	switch (Type)
	{
	case EOpAssignmentTypeRejectionType::Int8:
		return TEXT("int8");
	case EOpAssignmentTypeRejectionType::Int16:
		return TEXT("int16");
	case EOpAssignmentTypeRejectionType::Int:
		return TEXT("int");
	case EOpAssignmentTypeRejectionType::Int64:
		return TEXT("int64");
	case EOpAssignmentTypeRejectionType::Uint8:
		return TEXT("uint8");
	case EOpAssignmentTypeRejectionType::Uint16:
		return TEXT("uint16");
	case EOpAssignmentTypeRejectionType::Uint:
		return TEXT("uint");
	case EOpAssignmentTypeRejectionType::Uint64:
		return TEXT("uint64");
	case EOpAssignmentTypeRejectionType::Float32:
		return TEXT("float32");
	case EOpAssignmentTypeRejectionType::Float64:
		return TEXT("float64");
	case EOpAssignmentTypeRejectionType::Bool:
		return TEXT("bool");
	default:
		return nullptr;
	}
}

const TCHAR* ScriptType(const EOpAssignmentTypeRejectionType Type)
{
	if (Type == EOpAssignmentTypeRejectionType::Float64)
	{
		return TEXT("double");
	}
	return TypeCatalogName(Type);
}

const TCHAR* OperationSymbol(const EOpAssignmentTypeRejectionOperation Operation)
{
	switch (Operation)
	{
	case EOpAssignmentTypeRejectionOperation::Assign:
		return TEXT("=");
	case EOpAssignmentTypeRejectionOperation::AddAssign:
		return TEXT("+=");
	case EOpAssignmentTypeRejectionOperation::SubtractAssign:
		return TEXT("-=");
	case EOpAssignmentTypeRejectionOperation::MultiplyAssign:
		return TEXT("*=");
	case EOpAssignmentTypeRejectionOperation::DivideAssign:
		return TEXT("/=");
	case EOpAssignmentTypeRejectionOperation::ModuloAssign:
		return TEXT("%=");
	case EOpAssignmentTypeRejectionOperation::PowerAssign:
		return TEXT("**=");
	case EOpAssignmentTypeRejectionOperation::AndAssign:
		return TEXT("&=");
	case EOpAssignmentTypeRejectionOperation::OrAssign:
		return TEXT("|=");
	case EOpAssignmentTypeRejectionOperation::XorAssign:
		return TEXT("^=");
	case EOpAssignmentTypeRejectionOperation::ShiftLeftAssign:
		return TEXT("<<=");
	case EOpAssignmentTypeRejectionOperation::ShiftRightLogicalAssign:
		return TEXT(">>=");
	case EOpAssignmentTypeRejectionOperation::ShiftRightArithmeticAssign:
		return TEXT(">>>=");
	default:
		return nullptr;
	}
}

const TCHAR* InputLiteral(const EOpAssignmentTypeRejectionType Type)
{
	if (Type == EOpAssignmentTypeRejectionType::Bool)
	{
		return TEXT("false");
	}
	if (Type == EOpAssignmentTypeRejectionType::Float32)
	{
		return TEXT("4.0f");
	}
	if (Type == EOpAssignmentTypeRejectionType::Float64)
	{
		return TEXT("4.0");
	}
	return TEXT("4");
}

const TCHAR* SourceLiteral(const EOpAssignmentTypeRejectionType Type)
{
	if (Type == EOpAssignmentTypeRejectionType::Bool)
	{
		return TEXT("true");
	}
	if (Type == EOpAssignmentTypeRejectionType::Float32)
	{
		return TEXT("2.0f");
	}
	if (Type == EOpAssignmentTypeRejectionType::Float64)
	{
		return TEXT("2.0");
	}
	return TEXT("2");
}

bool IsIntegralType(const EOpAssignmentTypeRejectionType Type)
{
	return Type <= EOpAssignmentTypeRejectionType::Uint64;
}

bool IsNumericType(const EOpAssignmentTypeRejectionType Type)
{
	return Type <= EOpAssignmentTypeRejectionType::Float64;
}

bool SupportsOperation(
	const EOpAssignmentTypeRejectionOperation Operation, const EOpAssignmentTypeRejectionType Type)
{
	switch (Operation)
	{
	case EOpAssignmentTypeRejectionOperation::Assign:
		return true;
	case EOpAssignmentTypeRejectionOperation::AddAssign:
	case EOpAssignmentTypeRejectionOperation::SubtractAssign:
	case EOpAssignmentTypeRejectionOperation::MultiplyAssign:
	case EOpAssignmentTypeRejectionOperation::DivideAssign:
	case EOpAssignmentTypeRejectionOperation::ModuloAssign:
		return IsNumericType(Type);
	case EOpAssignmentTypeRejectionOperation::PowerAssign:
		return Type == EOpAssignmentTypeRejectionType::Float32
			|| Type == EOpAssignmentTypeRejectionType::Float64;
	case EOpAssignmentTypeRejectionOperation::AndAssign:
	case EOpAssignmentTypeRejectionOperation::OrAssign:
	case EOpAssignmentTypeRejectionOperation::XorAssign:
	case EOpAssignmentTypeRejectionOperation::ShiftLeftAssign:
	case EOpAssignmentTypeRejectionOperation::ShiftRightLogicalAssign:
	case EOpAssignmentTypeRejectionOperation::ShiftRightArithmeticAssign:
		return IsIntegralType(Type);
	default:
		return false;
	}
}

bool IsValidParams(const FOpAssignmentTypeRejectionParams& Params)
{
	return CategoryToken(Params.Category) != nullptr
		&& OperationToken(Params.Operation) != nullptr
		&& TypeToken(Params.Type) != nullptr
		&& !SupportsOperation(Params.Operation, Params.Type);
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FTypeRejectionCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-ASSIGNMENT-TYPE-REJECTION-%s-%s_%s"),
		CategoryToken(Cell.Category),
		OperationToken(Cell.Operation),
		TypeToken(Cell.Type));
}

FString MakeEntryName(const FTypeRejectionCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpAssignmentTypeRejectionParams ParamsFromCell(const FTypeRejectionCell& Cell)
{
	FOpAssignmentTypeRejectionParams Params;
	Params.Category = Cell.Category;
	Params.Operation = Cell.Operation;
	Params.Type = Cell.Type;
	return Params;
}

TArray<FTypeRejectionCell> AllCells()
{
	TArray<FTypeRejectionCell> Cells;
	Cells.SetNum(128);
	int32 Write = 0;
	for (int32 CategoryIndex = 0; CategoryIndex < 4; ++CategoryIndex)
	{
		for (int32 OperationIndex = 0; OperationIndex < 13; ++OperationIndex)
		{
			for (int32 TypeIndex = 0; TypeIndex < 11; ++TypeIndex)
			{
				const EOpAssignmentTypeRejectionOperation Operation =
					static_cast<EOpAssignmentTypeRejectionOperation>(OperationIndex);
				const EOpAssignmentTypeRejectionType Type =
					static_cast<EOpAssignmentTypeRejectionType>(TypeIndex);
				if (SupportsOperation(Operation, Type))
				{
					continue;
				}
				FTypeRejectionCell& Cell = Cells[Write];
				Cell.Category = static_cast<EOpAssignmentTypeRejectionCategory>(CategoryIndex);
				Cell.Operation = Operation;
				Cell.Type = Type;
				++Write;
			}
		}
	}
	checkf(Write == 128, TEXT("OpAssignmentTypeRejection AllCells must retain 128 reject cells"));
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FTypeRejectionCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FTypeRejectionCell> Cells = AllCells();
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

void AppendFieldOwner(FString& Source, const EOpAssignmentTypeRejectionType Type)
{
	AppendLine(Source, TEXT("struct FRejectedAssignmentField"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Value;"), ScriptType(Type)));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendEntry(
	FString& Source, const FOpAssignmentTypeRejectionParams& Params, const FString& FunctionName)
{
	const TCHAR* const TypeName = ScriptType(Params.Type);
	const TCHAR* const Catalog = TypeCatalogName(Params.Type);
	const TCHAR* const Symbol = OperationSymbol(Params.Operation);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Input = %s;"), TypeName, InputLiteral(Params.Type)));
	AppendLine(Source, FString::Printf(TEXT("\t%s Source = %s;"), TypeName, SourceLiteral(Params.Type)));
	if (Params.Category == EOpAssignmentTypeRejectionCategory::Local
		|| Params.Category == EOpAssignmentTypeRejectionCategory::Alias)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Value = Input;"), TypeName));
		AppendLine(
			Source,
			FString::Printf(TEXT("\tValue %s Source; // ASSIGNMENT_CAUSE"), Symbol));
	}
	else if (Params.Category == EOpAssignmentTypeRejectionCategory::Field)
	{
		AppendLine(Source, TEXT("\tFRejectedAssignmentField Owner;"));
		AppendLine(Source, TEXT("\tOwner.Value = Input;"));
		AppendLine(
			Source,
			FString::Printf(TEXT("\tOwner.Value %s Source; // ASSIGNMENT_CAUSE"), Symbol));
	}
	else
	{
		AppendLine(
			Source,
			FString::Printf(
				TEXT("\tFAssignmentProperty_%s Owner = MakeAssignmentProperty_%s();"),
				Catalog,
				Catalog));
		AppendLine(
			Source,
			FString::Printf(TEXT("\tOwner.Value %s Source; // ASSIGNMENT_CAUSE"), Symbol));
	}
	AppendLine(Source, TEXT("\treturn 0;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FOpAssignmentTypeRejectionParams& Params, const FString& FunctionName)
{
	FString Source;
	if (Params.Category == EOpAssignmentTypeRejectionCategory::Field)
	{
		AppendFieldOwner(Source, Params.Type);
	}
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FTypeRejectionCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.Outcome = EGeneratedCaseOutcome::CompileReject;
	Info.Execution = EGeneratedCaseExecution::SourceOnly;
	return Info;
}
}

FStringView FOpAssignmentTypeRejectionGenerator::GetProductId() const
{
	return TEXT("LANG-OP-ASSIGNMENT-TYPE-REJECTION");
}

TArray<FGeneratedCaseInfo> FOpAssignmentTypeRejectionGenerator::ListCases() const
{
	using namespace OpAssignmentTypeRejectionGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	const TArray<FTypeRejectionCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		Cases.Add(MakeCaseInfo(Cells[Index]));
	}
	return Cases;
}

FString FOpAssignmentTypeRejectionGenerator::BuildCaseSource(const FStringView CaseId) const
{
	return BuildRejectSource(CaseId);
}

FString FOpAssignmentTypeRejectionGenerator::BuildDumpSource() const
{
	using namespace OpAssignmentTypeRejectionGeneratorDetail;
	const TArray<FTypeRejectionCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-ASSIGNMENT-TYPE-REJECTION"));
	AppendLine(Source, TEXT("// Cases: 128 total, 0 normal, 128 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FTypeRejectionCell& Cell = Cells[Index];
		AppendLine(Source);
		AppendLine(Source, TEXT("// Reject module"));
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *MakeCaseId(Cell)));
		AppendLine(Source, TEXT("// EntryDeclaration:"));
		AppendLine(Source, TEXT("// Outcome: CompileReject"));
		AppendLine(Source, TEXT("// Execution: SourceOnly"));
		AppendLine(Source);
		if (Cell.Category == EOpAssignmentTypeRejectionCategory::Field)
		{
			AppendFieldOwner(Source, Cell.Type);
		}
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FOpAssignmentTypeRejectionGenerator::BuildAllSource(int32& OutCaseCount) const
{
	OutCaseCount = 0;
	return FString();
}

FString FOpAssignmentTypeRejectionGenerator::BuildTypeRejectionSource(
	const FOpAssignmentTypeRejectionParams& Params) const
{
	using namespace OpAssignmentTypeRejectionGeneratorDetail;
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

int32 FOpAssignmentTypeRejectionGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpAssignmentTypeRejectionGeneratorDetail;
	FTypeRejectionCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return 0;
}

FString FOpAssignmentTypeRejectionGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace OpAssignmentTypeRejectionGeneratorDetail;
	FTypeRejectionCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FOpAssignmentTypeRejectionGenerator::ListRejectCaseIds() const
{
	using namespace OpAssignmentTypeRejectionGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FTypeRejectionCell> Cells = AllCells();
	Ids.SetNum(Cells.Num());
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		Ids[Index] = MakeCaseId(Cells[Index]);
	}
	return Ids;
}
}
