#include "Framework/Generate/AngelscriptTestOpResultContextGenerator.h"

#include "Containers/Map.h"
#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace OpResultContextGeneratorDetail
{
struct FContextCell
{
	EOpResultContextFamily Family = EOpResultContextFamily::Unary;
	EOpResultContextContext Context = EOpResultContextContext::Assignment;
	EOpResultContextOutcome Outcome = EOpResultContextOutcome::Exact;
};

const TCHAR* FamilyToken(const EOpResultContextFamily Family)
{
	switch (Family)
	{
	case EOpResultContextFamily::Unary:
		return TEXT("UNARY");
	case EOpResultContextFamily::Arithmetic:
		return TEXT("ARITHMETIC");
	case EOpResultContextFamily::Power:
		return TEXT("POWER");
	case EOpResultContextFamily::Bitwise:
		return TEXT("BITWISE");
	case EOpResultContextFamily::Shift:
		return TEXT("SHIFT");
	case EOpResultContextFamily::Comparison:
		return TEXT("COMPARISON");
	case EOpResultContextFamily::Logical:
		return TEXT("LOGICAL");
	case EOpResultContextFamily::Assignment:
		return TEXT("ASSIGNMENT");
	case EOpResultContextFamily::Increment:
		return TEXT("INCREMENT");
	case EOpResultContextFamily::Overloaded:
		return TEXT("OVERLOADED");
	default:
		return nullptr;
	}
}

const TCHAR* ContextToken(const EOpResultContextContext Context)
{
	switch (Context)
	{
	case EOpResultContextContext::Assignment:
		return TEXT("ASSIGNMENT");
	case EOpResultContextContext::Return:
		return TEXT("RETURN");
	case EOpResultContextContext::Condition:
		return TEXT("CONDITION");
	case EOpResultContextContext::OverloadArgument:
		return TEXT("OVERLOAD_ARGUMENT");
	case EOpResultContextContext::Chain:
		return TEXT("CHAIN");
	case EOpResultContextContext::SwitchOrIndex:
		return TEXT("SWITCH_OR_INDEX");
	default:
		return nullptr;
	}
}

const TCHAR* OutcomeToken(const EOpResultContextOutcome Outcome)
{
	switch (Outcome)
	{
	case EOpResultContextOutcome::Exact:
		return TEXT("EXACT");
	case EOpResultContextOutcome::Converted:
		return TEXT("CONVERTED");
	case EOpResultContextOutcome::Ambiguous:
		return TEXT("AMBIGUOUS");
	case EOpResultContextOutcome::Rejected:
		return TEXT("REJECTED");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FOpResultContextParams& Params)
{
	return FamilyToken(Params.Family) != nullptr
		&& ContextToken(Params.Context) != nullptr
		&& OutcomeToken(Params.Outcome) != nullptr;
}

bool IsRejectCell(const FContextCell& Cell)
{
	return Cell.Outcome == EOpResultContextOutcome::Rejected;
}

bool IsAmbiguousCell(const FContextCell& Cell)
{
	return Cell.Outcome == EOpResultContextOutcome::Ambiguous;
}

int32 FamilyResult(const EOpResultContextFamily Family)
{
	switch (Family)
	{
	case EOpResultContextFamily::Unary:
		return -5;
	case EOpResultContextFamily::Bitwise:
		return 2;
	case EOpResultContextFamily::Shift:
	case EOpResultContextFamily::Power:
		return 8;
	default:
		return 5;
	}
}

int32 ExpectedEvaluations(const FContextCell& Cell)
{
	if (Cell.Context == EOpResultContextContext::Return)
	{
		return 0;
	}
	if (Cell.Context == EOpResultContextContext::Assignment || Cell.Family == EOpResultContextFamily::Assignment)
	{
		switch (Cell.Family)
		{
		case EOpResultContextFamily::Unary:
		case EOpResultContextFamily::Assignment:
			return 1;
		case EOpResultContextFamily::Increment:
			return 0;
		default:
			return 2;
		}
	}
	return 0;
}

int32 ExpectedFromCell(const FContextCell& Cell)
{
	if (IsRejectCell(Cell) || IsAmbiguousCell(Cell))
	{
		return 0;
	}
	return 1;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FContextCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-RESULT-CONTEXT-%s-%s-%s"),
		FamilyToken(Cell.Family),
		ContextToken(Cell.Context),
		OutcomeToken(Cell.Outcome));
}

FString MakeEntryName(const FContextCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpResultContextParams ParamsFromCell(const FContextCell& Cell)
{
	FOpResultContextParams Params;
	Params.Family = Cell.Family;
	Params.Context = Cell.Context;
	Params.Outcome = Cell.Outcome;
	return Params;
}

FContextCell CellFromParams(const FOpResultContextParams& Params)
{
	FContextCell Cell;
	Cell.Family = Params.Family;
	Cell.Context = Params.Context;
	Cell.Outcome = Params.Outcome;
	return Cell;
}

TArray<FContextCell> AllCells()
{
	TArray<FContextCell> Cells;
	Cells.SetNum(240);
	for (int32 Index = 0; Index < 240; ++Index)
	{
		FContextCell& Cell = Cells[Index];
		Cell.Family = static_cast<EOpResultContextFamily>(Index / 24);
		Cell.Context = static_cast<EOpResultContextContext>((Index / 4) % 6);
		Cell.Outcome = static_cast<EOpResultContextOutcome>(Index % 4);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FContextCell& OutCell)
{
	static const TArray<FContextCell> Cells = AllCells();
	static TMap<FString, int32> IndexById;
	if (IndexById.Num() == 0)
	{
		for (int32 Index = 0; Index < Cells.Num(); ++Index)
		{
			IndexById.Add(MakeCaseId(Cells[Index]), Index);
		}
	}
	if (const int32* const Found = IndexById.Find(FString(CaseId)))
	{
		OutCell = Cells[*Found];
		return true;
	}
	return false;
}

void AppendHelpers(FString& Source)
{
	AppendLine(Source, TEXT("struct FContextValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFContextValue(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint opAdd(const FContextValue& Other) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn Value + Other.Value;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int TraceInt(int&inout EvaluationCount, int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tEvaluationCount += 1;"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("float TraceFloat(int&inout EvaluationCount, float Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tEvaluationCount += 1;"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("bool TraceBool(int&inout EvaluationCount, bool Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tEvaluationCount += 1;"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FContextIndexProbe"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint opIndex(int Value) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn Value;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int VerifyExact(int Value, int EvaluationCount, int ExpectedValue, int ExpectedEvaluations)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value == ExpectedValue && EvaluationCount == ExpectedEvaluations ? 1 : -1;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int VerifyConverted(float Value, int EvaluationCount, int ExpectedValue, int ExpectedEvaluations)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value == ExpectedValue && EvaluationCount == ExpectedEvaluations ? 1 : -1;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ConsumeExact(int Value, int EvaluationCount, int ExpectedValue, int ExpectedEvaluations)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn VerifyExact(Value, EvaluationCount, ExpectedValue, ExpectedEvaluations);"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ConsumeConverted(float Value, int EvaluationCount, int ExpectedValue, int ExpectedEvaluations)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn VerifyConverted(Value, EvaluationCount, ExpectedValue, ExpectedEvaluations);"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ChainExact(int Value, int EvaluationCount, int ExpectedValue, int ExpectedEvaluations)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn ConsumeExact(Value, EvaluationCount, ExpectedValue, ExpectedEvaluations);"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ChainConverted(float Value, int EvaluationCount, int ExpectedValue, int ExpectedEvaluations)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn ConsumeConverted(Value, EvaluationCount, ExpectedValue, ExpectedEvaluations);"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int8 MakeAmbiguousOperand(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn int8(Value);"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int SelectAmbiguous(int64 Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int SelectAmbiguous(uint64 Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FContextNoConversion"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tbool Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int RejectReturn(FContextNoConversion Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value.Value ? 1 : 0;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("bool RejectCondition(FContextNoConversion Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value.Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int RejectArgument(FContextNoConversion Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value.Value ? 1 : 0;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ChainRejected(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
}

FString FamilyExpression(FString& Source, const EOpResultContextFamily Family)
{
	switch (Family)
	{
	case EOpResultContextFamily::Unary:
		return TEXT("-TraceInt(EvaluationCount, 5)");
	case EOpResultContextFamily::Arithmetic:
		return TEXT("TraceInt(EvaluationCount, 2) + TraceInt(EvaluationCount, 3)");
	case EOpResultContextFamily::Power:
		return TEXT("int(TraceFloat(EvaluationCount, 2.0f) ** TraceFloat(EvaluationCount, 3.0f))");
	case EOpResultContextFamily::Bitwise:
		return TEXT("TraceInt(EvaluationCount, 6) & TraceInt(EvaluationCount, 3)");
	case EOpResultContextFamily::Shift:
		return TEXT("TraceInt(EvaluationCount, 1) << TraceInt(EvaluationCount, 3)");
	case EOpResultContextFamily::Comparison:
		return TEXT("TraceInt(EvaluationCount, 3) < TraceInt(EvaluationCount, 4) ? 5 : 0");
	case EOpResultContextFamily::Logical:
		return TEXT("TraceBool(EvaluationCount, true) && TraceBool(EvaluationCount, true) ? 5 : 0");
	case EOpResultContextFamily::Assignment:
		AppendLine(Source, TEXT("\tint Assigned = 1;"));
		return TEXT("Assigned = TraceInt(EvaluationCount, 5)");
	case EOpResultContextFamily::Increment:
		AppendLine(Source, TEXT("\tint Incremented = 4;"));
		return TEXT("++Incremented");
	case EOpResultContextFamily::Overloaded:
		return TEXT("FContextValue(TraceInt(EvaluationCount, 2)) + FContextValue(TraceInt(EvaluationCount, 3))");
	default:
		return TEXT("0");
	}
}

void AppendConsumeExact(
	FString& Source,
	const FContextCell& Cell,
	const FString& Expression,
	const FString& FunctionName)
{
	const int32 Expected = FamilyResult(Cell.Family);
	const int32 Evaluations = ExpectedEvaluations(Cell);
	const bool bConverted = Cell.Outcome == EOpResultContextOutcome::Converted;
	const TCHAR* const Verify = bConverted ? TEXT("VerifyConverted") : TEXT("VerifyExact");
	const TCHAR* const Consume = bConverted ? TEXT("ConsumeConverted") : TEXT("ConsumeExact");
	const TCHAR* const Chain = bConverted ? TEXT("ChainConverted") : TEXT("ChainExact");
	if (Cell.Context == EOpResultContextContext::Assignment)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s Result = %s;"),
			bConverted ? TEXT("float") : TEXT("int"),
			*Expression));
		AppendLine(Source, FString::Printf(
			TEXT("\treturn %s(Result, EvaluationCount, %d, %d);"),
			Verify,
			Expected,
			Evaluations));
		return;
	}
	if (Cell.Context == EOpResultContextContext::Return)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\treturn %s(ReturnContextProbe_%s(EvaluationCount), EvaluationCount, %d, %d);"),
			Verify,
			*FunctionName,
			Expected,
			Evaluations));
		return;
	}
	if (Cell.Context == EOpResultContextContext::Condition)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\tif (%s(%s, EvaluationCount, %d, %d) == 1)"),
			Verify,
			*Expression,
			Expected,
			Evaluations));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 1;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn -1;"));
		return;
	}
	if (Cell.Context == EOpResultContextContext::OverloadArgument)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\treturn %s(%s, EvaluationCount, %d, %d);"),
			Consume,
			*Expression,
			Expected,
			Evaluations));
		return;
	}
	if (Cell.Context == EOpResultContextContext::Chain)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\treturn %s(%s, EvaluationCount, %d, %d);"),
			Chain,
			*Expression,
			Expected,
			Evaluations));
		return;
	}
	AppendLine(Source, TEXT("\tFContextIndexProbe Indexer;"));
	AppendLine(Source, FString::Printf(
		TEXT("\tswitch (Indexer[%s(%s, EvaluationCount, %d, %d)])"),
		Verify,
		*Expression,
		Expected,
		Evaluations));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\tcase 1:"));
	AppendLine(Source, TEXT("\t\treturn 1;"));
	AppendLine(Source, TEXT("\tdefault:"));
	AppendLine(Source, TEXT("\t\treturn -1;"));
	AppendLine(Source, TEXT("\t}"));
}

