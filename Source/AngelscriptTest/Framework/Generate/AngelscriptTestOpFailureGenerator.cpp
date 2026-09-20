#include "Framework/Generate/AngelscriptTestOpFailureGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace OpFailureGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native RecordOperatorStage and FNativeCaseValue fixtures.");
constexpr const TCHAR* HostFaultNotes = TEXT("Approximated host callback exception; not a native-engine string.");

struct FFailureCell
{
	EOpFailureFamily Family = EOpFailureFamily::InvalidShiftCount;
	EOpFailureObservation Observation = EOpFailureObservation::DiagnosticOrException;
	EOpFailureRecovery Recovery = EOpFailureRecovery::FreshModule;
};

const TCHAR* FamilyToken(const EOpFailureFamily Family)
{
	switch (Family)
	{
	case EOpFailureFamily::UnsupportedOperand:
		return TEXT("UNSUPPORTED_OPERAND");
	case EOpFailureFamily::DivideZero:
		return TEXT("DIVIDE_ZERO");
	case EOpFailureFamily::ModuloZero:
		return TEXT("MODULO_ZERO");
	case EOpFailureFamily::InvalidShiftCount:
		return TEXT("INVALID_SHIFT_COUNT");
	case EOpFailureFamily::SignedOverflow:
		return TEXT("SIGNED_OVERFLOW");
	case EOpFailureFamily::SignedRemainderOverflow:
		return TEXT("SIGNED_REMAINDER_OVERFLOW");
	case EOpFailureFamily::PowerOverflow:
		return TEXT("POWER_OVERFLOW");
	case EOpFailureFamily::InvalidLValue:
		return TEXT("INVALID_LVALUE");
	case EOpFailureFamily::ConstMutation:
		return TEXT("CONST_MUTATION");
	case EOpFailureFamily::MissingOverload:
		return TEXT("MISSING_OVERLOAD");
	case EOpFailureFamily::AmbiguousOverload:
		return TEXT("AMBIGUOUS_OVERLOAD");
	case EOpFailureFamily::InvalidSignature:
		return TEXT("INVALID_SIGNATURE");
	case EOpFailureFamily::DuplicateOperator:
		return TEXT("DUPLICATE_OPERATOR");
	case EOpFailureFamily::NullReceiver:
		return TEXT("NULL_RECEIVER");
	case EOpFailureFamily::LeftOperandException:
		return TEXT("LEFT_OPERAND_EXCEPTION");
	case EOpFailureFamily::RightOperandException:
		return TEXT("RIGHT_OPERAND_EXCEPTION");
	case EOpFailureFamily::AssignmentException:
		return TEXT("ASSIGNMENT_EXCEPTION");
	default:
		return nullptr;
	}
}

const TCHAR* ObservationToken(const EOpFailureObservation Observation)
{
	switch (Observation)
	{
	case EOpFailureObservation::DiagnosticOrException:
		return TEXT("DIAGNOSTIC_OR_EXCEPTION");
	case EOpFailureObservation::Cleanup:
		return TEXT("CLEANUP");
	case EOpFailureObservation::RecoveryResult:
		return TEXT("RECOVERY_RESULT");
	default:
		return nullptr;
	}
}

const TCHAR* RecoveryToken(const EOpFailureRecovery Recovery)
{
	switch (Recovery)
	{
	case EOpFailureRecovery::FreshModule:
		return TEXT("FRESH_MODULE");
	case EOpFailureRecovery::SameModuleOrContext:
		return TEXT("SAME_MODULE_OR_CONTEXT");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FOpFailureParams& Params)
{
	return FamilyToken(Params.Family) != nullptr
		&& ObservationToken(Params.Observation) != nullptr
		&& RecoveryToken(Params.Recovery) != nullptr;
}

bool IsRejectFamily(const EOpFailureFamily Family)
{
	return Family == EOpFailureFamily::UnsupportedOperand
		|| Family == EOpFailureFamily::InvalidLValue
		|| Family == EOpFailureFamily::ConstMutation
		|| Family == EOpFailureFamily::MissingOverload
		|| Family == EOpFailureFamily::AmbiguousOverload
		|| Family == EOpFailureFamily::InvalidSignature
		|| Family == EOpFailureFamily::DuplicateOperator;
}

bool IsMaskedShift(const EOpFailureFamily Family)
{
	return Family == EOpFailureFamily::InvalidShiftCount;
}

bool IsHostFault(const EOpFailureFamily Family)
{
	return Family == EOpFailureFamily::LeftOperandException
		|| Family == EOpFailureFamily::RightOperandException
		|| Family == EOpFailureFamily::AssignmentException;
}

const TCHAR* ExceptionText(const EOpFailureFamily Family)
{
	switch (Family)
	{
	case EOpFailureFamily::DivideZero:
	case EOpFailureFamily::ModuloZero:
		return TEXT("Divide by zero");
	case EOpFailureFamily::SignedOverflow:
	case EOpFailureFamily::SignedRemainderOverflow:
		return TEXT("Overflow in integer division");
	case EOpFailureFamily::PowerOverflow:
		return TEXT("Overflow in exponent operation");
	case EOpFailureFamily::NullReceiver:
		return TEXT("Null pointer access");
	case EOpFailureFamily::LeftOperandException:
		return TEXT("Operator left operand exception");
	case EOpFailureFamily::RightOperandException:
		return TEXT("Operator right operand exception");
	case EOpFailureFamily::AssignmentException:
		return TEXT("Operator assignment exception");
	default:
		return nullptr;
	}
}

int32 ExpectedFromFamily(const EOpFailureFamily Family)
{
	if (!IsMaskedShift(Family))
	{
		return 0;
	}
	return static_cast<int32>(1u << 31);
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FFailureCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-FAILURE-%s-%s-%s"),
		FamilyToken(Cell.Family),
		ObservationToken(Cell.Observation),
		RecoveryToken(Cell.Recovery));
}

