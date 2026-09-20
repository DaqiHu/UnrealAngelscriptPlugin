#include "Framework/Generate/AngelscriptTestPropFailureGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace PropFailureGeneratorDetail
{
constexpr int32 FailureCount = 14;
constexpr int32 ProbeCount = 2;
constexpr int32 RecoveryCount = 2;
constexpr int32 TotalCells = FailureCount * ProbeCount * RecoveryCount;

struct FFailureCell
{
	EPropFailureKind Failure = EPropFailureKind::RemovedPropertyDecorator;
	EPropFailureProbe Probe = EPropFailureProbe::Direct;
	EPropFailureRecovery Recovery = EPropFailureRecovery::FreshModule;
};

const TCHAR* FailureToken(const EPropFailureKind Failure)
{
	switch (Failure)
	{
	case EPropFailureKind::RemovedPropertyDecorator:
		return TEXT("REMOVED_PROPERTY_DECORATOR");
	case EPropFailureKind::RemovedVirtualProperty:
		return TEXT("REMOVED_VIRTUAL_PROPERTY");
	case EPropFailureKind::MissingGetter:
		return TEXT("MISSING_GETTER");
	case EPropFailureKind::MissingSetter:
		return TEXT("MISSING_SETTER");
	case EPropFailureKind::RegisteredMismatchedTypes:
		return TEXT("REGISTERED_MISMATCHED_TYPES");
	case EPropFailureKind::RegisteredDuplicateGetter:
		return TEXT("REGISTERED_DUPLICATE_GETTER");
	case EPropFailureKind::RegisteredDuplicateSetter:
		return TEXT("REGISTERED_DUPLICATE_SETTER");
	case EPropFailureKind::RecursiveGetter:
		return TEXT("RECURSIVE_GETTER");
	case EPropFailureKind::RecursiveSetter:
		return TEXT("RECURSIVE_SETTER");
	case EPropFailureKind::ThrowingGetter:
		return TEXT("THROWING_GETTER");
	case EPropFailureKind::ThrowingSetter:
		return TEXT("THROWING_SETTER");
	case EPropFailureKind::NullReceiver:
		return TEXT("NULL_RECEIVER");
	case EPropFailureKind::InaccessibleField:
		return TEXT("INACCESSIBLE_FIELD");
	case EPropFailureKind::CompoundValueReceiver:
		return TEXT("COMPOUND_VALUE_RECEIVER");
	default:
		return nullptr;
	}
}

const TCHAR* ProbeToken(const EPropFailureProbe Probe)
{
	switch (Probe)
	{
	case EPropFailureProbe::Direct:
		return TEXT("DIRECT");
	case EPropFailureProbe::AlternatePath:
		return TEXT("ALTERNATE_PATH");
	default:
		return nullptr;
	}
}

const TCHAR* RecoveryToken(const EPropFailureRecovery Recovery)
{
	switch (Recovery)
	{
	case EPropFailureRecovery::FreshModule:
		return TEXT("FRESH_MODULE");
	case EPropFailureRecovery::SameModuleOrContext:
		return TEXT("SAME_MODULE_OR_CONTEXT");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FPropFailureParams& Params)
{
	return FailureToken(Params.Failure) != nullptr
		&& ProbeToken(Params.Probe) != nullptr
		&& RecoveryToken(Params.Recovery) != nullptr;
}

bool IsReject(const EPropFailureKind Failure)
{
	return Failure == EPropFailureKind::RemovedPropertyDecorator
		|| Failure == EPropFailureKind::RemovedVirtualProperty
		|| Failure == EPropFailureKind::MissingGetter
		|| Failure == EPropFailureKind::MissingSetter
		|| Failure == EPropFailureKind::RegisteredMismatchedTypes
		|| Failure == EPropFailureKind::RegisteredDuplicateGetter
		|| Failure == EPropFailureKind::RegisteredDuplicateSetter
		|| Failure == EPropFailureKind::InaccessibleField
		|| Failure == EPropFailureKind::CompoundValueReceiver;
}

bool IsNullFault(const EPropFailureKind Failure)
{
	return Failure == EPropFailureKind::NullReceiver;
}

bool IsDivideFault(const EPropFailureKind Failure)
{
	return Failure == EPropFailureKind::RecursiveGetter
		|| Failure == EPropFailureKind::RecursiveSetter
		|| Failure == EPropFailureKind::ThrowingGetter
		|| Failure == EPropFailureKind::ThrowingSetter;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FFailureCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-PROP-FAILURE-%s-%s-%s"),
		FailureToken(Cell.Failure),
		ProbeToken(Cell.Probe),
		RecoveryToken(Cell.Recovery));
}

FString MakeEntryName(const FFailureCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FPropFailureParams ParamsFromCell(const FFailureCell& Cell)
{
	FPropFailureParams Params;
	Params.Failure = Cell.Failure;
	Params.Probe = Cell.Probe;
	Params.Recovery = Cell.Recovery;
	return Params;
}

TArray<FFailureCell> AllCells()
{
	TArray<FFailureCell> Cells;
	Cells.SetNum(TotalCells);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		FFailureCell& Cell = Cells[Index];
		Cell.Failure = static_cast<EPropFailureKind>(Index / (ProbeCount * RecoveryCount));
		Cell.Probe = static_cast<EPropFailureProbe>((Index / RecoveryCount) % ProbeCount);
		Cell.Recovery = static_cast<EPropFailureRecovery>(Index % RecoveryCount);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FFailureCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FFailureCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendRemovedSyntax(FString& Source, const FPropFailureParams& Params)
{
	AppendLine(Source, TEXT("class FRemovedPropertyOwner"));
	AppendLine(Source, TEXT("{"));
	if (Params.Probe == EPropFailureProbe::AlternatePath)
	{
		AppendLine(Source, TEXT("\tint Stable = 3;"));
		AppendLine(Source);
	}
	if (Params.Failure == EPropFailureKind::RemovedPropertyDecorator)
	{
		AppendLine(Source, TEXT("\tint GetValue() property"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, Params.Probe == EPropFailureProbe::AlternatePath
			? TEXT("\t\treturn Stable;")
			: TEXT("\t\treturn 3;"));
		AppendLine(Source, TEXT("\t}"));
	}
	else
	{
		AppendLine(Source, TEXT("\tint Value"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tget"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, Params.Probe == EPropFailureProbe::AlternatePath
			? TEXT("\t\t\treturn Stable;")
			: TEXT("\t\t\treturn 3;"));
		AppendLine(Source, TEXT("\t\t}"));
		AppendLine(Source, TEXT("\t}"));
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendInaccessible(FString& Source, const FPropFailureParams& Params, const FString& FunctionName)
{
	AppendLine(Source, TEXT("class FPrivatePropertyOwner"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tprivate int Hidden = 7;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	if (Params.Probe == EPropFailureProbe::AlternatePath)
	{
		AppendLine(Source, TEXT("class FPrivatePropertyProbe"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Invoke(FPrivatePropertyOwner Receiver)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn Receiver.Hidden;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFPrivatePropertyOwner Receiver = FPrivatePropertyOwner();"));
	if (Params.Probe == EPropFailureProbe::AlternatePath)
	{
		AppendLine(Source, TEXT("\tFPrivatePropertyProbe Probe = FPrivatePropertyProbe();"));
		AppendLine(Source, TEXT("\treturn Probe.Invoke(Receiver);"));
	}
	else
	{
		AppendLine(Source, TEXT("\treturn Receiver.Hidden;"));
	}
	AppendLine(Source, TEXT("}"));
}

void AppendValueReceiver(FString& Source, const FPropFailureParams& Params, const FString& FunctionName)
{
	if (Params.Probe == EPropFailureProbe::AlternatePath)
	{
		AppendLine(Source, TEXT("int InvokeValuePropertyFailure(FNativeCaseValue& inout Receiver)"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tReceiver.Access += 5;"));
		AppendLine(Source, TEXT("\treturn Receiver.Value;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseValue Receiver(31);"));
	if (Params.Probe == EPropFailureProbe::AlternatePath)
	{
		AppendLine(Source, TEXT("\treturn InvokeValuePropertyFailure(Receiver);"));
	}
	else
	{
		AppendLine(Source, TEXT("\tReceiver.Access += 5;"));
		AppendLine(Source, TEXT("\treturn Receiver.Value;"));
	}
	AppendLine(Source, TEXT("}"));
}

void AppendRegisteredOperation(FString& Source, const FPropFailureParams& Params, const TCHAR* Prefix)
{
	if (Params.Failure == EPropFailureKind::MissingGetter
		|| Params.Failure == EPropFailureKind::RecursiveGetter
		|| Params.Failure == EPropFailureKind::ThrowingGetter
		|| Params.Failure == EPropFailureKind::RegisteredDuplicateGetter
		|| Params.Failure == EPropFailureKind::NullReceiver)
	{
		AppendLine(Source, FString::Printf(TEXT("%sreturn Receiver.Value;"), Prefix));
		return;
	}
	if (Params.Failure == EPropFailureKind::RegisteredMismatchedTypes)
	{
		AppendLine(Source, FString::Printf(TEXT("%sReceiver.Value += 5;"), Prefix));
		AppendLine(Source, FString::Printf(TEXT("%sreturn ObservePropertyFailure(Receiver);"), Prefix));
		return;
	}
	AppendLine(Source, FString::Printf(TEXT("%sReceiver.Value = 73;"), Prefix));
	AppendLine(Source, FString::Printf(TEXT("%sreturn ObservePropertyFailure(Receiver);"), Prefix));
}

void AppendRegistered(FString& Source, const FPropFailureParams& Params, const FString& FunctionName)
{
	if (Params.Failure == EPropFailureKind::RecursiveGetter)
	{
		AppendLine(Source, TEXT("int ReenterPropertyFailureGet(FPropertyFailure& inout Receiver)"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Zero = 0;"));
		AppendLine(Source, TEXT("\treturn 1 / Zero;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Params.Failure == EPropFailureKind::RecursiveSetter)
	{
		AppendLine(Source, TEXT("void ReenterPropertyFailureSet(FPropertyFailure& inout Receiver, int Value)"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Zero = 0;"));
		AppendLine(Source, TEXT("\tReceiver.Value = 1 / Zero;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	if (Params.Probe == EPropFailureProbe::AlternatePath)
	{
		AppendLine(Source, TEXT("int InvokePropertyFailure(FPropertyFailure& inout Receiver)"));
		AppendLine(Source, TEXT("{"));
		if (IsDivideFault(Params.Failure))
		{
			AppendLine(Source, TEXT("\tint Zero = 0;"));
			AppendLine(Source, TEXT("\treturn 1 / Zero;"));
		}
		else
		{
			AppendRegisteredOperation(Source, Params, TEXT("\t"));
		}
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, IsNullFault(Params.Failure)
		? TEXT("\tFPropertyFailure Receiver = nullptr;")
		: TEXT("\tFPropertyFailure Receiver = CreatePropertyFailure(31);"));
	if (Params.Probe == EPropFailureProbe::AlternatePath)
	{
		AppendLine(Source, TEXT("\treturn InvokePropertyFailure(Receiver);"));
	}
	else if (IsDivideFault(Params.Failure))
	{
		AppendLine(Source, TEXT("\tint Observed = Receiver.Value;"));
		AppendLine(Source, TEXT("\tint Zero = 0;"));
		AppendLine(Source, TEXT("\treturn 1 / Zero;"));
	}
	else
	{
		AppendRegisteredOperation(Source, Params, TEXT("\t"));
	}
	AppendLine(Source, TEXT("}"));
}

void AppendEntry(FString& Source, const FPropFailureParams& Params, const FString& FunctionName)
{
	if (Params.Failure == EPropFailureKind::RemovedPropertyDecorator
		|| Params.Failure == EPropFailureKind::RemovedVirtualProperty)
	{
		AppendRemovedSyntax(Source, Params);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn 97;"));
		AppendLine(Source, TEXT("}"));
		return;
	}
	if (Params.Failure == EPropFailureKind::InaccessibleField)
	{
		AppendInaccessible(Source, Params, FunctionName);
		return;
	}
	if (Params.Failure == EPropFailureKind::CompoundValueReceiver)
	{
		AppendValueReceiver(Source, Params, FunctionName);
		return;
	}
	AppendRegistered(Source, Params, FunctionName);
}

FString BuildOwnedSource(const FPropFailureParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendLine(Source, FString::Printf(TEXT("// Recovery: %s"), RecoveryToken(Params.Recovery)));
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FFailureCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Failure))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedException = IsNullFault(Cell.Failure)
		? TEXT("Null pointer access")
		: TEXT("Divide by zero");
	return Info;
}
}

FStringView FPropFailureGenerator::GetProductId() const
{
	return TEXT("LANG-PROP-FAILURE");
}

TArray<FGeneratedCaseInfo> FPropFailureGenerator::ListCases() const
{
	using namespace PropFailureGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FFailureCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("PropFailure PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FPropFailureGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace PropFailureGeneratorDetail;
	FFailureCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FPropFailureGenerator::BuildDumpSource() const
{
	using namespace PropFailureGeneratorDetail;
	const TArray<FFailureCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-PROP-FAILURE"));
	AppendLine(Source, TEXT("// Cases: 56 total, 0 normal, 36 reject, 20 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);

	bool bNeedBlank = false;
	for (const FFailureCell& Cell : Cells)
	{
		if (IsReject(Cell.Failure))
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
		AppendLine(Source, TEXT("// Outcome: RuntimeException"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedException: %s"), *Info.ExpectedException));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Recovery: %s"), RecoveryToken(Cell.Recovery)));
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}

	for (const FFailureCell& Cell : Cells)
	{
		if (!IsReject(Cell.Failure))
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
		AppendLine(Source, FString::Printf(TEXT("// Recovery: %s"), RecoveryToken(Cell.Recovery)));
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FPropFailureGenerator::BuildPropertyFailureSource(const FPropFailureParams& Params) const
{
	using namespace PropFailureGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Failure))
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

FString FPropFailureGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace PropFailureGeneratorDetail;
	const TArray<FFailureCell> Cells = AllCells();
	FString Source;
	int32 Emitted = 0;
	for (const FFailureCell& Cell : Cells)
	{
		if (IsReject(Cell.Failure))
		{
			continue;
		}
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		AppendLine(Source, FString::Printf(TEXT("// Recovery: %s"), RecoveryToken(Cell.Recovery)));
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

int32 FPropFailureGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace PropFailureGeneratorDetail;
	FFailureCell Cell;
	TryFindCell(CaseId, Cell);
	return 0;
}

FString FPropFailureGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace PropFailureGeneratorDetail;
	FFailureCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Failure))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FPropFailureGenerator::ListRejectCaseIds() const
{
	using namespace PropFailureGeneratorDetail;
	TArray<FString> Ids;
	for (const FFailureCell& Cell : AllCells())
	{
		if (IsReject(Cell.Failure))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