void AppendAmbiguousBody(FString& Source, const FContextCell& Cell, const FString& Expression)
{
	const FString Ambiguous = FString::Printf(TEXT("SelectAmbiguous(MakeAmbiguousOperand(%s))"), *Expression);
	if (Cell.Context == EOpResultContextContext::Assignment)
	{
		AppendLine(Source, FString::Printf(TEXT("\tint Result = %s; // OP_CONTEXT_CAUSE"), *Ambiguous));
		AppendLine(Source, TEXT("\treturn Result;"));
		return;
	}
	if (Cell.Context == EOpResultContextContext::Return)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn %s; // OP_CONTEXT_CAUSE"), *Ambiguous));
		return;
	}
	if (Cell.Context == EOpResultContextContext::Condition)
	{
		AppendLine(Source, FString::Printf(TEXT("\tif (%s != 0) // OP_CONTEXT_CAUSE"), *Ambiguous));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 1;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn -1;"));
		return;
	}
	if (Cell.Context == EOpResultContextContext::OverloadArgument)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn ConsumeExact(%s, 0, 0, 0); // OP_CONTEXT_CAUSE"), *Ambiguous));
		return;
	}
	if (Cell.Context == EOpResultContextContext::Chain)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn ChainExact(%s, 0, 0, 0); // OP_CONTEXT_CAUSE"), *Ambiguous));
		return;
	}
	AppendLine(Source, TEXT("\tFContextIndexProbe Indexer;"));
	AppendLine(Source, FString::Printf(TEXT("\tswitch (Indexer[%s]) // OP_CONTEXT_CAUSE"), *Ambiguous));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\tcase 1:"));
	AppendLine(Source, TEXT("\t\treturn 1;"));
	AppendLine(Source, TEXT("\tdefault:"));
	AppendLine(Source, TEXT("\t\treturn -1;"));
	AppendLine(Source, TEXT("\t}"));
}

