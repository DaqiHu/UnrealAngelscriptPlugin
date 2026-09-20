#include "Framework/Generate/AngelscriptTestFnValueLifecycleGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace FnValueLifecycleGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native FNativeCaseValue and ArmNextNativeCaseValueCopyFault.");

struct FLifecycleCell
{
	EFnValueLifecycleFailure Failure = EFnValueLifecycleFailure::None;
	EFnValueLifecycleInitialized Initialized = EFnValueLifecycleInitialized::Zero;
	EFnValueLifecycleTransfer Transfer = EFnValueLifecycleTransfer::ValueArgument;
};

const TCHAR* FailureToken(const EFnValueLifecycleFailure Failure)
{
	switch (Failure)
	{
	case EFnValueLifecycleFailure::None:
		return TEXT("NONE");
	case EFnValueLifecycleFailure::ArgumentEvaluation:
		return TEXT("ARGUMENT_EVALUATION");
	case EFnValueLifecycleFailure::Body:
		return TEXT("BODY");
	case EFnValueLifecycleFailure::ReturnConstruction:
		return TEXT("RETURN_CONSTRUCTION");
	default:
		return nullptr;
	}
}

const TCHAR* InitializedToken(const EFnValueLifecycleInitialized Initialized)
{
	switch (Initialized)
	{
	case EFnValueLifecycleInitialized::Zero:
		return TEXT("ZERO");
	case EFnValueLifecycleInitialized::One:
		return TEXT("ONE");
	case EFnValueLifecycleInitialized::Many:
		return TEXT("MANY");
	default:
		return nullptr;
	}
}

const TCHAR* TransferToken(const EFnValueLifecycleTransfer Transfer)
{
	switch (Transfer)
	{
	case EFnValueLifecycleTransfer::ValueArgument:
		return TEXT("VALUE_ARGUMENT");
	case EFnValueLifecycleTransfer::ValueReturn:
		return TEXT("VALUE_RETURN");
	default:
		return nullptr;
	}
}

int32 InitializedCount(const EFnValueLifecycleInitialized Initialized)
{
	if (Initialized == EFnValueLifecycleInitialized::One)
	{
		return 1;
	}
	if (Initialized == EFnValueLifecycleInitialized::Many)
	{
		return 4;
	}
	return 0;
}

bool IsValidParams(const FFnValueLifecycleParams& Params)
{
	return FailureToken(Params.Failure) != nullptr
		&& InitializedToken(Params.Initialized) != nullptr
		&& TransferToken(Params.Transfer) != nullptr;
}

bool IsFault(const EFnValueLifecycleFailure Failure)
{
	return Failure != EFnValueLifecycleFailure::None;
}

const TCHAR* ExceptionText(const EFnValueLifecycleFailure Failure)
{
	return Failure == EFnValueLifecycleFailure::ReturnConstruction
		? TEXT("Native case value copy construction fault")
		: TEXT("Divide by zero");
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FLifecycleCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-FN-VALUE-LIFECYCLE-%s-%s-%s"),
		FailureToken(Cell.Failure),
		InitializedToken(Cell.Initialized),
		TransferToken(Cell.Transfer));
}

