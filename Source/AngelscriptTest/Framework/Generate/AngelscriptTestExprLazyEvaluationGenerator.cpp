#include "Framework/Generate/AngelscriptTestExprLazyEvaluationGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ExprLazyEvaluationGeneratorDetail
{
struct FLazyCell
{
	EExprLazyForm Form = EExprLazyForm::LogicalAnd;
	EExprLazyOperandOutcome OperandOutcome = EExprLazyOperandOutcome::Value;
	EExprLazySelector Selector = EExprLazySelector::False;
	EExprLazySourceShape SourceShape = EExprLazySourceShape::SingleLine;
};

const TCHAR* FormToken(const EExprLazyForm Form)
{
	switch (Form)
	{
	case EExprLazyForm::LogicalAnd:
		return TEXT("LOGICAL_AND");
	case EExprLazyForm::LogicalOr:
		return TEXT("LOGICAL_OR");
	case EExprLazyForm::Conditional:
		return TEXT("CONDITIONAL");
	default:
		return nullptr;
	}
}

const TCHAR* OperandOutcomeToken(const EExprLazyOperandOutcome OperandOutcome)
{
	switch (OperandOutcome)
	{
	case EExprLazyOperandOutcome::Value:
		return TEXT("VALUE");
	case EExprLazyOperandOutcome::SideEffect:
		return TEXT("SIDE_EFFECT");
	case EExprLazyOperandOutcome::Exception:
		return TEXT("EXCEPTION");
	default:
		return nullptr;
	}
}

const TCHAR* SelectorToken(const EExprLazySelector Selector)
{
	switch (Selector)
	{
	case EExprLazySelector::False:
		return TEXT("FALSE");
	case EExprLazySelector::True:
		return TEXT("TRUE");
	default:
		return nullptr;
	}
}

const TCHAR* SourceShapeToken(const EExprLazySourceShape SourceShape)
{
	switch (SourceShape)
	{
	case EExprLazySourceShape::SingleLine:
		return TEXT("SINGLE_LINE");
	case EExprLazySourceShape::Comments:
		return TEXT("COMMENTS");
	case EExprLazySourceShape::Multiline:
		return TEXT("MULTILINE");
	case EExprLazySourceShape::Parenthesized:
		return TEXT("PARENTHESIZED");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FExprLazyEvaluationParams& Params)
{
	return FormToken(Params.Form) != nullptr
		&& OperandOutcomeToken(Params.OperandOutcome) != nullptr
		&& SelectorToken(Params.Selector) != nullptr
		&& SourceShapeToken(Params.SourceShape) != nullptr;
}

bool SelectorValue(const EExprLazySelector Selector)
{
	return Selector == EExprLazySelector::True;
}

bool IsGuardedOperandSelected(const EExprLazyForm Form, const EExprLazySelector Selector)
{
	if (Form == EExprLazyForm::LogicalAnd)
	{
		return SelectorValue(Selector);
	}
	if (Form == EExprLazyForm::LogicalOr)
	{
		return !SelectorValue(Selector);
	}
	return SelectorValue(Selector);
}

bool IsFault(const FLazyCell& Cell)
{
	return Cell.OperandOutcome == EExprLazyOperandOutcome::Exception
		&& IsGuardedOperandSelected(Cell.Form, Cell.Selector);
}

int32 ExpectedFromCell(const FLazyCell& Cell)
{
	if (IsFault(Cell))
	{
		return 0;
	}
	if (Cell.Form == EExprLazyForm::LogicalAnd)
	{
		return SelectorValue(Cell.Selector) ? 1 : 0;
	}
	if (Cell.Form == EExprLazyForm::LogicalOr)
	{
		return 1;
	}
	return SelectorValue(Cell.Selector) ? 41 : 23;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FLazyCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-EXPR-LAZY-EVALUATION-%s-%s-%s-%s"),
		FormToken(Cell.Form),
		OperandOutcomeToken(Cell.OperandOutcome),
		SelectorToken(Cell.Selector),
		SourceShapeToken(Cell.SourceShape));
}

FString MakeEntryName(const FLazyCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FExprLazyEvaluationParams ParamsFromCell(const FLazyCell& Cell)
{
	FExprLazyEvaluationParams Params;
	Params.Form = Cell.Form;
	Params.OperandOutcome = Cell.OperandOutcome;
	Params.Selector = Cell.Selector;
	Params.SourceShape = Cell.SourceShape;
	return Params;
}

FString MakeGuardedExpression(const EExprLazyForm Form, const EExprLazyOperandOutcome OperandOutcome)
{
	const bool bLogical = Form == EExprLazyForm::LogicalAnd || Form == EExprLazyForm::LogicalOr;
	if (OperandOutcome == EExprLazyOperandOutcome::Exception)
	{
		return bLogical ? TEXT("RaiseGuardedBool()") : TEXT("RaiseGuardedInt()");
	}
	if (OperandOutcome == EExprLazyOperandOutcome::SideEffect)
	{
		return bLogical ? TEXT("RecordExpressionBool(2, true)") : TEXT("RecordExpressionInt(2, 41)");
	}
	return bLogical ? TEXT("true") : TEXT("41");
}

FString MakeSelectorExpression(const EExprLazySelector Selector)
{
	return FString::Printf(TEXT("RecordExpressionBool(1, %s)"), SelectorValue(Selector) ? TEXT("true") : TEXT("false"));
}

FString MakeSingleLineExpression(const FExprLazyEvaluationParams& Params)
{
	const FString Selector = MakeSelectorExpression(Params.Selector);
	const FString Guarded = MakeGuardedExpression(Params.Form, Params.OperandOutcome);
	if (Params.SourceShape == EExprLazySourceShape::Comments)
	{
		if (Params.Form == EExprLazyForm::LogicalAnd)
		{
			return FString::Printf(TEXT("%s /* selector */ && /* guarded */ %s ? 1 : 0"), *Selector, *Guarded);
		}
		if (Params.Form == EExprLazyForm::LogicalOr)
		{
			return FString::Printf(TEXT("%s /* selector */ || /* guarded */ %s ? 1 : 0"), *Selector, *Guarded);
		}
		return FString::Printf(
			TEXT("%s /* selector */ ? /* guarded */ %s : /* fallback */ RecordExpressionInt(3, 23)"),
			*Selector,
			*Guarded);
	}
	if (Params.SourceShape == EExprLazySourceShape::Parenthesized)
	{
		if (Params.Form == EExprLazyForm::LogicalAnd)
		{
			return FString::Printf(TEXT("((%s) && (%s)) ? 1 : 0"), *Selector, *Guarded);
		}
		if (Params.Form == EExprLazyForm::LogicalOr)
		{
			return FString::Printf(TEXT("((%s) || (%s)) ? 1 : 0"), *Selector, *Guarded);
		}
		return FString::Printf(TEXT("(%s) ? (%s) : (RecordExpressionInt(3, 23))"), *Selector, *Guarded);
	}
	if (Params.Form == EExprLazyForm::LogicalAnd)
	{
		return FString::Printf(TEXT("%s && %s ? 1 : 0"), *Selector, *Guarded);
	}
	if (Params.Form == EExprLazyForm::LogicalOr)
	{
		return FString::Printf(TEXT("%s || %s ? 1 : 0"), *Selector, *Guarded);
	}
	return FString::Printf(TEXT("%s ? %s : RecordExpressionInt(3, 23)"), *Selector, *Guarded);
}

void AppendHelpers(FString& Source)
{
	AppendLine(Source, TEXT("bool RecordExpressionBool(int Marker, bool Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int RecordExpressionInt(int Marker, int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("bool RaiseGuardedBool()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tRecordExpressionBool(2, true);"));
	AppendLine(Source, TEXT("\tint Zero = 0;"));
	AppendLine(Source, TEXT("\treturn 1 / Zero > 0;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int RaiseGuardedInt()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tRecordExpressionInt(2, 41);"));
	AppendLine(Source, TEXT("\tint Zero = 0;"));
	AppendLine(Source, TEXT("\treturn 1 / Zero;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendMultilineReturn(FString& Source, const FExprLazyEvaluationParams& Params)
{
	const FString Selector = MakeSelectorExpression(Params.Selector);
	const FString Guarded = MakeGuardedExpression(Params.Form, Params.OperandOutcome);
	AppendLine(Source, TEXT("\treturn ("));
	if (Params.Form == EExprLazyForm::LogicalAnd)
	{
		AppendLine(Source, FString::Printf(TEXT("\t\t%s"), *Selector));
		AppendLine(Source, TEXT("\t\t&&"));
		AppendLine(Source, FString::Printf(TEXT("\t\t%s"), *Guarded));
		AppendLine(Source, TEXT("\t) ? 1 : 0;"));
	}
	else if (Params.Form == EExprLazyForm::LogicalOr)
	{
		AppendLine(Source, FString::Printf(TEXT("\t\t%s"), *Selector));
		AppendLine(Source, TEXT("\t\t||"));
		AppendLine(Source, FString::Printf(TEXT("\t\t%s"), *Guarded));
		AppendLine(Source, TEXT("\t) ? 1 : 0;"));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\t\t%s"), *Selector));
		AppendLine(Source, TEXT("\t\t?"));
		AppendLine(Source, FString::Printf(TEXT("\t\t%s"), *Guarded));
		AppendLine(Source, TEXT("\t\t:"));
		AppendLine(Source, TEXT("\t\tRecordExpressionInt(3, 23)"));
		AppendLine(Source, TEXT("\t);"));
	}
}

void AppendEntry(FString& Source, const FExprLazyEvaluationParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.SourceShape == EExprLazySourceShape::Multiline)
	{
		AppendMultilineReturn(Source, Params);
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *MakeSingleLineExpression(Params)));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FExprLazyEvaluationParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendHelpers(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

TArray<FLazyCell> AllCells()
{
	TArray<FLazyCell> Cells;
	Cells.Reserve(72);
	for (int32 FormIndex = 0; FormIndex < 3; ++FormIndex)
	{
		for (int32 OutcomeIndex = 0; OutcomeIndex < 3; ++OutcomeIndex)
		{
			for (int32 SelectorIndex = 0; SelectorIndex < 2; ++SelectorIndex)
			{
				for (int32 ShapeIndex = 0; ShapeIndex < 4; ++ShapeIndex)
				{
					FLazyCell Cell;
					Cell.Form = static_cast<EExprLazyForm>(FormIndex);
					Cell.OperandOutcome = static_cast<EExprLazyOperandOutcome>(OutcomeIndex);
					Cell.Selector = static_cast<EExprLazySelector>(SelectorIndex);
					Cell.SourceShape = static_cast<EExprLazySourceShape>(ShapeIndex);
					Cells.Add(Cell);
				}
			}
		}
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FLazyCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FLazyCell> Cells = AllCells();
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

FGeneratedCaseInfo MakeCaseInfo(const FLazyCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Execution = EGeneratedCaseExecution::Standalone;
	if (IsFault(Cell))
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

FStringView FExprLazyEvaluationGenerator::GetProductId() const
{
	return TEXT("LANG-EXPR-LAZY-EVALUATION");
}

TArray<FGeneratedCaseInfo> FExprLazyEvaluationGenerator::ListCases() const
{
	using namespace ExprLazyEvaluationGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FLazyCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FString EntryName = MakeEntryName(Cells[Index]);
		checkf(!EntryNames.Contains(EntryName), TEXT("ExprLazyEvaluation PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MakeCaseInfo(Cells[Index]));
	}
	return Cases;
}

FString FExprLazyEvaluationGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ExprLazyEvaluationGeneratorDetail;
	FLazyCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FExprLazyEvaluationGenerator::BuildDumpSource() const
{
	using namespace ExprLazyEvaluationGeneratorDetail;
	const TArray<FLazyCell> Cells = AllCells();
	int32 NormalCount = 0;
	int32 FaultCount = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsFault(Cells[Index]))
		{
			++FaultCount;
		}
		else
		{
			++NormalCount;
		}
	}

	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-EXPR-LAZY-EVALUATION"));
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
		if (IsFault(Cells[Index]))
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

FString FExprLazyEvaluationGenerator::BuildLazyEvaluationSource(const FExprLazyEvaluationParams& Params) const
{
	using namespace ExprLazyEvaluationGeneratorDetail;
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

FString FExprLazyEvaluationGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ExprLazyEvaluationGeneratorDetail;
	const TArray<FLazyCell> Cells = AllCells();
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

int32 FExprLazyEvaluationGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ExprLazyEvaluationGeneratorDetail;
	FLazyCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsFault(Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}
}
