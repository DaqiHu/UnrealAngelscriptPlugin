#include "Framework/Generate/AngelscriptTestConvFloatFiniteSpecialGenerator.h"

#include "Containers/Set.h"
#include "HAL/UnrealMemory.h"

#include <limits>

namespace AngelscriptTest::Generate
{
namespace ConvFloatFiniteSpecialGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT(
	"Needs ObserveFloat32Bits or ObserveFloat64Bits host registration. ExpectedFiniteBits is the converted return-slot pattern.");
constexpr const TCHAR* LimitedNotes = TEXT(
	"ExpectedFiniteBits does not fit in int32. int32 GetExpected is not bitwise proof.");

struct FConvFloatFiniteSpecialCell
{
	EConvFloatFiniteSpecialSource SourceType = EConvFloatFiniteSpecialSource::Float32;
	EConvFloatFiniteSpecialTarget TargetType = EConvFloatFiniteSpecialTarget::Int8;
	EConvFloatFiniteSpecialForm Form = EConvFloatFiniteSpecialForm::Assignment;
	EConvFloatFiniteSpecialValue Value = EConvFloatFiniteSpecialValue::PositiveZero;
};

const TCHAR* SourceToken(const EConvFloatFiniteSpecialSource SourceType)
{
	switch (SourceType)
	{
	case EConvFloatFiniteSpecialSource::Float32:
		return TEXT("FLOAT32");
	case EConvFloatFiniteSpecialSource::Float64:
		return TEXT("FLOAT64");
	default:
		return nullptr;
	}
}

const TCHAR* TargetToken(const EConvFloatFiniteSpecialTarget TargetType)
{
	switch (TargetType)
	{
	case EConvFloatFiniteSpecialTarget::Int8:
		return TEXT("INT8");
	case EConvFloatFiniteSpecialTarget::Int16:
		return TEXT("INT16");
	case EConvFloatFiniteSpecialTarget::Int:
		return TEXT("INT");
	case EConvFloatFiniteSpecialTarget::Int64:
		return TEXT("INT64");
	case EConvFloatFiniteSpecialTarget::Uint8:
		return TEXT("UINT8");
	case EConvFloatFiniteSpecialTarget::Uint16:
		return TEXT("UINT16");
	case EConvFloatFiniteSpecialTarget::Uint:
		return TEXT("UINT");
	case EConvFloatFiniteSpecialTarget::Uint64:
		return TEXT("UINT64");
	case EConvFloatFiniteSpecialTarget::Float32:
		return TEXT("FLOAT32");
	case EConvFloatFiniteSpecialTarget::Float64:
		return TEXT("FLOAT64");
	default:
		return nullptr;
	}
}

const TCHAR* FormToken(const EConvFloatFiniteSpecialForm Form)
{
	switch (Form)
	{
	case EConvFloatFiniteSpecialForm::Assignment:
		return TEXT("ASSIGNMENT");
	case EConvFloatFiniteSpecialForm::Argument:
		return TEXT("ARGUMENT");
	case EConvFloatFiniteSpecialForm::Return:
		return TEXT("RETURN");
	case EConvFloatFiniteSpecialForm::ExplicitCast:
		return TEXT("EXPLICIT_CAST");
	default:
		return nullptr;
	}
}

const TCHAR* ValueToken(const EConvFloatFiniteSpecialValue Value)
{
	switch (Value)
	{
	case EConvFloatFiniteSpecialValue::PositiveZero:
		return TEXT("POSITIVE_ZERO");
	case EConvFloatFiniteSpecialValue::NegativeZero:
		return TEXT("NEGATIVE_ZERO");
	case EConvFloatFiniteSpecialValue::Subnormal:
		return TEXT("SUBNORMAL");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FConvFloatFiniteSpecialParams& Params)
{
	return SourceToken(Params.SourceType) != nullptr
		&& TargetToken(Params.TargetType) != nullptr
		&& FormToken(Params.Form) != nullptr
		&& ValueToken(Params.Value) != nullptr;
}

bool IsFloat32Target(const EConvFloatFiniteSpecialTarget TargetType)
{
	return TargetType == EConvFloatFiniteSpecialTarget::Float32;
}

bool IsFloat64Target(const EConvFloatFiniteSpecialTarget TargetType)
{
	return TargetType == EConvFloatFiniteSpecialTarget::Float64;
}

bool IsFloatTarget(const EConvFloatFiniteSpecialTarget TargetType)
{
	return IsFloat32Target(TargetType) || IsFloat64Target(TargetType);
}

const TCHAR* SourceScriptType(const EConvFloatFiniteSpecialSource SourceType)
{
	return SourceType == EConvFloatFiniteSpecialSource::Float64 ? TEXT("float") : TEXT("float32");
}

const TCHAR* TargetScriptType(const EConvFloatFiniteSpecialTarget TargetType)
{
	switch (TargetType)
	{
	case EConvFloatFiniteSpecialTarget::Int8:
		return TEXT("int8");
	case EConvFloatFiniteSpecialTarget::Int16:
		return TEXT("int16");
	case EConvFloatFiniteSpecialTarget::Int:
		return TEXT("int");
	case EConvFloatFiniteSpecialTarget::Int64:
		return TEXT("int64");
	case EConvFloatFiniteSpecialTarget::Uint8:
		return TEXT("uint8");
	case EConvFloatFiniteSpecialTarget::Uint16:
		return TEXT("uint16");
	case EConvFloatFiniteSpecialTarget::Uint:
		return TEXT("uint");
	case EConvFloatFiniteSpecialTarget::Uint64:
		return TEXT("uint64");
	case EConvFloatFiniteSpecialTarget::Float32:
		return TEXT("float32");
	case EConvFloatFiniteSpecialTarget::Float64:
		return TEXT("float");
	default:
		return TEXT("int");
	}
}

uint64 Float32Bits(const float Value)
{
	uint32 Bits = 0;
	FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
	return Bits;
}

uint64 Float64Bits(const double Value)
{
	uint64 Bits = 0;
	FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
	return Bits;
}

uint64 ExpectedFiniteBits(const FConvFloatFiniteSpecialCell& Cell)
{
	if (Cell.Value == EConvFloatFiniteSpecialValue::PositiveZero)
	{
		return 0;
	}
	if (Cell.Value == EConvFloatFiniteSpecialValue::NegativeZero)
	{
		if (IsFloat32Target(Cell.TargetType))
		{
			return 0x80000000ull;
		}
		if (IsFloat64Target(Cell.TargetType))
		{
			return 0x8000000000000000ull;
		}
		return 0;
	}
	if (IsFloat32Target(Cell.TargetType))
	{
		return Cell.SourceType == EConvFloatFiniteSpecialSource::Float32
			? Float32Bits(std::numeric_limits<float>::denorm_min())
			: 0;
	}
	if (IsFloat64Target(Cell.TargetType))
	{
		return Cell.SourceType == EConvFloatFiniteSpecialSource::Float32
			? Float64Bits(static_cast<double>(std::numeric_limits<float>::denorm_min()))
			: Float64Bits(std::numeric_limits<double>::denorm_min());
	}
	return 0;
}

bool BitsFitInt32(const uint64 Bits)
{
	return Bits <= static_cast<uint64>(MAX_int32);
}

int32 BitsAsInt32(const uint64 Bits)
{
	return static_cast<int32>(static_cast<uint32>(Bits));
}

bool IsLimitedBits(const FConvFloatFiniteSpecialCell& Cell)
{
	const uint64 Bits = ExpectedFiniteBits(Cell);
	return IsFloat64Target(Cell.TargetType) && !BitsFitInt32(Bits);
}

int32 ExpectedMarker(const FConvFloatFiniteSpecialCell& Cell)
{
	if (IsLimitedBits(Cell))
	{
		return 0;
	}
	if (IsFloat32Target(Cell.TargetType))
	{
		return BitsAsInt32(ExpectedFiniteBits(Cell));
	}
	return static_cast<int32>(ExpectedFiniteBits(Cell));
}

FString AxisPascal(const TCHAR* Token)
{
	return MakeCanonicalEntryName(Token).Mid(5);
}

FString PassHelperName(const EConvFloatFiniteSpecialTarget TargetType)
{
	return FString::Printf(TEXT("PassNumericBoundaryTarget%s"), *AxisPascal(TargetToken(TargetType)));
}

FString ReturnHelperName(
	const EConvFloatFiniteSpecialSource SourceType,
	const EConvFloatFiniteSpecialTarget TargetType)
{
	return FString::Printf(
		TEXT("ReturnNumericBoundaryTarget%s%s"),
		*AxisPascal(SourceToken(SourceType)),
		*AxisPascal(TargetToken(TargetType)));
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FConvFloatFiniteSpecialCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CONV-FLOAT-FINITE-SPECIAL-%s-%s-%s-%s"),
		SourceToken(Cell.SourceType),
		TargetToken(Cell.TargetType),
		FormToken(Cell.Form),
		ValueToken(Cell.Value));
}

FString MakeEntryName(const FConvFloatFiniteSpecialCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FConvFloatFiniteSpecialParams ParamsFromCell(const FConvFloatFiniteSpecialCell& Cell)
{
	FConvFloatFiniteSpecialParams Params;
	Params.SourceType = Cell.SourceType;
	Params.TargetType = Cell.TargetType;
	Params.Form = Cell.Form;
	Params.Value = Cell.Value;
	return Params;
}

TArray<FConvFloatFiniteSpecialCell> AllCells()
{
	TArray<FConvFloatFiniteSpecialCell> Cells;
	Cells.SetNum(240);
	for (int32 Index = 0; Index < 240; ++Index)
	{
		FConvFloatFiniteSpecialCell& Cell = Cells[Index];
		Cell.SourceType = static_cast<EConvFloatFiniteSpecialSource>(Index / 120);
		Cell.TargetType = static_cast<EConvFloatFiniteSpecialTarget>((Index / 12) % 10);
		Cell.Form = static_cast<EConvFloatFiniteSpecialForm>((Index / 3) % 4);
		Cell.Value = static_cast<EConvFloatFiniteSpecialValue>(Index % 3);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FConvFloatFiniteSpecialCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FConvFloatFiniteSpecialCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendPassHelper(FString& Source, const EConvFloatFiniteSpecialTarget TargetType)
{
	const TCHAR* const Type = TargetScriptType(TargetType);
	const FString Name = PassHelperName(TargetType);
	AppendLine(Source, FString::Printf(TEXT("%s %s(%s Value)"), Type, *Name, Type));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
}

void AppendReturnHelper(
	FString& Source,
	const EConvFloatFiniteSpecialSource SourceType,
	const EConvFloatFiniteSpecialTarget TargetType)
{
	const TCHAR* const Target = TargetScriptType(TargetType);
	const TCHAR* const SourceKind = SourceScriptType(SourceType);
	const FString Name = ReturnHelperName(SourceType, TargetType);
	AppendLine(Source, FString::Printf(TEXT("%s %s(%s Value)"), Target, *Name, SourceKind));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
}

void AppendAggregateHelpers(FString& Source)
{
	for (int32 TargetIndex = 0; TargetIndex < 10; ++TargetIndex)
	{
		AppendPassHelper(Source, static_cast<EConvFloatFiniteSpecialTarget>(TargetIndex));
		AppendLine(Source);
	}
	for (int32 SourceIndex = 0; SourceIndex < 2; ++SourceIndex)
	{
		for (int32 TargetIndex = 0; TargetIndex < 10; ++TargetIndex)
		{
			AppendReturnHelper(
				Source,
				static_cast<EConvFloatFiniteSpecialSource>(SourceIndex),
				static_cast<EConvFloatFiniteSpecialTarget>(TargetIndex));
			AppendLine(Source);
		}
	}
}

void AppendOwnedHelpers(FString& Source, const FConvFloatFiniteSpecialParams& Params)
{
	if (Params.Form == EConvFloatFiniteSpecialForm::Argument)
	{
		AppendPassHelper(Source, Params.TargetType);
		AppendLine(Source);
	}
	else if (Params.Form == EConvFloatFiniteSpecialForm::Return)
	{
		AppendReturnHelper(Source, Params.SourceType, Params.TargetType);
		AppendLine(Source);
	}
}

void AppendSourceValue(FString& Source, const FConvFloatFiniteSpecialParams& Params)
{
	const TCHAR* const Type = SourceScriptType(Params.SourceType);
	if (Params.Value == EConvFloatFiniteSpecialValue::PositiveZero)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s SourceValue = 0.0;"), Type));
	}
	else if (Params.Value == EConvFloatFiniteSpecialValue::NegativeZero)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s SourceValue = -0.0;"), Type));
	}
	else
	{
		const TCHAR* const Literal = Params.SourceType == EConvFloatFiniteSpecialSource::Float32
			? TEXT("1.40129846e-45f")
			: TEXT("4.9406564584124654e-324");
		AppendLine(Source, FString::Printf(TEXT("\t%s SourceValue = %s;"), Type, Literal));
	}
}

void AppendEntry(FString& Source, const FConvFloatFiniteSpecialParams& Params, const FString& FunctionName)
{
	const TCHAR* const TargetType = TargetScriptType(Params.TargetType);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendSourceValue(Source, Params);
	if (Params.Form == EConvFloatFiniteSpecialForm::Assignment)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Converted;"), TargetType));
		AppendLine(Source, TEXT("\tConverted = SourceValue;"));
	}
	else if (Params.Form == EConvFloatFiniteSpecialForm::Argument)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s Converted = %s(SourceValue);"),
			TargetType,
			*PassHelperName(Params.TargetType)));
	}
	else if (Params.Form == EConvFloatFiniteSpecialForm::Return)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s Converted = %s(SourceValue);"),
			TargetType,
			*ReturnHelperName(Params.SourceType, Params.TargetType)));
	}
	else
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s Converted = %s(SourceValue);"),
			TargetType,
			TargetType));
	}
	if (IsFloat32Target(Params.TargetType))
	{
		AppendLine(Source, TEXT("\treturn ObserveFloat32Bits(Converted);"));
	}
	else if (IsFloat64Target(Params.TargetType))
	{
		AppendLine(Source, TEXT("\treturn ObserveFloat64Bits(Converted);"));
	}
	else
	{
		AppendLine(Source, TEXT("\treturn int(Converted);"));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FConvFloatFiniteSpecialParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOwnedHelpers(Source, Params);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FConvFloatFiniteSpecialCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	if (IsLimitedBits(Cell))
	{
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		Info.bLimitedObservation = true;
		Info.ExecutionNotes = LimitedNotes;
		return Info;
	}
	Info.ExpectedReturn = ExpectedMarker(Cell);
	if (IsFloatTarget(Cell.TargetType))
	{
		Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
		Info.ExecutionNotes = HostNotes;
		return Info;
	}
	Info.Execution = EGeneratedCaseExecution::Standalone;
	return Info;
}
}

