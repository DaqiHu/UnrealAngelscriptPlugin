#include "Framework/Generate/AngelscriptTestOpNumericBinaryGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace OpNumericBinaryGeneratorDetail
{
constexpr int32 TypeCount = 10;
constexpr int32 OperatorCount = 11;
constexpr int32 ValueCount = 5;
constexpr int32 CellCount = TypeCount * OperatorCount * TypeCount * ValueCount;
constexpr const TCHAR* LimitedNotes = TEXT("int32 TypeMarker is not full-width arithmetic proof.");

struct FNumericBinaryCell
{
	EOpNumericBinaryType LeftType = EOpNumericBinaryType::Int;
	EOpNumericBinaryOperator Operator = EOpNumericBinaryOperator::Add;
	EOpNumericBinaryType RightType = EOpNumericBinaryType::Int;
	EOpNumericBinaryValue Value = EOpNumericBinaryValue::Zero;
};

const TCHAR* TypeToken(const EOpNumericBinaryType Type)
{
	switch (Type)
	{
	case EOpNumericBinaryType::Int8:
		return TEXT("INT8");
	case EOpNumericBinaryType::Int16:
		return TEXT("INT16");
	case EOpNumericBinaryType::Int:
		return TEXT("INT");
	case EOpNumericBinaryType::Int64:
		return TEXT("INT64");
	case EOpNumericBinaryType::Uint8:
		return TEXT("UINT8");
	case EOpNumericBinaryType::Uint16:
		return TEXT("UINT16");
	case EOpNumericBinaryType::Uint:
		return TEXT("UINT");
	case EOpNumericBinaryType::Uint64:
		return TEXT("UINT64");
	case EOpNumericBinaryType::Float32:
		return TEXT("FLOAT32");
	case EOpNumericBinaryType::Float64:
		return TEXT("FLOAT64");
	default:
		return nullptr;
	}
}

const TCHAR* TypeName(const EOpNumericBinaryType Type)
{
	switch (Type)
	{
	case EOpNumericBinaryType::Int8:
		return TEXT("int8");
	case EOpNumericBinaryType::Int16:
		return TEXT("int16");
	case EOpNumericBinaryType::Int:
		return TEXT("int");
	case EOpNumericBinaryType::Int64:
		return TEXT("int64");
	case EOpNumericBinaryType::Uint8:
		return TEXT("uint8");
	case EOpNumericBinaryType::Uint16:
		return TEXT("uint16");
	case EOpNumericBinaryType::Uint:
		return TEXT("uint");
	case EOpNumericBinaryType::Uint64:
		return TEXT("uint64");
	case EOpNumericBinaryType::Float32:
		return TEXT("float32");
	case EOpNumericBinaryType::Float64:
		return TEXT("float64");
	default:
		return nullptr;
	}
}

const TCHAR* OperatorToken(const EOpNumericBinaryOperator Operator)
{
	switch (Operator)
	{
	case EOpNumericBinaryOperator::Add:
		return TEXT("ADD");
	case EOpNumericBinaryOperator::Subtract:
		return TEXT("SUBTRACT");
	case EOpNumericBinaryOperator::Multiply:
		return TEXT("MULTIPLY");
	case EOpNumericBinaryOperator::Divide:
		return TEXT("DIVIDE");
	case EOpNumericBinaryOperator::Modulo:
		return TEXT("MODULO");
	case EOpNumericBinaryOperator::Less:
		return TEXT("LESS");
	case EOpNumericBinaryOperator::LessEqual:
		return TEXT("LESS_EQUAL");
	case EOpNumericBinaryOperator::Greater:
		return TEXT("GREATER");
	case EOpNumericBinaryOperator::GreaterEqual:
		return TEXT("GREATER_EQUAL");
	case EOpNumericBinaryOperator::Equal:
		return TEXT("EQUAL");
	case EOpNumericBinaryOperator::NotEqual:
		return TEXT("NOT_EQUAL");
	default:
		return nullptr;
	}
}

const TCHAR* OperatorText(const EOpNumericBinaryOperator Operator)
{
	switch (Operator)
	{
	case EOpNumericBinaryOperator::Add:
		return TEXT("+");
	case EOpNumericBinaryOperator::Subtract:
		return TEXT("-");
	case EOpNumericBinaryOperator::Multiply:
		return TEXT("*");
	case EOpNumericBinaryOperator::Divide:
		return TEXT("/");
	case EOpNumericBinaryOperator::Modulo:
		return TEXT("%");
	case EOpNumericBinaryOperator::Less:
		return TEXT("<");
	case EOpNumericBinaryOperator::LessEqual:
		return TEXT("<=");
	case EOpNumericBinaryOperator::Greater:
		return TEXT(">");
	case EOpNumericBinaryOperator::GreaterEqual:
		return TEXT(">=");
	case EOpNumericBinaryOperator::Equal:
		return TEXT("==");
	case EOpNumericBinaryOperator::NotEqual:
		return TEXT("!=");
	default:
		return nullptr;
	}
}

const TCHAR* ValueToken(const EOpNumericBinaryValue Value)
{
	switch (Value)
	{
	case EOpNumericBinaryValue::Zero:
		return TEXT("ZERO");
	case EOpNumericBinaryValue::One:
		return TEXT("ONE");
	case EOpNumericBinaryValue::Negative:
		return TEXT("NEGATIVE");
	case EOpNumericBinaryValue::NearMin:
		return TEXT("NEAR_MIN");
	case EOpNumericBinaryValue::NearMax:
		return TEXT("NEAR_MAX");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FOpNumericBinaryParams& Params)
{
	return TypeToken(Params.LeftType) != nullptr
		&& OperatorToken(Params.Operator) != nullptr
		&& TypeToken(Params.RightType) != nullptr
		&& ValueToken(Params.Value) != nullptr;
}

bool IsComparison(const EOpNumericBinaryOperator Operator)
{
	return Operator >= EOpNumericBinaryOperator::Less;
}

bool IsSigned(const EOpNumericBinaryType Type)
{
	return Type <= EOpNumericBinaryType::Int64;
}

bool IsFloat32(const EOpNumericBinaryType Type)
{
	return Type == EOpNumericBinaryType::Float32;
}

bool IsFloat64(const EOpNumericBinaryType Type)
{
	return Type == EOpNumericBinaryType::Float64;
}

bool IsWide(const EOpNumericBinaryType Type)
{
	return Type == EOpNumericBinaryType::Int64 || Type == EOpNumericBinaryType::Uint64
		|| Type == EOpNumericBinaryType::Float64;
}

int32 TypeMarker(const FNumericBinaryCell& Cell)
{
	if (IsComparison(Cell.Operator))
	{
		return 107;
	}
	if (IsFloat64(Cell.LeftType) || IsFloat64(Cell.RightType))
	{
		return 106;
	}
	if (IsFloat32(Cell.LeftType) || IsFloat32(Cell.RightType))
	{
		return 105;
	}
	const bool bWide = IsWide(Cell.LeftType) || IsWide(Cell.RightType);
	const bool bSigned = IsSigned(Cell.LeftType) || IsSigned(Cell.RightType);
	if (bWide)
	{
		return bSigned ? 103 : 104;
	}
	return bSigned ? 101 : 102;
}

FString TypedLiteral(const EOpNumericBinaryType Type, const TCHAR* Literal)
{
	return FString::Printf(TEXT("%s(%s)"), TypeName(Type), Literal);
}

const TCHAR* PartitionLiteral(const EOpNumericBinaryType Type, const EOpNumericBinaryValue Value)
{
	if (IsFloat32(Type) || IsFloat64(Type))
	{
		switch (Value)
		{
		case EOpNumericBinaryValue::Zero:
			return TEXT("0.0");
		case EOpNumericBinaryValue::One:
			return TEXT("1.0");
		case EOpNumericBinaryValue::Negative:
			return TEXT("-3.0");
		case EOpNumericBinaryValue::NearMin:
			return IsFloat32(Type) ? TEXT("-1.70141173e38") : TEXT("-8.988465674311579e307");
		case EOpNumericBinaryValue::NearMax:
			return IsFloat32(Type) ? TEXT("1.70141173e38") : TEXT("8.988465674311579e307");
		default:
			return TEXT("0.0");
		}
	}

	if (IsSigned(Type))
	{
		switch (Value)
		{
		case EOpNumericBinaryValue::Zero:
			return TEXT("0");
		case EOpNumericBinaryValue::One:
			return TEXT("1");
		case EOpNumericBinaryValue::Negative:
			return TEXT("-3");
		case EOpNumericBinaryValue::NearMin:
			switch (Type)
			{
			case EOpNumericBinaryType::Int8:
				return TEXT("-127");
			case EOpNumericBinaryType::Int16:
				return TEXT("-32767");
			case EOpNumericBinaryType::Int64:
				return TEXT("-9223372036854775807");
			default:
				return TEXT("-2147483647");
			}
		case EOpNumericBinaryValue::NearMax:
			switch (Type)
			{
			case EOpNumericBinaryType::Int8:
				return TEXT("126");
			case EOpNumericBinaryType::Int16:
				return TEXT("32766");
			case EOpNumericBinaryType::Int64:
				return TEXT("9223372036854775806");
			default:
				return TEXT("2147483646");
			}
		default:
			return TEXT("0");
		}
	}

	switch (Value)
	{
	case EOpNumericBinaryValue::Zero:
		return TEXT("0");
	case EOpNumericBinaryValue::One:
	case EOpNumericBinaryValue::NearMin:
		return TEXT("1");
	case EOpNumericBinaryValue::Negative:
		switch (Type)
		{
		case EOpNumericBinaryType::Uint8:
			return TEXT("253");
		case EOpNumericBinaryType::Uint16:
			return TEXT("65533");
		case EOpNumericBinaryType::Uint64:
			return TEXT("18446744073709551613");
		default:
			return TEXT("4294967293");
		}
	case EOpNumericBinaryValue::NearMax:
		switch (Type)
		{
		case EOpNumericBinaryType::Uint8:
			return TEXT("254");
		case EOpNumericBinaryType::Uint16:
			return TEXT("65534");
		case EOpNumericBinaryType::Uint64:
			return TEXT("18446744073709551614");
		default:
			return TEXT("4294967294");
		}
	default:
		return TEXT("0");
	}
}

const TCHAR* RightLiteral(const EOpNumericBinaryType Type, const EOpNumericBinaryOperator Operator, const EOpNumericBinaryValue Value)
{
	if (IsComparison(Operator))
	{
		return PartitionLiteral(Type, Value);
	}

	const bool bNear = Value == EOpNumericBinaryValue::NearMin || Value == EOpNumericBinaryValue::NearMax;
	if (Operator == EOpNumericBinaryOperator::Multiply || Operator == EOpNumericBinaryOperator::Divide)
	{
		return (IsFloat32(Type) || IsFloat64(Type)) ? (bNear ? TEXT("1.0") : TEXT("2.0")) : (bNear ? TEXT("1") : TEXT("2"));
	}
	if (Operator == EOpNumericBinaryOperator::Modulo)
	{
		return (IsFloat32(Type) || IsFloat64(Type)) ? TEXT("2.0") : TEXT("2");
	}
	return (IsFloat32(Type) || IsFloat64(Type)) ? TEXT("1.0") : TEXT("1");
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FNumericBinaryCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-NUMERIC-BINARY-%s-%s-%s-%s"),
		TypeToken(Cell.LeftType),
		OperatorToken(Cell.Operator),
		TypeToken(Cell.RightType),
		ValueToken(Cell.Value));
}

FString MakeEntryName(const FNumericBinaryCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpNumericBinaryParams ParamsFromCell(const FNumericBinaryCell& Cell)
{
	FOpNumericBinaryParams Params;
	Params.LeftType = Cell.LeftType;
	Params.Operator = Cell.Operator;
	Params.RightType = Cell.RightType;
	Params.Value = Cell.Value;
	return Params;
}

FNumericBinaryCell CellFromIndex(const int32 Index)
{
	FNumericBinaryCell Cell;
	Cell.LeftType = static_cast<EOpNumericBinaryType>(Index / (OperatorCount * TypeCount * ValueCount));
	Cell.Operator = static_cast<EOpNumericBinaryOperator>((Index / (TypeCount * ValueCount)) % OperatorCount);
	Cell.RightType = static_cast<EOpNumericBinaryType>((Index / ValueCount) % TypeCount);
	Cell.Value = static_cast<EOpNumericBinaryValue>(Index % ValueCount);
	return Cell;
}

bool TryParseToken(const TCHAR* const* Tokens, const int32 Count, const FString& Token, int32& OutIndex)
{
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Token == Tokens[Index])
		{
			OutIndex = Index;
			return true;
		}
	}
	return false;
}