void AppendRejectedBody(FString& Source, const FContextCell& Cell, const FString& Expression)
{
	if (Cell.Context == EOpResultContextContext::Assignment)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFContextNoConversion Result = %s; // OP_CONTEXT_CAUSE"), *Expression));
		AppendLine(Source, TEXT("\treturn Result.Value ? 1 : 0;"));
		return;
	}
	if (Cell.Context == EOpResultContextContext::Return)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn RejectReturn(%s); // OP_CONTEXT_CAUSE"), *Expression));
		return;
	}
	if (Cell.Context == EOpResultContextContext::Condition)
	{
		AppendLine(Source, FString::Printf(TEXT("\tif (RejectCondition(%s)) // OP_CONTEXT_CAUSE"), *Expression));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 1;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn -1;"));
		return;
	}
	if (Cell.Context == EOpResultContextContext::OverloadArgument)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn RejectArgument(%s); // OP_CONTEXT_CAUSE"), *Expression));
		return;
	}
	if (Cell.Context == EOpResultContextContext::Chain)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\treturn ChainRejected(RejectArgument(%s)); // OP_CONTEXT_CAUSE"),
			*Expression));
		return;
	}
	AppendLine(Source, TEXT("\tFContextIndexProbe Indexer;"));
	AppendLine(Source, FString::Printf(
		TEXT("\tswitch (Indexer[RejectArgument(%s)]) // OP_CONTEXT_CAUSE"),
		*Expression));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\tcase 1:"));
	AppendLine(Source, TEXT("\t\treturn 1;"));
	AppendLine(Source, TEXT("\tdefault:"));
	AppendLine(Source, TEXT("\t\treturn -1;"));
	AppendLine(Source, TEXT("\t}"));
}

