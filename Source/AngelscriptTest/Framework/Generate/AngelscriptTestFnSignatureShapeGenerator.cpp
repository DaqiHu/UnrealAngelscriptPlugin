#include "Framework/Generate/AngelscriptTestFnSignatureShapeGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace FnSignatureShapeGeneratorDetail
{
struct FShapeCell
{
	EFnSignatureShapeTarget Target = EFnSignatureShapeTarget::Global;
	EFnSignatureShapeDirectionPattern Direction = EFnSignatureShapeDirectionPattern::AllValue;
	EFnSignatureShapeTypePattern Type = EFnSignatureShapeTypePattern::HomogeneousInt;
	EFnSignatureShapeArity Arity = EFnSignatureShapeArity::One;
};

const TCHAR* TargetToken(const EFnSignatureShapeTarget Target)
{
	switch (Target)
	{
	case EFnSignatureShapeTarget::Global:
		return TEXT("GLOBAL");
	case EFnSignatureShapeTarget::NamespaceGlobal:
		return TEXT("NAMESPACE_GLOBAL");
	case EFnSignatureShapeTarget::InstanceMethod:
		return TEXT("INSTANCE_METHOD");
	default:
		return nullptr;
	}
}

const TCHAR* DirectionToken(const EFnSignatureShapeDirectionPattern Direction)
{
	switch (Direction)
	{
	case EFnSignatureShapeDirectionPattern::AllValue:
		return TEXT("ALL_VALUE");
	case EFnSignatureShapeDirectionPattern::AllIn:
		return TEXT("ALL_IN");
	case EFnSignatureShapeDirectionPattern::AllOut:
		return TEXT("ALL_OUT");
	case EFnSignatureShapeDirectionPattern::AlternatingInoutOut:
		return TEXT("ALTERNATING_INOUT_OUT");
	default:
		return nullptr;
	}
}

const TCHAR* TypeToken(const EFnSignatureShapeTypePattern Type)
{
	switch (Type)
	{
	case EFnSignatureShapeTypePattern::HomogeneousInt:
		return TEXT("HOMOGENEOUS_INT");
	case EFnSignatureShapeTypePattern::HomogeneousFloat:
		return TEXT("HOMOGENEOUS_FLOAT");
	case EFnSignatureShapeTypePattern::AlternatingIntFloat:
		return TEXT("ALTERNATING_INT_FLOAT");
	case EFnSignatureShapeTypePattern::AlternatingFloatInt:
		return TEXT("ALTERNATING_FLOAT_INT");
	default:
		return nullptr;
	}
}

const TCHAR* ArityToken(const EFnSignatureShapeArity Arity)
{
	switch (Arity)
	{
	case EFnSignatureShapeArity::One:
		return TEXT("ONE");
	case EFnSignatureShapeArity::Two:
		return TEXT("TWO");
	case EFnSignatureShapeArity::Three:
		return TEXT("THREE");
	case EFnSignatureShapeArity::Four:
		return TEXT("FOUR");
	default:
		return nullptr;
	}
}

int32 ArityCount(const EFnSignatureShapeArity Arity)
{
	return static_cast<int32>(Arity) + 1;
}

bool IsValidParams(const FFnSignatureShapeParams& Params)
{
	return TargetToken(Params.Target) != nullptr
		&& DirectionToken(Params.Direction) != nullptr
		&& TypeToken(Params.Type) != nullptr
		&& ArityToken(Params.Arity) != nullptr;
}

bool IsFloatParameter(const EFnSignatureShapeTypePattern Type, const int32 Index)
{
	if (Type == EFnSignatureShapeTypePattern::HomogeneousFloat)
	{
		return true;
	}
	if (Type == EFnSignatureShapeTypePattern::AlternatingIntFloat)
	{
		return (Index % 2) == 1;
	}
	if (Type == EFnSignatureShapeTypePattern::AlternatingFloatInt)
	{
		return (Index % 2) == 0;
	}
	return false;
}

bool IsWrittenParameter(const EFnSignatureShapeDirectionPattern Direction)
{
	return Direction == EFnSignatureShapeDirectionPattern::AllOut
		|| Direction == EFnSignatureShapeDirectionPattern::AlternatingInoutOut;
}

bool IsReadParameter(const EFnSignatureShapeDirectionPattern Direction, const int32 Index)
{
	if (Direction == EFnSignatureShapeDirectionPattern::AllValue
		|| Direction == EFnSignatureShapeDirectionPattern::AllIn)
	{
		return true;
	}
	return Direction == EFnSignatureShapeDirectionPattern::AlternatingInoutOut && (Index % 2) == 0;
}

FString DirectionSuffix(const EFnSignatureShapeDirectionPattern Direction, const int32 Index)
{
	if (Direction == EFnSignatureShapeDirectionPattern::AllValue)
	{
		return FString();
	}
	if (Direction == EFnSignatureShapeDirectionPattern::AllIn)
	{
		return TEXT("& in");
	}
	if (Direction == EFnSignatureShapeDirectionPattern::AllOut)
	{
		return TEXT("& out");
	}
	return (Index % 2) == 0 ? TEXT("& inout") : TEXT("& out");
}

int32 ExpectedFromParams(const FFnSignatureShapeParams& Params)
{
	int32 Result = 0;
	const int32 Count = ArityCount(Params.Arity);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!IsWrittenParameter(Params.Direction))
		{
			Result += Index + 3;
		}
		else if (IsReadParameter(Params.Direction, Index))
		{
			Result += Index + 8;
		}
		else
		{
			Result += 100 + Index;
		}
	}
	return (Result * 1000) + 1;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FShapeCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-FN-SIGNATURE-SHAPE-%s-%s-%s-%s"),
		TargetToken(Cell.Target),
		DirectionToken(Cell.Direction),
		TypeToken(Cell.Type),
		ArityToken(Cell.Arity));
}

