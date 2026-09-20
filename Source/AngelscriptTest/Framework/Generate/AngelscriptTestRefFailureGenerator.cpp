#include "Framework/Generate/AngelscriptTestRefFailureGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace RefFailureGeneratorDetail
{
constexpr const TCHAR* NullPointerException = TEXT("Null pointer access");

struct FRefFailureCell
{
	ERefFailureKind Failure = ERefFailureKind::ExplicitHandle;
	ERefFailureRecovery Recovery = ERefFailureRecovery::FreshModule;
};

const TCHAR* FailureToken(const ERefFailureKind Failure)
{
	switch (Failure)
	{
	case ERefFailureKind::ExplicitHandle:
		return TEXT("EXPLICIT_HANDLE");
	case ERefFailureKind::ConstRemoval:
		return TEXT("CONST_REMOVAL");
	case ERefFailureKind::TemporaryOut:
		return TEXT("TEMPORARY_OUT");
	case ERefFailureKind::ExpiredLocalReturn:
		return TEXT("EXPIRED_LOCAL_RETURN");
	case ERefFailureKind::UnrelatedAssignment:
		return TEXT("UNRELATED_ASSIGNMENT");
	case ERefFailureKind::UnrelatedCast:
		return TEXT("UNRELATED_CAST");
	case ERefFailureKind::NullMemberAccess:
		return TEXT("NULL_MEMBER_ACCESS");
	case ERefFailureKind::StaleModuleObject:
		return TEXT("STALE_MODULE_OBJECT");
	case ERefFailureKind::AmbiguousOverload:
		return TEXT("AMBIGUOUS_OVERLOAD");
	case ERefFailureKind::IncompatibleInOut:
		return TEXT("INCOMPATIBLE_INOUT");
	default:
		return nullptr;
	}
}

const TCHAR* RecoveryToken(const ERefFailureRecovery Recovery)
{
	switch (Recovery)
	{
	case ERefFailureRecovery::FreshModule:
		return TEXT("FRESH_MODULE");
	case ERefFailureRecovery::SameModuleOrContext:
		return TEXT("SAME_MODULE_OR_CONTEXT");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FRefFailureParams& Params)
{
	return FailureToken(Params.Failure) != nullptr && RecoveryToken(Params.Recovery) != nullptr;
}

bool IsFault(const ERefFailureKind Failure)
{
	return Failure == ERefFailureKind::NullMemberAccess;
}

bool IsReject(const ERefFailureKind Failure)
{
	return !IsFault(Failure);
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FRefFailureCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-REF-FAILURE-%s-%s"),
		FailureToken(Cell.Failure),
		RecoveryToken(Cell.Recovery));
}

FString MakeEntryName(const FRefFailureCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FRefFailureParams ParamsFromCell(const FRefFailureCell& Cell)
{
	FRefFailureParams Params;
	Params.Failure = Cell.Failure;
	Params.Recovery = Cell.Recovery;
	return Params;
}

TArray<FRefFailureCell> AllCells()
{
	TArray<FRefFailureCell> Cells;
	Cells.SetNum(20);
	for (int32 Index = 0; Index < 20; ++Index)
	{
		FRefFailureCell& Cell = Cells[Index];
		Cell.Failure = static_cast<ERefFailureKind>(Index / 2);
		Cell.Recovery = static_cast<ERefFailureRecovery>(Index % 2);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FRefFailureCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FRefFailureCell> Cells = AllCells();
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

void AppendRecoveryFunction(FString& Source)
{
	AppendLine(Source, TEXT("int RecoverReferenceFailure()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFRefRoot Value = MakeRefRoot(929);"));
	AppendLine(Source, TEXT("\treturn Value.GetValue();"));
	AppendLine(Source, TEXT("}"));
}

void AppendEntry(FString& Source, const FRefFailureParams& Params, const FString& FunctionName)
{
	switch (Params.Failure)
	{
	case ERefFailureKind::ExplicitHandle:
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFRefRoot@ Explicit = MakeRefRoot(1);"));
		AppendLine(Source, TEXT("\treturn Explicit.GetIdentity();"));
		AppendLine(Source, TEXT("}"));
		break;
	case ERefFailureKind::ConstRemoval:
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tconst FRefRoot ReadOnly = MakeRefRoot(2);"));
		AppendLine(Source, TEXT("\tFRefRoot Mutable = ReadOnly;"));
		AppendLine(Source, TEXT("\tMutable.SetValue(3);"));
		AppendLine(Source, TEXT("\treturn Mutable.GetValue();"));
		AppendLine(Source, TEXT("}"));
		break;
	case ERefFailureKind::TemporaryOut:
		AppendLine(Source, TEXT("void WriteTemporaryOut(FRefRoot& out Value)"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tValue = MakeRefRoot(3);"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tWriteTemporaryOut(MakeRefRoot(4));"));
		AppendLine(Source, TEXT("\treturn 0;"));
		AppendLine(Source, TEXT("}"));
		break;
	case ERefFailureKind::ExpiredLocalReturn:
		AppendLine(Source, TEXT("int& ReturnExpiredLocal()"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Local = 5;"));
		AppendLine(Source, TEXT("\treturn Local;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint& Alias = ReturnExpiredLocal();"));
		AppendLine(Source, TEXT("\treturn Alias;"));
		AppendLine(Source, TEXT("}"));
		break;
	case ERefFailureKind::UnrelatedAssignment:
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFRefRoot Root = MakeRefRoot(6);"));
		AppendLine(Source, TEXT("\tFRefUnrelated Other = MakeRefUnrelated(7);"));
		AppendLine(Source, TEXT("\tRoot = Other;"));
		AppendLine(Source, TEXT("\treturn Root.GetValue();"));
		AppendLine(Source, TEXT("}"));
		break;
	case ERefFailureKind::UnrelatedCast:
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFRefUnrelated Other = MakeRefUnrelated(8);"));
		AppendLine(Source, TEXT("\tFRefDerived Result = cast<FRefDerived>(Other);"));
		AppendLine(Source, TEXT("\treturn Result.GetValue();"));
		AppendLine(Source, TEXT("}"));
		break;
	case ERefFailureKind::NullMemberAccess:
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFRefRoot Missing = nullptr;"));
		AppendLine(Source, TEXT("\treturn Missing.GetValue();"));
		AppendLine(Source, TEXT("}"));
		break;
	case ERefFailureKind::StaleModuleObject:
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFModuleOwnedReference Value;"));
		AppendLine(Source, TEXT("\treturn Value.Value;"));
		AppendLine(Source, TEXT("}"));
		break;
	case ERefFailureKind::AmbiguousOverload:
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn SelectAmbiguous(nullptr);"));
		AppendLine(Source, TEXT("}"));
		break;
	case ERefFailureKind::IncompatibleInOut:
		AppendLine(Source, TEXT("void MutateDerived(FRefDerived& inout Value)"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tValue.SetValue(10);"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tFRefRoot Root = MakeRefRoot(9);"));
		AppendLine(Source, TEXT("\tMutateDerived(Root);"));
		AppendLine(Source, TEXT("\treturn Root.GetValue();"));
		AppendLine(Source, TEXT("}"));
		break;
	default:
		break;
	}
}

FString BuildOwnedSource(const FRefFailureParams& Params, const FString& FunctionName, const bool bIncludeRecovery)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	if (bIncludeRecovery)
	{
		AppendLine(Source);
		AppendRecoveryFunction(Source);
	}
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FRefFailureCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Failure))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
	Info.ExpectedException = NullPointerException;
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExecutionNotes = TEXT("Needs host recovery after Null pointer access.");
	return Info;
}
}

FStringView FRefFailureGenerator::GetProductId() const
{
	return TEXT("LANG-REF-FAILURE");
}

TArray<FGeneratedCaseInfo> FRefFailureGenerator::ListCases() const
{
	using namespace RefFailureGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FRefFailureCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cells[Index]);
			checkf(!EntryNames.Contains(EntryName), TEXT("RefFailure PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FRefFailureGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace RefFailureGeneratorDetail;
	FRefFailureCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	if (IsReject(Cell.Failure))
	{
		return BuildRejectSource(CaseId);
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell), true);
}

FString FRefFailureGenerator::BuildDumpSource() const
{
	using namespace RefFailureGeneratorDetail;
	const TArray<FRefFailureCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-REF-FAILURE"));
	AppendLine(Source, TEXT("// Cases: 20 total, 0 normal, 18 reject, 2 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendRecoveryFunction(Source);
	bool bNeedBlank = true;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Failure))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (bNeedBlank)
		{
			AppendLine(Source);
		}
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: RuntimeException"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedException: %s"), NullPointerException));
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		bNeedBlank = true;
	}
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (!IsReject(Cells[Index].Failure))
		{
			continue;
		}
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

FString FRefFailureGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace RefFailureGeneratorDetail;
	FString Source;
	AppendRecoveryFunction(Source);
	int32 Emitted = 0;
	const TArray<FRefFailureCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Failure))
		{
			continue;
		}
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

FString FRefFailureGenerator::BuildReferenceFailureSource(const FRefFailureParams& Params) const
{
	using namespace RefFailureGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Failure))
	{
		return FString();
	}
	if (!Params.FunctionName.IsEmpty() && !IsValidGeneratedFunctionName(Params.FunctionName))
	{
		return FString();
	}
	const FString FunctionName = Params.FunctionName.IsEmpty() ? FString(TEXT("Entry")) : Params.FunctionName;
	return BuildOwnedSource(Params, FunctionName, true);
}

int32 FRefFailureGenerator::GetExpected(const FStringView CaseId) const
{
	(void)CaseId;
	return 0;
}

FString FRefFailureGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace RefFailureGeneratorDetail;
	FRefFailureCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Failure))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell), false);
}

TArray<FString> FRefFailureGenerator::ListRejectCaseIds() const
{
	using namespace RefFailureGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FRefFailureCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Failure))
		{
			Ids.Add(MakeCaseId(Cells[Index]));
		}
	}
	return Ids;
}
}