bool TryFindCell(const FStringView CaseId, FNumericBinaryCell& OutCell)
{
	const FString Needle(CaseId);
	const FString Prefix(TEXT("LANG-OP-NUMERIC-BINARY-"));
	if (!Needle.StartsWith(Prefix))
	{
		return false;
	}

	TArray<FString> Parts;
	Needle.RightChop(Prefix.Len()).ParseIntoArray(Parts, TEXT("-"), true);
	if (Parts.Num() != 4)
	{
		return false;
	}

	static const TCHAR* const kTypes[] = {
		TEXT("INT8"), TEXT("INT16"), TEXT("INT"), TEXT("INT64"),
		TEXT("UINT8"), TEXT("UINT16"), TEXT("UINT"), TEXT("UINT64"),
		TEXT("FLOAT32"), TEXT("FLOAT64")};
	static const TCHAR* const kOperators[] = {
		TEXT("ADD"), TEXT("SUBTRACT"), TEXT("MULTIPLY"), TEXT("DIVIDE"), TEXT("MODULO"),
		TEXT("LESS"), TEXT("LESS_EQUAL"), TEXT("GREATER"), TEXT("GREATER_EQUAL"), TEXT("EQUAL"), TEXT("NOT_EQUAL")};
	static const TCHAR* const kValues[] = {
		TEXT("ZERO"), TEXT("ONE"), TEXT("NEGATIVE"), TEXT("NEAR_MIN"), TEXT("NEAR_MAX")};

	int32 LeftIndex = INDEX_NONE;
	int32 OperatorIndex = INDEX_NONE;
	int32 RightIndex = INDEX_NONE;
	int32 ValueIndex = INDEX_NONE;
	if (!TryParseToken(kTypes, TypeCount, Parts[0], LeftIndex)
		|| !TryParseToken(kOperators, OperatorCount, Parts[1], OperatorIndex)
		|| !TryParseToken(kTypes, TypeCount, Parts[2], RightIndex)
		|| !TryParseToken(kValues, ValueCount, Parts[3], ValueIndex))
	{
		return false;
	}

	OutCell.LeftType = static_cast<EOpNumericBinaryType>(LeftIndex);
	OutCell.Operator = static_cast<EOpNumericBinaryOperator>(OperatorIndex);
	OutCell.RightType = static_cast<EOpNumericBinaryType>(RightIndex);
	OutCell.Value = static_cast<EOpNumericBinaryValue>(ValueIndex);
	return true;
}

