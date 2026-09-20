#include "Framework/Generate/AngelscriptTestVarShadowGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace VarShadowGeneratorDetail
{
constexpr int32 PathCount = 22;

struct FShadowCell
{
	EVarShadowRelation Relation = EVarShadowRelation::None;
	EVarShadowPath Path = EVarShadowPath::FunctionAfterDeclaration;
};

const TCHAR* RelationToken(const EVarShadowRelation Relation)
{
	switch (Relation)
	{
	case EVarShadowRelation::None:
		return TEXT("NONE");
	case EVarShadowRelation::InnerOuter:
		return TEXT("INNER_OUTER");
	case EVarShadowRelation::ParameterGlobal:
		return TEXT("PARAMETER_GLOBAL");
	case EVarShadowRelation::ParameterMember:
		return TEXT("PARAMETER_MEMBER");
	case EVarShadowRelation::Sibling:
		return TEXT("SIBLING");
	default:
		return nullptr;
	}
}

const TCHAR* PathToken(const EVarShadowPath Path)
{
	switch (Path)
	{
	case EVarShadowPath::FunctionBeforeDeclaration:
		return TEXT("FUNCTION_BEFORE_DECLARATION");
	case EVarShadowPath::FunctionAfterDeclaration:
		return TEXT("FUNCTION_AFTER_DECLARATION");
	case EVarShadowPath::FunctionSiblingCall:
		return TEXT("FUNCTION_SIBLING_CALL");
	case EVarShadowPath::NestedBlockBefore:
		return TEXT("NESTED_BLOCK_BEFORE");
	case EVarShadowPath::NestedBlockInside:
		return TEXT("NESTED_BLOCK_INSIDE");
	case EVarShadowPath::NestedBlockAfter:
		return TEXT("NESTED_BLOCK_AFTER");
	case EVarShadowPath::IfBranchBefore:
		return TEXT("IF_BRANCH_BEFORE");
	case EVarShadowPath::IfBranchInside:
		return TEXT("IF_BRANCH_INSIDE");
	case EVarShadowPath::IfBranchAfter:
		return TEXT("IF_BRANCH_AFTER");
	case EVarShadowPath::SwitchCaseBefore:
		return TEXT("SWITCH_CASE_BEFORE");
	case EVarShadowPath::SwitchCaseInside:
		return TEXT("SWITCH_CASE_INSIDE");
	case EVarShadowPath::SwitchCaseAfter:
		return TEXT("SWITCH_CASE_AFTER");
	case EVarShadowPath::ForInitializer:
		return TEXT("FOR_INITIALIZER");
	case EVarShadowPath::ForBody:
		return TEXT("FOR_BODY");
	case EVarShadowPath::ForAfter:
		return TEXT("FOR_AFTER");
	case EVarShadowPath::WhileBefore:
		return TEXT("WHILE_BEFORE");
	case EVarShadowPath::WhileBody:
		return TEXT("WHILE_BODY");
	case EVarShadowPath::WhileAfter:
		return TEXT("WHILE_AFTER");
	case EVarShadowPath::NestedCallCallerBefore:
		return TEXT("NESTED_CALL_CALLER_BEFORE");
	case EVarShadowPath::NestedCallCallee:
		return TEXT("NESTED_CALL_CALLEE");
	case EVarShadowPath::NestedCallCallerAfter:
		return TEXT("NESTED_CALL_CALLER_AFTER");
	case EVarShadowPath::AfterOwner:
		return TEXT("AFTER_OWNER");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FVarShadowParams& Params)
{
	return RelationToken(Params.Relation) != nullptr && PathToken(Params.Path) != nullptr;
}

bool IsParameterRelation(const EVarShadowRelation Relation)
{
	return Relation == EVarShadowRelation::ParameterGlobal
		|| Relation == EVarShadowRelation::ParameterMember;
}

bool IsBeforeDeclarationPath(const EVarShadowPath Path)
{
	return Path == EVarShadowPath::FunctionBeforeDeclaration
		|| Path == EVarShadowPath::NestedBlockBefore
		|| Path == EVarShadowPath::IfBranchBefore
		|| Path == EVarShadowPath::SwitchCaseBefore
		|| Path == EVarShadowPath::WhileBefore
		|| Path == EVarShadowPath::NestedCallCallerBefore;
}

bool IsCallTargetPath(const EVarShadowPath Path)
{
	return Path == EVarShadowPath::FunctionSiblingCall
		|| Path == EVarShadowPath::NestedCallCallee;
}

bool IsInnerTargetPath(const EVarShadowPath Path)
{
	return Path == EVarShadowPath::NestedBlockInside
		|| Path == EVarShadowPath::IfBranchInside
		|| Path == EVarShadowPath::SwitchCaseInside
		|| Path == EVarShadowPath::ForInitializer
		|| Path == EVarShadowPath::ForBody
		|| Path == EVarShadowPath::WhileBody
		|| IsCallTargetPath(Path);
}

bool IsReject(const EVarShadowRelation Relation, const EVarShadowPath Path)
{
	if (Path == EVarShadowPath::AfterOwner)
	{
		return !IsParameterRelation(Relation);
	}
	return IsBeforeDeclarationPath(Path) && !IsParameterRelation(Relation);
}

int32 InnerValue(const EVarShadowRelation Relation)
{
	if (Relation == EVarShadowRelation::InnerOuter)
	{
		return 22;
	}
	if (Relation == EVarShadowRelation::Sibling)
	{
		return 21;
	}
	return 11;
}

int32 ExpectedFromCell(const EVarShadowRelation Relation, const EVarShadowPath Path)
{
	if (Path == EVarShadowPath::AfterOwner && IsParameterRelation(Relation))
	{
		return 5;
	}
	if (IsParameterRelation(Relation))
	{
		return 11;
	}
	if (IsInnerTargetPath(Path))
	{
		return InnerValue(Relation);
	}
	return 11;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FShadowCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-VAR-SHADOW-%s-%s"),
		RelationToken(Cell.Relation),
		PathToken(Cell.Path));
}

FString MakeEntryName(const FShadowCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FVarShadowParams ParamsFromCell(const FShadowCell& Cell)
{
	FVarShadowParams Params;
	Params.Relation = Cell.Relation;
	Params.Path = Cell.Path;
	return Params;
}

TArray<FShadowCell> AllCells()
{
	TArray<FShadowCell> Cells;
	Cells.SetNum(110);
	for (int32 Index = 0; Index < 110; ++Index)
	{
		FShadowCell& Cell = Cells[Index];
		Cell.Relation = static_cast<EVarShadowRelation>(Index / PathCount);
		Cell.Path = static_cast<EVarShadowPath>(Index % PathCount);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FShadowCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FShadowCell> Cells = AllCells();
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

FString HelperSuffix(const EVarShadowRelation Relation)
{
	FString Token(RelationToken(Relation));
	Token[0] = FChar::ToUpper(Token[0]);
	for (int32 Index = 1; Index < Token.Len(); ++Index)
	{
		if (Token[Index] == TEXT('_'))
		{
			continue;
		}
		if (Index > 0 && Token[Index - 1] == TEXT('_'))
		{
			Token[Index] = FChar::ToUpper(Token[Index]);
		}
		else
		{
			Token[Index] = FChar::ToLower(Token[Index]);
		}
	}
	Token.ReplaceInline(TEXT("_"), TEXT(""));
	return Token;
}

FString PathSuffix(const EVarShadowPath Path)
{
	return MakeCanonicalEntryName(FString(PathToken(Path))).Mid(5);
}

void AppendOuterDeclaration(FString& Source, const EVarShadowRelation Relation, const TCHAR* Prefix)
{
	if (!IsParameterRelation(Relation))
	{
		AppendLine(Source, FString::Printf(TEXT("%sint Subject = 11;"), Prefix));
	}
}

void AppendInnerDeclaration(FString& Source, const EVarShadowRelation Relation, const TCHAR* Prefix)
{
	if (Relation == EVarShadowRelation::InnerOuter || Relation == EVarShadowRelation::Sibling)
	{
		AppendLine(Source, FString::Printf(TEXT("%sint Subject = %d;"), Prefix, InnerValue(Relation)));
	}
}

void AppendCallHelper(FString& Source, const EVarShadowRelation Relation, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(
		TEXT("int %s(%s)"),
		*FunctionName,
		IsParameterRelation(Relation) ? TEXT("int Subject") : TEXT("")));
	AppendLine(Source, TEXT("{"));
	if (!IsParameterRelation(Relation))
	{
		AppendLine(Source, FString::Printf(TEXT("\tint Subject = %d;"), InnerValue(Relation)));
	}
	AppendLine(Source, TEXT("\treturn Subject;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

FString MakeCallExpression(const EVarShadowRelation Relation, const FString& FunctionName)
{
	return FString::Printf(
		TEXT("%s(%s)"),
		*FunctionName,
		IsParameterRelation(Relation) ? TEXT("Subject") : TEXT(""));
}

void AppendOwnerBody(FString& Source, const FVarShadowParams& Params, const FString& SiblingName, const FString& NestedName)
{
	const EVarShadowRelation Relation = Params.Relation;
	const EVarShadowPath Path = Params.Path;
	if (Path == EVarShadowPath::AfterOwner)
	{
		AppendLine(Source, TEXT("\treturn 0;"));
	}
	else if (Path == EVarShadowPath::FunctionBeforeDeclaration)
	{
		AppendLine(Source, TEXT("\tint Captured = Subject;"));
		AppendOuterDeclaration(Source, Relation, TEXT("\t"));
		AppendLine(Source, TEXT("\treturn Captured;"));
	}
	else if (Path == EVarShadowPath::FunctionAfterDeclaration)
	{
		AppendOuterDeclaration(Source, Relation, TEXT("\t"));
		AppendLine(Source, TEXT("\treturn Subject;"));
	}
	else if (Path == EVarShadowPath::FunctionSiblingCall)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *MakeCallExpression(Relation, SiblingName)));
	}
	else if (Path == EVarShadowPath::NestedBlockBefore
		|| Path == EVarShadowPath::NestedBlockInside
		|| Path == EVarShadowPath::NestedBlockAfter)
	{
		if (Path == EVarShadowPath::NestedBlockBefore)
		{
			AppendLine(Source, TEXT("\tint Captured = Subject;"));
		}
		AppendOuterDeclaration(Source, Relation, TEXT("\t"));
		AppendLine(Source, TEXT("\tint ScopeTrace = 0;"));
		AppendLine(Source, TEXT("\t{"));
		AppendInnerDeclaration(Source, Relation, TEXT("\t\t"));
		if (Path == EVarShadowPath::NestedBlockInside)
		{
			AppendLine(Source, TEXT("\t\treturn Subject;"));
		}
		else
		{
			AppendLine(Source, TEXT("\t\tScopeTrace = Subject;"));
		}
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, Path == EVarShadowPath::NestedBlockBefore
			? TEXT("\treturn Captured + ScopeTrace - ScopeTrace;")
			: TEXT("\treturn Subject + ScopeTrace - ScopeTrace;"));
	}
	else if (Path == EVarShadowPath::IfBranchBefore
		|| Path == EVarShadowPath::IfBranchInside
		|| Path == EVarShadowPath::IfBranchAfter)
	{
		if (Path == EVarShadowPath::IfBranchBefore)
		{
			AppendLine(Source, TEXT("\tint Captured = Subject;"));
		}
		AppendOuterDeclaration(Source, Relation, TEXT("\t"));
		AppendLine(Source, TEXT("\tint ScopeTrace = 0;"));
		AppendLine(Source, TEXT("\tif (true)"));
		AppendLine(Source, TEXT("\t{"));
		AppendInnerDeclaration(Source, Relation, TEXT("\t\t"));
		if (Path == EVarShadowPath::IfBranchInside)
		{
			AppendLine(Source, TEXT("\t\treturn Subject;"));
		}
		else
		{
			AppendLine(Source, TEXT("\t\tScopeTrace = Subject;"));
		}
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, Path == EVarShadowPath::IfBranchBefore
			? TEXT("\treturn Captured + ScopeTrace - ScopeTrace;")
			: TEXT("\treturn Subject + ScopeTrace - ScopeTrace;"));
	}
	else if (Path == EVarShadowPath::SwitchCaseBefore
		|| Path == EVarShadowPath::SwitchCaseInside
		|| Path == EVarShadowPath::SwitchCaseAfter)
	{
		if (Path == EVarShadowPath::SwitchCaseBefore)
		{
			AppendLine(Source, TEXT("\tint Captured = Subject;"));
		}
		AppendOuterDeclaration(Source, Relation, TEXT("\t"));
		AppendLine(Source, TEXT("\tint ScopeTrace = 0;"));
		AppendLine(Source, TEXT("\tswitch (1)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\tcase 1:"));
		AppendLine(Source, TEXT("\t{"));
		AppendInnerDeclaration(Source, Relation, TEXT("\t\t"));
		if (Path == EVarShadowPath::SwitchCaseInside)
		{
			AppendLine(Source, TEXT("\t\treturn Subject;"));
		}
		else
		{
			AppendLine(Source, TEXT("\t\tScopeTrace = Subject;"));
			AppendLine(Source, TEXT("\t\tbreak;"));
		}
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\tdefault:"));
		AppendLine(Source, TEXT("\t\tbreak;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, Path == EVarShadowPath::SwitchCaseBefore
			? TEXT("\treturn Captured + ScopeTrace - ScopeTrace;")
			: TEXT("\treturn Subject + ScopeTrace - ScopeTrace;"));
	}
	else if (Path == EVarShadowPath::ForInitializer)
	{
		if (IsParameterRelation(Relation))
		{
			AppendLine(Source, TEXT("\tfor (int Iteration = Subject; Iteration == 11; ++Iteration)"));
		}
		else
		{
			AppendLine(Source, FString::Printf(
				TEXT("\tfor (int Subject = %d; Subject == %d; ++Subject)"),
				InnerValue(Relation),
				InnerValue(Relation)));
		}
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn Subject;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn -1;"));
	}
	else if (Path == EVarShadowPath::ForBody || Path == EVarShadowPath::ForAfter)
	{
		AppendOuterDeclaration(Source, Relation, TEXT("\t"));
		AppendLine(Source, TEXT("\tint ScopeTrace = 0;"));
		AppendLine(Source, TEXT("\tfor (int Iteration = 0; Iteration < 1; ++Iteration)"));
		AppendLine(Source, TEXT("\t{"));
		AppendInnerDeclaration(Source, Relation, TEXT("\t\t"));
		if (Path == EVarShadowPath::ForBody)
		{
			AppendLine(Source, TEXT("\t\treturn Subject;"));
		}
		else
		{
			AppendLine(Source, TEXT("\t\tScopeTrace = Subject;"));
		}
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn Subject + ScopeTrace - ScopeTrace;"));
	}
	else if (Path == EVarShadowPath::WhileBefore
		|| Path == EVarShadowPath::WhileBody
		|| Path == EVarShadowPath::WhileAfter)
	{
		if (Path == EVarShadowPath::WhileBefore)
		{
			AppendLine(Source, TEXT("\tint Captured = Subject;"));
		}
		AppendOuterDeclaration(Source, Relation, TEXT("\t"));
		AppendLine(Source, TEXT("\tint ScopeTrace = 0;"));
		AppendLine(Source, TEXT("\tint Iteration = 0;"));
		AppendLine(Source, TEXT("\twhile (Iteration < 1)"));
		AppendLine(Source, TEXT("\t{"));
		AppendInnerDeclaration(Source, Relation, TEXT("\t\t"));
		AppendLine(Source, TEXT("\t\t++Iteration;"));
		if (Path == EVarShadowPath::WhileBody)
		{
			AppendLine(Source, TEXT("\t\treturn Subject;"));
		}
		else
		{
			AppendLine(Source, TEXT("\t\tScopeTrace = Subject;"));
		}
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, Path == EVarShadowPath::WhileBefore
			? TEXT("\treturn Captured + ScopeTrace - ScopeTrace;")
			: TEXT("\treturn Subject + ScopeTrace - ScopeTrace;"));
	}
	else if (Path == EVarShadowPath::NestedCallCallerBefore)
	{
		AppendLine(Source, TEXT("\tint Captured = Subject;"));
		AppendOuterDeclaration(Source, Relation, TEXT("\t"));
		AppendLine(Source, FString::Printf(
			TEXT("\tint NestedObserved = %s;"),
			*MakeCallExpression(Relation, NestedName)));
		AppendLine(Source, TEXT("\treturn Captured + NestedObserved - NestedObserved;"));
	}
	else if (Path == EVarShadowPath::NestedCallCallee)
	{
		AppendOuterDeclaration(Source, Relation, TEXT("\t"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *MakeCallExpression(Relation, NestedName)));
	}
	else
	{
		AppendOuterDeclaration(Source, Relation, TEXT("\t"));
		AppendLine(Source, FString::Printf(
			TEXT("\tint NestedObserved = %s;"),
			*MakeCallExpression(Relation, NestedName)));
		AppendLine(Source, TEXT("\treturn Subject + NestedObserved - NestedObserved;"));
	}
}

void AppendIndentedBody(
	FString& Source,
	const FVarShadowParams& Params,
	const FString& SiblingName,
	const FString& NestedName)
{
	FString Body;
	AppendOwnerBody(Body, Params, SiblingName, NestedName);
	TArray<FString> Lines;
	Body.ParseIntoArrayLines(Lines, false);
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		AppendLine(Source, Lines[Index].IsEmpty() ? FString() : TEXT("\t") + Lines[Index]);
	}
}

