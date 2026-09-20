#include "Framework/Generate/AngelscriptTestFnIndirectRegisteredFuncdefGenerator.h"

namespace AngelscriptTest::Generate
{
namespace FnIndirectRegisteredFuncdefGeneratorDetail
{
struct FRegisteredCell
{
	EFnIndirectRegisteredFuncdefScenario Scenario = EFnIndirectRegisteredFuncdefScenario::DeclarationMetadata;
};

const TCHAR* ScenarioToken(const EFnIndirectRegisteredFuncdefScenario Scenario)
{
	switch (Scenario)
	{
	case EFnIndirectRegisteredFuncdefScenario::DeclarationMetadata:
		return TEXT("DECLARATION_METADATA");
	case EFnIndirectRegisteredFuncdefScenario::CompatibleDirect:
		return TEXT("COMPATIBLE_DIRECT");
	case EFnIndirectRegisteredFuncdefScenario::CompatibleNested:
		return TEXT("COMPATIBLE_NESTED");
	case EFnIndirectRegisteredFuncdefScenario::NullOrUnbound:
		return TEXT("NULL_OR_UNBOUND");
	case EFnIndirectRegisteredFuncdefScenario::IncompatibleSignature:
		return TEXT("INCOMPATIBLE_SIGNATURE");
	case EFnIndirectRegisteredFuncdefScenario::RebuildOrRebind:
		return TEXT("REBUILD_OR_REBIND");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FFnIndirectRegisteredFuncdefParams& Params)
{
	return ScenarioToken(Params.Scenario) != nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FRegisteredCell& Cell)
{
	return FString::Printf(TEXT("LANG-FN-INDIRECT-REGISTERED-FUNCDEF-%s"), ScenarioToken(Cell.Scenario));
}

FString MakeEntryName(const FRegisteredCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FFnIndirectRegisteredFuncdefParams ParamsFromCell(const FRegisteredCell& Cell)
{
	FFnIndirectRegisteredFuncdefParams Params;
	Params.Scenario = Cell.Scenario;
	return Params;
}

TArray<FRegisteredCell> AllCells()
{
	TArray<FRegisteredCell> Cells;
	Cells.SetNum(6);
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Cells[Index].Scenario = static_cast<EFnIndirectRegisteredFuncdefScenario>(Index);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FRegisteredCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FRegisteredCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendEntry(FString& Source, const FFnIndirectRegisteredFuncdefParams& Params, const FString& FunctionName)
{
	AppendLine(Source, TEXT("// Host RegisterFuncdef(\"int FCallback(int Value)\")"));
	AppendLine(Source, TEXT("funcdef int FCallback(int Value);"));
	AppendLine(Source);
	const bool bNested = Params.Scenario == EFnIndirectRegisteredFuncdefScenario::CompatibleNested;
	const bool bIncompatible = Params.Scenario == EFnIndirectRegisteredFuncdefScenario::IncompatibleSignature;
	const int32 Marker = Params.Scenario == EFnIndirectRegisteredFuncdefScenario::RebuildOrRebind ? 10 : 1;
	if (bNested)
	{
		AppendLine(Source, TEXT("int IndirectHelper(int Value)"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn Value + %d;"), Marker));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	if (bIncompatible)
	{
		AppendLine(Source, TEXT("double Target(double Value)"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn Value;"));
		AppendLine(Source, TEXT("}"));
	}
	else if (Params.Scenario != EFnIndirectRegisteredFuncdefScenario::DeclarationMetadata
		&& Params.Scenario != EFnIndirectRegisteredFuncdefScenario::NullOrUnbound)
	{
		AppendLine(Source, TEXT("int Target(int Value)"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, bNested
			? TEXT("\treturn IndirectHelper(Value);")
			: FString::Printf(TEXT("\treturn Value + %d;"), Marker));
		AppendLine(Source, TEXT("}"));
	}
	if (Params.Scenario != EFnIndirectRegisteredFuncdefScenario::DeclarationMetadata)
	{
		AppendLine(Source);
	}
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Scenario == EFnIndirectRegisteredFuncdefScenario::NullOrUnbound)
	{
		AppendLine(Source, TEXT("\treturn 0;"));
	}
	else if (bIncompatible)
	{
		AppendLine(Source, TEXT("\treturn 0;"));
	}
	else if (Params.Scenario == EFnIndirectRegisteredFuncdefScenario::DeclarationMetadata)
	{
		AppendLine(Source, TEXT("\treturn 0;"));
	}
	else
	{
		AppendLine(Source, TEXT("\treturn Target(42);"));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FFnIndirectRegisteredFuncdefParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FRegisteredCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.Outcome = EGeneratedCaseOutcome::CompileReject;
	Info.Execution = EGeneratedCaseExecution::SourceOnly;
	Info.ExecutionNotes = TEXT("Registered funcdef requires host RegisterFuncdef.");
	return Info;
}
}

FStringView FFnIndirectRegisteredFuncdefGenerator::GetProductId() const
{
	return TEXT("LANG-FN-INDIRECT-REGISTERED-FUNCDEF");
}

TArray<FGeneratedCaseInfo> FFnIndirectRegisteredFuncdefGenerator::ListCases() const
{
	using namespace FnIndirectRegisteredFuncdefGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	for (const FRegisteredCell& Cell : AllCells())
	{
		Cases.Add(MakeCaseInfo(Cell));
	}
	return Cases;
}

FString FFnIndirectRegisteredFuncdefGenerator::BuildCaseSource(const FStringView CaseId) const
{
	return BuildRejectSource(CaseId);
}

FString FFnIndirectRegisteredFuncdefGenerator::BuildDumpSource() const
{
	using namespace FnIndirectRegisteredFuncdefGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FN-INDIRECT-REGISTERED-FUNCDEF"));
	AppendLine(Source, TEXT("// Cases: 6 total, 0 normal, 6 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (const FRegisteredCell& Cell : AllCells())
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

FString FFnIndirectRegisteredFuncdefGenerator::BuildAllSource(int32& OutCaseCount) const
{
	OutCaseCount = 0;
	return FString();
}

FString FFnIndirectRegisteredFuncdefGenerator::BuildRegisteredTargetSource(const FFnIndirectRegisteredFuncdefParams& Params) const
{
	using namespace FnIndirectRegisteredFuncdefGeneratorDetail;
	(void)Params;
	return FString();
}

int32 FFnIndirectRegisteredFuncdefGenerator::GetExpected(const FStringView CaseId) const
{
	(void)CaseId;
	return 0;
}

FString FFnIndirectRegisteredFuncdefGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace FnIndirectRegisteredFuncdefGeneratorDetail;
	FRegisteredCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsValidParams(ParamsFromCell(Cell)))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FFnIndirectRegisteredFuncdefGenerator::ListRejectCaseIds() const
{
	using namespace FnIndirectRegisteredFuncdefGeneratorDetail;
	TArray<FString> Ids;
	for (const FRegisteredCell& Cell : AllCells())
	{
		Ids.Add(MakeCaseId(Cell));
	}
	return Ids;
}
}
