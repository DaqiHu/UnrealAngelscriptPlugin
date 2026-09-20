#include "Framework/Generate/AngelscriptTestOpIntegralBitwiseGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace OpIntegralBitwiseGeneratorDetail
{
struct FBitwiseCell
{
	EOpIntegralBitwiseCategory Category = EOpIntegralBitwiseCategory::MutableLValue;
	EOpIntegralBitwiseOperator Operator = EOpIntegralBitwiseOperator::BitAnd;
	EOpIntegralBitwiseRight Right = EOpIntegralBitwiseRight::Zero;
	EOpIntegralBitwiseType Type = EOpIntegralBitwiseType::Int;
};

const TCHAR* CategoryToken(const EOpIntegralBitwiseCategory Category)
{
	switch (Category)
	{
	case EOpIntegralBitwiseCategory::MutableLValue:
		return TEXT("MUTABLE_LVALUE");
	case EOpIntegralBitwiseCategory::ConstLValue:
		return TEXT("CONST_LVALUE");
	case EOpIntegralBitwiseCategory::Temporary:
		return TEXT("TEMPORARY");
	case EOpIntegralBitwiseCategory::Field:
		return TEXT("FIELD");
	case EOpIntegralBitwiseCategory::Alias:
		return TEXT("ALIAS");
	default:
		return nullptr;
	}
}

const TCHAR* OperatorToken(const EOpIntegralBitwiseOperator Operator)
{
	switch (Operator)
	{
	case EOpIntegralBitwiseOperator::BitAnd:
		return TEXT("BIT_AND");
	case EOpIntegralBitwiseOperator::BitOr:
		return TEXT("BIT_OR");
	case EOpIntegralBitwiseOperator::BitXor:
		return TEXT("BIT_XOR");
	case EOpIntegralBitwiseOperator::ShiftLeft:
		return TEXT("SHIFT_LEFT");
	case EOpIntegralBitwiseOperator::ShiftRightLogical:
		return TEXT("SHIFT_RIGHT_LOGICAL");
	case EOpIntegralBitwiseOperator::ShiftRightArithmetic:
		return TEXT("SHIFT_RIGHT_ARITHMETIC");
	default:
		return nullptr;
	}
}

const TCHAR* RightToken(const EOpIntegralBitwiseRight Right)
{
	switch (Right)
	{
	case EOpIntegralBitwiseRight::Zero:
		return TEXT("ZERO");
	case EOpIntegralBitwiseRight::One:
		return TEXT("ONE");
	case EOpIntegralBitwiseRight::SourceWidthMinusOne:
		return TEXT("SOURCE_WIDTH_MINUS_ONE");
	case EOpIntegralBitwiseRight::SourceWidth:
		return TEXT("SOURCE_WIDTH");
	case EOpIntegralBitwiseRight::SourceWidthPlusOne:
		return TEXT("SOURCE_WIDTH_PLUS_ONE");
	case EOpIntegralBitwiseRight::Large:
		return TEXT("LARGE");
	case EOpIntegralBitwiseRight::Negative:
		return TEXT("NEGATIVE");
	default:
		return nullptr;
	}
}

const TCHAR* TypeToken(const EOpIntegralBitwiseType Type)
{
	switch (Type)
	{
	case EOpIntegralBitwiseType::Int8:
		return TEXT("INT8");
	case EOpIntegralBitwiseType::Int16:
		return TEXT("INT16");
	case EOpIntegralBitwiseType::Int:
		return TEXT("INT");
	case EOpIntegralBitwiseType::Int64:
		return TEXT("INT64");
	case EOpIntegralBitwiseType::Uint8:
		return TEXT("UINT8");
	case EOpIntegralBitwiseType::Uint16:
		return TEXT("UINT16");
	case EOpIntegralBitwiseType::Uint:
		return TEXT("UINT");
	case EOpIntegralBitwiseType::Uint64:
		return TEXT("UINT64");
	default:
		return nullptr;
	}
}

const TCHAR* ScriptType(const EOpIntegralBitwiseType Type)
{
	switch (Type)
	{
	case EOpIntegralBitwiseType::Int8:
		return TEXT("int8");
	case EOpIntegralBitwiseType::Int16:
		return TEXT("int16");
	case EOpIntegralBitwiseType::Int:
		return TEXT("int");
	case EOpIntegralBitwiseType::Int64:
		return TEXT("int64");
	case EOpIntegralBitwiseType::Uint8:
		return TEXT("uint8");
	case EOpIntegralBitwiseType::Uint16:
		return TEXT("uint16");
	case EOpIntegralBitwiseType::Uint:
		return TEXT("uint");
	case EOpIntegralBitwiseType::Uint64:
		return TEXT("uint64");
	default:
		return nullptr;
	}
}

const TCHAR* OperatorGlyph(const EOpIntegralBitwiseOperator Operator)
{
	switch (Operator)
	{
	case EOpIntegralBitwiseOperator::BitAnd:
		return TEXT("&");
	case EOpIntegralBitwiseOperator::BitOr:
		return TEXT("|");
	case EOpIntegralBitwiseOperator::BitXor:
		return TEXT("^");
	case EOpIntegralBitwiseOperator::ShiftLeft:
		return TEXT("<<");
	case EOpIntegralBitwiseOperator::ShiftRightLogical:
		return TEXT(">>");
	case EOpIntegralBitwiseOperator::ShiftRightArithmetic:
		return TEXT(">>>");
	default:
		return TEXT("&");
	}
}

const TCHAR* OperatorSuffix(const EOpIntegralBitwiseOperator Operator)
{
	switch (Operator)
	{
	case EOpIntegralBitwiseOperator::BitAnd:
		return TEXT("BitAnd");
	case EOpIntegralBitwiseOperator::BitOr:
		return TEXT("BitOr");
	case EOpIntegralBitwiseOperator::BitXor:
		return TEXT("BitXor");
	case EOpIntegralBitwiseOperator::ShiftLeft:
		return TEXT("ShiftLeft");
	case EOpIntegralBitwiseOperator::ShiftRightLogical:
		return TEXT("ShiftRightLogical");
	case EOpIntegralBitwiseOperator::ShiftRightArithmetic:
		return TEXT("ShiftRightArithmetic");
	default:
		return TEXT("Bitwise");
	}
}

bool IsValidParams(const FOpIntegralBitwiseParams& Params)
{
	return CategoryToken(Params.Category) != nullptr
		&& OperatorToken(Params.Operator) != nullptr
		&& RightToken(Params.Right) != nullptr
		&& TypeToken(Params.Type) != nullptr;
}

bool IsSignedType(const EOpIntegralBitwiseType Type)
{
	return Type == EOpIntegralBitwiseType::Int8
		|| Type == EOpIntegralBitwiseType::Int16
		|| Type == EOpIntegralBitwiseType::Int
		|| Type == EOpIntegralBitwiseType::Int64;
}

int32 SourceWidthBits(const EOpIntegralBitwiseType Type)
{
	switch (Type)
	{
	case EOpIntegralBitwiseType::Int8:
	case EOpIntegralBitwiseType::Uint8:
		return 8;
	case EOpIntegralBitwiseType::Int16:
	case EOpIntegralBitwiseType::Uint16:
		return 16;
	case EOpIntegralBitwiseType::Int:
	case EOpIntegralBitwiseType::Uint:
		return 32;
	case EOpIntegralBitwiseType::Int64:
	case EOpIntegralBitwiseType::Uint64:
		return 64;
	default:
		return 32;
	}
}

int32 ResultWidthBits(const EOpIntegralBitwiseType Type)
{
	return SourceWidthBits(Type) == 64 ? 64 : 32;
}

uint64 WidthMask(const int32 Width)
{
	return Width == 64 ? MAX_uint64 : (uint64{1} << Width) - 1;
}

uint64 SourceBits(const EOpIntegralBitwiseType Type)
{
	switch (SourceWidthBits(Type))
	{
	case 8:
		return 0xA5ull;
	case 16:
		return 0xA55Aull;
	case 32:
		return 0xA55AA55Aull;
	case 64:
		return 0xA55AA55AA55AA55Aull;
	default:
		return 0;
	}
}

uint64 PromotedBits(const EOpIntegralBitwiseType Type)
{
	const int32 SourceWidth = SourceWidthBits(Type);
	const int32 OutputWidth = ResultWidthBits(Type);
	const uint64 Bits = SourceBits(Type) & WidthMask(SourceWidth);
	if (!IsSignedType(Type) || SourceWidth == OutputWidth
		|| (Bits & (uint64{1} << (SourceWidth - 1))) == 0)
	{
		return Bits & WidthMask(OutputWidth);
	}
	return (Bits | ~WidthMask(SourceWidth)) & WidthMask(OutputWidth);
}

int32 RightValue(const EOpIntegralBitwiseType Type, const EOpIntegralBitwiseRight Right)
{
	const int32 SourceWidth = SourceWidthBits(Type);
	switch (Right)
	{
	case EOpIntegralBitwiseRight::Zero:
		return 0;
	case EOpIntegralBitwiseRight::One:
		return 1;
	case EOpIntegralBitwiseRight::SourceWidthMinusOne:
		return SourceWidth - 1;
	case EOpIntegralBitwiseRight::SourceWidth:
		return SourceWidth;
	case EOpIntegralBitwiseRight::SourceWidthPlusOne:
		return SourceWidth + 1;
	case EOpIntegralBitwiseRight::Large:
		return 100000;
	case EOpIntegralBitwiseRight::Negative:
		return -1;
	default:
		return 0;
	}
}

uint64 ArithmeticShiftRight(const uint64 Bits, const int32 Count, const int32 Width)
{
	const int32 EffectiveCount = Count & (Width - 1);
	const uint64 Mask = WidthMask(Width);
	const uint64 Value = Bits & Mask;
	if (EffectiveCount == 0)
	{
		return Value;
	}
	const uint64 Shifted = Value >> EffectiveCount;
	if ((Value & (uint64{1} << (Width - 1))) == 0)
	{
		return Shifted;
	}
	const uint64 Fill = Mask ^ ((uint64{1} << (Width - EffectiveCount)) - 1);
	return (Shifted | Fill) & Mask;
}

uint64 ExpectedBits(const FBitwiseCell& Cell)
{
	const int32 Width = ResultWidthBits(Cell.Type);
	const uint64 Mask = WidthMask(Width);
	const uint64 Left = PromotedBits(Cell.Type);
	const int32 RightSigned = RightValue(Cell.Type, Cell.Right);
	const uint64 Right = static_cast<uint64>(static_cast<int64>(RightSigned)) & Mask;
	const int32 Count = RightSigned & (Width - 1);
	switch (Cell.Operator)
	{
	case EOpIntegralBitwiseOperator::BitAnd:
		return Left & Right;
	case EOpIntegralBitwiseOperator::BitOr:
		return Left | Right;
	case EOpIntegralBitwiseOperator::BitXor:
		return Left ^ Right;
	case EOpIntegralBitwiseOperator::ShiftLeft:
		return (Left << Count) & Mask;
	case EOpIntegralBitwiseOperator::ShiftRightLogical:
		return Left >> Count;
	case EOpIntegralBitwiseOperator::ShiftRightArithmetic:
		return ArithmeticShiftRight(Left, Count, Width);
	default:
		return 0;
	}
}

int32 ExpectedFromCell(const FBitwiseCell& Cell)
{
	return static_cast<int32>(ExpectedBits(Cell) & 0xFFFFFFFFull);
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FBitwiseCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-INTEGRAL-BITWISE-%s-%s-%s-%s"),
		CategoryToken(Cell.Category),
		OperatorToken(Cell.Operator),
		RightToken(Cell.Right),
		TypeToken(Cell.Type));
}

