#include "Framework/Generate/AngelscriptTestLiveLocalCleanupGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace LiveLocalCleanupGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native BeginNativeScriptLifecycle and EndNativeScriptLifecycle.");

struct FLiveLocalCell
{
	ELiveLocalScope Scope = ELiveLocalScope::Loop;
	ELiveLocalExit Exit = ELiveLocalExit::Normal;
	ELiveLocalDepth Depth = ELiveLocalDepth::One;
	ELiveLocalCount LocalCount = ELiveLocalCount::One;
	ELiveLocalLineEnding LineEnding = ELiveLocalLineEnding::Lf;
};

const TCHAR* ScopeToken(const ELiveLocalScope Scope)
{
	switch (Scope)
	{
	case ELiveLocalScope::Loop:
		return TEXT("LOOP");
	case ELiveLocalScope::BranchLoop:
		return TEXT("BRANCH_LOOP");
	case ELiveLocalScope::NestedLoop:
		return TEXT("NESTED_LOOP");
	case ELiveLocalScope::SwitchLoop:
		return TEXT("SWITCH_LOOP");
	default:
		return nullptr;
	}
}

const TCHAR* ExitToken(const ELiveLocalExit Exit)
{
	switch (Exit)
	{
	case ELiveLocalExit::Normal:
		return TEXT("NORMAL");
	case ELiveLocalExit::Break:
		return TEXT("BREAK");
	case ELiveLocalExit::Continue:
		return TEXT("CONTINUE");
	case ELiveLocalExit::Return:
		return TEXT("RETURN");
	case ELiveLocalExit::Exception:
		return TEXT("EXCEPTION");
	default:
		return nullptr;
	}
}

const TCHAR* DepthToken(const ELiveLocalDepth Depth)
{
	switch (Depth)
	{
	case ELiveLocalDepth::One:
		return TEXT("ONE");
	case ELiveLocalDepth::Two:
		return TEXT("TWO");
	case ELiveLocalDepth::Three:
		return TEXT("THREE");
	default:
		return nullptr;
	}
}

const TCHAR* CountToken(const ELiveLocalCount Count)
{
	switch (Count)
	{
	case ELiveLocalCount::One:
		return TEXT("ONE");
	case ELiveLocalCount::Two:
		return TEXT("TWO");
	default:
		return nullptr;
	}
}

