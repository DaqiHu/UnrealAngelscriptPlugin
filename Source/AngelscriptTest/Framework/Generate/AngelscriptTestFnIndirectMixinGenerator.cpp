#include "Framework/Generate/AngelscriptTestFnIndirectMixinGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace FnIndirectMixinGeneratorDetail
{
struct FIndirectMixinCell
{
	EFnIndirectMixinScenario Scenario = EFnIndirectMixinScenario::DeclarationMetadata;
};

const TCHAR* ScenarioToken(const EFnIndirectMixinScenario Scenario)
{
	switch (Scenario)
	{
	case EFnIndirectMixinScenario::DeclarationMetadata:
		return TEXT("DECLARATION_METADATA");
	case EFnIndirectMixinScenario::CompatibleDirect:
		return TEXT("COMPATIBLE_DIRECT");
	case EFnIndirectMixinScenario::CompatibleNested:
		return TEXT("COMPATIBLE_NESTED");
	case EFnIndirectMixinScenario::NullOrUnbound:
		return TEXT("NULL_OR_UNBOUND");
	case EFnIndirectMixinScenario::IncompatibleSignature:
		return TEXT("INCOMPATIBLE_SIGNATURE");
	case EFnIndirectMixinScenario::RebuildOrRebind:
		return TEXT("REBUILD_OR_REBIND");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FFnIndirectMixinParams& Params)
{
	return ScenarioToken(Params.Scenario) != nullptr;
}

bool IsReject(const EFnIndirectMixinScenario Scenario)
{
	return Scenario == EFnIndirectMixinScenario::NullOrUnbound
		|| Scenario == EFnIndirectMixinScenario::IncompatibleSignature;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FIndirectMixinCell& Cell)
{
	return FString::Printf(TEXT("LANG-FN-INDIRECT-MIXIN-%s"), ScenarioToken(Cell.Scenario));
}

FString MakeEntryName(const FIndirectMixinCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FFnIndirectMixinParams ParamsFromCell(const FIndirectMixinCell& Cell)
{
	FFnIndirectMixinParams Params;
	Params.Scenario = Cell.Scenario;
	return Params;
}

TArray<FIndirectMixinCell> AllCells()
{
	TArray<FIndirectMixinCell> Cells;
	Cells.SetNum(6);
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Cells[Index].Scenario = static_cast<EFnIndirectMixinScenario>(Index);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FIndirectMixinCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FIndirectMixinCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FString HelperSuffix(const FString& FunctionName)
{
	return FunctionName == TEXT("Entry") ? FString() : FunctionName;
}

void AppendHelpers(FString& Source, const FString& FunctionName)
{
	const FString Suffix = HelperSuffix(FunctionName);
	AppendLine(Source, FString::Printf(TEXT("struct FMixinReceiver%s"), *Suffix));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 40;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(
		TEXT("mixin int AddValue%s(FMixinReceiver%s& Self, int Delta)"),
		*Suffix,
		*Suffix));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Self.Value + Delta + 0;"));
	AppendLine(Source, TEXT("}"));
}

void AppendEntry(FString& Source, const FFnIndirectMixinParams& Params, const FString& FunctionName)
{
	const FString Suffix = HelperSuffix(FunctionName);
	if (Params.Scenario == EFnIndirectMixinScenario::CompatibleNested)
	{
		AppendLine(Source, FString::Printf(TEXT("int InvokeMixin%s(FMixinReceiver%s& Value)"), *Suffix, *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn Value.AddValue%s(2);"), *Suffix));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Scenario == EFnIndirectMixinScenario::NullOrUnbound)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFMixinReceiver%s Value;"), *Suffix));
		AppendLine(Source, TEXT("\treturn Value.MissingMixin(2);"));
	}
	else if (Params.Scenario == EFnIndirectMixinScenario::IncompatibleSignature)
	{
		AppendLine(Source, TEXT("\tint Value = 40;"));
		AppendLine(Source, FString::Printf(TEXT("\treturn Value.AddValue%s(2);"), *Suffix));
	}
	else if (Params.Scenario == EFnIndirectMixinScenario::CompatibleNested)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFMixinReceiver%s Value;"), *Suffix));
		AppendLine(Source, FString::Printf(TEXT("\treturn InvokeMixin%s(Value);"), *Suffix));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\tFMixinReceiver%s Value;"), *Suffix));
		AppendLine(Source, FString::Printf(TEXT("\treturn Value.AddValue%s(2);"), *Suffix));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FFnIndirectMixinParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendHelpers(Source, FunctionName);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FIndirectMixinCell& Cell)
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
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.ExpectedReturn = 42;
	if (Cell.Scenario == EFnIndirectMixinScenario::RebuildOrRebind)
	{
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		Info.bLimitedObservation = true;
		Info.ExecutionNotes = TEXT("Rebuild observation is host lifecycle; source uses marker 0.");
	}
	else
	{
		Info.Execution = EGeneratedCaseExecution::Standalone;
	}
	return Info;
}
}

FStringView FFnIndirectMixinGenerator::GetProductId() const
{
	return TEXT("LANG-FN-INDIRECT-MIXIN");
}

TArray<FGeneratedCaseInfo> FFnIndirectMixinGenerator::ListCases() const
{
	using namespace FnIndirectMixinGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FIndirectMixinCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("FnIndirectMixin PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FFnIndirectMixinGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace FnIndirectMixinGeneratorDetail;
	FIndirectMixinCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FFnIndirectMixinGenerator::BuildDumpSource() const
{
	using namespace FnIndirectMixinGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FN-INDIRECT-MIXIN"));
	AppendLine(Source, TEXT("// Cases: 6 total, 4 normal, 2 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	bool bNeedBlank = false;
	for (const FIndirectMixinCell& Cell : AllCells())
	{
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
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, TEXT("// ExpectedReturn: 42"));
		AppendLine(Source, Info.Execution == EGeneratedCaseExecution::SourceOnly
			? TEXT("// Execution: SourceOnly")
			: TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendHelpers(Source, MakeEntryName(Cell));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}
	for (const FIndirectMixinCell& Cell : AllCells())
	{
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
		AppendHelpers(Source, MakeEntryName(Cell));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FFnIndirectMixinGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace FnIndirectMixinGeneratorDetail;
	FString Source;
	int32 Emitted = 0;
	for (const FIndirectMixinCell& Cell : AllCells())
	{
		if (IsReject(Cell.Scenario))
		{
			continue;
		}
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		AppendHelpers(Source, MakeEntryName(Cell));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

FString FFnIndirectMixinGenerator::BuildMixinSource(const FFnIndirectMixinParams& Params) const
{
	using namespace FnIndirectMixinGeneratorDetail;
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

int32 FFnIndirectMixinGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace FnIndirectMixinGeneratorDetail;
	FIndirectMixinCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Scenario))
	{
		return 0;
	}
	return 42;
}

FString FFnIndirectMixinGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace FnIndirectMixinGeneratorDetail;
	FIndirectMixinCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Scenario))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FFnIndirectMixinGenerator::ListRejectCaseIds() const
{
	using namespace FnIndirectMixinGeneratorDetail;
	TArray<FString> Ids;
	for (const FIndirectMixinCell& Cell : AllCells())
	{
		if (IsReject(Cell.Scenario))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
