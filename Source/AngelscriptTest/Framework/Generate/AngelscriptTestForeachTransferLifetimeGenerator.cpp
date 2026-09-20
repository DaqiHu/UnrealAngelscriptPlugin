#include "Framework/Generate/AngelscriptTestForeachTransferLifetimeGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ForeachTransferLifetimeGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native FNativeCaseRange and FNativeCaseValue.");
constexpr int32 TransferAxisCount = 7;
constexpr int32 NestingAxisCount = 4;
constexpr int32 ElementAxisCount = 3;
constexpr int32 TotalCells = TransferAxisCount * NestingAxisCount * ElementAxisCount;

struct FTransferCell
{
	EForeachLifetimeTransfer Transfer = EForeachLifetimeTransfer::Complete;
	EForeachLifetimeNesting Nesting = EForeachLifetimeNesting::Single;
	EForeachLifetimeElement Element = EForeachLifetimeElement::PrimitiveValue;
};

const TCHAR* TransferToken(const EForeachLifetimeTransfer Transfer)
{
	switch (Transfer)
	{
	case EForeachLifetimeTransfer::Complete:
		return TEXT("COMPLETE");
	case EForeachLifetimeTransfer::BreakFirst:
		return TEXT("BREAK_FIRST");
	case EForeachLifetimeTransfer::BreakMiddle:
		return TEXT("BREAK_MIDDLE");
	case EForeachLifetimeTransfer::ContinueFirst:
		return TEXT("CONTINUE_FIRST");
	case EForeachLifetimeTransfer::ContinueMiddle:
		return TEXT("CONTINUE_MIDDLE");
	case EForeachLifetimeTransfer::Return:
		return TEXT("RETURN");
	case EForeachLifetimeTransfer::Exception:
		return TEXT("EXCEPTION");
	default:
		return nullptr;
	}
}

const TCHAR* NestingToken(const EForeachLifetimeNesting Nesting)
{
	switch (Nesting)
	{
	case EForeachLifetimeNesting::Single:
		return TEXT("SINGLE");
	case EForeachLifetimeNesting::NestedSame:
		return TEXT("NESTED_SAME");
	case EForeachLifetimeNesting::NestedDistinct:
		return TEXT("NESTED_DISTINCT");
	case EForeachLifetimeNesting::InsideFor:
		return TEXT("INSIDE_FOR");
	default:
		return nullptr;
	}
}

const TCHAR* ElementToken(const EForeachLifetimeElement Element)
{
	switch (Element)
	{
	case EForeachLifetimeElement::PrimitiveValue:
		return TEXT("PRIMITIVE_VALUE");
	case EForeachLifetimeElement::ValueObjectCopy:
		return TEXT("VALUE_OBJECT_COPY");
	case EForeachLifetimeElement::ValueObjectConstRef:
		return TEXT("VALUE_OBJECT_CONST_REF");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FForeachTransferLifetimeParams& Params)
{
	return TransferToken(Params.Transfer) != nullptr
		&& NestingToken(Params.Nesting) != nullptr
		&& ElementToken(Params.Element) != nullptr;
}

bool IsFault(const EForeachLifetimeTransfer Transfer)
{
	return Transfer == EForeachLifetimeTransfer::Exception;
}

int32 TransferVisitCount(const EForeachLifetimeTransfer Transfer, const int32 Limit)
{
	if (Transfer == EForeachLifetimeTransfer::Return
		|| Transfer == EForeachLifetimeTransfer::Exception
		|| Transfer == EForeachLifetimeTransfer::BreakFirst)
	{
		return Limit < 1 ? Limit : 1;
	}
	if (Transfer == EForeachLifetimeTransfer::BreakMiddle)
	{
		return Limit < 2 ? Limit : 2;
	}
	return Limit;
}

int32 ExpectedFromParams(const FForeachTransferLifetimeParams& Params)
{
	if (IsFault(Params.Transfer))
	{
		return 0;
	}
	const int32 InnerLimit = (Params.Nesting == EForeachLifetimeNesting::NestedDistinct) ? 2 : 3;
	const int32 InnerVisits = TransferVisitCount(Params.Transfer, InnerLimit);
	const bool bStops = Params.Transfer == EForeachLifetimeTransfer::Return;
	if (Params.Nesting == EForeachLifetimeNesting::Single)
	{
		return InnerVisits;
	}
	if (Params.Nesting == EForeachLifetimeNesting::NestedSame
		|| Params.Nesting == EForeachLifetimeNesting::NestedDistinct)
	{
		const int32 Outer = bStops ? 1 : 3;
		return Outer * 100 + Outer * InnerVisits;
	}
	const int32 Outer = bStops ? 1 : 2;
	return Outer * 10 + Outer * InnerVisits;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FTransferCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-FE-TRANSFER-LIFETIME-%s-%s-%s"),
		TransferToken(Cell.Transfer),
		NestingToken(Cell.Nesting),
		ElementToken(Cell.Element));
}

