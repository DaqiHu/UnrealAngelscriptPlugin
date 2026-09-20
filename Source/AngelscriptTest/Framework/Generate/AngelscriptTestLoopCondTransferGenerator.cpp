#include "Framework/Generate/AngelscriptTestLoopCondTransferGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace LoopCondTransferGeneratorDetail
{
struct FLoopCondCell
{
	ELoopCondLoop Loop = ELoopCondLoop::While;
	ELoopCondExpr Condition = ELoopCondExpr::Variable;
	ELoopCondCount Count = ELoopCondCount::Zero;
	ELoopCondTransfer Transfer = ELoopCondTransfer::None;
};

const TCHAR* LoopToken(const ELoopCondLoop Loop)
{
	switch (Loop)
	{
	case ELoopCondLoop::While:
		return TEXT("WHILE");
	case ELoopCondLoop::DoWhile:
		return TEXT("DO_WHILE");
	case ELoopCondLoop::For:
		return TEXT("FOR");
	default:
		return nullptr;
	}
}

const TCHAR* ConditionToken(const ELoopCondExpr Condition)
{
	switch (Condition)
	{
	case ELoopCondExpr::Variable:
		return TEXT("VARIABLE");
	case ELoopCondExpr::Comparison:
		return TEXT("COMPARISON");
	case ELoopCondExpr::Logical:
		return TEXT("LOGICAL");
	case ELoopCondExpr::Negated:
		return TEXT("NEGATED");
	case ELoopCondExpr::SideEffect:
		return TEXT("SIDE_EFFECT");
	default:
		return nullptr;
	}
}

const TCHAR* CountToken(const ELoopCondCount Count)
{
	switch (Count)
	{
	case ELoopCondCount::Zero:
		return TEXT("ZERO");
	case ELoopCondCount::One:
		return TEXT("ONE");
	case ELoopCondCount::Two:
		return TEXT("TWO");
	default:
		return nullptr;
	}
}

const TCHAR* TransferToken(const ELoopCondTransfer Transfer)
{
	switch (Transfer)
	{
	case ELoopCondTransfer::None:
		return TEXT("NONE");
	case ELoopCondTransfer::Break:
		return TEXT("BREAK");
	case ELoopCondTransfer::Continue:
		return TEXT("CONTINUE");
	case ELoopCondTransfer::Return:
		return TEXT("RETURN");
	default:
		return nullptr;
	}
}

int32 CountLimit(const ELoopCondCount Count)
{
	switch (Count)
	{
	case ELoopCondCount::Zero:
		return 0;
	case ELoopCondCount::One:
		return 1;
	case ELoopCondCount::Two:
		return 2;
	default:
		return 0;
	}
}

bool IsValidParams(const FLoopCondTransferParams& Params)
{
	return LoopToken(Params.Loop) != nullptr
		&& ConditionToken(Params.Condition) != nullptr
		&& CountToken(Params.Count) != nullptr
		&& TransferToken(Params.Transfer) != nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FLoopCondCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CF-LOOP-COND-TRANSFER-DEPTH-%s-%s-%s-%s"),
		LoopToken(Cell.Loop),
		ConditionToken(Cell.Condition),
		CountToken(Cell.Count),
		TransferToken(Cell.Transfer));
}

