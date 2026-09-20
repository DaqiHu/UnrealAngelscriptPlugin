#include "Framework/Generate/AngelscriptTestOpPowerNegativeExponentGenerator.h"

#include "Containers/Map.h"
#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace OpPowerNegativeExponentGeneratorDetail
{
enum class EPowerKind : uint8
{
	Signed32,
	Unsigned32,
	Signed64,
	Unsigned64,
	Float32,
	Float64,
};

struct FPowerCell
{
	EOpPowerNegativeExponentBase Base = EOpPowerNegativeExponentBase::Int8;
	EOpPowerNegativeExponentExponent Exponent = EOpPowerNegativeExponentExponent::Int8;
	EOpPowerNegativeExponentShape Shape = EOpPowerNegativeExponentShape::Constant;
};

const TCHAR* BaseToken(const EOpPowerNegativeExponentBase Base)
{
	switch (Base)
	{
	case EOpPowerNegativeExponentBase::Int8:
		return TEXT("INT8");
	case EOpPowerNegativeExponentBase::Int16:
		return TEXT("INT16");
	case EOpPowerNegativeExponentBase::Int:
		return TEXT("INT");
	case EOpPowerNegativeExponentBase::Int64:
		return TEXT("INT64");
	case EOpPowerNegativeExponentBase::Uint8:
		return TEXT("UINT8");
	case EOpPowerNegativeExponentBase::Uint16:
		return TEXT("UINT16");
	case EOpPowerNegativeExponentBase::Uint:
		return TEXT("UINT");
	case EOpPowerNegativeExponentBase::Uint64:
		return TEXT("UINT64");
	case EOpPowerNegativeExponentBase::Float32:
		return TEXT("FLOAT32");
	case EOpPowerNegativeExponentBase::Float64:
		return TEXT("FLOAT64");
	default:
		return nullptr;
	}
}

const TCHAR* ExponentToken(const EOpPowerNegativeExponentExponent Exponent)
{
	switch (Exponent)
	{
	case EOpPowerNegativeExponentExponent::Int8:
		return TEXT("INT8");
	case EOpPowerNegativeExponentExponent::Int16:
		return TEXT("INT16");
	case EOpPowerNegativeExponentExponent::Int:
		return TEXT("INT");
	case EOpPowerNegativeExponentExponent::Int64:
		return TEXT("INT64");
	case EOpPowerNegativeExponentExponent::Float32:
		return TEXT("FLOAT32");
	case EOpPowerNegativeExponentExponent::Float64:
		return TEXT("FLOAT64");
	default:
		return nullptr;
	}
}

const TCHAR* ShapeToken(const EOpPowerNegativeExponentShape Shape)
{
	switch (Shape)
	{
	case EOpPowerNegativeExponentShape::Constant:
		return TEXT("CONSTANT");
	case EOpPowerNegativeExponentShape::MutableLvalue:
		return TEXT("MUTABLE_LVALUE");
	case EOpPowerNegativeExponentShape::ConstLvalue:
		return TEXT("CONST_LVALUE");
	case EOpPowerNegativeExponentShape::FunctionReturn:
		return TEXT("FUNCTION_RETURN");
	default:
		return nullptr;
	}
}

const TCHAR* ScriptNameFromBase(const EOpPowerNegativeExponentBase Base)
{
	switch (Base)
	{
	case EOpPowerNegativeExponentBase::Int8:
		return TEXT("int8");
	case EOpPowerNegativeExponentBase::Int16:
		return TEXT("int16");
	case EOpPowerNegativeExponentBase::Int:
		return TEXT("int");
	case EOpPowerNegativeExponentBase::Int64:
		return TEXT("int64");
	case EOpPowerNegativeExponentBase::Uint8:
		return TEXT("uint8");
	case EOpPowerNegativeExponentBase::Uint16:
		return TEXT("uint16");
	case EOpPowerNegativeExponentBase::Uint:
		return TEXT("uint");
	case EOpPowerNegativeExponentBase::Uint64:
		return TEXT("uint64");
	case EOpPowerNegativeExponentBase::Float32:
		return TEXT("float32");
	case EOpPowerNegativeExponentBase::Float64:
		return TEXT("float64");
	default:
		return nullptr;
	}
}

const TCHAR* ScriptNameFromExponent(const EOpPowerNegativeExponentExponent Exponent)
{
	switch (Exponent)
	{
	case EOpPowerNegativeExponentExponent::Int8:
		return TEXT("int8");
	case EOpPowerNegativeExponentExponent::Int16:
		return TEXT("int16");
	case EOpPowerNegativeExponentExponent::Int:
		return TEXT("int");
	case EOpPowerNegativeExponentExponent::Int64:
		return TEXT("int64");
	case EOpPowerNegativeExponentExponent::Float32:
		return TEXT("float32");
	case EOpPowerNegativeExponentExponent::Float64:
		return TEXT("float64");
	default:
		return nullptr;
	}
}

bool IsFloatBase(const EOpPowerNegativeExponentBase Base)
{
	return Base == EOpPowerNegativeExponentBase::Float32 || Base == EOpPowerNegativeExponentBase::Float64;
}

bool IsFloatExponent(const EOpPowerNegativeExponentExponent Exponent)
{
	return Exponent == EOpPowerNegativeExponentExponent::Float32
		|| Exponent == EOpPowerNegativeExponentExponent::Float64;
}

bool IsUnsignedBase(const EOpPowerNegativeExponentBase Base)
{
	return Base == EOpPowerNegativeExponentBase::Uint8
		|| Base == EOpPowerNegativeExponentBase::Uint16
		|| Base == EOpPowerNegativeExponentBase::Uint
		|| Base == EOpPowerNegativeExponentBase::Uint64;
}

bool IsSignedBase(const EOpPowerNegativeExponentBase Base)
{
	return Base == EOpPowerNegativeExponentBase::Int8
		|| Base == EOpPowerNegativeExponentBase::Int16
		|| Base == EOpPowerNegativeExponentBase::Int
		|| Base == EOpPowerNegativeExponentBase::Int64;
}

bool IsWideBase(const EOpPowerNegativeExponentBase Base)
{
	return Base == EOpPowerNegativeExponentBase::Int64 || Base == EOpPowerNegativeExponentBase::Uint64
		|| Base == EOpPowerNegativeExponentBase::Float64;
}

bool IsWideExponent(const EOpPowerNegativeExponentExponent Exponent)
{
	return Exponent == EOpPowerNegativeExponentExponent::Int64
		|| Exponent == EOpPowerNegativeExponentExponent::Float64;
}

bool IsIntegerExponent(const EOpPowerNegativeExponentExponent Exponent)
{
	return !IsFloatExponent(Exponent);
}

bool IsConstantShape(const EOpPowerNegativeExponentShape Shape)
{
	return Shape == EOpPowerNegativeExponentShape::Constant;
}

bool IsCompileTimeKnown(const EOpPowerNegativeExponentShape Shape)
{
	return Shape == EOpPowerNegativeExponentShape::Constant
		|| Shape == EOpPowerNegativeExponentShape::ConstLvalue;
}

bool IsValidParams(const FOpPowerNegativeExponentParams& Params)
{
	return BaseToken(Params.Base) != nullptr
		&& ExponentToken(Params.Exponent) != nullptr
		&& ShapeToken(Params.Shape) != nullptr;
}

EPowerKind ResultKind(const FPowerCell& Cell)
{
	if (Cell.Base == EOpPowerNegativeExponentBase::Float64
		|| Cell.Exponent == EOpPowerNegativeExponentExponent::Float64)
	{
		return EPowerKind::Float64;
	}
	if (Cell.Base == EOpPowerNegativeExponentBase::Float32
		|| Cell.Exponent == EOpPowerNegativeExponentExponent::Float32)
	{
		return EPowerKind::Float32;
	}
	const bool bWide = IsWideBase(Cell.Base) || IsWideExponent(Cell.Exponent);
	const bool bSigned = IsConstantShape(Cell.Shape)
		? !IsUnsignedBase(Cell.Base)
		: IsSignedBase(Cell.Base) || IsIntegerExponent(Cell.Exponent);
	if (bWide)
	{
		return bSigned ? EPowerKind::Signed64 : EPowerKind::Unsigned64;
	}
	return bSigned ? EPowerKind::Signed32 : EPowerKind::Unsigned32;
}

const TCHAR* ResultScript(const EPowerKind Kind)
{
	switch (Kind)
	{
	case EPowerKind::Signed32:
		return TEXT("int");
	case EPowerKind::Unsigned32:
		return TEXT("uint");
	case EPowerKind::Signed64:
		return TEXT("int64");
	case EPowerKind::Unsigned64:
		return TEXT("uint64");
	case EPowerKind::Float32:
		return TEXT("float32");
	case EPowerKind::Float64:
		return TEXT("float64");
	default:
		return TEXT("int");
	}
}

bool IsIntegerResult(const EPowerKind Kind)
{
	return Kind == EPowerKind::Signed32 || Kind == EPowerKind::Unsigned32
		|| Kind == EPowerKind::Signed64 || Kind == EPowerKind::Unsigned64;
}

bool IsRejectCell(const FPowerCell& Cell)
{
	if (IsCompileTimeKnown(Cell.Shape) && IsUnsignedBase(Cell.Base) && IsIntegerExponent(Cell.Exponent))
	{
		return true;
	}
	return !IsCompileTimeKnown(Cell.Shape) && IsIntegerResult(ResultKind(Cell));
}

bool IsLimitedCell(const FPowerCell& Cell)
{
	const EPowerKind Kind = ResultKind(Cell);
	return Kind == EPowerKind::Signed64 || Kind == EPowerKind::Unsigned64
		|| Kind == EPowerKind::Float32 || Kind == EPowerKind::Float64;
}

int32 ExpectedFromCell(const FPowerCell& Cell)
{
	if (IsRejectCell(Cell))
	{
		return 0;
	}
	return 0;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FPowerCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-POWER-NEGATIVE-EXPONENT-%s-%s-%s-NEGATIVE_EXPONENT"),
		BaseToken(Cell.Base),
		ExponentToken(Cell.Exponent),
		ShapeToken(Cell.Shape));
}

