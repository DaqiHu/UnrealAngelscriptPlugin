#include "Framework/Generate/AngelscriptTestCtorBoundaryGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace CtorBoundaryGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native constructor-boundary host markers.");

struct FCtorBoundaryCell
{
	ECtorBoundaryScenario Scenario = ECtorBoundaryScenario::DuplicateDefaultSignature;
	ECtorBoundaryObservation Observation = ECtorBoundaryObservation::CompileOrExecutionState;
};

const TCHAR* ScenarioToken(const ECtorBoundaryScenario Scenario)
{
	switch (Scenario)
	{
	case ECtorBoundaryScenario::DuplicateDefaultSignature:
		return TEXT("DUPLICATE_DEFAULT_SIGNATURE");
	case ECtorBoundaryScenario::DuplicateParameterSignature:
		return TEXT("DUPLICATE_PARAMETER_SIGNATURE");
	case ECtorBoundaryScenario::MismatchedConstructorName:
		return TEXT("MISMATCHED_CONSTRUCTOR_NAME");
	case ECtorBoundaryScenario::ConstructorValueReturn:
		return TEXT("CONSTRUCTOR_VALUE_RETURN");
	case ECtorBoundaryScenario::SuperOutsideConstructor:
		return TEXT("SUPER_OUTSIDE_CONSTRUCTOR");
	case ECtorBoundaryScenario::SuperAfterStatement:
		return TEXT("SUPER_AFTER_STATEMENT");
	case ECtorBoundaryScenario::RepeatedSuper:
		return TEXT("REPEATED_SUPER");
	case ECtorBoundaryScenario::MissingBaseArgument:
		return TEXT("MISSING_BASE_ARGUMENT");
	case ECtorBoundaryScenario::AmbiguousBaseArgument:
		return TEXT("AMBIGUOUS_BASE_ARGUMENT");
	case ECtorBoundaryScenario::InaccessibleBase:
		return TEXT("INACCESSIBLE_BASE");
	case ECtorBoundaryScenario::RecursiveValueFieldDirect:
		return TEXT("RECURSIVE_VALUE_FIELD_DIRECT");
	case ECtorBoundaryScenario::RecursiveValueFieldIndirect:
		return TEXT("RECURSIVE_VALUE_FIELD_INDIRECT");
	case ECtorBoundaryScenario::RecursiveConstructorDirect:
		return TEXT("RECURSIVE_CONSTRUCTOR_DIRECT");
	case ECtorBoundaryScenario::RecursiveConstructorIndirect:
		return TEXT("RECURSIVE_CONSTRUCTOR_INDIRECT");
	case ECtorBoundaryScenario::MutableReferenceGlobal:
		return TEXT("MUTABLE_REFERENCE_GLOBAL");
	case ECtorBoundaryScenario::ModuleDiscardConstValueGlobal:
		return TEXT("MODULE_DISCARD_CONST_VALUE_GLOBAL");
	case ECtorBoundaryScenario::EngineShutdownConstValueGlobal:
		return TEXT("ENGINE_SHUTDOWN_CONST_VALUE_GLOBAL");
	default:
		return nullptr;
	}
}