void AppendEntry(FString& Source, const FOpResultContextParams& Params, const FString& FunctionName)
{
	const FContextCell Cell = CellFromParams(Params);
	const bool bUseReturnProbe = !IsRejectCell(Cell) && Cell.Context == EOpResultContextContext::Return;
	if (bUseReturnProbe)
	{
		const TCHAR* const ReturnType = Cell.Outcome == EOpResultContextOutcome::Converted ? TEXT("float") : TEXT("int");
		AppendLine(Source, FString::Printf(TEXT("%s ReturnContextProbe_%s(int&inout EvaluationCount)"), ReturnType, *FunctionName));
		AppendLine(Source, TEXT("{"));
		FString ProbeExpression = FamilyExpression(Source, Cell.Family);
		if (Cell.Family == EOpResultContextFamily::Assignment)
		{
			AppendLine(Source, FString::Printf(TEXT("\t%s;"), *ProbeExpression));
			ProbeExpression = TEXT("Assigned");
		}
		AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *ProbeExpression));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}

	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint EvaluationCount = 0;"));
	FString Expression;
	if (!bUseReturnProbe)
	{
		Expression = FamilyExpression(Source, Cell.Family);
		if (Cell.Family == EOpResultContextFamily::Assignment && Cell.Outcome != EOpResultContextOutcome::Rejected)
		{
			AppendLine(Source, FString::Printf(TEXT("\t%s;"), *Expression));
			Expression = TEXT("Assigned");
		}
	}
	if (Cell.Outcome == EOpResultContextOutcome::Ambiguous)
	{
		AppendAmbiguousBody(Source, Cell, Expression);
	}
	else if (Cell.Outcome == EOpResultContextOutcome::Rejected)
	{
		AppendRejectedBody(Source, Cell, Expression);
	}
	else
	{
		AppendConsumeExact(Source, Cell, Expression, FunctionName);
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FOpResultContextParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendHelpers(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FContextCell& Cell)
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
	if (IsAmbiguousCell(Cell))
	{
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		Info.bLimitedObservation = true;
		Info.ExecutionNotes = TEXT("Current fork resolves the tied conversion set; no exact int32 observation.");
		return Info;
	}
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	return Info;
}
}