FString MakeEntryName(const FPowerCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpPowerNegativeExponentParams ParamsFromCell(const FPowerCell& Cell)
{
	FOpPowerNegativeExponentParams Params;
	Params.Base = Cell.Base;
	Params.Exponent = Cell.Exponent;
	Params.Shape = Cell.Shape;
	return Params;
}

FPowerCell CellFromParams(const FOpPowerNegativeExponentParams& Params)
{
	FPowerCell Cell;
	Cell.Base = Params.Base;
	Cell.Exponent = Params.Exponent;
	Cell.Shape = Params.Shape;
	return Cell;
}

TArray<FPowerCell> AllCells()
{
	TArray<FPowerCell> Cells;
	Cells.SetNum(240);
	for (int32 Index = 0; Index < 240; ++Index)
	{
		FPowerCell& Cell = Cells[Index];
		Cell.Base = static_cast<EOpPowerNegativeExponentBase>(Index / 24);
		Cell.Exponent = static_cast<EOpPowerNegativeExponentExponent>((Index / 4) % 6);
		Cell.Shape = static_cast<EOpPowerNegativeExponentShape>(Index % 4);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FPowerCell& OutCell)
{
	static const TArray<FPowerCell> Cells = AllCells();
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

FString CastLiteral(const TCHAR* TypeName, const TCHAR* Literal)
{
	return FString::Printf(TEXT("%s(%s)"), TypeName, Literal);
}

void AppendFunctionReturnHelpers(
	FString& Source,
	const FOpPowerNegativeExponentParams& Params,
	const FString& FunctionName)
{
	const TCHAR* const BaseType = ScriptNameFromBase(Params.Base);
	const TCHAR* const ExpType = ScriptNameFromExponent(Params.Exponent);
	AppendLine(Source, FString::Printf(TEXT("%s PowerBase_%s()"), BaseType, *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *CastLiteral(BaseType, TEXT("2"))));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("%s PowerExponent_%s()"), ExpType, *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *CastLiteral(ExpType, TEXT("-2"))));
	AppendLine(Source, TEXT("}"));
}

void AppendEntry(FString& Source, const FOpPowerNegativeExponentParams& Params, const FString& FunctionName)
{
	const FPowerCell Cell = CellFromParams(Params);
	const TCHAR* const BaseType = ScriptNameFromBase(Params.Base);
	const TCHAR* const ExpType = ScriptNameFromExponent(Params.Exponent);
	const TCHAR* const ResultType = ResultScript(ResultKind(Cell));
	const FString BaseExpr = CastLiteral(BaseType, TEXT("2"));
	const FString ExpExpr = CastLiteral(ExpType, TEXT("-2"));

	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Shape == EOpPowerNegativeExponentShape::Constant)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\tconst %s Result = %s ** %s; // POWER_CAUSE"),
			ResultType,
			*BaseExpr,
			*ExpExpr));
	}
	else if (Params.Shape == EOpPowerNegativeExponentShape::MutableLvalue)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Base = %s;"), BaseType, *BaseExpr));
		AppendLine(Source, FString::Printf(TEXT("\t%s Exponent = %s;"), ExpType, *ExpExpr));
		AppendLine(Source, FString::Printf(TEXT("\tconst %s Result = Base ** Exponent; // POWER_CAUSE"), ResultType));
	}
	else if (Params.Shape == EOpPowerNegativeExponentShape::ConstLvalue)
	{
		AppendLine(Source, FString::Printf(TEXT("\tconst %s Base = %s;"), BaseType, *BaseExpr));
		AppendLine(Source, FString::Printf(TEXT("\tconst %s Exponent = %s;"), ExpType, *ExpExpr));
		AppendLine(Source, FString::Printf(TEXT("\tconst %s Result = Base ** Exponent; // POWER_CAUSE"), ResultType));
	}
	else
	{
		AppendLine(Source, FString::Printf(
			TEXT("\tconst %s Result = PowerBase_%s() ** PowerExponent_%s(); // POWER_CAUSE"),
			ResultType,
			*FunctionName,
			*FunctionName));
	}
	AppendLine(Source, TEXT("\treturn int(Result);"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FOpPowerNegativeExponentParams& Params, const FString& FunctionName)
{
	FString Source;
	if (Params.Shape == EOpPowerNegativeExponentShape::FunctionReturn)
	{
		AppendFunctionReturnHelpers(Source, Params, FunctionName);
		AppendLine(Source);
	}
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FPowerCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsRejectCell(Cell))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	Info.bLimitedObservation = IsLimitedCell(Cell);
	if (Info.bLimitedObservation)
	{
		Info.ExecutionNotes = TEXT("int32 is the truncated 2 ** -2 result, not ExpectedResultBits.");
	}
	return Info;
}
}

