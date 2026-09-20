#include "Framework/Generate/AngelscriptTestOpPowerUniversalGenerator.h"

#include "Containers/Map.h"
#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace OpPowerUniversalGeneratorDetail
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
	EOpPowerUniversalBase Base = EOpPowerUniversalBase::Int8;
	EOpPowerUniversalExponent Exponent = EOpPowerUniversalExponent::Int8;
	EOpPowerUniversalShape Shape = EOpPowerUniversalShape::Constant;
	EOpPowerUniversalScenario Scenario = EOpPowerUniversalScenario::ZeroExponent;
};

const TCHAR* TypeToken(const int32 TypeIndex)
{
	switch (TypeIndex)
	{
	case 0:
		return TEXT("INT8");
	case 1:
		return TEXT("INT16");
	case 2:
		return TEXT("INT");
	case 3:
		return TEXT("INT64");
	case 4:
		return TEXT("UINT8");
	case 5:
		return TEXT("UINT16");
	case 6:
		return TEXT("UINT");
	case 7:
		return TEXT("UINT64");
	case 8:
		return TEXT("FLOAT32");
	case 9:
		return TEXT("FLOAT64");
	default:
		return nullptr;
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

const TCHAR* MaxLiteral(const int32 TypeIndex)
{
	switch (TypeIndex)
	{
	case 0:
		return TEXT("127");
	case 1:
		return TEXT("32767");
	case 2:
		return TEXT("2147483647");
	case 3:
		return TEXT("9223372036854775807");
	case 4:
		return TEXT("255");
	case 5:
		return TEXT("65535");
	case 6:
		return TEXT("4294967295");
	case 7:
		return TEXT("18446744073709551615");
	case 8:
		return TEXT("3.402823466e+38");
	case 9:
		return TEXT("1.7976931348623157e+308");
	default:
		return TEXT("1");
	}
}

int32 BaseIndex(const EOpPowerUniversalBase Base)
{
	return static_cast<int32>(Base);
}

int32 ExponentIndex(const EOpPowerUniversalExponent Exponent)
{
	return static_cast<int32>(Exponent);
}

const TCHAR* BaseToken(const EOpPowerUniversalBase Base)
{
	return TypeToken(BaseIndex(Base));
}

const TCHAR* ExponentToken(const EOpPowerUniversalExponent Exponent)
{
	return TypeToken(ExponentIndex(Exponent));
}

const TCHAR* ShapeToken(const EOpPowerUniversalShape Shape)
{
	switch (Shape)
	{
	case EOpPowerUniversalShape::Constant:
		return TEXT("CONSTANT");
	case EOpPowerUniversalShape::MutableLvalue:
		return TEXT("MUTABLE_LVALUE");
	case EOpPowerUniversalShape::ConstLvalue:
		return TEXT("CONST_LVALUE");
	case EOpPowerUniversalShape::FunctionReturn:
		return TEXT("FUNCTION_RETURN");
	default:
		return nullptr;
	}
}

const TCHAR* ScenarioToken(const EOpPowerUniversalScenario Scenario)
{
	switch (Scenario)
	{
	case EOpPowerUniversalScenario::ZeroExponent:
		return TEXT("ZERO_EXPONENT");
	case EOpPowerUniversalScenario::OneExponent:
		return TEXT("ONE_EXPONENT");
	case EOpPowerUniversalScenario::NearLimit:
		return TEXT("NEAR_LIMIT");
	case EOpPowerUniversalScenario::Overflow:
		return TEXT("OVERFLOW");
	default:
		return nullptr;
	}
}

bool IsFloatType(const int32 TypeIndex)
{
	return TypeIndex == 8 || TypeIndex == 9;
}

bool IsUnsignedType(const int32 TypeIndex)
{
	return TypeIndex >= 4 && TypeIndex <= 7;
}

bool IsSignedType(const int32 TypeIndex)
{
	return TypeIndex >= 0 && TypeIndex <= 3;
}

bool IsWideType(const int32 TypeIndex)
{
	return TypeIndex == 3 || TypeIndex == 7 || TypeIndex == 9;
}

bool IsIntegerType(const int32 TypeIndex)
{
	return TypeIndex >= 0 && TypeIndex <= 7;
}

uint8 TypeWidth(const int32 TypeIndex)
{
	switch (TypeIndex)
	{
	case 0:
	case 4:
		return 1;
	case 1:
	case 5:
		return 2;
	case 2:
	case 6:
	case 8:
		return 4;
	default:
		return 8;
	}
}

bool IsValidParams(const FOpPowerUniversalParams& Params)
{
	return BaseToken(Params.Base) != nullptr
		&& ExponentToken(Params.Exponent) != nullptr
		&& ShapeToken(Params.Shape) != nullptr
		&& ScenarioToken(Params.Scenario) != nullptr;
}

bool IsConstantShape(const EOpPowerUniversalShape Shape)
{
	return Shape == EOpPowerUniversalShape::Constant;
}

bool IsCompileTimeKnown(const EOpPowerUniversalShape Shape)
{
	return Shape == EOpPowerUniversalShape::Constant || Shape == EOpPowerUniversalShape::ConstLvalue;
}

bool HasMixedIntegerSigns(const FPowerCell& Cell)
{
	const int32 Base = BaseIndex(Cell.Base);
	const int32 Exp = ExponentIndex(Cell.Exponent);
	return IsIntegerType(Base) && IsIntegerType(Exp) && (IsUnsignedType(Base) != IsUnsignedType(Exp));
}

EPowerKind ResultKind(const FPowerCell& Cell)
{
	const int32 Base = BaseIndex(Cell.Base);
	const int32 Exp = ExponentIndex(Cell.Exponent);
	if (Base == 9 || Exp == 9)
	{
		return EPowerKind::Float64;
	}
	if (Base == 8 || Exp == 8)
	{
		return EPowerKind::Float32;
	}
	const bool bWide = IsWideType(Base) || IsWideType(Exp);
	const bool bSigned = IsConstantShape(Cell.Shape)
		? !IsUnsignedType(Base) && !IsUnsignedType(Exp)
		: IsSignedType(Base) || IsSignedType(Exp);
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
	const EPowerKind Kind = ResultKind(Cell);
	if (Cell.Scenario == EOpPowerUniversalScenario::Overflow && IsCompileTimeKnown(Cell.Shape))
	{
		return IsConstantShape(Cell.Shape) || !HasMixedIntegerSigns(Cell);
	}
	return !IsCompileTimeKnown(Cell.Shape) && IsIntegerResult(Kind);
}

bool IsFaultCell(const FPowerCell& Cell)
{
	return Cell.Scenario == EOpPowerUniversalScenario::Overflow
		&& !IsCompileTimeKnown(Cell.Shape)
		&& !IsIntegerResult(ResultKind(Cell));
}

bool IsWrapCell(const FPowerCell& Cell)
{
	return Cell.Scenario == EOpPowerUniversalScenario::Overflow
		&& Cell.Shape == EOpPowerUniversalShape::ConstLvalue
		&& HasMixedIntegerSigns(Cell);
}

bool IsLimitedCell(const FPowerCell& Cell)
{
	if (IsRejectCell(Cell) || IsFaultCell(Cell))
	{
		return false;
	}
	const EPowerKind Kind = ResultKind(Cell);
	return IsWrapCell(Cell)
		|| Kind == EPowerKind::Signed64
		|| Kind == EPowerKind::Unsigned64
		|| Kind == EPowerKind::Float32
		|| Kind == EPowerKind::Float64;
}

int32 ExpectedFromCell(const FPowerCell& Cell)
{
	if (IsRejectCell(Cell) || IsFaultCell(Cell) || IsWrapCell(Cell))
	{
		return 0;
	}
	return Cell.Scenario == EOpPowerUniversalScenario::OneExponent ? 2 : 1;
}

const TCHAR* OverflowExponentLiteral(const EPowerKind Kind)
{
	switch (Kind)
	{
	case EPowerKind::Signed32:
		return TEXT("31");
	case EPowerKind::Unsigned32:
		return TEXT("32");
	case EPowerKind::Signed64:
		return TEXT("63");
	case EPowerKind::Unsigned64:
		return TEXT("64");
	case EPowerKind::Float32:
		return TEXT("128");
	case EPowerKind::Float64:
		return TEXT("1024");
	default:
		return TEXT("1");
	}
}

void OperandLiterals(const FPowerCell& Cell, const TCHAR*& OutBase, const TCHAR*& OutExp)
{
	const int32 Base = BaseIndex(Cell.Base);
	const int32 Exp = ExponentIndex(Cell.Exponent);
	switch (Cell.Scenario)
	{
	case EOpPowerUniversalScenario::ZeroExponent:
		OutBase = TEXT("2");
		OutExp = TEXT("0");
		break;
	case EOpPowerUniversalScenario::OneExponent:
		OutBase = TEXT("2");
		OutExp = TEXT("1");
		break;
	case EOpPowerUniversalScenario::NearLimit:
		OutBase = TEXT("1");
		if (Base == 9 && IsIntegerType(Exp) && TypeWidth(Exp) >= 4)
		{
			OutExp = TEXT("2147483647");
		}
		else
		{
			OutExp = MaxLiteral(Exp);
		}
		break;
	case EOpPowerUniversalScenario::Overflow:
		if (IsFloatType(Base) && IsIntegerType(Exp))
		{
			OutBase = MaxLiteral(Base);
			OutExp = TEXT("2");
		}
		else
		{
			OutBase = TEXT("2");
			OutExp = OverflowExponentLiteral(ResultKind(Cell));
		}
		break;
	default:
		OutBase = TEXT("0");
		OutExp = TEXT("0");
		break;
	}
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FPowerCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-POWER-UNIVERSAL-%s-%s-%s-%s"),
		BaseToken(Cell.Base),
		ExponentToken(Cell.Exponent),
		ShapeToken(Cell.Shape),
		ScenarioToken(Cell.Scenario));
}

FString MakeEntryName(const FPowerCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpPowerUniversalParams ParamsFromCell(const FPowerCell& Cell)
{
	FOpPowerUniversalParams Params;
	Params.Base = Cell.Base;
	Params.Exponent = Cell.Exponent;
	Params.Shape = Cell.Shape;
	Params.Scenario = Cell.Scenario;
	return Params;
}

FPowerCell CellFromParams(const FOpPowerUniversalParams& Params)
{
	FPowerCell Cell;
	Cell.Base = Params.Base;
	Cell.Exponent = Params.Exponent;
	Cell.Shape = Params.Shape;
	Cell.Scenario = Params.Scenario;
	return Cell;
}

TArray<FPowerCell> AllCells()
{
	TArray<FPowerCell> Cells;
	Cells.SetNum(1600);
	for (int32 Index = 0; Index < 1600; ++Index)
	{
		FPowerCell& Cell = Cells[Index];
		Cell.Base = static_cast<EOpPowerUniversalBase>(Index / 160);
		Cell.Exponent = static_cast<EOpPowerUniversalExponent>((Index / 16) % 10);
		Cell.Shape = static_cast<EOpPowerUniversalShape>((Index / 4) % 4);
		Cell.Scenario = static_cast<EOpPowerUniversalScenario>(Index % 4);
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
	const FOpPowerUniversalParams& Params,
	const FString& FunctionName)
{
	const FPowerCell Cell = CellFromParams(Params);
	const TCHAR* BaseLit = nullptr;
	const TCHAR* ExpLit = nullptr;
	OperandLiterals(Cell, BaseLit, ExpLit);
	const TCHAR* const BaseType = ScriptName(BaseIndex(Params.Base));
	const TCHAR* const ExpType = ScriptName(ExponentIndex(Params.Exponent));
	AppendLine(Source, FString::Printf(TEXT("%s PowerBase_%s()"), BaseType, *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *CastLiteral(BaseType, BaseLit)));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("%s PowerExponent_%s()"), ExpType, *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *CastLiteral(ExpType, ExpLit)));
	AppendLine(Source, TEXT("}"));
}

void AppendEntry(FString& Source, const FOpPowerUniversalParams& Params, const FString& FunctionName)
{
	const FPowerCell Cell = CellFromParams(Params);
	const TCHAR* BaseLit = nullptr;
	const TCHAR* ExpLit = nullptr;
	OperandLiterals(Cell, BaseLit, ExpLit);
	const TCHAR* const BaseType = ScriptName(BaseIndex(Params.Base));
	const TCHAR* const ExpType = ScriptName(ExponentIndex(Params.Exponent));
	const TCHAR* const ResultType = ResultScript(ResultKind(Cell));
	const FString BaseExpr = CastLiteral(BaseType, BaseLit);
	const FString ExpExpr = CastLiteral(ExpType, ExpLit);

	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Shape == EOpPowerUniversalShape::Constant)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\tconst %s Result = %s ** %s; // POWER_CAUSE"),
			ResultType,
			*BaseExpr,
			*ExpExpr));
	}
	else if (Params.Shape == EOpPowerUniversalShape::MutableLvalue)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Base = %s;"), BaseType, *BaseExpr));
		AppendLine(Source, FString::Printf(TEXT("\t%s Exponent = %s;"), ExpType, *ExpExpr));
		AppendLine(Source, FString::Printf(TEXT("\tconst %s Result = Base ** Exponent; // POWER_CAUSE"), ResultType));
	}
	else if (Params.Shape == EOpPowerUniversalShape::ConstLvalue)
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

