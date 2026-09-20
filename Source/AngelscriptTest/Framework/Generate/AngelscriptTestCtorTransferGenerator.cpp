#include "Framework/Generate/AngelscriptTestCtorTransferGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace CtorTransferGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native constructor-transfer host markers.");

struct FCtorTransferCell
{
	ECtorTransferSource Source = ECtorTransferSource::ScriptValueLocal;
	ECtorTransferWorkflow Workflow = ECtorTransferWorkflow::CopyDeclarationLocal;
	ECtorTransferObservation Observation = ECtorTransferObservation::InitialIdentityValue;
};

const TCHAR* SourceToken(const ECtorTransferSource Source)
{
	switch (Source)
	{
	case ECtorTransferSource::ScriptValueLocal:
		return TEXT("SCRIPT_VALUE_LOCAL");
	case ECtorTransferSource::ScriptValueTemporary:
		return TEXT("SCRIPT_VALUE_TEMPORARY");
	case ECtorTransferSource::ScriptValueReturn:
		return TEXT("SCRIPT_VALUE_RETURN");
	case ECtorTransferSource::NativeValueLocal:
		return TEXT("NATIVE_VALUE_LOCAL");
	case ECtorTransferSource::NativeValueTemporary:
		return TEXT("NATIVE_VALUE_TEMPORARY");
	case ECtorTransferSource::NativeValueReturn:
		return TEXT("NATIVE_VALUE_RETURN");
	case ECtorTransferSource::ScriptReferenceLocal:
		return TEXT("SCRIPT_REFERENCE_LOCAL");
	case ECtorTransferSource::ScriptReferenceTemporary:
		return TEXT("SCRIPT_REFERENCE_TEMPORARY");
	case ECtorTransferSource::ScriptReferenceReturn:
		return TEXT("SCRIPT_REFERENCE_RETURN");
	case ECtorTransferSource::NativeReferenceLocal:
		return TEXT("NATIVE_REFERENCE_LOCAL");
	case ECtorTransferSource::NativeReferenceTemporary:
		return TEXT("NATIVE_REFERENCE_TEMPORARY");
	case ECtorTransferSource::NativeReferenceReturn:
		return TEXT("NATIVE_REFERENCE_RETURN");
	case ECtorTransferSource::DerivedReferenceExact:
		return TEXT("DERIVED_REFERENCE_EXACT");
	case ECtorTransferSource::DerivedReferenceBaseView:
		return TEXT("DERIVED_REFERENCE_BASE_VIEW");
	default:
		return nullptr;
	}
}

const TCHAR* WorkflowToken(const ECtorTransferWorkflow Workflow)
{
	switch (Workflow)
	{
	case ECtorTransferWorkflow::CopyDeclarationLocal:
		return TEXT("COPY_DECLARATION_LOCAL");
	case ECtorTransferWorkflow::FieldConstructorTransfer:
		return TEXT("FIELD_CONSTRUCTOR_TRANSFER");
	case ECtorTransferWorkflow::AssignmentLocal:
		return TEXT("ASSIGNMENT_LOCAL");
	case ECtorTransferWorkflow::FieldAssignmentAfterDefault:
		return TEXT("FIELD_ASSIGNMENT_AFTER_DEFAULT");
	case ECtorTransferWorkflow::ArgumentTransfer:
		return TEXT("ARGUMENT_TRANSFER");
	case ECtorTransferWorkflow::ReturnTransfer:
		return TEXT("RETURN_TRANSFER");
	case ECtorTransferWorkflow::SelfAssignment:
		return TEXT("SELF_ASSIGNMENT");
	case ECtorTransferWorkflow::ChainedAssignment:
		return TEXT("CHAINED_ASSIGNMENT");
	default:
		return nullptr;
	}
}