FString MakeEntryName(const FShapeCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FString HelperStem(const FString& FunctionName)
{
	if (FunctionName == TEXT("Entry") || FunctionName == TEXT("ProbeEntry"))
	{
		return FString();
	}
	if (FunctionName.StartsWith(TEXT("Entry")))
	{
		return FunctionName.Mid(5);
	}
	return FunctionName;
}

FString ProbeName(const FString& FunctionName)
{
	const FString Stem = HelperStem(FunctionName);
	return Stem.IsEmpty() ? FString(TEXT("Probe")) : TEXT("Probe") + Stem;
}

FString NamespaceName(const FString& FunctionName)
{
	const FString Stem = HelperStem(FunctionName);
	return Stem.IsEmpty() ? FString(TEXT("NShape")) : TEXT("N") + Stem;
}

FString OwnerName(const FString& FunctionName)
{
	const FString Stem = HelperStem(FunctionName);
	return Stem.IsEmpty() ? FString(TEXT("FOwner")) : TEXT("FOwner") + Stem;
}

FFnSignatureShapeParams ParamsFromCell(const FShapeCell& Cell)
{
	FFnSignatureShapeParams Params;
	Params.Target = Cell.Target;
	Params.Direction = Cell.Direction;
	Params.Type = Cell.Type;
	Params.Arity = Cell.Arity;
	return Params;
}

TArray<FShapeCell> AllCells()
{
	TArray<FShapeCell> Cells;
	Cells.SetNum(192);
	for (int32 Index = 0; Index < 192; ++Index)
	{
		FShapeCell& Cell = Cells[Index];
		Cell.Target = static_cast<EFnSignatureShapeTarget>(Index / 64);
		Cell.Direction = static_cast<EFnSignatureShapeDirectionPattern>((Index / 16) % 4);
		Cell.Type = static_cast<EFnSignatureShapeTypePattern>((Index / 4) % 4);
		Cell.Arity = static_cast<EFnSignatureShapeArity>(Index % 4);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FShapeCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FShapeCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FString MakeParameters(const FFnSignatureShapeParams& Params)
{
	FString Result;
	const int32 Count = ArityCount(Params.Arity);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Index > 0)
		{
			Result += TEXT(", ");
		}
		Result += FString::Printf(
			TEXT("%s%s P%d"),
			IsFloatParameter(Params.Type, Index) ? TEXT("float") : TEXT("int"),
			*DirectionSuffix(Params.Direction, Index),
			Index);
	}
	return Result;
}

FString ReturnExpression(const FFnSignatureShapeParams& Params)
{
	FString Result;
	const int32 Count = ArityCount(Params.Arity);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Index > 0)
		{
			Result += TEXT(" + ");
		}
		Result += IsFloatParameter(Params.Type, Index)
			? FString::Printf(TEXT("int(P%d)"), Index)
			: FString::Printf(TEXT("P%d"), Index);
	}
	return Result;
}

