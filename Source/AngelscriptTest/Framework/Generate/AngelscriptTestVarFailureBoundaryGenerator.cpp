#include "Framework/Generate/AngelscriptTestVarFailureBoundaryGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace VarFailureBoundaryGeneratorDetail
{
constexpr int32 SupportedLocalCount = 128;
constexpr int32 PracticalBoundaryLocalCount = 2048;
constexpr int32 PressureObjectCount = 128;
constexpr int32 LongIdentifierLength = 512;

constexpr const TCHAR* UninitializedNotes = TEXT("Uninitialized read has no stable scalar oracle.");
constexpr const TCHAR* DiscardNotes = TEXT("Module discard observation is host-only.");

struct FFailureCell
{
	EVarFailureRecovery Recovery = EVarFailureRecovery::FreshModule;
	EVarFailureScenario Scenario = EVarFailureScenario::UseBeforeDeclaration;
};

const TCHAR* RecoveryToken(const EVarFailureRecovery Recovery)
{
	switch (Recovery)
	{
	case EVarFailureRecovery::FreshModule:
		return TEXT("FRESH_MODULE");
	case EVarFailureRecovery::SameModuleOrContext:
		return TEXT("SAME_MODULE_OR_CONTEXT");
	default:
		return nullptr;
	}
}

const TCHAR* ScenarioToken(const EVarFailureScenario Scenario)
{
	switch (Scenario)
	{
	case EVarFailureScenario::UseBeforeDeclaration:
		return TEXT("USE_BEFORE_DECLARATION");
	case EVarFailureScenario::UseAfterScope:
		return TEXT("USE_AFTER_SCOPE");
	case EVarFailureScenario::DuplicateSameScope:
		return TEXT("DUPLICATE_SAME_SCOPE");
	case EVarFailureScenario::IncompatibleInitializer:
		return TEXT("INCOMPATIBLE_INITIALIZER");
	case EVarFailureScenario::MutableGlobal:
		return TEXT("MUTABLE_GLOBAL");
	case EVarFailureScenario::ReferenceGlobal:
		return TEXT("REFERENCE_GLOBAL");
	case EVarFailureScenario::UninitializedRead:
		return TEXT("UNINITIALIZED_READ");
	case EVarFailureScenario::InitializerException:
		return TEXT("INITIALIZER_EXCEPTION");
	case EVarFailureScenario::FailedInitializerAtomicity:
		return TEXT("FAILED_INITIALIZER_ATOMICITY");
	case EVarFailureScenario::LongIdentifier:
		return TEXT("LONG_IDENTIFIER");
	case EVarFailureScenario::ManyLocalsSupported:
		return TEXT("MANY_LOCALS_SUPPORTED");
	case EVarFailureScenario::ManyLocalsBoundary:
		return TEXT("MANY_LOCALS_BOUNDARY");
	case EVarFailureScenario::StackFramePressure:
		return TEXT("STACK_FRAME_PRESSURE");
	case EVarFailureScenario::ModuleDiscard:
		return TEXT("MODULE_DISCARD");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FVarFailureBoundaryParams& Params)
{
	return RecoveryToken(Params.Recovery) != nullptr && ScenarioToken(Params.Scenario) != nullptr;
}

bool IsReject(const EVarFailureScenario Scenario)
{
	return static_cast<uint8>(Scenario) <= static_cast<uint8>(EVarFailureScenario::ReferenceGlobal);
}

bool IsFault(const EVarFailureScenario Scenario)
{
	return Scenario == EVarFailureScenario::InitializerException
		|| Scenario == EVarFailureScenario::FailedInitializerAtomicity;
}

int32 ExpectedFromScenario(const EVarFailureScenario Scenario)
{
	switch (Scenario)
	{
	case EVarFailureScenario::LongIdentifier:
	case EVarFailureScenario::ModuleDiscard:
		return 61;
	case EVarFailureScenario::ManyLocalsSupported:
	case EVarFailureScenario::StackFramePressure:
		return 128;
	case EVarFailureScenario::ManyLocalsBoundary:
		return 2048;
	default:
		return 0;
	}
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FFailureCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-VAR-FAILURE-BOUNDARY-%s-%s"),
		RecoveryToken(Cell.Recovery),
		ScenarioToken(Cell.Scenario));
}

FString MakeEntryName(const FFailureCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FVarFailureBoundaryParams ParamsFromCell(const FFailureCell& Cell)
{
	FVarFailureBoundaryParams Params;
	Params.Recovery = Cell.Recovery;
	Params.Scenario = Cell.Scenario;
	return Params;
}

TArray<FFailureCell> AllCells()
{
	TArray<FFailureCell> Cells;
	Cells.SetNum(28);
	for (int32 Index = 0; Index < 28; ++Index)
	{
		FFailureCell& Cell = Cells[Index];
		Cell.Recovery = static_cast<EVarFailureRecovery>(Index / 14);
		Cell.Scenario = static_cast<EVarFailureScenario>(Index % 14);
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

void AppendNativeValueHelper(FString& Source)
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
}

void AppendNativeReferenceHelper(FString& Source)
{
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
}

void AppendDiscardOwner(FString& Source)
{
	AppendLine(Source, TEXT("class FDiscardVariableOwner"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 61;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendSharedHelpers(FString& Source)
{
	AppendNativeValueHelper(Source);
	AppendNativeReferenceHelper(Source);
	AppendDiscardOwner(Source);
}

void AppendCompileFailureEntry(FString& Source, const EVarFailureScenario Scenario, const FString& FunctionName)
{
	if (Scenario == EVarFailureScenario::MutableGlobal)
	{
		AppendLine(Source, TEXT("int MutableBoundaryGlobal = 17;"));
		AppendLine(Source);
	}
	else if (Scenario == EVarFailureScenario::ReferenceGlobal)
	{
		AppendLine(Source, TEXT("FNativeCaseReference ReferenceBoundaryGlobal = CreateNativeCaseReference(17);"));
		AppendLine(Source);
	}

	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Scenario == EVarFailureScenario::UseBeforeDeclaration)
	{
		AppendLine(Source, TEXT("\tint Captured = Value;"));
		AppendLine(Source, TEXT("\tint Value = 19;"));
		AppendLine(Source, TEXT("\treturn Captured;"));
	}
	else if (Scenario == EVarFailureScenario::UseAfterScope)
	{
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tint ScopedValue = 23;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn ScopedValue;"));
	}
	else if (Scenario == EVarFailureScenario::DuplicateSameScope)
	{
		AppendLine(Source, TEXT("\tint DuplicateValue = 29;"));
		AppendLine(Source, TEXT("\tint DuplicateValue = 31;"));
		AppendLine(Source, TEXT("\treturn DuplicateValue;"));
	}
	else if (Scenario == EVarFailureScenario::IncompatibleInitializer)
	{
		AppendLine(Source, TEXT("\tint IncompatibleValue = FNativeCaseValue(37);"));
		AppendLine(Source, TEXT("\treturn IncompatibleValue;"));
	}
	else if (Scenario == EVarFailureScenario::MutableGlobal)
	{
		AppendLine(Source, TEXT("\treturn MutableBoundaryGlobal;"));
	}
	else
	{
		AppendLine(Source, TEXT("\treturn ReferenceBoundaryGlobal.Value;"));
	}
	AppendLine(Source, TEXT("}"));
}

void AppendManyLocalsEntry(FString& Source, const FString& FunctionName, const int32 LocalCount)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Sum = 0;"));
	for (int32 Index = 0; Index < LocalCount; ++Index)
	{
		AppendLine(Source, FString::Printf(TEXT("\tint Local%04d = 1;"), Index));
		AppendLine(Source, FString::Printf(TEXT("\tSum += Local%04d;"), Index));
	}
	AppendLine(Source, TEXT("\treturn Sum;"));
	AppendLine(Source, TEXT("}"));
}

void AppendStackPressureEntry(FString& Source, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	for (int32 Index = 0; Index < PressureObjectCount; ++Index)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFNativeCaseValue PressureValue%03d(%d);"), Index, 1000 + Index));
	}
	AppendLine(Source, TEXT("\tint Result = 0;"));
	for (int32 Index = 0; Index < PressureObjectCount; ++Index)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\tResult += PressureValue%03d.Value - %d;"),
			Index,
			999 + Index));
	}
	AppendLine(Source, TEXT("\treturn Result;"));
	AppendLine(Source, TEXT("}"));
}

