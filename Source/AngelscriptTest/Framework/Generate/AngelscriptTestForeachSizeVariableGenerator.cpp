#include "Framework/Generate/AngelscriptTestForeachSizeVariableGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ForeachSizeVariableGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native FNativeCaseRange and FNativeCaseReference.");
constexpr int32 SizeAxisCount = 4;
constexpr int32 ElementAxisCount = 4;
constexpr int32 VariableAxisCount = 5;
constexpr int32 TransferAxisCount = 8;
constexpr int32 TotalCells = SizeAxisCount * ElementAxisCount * VariableAxisCount * TransferAxisCount;

struct FSizeVariableCell
{
	EForeachSize Size = EForeachSize::Empty;
	EForeachElement Element = EForeachElement::Primitive;
	EForeachVariable Variable = EForeachVariable::Value;
	EForeachSizeTransfer Transfer = EForeachSizeTransfer::Complete;
};

const TCHAR* SizeToken(const EForeachSize Size)
{
	switch (Size)
	{
	case EForeachSize::Empty:
		return TEXT("EMPTY");
	case EForeachSize::One:
		return TEXT("ONE");
	case EForeachSize::Two:
		return TEXT("TWO");
	case EForeachSize::Many:
		return TEXT("MANY");
	default:
		return nullptr;
	}
}

const TCHAR* ElementToken(const EForeachElement Element)
{
	switch (Element)
	{
	case EForeachElement::Primitive:
		return TEXT("PRIMITIVE");
	case EForeachElement::ValueObject:
		return TEXT("VALUE_OBJECT");
	case EForeachElement::ReferenceObject:
		return TEXT("REFERENCE_OBJECT");
	case EForeachElement::ConstElement:
		return TEXT("CONST_ELEMENT");
	default:
		return nullptr;
	}
}

const TCHAR* VariableToken(const EForeachVariable Variable)
{
	switch (Variable)
	{
	case EForeachVariable::Value:
		return TEXT("VALUE");
	case EForeachVariable::Auto:
		return TEXT("AUTO");
	case EForeachVariable::MutableReference:
		return TEXT("MUTABLE_REFERENCE");
	case EForeachVariable::ConstReference:
		return TEXT("CONST_REFERENCE");
	case EForeachVariable::Incompatible:
		return TEXT("INCOMPATIBLE");
	default:
		return nullptr;
	}
}

const TCHAR* TransferToken(const EForeachSizeTransfer Transfer)
{
	switch (Transfer)
	{
	case EForeachSizeTransfer::Complete:
		return TEXT("COMPLETE");
	case EForeachSizeTransfer::BreakFirst:
		return TEXT("BREAK_FIRST");
	case EForeachSizeTransfer::BreakMiddle:
		return TEXT("BREAK_MIDDLE");
	case EForeachSizeTransfer::BreakLast:
		return TEXT("BREAK_LAST");
	case EForeachSizeTransfer::ContinueFirst:
		return TEXT("CONTINUE_FIRST");
	case EForeachSizeTransfer::ContinueMiddle:
		return TEXT("CONTINUE_MIDDLE");
	case EForeachSizeTransfer::Return:
		return TEXT("RETURN");
	case EForeachSizeTransfer::Exception:
		return TEXT("EXCEPTION");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FForeachSizeVariableParams& Params)
{
	return SizeToken(Params.Size) != nullptr
		&& ElementToken(Params.Element) != nullptr
		&& VariableToken(Params.Variable) != nullptr
		&& TransferToken(Params.Transfer) != nullptr;
}

int32 CountFor(const EForeachSize Size)
{
	if (Size == EForeachSize::Empty)
	{
		return 0;
	}
	if (Size == EForeachSize::One)
	{
		return 1;
	}
	if (Size == EForeachSize::Two)
	{
		return 2;
	}
	return 4;
}

bool IsReject(const FForeachSizeVariableParams& Params)
{
	return Params.Variable == EForeachVariable::Incompatible
		|| Params.Element == EForeachElement::ReferenceObject
		|| (Params.Element == EForeachElement::ConstElement
			&& Params.Variable == EForeachVariable::MutableReference);
}

bool IsFault(const FForeachSizeVariableParams& Params)
{
	return !IsReject(Params)
		&& Params.Transfer == EForeachSizeTransfer::Exception
		&& CountFor(Params.Size) > 0;
}

int32 ExpectedFromParams(const FForeachSizeVariableParams& Params)
{
	if (IsReject(Params) || IsFault(Params))
	{
		return 0;
	}
	const int32 Count = CountFor(Params.Size);
	const int32 Contribution = Params.Variable == EForeachVariable::MutableReference ? 2 : 1;
	if (Count == 0)
	{
		return 0;
	}
	if (Params.Transfer == EForeachSizeTransfer::Return)
	{
		return Contribution;
	}
	if (Params.Transfer == EForeachSizeTransfer::BreakFirst)
	{
		return 0;
	}
	if (Params.Transfer == EForeachSizeTransfer::BreakMiddle)
	{
		return Contribution;
	}
	if (Params.Transfer == EForeachSizeTransfer::BreakLast
		|| Params.Transfer == EForeachSizeTransfer::ContinueFirst)
	{
		return (Count - 1) * Contribution;
	}
	if (Params.Transfer == EForeachSizeTransfer::ContinueMiddle)
	{
		return (Count == 1 ? 1 : Count - 1) * Contribution;
	}
	return Count * Contribution;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FSizeVariableCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-FE-SIZE-VARIABLE-%s-%s-%s-%s"),
		SizeToken(Cell.Size),
		ElementToken(Cell.Element),
		VariableToken(Cell.Variable),
		TransferToken(Cell.Transfer));
}

