#include "Framework/Generate/AngelscriptTestOpPowerFractionalExponentGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace OpPowerFractionalExponentGeneratorDetail
{
constexpr int32 BaseCount = 10;
constexpr int32 ExponentCount = 2;
constexpr int32 ShapeCount = 4;
constexpr int32 CellCount = BaseCount * ExponentCount * ShapeCount;
constexpr const TCHAR* LimitedNotes = TEXT("int32 TypeMarker is not full-width power-bit proof.");

struct FPowerCell
{
	EOpPowerFractionalExponentBase Base = EOpPowerFractionalExponentBase::Int;
	EOpPowerFractionalExponentExponent Exponent = EOpPowerFractionalExponentExponent::Float32;
	EOpPowerFractionalExponentShape Shape = EOpPowerFractionalExponentShape::Constant;
	EOpPowerFractionalExponentScenario Scenario = EOpPowerFractionalExponentScenario::FractionalExponent;
};

const TCHAR* BaseToken(const EOpPowerFractionalExponentBase Base)
{
	switch (Base)
	{
	case EOpPowerFractionalExponentBase::Int8:
		return TEXT("INT8");
	case EOpPowerFractionalExponentBase::Int16:
		return TEXT("INT16");
	case EOpPowerFractionalExponentBase::Int:
		return TEXT("INT");
	case EOpPowerFractionalExponentBase::Int64:
		return TEXT("INT64");
	case EOpPowerFractionalExponentBase::Uint8:
		return TEXT("UINT8");
	case EOpPowerFractionalExponentBase::Uint16:
		return TEXT("UINT16");
	case EOpPowerFractionalExponentBase::Uint:
		return TEXT("UINT");
	case EOpPowerFractionalExponentBase::Uint64:
		return TEXT("UINT64");
	case EOpPowerFractionalExponentBase::Float32:
		return TEXT("FLOAT32");
	case EOpPowerFractionalExponentBase::Float64:
		return TEXT("FLOAT64");
	default:
		return nullptr;
	}
}

const TCHAR* BaseName(const EOpPowerFractionalExponentBase Base)
{
	switch (Base)
	{
	case EOpPowerFractionalExponentBase::Int8:
		return TEXT("int8");
	case EOpPowerFractionalExponentBase::Int16:
		return TEXT("int16");
	case EOpPowerFractionalExponentBase::Int:
		return TEXT("int");
	case EOpPowerFractionalExponentBase::Int64:
		return TEXT("int64");
	case EOpPowerFractionalExponentBase::Uint8:
		return TEXT("uint8");
	case EOpPowerFractionalExponentBase::Uint16:
		return TEXT("uint16");
	case EOpPowerFractionalExponentBase::Uint:
		return TEXT("uint");
	case EOpPowerFractionalExponentBase::Uint64:
		return TEXT("uint64");
	case EOpPowerFractionalExponentBase::Float32:
		return TEXT("float32");
	case EOpPowerFractionalExponentBase::Float64:
		return TEXT("float64");
	default:
		return nullptr;
	}
}

const TCHAR* ExponentToken(const EOpPowerFractionalExponentExponent Exponent)
{
	switch (Exponent)
	{
	case EOpPowerFractionalExponentExponent::Float32:
		return TEXT("FLOAT32");
	case EOpPowerFractionalExponentExponent::Float64:
		return TEXT("FLOAT64");
	default:
		return nullptr;
	}
}

const TCHAR* ExponentName(const EOpPowerFractionalExponentExponent Exponent)
{
	switch (Exponent)
	{
	case EOpPowerFractionalExponentExponent::Float32:
		return TEXT("float32");
	case EOpPowerFractionalExponentExponent::Float64:
		return TEXT("float64");
	default:
		return nullptr;
	}
}

const TCHAR* ShapeToken(const EOpPowerFractionalExponentShape Shape)
{
	switch (Shape)
	{
	case EOpPowerFractionalExponentShape::Constant:
		return TEXT("CONSTANT");
	case EOpPowerFractionalExponentShape::MutableLvalue:
		return TEXT("MUTABLE_LVALUE");
	case EOpPowerFractionalExponentShape::ConstLvalue:
		return TEXT("CONST_LVALUE");
	case EOpPowerFractionalExponentShape::FunctionReturn:
		return TEXT("FUNCTION_RETURN");
	default:
		return nullptr;
	}
}

const TCHAR* ScenarioToken(const EOpPowerFractionalExponentScenario Scenario)
{
	switch (Scenario)
	{
	case EOpPowerFractionalExponentScenario::FractionalExponent:
		return TEXT("FRACTIONAL_EXPONENT");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FOpPowerFractionalExponentParams& Params)
{
	return BaseToken(Params.Base) != nullptr
		&& ExponentToken(Params.Exponent) != nullptr
		&& ShapeToken(Params.Shape) != nullptr
		&& ScenarioToken(Params.Scenario) != nullptr;
}

int32 TypeMarker(const FPowerCell& Cell)
{
	return (Cell.Base == EOpPowerFractionalExponentBase::Float64
		|| Cell.Exponent == EOpPowerFractionalExponentExponent::Float64)
		? 106
		: 105;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FPowerCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-POWER-FRACTIONAL-EXPONENT-%s-%s-%s-%s"),
		BaseToken(Cell.Base),
		ExponentToken(Cell.Exponent),
		ShapeToken(Cell.Shape),
		ScenarioToken(Cell.Scenario));
}

FString MakeEntryName(const FPowerCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpPowerFractionalExponentParams ParamsFromCell(const FPowerCell& Cell)
{
	FOpPowerFractionalExponentParams Params;
	Params.Base = Cell.Base;
	Params.Exponent = Cell.Exponent;
	Params.Shape = Cell.Shape;
	Params.Scenario = Cell.Scenario;
	return Params;
}

FPowerCell CellFromIndex(const int32 Index)
{
	FPowerCell Cell;
	Cell.Base = static_cast<EOpPowerFractionalExponentBase>(Index / (ExponentCount * ShapeCount));
	Cell.Exponent = static_cast<EOpPowerFractionalExponentExponent>((Index / ShapeCount) % ExponentCount);
	Cell.Shape = static_cast<EOpPowerFractionalExponentShape>(Index % ShapeCount);
	return Cell;
}

bool TryFindCell(const FStringView CaseId, FPowerCell& OutCell)
{
	const FString Needle(CaseId);
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FPowerCell Cell = CellFromIndex(Index);
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FString BaseLiteral(const EOpPowerFractionalExponentBase Base)
{
	const TCHAR* const Literal = (Base == EOpPowerFractionalExponentBase::Float32
		|| Base == EOpPowerFractionalExponentBase::Float64)
		? TEXT("4.0")
		: TEXT("4");
	return FString::Printf(TEXT("%s(%s)"), BaseName(Base), Literal);
}

FString ExponentLiteral(const EOpPowerFractionalExponentExponent Exponent)
{
	return FString::Printf(TEXT("%s(0.5)"), ExponentName(Exponent));
}

FString HelperSuffix(const FOpPowerFractionalExponentParams& Params)
{
	return FString::Printf(TEXT("%s%s"), BaseToken(Params.Base), ExponentToken(Params.Exponent));
}

void AppendSharedHelpers(FString& Source)
{
	AppendLine(Source, TEXT("int ObserveNumericType(float32 Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 105;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ObserveNumericType(float64 Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 106;"));
	AppendLine(Source, TEXT("}"));
}

void AppendFunctionReturnHelpers(FString& Source, const FOpPowerFractionalExponentParams& Params)
{
	const FString Suffix = HelperSuffix(Params);
	AppendLine(Source, FString::Printf(TEXT("%s PowerBaseValue%s()"), BaseName(Params.Base), *Suffix));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *BaseLiteral(Params.Base)));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("%s PowerExponentValue%s()"), ExponentName(Params.Exponent), *Suffix));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *ExponentLiteral(Params.Exponent)));
	AppendLine(Source, TEXT("}"));
}

