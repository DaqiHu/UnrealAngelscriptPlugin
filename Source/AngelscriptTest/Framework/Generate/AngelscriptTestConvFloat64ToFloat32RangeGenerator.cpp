#include "Framework/Generate/AngelscriptTestConvFloat64ToFloat32RangeGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ConvFloat64ToFloat32RangeGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT(
	"Needs ObserveFloat32Bits host registration. ExpectedFiniteBits is the float32 inf pattern; overflow is not Divide by zero.");

struct FConvFloat64ToFloat32RangeCell
{
	EConvFloat64ToFloat32RangeForm Form = EConvFloat64ToFloat32RangeForm::Assignment;
	EConvFloat64ToFloat32RangeBoundaryDirection BoundaryDirection =
		EConvFloat64ToFloat32RangeBoundaryDirection::AboveTargetMax;
};

const TCHAR* FormToken(const EConvFloat64ToFloat32RangeForm Form)
{
	switch (Form)
	{
	case EConvFloat64ToFloat32RangeForm::Assignment:
		return TEXT("ASSIGNMENT");
	case EConvFloat64ToFloat32RangeForm::Argument:
		return TEXT("ARGUMENT");
	case EConvFloat64ToFloat32RangeForm::Return:
		return TEXT("RETURN");
	case EConvFloat64ToFloat32RangeForm::ExplicitCast:
		return TEXT("EXPLICIT_CAST");
	default:
		return nullptr;
	}
}

const TCHAR* BoundaryToken(const EConvFloat64ToFloat32RangeBoundaryDirection BoundaryDirection)
{
	switch (BoundaryDirection)
	{
	case EConvFloat64ToFloat32RangeBoundaryDirection::AboveTargetMax:
		return TEXT("ABOVE_TARGET_MAX");
	case EConvFloat64ToFloat32RangeBoundaryDirection::BelowTargetMin:
		return TEXT("BELOW_TARGET_MIN");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FConvFloat64ToFloat32RangeParams& Params)
{
	return FormToken(Params.Form) != nullptr && BoundaryToken(Params.BoundaryDirection) != nullptr;
}

int32 ExpectedBits(const EConvFloat64ToFloat32RangeBoundaryDirection BoundaryDirection)
{
	return BoundaryDirection == EConvFloat64ToFloat32RangeBoundaryDirection::AboveTargetMax
		? static_cast<int32>(0x7F800000u)
		: static_cast<int32>(0xFF800000u);
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FConvFloat64ToFloat32RangeCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CONV-FLOAT64-TO-FLOAT32-RANGE-%s-%s"),
		FormToken(Cell.Form),
		BoundaryToken(Cell.BoundaryDirection));
}

