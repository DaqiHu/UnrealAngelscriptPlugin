#include "Framework/Generate/AngelscriptTestStatementTransferGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace StatementTransferGeneratorDetail
{
constexpr int32 StatementAxisCount = 8;
constexpr int32 CountAxisCount = 4;
constexpr int32 TransferAxisCount = 8;
constexpr int32 NestingAxisCount = 6;
constexpr int32 TotalCells = StatementAxisCount * CountAxisCount * TransferAxisCount * NestingAxisCount;

struct FStatementCell
{
	EStatementKind Statement = EStatementKind::If;
	EStatementCount Count = EStatementCount::Zero;
	EStatementTransfer Transfer = EStatementTransfer::None;
	EStatementNesting Nesting = EStatementNesting::None;
};

const TCHAR* StatementToken(const EStatementKind Statement)
{
	switch (Statement)
	{
	case EStatementKind::If:
		return TEXT("IF");
	case EStatementKind::IfElse:
		return TEXT("IF_ELSE");
	case EStatementKind::ElseIf:
		return TEXT("ELSE_IF");
	case EStatementKind::While:
		return TEXT("WHILE");
	case EStatementKind::DoWhile:
		return TEXT("DO_WHILE");
	case EStatementKind::For:
		return TEXT("FOR");
	case EStatementKind::Switch:
		return TEXT("SWITCH");
	case EStatementKind::NestedBlock:
		return TEXT("NESTED_BLOCK");
	default:
		return nullptr;
	}
}

const TCHAR* CountToken(const EStatementCount Count)
{
	switch (Count)
	{
	case EStatementCount::Zero:
		return TEXT("ZERO");
	case EStatementCount::One:
		return TEXT("ONE");
	case EStatementCount::Two:
		return TEXT("TWO");
	case EStatementCount::Many:
		return TEXT("MANY");
	default:
		return nullptr;
	}
}

const TCHAR* TransferToken(const EStatementTransfer Transfer)
{
	switch (Transfer)
	{
	case EStatementTransfer::None:
		return TEXT("NONE");
	case EStatementTransfer::BreakLoop:
		return TEXT("BREAK_LOOP");
	case EStatementTransfer::BreakSwitch:
		return TEXT("BREAK_SWITCH");
	case EStatementTransfer::Continue:
		return TEXT("CONTINUE");
	case EStatementTransfer::EarlyReturn:
		return TEXT("EARLY_RETURN");
	case EStatementTransfer::NestedReturn:
		return TEXT("NESTED_RETURN");
	case EStatementTransfer::Fallthrough:
		return TEXT("FALLTHROUGH");
	case EStatementTransfer::Exception:
		return TEXT("EXCEPTION");
	default:
		return nullptr;
	}
}

const TCHAR* NestingToken(const EStatementNesting Nesting)
{
	switch (Nesting)
	{
	case EStatementNesting::None:
		return TEXT("NONE");
	case EStatementNesting::SameKind:
		return TEXT("SAME_KIND");
	case EStatementNesting::MixedLoop:
		return TEXT("MIXED_LOOP");
	case EStatementNesting::LoopSwitch:
		return TEXT("LOOP_SWITCH");
	case EStatementNesting::BranchLoop:
		return TEXT("BRANCH_LOOP");
	case EStatementNesting::ThreeLevel:
		return TEXT("THREE_LEVEL");
	default:
		return nullptr;
	}
}

// Legacy StatementCases weights (Trace += Weight inside the selected statement body).
int32 StatementWeight(const EStatementKind Statement)
{
	switch (Statement)
	{
	case EStatementKind::If:
		return 1;
	case EStatementKind::IfElse:
		return 2;
	case EStatementKind::ElseIf:
		return 3;
	case EStatementKind::While:
		return 4;
	case EStatementKind::DoWhile:
		return 5;
	case EStatementKind::For:
		return 6;
	case EStatementKind::Switch:
		return 7;
	case EStatementKind::NestedBlock:
		return 8;
	default:
		return 0;
	}
}

// Legacy CountCases weights (loop iteration Limit).
int32 CountLimit(const EStatementCount Count)
{
	switch (Count)
	{
	case EStatementCount::Zero:
		return 0;
	case EStatementCount::One:
		return 1;
	case EStatementCount::Two:
		return 2;
	case EStatementCount::Many:
		return 4;
	default:
		return 0;
	}
}

// Legacy NestingCases weights (Trace += Weight before the nested statement).
int32 NestingWeight(const EStatementNesting Nesting)
{
	switch (Nesting)
	{
	case EStatementNesting::None:
		return 0;
	case EStatementNesting::SameKind:
		return 10;
	case EStatementNesting::MixedLoop:
		return 20;
	case EStatementNesting::LoopSwitch:
		return 30;
	case EStatementNesting::BranchLoop:
		return 40;
	case EStatementNesting::ThreeLevel:
		return 50;
	default:
		return 0;
	}
}

bool IsValidParams(const FStatementTransferParams& Params)
{
	return StatementToken(Params.Statement) != nullptr
		&& CountToken(Params.Count) != nullptr
		&& TransferToken(Params.Transfer) != nullptr
		&& NestingToken(Params.Nesting) != nullptr;
}

// The exception transfer emits a divide-by-zero fault path; the catalog partitions
// every exception cell (across all counts) as a divide_by_zero runtime fault.
bool IsFault(const EStatementTransfer Transfer)
{
	return Transfer == EStatementTransfer::Exception;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FStatementCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CF-STATEMENT-COUNT-TRANSFER-%s-%s-%s-%s"),
		StatementToken(Cell.Statement),
		CountToken(Cell.Count),
		TransferToken(Cell.Transfer),
		NestingToken(Cell.Nesting));
}

