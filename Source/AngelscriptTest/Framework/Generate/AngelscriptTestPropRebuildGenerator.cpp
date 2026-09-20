#include "Framework/Generate/AngelscriptTestPropRebuildGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace PropRebuildGeneratorDetail
{
constexpr int32 ObservationCount = 3;
constexpr int32 PathCount = 2;
constexpr int32 ScenarioCount = 15;
constexpr int32 TotalCells = ObservationCount * PathCount * ScenarioCount;
constexpr const TCHAR* LifecycleNotes = TEXT("Second-version source only; does not execute rebuild, save/load, or old-handle cleanup.");

struct FRebuildCell
{
	EPropRebuildObservation Observation = EPropRebuildObservation::Metadata;
	EPropRebuildPath Path = EPropRebuildPath::Rebuild;
	EPropRebuildScenario Scenario = EPropRebuildScenario::StoredSameSource;
};

const TCHAR* ObservationToken(const EPropRebuildObservation Observation)
{
	switch (Observation)
	{
	case EPropRebuildObservation::Metadata:
		return TEXT("METADATA");
	case EPropRebuildObservation::Runtime:
		return TEXT("RUNTIME");
	case EPropRebuildObservation::OldHandleCleanup:
		return TEXT("OLD_HANDLE_CLEANUP");
	default:
		return nullptr;
	}
}

const TCHAR* PathToken(const EPropRebuildPath Path)
{
	switch (Path)
	{
	case EPropRebuildPath::Rebuild:
		return TEXT("REBUILD");
	case EPropRebuildPath::SaveLoad:
		return TEXT("SAVE_LOAD");
	default:
		return nullptr;
	}
}

const TCHAR* ScenarioToken(const EPropRebuildScenario Scenario)
{
	switch (Scenario)
	{
	case EPropRebuildScenario::StoredSameSource:
		return TEXT("STORED_SAME_SOURCE");
	case EPropRebuildScenario::StoredValue:
		return TEXT("STORED_VALUE");
	case EPropRebuildScenario::StoredFieldType:
		return TEXT("STORED_FIELD_TYPE");
	case EPropRebuildScenario::StoredFieldOrder:
		return TEXT("STORED_FIELD_ORDER");
	case EPropRebuildScenario::StoredInheritance:
		return TEXT("STORED_INHERITANCE");
	case EPropRebuildScenario::RegisteredSameSource:
		return TEXT("REGISTERED_SAME_SOURCE");
	case EPropRebuildScenario::RegisteredReadValue:
		return TEXT("REGISTERED_READ_VALUE");
	case EPropRebuildScenario::RegisteredGetterPresence:
		return TEXT("REGISTERED_GETTER_PRESENCE");
	case EPropRebuildScenario::RegisteredSetterPresence:
		return TEXT("REGISTERED_SETTER_PRESENCE");
	case EPropRebuildScenario::RegisteredConstness:
		return TEXT("REGISTERED_CONSTNESS");
	case EPropRebuildScenario::RegisteredIndexedSameSource:
		return TEXT("REGISTERED_INDEXED_SAME_SOURCE");
	case EPropRebuildScenario::RegisteredIndexedValue:
		return TEXT("REGISTERED_INDEXED_VALUE");
	case EPropRebuildScenario::RegisteredIndexedConstness:
		return TEXT("REGISTERED_INDEXED_CONSTNESS");
	case EPropRebuildScenario::RegisteredIndexedIndexType:
		return TEXT("REGISTERED_INDEXED_INDEX_TYPE");
	case EPropRebuildScenario::RegisteredIndexedOverloadSet:
		return TEXT("REGISTERED_INDEXED_OVERLOAD_SET");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FPropRebuildParams& Params)
{
	return ObservationToken(Params.Observation) != nullptr
		&& PathToken(Params.Path) != nullptr
		&& ScenarioToken(Params.Scenario) != nullptr;
}

bool IsIndexed(const EPropRebuildScenario Scenario)
{
	return Scenario == EPropRebuildScenario::RegisteredIndexedSameSource
		|| Scenario == EPropRebuildScenario::RegisteredIndexedValue
		|| Scenario == EPropRebuildScenario::RegisteredIndexedConstness
		|| Scenario == EPropRebuildScenario::RegisteredIndexedIndexType
		|| Scenario == EPropRebuildScenario::RegisteredIndexedOverloadSet;
}

bool IsStored(const EPropRebuildScenario Scenario)
{
	return Scenario == EPropRebuildScenario::StoredSameSource
		|| Scenario == EPropRebuildScenario::StoredValue
		|| Scenario == EPropRebuildScenario::StoredFieldType
		|| Scenario == EPropRebuildScenario::StoredFieldOrder
		|| Scenario == EPropRebuildScenario::StoredInheritance;
}

int32 ExpectedRuntimeValue(const EPropRebuildScenario Scenario)
{
	if (Scenario == EPropRebuildScenario::RegisteredSetterPresence)
	{
		return 73;
	}
	const bool bSecondChanged =
		Scenario == EPropRebuildScenario::StoredValue
		|| Scenario == EPropRebuildScenario::StoredFieldType
		|| Scenario == EPropRebuildScenario::StoredFieldOrder
		|| Scenario == EPropRebuildScenario::StoredInheritance
		|| Scenario == EPropRebuildScenario::RegisteredReadValue
		|| Scenario == EPropRebuildScenario::RegisteredIndexedValue;
	const int32 BaseValue = bSecondChanged ? 29 : 11;
	return IsIndexed(Scenario) ? BaseValue + 1 : BaseValue;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FRebuildCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-PROP-REBUILD-%s-%s-%s"),
		ObservationToken(Cell.Observation),
		PathToken(Cell.Path),
		ScenarioToken(Cell.Scenario));
}

FString MakeEntryName(const FRebuildCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FPropRebuildParams ParamsFromCell(const FRebuildCell& Cell)
{
	FPropRebuildParams Params;
	Params.Observation = Cell.Observation;
	Params.Path = Cell.Path;
	Params.Scenario = Cell.Scenario;
	return Params;
}

TArray<FRebuildCell> AllCells()
{
	TArray<FRebuildCell> Cells;
	Cells.SetNum(TotalCells);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		FRebuildCell& Cell = Cells[Index];
		Cell.Observation = static_cast<EPropRebuildObservation>(Index / (PathCount * ScenarioCount));
		Cell.Path = static_cast<EPropRebuildPath>((Index / ScenarioCount) % PathCount);
		Cell.Scenario = static_cast<EPropRebuildScenario>(Index % ScenarioCount);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FRebuildCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FRebuildCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendSharedHelpers(FString& Source)
{
	(void)Source;
}

void AppendStored(FString& Source, const FPropRebuildParams& Params, const FString& Qualifier)
{
	const int32 Value = ExpectedRuntimeValue(Params.Scenario);
	const bool bInt64 = Params.Scenario == EPropRebuildScenario::StoredFieldType;
	const bool bDerived = Params.Scenario == EPropRebuildScenario::StoredInheritance;
	const bool bFirst = Params.Scenario != EPropRebuildScenario::StoredFieldOrder;
	AppendLine(Source, FString::Printf(TEXT("class FStoredRebuildBase_%s"), *Qualifier));
	AppendLine(Source, TEXT("{"));
	if (!bDerived)
	{
		if (bFirst)
		{
			AppendLine(Source, FString::Printf(TEXT("\t%s Stored = %d;"), bInt64 ? TEXT("int64") : TEXT("int"), Value));
			AppendLine(Source, TEXT("\tint Padding = 5;"));
		}
		else
		{
			AppendLine(Source, TEXT("\tint Padding = 5;"));
			AppendLine(Source, FString::Printf(TEXT("\t%s Stored = %d;"), bInt64 ? TEXT("int64") : TEXT("int"), Value));
		}
	}
	else
	{
		AppendLine(Source, TEXT("\tint Padding = 5;"));
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(
		TEXT("class FStoredRebuildDerived_%s : FStoredRebuildBase_%s"),
		*Qualifier,
		*Qualifier));
	AppendLine(Source, TEXT("{"));
	if (bDerived)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Stored = %d;"), bInt64 ? TEXT("int64") : TEXT("int"), Value));
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendEntry(FString& Source, const FPropRebuildParams& Params, const FString& FunctionName)
{
	const FString Qualifier = FunctionName;
	AppendLine(Source, FString::Printf(TEXT("// Observation: %s"), ObservationToken(Params.Observation)));
	AppendLine(Source, FString::Printf(TEXT("// Path: %s"), PathToken(Params.Path)));
	if (IsStored(Params.Scenario))
	{
		AppendStored(Source, Params, Qualifier);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(
			TEXT("\tFStoredRebuildDerived_%s Receiver = FStoredRebuildDerived_%s();"),
			*Qualifier,
			*Qualifier));
		AppendLine(Source, TEXT("\treturn int(Receiver.Stored);"));
		AppendLine(Source, TEXT("}"));
		return;
	}

	const bool bConst = Params.Scenario == EPropRebuildScenario::RegisteredConstness
		|| Params.Scenario == EPropRebuildScenario::RegisteredIndexedConstness;
	const bool bUseB = Params.Scenario == EPropRebuildScenario::RegisteredSameSource
		|| Params.Scenario == EPropRebuildScenario::RegisteredGetterPresence
		|| Params.Scenario == EPropRebuildScenario::RegisteredSetterPresence
		|| Params.Scenario == EPropRebuildScenario::RegisteredConstness
		|| Params.Scenario == EPropRebuildScenario::RegisteredIndexedSameSource
		|| Params.Scenario == EPropRebuildScenario::RegisteredIndexedConstness
		|| Params.Scenario == EPropRebuildScenario::RegisteredIndexedIndexType
		|| Params.Scenario == EPropRebuildScenario::RegisteredIndexedOverloadSet;
	const TCHAR* const TypeName = IsIndexed(Params.Scenario)
		? (bUseB ? TEXT("FRebuildIndexedB") : TEXT("FRebuildIndexedA"))
		: (bUseB ? TEXT("FRebuildRegisteredB") : TEXT("FRebuildRegisteredA"));
	const TCHAR* const IndexType = Params.Scenario == EPropRebuildScenario::RegisteredIndexedIndexType
		? TEXT("uint")
		: TEXT("int");
	const int32 Initial = ExpectedRuntimeValue(Params.Scenario);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(
		TEXT("\t%s%s@ Receiver = Create%s(%d);"),
		bConst ? TEXT("const ") : TEXT(""),
		TypeName,
		TypeName,
		IsIndexed(Params.Scenario) ? (Initial - 1) : Initial));
	if (Params.Scenario == EPropRebuildScenario::RegisteredSetterPresence)
	{
		AppendLine(Source, TEXT("\tReceiver.Value = 73;"));
		AppendLine(Source, FString::Printf(TEXT("\treturn Observe%s(Receiver);"), TypeName));
	}
	else if (IsIndexed(Params.Scenario))
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Index = %s(1);"), IndexType, IndexType));
		AppendLine(Source, TEXT("\treturn Receiver.Value[Index];"));
	}
	else
	{
		AppendLine(Source, TEXT("\treturn Receiver.Value;"));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FPropRebuildParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FRebuildCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.ExpectedReturn = ExpectedRuntimeValue(Cell.Scenario);
	Info.bLimitedObservation = true;
	Info.ExecutionNotes = LifecycleNotes;
	if (Cell.Observation == EPropRebuildObservation::Runtime)
	{
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.Execution = EGeneratedCaseExecution::Standalone;
	}
	else
	{
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
	}
	return Info;
}
}

FStringView FPropRebuildGenerator::GetProductId() const
{
	return TEXT("LANG-PROP-REBUILD");
}

TArray<FGeneratedCaseInfo> FPropRebuildGenerator::ListCases() const
{
	using namespace PropRebuildGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FRebuildCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("PropRebuild PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FPropRebuildGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace PropRebuildGeneratorDetail;
	FRebuildCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FPropRebuildGenerator::BuildDumpSource() const
{
	using namespace PropRebuildGeneratorDetail;
	const TArray<FRebuildCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-PROP-REBUILD"));
	AppendLine(Source, TEXT("// Cases: 90 total, 90 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	bool bNeedBlank = false;
	for (const FRebuildCell& Cell : Cells)
	{
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (bNeedBlank)
		{
			AppendLine(Source);
		}
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, Info.Execution == EGeneratedCaseExecution::SourceOnly
			? TEXT("// Execution: SourceOnly")
			: TEXT("// Execution: Standalone"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}
	return Source;
}

FString FPropRebuildGenerator::BuildScenarioSource(const FPropRebuildParams& Params) const
{
	using namespace PropRebuildGeneratorDetail;
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

FString FPropRebuildGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace PropRebuildGeneratorDetail;
	const TArray<FRebuildCell> Cells = AllCells();
	FString Source;
	int32 Emitted = 0;
	for (const FRebuildCell& Cell : Cells)
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

int32 FPropRebuildGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace PropRebuildGeneratorDetail;
	FRebuildCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedRuntimeValue(Cell.Scenario);
}
}