void AppendEntry(FString& Source, const FVarShadowParams& Params, const FString& FunctionName)
{
	const FString RelationSuffix = HelperSuffix(Params.Relation);
	const FString SiblingName = TEXT("ScopeSibling") + RelationSuffix;
	const FString NestedName = TEXT("NestedScopeCall") + RelationSuffix + PathSuffix(Params.Path);
	if (Params.Path == EVarShadowPath::FunctionSiblingCall)
	{
		AppendCallHelper(Source, Params.Relation, SiblingName);
	}
	if (Params.Path == EVarShadowPath::NestedCallCallerBefore
		|| Params.Path == EVarShadowPath::NestedCallCallee
		|| Params.Path == EVarShadowPath::NestedCallCallerAfter)
	{
		AppendCallHelper(Source, Params.Relation, NestedName);
	}

	if (Params.Relation == EVarShadowRelation::ParameterMember)
	{
		const FString ClassName = TEXT("FScopeOwner") + PathSuffix(Params.Path);
		AppendLine(Source, FString::Printf(TEXT("class %s"), *ClassName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Subject = 5;"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\tint Evaluate(int Subject)"));
		AppendLine(Source, TEXT("\t{"));
		AppendIndentedBody(Source, Params, SiblingName, NestedName);
		AppendLine(Source, TEXT("\t}"));
		if (Params.Path == EVarShadowPath::AfterOwner)
		{
			AppendLine(Source);
			AppendLine(Source, TEXT("\tint ReadAfterOwner()"));
			AppendLine(Source, TEXT("\t{"));
			AppendLine(Source, TEXT("\t\treturn Subject;"));
			AppendLine(Source, TEXT("\t}"));
		}
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\t%s Owner = %s();"), *ClassName, *ClassName));
		AppendLine(Source, Params.Path == EVarShadowPath::AfterOwner
			? TEXT("\treturn Owner.ReadAfterOwner();")
			: TEXT("\treturn Owner.Evaluate(11);"));
		AppendLine(Source, TEXT("}"));
		return;
	}

	if (Params.Relation == EVarShadowRelation::ParameterGlobal)
	{
		AppendLine(Source, TEXT("const int Subject = 5;"));
		AppendLine(Source);
	}
	AppendLine(Source, FString::Printf(
		TEXT("int ScopeOwner%s(%s)"),
		*FunctionName,
		Params.Relation == EVarShadowRelation::ParameterGlobal ? TEXT("int Subject") : TEXT("")));
	AppendLine(Source, TEXT("{"));
	AppendOwnerBody(Source, Params, SiblingName, NestedName);
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	if (Params.Path == EVarShadowPath::AfterOwner)
	{
		AppendLine(Source, TEXT("int ReadAfterOwner()"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn Subject;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Path == EVarShadowPath::AfterOwner)
	{
		AppendLine(Source, TEXT("\treturn ReadAfterOwner();"));
	}
	else
	{
		AppendLine(Source, FString::Printf(
			TEXT("\treturn ScopeOwner%s(%s);"),
			*FunctionName,
			Params.Relation == EVarShadowRelation::ParameterGlobal ? TEXT("11") : TEXT("")));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FVarShadowParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FShadowCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Relation, Cell.Path))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = ExpectedFromCell(Cell.Relation, Cell.Path);
	return Info;
}
}