FString MakeEntryName(const FStatementCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FStatementTransferParams ParamsFromCell(const FStatementCell& Cell)
{
	FStatementTransferParams Params;
	Params.Statement = Cell.Statement;
	Params.Count = Cell.Count;
	Params.Transfer = Cell.Transfer;
	Params.Nesting = Cell.Nesting;
	return Params;
}

// Ported from AngelscriptNativeStatementTransferTests::AppendStatementBody.
void AppendStatementBody(FString& Source, const EStatementKind Statement, const FString& Indent)
{
	const int32 Weight = StatementWeight(Statement);
	if (Statement == EStatementKind::If)
	{
		AppendLine(Source, Indent + TEXT("if (Index >= 0)"));
		AppendLine(Source, Indent + TEXT("{"));
		AppendLine(Source, Indent + FString::Printf(TEXT("\tTrace += %d;"), Weight));
		AppendLine(Source, Indent + TEXT("}"));
	}
	else if (Statement == EStatementKind::IfElse)
	{
		AppendLine(Source, Indent + TEXT("if ((Index & 1) == 0)"));
		AppendLine(Source, Indent + TEXT("{"));
		AppendLine(Source, Indent + FString::Printf(TEXT("\tTrace += %d;"), Weight));
		AppendLine(Source, Indent + TEXT("}"));
		AppendLine(Source, Indent + TEXT("else"));
		AppendLine(Source, Indent + TEXT("{"));
		AppendLine(Source, Indent + FString::Printf(TEXT("\tTrace += %d;"), Weight));
		AppendLine(Source, Indent + TEXT("}"));
	}
	else if (Statement == EStatementKind::ElseIf)
	{
		AppendLine(Source, Indent + TEXT("if (Index < 0)"));
		AppendLine(Source, Indent + TEXT("{"));
		AppendLine(Source, Indent + TEXT("\tTrace += 1000;"));
		AppendLine(Source, Indent + TEXT("}"));
		AppendLine(Source, Indent + TEXT("else if (Index >= 0)"));
		AppendLine(Source, Indent + TEXT("{"));
		AppendLine(Source, Indent + FString::Printf(TEXT("\tTrace += %d;"), Weight));
		AppendLine(Source, Indent + TEXT("}"));
		AppendLine(Source, Indent + TEXT("else"));
		AppendLine(Source, Indent + TEXT("{"));
		AppendLine(Source, Indent + TEXT("\tTrace += 2000;"));
		AppendLine(Source, Indent + TEXT("}"));
	}
	else if (Statement == EStatementKind::While)
	{
		AppendLine(Source, Indent + TEXT("int Once = 0;"));
		AppendLine(Source, Indent + TEXT("while (Once < 1)"));
		AppendLine(Source, Indent + TEXT("{"));
		AppendLine(Source, Indent + TEXT("\t++Once;"));
		AppendLine(Source, Indent + FString::Printf(TEXT("\tTrace += %d;"), Weight));
		AppendLine(Source, Indent + TEXT("}"));
	}
	else if (Statement == EStatementKind::DoWhile)
	{
		AppendLine(Source, Indent + TEXT("int Once = 0;"));
		AppendLine(Source, Indent + TEXT("do"));
		AppendLine(Source, Indent + TEXT("{"));
		AppendLine(Source, Indent + TEXT("\t++Once;"));
		AppendLine(Source, Indent + FString::Printf(TEXT("\tTrace += %d;"), Weight));
		AppendLine(Source, Indent + TEXT("}"));
		AppendLine(Source, Indent + TEXT("while (Once < 1);"));
	}
	else if (Statement == EStatementKind::For)
	{
		AppendLine(Source, Indent + TEXT("for (int Once = 0; Once < 1; ++Once)"));
		AppendLine(Source, Indent + TEXT("{"));
		AppendLine(Source, Indent + FString::Printf(TEXT("\tTrace += %d;"), Weight));
		AppendLine(Source, Indent + TEXT("}"));
	}
	else if (Statement == EStatementKind::Switch)
	{
		AppendLine(Source, Indent + TEXT("switch (Index & 1)"));
		AppendLine(Source, Indent + TEXT("{"));
		AppendLine(Source, Indent + TEXT("case 0:"));
		AppendLine(Source, Indent + FString::Printf(TEXT("\tTrace += %d;"), Weight));
		AppendLine(Source, Indent + TEXT("\tbreak;"));
		AppendLine(Source, Indent + TEXT("default:"));
		AppendLine(Source, Indent + FString::Printf(TEXT("\tTrace += %d;"), Weight));
		AppendLine(Source, Indent + TEXT("\tbreak;"));
		AppendLine(Source, Indent + TEXT("}"));
	}
	else
	{
		AppendLine(Source, Indent + TEXT("{"));
		AppendLine(Source, Indent + TEXT("\t{"));
		AppendLine(Source, Indent + FString::Printf(TEXT("\t\tTrace += %d;"), Weight));
		AppendLine(Source, Indent + TEXT("\t}"));
		AppendLine(Source, Indent + TEXT("}"));
	}
}

// Ported from AngelscriptNativeStatementTransferTests::AppendNestedStatement.
void AppendNestedStatement(FString& Source, const EStatementKind Statement, const EStatementNesting Nesting)
{
	AppendLine(Source, FString::Printf(TEXT("\t\tTrace += %d;"), NestingWeight(Nesting)));
	if (Nesting == EStatementNesting::None)
	{
		AppendStatementBody(Source, Statement, TEXT("\t\t"));
	}
	else if (Nesting == EStatementNesting::SameKind)
	{
		AppendLine(Source, TEXT("\t\tfor (int SameKind = 0; SameKind < 1; ++SameKind)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendStatementBody(Source, Statement, TEXT("\t\t\t"));
		AppendLine(Source, TEXT("\t\t}"));
	}
	else if (Nesting == EStatementNesting::MixedLoop)
	{
		AppendLine(Source, TEXT("\t\tint MixedLoop = 0;"));
		AppendLine(Source, TEXT("\t\twhile (MixedLoop < 1)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, TEXT("\t\t\t++MixedLoop;"));
		AppendStatementBody(Source, Statement, TEXT("\t\t\t"));
		AppendLine(Source, TEXT("\t\t}"));
	}
	else if (Nesting == EStatementNesting::LoopSwitch)
	{
		AppendLine(Source, TEXT("\t\tswitch (0)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, TEXT("\t\tcase 0:"));
		// The current fork rejects declarations directly under a case label;
		// scope every generated case body so loop/initializer statements remain legal.
		AppendLine(Source, TEXT("\t\t{"));
		AppendStatementBody(Source, Statement, TEXT("\t\t\t"));
		AppendLine(Source, TEXT("\t\t}"));
		AppendLine(Source, TEXT("\t\t\tbreak;"));
		AppendLine(Source, TEXT("\t\tdefault:"));
		AppendLine(Source, TEXT("\t\t\tbreak;"));
		AppendLine(Source, TEXT("\t\t}"));
	}
	else if (Nesting == EStatementNesting::BranchLoop)
	{
		AppendLine(Source, TEXT("\t\tif (Index >= 0)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, TEXT("\t\t\tfor (int BranchLoop = 0; BranchLoop < 1; ++BranchLoop)"));
		AppendLine(Source, TEXT("\t\t\t{"));
		AppendStatementBody(Source, Statement, TEXT("\t\t\t\t"));
		AppendLine(Source, TEXT("\t\t\t}"));
		AppendLine(Source, TEXT("\t\t}"));
	}
	else
	{
		AppendLine(Source, TEXT("\t\tif (Index >= 0)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, TEXT("\t\t\tswitch (0)"));
		AppendLine(Source, TEXT("\t\t\t{"));
		AppendLine(Source, TEXT("\t\t\tcase 0:"));
		// A declaration directly after a case label is rejected by the fork;
		// scope the case body while preserving the same switch/loop nesting.
		AppendLine(Source, TEXT("\t\t\t{"));
		AppendLine(Source, TEXT("\t\t\t\tint ThreeLevel = 0;"));
		AppendLine(Source, TEXT("\t\t\t\twhile (ThreeLevel < 1)"));
		AppendLine(Source, TEXT("\t\t\t\t{"));
		AppendLine(Source, TEXT("\t\t\t\t\t++ThreeLevel;"));
		AppendStatementBody(Source, Statement, TEXT("\t\t\t\t\t"));
		AppendLine(Source, TEXT("\t\t\t\t}"));
		AppendLine(Source, TEXT("\t\t\t\tbreak;"));
		AppendLine(Source, TEXT("\t\t\t}"));
		AppendLine(Source, TEXT("\t\t\tdefault:"));
		AppendLine(Source, TEXT("\t\t\t\tbreak;"));
		AppendLine(Source, TEXT("\t\t\t}"));
		AppendLine(Source, TEXT("\t\t}"));
	}
}

// Ported from AngelscriptNativeStatementTransferTests::AppendTransfer.
void AppendTransfer(FString& Source, const EStatementTransfer Transfer)
{
	if (Transfer == EStatementTransfer::BreakLoop)
	{
		AppendLine(Source, TEXT("\t\tTrace += 10;"));
		AppendLine(Source, TEXT("\t\tbreak;"));
	}
	else if (Transfer == EStatementTransfer::BreakSwitch)
	{
		AppendLine(Source, TEXT("\t\tswitch (0)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, TEXT("\t\tcase 0:"));
		AppendLine(Source, TEXT("\t\t\tTrace += 10;"));
		AppendLine(Source, TEXT("\t\t\tbreak;"));
		AppendLine(Source, TEXT("\t\tdefault:"));
		AppendLine(Source, TEXT("\t\t\tTrace += 1000;"));
		AppendLine(Source, TEXT("\t\t\tbreak;"));
		AppendLine(Source, TEXT("\t\t}"));
		AppendLine(Source, TEXT("\t\tTrace += 1;"));
	}
	else if (Transfer == EStatementTransfer::Continue)
	{
		AppendLine(Source, TEXT("\t\tTrace += 10;"));
		AppendLine(Source, TEXT("\t\tcontinue;"));
	}
	else if (Transfer == EStatementTransfer::EarlyReturn)
	{
		AppendLine(Source, TEXT("\t\treturn Trace + 20;"));
	}
	else if (Transfer == EStatementTransfer::NestedReturn)
	{
		AppendLine(Source, TEXT("\t\tif (Trace >= 0)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, TEXT("\t\t\treturn Trace + 30;"));
		AppendLine(Source, TEXT("\t\t}"));
	}
	else if (Transfer == EStatementTransfer::Fallthrough)
	{
		AppendLine(Source, TEXT("\t\tswitch (0)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, TEXT("\t\tcase 0:"));
		AppendLine(Source, TEXT("\t\t\tTrace += 10;"));
		AppendLine(Source, TEXT("\t\tcase 1:"));
		AppendLine(Source, TEXT("\t\t\tTrace += 1;"));
		AppendLine(Source, TEXT("\t\t\tbreak;"));
		AppendLine(Source, TEXT("\t\tdefault:"));
		AppendLine(Source, TEXT("\t\t\tbreak;"));
		AppendLine(Source, TEXT("\t\t}"));
	}
	else if (Transfer == EStatementTransfer::Exception)
	{
		// The current fork does not recognize string literals in throw(); use a
		// runtime divide-by-zero fault so the transfer path remains executable.
		AppendLine(Source, TEXT("\t\tint ExceptionDivisor = 0;"));
		AppendLine(Source, TEXT("\t\tTrace += 1 / ExceptionDivisor;"));
	}
	else
	{
		AppendLine(Source, TEXT("\t\tTrace += 1;"));
	}
}

// Ported from AngelscriptNativeStatementTransferTests::BuildStatementTransferSource.
void AppendEntry(FString& Source, const FStatementTransferParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Trace = 0;"));
	AppendLine(Source, FString::Printf(TEXT("\tint Limit = %d;"), CountLimit(Params.Count)));
	AppendLine(Source, TEXT("\tfor (int Index = 0; Index < Limit; ++Index)"));
	AppendLine(Source, TEXT("\t{"));
	AppendNestedStatement(Source, Params.Statement, Params.Nesting);
	AppendTransfer(Source, Params.Transfer);
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("\treturn Trace;"));
	AppendLine(Source, TEXT("}"));
}

// Ported from AngelscriptNativeStatementTransferTests::ExpectedResult (normal-return branch only).
// Fault (exception) cells carry no integer expectation and return 0 here.
int32 ExpectedReturnValue(const FStatementCell& Cell)
{
	if (IsFault(Cell.Transfer))
	{
		return 0;
	}
	const int32 Limit = CountLimit(Cell.Count);
	if (Limit == 0)
	{
		return 0;
	}
	const int32 PerIteration = StatementWeight(Cell.Statement) + NestingWeight(Cell.Nesting);
	switch (Cell.Transfer)
	{
	case EStatementTransfer::BreakLoop:
		return PerIteration + 10;
	case EStatementTransfer::EarlyReturn:
		return PerIteration + 20;
	case EStatementTransfer::NestedReturn:
		return PerIteration + 30;
	case EStatementTransfer::BreakSwitch:
	case EStatementTransfer::Fallthrough:
		return Limit * (PerIteration + 11);
	case EStatementTransfer::Continue:
		return Limit * (PerIteration + 10);
	default:
		return Limit * (PerIteration + 1);
	}
}

TArray<FStatementCell> AllCells()
{
	TArray<FStatementCell> Cells;
	Cells.SetNum(TotalCells);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		FStatementCell& Cell = Cells[Index];
		Cell.Statement = static_cast<EStatementKind>(Index / (CountAxisCount * TransferAxisCount * NestingAxisCount));
		const int32 CountRemainder = Index % (CountAxisCount * TransferAxisCount * NestingAxisCount);
		Cell.Count = static_cast<EStatementCount>(CountRemainder / (TransferAxisCount * NestingAxisCount));
		const int32 TransferRemainder = CountRemainder % (TransferAxisCount * NestingAxisCount);
		Cell.Transfer = static_cast<EStatementTransfer>(TransferRemainder / NestingAxisCount);
		Cell.Nesting = static_cast<EStatementNesting>(TransferRemainder % NestingAxisCount);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FStatementCell& OutCell)
{
	const FString Needle(CaseId);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		FStatementCell Cell;
		Cell.Statement = static_cast<EStatementKind>(Index / (CountAxisCount * TransferAxisCount * NestingAxisCount));
		const int32 CountRemainder = Index % (CountAxisCount * TransferAxisCount * NestingAxisCount);
		Cell.Count = static_cast<EStatementCount>(CountRemainder / (TransferAxisCount * NestingAxisCount));
		const int32 TransferRemainder = CountRemainder % (TransferAxisCount * NestingAxisCount);
		Cell.Transfer = static_cast<EStatementTransfer>(TransferRemainder / NestingAxisCount);
		Cell.Nesting = static_cast<EStatementNesting>(TransferRemainder % NestingAxisCount);
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FGeneratedCaseInfo MakeCaseInfo(const FStatementCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Execution = EGeneratedCaseExecution::Standalone;
	if (IsFault(Cell.Transfer))
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.ExpectedException = TEXT("Divide by zero");
	}
	else
	{
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.ExpectedReturn = ExpectedReturnValue(Cell);
	}
	return Info;
}

FString BuildOwnedSource(const FStatementTransferParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}
}

FStringView FStatementTransferGenerator::GetProductId() const
{
	return TEXT("LANG-CF-STATEMENT-COUNT-TRANSFER");
}

TArray<FGeneratedCaseInfo> FStatementTransferGenerator::ListCases() const
{
	using namespace StatementTransferGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FStatementCell& Cell : AllCells())
	{
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("StatementTransfer PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MakeCaseInfo(Cell));
	}
	return Cases;
}

FString FStatementTransferGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace StatementTransferGeneratorDetail;
	FStatementCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FStatementTransferGenerator::BuildDumpSource() const
{
	using namespace StatementTransferGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CF-STATEMENT-COUNT-TRANSFER"));
	AppendLine(Source, TEXT("// Cases: 1536 total, 1344 normal, 0 reject, 192 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));

	for (const FStatementCell& Cell : AllCells())
	{
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		if (IsFault(Cell.Transfer))
		{
			AppendLine(Source, TEXT("// Outcome: RuntimeException"));
			AppendLine(Source, FString::Printf(TEXT("// ExpectedException: %s"), *Info.ExpectedException));
		}
		else
		{
			AppendLine(Source, TEXT("// Outcome: ReturnValue"));
			AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		}
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FStatementTransferGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace StatementTransferGeneratorDetail;
	FString Source;
	int32 Emitted = 0;
	for (const FStatementCell& Cell : AllCells())
	{
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

FString FStatementTransferGenerator::BuildStatementTransferSource(const FStatementTransferParams& Params) const
{
	using namespace StatementTransferGeneratorDetail;
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

int32 FStatementTransferGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace StatementTransferGeneratorDetail;
	FStatementCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsFault(Cell.Transfer))
	{
		return 0;
	}
	return ExpectedReturnValue(Cell);
}
}
