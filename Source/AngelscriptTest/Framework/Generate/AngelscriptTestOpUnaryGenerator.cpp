#include "Framework/Generate/AngelscriptTestOpUnaryGenerator.h"

#include "Containers/Map.h"
#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace OpUnaryGeneratorDetail
{
enum class EUnaryOp : uint8
{
	Positive,
	Negative,
	BitNot,
};

struct FUnaryCell
{
	EOpUnaryCategory Category = EOpUnaryCategory::MutableLvalue;
	EOpUnaryOperation Operation = EOpUnaryOperation::PositiveInt8;
	EOpUnaryValue Value = EOpUnaryValue::Zero;
};

const TCHAR* CategoryToken(const EOpUnaryCategory Category)
{
	switch (Category)
	{
	case EOpUnaryCategory::MutableLvalue:
		return TEXT("MUTABLE_LVALUE");
	case EOpUnaryCategory::ConstLvalue:
		return TEXT("CONST_LVALUE");
	case EOpUnaryCategory::Temporary:
		return TEXT("TEMPORARY");
	case EOpUnaryCategory::Field:
		return TEXT("FIELD");
	case EOpUnaryCategory::Alias:
		return TEXT("ALIAS");
	default:
		return nullptr;
	}
}

const TCHAR* OperationToken(const EOpUnaryOperation Operation)
{
	switch (Operation)
	{
	case EOpUnaryOperation::PositiveInt8:
		return TEXT("POSITIVE_INT8");
	case EOpUnaryOperation::PositiveInt16:
		return TEXT("POSITIVE_INT16");
	case EOpUnaryOperation::PositiveInt:
		return TEXT("POSITIVE_INT");
	case EOpUnaryOperation::PositiveInt64:
		return TEXT("POSITIVE_INT64");
	case EOpUnaryOperation::PositiveUint8:
		return TEXT("POSITIVE_UINT8");
	case EOpUnaryOperation::PositiveUint16:
		return TEXT("POSITIVE_UINT16");
	case EOpUnaryOperation::PositiveUint:
		return TEXT("POSITIVE_UINT");
	case EOpUnaryOperation::PositiveUint64:
		return TEXT("POSITIVE_UINT64");
	case EOpUnaryOperation::PositiveFloat32:
		return TEXT("POSITIVE_FLOAT32");
	case EOpUnaryOperation::PositiveFloat64:
		return TEXT("POSITIVE_FLOAT64");
	case EOpUnaryOperation::NegativeInt8:
		return TEXT("NEGATIVE_INT8");
	case EOpUnaryOperation::NegativeInt16:
		return TEXT("NEGATIVE_INT16");
	case EOpUnaryOperation::NegativeInt:
		return TEXT("NEGATIVE_INT");
	case EOpUnaryOperation::NegativeInt64:
		return TEXT("NEGATIVE_INT64");
	case EOpUnaryOperation::NegativeUint8:
		return TEXT("NEGATIVE_UINT8");
	case EOpUnaryOperation::NegativeUint16:
		return TEXT("NEGATIVE_UINT16");
	case EOpUnaryOperation::NegativeUint:
		return TEXT("NEGATIVE_UINT");
	case EOpUnaryOperation::NegativeUint64:
		return TEXT("NEGATIVE_UINT64");
	case EOpUnaryOperation::NegativeFloat32:
		return TEXT("NEGATIVE_FLOAT32");
	case EOpUnaryOperation::NegativeFloat64:
		return TEXT("NEGATIVE_FLOAT64");
	case EOpUnaryOperation::BitNotInt8:
		return TEXT("BIT_NOT_INT8");
	case EOpUnaryOperation::BitNotInt16:
		return TEXT("BIT_NOT_INT16");
	case EOpUnaryOperation::BitNotInt:
		return TEXT("BIT_NOT_INT");
	case EOpUnaryOperation::BitNotInt64:
		return TEXT("BIT_NOT_INT64");
	case EOpUnaryOperation::BitNotUint8:
		return TEXT("BIT_NOT_UINT8");
	case EOpUnaryOperation::BitNotUint16:
		return TEXT("BIT_NOT_UINT16");
	case EOpUnaryOperation::BitNotUint:
		return TEXT("BIT_NOT_UINT");
	case EOpUnaryOperation::BitNotUint64:
		return TEXT("BIT_NOT_UINT64");
	default:
		return nullptr;
	}
}

const TCHAR* ValueToken(const EOpUnaryValue Value)
{
	switch (Value)
	{
	case EOpUnaryValue::Zero:
		return TEXT("ZERO");
	case EOpUnaryValue::One:
		return TEXT("ONE");
	case EOpUnaryValue::Negative:
		return TEXT("NEGATIVE");
	case EOpUnaryValue::NearMin:
		return TEXT("NEAR_MIN");
	case EOpUnaryValue::NearMax:
		return TEXT("NEAR_MAX");
	default:
		return nullptr;
	}
}

int32 SourceTypeIndex(const EOpUnaryOperation Operation)
{
	const int32 Raw = static_cast<int32>(Operation);
	if (Raw < 10)
	{
		return Raw;
	}
	if (Raw < 20)
	{
		return Raw - 10;
	}
	return Raw - 20;
}

EUnaryOp OperatorKind(const EOpUnaryOperation Operation)
{
	const int32 Raw = static_cast<int32>(Operation);
	if (Raw < 10)
	{
		return EUnaryOp::Positive;
	}
	if (Raw < 20)
	{
		return EUnaryOp::Negative;
	}
	return EUnaryOp::BitNot;
}

const TCHAR* OperatorToken(const EUnaryOp Operator)
{
	switch (Operator)
	{
	case EUnaryOp::Positive:
		return TEXT("+");
	case EUnaryOp::Negative:
		return TEXT("-");
	default:
		return TEXT("~");
	}
}

const TCHAR* ScriptName(const int32 TypeIndex)
{
	switch (TypeIndex)
	{
	case 0:
		return TEXT("int8");
	case 1:
		return TEXT("int16");
	case 2:
		return TEXT("int");
	case 3:
		return TEXT("int64");
	case 4:
		return TEXT("uint8");
	case 5:
		return TEXT("uint16");
	case 6:
		return TEXT("uint");
	case 7:
		return TEXT("uint64");
	case 8:
		return TEXT("float32");
	case 9:
		return TEXT("float64");
	default:
		return nullptr;
	}
}

const TCHAR* TypeSuffix(const int32 TypeIndex)
{
	switch (TypeIndex)
	{
	case 0:
		return TEXT("Int8");
	case 1:
		return TEXT("Int16");
	case 2:
		return TEXT("Int");
	case 3:
		return TEXT("Int64");
	case 4:
		return TEXT("Uint8");
	case 5:
		return TEXT("Uint16");
	case 6:
		return TEXT("Uint");
	case 7:
		return TEXT("Uint64");
	case 8:
		return TEXT("Float32");
	case 9:
		return TEXT("Float64");
	default:
		return TEXT("Int");
	}
}

int32 ResultTypeIndex(const EOpUnaryOperation Operation)
{
	const int32 Source = SourceTypeIndex(Operation);
	if (OperatorKind(Operation) == EUnaryOp::BitNot || Source <= 3 || Source >= 8)
	{
		return Source;
	}
	return Source - 4;
}

int32 TypeMarker(const int32 TypeIndex)
{
	return 201 + TypeIndex;
}

const TCHAR* ValueLiteral(const int32 TypeIndex, const EOpUnaryValue Value)
{
	if (TypeIndex == 8)
	{
		switch (Value)
		{
		case EOpUnaryValue::Zero:
			return TEXT("0.0");
		case EOpUnaryValue::One:
			return TEXT("1.0");
		case EOpUnaryValue::Negative:
			return TEXT("-3.0");
		case EOpUnaryValue::NearMin:
			return TEXT("-3.402823466e+38 / 2.0");
		default:
			return TEXT("3.402823466e+38 / 2.0");
		}
	}
	if (TypeIndex == 9)
	{
		switch (Value)
		{
		case EOpUnaryValue::Zero:
			return TEXT("0.0");
		case EOpUnaryValue::One:
			return TEXT("1.0");
		case EOpUnaryValue::Negative:
			return TEXT("-3.0");
		case EOpUnaryValue::NearMin:
			return TEXT("-1.7976931348623157e+308 / 2.0");
		default:
			return TEXT("1.7976931348623157e+308 / 2.0");
		}
	}
	if (TypeIndex <= 3)
	{
		switch (Value)
		{
		case EOpUnaryValue::Zero:
			return TEXT("0");
		case EOpUnaryValue::One:
			return TEXT("1");
		case EOpUnaryValue::Negative:
			return TEXT("-3");
		case EOpUnaryValue::NearMin:
			switch (TypeIndex)
			{
			case 0:
				return TEXT("-127");
			case 1:
				return TEXT("-32767");
			case 2:
				return TEXT("-2147483647");
			default:
				return TEXT("-9223372036854775807");
			}
		default:
			switch (TypeIndex)
			{
			case 0:
				return TEXT("126");
			case 1:
				return TEXT("32766");
			case 2:
				return TEXT("2147483646");
			default:
				return TEXT("9223372036854775806");
			}
		}
	}
	switch (Value)
	{
	case EOpUnaryValue::Zero:
		return TEXT("0");
	case EOpUnaryValue::One:
		return TEXT("1");
	case EOpUnaryValue::Negative:
		switch (TypeIndex)
		{
		case 4:
			return TEXT("253");
		case 5:
			return TEXT("65533");
		case 6:
			return TEXT("4294967293");
		default:
			return TEXT("18446744073709551613");
		}
	case EOpUnaryValue::NearMin:
		return TEXT("1");
	default:
		switch (TypeIndex)
		{
		case 4:
			return TEXT("254");
		case 5:
			return TEXT("65534");
		case 6:
			return TEXT("4294967294");
		default:
			return TEXT("18446744073709551614");
		}
	}
}

bool IsValidParams(const FOpUnaryParams& Params)
{
	return CategoryToken(Params.Category) != nullptr
		&& OperationToken(Params.Operation) != nullptr
		&& ValueToken(Params.Value) != nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FUnaryCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-UNARY-%s-%s-%s"),
		CategoryToken(Cell.Category),
		OperationToken(Cell.Operation),
		ValueToken(Cell.Value));
}