FString MakeEntryName(const FLifecycleCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FFnValueLifecycleParams ParamsFromCell(const FLifecycleCell& Cell)
{
	FFnValueLifecycleParams Params;
	Params.Failure = Cell.Failure;
	Params.Initialized = Cell.Initialized;
	Params.Transfer = Cell.Transfer;
	return Params;
}

TArray<FLifecycleCell> AllCells()
{
	TArray<FLifecycleCell> Cells;
	Cells.SetNum(24);
	for (int32 Index = 0; Index < 24; ++Index)
	{
		FLifecycleCell& Cell = Cells[Index];
		Cell.Failure = static_cast<EFnValueLifecycleFailure>(Index / 6);
		Cell.Initialized = static_cast<EFnValueLifecycleInitialized>((Index / 2) % 3);
		Cell.Transfer = static_cast<EFnValueLifecycleTransfer>(Index % 2);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FLifecycleCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FLifecycleCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendArgumentHelpers(FString& Source)
{
	AppendLine(Source, TEXT("FNativeCaseValue ProduceArgumentValue(int Seed, bool ArmCopyFault)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseValue Result(Seed);"));
	AppendLine(Source, TEXT("\tif (ArmCopyFault)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tArmNextNativeCaseValueCopyFault();"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("\treturn Result;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ConsumeValueArgument(FNativeCaseValue Value, int Divisor)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value.Value / Divisor;"));
	AppendLine(Source, TEXT("}"));
}

void AppendReturnHelpers(FString& Source)
{
	AppendLine(Source, TEXT("FNativeCaseValue ProduceReturnValue(int Seed, int Divisor, bool ArmCopyFault)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseValue Result(Seed);"));
	AppendLine(Source, TEXT("\tResult.Value = Result.Value / Divisor;"));
	AppendLine(Source, TEXT("\tif (ArmCopyFault)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tArmNextNativeCaseValueCopyFault();"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("\treturn Result;"));
	AppendLine(Source, TEXT("}"));
}

void AppendSharedHelpers(FString& Source)
{
	AppendArgumentHelpers(Source);
	AppendLine(Source);
	AppendReturnHelpers(Source);
	AppendLine(Source);
	AppendLine(Source, TEXT("int CleanAfterValueLifecycle()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 73;"));
	AppendLine(Source, TEXT("}"));
}

void AppendInitialized(FString& Source, const EFnValueLifecycleInitialized Initialized)
{
	const int32 Count = InitializedCount(Initialized);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFNativeCaseValue Initialized%d(%d);"), Index, 100 + Index));
	}
	if (Count > 0)
	{
		AppendLine(Source);
	}
}

void AppendEntry(FString& Source, const FFnValueLifecycleParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendInitialized(Source, Params.Initialized);
	if (Params.Transfer == EFnValueLifecycleTransfer::ValueArgument)
	{
		if (Params.Failure == EFnValueLifecycleFailure::ArgumentEvaluation)
		{
			AppendLine(Source, TEXT("\tint Zero = 0;"));
			AppendLine(Source, TEXT("\treturn ConsumeValueArgument(FNativeCaseValue(41), 1 / Zero);"));
		}
		else if (Params.Failure == EFnValueLifecycleFailure::Body)
		{
			AppendLine(Source, TEXT("\treturn ConsumeValueArgument(FNativeCaseValue(41), 0);"));
		}
		else if (Params.Failure == EFnValueLifecycleFailure::ReturnConstruction)
		{
			AppendLine(Source, TEXT("\treturn ConsumeValueArgument(ProduceArgumentValue(41, true), 1);"));
		}
		else
		{
			AppendLine(Source, TEXT("\treturn ConsumeValueArgument(ProduceArgumentValue(41, false), 1);"));
		}
	}
	else
	{
		if (Params.Failure == EFnValueLifecycleFailure::ArgumentEvaluation)
		{
			AppendLine(Source, TEXT("\tint Zero = 0;"));
			AppendLine(Source, TEXT("\tFNativeCaseValue Value = ProduceReturnValue(41, 1 / Zero, false);"));
		}
		else if (Params.Failure == EFnValueLifecycleFailure::Body)
		{
			AppendLine(Source, TEXT("\tFNativeCaseValue Value = ProduceReturnValue(41, 0, false);"));
		}
		else if (Params.Failure == EFnValueLifecycleFailure::ReturnConstruction)
		{
			AppendLine(Source, TEXT("\tFNativeCaseValue Value = ProduceReturnValue(41, 1, true);"));
		}
		else
		{
			AppendLine(Source, TEXT("\tFNativeCaseValue Value = ProduceReturnValue(41, 1, false);"));
		}
		AppendLine(Source, TEXT("\treturn Value.Value;"));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FFnValueLifecycleParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FLifecycleCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExecutionNotes = HostNotes;
	Info.bLimitedObservation = true;
	if (IsFault(Cell.Failure))
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.ExpectedException = ExceptionText(Cell.Failure);
	}
	else
	{
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.ExpectedReturn = 41;
	}
	return Info;
}
}

FStringView FFnValueLifecycleGenerator::GetProductId() const
{
	return TEXT("LANG-FN-VALUE-LIFECYCLE");
}

TArray<FGeneratedCaseInfo> FFnValueLifecycleGenerator::ListCases() const
{
	using namespace FnValueLifecycleGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FLifecycleCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("FnValueLifecycle PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FFnValueLifecycleGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace FnValueLifecycleGeneratorDetail;
	FLifecycleCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FFnValueLifecycleGenerator::BuildDumpSource() const
{
	using namespace FnValueLifecycleGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FN-VALUE-LIFECYCLE"));
	AppendLine(Source, TEXT("// Cases: 24 total, 6 normal, 0 reject, 18 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	for (const FLifecycleCell& Cell : AllCells())
	{
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		if (IsFault(Cell.Failure))
		{
			AppendLine(Source, TEXT("// Outcome: RuntimeException"));
			AppendLine(Source, FString::Printf(TEXT("// ExpectedException: %s"), *Info.ExpectedException));
		}
		else
		{
			AppendLine(Source, TEXT("// Outcome: ReturnValue"));
			AppendLine(Source, TEXT("// ExpectedReturn: 41"));
		}
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), HostNotes));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FFnValueLifecycleGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace FnValueLifecycleGeneratorDetail;
	const TArray<FLifecycleCell> Cells = AllCells();
	OutCaseCount = Cells.Num();
	FString Source;
	AppendSharedHelpers(Source);
	for (const FLifecycleCell& Cell : Cells)
	{
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FFnValueLifecycleGenerator::BuildSource(const FFnValueLifecycleParams& Params) const
{
	using namespace FnValueLifecycleGeneratorDetail;
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

int32 FFnValueLifecycleGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace FnValueLifecycleGeneratorDetail;
	FLifecycleCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsFault(Cell.Failure))
	{
		return 0;
	}
	return 41;
}
}