const TCHAR* ObservationToken(const ECtorBoundaryObservation Observation)
{
	switch (Observation)
	{
	case ECtorBoundaryObservation::CompileOrExecutionState:
		return TEXT("COMPILE_OR_EXECUTION_STATE");
	case ECtorBoundaryObservation::DiagnosticOrMetadata:
		return TEXT("DIAGNOSTIC_OR_METADATA");
	case ECtorBoundaryObservation::LifecycleCleanup:
		return TEXT("LIFECYCLE_CLEANUP");
	case ECtorBoundaryObservation::RecoveryOrTeardown:
		return TEXT("RECOVERY_OR_TEARDOWN");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FCtorBoundaryParams& Params)
{
	return ScenarioToken(Params.Scenario) != nullptr && ObservationToken(Params.Observation) != nullptr;
}

bool IsReject(const ECtorBoundaryScenario Scenario)
{
	switch (Scenario)
	{
	case ECtorBoundaryScenario::MismatchedConstructorName:
	case ECtorBoundaryScenario::ConstructorValueReturn:
	case ECtorBoundaryScenario::SuperOutsideConstructor:
	case ECtorBoundaryScenario::RepeatedSuper:
	case ECtorBoundaryScenario::MissingBaseArgument:
	case ECtorBoundaryScenario::AmbiguousBaseArgument:
	case ECtorBoundaryScenario::InaccessibleBase:
	case ECtorBoundaryScenario::RecursiveValueFieldDirect:
	case ECtorBoundaryScenario::RecursiveValueFieldIndirect:
	case ECtorBoundaryScenario::MutableReferenceGlobal:
		return true;
	default:
		return false;
	}
}

bool IsFault(const ECtorBoundaryScenario Scenario)
{
	return Scenario == ECtorBoundaryScenario::RecursiveConstructorDirect
		|| Scenario == ECtorBoundaryScenario::RecursiveConstructorIndirect;
}

bool IsLimitedObservation(const FCtorBoundaryCell& Cell)
{
	if (Cell.Scenario == ECtorBoundaryScenario::DuplicateParameterSignature)
	{
		return true;
	}
	if (Cell.Scenario == ECtorBoundaryScenario::ModuleDiscardConstValueGlobal
		|| Cell.Scenario == ECtorBoundaryScenario::EngineShutdownConstValueGlobal)
	{
		return Cell.Observation != ECtorBoundaryObservation::CompileOrExecutionState;
	}
	return Cell.Observation == ECtorBoundaryObservation::DiagnosticOrMetadata
		|| Cell.Observation == ECtorBoundaryObservation::LifecycleCleanup
		|| Cell.Observation == ECtorBoundaryObservation::RecoveryOrTeardown;
}

int32 ExpectedFromScenario(const ECtorBoundaryScenario Scenario)
{
	switch (Scenario)
	{
	case ECtorBoundaryScenario::DuplicateDefaultSignature:
		return 42;
	case ECtorBoundaryScenario::SuperAfterStatement:
		return 7;
	case ECtorBoundaryScenario::ModuleDiscardConstValueGlobal:
	case ECtorBoundaryScenario::EngineShutdownConstValueGlobal:
		return 71;
	default:
		return 0;
	}
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FCtorBoundaryCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CTOR-BOUNDARY-%s-%s"),
		ScenarioToken(Cell.Scenario),
		ObservationToken(Cell.Observation));
}