void AppendEntry(FString& Source, const FVarFailureBoundaryParams& Params, const FString& FunctionName)
{
	if (IsReject(Params.Scenario))
	{
		AppendCompileFailureEntry(Source, Params.Scenario, FunctionName);
		return;
	}
	if (Params.Scenario == EVarFailureScenario::UninitializedRead)
	{
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value;"));
		AppendLine(Source, TEXT("\treturn Value;"));
		AppendLine(Source, TEXT("}"));
		return;
	}
	if (Params.Scenario == EVarFailureScenario::InitializerException)
	{
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Zero = 0;"));
		AppendLine(Source, TEXT("\tint Value = 41 / Zero;"));
		AppendLine(Source, TEXT("\treturn Value;"));
		AppendLine(Source, TEXT("}"));
		return;
	}
	if (Params.Scenario == EVarFailureScenario::FailedInitializerAtomicity)
	{
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFNativeCaseValue SourceValue(43);"));
		AppendLine(Source, TEXT("\tint Zero = 0;"));
		AppendLine(Source, TEXT("\tint Fault = 1 / Zero;"));
		AppendLine(Source, TEXT("\tFNativeCaseValue TargetValue(SourceValue);"));
		AppendLine(Source, TEXT("\treturn TargetValue.Value + Fault;"));
		AppendLine(Source, TEXT("}"));
		return;
	}
	if (Params.Scenario == EVarFailureScenario::LongIdentifier)
	{
		const FString Identifier = TEXT("LongVariable_") + FString::ChrN(LongIdentifierLength, TEXT('L'));
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tint %s = 61;"), *Identifier));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *Identifier));
		AppendLine(Source, TEXT("}"));
		return;
	}
	if (Params.Scenario == EVarFailureScenario::ManyLocalsSupported)
	{
		AppendManyLocalsEntry(Source, FunctionName, SupportedLocalCount);
		return;
	}
	if (Params.Scenario == EVarFailureScenario::ManyLocalsBoundary)
	{
		AppendManyLocalsEntry(Source, FunctionName, PracticalBoundaryLocalCount);
		return;
	}
	if (Params.Scenario == EVarFailureScenario::StackFramePressure)
	{
		AppendStackPressureEntry(Source, FunctionName);
		return;
	}

	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFDiscardVariableOwner Owner = FDiscardVariableOwner();"));
	AppendLine(Source, TEXT("\treturn Owner.Value;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FVarFailureBoundaryParams& Params, const FString& FunctionName)
{
	FString Source;
	if (Params.Scenario == EVarFailureScenario::IncompatibleInitializer
		|| Params.Scenario == EVarFailureScenario::FailedInitializerAtomicity
		|| Params.Scenario == EVarFailureScenario::StackFramePressure)
	{
		AppendNativeValueHelper(Source);
	}
	if (Params.Scenario == EVarFailureScenario::ReferenceGlobal)
	{
		AppendNativeReferenceHelper(Source);
	}
	if (Params.Scenario == EVarFailureScenario::ModuleDiscard)
	{
		AppendDiscardOwner(Source);
	}
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FFailureCell& Cell)
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
	if (IsFault(Cell.Scenario))
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.Execution = EGeneratedCaseExecution::Standalone;
		Info.ExpectedException = TEXT("Divide by zero");
		return Info;
	}
	if (Cell.Scenario == EVarFailureScenario::UninitializedRead)
	{
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		Info.bLimitedObservation = true;
		Info.ExecutionNotes = UninitializedNotes;
		return Info;
	}
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.ExpectedReturn = ExpectedFromScenario(Cell.Scenario);
	if (Cell.Scenario == EVarFailureScenario::ModuleDiscard)
	{
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		Info.bLimitedObservation = true;
		Info.ExecutionNotes = DiscardNotes;
	}
	else
	{
		Info.Execution = EGeneratedCaseExecution::Standalone;
	}
	return Info;
}
}

