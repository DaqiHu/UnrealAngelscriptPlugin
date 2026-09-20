#include "Framework/Generate/AngelscriptTestRefForkDerivedInrefGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace RefForkDerivedInrefGeneratorDetail
{
constexpr const TCHAR* CatalogCaseId = TEXT("LANG-REF-FORK-DERIVED-TO-BASE-INREF");

struct FRefForkDerivedInrefCell
{
	ERefForkDerivedInrefSource Source = ERefForkDerivedInrefSource::Derived;
	ERefForkDerivedInrefTarget Target = ERefForkDerivedInrefTarget::BaseInref;
};

const TCHAR* SourceToken(const ERefForkDerivedInrefSource Source)
{
	switch (Source)
	{
	case ERefForkDerivedInrefSource::Derived:
		return TEXT("DERIVED");
	default:
		return nullptr;
	}
}

const TCHAR* TargetToken(const ERefForkDerivedInrefTarget Target)
{
	switch (Target)
	{
	case ERefForkDerivedInrefTarget::BaseInref:
		return TEXT("BASE_INREF");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FRefForkDerivedInrefParams& Params)
{
	return SourceToken(Params.Source) != nullptr && TargetToken(Params.Target) != nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FRefForkDerivedInrefCell& Cell)
{
	(void)Cell;
	return FString(CatalogCaseId);
}

FString MakeEntryName(const FRefForkDerivedInrefCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FRefForkDerivedInrefParams ParamsFromCell(const FRefForkDerivedInrefCell& Cell)
{
	FRefForkDerivedInrefParams Params;
	Params.Source = Cell.Source;
	Params.Target = Cell.Target;
	return Params;
}

TArray<FRefForkDerivedInrefCell> AllCells()
{
	TArray<FRefForkDerivedInrefCell> Cells;
	Cells.SetNum(1);
	Cells[0].Source = ERefForkDerivedInrefSource::Derived;
	Cells[0].Target = ERefForkDerivedInrefTarget::BaseInref;
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FRefForkDerivedInrefCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FRefForkDerivedInrefCell> Cells = AllCells();
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

void AppendEntry(FString& Source, const FRefForkDerivedInrefParams& Params, const FString& FunctionName)
{
	(void)Params;
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFRefDerived Source = MakeRefDerived(16);"));
	AppendLine(Source, TEXT("\treturn RequireRootInput(Source, Source) ? 1 : 0;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FRefForkDerivedInrefParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FRefForkDerivedInrefCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.Outcome = EGeneratedCaseOutcome::CompileReject;
	Info.Execution = EGeneratedCaseExecution::SourceOnly;
	return Info;
}
}

FStringView FRefForkDerivedInrefGenerator::GetProductId() const
{
	return TEXT("LANG-REF-FORK-DERIVED-INREF");
}

TArray<FGeneratedCaseInfo> FRefForkDerivedInrefGenerator::ListCases() const
{
	using namespace RefForkDerivedInrefGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	const TArray<FRefForkDerivedInrefCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		Cases.Add(MakeCaseInfo(Cells[Index]));
	}
	return Cases;
}

FString FRefForkDerivedInrefGenerator::BuildCaseSource(const FStringView CaseId) const
{
	return BuildRejectSource(CaseId);
}

FString FRefForkDerivedInrefGenerator::BuildDumpSource() const
{
	using namespace RefForkDerivedInrefGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-REF-FORK-DERIVED-INREF"));
	AppendLine(Source, TEXT("// Cases: 1 total, 0 normal, 1 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	const TArray<FRefForkDerivedInrefCell> Cells = AllCells();
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

FString FRefForkDerivedInrefGenerator::BuildAllSource(int32& OutCaseCount) const
{
	OutCaseCount = 0;
	return FString();
}

FString FRefForkDerivedInrefGenerator::BuildRefForkDerivedInrefSource(const FRefForkDerivedInrefParams& Params) const
{
	using namespace RefForkDerivedInrefGeneratorDetail;
	(void)Params;
	return FString();
}

int32 FRefForkDerivedInrefGenerator::GetExpected(const FStringView CaseId) const
{
	(void)CaseId;
	return 0;
}

FString FRefForkDerivedInrefGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace RefForkDerivedInrefGeneratorDetail;
	FRefForkDerivedInrefCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsValidParams(ParamsFromCell(Cell)))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FRefForkDerivedInrefGenerator::ListRejectCaseIds() const
{
	using namespace RefForkDerivedInrefGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FRefForkDerivedInrefCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		Ids.Add(MakeCaseId(Cells[Index]));
	}
	return Ids;
}
}