FString WrittenValue(const FFnSignatureShapeParams& Params, const int32 Index)
{
	if (!IsReadParameter(Params.Direction, Index))
	{
		return IsFloatParameter(Params.Type, Index)
			? FString::Printf(TEXT("%d.0f"), 100 + Index)
			: FString::FromInt(100 + Index);
	}
	return IsFloatParameter(Params.Type, Index)
		? FString::Printf(TEXT("P%d + 1.0f"), Index)
		: FString::Printf(TEXT("P%d + 1"), Index);
}

FString ExpectedCallerValue(const FFnSignatureShapeParams& Params, const int32 Index)
{
	if (!IsReadParameter(Params.Direction, Index))
	{
		return IsFloatParameter(Params.Type, Index)
			? FString::Printf(TEXT("%d.0f"), 100 + Index)
			: FString::FromInt(100 + Index);
	}
	return IsFloatParameter(Params.Type, Index)
		? FString::Printf(TEXT("%d.5f"), Index + 8)
		: FString::FromInt(Index + 8);
}

void AppendProbeBody(FString& Source, const FFnSignatureShapeParams& Params, const TCHAR* Indent)
{
	const int32 Count = ArityCount(Params.Arity);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (IsWrittenParameter(Params.Direction))
		{
			AppendLine(Source, FString::Printf(TEXT("%sP%d = %s;"), Indent, Index, *WrittenValue(Params, Index)));
		}
	}
	AppendLine(Source, FString::Printf(TEXT("%sreturn %s;"), Indent, *ReturnExpression(Params)));
}

