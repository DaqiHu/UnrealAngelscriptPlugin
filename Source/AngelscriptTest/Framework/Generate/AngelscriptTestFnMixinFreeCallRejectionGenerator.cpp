#include "Framework/Generate/AngelscriptTestFnMixinFreeCallRejectionGenerator.h"

namespace AngelscriptTest::Generate
{
namespace FnMixinFreeCallRejectionGeneratorDetail
{
struct FMixinRejectCell
{
	EFnMixinFreeCallRejectionNamespace Namespace = EFnMixinFreeCallRejectionNamespace::Global;
};

const TCHAR* NamespaceToken(const EFnMixinFreeCallRejectionNamespace Namespace)
{
	switch (Namespace)
	{
	case EFnMixinFreeCallRejectionNamespace::Global:
		return TEXT("GLOBAL");
	case EFnMixinFreeCallRejectionNamespace::Nested:
		return TEXT("NESTED");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FFnMixinFreeCallRejectionParams& Params)
{
	return NamespaceToken(Params.Namespace) != nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FMixinRejectCell& Cell)
{
	return FString::Printf(TEXT("LANG-FN-MIXIN-FREE-CALL-REJECTION-%s"), NamespaceToken(Cell.Namespace));
}

FString MakeEntryName(const FMixinRejectCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FFnMixinFreeCallRejectionParams ParamsFromCell(const FMixinRejectCell& Cell)
{
	FFnMixinFreeCallRejectionParams Params;
	Params.Namespace = Cell.Namespace;
	return Params;
}

TArray<FMixinRejectCell> AllCells()
{
	TArray<FMixinRejectCell> Cells;
	Cells.SetNum(2);
	Cells[0].Namespace = EFnMixinFreeCallRejectionNamespace::Global;
	Cells[1].Namespace = EFnMixinFreeCallRejectionNamespace::Nested;
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FMixinRejectCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FMixinRejectCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendCounterAndMixin(FString& Source, const TCHAR* Indent)
{
	AppendLine(Source, FString::Printf(TEXT("%sstruct Counter"), Indent));
	AppendLine(Source, FString::Printf(TEXT("%s{"), Indent));
	AppendLine(Source, FString::Printf(TEXT("%s\tint Value = 0;"), Indent));
	AppendLine(Source, FString::Printf(TEXT("%s}"), Indent));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("%smixin void AddToCounter(Counter& Self, int Delta)"), Indent));
	AppendLine(Source, FString::Printf(TEXT("%s{"), Indent));
	AppendLine(Source, FString::Printf(TEXT("%s\tSelf.Value += Delta;"), Indent));
	AppendLine(Source, FString::Printf(TEXT("%s}"), Indent));
}

void AppendEntry(FString& Source, const FFnMixinFreeCallRejectionParams& Params, const FString& FunctionName)
{
	if (Params.Namespace == EFnMixinFreeCallRejectionNamespace::Nested)
	{
		AppendLine(Source, TEXT("namespace Tools"));
		AppendLine(Source, TEXT("{"));
	}
	AppendCounterAndMixin(Source, TEXT(""));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tCounter Value;"));
	AppendLine(Source, TEXT("\tAddToCounter(Value, 3);"));
	AppendLine(Source, TEXT("\treturn Value.Value == 3 ? 1 : 0;"));
	AppendLine(Source, TEXT("}"));
	if (Params.Namespace == EFnMixinFreeCallRejectionNamespace::Nested)
	{
		AppendLine(Source, TEXT("}"));
	}
}

FString BuildOwnedSource(const FFnMixinFreeCallRejectionParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FMixinRejectCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.Outcome = EGeneratedCaseOutcome::CompileReject;
	Info.Execution = EGeneratedCaseExecution::SourceOnly;
	return Info;
}
}

FStringView FFnMixinFreeCallRejectionGenerator::GetProductId() const
{
	return TEXT("LANG-FN-MIXIN-FREE-CALL-REJECTION");
}

TArray<FGeneratedCaseInfo> FFnMixinFreeCallRejectionGenerator::ListCases() const
{
	using namespace FnMixinFreeCallRejectionGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	for (const FMixinRejectCell& Cell : AllCells())
	{
		Cases.Add(MakeCaseInfo(Cell));
	}
	return Cases;
}

FString FFnMixinFreeCallRejectionGenerator::BuildCaseSource(const FStringView CaseId) const
{
	return BuildRejectSource(CaseId);
}

FString FFnMixinFreeCallRejectionGenerator::BuildDumpSource() const
{
	using namespace FnMixinFreeCallRejectionGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FN-MIXIN-FREE-CALL-REJECTION"));
	AppendLine(Source, TEXT("// Cases: 2 total, 0 normal, 2 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (const FMixinRejectCell& Cell : AllCells())
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

FString FFnMixinFreeCallRejectionGenerator::BuildAllSource(int32& OutCaseCount) const
{
	OutCaseCount = 0;
	return FString();
}

FString FFnMixinFreeCallRejectionGenerator::BuildFnMixinFreeCallRejectionSource(
	const FFnMixinFreeCallRejectionParams& Params) const
{
	using namespace FnMixinFreeCallRejectionGeneratorDetail;
	if (!IsValidParams(Params))
	{
		return FString();
	}
	if (!Params.FunctionName.IsEmpty() && !IsValidGeneratedFunctionName(Params.FunctionName))
	{
		return FString();
	}
	return FString();
}

int32 FFnMixinFreeCallRejectionGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace FnMixinFreeCallRejectionGeneratorDetail;
	FMixinRejectCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return 0;
}

FString FFnMixinFreeCallRejectionGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace FnMixinFreeCallRejectionGeneratorDetail;
	FMixinRejectCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FFnMixinFreeCallRejectionGenerator::ListRejectCaseIds() const
{
	using namespace FnMixinFreeCallRejectionGeneratorDetail;
	TArray<FString> Ids;
	for (const FMixinRejectCell& Cell : AllCells())
	{
		Ids.Add(MakeCaseId(Cell));
	}
	return Ids;
}
}
