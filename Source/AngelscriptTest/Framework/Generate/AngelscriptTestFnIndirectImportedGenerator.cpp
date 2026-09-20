#include "Framework/Generate/AngelscriptTestFnIndirectImportedGenerator.h"

namespace AngelscriptTest::Generate
{
namespace FnIndirectImportedGeneratorDetail
{
struct FImportedCell
{
	EFnIndirectImportedScenario Scenario = EFnIndirectImportedScenario::DeclarationMetadata;
};

const TCHAR* ScenarioToken(const EFnIndirectImportedScenario Scenario)
{
	switch (Scenario)
	{
	case EFnIndirectImportedScenario::DeclarationMetadata:
		return TEXT("DECLARATION_METADATA");
	case EFnIndirectImportedScenario::CompatibleDirect:
		return TEXT("COMPATIBLE_DIRECT");
	case EFnIndirectImportedScenario::CompatibleNested:
		return TEXT("COMPATIBLE_NESTED");
	case EFnIndirectImportedScenario::NullOrUnbound:
		return TEXT("NULL_OR_UNBOUND");
	case EFnIndirectImportedScenario::IncompatibleSignature:
		return TEXT("INCOMPATIBLE_SIGNATURE");
	case EFnIndirectImportedScenario::RebuildOrRebind:
		return TEXT("REBUILD_OR_REBIND");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FFnIndirectImportedParams& Params)
{
	return ScenarioToken(Params.Scenario) != nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FImportedCell& Cell)
{
	return FString::Printf(TEXT("LANG-FN-INDIRECT-IMPORTED-%s"), ScenarioToken(Cell.Scenario));
}

FString MakeEntryName(const FImportedCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FFnIndirectImportedParams ParamsFromCell(const FImportedCell& Cell)
{
	FFnIndirectImportedParams Params;
	Params.Scenario = Cell.Scenario;
	return Params;
}

TArray<FImportedCell> AllCells()
{
	TArray<FImportedCell> Cells;
	Cells.SetNum(6);
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Cells[Index].Scenario = static_cast<EFnIndirectImportedScenario>(Index);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FImportedCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FImportedCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendEntry(FString& Source, const FFnIndirectImportedParams& Params, const FString& FunctionName)
{
	const bool bNested = Params.Scenario == EFnIndirectImportedScenario::CompatibleNested;
	const bool bIncompatible = Params.Scenario == EFnIndirectImportedScenario::IncompatibleSignature;
	const bool bRebuild = Params.Scenario == EFnIndirectImportedScenario::RebuildOrRebind;
	const bool bHasProvider = Params.Scenario != EFnIndirectImportedScenario::DeclarationMetadata
		&& Params.Scenario != EFnIndirectImportedScenario::NullOrUnbound;
	if (bHasProvider)
	{
		if (bNested)
		{
			AppendLine(Source, TEXT("int ProviderHelper()"));
			AppendLine(Source, TEXT("{"));
			AppendLine(Source, TEXT("\treturn 42;"));
			AppendLine(Source, TEXT("}"));
			AppendLine(Source);
		}
		if (bIncompatible)
		{
			AppendLine(Source, TEXT("int SharedValue(int Value)"));
			AppendLine(Source, TEXT("{"));
			AppendLine(Source, TEXT("\treturn Value;"));
			AppendLine(Source, TEXT("}"));
		}
		else
		{
			AppendLine(Source, TEXT("int SharedValue()"));
			AppendLine(Source, TEXT("{"));
			AppendLine(Source, bNested ? TEXT("\treturn ProviderHelper();") : TEXT("\treturn 42;"));
			AppendLine(Source, TEXT("}"));
		}
		AppendLine(Source);
		if (bRebuild)
		{
			AppendLine(Source, TEXT("int ReplacementSharedValue()"));
			AppendLine(Source, TEXT("{"));
			AppendLine(Source, TEXT("\treturn 29;"));
			AppendLine(Source, TEXT("}"));
			AppendLine(Source);
		}
	}
	AppendLine(Source, TEXT("import int SharedValue() from \"ImportProvider\";"));
	AppendLine(Source);
	if (bNested)
	{
		AppendLine(Source, TEXT("int InvokeImport()"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn SharedValue();"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, bNested ? TEXT("\treturn InvokeImport();") : TEXT("\treturn SharedValue();"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FFnIndirectImportedParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FImportedCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.Outcome = EGeneratedCaseOutcome::CompileReject;
	Info.Execution = EGeneratedCaseExecution::SourceOnly;
	Info.ExecutionNotes = TEXT("Import binding is host module protocol, not a standalone aggregate.");
	return Info;
}
}

FStringView FFnIndirectImportedGenerator::GetProductId() const
{
	return TEXT("LANG-FN-INDIRECT-IMPORTED");
}

TArray<FGeneratedCaseInfo> FFnIndirectImportedGenerator::ListCases() const
{
	using namespace FnIndirectImportedGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	for (const FImportedCell& Cell : AllCells())
	{
		Cases.Add(MakeCaseInfo(Cell));
	}
	return Cases;
}

FString FFnIndirectImportedGenerator::BuildCaseSource(const FStringView CaseId) const
{
	return BuildRejectSource(CaseId);
}

FString FFnIndirectImportedGenerator::BuildDumpSource() const
{
	using namespace FnIndirectImportedGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FN-INDIRECT-IMPORTED"));
	AppendLine(Source, TEXT("// Cases: 6 total, 0 normal, 6 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (const FImportedCell& Cell : AllCells())
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

FString FFnIndirectImportedGenerator::BuildAllSource(int32& OutCaseCount) const
{
	OutCaseCount = 0;
	return FString();
}

FString FFnIndirectImportedGenerator::BuildImportProviderSource(const FFnIndirectImportedParams& Params) const
{
	using namespace FnIndirectImportedGeneratorDetail;
	(void)Params;
	return FString();
}

int32 FFnIndirectImportedGenerator::GetExpected(const FStringView CaseId) const
{
	(void)CaseId;
	return 0;
}

FString FFnIndirectImportedGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace FnIndirectImportedGeneratorDetail;
	FImportedCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsValidParams(ParamsFromCell(Cell)))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FFnIndirectImportedGenerator::ListRejectCaseIds() const
{
	using namespace FnIndirectImportedGeneratorDetail;
	TArray<FString> Ids;
	for (const FImportedCell& Cell : AllCells())
	{
		Ids.Add(MakeCaseId(Cell));
	}
	return Ids;
}
}