FString MakeEntryName(const FLoopCondCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FString ConditionExpression(const ELoopCondExpr Condition)
{
	switch (Condition)
	{
	case ELoopCondExpr::Variable:
		return TEXT("KeepGoing");
	case ELoopCondExpr::Comparison:
		return TEXT("Index < Limit");
	case ELoopCondExpr::Logical:
		return TEXT("(Index < Limit) && (Limit >= 0)");
	case ELoopCondExpr::Negated:
		return TEXT("!(Index >= Limit)");
	case ELoopCondExpr::SideEffect:
	default:
		return TEXT("CheckCondition(Index, Limit)");
	}
}

int32 ExpectedBodyCalls(const ELoopCondCount Count, const ELoopCondTransfer Transfer)
{
	const int32 Limit = CountLimit(Count);
	if (Transfer == ELoopCondTransfer::Break)
	{
		return FMath::Min(Limit, 1);
	}
	if (Transfer == ELoopCondTransfer::Return)
	{
		return Limit > 0 ? 1 : 0;
	}
	return Limit;
}

int32 ExpectedConditionCalls(const FLoopCondCell& Cell)
{
	if (Cell.Condition != ELoopCondExpr::SideEffect)
	{
		return 0;
	}
	const int32 BodyCalls = ExpectedBodyCalls(Cell.Count, Cell.Transfer);
	const bool bBreakOrReturn = Cell.Transfer == ELoopCondTransfer::Break
		|| Cell.Transfer == ELoopCondTransfer::Return;
	if (Cell.Loop == ELoopCondLoop::DoWhile)
	{
		return bBreakOrReturn ? 0 : BodyCalls;
	}
	return 1 + (bBreakOrReturn ? 0 : BodyCalls);
}

int32 ExpectedFromCell(const FLoopCondCell& Cell)
{
	const int32 BodyCalls = ExpectedBodyCalls(Cell.Count, Cell.Transfer);
	const int32 ConditionCalls = ExpectedConditionCalls(Cell);
	if (Cell.Transfer == ELoopCondTransfer::Return && CountLimit(Cell.Count) > 0)
	{
		return 1000 + BodyCalls * 100 + ConditionCalls;
	}
	return BodyCalls * 100 + ConditionCalls;
}

FString HostNotes(const FLoopCondCell& Cell)
{
	if (Cell.Condition == ELoopCondExpr::SideEffect)
	{
		return TEXT("Requires host functions CheckCondition(int, int) and GetConditionCalls().");
	}
	return TEXT("Requires host function GetConditionCalls().");
}

void AppendEntry(FString& Source, const FLoopCondCell& Cell, const FString& FunctionName)
{
	const FString Condition = ConditionExpression(Cell.Condition);
	const bool bDoWhile = Cell.Loop == ELoopCondLoop::DoWhile;
	const bool bFor = Cell.Loop == ELoopCondLoop::For;
	const bool bVariable = Cell.Condition == ELoopCondExpr::Variable;
	const bool bBreak = Cell.Transfer == ELoopCondTransfer::Break;
	const bool bContinue = Cell.Transfer == ELoopCondTransfer::Continue;
	const bool bReturn = Cell.Transfer == ELoopCondTransfer::Return;

	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\tint Limit = %d;"), CountLimit(Cell.Count)));
	AppendLine(Source, TEXT("\tint BodyCalls = 0;"));
	AppendLine(Source, TEXT("\tint Index = 0;"));
	if (bVariable)
	{
		AppendLine(Source, TEXT("\tbool KeepGoing = Index < Limit;"));
	}

	if (bDoWhile)
	{
		AppendLine(Source, TEXT("\tif (Limit == 0)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 0;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\tdo"));
		AppendLine(Source, TEXT("\t{"));
	}
	else if (bFor)
	{
		AppendLine(Source, FString::Printf(TEXT("\tfor (; %s; ++Index)"), *Condition));
		AppendLine(Source, TEXT("\t{"));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\twhile (%s)"), *Condition));
		AppendLine(Source, TEXT("\t{"));
	}

	AppendLine(Source, TEXT("\t\t++BodyCalls;"));
	if (bBreak)
	{
		AppendLine(Source, TEXT("\t\tif (Index == 0)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, TEXT("\t\t\tbreak;"));
		AppendLine(Source, TEXT("\t\t}"));
	}
	else if (bContinue)
	{
		AppendLine(Source, TEXT("\t\tif (Index == 0)"));
		AppendLine(Source, TEXT("\t\t{"));
		if (!bFor)
		{
			AppendLine(Source, TEXT("\t\t\t++Index;"));
		}
		if (bVariable)
		{
			AppendLine(Source, bFor
				? TEXT("\t\t\tKeepGoing = Index + 1 < Limit;")
				: TEXT("\t\t\tKeepGoing = Index < Limit;"));
		}
		AppendLine(Source, TEXT("\t\t\tcontinue;"));
		AppendLine(Source, TEXT("\t\t}"));
	}
	else if (bReturn)
	{
		AppendLine(Source, TEXT("\t\tif (Index == 0)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, TEXT("\t\t\treturn 1000 + BodyCalls * 100 + GetConditionCalls();"));
		AppendLine(Source, TEXT("\t\t}"));
	}

	if (bFor && bVariable && !bBreak && !bReturn)
	{
		AppendLine(Source, TEXT("\t\tKeepGoing = Index + 1 < Limit;"));
	}

	if (!bFor)
	{
		AppendLine(Source, TEXT("\t\t++Index;"));
		if (bVariable)
		{
			AppendLine(Source, TEXT("\t\tKeepGoing = Index < Limit;"));
		}
	}

	AppendLine(Source, TEXT("\t}"));
	if (bDoWhile)
	{
		AppendLine(Source, FString::Printf(TEXT("\twhile (%s);"), *Condition));
	}
	AppendLine(Source);
	AppendLine(Source, TEXT("\treturn BodyCalls * 100 + GetConditionCalls();"));
	AppendLine(Source, TEXT("}"));
}

TArray<FLoopCondCell> AllCells()
{
	TArray<FLoopCondCell> Cells;
	Cells.SetNum(180);
	for (int32 Index = 0; Index < 180; ++Index)
	{
		FLoopCondCell& Cell = Cells[Index];
		Cell.Loop = static_cast<ELoopCondLoop>(Index / 60);
		Cell.Condition = static_cast<ELoopCondExpr>((Index / 12) % 5);
		Cell.Count = static_cast<ELoopCondCount>((Index / 4) % 3);
		Cell.Transfer = static_cast<ELoopCondTransfer>(Index % 4);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FLoopCondCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FLoopCondCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FGeneratedCaseInfo MakeCaseInfo(const FLoopCondCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	Info.ExecutionNotes = HostNotes(Cell);
	return Info;
}
}

FStringView FLoopCondTransferGenerator::GetProductId() const
{
	return TEXT("LANG-CF-LOOP-COND-TRANSFER-DEPTH");
}

TArray<FGeneratedCaseInfo> FLoopCondTransferGenerator::ListCases() const
{
	using namespace LoopCondTransferGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FLoopCondCell& Cell : AllCells())
	{
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("LoopCondTransfer PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MakeCaseInfo(Cell));
	}
	return Cases;
}

FString FLoopCondTransferGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace LoopCondTransferGeneratorDetail;
	FLoopCondCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	FString Source;
	AppendEntry(Source, Cell, MakeEntryName(Cell));
	return Source;
}

FString FLoopCondTransferGenerator::BuildDumpSource() const
{
	using namespace LoopCondTransferGeneratorDetail;
	const TArray<FLoopCondCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CF-LOOP-COND-TRANSFER-DEPTH"));
	AppendLine(Source, TEXT("// Cases: 180 total, 180 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FLoopCondCell& Cell = Cells[Index];
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		AppendLine(Source);
		AppendEntry(Source, Cell, MakeEntryName(Cell));
		if (Index + 1 < Cells.Num())
		{
			AppendLine(Source);
		}
	}
	return Source;
}

FString FLoopCondTransferGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace LoopCondTransferGeneratorDetail;
	const TArray<FLoopCondCell> Cells = AllCells();
	OutCaseCount = Cells.Num();

	FString Source;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		AppendEntry(Source, Cells[Index], MakeEntryName(Cells[Index]));
		if (Index + 1 < Cells.Num())
		{
			AppendLine(Source);
		}
	}
	return Source;
}

FString FLoopCondTransferGenerator::BuildLoopCondSource(const FLoopCondTransferParams& Params) const
{
	using namespace LoopCondTransferGeneratorDetail;
	if (!IsValidParams(Params))
	{
		return FString();
	}
	if (!Params.FunctionName.IsEmpty() && !IsValidGeneratedFunctionName(Params.FunctionName))
	{
		return FString();
	}
	FLoopCondCell Cell;
	Cell.Loop = Params.Loop;
	Cell.Condition = Params.Condition;
	Cell.Count = Params.Count;
	Cell.Transfer = Params.Transfer;
	const FString FunctionName = Params.FunctionName.IsEmpty() ? FString(TEXT("Entry")) : Params.FunctionName;
	FString Source;
	AppendEntry(Source, Cell, FunctionName);
	return Source;
}

int32 FLoopCondTransferGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace LoopCondTransferGeneratorDetail;
	FLoopCondCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}
}
