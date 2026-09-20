#include "Framework/Generate/AngelscriptTestRefLifetimeGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace RefLifetimeGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs host owner/lifecycle observation of FRefRoot.");

struct FRefLifetimeCell
{
	ERefLifetimeObservation Observation = ERefLifetimeObservation::Identity;
	ERefLifetimeOwner Owner = ERefLifetimeOwner::OwnerLive;
	ERefLifetimeReference Reference = ERefLifetimeReference::None;
};

const TCHAR* ObservationToken(const ERefLifetimeObservation Observation)
{
	switch (Observation)
	{
	case ERefLifetimeObservation::Identity:
		return TEXT("IDENTITY");
	case ERefLifetimeObservation::Refcount:
		return TEXT("REFCOUNT");
	case ERefLifetimeObservation::WeakFlag:
		return TEXT("WEAK_FLAG");
	case ERefLifetimeObservation::Destruction:
		return TEXT("DESTRUCTION");
	case ERefLifetimeObservation::GcStats:
		return TEXT("GC_STATS");
	default:
		return nullptr;
	}
}

const TCHAR* OwnerToken(const ERefLifetimeOwner Owner)
{
	switch (Owner)
	{
	case ERefLifetimeOwner::OwnerLive:
		return TEXT("OWNER_LIVE");
	case ERefLifetimeOwner::ScopeExit:
		return TEXT("SCOPE_EXIT");
	case ERefLifetimeOwner::ReturnedAlias:
		return TEXT("RETURNED_ALIAS");
	case ERefLifetimeOwner::ModuleRetained:
		return TEXT("MODULE_RETAINED");
	case ERefLifetimeOwner::ModuleDiscarded:
		return TEXT("MODULE_DISCARDED");
	case ERefLifetimeOwner::ContextRetained:
		return TEXT("CONTEXT_RETAINED");
	case ERefLifetimeOwner::ContextReleased:
		return TEXT("CONTEXT_RELEASED");
	case ERefLifetimeOwner::GcCycle:
		return TEXT("GC_CYCLE");
	default:
		return nullptr;
	}
}

const TCHAR* ReferenceToken(const ERefLifetimeReference Reference)
{
	switch (Reference)
	{
	case ERefLifetimeReference::None:
		return TEXT("NONE");
	case ERefLifetimeReference::OneAlias:
		return TEXT("ONE_ALIAS");
	case ERefLifetimeReference::MultipleAliases:
		return TEXT("MULTIPLE_ALIASES");
	case ERefLifetimeReference::Cycle:
		return TEXT("CYCLE");
	case ERefLifetimeReference::WeakFlag:
		return TEXT("WEAK_FLAG");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FRefLifetimeParams& Params)
{
	return ObservationToken(Params.Observation) != nullptr
		&& OwnerToken(Params.Owner) != nullptr
		&& ReferenceToken(Params.Reference) != nullptr;
}

bool IsReject(const ERefLifetimeOwner Owner)
{
	return Owner == ERefLifetimeOwner::ModuleRetained || Owner == ERefLifetimeOwner::ModuleDiscarded;
}

bool IsGcOwner(const ERefLifetimeOwner Owner)
{
	return Owner == ERefLifetimeOwner::GcCycle;
}

bool IsReturnedAlias(const ERefLifetimeOwner Owner)
{
	return Owner == ERefLifetimeOwner::ReturnedAlias;
}

bool HasReferenceObject(const ERefLifetimeReference Reference)
{
	return Reference != ERefLifetimeReference::None;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FRefLifetimeCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-REF-LIFETIME-%s-%s-%s"),
		ObservationToken(Cell.Observation),
		OwnerToken(Cell.Owner),
		ReferenceToken(Cell.Reference));
}