FStringView FConvFloatFiniteSpecialGenerator::GetProductId() const
{
	return TEXT("LANG-CONV-FLOAT-FINITE-SPECIAL");
}

TArray<FGeneratedCaseInfo> FConvFloatFiniteSpecialGenerator::ListCases() const
{
	using namespace ConvFloatFiniteSpecialGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FConvFloatFiniteSpecialCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("ConvFloatFiniteSpecial PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FConvFloatFiniteSpecialGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ConvFloatFiniteSpecialGeneratorDetail;
	FConvFloatFiniteSpecialCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FConvFloatFiniteSpecialGenerator::BuildDumpSource() const
{
	using namespace ConvFloatFiniteSpecialGeneratorDetail;
	const TArray<FConvFloatFiniteSpecialCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CONV-FLOAT-FINITE-SPECIAL"));
	AppendLine(Source, TEXT("// Cases: 240 total, 240 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendAggregateHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FConvFloatFiniteSpecialCell& Cell = Cells[Index];
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		if (Info.ExpectedReturn.IsSet())
		{
			AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		}
		if (Info.Execution == EGeneratedCaseExecution::RequiresHostSetup)
		{
			AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		}
		else if (Info.Execution == EGeneratedCaseExecution::SourceOnly)
		{
			AppendLine(Source, TEXT("// Execution: SourceOnly"));
		}
		else
		{
			AppendLine(Source, TEXT("// Execution: Standalone"));
		}
		if (!Info.ExecutionNotes.IsEmpty())
		{
			AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		}
		if (Info.bLimitedObservation)
		{
			AppendLine(Source, TEXT("// LimitedObservation: true"));
		}
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FConvFloatFiniteSpecialGenerator::BuildNumericBoundaryConversionSource(
	const FConvFloatFiniteSpecialParams& Params) const
{
	using namespace ConvFloatFiniteSpecialGeneratorDetail;
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

FString FConvFloatFiniteSpecialGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ConvFloatFiniteSpecialGeneratorDetail;
	const TArray<FConvFloatFiniteSpecialCell> Cells = AllCells();
	OutCaseCount = Cells.Num();
	FString Source;
	AppendAggregateHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

int32 FConvFloatFiniteSpecialGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ConvFloatFiniteSpecialGeneratorDetail;
	FConvFloatFiniteSpecialCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedMarker(Cell);
}
}
