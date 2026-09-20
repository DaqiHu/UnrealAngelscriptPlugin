#include "Framework/Generate/AngelscriptTestForLoopGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ForLoopGeneratorDetail
{
struct FForLoopCell
{
	bool bInit = false;
	bool bCondition = false;
	bool bIncrement = false;
	int32 Limit = 0;
	const TCHAR* CountToken = TEXT("ZERO");
};

TArray<FForLoopCell> AllCells();
int32 ExpectedFromCell(const FForLoopCell& Cell);

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

const TCHAR* PresenceToken(const bool bPresent)
{
	return bPresent ? TEXT("PRESENT") : TEXT("OMITTED");
}

FString MakeCaseId(const FForLoopCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CF-FOR-CLAUSES-%s-%s-%s-%s"),
		PresenceToken(Cell.bInit),
		PresenceToken(Cell.bCondition),
		PresenceToken(Cell.bIncrement),
		Cell.CountToken);
}

FString MakeEntryName(const FForLoopCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

bool TryFindCell(const FStringView CaseId, FForLoopCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FForLoopCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FForLoopParams ParamsFromCell(const FForLoopCell& Cell)
{
	FForLoopParams Params;
	Params.bInit = Cell.bInit;
	Params.bCondition = Cell.bCondition;
	Params.bIncrement = Cell.bIncrement;
	Params.Limit = Cell.Limit;
	return Params;
}

FGeneratedCaseInfo MakeCaseInfo(const FForLoopCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	return Info;
}

int32 ExpectedFromCell(const FForLoopCell& Cell)
{
	const int32 InitCount = Cell.bInit ? 1 : 0;
	const int32 BodyCount = Cell.bCondition ? Cell.Limit : (Cell.Limit > 0 ? Cell.Limit : 1);
	const int32 ConditionCount = Cell.bCondition ? Cell.Limit + 1 : 0;
	int32 IncrementCount = 0;
	if (Cell.bIncrement)
	{
		IncrementCount = Cell.bCondition ? Cell.Limit : (BodyCount > 0 ? BodyCount - 1 : 0);
	}
	return InitCount * 1000 + BodyCount * 100 + ConditionCount * 10 + IncrementCount;
}

void AppendHelpers(FString& Source)
{
	AppendLine(Source, TEXT("int InitializeIndex(int&inout InitCount)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\t++InitCount;"));
	AppendLine(Source, TEXT("\treturn 0;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("bool CountCondition(int Index, int Limit, int&inout ConditionCount)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\t++ConditionCount;"));
	AppendLine(Source, TEXT("\treturn Index < Limit;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int CountIncrement(int&inout Index, int&inout IncrementCount)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\t++IncrementCount;"));
	AppendLine(Source, TEXT("\t++Index;"));
	AppendLine(Source, TEXT("\treturn Index;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendEntry(FString& Source, const FForLoopParams& Params, const FString& FunctionName)
{
	const FString InitClause = Params.bInit ? TEXT("Index = InitializeIndex(InitCount)") : FString();
	const FString ConditionClause =
		Params.bCondition ? TEXT("CountCondition(Index, Limit, ConditionCount)") : FString();
	const FString IncrementClause =
		Params.bIncrement ? TEXT("CountIncrement(Index, IncrementCount)") : FString();

	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\tint Limit = %d;"), Params.Limit));
	AppendLine(Source, TEXT("\tint Index = 0;"));
	AppendLine(Source, TEXT("\tint InitCount = 0;"));
	AppendLine(Source, TEXT("\tint BodyCount = 0;"));
	AppendLine(Source, TEXT("\tint ConditionCount = 0;"));
	AppendLine(Source, TEXT("\tint IncrementCount = 0;"));
	AppendLine(Source, FString::Printf(
		TEXT("\tfor (%s; %s; %s)"),
		*InitClause,
		*ConditionClause,
		*IncrementClause));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\t++BodyCount;"));
	if (!Params.bCondition)
	{
		AppendLine(Source, TEXT("\t\tif (Index >= Limit - 1)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, TEXT("\t\t\tbreak;"));
		AppendLine(Source, TEXT("\t\t}"));
	}
	if (!Params.bIncrement)
	{
		AppendLine(Source, TEXT("\t\t++Index;"));
	}
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("\treturn InitCount * 1000 + BodyCount * 100 + ConditionCount * 10 + IncrementCount;"));
	AppendLine(Source, TEXT("}"));
}

TArray<FForLoopCell> AllCells()
{
	static const bool Presence[] = {true, false};
	static const int32 Limits[] = {0, 1, 4};
	static const TCHAR* CountTokens[] = {TEXT("ZERO"), TEXT("ONE"), TEXT("MANY")};

	TArray<FForLoopCell> Cells;
	Cells.Reserve(24);
	for (const bool bInit : Presence)
	{
		for (const bool bCondition : Presence)
		{
			for (const bool bIncrement : Presence)
			{
				for (int32 CountIndex = 0; CountIndex < 3; ++CountIndex)
				{
					Cells.Add({
						.bInit = bInit,
						.bCondition = bCondition,
						.bIncrement = bIncrement,
						.Limit = Limits[CountIndex],
						.CountToken = CountTokens[CountIndex],
					});
				}
			}
		}
	}
	return Cells;
}
}

FStringView FForLoopGenerator::GetProductId() const
{
	return TEXT("LANG-CF-FOR-CLAUSES");
}

TArray<FGeneratedCaseInfo> FForLoopGenerator::ListCases() const
{
	using namespace ForLoopGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FForLoopCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("ForLoop PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FForLoopGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ForLoopGeneratorDetail;
	FForLoopCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	FString Source;
	AppendHelpers(Source);
	AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	return Source;
}

FString FForLoopGenerator::BuildDumpSource() const
{
	using namespace ForLoopGeneratorDetail;
	const TArray<FForLoopCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CF-FOR-CLAUSES"));
	AppendLine(Source, TEXT("// Cases: 24 total, 24 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		if (Index + 1 < Cells.Num())
		{
			AppendLine(Source);
		}
	}
	return Source;
}

FString FForLoopGenerator::BuildForSource(const FForLoopParams& Params) const
{
	using namespace ForLoopGeneratorDetail;
	if (!Params.FunctionName.IsEmpty() && !IsValidGeneratedFunctionName(Params.FunctionName))
	{
		return FString();
	}
	FString Source;
	AppendHelpers(Source);
	const FString FunctionName = Params.FunctionName.IsEmpty() ? FString(TEXT("Entry")) : Params.FunctionName;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FString FForLoopGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ForLoopGeneratorDetail;
	const TArray<FForLoopCell> Cells = AllCells();
	OutCaseCount = Cells.Num();

	FString Source;
	AppendHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FForLoopCell& Cell = Cells[Index];
		FForLoopParams Params;
		Params.bInit = Cell.bInit;
		Params.bCondition = Cell.bCondition;
		Params.bIncrement = Cell.bIncrement;
		Params.Limit = Cell.Limit;
		AppendEntry(Source, Params, MakeEntryName(Cell));
		if (Index + 1 < Cells.Num())
		{
			AppendLine(Source);
		}
	}
	return Source;
}

int32 FForLoopGenerator::GetExpected(FStringView CaseId) const
{
	using namespace ForLoopGeneratorDetail;
	const FString Needle(CaseId);
	for (const FForLoopCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			return ExpectedFromCell(Cell);
		}
	}
	return 0;
}
}