FStringView FVarShadowGenerator::GetProductId() const
{
	return TEXT("LANG-VAR-SHADOW");
}

TArray<FGeneratedCaseInfo> FVarShadowGenerator::ListCases() const
{
	using namespace VarShadowGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FShadowCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cells[Index]);
			checkf(!EntryNames.Contains(EntryName), TEXT("VarShadow PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FVarShadowGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace VarShadowGeneratorDetail;
	FShadowCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FVarShadowGenerator::BuildDumpSource() const
{
	using namespace VarShadowGeneratorDetail;
	const TArray<FShadowCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-VAR-SHADOW"));
	AppendLine(Source, TEXT("// Cases: 110 total, 89 normal, 21 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FShadowCell& Cell = Cells[Index];
		if (IsReject(Cell.Relation, Cell.Path))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FShadowCell& Cell = Cells[Index];
		if (!IsReject(Cell.Relation, Cell.Path))
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
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FVarShadowGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace VarShadowGeneratorDetail;
	const TArray<FShadowCell> Cells = AllCells();
	FString Source;
	int32 Emitted = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Relation, Cells[Index].Path))
		{
			continue;
		}
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

FString FVarShadowGenerator::BuildScopeSource(const FVarShadowParams& Params) const
{
	using namespace VarShadowGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Relation, Params.Path))
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

int32 FVarShadowGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace VarShadowGeneratorDetail;
	FShadowCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Relation, Cell.Path))
	{
		return 0;
	}
	return ExpectedFromCell(Cell.Relation, Cell.Path);
}

FString FVarShadowGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace VarShadowGeneratorDetail;
	FShadowCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Relation, Cell.Path))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FVarShadowGenerator::ListRejectCaseIds() const
{
	using namespace VarShadowGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FShadowCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].Relation, Cells[Index].Path))
		{
			Ids.Add(MakeCaseId(Cells[Index]));
		}
	}
	return Ids;
}
}
