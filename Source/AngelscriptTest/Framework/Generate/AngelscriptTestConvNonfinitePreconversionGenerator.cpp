#include "Framework/Generate/AngelscriptTestConvNonfinitePreconversionGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ConvNonfinitePreconversionGeneratorDetail
{
constexpr int32 SourceCount = 2;
constexpr int32 TargetCount = 10;
constexpr int32 FormCount = 4;
constexpr int32 ValueCount = 3;
constexpr int32 TotalCells = SourceCount * TargetCount * FormCount * ValueCount;

struct FNonfiniteCell
{
	EConvNonfiniteSource Source = EConvNonfiniteSource::Float32;
	EConvNonfiniteTarget Target = EConvNonfiniteTarget::Int8;
	EConvNonfiniteForm Form = EConvNonfiniteForm::Assignment;
	EConvNonfiniteValue Value = EConvNonfiniteValue::PositiveInfinity;
};

const TCHAR* SourceToken(const EConvNonfiniteSource Source)
{
	switch (Source)
	{
	case EConvNonfiniteSource::Float32:
		return TEXT("FLOAT32");
	case EConvNonfiniteSource::Float64:
		return TEXT("FLOAT64");
	default:
		return nullptr;
	}
}

const TCHAR* TargetToken(const EConvNonfiniteTarget Target)
{
	switch (Target)
	{
	case EConvNonfiniteTarget::Int8:
		return TEXT("INT8");
	case EConvNonfiniteTarget::Int16:
		return TEXT("INT16");
	case EConvNonfiniteTarget::Int:
		return TEXT("INT");
	case EConvNonfiniteTarget::Int64:
		return TEXT("INT64");
	case EConvNonfiniteTarget::Uint8:
		return TEXT("UINT8");
	case EConvNonfiniteTarget::Uint16:
		return TEXT("UINT16");
	case EConvNonfiniteTarget::Uint:
		return TEXT("UINT");
	case EConvNonfiniteTarget::Uint64:
		return TEXT("UINT64");
	case EConvNonfiniteTarget::Float32:
		return TEXT("FLOAT32");
	case EConvNonfiniteTarget::Float64:
		return TEXT("FLOAT64");
	default:
		return nullptr;
	}
}

const TCHAR* FormToken(const EConvNonfiniteForm Form)
{
	switch (Form)
	{
	case EConvNonfiniteForm::Assignment:
		return TEXT("ASSIGNMENT");
	case EConvNonfiniteForm::Argument:
		return TEXT("ARGUMENT");
	case EConvNonfiniteForm::Return:
		return TEXT("RETURN");
	case EConvNonfiniteForm::ExplicitCast:
		return TEXT("EXPLICIT_CAST");
	default:
		return nullptr;
	}
}

const TCHAR* ValueToken(const EConvNonfiniteValue Value)
{
	switch (Value)
	{
	case EConvNonfiniteValue::PositiveInfinity:
		return TEXT("POSITIVE_INFINITY");
	case EConvNonfiniteValue::NegativeInfinity:
		return TEXT("NEGATIVE_INFINITY");
	case EConvNonfiniteValue::Nan:
		return TEXT("NAN");
	default:
		return nullptr;
	}
}

const TCHAR* SourceScriptType(const EConvNonfiniteSource Source)
{
	return Source == EConvNonfiniteSource::Float32 ? TEXT("float") : TEXT("double");
}

const TCHAR* TargetScriptType(const EConvNonfiniteTarget Target)
{
	switch (Target)
	{
	case EConvNonfiniteTarget::Int8:
		return TEXT("int8");
	case EConvNonfiniteTarget::Int16:
		return TEXT("int16");
	case EConvNonfiniteTarget::Int:
		return TEXT("int");
	case EConvNonfiniteTarget::Int64:
		return TEXT("int64");
	case EConvNonfiniteTarget::Uint8:
		return TEXT("uint8");
	case EConvNonfiniteTarget::Uint16:
		return TEXT("uint16");
	case EConvNonfiniteTarget::Uint:
		return TEXT("uint");
	case EConvNonfiniteTarget::Uint64:
		return TEXT("uint64");
	case EConvNonfiniteTarget::Float32:
		return TEXT("float");
	case EConvNonfiniteTarget::Float64:
		return TEXT("double");
	default:
		return nullptr;
	}
}

const TCHAR* TargetHelperSuffix(const EConvNonfiniteTarget Target)
{
	switch (Target)
	{
	case EConvNonfiniteTarget::Int8:
		return TEXT("Int8");
	case EConvNonfiniteTarget::Int16:
		return TEXT("Int16");
	case EConvNonfiniteTarget::Int:
		return TEXT("Int");
	case EConvNonfiniteTarget::Int64:
		return TEXT("Int64");
	case EConvNonfiniteTarget::Uint8:
		return TEXT("Uint8");
	case EConvNonfiniteTarget::Uint16:
		return TEXT("Uint16");
	case EConvNonfiniteTarget::Uint:
		return TEXT("Uint");
	case EConvNonfiniteTarget::Uint64:
		return TEXT("Uint64");
	case EConvNonfiniteTarget::Float32:
		return TEXT("Float");
	case EConvNonfiniteTarget::Float64:
		return TEXT("Double");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FConvNonfinitePreconversionParams& Params)
{
	return SourceToken(Params.Source) != nullptr
		&& TargetToken(Params.Target) != nullptr
		&& FormToken(Params.Form) != nullptr
		&& ValueToken(Params.Value) != nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FNonfiniteCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CONV-NONFINITE-PRECONVERSION-%s-%s-%s-%s"),
		SourceToken(Cell.Source),
		TargetToken(Cell.Target),
		FormToken(Cell.Form),
		ValueToken(Cell.Value));
}

FString MakeEntryName(const FNonfiniteCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FConvNonfinitePreconversionParams ParamsFromCell(const FNonfiniteCell& Cell)
{
	FConvNonfinitePreconversionParams Params;
	Params.Source = Cell.Source;
	Params.Target = Cell.Target;
	Params.Form = Cell.Form;
	Params.Value = Cell.Value;
	return Params;
}

FNonfiniteCell CellFromIndex(const int32 Index)
{
	FNonfiniteCell Cell;
	Cell.Source = static_cast<EConvNonfiniteSource>(Index / (TargetCount * FormCount * ValueCount));
	const int32 AfterSource = Index % (TargetCount * FormCount * ValueCount);
	Cell.Target = static_cast<EConvNonfiniteTarget>(AfterSource / (FormCount * ValueCount));
	const int32 AfterTarget = AfterSource % (FormCount * ValueCount);
	Cell.Form = static_cast<EConvNonfiniteForm>(AfterTarget / ValueCount);
	Cell.Value = static_cast<EConvNonfiniteValue>(AfterTarget % ValueCount);
	return Cell;
}

TArray<FNonfiniteCell> AllCells()
{
	TArray<FNonfiniteCell> Cells;
	Cells.Reserve(TotalCells);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		Cells.Add(CellFromIndex(Index));
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FNonfiniteCell& OutCell)
{
	const FString Needle(CaseId);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FNonfiniteCell Cell = CellFromIndex(Index);
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendPassHelper(FString& Source, const EConvNonfiniteTarget Target)
{
	const TCHAR* const Type = TargetScriptType(Target);
	const TCHAR* const Suffix = TargetHelperSuffix(Target);
	AppendLine(Source, FString::Printf(TEXT("%s PassNumeric%s(%s Value)"), Type, Suffix, Type));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
}

void AppendReturnHelper(FString& Source, const EConvNonfiniteSource SourceType, const EConvNonfiniteTarget Target)
{
	const TCHAR* const TargetType = TargetScriptType(Target);
	const TCHAR* const SourceName = SourceScriptType(SourceType);
	const TCHAR* const Suffix = TargetHelperSuffix(Target);
	AppendLine(Source, FString::Printf(TEXT("%s ReturnNumeric%s(%s Value)"), TargetType, Suffix, SourceName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
}

void AppendSharedHelpers(FString& Source)
{
	for (int32 TargetIndex = 0; TargetIndex < TargetCount; ++TargetIndex)
	{
		if (TargetIndex > 0)
		{
			AppendLine(Source);
		}
		AppendPassHelper(Source, static_cast<EConvNonfiniteTarget>(TargetIndex));
	}
	for (int32 SourceIndex = 0; SourceIndex < SourceCount; ++SourceIndex)
	{
		for (int32 TargetIndex = 0; TargetIndex < TargetCount; ++TargetIndex)
		{
			AppendLine(Source);
			AppendReturnHelper(
				Source,
				static_cast<EConvNonfiniteSource>(SourceIndex),
				static_cast<EConvNonfiniteTarget>(TargetIndex));
		}
	}
}

void AppendOwnedHelpers(FString& Source, const FConvNonfinitePreconversionParams& Params)
{
	if (Params.Form == EConvNonfiniteForm::Argument)
	{
		AppendPassHelper(Source, Params.Target);
		AppendLine(Source);
	}
	else if (Params.Form == EConvNonfiniteForm::Return)
	{
		AppendReturnHelper(Source, Params.Source, Params.Target);
		AppendLine(Source);
	}
}

void AppendSpecialSourceValue(FString& Source, const FConvNonfinitePreconversionParams& Params)
{
	const TCHAR* const SourceType = SourceScriptType(Params.Source);
	AppendLine(Source, FString::Printf(TEXT("\t%s Unit = 1.0;"), SourceType));
	AppendLine(Source, FString::Printf(TEXT("\t%s Zero = 0.0;"), SourceType));
	if (Params.Value == EConvNonfiniteValue::PositiveInfinity)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s SourceValue = Unit / Zero;"), SourceType));
	}
	else if (Params.Value == EConvNonfiniteValue::NegativeInfinity)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s SourceValue = -Unit / Zero;"), SourceType));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s SourceValue = Zero / Zero;"), SourceType));
	}
}

