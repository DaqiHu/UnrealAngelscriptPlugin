#include "Framework/Generate/AngelscriptTestLoopDepthGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace LoopDepthGeneratorDetail
{
struct FLoopDepthCell
{
	ELoopDepthLoop Loop = ELoopDepthLoop::While;
	ELoopDepthCount Count = ELoopDepthCount::Zero;
	ELoopDepthTransfer Transfer = ELoopDepthTransfer::None;
};

const TCHAR* LoopToken(const ELoopDepthLoop Loop)
{
	switch (Loop)
	{
	case ELoopDepthLoop::While:
		return TEXT("WHILE");
	case ELoopDepthLoop::DoWhile:
		return TEXT("DO_WHILE");
	case ELoopDepthLoop::For:
		return TEXT("FOR");
	default:
		return nullptr;
	}
}

const TCHAR* CountToken(const ELoopDepthCount Count)
{
	switch (Count)
	{
	case ELoopDepthCount::Zero:
		return TEXT("ZERO");
	case ELoopDepthCount::One:
		return TEXT("ONE");
	case ELoopDepthCount::Two:
		return TEXT("TWO");
	case ELoopDepthCount::Many:
		return TEXT("MANY");
	default:
		return nullptr;
	}
}

const TCHAR* TransferToken(const ELoopDepthTransfer Transfer)
{
	switch (Transfer)
	{
	case ELoopDepthTransfer::None:
		return TEXT("NONE");
	case ELoopDepthTransfer::Break:
		return TEXT("BREAK");
	case ELoopDepthTransfer::Continue:
		return TEXT("CONTINUE");
	case ELoopDepthTransfer::Return:
		return TEXT("RETURN");
	default:
		return nullptr;
	}
}

int32 CountLimit(const ELoopDepthCount Count)
{
	switch (Count)
	{
	case ELoopDepthCount::Zero:
		return 0;
	case ELoopDepthCount::One:
		return 1;
	case ELoopDepthCount::Two:
		return 2;
	case ELoopDepthCount::Many:
		return 4;
	default:
		return 0;
	}
}

bool IsValidParams(const FLoopDepthParams& Params)
{
	return LoopToken(Params.Loop) != nullptr
		&& CountToken(Params.Count) != nullptr
		&& TransferToken(Params.Transfer) != nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FLoopDepthCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CF-LOOP-DEPTH-%s-%s-%s"),
		LoopToken(Cell.Loop),
		CountToken(Cell.Count),
		TransferToken(Cell.Transfer));
}

