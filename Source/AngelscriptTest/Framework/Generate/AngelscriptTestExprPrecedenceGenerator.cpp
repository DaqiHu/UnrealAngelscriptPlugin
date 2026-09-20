#include "Framework/Generate/AngelscriptTestExprPrecedenceGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ExprPrecedenceGeneratorDetail
{
struct FLevelInfo
{
	EExprPrecedenceLevel Level = EExprPrecedenceLevel::Multiplicative;
	const TCHAR* Token = TEXT("*");
	int32 Rank = 0;
	bool bRightAssociative = false;
	bool bUsesControlFlow = false;
};

struct FExpressionForms
{
	FString Unparenthesized;
	FString LeftParenthesized;
	FString RightParenthesized;
	bool bUnparenthesizedUsesLeft = false;
};

struct FPrecedenceCell
{
	EExprPrecedenceGrouping Grouping = EExprPrecedenceGrouping::Unparenthesized;
	EExprPrecedenceLevel Left = EExprPrecedenceLevel::Multiplicative;
	EExprPrecedenceLevel Right = EExprPrecedenceLevel::Additive;
};

const TCHAR* GroupingToken(const EExprPrecedenceGrouping Grouping)
{
	switch (Grouping)
	{
	case EExprPrecedenceGrouping::Unparenthesized:
		return TEXT("UNPARENTHESIZED");
	case EExprPrecedenceGrouping::LeftParenthesized:
		return TEXT("LEFT_PARENTHESIZED");
	case EExprPrecedenceGrouping::RightParenthesized:
		return TEXT("RIGHT_PARENTHESIZED");
	default:
		return nullptr;
	}
}

const TCHAR* LevelToken(const EExprPrecedenceLevel Level)
{
	switch (Level)
	{
	case EExprPrecedenceLevel::Multiplicative:
		return TEXT("MULTIPLICATIVE");
	case EExprPrecedenceLevel::Additive:
		return TEXT("ADDITIVE");
	case EExprPrecedenceLevel::Shift:
		return TEXT("SHIFT");
	case EExprPrecedenceLevel::Relational:
		return TEXT("RELATIONAL");
	case EExprPrecedenceLevel::Equality:
		return TEXT("EQUALITY");
	case EExprPrecedenceLevel::BitwiseAnd:
		return TEXT("BITWISE_AND");
	case EExprPrecedenceLevel::BitwiseXor:
		return TEXT("BITWISE_XOR");
	case EExprPrecedenceLevel::BitwiseOr:
		return TEXT("BITWISE_OR");
	case EExprPrecedenceLevel::LogicalAnd:
		return TEXT("LOGICAL_AND");
	case EExprPrecedenceLevel::LogicalOr:
		return TEXT("LOGICAL_OR");
	case EExprPrecedenceLevel::Conditional:
		return TEXT("CONDITIONAL");
	case EExprPrecedenceLevel::Assignment:
		return TEXT("ASSIGNMENT");
	default:
		return nullptr;
	}
}

FLevelInfo LevelInfo(const EExprPrecedenceLevel Level)
{
	switch (Level)
	{
	case EExprPrecedenceLevel::Multiplicative:
		return {Level, TEXT("*"), 0, false, false};
	case EExprPrecedenceLevel::Additive:
		return {Level, TEXT("+"), 1, false, false};
	case EExprPrecedenceLevel::Shift:
		return {Level, TEXT("<<"), 2, false, false};
	case EExprPrecedenceLevel::Relational:
		return {Level, TEXT("<"), 3, false, false};
	case EExprPrecedenceLevel::Equality:
		return {Level, TEXT("=="), 4, false, false};
	case EExprPrecedenceLevel::BitwiseAnd:
		return {Level, TEXT("&"), 5, false, false};
	case EExprPrecedenceLevel::BitwiseXor:
		return {Level, TEXT("^"), 6, false, false};
	case EExprPrecedenceLevel::BitwiseOr:
		return {Level, TEXT("|"), 7, false, false};
	case EExprPrecedenceLevel::LogicalAnd:
		return {Level, TEXT("&&"), 8, false, true};
	case EExprPrecedenceLevel::LogicalOr:
		return {Level, TEXT("||"), 9, false, true};
	case EExprPrecedenceLevel::Conditional:
		return {Level, TEXT("?"), 10, true, true};
	case EExprPrecedenceLevel::Assignment:
		return {Level, TEXT("="), 11, true, false};
	default:
		return {};
	}
}

bool IsValidParams(const FExprPrecedenceParams& Params)
{
	return GroupingToken(Params.Grouping) != nullptr
		&& LevelToken(Params.Left) != nullptr
		&& LevelToken(Params.Right) != nullptr;
}

bool IsRegularLevel(const EExprPrecedenceLevel Level)
{
	return LevelInfo(Level).Rank <= 9;
}

bool ContainsForkExpressionBoundary(const FString& Expression)
{
	const bool bAssignment = Expression.Contains(TEXT(" = ")) || Expression.Contains(TEXT(" += "));
	const bool bCustomLogical =
		(Expression.Contains(TEXT("&&")) || Expression.Contains(TEXT("||"))) && !Expression.Contains(TEXT("Q"));
	const bool bConditionalWithComparison = Expression.Contains(TEXT("?"))
		&& (Expression.Contains(TEXT(" < ")) || Expression.Contains(TEXT(" >= "))
			|| Expression.Contains(TEXT(" == ")) || Expression.Contains(TEXT(" != ")));
	return bAssignment || bCustomLogical || bConditionalWithComparison;
}

FExpressionForms BuildPrecedenceExpressions(const EExprPrecedenceLevel Left, const EExprPrecedenceLevel Right)
{
	FExpressionForms Forms;
	const FLevelInfo LeftInfo = LevelInfo(Left);
	const FLevelInfo RightInfo = LevelInfo(Right);
	if (IsRegularLevel(Left) && IsRegularLevel(Right))
	{
		Forms.Unparenthesized = FString::Printf(TEXT("A %s B %s C"), LeftInfo.Token, RightInfo.Token);
		Forms.LeftParenthesized = FString::Printf(TEXT("(A %s B) %s C"), LeftInfo.Token, RightInfo.Token);
		Forms.RightParenthesized = FString::Printf(TEXT("A %s (B %s C)"), LeftInfo.Token, RightInfo.Token);
		const bool bLeftBitwise = Left == EExprPrecedenceLevel::BitwiseAnd
			|| Left == EExprPrecedenceLevel::BitwiseXor
			|| Left == EExprPrecedenceLevel::BitwiseOr;
		const bool bRightBitwise = Right == EExprPrecedenceLevel::BitwiseAnd
			|| Right == EExprPrecedenceLevel::BitwiseXor
			|| Right == EExprPrecedenceLevel::BitwiseOr;
		const bool bLeftRelEq = Left == EExprPrecedenceLevel::Relational || Left == EExprPrecedenceLevel::Equality;
		const bool bRightRelEq = Right == EExprPrecedenceLevel::Relational || Right == EExprPrecedenceLevel::Equality;
		const bool bForkBitwiseOrdering = (bLeftBitwise && bRightRelEq) || (bRightBitwise && bLeftRelEq);
		Forms.bUnparenthesizedUsesLeft = bForkBitwiseOrdering
			? !(LeftInfo.Rank <= RightInfo.Rank)
			: LeftInfo.Rank <= RightInfo.Rank;
		return Forms;
	}
	if (Left == EExprPrecedenceLevel::Conditional && Right == EExprPrecedenceLevel::Conditional)
	{
		Forms.Unparenthesized = TEXT("A ? B : C ? D : E");
		Forms.LeftParenthesized = TEXT("(A ? B : C) ? D : E");
		Forms.RightParenthesized = TEXT("A ? B : (C ? D : E)");
		return Forms;
	}
	if (Left == EExprPrecedenceLevel::Conditional && Right == EExprPrecedenceLevel::Assignment)
	{
		Forms.Unparenthesized = TEXT("A ? B : C = D");
		Forms.LeftParenthesized = TEXT("(A ? B : C) = D");
		Forms.RightParenthesized = TEXT("A ? B : (C = D)");
		return Forms;
	}
	if (Left == EExprPrecedenceLevel::Assignment && Right == EExprPrecedenceLevel::Conditional)
	{
		Forms.Unparenthesized = TEXT("A = B ? C : D");
		Forms.LeftParenthesized = TEXT("(A = B) ? C : D");
		Forms.RightParenthesized = TEXT("A = (B ? C : D)");
		return Forms;
	}
	if (Left == EExprPrecedenceLevel::Assignment && Right == EExprPrecedenceLevel::Assignment)
	{
		Forms.Unparenthesized = TEXT("A = B = C");
		Forms.LeftParenthesized = TEXT("(A = B) = C");
		Forms.RightParenthesized = TEXT("A = (B = C)");
		return Forms;
	}
	if (Left == EExprPrecedenceLevel::Conditional)
	{
		Forms.Unparenthesized = FString::Printf(TEXT("A ? B : C %s D"), RightInfo.Token);
		Forms.LeftParenthesized = FString::Printf(TEXT("(A ? B : C) %s D"), RightInfo.Token);
		Forms.RightParenthesized = FString::Printf(TEXT("A ? B : (C %s D)"), RightInfo.Token);
		return Forms;
	}
	if (Right == EExprPrecedenceLevel::Conditional)
	{
		Forms.Unparenthesized = FString::Printf(TEXT("A %s B ? C : D"), LeftInfo.Token);
		Forms.LeftParenthesized = FString::Printf(TEXT("(A %s B) ? C : D"), LeftInfo.Token);
		Forms.RightParenthesized = FString::Printf(TEXT("A %s (B ? C : D)"), LeftInfo.Token);
		Forms.bUnparenthesizedUsesLeft = true;
		return Forms;
	}
	if (Left == EExprPrecedenceLevel::Assignment)
	{
		Forms.Unparenthesized = FString::Printf(TEXT("A = B %s C"), RightInfo.Token);
		Forms.LeftParenthesized = FString::Printf(TEXT("(A = B) %s C"), RightInfo.Token);
		Forms.RightParenthesized = FString::Printf(TEXT("A = (B %s C)"), RightInfo.Token);
		return Forms;
	}
	Forms.Unparenthesized = FString::Printf(TEXT("A %s B ? C : D = E"), LeftInfo.Token);
	Forms.LeftParenthesized = FString::Printf(TEXT("((A %s B) ? C : D) = E"), LeftInfo.Token);
	Forms.RightParenthesized = FString::Printf(TEXT("(A %s B) ? C : (D = E)"), LeftInfo.Token);
	return Forms;
}

bool IsForkExpressionBoundary(const FExpressionForms& Forms)
{
	return ContainsForkExpressionBoundary(Forms.Unparenthesized)
		|| ContainsForkExpressionBoundary(Forms.LeftParenthesized)
		|| ContainsForkExpressionBoundary(Forms.RightParenthesized);
}

bool SelectedUsesLeftControl(const FExpressionForms& Forms, const EExprPrecedenceGrouping Grouping)
{
	switch (Grouping)
	{
	case EExprPrecedenceGrouping::LeftParenthesized:
		return true;
	case EExprPrecedenceGrouping::RightParenthesized:
		return false;
	case EExprPrecedenceGrouping::Unparenthesized:
	default:
		return Forms.bUnparenthesizedUsesLeft;
	}
}

FString SelectedExpression(const FExpressionForms& Forms, const EExprPrecedenceGrouping Grouping)
{
	switch (Grouping)
	{
	case EExprPrecedenceGrouping::LeftParenthesized:
		return Forms.LeftParenthesized;
	case EExprPrecedenceGrouping::RightParenthesized:
		return Forms.RightParenthesized;
	case EExprPrecedenceGrouping::Unparenthesized:
	default:
		return Forms.Unparenthesized;
	}
}

bool IsReject(const FPrecedenceCell& Cell)
{
	return IsForkExpressionBoundary(BuildPrecedenceExpressions(Cell.Left, Cell.Right));
}

int32 ExpectedFromCell(const FPrecedenceCell& Cell)
{
	const FExpressionForms Forms = BuildPrecedenceExpressions(Cell.Left, Cell.Right);
	if (IsForkExpressionBoundary(Forms))
	{
		return 0;
	}
	return SelectedUsesLeftControl(Forms, Cell.Grouping) ? 1 : 0;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FPrecedenceCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-EXPR-PRECEDENCE-%s-%s-%s"),
		GroupingToken(Cell.Grouping),
		LevelToken(Cell.Left),
		LevelToken(Cell.Right));
}