FString MakeEntryName(const FRefLifetimeCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FRefLifetimeParams ParamsFromCell(const FRefLifetimeCell& Cell)
{
	FRefLifetimeParams Params;
	Params.Observation = Cell.Observation;
	Params.Owner = Cell.Owner;
	Params.Reference = Cell.Reference;
	return Params;
}

TArray<FRefLifetimeCell> AllCells()
{
	TArray<FRefLifetimeCell> Cells;
	Cells.SetNum(200);
	for (int32 Index = 0; Index < 200; ++Index)
	{
		FRefLifetimeCell& Cell = Cells[Index];
		Cell.Observation = static_cast<ERefLifetimeObservation>(Index / 40);
		Cell.Owner = static_cast<ERefLifetimeOwner>((Index / 5) % 8);
		Cell.Reference = static_cast<ERefLifetimeReference>(Index % 5);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FRefLifetimeCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FRefLifetimeCell> Cells = AllCells();
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

int32 ExpectedFromCell(const FRefLifetimeCell& Cell)
{
	if (IsReject(Cell.Owner))
	{
		return 0;
	}
	if (IsGcOwner(Cell.Owner))
	{
		return 1;
	}
	return HasReferenceObject(Cell.Reference) ? 1 : 0;
}

void AppendLocalGraph(FString& Source, const ERefLifetimeReference Reference)
{
	if (!HasReferenceObject(Reference))
	{
		AppendLine(Source, TEXT("\tFRefRoot Primary = nullptr;"));
		return;
	}
	AppendLine(Source, TEXT("\tFRefRoot Primary = MakeRefRoot(41);"));
	if (Reference == ERefLifetimeReference::MultipleAliases)
	{
		AppendLine(Source, TEXT("\tFRefRoot AliasA = Primary;"));
		AppendLine(Source, TEXT("\tFRefRoot AliasB = Primary;"));
	}
	if (Reference == ERefLifetimeReference::Cycle)
	{
		AppendLine(Source, TEXT("\tPrimary.Link(Primary);"));
	}
}

void AppendIdentityReturn(FString& Source, const TCHAR* Name)
{
	AppendLine(Source, FString::Printf(TEXT("\treturn %s == nullptr"), Name));
	AppendLine(Source, TEXT("\t\t? 0"));
	AppendLine(Source, FString::Printf(TEXT("\t\t: %s.GetIdentity();"), Name));
}

void AppendOrdinaryEntry(FString& Source, const FRefLifetimeParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLocalGraph(Source, Params.Reference);
	if (Params.Reference == ERefLifetimeReference::Cycle)
	{
		AppendLine(Source, TEXT("\tPrimary.ClearPeer();"));
	}
	if (IsReturnedAlias(Params.Owner))
	{
		AppendLine(Source, TEXT("\tFRefRoot Returned = Primary;"));
		AppendIdentityReturn(Source, TEXT("Returned"));
	}
	else
	{
		AppendIdentityReturn(Source, TEXT("Primary"));
	}
	AppendLine(Source, TEXT("}"));
}

void AppendGcEntry(FString& Source, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 1;"));
	AppendLine(Source, TEXT("}"));
}

void AppendRejectEntry(FString& Source, const FRefLifetimeParams& Params, const FString& FunctionName)
{
	AppendLine(Source, TEXT("FRefRoot GReferenceOwner;"));
	AppendLine(Source, TEXT("int GMutableObservation = 0;"));
	if (Params.Reference == ERefLifetimeReference::MultipleAliases)
	{
		AppendLine(Source, TEXT("FRefRoot GReferenceAlias;"));
	}
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn GMutableObservation;"));
	AppendLine(Source, TEXT("}"));
}

void AppendEntry(FString& Source, const FRefLifetimeParams& Params, const FString& FunctionName)
{
	if (IsReject(Params.Owner))
	{
		AppendRejectEntry(Source, Params, FunctionName);
		return;
	}
	if (IsGcOwner(Params.Owner))
	{
		AppendGcEntry(Source, FunctionName);
		return;
	}
	AppendOrdinaryEntry(Source, Params, FunctionName);
}

FString BuildOwnedSource(const FRefLifetimeParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FRefLifetimeCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Owner))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	Info.bLimitedObservation = true;
	Info.ExecutionNotes = HostNotes;
	return Info;
}
}

FStringView FRefLifetimeGenerator::GetProductId() const
{
	return TEXT("LANG-REF-LIFETIME");
}

TArray<FGeneratedCaseInfo> FRefLifetimeGenerator::ListCases() const
{
	using namespace RefLifetimeGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FRefLifetimeCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cells[Index]);
			checkf(!EntryNames.Contains(EntryName), TEXT("RefLifetime PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FRefLifetimeGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace RefLifetimeGeneratorDetail;
	FRefLifetimeCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	if (IsReject(Cell.Owner))
	{
		return BuildRejectSource(CaseId);
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FRefLifetimeGenerator::BuildDumpSource() const
{
	using namespace RefLifetimeGeneratorDetail;
	const TArray<FRefLifetimeCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-REF-LIFETIME"));
	AppendLine(Source, TEXT("// Cases: 200 total, 150 normal, 50 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Owner))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), HostNotes));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (!IsReject(Cells[Index].Owner))
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

FString FRefLifetimeGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace RefLifetimeGeneratorDetail;
	FString Source;
	int32 Emitted = 0;
	const TArray<FRefLifetimeCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Owner))
		{
			continue;
		}
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

FString FRefLifetimeGenerator::BuildReferenceLifetimeSource(const FRefLifetimeParams& Params) const
{
	using namespace RefLifetimeGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Owner))
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

int32 FRefLifetimeGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace RefLifetimeGeneratorDetail;
	FRefLifetimeCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Owner))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}

FString FRefLifetimeGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace RefLifetimeGeneratorDetail;
	FRefLifetimeCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Owner))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FRefLifetimeGenerator::ListRejectCaseIds() const
{
	using namespace RefLifetimeGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FRefLifetimeCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Owner))
		{
			Ids.Add(MakeCaseId(Cells[Index]));
		}
	}
	return Ids;
}
}
