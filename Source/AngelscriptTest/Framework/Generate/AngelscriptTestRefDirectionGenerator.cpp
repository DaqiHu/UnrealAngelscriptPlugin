#include "Framework/Generate/AngelscriptTestRefDirectionGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace RefDirectionGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native CaptureDirectionSnapshot.");

struct FRefDirectionCell
{
	ERefDirectionKind Direction = ERefDirectionKind::Value;
	ERefDirectionNull NullState = ERefDirectionNull::NonNull;
	ERefDirectionRelation Relation = ERefDirectionRelation::SameTwoNames;
};

const TCHAR* DirectionToken(const ERefDirectionKind Direction)
{
	switch (Direction)
	{
	case ERefDirectionKind::Value:
		return TEXT("VALUE");
	case ERefDirectionKind::In:
		return TEXT("IN");
	case ERefDirectionKind::Out:
		return TEXT("OUT");
	case ERefDirectionKind::InOut:
		return TEXT("INOUT");
	default:
		return nullptr;
	}
}

const TCHAR* NullToken(const ERefDirectionNull NullState)
{
	switch (NullState)
	{
	case ERefDirectionNull::NonNull:
		return TEXT("NON_NULL");
	case ERefDirectionNull::NullInput:
		return TEXT("NULL_INPUT");
	case ERefDirectionNull::NullOutput:
		return TEXT("NULL_OUTPUT");
	case ERefDirectionNull::NullReturn:
		return TEXT("NULL_RETURN");
	default:
		return nullptr;
	}
}

const TCHAR* RelationToken(const ERefDirectionRelation Relation)
{
	switch (Relation)
	{
	case ERefDirectionRelation::SameTwoNames:
		return TEXT("SAME_TWO_NAMES");
	case ERefDirectionRelation::Distinct:
		return TEXT("DISTINCT");
	case ERefDirectionRelation::SelfAssignment:
		return TEXT("SELF_ASSIGNMENT");
	case ERefDirectionRelation::BaseDerivedViews:
		return TEXT("BASE_DERIVED_VIEWS");
	case ERefDirectionRelation::OutReplacement:
		return TEXT("OUT_REPLACEMENT");
	case ERefDirectionRelation::InOutMutation:
		return TEXT("INOUT_MUTATION");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FRefDirectionParams& Params)
{
	return DirectionToken(Params.Direction) != nullptr
		&& NullToken(Params.NullState) != nullptr
		&& RelationToken(Params.Relation) != nullptr;
}

int32 RelationInitialValue(const ERefDirectionRelation Relation)
{
	switch (Relation)
	{
	case ERefDirectionRelation::SameTwoNames:
		return 21;
	case ERefDirectionRelation::Distinct:
		return 22;
	case ERefDirectionRelation::SelfAssignment:
		return 23;
	case ERefDirectionRelation::BaseDerivedViews:
		return 24;
	case ERefDirectionRelation::OutReplacement:
		return 25;
	case ERefDirectionRelation::InOutMutation:
		return 26;
	default:
		return 0;
	}
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FRefDirectionCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-REF-DIRECTION-%s-%s-%s"),
		DirectionToken(Cell.Direction),
		NullToken(Cell.NullState),
		RelationToken(Cell.Relation));
}

