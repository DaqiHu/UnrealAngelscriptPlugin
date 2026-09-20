#include "Framework/Generate/AngelscriptTestOpLogicalGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace OpLogicalGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native TraceLogicalOperand.");

struct FLogicalCell
{
	EOpLogicalContext Context = EOpLogicalContext::Assignment;
	EOpLogicalOperator Operator = EOpLogicalOperator::And;
	EOpLogicalSource Source = EOpLogicalSource::BoolLiteral;
	EOpLogicalTruth Truth = EOpLogicalTruth::FalseFalse;
};

const TCHAR* ContextToken(const EOpLogicalContext Context)
{
	switch (Context)
	{
	case EOpLogicalContext::Assignment:
		return TEXT("ASSIGNMENT");
	case EOpLogicalContext::Return:
		return TEXT("RETURN");
	case EOpLogicalContext::Condition:
		return TEXT("CONDITION");
	case EOpLogicalContext::Argument:
		return TEXT("ARGUMENT");
	default:
		return nullptr;
	}
}

const TCHAR* OperatorToken(const EOpLogicalOperator Operator)
{
	switch (Operator)
	{
	case EOpLogicalOperator::And:
		return TEXT("AND");
	case EOpLogicalOperator::Or:
		return TEXT("OR");
	case EOpLogicalOperator::Xor:
		return TEXT("XOR");
	default:
		return nullptr;
	}
}

const TCHAR* SourceToken(const EOpLogicalSource Source)
{
	switch (Source)
	{
	case EOpLogicalSource::BoolLiteral:
		return TEXT("BOOL_LITERAL");
	case EOpLogicalSource::BoolLValue:
		return TEXT("BOOL_LVALUE");
	case EOpLogicalSource::Comparison:
		return TEXT("COMPARISON");
	case EOpLogicalSource::ConversionOperator:
		return TEXT("CONVERSION_OPERATOR");
	default:
		return nullptr;
	}
}

const TCHAR* TruthToken(const EOpLogicalTruth Truth)
{
	switch (Truth)
	{
	case EOpLogicalTruth::FalseFalse:
		return TEXT("FALSE_FALSE");
	case EOpLogicalTruth::FalseTrue:
		return TEXT("FALSE_TRUE");
	case EOpLogicalTruth::TrueFalse:
		return TEXT("TRUE_FALSE");
	case EOpLogicalTruth::TrueTrue:
		return TEXT("TRUE_TRUE");
	default:
		return nullptr;
	}
}

const TCHAR* OperatorGlyph(const EOpLogicalOperator Operator)
{
	switch (Operator)
	{
	case EOpLogicalOperator::And:
		return TEXT("&&");
	case EOpLogicalOperator::Or:
		return TEXT("||");
	case EOpLogicalOperator::Xor:
		return TEXT("^^");
	default:
		return TEXT("&&");
	}
}

bool IsValidParams(const FOpLogicalParams& Params)
{
	return ContextToken(Params.Context) != nullptr
		&& OperatorToken(Params.Operator) != nullptr
		&& SourceToken(Params.Source) != nullptr
		&& TruthToken(Params.Truth) != nullptr;
}

bool LeftTruth(const EOpLogicalTruth Truth)
{
	return Truth == EOpLogicalTruth::TrueFalse || Truth == EOpLogicalTruth::TrueTrue;
}

bool RightTruth(const EOpLogicalTruth Truth)
{
	return Truth == EOpLogicalTruth::FalseTrue || Truth == EOpLogicalTruth::TrueTrue;
}

int32 ExpectedFromCell(const FLogicalCell& Cell)
{
	const bool bLeft = LeftTruth(Cell.Truth);
	const bool bRight = RightTruth(Cell.Truth);
	if (Cell.Operator == EOpLogicalOperator::And)
	{
		return (bLeft && bRight) ? 1 : 0;
	}
	if (Cell.Operator == EOpLogicalOperator::Or)
	{
		return (bLeft || bRight) ? 1 : 0;
	}
	return (bLeft != bRight) ? 1 : 0;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FLogicalCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-OP-LOGICAL-%s-%s-%s-%s"),
		ContextToken(Cell.Context),
		OperatorToken(Cell.Operator),
		SourceToken(Cell.Source),
		TruthToken(Cell.Truth));
}