void AppendEntry(FString& Source, const FOpPowerFractionalExponentParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Shape == EOpPowerFractionalExponentShape::Constant)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\treturn ObserveNumericType(%s ** %s);"),
			*BaseLiteral(Params.Base),
			*ExponentLiteral(Params.Exponent)));
	}
	else if (Params.Shape == EOpPowerFractionalExponentShape::MutableLvalue)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Base = %s;"), BaseName(Params.Base), *BaseLiteral(Params.Base)));
		AppendLine(Source, FString::Printf(
			TEXT("\t%s Exponent = %s;"),
			ExponentName(Params.Exponent),
			*ExponentLiteral(Params.Exponent)));
		AppendLine(Source, TEXT("\treturn ObserveNumericType(Base ** Exponent);"));
	}
	else if (Params.Shape == EOpPowerFractionalExponentShape::ConstLvalue)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\tconst %s Base = %s;"),
			BaseName(Params.Base),
			*BaseLiteral(Params.Base)));
		AppendLine(Source, FString::Printf(
			TEXT("\tconst %s Exponent = %s;"),
			ExponentName(Params.Exponent),
			*ExponentLiteral(Params.Exponent)));
		AppendLine(Source, TEXT("\treturn ObserveNumericType(Base ** Exponent);"));
	}
	else
	{
		const FString Suffix = HelperSuffix(Params);
		AppendLine(Source, FString::Printf(
			TEXT("\treturn ObserveNumericType(PowerBaseValue%s() ** PowerExponentValue%s());"),
			*Suffix,
			*Suffix));
	}
	AppendLine(Source, TEXT("}"));
}

