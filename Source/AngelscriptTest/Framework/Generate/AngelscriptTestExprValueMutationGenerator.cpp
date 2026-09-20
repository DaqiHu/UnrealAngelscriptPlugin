#include "Framework/Generate/AngelscriptTestExprValueMutationGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ExprValueMutationGeneratorDetail
{
struct FValueMutationCell
{
	EExprValueMutationCategory Category = EExprValueMutationCategory::MutableLValue;
	EExprValueMutationMutation Mutation = EExprValueMutationMutation::Assign;
	EExprValueMutationPlacement Placement = EExprValueMutationPlacement::Local;
};

const TCHAR* CategoryToken(const EExprValueMutationCategory Category)
{
	switch (Category)
	{
	case EExprValueMutationCategory::MutableLValue:
		return TEXT("MUTABLE_LVALUE");
	case EExprValueMutationCategory::ConstLValue:
		return TEXT("CONST_LVALUE");
	case EExprValueMutationCategory::Temporary:
		return TEXT("TEMPORARY");
	case EExprValueMutationCategory::ReferenceAlias:
		return TEXT("REFERENCE_ALIAS");
	case EExprValueMutationCategory::Field:
		return TEXT("FIELD");
	case EExprValueMutationCategory::Property:
		return TEXT("PROPERTY");
	case EExprValueMutationCategory::InvalidNonLValue:
		return TEXT("INVALID_NON_LVALUE");
	default:
		return nullptr;
	}
}

const TCHAR* MutationToken(const EExprValueMutationMutation Mutation)
{
	switch (Mutation)
	{
	case EExprValueMutationMutation::Assign:
		return TEXT("ASSIGN");
	case EExprValueMutationMutation::CompoundAssign:
		return TEXT("COMPOUND_ASSIGN");
	case EExprValueMutationMutation::PrefixIncrement:
		return TEXT("PREFIX_INCREMENT");
	case EExprValueMutationMutation::PostfixIncrement:
		return TEXT("POSTFIX_INCREMENT");
	case EExprValueMutationMutation::OutArgument:
		return TEXT("OUT_ARGUMENT");
	default:
		return nullptr;
	}
}

const TCHAR* PlacementToken(const EExprValueMutationPlacement Placement)
{
	switch (Placement)
	{
	case EExprValueMutationPlacement::Local:
		return TEXT("LOCAL");
	case EExprValueMutationPlacement::Global:
		return TEXT("GLOBAL");
	case EExprValueMutationPlacement::Member:
		return TEXT("MEMBER");
	case EExprValueMutationPlacement::Indexed:
		return TEXT("INDEXED");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FExprValueMutationParams& Params)
{
	return CategoryToken(Params.Category) != nullptr
		&& MutationToken(Params.Mutation) != nullptr
		&& PlacementToken(Params.Placement) != nullptr;
}

bool IsWritable(const EExprValueMutationCategory Category)
{
	return Category == EExprValueMutationCategory::MutableLValue
		|| Category == EExprValueMutationCategory::ReferenceAlias
		|| Category == EExprValueMutationCategory::Field;
}

bool IsReject(const EExprValueMutationCategory Category)
{
	return !IsWritable(Category);
}

int32 MutationResult(const EExprValueMutationMutation Mutation)
{
	switch (Mutation)
	{
	case EExprValueMutationMutation::Assign:
		return 20;
	case EExprValueMutationMutation::CompoundAssign:
		return 15;
	case EExprValueMutationMutation::PrefixIncrement:
		return 11;
	case EExprValueMutationMutation::PostfixIncrement:
		return 10;
	case EExprValueMutationMutation::OutArgument:
		return 31;
	default:
		return 0;
	}
}

int32 MutationAfter(const EExprValueMutationMutation Mutation)
{
	switch (Mutation)
	{
	case EExprValueMutationMutation::Assign:
		return 20;
	case EExprValueMutationMutation::CompoundAssign:
		return 15;
	case EExprValueMutationMutation::PrefixIncrement:
		return 11;
	case EExprValueMutationMutation::PostfixIncrement:
		return 11;
	case EExprValueMutationMutation::OutArgument:
		return 31;
	default:
		return 0;
	}
}

int32 ExpectedFromCell(const FValueMutationCell& Cell)
{
	if (IsReject(Cell.Category))
	{
		return 0;
	}
	const int32 Result = MutationResult(Cell.Mutation);
	const int32 After = MutationAfter(Cell.Mutation);
	return Result * 10000 + After * 100 + After;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FValueMutationCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-EXPR-VALUE-MUTATION-%s-%s-%s"),
		CategoryToken(Cell.Category),
		MutationToken(Cell.Mutation),
		PlacementToken(Cell.Placement));
}

FString MakeEntryName(const FValueMutationCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FExprValueMutationParams ParamsFromCell(const FValueMutationCell& Cell)
{
	FExprValueMutationParams Params;
	Params.Category = Cell.Category;
	Params.Mutation = Cell.Mutation;
	Params.Placement = Cell.Placement;
	return Params;
}

TArray<FValueMutationCell> AllCells()
{
	TArray<FValueMutationCell> Cells;
	Cells.SetNum(140);
	for (int32 Index = 0; Index < 140; ++Index)
	{
		FValueMutationCell& Cell = Cells[Index];
		Cell.Category = static_cast<EExprValueMutationCategory>(Index / 20);
		Cell.Mutation = static_cast<EExprValueMutationMutation>((Index / 4) % 5);
		Cell.Placement = static_cast<EExprValueMutationPlacement>(Index % 4);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FValueMutationCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FValueMutationCell> Cells = AllCells();
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

void AppendSharedHelpers(FString& Source)
{
	AppendLine(Source, TEXT("int MutationMetadataWitness(int& inout Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int NextMutationOperand(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int MakeMutationTemporary(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ReadMutationNonLValue(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("void WriteMutationOut(int& out Target)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tTarget = 31;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int& BindMutationAlias(int& inout Target)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Target;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FMutationOwner"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint DirectTarget = 10;"));
	AppendLine(Source, TEXT("\tint FieldTarget = 10;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint GetPropertyTarget() const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn 10;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tvoid SetPropertyTarget(int Value)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint GetItem(int Index) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn 10;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tvoid SetItem(int Index, int Value)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint& IndexedAlias(int Index)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn DirectTarget;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FMutationOwner MakeMutationOwner()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn FMutationOwner();"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FMutationOwner GetGlobalMutationOwner()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn FMutationOwner();"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

struct FGeneratedTarget
{
	FString TargetExpression;
	FString AliasObservationExpression;
};

FString QualifiedGlobalName(const FString& FunctionName, const bool bConst)
{
	return (bConst ? TEXT("ConstGlobalTarget_") : TEXT("GlobalTarget_")) + FunctionName;
}

void AppendOwnerLocal(FString& Source, const FString& Name, const bool bConst)
{
	AppendLine(
		Source,
		FString::Printf(TEXT("\t%sFMutationOwner %s = MakeMutationOwner();"),
			bConst ? TEXT("const ") : TEXT(""),
			*Name));
}

FGeneratedTarget BuildMutablePlacementTarget(
	FString& Source,
	const EExprValueMutationPlacement Placement,
	const FString& FunctionName)
{
	FGeneratedTarget Result;
	switch (Placement)
	{
	case EExprValueMutationPlacement::Local:
		AppendLine(Source, TEXT("\tint LocalTarget = 10;"));
		Result.TargetExpression = TEXT("LocalTarget");
		break;
	case EExprValueMutationPlacement::Global:
		Result.TargetExpression = QualifiedGlobalName(FunctionName, false);
		break;
	case EExprValueMutationPlacement::Member:
		AppendOwnerLocal(Source, TEXT("RootOwner"), false);
		Result.TargetExpression = TEXT("RootOwner.DirectTarget");
		break;
	case EExprValueMutationPlacement::Indexed:
		AppendOwnerLocal(Source, TEXT("RootOwner"), false);
		AppendLine(Source, TEXT("\tint& IndexedTarget = RootOwner.IndexedAlias(0);"));
		Result.TargetExpression = TEXT("IndexedTarget");
		break;
	}
	return Result;
}

FGeneratedTarget BuildConstPlacementTarget(
	FString& Source,
	const EExprValueMutationPlacement Placement,
	const FString& FunctionName)
{
	FGeneratedTarget Result;
	switch (Placement)
	{
	case EExprValueMutationPlacement::Local:
		AppendLine(Source, TEXT("\tconst int ConstLocalTarget = 10;"));
		Result.TargetExpression = TEXT("ConstLocalTarget");
		break;
	case EExprValueMutationPlacement::Global:
		Result.TargetExpression = QualifiedGlobalName(FunctionName, true);
		break;
	case EExprValueMutationPlacement::Member:
		AppendOwnerLocal(Source, TEXT("ConstOwner"), true);
		Result.TargetExpression = TEXT("ConstOwner.DirectTarget");
		break;
	case EExprValueMutationPlacement::Indexed:
		AppendOwnerLocal(Source, TEXT("ConstOwner"), true);
		AppendLine(Source, TEXT("\tint& IndexedTarget = ConstOwner.IndexedAlias(0);"));
		Result.TargetExpression = TEXT("IndexedTarget");
		break;
	}
	return Result;
}

FGeneratedTarget BuildReferenceAliasTarget(
	FString& Source,
	const EExprValueMutationPlacement Placement,
	const FString& FunctionName)
{
	FGeneratedTarget Result;
	switch (Placement)
	{
	case EExprValueMutationPlacement::Local:
		AppendLine(Source, TEXT("\tint LocalTarget = 10;"));
		AppendLine(Source, TEXT("\tint& AliasTarget = BindMutationAlias(LocalTarget);"));
		Result.AliasObservationExpression = TEXT("LocalTarget");
		break;
	case EExprValueMutationPlacement::Global:
		AppendLine(
			Source,
			FString::Printf(TEXT("\tint& AliasTarget = BindMutationAlias(%s);"),
				*QualifiedGlobalName(FunctionName, false)));
		Result.AliasObservationExpression = QualifiedGlobalName(FunctionName, false);
		break;
	case EExprValueMutationPlacement::Member:
		AppendOwnerLocal(Source, TEXT("RootOwner"), false);
		AppendLine(Source, TEXT("\tint& AliasTarget = BindMutationAlias(RootOwner.DirectTarget);"));
		Result.AliasObservationExpression = TEXT("RootOwner.DirectTarget");
		break;
	case EExprValueMutationPlacement::Indexed:
		AppendOwnerLocal(Source, TEXT("RootOwner"), false);
		AppendLine(Source, TEXT("\tint& AliasTarget = BindMutationAlias(RootOwner.IndexedAlias(0));"));
		Result.AliasObservationExpression = TEXT("RootOwner.GetItem(0)");
		break;
	}
	Result.TargetExpression = TEXT("AliasTarget");
	return Result;
}

FString OwnerRouteExpression(FString& Source, const EExprValueMutationPlacement Placement)
{
	switch (Placement)
	{
	case EExprValueMutationPlacement::Local:
		AppendOwnerLocal(Source, TEXT("LocalOwner"), false);
		return TEXT("LocalOwner");
	case EExprValueMutationPlacement::Global:
		return TEXT("GetGlobalMutationOwner()");
	case EExprValueMutationPlacement::Member:
	case EExprValueMutationPlacement::Indexed:
		AppendOwnerLocal(Source, TEXT("RootOwner"), false);
		return TEXT("RootOwner");
	default:
		return TEXT("MakeMutationOwner()");
	}
}

FGeneratedTarget BuildFieldOrPropertyTarget(
	FString& Source,
	const EExprValueMutationCategory Category,
	const EExprValueMutationPlacement Placement)
{
	FGeneratedTarget Result;
	const FString Owner = OwnerRouteExpression(Source, Placement);
	if (Category == EExprValueMutationCategory::Field)
	{
		if (Placement == EExprValueMutationPlacement::Indexed)
		{
			AppendLine(Source, TEXT("\tint& IndexedTarget = RootOwner.IndexedAlias(0);"));
			Result.TargetExpression = TEXT("IndexedTarget");
		}
		else
		{
			Result.TargetExpression = Owner + TEXT(".FieldTarget");
		}
	}
	else
	{
		Result.TargetExpression = Owner + TEXT(".GetPropertyTarget()");
	}
	return Result;
}

FGeneratedTarget BuildGeneratedTarget(
	FString& Source,
	const FExprValueMutationParams& Params,
	const FString& FunctionName)
{
	switch (Params.Category)
	{
	case EExprValueMutationCategory::MutableLValue:
		return BuildMutablePlacementTarget(Source, Params.Placement, FunctionName);
	case EExprValueMutationCategory::ConstLValue:
		return BuildConstPlacementTarget(Source, Params.Placement, FunctionName);
	case EExprValueMutationCategory::Temporary:
	{
		FGeneratedTarget Result = BuildMutablePlacementTarget(Source, Params.Placement, FunctionName);
		Result.TargetExpression = TEXT("MakeMutationTemporary(") + Result.TargetExpression + TEXT(")");
		return Result;
	}
	case EExprValueMutationCategory::ReferenceAlias:
		return BuildReferenceAliasTarget(Source, Params.Placement, FunctionName);
	case EExprValueMutationCategory::Field:
	case EExprValueMutationCategory::Property:
		return BuildFieldOrPropertyTarget(Source, Params.Category, Params.Placement);
	case EExprValueMutationCategory::InvalidNonLValue:
	{
		FGeneratedTarget Result = BuildMutablePlacementTarget(Source, Params.Placement, FunctionName);
		Result.TargetExpression =
			TEXT("(ReadMutationNonLValue(") + Result.TargetExpression + TEXT(") + 0)");
		return Result;
	}
	default:
		return FGeneratedTarget();
	}
}

void AppendMutationOperation(FString& Source, const FString& Target, const EExprValueMutationMutation Mutation)
{
	switch (Mutation)
	{
	case EExprValueMutationMutation::Assign:
		AppendLine(Source, TEXT("\t") + Target + TEXT(" = NextMutationOperand(20);"));
		AppendLine(Source, TEXT("\tint Result = ") + Target + TEXT(";"));
		break;
	case EExprValueMutationMutation::CompoundAssign:
		AppendLine(Source, TEXT("\t") + Target + TEXT(" += NextMutationOperand(5);"));
		AppendLine(Source, TEXT("\tint Result = ") + Target + TEXT(";"));
		break;
	case EExprValueMutationMutation::PrefixIncrement:
		AppendLine(Source, TEXT("\t++") + Target + TEXT(";"));
		AppendLine(Source, TEXT("\tint Result = ") + Target + TEXT(";"));
		break;
	case EExprValueMutationMutation::PostfixIncrement:
		AppendLine(Source, TEXT("\tint Result = ") + Target + TEXT("++;"));
		break;
	case EExprValueMutationMutation::OutArgument:
		AppendLine(Source, TEXT("\tWriteMutationOut(") + Target + TEXT(");"));
		AppendLine(Source, TEXT("\tint Result = 31;"));
		break;
	}
}

void AppendQualifiedGlobals(FString& Source, const FExprValueMutationParams& Params, const FString& FunctionName)
{
	if (Params.Placement != EExprValueMutationPlacement::Global)
	{
		return;
	}
	if (Params.Category == EExprValueMutationCategory::ConstLValue)
	{
		AppendLine(Source, FString::Printf(TEXT("const int %s = 10;"), *QualifiedGlobalName(FunctionName, true)));
		AppendLine(Source);
		return;
	}
	AppendLine(Source, FString::Printf(TEXT("int %s = 10;"), *QualifiedGlobalName(FunctionName, false)));
	AppendLine(Source);
}

void AppendEntry(FString& Source, const FExprValueMutationParams& Params, const FString& FunctionName)
{
	AppendQualifiedGlobals(Source, Params, FunctionName);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	const FGeneratedTarget Target = BuildGeneratedTarget(Source, Params, FunctionName);
	AppendMutationOperation(Source, Target.TargetExpression, Params.Mutation);
	if (IsWritable(Params.Category))
	{
		AppendLine(Source, TEXT("\tint After = ") + Target.TargetExpression + TEXT(";"));
		AppendLine(
			Source,
			TEXT("\tint AliasAfter = ") +
				(Target.AliasObservationExpression.IsEmpty() ? TEXT("After") : Target.AliasObservationExpression) +
				TEXT(";"));
		AppendLine(Source, TEXT("\treturn Result * 10000 + After * 100 + AliasAfter;"));
	}
	else
	{
		AppendLine(Source, TEXT("\treturn Result;"));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FExprValueMutationParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FValueMutationCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Category))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	return Info;
}
}

FStringView FExprValueMutationGenerator::GetProductId() const
{
	return TEXT("LANG-EXPR-VALUE-MUTATION");
}

TArray<FGeneratedCaseInfo> FExprValueMutationGenerator::ListCases() const
{
	using namespace ExprValueMutationGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FValueMutationCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cells[Index]);
			checkf(!EntryNames.Contains(EntryName), TEXT("ExprValueMutation PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FExprValueMutationGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ExprValueMutationGeneratorDetail;
	FValueMutationCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FExprValueMutationGenerator::BuildDumpSource() const
{
	using namespace ExprValueMutationGeneratorDetail;
	const TArray<FValueMutationCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-EXPR-VALUE-MUTATION"));
	AppendLine(Source, TEXT("// Cases: 140 total, 60 normal, 80 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	bool bNeedBlank = true;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Category))
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
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		bNeedBlank = true;
	}
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (!IsReject(Cells[Index].Category))
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
		AppendSharedHelpers(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

FString FExprValueMutationGenerator::BuildExpressionValueCategorySource(const FExprValueMutationParams& Params) const
{
	using namespace ExprValueMutationGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Category))
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

FString FExprValueMutationGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ExprValueMutationGeneratorDetail;
	const TArray<FValueMutationCell> Cells = AllCells();
	FString Source;
	AppendSharedHelpers(Source);
	int32 Emitted = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Category))
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

int32 FExprValueMutationGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ExprValueMutationGeneratorDetail;
	FValueMutationCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Category))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}

FString FExprValueMutationGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace ExprValueMutationGeneratorDetail;
	FValueMutationCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Category))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FExprValueMutationGenerator::ListRejectCaseIds() const
{
	using namespace ExprValueMutationGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FValueMutationCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Category))
		{
			Ids.Add(MakeCaseId(Cells[Index]));
		}
	}
	return Ids;
}
}