void AppendSharedHelpers(FString& Source)
{
	static const TCHAR* const kDecls[] = {
		TEXT("int"), TEXT("uint"), TEXT("int64"), TEXT("uint64"), TEXT("float32"), TEXT("float64"), TEXT("bool")};
	static const int32 kMarkers[] = {101, 102, 103, 104, 105, 106, 107};
	for (int32 Index = 0; Index < 7; ++Index)
	{
		if (Index > 0)
		{
			AppendLine(Source);
		}
		AppendLine(Source, FString::Printf(TEXT("int ObserveNumericType(%s Value)"), kDecls[Index]));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %d;"), kMarkers[Index]));
		AppendLine(Source, TEXT("}"));
	}
}

void AppendEntry(FString& Source, const FOpNumericBinaryParams& Params, const FString& FunctionName)
{
	const FString Left = TypedLiteral(Params.LeftType, PartitionLiteral(Params.LeftType, Params.Value));
	const FString Right = TypedLiteral(
		Params.RightType,
		RightLiteral(Params.RightType, Params.Operator, Params.Value));
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Left = %s;"), TypeName(Params.LeftType), *Left));
	AppendLine(Source, FString::Printf(TEXT("\t%s Right = %s;"), TypeName(Params.RightType), *Right));
	AppendLine(Source, FString::Printf(TEXT("\treturn ObserveNumericType(Left %s Right);"), OperatorText(Params.Operator)));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FOpNumericBinaryParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FNumericBinaryCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = TypeMarker(Cell);
	Info.bLimitedObservation = true;
	Info.ExecutionNotes = LimitedNotes;
	return Info;
}
}

