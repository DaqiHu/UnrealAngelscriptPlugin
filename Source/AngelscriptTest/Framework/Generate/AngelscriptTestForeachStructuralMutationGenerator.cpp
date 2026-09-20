#include "Framework/Generate/AngelscriptTestForeachStructuralMutationGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ForeachStructuralMutationGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native FNativeCaseRange and FNativeCaseValue.");
constexpr int32 SizeAxisCount = 3;
constexpr int32 MutationAxisCount = 4;
constexpr int32 ElementAxisCount = 3;
constexpr int32 TotalCells = SizeAxisCount * MutationAxisCount * ElementAxisCount;

struct FMutationCell
{
	EForeachMutationSize Size = EForeachMutationSize::One;
	EForeachMutation Mutation = EForeachMutation::Stable;
	EForeachMutationElement Element = EForeachMutationElement::PrimitiveValue;
};

const TCHAR* SizeToken(const EForeachMutationSize Size)
{
	switch (Size)
	{
	case EForeachMutationSize::One:
		return TEXT("ONE");
	case EForeachMutationSize::Two:
		return TEXT("TWO");
	case EForeachMutationSize::Many:
		return TEXT("MANY");
	default:
		return nullptr;
	}
}

const TCHAR* MutationToken(const EForeachMutation Mutation)
{
	switch (Mutation)
	{
	case EForeachMutation::Stable:
		return TEXT("STABLE");
	case EForeachMutation::ShrinkFirst:
		return TEXT("SHRINK_FIRST");
	case EForeachMutation::ShrinkMiddle:
		return TEXT("SHRINK_MIDDLE");
	case EForeachMutation::ClearAfterFirst:
		return TEXT("CLEAR_AFTER_FIRST");
	default:
		return nullptr;
	}
}

const TCHAR* ElementToken(const EForeachMutationElement Element)
{
	switch (Element)
	{
	case EForeachMutationElement::PrimitiveValue:
		return TEXT("PRIMITIVE_VALUE");
	case EForeachMutationElement::ValueObjectCopy:
		return TEXT("VALUE_OBJECT_COPY");
	case EForeachMutationElement::ValueObjectConstRef:
		return TEXT("VALUE_OBJECT_CONST_REF");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FForeachStructuralMutationParams& Params)
{
	return SizeToken(Params.Size) != nullptr
		&& MutationToken(Params.Mutation) != nullptr
		&& ElementToken(Params.Element) != nullptr;
}

int32 SizeValue(const EForeachMutationSize Size)
{
	if (Size == EForeachMutationSize::One)
	{
		return 1;
	}
	if (Size == EForeachMutationSize::Two)
	{
		return 2;
	}
	return 4;
}

int32 ExpectedMutationVisits(const FForeachStructuralMutationParams& Params)
{
	const int32 Size = SizeValue(Params.Size);
	if (Params.Mutation == EForeachMutation::Stable)
	{
		return Size;
	}
	if (Params.Mutation == EForeachMutation::ShrinkFirst
		|| Params.Mutation == EForeachMutation::ClearAfterFirst)
	{
		return Size < 1 ? Size : 1;
	}
	return Size < 2 ? Size : 2;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FMutationCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-FE-STRUCTURAL-MUTATION-%s-%s-%s"),
		SizeToken(Cell.Size),
		MutationToken(Cell.Mutation),
		ElementToken(Cell.Element));
}