FString MakeEntryName(const FLoopDepthCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

void AppendHelpers(FString& Source)
{
	AppendLine(Source, TEXT("bool CountCondition(int Index, int Limit, int&inout Counter)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\t++Counter;"));
	AppendLine(Source, TEXT("\treturn Index < Limit;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int CountIncrement(int&inout Index, int&inout Counter)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\t++Counter;"));
	AppendLine(Source, TEXT("\t++Index;"));
	AppendLine(Source, TEXT("\treturn Index;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendLoopBody(FString& Source, const ELoopDepthTransfer Transfer)
{
	AppendLine(Source, TEXT("\t\t++BodyCount;"));
	if (Transfer == ELoopDepthTransfer::Break)
	{
		AppendLine(Source, TEXT("\t\tbreak;"));
	}
	else if (Transfer == ELoopDepthTransfer::Return)
	{
		AppendLine(Source, TEXT("\t\treturn BodyCount * 100 + ConditionCount * 10 + IncrementCount;"));
	}
	else
	{
		AppendLine(Source, TEXT("\t\tCountIncrement(Index, IncrementCount);"));
		if (Transfer == ELoopDepthTransfer::Continue)
		{
			AppendLine(Source, TEXT("\t\tcontinue;"));
		}
	}
}

void AppendEntry(FString& Source, const FLoopDepthCell& Cell, const FString& FunctionName)
{
	const bool bWhile = Cell.Loop == ELoopDepthLoop::While;
	const bool bDoWhile = Cell.Loop == ELoopDepthLoop::DoWhile;

	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\tint Limit = %d;"), CountLimit(Cell.Count)));
	AppendLine(Source, TEXT("\tint Index = 0;"));
	AppendLine(Source, TEXT("\tint BodyCount = 0;"));
	AppendLine(Source, TEXT("\tint ConditionCount = 0;"));
	AppendLine(Source, TEXT("\tint IncrementCount = 0;"));

	if (bWhile)
	{
		AppendLine(Source, TEXT("\twhile (CountCondition(Index, Limit, ConditionCount))"));
		AppendLine(Source, TEXT("\t{"));
		AppendLoopBody(Source, Cell.Transfer);
		AppendLine(Source, TEXT("\t}"));
	}
	else if (bDoWhile)
	{
		AppendLine(Source, TEXT("\tdo"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tif (Index >= Limit)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, TEXT("\t\t\tbreak;"));
		AppendLine(Source, TEXT("\t\t}"));
		AppendLoopBody(Source, Cell.Transfer);
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\twhile (CountCondition(Index, Limit, ConditionCount));"));
	}
	else
	{
		AppendLine(Source, TEXT("\tfor (Index = 0; CountCondition(Index, Limit, ConditionCount); CountIncrement(Index, IncrementCount))"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\t++BodyCount;"));
		if (Cell.Transfer == ELoopDepthTransfer::Break)
		{
			AppendLine(Source, TEXT("\t\tbreak;"));
		}
		else if (Cell.Transfer == ELoopDepthTransfer::Return)
		{
			AppendLine(Source, TEXT("\t\treturn BodyCount * 100 + ConditionCount * 10 + IncrementCount;"));
		}
		else if (Cell.Transfer == ELoopDepthTransfer::Continue)
		{
			AppendLine(Source, TEXT("\t\tcontinue;"));
		}
		AppendLine(Source, TEXT("\t}"));
	}

	AppendLine(Source, TEXT("\treturn BodyCount * 100 + ConditionCount * 10 + IncrementCount;"));
	AppendLine(Source, TEXT("}"));
}

int32 ExpectedFromCell(const FLoopDepthCell& Cell)
{
	const int32 Limit = CountLimit(Cell.Count);
	const bool bDoWhile = Cell.Loop == ELoopDepthLoop::DoWhile;
	int32 BodyCount = 0;
	int32 ConditionCount = 0;
	int32 IncrementCount = 0;
	if (Limit == 0)
	{
		ConditionCount = bDoWhile ? 0 : 1;
	}
	else if (Cell.Transfer == ELoopDepthTransfer::Break || Cell.Transfer == ELoopDepthTransfer::Return)
	{
		BodyCount = 1;
		ConditionCount = bDoWhile ? 0 : 1;
	}
	else
	{
		BodyCount = Limit;
		ConditionCount = bDoWhile ? Limit : Limit + 1;
		IncrementCount = Limit;
	}
	return BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

TArray<FLoopDepthCell> AllCells()
{
	TArray<FLoopDepthCell> Cells;
	Cells.SetNum(48);
	for (int32 Index = 0; Index < 48; ++Index)
	{
		FLoopDepthCell& Cell = Cells[Index];
		Cell.Loop = static_cast<ELoopDepthLoop>(Index / 16);
		Cell.Count = static_cast<ELoopDepthCount>((Index / 4) % 4);
		Cell.Transfer = static_cast<ELoopDepthTransfer>(Index % 4);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FLoopDepthCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FLoopDepthCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FGeneratedCaseInfo MakeCaseInfo(const FLoopDepthCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	return Info;
}
}

FStringView FLoopDepthGenerator::GetProductId() const
{
	return TEXT("LANG-CF-LOOP-DEPTH");
}

TArray<FGeneratedCaseInfo> FLoopDepthGenerator::ListCases() const
{
	using namespace LoopDepthGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FLoopDepthCell& Cell : AllCells())
	{
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("LoopDepth PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MakeCaseInfo(Cell));
	}
	return Cases;
}

FString FLoopDepthGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace LoopDepthGeneratorDetail;
	FLoopDepthCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	FString Source;
	AppendHelpers(Source);
	AppendEntry(Source, Cell, MakeEntryName(Cell));
	return Source;
}

FString FLoopDepthGenerator::BuildDumpSource() const
{
	using namespace LoopDepthGeneratorDetail;
	const TArray<FLoopDepthCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CF-LOOP-DEPTH"));
	AppendLine(Source, TEXT("// Cases: 48 total, 48 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FLoopDepthCell& Cell = Cells[Index];
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendEntry(Source, Cell, MakeEntryName(Cell));
		if (Index + 1 < Cells.Num())
		{
			AppendLine(Source);
		}
	}
	return Source;
}

FString FLoopDepthGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace LoopDepthGeneratorDetail;
	const TArray<FLoopDepthCell> Cells = AllCells();
	OutCaseCount = Cells.Num();

	FString Source;
	AppendHelpers(Source);
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

FString FLoopDepthGenerator::BuildLoopSource(const FLoopDepthParams& Params) const
{
	using namespace LoopDepthGeneratorDetail;
	if (!IsValidParams(Params))
	{
		return FString();
	}
	if (!Params.FunctionName.IsEmpty() && !IsValidGeneratedFunctionName(Params.FunctionName))
	{
		return FString();
	}
	FLoopDepthCell Cell;
	Cell.Loop = Params.Loop;
	Cell.Count = Params.Count;
	Cell.Transfer = Params.Transfer;
	const FString FunctionName = Params.FunctionName.IsEmpty() ? FString(TEXT("Entry")) : Params.FunctionName;
	FString Source;
	AppendHelpers(Source);
	AppendEntry(Source, Cell, FunctionName);
	return Source;
}

int32 FLoopDepthGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace LoopDepthGeneratorDetail;
	FLoopDepthCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}
}