FString MakeEntryName(const FLogicalCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FOpLogicalParams ParamsFromCell(const FLogicalCell& Cell)
{
	FOpLogicalParams Params;
	Params.Context = Cell.Context;
	Params.Operator = Cell.Operator;
	Params.Source = Cell.Source;
	Params.Truth = Cell.Truth;
	return Params;
}

TArray<FLogicalCell> AllCells()
{
	TArray<FLogicalCell> Cells;
	Cells.SetNum(192);
	for (int32 Index = 0; Index < 192; ++Index)
	{
		FLogicalCell& Cell = Cells[Index];
		Cell.Context = static_cast<EOpLogicalContext>(Index / 48);
		Cell.Operator = static_cast<EOpLogicalOperator>((Index / 16) % 3);
		Cell.Source = static_cast<EOpLogicalSource>((Index / 4) % 4);
		Cell.Truth = static_cast<EOpLogicalTruth>(Index % 4);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FLogicalCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FLogicalCell> Cells = AllCells();
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
	AppendLine(Source, TEXT("struct FLogicalConversionValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tbool Value;"));
	AppendLine(Source, TEXT("\tint Marker;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFLogicalConversionValue(bool InValue, int InMarker)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t\tMarker = InMarker;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tbool opImplConv() const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn TraceLogicalOperand(Marker, Value);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("bool ConsumeLogical(bool Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
}

FString LogicalExpression(const FOpLogicalParams& Params)
{
	const TCHAR* const Glyph = OperatorGlyph(Params.Operator);
	const TCHAR* const LeftLiteral = LeftTruth(Params.Truth) ? TEXT("true") : TEXT("false");
	const TCHAR* const RightLiteral = RightTruth(Params.Truth) ? TEXT("true") : TEXT("false");
	if (Params.Source == EOpLogicalSource::BoolLiteral)
	{
		return FString::Printf(
			TEXT("TraceLogicalOperand(1, %s) %s TraceLogicalOperand(2, %s)"),
			LeftLiteral,
			Glyph,
			RightLiteral);
	}
	if (Params.Source == EOpLogicalSource::BoolLValue)
	{
		return FString::Printf(TEXT("TraceLogicalOperand(1, LeftValue) %s TraceLogicalOperand(2, RightValue)"), Glyph);
	}
	if (Params.Source == EOpLogicalSource::Comparison)
	{
		return FString::Printf(
			TEXT("TraceLogicalOperand(1, LeftNumber == 1) %s TraceLogicalOperand(2, RightNumber == 1)"),
			Glyph);
	}
	return FString::Printf(TEXT("LeftValue.opImplConv() %s RightValue.opImplConv()"), Glyph);
}

void AppendEntry(FString& Source, const FOpLogicalParams& Params, const FString& FunctionName)
{
	const TCHAR* const LeftLiteral = LeftTruth(Params.Truth) ? TEXT("true") : TEXT("false");
	const TCHAR* const RightLiteral = RightTruth(Params.Truth) ? TEXT("true") : TEXT("false");
	const FString Expression = LogicalExpression(Params);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Source == EOpLogicalSource::BoolLValue)
	{
		AppendLine(Source, FString::Printf(TEXT("\tbool LeftValue = %s;"), LeftLiteral));
		AppendLine(Source, FString::Printf(TEXT("\tbool RightValue = %s;"), RightLiteral));
	}
	else if (Params.Source == EOpLogicalSource::Comparison)
	{
		AppendLine(Source, FString::Printf(TEXT("\tint LeftNumber = %d;"), LeftTruth(Params.Truth) ? 1 : 0));
		AppendLine(Source, FString::Printf(TEXT("\tint RightNumber = %d;"), RightTruth(Params.Truth) ? 1 : 0));
	}
	else if (Params.Source == EOpLogicalSource::ConversionOperator)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFLogicalConversionValue LeftValue(%s, 1);"), LeftLiteral));
		AppendLine(Source, FString::Printf(TEXT("\tFLogicalConversionValue RightValue(%s, 2);"), RightLiteral));
	}
	if (Params.Context == EOpLogicalContext::Assignment)
	{
		AppendLine(Source, FString::Printf(TEXT("\tbool Result = %s;"), *Expression));
		AppendLine(Source, TEXT("\treturn Result ? 1 : 0;"));
	}
	else if (Params.Context == EOpLogicalContext::Return)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn (%s) ? 1 : 0;"), *Expression));
	}
	else if (Params.Context == EOpLogicalContext::Condition)
	{
		AppendLine(Source, FString::Printf(TEXT("\tif (%s)"), *Expression));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 1;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn 0;"));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn ConsumeLogical(%s) ? 1 : 0;"), *Expression));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FOpLogicalParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FLogicalCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	Info.ExecutionNotes = HostNotes;
	return Info;
}
}

FStringView FOpLogicalGenerator::GetProductId() const
{
	return TEXT("LANG-OP-LOGICAL");
}

TArray<FGeneratedCaseInfo> FOpLogicalGenerator::ListCases() const
{
	using namespace OpLogicalGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FLogicalCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		const FString EntryName = MakeEntryName(Cells[Index]);
		checkf(!EntryNames.Contains(EntryName), TEXT("OpLogical PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FOpLogicalGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace OpLogicalGeneratorDetail;
	FLogicalCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FOpLogicalGenerator::BuildDumpSource() const
{
	using namespace OpLogicalGeneratorDetail;
	const TArray<FLogicalCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-OP-LOGICAL"));
	AppendLine(Source, TEXT("// Cases: 192 total, 192 normal, 0 reject, 0 fault"));
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
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

FString FOpLogicalGenerator::BuildLogicalSource(const FOpLogicalParams& Params) const
{
	using namespace OpLogicalGeneratorDetail;
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

FString FOpLogicalGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace OpLogicalGeneratorDetail;
	const TArray<FLogicalCell> Cells = AllCells();
	FString Source;
	AppendSharedHelpers(Source);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	OutCaseCount = Cells.Num();
	return Source;
}

int32 FOpLogicalGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace OpLogicalGeneratorDetail;
	FLogicalCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}
}
