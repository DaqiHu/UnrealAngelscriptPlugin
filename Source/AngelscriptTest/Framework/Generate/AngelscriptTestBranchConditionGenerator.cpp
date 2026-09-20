#include "Framework/Generate/AngelscriptTestBranchConditionGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace BranchConditionGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native EvaluateBranchCondition and ReadBranchConditionCalls.");

struct FBranchConditionCell
{
	EBranchConditionBranch Branch = EBranchConditionBranch::If;
	EBranchConditionKind Condition = EBranchConditionKind::Variable;
	EBranchConditionSelection Selection = EBranchConditionSelection::First;
	EBranchConditionLineEnding LineEnding = EBranchConditionLineEnding::Lf;
};

const TCHAR* BranchToken(const EBranchConditionBranch Branch)
{
	switch (Branch)
	{
	case EBranchConditionBranch::If:
		return TEXT("IF");
	case EBranchConditionBranch::IfElse:
		return TEXT("IF_ELSE");
	case EBranchConditionBranch::ElseIfChain:
		return TEXT("ELSE_IF_CHAIN");
	default:
		return nullptr;
	}
}

const TCHAR* ConditionToken(const EBranchConditionKind Condition)
{
	switch (Condition)
	{
	case EBranchConditionKind::Variable:
		return TEXT("VARIABLE");
	case EBranchConditionKind::Comparison:
		return TEXT("COMPARISON");
	case EBranchConditionKind::Logical:
		return TEXT("LOGICAL");
	case EBranchConditionKind::Negated:
		return TEXT("NEGATED");
	case EBranchConditionKind::SideEffect:
		return TEXT("SIDE_EFFECT");
	default:
		return nullptr;
	}
}

const TCHAR* SelectionToken(const EBranchConditionSelection Selection)
{
	switch (Selection)
	{
	case EBranchConditionSelection::First:
		return TEXT("FIRST");
	case EBranchConditionSelection::Second:
		return TEXT("SECOND");
	case EBranchConditionSelection::None:
		return TEXT("NONE");
	default:
		return nullptr;
	}
}

const TCHAR* LineEndingToken(const EBranchConditionLineEnding LineEnding)
{
	switch (LineEnding)
	{
	case EBranchConditionLineEnding::Lf:
		return TEXT("LF");
	case EBranchConditionLineEnding::Crlf:
		return TEXT("CRLF");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FBranchConditionParams& Params)
{
	return BranchToken(Params.Branch) != nullptr
		&& ConditionToken(Params.Condition) != nullptr
		&& SelectionToken(Params.Selection) != nullptr
		&& LineEndingToken(Params.LineEnding) != nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

void ApplyLineEnding(FString& Source, const EBranchConditionLineEnding LineEnding)
{
	if (LineEnding == EBranchConditionLineEnding::Crlf)
	{
		Source.ReplaceInline(TEXT("\n"), TEXT("\r\n"));
	}
}

FString MakeCaseId(const FBranchConditionCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CF-BRANCH-CONDITION-DEPTH-%s-%s-%s-%s"),
		BranchToken(Cell.Branch),
		ConditionToken(Cell.Condition),
		SelectionToken(Cell.Selection),
		LineEndingToken(Cell.LineEnding));
}

FString MakeEntryName(const FBranchConditionCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FString ConditionExpression(const EBranchConditionKind Condition, const TCHAR* Name)
{
	switch (Condition)
	{
	case EBranchConditionKind::Comparison:
		return FString::Printf(TEXT("%s == true"), Name);
	case EBranchConditionKind::Logical:
		return FString::Printf(TEXT("%s && true"), Name);
	case EBranchConditionKind::Negated:
		return FString::Printf(TEXT("!(%s == false)"), Name);
	case EBranchConditionKind::SideEffect:
		return FString::Printf(TEXT("EvaluateBranchCondition(%s)"), Name);
	case EBranchConditionKind::Variable:
	default:
		return FString(Name);
	}
}

int32 ExpectedFromCell(const FBranchConditionCell& Cell)
{
	const bool bFirst = Cell.Selection == EBranchConditionSelection::First;
	const bool bSecond = Cell.Selection == EBranchConditionSelection::Second;
	int32 Marker = 0;
	if (Cell.Branch == EBranchConditionBranch::If)
	{
		Marker = bFirst ? 10 : 0;
	}
	else if (Cell.Branch == EBranchConditionBranch::IfElse)
	{
		Marker = bFirst ? 10 : 20;
	}
	else
	{
		Marker = bFirst ? 10 : (bSecond ? 20 : 30);
	}

	int32 Calls = 0;
	if (Cell.Condition == EBranchConditionKind::SideEffect)
	{
		Calls = (Cell.Branch == EBranchConditionBranch::ElseIfChain && !bFirst) ? 2 : 1;
	}
	return Marker * 100 + Calls;
}

FBranchConditionParams ParamsFromCell(const FBranchConditionCell& Cell)
{
	FBranchConditionParams Params;
	Params.Branch = Cell.Branch;
	Params.Condition = Cell.Condition;
	Params.Selection = Cell.Selection;
	Params.LineEnding = Cell.LineEnding;
	return Params;
}

TArray<FBranchConditionCell> AllCells()
{
	TArray<FBranchConditionCell> Cells;
	Cells.SetNum(45);
	for (int32 Index = 0; Index < 45; ++Index)
	{
		FBranchConditionCell& Cell = Cells[Index];
		Cell.Branch = static_cast<EBranchConditionBranch>(Index / 30);
		Cell.Condition = static_cast<EBranchConditionKind>((Index / 6) % 5);
		Cell.Selection = static_cast<EBranchConditionSelection>((Index / 2) % 3);
		Cell.LineEnding = static_cast<EBranchConditionLineEnding>(Index % 2);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FBranchConditionCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FBranchConditionCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendEntry(FString& Source, const FBranchConditionParams& Params, const FString& FunctionName)
{
	const bool bFirst = Params.Selection == EBranchConditionSelection::First;
	const bool bSecond = Params.Selection == EBranchConditionSelection::Second;
	const FString FirstCondition = ConditionExpression(Params.Condition, TEXT("FirstCondition"));
	const FString SecondCondition = ConditionExpression(Params.Condition, TEXT("SecondCondition"));

	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, bFirst ? TEXT("\tbool FirstCondition = true;") : TEXT("\tbool FirstCondition = false;"));
	AppendLine(Source, bSecond ? TEXT("\tbool SecondCondition = true;") : TEXT("\tbool SecondCondition = false;"));
	AppendLine(Source, TEXT("\tint BranchMarker = 0;"));

	if (Params.Branch == EBranchConditionBranch::If)
	{
		AppendLine(Source, FString::Printf(TEXT("\tif (%s)"), *FirstCondition));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tBranchMarker = 10;"));
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Params.Branch == EBranchConditionBranch::IfElse)
	{
		AppendLine(Source, FString::Printf(TEXT("\tif (%s)"), *FirstCondition));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tBranchMarker = 10;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\telse"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tBranchMarker = 20;"));
		AppendLine(Source, TEXT("\t}"));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\tif (%s)"), *FirstCondition));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tBranchMarker = 10;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, FString::Printf(TEXT("\telse if (%s)"), *SecondCondition));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tBranchMarker = 20;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\telse"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tBranchMarker = 30;"));
		AppendLine(Source, TEXT("\t}"));
	}

	AppendLine(Source, TEXT("\treturn BranchMarker * 100 + ReadBranchConditionCalls();"));
	AppendLine(Source, TEXT("}"));
}

FString FinishSource(FString Source, const EBranchConditionLineEnding LineEnding)
{
	ApplyLineEnding(Source, LineEnding);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FBranchConditionCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	Info.ExecutionNotes = HostNotes;
	return Info;
}
}

