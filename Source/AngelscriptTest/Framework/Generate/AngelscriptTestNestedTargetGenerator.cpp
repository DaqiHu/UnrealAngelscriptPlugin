#include "Framework/Generate/AngelscriptTestNestedTargetGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace NestedTargetGeneratorDetail
{
struct FNestedTargetCell
{
	ENestedTargetNesting Nesting = ENestedTargetNesting::NestedLoop;
	ENestedTargetTransfer Transfer = ENestedTargetTransfer::Break;
	ENestedTargetSite Target = ENestedTargetSite::Inner;
};

const TCHAR* NestingToken(const ENestedTargetNesting Nesting)
{
	switch (Nesting)
	{
	case ENestedTargetNesting::NestedLoop:
		return TEXT("NESTED_LOOP");
	case ENestedTargetNesting::BranchLoop:
		return TEXT("BRANCH_LOOP");
	case ENestedTargetNesting::ThreeLevel:
		return TEXT("THREE_LEVEL");
	default:
		return nullptr;
	}
}

const TCHAR* TransferToken(const ENestedTargetTransfer Transfer)
{
	switch (Transfer)
	{
	case ENestedTargetTransfer::Break:
		return TEXT("BREAK");
	case ENestedTargetTransfer::Continue:
		return TEXT("CONTINUE");
	case ENestedTargetTransfer::Return:
		return TEXT("RETURN");
	default:
		return nullptr;
	}
}

const TCHAR* TargetToken(const ENestedTargetSite Target)
{
	switch (Target)
	{
	case ENestedTargetSite::Inner:
		return TEXT("INNER");
	case ENestedTargetSite::Outer:
		return TEXT("OUTER");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FNestedTargetParams& Params)
{
	return NestingToken(Params.Nesting) != nullptr
		&& TransferToken(Params.Transfer) != nullptr
		&& TargetToken(Params.Target) != nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FNestedTargetCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CF-NESTED-TARGETS-%s-%s-%s"),
		NestingToken(Cell.Nesting),
		TransferToken(Cell.Transfer),
		TargetToken(Cell.Target));
}

FString MakeEntryName(const FNestedTargetCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

void AppendInnerTransfer(FString& Source, const FNestedTargetCell& Cell, const FString& Indent)
{
	if (Cell.Target != ENestedTargetSite::Inner)
	{
		return;
	}
	if (Cell.Transfer == ENestedTargetTransfer::Break)
	{
		AppendLine(Source, Indent + TEXT("break;"));
	}
	else if (Cell.Transfer == ENestedTargetTransfer::Continue)
	{
		AppendLine(Source, Indent + TEXT("continue;"));
	}
	else
	{
		AppendLine(Source, Indent + TEXT("return Trace;"));
	}
}

void AppendOuterTransfer(FString& Source, const FNestedTargetCell& Cell)
{
	if (Cell.Target != ENestedTargetSite::Outer)
	{
		return;
	}
	if (Cell.Transfer == ENestedTargetTransfer::Break)
	{
		AppendLine(Source, TEXT("\t\tbreak;"));
	}
	else if (Cell.Transfer == ENestedTargetTransfer::Continue)
	{
		AppendLine(Source, TEXT("\t\tcontinue;"));
	}
	else
	{
		AppendLine(Source, TEXT("\t\treturn Trace;"));
	}
}

void AppendEntry(FString& Source, const FNestedTargetCell& Cell, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Trace = 0;"));
	AppendLine(Source, TEXT("\tfor (int Outer = 0; Outer < 2; ++Outer)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tTrace += 1;"));
	if (Cell.Nesting == ENestedTargetNesting::NestedLoop)
	{
		AppendLine(Source, TEXT("\t\tfor (int Inner = 0; Inner < 2; ++Inner)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, TEXT("\t\t\tTrace += 10;"));
		AppendInnerTransfer(Source, Cell, TEXT("\t\t\t"));
		AppendLine(Source, TEXT("\t\t}"));
	}
	else if (Cell.Nesting == ENestedTargetNesting::BranchLoop)
	{
		AppendLine(Source, TEXT("\t\tif (Outer >= 0)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, TEXT("\t\t\tTrace += 2;"));
		AppendLine(Source, TEXT("\t\t\tfor (int Inner = 0; Inner < 2; ++Inner)"));
		AppendLine(Source, TEXT("\t\t\t{"));
		AppendLine(Source, TEXT("\t\t\t\tTrace += 10;"));
		AppendInnerTransfer(Source, Cell, TEXT("\t\t\t\t"));
		AppendLine(Source, TEXT("\t\t\t}"));
		AppendLine(Source, TEXT("\t\t}"));
	}
	else
	{
		AppendLine(Source, TEXT("\t\tif (Outer >= 0)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, TEXT("\t\t\tfor (int Middle = 0; Middle < 2; ++Middle)"));
		AppendLine(Source, TEXT("\t\t\t{"));
		AppendLine(Source, TEXT("\t\t\t\tTrace += 10;"));
		AppendLine(Source, TEXT("\t\t\t\tfor (int Inner = 0; Inner < 2; ++Inner)"));
		AppendLine(Source, TEXT("\t\t\t\t{"));
		AppendLine(Source, TEXT("\t\t\t\t\tTrace += 100;"));
		AppendInnerTransfer(Source, Cell, TEXT("\t\t\t\t\t"));
		AppendLine(Source, TEXT("\t\t\t\t}"));
		AppendLine(Source, TEXT("\t\t\t}"));
		AppendLine(Source, TEXT("\t\t}"));
	}
	AppendLine(Source, TEXT("\t\tTrace += 1000;"));
	AppendOuterTransfer(Source, Cell);
	AppendLine(Source, TEXT("\t\tTrace += 10000;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("\treturn Trace;"));
	AppendLine(Source, TEXT("}"));
}

int32 SimulateExpectedTrace(const FNestedTargetCell& Cell)
{
	int32 Trace = 0;
	for (int32 Outer = 0; Outer < 2; ++Outer)
	{
		Trace += 1;
		if (Cell.Nesting == ENestedTargetNesting::NestedLoop)
		{
			for (int32 Inner = 0; Inner < 2; ++Inner)
			{
				Trace += 10;
				if (Cell.Target == ENestedTargetSite::Inner)
				{
					if (Cell.Transfer == ENestedTargetTransfer::Break)
					{
						break;
					}
					if (Cell.Transfer == ENestedTargetTransfer::Continue)
					{
						continue;
					}
					return Trace;
				}
			}
		}
		else if (Cell.Nesting == ENestedTargetNesting::BranchLoop)
		{
			Trace += 2;
			for (int32 Inner = 0; Inner < 2; ++Inner)
			{
				Trace += 10;
				if (Cell.Target == ENestedTargetSite::Inner)
				{
					if (Cell.Transfer == ENestedTargetTransfer::Break)
					{
						break;
					}
					if (Cell.Transfer == ENestedTargetTransfer::Continue)
					{
						continue;
					}
					return Trace;
				}
			}
		}
		else
		{
			for (int32 Middle = 0; Middle < 2; ++Middle)
			{
				Trace += 10;
				for (int32 Inner = 0; Inner < 2; ++Inner)
				{
					Trace += 100;
					if (Cell.Target == ENestedTargetSite::Inner)
					{
						if (Cell.Transfer == ENestedTargetTransfer::Break)
						{
							break;
						}
						if (Cell.Transfer == ENestedTargetTransfer::Continue)
						{
							continue;
						}
						return Trace;
					}
				}
			}
		}

		Trace += 1000;
		if (Cell.Target == ENestedTargetSite::Outer)
		{
			if (Cell.Transfer == ENestedTargetTransfer::Break)
			{
				break;
			}
			if (Cell.Transfer == ENestedTargetTransfer::Continue)
			{
				continue;
			}
			return Trace;
		}
		Trace += 10000;
	}
	return Trace;
}

TArray<FNestedTargetCell> AllCells()
{
	TArray<FNestedTargetCell> Cells;
	Cells.SetNum(18);
	for (int32 Index = 0; Index < 18; ++Index)
	{
		FNestedTargetCell& Cell = Cells[Index];
		Cell.Nesting = static_cast<ENestedTargetNesting>(Index / 6);
		Cell.Transfer = static_cast<ENestedTargetTransfer>((Index / 2) % 3);
		Cell.Target = static_cast<ENestedTargetSite>(Index % 2);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FNestedTargetCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FNestedTargetCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FNestedTargetParams ParamsFromCell(const FNestedTargetCell& Cell)
{
	FNestedTargetParams Params;
	Params.Nesting = Cell.Nesting;
	Params.Transfer = Cell.Transfer;
	Params.Target = Cell.Target;
	return Params;
}

FString BuildOwnedSource(const FNestedTargetParams& Params, const FString& FunctionName)
{
	FNestedTargetCell Cell;
	Cell.Nesting = Params.Nesting;
	Cell.Transfer = Params.Transfer;
	Cell.Target = Params.Target;
	FString Source;
	AppendEntry(Source, Cell, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FNestedTargetCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = SimulateExpectedTrace(Cell);
	return Info;
}
}

FStringView FNestedTargetGenerator::GetProductId() const
{
	return TEXT("LANG-CF-NESTED-TARGETS");
}

TArray<FGeneratedCaseInfo> FNestedTargetGenerator::ListCases() const
{
	using namespace NestedTargetGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FNestedTargetCell& Cell : AllCells())
	{
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("NestedTarget PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MakeCaseInfo(Cell));
	}
	return Cases;
}

FString FNestedTargetGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace NestedTargetGeneratorDetail;
	FNestedTargetCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FNestedTargetGenerator::BuildDumpSource() const
{
	using namespace NestedTargetGeneratorDetail;
	const TArray<FNestedTargetCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CF-NESTED-TARGETS"));
	AppendLine(Source, TEXT("// Cases: 18 total, 18 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FNestedTargetCell& Cell = Cells[Index];
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendEntry(Source, Cell, MakeEntryName(Cell));
	}
	return Source;
}

FString FNestedTargetGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace NestedTargetGeneratorDetail;
	const TArray<FNestedTargetCell> Cells = AllCells();
	OutCaseCount = Cells.Num();
	FString Source;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (Index > 0)
		{
			AppendLine(Source);
		}
		AppendEntry(Source, Cells[Index], MakeEntryName(Cells[Index]));
	}
	return Source;
}

FString FNestedTargetGenerator::BuildNestedSource(const FNestedTargetParams& Params) const
{
	using namespace NestedTargetGeneratorDetail;
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

int32 FNestedTargetGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace NestedTargetGeneratorDetail;
	FNestedTargetCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return SimulateExpectedTrace(Cell);
}
}