void AppendEntry(FString& Source, const FFnSignatureShapeParams& Params, const FString& FunctionName)
{
	const FString Probe = ProbeName(FunctionName);
	const FString Parameters = MakeParameters(Params);
	const int32 Count = ArityCount(Params.Arity);
	if (Params.Target == EFnSignatureShapeTarget::Global)
	{
		AppendLine(Source, FString::Printf(TEXT("int %s(%s)"), *Probe, *Parameters));
		AppendLine(Source, TEXT("{"));
		AppendProbeBody(Source, Params, TEXT("\t"));
		AppendLine(Source, TEXT("}"));
	}
	else if (Params.Target == EFnSignatureShapeTarget::NamespaceGlobal)
	{
		const FString Namespace = NamespaceName(FunctionName);
		AppendLine(Source, FString::Printf(TEXT("namespace %s"), *Namespace));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tint %s(%s)"), *Probe, *Parameters));
		AppendLine(Source, TEXT("\t{"));
		AppendProbeBody(Source, Params, TEXT("\t\t"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
	}
	else
	{
		const FString Owner = OwnerName(FunctionName);
		AppendLine(Source, FString::Printf(TEXT("struct %s"), *Owner));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tint %s(%s)"), *Probe, *Parameters));
		AppendLine(Source, TEXT("\t{"));
		AppendProbeBody(Source, Params, TEXT("\t\t"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
	}
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Params.Direction == EFnSignatureShapeDirectionPattern::AllValue)
		{
			continue;
		}
		const int32 Initial = Params.Direction == EFnSignatureShapeDirectionPattern::AllIn ? Index + 3 : Index + 7;
		const FString Value = IsFloatParameter(Params.Type, Index)
			? FString::Printf(TEXT("%d.5f"), Initial)
			: FString::FromInt(Initial);
		AppendLine(Source, FString::Printf(
			TEXT("\t%s A%d = %s;"),
			IsFloatParameter(Params.Type, Index) ? TEXT("float") : TEXT("int"),
			Index,
			*Value));
	}
	FString Arguments;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Index > 0)
		{
			Arguments += TEXT(", ");
		}
		if (Params.Direction == EFnSignatureShapeDirectionPattern::AllValue)
		{
			Arguments += IsFloatParameter(Params.Type, Index)
				? FString::Printf(TEXT("%d.5f"), Index + 3)
				: FString::FromInt(Index + 3);
		}
		else
		{
			Arguments += FString::Printf(TEXT("A%d"), Index);
		}
	}
	FString Call;
	if (Params.Target == EFnSignatureShapeTarget::NamespaceGlobal)
	{
		Call = NamespaceName(FunctionName) + TEXT("::") + Probe;
	}
	else if (Params.Target == EFnSignatureShapeTarget::InstanceMethod)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Owner;"), *OwnerName(FunctionName)));
		Call = TEXT("Owner.") + Probe;
	}
	else
	{
		Call = Probe;
	}
	AppendLine(Source, FString::Printf(TEXT("\tint Result = %s(%s);"), *Call, *Arguments));
	FString Checks;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (!IsWrittenParameter(Params.Direction))
		{
			continue;
		}
		if (!Checks.IsEmpty())
		{
			Checks += TEXT(" && ");
		}
		Checks += FString::Printf(TEXT("A%d == %s"), Index, *ExpectedCallerValue(Params, Index));
	}
	if (Checks.IsEmpty())
	{
		Checks = TEXT("true");
	}
	AppendLine(Source, FString::Printf(TEXT("\treturn (Result * 1000) + (%s ? 1 : 0);"), *Checks));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FFnSignatureShapeParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FShapeCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = ExpectedFromParams(ParamsFromCell(Cell));
	return Info;
}
}

FStringView FFnSignatureShapeGenerator::GetProductId() const
{
	return TEXT("LANG-FN-SIGNATURE-SHAPE");
}

TArray<FGeneratedCaseInfo> FFnSignatureShapeGenerator::ListCases() const
{
	using namespace FnSignatureShapeGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FShapeCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("FnSignatureShape PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FFnSignatureShapeGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace FnSignatureShapeGeneratorDetail;
	FShapeCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FFnSignatureShapeGenerator::BuildDumpSource() const
{
	using namespace FnSignatureShapeGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FN-SIGNATURE-SHAPE"));
	AppendLine(Source, TEXT("// Cases: 192 total, 192 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	bool bFirst = true;
	for (const FShapeCell& Cell : AllCells())
	{
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (bFirst)
		{
			AppendLine(Source);
			bFirst = false;
		}
		else
		{
			AppendLine(Source);
		}
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FFnSignatureShapeGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace FnSignatureShapeGeneratorDetail;
	const TArray<FShapeCell> Cells = AllCells();
	OutCaseCount = Cells.Num();
	FString Source;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (Index > 0)
		{
			AppendLine(Source);
		}
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

FString FFnSignatureShapeGenerator::BuildSource(const FFnSignatureShapeParams& Params) const
{
	using namespace FnSignatureShapeGeneratorDetail;
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

int32 FFnSignatureShapeGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace FnSignatureShapeGeneratorDetail;
	FShapeCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromParams(ParamsFromCell(Cell));
}
}
