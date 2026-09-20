#include "Framework/Generate/AngelscriptTestDeclFailureRecoveryGenerator.h"

namespace AngelscriptTest::Generate
{
namespace DeclFailureRecoveryGeneratorDetail
{
struct FFailureCell
{
	EDeclFailureRecoveryForm Form = EDeclFailureRecoveryForm::UnbalancedClass;
	EDeclFailureRecoveryPlacement Placement = EDeclFailureRecoveryPlacement::EntryBody;
	EDeclFailureRecoveryLineEnding LineEnding = EDeclFailureRecoveryLineEnding::Lf;
};

const TCHAR* FormToken(const EDeclFailureRecoveryForm Form)
{
	switch (Form)
	{
	case EDeclFailureRecoveryForm::UnbalancedClass:
		return TEXT("UNBALANCED_CLASS");
	case EDeclFailureRecoveryForm::BadParameterList:
		return TEXT("BAD_PARAMETER_LIST");
	case EDeclFailureRecoveryForm::UnclosedFunctionBody:
		return TEXT("UNCLOSED_FUNCTION_BODY");
	case EDeclFailureRecoveryForm::MissingTypeName:
		return TEXT("MISSING_TYPE_NAME");
	case EDeclFailureRecoveryForm::UnexpectedHandle:
		return TEXT("UNEXPECTED_HANDLE");
	case EDeclFailureRecoveryForm::UnknownBase:
		return TEXT("UNKNOWN_BASE");
	default:
		return nullptr;
	}
}

const TCHAR* PlacementToken(const EDeclFailureRecoveryPlacement Placement)
{
	switch (Placement)
	{
	case EDeclFailureRecoveryPlacement::EntryBody:
		return TEXT("ENTRY_BODY");
	case EDeclFailureRecoveryPlacement::AfterValidFunction:
		return TEXT("AFTER_VALID_FUNCTION");
	case EDeclFailureRecoveryPlacement::InsideNamespace:
		return TEXT("INSIDE_NAMESPACE");
	default:
		return nullptr;
	}
}

const TCHAR* LineEndingToken(const EDeclFailureRecoveryLineEnding LineEnding)
{
	switch (LineEnding)
	{
	case EDeclFailureRecoveryLineEnding::Lf:
		return TEXT("LF");
	case EDeclFailureRecoveryLineEnding::Crlf:
		return TEXT("CRLF");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FDeclFailureRecoveryParams& Params)
{
	return FormToken(Params.Form) != nullptr
		&& PlacementToken(Params.Placement) != nullptr
		&& Params.LineEnding == EDeclFailureRecoveryLineEnding::Lf;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FFailureCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-DECL-FAILURE-RECOVERY-%s-%s-%s"),
		FormToken(Cell.Form),
		PlacementToken(Cell.Placement),
		LineEndingToken(Cell.LineEnding));
}

FString MakeEntryName(const FFailureCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FDeclFailureRecoveryParams ParamsFromCell(const FFailureCell& Cell)
{
	FDeclFailureRecoveryParams Params;
	Params.Form = Cell.Form;
	Params.Placement = Cell.Placement;
	Params.LineEnding = Cell.LineEnding;
	return Params;
}

TArray<FFailureCell> AllCells()
{
	TArray<FFailureCell> Cells;
	Cells.SetNum(18);
	for (int32 Index = 0; Index < 18; ++Index)
	{
		FFailureCell& Cell = Cells[Index];
		Cell.Form = static_cast<EDeclFailureRecoveryForm>(Index / 3);
		Cell.Placement = static_cast<EDeclFailureRecoveryPlacement>(Index % 3);
		Cell.LineEnding = EDeclFailureRecoveryLineEnding::Lf;
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FFailureCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FFailureCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendInvalidForm(FString& Source, const EDeclFailureRecoveryForm Form)
{
	if (Form == EDeclFailureRecoveryForm::UnbalancedClass)
	{
		AppendLine(Source, TEXT("class FBroken"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tvoid Run()"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t}"));
		return;
	}
	if (Form == EDeclFailureRecoveryForm::BadParameterList)
	{
		AppendLine(Source, TEXT("void Broken(int A,, int B)"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("}"));
		return;
	}
	if (Form == EDeclFailureRecoveryForm::UnclosedFunctionBody)
	{
		AppendLine(Source, TEXT("int Broken()"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn 1;"));
		return;
	}
	if (Form == EDeclFailureRecoveryForm::MissingTypeName)
	{
		AppendLine(Source, TEXT("int;"));
		return;
	}
	if (Form == EDeclFailureRecoveryForm::UnexpectedHandle)
	{
		AppendLine(Source, TEXT("int @Broken;"));
		return;
	}
	AppendLine(Source, TEXT("class FBroken : MissingBase"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
}

FString BuildInvalidBody(const FFailureCell& Cell, const FString& FunctionName)
{
	FString Source;
	if (Cell.Placement == EDeclFailureRecoveryPlacement::AfterValidFunction)
	{
		AppendLine(Source, TEXT("int Before()"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn 7;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Cell.Placement == EDeclFailureRecoveryPlacement::InsideNamespace)
	{
		AppendLine(Source, TEXT("namespace FailureNamespace"));
		AppendLine(Source, TEXT("{"));
	}
	AppendInvalidForm(Source, Cell.Form);
	if (Cell.Placement == EDeclFailureRecoveryPlacement::InsideNamespace)
	{
		AppendLine(Source, TEXT("}"));
	}
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 23;"));
	AppendLine(Source, TEXT("}"));
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FFailureCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.Outcome = EGeneratedCaseOutcome::CompileReject;
	Info.Execution = EGeneratedCaseExecution::SourceOnly;
	return Info;
}
}

FStringView FDeclFailureRecoveryGenerator::GetProductId() const
{
	return TEXT("LANG-DECL-FAILURE-RECOVERY");
}

TArray<FGeneratedCaseInfo> FDeclFailureRecoveryGenerator::ListCases() const
{
	using namespace DeclFailureRecoveryGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	for (const FFailureCell& Cell : AllCells())
	{
		Cases.Add(MakeCaseInfo(Cell));
	}
	return Cases;
}

FString FDeclFailureRecoveryGenerator::BuildCaseSource(const FStringView CaseId) const
{
	return BuildRejectSource(CaseId);
}

FString FDeclFailureRecoveryGenerator::BuildDumpSource() const
{
	using namespace DeclFailureRecoveryGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-DECL-FAILURE-RECOVERY"));
	AppendLine(Source, TEXT("// Cases: 18 total, 0 normal, 18 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (const FFailureCell& Cell : AllCells())
	{
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, TEXT("// Reject module"));
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, TEXT("// EntryDeclaration:"));
		AppendLine(Source, TEXT("// Outcome: CompileReject"));
		AppendLine(Source, TEXT("// Execution: SourceOnly"));
		AppendLine(Source);
		Source += BuildInvalidBody(Cell, MakeEntryName(Cell));
	}
	return Source;
}

FString FDeclFailureRecoveryGenerator::BuildAllSource(int32& OutCaseCount) const
{
	OutCaseCount = 0;
	return FString();
}

FString FDeclFailureRecoveryGenerator::BuildInvalidSource(const FDeclFailureRecoveryParams& Params) const
{
	using namespace DeclFailureRecoveryGeneratorDetail;
	if (!IsValidParams(Params))
	{
		return FString();
	}
	if (!Params.FunctionName.IsEmpty() && !IsValidGeneratedFunctionName(Params.FunctionName))
	{
		return FString();
	}
	FFailureCell Cell;
	Cell.Form = Params.Form;
	Cell.Placement = Params.Placement;
	Cell.LineEnding = Params.LineEnding;
	const FString FunctionName = Params.FunctionName.IsEmpty() ? FString(TEXT("Entry")) : Params.FunctionName;
	return BuildInvalidBody(Cell, FunctionName);
}

int32 FDeclFailureRecoveryGenerator::GetExpected(const FStringView CaseId) const
{
	(void)CaseId;
	return 0;
}

FString FDeclFailureRecoveryGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace DeclFailureRecoveryGeneratorDetail;
	FFailureCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildInvalidBody(Cell, MakeEntryName(Cell));
}

TArray<FString> FDeclFailureRecoveryGenerator::ListRejectCaseIds() const
{
	using namespace DeclFailureRecoveryGeneratorDetail;
	TArray<FString> Ids;
	for (const FFailureCell& Cell : AllCells())
	{
		Ids.Add(MakeCaseId(Cell));
	}
	return Ids;
}
}
