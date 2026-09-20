#include "Framework/Generate/AngelscriptTestExprEvalOrderGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ExprEvalOrderGeneratorDetail
{
struct FEvalOrderCell
{
	EExprEvalOrderComposition Composition = EExprEvalOrderComposition::Binary;
	EExprEvalOrderOperandCount OperandCount = EExprEvalOrderOperandCount::Two;
	EExprEvalOrderOutcome Outcome = EExprEvalOrderOutcome::Complete;
	EExprEvalOrderSourceShape SourceShape = EExprEvalOrderSourceShape::SingleLine;
};

const TCHAR* CompositionToken(const EExprEvalOrderComposition Composition)
{
	switch (Composition)
	{
	case EExprEvalOrderComposition::Binary:
		return TEXT("BINARY");
	case EExprEvalOrderComposition::Assignment:
		return TEXT("ASSIGNMENT");
	case EExprEvalOrderComposition::CompoundAssignment:
		return TEXT("COMPOUND_ASSIGNMENT");
	case EExprEvalOrderComposition::CallArguments:
		return TEXT("CALL_ARGUMENTS");
	case EExprEvalOrderComposition::ConstructorArguments:
		return TEXT("CONSTRUCTOR_ARGUMENTS");
	case EExprEvalOrderComposition::IndexArguments:
		return TEXT("INDEX_ARGUMENTS");
	case EExprEvalOrderComposition::CallChain:
		return TEXT("CALL_CHAIN");
	case EExprEvalOrderComposition::MemberIndexChain:
		return TEXT("MEMBER_INDEX_CHAIN");
	case EExprEvalOrderComposition::NestedCast:
		return TEXT("NESTED_CAST");
	default:
		return nullptr;
	}
}

const TCHAR* OperandCountToken(const EExprEvalOrderOperandCount OperandCount)
{
	switch (OperandCount)
	{
	case EExprEvalOrderOperandCount::Two:
		return TEXT("TWO");
	case EExprEvalOrderOperandCount::Three:
		return TEXT("THREE");
	case EExprEvalOrderOperandCount::Eight:
		return TEXT("EIGHT");
	default:
		return nullptr;
	}
}

const TCHAR* OutcomeToken(const EExprEvalOrderOutcome Outcome)
{
	switch (Outcome)
	{
	case EExprEvalOrderOutcome::Complete:
		return TEXT("COMPLETE");
	case EExprEvalOrderOutcome::ExceptionFirst:
		return TEXT("EXCEPTION_FIRST");
	case EExprEvalOrderOutcome::ExceptionMiddle:
		return TEXT("EXCEPTION_MIDDLE");
	case EExprEvalOrderOutcome::ExceptionLast:
		return TEXT("EXCEPTION_LAST");
	default:
		return nullptr;
	}
}

const TCHAR* SourceShapeToken(const EExprEvalOrderSourceShape SourceShape)
{
	switch (SourceShape)
	{
	case EExprEvalOrderSourceShape::SingleLine:
		return TEXT("SINGLE_LINE");
	case EExprEvalOrderSourceShape::Whitespace:
		return TEXT("WHITESPACE");
	case EExprEvalOrderSourceShape::Comments:
		return TEXT("COMMENTS");
	case EExprEvalOrderSourceShape::Multiline:
		return TEXT("MULTILINE");
	case EExprEvalOrderSourceShape::NestedParentheses:
		return TEXT("NESTED_PARENTHESES");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FExprEvalOrderParams& Params)
{
	return CompositionToken(Params.Composition) != nullptr
		&& OperandCountToken(Params.OperandCount) != nullptr
		&& OutcomeToken(Params.Outcome) != nullptr
		&& SourceShapeToken(Params.SourceShape) != nullptr;
}

int32 OperandCountValue(const EExprEvalOrderOperandCount OperandCount)
{
	switch (OperandCount)
	{
	case EExprEvalOrderOperandCount::Two:
		return 2;
	case EExprEvalOrderOperandCount::Three:
		return 3;
	case EExprEvalOrderOperandCount::Eight:
		return 8;
	default:
		return 0;
	}
}

bool IsFault(const EExprEvalOrderOutcome Outcome)
{
	return Outcome != EExprEvalOrderOutcome::Complete;
}

bool UsesReverseEagerOperandOrder(const EExprEvalOrderComposition Composition)
{
	return Composition == EExprEvalOrderComposition::CallArguments
		|| Composition == EExprEvalOrderComposition::ConstructorArguments
		|| Composition == EExprEvalOrderComposition::IndexArguments
		|| Composition == EExprEvalOrderComposition::CallChain
		|| Composition == EExprEvalOrderComposition::MemberIndexChain;
}

int32 EagerStageAtExecutionPosition(
	const EExprEvalOrderComposition Composition,
	const int32 Count,
	const int32 Position)
{
	return UsesReverseEagerOperandOrder(Composition) ? Count - Position : Position + 1;
}

int32 EagerExceptionStage(const FExprEvalOrderParams& Params)
{
	const int32 Count = OperandCountValue(Params.OperandCount);
	switch (Params.Outcome)
	{
	case EExprEvalOrderOutcome::ExceptionFirst:
		return EagerStageAtExecutionPosition(Params.Composition, Count, 0);
	case EExprEvalOrderOutcome::ExceptionMiddle:
		return Count == 2
			? INDEX_NONE
			: EagerStageAtExecutionPosition(Params.Composition, Count, (Count - 1) / 2);
	case EExprEvalOrderOutcome::ExceptionLast:
		return EagerStageAtExecutionPosition(Params.Composition, Count, Count - 1);
	case EExprEvalOrderOutcome::Complete:
	default:
		return INDEX_NONE;
	}
}

bool UsesTwoStageCompletionException(const FExprEvalOrderParams& Params)
{
	return OperandCountValue(Params.OperandCount) == 2
		&& Params.Outcome == EExprEvalOrderOutcome::ExceptionMiddle;
}

int32 ExpectedFromCell(const FEvalOrderCell& Cell)
{
	if (IsFault(Cell.Outcome))
	{
		return 0;
	}
	const int32 Count = OperandCountValue(Cell.OperandCount);
	const int32 Sum = Count * (Count + 1) / 2;
	return Cell.Composition == EExprEvalOrderComposition::CompoundAssignment ? 10 + Sum : Sum;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FEvalOrderCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-EXPR-EVAL-ORDER-%s-%s-%s-%s"),
		CompositionToken(Cell.Composition),
		OperandCountToken(Cell.OperandCount),
		OutcomeToken(Cell.Outcome),
		SourceShapeToken(Cell.SourceShape));
}

FString MakeEntryName(const FEvalOrderCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FExprEvalOrderParams ParamsFromCell(const FEvalOrderCell& Cell)
{
	FExprEvalOrderParams Params;
	Params.Composition = Cell.Composition;
	Params.OperandCount = Cell.OperandCount;
	Params.Outcome = Cell.Outcome;
	Params.SourceShape = Cell.SourceShape;
	return Params;
}

FString MakeEagerStageExpression(const int32 Stage, const int32 ExceptionStage, const EExprEvalOrderSourceShape SourceShape)
{
	FString Expression = FString::Printf(
		TEXT("RecordEagerStage(%d, %d, %s)"),
		Stage,
		Stage,
		Stage == ExceptionStage ? TEXT("true") : TEXT("false"));
	if (SourceShape == EExprEvalOrderSourceShape::Comments)
	{
		Expression = FString::Printf(TEXT("/* stage_%d */ %s"), Stage, *Expression);
	}
	else if (SourceShape == EExprEvalOrderSourceShape::NestedParentheses)
	{
		Expression = FString::Printf(TEXT("(((%s)))"), *Expression);
	}
	return Expression;
}

FString JoinEagerTerms(
	const TArray<FString>& Terms,
	const EExprEvalOrderSourceShape SourceShape,
	const TCHAR* OperatorToken,
	const bool bArgumentList)
{
	FString Separator;
	if (SourceShape == EExprEvalOrderSourceShape::Whitespace)
	{
		Separator = bArgumentList ? TEXT("  ,\t ") : FString::Printf(TEXT("  %s\t "), OperatorToken);
	}
	else if (SourceShape == EExprEvalOrderSourceShape::Comments)
	{
		Separator = bArgumentList
			? TEXT(" /* argument boundary */ , /* next argument */ ")
			: FString::Printf(TEXT(" /* operator boundary */ %s /* next operand */ "), OperatorToken);
	}
	else if (SourceShape == EExprEvalOrderSourceShape::Multiline)
	{
		Separator = bArgumentList
			? FString(TEXT(",\n\t\t"))
			: FString::Printf(TEXT(" %s\n\t\t"), OperatorToken);
	}
	else
	{
		Separator = bArgumentList ? TEXT(", ") : FString::Printf(TEXT(" %s "), OperatorToken);
	}

	FString Result = FString::Join(Terms, *Separator);
	if (SourceShape == EExprEvalOrderSourceShape::NestedParentheses && !bArgumentList)
	{
		Result = FString::Printf(TEXT("(((%s)))"), *Result);
	}
	return Result;
}

TArray<FString> MakeEagerStageExpressions(const FExprEvalOrderParams& Params)
{
	const int32 Count = OperandCountValue(Params.OperandCount);
	const int32 ExceptionStage = EagerExceptionStage(Params);
	TArray<FString> Terms;
	Terms.Reserve(Count);
	for (int32 Stage = 1; Stage <= Count; ++Stage)
	{
		Terms.Add(MakeEagerStageExpression(Stage, ExceptionStage, Params.SourceShape));
	}
	return Terms;
}

FString MakeParameterDeclarations(const int32 Count)
{
	TArray<FString> Parameters;
	Parameters.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Parameters.Add(FString::Printf(TEXT("int Value%d"), Index));
	}
	return FString::Join(Parameters, TEXT(", "));
}

FString MakeParameterSum(const int32 Count)
{
	TArray<FString> Values;
	Values.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Values.Add(FString::Printf(TEXT("Value%d"), Index));
	}
	return FString::Join(Values, TEXT(" + "));
}

