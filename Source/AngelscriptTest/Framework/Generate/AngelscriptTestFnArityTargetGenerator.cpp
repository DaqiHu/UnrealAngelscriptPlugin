#include "Framework/Generate/AngelscriptTestFnArityTargetGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace FnArityTargetGeneratorDetail
{
struct FArityTargetCell
{
	EFnArityTargetArity Arity = EFnArityTargetArity::Zero;
	EFnArityTargetTarget Target = EFnArityTargetTarget::Global;
};

const TCHAR* ArityToken(const EFnArityTargetArity Arity)
{
	switch (Arity)
	{
	case EFnArityTargetArity::Zero:
		return TEXT("ZERO");
	case EFnArityTargetArity::One:
		return TEXT("ONE");
	case EFnArityTargetArity::Two:
		return TEXT("TWO");
	case EFnArityTargetArity::Three:
		return TEXT("THREE");
	case EFnArityTargetArity::Eight:
		return TEXT("EIGHT");
	case EFnArityTargetArity::CurrentBoundary:
		return TEXT("CURRENT_BOUNDARY");
	case EFnArityTargetArity::BoundaryPlusOne:
		return TEXT("BOUNDARY_PLUS_ONE");
	default:
		return nullptr;
	}
}

const TCHAR* TargetToken(const EFnArityTargetTarget Target)
{
	switch (Target)
	{
	case EFnArityTargetTarget::Global:
		return TEXT("GLOBAL");
	case EFnArityTargetTarget::NamespaceGlobal:
		return TEXT("NAMESPACE_GLOBAL");
	case EFnArityTargetTarget::InstanceMethod:
		return TEXT("INSTANCE_METHOD");
	default:
		return nullptr;
	}
}

int32 ArityCount(const EFnArityTargetArity Arity)
{
	switch (Arity)
	{
	case EFnArityTargetArity::Zero:
		return 0;
	case EFnArityTargetArity::One:
		return 1;
	case EFnArityTargetArity::Two:
		return 2;
	case EFnArityTargetArity::Three:
		return 3;
	case EFnArityTargetArity::Eight:
		return 8;
	case EFnArityTargetArity::CurrentBoundary:
		return 64;
	case EFnArityTargetArity::BoundaryPlusOne:
		return 65;
	default:
		return -1;
	}
}

bool IsValidParams(const FFnArityTargetParams& Params)
{
	return ArityToken(Params.Arity) != nullptr && TargetToken(Params.Target) != nullptr;
}

int32 ExpectedFromArity(const EFnArityTargetArity Arity)
{
	const int32 Count = ArityCount(Arity);
	return Count == 0 ? 42 : Count * (Count + 1) / 2;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FArityTargetCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-FN-ARITY-TARGET-%s-%s"),
		ArityToken(Cell.Arity),
		TargetToken(Cell.Target));
}

FString MakeEntryName(const FArityTargetCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FFnArityTargetParams ParamsFromCell(const FArityTargetCell& Cell)
{
	FFnArityTargetParams Params;
	Params.Arity = Cell.Arity;
	Params.Target = Cell.Target;
	return Params;
}

TArray<FArityTargetCell> AllCells()
{
	TArray<FArityTargetCell> Cells;
	Cells.SetNum(21);
	for (int32 Index = 0; Index < 21; ++Index)
	{
		FArityTargetCell& Cell = Cells[Index];
		Cell.Arity = static_cast<EFnArityTargetArity>(Index / 3);
		Cell.Target = static_cast<EFnArityTargetTarget>(Index % 3);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FArityTargetCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FArityTargetCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FString NameSuffix(const FString& FunctionName)
{
	return FunctionName == TEXT("Entry") ? FString() : FunctionName;
}

FString MakeParameters(const int32 Count)
{
	if (Count == 0)
	{
		return FString();
	}
	TArray<FString> Parameters;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Parameters.Add(FString::Printf(TEXT("int P%d"), Index));
	}
	return FString::Join(Parameters, TEXT(", "));
}

FString MakeArguments(const int32 Count)
{
	if (Count == 0)
	{
		return FString();
	}
	TArray<FString> Arguments;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Arguments.Add(FString::FromInt(Index + 1));
	}
	return FString::Join(Arguments, TEXT(", "));
}

FString MakeSumExpression(const int32 Count)
{
	if (Count == 0)
	{
		return TEXT("42");
	}
	TArray<FString> Terms;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Terms.Add(FString::Printf(TEXT("P%d"), Index));
	}
	return FString::Join(Terms, TEXT(" + "));
}

