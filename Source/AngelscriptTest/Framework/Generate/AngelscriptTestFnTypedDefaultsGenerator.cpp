#include "Framework/Generate/AngelscriptTestFnTypedDefaultsGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace FnTypedDefaultsGeneratorDetail
{
struct FDefaultsCell
{
	EFnTypedDefaultsArity Arity = EFnTypedDefaultsArity::One;
	EFnTypedDefaultsTypePattern Type = EFnTypedDefaultsTypePattern::HomogeneousInt;
	EFnTypedDefaultsTarget Target = EFnTypedDefaultsTarget::Global;
	int32 OmittedCount = 0;
};

const TCHAR* ArityToken(const EFnTypedDefaultsArity Arity)
{
	switch (Arity)
	{
	case EFnTypedDefaultsArity::One:
		return TEXT("ONE");
	case EFnTypedDefaultsArity::Two:
		return TEXT("TWO");
	case EFnTypedDefaultsArity::Three:
		return TEXT("THREE");
	case EFnTypedDefaultsArity::Four:
		return TEXT("FOUR");
	default:
		return nullptr;
	}
}

const TCHAR* TypeToken(const EFnTypedDefaultsTypePattern Type)
{
	switch (Type)
	{
	case EFnTypedDefaultsTypePattern::HomogeneousInt:
		return TEXT("HOMOGENEOUS_INT");
	case EFnTypedDefaultsTypePattern::HomogeneousBool:
		return TEXT("HOMOGENEOUS_BOOL");
	case EFnTypedDefaultsTypePattern::AlternatingIntBool:
		return TEXT("ALTERNATING_INT_BOOL");
	case EFnTypedDefaultsTypePattern::AlternatingBoolInt:
		return TEXT("ALTERNATING_BOOL_INT");
	default:
		return nullptr;
	}
}

const TCHAR* TargetToken(const EFnTypedDefaultsTarget Target)
{
	switch (Target)
	{
	case EFnTypedDefaultsTarget::Global:
		return TEXT("GLOBAL");
	case EFnTypedDefaultsTarget::NamespaceGlobal:
		return TEXT("NAMESPACE_GLOBAL");
	case EFnTypedDefaultsTarget::InstanceMethod:
		return TEXT("INSTANCE_METHOD");
	default:
		return nullptr;
	}
}

int32 ArityCount(const EFnTypedDefaultsArity Arity)
{
	return static_cast<int32>(Arity) + 1;
}

bool IsValidParams(const FFnTypedDefaultsParams& Params)
{
	const TCHAR* const Arity = ArityToken(Params.Arity);
	if (Arity == nullptr || TypeToken(Params.Type) == nullptr || TargetToken(Params.Target) == nullptr)
	{
		return false;
	}
	return Params.OmittedCount >= 0 && Params.OmittedCount <= ArityCount(Params.Arity);
}

bool IsBoolType(const EFnTypedDefaultsTypePattern Type, const int32 Index)
{
	if (Type == EFnTypedDefaultsTypePattern::HomogeneousBool)
	{
		return true;
	}
	if (Type == EFnTypedDefaultsTypePattern::AlternatingIntBool)
	{
		return (Index % 2) == 1;
	}
	if (Type == EFnTypedDefaultsTypePattern::AlternatingBoolInt)
	{
		return (Index % 2) == 0;
	}
	return false;
}

const TCHAR* DefaultExpression(const EFnTypedDefaultsTypePattern Type, const int32 Index)
{
	if (IsBoolType(Type, Index))
	{
		static const TCHAR* const BoolDefaults[] = {TEXT("true"), TEXT("false"), TEXT("1 < 2"), TEXT("2 == 3")};
		return BoolDefaults[Index];
	}
	static const TCHAR* const IntDefaults[] = {TEXT("100"), TEXT("10 + 2"), TEXT("0x10"), TEXT("-7")};
	return IntDefaults[Index];
}

int32 DefaultValue(const EFnTypedDefaultsTypePattern Type, const int32 Index)
{
	if (IsBoolType(Type, Index))
	{
		return Index == 0 || Index == 2 ? 1 : 0;
	}
	static const int32 IntDefaults[] = {100, 12, 16, -7};
	return IntDefaults[Index];
}

const TCHAR* ExplicitArgument(const EFnTypedDefaultsTypePattern Type, const int32 Index)
{
	if (IsBoolType(Type, Index))
	{
		static const TCHAR* const BoolArguments[] = {TEXT("false"), TEXT("true"), TEXT("false"), TEXT("true")};
		return BoolArguments[Index];
	}
	static const TCHAR* const IntArguments[] = {TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4")};
	return IntArguments[Index];
}

int32 ExplicitValue(const EFnTypedDefaultsTypePattern Type, const int32 Index)
{
	if (IsBoolType(Type, Index))
	{
		return Index % 2 == 1 ? 1 : 0;
	}
	return Index + 1;
}

int32 ExpectedFromParams(const FFnTypedDefaultsParams& Params)
{
	int32 Result = 0;
	const int32 Count = ArityCount(Params.Arity);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Result += Index >= Count - Params.OmittedCount
			? DefaultValue(Params.Type, Index)
			: ExplicitValue(Params.Type, Index);
	}
	return Result;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FDefaultsCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-FN-TYPED-DEFAULTS-%s-%s-%s-OMIT%d"),
		ArityToken(Cell.Arity),
		TypeToken(Cell.Type),
		TargetToken(Cell.Target),
		Cell.OmittedCount);
}

FString MakeEntryName(const FDefaultsCell& Cell)
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
	return Stem.IsEmpty() ? FString(TEXT("NDefaults")) : TEXT("N") + Stem;
}