FString BuildOwnedSource(const FOpPowerUniversalParams& Params, const FString& FunctionName)
{
	FString Source;
	if (Params.Shape == EOpPowerUniversalShape::FunctionReturn)
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
	Info.Execution = EGeneratedCaseExecution::Standalone;
	if (IsFaultCell(Cell))
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.ExpectedException = TEXT("Overflow in exponent operation");
		return Info;
	}
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	Info.bLimitedObservation = IsLimitedCell(Cell);
	if (Info.bLimitedObservation)
	{
		Info.ExecutionNotes = TEXT("int32 is the truncated mathematical result, not ExpectedResultBits.");
	}
	return Info;
}

void AppendOwnedEntry(FString& Source, const FPowerCell& Cell)
{
	const FOpPowerUniversalParams Params = ParamsFromCell(Cell);
	const FString FunctionName = MakeEntryName(Cell);
	if (Cell.Shape == EOpPowerUniversalShape::FunctionReturn)
	{
		AppendFunctionReturnHelpers(Source, Params, FunctionName);
		AppendLine(Source);
	}
	AppendEntry(Source, Params, FunctionName);
}
}

FStringView FOpPowerUniversalGenerator::GetProductId() const
{
	return TEXT("LANG-OP-POWER-UNIVERSAL");
}