FString MakeEntryName(const FFailureCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpFailureParams ParamsFromCell(const FFailureCell& Cell)
{
	FOpFailureParams Params;
	Params.Family = Cell.Family;
	Params.Observation = Cell.Observation;
	Params.Recovery = Cell.Recovery;
	return Params;
}

TArray<FFailureCell> AllCells()
{
	TArray<FFailureCell> Cells;
	Cells.SetNum(102);
	for (int32 Index = 0; Index < 102; ++Index)
	{
		FFailureCell& Cell = Cells[Index];
		Cell.Family = static_cast<EOpFailureFamily>(Index / 6);
		Cell.Observation = static_cast<EOpFailureObservation>((Index / 2) % 3);
		Cell.Recovery = static_cast<EOpFailureRecovery>(Index % 2);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FFailureCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FFailureCell> Cells = AllCells();
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

void AppendThrowingAssignment(FString& Source)
{
	AppendLine(Source, TEXT("struct FThrowingAssignment"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFThrowingAssignment& opAssign(const FThrowingAssignment& Other)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tThrowOperatorStage(3); // ASSIGNMENT_CAUSE"));
	AppendLine(Source, TEXT("\t\treturn this;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
}

void AppendEntryStart(FString& Source, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseValue ScopeValue(77);"));
}

void AppendFamilyPrelude(FString& Source, const EOpFailureFamily Family)
{
	switch (Family)
	{
	case EOpFailureFamily::MissingOverload:
		AppendLine(Source, TEXT("struct FMissingOperator"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		break;
	case EOpFailureFamily::AmbiguousOverload:
		AppendLine(Source, TEXT("int ResolveAmbiguousOperator(int64 Value)"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn 1;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, TEXT("int ResolveAmbiguousOperator(uint64 Value)"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn 2;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		break;
	case EOpFailureFamily::InvalidSignature:
		AppendLine(Source, TEXT("struct FInvalidOperatorSignature"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value;"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\tFInvalidOperatorSignature opAdd() const"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn FInvalidOperatorSignature();"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		break;
	case EOpFailureFamily::DuplicateOperator:
		AppendLine(Source, TEXT("struct FDuplicateOperator"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint opAdd(const FDuplicateOperator& Other) const"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 1;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\tint opAdd(const FDuplicateOperator& Other) const // OP_CAUSE"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 2;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		break;
	case EOpFailureFamily::AssignmentException:
		AppendThrowingAssignment(Source);
		AppendLine(Source);
		break;
	default:
		break;
	}
}

void AppendFamilyBody(FString& Source, const EOpFailureFamily Family)
{
	switch (Family)
	{
	case EOpFailureFamily::UnsupportedOperand:
		AppendLine(Source, TEXT("\tbool Left = true;"));
		AppendLine(Source, TEXT("\tbool Right = false;"));
		AppendLine(Source, TEXT("\treturn Left + Right; // OP_CAUSE"));
		break;
	case EOpFailureFamily::DivideZero:
		AppendLine(Source, TEXT("\tint Numerator = RecordOperatorStage(1);"));
		AppendLine(Source, TEXT("\tint Divisor = RecordOperatorStage(0);"));
		AppendLine(Source, TEXT("\treturn (Numerator / Divisor) + RecordOperatorStage(9); // OP_CAUSE"));
		break;
	case EOpFailureFamily::ModuloZero:
		AppendLine(Source, TEXT("\tint Dividend = RecordOperatorStage(1);"));
		AppendLine(Source, TEXT("\tint Divisor = RecordOperatorStage(0);"));
		AppendLine(Source, TEXT("\treturn (Dividend % Divisor) + RecordOperatorStage(9); // OP_CAUSE"));
		break;
	case EOpFailureFamily::InvalidShiftCount:
		AppendLine(Source, TEXT("\tuint Value = 1;"));
		AppendLine(Source, TEXT("\tint Count = -1;"));
		AppendLine(Source, TEXT("\treturn int(Value << Count); // OP_CAUSE"));
		break;
	case EOpFailureFamily::SignedOverflow:
		AppendLine(Source, TEXT("\tint64 Numerator = -9223372036854775807 - 1;"));
		AppendLine(Source, TEXT("\tint64 Divisor = -1;"));
		AppendLine(Source, TEXT("\treturn int(Numerator / Divisor) + RecordOperatorStage(9); // OP_CAUSE"));
		break;
	case EOpFailureFamily::SignedRemainderOverflow:
		AppendLine(Source, TEXT("\tint64 Dividend = -9223372036854775807 - 1;"));
		AppendLine(Source, TEXT("\tint64 Divisor = -1;"));
		AppendLine(Source, TEXT("\treturn int(Dividend % Divisor) + RecordOperatorStage(9); // OP_CAUSE"));
		break;
	case EOpFailureFamily::PowerOverflow:
		AppendLine(Source, TEXT("\tfloat Base = 1.7976931348623157e308;"));
		AppendLine(Source, TEXT("\tint Exponent = 2;"));
		AppendLine(Source, TEXT("\treturn int(Base ** Exponent + RecordOperatorStage(9)); // OP_CAUSE"));
		break;
	case EOpFailureFamily::InvalidLValue:
		AppendLine(Source, TEXT("\tint Value = 7;"));
		AppendLine(Source, TEXT("\treturn (Value + 1) = 3; // OP_CAUSE"));
		break;
	case EOpFailureFamily::ConstMutation:
		AppendLine(Source, TEXT("\tconst int Value = 7;"));
		AppendLine(Source, TEXT("\tValue += 1; // OP_CAUSE"));
		AppendLine(Source, TEXT("\treturn Value;"));
		break;
	case EOpFailureFamily::MissingOverload:
		AppendLine(Source, TEXT("\tFMissingOperator Left;"));
		AppendLine(Source, TEXT("\tFMissingOperator Right;"));
		AppendLine(Source, TEXT("\treturn (Left - Right).Value; // OP_CAUSE"));
		break;
	case EOpFailureFamily::AmbiguousOverload:
		AppendLine(Source, TEXT("\treturn ResolveAmbiguousOperator(null); // OP_CAUSE"));
		break;
	case EOpFailureFamily::InvalidSignature:
		AppendLine(Source, TEXT("\tFInvalidOperatorSignature Left;"));
		AppendLine(Source, TEXT("\tFInvalidOperatorSignature Right;"));
		AppendLine(Source, TEXT("\treturn (Left + Right).Value; // OP_CAUSE"));
		break;
	case EOpFailureFamily::DuplicateOperator:
		AppendLine(Source, TEXT("\treturn 0;"));
		break;
	case EOpFailureFamily::NullReceiver:
		AppendLine(Source, TEXT("\tFNativeCaseReference Receiver = nullptr;"));
		AppendLine(Source, TEXT("\treturn Receiver.Value + RecordOperatorStage(9); // OP_CAUSE"));
		break;
	case EOpFailureFamily::LeftOperandException:
		AppendLine(Source, TEXT("\treturn ThrowOperatorStage(1) + RecordOperatorStage(2); // OP_CAUSE"));
		break;
	case EOpFailureFamily::RightOperandException:
		AppendLine(Source, TEXT("\treturn RecordOperatorStage(1) + ThrowOperatorStage(2); // OP_CAUSE"));
		break;
	case EOpFailureFamily::AssignmentException:
		AppendLine(Source, TEXT("\tFThrowingAssignment Left;"));
		AppendLine(Source, TEXT("\tFThrowingAssignment Right;"));
		AppendLine(Source, TEXT("\tLeft = Right;"));
		AppendLine(Source, TEXT("\treturn RecordOperatorStage(4);"));
		break;
	default:
		AppendLine(Source, TEXT("\treturn 0;"));
		break;
	}
}

void AppendEntry(FString& Source, const FOpFailureParams& Params, const FString& FunctionName)
{
	AppendFamilyPrelude(Source, Params.Family);
	AppendEntryStart(Source, FunctionName);
	AppendFamilyBody(Source, Params.Family);
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FOpFailureParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FFailureCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsRejectFamily(Cell.Family))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	if (IsMaskedShift(Cell.Family))
	{
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.ExpectedReturn = ExpectedFromFamily(Cell.Family);
		Info.bLimitedObservation = true;
		Info.ExecutionNotes = TEXT("Masked 32-bit shift count; int32 is not full-width proof.");
		return Info;
	}
	Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
	Info.ExpectedException = ExceptionText(Cell.Family);
	if (IsHostFault(Cell.Family))
	{
		Info.bLimitedObservation = true;
		Info.ExecutionNotes = HostFaultNotes;
	}
	else
	{
		Info.ExecutionNotes = HostNotes;
	}
	return Info;
}
}

FStringView FOpFailureGenerator::GetProductId() const
{
	return TEXT("LANG-OP-FAILURE");
}

TArray<FGeneratedCaseInfo> FOpFailureGenerator::ListCases() const
{
	using namespace OpFailureGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FFailureCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cells[Index]);
			checkf(!EntryNames.Contains(EntryName), TEXT("OpFailure PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FOpFailureGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace OpFailureGeneratorDetail;
	FFailureCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FOpFailureGenerator::BuildDumpSource() const
{
	using namespace OpFailureGeneratorDetail;
	const TArray<FFailureCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-FAILURE"));
	AppendLine(Source, TEXT("// Cases: 102 total, 6 normal, 42 reject, 54 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	bool bEmittedAggregateHelper = false;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsRejectFamily(Cells[Index].Family))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		AppendLine(Source);
		if (!bEmittedAggregateHelper && Cells[Index].Family == EOpFailureFamily::AssignmentException)
		{
			AppendThrowingAssignment(Source);
			AppendLine(Source);
			bEmittedAggregateHelper = true;
		}
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		if (IsMaskedShift(Cells[Index].Family))
		{
			AppendLine(Source, TEXT("// Outcome: ReturnValue"));
			AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		}
		else
		{
			AppendLine(Source, TEXT("// Outcome: RuntimeException"));
			AppendLine(Source, FString::Printf(TEXT("// ExpectedException: %s"), *Info.ExpectedException));
		}
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		AppendLine(Source);
		AppendEntryStart(Source, MakeEntryName(Cells[Index]));
		AppendFamilyBody(Source, Cells[Index].Family);
		AppendLine(Source, TEXT("}"));
	}
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (!IsRejectFamily(Cells[Index].Family))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		AppendLine(Source);
		AppendLine(Source, TEXT("// Reject module"));
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, TEXT("// EntryDeclaration:"));
		AppendLine(Source, TEXT("// Outcome: CompileReject"));
		AppendLine(Source, TEXT("// Execution: SourceOnly"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

FString FOpFailureGenerator::BuildFailureSource(const FOpFailureParams& Params) const
{
	using namespace OpFailureGeneratorDetail;
	if (!IsValidParams(Params) || IsRejectFamily(Params.Family))
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

FString FOpFailureGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace OpFailureGeneratorDetail;
	const TArray<FFailureCell> Cells = AllCells();
	FString Source;
	bool bNeedBlank = false;
	bool bEmittedAssignment = false;
	int32 Emitted = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsRejectFamily(Cells[Index].Family))
		{
			continue;
		}
		if (!bEmittedAssignment && Cells[Index].Family == EOpFailureFamily::AssignmentException)
		{
			if (bNeedBlank)
			{
				AppendLine(Source);
			}
			AppendThrowingAssignment(Source);
			bEmittedAssignment = true;
			bNeedBlank = true;
		}
		if (bNeedBlank)
		{
			AppendLine(Source);
		}
		AppendEntryStart(Source, MakeEntryName(Cells[Index]));
		AppendFamilyBody(Source, Cells[Index].Family);
		AppendLine(Source, TEXT("}"));
		bNeedBlank = true;
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

int32 FOpFailureGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpFailureGeneratorDetail;
	FFailureCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsRejectFamily(Cell.Family) || !IsMaskedShift(Cell.Family))
	{
		return 0;
	}
	return ExpectedFromFamily(Cell.Family);
}

FString FOpFailureGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace OpFailureGeneratorDetail;
	FFailureCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsRejectFamily(Cell.Family))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FOpFailureGenerator::ListRejectCaseIds() const
{
	using namespace OpFailureGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FFailureCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsRejectFamily(Cells[Index].Family))
		{
			Ids.Add(MakeCaseId(Cells[Index]));
		}
	}
	return Ids;
}
}
