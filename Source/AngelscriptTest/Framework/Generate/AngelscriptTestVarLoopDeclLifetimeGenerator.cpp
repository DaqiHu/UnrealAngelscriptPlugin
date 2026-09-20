#include "Framework/Generate/AngelscriptTestVarLoopDeclLifetimeGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace VarLoopDeclLifetimeGeneratorDetail
{
struct FLoopCell
{
	EVarLoopExit Exit = EVarLoopExit::Normal;
	EVarLoopIteration Iteration = EVarLoopIteration::Zero;
	EVarLoopPlacement Placement = EVarLoopPlacement::ForInitializer;
	EVarLoopType Type = EVarLoopType::ScriptValue;
};

const TCHAR* ExitToken(const EVarLoopExit Exit)
{
	switch (Exit)
	{
	case EVarLoopExit::Normal:
		return TEXT("NORMAL");
	case EVarLoopExit::Break:
		return TEXT("BREAK");
	case EVarLoopExit::Continue:
		return TEXT("CONTINUE");
	case EVarLoopExit::Return:
		return TEXT("RETURN");
	case EVarLoopExit::Exception:
		return TEXT("EXCEPTION");
	default:
		return nullptr;
	}
}

const TCHAR* IterationToken(const EVarLoopIteration Iteration)
{
	switch (Iteration)
	{
	case EVarLoopIteration::Zero:
		return TEXT("ZERO");
	case EVarLoopIteration::One:
		return TEXT("ONE");
	case EVarLoopIteration::Three:
		return TEXT("THREE");
	case EVarLoopIteration::Eight:
		return TEXT("EIGHT");
	default:
		return nullptr;
	}
}

const TCHAR* PlacementToken(const EVarLoopPlacement Placement)
{
	switch (Placement)
	{
	case EVarLoopPlacement::ForInitializer:
		return TEXT("FOR_INITIALIZER");
	case EVarLoopPlacement::ForBody:
		return TEXT("FOR_BODY");
	case EVarLoopPlacement::WhileBody:
		return TEXT("WHILE_BODY");
	case EVarLoopPlacement::DoBody:
		return TEXT("DO_BODY");
	case EVarLoopPlacement::NestedBody:
		return TEXT("NESTED_BODY");
	default:
		return nullptr;
	}
}

const TCHAR* TypeToken(const EVarLoopType Type)
{
	switch (Type)
	{
	case EVarLoopType::ScriptValue:
		return TEXT("SCRIPT_VALUE");
	case EVarLoopType::NativeValue:
		return TEXT("NATIVE_VALUE");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FVarLoopDeclLifetimeParams& Params)
{
	return ExitToken(Params.Exit) != nullptr
		&& IterationToken(Params.Iteration) != nullptr
		&& PlacementToken(Params.Placement) != nullptr
		&& TypeToken(Params.Type) != nullptr;
}

int32 IterationCount(const EVarLoopIteration Iteration)
{
	switch (Iteration)
	{
	case EVarLoopIteration::One:
		return 1;
	case EVarLoopIteration::Three:
		return 3;
	case EVarLoopIteration::Eight:
		return 8;
	default:
		return 0;
	}
}

int32 ReturnBase(const EVarLoopExit Exit)
{
	switch (Exit)
	{
	case EVarLoopExit::Normal:
		return 7000;
	case EVarLoopExit::Break:
		return 7100;
	case EVarLoopExit::Continue:
		return 7200;
	case EVarLoopExit::Return:
		return 7300;
	default:
		return 7400;
	}
}

int32 EventBase(const EVarLoopExit Exit)
{
	switch (Exit)
	{
	case EVarLoopExit::Normal:
		return 3000;
	case EVarLoopExit::Break:
		return 3100;
	case EVarLoopExit::Continue:
		return 3200;
	case EVarLoopExit::Return:
		return 3300;
	default:
		return 3400;
	}
}

const TCHAR* ScriptTypeName(const EVarLoopType Type)
{
	return Type == EVarLoopType::ScriptValue ? TEXT("FScriptLoopValue") : TEXT("FNativeCaseValue");
}

int32 FullBodyCount(const EVarLoopPlacement Placement, const EVarLoopIteration Iteration)
{
	const int32 Count = IterationCount(Iteration);
	return Placement == EVarLoopPlacement::DoBody ? FMath::Max(1, Count) : Count;
}

int32 ExpectedBodyCount(const FLoopCell& Cell)
{
	const int32 FullCount = FullBodyCount(Cell.Placement, Cell.Iteration);
	if (FullCount > 0
		&& (Cell.Exit == EVarLoopExit::Break
			|| Cell.Exit == EVarLoopExit::Return
			|| Cell.Exit == EVarLoopExit::Exception))
	{
		return 1;
	}
	return FullCount;
}

bool IsFault(const FLoopCell& Cell)
{
	return Cell.Exit == EVarLoopExit::Exception && ExpectedBodyCount(Cell) > 0;
}

int32 ExpectedFromCell(const FLoopCell& Cell)
{
	return ReturnBase(Cell.Exit) + ExpectedBodyCount(Cell);
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FLoopCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-VAR-LOOP-DECL-LIFETIME-%s-%s-%s-%s"),
		ExitToken(Cell.Exit),
		IterationToken(Cell.Iteration),
		PlacementToken(Cell.Placement),
		TypeToken(Cell.Type));
}

FString MakeEntryName(const FLoopCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FVarLoopDeclLifetimeParams ParamsFromCell(const FLoopCell& Cell)
{
	FVarLoopDeclLifetimeParams Params;
	Params.Exit = Cell.Exit;
	Params.Iteration = Cell.Iteration;
	Params.Placement = Cell.Placement;
	Params.Type = Cell.Type;
	return Params;
}

TArray<FLoopCell> AllCells()
{
	TArray<FLoopCell> Cells;
	Cells.SetNum(200);
	for (int32 Index = 0; Index < 200; ++Index)
	{
		FLoopCell& Cell = Cells[Index];
		Cell.Exit = static_cast<EVarLoopExit>(Index / 40);
		Cell.Iteration = static_cast<EVarLoopIteration>((Index / 10) % 4);
		Cell.Placement = static_cast<EVarLoopPlacement>((Index / 2) % 5);
		Cell.Type = static_cast<EVarLoopType>(Index % 2);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FLoopCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FLoopCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (MakeCaseId(Cells[Index]) == Needle)
		{
			OutCell = Cells[Index];
			return true;
		}
	}
	return false;
}

void AppendSharedHelpers(FString& Source)
{
	AppendLine(Source, TEXT("int BeginScriptLoopValue(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("void EndScriptLoopValue(int ObjectId, int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("void RecordVariableLoopEvent(int Event)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int AdvanceVariableLoop(int Index)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tRecordVariableLoopEvent(4000 + Index);"));
	AppendLine(Source, TEXT("\treturn Index + 1;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FScriptLoopValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source, TEXT("\tint ObjectId = -1;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFScriptLoopValue(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t\tObjectId = BeginScriptLoopValue(Value);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\t~FScriptLoopValue()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tEndScriptLoopValue(ObjectId, Value);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FNativeCaseValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseValue(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int RaiseVariableLoopException()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Zero = 0;"));
	AppendLine(Source, TEXT("\treturn 1 / Zero;"));
	AppendLine(Source, TEXT("}"));
}

bool UsesManualIncrement(const EVarLoopPlacement Placement)
{
	return Placement == EVarLoopPlacement::WhileBody || Placement == EVarLoopPlacement::DoBody;
}

void AppendOwnerDeclaration(FString& Source, const EVarLoopType Type, const FString& Indent, const FString& ValueExpression)
{
	AppendLine(Source, FString::Printf(
		TEXT("%s%s Owner(100 + %s);"),
		*Indent,
		ScriptTypeName(Type),
		*ValueExpression));
}

void AppendBodyPrelude(
	FString& Source,
	const EVarLoopType Type,
	const EVarLoopPlacement Placement,
	const FString& Indent)
{
	AppendLine(Source, Indent + TEXT("++BodyCount;"));
	AppendLine(Source, Indent + TEXT("RecordVariableLoopEvent(1000 + Index);"));
	if (Placement != EVarLoopPlacement::ForInitializer)
	{
		AppendOwnerDeclaration(Source, Type, Indent, TEXT("Index"));
	}
	AppendLine(Source, Indent + TEXT("BodyCount += Owner.Value - Owner.Value;"));
}

void AppendTransfer(
	FString& Source,
	const EVarLoopExit Exit,
	const EVarLoopPlacement Placement,
	const FString& Indent)
{
	AppendLine(Source, FString::Printf(TEXT("%sRecordVariableLoopEvent(%d + Index);"), *Indent, EventBase(Exit)));
	if (Exit == EVarLoopExit::Break)
	{
		AppendLine(Source, Indent + TEXT("break;"));
	}
	else if (Exit == EVarLoopExit::Continue)
	{
		if (UsesManualIncrement(Placement))
		{
			AppendLine(Source, Indent + TEXT("Index = AdvanceVariableLoop(Index);"));
		}
		AppendLine(Source, Indent + TEXT("continue;"));
	}
	else if (Exit == EVarLoopExit::Return)
	{
		AppendLine(Source, FString::Printf(TEXT("%sreturn %d + BodyCount;"), *Indent, ReturnBase(Exit)));
	}
	else if (Exit == EVarLoopExit::Exception)
	{
		AppendLine(Source, Indent + TEXT("BodyCount += RaiseVariableLoopException();"));
	}
}

void AppendLoopBody(FString& Source, const FVarLoopDeclLifetimeParams& Params)
{
	const int32 Count = IterationCount(Params.Iteration);
	const TCHAR* TypeName = ScriptTypeName(Params.Type);
	if (Params.Placement == EVarLoopPlacement::ForInitializer)
	{
		AppendLine(Source, FString::Printf(TEXT("\tint Limit = %d;"), Count));
		AppendLine(Source, TEXT("\tint Index = 0;"));
		AppendLine(Source, TEXT("\tint BodyCount = 0;"));
		AppendLine(Source, FString::Printf(
			TEXT("\tfor (%s Owner(100); Index < Limit; Index = AdvanceVariableLoop(Index))"),
			TypeName));
		AppendLine(Source, TEXT("\t{"));
		AppendBodyPrelude(Source, Params.Type, Params.Placement, TEXT("\t\t"));
		AppendTransfer(Source, Params.Exit, Params.Placement, TEXT("\t\t"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %d + BodyCount;"), ReturnBase(Params.Exit)));
		return;
	}
	if (Params.Placement == EVarLoopPlacement::WhileBody)
	{
		AppendLine(Source, FString::Printf(TEXT("\tint Limit = %d;"), Count));
		AppendLine(Source, TEXT("\tint Index = 0;"));
		AppendLine(Source, TEXT("\tint BodyCount = 0;"));
		AppendLine(Source, TEXT("\twhile (Index < Limit)"));
		AppendLine(Source, TEXT("\t{"));
		AppendBodyPrelude(Source, Params.Type, Params.Placement, TEXT("\t\t"));
		AppendTransfer(Source, Params.Exit, Params.Placement, TEXT("\t\t"));
		if (Params.Exit == EVarLoopExit::Normal)
		{
			AppendLine(Source, TEXT("\t\tIndex = AdvanceVariableLoop(Index);"));
		}
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %d + BodyCount;"), ReturnBase(Params.Exit)));
		return;
	}
	if (Params.Placement == EVarLoopPlacement::DoBody)
	{
		AppendLine(Source, FString::Printf(TEXT("\tint Limit = %d;"), Count));
		AppendLine(Source, TEXT("\tint Index = 0;"));
		AppendLine(Source, TEXT("\tint BodyCount = 0;"));
		AppendLine(Source, TEXT("\tdo"));
		AppendLine(Source, TEXT("\t{"));
		AppendBodyPrelude(Source, Params.Type, Params.Placement, TEXT("\t\t"));
		AppendTransfer(Source, Params.Exit, Params.Placement, TEXT("\t\t"));
		if (Params.Exit == EVarLoopExit::Normal)
		{
			AppendLine(Source, TEXT("\t\tIndex = AdvanceVariableLoop(Index);"));
		}
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\twhile (Index < Limit);"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %d + BodyCount;"), ReturnBase(Params.Exit)));
		return;
	}

	const bool bNested = Params.Placement == EVarLoopPlacement::NestedBody;
	AppendLine(Source, FString::Printf(TEXT("\tint Limit = %d;"), Count));
	AppendLine(Source, TEXT("\tint Index = 0;"));
	AppendLine(Source, TEXT("\tint BodyCount = 0;"));
	AppendLine(Source, TEXT("\tfor (; Index < Limit; Index = AdvanceVariableLoop(Index))"));
	AppendLine(Source, TEXT("\t{"));
	if (bNested)
	{
		AppendLine(Source, TEXT("\t\t{"));
	}
	const FString Indent = bNested ? FString(TEXT("\t\t\t")) : FString(TEXT("\t\t"));
	AppendBodyPrelude(Source, Params.Type, Params.Placement, Indent);
	AppendTransfer(Source, Params.Exit, Params.Placement, Indent);
	if (bNested)
	{
		AppendLine(Source, TEXT("\t\t}"));
	}
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, FString::Printf(TEXT("\treturn %d + BodyCount;"), ReturnBase(Params.Exit)));
}

void AppendEntry(FString& Source, const FVarLoopDeclLifetimeParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLoopBody(Source, Params);
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FVarLoopDeclLifetimeParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FLoopCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	if (IsFault(Cell))
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.Execution = EGeneratedCaseExecution::Standalone;
		Info.ExpectedException = TEXT("Divide by zero");
		return Info;
	}
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	return Info;
}
}

FStringView FVarLoopDeclLifetimeGenerator::GetProductId() const
{
	return TEXT("LANG-VAR-LOOP-DECL-LIFETIME");
}

TArray<FGeneratedCaseInfo> FVarLoopDeclLifetimeGenerator::ListCases() const
{
	using namespace VarLoopDeclLifetimeGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FLoopCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FString EntryName = MakeEntryName(Cells[Index]);
		checkf(!EntryNames.Contains(EntryName), TEXT("VarLoopDeclLifetime PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MakeCaseInfo(Cells[Index]));
	}
	return Cases;
}

FString FVarLoopDeclLifetimeGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace VarLoopDeclLifetimeGeneratorDetail;
	FLoopCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FVarLoopDeclLifetimeGenerator::BuildDumpSource() const
{
	using namespace VarLoopDeclLifetimeGeneratorDetail;
	const TArray<FLoopCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-VAR-LOOP-DECL-LIFETIME"));
	AppendLine(Source, TEXT("// Cases: 200 total, 168 normal, 0 reject, 32 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FLoopCell& Cell = Cells[Index];
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		if (IsFault(Cell))
		{
			AppendLine(Source, TEXT("// Outcome: RuntimeException"));
			AppendLine(Source, TEXT("// ExpectedException: Divide by zero"));
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

FString FVarLoopDeclLifetimeGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace VarLoopDeclLifetimeGeneratorDetail;
	const TArray<FLoopCell> Cells = AllCells();
	OutCaseCount = Cells.Num();
	FString Source;
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

FString FVarLoopDeclLifetimeGenerator::BuildVariableLoopSource(const FVarLoopDeclLifetimeParams& Params) const
{
	using namespace VarLoopDeclLifetimeGeneratorDetail;
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

int32 FVarLoopDeclLifetimeGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace VarLoopDeclLifetimeGeneratorDetail;
	FLoopCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsFault(Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}
}
