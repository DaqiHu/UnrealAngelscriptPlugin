#include "Framework/Generate/AngelscriptTestSwitchPlacementGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace SwitchPlacementGeneratorDetail
{
struct FSwitchPlacementCell
{
	ESwitchPlacementScope Scope = ESwitchPlacementScope::Function;
	ESwitchPlacementKind Kind = ESwitchPlacementKind::CaseOutside;
};

const TCHAR* ScopeToken(const ESwitchPlacementScope Scope)
{
	switch (Scope)
	{
	case ESwitchPlacementScope::Function:
		return TEXT("FUNCTION");
	case ESwitchPlacementScope::Branch:
		return TEXT("BRANCH");
	case ESwitchPlacementScope::Loop:
		return TEXT("LOOP");
	case ESwitchPlacementScope::AfterSwitch:
		return TEXT("AFTER_SWITCH");
	default:
		return nullptr;
	}
}

const TCHAR* KindToken(const ESwitchPlacementKind Kind)
{
	switch (Kind)
	{
	case ESwitchPlacementKind::CaseOutside:
		return TEXT("CASE_OUTSIDE");
	case ESwitchPlacementKind::DefaultOutside:
		return TEXT("DEFAULT_OUTSIDE");
	case ESwitchPlacementKind::DuplicateDefault:
		return TEXT("DUPLICATE_DEFAULT");
	case ESwitchPlacementKind::CaseAfterDefault:
		return TEXT("CASE_AFTER_DEFAULT");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FSwitchPlacementParams& Params)
{
	return ScopeToken(Params.Scope) != nullptr && KindToken(Params.Kind) != nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FSwitchPlacementCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CF-SWITCH-PLACEMENT-%s-%s"),
		ScopeToken(Cell.Scope),
		KindToken(Cell.Kind));
}

FString MakeEntryName(const FSwitchPlacementCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FSwitchPlacementParams ParamsFromCell(const FSwitchPlacementCell& Cell)
{
	FSwitchPlacementParams Params;
	Params.Scope = Cell.Scope;
	Params.Kind = Cell.Kind;
	return Params;
}

TArray<FSwitchPlacementCell> AllCells()
{
	TArray<FSwitchPlacementCell> Cells;
	Cells.SetNum(16);
	for (int32 Index = 0; Index < 16; ++Index)
	{
		FSwitchPlacementCell& Cell = Cells[Index];
		Cell.Scope = static_cast<ESwitchPlacementScope>(Index / 4);
		Cell.Kind = static_cast<ESwitchPlacementKind>(Index % 4);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FSwitchPlacementCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FSwitchPlacementCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendIllegalKind(FString& Source, const ESwitchPlacementKind Kind, const TCHAR* Indent)
{
	if (Kind == ESwitchPlacementKind::CaseOutside)
	{
		AppendLine(Source, FString::Printf(TEXT("%scase 1:"), Indent));
		AppendLine(Source, FString::Printf(TEXT("%s\tValue += 10;"), Indent));
		return;
	}
	if (Kind == ESwitchPlacementKind::DefaultOutside)
	{
		AppendLine(Source, FString::Printf(TEXT("%sdefault:"), Indent));
		AppendLine(Source, FString::Printf(TEXT("%s\tValue += 20;"), Indent));
		return;
	}
	if (Kind == ESwitchPlacementKind::DuplicateDefault)
	{
		AppendLine(Source, FString::Printf(TEXT("%sswitch (Value)"), Indent));
		AppendLine(Source, FString::Printf(TEXT("%s{"), Indent));
		AppendLine(Source, FString::Printf(TEXT("%sdefault:"), Indent));
		AppendLine(Source, FString::Printf(TEXT("%s\tValue += 30;"), Indent));
		AppendLine(Source, FString::Printf(TEXT("%sdefault:"), Indent));
		AppendLine(Source, FString::Printf(TEXT("%s\tValue += 40;"), Indent));
		AppendLine(Source, FString::Printf(TEXT("%s}"), Indent));
		return;
	}
	AppendLine(Source, FString::Printf(TEXT("%sswitch (Value)"), Indent));
	AppendLine(Source, FString::Printf(TEXT("%s{"), Indent));
	AppendLine(Source, FString::Printf(TEXT("%sdefault:"), Indent));
	AppendLine(Source, FString::Printf(TEXT("%s\tValue += 50;"), Indent));
	AppendLine(Source, FString::Printf(TEXT("%scase 1:"), Indent));
	AppendLine(Source, FString::Printf(TEXT("%s\tValue += 60;"), Indent));
	AppendLine(Source, FString::Printf(TEXT("%s}"), Indent));
}

void AppendValidPrefix(FString& Source)
{
	AppendLine(Source, TEXT("\tswitch (Value)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\tcase 0:"));
	AppendLine(Source, TEXT("\t\tValue += 1;"));
	AppendLine(Source, TEXT("\t\tbreak;"));
	AppendLine(Source, TEXT("\tdefault:"));
	AppendLine(Source, TEXT("\t\tValue += 2;"));
	AppendLine(Source, TEXT("\t\tbreak;"));
	AppendLine(Source, TEXT("\t}"));
}

void AppendEntry(FString& Source, const FSwitchPlacementParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	if (Params.Scope == ESwitchPlacementScope::AfterSwitch)
	{
		AppendValidPrefix(Source);
		AppendIllegalKind(Source, Params.Kind, TEXT("\t"));
	}
	else if (Params.Scope == ESwitchPlacementScope::Branch)
	{
		AppendLine(Source, TEXT("\tif (true)"));
		AppendLine(Source, TEXT("\t{"));
		AppendIllegalKind(Source, Params.Kind, TEXT("\t\t"));
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Params.Scope == ESwitchPlacementScope::Loop)
	{
		AppendLine(Source, TEXT("\twhile (false)"));
		AppendLine(Source, TEXT("\t{"));
		AppendIllegalKind(Source, Params.Kind, TEXT("\t\t"));
		AppendLine(Source, TEXT("\t}"));
	}
	else
	{
		AppendIllegalKind(Source, Params.Kind, TEXT("\t"));
	}
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FSwitchPlacementParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FSwitchPlacementCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.Outcome = EGeneratedCaseOutcome::CompileReject;
	Info.Execution = EGeneratedCaseExecution::SourceOnly;
	return Info;
}
}

FStringView FSwitchPlacementGenerator::GetProductId() const
{
	return TEXT("LANG-CF-SWITCH-PLACEMENT");
}

TArray<FGeneratedCaseInfo> FSwitchPlacementGenerator::ListCases() const
{
	using namespace SwitchPlacementGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	for (const FSwitchPlacementCell& Cell : AllCells())
	{
		Cases.Add(MakeCaseInfo(Cell));
	}
	return Cases;
}

FString FSwitchPlacementGenerator::BuildCaseSource(const FStringView CaseId) const
{
	return BuildRejectSource(CaseId);
}

FString FSwitchPlacementGenerator::BuildDumpSource() const
{
	using namespace SwitchPlacementGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CF-SWITCH-PLACEMENT"));
	AppendLine(Source, TEXT("// Cases: 16 total, 0 normal, 16 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (const FSwitchPlacementCell& Cell : AllCells())
	{
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

FString FSwitchPlacementGenerator::BuildAllSource(int32& OutCaseCount) const
{
	OutCaseCount = 0;
	return FString();
}

FString FSwitchPlacementGenerator::BuildSwitchPlacementSource(const FSwitchPlacementParams& Params) const
{
	using namespace SwitchPlacementGeneratorDetail;
	(void)Params;
	return FString();
}

int32 FSwitchPlacementGenerator::GetExpected(const FStringView CaseId) const
{
	(void)CaseId;
	return 0;
}

FString FSwitchPlacementGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace SwitchPlacementGeneratorDetail;
	FSwitchPlacementCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsValidParams(ParamsFromCell(Cell)))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FSwitchPlacementGenerator::ListRejectCaseIds() const
{
	using namespace SwitchPlacementGeneratorDetail;
	TArray<FString> Ids;
	for (const FSwitchPlacementCell& Cell : AllCells())
	{
		Ids.Add(MakeCaseId(Cell));
	}
	return Ids;
}
}