void AppendEntry(FString& Source, const FConvNonfinitePreconversionParams& Params, const FString& FunctionName)
{
	const TCHAR* const TargetType = TargetScriptType(Params.Target);
	const TCHAR* const Suffix = TargetHelperSuffix(Params.Target);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendSpecialSourceValue(Source, Params);
	if (Params.Form == EConvNonfiniteForm::Assignment)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Converted;"), TargetType));
		AppendLine(Source, TEXT("\tConverted = SourceValue;"));
	}
	else if (Params.Form == EConvNonfiniteForm::Argument)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Converted = PassNumeric%s(SourceValue);"), TargetType, Suffix));
	}
	else if (Params.Form == EConvNonfiniteForm::Return)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Converted = ReturnNumeric%s(SourceValue);"), TargetType, Suffix));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Converted = %s(SourceValue);"), TargetType, TargetType));
	}
	AppendLine(Source, TEXT("\treturn 0;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FConvNonfinitePreconversionParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOwnedHelpers(Source, Params);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FNonfiniteCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedException = TEXT("Divide by zero");
	return Info;
}
}

FStringView FConvNonfinitePreconversionGenerator::GetProductId() const
{
	return TEXT("LANG-CONV-NONFINITE-PRECONVERSION");
}

TArray<FGeneratedCaseInfo> FConvNonfinitePreconversionGenerator::ListCases() const
{
	using namespace ConvNonfinitePreconversionGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	Cases.Reserve(TotalCells);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FNonfiniteCell Cell = CellFromIndex(Index);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("ConvNonfinitePreconversion PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MakeCaseInfo(Cell));
	}
	return Cases;
}

FString FConvNonfinitePreconversionGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ConvNonfinitePreconversionGeneratorDetail;
	FNonfiniteCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FConvNonfinitePreconversionGenerator::BuildDumpSource() const
{
	using namespace ConvNonfinitePreconversionGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CONV-NONFINITE-PRECONVERSION"));
	AppendLine(Source, TEXT("// Cases: 240 total, 0 normal, 0 reject, 240 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FNonfiniteCell Cell = CellFromIndex(Index);
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: RuntimeException"));
		AppendLine(Source, TEXT("// ExpectedException: Divide by zero"));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FConvNonfinitePreconversionGenerator::BuildNumericBoundaryConversionSource(
	const FConvNonfinitePreconversionParams& Params) const
{
	using namespace ConvNonfinitePreconversionGeneratorDetail;
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

FString FConvNonfinitePreconversionGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ConvNonfinitePreconversionGeneratorDetail;
	FString Source;
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		AppendLine(Source);
		const FNonfiniteCell Cell = CellFromIndex(Index);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	OutCaseCount = TotalCells;
	return Source;
}

int32 FConvNonfinitePreconversionGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ConvNonfinitePreconversionGeneratorDetail;
	FNonfiniteCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return 0;
}
}