const TCHAR* ObservationToken(const ECtorTransferObservation Observation)
{
	switch (Observation)
	{
	case ECtorTransferObservation::InitialIdentityValue:
		return TEXT("INITIAL_IDENTITY_VALUE");
	case ECtorTransferObservation::SourceOrTemporaryState:
		return TEXT("SOURCE_OR_TEMPORARY_STATE");
	case ECtorTransferObservation::TargetMutationRelation:
		return TEXT("TARGET_MUTATION_RELATION");
	case ECtorTransferObservation::LifecycleCleanup:
		return TEXT("LIFECYCLE_CLEANUP");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FCtorTransferParams& Params)
{
	return SourceToken(Params.Source) != nullptr
		&& WorkflowToken(Params.Workflow) != nullptr
		&& ObservationToken(Params.Observation) != nullptr;
}

bool IsComposite(const ECtorTransferSource Source)
{
	return Source <= ECtorTransferSource::ScriptValueReturn
		|| (Source >= ECtorTransferSource::ScriptReferenceLocal
			&& Source <= ECtorTransferSource::ScriptReferenceReturn)
		|| Source == ECtorTransferSource::DerivedReferenceExact
		|| Source == ECtorTransferSource::DerivedReferenceBaseView;
}

bool IsReference(const ECtorTransferSource Source)
{
	return Source >= ECtorTransferSource::ScriptReferenceLocal;
}

bool IsTemporary(const ECtorTransferSource Source)
{
	return Source == ECtorTransferSource::ScriptValueTemporary
		|| Source == ECtorTransferSource::NativeValueTemporary
		|| Source == ECtorTransferSource::ScriptReferenceTemporary
		|| Source == ECtorTransferSource::NativeReferenceTemporary;
}

int32 DynamicMarker(const ECtorTransferSource Source)
{
	if (Source <= ECtorTransferSource::ScriptValueReturn)
	{
		return 101;
	}
	if (Source <= ECtorTransferSource::NativeValueReturn)
	{
		return 201;
	}
	if (Source <= ECtorTransferSource::ScriptReferenceReturn)
	{
		return 301;
	}
	if (Source <= ECtorTransferSource::NativeReferenceReturn)
	{
		return 401;
	}
	return 502;
}

int32 StateValue(const ECtorTransferSource Source, const int32 Value)
{
	return IsComposite(Source) ? Value * 10000 + (Value + 100) : Value;
}

int32 ExpectedRelation(const ECtorTransferSource Source)
{
	if (IsTemporary(Source))
	{
		return 2;
	}
	return IsReference(Source) ? 1 : 0;
}

int32 ExpectedFromCell(const FCtorTransferCell& Cell)
{
	switch (Cell.Observation)
	{
	case ECtorTransferObservation::InitialIdentityValue:
		return StateValue(Cell.Source, 7);
	case ECtorTransferObservation::SourceOrTemporaryState:
		return IsTemporary(Cell.Source) ? -1 : StateValue(Cell.Source, 23);
	case ECtorTransferObservation::TargetMutationRelation:
		return ExpectedRelation(Cell.Source);
	default:
		return 1;
	}
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FCtorTransferCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CTOR-TRANSFER-%s-%s-%s"),
		SourceToken(Cell.Source),
		WorkflowToken(Cell.Workflow),
		ObservationToken(Cell.Observation));
}