FString MakeEntryName(const FMutationCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FForeachStructuralMutationParams ParamsFromCell(const FMutationCell& Cell)
{
	FForeachStructuralMutationParams Params;
	Params.Size = Cell.Size;
	Params.Mutation = Cell.Mutation;
	Params.Element = Cell.Element;
	return Params;
}

bool IsValueObject(const EForeachMutationElement Element)
{
	return Element != EForeachMutationElement::PrimitiveValue;
}

FString RangeTypeName(const FString& FunctionName)
{
	return FString(TEXT("FForeachTransferRange_")) + FunctionName;
}

FString VariableDeclaration(const EForeachMutationElement Element, const TCHAR* VariableName)
{
	if (Element == EForeachMutationElement::PrimitiveValue)
	{
		return FString::Printf(TEXT("int %s"), VariableName);
	}
	if (Element == EForeachMutationElement::ValueObjectConstRef)
	{
		return FString::Printf(TEXT("const FNativeCaseValue& %s"), VariableName);
	}
	return FString::Printf(TEXT("FNativeCaseValue %s"), VariableName);
}

void AppendOwnedTypes(FString& Source, const FForeachStructuralMutationParams& Params, const FString& FunctionName)
{
	const FString TypeName = RangeTypeName(FunctionName);
	AppendLine(Source, FString::Printf(TEXT("struct %s"), *TypeName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseRange Native;"));
	if (IsValueObject(Params.Element))
	{
		AppendLine(Source, TEXT("\tFNativeCaseValue Stored;"));
	}
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint opForBegin()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn Native.opForBegin();"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tbool opForEnd(const int Iterator)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn Native.opForEnd(Iterator);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tvoid opForNext(int& inout Iterator)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tNative.opForNext(Iterator);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	if (IsValueObject(Params.Element))
	{
		AppendLine(Source, TEXT("\tFNativeCaseValue& opForValue(const int Iterator)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tStored.Value = Native.opForValue(Iterator);"));
		AppendLine(Source, TEXT("\t\treturn Stored;"));
		AppendLine(Source, TEXT("\t}"));
	}
	else
	{
		AppendLine(Source, TEXT("\tint& opForValue(const int Iterator)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn Native.opForValue(Iterator);"));
		AppendLine(Source, TEXT("\t}"));
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendEntry(FString& Source, const FForeachStructuralMutationParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Range;"), *RangeTypeName(FunctionName)));
	AppendLine(Source, FString::Printf(TEXT("\tRange.Native.Count = %d;"), SizeValue(Params.Size)));
	AppendLine(Source, TEXT("\tint Trace = 0;"));
	AppendLine(Source, TEXT("\tint Iteration = 0;"));
	AppendLine(Source, TEXT("\tforeach (") + VariableDeclaration(Params.Element, TEXT("Value")) + TEXT(" : Range)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tint CurrentIteration = Iteration;"));
	AppendLine(Source, TEXT("\t\t++Iteration;"));
	AppendLine(Source, TEXT("\t\tTrace += 1;"));
	if (Params.Mutation == EForeachMutation::ShrinkFirst)
	{
		AppendLine(Source, TEXT("\t\tif (CurrentIteration == 0)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, TEXT("\t\t\tRange.Native.Count = 1;"));
		AppendLine(Source, TEXT("\t\t}"));
	}
	else if (Params.Mutation == EForeachMutation::ShrinkMiddle)
	{
		AppendLine(Source, TEXT("\t\tif (CurrentIteration == 1)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, TEXT("\t\t\tRange.Native.Count = 2;"));
		AppendLine(Source, TEXT("\t\t}"));
	}
	else if (Params.Mutation == EForeachMutation::ClearAfterFirst)
	{
		AppendLine(Source, TEXT("\t\tif (CurrentIteration == 0)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, TEXT("\t\t\tRange.Native.Count = 0;"));
		AppendLine(Source, TEXT("\t\t}"));
	}
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("\treturn Trace;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FForeachStructuralMutationParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOwnedTypes(Source, Params, FunctionName);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

TArray<FMutationCell> AllCells()
{
	TArray<FMutationCell> Cells;
	Cells.SetNum(TotalCells);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		FMutationCell& Cell = Cells[Index];
		Cell.Size = static_cast<EForeachMutationSize>(Index / (MutationAxisCount * ElementAxisCount));
		const int32 Remainder = Index % (MutationAxisCount * ElementAxisCount);
		Cell.Mutation = static_cast<EForeachMutation>(Remainder / ElementAxisCount);
		Cell.Element = static_cast<EForeachMutationElement>(Remainder % ElementAxisCount);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FMutationCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FMutationCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FGeneratedCaseInfo MakeCaseInfo(const FMutationCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExpectedReturn = ExpectedMutationVisits(ParamsFromCell(Cell));
	Info.ExecutionNotes = HostNotes;
	return Info;
}
}

FStringView FForeachStructuralMutationGenerator::GetProductId() const
{
	return TEXT("LANG-FE-STRUCTURAL-MUTATION");
}

TArray<FGeneratedCaseInfo> FForeachStructuralMutationGenerator::ListCases() const
{
	using namespace ForeachStructuralMutationGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FMutationCell& Cell : AllCells())
	{
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("ForeachStructuralMutation PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MakeCaseInfo(Cell));
	}
	return Cases;
}

FString FForeachStructuralMutationGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ForeachStructuralMutationGeneratorDetail;
	FMutationCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FForeachStructuralMutationGenerator::BuildDumpSource() const
{
	using namespace ForeachStructuralMutationGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FE-STRUCTURAL-MUTATION"));
	AppendLine(Source, TEXT("// Cases: 36 total, 36 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));

	for (const FMutationCell& Cell : AllCells())
	{
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), HostNotes));
		AppendLine(Source);
		AppendOwnedTypes(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FForeachStructuralMutationGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ForeachStructuralMutationGeneratorDetail;
	FString Source;
	int32 Emitted = 0;
	for (const FMutationCell& Cell : AllCells())
	{
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		AppendOwnedTypes(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

FString FForeachStructuralMutationGenerator::BuildStructuralMutationSource(
	const FForeachStructuralMutationParams& Params) const
{
	using namespace ForeachStructuralMutationGeneratorDetail;
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

int32 FForeachStructuralMutationGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ForeachStructuralMutationGeneratorDetail;
	FMutationCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedMutationVisits(ParamsFromCell(Cell));
}
}
