#include "Framework/Generate/AngelscriptTestDtorPartialGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace DtorPartialGeneratorDetail
{
constexpr const TCHAR* HostNotes =
	TEXT("Needs native FNativeCaseReference, CreateNativeCaseReference, and ReachDestructorPartialBoundary.");
constexpr int32 TopologyAxisCount = 7;
constexpr int32 BoundaryAxisCount = 6;
constexpr int32 ExitAxisCount = 4;
constexpr int32 TotalCells = TopologyAxisCount * BoundaryAxisCount * ExitAxisCount;

struct FPartialCell
{
	EDtorPartialTopology Topology = EDtorPartialTopology::Independent;
	EDtorPartialBoundary Boundary = EDtorPartialBoundary::BeforeRoot;
	EDtorPartialExit Exit = EDtorPartialExit::Normal;
};

const TCHAR* TopologyToken(const EDtorPartialTopology Topology)
{
	switch (Topology)
	{
	case EDtorPartialTopology::Independent:
		return TEXT("INDEPENDENT");
	case EDtorPartialTopology::NestedMember:
		return TEXT("NESTED_MEMBER");
	case EDtorPartialTopology::BaseDerived:
		return TEXT("BASE_DERIVED");
	case EDtorPartialTopology::CopyTransfer:
		return TEXT("COPY_TRANSFER");
	case EDtorPartialTopology::AssignmentTransfer:
		return TEXT("ASSIGNMENT_TRANSFER");
	case EDtorPartialTopology::SelfAssignment:
		return TEXT("SELF_ASSIGNMENT");
	case EDtorPartialTopology::ReferenceAlias:
		return TEXT("REFERENCE_ALIAS");
	default:
		return nullptr;
	}
}

const TCHAR* BoundaryToken(const EDtorPartialBoundary Boundary)
{
	switch (Boundary)
	{
	case EDtorPartialBoundary::BeforeRoot:
		return TEXT("BEFORE_ROOT");
	case EDtorPartialBoundary::RootStarted:
		return TEXT("ROOT_STARTED");
	case EDtorPartialBoundary::FirstOwned:
		return TEXT("FIRST_OWNED");
	case EDtorPartialBoundary::MiddleOwned:
		return TEXT("MIDDLE_OWNED");
	case EDtorPartialBoundary::AllOwned:
		return TEXT("ALL_OWNED");
	case EDtorPartialBoundary::Complete:
		return TEXT("COMPLETE");
	default:
		return nullptr;
	}
}

const TCHAR* ExitToken(const EDtorPartialExit Exit)
{
	switch (Exit)
	{
	case EDtorPartialExit::Normal:
		return TEXT("NORMAL");
	case EDtorPartialExit::Exception:
		return TEXT("EXCEPTION");
	case EDtorPartialExit::Abort:
		return TEXT("ABORT");
	case EDtorPartialExit::Unprepare:
		return TEXT("UNPREPARE");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FDtorPartialParams& Params)
{
	return TopologyToken(Params.Topology) != nullptr
		&& BoundaryToken(Params.Boundary) != nullptr
		&& ExitToken(Params.Exit) != nullptr;
}

bool IsFault(const EDtorPartialExit Exit)
{
	return Exit == EDtorPartialExit::Exception;
}

int32 BoundaryStage(const EDtorPartialBoundary Boundary)
{
	return static_cast<int32>(Boundary);
}

int32 ExpectedNormalResult(const EDtorPartialBoundary Boundary)
{
	switch (Boundary)
	{
	case EDtorPartialBoundary::BeforeRoot:
		return 0;
	case EDtorPartialBoundary::RootStarted:
		return 1;
	case EDtorPartialBoundary::FirstOwned:
		return 101;
	case EDtorPartialBoundary::MiddleOwned:
		return 303;
	case EDtorPartialBoundary::AllOwned:
	case EDtorPartialBoundary::Complete:
		return 606;
	default:
		return 0;
	}
}

bool DuplicatesObservedValues(const FDtorPartialParams& Params)
{
	return Params.Boundary == EDtorPartialBoundary::Complete
		&& (Params.Topology == EDtorPartialTopology::CopyTransfer
			|| Params.Topology == EDtorPartialTopology::AssignmentTransfer
			|| Params.Topology == EDtorPartialTopology::ReferenceAlias);
}

int32 ExpectedFromParams(const FDtorPartialParams& Params)
{
	if (IsFault(Params.Exit))
	{
		return 0;
	}
	const int32 Base = ExpectedNormalResult(Params.Boundary);
	return DuplicatesObservedValues(Params) ? Base * 2 : Base;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FPartialCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-DTOR-PARTIAL-%s-%s-%s"),
		TopologyToken(Cell.Topology),
		BoundaryToken(Cell.Boundary),
		ExitToken(Cell.Exit));
}

FString MakeEntryName(const FPartialCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FDtorPartialParams ParamsFromCell(const FPartialCell& Cell)
{
	FDtorPartialParams Params;
	Params.Topology = Cell.Topology;
	Params.Boundary = Cell.Boundary;
	Params.Exit = Cell.Exit;
	return Params;
}

bool HasTransfer(const EDtorPartialTopology Topology)
{
	return Topology == EDtorPartialTopology::CopyTransfer
		|| Topology == EDtorPartialTopology::AssignmentTransfer
		|| Topology == EDtorPartialTopology::SelfAssignment
		|| Topology == EDtorPartialTopology::ReferenceAlias;
}

const TCHAR* PrimaryType(const EDtorPartialTopology Topology)
{
	switch (Topology)
	{
	case EDtorPartialTopology::NestedMember:
		return TEXT("FPartialNestedOwner");
	case EDtorPartialTopology::BaseDerived:
		return TEXT("FPartialDerivedOwner");
	case EDtorPartialTopology::ReferenceAlias:
		return TEXT("FPartialReferenceOwner");
	case EDtorPartialTopology::CopyTransfer:
	case EDtorPartialTopology::AssignmentTransfer:
	case EDtorPartialTopology::SelfAssignment:
		return TEXT("FPartialBundle");
	default:
		return nullptr;
	}
}

FString RootVariableName(const EDtorPartialTopology Topology)
{
	if (Topology == EDtorPartialTopology::Independent)
	{
		return FString();
	}
	return HasTransfer(Topology) ? FString(TEXT("Source")) : FString(TEXT("Root"));
}

FString OwnedExpression(const EDtorPartialTopology Topology, const TCHAR* FieldName)
{
	if (Topology == EDtorPartialTopology::Independent)
	{
		return FString(FieldName);
	}
	return RootVariableName(Topology) + TEXT(".") + FieldName;
}

FString SourceChecksum(const EDtorPartialTopology Topology)
{
	return OwnedExpression(Topology, TEXT("First")) + TEXT(".Value + ")
		+ OwnedExpression(Topology, TEXT("Middle")) + TEXT(".Value + ")
		+ OwnedExpression(Topology, TEXT("Last")) + TEXT(".Value");
}

FString CompleteResultExpression(const EDtorPartialTopology Topology)
{
	if (Topology == EDtorPartialTopology::CopyTransfer
		|| Topology == EDtorPartialTopology::AssignmentTransfer)
	{
		return SourceChecksum(Topology)
			+ TEXT(" + Target.First.Value + Target.Middle.Value + Target.Last.Value");
	}
	if (Topology == EDtorPartialTopology::ReferenceAlias)
	{
		return SourceChecksum(Topology)
			+ TEXT(" + Alias.First.Value + Alias.Middle.Value + Alias.Last.Value");
	}
	return SourceChecksum(Topology);
}

FString ResultExpressionAtStage(const EDtorPartialTopology Topology, const int32 Stage)
{
	switch (Stage)
	{
	case 0:
		return TEXT("0");
	case 1:
		return TEXT("1");
	case 2:
		return OwnedExpression(Topology, TEXT("First")) + TEXT(".Value");
	case 3:
		return OwnedExpression(Topology, TEXT("First")) + TEXT(".Value + ")
			+ OwnedExpression(Topology, TEXT("Middle")) + TEXT(".Value");
	case 4:
		return SourceChecksum(Topology);
	default:
		return CompleteResultExpression(Topology);
	}
}

void AppendSharedTypes(FString& Source)
{
	AppendLine(Source, TEXT("class FPartialNestedOwner"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseReference First;"));
	AppendLine(Source, TEXT("\tFNativeCaseReference Middle;"));
	AppendLine(Source, TEXT("\tFNativeCaseReference Last;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FPartialBaseOwner"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseReference First;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FPartialDerivedOwner : FPartialBaseOwner"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseReference Middle;"));
	AppendLine(Source, TEXT("\tFNativeCaseReference Last;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FPartialBundle"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseReference First;"));
	AppendLine(Source, TEXT("\tFNativeCaseReference Middle;"));
	AppendLine(Source, TEXT("\tFNativeCaseReference Last;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FPartialReferenceOwner"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseReference First;"));
	AppendLine(Source, TEXT("\tFNativeCaseReference Middle;"));
	AppendLine(Source, TEXT("\tFNativeCaseReference Last;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendTopologyTypes(FString& Source, const EDtorPartialTopology Topology)
{
	if (Topology == EDtorPartialTopology::Independent)
	{
		return;
	}
	if (Topology == EDtorPartialTopology::NestedMember)
	{
		AppendLine(Source, TEXT("class FPartialNestedOwner"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFNativeCaseReference First;"));
		AppendLine(Source, TEXT("\tFNativeCaseReference Middle;"));
		AppendLine(Source, TEXT("\tFNativeCaseReference Last;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Topology == EDtorPartialTopology::BaseDerived)
	{
		AppendLine(Source, TEXT("class FPartialBaseOwner"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFNativeCaseReference First;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, TEXT("class FPartialDerivedOwner : FPartialBaseOwner"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFNativeCaseReference Middle;"));
		AppendLine(Source, TEXT("\tFNativeCaseReference Last;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Topology == EDtorPartialTopology::ReferenceAlias)
	{
		AppendLine(Source, TEXT("class FPartialReferenceOwner"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFNativeCaseReference First;"));
		AppendLine(Source, TEXT("\tFNativeCaseReference Middle;"));
		AppendLine(Source, TEXT("\tFNativeCaseReference Last;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else
	{
		AppendLine(Source, TEXT("struct FPartialBundle"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFNativeCaseReference First;"));
		AppendLine(Source, TEXT("\tFNativeCaseReference Middle;"));
		AppendLine(Source, TEXT("\tFNativeCaseReference Last;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
}

void AppendRecovery(FString& Source)
{
	AppendLine(Source, TEXT("int RunDestructorPartialRecovery()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseReference Recovery = CreateNativeCaseReference(909);"));
	AppendLine(Source, TEXT("\treturn Recovery.Value;"));
	AppendLine(Source, TEXT("}"));
}

void AppendBoundaryExit(FString& Source, const int32 Stage, const FString& ResultExpression, const bool bFault)
{
	AppendLine(Source, FString::Printf(TEXT("\tRecordDestructorPartialStage(%d);"), Stage));
	AppendLine(Source, FString::Printf(TEXT("\tif (ReachDestructorPartialBoundary(%d))"), Stage));
	AppendLine(Source, TEXT("\t{"));
	if (bFault)
	{
		AppendLine(Source, TEXT("\t\tint Zero = 0;"));
		AppendLine(Source, TEXT("\t\treturn 1 / Zero;"));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\t\treturn %s;"), *ResultExpression));
	}
	AppendLine(Source, TEXT("\t}"));
}

void AppendRootStart(FString& Source, const EDtorPartialTopology Topology)
{
	if (Topology == EDtorPartialTopology::Independent)
	{
		AppendLine(Source, TEXT("\tint RootStarted = 1;"));
		return;
	}
	const TCHAR* const Type = PrimaryType(Topology);
	AppendLine(Source, FString::Printf(TEXT("\t%s %s = %s();"), Type, *RootVariableName(Topology), Type));
}

void AppendOwnedStage(FString& Source, const EDtorPartialTopology Topology, const TCHAR* FieldName, const int32 Value)
{
	if (Topology == EDtorPartialTopology::Independent)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\tFNativeCaseReference %s = CreateNativeCaseReference(%d);"),
			FieldName,
			Value));
		return;
	}
	AppendLine(Source, FString::Printf(
		TEXT("\t%s = CreateNativeCaseReference(%d);"),
		*OwnedExpression(Topology, FieldName),
		Value));
}

void AppendTransferCompletion(FString& Source, const EDtorPartialTopology Topology)
{
	if (!HasTransfer(Topology))
	{
		AppendLine(Source, TEXT("\tint Completed = 1;"));
		return;
	}
	AppendLine(Source, TEXT("\tRecordDestructorPartialTransferStart();"));
	if (Topology == EDtorPartialTopology::CopyTransfer)
	{
		AppendLine(Source, TEXT("\tFPartialBundle Target = Source;"));
		AppendLine(Source, TEXT("\tRecordDestructorPartialRelation(false, Source.First is Target.First, Source.Middle is Target.Middle, Source.Last is Target.Last);"));
	}
	else if (Topology == EDtorPartialTopology::AssignmentTransfer)
	{
		AppendLine(Source, TEXT("\tFPartialBundle Target;"));
		AppendLine(Source, TEXT("\tTarget = Source;"));
		AppendLine(Source, TEXT("\tRecordDestructorPartialRelation(false, Source.First is Target.First, Source.Middle is Target.Middle, Source.Last is Target.Last);"));
	}
	else if (Topology == EDtorPartialTopology::SelfAssignment)
	{
		AppendLine(Source, TEXT("\tSource = Source;"));
		AppendLine(Source, TEXT("\tRecordDestructorPartialRelation(true, Source.First is Source.First, Source.Middle is Source.Middle, Source.Last is Source.Last);"));
	}
	else
	{
		AppendLine(Source, TEXT("\tFPartialReferenceOwner Alias = Source;"));
		AppendLine(Source, TEXT("\tRecordDestructorPartialRelation(Source is Alias, Source.First is Alias.First, Source.Middle is Alias.Middle, Source.Last is Alias.Last);"));
	}
}

void AppendEntry(FString& Source, const FDtorPartialParams& Params, const FString& FunctionName)
{
	const int32 Stage = BoundaryStage(Params.Boundary);
	const bool bFault = IsFault(Params.Exit);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Stage == 0)
	{
		AppendBoundaryExit(Source, 0, ResultExpressionAtStage(Params.Topology, 0), bFault);
		AppendLine(Source, TEXT("\treturn -1;"));
		AppendLine(Source, TEXT("}"));
		return;
	}
	AppendRootStart(Source, Params.Topology);
	if (Stage == 1)
	{
		AppendBoundaryExit(Source, 1, ResultExpressionAtStage(Params.Topology, 1), bFault);
		AppendLine(Source, TEXT("\treturn -1;"));
		AppendLine(Source, TEXT("}"));
		return;
	}
	AppendLine(Source, TEXT("\tRecordDestructorPartialStage(1);"));
	AppendOwnedStage(Source, Params.Topology, TEXT("First"), 101);
	if (Stage == 2)
	{
		AppendBoundaryExit(Source, 2, ResultExpressionAtStage(Params.Topology, 2), bFault);
		AppendLine(Source, TEXT("\treturn -1;"));
		AppendLine(Source, TEXT("}"));
		return;
	}
	AppendLine(Source, TEXT("\tRecordDestructorPartialStage(2);"));
	AppendOwnedStage(Source, Params.Topology, TEXT("Middle"), 202);
	if (Stage == 3)
	{
		AppendBoundaryExit(Source, 3, ResultExpressionAtStage(Params.Topology, 3), bFault);
		AppendLine(Source, TEXT("\treturn -1;"));
		AppendLine(Source, TEXT("}"));
		return;
	}
	AppendLine(Source, TEXT("\tRecordDestructorPartialStage(3);"));
	AppendOwnedStage(Source, Params.Topology, TEXT("Last"), 303);
	if (Stage == 4)
	{
		AppendBoundaryExit(Source, 4, ResultExpressionAtStage(Params.Topology, 4), bFault);
		AppendLine(Source, TEXT("\treturn -1;"));
		AppendLine(Source, TEXT("}"));
		return;
	}
	AppendLine(Source, TEXT("\tRecordDestructorPartialStage(4);"));
	AppendTransferCompletion(Source, Params.Topology);
	AppendBoundaryExit(Source, 5, ResultExpressionAtStage(Params.Topology, 5), bFault);
	AppendLine(Source, TEXT("\treturn -1;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FDtorPartialParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendTopologyTypes(Source, Params.Topology);
	AppendEntry(Source, Params, FunctionName);
	AppendLine(Source);
	AppendRecovery(Source);
	return Source;
}

TArray<FPartialCell> AllCells()
{
	TArray<FPartialCell> Cells;
	Cells.SetNum(TotalCells);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		FPartialCell& Cell = Cells[Index];
		Cell.Topology = static_cast<EDtorPartialTopology>(Index / (BoundaryAxisCount * ExitAxisCount));
		const int32 Remainder = Index % (BoundaryAxisCount * ExitAxisCount);
		Cell.Boundary = static_cast<EDtorPartialBoundary>(Remainder / ExitAxisCount);
		Cell.Exit = static_cast<EDtorPartialExit>(Remainder % ExitAxisCount);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FPartialCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FPartialCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FGeneratedCaseInfo MakeCaseInfo(const FPartialCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExecutionNotes = HostNotes;
	if (IsFault(Cell.Exit))
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.ExpectedException = TEXT("Divide by zero");
	}
	else
	{
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.ExpectedReturn = ExpectedFromParams(ParamsFromCell(Cell));
	}
	return Info;
}
}

FStringView FDtorPartialGenerator::GetProductId() const
{
	return TEXT("LANG-DTOR-PARTIAL");
}

TArray<FGeneratedCaseInfo> FDtorPartialGenerator::ListCases() const
{
	using namespace DtorPartialGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FPartialCell& Cell : AllCells())
	{
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("DtorPartial PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MakeCaseInfo(Cell));
	}
	return Cases;
}

FString FDtorPartialGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace DtorPartialGeneratorDetail;
	FPartialCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FDtorPartialGenerator::BuildDumpSource() const
{
	using namespace DtorPartialGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-DTOR-PARTIAL"));
	AppendLine(Source, TEXT("// Cases: 168 total, 126 normal, 0 reject, 42 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedTypes(Source);
	AppendRecovery(Source);
	for (const FPartialCell& Cell : AllCells())
	{
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		if (IsFault(Cell.Exit))
		{
			AppendLine(Source, TEXT("// Outcome: RuntimeException"));
			AppendLine(Source, TEXT("// ExpectedException: Divide by zero"));
		}
		else
		{
			AppendLine(Source, TEXT("// Outcome: ReturnValue"));
			AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		}
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), HostNotes));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FDtorPartialGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace DtorPartialGeneratorDetail;
	FString Source;
	AppendSharedTypes(Source);
	AppendRecovery(Source);
	int32 Emitted = 0;
	for (const FPartialCell& Cell : AllCells())
	{
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

FString FDtorPartialGenerator::BuildDestructorPartialSource(const FDtorPartialParams& Params) const
{
	using namespace DtorPartialGeneratorDetail;
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

int32 FDtorPartialGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace DtorPartialGeneratorDetail;
	FPartialCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsFault(Cell.Exit))
	{
		return 0;
	}
	return ExpectedFromParams(ParamsFromCell(Cell));
}
}