FString MakeEntryName(const FTransferCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FForeachTransferLifetimeParams ParamsFromCell(const FTransferCell& Cell)
{
	FForeachTransferLifetimeParams Params;
	Params.Transfer = Cell.Transfer;
	Params.Nesting = Cell.Nesting;
	Params.Element = Cell.Element;
	return Params;
}

bool IsValueObject(const EForeachLifetimeElement Element)
{
	return Element != EForeachLifetimeElement::PrimitiveValue;
}

FString RangeTypeName(const FString& FunctionName)
{
	return FString(TEXT("FForeachTransferRange_")) + FunctionName;
}

FString VariableDeclaration(const EForeachLifetimeElement Element, const TCHAR* VariableName)
{
	if (Element == EForeachLifetimeElement::PrimitiveValue)
	{
		return FString::Printf(TEXT("int %s"), VariableName);
	}
	if (Element == EForeachLifetimeElement::ValueObjectConstRef)
	{
		return FString::Printf(TEXT("const FNativeCaseValue& %s"), VariableName);
	}
	return FString::Printf(TEXT("FNativeCaseValue %s"), VariableName);
}

void AppendOwnedTypes(FString& Source, const FForeachTransferLifetimeParams& Params, const FString& FunctionName)
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

void AppendTransferAction(
	FString& Source,
	const EForeachLifetimeTransfer Transfer,
	const TCHAR* IterationName,
	const FString& Indent)
{
	if (Transfer == EForeachLifetimeTransfer::BreakFirst)
	{
		AppendLine(Source, FString::Printf(TEXT("%sif (%s == 0)"), *Indent, IterationName));
		AppendLine(Source, Indent + TEXT("{"));
		AppendLine(Source, Indent + TEXT("\tbreak;"));
		AppendLine(Source, Indent + TEXT("}"));
	}
	else if (Transfer == EForeachLifetimeTransfer::BreakMiddle)
	{
		AppendLine(Source, FString::Printf(TEXT("%sif (%s == 1)"), *Indent, IterationName));
		AppendLine(Source, Indent + TEXT("{"));
		AppendLine(Source, Indent + TEXT("\tbreak;"));
		AppendLine(Source, Indent + TEXT("}"));
	}
	else if (Transfer == EForeachLifetimeTransfer::ContinueFirst)
	{
		AppendLine(Source, FString::Printf(TEXT("%sif (%s == 0)"), *Indent, IterationName));
		AppendLine(Source, Indent + TEXT("{"));
		AppendLine(Source, Indent + TEXT("\tcontinue;"));
		AppendLine(Source, Indent + TEXT("}"));
	}
	else if (Transfer == EForeachLifetimeTransfer::ContinueMiddle)
	{
		AppendLine(Source, FString::Printf(TEXT("%sif (%s == 1)"), *Indent, IterationName));
		AppendLine(Source, Indent + TEXT("{"));
		AppendLine(Source, Indent + TEXT("\tcontinue;"));
		AppendLine(Source, Indent + TEXT("}"));
	}
	else if (Transfer == EForeachLifetimeTransfer::Return)
	{
		AppendLine(Source, Indent + TEXT("return Trace;"));
	}
	else if (Transfer == EForeachLifetimeTransfer::Exception)
	{
		AppendLine(Source, Indent + TEXT("int Zero = 0;"));
		AppendLine(Source, Indent + TEXT("Trace += 1 / Zero;"));
	}
}

void AppendLoopBody(
	FString& Source,
	const FForeachTransferLifetimeParams& Params,
	const TCHAR* RangeName,
	const TCHAR* IterationName,
	const TCHAR* VariableName,
	const TCHAR* Indent)
{
	const FString Prefix(Indent);
	AppendLine(
		Source,
		Prefix + TEXT("foreach (") + VariableDeclaration(Params.Element, VariableName) + TEXT(" : ") + RangeName + TEXT(")"));
	AppendLine(Source, Prefix + TEXT("{"));
	AppendLine(Source, Prefix + TEXT("\tint CurrentIteration = ") + IterationName + TEXT(";"));
	AppendLine(Source, Prefix + TEXT("\t++") + IterationName + TEXT(";"));
	AppendLine(Source, Prefix + TEXT("\tTrace += 1;"));
	if (IsValueObject(Params.Element))
	{
		AppendLine(Source, Prefix + FString::Printf(TEXT("\tTrace += %s.Value - %s.Value;"), VariableName, VariableName));
	}
	else
	{
		AppendLine(Source, Prefix + FString::Printf(TEXT("\tTrace += %s - %s;"), VariableName, VariableName));
	}
	AppendTransferAction(Source, Params.Transfer, TEXT("CurrentIteration"), Prefix + TEXT("\t"));
	AppendLine(Source, Prefix + TEXT("}"));
}

void AppendEntry(FString& Source, const FForeachTransferLifetimeParams& Params, const FString& FunctionName)
{
	const FString TypeName = RangeTypeName(FunctionName);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Range;"), *TypeName));
	AppendLine(Source, TEXT("\tRange.Native.Count = 3;"));
	if (Params.Nesting == EForeachLifetimeNesting::NestedDistinct)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s OtherRange;"), *TypeName));
		AppendLine(Source, TEXT("\tOtherRange.Native.Count = 2;"));
	}
	AppendLine(Source, TEXT("\tint Trace = 0;"));
	if (Params.Nesting == EForeachLifetimeNesting::Single)
	{
		AppendLine(Source, TEXT("\tint Iteration = 0;"));
		AppendLoopBody(Source, Params, TEXT("Range"), TEXT("Iteration"), TEXT("Value"), TEXT("\t"));
	}
	else if (Params.Nesting == EForeachLifetimeNesting::NestedSame
		|| Params.Nesting == EForeachLifetimeNesting::NestedDistinct)
	{
		AppendLine(Source, TEXT("\tint OuterIteration = 0;"));
		AppendLine(Source, TEXT("\tforeach (") + VariableDeclaration(Params.Element, TEXT("OuterValue")) + TEXT(" : Range)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tint CurrentOuterIteration = OuterIteration;"));
		AppendLine(Source, TEXT("\t\t++OuterIteration;"));
		AppendLine(Source, TEXT("\t\tTrace += 100;"));
		AppendLine(Source, TEXT("\t\tint InnerIteration = 0;"));
		AppendLoopBody(
			Source,
			Params,
			Params.Nesting == EForeachLifetimeNesting::NestedSame ? TEXT("Range") : TEXT("OtherRange"),
			TEXT("InnerIteration"),
			TEXT("InnerValue"),
			TEXT("\t\t"));
		AppendLine(Source, TEXT("\t}"));
	}
	else
	{
		AppendLine(Source, TEXT("\tfor (int OuterIndex = 0; OuterIndex < 2; ++OuterIndex)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tTrace += 10;"));
		AppendLine(Source, TEXT("\t\tint Iteration = 0;"));
		AppendLoopBody(Source, Params, TEXT("Range"), TEXT("Iteration"), TEXT("Value"), TEXT("\t\t"));
		AppendLine(Source, TEXT("\t}"));
	}
	AppendLine(Source, TEXT("\treturn Trace;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FForeachTransferLifetimeParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOwnedTypes(Source, Params, FunctionName);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

TArray<FTransferCell> AllCells()
{
	TArray<FTransferCell> Cells;
	Cells.SetNum(TotalCells);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		FTransferCell& Cell = Cells[Index];
		Cell.Transfer = static_cast<EForeachLifetimeTransfer>(Index / (NestingAxisCount * ElementAxisCount));
		const int32 Remainder = Index % (NestingAxisCount * ElementAxisCount);
		Cell.Nesting = static_cast<EForeachLifetimeNesting>(Remainder / ElementAxisCount);
		Cell.Element = static_cast<EForeachLifetimeElement>(Remainder % ElementAxisCount);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FTransferCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FTransferCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FGeneratedCaseInfo MakeCaseInfo(const FTransferCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExecutionNotes = HostNotes;
	if (IsFault(Cell.Transfer))
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.ExpectedException = TEXT("Divide by zero");
	}
	else
	{
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.ExpectedReturn = ExpectedFromParams(ParamsFromCell(Cell));
	}
	return Info;
}
}

FStringView FForeachTransferLifetimeGenerator::GetProductId() const
{
	return TEXT("LANG-FE-TRANSFER-LIFETIME");
}

TArray<FGeneratedCaseInfo> FForeachTransferLifetimeGenerator::ListCases() const
{
	using namespace ForeachTransferLifetimeGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FTransferCell& Cell : AllCells())
	{
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("ForeachTransferLifetime PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MakeCaseInfo(Cell));
	}
	return Cases;
}

FString FForeachTransferLifetimeGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ForeachTransferLifetimeGeneratorDetail;
	FTransferCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FForeachTransferLifetimeGenerator::BuildDumpSource() const
{
	using namespace ForeachTransferLifetimeGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FE-TRANSFER-LIFETIME"));
	AppendLine(Source, TEXT("// Cases: 84 total, 72 normal, 0 reject, 12 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));

	for (const FTransferCell& Cell : AllCells())
	{
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		if (IsFault(Cell.Transfer))
		{
			AppendLine(Source, TEXT("// Outcome: RuntimeException"));
			AppendLine(Source, TEXT("// ExpectedException: Divide by zero"));
		}
		else
		{
			AppendLine(Source, TEXT("// Outcome: ReturnValue"));
			AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		}
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), HostNotes));
		AppendLine(Source);
		AppendOwnedTypes(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FForeachTransferLifetimeGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ForeachTransferLifetimeGeneratorDetail;
	FString Source;
	int32 Emitted = 0;
	for (const FTransferCell& Cell : AllCells())
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

FString FForeachTransferLifetimeGenerator::BuildTransferSource(const FForeachTransferLifetimeParams& Params) const
{
	using namespace ForeachTransferLifetimeGeneratorDetail;
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

int32 FForeachTransferLifetimeGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ForeachTransferLifetimeGeneratorDetail;
	FTransferCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromParams(ParamsFromCell(Cell));
}
}