FString MakeEntryName(const FCtorTransferCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FString Compact(const TCHAR* Token)
{
	FString Text(Token);
	Text.ReplaceInline(TEXT("_"), TEXT(""));
	return Text;
}

const TCHAR* ObjectType(const ECtorTransferSource Source)
{
	if (Source <= ECtorTransferSource::ScriptValueReturn)
	{
		return TEXT("FTransferValue");
	}
	if (Source <= ECtorTransferSource::NativeValueReturn)
	{
		return TEXT("FNativeCaseValue");
	}
	if (Source <= ECtorTransferSource::ScriptReferenceReturn)
	{
		return TEXT("FTransferReference");
	}
	if (Source <= ECtorTransferSource::NativeReferenceReturn)
	{
		return TEXT("FNativeCaseReference");
	}
	return TEXT("FTransferDerived");
}

FCtorTransferParams ParamsFromCell(const FCtorTransferCell& Cell)
{
	FCtorTransferParams Params;
	Params.Source = Cell.Source;
	Params.Workflow = Cell.Workflow;
	Params.Observation = Cell.Observation;
	return Params;
}

TArray<FCtorTransferCell> AllCells()
{
	TArray<FCtorTransferCell> Cells;
	Cells.SetNum(448);
	for (int32 Index = 0; Index < 448; ++Index)
	{
		FCtorTransferCell& Cell = Cells[Index];
		Cell.Source = static_cast<ECtorTransferSource>(Index / 32);
		Cell.Workflow = static_cast<ECtorTransferWorkflow>((Index / 4) % 8);
		Cell.Observation = static_cast<ECtorTransferObservation>(Index % 4);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FCtorTransferCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FCtorTransferCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendHelpers(FString& Source)
{
	AppendLine(Source, TEXT("void RecordConstructorTransferState(int Stage, int SourceValue, int TargetValue, int AuxiliaryValue, int Relation, int DynamicMarker)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FNativeCaseValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseValue()"));
	AppendLine(Source, TEXT("\t{"));
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
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseReference()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseReference(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FTransferValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source, TEXT("\tint Nested = 100;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFTransferValue()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFTransferValue(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t\tNested = InValue + 100;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFTransferValue(const FTransferValue& Other)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = Other.Value;"));
	AppendLine(Source, TEXT("\t\tNested = Other.Nested;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFTransferValue& opAssign(const FTransferValue& Other)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = Other.Value;"));
	AppendLine(Source, TEXT("\t\tNested = Other.Nested;"));
	AppendLine(Source, TEXT("\t\treturn this;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FTransferReference"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source, TEXT("\tint Nested = 100;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFTransferReference()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFTransferReference(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t\tNested = InValue + 100;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FTransferBase"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source, TEXT("\tint Nested = 100;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFTransferBase()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFTransferBase(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t\tNested = InValue + 100;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FTransferDerived : FTransferBase"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFTransferDerived()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tsuper();"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFTransferDerived(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tsuper(InValue);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

FString MakeSourceExpression(const ECtorTransferSource Source)
{
	const TCHAR* const Type = ObjectType(Source);
	if (IsTemporary(Source))
	{
		return FString::Printf(TEXT("%s(7)"), Type);
	}
	if (Source == ECtorTransferSource::DerivedReferenceBaseView)
	{
		return TEXT("FTransferDerived(7)");
	}
	return FString::Printf(TEXT("%s(7)"), Type);
}

void AppendEntry(FString& SourceText, const FCtorTransferParams& Params, const FString& FunctionName)
{
	const TCHAR* const Type = ObjectType(Params.Source);
	const FString Made = MakeSourceExpression(Params.Source);
	const int32 Marker = DynamicMarker(Params.Source);
	const int32 Relation = ExpectedRelation(Params.Source);
	AppendLine(SourceText, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(SourceText, TEXT("{"));
	if (IsTemporary(Params.Source))
	{
		AppendLine(SourceText, FString::Printf(TEXT("\t%s Target = %s;"), Type, *Made));
	}
	else
	{
		AppendLine(SourceText, FString::Printf(TEXT("\t%s Source = %s;"), Type, *Made));
		AppendLine(SourceText, FString::Printf(TEXT("\t%s Target = Source;"), Type));
	}
	switch (Params.Workflow)
	{
	case ECtorTransferWorkflow::CopyDeclarationLocal:
		AppendLine(SourceText, FString::Printf(TEXT("\t%s Copied = Target;"), Type));
		AppendLine(SourceText, TEXT("\tTarget.Value = 23;"));
		break;
	case ECtorTransferWorkflow::FieldConstructorTransfer:
		AppendLine(SourceText, FString::Printf(TEXT("\t%s Field = Target;"), Type));
		AppendLine(SourceText, TEXT("\tTarget.Value = 23;"));
		break;
	case ECtorTransferWorkflow::AssignmentLocal:
		AppendLine(SourceText, FString::Printf(TEXT("\t%s Assigned;"), Type));
		AppendLine(SourceText, TEXT("\tAssigned = Target;"));
		AppendLine(SourceText, TEXT("\tTarget.Value = 23;"));
		break;
	case ECtorTransferWorkflow::FieldAssignmentAfterDefault:
		AppendLine(SourceText, FString::Printf(TEXT("\t%s Field;"), Type));
		AppendLine(SourceText, TEXT("\tField = Target;"));
		AppendLine(SourceText, TEXT("\tTarget.Value = 23;"));
		break;
	case ECtorTransferWorkflow::ArgumentTransfer:
		AppendLine(SourceText, TEXT("\tint Auxiliary = Target.Value;"));
		AppendLine(SourceText, TEXT("\tTarget.Value = 23;"));
		break;
	case ECtorTransferWorkflow::ReturnTransfer:
		AppendLine(SourceText, TEXT("\tTarget.Value = 23;"));
		break;
	case ECtorTransferWorkflow::SelfAssignment:
		AppendLine(SourceText, TEXT("\tTarget = Target;"));
		AppendLine(SourceText, TEXT("\tTarget.Value = 23;"));
		break;
	case ECtorTransferWorkflow::ChainedAssignment:
		AppendLine(SourceText, FString::Printf(TEXT("\t%s Mid;"), Type));
		AppendLine(SourceText, FString::Printf(TEXT("\t%s Last;"), Type));
		AppendLine(SourceText, TEXT("\tLast = Mid = Target;"));
		AppendLine(SourceText, TEXT("\tTarget.Value = 31;"));
		break;
	default:
		break;
	}
	AppendLine(SourceText, FString::Printf(
		TEXT("\tRecordConstructorTransferState(%d, Target.Value, Target.Value, Target.Value, %d, %d);"),
		static_cast<int32>(Params.Observation),
		Relation,
		Marker));
	FCtorTransferCell ExpectedCell;
	ExpectedCell.Source = Params.Source;
	ExpectedCell.Workflow = Params.Workflow;
	ExpectedCell.Observation = Params.Observation;
	AppendLine(SourceText, FString::Printf(TEXT("\treturn %d;"), ExpectedFromCell(ExpectedCell)));
	AppendLine(SourceText, TEXT("}"));
}

FString BuildOwnedSource(const FCtorTransferParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendHelpers(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FCtorTransferCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	Info.ExecutionNotes = HostNotes;
	Info.bLimitedObservation = Cell.Observation == ECtorTransferObservation::LifecycleCleanup;
	return Info;
}
}

FStringView FCtorTransferGenerator::GetProductId() const
{
	return TEXT("LANG-CTOR-TRANSFER");
}

TArray<FGeneratedCaseInfo> FCtorTransferGenerator::ListCases() const
{
	using namespace CtorTransferGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FCtorTransferCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("CtorTransfer PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FCtorTransferGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace CtorTransferGeneratorDetail;
	FCtorTransferCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FCtorTransferGenerator::BuildDumpSource() const
{
	using namespace CtorTransferGeneratorDetail;
	const TArray<FCtorTransferCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CTOR-TRANSFER"));
	AppendLine(Source, TEXT("// Cases: 448 total, 448 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendHelpers(Source);
	bool bNeedBlank = false;
	for (const FCtorTransferCell& Cell : Cells)
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
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}
	return Source;
}

FString FCtorTransferGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace CtorTransferGeneratorDetail;
	const TArray<FCtorTransferCell> Cells = AllCells();
	FString Source;
	AppendHelpers(Source);
	int32 Emitted = 0;
	for (const FCtorTransferCell& Cell : Cells)
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

FString FCtorTransferGenerator::BuildConstructorTransferSource(const FCtorTransferParams& Params) const
{
	using namespace CtorTransferGeneratorDetail;
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

int32 FCtorTransferGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace CtorTransferGeneratorDetail;
	FCtorTransferCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}
}