FStringView FVarFailureBoundaryGenerator::GetProductId() const
{
	return TEXT("LANG-VAR-FAILURE-BOUNDARY");
}

TArray<FGeneratedCaseInfo> FVarFailureBoundaryGenerator::ListCases() const
{
	using namespace VarFailureBoundaryGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FFailureCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cells[Index]);
			checkf(!EntryNames.Contains(EntryName), TEXT("VarFailureBoundary PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FVarFailureBoundaryGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace VarFailureBoundaryGeneratorDetail;
	FFailureCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FVarFailureBoundaryGenerator::BuildDumpSource() const
{
	using namespace VarFailureBoundaryGeneratorDetail;
	const TArray<FFailureCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-VAR-FAILURE-BOUNDARY"));
	AppendLine(Source, TEXT("// Cases: 28 total, 12 normal, 12 reject, 4 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);

	bool bNeedBlank = true;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FFailureCell& Cell = Cells[Index];
		if (IsReject(Cell.Scenario))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (bNeedBlank)
		{
			AppendLine(Source);
		}
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		if (IsFault(Cell.Scenario))
		{
			AppendLine(Source, TEXT("// Outcome: RuntimeException"));
			AppendLine(Source, TEXT("// ExpectedException: Divide by zero"));
			AppendLine(Source, TEXT("// Execution: Standalone"));
		}
		else if (Cell.Scenario == EVarFailureScenario::UninitializedRead)
		{
			AppendLine(Source, TEXT("// Outcome: ReturnValue"));
			AppendLine(Source, TEXT("// Execution: SourceOnly"));
			AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		}
		else
		{
			AppendLine(Source, TEXT("// Outcome: ReturnValue"));
			AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
			if (Cell.Scenario == EVarFailureScenario::ModuleDiscard)
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
		bNeedBlank = true;
	}

	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FFailureCell& Cell = Cells[Index];
		if (!IsReject(Cell.Scenario))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, TEXT("// Reject module"));
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, TEXT("// EntryDeclaration:"));
		AppendLine(Source, TEXT("// Outcome: CompileReject"));
		AppendLine(Source, TEXT("// Execution: SourceOnly"));
		AppendLine(Source);
		if (Cell.Scenario == EVarFailureScenario::IncompatibleInitializer)
		{
			AppendNativeValueHelper(Source);
		}
		if (Cell.Scenario == EVarFailureScenario::ReferenceGlobal)
		{
			AppendNativeReferenceHelper(Source);
		}
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FVarFailureBoundaryGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace VarFailureBoundaryGeneratorDetail;
	const TArray<FFailureCell> Cells = AllCells();
	FString Source;
	AppendSharedHelpers(Source);
	int32 Emitted = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Scenario))
		{
			continue;
		}
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

FString FVarFailureBoundaryGenerator::BuildBoundarySource(const FVarFailureBoundaryParams& Params) const
{
	using namespace VarFailureBoundaryGeneratorDetail;
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

int32 FVarFailureBoundaryGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace VarFailureBoundaryGeneratorDetail;
	FFailureCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Scenario) || IsFault(Cell.Scenario))
	{
		return 0;
	}
	return ExpectedFromScenario(Cell.Scenario);
}

FString FVarFailureBoundaryGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace VarFailureBoundaryGeneratorDetail;
	FFailureCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Scenario))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FVarFailureBoundaryGenerator::ListRejectCaseIds() const
{
	using namespace VarFailureBoundaryGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FFailureCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Scenario))
		{
			Ids.Add(MakeCaseId(Cells[Index]));
		}
	}
	return Ids;
}
}