FString MakeEntryName(const FCtorBoundaryCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FString TypeTag(const ECtorBoundaryScenario Scenario)
{
	FString Tag(ScenarioToken(Scenario));
	Tag.ReplaceInline(TEXT("_"), TEXT(""));
	return Tag;
}

FCtorBoundaryParams ParamsFromCell(const FCtorBoundaryCell& Cell)
{
	FCtorBoundaryParams Params;
	Params.Scenario = Cell.Scenario;
	Params.Observation = Cell.Observation;
	return Params;
}

TArray<FCtorBoundaryCell> AllCells()
{
	TArray<FCtorBoundaryCell> Cells;
	Cells.SetNum(68);
	for (int32 Index = 0; Index < 68; ++Index)
	{
		FCtorBoundaryCell& Cell = Cells[Index];
		Cell.Scenario = static_cast<ECtorBoundaryScenario>(Index / 4);
		Cell.Observation = static_cast<ECtorBoundaryObservation>(Index % 4);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FCtorBoundaryCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FCtorBoundaryCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendScenarioTypes(FString& Source, const ECtorBoundaryScenario Scenario)
{
	const FString Tag = TypeTag(Scenario);
	switch (Scenario)
	{
	case ECtorBoundaryScenario::DuplicateDefaultSignature:
		AppendLine(Source, FString::Printf(TEXT("struct FBoundaryValue%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 0;"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryValue%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = 41;"));
		AppendLine(Source, TEXT("\t\tRecordConstructorBoundaryMarker(41);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryValue%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = 42;"));
		AppendLine(Source, TEXT("\t\tRecordConstructorBoundaryMarker(42);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorBoundaryScenario::DuplicateParameterSignature:
		AppendLine(Source, FString::Printf(TEXT("struct FBoundaryValue%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 0;"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryValue%s(int Value)"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tthis.Value = Value + 51;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryValue%s(int OtherValue)"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = OtherValue + 52;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorBoundaryScenario::MismatchedConstructorName:
		AppendLine(Source, FString::Printf(TEXT("struct FBoundaryValue%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFBoundaryOther()"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorBoundaryScenario::ConstructorValueReturn:
		AppendLine(Source, FString::Printf(TEXT("struct FBoundaryValue%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryValue%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 1;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorBoundaryScenario::SuperOutsideConstructor:
		AppendLine(Source, TEXT("void InvokeSuperOutsideConstructor()"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tsuper();"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorBoundaryScenario::SuperAfterStatement:
		AppendLine(Source, FString::Printf(TEXT("class FBoundaryBase%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 0;"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryBase%s(int InValue)"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = InValue;"));
		AppendLine(Source, TEXT("\t\tRecordConstructorBoundaryMarker(2);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("class FBoundaryDerived%s : FBoundaryBase%s"), *Tag, *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryDerived%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tRecordConstructorBoundaryMarker(1);"));
		AppendLine(Source, TEXT("\t\tsuper(7);"));
		AppendLine(Source, TEXT("\t\tRecordConstructorBoundaryMarker(3);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorBoundaryScenario::RepeatedSuper:
		AppendLine(Source, FString::Printf(TEXT("class FBoundaryBase%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryBase%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("class FBoundaryDerived%s : FBoundaryBase%s"), *Tag, *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryDerived%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tsuper();"));
		AppendLine(Source, TEXT("\t\tsuper();"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorBoundaryScenario::MissingBaseArgument:
		AppendLine(Source, FString::Printf(TEXT("class FBoundaryBase%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryBase%s(int Value)"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("class FBoundaryDerived%s : FBoundaryBase%s"), *Tag, *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryDerived%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tsuper();"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorBoundaryScenario::AmbiguousBaseArgument:
		AppendLine(Source, FString::Printf(TEXT("class FBoundaryBase%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryBase%s(int Value, int Extra = 1)"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryBase%s(int Value, float Extra = 1.0f)"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("class FBoundaryDerived%s : FBoundaryBase%s"), *Tag, *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryDerived%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tsuper(1);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorBoundaryScenario::InaccessibleBase:
		AppendLine(Source, FString::Printf(TEXT("class FBoundaryBase%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tprivate FBoundaryBase%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("class FBoundaryDerived%s : FBoundaryBase%s"), *Tag, *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryDerived%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tsuper();"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorBoundaryScenario::RecursiveValueFieldDirect:
		AppendLine(Source, FString::Printf(TEXT("struct FBoundaryRecursive%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryRecursive%s Nested;"), *Tag));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorBoundaryScenario::RecursiveValueFieldIndirect:
		AppendLine(Source, FString::Printf(TEXT("struct FBoundaryRecursiveA%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryRecursiveB%s Nested;"), *Tag));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("struct FBoundaryRecursiveB%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryRecursiveA%s Nested;"), *Tag));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorBoundaryScenario::RecursiveConstructorDirect:
		AppendLine(Source, FString::Printf(TEXT("class FBoundaryRecursive%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryRecursive%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\tFBoundaryRecursive%s Next = FBoundaryRecursive%s();"), *Tag, *Tag));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorBoundaryScenario::RecursiveConstructorIndirect:
		AppendLine(Source, FString::Printf(TEXT("class FBoundaryRecursiveA%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryRecursiveA%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\tFBoundaryRecursiveB%s Next = FBoundaryRecursiveB%s();"), *Tag, *Tag));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("class FBoundaryRecursiveB%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryRecursiveB%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\tFBoundaryRecursiveA%s Next = FBoundaryRecursiveA%s();"), *Tag, *Tag));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorBoundaryScenario::MutableReferenceGlobal:
		AppendLine(Source, FString::Printf(TEXT("class FBoundaryRetained%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 71;"));
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryRetained%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("FBoundaryRetained%s RetainedGlobal = FBoundaryRetained%s();"), *Tag, *Tag));
		break;
	case ECtorBoundaryScenario::ModuleDiscardConstValueGlobal:
	case ECtorBoundaryScenario::EngineShutdownConstValueGlobal:
		AppendLine(Source, FString::Printf(TEXT("struct FBoundaryRetained%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 0;"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryRetained%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = 71;"));
		AppendLine(Source, TEXT("\t\tRecordConstructorBoundaryMarker(11);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(
			TEXT("const FBoundaryRetained%s RetainedGlobal = FBoundaryRetained%s();"),
			*Tag,
			*Tag));
		break;
	default:
		break;
	}
	AppendLine(Source);
}

void AppendEntry(FString& Source, const FCtorBoundaryParams& Params, const FString& FunctionName)
{
	const FString Tag = TypeTag(Params.Scenario);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	switch (Params.Scenario)
	{
	case ECtorBoundaryScenario::DuplicateDefaultSignature:
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryValue%s Value = FBoundaryValue%s();"), *Tag, *Tag));
		AppendLine(Source, TEXT("\treturn Value.Value;"));
		break;
	case ECtorBoundaryScenario::SuperAfterStatement:
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryDerived%s Object = FBoundaryDerived%s();"), *Tag, *Tag));
		AppendLine(Source, TEXT("\treturn Object.Value;"));
		break;
	case ECtorBoundaryScenario::RecursiveConstructorDirect:
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryRecursive%s Root = FBoundaryRecursive%s();"), *Tag, *Tag));
		AppendLine(Source, TEXT("\treturn 1;"));
		break;
	case ECtorBoundaryScenario::RecursiveConstructorIndirect:
		AppendLine(Source, FString::Printf(TEXT("\tFBoundaryRecursiveA%s Root = FBoundaryRecursiveA%s();"), *Tag, *Tag));
		AppendLine(Source, TEXT("\treturn 1;"));
		break;
	case ECtorBoundaryScenario::ModuleDiscardConstValueGlobal:
	case ECtorBoundaryScenario::EngineShutdownConstValueGlobal:
		AppendLine(Source, TEXT("\treturn RetainedGlobal.Value;"));
		break;
	case ECtorBoundaryScenario::DuplicateParameterSignature:
		AppendLine(Source, TEXT("\treturn 0;"));
		break;
	default:
		AppendLine(Source, TEXT("\treturn 0;"));
		break;
	}
	AppendLine(Source, TEXT("}"));
}

void AppendHelpers(FString& Source)
{
	AppendLine(Source, TEXT("void RecordConstructorBoundaryMarker(int Marker)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

FString BuildOwnedSource(const FCtorBoundaryParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendHelpers(Source);
	AppendScenarioTypes(Source, Params.Scenario);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FCtorBoundaryCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Scenario))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExecutionNotes = HostNotes;
	Info.bLimitedObservation = IsLimitedObservation(Cell);
	if (IsFault(Cell.Scenario))
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.ExpectedException = TEXT("Stack overflow");
		return Info;
	}
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.ExpectedReturn = ExpectedFromScenario(Cell.Scenario);
	return Info;
}

void AppendCaseHeader(FString& Source, const FCtorBoundaryCell& Cell)
{
	const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
	AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
	if (Info.EntryDeclaration.IsEmpty())
	{
		AppendLine(Source, TEXT("// EntryDeclaration:"));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
	}
	if (IsReject(Cell.Scenario))
	{
		AppendLine(Source, TEXT("// Outcome: CompileReject"));
		AppendLine(Source, TEXT("// Execution: SourceOnly"));
	}
	else if (IsFault(Cell.Scenario))
	{
		AppendLine(Source, TEXT("// Outcome: RuntimeException"));
		AppendLine(Source, TEXT("// ExpectedException: Stack overflow"));
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
	}
	else
	{
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
	}
	AppendLine(Source);
}
}

FStringView FCtorBoundaryGenerator::GetProductId() const
{
	return TEXT("LANG-CTOR-BOUNDARY");
}

TArray<FGeneratedCaseInfo> FCtorBoundaryGenerator::ListCases() const
{
	using namespace CtorBoundaryGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FCtorBoundaryCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("CtorBoundary PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FCtorBoundaryGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace CtorBoundaryGeneratorDetail;
	FCtorBoundaryCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FCtorBoundaryGenerator::BuildDumpSource() const
{
	using namespace CtorBoundaryGeneratorDetail;
	const TArray<FCtorBoundaryCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CTOR-BOUNDARY"));
	AppendLine(Source, TEXT("// Cases: 68 total, 20 normal, 40 reject, 8 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendHelpers(Source);

	TSet<uint8> EmittedScenarios;
	for (const FCtorBoundaryCell& Cell : Cells)
	{
		if (IsReject(Cell.Scenario))
		{
			continue;
		}
		const uint8 ScenarioKey = static_cast<uint8>(Cell.Scenario);
		if (!EmittedScenarios.Contains(ScenarioKey))
		{
			AppendScenarioTypes(Source, Cell.Scenario);
			EmittedScenarios.Add(ScenarioKey);
		}
	}

	bool bNeedBlank = false;
	for (const FCtorBoundaryCell& Cell : Cells)
	{
		if (IsReject(Cell.Scenario))
		{
			continue;
		}
		if (bNeedBlank)
		{
			AppendLine(Source);
		}
		AppendCaseHeader(Source, Cell);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}
	for (const FCtorBoundaryCell& Cell : Cells)
	{
		if (!IsReject(Cell.Scenario))
		{
			continue;
		}
		AppendLine(Source);
		AppendLine(Source, TEXT("// Reject module"));
		AppendCaseHeader(Source, Cell);
		AppendHelpers(Source);
		AppendScenarioTypes(Source, Cell.Scenario);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FCtorBoundaryGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace CtorBoundaryGeneratorDetail;
	const TArray<FCtorBoundaryCell> Cells = AllCells();
	FString Source;
	AppendHelpers(Source);
	TSet<uint8> EmittedScenarios;
	for (const FCtorBoundaryCell& Cell : Cells)
	{
		if (IsReject(Cell.Scenario))
		{
			continue;
		}
		const uint8 ScenarioKey = static_cast<uint8>(Cell.Scenario);
		if (!EmittedScenarios.Contains(ScenarioKey))
		{
			AppendScenarioTypes(Source, Cell.Scenario);
			EmittedScenarios.Add(ScenarioKey);
		}
	}
	int32 Emitted = 0;
	for (const FCtorBoundaryCell& Cell : Cells)
	{
		if (IsReject(Cell.Scenario))
		{
			continue;
		}
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

FString FCtorBoundaryGenerator::BuildConstructorBoundarySource(const FCtorBoundaryParams& Params) const
{
	using namespace CtorBoundaryGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Scenario))
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

int32 FCtorBoundaryGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace CtorBoundaryGeneratorDetail;
	FCtorBoundaryCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Scenario) || IsFault(Cell.Scenario))
	{
		return 0;
	}
	return ExpectedFromScenario(Cell.Scenario);
}

FString FCtorBoundaryGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace CtorBoundaryGeneratorDetail;
	FCtorBoundaryCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Scenario))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FCtorBoundaryGenerator::ListRejectCaseIds() const
{
	using namespace CtorBoundaryGeneratorDetail;
	TArray<FString> Ids;
	for (const FCtorBoundaryCell& Cell : AllCells())
	{
		if (IsReject(Cell.Scenario))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
