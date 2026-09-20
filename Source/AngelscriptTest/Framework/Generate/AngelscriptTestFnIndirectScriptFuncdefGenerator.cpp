#include "Framework/Generate/AngelscriptTestFnIndirectScriptFuncdefGenerator.h"

namespace AngelscriptTest::Generate
{
namespace FnIndirectScriptFuncdefGeneratorDetail
{
struct FScriptFuncdefCell
{
	EFnIndirectScriptFuncdefScenario Scenario = EFnIndirectScriptFuncdefScenario::DeclarationMetadata;
};

const TCHAR* ScenarioToken(const EFnIndirectScriptFuncdefScenario Scenario)
{
	switch (Scenario)
	{
	case EFnIndirectScriptFuncdefScenario::DeclarationMetadata:
		return TEXT("DECLARATION_METADATA");
	case EFnIndirectScriptFuncdefScenario::CompatibleDirect:
		return TEXT("COMPATIBLE_DIRECT");
	case EFnIndirectScriptFuncdefScenario::CompatibleNested:
		return TEXT("COMPATIBLE_NESTED");
	case EFnIndirectScriptFuncdefScenario::NullOrUnbound:
		return TEXT("NULL_OR_UNBOUND");
	case EFnIndirectScriptFuncdefScenario::IncompatibleSignature:
		return TEXT("INCOMPATIBLE_SIGNATURE");
	case EFnIndirectScriptFuncdefScenario::RebuildOrRebind:
		return TEXT("REBUILD_OR_REBIND");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FFnIndirectScriptFuncdefParams& Params)
{
	return ScenarioToken(Params.Scenario) != nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FScriptFuncdefCell& Cell)
{
	return FString::Printf(TEXT("LANG-FN-INDIRECT-SCRIPT-FUNCDEF-%s"), ScenarioToken(Cell.Scenario));
}

FString MakeEntryName(const FScriptFuncdefCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FFnIndirectScriptFuncdefParams ParamsFromCell(const FScriptFuncdefCell& Cell)
{
	FFnIndirectScriptFuncdefParams Params;
	Params.Scenario = Cell.Scenario;
	return Params;
}

TArray<FScriptFuncdefCell> AllCells()
{
	TArray<FScriptFuncdefCell> Cells;
	Cells.SetNum(6);
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Cells[Index].Scenario = static_cast<EFnIndirectScriptFuncdefScenario>(Index);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FScriptFuncdefCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FScriptFuncdefCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendEntry(FString& Source, const FFnIndirectScriptFuncdefParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("funcdef int FScriptCallback_%s(int Value);"), ScenarioToken(Params.Scenario)));
	AppendLine(Source);
	AppendLine(Source, TEXT("int Target(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, Params.Scenario == EFnIndirectScriptFuncdefScenario::CompatibleNested
		? TEXT("\treturn Value + 1;")
		: TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, Params.Scenario == EFnIndirectScriptFuncdefScenario::NullOrUnbound
		? TEXT("\treturn 0;")
		: TEXT("\treturn Target(42);"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FFnIndirectScriptFuncdefParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FScriptFuncdefCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.Outcome = EGeneratedCaseOutcome::CompileReject;
	Info.Execution = EGeneratedCaseExecution::SourceOnly;
	Info.ExecutionNotes = TEXT("Script-level funcdef is rejected by the current fork parser.");
	return Info;
}
}

FStringView FFnIndirectScriptFuncdefGenerator::GetProductId() const
{
	return TEXT("LANG-FN-INDIRECT-SCRIPT-FUNCDEF");
}

TArray<FGeneratedCaseInfo> FFnIndirectScriptFuncdefGenerator::ListCases() const
{
	using namespace FnIndirectScriptFuncdefGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	for (const FScriptFuncdefCell& Cell : AllCells())
	{
		Cases.Add(MakeCaseInfo(Cell));
	}
	return Cases;
}

FString FFnIndirectScriptFuncdefGenerator::BuildCaseSource(const FStringView CaseId) const
{
	return BuildRejectSource(CaseId);
}

FString FFnIndirectScriptFuncdefGenerator::BuildDumpSource() const
{
	using namespace FnIndirectScriptFuncdefGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FN-INDIRECT-SCRIPT-FUNCDEF"));
	AppendLine(Source, TEXT("// Cases: 6 total, 0 normal, 6 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (const FScriptFuncdefCell& Cell : AllCells())
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

FString FFnIndirectScriptFuncdefGenerator::BuildAllSource(int32& OutCaseCount) const
{
	OutCaseCount = 0;
	return FString();
}

FString FFnIndirectScriptFuncdefGenerator::BuildScriptFuncdefSource(const FFnIndirectScriptFuncdefParams& Params) const
{
	using namespace FnIndirectScriptFuncdefGeneratorDetail;
	(void)Params;
	return FString();
}

int32 FFnIndirectScriptFuncdefGenerator::GetExpected(const FStringView CaseId) const
{
	(void)CaseId;
	return 0;
}

FString FFnIndirectScriptFuncdefGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace FnIndirectScriptFuncdefGeneratorDetail;
	FScriptFuncdefCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsValidParams(ParamsFromCell(Cell)))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FFnIndirectScriptFuncdefGenerator::ListRejectCaseIds() const
{
	using namespace FnIndirectScriptFuncdefGeneratorDetail;
	TArray<FString> Ids;
	for (const FScriptFuncdefCell& Cell : AllCells())
	{
		Ids.Add(MakeCaseId(Cell));
	}
	return Ids;
}
}