FStringView FOpPowerNegativeExponentGenerator::GetProductId() const
{
	return TEXT("LANG-OP-POWER-NEGATIVE-EXPONENT");
}

TArray<FGeneratedCaseInfo> FOpPowerNegativeExponentGenerator::ListCases() const
{
	using namespace OpPowerNegativeExponentGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FPowerCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cells[Index]);
			checkf(!EntryNames.Contains(EntryName), TEXT("OpPowerNegativeExponent PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FOpPowerNegativeExponentGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace OpPowerNegativeExponentGeneratorDetail;
	FPowerCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FOpPowerNegativeExponentGenerator::BuildDumpSource() const
{
	using namespace OpPowerNegativeExponentGeneratorDetail;
	const TArray<FPowerCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-POWER-NEGATIVE-EXPONENT"));
	AppendLine(Source, TEXT("// Cases: 240 total, 144 normal, 96 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	bool bNeedBlank = true;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsRejectCell(Cells[Index]))
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
		AppendLine(Source, TEXT("// Execution: Standalone"));
		if (Info.bLimitedObservation)
		{
			AppendLine(Source, TEXT("// LimitedObservation: true"));
			AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		}
		AppendLine(Source);
		if (Cells[Index].Shape == EOpPowerNegativeExponentShape::FunctionReturn)
		{
			AppendFunctionReturnHelpers(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
			AppendLine(Source);
		}
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		bNeedBlank = true;
	}
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (!IsRejectCell(Cells[Index]))
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
		if (Cells[Index].Shape == EOpPowerNegativeExponentShape::FunctionReturn)
		{
			AppendFunctionReturnHelpers(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
			AppendLine(Source);
		}
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

FString FOpPowerNegativeExponentGenerator::BuildPowerSource(const FOpPowerNegativeExponentParams& Params) const
{
	using namespace OpPowerNegativeExponentGeneratorDetail;
	if (!IsValidParams(Params) || IsRejectCell(CellFromParams(Params)))
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

FString FOpPowerNegativeExponentGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace OpPowerNegativeExponentGeneratorDetail;
	const TArray<FPowerCell> Cells = AllCells();
	FString Source;
	int32 Emitted = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsRejectCell(Cells[Index]))
		{
			continue;
		}
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		if (Cells[Index].Shape == EOpPowerNegativeExponentShape::FunctionReturn)
		{
			AppendFunctionReturnHelpers(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
			AppendLine(Source);
		}
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

int32 FOpPowerNegativeExponentGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpPowerNegativeExponentGeneratorDetail;
	FPowerCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsRejectCell(Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}

FString FOpPowerNegativeExponentGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace OpPowerNegativeExponentGeneratorDetail;
	FPowerCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsRejectCell(Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FOpPowerNegativeExponentGenerator::ListRejectCaseIds() const
{
	using namespace OpPowerNegativeExponentGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FPowerCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsRejectCell(Cells[Index]))
		{
			Ids.Add(MakeCaseId(Cells[Index]));
		}
	}
	return Ids;
}
}
