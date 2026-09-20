#include "Framework/Generate/AngelscriptTestOpIncrementGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace OpIncrementGeneratorDetail
{
constexpr const TCHAR* PropertyNotes = TEXT("Needs native MakeIncrementProperty_* fixtures.");

struct FIncrementCell
{
	EOpIncrementCategory Category = EOpIncrementCategory::Local;
	EOpIncrementObservation Observation = EOpIncrementObservation::Before;
	EOpIncrementOperator Operator = EOpIncrementOperator::PreIncrement;
	EOpIncrementType Type = EOpIncrementType::Int;
};

const TCHAR* CategoryToken(const EOpIncrementCategory Category)
{
	switch (Category)
	{
	case EOpIncrementCategory::Local:
		return TEXT("LOCAL");
	case EOpIncrementCategory::Field:
		return TEXT("FIELD");
	case EOpIncrementCategory::Property:
		return TEXT("PROPERTY");
	case EOpIncrementCategory::Alias:
		return TEXT("ALIAS");
	default:
		return nullptr;
	}
}

const TCHAR* ObservationToken(const EOpIncrementObservation Observation)
{
	switch (Observation)
	{
	case EOpIncrementObservation::Before:
		return TEXT("BEFORE");
	case EOpIncrementObservation::ExpressionResult:
		return TEXT("EXPRESSION_RESULT");
	case EOpIncrementObservation::After:
		return TEXT("AFTER");
	default:
		return nullptr;
	}
}

const TCHAR* OperatorToken(const EOpIncrementOperator Operator)
{
	switch (Operator)
	{
	case EOpIncrementOperator::PreIncrement:
		return TEXT("PRE_INCREMENT");
	case EOpIncrementOperator::PostIncrement:
		return TEXT("POST_INCREMENT");
	case EOpIncrementOperator::PreDecrement:
		return TEXT("PRE_DECREMENT");
	case EOpIncrementOperator::PostDecrement:
		return TEXT("POST_DECREMENT");
	default:
		return nullptr;
	}
}

const TCHAR* TypeToken(const EOpIncrementType Type)
{
	switch (Type)
	{
	case EOpIncrementType::Int8:
		return TEXT("INT8");
	case EOpIncrementType::Int16:
		return TEXT("INT16");
	case EOpIncrementType::Int:
		return TEXT("INT");
	case EOpIncrementType::Int64:
		return TEXT("INT64");
	case EOpIncrementType::Uint8:
		return TEXT("UINT8");
	case EOpIncrementType::Uint16:
		return TEXT("UINT16");
	case EOpIncrementType::Uint:
		return TEXT("UINT");
	case EOpIncrementType::Uint64:
		return TEXT("UINT64");
	case EOpIncrementType::Float32:
		return TEXT("FLOAT32");
	case EOpIncrementType::Float64:
		return TEXT("FLOAT64");
	default:
		return nullptr;
	}
}

const TCHAR* ScriptType(const EOpIncrementType Type)
{
	switch (Type)
	{
	case EOpIncrementType::Int8:
		return TEXT("int8");
	case EOpIncrementType::Int16:
		return TEXT("int16");
	case EOpIncrementType::Int:
		return TEXT("int");
	case EOpIncrementType::Int64:
		return TEXT("int64");
	case EOpIncrementType::Uint8:
		return TEXT("uint8");
	case EOpIncrementType::Uint16:
		return TEXT("uint16");
	case EOpIncrementType::Uint:
		return TEXT("uint");
	case EOpIncrementType::Uint64:
		return TEXT("uint64");
	case EOpIncrementType::Float32:
		return TEXT("float32");
	case EOpIncrementType::Float64:
		return TEXT("double");
	default:
		return nullptr;
	}
}

const TCHAR* CatalogType(const EOpIncrementType Type)
{
	switch (Type)
	{
	case EOpIncrementType::Int8:
		return TEXT("int8");
	case EOpIncrementType::Int16:
		return TEXT("int16");
	case EOpIncrementType::Int:
		return TEXT("int");
	case EOpIncrementType::Int64:
		return TEXT("int64");
	case EOpIncrementType::Uint8:
		return TEXT("uint8");
	case EOpIncrementType::Uint16:
		return TEXT("uint16");
	case EOpIncrementType::Uint:
		return TEXT("uint");
	case EOpIncrementType::Uint64:
		return TEXT("uint64");
	case EOpIncrementType::Float32:
		return TEXT("float32");
	case EOpIncrementType::Float64:
		return TEXT("float64");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FOpIncrementParams& Params)
{
	return CategoryToken(Params.Category) != nullptr
		&& ObservationToken(Params.Observation) != nullptr
		&& OperatorToken(Params.Operator) != nullptr
		&& TypeToken(Params.Type) != nullptr;
}

bool IsIncrement(const EOpIncrementOperator Operator)
{
	return Operator == EOpIncrementOperator::PreIncrement
		|| Operator == EOpIncrementOperator::PostIncrement;
}

bool IsPrefix(const EOpIncrementOperator Operator)
{
	return Operator == EOpIncrementOperator::PreIncrement
		|| Operator == EOpIncrementOperator::PreDecrement;
}

bool IsWideOrFloat(const EOpIncrementType Type)
{
	return Type == EOpIncrementType::Int64
		|| Type == EOpIncrementType::Uint64
		|| Type == EOpIncrementType::Float32
		|| Type == EOpIncrementType::Float64;
}

int32 ExpectedFromCell(const FIncrementCell& Cell)
{
	const int32 Initial = 12;
	const int32 Final = IsIncrement(Cell.Operator) ? 13 : 11;
	if (Cell.Observation == EOpIncrementObservation::Before)
	{
		return Initial;
	}
	if (Cell.Observation == EOpIncrementObservation::ExpressionResult)
	{
		return IsPrefix(Cell.Operator) ? Final : Initial;
	}
	return Final;
}

FString OperatorExpression(const EOpIncrementOperator Operator, const FString& Target)
{
	switch (Operator)
	{
	case EOpIncrementOperator::PreIncrement:
		return TEXT("++") + Target;
	case EOpIncrementOperator::PostIncrement:
		return Target + TEXT("++");
	case EOpIncrementOperator::PreDecrement:
		return TEXT("--") + Target;
	case EOpIncrementOperator::PostDecrement:
		return Target + TEXT("--");
	default:
		return Target;
	}
}

FString OperatorSuffix(const EOpIncrementOperator Operator)
{
	switch (Operator)
	{
	case EOpIncrementOperator::PreIncrement:
		return TEXT("PreIncrement");
	case EOpIncrementOperator::PostIncrement:
		return TEXT("PostIncrement");
	case EOpIncrementOperator::PreDecrement:
		return TEXT("PreDecrement");
	case EOpIncrementOperator::PostDecrement:
		return TEXT("PostDecrement");
	default:
		return TEXT("Increment");
	}
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FIncrementCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-INCREMENT-%s-%s-%s-%s"),
		CategoryToken(Cell.Category),
		ObservationToken(Cell.Observation),
		OperatorToken(Cell.Operator),
		TypeToken(Cell.Type));
}

FString MakeEntryName(const FIncrementCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpIncrementParams ParamsFromCell(const FIncrementCell& Cell)
{
	FOpIncrementParams Params;
	Params.Category = Cell.Category;
	Params.Observation = Cell.Observation;
	Params.Operator = Cell.Operator;
	Params.Type = Cell.Type;
	return Params;
}

TArray<FIncrementCell> AllCells()
{
	TArray<FIncrementCell> Cells;
	Cells.SetNum(480);
	for (int32 Index = 0; Index < 480; ++Index)
	{
		FIncrementCell& Cell = Cells[Index];
		Cell.Category = static_cast<EOpIncrementCategory>(Index / 120);
		Cell.Observation = static_cast<EOpIncrementObservation>((Index / 40) % 3);
		Cell.Operator = static_cast<EOpIncrementOperator>((Index / 10) % 4);
		Cell.Type = static_cast<EOpIncrementType>(Index % 10);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FIncrementCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FIncrementCell> Cells = AllCells();
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

FString FieldOwnerName(const EOpIncrementType Type)
{
	return FString::Printf(TEXT("FIncrementFieldOwner_%s"), CatalogType(Type));
}

FString AliasName(const EOpIncrementOperator Operator, const EOpIncrementType Type)
{
	return FString::Printf(TEXT("Apply%sAlias_%s"), *OperatorSuffix(Operator), CatalogType(Type));
}

FString PropertyTypeName(const EOpIncrementType Type)
{
	return FString::Printf(TEXT("FIncrementProperty_%s"), CatalogType(Type));
}

FString PropertyFactoryName(const EOpIncrementType Type)
{
	return FString::Printf(TEXT("MakeIncrementProperty_%s"), CatalogType(Type));
}

FString InputLiteral(const EOpIncrementType Type)
{
	if (Type == EOpIncrementType::Float32)
	{
		return TEXT("12.5");
	}
	if (Type == EOpIncrementType::Float64)
	{
		return TEXT("12.5");
	}
	return TEXT("12");
}

void AppendFieldOwner(FString& Source, const EOpIncrementType Type)
{
	AppendLine(Source, FString::Printf(TEXT("struct %s"), *FieldOwnerName(Type)));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Value;"), ScriptType(Type)));
	AppendLine(Source, TEXT("}"));
}

void AppendAliasHelper(FString& Source, const EOpIncrementOperator Operator, const EOpIncrementType Type)
{
	const TCHAR* const TypeName = ScriptType(Type);
	AppendLine(Source, FString::Printf(
		TEXT("%s %s(%s& inout Value)"),
		TypeName,
		*AliasName(Operator, Type),
		TypeName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *OperatorExpression(Operator, TEXT("Value"))));
	AppendLine(Source, TEXT("}"));
}

void AppendEntry(FString& Source, const FOpIncrementParams& Params, const FString& FunctionName)
{
	const TCHAR* const TypeName = ScriptType(Params.Type);
	const FString Literal = InputLiteral(Params.Type);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Input = %s;"), TypeName, *Literal));
	if (Params.Category == EOpIncrementCategory::Local)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Value = Input;"), TypeName));
		if (Params.Observation == EOpIncrementObservation::Before)
		{
			AppendLine(Source, FString::Printf(TEXT("\t%s Before = Value;"), TypeName));
			AppendLine(Source, FString::Printf(TEXT("\t%s;"), *OperatorExpression(Params.Operator, TEXT("Value"))));
			AppendLine(Source, TEXT("\treturn int(Before);"));
		}
		else if (Params.Observation == EOpIncrementObservation::ExpressionResult)
		{
			AppendLine(Source, FString::Printf(TEXT("\t%s Result = %s;"), TypeName, *OperatorExpression(Params.Operator, TEXT("Value"))));
			AppendLine(Source, TEXT("\treturn int(Result);"));
		}
		else
		{
			AppendLine(Source, FString::Printf(TEXT("\t%s;"), *OperatorExpression(Params.Operator, TEXT("Value"))));
			AppendLine(Source, TEXT("\treturn int(Value);"));
		}
	}
	else if (Params.Category == EOpIncrementCategory::Field)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Owner;"), *FieldOwnerName(Params.Type)));
		AppendLine(Source, TEXT("\tOwner.Value = Input;"));
		if (Params.Observation == EOpIncrementObservation::Before)
		{
			AppendLine(Source, FString::Printf(TEXT("\t%s Before = Owner.Value;"), TypeName));
			AppendLine(Source, FString::Printf(TEXT("\t%s;"), *OperatorExpression(Params.Operator, TEXT("Owner.Value"))));
			AppendLine(Source, TEXT("\treturn int(Before);"));
		}
		else if (Params.Observation == EOpIncrementObservation::ExpressionResult)
		{
			AppendLine(Source, FString::Printf(
				TEXT("\t%s Result = %s;"),
				TypeName,
				*OperatorExpression(Params.Operator, TEXT("Owner.Value"))));
			AppendLine(Source, TEXT("\treturn int(Result);"));
		}
		else
		{
			AppendLine(Source, FString::Printf(TEXT("\t%s;"), *OperatorExpression(Params.Operator, TEXT("Owner.Value"))));
			AppendLine(Source, TEXT("\treturn int(Owner.Value);"));
		}
	}
	else if (Params.Category == EOpIncrementCategory::Property)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s Owner = %s();"),
			*PropertyTypeName(Params.Type),
			*PropertyFactoryName(Params.Type)));
		AppendLine(Source, FString::Printf(TEXT("\t%s Before = Owner.get_Value();"), TypeName));
		AppendLine(Source, FString::Printf(TEXT("\t%s Updated = Before;"), TypeName));
		AppendLine(Source, FString::Printf(TEXT("\t%s;"), *OperatorExpression(Params.Operator, TEXT("Updated"))));
		AppendLine(Source, TEXT("\tOwner.set_Value(Updated);"));
		if (Params.Observation == EOpIncrementObservation::Before)
		{
			AppendLine(Source, TEXT("\treturn int(Before);"));
		}
		else if (Params.Observation == EOpIncrementObservation::ExpressionResult)
		{
			AppendLine(Source, IsPrefix(Params.Operator) ? TEXT("\treturn int(Updated);") : TEXT("\treturn int(Before);"));
		}
		else
		{
			AppendLine(Source, TEXT("\treturn int(Owner.get_Value());"));
		}
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Value = Input;"), TypeName));
		if (Params.Observation == EOpIncrementObservation::Before)
		{
			AppendLine(Source, FString::Printf(TEXT("\t%s Before = Value;"), TypeName));
			AppendLine(Source, FString::Printf(TEXT("\t%s(Value);"), *AliasName(Params.Operator, Params.Type)));
			AppendLine(Source, TEXT("\treturn int(Before);"));
		}
		else if (Params.Observation == EOpIncrementObservation::ExpressionResult)
		{
			AppendLine(Source, FString::Printf(
				TEXT("\t%s Result = %s(Value);"),
				TypeName,
				*AliasName(Params.Operator, Params.Type)));
			AppendLine(Source, TEXT("\treturn int(Result);"));
		}
		else
		{
			AppendLine(Source, FString::Printf(TEXT("\t%s(Value);"), *AliasName(Params.Operator, Params.Type)));
			AppendLine(Source, TEXT("\treturn int(Value);"));
		}
	}
	AppendLine(Source, TEXT("}"));
}

