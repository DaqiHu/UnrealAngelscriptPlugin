#include "Framework/Generate/AngelscriptTestFnMixinDirectDispatchGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace FnMixinDirectDispatchGeneratorDetail
{
struct FMixinDirectCell
{
	EFnMixinDirectDispatchNamespace Namespace = EFnMixinDirectDispatchNamespace::Global;
};

const TCHAR* NamespaceToken(const EFnMixinDirectDispatchNamespace Namespace)
{
	switch (Namespace)
	{
	case EFnMixinDirectDispatchNamespace::Global:
		return TEXT("GLOBAL");
	case EFnMixinDirectDispatchNamespace::Nested:
		return TEXT("NESTED");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FFnMixinDirectDispatchParams& Params)
{
	return NamespaceToken(Params.Namespace) != nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FMixinDirectCell& Cell)
{
	return FString::Printf(TEXT("LANG-FN-MIXIN-DIRECT-DISPATCH-%s"), NamespaceToken(Cell.Namespace));
}

FString MakeEntryName(const FMixinDirectCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FFnMixinDirectDispatchParams ParamsFromCell(const FMixinDirectCell& Cell)
{
	FFnMixinDirectDispatchParams Params;
	Params.Namespace = Cell.Namespace;
	return Params;
}

TArray<FMixinDirectCell> AllCells()
{
	TArray<FMixinDirectCell> Cells;
	Cells.SetNum(2);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		Cells[Index].Namespace = static_cast<EFnMixinDirectDispatchNamespace>(Index);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FMixinDirectCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FMixinDirectCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FString OwnerPrefix(const FString& FunctionName)
{
	return FunctionName == TEXT("Entry") ? FString() : FunctionName;
}

void AppendHelpers(FString& Source, const FFnMixinDirectDispatchParams& Params, const FString& FunctionName)
{
	const bool bNested = Params.Namespace == EFnMixinDirectDispatchNamespace::Nested;
	const FString Prefix = OwnerPrefix(FunctionName);
	if (bNested)
	{
		AppendLine(Source, FString::Printf(TEXT("namespace Tools%s"), *Prefix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tstruct Counter"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tint Value = 0;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\tmixin void AddToCounter(Counter& Self, int Delta)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tSelf.Value += Delta;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("struct Counter%s"), *Prefix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 0;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("mixin void AddToCounter%s(Counter%s& Self, int Delta)"), *Prefix, *Prefix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tSelf.Value += Delta;"));
		AppendLine(Source, TEXT("}"));
	}
}

void AppendEntry(FString& Source, const FFnMixinDirectDispatchParams& Params, const FString& FunctionName)
{
	const bool bNested = Params.Namespace == EFnMixinDirectDispatchNamespace::Nested;
	const FString Prefix = OwnerPrefix(FunctionName);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (bNested)
	{
		AppendLine(Source, FString::Printf(TEXT("\tTools%s::Counter Value;"), *Prefix));
		AppendLine(Source, TEXT("\tValue.AddToCounter(3);"));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\tCounter%s Value;"), *Prefix));
		AppendLine(Source, FString::Printf(TEXT("\tValue.AddToCounter%s(3);"), *Prefix));
	}
	AppendLine(Source, TEXT("\treturn Value.Value;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FFnMixinDirectDispatchParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendHelpers(Source, Params, FunctionName);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FMixinDirectCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = 3;
	return Info;
}
}

FStringView FFnMixinDirectDispatchGenerator::GetProductId() const
{
	return TEXT("LANG-FN-MIXIN-DIRECT-DISPATCH");
}

TArray<FGeneratedCaseInfo> FFnMixinDirectDispatchGenerator::ListCases() const
{
	using namespace FnMixinDirectDispatchGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FMixinDirectCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("FnMixinDirectDispatch PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FFnMixinDirectDispatchGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace FnMixinDirectDispatchGeneratorDetail;
	FMixinDirectCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FFnMixinDirectDispatchGenerator::BuildDumpSource() const
{
	using namespace FnMixinDirectDispatchGeneratorDetail;
	const TArray<FMixinDirectCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FN-MIXIN-DIRECT-DISPATCH"));
	AppendLine(Source, TEXT("// Cases: 2 total, 2 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FMixinDirectCell& Cell = Cells[Index];
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, TEXT("// ExpectedReturn: 3"));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendHelpers(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FFnMixinDirectDispatchGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace FnMixinDirectDispatchGeneratorDetail;
	const TArray<FMixinDirectCell> Cells = AllCells();
	FString Source;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (Index > 0)
		{
			AppendLine(Source);
		}
		AppendHelpers(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	OutCaseCount = Cells.Num();
	return Source;
}

FString FFnMixinDirectDispatchGenerator::BuildFnMixinDirectDispatchSource(const FFnMixinDirectDispatchParams& Params) const
{
	using namespace FnMixinDirectDispatchGeneratorDetail;
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

int32 FFnMixinDirectDispatchGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace FnMixinDirectDispatchGeneratorDetail;
	FMixinDirectCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return 3;
}
}
