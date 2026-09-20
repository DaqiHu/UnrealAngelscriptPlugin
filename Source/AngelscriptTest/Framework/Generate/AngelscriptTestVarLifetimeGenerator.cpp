#include "Framework/Generate/AngelscriptTestVarLifetimeGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace VarLifetimeGeneratorDetail
{
struct FLifetimeCell
{
	EVarLifetimeExit Exit = EVarLifetimeExit::BlockEnd;
	EVarLifetimeNesting Nesting = EVarLifetimeNesting::One;
	EVarLifetimeOwner Owner = EVarLifetimeOwner::LocalValue;
};

const TCHAR* ExitToken(const EVarLifetimeExit Exit)
{
	switch (Exit)
	{
	case EVarLifetimeExit::BlockEnd:
		return TEXT("BLOCK_END");
	case EVarLifetimeExit::Return:
		return TEXT("RETURN");
	case EVarLifetimeExit::Break:
		return TEXT("BREAK");
	case EVarLifetimeExit::Continue:
		return TEXT("CONTINUE");
	case EVarLifetimeExit::Exception:
		return TEXT("EXCEPTION");
	default:
		return nullptr;
	}
}

const TCHAR* NestingToken(const EVarLifetimeNesting Nesting)
{
	switch (Nesting)
	{
	case EVarLifetimeNesting::One:
		return TEXT("ONE");
	case EVarLifetimeNesting::Sequential:
		return TEXT("SEQUENTIAL");
	case EVarLifetimeNesting::NestedScopes:
		return TEXT("NESTED_SCOPES");
	case EVarLifetimeNesting::Loop:
		return TEXT("LOOP");
	case EVarLifetimeNesting::NestedCall:
		return TEXT("NESTED_CALL");
	default:
		return nullptr;
	}
}

const TCHAR* OwnerToken(const EVarLifetimeOwner Owner)
{
	switch (Owner)
	{
	case EVarLifetimeOwner::LocalValue:
		return TEXT("LOCAL_VALUE");
	case EVarLifetimeOwner::NestedValue:
		return TEXT("NESTED_VALUE");
	case EVarLifetimeOwner::Field:
		return TEXT("FIELD");
	case EVarLifetimeOwner::Reference:
		return TEXT("REFERENCE");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FVarLifetimeParams& Params)
{
	return ExitToken(Params.Exit) != nullptr
		&& NestingToken(Params.Nesting) != nullptr
		&& OwnerToken(Params.Owner) != nullptr;
}

bool IsFault(const EVarLifetimeExit Exit)
{
	return Exit == EVarLifetimeExit::Exception;
}

int32 ExpectedFromExit(const EVarLifetimeExit Exit)
{
	switch (Exit)
	{
	case EVarLifetimeExit::BlockEnd:
		return 71;
	case EVarLifetimeExit::Return:
		return 72;
	case EVarLifetimeExit::Break:
		return 73;
	case EVarLifetimeExit::Continue:
		return 74;
	default:
		return 0;
	}
}

int32 OwnerSeed(const EVarLifetimeOwner Owner)
{
	switch (Owner)
	{
	case EVarLifetimeOwner::LocalValue:
		return 110;
	case EVarLifetimeOwner::NestedValue:
		return 210;
	case EVarLifetimeOwner::Field:
		return 310;
	default:
		return 410;
	}
}

const TCHAR* OwnerCatalog(const EVarLifetimeOwner Owner)
{
	switch (Owner)
	{
	case EVarLifetimeOwner::LocalValue:
		return TEXT("local_value");
	case EVarLifetimeOwner::NestedValue:
		return TEXT("nested_value");
	case EVarLifetimeOwner::Field:
		return TEXT("field");
	default:
		return TEXT("reference");
	}
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FLifetimeCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-VAR-LIFETIME-%s-%s-%s"),
		ExitToken(Cell.Exit),
		NestingToken(Cell.Nesting),
		OwnerToken(Cell.Owner));
}

FString MakeEntryName(const FLifetimeCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FVarLifetimeParams ParamsFromCell(const FLifetimeCell& Cell)
{
	FVarLifetimeParams Params;
	Params.Exit = Cell.Exit;
	Params.Nesting = Cell.Nesting;
	Params.Owner = Cell.Owner;
	return Params;
}

TArray<FLifetimeCell> AllCells()
{
	TArray<FLifetimeCell> Cells;
	Cells.SetNum(100);
	for (int32 Index = 0; Index < 100; ++Index)
	{
		FLifetimeCell& Cell = Cells[Index];
		Cell.Exit = static_cast<EVarLifetimeExit>(Index / 20);
		Cell.Nesting = static_cast<EVarLifetimeNesting>((Index / 4) % 5);
		Cell.Owner = static_cast<EVarLifetimeOwner>(Index % 4);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FLifetimeCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FLifetimeCell> Cells = AllCells();
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
	AppendLine(Source, TEXT("struct FNativeCaseValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseValue()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = 0;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseValue(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FNativeCaseReference"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseReference(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FNativeCaseReference CreateNativeCaseReference(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn FNativeCaseReference(Value);"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FNestedLifetimeValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNestedLifetimeValue()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FFieldLifetimeOwner"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseValue Tracked;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFFieldLifetimeOwner()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int RaiseVariableLifetimeException()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Zero = 0;"));
	AppendLine(Source, TEXT("\treturn 1 / Zero;"));
	AppendLine(Source, TEXT("}"));
}

FString MakeOwnerVariableName(const EVarLifetimeOwner Owner, const int32 Ordinal)
{
	return FString::Printf(TEXT("%sOwner%d"), OwnerCatalog(Owner), Ordinal);
}

void AppendOwnerDeclaration(
	FString& Source,
	const EVarLifetimeOwner Owner,
	const int32 Ordinal,
	const FString& Indent)
{
	const FString VariableName = MakeOwnerVariableName(Owner, Ordinal);
	const int32 Value = OwnerSeed(Owner) + Ordinal;
	if (Owner == EVarLifetimeOwner::LocalValue)
	{
		AppendLine(Source, FString::Printf(TEXT("%sFNativeCaseValue %s(%d);"), *Indent, *VariableName, Value));
	}
	else if (Owner == EVarLifetimeOwner::NestedValue)
	{
		AppendLine(Source, FString::Printf(TEXT("%sFNestedLifetimeValue %s;"), *Indent, *VariableName));
		AppendLine(Source, FString::Printf(TEXT("%s%s.Tracked.Value = %d;"), *Indent, *VariableName, Value));
	}
	else if (Owner == EVarLifetimeOwner::Field)
	{
		AppendLine(Source, FString::Printf(
			TEXT("%sFFieldLifetimeOwner %s = FFieldLifetimeOwner();"),
			*Indent,
			*VariableName));
		AppendLine(Source, FString::Printf(TEXT("%s%s.Tracked.Value = %d;"), *Indent, *VariableName, Value));
	}
	else
	{
		AppendLine(Source, FString::Printf(
			TEXT("%sFNativeCaseReference %s = CreateNativeCaseReference(%d);"),
			*Indent,
			*VariableName,
			Value));
	}
}

void AppendOwnerObservation(
	FString& Source,
	const EVarLifetimeOwner Owner,
	const int32 Ordinal,
	const FString& Indent)
{
	const FString VariableName = MakeOwnerVariableName(Owner, Ordinal);
	const TCHAR* Access = (Owner == EVarLifetimeOwner::LocalValue || Owner == EVarLifetimeOwner::Reference)
		? TEXT(".Value")
		: TEXT(".Tracked.Value");
	AppendLine(Source, FString::Printf(TEXT("%sTrace += %s%s;"), *Indent, *VariableName, Access));
}

void AppendOwnerAndObservation(
	FString& Source,
	const EVarLifetimeOwner Owner,
	const int32 Ordinal,
	const FString& Indent)
{
	AppendOwnerDeclaration(Source, Owner, Ordinal, Indent);
	AppendOwnerObservation(Source, Owner, Ordinal, Indent);
}

void AppendTransfer(FString& Source, const EVarLifetimeExit Exit, const int32 ReturnValue, const FString& Indent)
{
	if (Exit == EVarLifetimeExit::Return)
	{
		AppendLine(Source, FString::Printf(TEXT("%sreturn %d + Trace - Trace;"), *Indent, ReturnValue));
	}
	else if (Exit == EVarLifetimeExit::Break)
	{
		AppendLine(Source, Indent + TEXT("break;"));
	}
	else if (Exit == EVarLifetimeExit::Continue)
	{
		AppendLine(Source, Indent + TEXT("continue;"));
	}
	else if (Exit == EVarLifetimeExit::Exception)
	{
		AppendLine(Source, Indent + TEXT("Trace += RaiseVariableLifetimeException();"));
	}
}

void AppendNestedCallFunction(FString& Source, const FVarLifetimeParams& Params, const FString& NestedName)
{
	const int32 ReturnValue = ExpectedFromExit(Params.Exit);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *NestedName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Trace = 0;"));
	AppendLine(Source, TEXT("\tfor (int TransferLoop = 0; TransferLoop < 1; ++TransferLoop)"));
	AppendLine(Source, TEXT("\t{"));
	AppendOwnerAndObservation(Source, Params.Owner, 1, TEXT("\t\t"));
	AppendTransfer(Source, Params.Exit, ReturnValue, TEXT("\t\t"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, FString::Printf(TEXT("\treturn %d + Trace - Trace;"), ReturnValue));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendEntry(FString& Source, const FVarLifetimeParams& Params, const FString& FunctionName)
{
	const int32 ReturnValue = ExpectedFromExit(Params.Exit);
	if (Params.Nesting == EVarLifetimeNesting::NestedCall)
	{
		AppendNestedCallFunction(Source, Params, TEXT("Nested") + FunctionName);
	}

	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Trace = 0;"));
	if (Params.Nesting == EVarLifetimeNesting::One)
	{
		AppendLine(Source, TEXT("\tfor (int TransferLoop = 0; TransferLoop < 1; ++TransferLoop)"));
		AppendLine(Source, TEXT("\t{"));
		AppendOwnerAndObservation(Source, Params.Owner, 0, TEXT("\t\t"));
		AppendTransfer(Source, Params.Exit, ReturnValue, TEXT("\t\t"));
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Params.Nesting == EVarLifetimeNesting::Sequential)
	{
		AppendLine(Source, TEXT("\tfor (int TransferLoop = 0; TransferLoop < 1; ++TransferLoop)"));
		AppendLine(Source, TEXT("\t{"));
		AppendOwnerAndObservation(Source, Params.Owner, 0, TEXT("\t\t"));
		AppendOwnerAndObservation(Source, Params.Owner, 1, TEXT("\t\t"));
		AppendTransfer(Source, Params.Exit, ReturnValue, TEXT("\t\t"));
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Params.Nesting == EVarLifetimeNesting::NestedScopes)
	{
		AppendLine(Source, TEXT("\tfor (int TransferLoop = 0; TransferLoop < 1; ++TransferLoop)"));
		AppendLine(Source, TEXT("\t{"));
		AppendOwnerAndObservation(Source, Params.Owner, 0, TEXT("\t\t"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendOwnerAndObservation(Source, Params.Owner, 1, TEXT("\t\t\t"));
		AppendTransfer(Source, Params.Exit, ReturnValue, TEXT("\t\t\t"));
		AppendLine(Source, TEXT("\t\t}"));
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Params.Nesting == EVarLifetimeNesting::Loop)
	{
		AppendLine(Source, TEXT("\tfor (int OwnerIteration = 0; OwnerIteration < 2; ++OwnerIteration)"));
		AppendLine(Source, TEXT("\t{"));
		AppendOwnerAndObservation(Source, Params.Owner, 0, TEXT("\t\t"));
		AppendTransfer(Source, Params.Exit, ReturnValue, TEXT("\t\t"));
		AppendLine(Source, TEXT("\t}"));
	}
	else
	{
		AppendOwnerAndObservation(Source, Params.Owner, 0, TEXT("\t"));
		AppendLine(Source, FString::Printf(TEXT("\tint NestedResult = Nested%s();"), *FunctionName));
		AppendLine(Source, TEXT("\treturn NestedResult + Trace - Trace;"));
		AppendLine(Source, TEXT("}"));
		return;
	}
	AppendLine(Source, FString::Printf(TEXT("\treturn %d + Trace - Trace;"), ReturnValue));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FVarLifetimeParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FLifetimeCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	if (IsFault(Cell.Exit))
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.Execution = EGeneratedCaseExecution::Standalone;
		Info.ExpectedException = TEXT("Divide by zero");
		return Info;
	}
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = ExpectedFromExit(Cell.Exit);
	return Info;
}
}

FStringView FVarLifetimeGenerator::GetProductId() const
{
	return TEXT("LANG-VAR-LIFETIME");
}

TArray<FGeneratedCaseInfo> FVarLifetimeGenerator::ListCases() const
{
	using namespace VarLifetimeGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FLifetimeCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FString EntryName = MakeEntryName(Cells[Index]);
		checkf(!EntryNames.Contains(EntryName), TEXT("VarLifetime PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MakeCaseInfo(Cells[Index]));
	}
	return Cases;
}

FString FVarLifetimeGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace VarLifetimeGeneratorDetail;
	FLifetimeCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FVarLifetimeGenerator::BuildDumpSource() const
{
	using namespace VarLifetimeGeneratorDetail;
	const TArray<FLifetimeCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-VAR-LIFETIME"));
	AppendLine(Source, TEXT("// Cases: 100 total, 80 normal, 0 reject, 20 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FLifetimeCell& Cell = Cells[Index];
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		if (IsFault(Cell.Exit))
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

FString FVarLifetimeGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace VarLifetimeGeneratorDetail;
	const TArray<FLifetimeCell> Cells = AllCells();
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

FString FVarLifetimeGenerator::BuildVariableLifetimeSource(const FVarLifetimeParams& Params) const
{
	using namespace VarLifetimeGeneratorDetail;
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

int32 FVarLifetimeGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace VarLifetimeGeneratorDetail;
	FLifetimeCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsFault(Cell.Exit))
	{
		return 0;
	}
	return ExpectedFromExit(Cell.Exit);
}
}