FStringView FOpNumericBinaryGenerator::GetProductId() const
{
	return TEXT("LANG-OP-NUMERIC-BINARY");
}

TArray<FGeneratedCaseInfo> FOpNumericBinaryGenerator::ListCases() const
{
	using namespace OpNumericBinaryGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	Cases.Reserve(CellCount);
	TSet<FString> EntryNames;
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FNumericBinaryCell Cell = CellFromIndex(Index);
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("OpNumericBinary PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FOpNumericBinaryGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace OpNumericBinaryGeneratorDetail;
	FNumericBinaryCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FOpNumericBinaryGenerator::BuildDumpSource() const
{
	using namespace OpNumericBinaryGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-NUMERIC-BINARY"));
	AppendLine(Source, TEXT("// Cases: 5500 total, 5500 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FNumericBinaryCell Cell = CellFromIndex(Index);
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source, TEXT("// LimitedObservation: true"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FOpNumericBinaryGenerator::BuildNumericBinarySource(const FOpNumericBinaryParams& Params) const
{
	using namespace OpNumericBinaryGeneratorDetail;
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

FString FOpNumericBinaryGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace OpNumericBinaryGeneratorDetail;
	OutCaseCount = CellCount;
	FString Source;
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(CellFromIndex(Index)), MakeEntryName(CellFromIndex(Index)));
	}
	return Source;
}

int32 FOpNumericBinaryGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpNumericBinaryGeneratorDetail;
	FNumericBinaryCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return TypeMarker(Cell);
}
}