void AppendEntry(FString& Source, const FFnArityTargetParams& Params, const FString& FunctionName)
{
	const int32 Count = ArityCount(Params.Arity);
	const FString Parameters = MakeParameters(Count);
	const FString Arguments = MakeArguments(Count);
	const FString Sum = MakeSumExpression(Count);
	const FString Suffix = NameSuffix(FunctionName);
	const FString ProbeName = TEXT("Probe") + Suffix;
	if (Params.Target == EFnArityTargetTarget::Global)
	{
		AppendLine(Source, FString::Printf(TEXT("int %s(%s)"), *ProbeName, *Parameters));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn ") + Sum + TEXT(";"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s(%s);"), *ProbeName, *Arguments));
		AppendLine(Source, TEXT("}"));
	}
	else if (Params.Target == EFnArityTargetTarget::NamespaceGlobal)
	{
		const FString Namespace = TEXT("N") + Suffix;
		AppendLine(Source, FString::Printf(TEXT("namespace %s"), *Namespace));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tint %s(%s)"), *ProbeName, *Parameters));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn ") + Sum + TEXT(";"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s::%s(%s);"), *Namespace, *ProbeName, *Arguments));
		AppendLine(Source, TEXT("}"));
	}
	else
	{
		const FString TypeName = TEXT("FOwner") + Suffix;
		AppendLine(Source, FString::Printf(TEXT("struct %s"), *TypeName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tint %s(%s)"), *ProbeName, *Parameters));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn ") + Sum + TEXT(";"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\t%s Owner;"), *TypeName));
		AppendLine(Source, FString::Printf(TEXT("\treturn Owner.%s(%s);"), *ProbeName, *Arguments));
		AppendLine(Source, TEXT("}"));
	}
}

FString BuildOwnedSource(const FFnArityTargetParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FArityTargetCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = ExpectedFromArity(Cell.Arity);
	return Info;
}
}

FStringView FFnArityTargetGenerator::GetProductId() const
{
	return TEXT("LANG-FN-ARITY-TARGET");
}

TArray<FGeneratedCaseInfo> FFnArityTargetGenerator::ListCases() const
{
	using namespace FnArityTargetGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FArityTargetCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("FnArityTarget PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FFnArityTargetGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace FnArityTargetGeneratorDetail;
	FArityTargetCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FFnArityTargetGenerator::BuildDumpSource() const
{
	using namespace FnArityTargetGeneratorDetail;
	const TArray<FArityTargetCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FN-ARITY-TARGET"));
	AppendLine(Source, TEXT("// Cases: 21 total, 21 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FArityTargetCell& Cell = Cells[Index];
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
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

FString FFnArityTargetGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace FnArityTargetGeneratorDetail;
	const TArray<FArityTargetCell> Cells = AllCells();
	FString Source;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (Index > 0)
		{
			AppendLine(Source);
		}
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	OutCaseCount = Cells.Num();
	return Source;
}

FString FFnArityTargetGenerator::BuildAritySource(const FFnArityTargetParams& Params) const
{
	using namespace FnArityTargetGeneratorDetail;
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

int32 FFnArityTargetGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace FnArityTargetGeneratorDetail;
	FArityTargetCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromArity(Cell.Arity);
}
}