FString MakeEntryName(const FPrecedenceCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FExprPrecedenceParams ParamsFromCell(const FPrecedenceCell& Cell)
{
	FExprPrecedenceParams Params;
	Params.Grouping = Cell.Grouping;
	Params.Left = Cell.Left;
	Params.Right = Cell.Right;
	return Params;
}

TArray<FPrecedenceCell> AllCells()
{
	TArray<FPrecedenceCell> Cells;
	Cells.SetNum(432);
	for (int32 Index = 0; Index < 432; ++Index)
	{
		FPrecedenceCell& Cell = Cells[Index];
		Cell.Grouping = static_cast<EExprPrecedenceGrouping>(Index / 144);
		Cell.Left = static_cast<EExprPrecedenceLevel>((Index / 12) % 12);
		Cell.Right = static_cast<EExprPrecedenceLevel>(Index % 12);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FPrecedenceCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FPrecedenceCell> Cells = AllCells();
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
	AppendLine(Source, TEXT("void MarkPrecedence(int Marker)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int BoolCode(bool Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value ? 17 : 13;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int FoldPrecedence(int Marker, int Left, int Right)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Marker * 65537 + Left * 131 + Right * 17;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FPrecedenceProbe"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Code = 0;"));
	AppendLine(Source, TEXT("\tbool Truth = false;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFPrecedenceProbe()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFPrecedenceProbe(int InCode, bool InTruth)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tCode = InCode;"));
	AppendLine(Source, TEXT("\t\tTruth = InTruth;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFPrecedenceProbe opMul(const FPrecedenceProbe& Other) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tMarkPrecedence(101);"));
	AppendLine(Source, TEXT("\t\treturn FPrecedenceProbe(FoldPrecedence(101, Code, Other.Code), Truth != Other.Truth);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFPrecedenceProbe opDiv(const FPrecedenceProbe& Other) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tMarkPrecedence(201);"));
	AppendLine(Source, TEXT("\t\treturn FPrecedenceProbe(FoldPrecedence(201, Code, Other.Code), Truth != Other.Truth);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFPrecedenceProbe opAdd(const FPrecedenceProbe& Other) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tMarkPrecedence(102);"));
	AppendLine(Source, TEXT("\t\treturn FPrecedenceProbe(FoldPrecedence(102, Code, Other.Code), Truth != Other.Truth);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFPrecedenceProbe opSub(const FPrecedenceProbe& Other) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tMarkPrecedence(202);"));
	AppendLine(Source, TEXT("\t\treturn FPrecedenceProbe(FoldPrecedence(202, Code, Other.Code), Truth != Other.Truth);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFPrecedenceProbe opShl(const FPrecedenceProbe& Other) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tMarkPrecedence(103);"));
	AppendLine(Source, TEXT("\t\treturn FPrecedenceProbe(FoldPrecedence(103, Code, Other.Code), Truth != Other.Truth);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFPrecedenceProbe opShr(const FPrecedenceProbe& Other) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tMarkPrecedence(203);"));
	AppendLine(Source, TEXT("\t\treturn FPrecedenceProbe(FoldPrecedence(203, Code, Other.Code), Truth != Other.Truth);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFPrecedenceProbe opAnd(const FPrecedenceProbe& Other) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tMarkPrecedence(106);"));
	AppendLine(Source, TEXT("\t\treturn FPrecedenceProbe(FoldPrecedence(106, Code, Other.Code), Truth != Other.Truth);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFPrecedenceProbe opXor(const FPrecedenceProbe& Other) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tMarkPrecedence(107);"));
	AppendLine(Source, TEXT("\t\treturn FPrecedenceProbe(FoldPrecedence(107, Code, Other.Code), Truth != Other.Truth);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFPrecedenceProbe opOr(const FPrecedenceProbe& Other) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tMarkPrecedence(108);"));
	AppendLine(Source, TEXT("\t\treturn FPrecedenceProbe(FoldPrecedence(108, Code, Other.Code), Truth != Other.Truth);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint opCmp(const FPrecedenceProbe& Other) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tMarkPrecedence(104);"));
	AppendLine(Source, TEXT("\t\treturn Code < Other.Code ? -1 : (Code > Other.Code ? 1 : 0);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tbool opEquals(const FPrecedenceProbe& Other) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tMarkPrecedence(105);"));
	AppendLine(Source, TEXT("\t\treturn Code == Other.Code;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tbool opImplConv() const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tMarkPrecedence(3000 + (Code & 255));"));
	AppendLine(Source, TEXT("\t\treturn Truth;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFPrecedenceProbe& opAssign(const FPrecedenceProbe& Other)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tMarkPrecedence(112);"));
	AppendLine(Source, TEXT("\t\tCode = FoldPrecedence(112, Code, Other.Code);"));
	AppendLine(Source, TEXT("\t\tTruth = Other.Truth;"));
	AppendLine(Source, TEXT("\t\treturn this;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFPrecedenceProbe& opAddAssign(const FPrecedenceProbe& Other)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tMarkPrecedence(212);"));
	AppendLine(Source, TEXT("\t\tCode = FoldPrecedence(212, Code, Other.Code);"));
	AppendLine(Source, TEXT("\t\tTruth = Truth != Other.Truth;"));
	AppendLine(Source, TEXT("\t\treturn this;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ObservePrecedence(const FPrecedenceProbe& Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value.Code;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ObservePrecedence(bool Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value ? 1 : 0;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendEntry(FString& Source, const FExprPrecedenceParams& Params, const FString& FunctionName)
{
	const FExpressionForms Forms = BuildPrecedenceExpressions(Params.Left, Params.Right);
	const FString Expression = SelectedExpression(Forms, Params.Grouping);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFPrecedenceProbe A(2, true);"));
	AppendLine(Source, TEXT("\tFPrecedenceProbe B(3, false);"));
	AppendLine(Source, TEXT("\tFPrecedenceProbe C(5, true);"));
	AppendLine(Source, TEXT("\tFPrecedenceProbe D(7, false);"));
	AppendLine(Source, TEXT("\tFPrecedenceProbe E(11, true);"));
	AppendLine(Source, TEXT("\tbool Q = false;"));
	AppendLine(Source, FString::Printf(TEXT("\treturn ObservePrecedence(%s);"), *Expression));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FExprPrecedenceParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FPrecedenceCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::SourceOnly;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	Info.bLimitedObservation = true;
	Info.ExecutionNotes = TEXT("Grouping-direction marker from SelectedUsesLeftControl; runtime Code/bytecode comparison is out of scope.");
	return Info;
}
}

FStringView FExprPrecedenceGenerator::GetProductId() const
{
	return TEXT("LANG-EXPR-PRECEDENCE");
}

TArray<FGeneratedCaseInfo> FExprPrecedenceGenerator::ListCases() const
{
	using namespace ExprPrecedenceGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FPrecedenceCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cells[Index]);
			checkf(!EntryNames.Contains(EntryName), TEXT("ExprPrecedence PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FExprPrecedenceGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ExprPrecedenceGeneratorDetail;
	FPrecedenceCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FExprPrecedenceGenerator::BuildDumpSource() const
{
	using namespace ExprPrecedenceGeneratorDetail;
	const TArray<FPrecedenceCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-EXPR-PRECEDENCE"));
	AppendLine(Source, TEXT("// Cases: 432 total, 231 normal, 201 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	bool bNeedBlank = true;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index]))
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
		AppendLine(Source, TEXT("// Execution: SourceOnly"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		AppendLine(Source, TEXT("// LimitedObservation: true"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		bNeedBlank = true;
	}
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (!IsReject(Cells[Index]))
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

FString FExprPrecedenceGenerator::BuildPrecedenceSource(const FExprPrecedenceParams& Params) const
{
	using namespace ExprPrecedenceGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(FPrecedenceCell{Params.Grouping, Params.Left, Params.Right}))
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

FString FExprPrecedenceGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ExprPrecedenceGeneratorDetail;
	const TArray<FPrecedenceCell> Cells = AllCells();
	FString Source;
	AppendSharedHelpers(Source);
	int32 Emitted = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index]))
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

int32 FExprPrecedenceGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ExprPrecedenceGeneratorDetail;
	FPrecedenceCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}

FString FExprPrecedenceGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace ExprPrecedenceGeneratorDetail;
	FPrecedenceCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FExprPrecedenceGenerator::ListRejectCaseIds() const
{
	using namespace ExprPrecedenceGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FPrecedenceCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index]))
		{
			Ids.Add(MakeCaseId(Cells[Index]));
		}
	}
	return Ids;
}
}