TArray<FGeneratedCaseInfo> FOpPowerUniversalGenerator::ListCases() const
{
	using namespace OpPowerUniversalGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FPowerCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cells[Index]);
			checkf(!EntryNames.Contains(EntryName), TEXT("OpPowerUniversal PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FOpPowerUniversalGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace OpPowerUniversalGeneratorDetail;
	FPowerCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FOpPowerUniversalGenerator::BuildDumpSource() const
{
	using namespace OpPowerUniversalGeneratorDetail;
	const TArray<FPowerCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-POWER-UNIVERSAL"));
	AppendLine(Source, TEXT("// Cases: 1600 total, 848 normal, 680 reject, 72 fault"));
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
		if (IsFaultCell(Cells[Index]))
		{
			AppendLine(Source, TEXT("// Outcome: RuntimeException"));
			AppendLine(Source, TEXT("// ExpectedException: Overflow in exponent operation"));
		}
		else
		{
			AppendLine(Source, TEXT("// Outcome: ReturnValue"));
			AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		}
		AppendLine(Source, TEXT("// Execution: Standalone"));
		if (Info.bLimitedObservation)
		{
			AppendLine(Source, TEXT("// LimitedObservation: true"));
			AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		}
		AppendLine(Source);
		AppendOwnedEntry(Source, Cells[Index]);
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
		AppendOwnedEntry(Source, Cells[Index]);
	}
	return Source;
}

FString FOpPowerUniversalGenerator::BuildPowerSource(const FOpPowerUniversalParams& Params) const
{
	using namespace OpPowerUniversalGeneratorDetail;
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

FString FOpPowerUniversalGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace OpPowerUniversalGeneratorDetail;
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
		AppendOwnedEntry(Source, Cells[Index]);
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

int32 FOpPowerUniversalGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpPowerUniversalGeneratorDetail;
	FPowerCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsRejectCell(Cell) || IsFaultCell(Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}

FString FOpPowerUniversalGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace OpPowerUniversalGeneratorDetail;
	FPowerCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsRejectCell(Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FOpPowerUniversalGenerator::ListRejectCaseIds() const
{
	using namespace OpPowerUniversalGeneratorDetail;
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
