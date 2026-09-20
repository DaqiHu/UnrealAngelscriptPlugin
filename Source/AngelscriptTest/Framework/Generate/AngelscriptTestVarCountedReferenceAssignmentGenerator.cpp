#include "Framework/Generate/AngelscriptTestVarCountedReferenceAssignmentGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace VarCountedReferenceAssignmentGeneratorDetail
{
constexpr const TCHAR* SaveLoadNotes = TEXT("Bytecode save/load observation is host-only.");

struct FCountedReferenceCell
{
	EVarReferenceAssignmentScenario Scenario = EVarReferenceAssignmentScenario::FactoryLocal;
};

const TCHAR* ScenarioToken(const EVarReferenceAssignmentScenario Scenario)
{
	switch (Scenario)
	{
	case EVarReferenceAssignmentScenario::FactoryLocal:
		return TEXT("FACTORY_LOCAL");
	case EVarReferenceAssignmentScenario::Overwrite:
		return TEXT("OVERWRITE");
	case EVarReferenceAssignmentScenario::NullAssignment:
		return TEXT("NULL_ASSIGNMENT");
	case EVarReferenceAssignmentScenario::ParameterReturn:
		return TEXT("PARAMETER_RETURN");
	case EVarReferenceAssignmentScenario::ExceptionFrame:
		return TEXT("EXCEPTION_FRAME");
	case EVarReferenceAssignmentScenario::SaveLoad:
		return TEXT("SAVE_LOAD");
	case EVarReferenceAssignmentScenario::ParameterReturnSaveLoad:
		return TEXT("PARAMETER_RETURN_SAVE_LOAD");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FVarCountedReferenceAssignmentParams& Params)
{
	return ScenarioToken(Params.Scenario) != nullptr;
}

bool IsFault(const EVarReferenceAssignmentScenario Scenario)
{
	return Scenario == EVarReferenceAssignmentScenario::ExceptionFrame;
}

bool IsSaveLoad(const EVarReferenceAssignmentScenario Scenario)
{
	return Scenario == EVarReferenceAssignmentScenario::SaveLoad
		|| Scenario == EVarReferenceAssignmentScenario::ParameterReturnSaveLoad;
}

int32 ExpectedFromScenario(const EVarReferenceAssignmentScenario Scenario)
{
	switch (Scenario)
	{
	case EVarReferenceAssignmentScenario::FactoryLocal:
	case EVarReferenceAssignmentScenario::SaveLoad:
		return 410;
	case EVarReferenceAssignmentScenario::Overwrite:
		return 421;
	case EVarReferenceAssignmentScenario::NullAssignment:
		return 1;
	case EVarReferenceAssignmentScenario::ParameterReturn:
	case EVarReferenceAssignmentScenario::ParameterReturnSaveLoad:
		return 440;
	default:
		return 0;
	}
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FCountedReferenceCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-VAR-COUNTED-REFERENCE-ASSIGNMENT-%s"),
		ScenarioToken(Cell.Scenario));
}

FString MakeEntryName(const FCountedReferenceCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FVarCountedReferenceAssignmentParams ParamsFromCell(const FCountedReferenceCell& Cell)
{
	FVarCountedReferenceAssignmentParams Params;
	Params.Scenario = Cell.Scenario;
	return Params;
}

TArray<FCountedReferenceCell> AllCells()
{
	TArray<FCountedReferenceCell> Cells;
	Cells.SetNum(7);
	for (int32 Index = 0; Index < 7; ++Index)
	{
		Cells[Index].Scenario = static_cast<EVarReferenceAssignmentScenario>(Index);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FCountedReferenceCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FCountedReferenceCell> Cells = AllCells();
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
	AppendLine(Source, TEXT("class FNativeCaseReference"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseReference()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = 0;"));
	AppendLine(Source, TEXT("\t}"));
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
	AppendLine(Source, TEXT("FNativeCaseReference PassCountedReference(FNativeCaseReference Input)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Input;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int RaiseCountedReferenceException()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Zero = 0;"));
	AppendLine(Source, TEXT("\treturn 1 / Zero;"));
	AppendLine(Source, TEXT("}"));
}

void AppendEntry(FString& Source, const FVarCountedReferenceAssignmentParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	switch (Params.Scenario)
	{
	case EVarReferenceAssignmentScenario::FactoryLocal:
	case EVarReferenceAssignmentScenario::SaveLoad:
		AppendLine(Source, TEXT("\tFNativeCaseReference Source = CreateNativeCaseReference(410);"));
		AppendLine(Source, TEXT("\tFNativeCaseReference Value;"));
		AppendLine(Source, TEXT("\tValue = Source;"));
		AppendLine(Source, TEXT("\treturn Value.Value;"));
		break;
	case EVarReferenceAssignmentScenario::Overwrite:
		AppendLine(Source, TEXT("\tFNativeCaseReference Value = CreateNativeCaseReference(420);"));
		AppendLine(Source, TEXT("\tValue = CreateNativeCaseReference(421);"));
		AppendLine(Source, TEXT("\treturn Value.Value;"));
		break;
	case EVarReferenceAssignmentScenario::NullAssignment:
		AppendLine(Source, TEXT("\tFNativeCaseReference Value = CreateNativeCaseReference(430);"));
		AppendLine(Source, TEXT("\tValue = nullptr;"));
		AppendLine(Source, TEXT("\tif (Value == nullptr)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 1;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn 0;"));
		break;
	case EVarReferenceAssignmentScenario::ParameterReturn:
	case EVarReferenceAssignmentScenario::ParameterReturnSaveLoad:
		AppendLine(Source, TEXT("\tFNativeCaseReference Value = CreateNativeCaseReference(440);"));
		AppendLine(Source, TEXT("\tFNativeCaseReference Result = PassCountedReference(Value);"));
		AppendLine(Source, TEXT("\treturn Result.Value;"));
		break;
	case EVarReferenceAssignmentScenario::ExceptionFrame:
		AppendLine(Source, TEXT("\tFNativeCaseReference Value = CreateNativeCaseReference(450);"));
		AppendLine(Source, TEXT("\treturn RaiseCountedReferenceException() + Value.Value;"));
		break;
	default:
		AppendLine(Source, TEXT("\treturn 0;"));
		break;
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FVarCountedReferenceAssignmentParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FCountedReferenceCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	if (IsFault(Cell.Scenario))
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.Execution = EGeneratedCaseExecution::Standalone;
		Info.ExpectedException = TEXT("Divide by zero");
		return Info;
	}
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.ExpectedReturn = ExpectedFromScenario(Cell.Scenario);
	if (IsSaveLoad(Cell.Scenario))
	{
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		Info.bLimitedObservation = true;
		Info.ExecutionNotes = SaveLoadNotes;
	}
	else
	{
		Info.Execution = EGeneratedCaseExecution::Standalone;
	}
	return Info;
}
}

FStringView FVarCountedReferenceAssignmentGenerator::GetProductId() const
{
	return TEXT("LANG-VAR-COUNTED-REFERENCE-ASSIGNMENT");
}

TArray<FGeneratedCaseInfo> FVarCountedReferenceAssignmentGenerator::ListCases() const
{
	using namespace VarCountedReferenceAssignmentGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FCountedReferenceCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FCountedReferenceCell& Cell = Cells[Index];
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("VarCountedReferenceAssignment PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MakeCaseInfo(Cell));
	}
	return Cases;
}

FString FVarCountedReferenceAssignmentGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace VarCountedReferenceAssignmentGeneratorDetail;
	FCountedReferenceCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FVarCountedReferenceAssignmentGenerator::BuildDumpSource() const
{
	using namespace VarCountedReferenceAssignmentGeneratorDetail;
	const TArray<FCountedReferenceCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-VAR-COUNTED-REFERENCE-ASSIGNMENT"));
	AppendLine(Source, TEXT("// Cases: 7 total, 6 normal, 0 reject, 1 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FCountedReferenceCell& Cell = Cells[Index];
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		if (IsFault(Cell.Scenario))
		{
			AppendLine(Source, TEXT("// Outcome: RuntimeException"));
			AppendLine(Source, TEXT("// ExpectedException: Divide by zero"));
			AppendLine(Source, TEXT("// Execution: Standalone"));
		}
		else
		{
			AppendLine(Source, TEXT("// Outcome: ReturnValue"));
			AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
			if (IsSaveLoad(Cell.Scenario))
			{
				AppendLine(Source, TEXT("// Execution: SourceOnly"));
				AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
			}
			else
			{
				AppendLine(Source, TEXT("// Execution: Standalone"));
			}
		}
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FVarCountedReferenceAssignmentGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace VarCountedReferenceAssignmentGeneratorDetail;
	const TArray<FCountedReferenceCell> Cells = AllCells();
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

FString FVarCountedReferenceAssignmentGenerator::BuildReferenceAssignmentSource(
	const FVarCountedReferenceAssignmentParams& Params) const
{
	using namespace VarCountedReferenceAssignmentGeneratorDetail;
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

int32 FVarCountedReferenceAssignmentGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace VarCountedReferenceAssignmentGeneratorDetail;
	FCountedReferenceCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsFault(Cell.Scenario))
	{
		return 0;
	}
	return ExpectedFromScenario(Cell.Scenario);
}
}
