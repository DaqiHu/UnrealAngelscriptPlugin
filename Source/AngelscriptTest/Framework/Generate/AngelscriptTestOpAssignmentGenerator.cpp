#include "Framework/Generate/AngelscriptTestOpAssignmentGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace OpAssignmentGeneratorDetail
{
constexpr const TCHAR* HostNotes =
	TEXT("Needs ObserveAssignment_* and assignment property fixtures.");

struct FAssignmentCell
{
	EOpAssignmentCategory Category = EOpAssignmentCategory::Local;
	EOpAssignmentOperation Operation = EOpAssignmentOperation::Assign;
	EOpAssignmentType Type = EOpAssignmentType::Int;
};

const TCHAR* CategoryToken(const EOpAssignmentCategory Category)
{
	switch (Category)
	{
	case EOpAssignmentCategory::Local:
		return TEXT("LOCAL");
	case EOpAssignmentCategory::Field:
		return TEXT("FIELD");
	case EOpAssignmentCategory::Property:
		return TEXT("PROPERTY");
	case EOpAssignmentCategory::Alias:
		return TEXT("ALIAS");
	default:
		return nullptr;
	}
}

const TCHAR* OperationToken(const EOpAssignmentOperation Operation)
{
	switch (Operation)
	{
	case EOpAssignmentOperation::Assign:
		return TEXT("ASSIGN");
	case EOpAssignmentOperation::AddAssign:
		return TEXT("ADD_ASSIGN");
	case EOpAssignmentOperation::SubtractAssign:
		return TEXT("SUBTRACT_ASSIGN");
	case EOpAssignmentOperation::MultiplyAssign:
		return TEXT("MULTIPLY_ASSIGN");
	case EOpAssignmentOperation::DivideAssign:
		return TEXT("DIVIDE_ASSIGN");
	case EOpAssignmentOperation::ModuloAssign:
		return TEXT("MODULO_ASSIGN");
	case EOpAssignmentOperation::PowerAssign:
		return TEXT("POWER_ASSIGN");
	case EOpAssignmentOperation::AndAssign:
		return TEXT("AND_ASSIGN");
	case EOpAssignmentOperation::OrAssign:
		return TEXT("OR_ASSIGN");
	case EOpAssignmentOperation::XorAssign:
		return TEXT("XOR_ASSIGN");
	case EOpAssignmentOperation::ShiftLeftAssign:
		return TEXT("SHIFT_LEFT_ASSIGN");
	case EOpAssignmentOperation::ShiftRightLogicalAssign:
		return TEXT("SHIFT_RIGHT_LOGICAL_ASSIGN");
	case EOpAssignmentOperation::ShiftRightArithmeticAssign:
		return TEXT("SHIFT_RIGHT_ARITHMETIC_ASSIGN");
	default:
		return nullptr;
	}
}

const TCHAR* TypeToken(const EOpAssignmentType Type)
{
	switch (Type)
	{
	case EOpAssignmentType::Int8:
		return TEXT("INT8");
	case EOpAssignmentType::Int16:
		return TEXT("INT16");
	case EOpAssignmentType::Int:
		return TEXT("INT");
	case EOpAssignmentType::Int64:
		return TEXT("INT64");
	case EOpAssignmentType::Uint8:
		return TEXT("UINT8");
	case EOpAssignmentType::Uint16:
		return TEXT("UINT16");
	case EOpAssignmentType::Uint:
		return TEXT("UINT");
	case EOpAssignmentType::Uint64:
		return TEXT("UINT64");
	case EOpAssignmentType::Float32:
		return TEXT("FLOAT32");
	case EOpAssignmentType::Float64:
		return TEXT("FLOAT64");
	case EOpAssignmentType::Bool:
		return TEXT("BOOL");
	default:
		return nullptr;
	}
}

const TCHAR* TypeCatalogName(const EOpAssignmentType Type)
{
	switch (Type)
	{
	case EOpAssignmentType::Int8:
		return TEXT("int8");
	case EOpAssignmentType::Int16:
		return TEXT("int16");
	case EOpAssignmentType::Int:
		return TEXT("int");
	case EOpAssignmentType::Int64:
		return TEXT("int64");
	case EOpAssignmentType::Uint8:
		return TEXT("uint8");
	case EOpAssignmentType::Uint16:
		return TEXT("uint16");
	case EOpAssignmentType::Uint:
		return TEXT("uint");
	case EOpAssignmentType::Uint64:
		return TEXT("uint64");
	case EOpAssignmentType::Float32:
		return TEXT("float32");
	case EOpAssignmentType::Float64:
		return TEXT("float64");
	case EOpAssignmentType::Bool:
		return TEXT("bool");
	default:
		return nullptr;
	}
}

const TCHAR* ScriptType(const EOpAssignmentType Type)
{
	if (Type == EOpAssignmentType::Float64)
	{
		return TEXT("double");
	}
	return TypeCatalogName(Type);
}

const TCHAR* OperationSymbol(const EOpAssignmentOperation Operation)
{
	switch (Operation)
	{
	case EOpAssignmentOperation::Assign:
		return TEXT("=");
	case EOpAssignmentOperation::AddAssign:
		return TEXT("+=");
	case EOpAssignmentOperation::SubtractAssign:
		return TEXT("-=");
	case EOpAssignmentOperation::MultiplyAssign:
		return TEXT("*=");
	case EOpAssignmentOperation::DivideAssign:
		return TEXT("/=");
	case EOpAssignmentOperation::ModuloAssign:
		return TEXT("%=");
	case EOpAssignmentOperation::PowerAssign:
		return TEXT("**=");
	case EOpAssignmentOperation::AndAssign:
		return TEXT("&=");
	case EOpAssignmentOperation::OrAssign:
		return TEXT("|=");
	case EOpAssignmentOperation::XorAssign:
		return TEXT("^=");
	case EOpAssignmentOperation::ShiftLeftAssign:
		return TEXT("<<=");
	case EOpAssignmentOperation::ShiftRightLogicalAssign:
		return TEXT(">>=");
	case EOpAssignmentOperation::ShiftRightArithmeticAssign:
		return TEXT(">>>=");
	default:
		return nullptr;
	}
}

const TCHAR* AliasFunctionSuffix(const EOpAssignmentOperation Operation)
{
	switch (Operation)
	{
	case EOpAssignmentOperation::Assign:
		return TEXT("Assign");
	case EOpAssignmentOperation::AddAssign:
		return TEXT("AddAssign");
	case EOpAssignmentOperation::SubtractAssign:
		return TEXT("SubtractAssign");
	case EOpAssignmentOperation::MultiplyAssign:
		return TEXT("MultiplyAssign");
	case EOpAssignmentOperation::DivideAssign:
		return TEXT("DivideAssign");
	case EOpAssignmentOperation::ModuloAssign:
		return TEXT("ModuloAssign");
	case EOpAssignmentOperation::PowerAssign:
		return TEXT("PowerAssign");
	case EOpAssignmentOperation::AndAssign:
		return TEXT("AndAssign");
	case EOpAssignmentOperation::OrAssign:
		return TEXT("OrAssign");
	case EOpAssignmentOperation::XorAssign:
		return TEXT("XorAssign");
	case EOpAssignmentOperation::ShiftLeftAssign:
		return TEXT("ShiftLeftAssign");
	case EOpAssignmentOperation::ShiftRightLogicalAssign:
		return TEXT("ShiftRightLogicalAssign");
	case EOpAssignmentOperation::ShiftRightArithmeticAssign:
		return TEXT("ShiftRightArithmeticAssign");
	default:
		return nullptr;
	}
}

const TCHAR* InputLiteral(const EOpAssignmentType Type)
{
	if (Type == EOpAssignmentType::Bool)
	{
		return TEXT("false");
	}
	if (Type == EOpAssignmentType::Float32)
	{
		return TEXT("4.0f");
	}
	if (Type == EOpAssignmentType::Float64)
	{
		return TEXT("4.0");
	}
	return TEXT("4");
}

const TCHAR* SourceLiteral(const EOpAssignmentType Type)
{
	if (Type == EOpAssignmentType::Bool)
	{
		return TEXT("true");
	}
	if (Type == EOpAssignmentType::Float32)
	{
		return TEXT("2.0f");
	}
	if (Type == EOpAssignmentType::Float64)
	{
		return TEXT("2.0");
	}
	return TEXT("2");
}

bool IsIntegralType(const EOpAssignmentType Type)
{
	return Type <= EOpAssignmentType::Uint64;
}

bool IsNumericType(const EOpAssignmentType Type)
{
	return Type <= EOpAssignmentType::Float64;
}

bool SupportsOperation(const EOpAssignmentOperation Operation, const EOpAssignmentType Type)
{
	switch (Operation)
	{
	case EOpAssignmentOperation::Assign:
		return true;
	case EOpAssignmentOperation::AddAssign:
	case EOpAssignmentOperation::SubtractAssign:
	case EOpAssignmentOperation::MultiplyAssign:
	case EOpAssignmentOperation::DivideAssign:
	case EOpAssignmentOperation::ModuloAssign:
		return IsNumericType(Type);
	case EOpAssignmentOperation::PowerAssign:
		return Type == EOpAssignmentType::Float32 || Type == EOpAssignmentType::Float64;
	case EOpAssignmentOperation::AndAssign:
	case EOpAssignmentOperation::OrAssign:
	case EOpAssignmentOperation::XorAssign:
	case EOpAssignmentOperation::ShiftLeftAssign:
	case EOpAssignmentOperation::ShiftRightLogicalAssign:
	case EOpAssignmentOperation::ShiftRightArithmeticAssign:
		return IsIntegralType(Type);
	default:
		return false;
	}
}

bool IsValidParams(const FOpAssignmentParams& Params)
{
	return CategoryToken(Params.Category) != nullptr
		&& OperationToken(Params.Operation) != nullptr
		&& TypeToken(Params.Type) != nullptr
		&& SupportsOperation(Params.Operation, Params.Type);
}

bool IsLimitedObservation(const EOpAssignmentType Type)
{
	return Type == EOpAssignmentType::Int64
		|| Type == EOpAssignmentType::Uint64
		|| Type == EOpAssignmentType::Float32
		|| Type == EOpAssignmentType::Float64;
}

int32 ExpectedFromCell(const FAssignmentCell& Cell)
{
	if (Cell.Type == EOpAssignmentType::Bool)
	{
		return 1;
	}
	switch (Cell.Operation)
	{
	case EOpAssignmentOperation::Assign:
		return 2;
	case EOpAssignmentOperation::AddAssign:
		return 6;
	case EOpAssignmentOperation::SubtractAssign:
		return 2;
	case EOpAssignmentOperation::MultiplyAssign:
		return 8;
	case EOpAssignmentOperation::DivideAssign:
		return 2;
	case EOpAssignmentOperation::ModuloAssign:
		return 0;
	case EOpAssignmentOperation::PowerAssign:
		return 16;
	case EOpAssignmentOperation::AndAssign:
		return 0;
	case EOpAssignmentOperation::OrAssign:
		return 6;
	case EOpAssignmentOperation::XorAssign:
		return 6;
	case EOpAssignmentOperation::ShiftLeftAssign:
		return 16;
	case EOpAssignmentOperation::ShiftRightLogicalAssign:
	case EOpAssignmentOperation::ShiftRightArithmeticAssign:
		return 1;
	default:
		return 0;
	}
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FAssignmentCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-ASSIGNMENT-%s-%s_%s"),
		CategoryToken(Cell.Category),
		OperationToken(Cell.Operation),
		TypeToken(Cell.Type));
}

