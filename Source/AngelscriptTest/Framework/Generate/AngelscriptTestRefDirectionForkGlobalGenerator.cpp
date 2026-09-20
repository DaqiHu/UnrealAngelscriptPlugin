#include "Framework/Generate/AngelscriptTestRefDirectionForkGlobalGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace RefDirectionForkGlobalGeneratorDetail
{
struct FRefDirectionForkGlobalCell
{
	ERefDirectionForkGlobalState State = ERefDirectionForkGlobalState::Mutable;
};

const TCHAR* StateToken(const ERefDirectionForkGlobalState State)
{
	switch (State)
	{
	case ERefDirectionForkGlobalState::Mutable:
		return TEXT("MUTABLE");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FRefDirectionForkGlobalParams& Params)
{
	return StateToken(Params.State) != nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FRefDirectionForkGlobalCell& Cell)
{
	return FString::Printf(TEXT("LANG-REF-DIRECTION-FORK-GLOBAL-%s"), StateToken(Cell.State));
}

FString MakeEntryName(const FRefDirectionForkGlobalCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FRefDirectionForkGlobalParams ParamsFromCell(const FRefDirectionForkGlobalCell& Cell)
{
	FRefDirectionForkGlobalParams Params;
	Params.State = Cell.State;
	return Params;
}

TArray<FRefDirectionForkGlobalCell> AllCells()
{
	TArray<FRefDirectionForkGlobalCell> Cells;
	Cells.SetNum(1);
	Cells[0].State = ERefDirectionForkGlobalState::Mutable;
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FRefDirectionForkGlobalCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FRefDirectionForkGlobalCell> Cells = AllCells();
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

void AppendEntry(FString& Source, const FRefDirectionForkGlobalParams& Params, const FString& FunctionName)
{
	(void)Params;
	AppendLine(Source, TEXT("int GReferenceDirectionForkRestriction = 0;"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn GReferenceDirectionForkRestriction;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FRefDirectionForkGlobalParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FRefDirectionForkGlobalCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.Outcome = EGeneratedCaseOutcome::CompileReject;
	Info.Execution = EGeneratedCaseExecution::SourceOnly;
	return Info;
}
}

FStringView FRefDirectionForkGlobalGenerator::GetProductId() const
{
	return TEXT("LANG-REF-DIRECTION-FORK-GLOBAL");
}

TArray<FGeneratedCaseInfo> FRefDirectionForkGlobalGenerator::ListCases() const
{
	using namespace RefDirectionForkGlobalGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	const TArray<FRefDirectionForkGlobalCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		Cases.Add(MakeCaseInfo(Cells[Index]));
	}
	return Cases;
}

FString FRefDirectionForkGlobalGenerator::BuildCaseSource(const FStringView CaseId) const
{
	return BuildRejectSource(CaseId);
}

FString FRefDirectionForkGlobalGenerator::BuildDumpSource() const
{
	using namespace RefDirectionForkGlobalGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-REF-DIRECTION-FORK-GLOBAL"));
	AppendLine(Source, TEXT("// Cases: 1 total, 0 normal, 1 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	const TArray<FRefDirectionForkGlobalCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
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

FString FRefDirectionForkGlobalGenerator::BuildAllSource(int32& OutCaseCount) const
{
	OutCaseCount = 0;
	return FString();
}

FString FRefDirectionForkGlobalGenerator::BuildRefDirectionForkGlobalSource(
	const FRefDirectionForkGlobalParams& Params) const
{
	using namespace RefDirectionForkGlobalGeneratorDetail;
	(void)Params;
	return FString();
}

int32 FRefDirectionForkGlobalGenerator::GetExpected(const FStringView CaseId) const
{
	(void)CaseId;
	return 0;
}

FString FRefDirectionForkGlobalGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace RefDirectionForkGlobalGeneratorDetail;
	FRefDirectionForkGlobalCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsValidParams(ParamsFromCell(Cell)))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FRefDirectionForkGlobalGenerator::ListRejectCaseIds() const
{
	using namespace RefDirectionForkGlobalGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FRefDirectionForkGlobalCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		Ids.Add(MakeCaseId(Cells[Index]));
	}
	return Ids;
}
}