void AppendOwnedModule(FString& Source, const FOpPowerFractionalExponentParams& Params, const FString& FunctionName)
{
	AppendSharedHelpers(Source);
	if (Params.Shape == EOpPowerFractionalExponentShape::FunctionReturn)
	{
		AppendLine(Source);
		AppendFunctionReturnHelpers(Source, Params);
	}
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
}

FString BuildOwnedSource(const FOpPowerFractionalExponentParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOwnedModule(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FPowerCell& Cell)
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

FStringView FOpPowerFractionalExponentGenerator::GetProductId() const
{
	return TEXT("LANG-OP-POWER-FRACTIONAL-EXPONENT");
}

TArray<FGeneratedCaseInfo> FOpPowerFractionalExponentGenerator::ListCases() const
{
	using namespace OpPowerFractionalExponentGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FPowerCell Cell = CellFromIndex(Index);
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("OpPowerFractionalExponent PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FOpPowerFractionalExponentGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace OpPowerFractionalExponentGeneratorDetail;
	FPowerCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FOpPowerFractionalExponentGenerator::BuildDumpSource() const
{
	using namespace OpPowerFractionalExponentGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-POWER-FRACTIONAL-EXPONENT"));
	AppendLine(Source, TEXT("// Cases: 80 total, 80 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FPowerCell Cell = CellFromIndex(Index);
		const FOpPowerFractionalExponentParams Params = ParamsFromCell(Cell);
		if (Params.Shape == EOpPowerFractionalExponentShape::FunctionReturn)
		{
			AppendLine(Source);
			AppendFunctionReturnHelpers(Source, Params);
		}
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
		AppendEntry(Source, Params, MakeEntryName(Cell));
	}
	return Source;
}

FString FOpPowerFractionalExponentGenerator::BuildPowerSource(const FOpPowerFractionalExponentParams& Params) const
{
	using namespace OpPowerFractionalExponentGeneratorDetail;
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

FString FOpPowerFractionalExponentGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace OpPowerFractionalExponentGeneratorDetail;
	OutCaseCount = CellCount;
	FString Source;
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < CellCount; ++Index)
	{
		const FPowerCell Cell = CellFromIndex(Index);
		const FOpPowerFractionalExponentParams Params = ParamsFromCell(Cell);
		if (Params.Shape == EOpPowerFractionalExponentShape::FunctionReturn)
		{
			AppendLine(Source);
			AppendFunctionReturnHelpers(Source, Params);
		}
		AppendLine(Source);
		AppendEntry(Source, Params, MakeEntryName(Cell));
	}
	return Source;
}

int32 FOpPowerFractionalExponentGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpPowerFractionalExponentGeneratorDetail;
	FPowerCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return TypeMarker(Cell);
}
}