FString MakeEntryName(const FAssignmentCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FString MakeAliasName(const EOpAssignmentOperation Operation, const EOpAssignmentType Type)
{
	return FString::Printf(TEXT("Apply%sAlias_%s"), AliasFunctionSuffix(Operation), TypeCatalogName(Type));
}

FString MakeFieldOwnerName(const EOpAssignmentType Type)
{
	return FString::Printf(TEXT("FAssignmentFieldOwner_%s"), TypeCatalogName(Type));
}

FOpAssignmentParams ParamsFromCell(const FAssignmentCell& Cell)
{
	FOpAssignmentParams Params;
	Params.Category = Cell.Category;
	Params.Operation = Cell.Operation;
	Params.Type = Cell.Type;
	return Params;
}

TArray<FAssignmentCell> AllCells()
{
	TArray<FAssignmentCell> Cells;
	Cells.SetNum(444);
	int32 Write = 0;
	for (int32 CategoryIndex = 0; CategoryIndex < 4; ++CategoryIndex)
	{
		for (int32 OperationIndex = 0; OperationIndex < 13; ++OperationIndex)
		{
			for (int32 TypeIndex = 0; TypeIndex < 11; ++TypeIndex)
			{
				const EOpAssignmentOperation Operation =
					static_cast<EOpAssignmentOperation>(OperationIndex);
				const EOpAssignmentType Type = static_cast<EOpAssignmentType>(TypeIndex);
				if (!SupportsOperation(Operation, Type))
				{
					continue;
				}
				FAssignmentCell& Cell = Cells[Write];
				Cell.Category = static_cast<EOpAssignmentCategory>(CategoryIndex);
				Cell.Operation = Operation;
				Cell.Type = Type;
				++Write;
			}
		}
	}
	checkf(Write == 444, TEXT("OpAssignment AllCells must retain 444 supported cells"));
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FAssignmentCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FAssignmentCell> Cells = AllCells();
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

void AppendFieldOwner(FString& Source, const EOpAssignmentType Type)
{
	AppendLine(Source, FString::Printf(TEXT("struct %s"), *MakeFieldOwnerName(Type)));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Value;"), ScriptType(Type)));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendAliasHelper(
	FString& Source, const EOpAssignmentOperation Operation, const EOpAssignmentType Type)
{
	const TCHAR* const TypeName = ScriptType(Type);
	const TCHAR* const Catalog = TypeCatalogName(Type);
	AppendLine(
		Source,
		FString::Printf(
			TEXT("%s %s(%s& inout Value, %s Source)"),
			TypeName,
			*MakeAliasName(Operation, Type),
			TypeName,
			TypeName));
	AppendLine(Source, TEXT("{"));
	AppendLine(
		Source,
		FString::Printf(TEXT("\tValue %s Source;"), OperationSymbol(Operation)));
	AppendLine(Source, FString::Printf(TEXT("\t%s Result = Value;"), TypeName));
	AppendLine(
		Source,
		FString::Printf(TEXT("\tObserveAssignment_%s(Result, Value);"), Catalog));
	AppendLine(Source, TEXT("\treturn Result;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendSharedHelpers(FString& Source)
{
	for (int32 TypeIndex = 0; TypeIndex < 11; ++TypeIndex)
	{
		AppendFieldOwner(Source, static_cast<EOpAssignmentType>(TypeIndex));
	}
	for (int32 OperationIndex = 0; OperationIndex < 13; ++OperationIndex)
	{
		for (int32 TypeIndex = 0; TypeIndex < 11; ++TypeIndex)
		{
			const EOpAssignmentOperation Operation =
				static_cast<EOpAssignmentOperation>(OperationIndex);
			const EOpAssignmentType Type = static_cast<EOpAssignmentType>(TypeIndex);
			if (SupportsOperation(Operation, Type))
			{
				AppendAliasHelper(Source, Operation, Type);
			}
		}
	}
}

void AppendReturn(FString& Source, const EOpAssignmentType Type)
{
	if (Type == EOpAssignmentType::Bool)
	{
		AppendLine(Source, TEXT("\treturn Result ? 1 : 0;"));
		return;
	}
	AppendLine(Source, TEXT("\treturn int(Result);"));
}

void AppendEntry(FString& Source, const FOpAssignmentParams& Params, const FString& FunctionName)
{
	const TCHAR* const TypeName = ScriptType(Params.Type);
	const TCHAR* const Catalog = TypeCatalogName(Params.Type);
	const TCHAR* const Symbol = OperationSymbol(Params.Operation);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Input = %s;"), TypeName, InputLiteral(Params.Type)));
	AppendLine(Source, FString::Printf(TEXT("\t%s Source = %s;"), TypeName, SourceLiteral(Params.Type)));
	if (Params.Category == EOpAssignmentCategory::Local)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Value = Input;"), TypeName));
		AppendLine(Source, FString::Printf(TEXT("\tValue %s Source;"), Symbol));
		AppendLine(Source, FString::Printf(TEXT("\t%s Result = Value;"), TypeName));
		AppendLine(Source, FString::Printf(TEXT("\tObserveAssignment_%s(Result, Value);"), Catalog));
	}
	else if (Params.Category == EOpAssignmentCategory::Field)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Owner;"), *MakeFieldOwnerName(Params.Type)));
		AppendLine(Source, TEXT("\tOwner.Value = Input;"));
		AppendLine(Source, FString::Printf(TEXT("\tOwner.Value %s Source;"), Symbol));
		AppendLine(Source, FString::Printf(TEXT("\t%s Result = Owner.Value;"), TypeName));
		AppendLine(
			Source,
			FString::Printf(TEXT("\tObserveAssignment_%s(Result, Owner.Value);"), Catalog));
	}
	else if (Params.Category == EOpAssignmentCategory::Property)
	{
		AppendLine(
			Source,
			FString::Printf(
				TEXT("\tFAssignmentProperty_%s Owner = MakeAssignmentProperty_%s();"),
				Catalog,
				Catalog));
		if (Params.Operation == EOpAssignmentOperation::Assign)
		{
			AppendLine(Source, FString::Printf(TEXT("\t%s Result = Source;"), TypeName));
		}
		else
		{
			AppendLine(Source, FString::Printf(TEXT("\t%s Current = Owner.get_Value();"), TypeName));
			AppendLine(Source, FString::Printf(TEXT("\tCurrent %s Source;"), Symbol));
			AppendLine(Source, FString::Printf(TEXT("\t%s Result = Current;"), TypeName));
		}
		AppendLine(Source, TEXT("\tOwner.set_Value(Result);"));
		AppendLine(
			Source,
			FString::Printf(TEXT("\tObserveAssignment_%s(Result, Owner.get_Value());"), Catalog));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Value = Input;"), TypeName));
		AppendLine(
			Source,
			FString::Printf(
				TEXT("\t%s Result = %s(Value, Source);"),
				TypeName,
				*MakeAliasName(Params.Operation, Params.Type)));
	}
	AppendReturn(Source, Params.Type);
	AppendLine(Source, TEXT("}"));
}

void AppendOwnedHelpers(FString& Source, const FOpAssignmentParams& Params)
{
	if (Params.Category == EOpAssignmentCategory::Field)
	{
		AppendFieldOwner(Source, Params.Type);
	}
	else if (Params.Category == EOpAssignmentCategory::Alias)
	{
		AppendAliasHelper(Source, Params.Operation, Params.Type);
	}
}

FString BuildOwnedSource(const FOpAssignmentParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOwnedHelpers(Source, Params);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FAssignmentCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	Info.bLimitedObservation = IsLimitedObservation(Cell.Type);
	Info.ExecutionNotes = HostNotes;
	return Info;
}
}

FStringView FOpAssignmentGenerator::GetProductId() const
{
	return TEXT("LANG-OP-ASSIGNMENT");
}

TArray<FGeneratedCaseInfo> FOpAssignmentGenerator::ListCases() const
{
	using namespace OpAssignmentGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FAssignmentCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		const FString EntryName = MakeEntryName(Cells[Index]);
		checkf(!EntryNames.Contains(EntryName), TEXT("OpAssignment PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FOpAssignmentGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace OpAssignmentGeneratorDetail;
	FAssignmentCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FOpAssignmentGenerator::BuildDumpSource() const
{
	using namespace OpAssignmentGeneratorDetail;
	const TArray<FAssignmentCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-ASSIGNMENT"));
	AppendLine(Source, TEXT("// Cases: 444 total, 444 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FAssignmentCell& Cell = Cells[Index];
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		if (Info.bLimitedObservation)
		{
			AppendLine(Source, TEXT("// LimitedObservation: true"));
		}
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FOpAssignmentGenerator::BuildLegalSource(const FOpAssignmentParams& Params) const
{
	using namespace OpAssignmentGeneratorDetail;
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

FString FOpAssignmentGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace OpAssignmentGeneratorDetail;
	const TArray<FAssignmentCell> Cells = AllCells();
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

int32 FOpAssignmentGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpAssignmentGeneratorDetail;
	FAssignmentCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}
}