const TCHAR* LineEndingToken(const ELiveLocalLineEnding LineEnding)
{
	switch (LineEnding)
	{
	case ELiveLocalLineEnding::Lf:
		return TEXT("LF");
	case ELiveLocalLineEnding::Crlf:
		return TEXT("CRLF");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FLiveLocalCleanupParams& Params)
{
	return ScopeToken(Params.Scope) != nullptr
		&& ExitToken(Params.Exit) != nullptr
		&& DepthToken(Params.Depth) != nullptr
		&& CountToken(Params.LocalCount) != nullptr
		&& LineEndingToken(Params.LineEnding) != nullptr;
}

int32 DepthValue(const ELiveLocalDepth Depth)
{
	return static_cast<int32>(Depth) + 1;
}

int32 LocalCountValue(const ELiveLocalCount Count)
{
	return static_cast<int32>(Count) + 1;
}

int32 MakeLocalValue(const int32 Level, const int32 Slot)
{
	return (Level + 1) * 100 + Slot + 1;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

void ApplyLineEnding(FString& Source, const ELiveLocalLineEnding LineEnding)
{
	if (LineEnding == ELiveLocalLineEnding::Crlf)
	{
		Source.ReplaceInline(TEXT("\n"), TEXT("\r\n"));
	}
}

FString MakeCaseId(const FLiveLocalCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CF-LIVE-LOCAL-CLEANUP-%s-%s-%s-%s-%s"),
		ScopeToken(Cell.Scope),
		ExitToken(Cell.Exit),
		DepthToken(Cell.Depth),
		CountToken(Cell.LocalCount),
		LineEndingToken(Cell.LineEnding));
}

FString MakeEntryName(const FLiveLocalCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FLiveLocalCleanupParams ParamsFromCell(const FLiveLocalCell& Cell)
{
	FLiveLocalCleanupParams Params;
	Params.Scope = Cell.Scope;
	Params.Exit = Cell.Exit;
	Params.Depth = Cell.Depth;
	Params.LocalCount = Cell.LocalCount;
	Params.LineEnding = Cell.LineEnding;
	return Params;
}

struct FTraceResult
{
	int32 Value = 0;
	bool bReturned = false;
};

FTraceResult SimulateNestedScopes(const int32 CurrentLevel, const int32 Depth, const ELiveLocalExit Exit)
{
	FTraceResult Result;
	Result.Value += (CurrentLevel + 1) * 100;
	if (CurrentLevel + 1 == Depth)
	{
		if (Exit == ELiveLocalExit::Return)
		{
			Result.Value += 13;
			Result.bReturned = true;
			return Result;
		}
		if (Exit == ELiveLocalExit::Break)
		{
			Result.Value += 7;
		}
		else if (Exit == ELiveLocalExit::Continue)
		{
			Result.Value += 11;
		}
		else if (Exit == ELiveLocalExit::Exception)
		{
			Result.Value += 17;
			return Result;
		}
	}
	else
	{
		const FTraceResult Nested = SimulateNestedScopes(CurrentLevel + 1, Depth, Exit);
		Result.Value += Nested.Value;
		if (Nested.bReturned)
		{
			Result.bReturned = true;
			return Result;
		}
	}
	Result.Value += (CurrentLevel + 1) * 1000;
	return Result;
}

int32 ExpectedTrace(const FLiveLocalCell& Cell)
{
	if (Cell.Exit == ELiveLocalExit::Exception)
	{
		return 0;
	}
	const FTraceResult Nested = SimulateNestedScopes(0, DepthValue(Cell.Depth), Cell.Exit);
	return Nested.bReturned ? Nested.Value : Nested.Value + 97;
}

TArray<FLiveLocalCell> AllCells()
{
	TArray<FLiveLocalCell> Cells;
	Cells.SetNum(120);
	for (int32 Index = 0; Index < 120; ++Index)
	{
		FLiveLocalCell& Cell = Cells[Index];
		Cell.Scope = static_cast<ELiveLocalScope>(Index / 30);
		Cell.Exit = static_cast<ELiveLocalExit>((Index / 6) % 5);
		Cell.Depth = static_cast<ELiveLocalDepth>((Index / 2) % 3);
		Cell.LocalCount = static_cast<ELiveLocalCount>(Index % 2);
		Cell.LineEnding = ELiveLocalLineEnding::Lf;
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FLiveLocalCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FLiveLocalCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendProbeDeclarations(FString& Source)
{
	AppendLine(Source, TEXT("struct FScopedLifetimeProbe"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source, TEXT("\tint ObjectId = 0;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFScopedLifetimeProbe(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Value);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\t~FScopedLifetimeProbe()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tEndNativeScriptLifecycle(ObjectId, Value);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendLocalDeclarations(FString& Source, const int32 Level, const int32 LocalCount, const FString& Indent)
{
	for (int32 Slot = 0; Slot < LocalCount; ++Slot)
	{
		AppendLine(Source, FString::Printf(
			TEXT("%sFScopedLifetimeProbe Local_%d_%d(%d);"),
			*Indent,
			Level + 1,
			Slot + 1,
			MakeLocalValue(Level, Slot)));
	}
}

void AppendDeepestExit(FString& Source, const ELiveLocalExit Exit, const FString& Indent)
{
	if (Exit == ELiveLocalExit::Break)
	{
		AppendLine(Source, Indent + TEXT("Trace += 7;"));
		AppendLine(Source, Indent + TEXT("break;"));
	}
	else if (Exit == ELiveLocalExit::Continue)
	{
		AppendLine(Source, Indent + TEXT("Trace += 11;"));
		AppendLine(Source, Indent + TEXT("continue;"));
	}
	else if (Exit == ELiveLocalExit::Return)
	{
		AppendLine(Source, Indent + TEXT("Trace += 13;"));
		AppendLine(Source, Indent + TEXT("return Trace;"));
	}
	else if (Exit == ELiveLocalExit::Exception)
	{
		AppendLine(Source, Indent + TEXT("Trace += 17;"));
		AppendLine(Source, Indent + TEXT("int ExceptionDivisor = 0;"));
		AppendLine(Source, Indent + TEXT("Trace += 1 / ExceptionDivisor;"));
	}
}

void AppendNestedScopes(
	FString& Source,
	const int32 CurrentLevel,
	const int32 Depth,
	const int32 LocalCount,
	const ELiveLocalExit Exit,
	const FString& Indent)
{
	AppendLine(Source, FString::Printf(
		TEXT("%sfor (int Level%d = 0; Level%d < 1; ++Level%d)"),
		*Indent,
		CurrentLevel + 1,
		CurrentLevel + 1,
		CurrentLevel + 1));
	AppendLine(Source, Indent + TEXT("{"));
	const FString BodyIndent = Indent + TEXT("\t");
	AppendLocalDeclarations(Source, CurrentLevel, LocalCount, BodyIndent);
	AppendLine(Source, FString::Printf(TEXT("%sTrace += %d;"), *BodyIndent, (CurrentLevel + 1) * 100));
	if (CurrentLevel + 1 == Depth)
	{
		AppendDeepestExit(Source, Exit, BodyIndent);
	}
	else
	{
		AppendNestedScopes(Source, CurrentLevel + 1, Depth, LocalCount, Exit, BodyIndent);
	}
	AppendLine(Source, Indent + TEXT("}"));
	AppendLine(Source, FString::Printf(TEXT("%sTrace += %d;"), *Indent, (CurrentLevel + 1) * 1000));
}

void AppendShapeWrapper(
	FString& Source,
	const ELiveLocalScope Scope,
	const int32 Depth,
	const int32 LocalCount,
	const ELiveLocalExit Exit)
{
	if (Scope == ELiveLocalScope::Loop)
	{
		AppendNestedScopes(Source, 0, Depth, LocalCount, Exit, TEXT("\t"));
		return;
	}
	if (Scope == ELiveLocalScope::BranchLoop)
	{
		AppendLine(Source, TEXT("\tif (true)"));
		AppendLine(Source, TEXT("\t{"));
		AppendNestedScopes(Source, 0, Depth, LocalCount, Exit, TEXT("\t\t"));
		AppendLine(Source, TEXT("\t}"));
		return;
	}
	if (Scope == ELiveLocalScope::NestedLoop)
	{
		AppendLine(Source, TEXT("\tfor (int ShapeLoop = 0; ShapeLoop < 1; ++ShapeLoop)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tif (ShapeLoop >= 0)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendNestedScopes(Source, 0, Depth, LocalCount, Exit, TEXT("\t\t\t"));
		AppendLine(Source, TEXT("\t\t}"));
		AppendLine(Source, TEXT("\t}"));
		return;
	}
	AppendLine(Source, TEXT("\tswitch (0)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\tcase 0:"));
	AppendLine(Source, TEXT("\t{"));
	AppendNestedScopes(Source, 0, Depth, LocalCount, Exit, TEXT("\t\t"));
	AppendLine(Source, TEXT("\t\tbreak;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("\tdefault:"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tbreak;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("\t}"));
}

void AppendRecovery(FString& Source)
{
	AppendLine(Source, TEXT("int Recovery()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 97;"));
	AppendLine(Source, TEXT("}"));
}

void AppendEntry(FString& Source, const FLiveLocalCleanupParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Trace = 0;"));
	AppendShapeWrapper(Source, Params.Scope, DepthValue(Params.Depth), LocalCountValue(Params.LocalCount), Params.Exit);
	AppendLine(Source, TEXT("\treturn Trace + 97;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FLiveLocalCleanupParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendProbeDeclarations(Source);
	AppendEntry(Source, Params, FunctionName);
	AppendLine(Source);
	AppendRecovery(Source);
	ApplyLineEnding(Source, Params.LineEnding);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FLiveLocalCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExecutionNotes = HostNotes;
	if (Cell.Exit == ELiveLocalExit::Exception)
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.ExpectedException = TEXT("Divide by zero");
	}
	else
	{
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.ExpectedReturn = ExpectedTrace(Cell);
	}
	return Info;
}
}

FStringView FLiveLocalCleanupGenerator::GetProductId() const
{
	return TEXT("LANG-CF-LIVE-LOCAL-CLEANUP");
}

TArray<FGeneratedCaseInfo> FLiveLocalCleanupGenerator::ListCases() const
{
	using namespace LiveLocalCleanupGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FLiveLocalCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("LiveLocalCleanup PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FLiveLocalCleanupGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace LiveLocalCleanupGeneratorDetail;
	FLiveLocalCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FLiveLocalCleanupGenerator::BuildDumpSource() const
{
	using namespace LiveLocalCleanupGeneratorDetail;
	const TArray<FLiveLocalCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CF-LIVE-LOCAL-CLEANUP"));
	AppendLine(Source, TEXT("// Cases: 120 total, 96 normal, 0 reject, 24 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendProbeDeclarations(Source);
	AppendRecovery(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FLiveLocalCell& Cell = Cells[Index];
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		if (Cell.Exit == ELiveLocalExit::Exception)
		{
			AppendLine(Source, TEXT("// Outcome: RuntimeException"));
			AppendLine(Source, TEXT("// ExpectedException: Divide by zero"));
		}
		else
		{
			AppendLine(Source, TEXT("// Outcome: ReturnValue"));
			AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		}
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FLiveLocalCleanupGenerator::BuildLiveLocalSource(const FLiveLocalCleanupParams& Params) const
{
	using namespace LiveLocalCleanupGeneratorDetail;
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

FString FLiveLocalCleanupGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace LiveLocalCleanupGeneratorDetail;
	const TArray<FLiveLocalCell> Cells = AllCells();
	OutCaseCount = Cells.Num();
	FString Source;
	AppendProbeDeclarations(Source);
	AppendRecovery(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

int32 FLiveLocalCleanupGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace LiveLocalCleanupGeneratorDetail;
	FLiveLocalCell Cell;
	if (!TryFindCell(CaseId, Cell) || Cell.Exit == ELiveLocalExit::Exception)
	{
		return 0;
	}
	return ExpectedTrace(Cell);
}
}