void AppendCountHelpers(FString& Source, const int32 Count)
{
	const FString Parameters = MakeParameterDeclarations(Count);
	const FString Sum = MakeParameterSum(Count);
	AppendLine(Source, FString::Printf(TEXT("int CollectEager(%s)"), *Parameters));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *Sum));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("struct FEagerConstructed%d"), Count));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("\tFEagerConstructed%d(%s)"), Count, *Parameters));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, FString::Printf(TEXT("\t\tValue = %s;"), *Sum));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("struct FEagerIndexer%d"), Count));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\tint opIndex(%s) const"), *Parameters));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, FString::Printf(TEXT("\t\treturn %s;"), *Sum));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendHelpers(FString& Source)
{
	AppendLine(Source, TEXT("int RecordEagerStage(int Stage, int Value, bool bThrow)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tif (bThrow)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tint Zero = 0;"));
	AppendLine(Source, TEXT("\t\treturn 1 / Zero;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int CompleteEagerBoundary(int Value, bool bThrow)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tif (bThrow)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tint Zero = 0;"));
	AppendLine(Source, TEXT("\t\treturn 1 / Zero;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FEagerChain"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFEagerChain()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFEagerChain(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFEagerChain Step(int InValue) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn FEagerChain(Value + InValue);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFEagerChain opIndex(int InValue) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn FEagerChain(Value + InValue);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint Read() const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn Value;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendCountHelpers(Source, 2);
	AppendCountHelpers(Source, 3);
	AppendCountHelpers(Source, 8);
}

FString WrapEagerCall(
	const FString& Prefix,
	const FString& Arguments,
	const FString& Suffix,
	const EExprEvalOrderSourceShape SourceShape)
{
	if (SourceShape == EExprEvalOrderSourceShape::Multiline)
	{
		return Prefix + TEXT("(\n\t\t") + Arguments + TEXT("\n\t)") + Suffix;
	}
	return Prefix + TEXT("(") + Arguments + TEXT(")") + Suffix;
}

FString MakeEagerCallChain(
	const TArray<FString>& Terms,
	const EExprEvalOrderSourceShape SourceShape,
	const bool bMemberIndex)
{
	FString Expression = FString::Printf(TEXT("FEagerChain(%s)"), *Terms[0]);
	for (int32 Index = 1; Index < Terms.Num(); ++Index)
	{
		FString Segment = FString::Printf(TEXT(".Step(%s)"), *Terms[Index]);
		if (bMemberIndex)
		{
			Segment += TEXT("[0]");
		}
		if (SourceShape == EExprEvalOrderSourceShape::Comments)
		{
			Segment = FString::Printf(TEXT(" /* chain_%d */ %s"), Index + 1, *Segment);
		}
		else if (SourceShape == EExprEvalOrderSourceShape::Whitespace)
		{
			Segment = TEXT("  ") + Segment;
		}
		else if (SourceShape == EExprEvalOrderSourceShape::Multiline)
		{
			Segment = TEXT("\n\t\t") + Segment;
		}
		Expression += Segment;
		if (SourceShape == EExprEvalOrderSourceShape::NestedParentheses)
		{
			Expression = FString::Printf(TEXT("(%s)"), *Expression);
		}
	}
	Expression += bMemberIndex ? TEXT(".Value") : TEXT(".Read()");
	return Expression;
}

FString MakeNestedCastExpression(
	const FString& SumExpression,
	const int32 CastDepth,
	const EExprEvalOrderSourceShape SourceShape)
{
	FString Expression = SumExpression;
	for (int32 Depth = 0; Depth < CastDepth; ++Depth)
	{
		const TCHAR* const TypeName =
			Depth % 3 == 0 ? TEXT("double") : (Depth % 3 == 1 ? TEXT("float") : TEXT("int"));
		if (SourceShape == EExprEvalOrderSourceShape::Multiline)
		{
			FString IndentedExpression = Expression;
			IndentedExpression.ReplaceInline(TEXT("\n"), TEXT("\n\t\t"));
			Expression = FString::Printf(TEXT("%s(\n\t\t%s\n\t)"), TypeName, *IndentedExpression);
		}
		else
		{
			Expression = FString::Printf(TEXT("%s(%s)"), TypeName, *Expression);
		}
	}
	return FString::Printf(TEXT("int(%s)"), *Expression);
}

FString MakeEagerCompositionExpression(const FExprEvalOrderParams& Params)
{
	const TArray<FString> Terms = MakeEagerStageExpressions(Params);
	const FString SumExpression = JoinEagerTerms(Terms, Params.SourceShape, TEXT("+"), false);
	const FString Arguments = JoinEagerTerms(Terms, Params.SourceShape, TEXT(","), true);
	const int32 Count = OperandCountValue(Params.OperandCount);
	FString Expression;
	if (Params.Composition == EExprEvalOrderComposition::Binary)
	{
		Expression = SumExpression;
	}
	else if (Params.Composition == EExprEvalOrderComposition::Assignment)
	{
		if (Params.SourceShape == EExprEvalOrderSourceShape::Comments)
		{
			Expression = TEXT("Target /* assignment */ = /* eager value */ ") + SumExpression;
		}
		else if (Params.SourceShape == EExprEvalOrderSourceShape::Whitespace)
		{
			Expression = TEXT("Target   =\t ") + SumExpression;
		}
		else if (Params.SourceShape == EExprEvalOrderSourceShape::Multiline)
		{
			Expression = TEXT("Target\n\t\t= ") + SumExpression;
		}
		else
		{
			Expression = TEXT("Target = ") + SumExpression;
		}
	}
	else if (Params.Composition == EExprEvalOrderComposition::CompoundAssignment)
	{
		if (Params.SourceShape == EExprEvalOrderSourceShape::Comments)
		{
			Expression = TEXT("Target /* compound */ += /* eager value */ ") + SumExpression;
		}
		else if (Params.SourceShape == EExprEvalOrderSourceShape::Whitespace)
		{
			Expression = TEXT("Target   +=\t ") + SumExpression;
		}
		else if (Params.SourceShape == EExprEvalOrderSourceShape::Multiline)
		{
			Expression = TEXT("Target\n\t\t+= ") + SumExpression;
		}
		else
		{
			Expression = TEXT("Target += ") + SumExpression;
		}
	}
	else if (Params.Composition == EExprEvalOrderComposition::CallArguments)
	{
		Expression = WrapEagerCall(TEXT("CollectEager"), Arguments, TEXT(""), Params.SourceShape);
	}
	else if (Params.Composition == EExprEvalOrderComposition::ConstructorArguments)
	{
		Expression = WrapEagerCall(
			FString::Printf(TEXT("FEagerConstructed%d"), Count),
			Arguments,
			TEXT(".Value"),
			Params.SourceShape);
	}
	else if (Params.Composition == EExprEvalOrderComposition::IndexArguments)
	{
		if (Params.SourceShape == EExprEvalOrderSourceShape::Multiline)
		{
			Expression = FString::Printf(TEXT("Indexer[\n\t\t%s\n\t]"), *Arguments);
		}
		else
		{
			Expression = FString::Printf(TEXT("Indexer[%s]"), *Arguments);
		}
	}
	else if (Params.Composition == EExprEvalOrderComposition::CallChain)
	{
		Expression = MakeEagerCallChain(Terms, Params.SourceShape, false);
	}
	else if (Params.Composition == EExprEvalOrderComposition::MemberIndexChain)
	{
		Expression = MakeEagerCallChain(Terms, Params.SourceShape, true);
	}
	else
	{
		Expression = MakeNestedCastExpression(SumExpression, Count, Params.SourceShape);
	}

	if (Params.SourceShape == EExprEvalOrderSourceShape::NestedParentheses
		&& Params.Composition != EExprEvalOrderComposition::Assignment
		&& Params.Composition != EExprEvalOrderComposition::CompoundAssignment)
	{
		Expression = FString::Printf(TEXT("(((%s)))"), *Expression);
	}
	if (UsesTwoStageCompletionException(Params))
	{
		if (Params.Composition == EExprEvalOrderComposition::Assignment
			|| Params.Composition == EExprEvalOrderComposition::CompoundAssignment)
		{
			const FString Boundary = FString::Printf(TEXT("CompleteEagerBoundary(%s, true)"), *SumExpression);
			Expression.ReplaceInline(*SumExpression, *Boundary, ESearchCase::CaseSensitive);
		}
		else
		{
			Expression = FString::Printf(TEXT("CompleteEagerBoundary(%s, true)"), *Expression);
		}
	}
	return Expression;
}

void AppendEntry(FString& Source, const FExprEvalOrderParams& Params, const FString& FunctionName)
{
	const int32 Count = OperandCountValue(Params.OperandCount);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	bool bEmittedLocal = false;
	if (Params.Composition == EExprEvalOrderComposition::Assignment)
	{
		AppendLine(Source, TEXT("\tint Target = 0;"));
		bEmittedLocal = true;
	}
	else if (Params.Composition == EExprEvalOrderComposition::CompoundAssignment)
	{
		AppendLine(Source, TEXT("\tint Target = 10;"));
		bEmittedLocal = true;
	}
	else if (Params.Composition == EExprEvalOrderComposition::IndexArguments)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFEagerIndexer%d Indexer;"), Count));
		bEmittedLocal = true;
	}
	if (bEmittedLocal)
	{
		AppendLine(Source);
	}
	const FString CompositionExpression = MakeEagerCompositionExpression(Params);
	if (Params.Composition == EExprEvalOrderComposition::Assignment
		|| Params.Composition == EExprEvalOrderComposition::CompoundAssignment)
	{
		AppendLine(Source, TEXT("\t") + CompositionExpression + TEXT(";"));
		AppendLine(Source, TEXT("\treturn Target;"));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *CompositionExpression));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FExprEvalOrderParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendHelpers(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

TArray<FEvalOrderCell> AllCells()
{
	TArray<FEvalOrderCell> Cells;
	Cells.Reserve(540);
	for (int32 CompositionIndex = 0; CompositionIndex < 9; ++CompositionIndex)
	{
		for (int32 CountIndex = 0; CountIndex < 3; ++CountIndex)
		{
			for (int32 OutcomeIndex = 0; OutcomeIndex < 4; ++OutcomeIndex)
			{
				for (int32 ShapeIndex = 0; ShapeIndex < 5; ++ShapeIndex)
				{
					FEvalOrderCell Cell;
					Cell.Composition = static_cast<EExprEvalOrderComposition>(CompositionIndex);
					Cell.OperandCount = static_cast<EExprEvalOrderOperandCount>(CountIndex);
					Cell.Outcome = static_cast<EExprEvalOrderOutcome>(OutcomeIndex);
					Cell.SourceShape = static_cast<EExprEvalOrderSourceShape>(ShapeIndex);
					Cells.Add(Cell);
				}
			}
		}
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FEvalOrderCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FEvalOrderCell> Cells = AllCells();
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

FGeneratedCaseInfo MakeCaseInfo(const FEvalOrderCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Execution = EGeneratedCaseExecution::Standalone;
	if (IsFault(Cell.Outcome))
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.ExpectedException = TEXT("Divide by zero");
	}
	else
	{
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.ExpectedReturn = ExpectedFromCell(Cell);
	}
	return Info;
}
}

FStringView FExprEvalOrderGenerator::GetProductId() const
{
	return TEXT("LANG-EXPR-EVAL-ORDER");
}

TArray<FGeneratedCaseInfo> FExprEvalOrderGenerator::ListCases() const
{
	using namespace ExprEvalOrderGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FEvalOrderCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FString EntryName = MakeEntryName(Cells[Index]);
		checkf(!EntryNames.Contains(EntryName), TEXT("ExprEvalOrder PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MakeCaseInfo(Cells[Index]));
	}
	return Cases;
}

FString FExprEvalOrderGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ExprEvalOrderGeneratorDetail;
	FEvalOrderCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FExprEvalOrderGenerator::BuildDumpSource() const
{
	using namespace ExprEvalOrderGeneratorDetail;
	const TArray<FEvalOrderCell> Cells = AllCells();
	int32 NormalCount = 0;
	int32 FaultCount = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsFault(Cells[Index].Outcome))
		{
			++FaultCount;
		}
		else
		{
			++NormalCount;
		}
	}

	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-EXPR-EVAL-ORDER"));
	AppendLine(Source, FString::Printf(
		TEXT("// Cases: %d total, %d normal, 0 reject, %d fault"),
		Cells.Num(),
		NormalCount,
		FaultCount));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendHelpers(Source);

	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (Index > 0)
		{
			AppendLine(Source);
		}
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		if (IsFault(Cells[Index].Outcome))
		{
			AppendLine(Source, TEXT("// Outcome: RuntimeException"));
			AppendLine(Source, TEXT("// ExpectedException: Divide by zero"));
		}
		else
		{
			AppendLine(Source, TEXT("// Outcome: ReturnValue"));
			AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		}
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

FString FExprEvalOrderGenerator::BuildEagerEvaluationSource(const FExprEvalOrderParams& Params) const
{
	using namespace ExprEvalOrderGeneratorDetail;
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

FString FExprEvalOrderGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ExprEvalOrderGeneratorDetail;
	const TArray<FEvalOrderCell> Cells = AllCells();
	FString Source;
	AppendHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (Index > 0)
		{
			AppendLine(Source);
		}
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	OutCaseCount = Cells.Num();
	return Source;
}

int32 FExprEvalOrderGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ExprEvalOrderGeneratorDetail;
	FEvalOrderCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsFault(Cell.Outcome))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}
}