FString MakeEntryName(const FUnaryCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpUnaryParams ParamsFromCell(const FUnaryCell& Cell)
{
	FOpUnaryParams Params;
	Params.Category = Cell.Category;
	Params.Operation = Cell.Operation;
	Params.Value = Cell.Value;
	return Params;
}

FUnaryCell CellFromParams(const FOpUnaryParams& Params)
{
	FUnaryCell Cell;
	Cell.Category = Params.Category;
	Cell.Operation = Params.Operation;
	Cell.Value = Params.Value;
	return Cell;
}

TArray<FUnaryCell> AllCells()
{
	TArray<FUnaryCell> Cells;
	Cells.SetNum(700);
	for (int32 Index = 0; Index < 700; ++Index)
	{
		FUnaryCell& Cell = Cells[Index];
		Cell.Category = static_cast<EOpUnaryCategory>(Index / 140);
		Cell.Operation = static_cast<EOpUnaryOperation>((Index / 5) % 28);
		Cell.Value = static_cast<EOpUnaryValue>(Index % 5);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FUnaryCell& OutCell)
{
	static const TArray<FUnaryCell> Cells = AllCells();
	static TMap<FString, int32> IndexById;
	if (IndexById.Num() == 0)
	{
		for (int32 Index = 0; Index < Cells.Num(); ++Index)
		{
			IndexById.Add(MakeCaseId(Cells[Index]), Index);
		}
	}
	if (const int32* const Found = IndexById.Find(FString(CaseId)))
	{
		OutCell = Cells[*Found];
		return true;
	}
	return false;
}

void AppendObserveHelpers(FString& Source)
{
	for (int32 TypeIndex = 0; TypeIndex < 10; ++TypeIndex)
	{
		if (TypeIndex > 0)
		{
			AppendLine(Source);
		}
		AppendLine(Source, FString::Printf(TEXT("int ObserveUnaryNumericType(%s Value)"), ScriptName(TypeIndex)));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %d;"), TypeMarker(TypeIndex)));
		AppendLine(Source, TEXT("}"));
	}
}

void AppendCategoryHelpers(FString& Source, const FOpUnaryParams& Params)
{
	const int32 SourceType = SourceTypeIndex(Params.Operation);
	const int32 ResultType = ResultTypeIndex(Params.Operation);
	const TCHAR* const SourceName = ScriptName(SourceType);
	const TCHAR* const ResultName = ScriptName(ResultType);
	const TCHAR* const Suffix = TypeSuffix(SourceType);
	const TCHAR* const Token = OperatorToken(OperatorKind(Params.Operation));
	if (Params.Category == EOpUnaryCategory::Temporary)
	{
		AppendLine(Source, FString::Printf(TEXT("%s MakeUnaryTemporary%s(%s Input)"), SourceName, Suffix, SourceName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn Input;"));
		AppendLine(Source, TEXT("}"));
		return;
	}
	if (Params.Category == EOpUnaryCategory::Field)
	{
		AppendLine(Source, FString::Printf(TEXT("struct FUnaryFieldOwner%s"), Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\t%s Value;"), SourceName));
		AppendLine(Source, TEXT("}"));
		return;
	}
	if (Params.Category == EOpUnaryCategory::Alias)
	{
		const FString AliasSuffix = FString::Printf(TEXT("%s_%s"), Suffix, OperationToken(Params.Operation));
		AppendLine(Source, FString::Printf(TEXT("%s ApplyUnaryAlias%s(%s& in Value)"), ResultName, *AliasSuffix, SourceName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %sValue;"), Token));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int ObserveUnaryAliasType%s(%s& in Value)"), *AliasSuffix, SourceName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn ObserveUnaryNumericType(%sValue);"), Token));
		AppendLine(Source, TEXT("}"));
	}
}

void AppendAllCategoryHelpers(FString& Source)
{
	for (int32 TypeIndex = 0; TypeIndex < 10; ++TypeIndex)
	{
		AppendLine(Source);
		AppendLine(Source, FString::Printf(
			TEXT("%s MakeUnaryTemporary%s(%s Input)"),
			ScriptName(TypeIndex),
			TypeSuffix(TypeIndex),
			ScriptName(TypeIndex)));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn Input;"));
		AppendLine(Source, TEXT("}"));
	}
	for (int32 TypeIndex = 0; TypeIndex < 10; ++TypeIndex)
	{
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("struct FUnaryFieldOwner%s"), TypeSuffix(TypeIndex)));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\t%s Value;"), ScriptName(TypeIndex)));
		AppendLine(Source, TEXT("}"));
	}
	for (int32 OperationIndex = 0; OperationIndex < 28; ++OperationIndex)
	{
		const EOpUnaryOperation Operation = static_cast<EOpUnaryOperation>(OperationIndex);
		const int32 SourceType = SourceTypeIndex(Operation);
		const int32 ResultType = ResultTypeIndex(Operation);
		const TCHAR* const Token = OperatorToken(OperatorKind(Operation));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(
			TEXT("%s ApplyUnaryAlias%s_%s(%s& in Value)"),
			ScriptName(ResultType),
			TypeSuffix(SourceType),
			OperationToken(Operation),
			ScriptName(SourceType)));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %sValue;"), Token));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(
			TEXT("int ObserveUnaryAliasType%s_%s(%s& in Value)"),
			TypeSuffix(SourceType),
			OperationToken(Operation),
			ScriptName(SourceType)));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn ObserveUnaryNumericType(%sValue);"), Token));
		AppendLine(Source, TEXT("}"));
	}
}

FString AliasHelperSuffix(const EOpUnaryOperation Operation)
{
	return FString::Printf(TEXT("%s_%s"), TypeSuffix(SourceTypeIndex(Operation)), OperationToken(Operation));
}

void AppendEntry(FString& Source, const FOpUnaryParams& Params, const FString& FunctionName)
{
	const int32 SourceType = SourceTypeIndex(Params.Operation);
	const TCHAR* const SourceName = ScriptName(SourceType);
	const TCHAR* const Suffix = TypeSuffix(SourceType);
	const TCHAR* const Token = OperatorToken(OperatorKind(Params.Operation));
	const FString Input = FString::Printf(TEXT("%s(%s)"), SourceName, ValueLiteral(SourceType, Params.Value));
	const FString AliasSuffix = AliasHelperSuffix(Params.Operation);

	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Input = %s;"), SourceName, *Input));
	if (Params.Category == EOpUnaryCategory::MutableLvalue)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Value = Input;"), SourceName));
		AppendLine(Source, FString::Printf(TEXT("\treturn ObserveUnaryNumericType(%sValue);"), Token));
	}
	else if (Params.Category == EOpUnaryCategory::ConstLvalue)
	{
		AppendLine(Source, FString::Printf(TEXT("\tconst %s Value = Input;"), SourceName));
		AppendLine(Source, FString::Printf(TEXT("\treturn ObserveUnaryNumericType(%sValue);"), Token));
	}
	else if (Params.Category == EOpUnaryCategory::Temporary)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\treturn ObserveUnaryNumericType(%sMakeUnaryTemporary%s(Input));"),
			Token,
			Suffix));
	}
	else if (Params.Category == EOpUnaryCategory::Field)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFUnaryFieldOwner%s Owner;"), Suffix));
		AppendLine(Source, TEXT("\tOwner.Value = Input;"));
		AppendLine(Source, FString::Printf(TEXT("\treturn ObserveUnaryNumericType(%sOwner.Value);"), Token));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Value = Input;"), SourceName));
		AppendLine(Source, FString::Printf(TEXT("\treturn ObserveUnaryAliasType%s(Value);"), *AliasSuffix));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FOpUnaryParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendObserveHelpers(Source);
	if (Params.Category == EOpUnaryCategory::Temporary
		|| Params.Category == EOpUnaryCategory::Field
		|| Params.Category == EOpUnaryCategory::Alias)
	{
		AppendLine(Source);
		AppendCategoryHelpers(Source, Params);
	}
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

