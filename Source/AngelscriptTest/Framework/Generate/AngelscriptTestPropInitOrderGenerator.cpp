#include "Framework/Generate/AngelscriptTestPropInitOrderGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace PropInitOrderGeneratorDetail
{
constexpr int32 ObservationCount = 4;
constexpr int32 PositionCount = 5;
constexpr int32 SourceCount = 5;
constexpr int32 TypeCount = 15;
constexpr int32 TotalCells = ObservationCount * PositionCount * SourceCount * TypeCount;
constexpr const TCHAR* HostNotes = TEXT("Needs native RecordPropertyInitializationMarker and RecordPropertyInitializationCheckpoint.");

struct FInitCell
{
	EPropInitOrderObservation Observation = EPropInitOrderObservation::BaseEntry;
	EPropInitOrderPosition Position = EPropInitOrderPosition::BaseFirst;
	EPropInitOrderSource Source = EPropInitOrderSource::DefaultValue;
	EPropInitOrderType Type = EPropInitOrderType::Int8;
};

const TCHAR* ObservationToken(const EPropInitOrderObservation Observation)
{
	switch (Observation)
	{
	case EPropInitOrderObservation::BaseEntry:
		return TEXT("BASE_ENTRY");
	case EPropInitOrderObservation::BaseExit:
		return TEXT("BASE_EXIT");
	case EPropInitOrderObservation::DerivedEntry:
		return TEXT("DERIVED_ENTRY");
	case EPropInitOrderObservation::DerivedExit:
		return TEXT("DERIVED_EXIT");
	default:
		return nullptr;
	}
}

const TCHAR* PositionToken(const EPropInitOrderPosition Position)
{
	switch (Position)
	{
	case EPropInitOrderPosition::BaseFirst:
		return TEXT("BASE_FIRST");
	case EPropInitOrderPosition::BaseMiddle:
		return TEXT("BASE_MIDDLE");
	case EPropInitOrderPosition::BaseLast:
		return TEXT("BASE_LAST");
	case EPropInitOrderPosition::DerivedFirst:
		return TEXT("DERIVED_FIRST");
	case EPropInitOrderPosition::DerivedLast:
		return TEXT("DERIVED_LAST");
	default:
		return nullptr;
	}
}

const TCHAR* SourceToken(const EPropInitOrderSource Source)
{
	switch (Source)
	{
	case EPropInitOrderSource::DefaultValue:
		return TEXT("DEFAULT_VALUE");
	case EPropInitOrderSource::DeclarationInitializer:
		return TEXT("DECLARATION_INITIALIZER");
	case EPropInitOrderSource::OwnerLiteralAssignment:
		return TEXT("OWNER_LITERAL_ASSIGNMENT");
	case EPropInitOrderSource::OwnerSourceAssignment:
		return TEXT("OWNER_SOURCE_ASSIGNMENT");
	case EPropInitOrderSource::DerivedReassignment:
		return TEXT("DERIVED_REASSIGNMENT");
	default:
		return nullptr;
	}
}

const TCHAR* TypeToken(const EPropInitOrderType Type)
{
	switch (Type)
	{
	case EPropInitOrderType::Int8:
		return TEXT("INT8");
	case EPropInitOrderType::Int16:
		return TEXT("INT16");
	case EPropInitOrderType::Int:
		return TEXT("INT");
	case EPropInitOrderType::Int64:
		return TEXT("INT64");
	case EPropInitOrderType::UInt8:
		return TEXT("UINT8");
	case EPropInitOrderType::UInt16:
		return TEXT("UINT16");
	case EPropInitOrderType::UInt:
		return TEXT("UINT");
	case EPropInitOrderType::UInt64:
		return TEXT("UINT64");
	case EPropInitOrderType::Float32:
		return TEXT("FLOAT32");
	case EPropInitOrderType::Float64:
		return TEXT("FLOAT64");
	case EPropInitOrderType::Bool:
		return TEXT("BOOL");
	case EPropInitOrderType::Enum:
		return TEXT("ENUM");
	case EPropInitOrderType::Typedef:
		return TEXT("TYPEDEF");
	case EPropInitOrderType::ScriptValue:
		return TEXT("SCRIPT_VALUE");
	case EPropInitOrderType::NativeValue:
		return TEXT("NATIVE_VALUE");
	default:
		return nullptr;
	}
}

const TCHAR* ScriptType(const EPropInitOrderType Type)
{
	switch (Type)
	{
	case EPropInitOrderType::Int8:
		return TEXT("int8");
	case EPropInitOrderType::Int16:
		return TEXT("int16");
	case EPropInitOrderType::Int:
		return TEXT("int");
	case EPropInitOrderType::Int64:
		return TEXT("int64");
	case EPropInitOrderType::UInt8:
		return TEXT("uint8");
	case EPropInitOrderType::UInt16:
		return TEXT("uint16");
	case EPropInitOrderType::UInt:
		return TEXT("uint");
	case EPropInitOrderType::UInt64:
		return TEXT("uint64");
	case EPropInitOrderType::Float32:
		return TEXT("float");
	case EPropInitOrderType::Float64:
		return TEXT("double");
	case EPropInitOrderType::Bool:
		return TEXT("bool");
	case EPropInitOrderType::Enum:
		return TEXT("ENativeCaseEnum");
	case EPropInitOrderType::Typedef:
		return TEXT("NativeCaseAlias");
	case EPropInitOrderType::ScriptValue:
		return TEXT("FScriptCaseValue");
	case EPropInitOrderType::NativeValue:
		return TEXT("FNativeCaseValue");
	default:
		return nullptr;
	}
}

bool TargetInBase(const EPropInitOrderPosition Position)
{
	return Position == EPropInitOrderPosition::BaseFirst
		|| Position == EPropInitOrderPosition::BaseMiddle
		|| Position == EPropInitOrderPosition::BaseLast;
}

int32 OwnerIndex(const EPropInitOrderPosition Position)
{
	switch (Position)
	{
	case EPropInitOrderPosition::BaseFirst:
	case EPropInitOrderPosition::DerivedFirst:
		return 0;
	case EPropInitOrderPosition::BaseMiddle:
		return 2;
	default:
		return 4;
	}
}

int32 ObservationStage(const EPropInitOrderObservation Observation)
{
	return static_cast<int32>(Observation);
}

int32 ExpectedFinal(const EPropInitOrderSource Source)
{
	return (Source == EPropInitOrderSource::DefaultValue
		|| Source == EPropInitOrderSource::DerivedReassignment)
		? 0
		: 1;
}

bool IsValidParams(const FPropInitOrderParams& Params)
{
	return ObservationToken(Params.Observation) != nullptr
		&& PositionToken(Params.Position) != nullptr
		&& SourceToken(Params.Source) != nullptr
		&& TypeToken(Params.Type) != nullptr;
}

FString TypedValue(const EPropInitOrderType Type, const int32 Value)
{
	if (Type == EPropInitOrderType::Bool)
	{
		return Value == 0 ? TEXT("false") : TEXT("true");
	}
	if (Type == EPropInitOrderType::Enum)
	{
		return Value == 0 ? TEXT("ENativeCaseEnum::Zero") : TEXT("ENativeCaseEnum::One");
	}
	return FString::Printf(TEXT("%s(%d)"), ScriptType(Type), Value);
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FInitCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-PROP-INIT-ORDER-%s-%s-%s-%s"),
		ObservationToken(Cell.Observation),
		PositionToken(Cell.Position),
		SourceToken(Cell.Source),
		TypeToken(Cell.Type));
}