FStringView FBranchConditionGenerator::GetProductId() const
{
	return TEXT("LANG-CF-BRANCH-CONDITION-DEPTH");
}

TArray<FGeneratedCaseInfo> FBranchConditionGenerator::ListCases() const
{
	using namespace BranchConditionGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FBranchConditionCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("BranchCondition PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FBranchConditionGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace BranchConditionGeneratorDetail;
	FBranchConditionCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	FString Source;
	AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	return FinishSource(MoveTemp(Source), Cell.LineEnding);
}

FString FBranchConditionGenerator::BuildDumpSource() const
{
	using namespace BranchConditionGeneratorDetail;
	const TArray<FBranchConditionCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CF-BRANCH-CONDITION-DEPTH"));
	AppendLine(Source, TEXT("// Cases: 45 total, 45 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);

	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FBranchConditionCell& Cell = Cells[Index];
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		FString Section;
		AppendLine(Section, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Section, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Section, TEXT("// Outcome: ReturnValue"));
		AppendLine(Section, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Section, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Section, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		AppendLine(Section);
		AppendEntry(Section, ParamsFromCell(Cell), MakeEntryName(Cell));
		Source += FinishSource(MoveTemp(Section), Cell.LineEnding);
		if (Index + 1 < Cells.Num())
		{
			Source += (Cell.LineEnding == EBranchConditionLineEnding::Crlf) ? TEXT("\r\n") : TEXT("\n");
		}
	}
	return Source;
}

FString FBranchConditionGenerator::BuildBranchSource(const FBranchConditionParams& Params) const
{
	using namespace BranchConditionGeneratorDetail;
	if (!IsValidParams(Params))
	{
		return FString();
	}
	if (!Params.FunctionName.IsEmpty() && !IsValidGeneratedFunctionName(Params.FunctionName))
	{
		return FString();
	}
	FString Source;
	const FString FunctionName = Params.FunctionName.IsEmpty() ? FString(TEXT("Entry")) : Params.FunctionName;
	AppendEntry(Source, Params, FunctionName);
	return FinishSource(MoveTemp(Source), Params.LineEnding);
}

FString FBranchConditionGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace BranchConditionGeneratorDetail;
	const TArray<FBranchConditionCell> Cells = AllCells();
	OutCaseCount = Cells.Num();

	FString Source;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FBranchConditionCell& Cell = Cells[Index];
		FString Entry;
		AppendEntry(Entry, ParamsFromCell(Cell), MakeEntryName(Cell));
		Source += FinishSource(MoveTemp(Entry), Cell.LineEnding);
		if (Index + 1 < Cells.Num())
		{
			Source += (Cell.LineEnding == EBranchConditionLineEnding::Crlf) ? TEXT("\r\n") : TEXT("\n");
		}
	}
	return Source;
}

int32 FBranchConditionGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace BranchConditionGeneratorDetail;
	FBranchConditionCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}
}
