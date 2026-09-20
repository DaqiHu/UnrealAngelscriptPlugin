#include "Framework/Generate/AngelscriptTestExprAssociativityGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ExprAssociativityGeneratorDetail
{
constexpr const TCHAR* HostNotes =
	TEXT("Needs native MarkPrecedence. Observation is SelectedUsesLeftControl.");

struct FAssociativityCell
{
	EExprAssociativityGrouping Grouping = EExprAssociativityGrouping::Unparenthesized;
	EExprAssociativityLevel Level = EExprAssociativityLevel::Multiplicative;
	EExprAssociativitySequence Sequence = EExprAssociativitySequence::RepeatedOperator;
};

const TCHAR* GroupingToken(const EExprAssociativityGrouping Grouping)
{
	switch (Grouping)
	{
	case EExprAssociativityGrouping::Unparenthesized:
		return TEXT("UNPARENTHESIZED");
	case EExprAssociativityGrouping::LeftParenthesized:
		return TEXT("LEFT_PARENTHESIZED");
	case EExprAssociativityGrouping::RightParenthesized:
		return TEXT("RIGHT_PARENTHESIZED");
	default:
		return nullptr;
	}
}

const TCHAR* LevelToken(const EExprAssociativityLevel Level)
{
	switch (Level)
	{
	case EExprAssociativityLevel::Multiplicative:
		return TEXT("MULTIPLICATIVE");
	case EExprAssociativityLevel::Additive:
		return TEXT("ADDITIVE");
	case EExprAssociativityLevel::Shift:
		return TEXT("SHIFT");
	case EExprAssociativityLevel::Relational:
		return TEXT("RELATIONAL");
	case EExprAssociativityLevel::Equality:
		return TEXT("EQUALITY");
	case EExprAssociativityLevel::BitwiseAnd:
		return TEXT("BITWISE_AND");
	case EExprAssociativityLevel::BitwiseXor:
		return TEXT("BITWISE_XOR");
	case EExprAssociativityLevel::BitwiseOr:
		return TEXT("BITWISE_OR");
	case EExprAssociativityLevel::LogicalAnd:
		return TEXT("LOGICAL_AND");
	case EExprAssociativityLevel::LogicalOr:
		return TEXT("LOGICAL_OR");
	case EExprAssociativityLevel::Conditional:
		return TEXT("CONDITIONAL");
	case EExprAssociativityLevel::Assignment:
		return TEXT("ASSIGNMENT");
	default:
		return nullptr;
	}
}

const TCHAR* SequenceToken(const EExprAssociativitySequence Sequence)
{
	switch (Sequence)
	{
	case EExprAssociativitySequence::RepeatedOperator:
		return TEXT("REPEATED_OPERATOR");
	case EExprAssociativitySequence::MixedSameLevel:
		return TEXT("MIXED_SAME_LEVEL");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FExprAssociativityParams& Params)
{
	return GroupingToken(Params.Grouping) != nullptr
		&& LevelToken(Params.Level) != nullptr
		&& SequenceToken(Params.Sequence) != nullptr;
}

bool IsReject(const EExprAssociativityLevel Level, const EExprAssociativitySequence Sequence)
{
	if (Level == EExprAssociativityLevel::Assignment)
	{
		return true;
	}
	if (Sequence != EExprAssociativitySequence::RepeatedOperator)
	{
		return false;
	}
	return Level == EExprAssociativityLevel::LogicalAnd || Level == EExprAssociativityLevel::LogicalOr;
}

bool IsRejectCell(const FAssociativityCell& Cell)
{
	return IsReject(Cell.Level, Cell.Sequence);
}

bool SelectedUsesLeftControl(const FAssociativityCell& Cell)
{
	if (Cell.Grouping == EExprAssociativityGrouping::LeftParenthesized)
	{
		return true;
	}
	if (Cell.Grouping == EExprAssociativityGrouping::RightParenthesized)
	{
		return false;
	}
	return Cell.Level != EExprAssociativityLevel::Conditional
		&& Cell.Level != EExprAssociativityLevel::Assignment;
}

int32 ExpectedFromCell(const FAssociativityCell& Cell)
{
	return SelectedUsesLeftControl(Cell) ? 1 : 0;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FAssociativityCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-EXPR-ASSOCIATIVITY-%s-%s-%s"),
		GroupingToken(Cell.Grouping),
		LevelToken(Cell.Level),
		SequenceToken(Cell.Sequence));
}

FString MakeEntryName(const FAssociativityCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FExprAssociativityParams ParamsFromCell(const FAssociativityCell& Cell)
{
	FExprAssociativityParams Params;
	Params.Grouping = Cell.Grouping;
	Params.Level = Cell.Level;
	Params.Sequence = Cell.Sequence;
	return Params;
}

const TCHAR* PrimaryToken(const EExprAssociativityLevel Level)
{
	switch (Level)
	{
	case EExprAssociativityLevel::Multiplicative:
		return TEXT("*");
	case EExprAssociativityLevel::Additive:
		return TEXT("+");
	case EExprAssociativityLevel::Shift:
		return TEXT("<<");
	case EExprAssociativityLevel::Relational:
		return TEXT("<");
	case EExprAssociativityLevel::Equality:
		return TEXT("==");
	case EExprAssociativityLevel::BitwiseAnd:
		return TEXT("&");
	case EExprAssociativityLevel::BitwiseXor:
		return TEXT("^");
	case EExprAssociativityLevel::BitwiseOr:
		return TEXT("|");
	case EExprAssociativityLevel::LogicalAnd:
		return TEXT("&&");
	case EExprAssociativityLevel::LogicalOr:
		return TEXT("||");
	default:
		return TEXT("=");
	}
}

const TCHAR* SecondaryToken(const EExprAssociativityLevel Level)
{
	switch (Level)
	{
	case EExprAssociativityLevel::Multiplicative:
		return TEXT("/");
	case EExprAssociativityLevel::Additive:
		return TEXT("-");
	case EExprAssociativityLevel::Shift:
		return TEXT(">>");
	case EExprAssociativityLevel::Relational:
		return TEXT(">=");
	case EExprAssociativityLevel::Equality:
		return TEXT("!=");
	case EExprAssociativityLevel::Assignment:
		return TEXT("+=");
	default:
		return PrimaryToken(Level);
	}
}

bool HasDistinctSecondaryToken(const EExprAssociativityLevel Level)
{
	return Level == EExprAssociativityLevel::Multiplicative
		|| Level == EExprAssociativityLevel::Additive
		|| Level == EExprAssociativityLevel::Shift
		|| Level == EExprAssociativityLevel::Relational
		|| Level == EExprAssociativityLevel::Equality
		|| Level == EExprAssociativityLevel::Assignment;
}

FString SelectedExpression(const FExprAssociativityParams& Params)
{
	const bool bMixed = Params.Sequence == EExprAssociativitySequence::MixedSameLevel;
	FString Unparenthesized;
	FString LeftParenthesized;
	FString RightParenthesized;
	if (Params.Level == EExprAssociativityLevel::Conditional)
	{
		if (bMixed)
		{
			Unparenthesized = TEXT("A ? B ? C : D : E");
			LeftParenthesized = TEXT("(A ? B : C) ? D : E");
			RightParenthesized = TEXT("A ? (B ? C : D) : E");
		}
		else
		{
			Unparenthesized = TEXT("A ? B : C ? D : E");
			LeftParenthesized = TEXT("(A ? B : C) ? D : E");
			RightParenthesized = TEXT("A ? B : (C ? D : E)");
		}
	}
	else if (Params.Level == EExprAssociativityLevel::Assignment)
	{
		const TCHAR* const FirstToken = bMixed ? SecondaryToken(Params.Level) : PrimaryToken(Params.Level);
		Unparenthesized = FString::Printf(TEXT("A %s B = C"), FirstToken);
		LeftParenthesized = FString::Printf(TEXT("(A %s B) = C"), FirstToken);
		RightParenthesized = FString::Printf(TEXT("A %s (B = C)"), FirstToken);
	}
	else
	{
		const TCHAR* const RightToken = bMixed ? SecondaryToken(Params.Level) : PrimaryToken(Params.Level);
		const TCHAR* const MiddleOperand =
			bMixed && !HasDistinctSecondaryToken(Params.Level) ? TEXT("Q") : TEXT("B");
		Unparenthesized = FString::Printf(
			TEXT("A %s %s %s C"), PrimaryToken(Params.Level), MiddleOperand, RightToken);
		LeftParenthesized = FString::Printf(
			TEXT("(A %s %s) %s C"), PrimaryToken(Params.Level), MiddleOperand, RightToken);
		RightParenthesized = FString::Printf(
			TEXT("A %s (%s %s C)"), PrimaryToken(Params.Level), MiddleOperand, RightToken);
	}

	if (Params.Grouping == EExprAssociativityGrouping::LeftParenthesized)
	{
		return LeftParenthesized;
	}
	if (Params.Grouping == EExprAssociativityGrouping::RightParenthesized)
	{
		return RightParenthesized;
	}
	return Unparenthesized;
}

void AppendHelpers(FString& Source)
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

void AppendEntry(FString& Source, const FExprAssociativityParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFPrecedenceProbe A(2, true);"));
	AppendLine(Source, TEXT("\tFPrecedenceProbe B(3, false);"));
	AppendLine(Source, TEXT("\tFPrecedenceProbe C(5, true);"));
	AppendLine(Source, TEXT("\tFPrecedenceProbe D(7, false);"));
	AppendLine(Source, TEXT("\tFPrecedenceProbe E(11, true);"));
	AppendLine(Source, TEXT("\tbool Q = false;"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("\treturn ObservePrecedence(%s);"), *SelectedExpression(Params)));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FExprAssociativityParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendHelpers(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

TArray<FAssociativityCell> AllCells()
{
	TArray<FAssociativityCell> Cells;
	Cells.Reserve(72);
	for (int32 GroupingIndex = 0; GroupingIndex < 3; ++GroupingIndex)
	{
		for (int32 LevelIndex = 0; LevelIndex < 12; ++LevelIndex)
		{
			for (int32 SequenceIndex = 0; SequenceIndex < 2; ++SequenceIndex)
			{
				FAssociativityCell Cell;
				Cell.Grouping = static_cast<EExprAssociativityGrouping>(GroupingIndex);
				Cell.Level = static_cast<EExprAssociativityLevel>(LevelIndex);
				Cell.Sequence = static_cast<EExprAssociativitySequence>(SequenceIndex);
				Cells.Add(Cell);
			}
		}
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FAssociativityCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FAssociativityCell> Cells = AllCells();
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

FGeneratedCaseInfo MakeCaseInfo(const FAssociativityCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsRejectCell(Cell))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	Info.bLimitedObservation = true;
	Info.ExecutionNotes = HostNotes;
	return Info;
}
}

FStringView FExprAssociativityGenerator::GetProductId() const
{
	return TEXT("LANG-EXPR-ASSOCIATIVITY");
}

TArray<FGeneratedCaseInfo> FExprAssociativityGenerator::ListCases() const
{
	using namespace ExprAssociativityGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FAssociativityCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cells[Index]);
			checkf(!EntryNames.Contains(EntryName), TEXT("ExprAssociativity PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FExprAssociativityGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ExprAssociativityGeneratorDetail;
	FAssociativityCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FExprAssociativityGenerator::BuildDumpSource() const
{
	using namespace ExprAssociativityGeneratorDetail;
	const TArray<FAssociativityCell> Cells = AllCells();
	int32 NormalCount = 0;
	int32 RejectCount = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsRejectCell(Cells[Index]))
		{
			++RejectCount;
		}
		else
		{
			++NormalCount;
		}
	}

	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-EXPR-ASSOCIATIVITY"));
	AppendLine(Source, FString::Printf(
		TEXT("// Cases: %d total, %d normal, %d reject, 0 fault"),
		Cells.Num(),
		NormalCount,
		RejectCount));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendHelpers(Source);

	bool bNeedBlank = false;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsRejectCell(Cells[Index]))
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
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		AppendLine(Source, TEXT("// LimitedObservation: true"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		bNeedBlank = true;
	}

	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (!IsRejectCell(Cells[Index]))
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
		AppendHelpers(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

FString FExprAssociativityGenerator::BuildPrecedenceSource(const FExprAssociativityParams& Params) const
{
	using namespace ExprAssociativityGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Level, Params.Sequence))
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

FString FExprAssociativityGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ExprAssociativityGeneratorDetail;
	const TArray<FAssociativityCell> Cells = AllCells();
	FString Source;
	AppendHelpers(Source);
	int32 Emitted = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsRejectCell(Cells[Index]))
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

int32 FExprAssociativityGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ExprAssociativityGeneratorDetail;
	FAssociativityCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsRejectCell(Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}

FString FExprAssociativityGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace ExprAssociativityGeneratorDetail;
	FAssociativityCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsRejectCell(Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FExprAssociativityGenerator::ListRejectCaseIds() const
{
	using namespace ExprAssociativityGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FAssociativityCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsRejectCell(Cells[Index]))
		{
			Ids.Add(MakeCaseId(Cells[Index]));
		}
	}
	return Ids;
}
}