FString OwnerName(const FString& FunctionName)
{
	const FString Stem = HelperStem(FunctionName);
	return Stem.IsEmpty() ? FString(TEXT("FOwner")) : TEXT("FOwner") + Stem;
}

FFnTypedDefaultsParams ParamsFromCell(const FDefaultsCell& Cell)
{
	FFnTypedDefaultsParams Params;
	Params.Arity = Cell.Arity;
	Params.Type = Cell.Type;
	Params.Target = Cell.Target;
	Params.OmittedCount = Cell.OmittedCount;
	return Params;
}

TArray<FDefaultsCell> AllCells()
{
	TArray<FDefaultsCell> Cells;
	Cells.Reserve(168);
	for (int32 ArityIndex = 0; ArityIndex < 4; ++ArityIndex)
	{
		const int32 Count = ArityIndex + 1;
		for (int32 TypeIndex = 0; TypeIndex < 4; ++TypeIndex)
		{
			for (int32 TargetIndex = 0; TargetIndex < 3; ++TargetIndex)
			{
				for (int32 Omitted = 0; Omitted <= Count; ++Omitted)
				{
					FDefaultsCell Cell;
					Cell.Arity = static_cast<EFnTypedDefaultsArity>(ArityIndex);
					Cell.Type = static_cast<EFnTypedDefaultsTypePattern>(TypeIndex);
					Cell.Target = static_cast<EFnTypedDefaultsTarget>(TargetIndex);
					Cell.OmittedCount = Omitted;
					Cells.Add(Cell);
				}
			}
		}
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FDefaultsCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FDefaultsCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FString MakeParameters(const FFnTypedDefaultsParams& Params)
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
			TEXT("%s P%d = %s"),
			IsBoolType(Params.Type, Index) ? TEXT("bool") : TEXT("int"),
			Index,
			DefaultExpression(Params.Type, Index));
	}
	return Result;
}

FString MakeArguments(const FFnTypedDefaultsParams& Params)
{
	FString Result;
	const int32 ExplicitCount = ArityCount(Params.Arity) - Params.OmittedCount;
	for (int32 Index = 0; Index < ExplicitCount; ++Index)
	{
		if (Index > 0)
		{
			Result += TEXT(", ");
		}
		Result += ExplicitArgument(Params.Type, Index);
	}
	return Result;
}

FString ResultExpression(const FFnTypedDefaultsParams& Params)
{
	FString Result;
	const int32 Count = ArityCount(Params.Arity);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Index > 0)
		{
			Result += TEXT(" + ");
		}
		Result += IsBoolType(Params.Type, Index)
			? FString::Printf(TEXT("(P%d ? 1 : 0)"), Index)
			: FString::Printf(TEXT("P%d"), Index);
	}
	return Result;
}

void AppendEntry(FString& Source, const FFnTypedDefaultsParams& Params, const FString& FunctionName)
{
	const FString Probe = ProbeName(FunctionName);
	const FString Parameters = MakeParameters(Params);
	const FString Arguments = MakeArguments(Params);
	const FString Result = ResultExpression(Params);
	if (Params.Target == EFnTypedDefaultsTarget::Global)
	{
		AppendLine(Source, FString::Printf(TEXT("int %s(%s)"), *Probe, *Parameters));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *Result));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s(%s);"), *Probe, *Arguments));
		AppendLine(Source, TEXT("}"));
	}
	else if (Params.Target == EFnTypedDefaultsTarget::NamespaceGlobal)
	{
		const FString Namespace = NamespaceName(FunctionName);
		AppendLine(Source, FString::Printf(TEXT("namespace %s"), *Namespace));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tint %s(%s)"), *Probe, *Parameters));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\treturn %s;"), *Result));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s::%s(%s);"), *Namespace, *Probe, *Arguments));
		AppendLine(Source, TEXT("}"));
	}
	else
	{
		const FString Owner = OwnerName(FunctionName);
		AppendLine(Source, FString::Printf(TEXT("struct %s"), *Owner));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tint %s(%s)"), *Probe, *Parameters));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\treturn %s;"), *Result));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\t%s Owner;"), *Owner));
		AppendLine(Source, FString::Printf(TEXT("\treturn Owner.%s(%s);"), *Probe, *Arguments));
		AppendLine(Source, TEXT("}"));
	}
}

FString BuildOwnedSource(const FFnTypedDefaultsParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FDefaultsCell& Cell)
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

FStringView FFnTypedDefaultsGenerator::GetProductId() const
{
	return TEXT("LANG-FN-TYPED-DEFAULTS");
}

TArray<FGeneratedCaseInfo> FFnTypedDefaultsGenerator::ListCases() const
{
	using namespace FnTypedDefaultsGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FDefaultsCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("FnTypedDefaults PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FFnTypedDefaultsGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace FnTypedDefaultsGeneratorDetail;
	FDefaultsCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FFnTypedDefaultsGenerator::BuildDumpSource() const
{
	using namespace FnTypedDefaultsGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FN-TYPED-DEFAULTS"));
	AppendLine(Source, TEXT("// Cases: 168 total, 168 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	const TArray<FDefaultsCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

FString FFnTypedDefaultsGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace FnTypedDefaultsGeneratorDetail;
	const TArray<FDefaultsCell> Cells = AllCells();
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

FString FFnTypedDefaultsGenerator::BuildSource(const FFnTypedDefaultsParams& Params) const
{
	using namespace FnTypedDefaultsGeneratorDetail;
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

int32 FFnTypedDefaultsGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace FnTypedDefaultsGeneratorDetail;
	FDefaultsCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromParams(ParamsFromCell(Cell));
}
}