FString MakeEntryName(const FConvFloat64ToFloat32RangeCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FConvFloat64ToFloat32RangeParams ParamsFromCell(const FConvFloat64ToFloat32RangeCell& Cell)
{
	FConvFloat64ToFloat32RangeParams Params;
	Params.Form = Cell.Form;
	Params.BoundaryDirection = Cell.BoundaryDirection;
	return Params;
}

TArray<FConvFloat64ToFloat32RangeCell> AllCells()
{
	TArray<FConvFloat64ToFloat32RangeCell> Cells;
	Cells.SetNum(8);
	for (int32 Index = 0; Index < 8; ++Index)
	{
		FConvFloat64ToFloat32RangeCell& Cell = Cells[Index];
		Cell.Form = static_cast<EConvFloat64ToFloat32RangeForm>(Index / 2);
		Cell.BoundaryDirection = static_cast<EConvFloat64ToFloat32RangeBoundaryDirection>(Index % 2);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FConvFloat64ToFloat32RangeCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FConvFloat64ToFloat32RangeCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendPassHelper(FString& Source)
{
	AppendLine(Source, TEXT("float32 PassNumericBoundaryTargetFloat32(float32 Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
}

void AppendReturnHelper(FString& Source)
{
	AppendLine(Source, TEXT("float32 ReturnNumericBoundaryTargetFloat64Float32(float Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
}

void AppendAggregateHelpers(FString& Source)
{
	AppendPassHelper(Source);
	AppendLine(Source);
	AppendReturnHelper(Source);
}

void AppendOwnedHelpers(FString& Source, const FConvFloat64ToFloat32RangeParams& Params)
{
	if (Params.Form == EConvFloat64ToFloat32RangeForm::Argument)
	{
		AppendPassHelper(Source);
		AppendLine(Source);
	}
	else if (Params.Form == EConvFloat64ToFloat32RangeForm::Return)
	{
		AppendReturnHelper(Source);
		AppendLine(Source);
	}
}

void AppendEntry(FString& Source, const FConvFloat64ToFloat32RangeParams& Params, const FString& FunctionName)
{
	const TCHAR* const Literal = Params.BoundaryDirection
			== EConvFloat64ToFloat32RangeBoundaryDirection::AboveTargetMax
		? TEXT("6.805646932e+38")
		: TEXT("-6.805646932e+38");
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\tfloat SourceValue = %s;"), Literal));
	if (Params.Form == EConvFloat64ToFloat32RangeForm::Assignment)
	{
		AppendLine(Source, TEXT("\tfloat32 Converted;"));
		AppendLine(Source, TEXT("\tConverted = SourceValue;"));
	}
	else if (Params.Form == EConvFloat64ToFloat32RangeForm::Argument)
	{
		AppendLine(Source, TEXT("\tfloat32 Converted = PassNumericBoundaryTargetFloat32(SourceValue);"));
	}
	else if (Params.Form == EConvFloat64ToFloat32RangeForm::Return)
	{
		AppendLine(Source, TEXT("\tfloat32 Converted = ReturnNumericBoundaryTargetFloat64Float32(SourceValue);"));
	}
	else
	{
		AppendLine(Source, TEXT("\tfloat32 Converted = float32(SourceValue);"));
	}
	AppendLine(Source, TEXT("\treturn ObserveFloat32Bits(Converted);"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FConvFloat64ToFloat32RangeParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOwnedHelpers(Source, Params);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FConvFloat64ToFloat32RangeCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExpectedReturn = ExpectedBits(Cell.BoundaryDirection);
	Info.ExecutionNotes = HostNotes;
	return Info;
}
}

FStringView FConvFloat64ToFloat32RangeGenerator::GetProductId() const
{
	return TEXT("LANG-CONV-FLOAT64-TO-FLOAT32-RANGE");
}

TArray<FGeneratedCaseInfo> FConvFloat64ToFloat32RangeGenerator::ListCases() const
{
	using namespace ConvFloat64ToFloat32RangeGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FConvFloat64ToFloat32RangeCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("ConvFloat64ToFloat32Range PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FConvFloat64ToFloat32RangeGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ConvFloat64ToFloat32RangeGeneratorDetail;
	FConvFloat64ToFloat32RangeCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FConvFloat64ToFloat32RangeGenerator::BuildDumpSource() const
{
	using namespace ConvFloat64ToFloat32RangeGeneratorDetail;
	const TArray<FConvFloat64ToFloat32RangeCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CONV-FLOAT64-TO-FLOAT32-RANGE"));
	AppendLine(Source, TEXT("// Cases: 8 total, 8 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendAggregateHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FConvFloat64ToFloat32RangeCell& Cell = Cells[Index];
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FConvFloat64ToFloat32RangeGenerator::BuildNumericBoundaryConversionSource(
	const FConvFloat64ToFloat32RangeParams& Params) const
{
	using namespace ConvFloat64ToFloat32RangeGeneratorDetail;
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

FString FConvFloat64ToFloat32RangeGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ConvFloat64ToFloat32RangeGeneratorDetail;
	const TArray<FConvFloat64ToFloat32RangeCell> Cells = AllCells();
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

int32 FConvFloat64ToFloat32RangeGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ConvFloat64ToFloat32RangeGeneratorDetail;
	FConvFloat64ToFloat32RangeCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedBits(Cell.BoundaryDirection);
}
}