int32 ExpectedFromCell(const FUnaryCell& Cell)
{
	return TypeMarker(ResultTypeIndex(Cell.Operation));
}

FGeneratedCaseInfo MakeCaseInfo(const FUnaryCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	Info.bLimitedObservation = true;
	Info.ExecutionNotes = TEXT("int32 is the result type marker, not ExpectedBits.");
	return Info;
}
}

FStringView FOpUnaryGenerator::GetProductId() const
{
	return TEXT("LANG-OP-UNARY");
}

TArray<FGeneratedCaseInfo> FOpUnaryGenerator::ListCases() const
{
	using namespace OpUnaryGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FUnaryCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		const FString EntryName = MakeEntryName(Cells[Index]);
		checkf(!EntryNames.Contains(EntryName), TEXT("OpUnary PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FOpUnaryGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace OpUnaryGeneratorDetail;
	FUnaryCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FOpUnaryGenerator::BuildDumpSource() const
{
	using namespace OpUnaryGeneratorDetail;
	const TArray<FUnaryCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-UNARY"));
	AppendLine(Source, TEXT("// Cases: 700 total, 700 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendObserveHelpers(Source);
	AppendAllCategoryHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source, TEXT("// LimitedObservation: true"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

FString FOpUnaryGenerator::BuildUnarySource(const FOpUnaryParams& Params) const
{
	using namespace OpUnaryGeneratorDetail;
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

FString FOpUnaryGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace OpUnaryGeneratorDetail;
	const TArray<FUnaryCell> Cells = AllCells();
	FString Source;
	AppendObserveHelpers(Source);
	AppendAllCategoryHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	OutCaseCount = Cells.Num();
	return Source;
}

int32 FOpUnaryGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpUnaryGeneratorDetail;
	FUnaryCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}
}