FStringView FOpResultContextGenerator::GetProductId() const
{
	return TEXT("LANG-OP-RESULT-CONTEXT");
}

TArray<FGeneratedCaseInfo> FOpResultContextGenerator::ListCases() const
{
	using namespace OpResultContextGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FContextCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cells[Index]);
			checkf(!EntryNames.Contains(EntryName), TEXT("OpResultContext PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FOpResultContextGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace OpResultContextGeneratorDetail;
	FContextCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FOpResultContextGenerator::BuildDumpSource() const
{
	using namespace OpResultContextGeneratorDetail;
	const TArray<FContextCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-RESULT-CONTEXT"));
	AppendLine(Source, TEXT("// Cases: 240 total, 180 normal, 60 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsRejectCell(Cells[Index]))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		if (Info.ExpectedReturn.IsSet())
		{
			AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
			AppendLine(Source, TEXT("// Execution: Standalone"));
		}
		else
		{
			AppendLine(Source, TEXT("// Execution: SourceOnly"));
			AppendLine(Source, TEXT("// LimitedObservation: true"));
			AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		}
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
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
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

FString FOpResultContextGenerator::BuildSource(const FOpResultContextParams& Params) const
{
	using namespace OpResultContextGeneratorDetail;
	if (!IsValidParams(Params) || IsRejectCell(CellFromParams(Params)))
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

FString FOpResultContextGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace OpResultContextGeneratorDetail;
	const TArray<FContextCell> Cells = AllCells();
	FString Source;
	AppendHelpers(Source);
	int32 Emitted = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsRejectCell(Cells[Index]))
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

int32 FOpResultContextGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpResultContextGeneratorDetail;
	FContextCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsRejectCell(Cell) || IsAmbiguousCell(Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}

FString FOpResultContextGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace OpResultContextGeneratorDetail;
	FContextCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsRejectCell(Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FOpResultContextGenerator::ListRejectCaseIds() const
{
	using namespace OpResultContextGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FContextCell> Cells = AllCells();
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