FString MakeEntryName(const FRefDirectionCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FString MakeApplyName(const FString& FunctionName)
{
	return TEXT("Apply") + FunctionName;
}

FRefDirectionParams ParamsFromCell(const FRefDirectionCell& Cell)
{
	FRefDirectionParams Params;
	Params.Direction = Cell.Direction;
	Params.NullState = Cell.NullState;
	Params.Relation = Cell.Relation;
	return Params;
}

TArray<FRefDirectionCell> AllCells()
{
	TArray<FRefDirectionCell> Cells;
	Cells.SetNum(96);
	for (int32 Index = 0; Index < 96; ++Index)
	{
		FRefDirectionCell& Cell = Cells[Index];
		Cell.Direction = static_cast<ERefDirectionKind>(Index / 24);
		Cell.NullState = static_cast<ERefDirectionNull>((Index / 6) % 4);
		Cell.Relation = static_cast<ERefDirectionRelation>(Index % 6);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FRefDirectionCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FRefDirectionCell> Cells = AllCells();
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

FString DirectionParameter(const ERefDirectionKind Direction)
{
	switch (Direction)
	{
	case ERefDirectionKind::In:
		return TEXT("FRefRoot & in Target");
	case ERefDirectionKind::Out:
		return TEXT("FRefRoot & out Target");
	case ERefDirectionKind::InOut:
		return TEXT("FRefRoot & inout Target");
	default:
		return TEXT("FRefRoot Target");
	}
}

const TCHAR* WorkingName(const ERefDirectionKind Direction)
{
	return Direction == ERefDirectionKind::In ? TEXT("Working") : TEXT("Target");
}

void AppendSharedHelpers(FString& Source)
{
	AppendLine(Source, TEXT("int ReferenceIdentity(const FRefRoot Object)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Object == nullptr"));
	AppendLine(Source, TEXT("\t\t? 0"));
	AppendLine(Source, TEXT("\t\t: Object.GetIdentity();"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ReferenceValue(const FRefRoot Object)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Object == nullptr"));
	AppendLine(Source, TEXT("\t\t? -1"));
	AppendLine(Source, TEXT("\t\t: Object.GetValue();"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FRefRoot ReturnNullReference()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn nullptr;"));
	AppendLine(Source, TEXT("}"));
}

void AppendDirectionAction(FString& Source, const FRefDirectionParams& Params)
{
	const TCHAR* const Working = WorkingName(Params.Direction);
	if (Params.Direction == ERefDirectionKind::In)
	{
		AppendLine(Source, TEXT("\tFRefRoot Working = Target;"));
	}
	if (Params.NullState == ERefDirectionNull::NullOutput)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s = nullptr;"), Working));
		return;
	}
	if (Params.NullState == ERefDirectionNull::NullReturn)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s = ReturnNullReference();"), Working));
		return;
	}
	if (Params.Direction == ERefDirectionKind::Out
		&& Params.Relation != ERefDirectionRelation::OutReplacement
		&& Params.Relation != ERefDirectionRelation::InOutMutation)
	{
		if (Params.Relation == ERefDirectionRelation::BaseDerivedViews)
		{
			AppendLine(Source, FString::Printf(TEXT("\tTarget = MakeRefDerivedAsRoot(%d);"), RelationInitialValue(Params.Relation)));
		}
		else
		{
			AppendLine(Source, FString::Printf(TEXT("\tTarget = MakeRefRoot(%d);"), RelationInitialValue(Params.Relation)));
		}
	}
	switch (Params.Relation)
	{
	case ERefDirectionRelation::SameTwoNames:
		AppendLine(Source, FString::Printf(TEXT("\tif (%s != nullptr)"), Working));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\t%s.SetValue(61);"), Working));
		AppendLine(Source, TEXT("\t}"));
		break;
	case ERefDirectionRelation::Distinct:
		AppendLine(Source, FString::Printf(TEXT("\tif (%s != nullptr)"), Working));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\t%s.SetValue(62);"), Working));
		AppendLine(Source, TEXT("\t}"));
		break;
	case ERefDirectionRelation::SelfAssignment:
		AppendLine(Source, FString::Printf(TEXT("\t%s = %s;"), Working, Working));
		break;
	case ERefDirectionRelation::BaseDerivedViews:
		AppendLine(Source, FString::Printf(TEXT("\tif (%s != nullptr)"), Working));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\t%s.SetValue(63);"), Working));
		AppendLine(Source, TEXT("\t}"));
		break;
	case ERefDirectionRelation::OutReplacement:
		AppendLine(Source, FString::Printf(TEXT("\t%s = MakeRefRoot(71);"), Working));
		break;
	case ERefDirectionRelation::InOutMutation:
		AppendLine(Source, FString::Printf(TEXT("\tif (%s == nullptr)"), Working));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\t%s = MakeRefRoot(72);"), Working));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\telse"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\t%s.SetValue(72);"), Working));
		AppendLine(Source, TEXT("\t}"));
		break;
	default:
		break;
	}
}

void AppendApply(FString& Source, const FRefDirectionParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("void %s(%s)"), *MakeApplyName(FunctionName), *DirectionParameter(Params.Direction)));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tCaptureDirectionSnapshot(2, Target);"));
	AppendDirectionAction(Source, Params);
	AppendLine(Source, FString::Printf(TEXT("\tCaptureDirectionSnapshot(3, %s);"), WorkingName(Params.Direction)));
	AppendLine(Source, TEXT("}"));
}

void AppendEntry(FString& Source, const FRefDirectionParams& Params, const FString& FunctionName)
{
	const int32 Initial = RelationInitialValue(Params.Relation);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Relation == ERefDirectionRelation::BaseDerivedViews)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFRefDerived Derived = MakeRefDerived(%d);"), Initial));
		AppendLine(Source, TEXT("\tFRefRoot Source = Derived.opImplCast();"));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\tFRefRoot Source = MakeRefRoot(%d);"), Initial));
	}
	if (Params.NullState == ERefDirectionNull::NullInput)
	{
		AppendLine(Source, TEXT("\tFRefRoot Target = nullptr;"));
	}
	else if (Params.Relation == ERefDirectionRelation::Distinct)
	{
		AppendLine(Source, TEXT("\tFRefRoot Target = MakeRefRoot(32);"));
	}
	else
	{
		AppendLine(Source, TEXT("\tFRefRoot Target = Source;"));
	}
	AppendLine(Source, TEXT("\tCaptureDirectionSnapshot(0, Source);"));
	AppendLine(Source, TEXT("\tCaptureDirectionSnapshot(1, Target);"));
	AppendLine(Source, FString::Printf(TEXT("\t%s(Target);"), *MakeApplyName(FunctionName)));
	AppendLine(Source, TEXT("\tCaptureDirectionSnapshot(4, Source);"));
	AppendLine(Source, TEXT("\tCaptureDirectionSnapshot(5, Target);"));
	AppendLine(Source, TEXT("\tCaptureDirectionSnapshot(6, Source);"));
	AppendLine(Source, TEXT("\tCaptureDirectionSnapshot(7, Target);"));
	AppendLine(Source, TEXT("\treturn 1;"));
	AppendLine(Source, TEXT("}"));
}

void AppendCaseFunctions(FString& Source, const FRefDirectionParams& Params, const FString& FunctionName)
{
	AppendApply(Source, Params, FunctionName);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
}

FString BuildOwnedSource(const FRefDirectionParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendCaseFunctions(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FRefDirectionCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExpectedReturn = 1;
	Info.bLimitedObservation = true;
	Info.ExecutionNotes = HostNotes;
	return Info;
}
}

FStringView FRefDirectionGenerator::GetProductId() const
{
	return TEXT("LANG-REF-DIRECTION");
}

TArray<FGeneratedCaseInfo> FRefDirectionGenerator::ListCases() const
{
	using namespace RefDirectionGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FRefDirectionCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		const FString EntryName = MakeEntryName(Cells[Index]);
		checkf(!EntryNames.Contains(EntryName), TEXT("RefDirection PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FRefDirectionGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace RefDirectionGeneratorDetail;
	FRefDirectionCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FRefDirectionGenerator::BuildDumpSource() const
{
	using namespace RefDirectionGeneratorDetail;
	const TArray<FRefDirectionCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-REF-DIRECTION"));
	AppendLine(Source, TEXT("// Cases: 96 total, 96 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, TEXT("// ExpectedReturn: 1"));
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), HostNotes));
		AppendLine(Source);
		AppendCaseFunctions(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

FString FRefDirectionGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace RefDirectionGeneratorDetail;
	const TArray<FRefDirectionCell> Cells = AllCells();
	OutCaseCount = Cells.Num();
	FString Source;
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		AppendLine(Source);
		AppendCaseFunctions(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

FString FRefDirectionGenerator::BuildReferenceDirectionSource(const FRefDirectionParams& Params) const
{
	using namespace RefDirectionGeneratorDetail;
	if (!IsValidParams(Params))
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

int32 FRefDirectionGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace RefDirectionGeneratorDetail;
	FRefDirectionCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return 1;
}
}