FString MakeEntryName(const FSizeVariableCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FForeachSizeVariableParams ParamsFromCell(const FSizeVariableCell& Cell)
{
	FForeachSizeVariableParams Params;
	Params.Size = Cell.Size;
	Params.Element = Cell.Element;
	Params.Variable = Cell.Variable;
	Params.Transfer = Cell.Transfer;
	return Params;
}

FString ValueElementTypeName(const FString& FunctionName)
{
	return FString(TEXT("FForeachValueElement_")) + FunctionName;
}

FString IncompatibleTypeName(const FString& FunctionName)
{
	return FString(TEXT("FForeachIncompatibleElement_")) + FunctionName;
}

FString VariableDeclaration(const FForeachSizeVariableParams& Params, const FString& FunctionName)
{
	if (Params.Variable == EForeachVariable::Auto)
	{
		return TEXT("auto Value");
	}
	if (Params.Variable == EForeachVariable::Incompatible)
	{
		return IncompatibleTypeName(FunctionName) + TEXT(" Value");
	}
	if (Params.Element == EForeachElement::ValueObject)
	{
		const FString ElementType = ValueElementTypeName(FunctionName);
		if (Params.Variable == EForeachVariable::MutableReference)
		{
			return ElementType + TEXT("& Value");
		}
		if (Params.Variable == EForeachVariable::ConstReference)
		{
			return FString(TEXT("const ")) + ElementType + TEXT("& Value");
		}
		return ElementType + TEXT(" Value");
	}
	if (Params.Element == EForeachElement::ReferenceObject)
	{
		if (Params.Variable == EForeachVariable::MutableReference)
		{
			return TEXT("FNativeCaseReference& Value");
		}
		if (Params.Variable == EForeachVariable::ConstReference)
		{
			return TEXT("const FNativeCaseReference@ Value");
		}
		return TEXT("FNativeCaseReference@ Value");
	}
	if (Params.Variable == EForeachVariable::MutableReference)
	{
		return TEXT("int& Value");
	}
	if (Params.Variable == EForeachVariable::ConstReference)
	{
		return TEXT("const int& Value");
	}
	return TEXT("int Value");
}

FString RangeTypeName(const FString& FunctionName)
{
	return FString(TEXT("FForeachIterationRange_")) + FunctionName;
}

void AppendOwnedTypes(FString& Source, const FForeachSizeVariableParams& Params, const FString& FunctionName)
{
	if (Params.Element == EForeachElement::ValueObject)
	{
		AppendLine(Source, FString::Printf(TEXT("struct %s"), *ValueElementTypeName(FunctionName)));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 0;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	if (Params.Variable == EForeachVariable::Incompatible)
	{
		AppendLine(Source, FString::Printf(TEXT("struct %s"), *IncompatibleTypeName(FunctionName)));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 0;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}

	const FString TypeName = RangeTypeName(FunctionName);
	AppendLine(Source, FString::Printf(TEXT("struct %s"), *TypeName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseRange Native;"));
	if (Params.Element == EForeachElement::ValueObject)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Stored;"), *ValueElementTypeName(FunctionName)));
	}
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint opForBegin()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn Native.opForBegin();"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tbool opForEnd(const int Iterator)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn Iterator < 0;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tvoid opForNext(int& inout Iterator)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tNative.opForNext(Iterator);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	if (Params.Element == EForeachElement::ValueObject)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s& opForValue(const int Iterator)"), *ValueElementTypeName(FunctionName)));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tStored.Value = Native.opForValue(Iterator);"));
		AppendLine(Source, TEXT("\t\treturn Stored;"));
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Params.Element == EForeachElement::ReferenceObject)
	{
		AppendLine(Source, TEXT("\tFNativeCaseReference@ opForValue(const int Iterator)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn CreateNativeCaseReference(Native.opForValue(Iterator));"));
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Params.Element == EForeachElement::ConstElement)
	{
		AppendLine(Source, TEXT("\tconst int& opForValue(const int Iterator)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn Native.opForValue(Iterator);"));
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

void AppendTransfer(FString& Source, const EForeachSizeTransfer Transfer)
{
	if (Transfer == EForeachSizeTransfer::BreakFirst)
	{
		AppendLine(Source, TEXT("\t\tif (CurrentIteration == 0)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, TEXT("\t\t\tbreak;"));
		AppendLine(Source, TEXT("\t\t}"));
	}
	else if (Transfer == EForeachSizeTransfer::BreakMiddle)
	{
		AppendLine(Source, TEXT("\t\tif (CurrentIteration == 1)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, TEXT("\t\t\tbreak;"));
		AppendLine(Source, TEXT("\t\t}"));
	}
	else if (Transfer == EForeachSizeTransfer::BreakLast)
	{
		AppendLine(Source, TEXT("\t\tif (CurrentIteration + 1 >= Limit)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, TEXT("\t\t\tbreak;"));
		AppendLine(Source, TEXT("\t\t}"));
	}
	else if (Transfer == EForeachSizeTransfer::ContinueFirst)
	{
		AppendLine(Source, TEXT("\t\tif (CurrentIteration == 0)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, TEXT("\t\t\tcontinue;"));
		AppendLine(Source, TEXT("\t\t}"));
	}
	else if (Transfer == EForeachSizeTransfer::ContinueMiddle)
	{
		AppendLine(Source, TEXT("\t\tif (CurrentIteration == 1)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, TEXT("\t\t\tcontinue;"));
		AppendLine(Source, TEXT("\t\t}"));
	}
	else if (Transfer == EForeachSizeTransfer::Return)
	{
		AppendLine(Source, TEXT("\t\treturn Trace + Contribution;"));
	}
	else if (Transfer == EForeachSizeTransfer::Exception)
	{
		AppendLine(Source, TEXT("\t\tint Zero = 0;"));
		AppendLine(Source, TEXT("\t\tTrace += 1 / Zero;"));
	}
}

void AppendEntry(FString& Source, const FForeachSizeVariableParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Range;"), *RangeTypeName(FunctionName)));
	AppendLine(Source, FString::Printf(TEXT("\tRange.Native.Count = %d;"), CountFor(Params.Size)));
	AppendLine(Source, TEXT("\tint Limit = Range.Native.Count;"));
	AppendLine(Source, TEXT("\tint Iteration = 0;"));
	AppendLine(Source, TEXT("\tint Trace = 0;"));
	AppendLine(Source, TEXT("\tforeach (") + VariableDeclaration(Params, FunctionName) + TEXT(" : Range)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tint CurrentIteration = Iteration;"));
	AppendLine(Source, TEXT("\t\t++Iteration;"));
	if (Params.Variable == EForeachVariable::MutableReference)
	{
		if (Params.Element == EForeachElement::ValueObject
			|| Params.Element == EForeachElement::ReferenceObject)
		{
			AppendLine(Source, TEXT("\t\t++Value.Value;"));
		}
		else
		{
			AppendLine(Source, TEXT("\t\t++Value;"));
		}
	}
	if (Params.Element == EForeachElement::Primitive || Params.Element == EForeachElement::ConstElement)
	{
		AppendLine(Source, TEXT("\t\tint ElementBias = Value;"));
	}
	else
	{
		AppendLine(Source, TEXT("\t\tint ElementBias = Value.Value;"));
	}
	AppendLine(Source, TEXT("\t\tint Contribution = ElementBias + 1;"));
	AppendTransfer(Source, Params.Transfer);
	AppendLine(Source, TEXT("\t\tTrace += Contribution;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("\treturn Trace;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FForeachSizeVariableParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOwnedTypes(Source, Params, FunctionName);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

TArray<FSizeVariableCell> AllCells()
{
	TArray<FSizeVariableCell> Cells;
	Cells.SetNum(TotalCells);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		FSizeVariableCell& Cell = Cells[Index];
		Cell.Size = static_cast<EForeachSize>(Index / (ElementAxisCount * VariableAxisCount * TransferAxisCount));
		const int32 ElementRemainder = Index % (ElementAxisCount * VariableAxisCount * TransferAxisCount);
		Cell.Element = static_cast<EForeachElement>(ElementRemainder / (VariableAxisCount * TransferAxisCount));
		const int32 VariableRemainder = ElementRemainder % (VariableAxisCount * TransferAxisCount);
		Cell.Variable = static_cast<EForeachVariable>(VariableRemainder / TransferAxisCount);
		Cell.Transfer = static_cast<EForeachSizeTransfer>(VariableRemainder % TransferAxisCount);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FSizeVariableCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FSizeVariableCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FGeneratedCaseInfo MakeCaseInfo(const FSizeVariableCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	const FForeachSizeVariableParams Params = ParamsFromCell(Cell);
	if (IsReject(Params))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExecutionNotes = HostNotes;
	if (IsFault(Params))
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.ExpectedException = TEXT("Divide by zero");
	}
	else
	{
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.ExpectedReturn = ExpectedFromParams(Params);
	}
	return Info;
}
}

FStringView FForeachSizeVariableGenerator::GetProductId() const
{
	return TEXT("LANG-FE-SIZE-VARIABLE");
}

TArray<FGeneratedCaseInfo> FForeachSizeVariableGenerator::ListCases() const
{
	using namespace ForeachSizeVariableGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FSizeVariableCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("ForeachSizeVariable PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FForeachSizeVariableGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ForeachSizeVariableGeneratorDetail;
	FSizeVariableCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FForeachSizeVariableGenerator::BuildDumpSource() const
{
	using namespace ForeachSizeVariableGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FE-SIZE-VARIABLE"));
	AppendLine(Source, TEXT("// Cases: 640 total, 319 normal, 288 reject, 33 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));

	bool bNeedBlank = true;
	for (const FSizeVariableCell& Cell : AllCells())
	{
		const FForeachSizeVariableParams Params = ParamsFromCell(Cell);
		if (IsReject(Params))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (bNeedBlank)
		{
			AppendLine(Source);
		}
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		if (IsFault(Params))
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
		AppendOwnedTypes(Source, Params, MakeEntryName(Cell));
		AppendEntry(Source, Params, MakeEntryName(Cell));
		bNeedBlank = true;
	}

	for (const FSizeVariableCell& Cell : AllCells())
	{
		const FForeachSizeVariableParams Params = ParamsFromCell(Cell);
		if (!IsReject(Params))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, TEXT("// Reject module"));
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, TEXT("// EntryDeclaration:"));
		AppendLine(Source, TEXT("// Outcome: CompileReject"));
		AppendLine(Source, TEXT("// Execution: SourceOnly"));
		AppendLine(Source);
		AppendOwnedTypes(Source, Params, MakeEntryName(Cell));
		AppendEntry(Source, Params, MakeEntryName(Cell));
	}
	return Source;
}

FString FForeachSizeVariableGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ForeachSizeVariableGeneratorDetail;
	FString Source;
	int32 Emitted = 0;
	for (const FSizeVariableCell& Cell : AllCells())
	{
		const FForeachSizeVariableParams Params = ParamsFromCell(Cell);
		if (IsReject(Params))
		{
			continue;
		}
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		AppendOwnedTypes(Source, Params, MakeEntryName(Cell));
		AppendEntry(Source, Params, MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

FString FForeachSizeVariableGenerator::BuildForeachIterationSource(const FForeachSizeVariableParams& Params) const
{
	using namespace ForeachSizeVariableGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params))
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

int32 FForeachSizeVariableGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ForeachSizeVariableGeneratorDetail;
	FSizeVariableCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromParams(ParamsFromCell(Cell));
}

FString FForeachSizeVariableGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace ForeachSizeVariableGeneratorDetail;
	FSizeVariableCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(ParamsFromCell(Cell)))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FForeachSizeVariableGenerator::ListRejectCaseIds() const
{
	using namespace ForeachSizeVariableGeneratorDetail;
	TArray<FString> Ids;
	for (const FSizeVariableCell& Cell : AllCells())
	{
		if (IsReject(ParamsFromCell(Cell)))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