FString MakeEntryName(const FInitCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FPropInitOrderParams ParamsFromCell(const FInitCell& Cell)
{
	FPropInitOrderParams Params;
	Params.Observation = Cell.Observation;
	Params.Position = Cell.Position;
	Params.Source = Cell.Source;
	Params.Type = Cell.Type;
	return Params;
}

TArray<FInitCell> AllCells()
{
	TArray<FInitCell> Cells;
	Cells.SetNum(TotalCells);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		FInitCell& Cell = Cells[Index];
		Cell.Observation = static_cast<EPropInitOrderObservation>(
			Index / (PositionCount * SourceCount * TypeCount));
		Cell.Position = static_cast<EPropInitOrderPosition>(
			(Index / (SourceCount * TypeCount)) % PositionCount);
		Cell.Source = static_cast<EPropInitOrderSource>((Index / TypeCount) % SourceCount);
		Cell.Type = static_cast<EPropInitOrderType>(Index % TypeCount);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FInitCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FInitCell& Cell : AllCells())
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
	AppendLine(Source, TEXT("enum ENativeCaseEnum"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tZero = 0,"));
	AppendLine(Source, TEXT("\tOne = 1"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FScriptCaseValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source, TEXT("\tFScriptCaseValue() {}"));
	AppendLine(Source, TEXT("\tFScriptCaseValue(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int RecordPropertyInitializationMarker(int Marker)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Marker;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("void RecordPropertyInitializationCheckpoint(int Stage, bool Available, int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
}

void AppendOwnerFields(
	FString& Source,
	const bool bBaseOwner,
	const FPropInitOrderParams& Params,
	const FString& FunctionName)
{
	const bool bTargetOwner = TargetInBase(Params.Position) == bBaseOwner;
	const int32 TargetIndex = OwnerIndex(Params.Position);
	const bool bHasInit = Params.Source == EPropInitOrderSource::DeclarationInitializer
		|| Params.Source == EPropInitOrderSource::DerivedReassignment;
	for (int32 PropertyIndex = 0; PropertyIndex < 5; ++PropertyIndex)
	{
		if (bTargetOwner && PropertyIndex == TargetIndex)
		{
			if (bHasInit)
			{
				AppendLine(Source, FString::Printf(
					TEXT("\t%s Observed = InitializePropertyValue_%s();"),
					ScriptType(Params.Type),
					*FunctionName));
			}
			else
			{
				AppendLine(Source, FString::Printf(TEXT("\t%s Observed;"), ScriptType(Params.Type)));
			}
			continue;
		}
		const int32 Marker = (bBaseOwner ? 10 : 20) + PropertyIndex;
		AppendLine(Source, FString::Printf(
			TEXT("\tint %sField%d = RecordPropertyInitializationMarker(%d);"),
			bBaseOwner ? TEXT("Base") : TEXT("Derived"),
			PropertyIndex,
			Marker));
	}
}

void AppendAssignment(
	FString& Source,
	const FPropInitOrderParams& Params,
	const bool bInBaseConstructor)
{
	const bool bOwnerAssignment =
		(Params.Source == EPropInitOrderSource::OwnerLiteralAssignment
			|| Params.Source == EPropInitOrderSource::OwnerSourceAssignment)
		&& TargetInBase(Params.Position) == bInBaseConstructor;
	const bool bDerivedReassignment =
		Params.Source == EPropInitOrderSource::DerivedReassignment && !bInBaseConstructor;
	if (!bOwnerAssignment && !bDerivedReassignment)
	{
		return;
	}
	if (Params.Source == EPropInitOrderSource::OwnerSourceAssignment)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t\t%s SourceValue = %s;"),
			ScriptType(Params.Type),
			*TypedValue(Params.Type, 1)));
		AppendLine(Source, TEXT("\t\tObserved = SourceValue;"));
		return;
	}
	const int32 Assigned = bDerivedReassignment ? 0 : 1;
	AppendLine(Source, FString::Printf(TEXT("\t\tObserved = %s;"), *TypedValue(Params.Type, Assigned)));
}

void AppendEntry(FString& Source, const FPropInitOrderParams& Params, const FString& FunctionName)
{
	const FString Qualifier = FunctionName;
	const bool bHasInit = Params.Source == EPropInitOrderSource::DeclarationInitializer
		|| Params.Source == EPropInitOrderSource::DerivedReassignment;
	if (bHasInit)
	{
		AppendLine(Source, FString::Printf(TEXT("%s InitializePropertyValue_%s()"), ScriptType(Params.Type), *Qualifier));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(
			TEXT("\tRecordPropertyInitializationMarker(%d);"),
			TargetInBase(Params.Position) ? 100 + OwnerIndex(Params.Position) : 200 + OwnerIndex(Params.Position)));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *TypedValue(Params.Type, 1)));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	AppendLine(Source, FString::Printf(TEXT("class FPropertyInitializationBase_%s"), *Qualifier));
	AppendLine(Source, TEXT("{"));
	AppendOwnerFields(Source, true, Params, Qualifier);
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("\tFPropertyInitializationBase_%s()"), *Qualifier));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TargetInBase(Params.Position)
		? TEXT("\t\tRecordPropertyInitializationCheckpoint(0, true, 0);")
		: TEXT("\t\tRecordPropertyInitializationCheckpoint(0, false, -777);"));
	AppendAssignment(Source, Params, true);
	AppendLine(Source, TargetInBase(Params.Position)
		? TEXT("\t\tRecordPropertyInitializationCheckpoint(1, true, 0);")
		: TEXT("\t\tRecordPropertyInitializationCheckpoint(1, false, -777);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(
		TEXT("class FPropertyInitializationDerived_%s : FPropertyInitializationBase_%s"),
		*Qualifier,
		*Qualifier));
	AppendLine(Source, TEXT("{"));
	AppendOwnerFields(Source, false, Params, Qualifier);
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("\tFPropertyInitializationDerived_%s()"), *Qualifier));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tsuper();"));
	AppendLine(Source, TEXT("\t\tRecordPropertyInitializationCheckpoint(2, true, 0);"));
	AppendAssignment(Source, Params, false);
	AppendLine(Source, TEXT("\t\tRecordPropertyInitializationCheckpoint(3, true, 0);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(
		TEXT("\tFPropertyInitializationDerived_%s Receiver = FPropertyInitializationDerived_%s();"),
		*Qualifier,
		*Qualifier));
	AppendLine(Source, FString::Printf(TEXT("\tconst int ObservationStage = %d;"), ObservationStage(Params.Observation)));
	AppendLine(Source, FString::Printf(TEXT("\treturn %d;"), ExpectedFinal(Params.Source)));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FPropInitOrderParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FInitCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.ExpectedReturn = ExpectedFinal(Cell.Source);
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExecutionNotes = HostNotes;
	if (Cell.Type == EPropInitOrderType::Float32 || Cell.Type == EPropInitOrderType::Float64
		|| Cell.Type == EPropInitOrderType::Int64 || Cell.Type == EPropInitOrderType::UInt64
		|| Cell.Type == EPropInitOrderType::NativeValue || Cell.Type == EPropInitOrderType::Typedef)
	{
		Info.bLimitedObservation = true;
	}
	return Info;
}
}