FString MakeEntryName(const FBitwiseCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpIntegralBitwiseParams ParamsFromCell(const FBitwiseCell& Cell)
{
	FOpIntegralBitwiseParams Params;
	Params.Category = Cell.Category;
	Params.Operator = Cell.Operator;
	Params.Right = Cell.Right;
	Params.Type = Cell.Type;
	return Params;
}

TArray<FBitwiseCell> AllCells()
{
	TArray<FBitwiseCell> Cells;
	Cells.SetNum(1680);
	for (int32 Index = 0; Index < 1680; ++Index)
	{
		FBitwiseCell& Cell = Cells[Index];
		Cell.Category = static_cast<EOpIntegralBitwiseCategory>(Index / 336);
		Cell.Operator = static_cast<EOpIntegralBitwiseOperator>((Index / 56) % 6);
		Cell.Right = static_cast<EOpIntegralBitwiseRight>((Index / 8) % 7);
		Cell.Type = static_cast<EOpIntegralBitwiseType>(Index % 8);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FBitwiseCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FBitwiseCell> Cells = AllCells();
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

FString FieldOwnerName(const EOpIntegralBitwiseType Type)
{
	return FString::Printf(TEXT("FBitwiseFieldOwner_%s"), ScriptType(Type));
}

FString TemporaryName(const EOpIntegralBitwiseType Type)
{
	return FString::Printf(TEXT("MakeBitwiseTemporary_%s"), ScriptType(Type));
}

FString AliasName(const EOpIntegralBitwiseOperator Operator, const EOpIntegralBitwiseType Type)
{
	return FString::Printf(TEXT("Apply%sAlias_%s"), OperatorSuffix(Operator), ScriptType(Type));
}

FString SourceLiteral(const EOpIntegralBitwiseType Type)
{
	switch (Type)
	{
	case EOpIntegralBitwiseType::Int8:
		return TEXT("int8(0xA5)");
	case EOpIntegralBitwiseType::Int16:
		return TEXT("int16(0xA55A)");
	case EOpIntegralBitwiseType::Int:
		return TEXT("int(0xA55AA55A)");
	case EOpIntegralBitwiseType::Int64:
		return TEXT("int64(0xA55AA55AA55AA55A)");
	case EOpIntegralBitwiseType::Uint8:
		return TEXT("uint8(0xA5)");
	case EOpIntegralBitwiseType::Uint16:
		return TEXT("uint16(0xA55A)");
	case EOpIntegralBitwiseType::Uint:
		return TEXT("uint(0xA55AA55A)");
	case EOpIntegralBitwiseType::Uint64:
		return TEXT("uint64(0xA55AA55AA55AA55A)");
	default:
		return TEXT("0");
	}
}

void AppendObservers(FString& Source)
{
	AppendLine(Source, TEXT("int ObserveBitwiseType(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 301;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ObserveBitwiseType(uint Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 302;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ObserveBitwiseType(int64 Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 303;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ObserveBitwiseType(uint64 Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 304;"));
	AppendLine(Source, TEXT("}"));
}

void AppendFieldOwner(FString& Source, const EOpIntegralBitwiseType Type)
{
	AppendLine(Source, FString::Printf(TEXT("struct %s"), *FieldOwnerName(Type)));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Value;"), ScriptType(Type)));
	AppendLine(Source, TEXT("}"));
}

void AppendTemporary(FString& Source, const EOpIntegralBitwiseType Type)
{
	const TCHAR* const TypeName = ScriptType(Type);
	AppendLine(Source, FString::Printf(TEXT("%s %s(%s Input)"), TypeName, *TemporaryName(Type), TypeName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Input;"));
	AppendLine(Source, TEXT("}"));
}

void AppendAliasHelper(FString& Source, const EOpIntegralBitwiseOperator Operator, const EOpIntegralBitwiseType Type)
{
	const TCHAR* const TypeName = ScriptType(Type);
	AppendLine(Source, FString::Printf(
		TEXT("int %s(%s& in Value, int Right)"),
		*AliasName(Operator, Type),
		TypeName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\treturn int(Value %s Right);"), OperatorGlyph(Operator)));
	AppendLine(Source, TEXT("}"));
}

FString CategoryExpression(const FOpIntegralBitwiseParams& Params)
{
	const TCHAR* const Glyph = OperatorGlyph(Params.Operator);
	if (Params.Category == EOpIntegralBitwiseCategory::Temporary)
	{
		return FString::Printf(TEXT("%s(Input) %s Right"), *TemporaryName(Params.Type), Glyph);
	}
	if (Params.Category == EOpIntegralBitwiseCategory::Field)
	{
		return FString::Printf(TEXT("Owner.Value %s Right"), Glyph);
	}
	if (Params.Category == EOpIntegralBitwiseCategory::Alias)
	{
		return FString::Printf(TEXT("%s(Value, Right)"), *AliasName(Params.Operator, Params.Type));
	}
	return FString::Printf(TEXT("Value %s Right"), Glyph);
}

void AppendEntry(FString& Source, const FOpIntegralBitwiseParams& Params, const FString& FunctionName)
{
	const TCHAR* const TypeName = ScriptType(Params.Type);
	const int32 Right = RightValue(Params.Type, Params.Right);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Input = %s;"), TypeName, *SourceLiteral(Params.Type)));
	AppendLine(Source, FString::Printf(TEXT("\tint Right = %d;"), Right));
	if (Params.Category == EOpIntegralBitwiseCategory::MutableLValue)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Value = Input;"), TypeName));
	}
	else if (Params.Category == EOpIntegralBitwiseCategory::ConstLValue)
	{
		AppendLine(Source, FString::Printf(TEXT("\tconst %s Value = Input;"), TypeName));
	}
	else if (Params.Category == EOpIntegralBitwiseCategory::Field)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Owner;"), *FieldOwnerName(Params.Type)));
		AppendLine(Source, TEXT("\tOwner.Value = Input;"));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Value = Input;"), TypeName));
	}
	AppendLine(Source, FString::Printf(TEXT("\treturn int(%s);"), *CategoryExpression(Params)));
	AppendLine(Source, TEXT("}"));
}

void AppendOwnedHelpers(FString& Source, const FOpIntegralBitwiseParams& Params)
{
	AppendObservers(Source);
	AppendLine(Source);
	if (Params.Category == EOpIntegralBitwiseCategory::Temporary)
	{
		AppendTemporary(Source, Params.Type);
		AppendLine(Source);
	}
	else if (Params.Category == EOpIntegralBitwiseCategory::Field)
	{
		AppendFieldOwner(Source, Params.Type);
		AppendLine(Source);
	}
	else if (Params.Category == EOpIntegralBitwiseCategory::Alias)
	{
		AppendAliasHelper(Source, Params.Operator, Params.Type);
		AppendLine(Source);
	}
}

FString BuildOwnedSource(const FOpIntegralBitwiseParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOwnedHelpers(Source, Params);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FBitwiseCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	Info.bLimitedObservation = true;
	Info.ExecutionNotes = TEXT("int32 is the low 32 bits of the bitwise result, not full-width proof.");
	return Info;
}
}

FStringView FOpIntegralBitwiseGenerator::GetProductId() const
{
	return TEXT("LANG-OP-INTEGRAL-BITWISE");
}

TArray<FGeneratedCaseInfo> FOpIntegralBitwiseGenerator::ListCases() const
{
	using namespace OpIntegralBitwiseGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FBitwiseCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		const FString EntryName = MakeEntryName(Cells[Index]);
		checkf(!EntryNames.Contains(EntryName), TEXT("OpIntegralBitwise PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FOpIntegralBitwiseGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace OpIntegralBitwiseGeneratorDetail;
	FBitwiseCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FOpIntegralBitwiseGenerator::BuildDumpSource() const
{
	using namespace OpIntegralBitwiseGeneratorDetail;
	const TArray<FBitwiseCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-INTEGRAL-BITWISE"));
	AppendLine(Source, TEXT("// Cases: 1680 total, 1680 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendObservers(Source);
	AppendLine(Source);
	for (int32 TypeIndex = 0; TypeIndex < 8; ++TypeIndex)
	{
		const EOpIntegralBitwiseType Type = static_cast<EOpIntegralBitwiseType>(TypeIndex);
		AppendTemporary(Source, Type);
		AppendLine(Source);
		AppendFieldOwner(Source, Type);
		AppendLine(Source);
	}
	for (int32 OperatorIndex = 0; OperatorIndex < 6; ++OperatorIndex)
	{
		for (int32 TypeIndex = 0; TypeIndex < 8; ++TypeIndex)
		{
			AppendAliasHelper(
				Source,
				static_cast<EOpIntegralBitwiseOperator>(OperatorIndex),
				static_cast<EOpIntegralBitwiseType>(TypeIndex));
			AppendLine(Source);
		}
	}
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		if (Index + 1 < Cells.Num())
		{
			AppendLine(Source);
		}
	}
	return Source;
}

FString FOpIntegralBitwiseGenerator::BuildBitwiseSource(const FOpIntegralBitwiseParams& Params) const
{
	using namespace OpIntegralBitwiseGeneratorDetail;
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

FString FOpIntegralBitwiseGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace OpIntegralBitwiseGeneratorDetail;
	const TArray<FBitwiseCell> Cells = AllCells();
	FString Source;
	AppendObservers(Source);
	AppendLine(Source);
	for (int32 TypeIndex = 0; TypeIndex < 8; ++TypeIndex)
	{
		const EOpIntegralBitwiseType Type = static_cast<EOpIntegralBitwiseType>(TypeIndex);
		AppendTemporary(Source, Type);
		AppendLine(Source);
		AppendFieldOwner(Source, Type);
		AppendLine(Source);
	}
	for (int32 OperatorIndex = 0; OperatorIndex < 6; ++OperatorIndex)
	{
		for (int32 TypeIndex = 0; TypeIndex < 8; ++TypeIndex)
		{
			AppendAliasHelper(
				Source,
				static_cast<EOpIntegralBitwiseOperator>(OperatorIndex),
				static_cast<EOpIntegralBitwiseType>(TypeIndex));
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

int32 FOpIntegralBitwiseGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpIntegralBitwiseGeneratorDetail;
	FBitwiseCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}
}