void AppendOwnedHelpers(FString& Source, const FOpIncrementParams& Params)
{
	if (Params.Category == EOpIncrementCategory::Field)
	{
		AppendFieldOwner(Source, Params.Type);
		AppendLine(Source);
	}
	else if (Params.Category == EOpIncrementCategory::Alias)
	{
		AppendAliasHelper(Source, Params.Operator, Params.Type);
		AppendLine(Source);
	}
}

FString BuildOwnedSource(const FOpIncrementParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOwnedHelpers(Source, Params);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FIncrementCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	if (Cell.Category == EOpIncrementCategory::Property)
	{
		Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
		Info.ExecutionNotes = PropertyNotes;
		Info.bLimitedObservation = IsWideOrFloat(Cell.Type);
	}
	else
	{
		Info.Execution = EGeneratedCaseExecution::Standalone;
		if (IsWideOrFloat(Cell.Type))
		{
			Info.bLimitedObservation = true;
			Info.ExecutionNotes = TEXT("int32 truncates 64-bit and float increment observations.");
		}
	}
	return Info;
}
}

FStringView FOpIncrementGenerator::GetProductId() const
{
	return TEXT("LANG-OP-INCREMENT");
}

TArray<FGeneratedCaseInfo> FOpIncrementGenerator::ListCases() const
{
	using namespace OpIncrementGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FIncrementCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		const FString EntryName = MakeEntryName(Cells[Index]);
		checkf(!EntryNames.Contains(EntryName), TEXT("OpIncrement PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FOpIncrementGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace OpIncrementGeneratorDetail;
	FIncrementCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FOpIncrementGenerator::BuildDumpSource() const
{
	using namespace OpIncrementGeneratorDetail;
	const TArray<FIncrementCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-INCREMENT"));
	AppendLine(Source, TEXT("// Cases: 480 total, 480 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	bool bFieldOwners[10] = {};
	bool bAliases[40] = {};
	for (int32 TypeIndex = 0; TypeIndex < 10; ++TypeIndex)
	{
		const EOpIncrementType Type = static_cast<EOpIncrementType>(TypeIndex);
		AppendFieldOwner(Source, Type);
		AppendLine(Source);
		bFieldOwners[TypeIndex] = true;
	}
	for (int32 OperatorIndex = 0; OperatorIndex < 4; ++OperatorIndex)
	{
		for (int32 TypeIndex = 0; TypeIndex < 10; ++TypeIndex)
		{
			AppendAliasHelper(
				Source,
				static_cast<EOpIncrementOperator>(OperatorIndex),
				static_cast<EOpIncrementType>(TypeIndex));
			AppendLine(Source);
			bAliases[OperatorIndex * 10 + TypeIndex] = true;
		}
	}
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		if (Info.Execution == EGeneratedCaseExecution::RequiresHostSetup)
		{
			AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
			AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		}
		else
		{
			AppendLine(Source, TEXT("// Execution: Standalone"));
			if (Info.bLimitedObservation)
			{
				AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
			}
		}
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		if (Index + 1 < Cells.Num())
		{
			AppendLine(Source);
		}
	}
	(void)bFieldOwners;
	(void)bAliases;
	return Source;
}

FString FOpIncrementGenerator::BuildLegalSource(const FOpIncrementParams& Params) const
{
	using namespace OpIncrementGeneratorDetail;
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

FString FOpIncrementGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace OpIncrementGeneratorDetail;
	const TArray<FIncrementCell> Cells = AllCells();
	FString Source;
	for (int32 TypeIndex = 0; TypeIndex < 10; ++TypeIndex)
	{
		AppendFieldOwner(Source, static_cast<EOpIncrementType>(TypeIndex));
		AppendLine(Source);
	}
	for (int32 OperatorIndex = 0; OperatorIndex < 4; ++OperatorIndex)
	{
		for (int32 TypeIndex = 0; TypeIndex < 10; ++TypeIndex)
		{
			AppendAliasHelper(
				Source,
				static_cast<EOpIncrementOperator>(OperatorIndex),
				static_cast<EOpIncrementType>(TypeIndex));
			AppendLine(Source);
		}
	}
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		if (Index + 1 < Cells.Num())
		{
			AppendLine(Source);
		}
	}
	OutCaseCount = Cells.Num();
	return Source;
}

int32 FOpIncrementGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpIncrementGeneratorDetail;
	FIncrementCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}
}
