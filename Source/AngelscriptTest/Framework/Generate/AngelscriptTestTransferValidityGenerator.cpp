#include "Framework/Generate/AngelscriptTestTransferValidityGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace TransferValidityGeneratorDetail
{
struct FTransferValidityCell
{
	ETransferValidityPlacement Placement = ETransferValidityPlacement::Function;
	ETransferValidityTransfer Transfer = ETransferValidityTransfer::Break;
};

const TCHAR* PlacementToken(const ETransferValidityPlacement Placement)
{
	switch (Placement)
	{
	case ETransferValidityPlacement::Function:
		return TEXT("FUNCTION");
	case ETransferValidityPlacement::Branch:
		return TEXT("BRANCH");
	case ETransferValidityPlacement::Switch:
		return TEXT("SWITCH");
	case ETransferValidityPlacement::Loop:
		return TEXT("LOOP");
	default:
		return nullptr;
	}
}

const TCHAR* TransferToken(const ETransferValidityTransfer Transfer)
{
	switch (Transfer)
	{
	case ETransferValidityTransfer::Break:
		return TEXT("BREAK");
	case ETransferValidityTransfer::Continue:
		return TEXT("CONTINUE");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FTransferValidityParams& Params)
{
	return PlacementToken(Params.Placement) != nullptr && TransferToken(Params.Transfer) != nullptr;
}

bool IsReject(const ETransferValidityPlacement Placement, const ETransferValidityTransfer Transfer)
{
	if (Placement == ETransferValidityPlacement::Function || Placement == ETransferValidityPlacement::Branch)
	{
		return true;
	}
	return Placement == ETransferValidityPlacement::Switch && Transfer == ETransferValidityTransfer::Continue;
}

bool IsRejectCell(const FTransferValidityCell& Cell)
{
	return IsReject(Cell.Placement, Cell.Transfer);
}

int32 ExpectedFromCell(const FTransferValidityCell& Cell)
{
	if (IsRejectCell(Cell))
	{
		return 0;
	}
	return Cell.Placement == ETransferValidityPlacement::Loop && Cell.Transfer == ETransferValidityTransfer::Continue
		? 5
		: 3;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FTransferValidityCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CF-TRANSFER-VALIDITY-%s-%s"),
		PlacementToken(Cell.Placement),
		TransferToken(Cell.Transfer));
}

FString MakeEntryName(const FTransferValidityCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FTransferValidityParams ParamsFromCell(const FTransferValidityCell& Cell)
{
	FTransferValidityParams Params;
	Params.Placement = Cell.Placement;
	Params.Transfer = Cell.Transfer;
	return Params;
}

TArray<FTransferValidityCell> AllCells()
{
	TArray<FTransferValidityCell> Cells;
	Cells.SetNum(8);
	for (int32 Index = 0; Index < 8; ++Index)
	{
		FTransferValidityCell& Cell = Cells[Index];
		Cell.Placement = static_cast<ETransferValidityPlacement>(Index / 2);
		Cell.Transfer = static_cast<ETransferValidityTransfer>(Index % 2);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FTransferValidityCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FTransferValidityCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

const TCHAR* TransferKeyword(const ETransferValidityTransfer Transfer)
{
	return Transfer == ETransferValidityTransfer::Continue ? TEXT("continue") : TEXT("break");
}

void AppendEntry(FString& Source, const FTransferValidityParams& Params, const FString& FunctionName)
{
	const TCHAR* const Keyword = TransferKeyword(Params.Transfer);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Placement == ETransferValidityPlacement::Function)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s;"), Keyword));
		AppendLine(Source, TEXT("\treturn 0;"));
	}
	else if (Params.Placement == ETransferValidityPlacement::Branch)
	{
		AppendLine(Source, TEXT("\tif (true)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\t%s;"), Keyword));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn 0;"));
	}
	else if (Params.Placement == ETransferValidityPlacement::Switch)
	{
		AppendLine(Source, TEXT("\tswitch (1)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\tcase 1:"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\t%s;"), Keyword));
		AppendLine(Source, TEXT("\t\tbreak;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\tdefault:"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tbreak;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn 3;"));
	}
	else
	{
		AppendLine(Source, TEXT("\tfor (int Index = 0; Index < 2; ++Index)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tif (Index == 0)"));
		AppendLine(Source, TEXT("\t\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\t\t%s;"), Keyword));
		AppendLine(Source, TEXT("\t\t}"));
		AppendLine(Source, TEXT("\t\treturn 5;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn 3;"));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FTransferValidityParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FTransferValidityCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsRejectCell(Cell))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	return Info;
}
}

FStringView FTransferValidityGenerator::GetProductId() const
{
	return TEXT("LANG-CF-TRANSFER-VALIDITY");
}

TArray<FGeneratedCaseInfo> FTransferValidityGenerator::ListCases() const
{
	using namespace TransferValidityGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FTransferValidityCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("TransferValidity PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FTransferValidityGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace TransferValidityGeneratorDetail;
	FTransferValidityCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	if (IsRejectCell(Cell))
	{
		return BuildRejectSource(CaseId);
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FTransferValidityGenerator::BuildDumpSource() const
{
	using namespace TransferValidityGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CF-TRANSFER-VALIDITY"));
	AppendLine(Source, TEXT("// Cases: 8 total, 3 normal, 5 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	bool bNeedBlank = false;
	for (const FTransferValidityCell& Cell : AllCells())
	{
		if (IsRejectCell(Cell))
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
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}
	for (const FTransferValidityCell& Cell : AllCells())
	{
		if (!IsRejectCell(Cell))
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
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FTransferValidityGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace TransferValidityGeneratorDetail;
	FString Source;
	int32 Emitted = 0;
	for (const FTransferValidityCell& Cell : AllCells())
	{
		if (IsRejectCell(Cell))
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

FString FTransferValidityGenerator::BuildValiditySource(const FTransferValidityParams& Params) const
{
	using namespace TransferValidityGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Placement, Params.Transfer))
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

int32 FTransferValidityGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace TransferValidityGeneratorDetail;
	FTransferValidityCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsRejectCell(Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}

FString FTransferValidityGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace TransferValidityGeneratorDetail;
	FTransferValidityCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsRejectCell(Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FTransferValidityGenerator::ListRejectCaseIds() const
{
	using namespace TransferValidityGeneratorDetail;
	TArray<FString> Ids;
	for (const FTransferValidityCell& Cell : AllCells())
	{
		if (IsRejectCell(Cell))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