FStringView FPropInitOrderGenerator::GetProductId() const
{
	return TEXT("LANG-PROP-INIT-ORDER");
}

TArray<FGeneratedCaseInfo> FPropInitOrderGenerator::ListCases() const
{
	using namespace PropInitOrderGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FInitCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("PropInitOrder PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FPropInitOrderGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace PropInitOrderGeneratorDetail;
	FInitCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FPropInitOrderGenerator::BuildDumpSource() const
{
	using namespace PropInitOrderGeneratorDetail;
	const TArray<FInitCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-PROP-INIT-ORDER"));
	AppendLine(Source, TEXT("// Cases: 1500 total, 1500 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	bool bNeedBlank = true;
	for (const FInitCell& Cell : Cells)
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
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}
	return Source;
}

FString FPropInitOrderGenerator::BuildPropertyInitializationSource(const FPropInitOrderParams& Params) const
{
	using namespace PropInitOrderGeneratorDetail;
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

FString FPropInitOrderGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace PropInitOrderGeneratorDetail;
	const TArray<FInitCell> Cells = AllCells();
	FString Source;
	AppendSharedHelpers(Source);
	int32 Emitted = 0;
	for (const FInitCell& Cell : Cells)
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

int32 FPropInitOrderGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace PropInitOrderGeneratorDetail;
	FInitCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFinal(Cell.Source);
}
}
