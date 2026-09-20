#include "Framework/Generate/AngelscriptTestConditionGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ConditionGeneratorDetail
{
struct FConditionCell
{
	EConditionStatement Statement = EConditionStatement::If;
	EConditionExpr Condition = EConditionExpr::BoolLiteral;
	EConditionTruth Truth = EConditionTruth::False;
};

const TCHAR* StatementToken(const EConditionStatement Statement)
{
	switch (Statement)
	{
	case EConditionStatement::If:
		return TEXT("IF");
	case EConditionStatement::While:
		return TEXT("WHILE");
	case EConditionStatement::DoWhile:
		return TEXT("DO_WHILE");
	case EConditionStatement::For:
		return TEXT("FOR");
	default:
		return nullptr;
	}
}

const TCHAR* ConditionToken(const EConditionExpr Condition)
{
	switch (Condition)
	{
	case EConditionExpr::BoolLiteral:
		return TEXT("BOOL_LITERAL");
	case EConditionExpr::Variable:
		return TEXT("VARIABLE");
	case EConditionExpr::Comparison:
		return TEXT("COMPARISON");
	case EConditionExpr::Logical:
		return TEXT("LOGICAL");
	case EConditionExpr::SideEffectCall:
		return TEXT("SIDE_EFFECT_CALL");
	case EConditionExpr::OverloadedConversion:
		return TEXT("OVERLOADED_CONVERSION");
	case EConditionExpr::InvalidType:
		return TEXT("INVALID_TYPE");
	default:
		return nullptr;
	}
}

const TCHAR* TruthToken(const EConditionTruth Truth)
{
	switch (Truth)
	{
	case EConditionTruth::False:
		return TEXT("FALSE");
	case EConditionTruth::True:
		return TEXT("TRUE");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FConditionParams& Params)
{
	return StatementToken(Params.Statement) != nullptr
		&& ConditionToken(Params.Condition) != nullptr
		&& TruthToken(Params.Truth) != nullptr;
}

bool IsReject(const EConditionExpr Condition)
{
	return Condition == EConditionExpr::InvalidType;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FConditionCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CF-CONDITION-%s-%s-%s"),
		StatementToken(Cell.Statement),
		ConditionToken(Cell.Condition),
		TruthToken(Cell.Truth));
}

FString MakeEntryName(const FConditionCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FConditionParams ParamsFromCell(const FConditionCell& Cell)
{
	FConditionParams Params;
	Params.Statement = Cell.Statement;
	Params.Condition = Cell.Condition;
	Params.Truth = Cell.Truth;
	return Params;
}

FString ConditionExpression(const EConditionExpr Condition, const bool bTruth)
{
	switch (Condition)
	{
	case EConditionExpr::BoolLiteral:
		return bTruth ? TEXT("true") : TEXT("false");
	case EConditionExpr::Variable:
		return TEXT("ConditionValue");
	case EConditionExpr::Comparison:
		return bTruth ? TEXT("2 > 1") : TEXT("2 < 1");
	case EConditionExpr::Logical:
		return bTruth ? TEXT("ConditionValue && true") : TEXT("ConditionValue && false");
	case EConditionExpr::SideEffectCall:
		return TEXT("EvaluateCondition(ConditionValue, EvaluationCount)");
	case EConditionExpr::OverloadedConversion:
		return TEXT("FConditionValue(ConditionValue)");
	case EConditionExpr::InvalidType:
	default:
		return TEXT("FInvalidCondition()");
	}
}

int32 ExpectedFromCell(const FConditionCell& Cell)
{
	if (IsReject(Cell.Condition))
	{
		return 0;
	}
	const bool bTruth = Cell.Truth == EConditionTruth::True;
	const int32 Trace = (Cell.Statement == EConditionStatement::DoWhile || bTruth) ? 1 : 0;
	if (Cell.Condition != EConditionExpr::SideEffectCall)
	{
		return Trace * 10;
	}
	const bool bRepeated = Cell.Statement == EConditionStatement::While
		|| Cell.Statement == EConditionStatement::For;
	const int32 EvaluationCount = (bTruth && bRepeated) ? 2 : 1;
	return Trace * 10 + EvaluationCount;
}

void AppendHelpers(FString& Source)
{
	AppendLine(Source, TEXT("struct FConditionValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tbool Value;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFConditionValue(bool InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tbool opImplConv() const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn Value;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("class FInvalidCondition"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("bool EvaluateCondition(bool InValue, int&inout EvaluationCount)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\t++EvaluationCount;"));
	AppendLine(Source, TEXT("\treturn InValue;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendEntry(FString& Source, const FConditionParams& Params, const FString& FunctionName)
{
	const bool bTruth = Params.Truth == EConditionTruth::True;
	const FString Condition = ConditionExpression(Params.Condition, bTruth);

	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, bTruth ? TEXT("\tbool ConditionValue = true;") : TEXT("\tbool ConditionValue = false;"));
	AppendLine(Source, TEXT("\tint Trace = 0;"));
	AppendLine(Source, TEXT("\tint Guard = 0;"));
	AppendLine(Source, TEXT("\tint EvaluationCount = 0;"));

	if (Params.Statement == EConditionStatement::If)
	{
		AppendLine(Source, FString::Printf(TEXT("\tif (%s)"), *Condition));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tTrace = 1;"));
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Params.Statement == EConditionStatement::While)
	{
		AppendLine(Source, FString::Printf(TEXT("\twhile ((%s) && Guard < 1)"), *Condition));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\t++Trace;"));
		AppendLine(Source, TEXT("\t\t++Guard;"));
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Params.Statement == EConditionStatement::DoWhile)
	{
		AppendLine(Source, TEXT("\tdo"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\t++Trace;"));
		AppendLine(Source, TEXT("\t\t++Guard;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, FString::Printf(TEXT("\twhile ((%s) && Guard < 1);"), *Condition));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\tfor (; (%s) && Guard < 1; ++Guard)"), *Condition));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\t++Trace;"));
		AppendLine(Source, TEXT("\t}"));
	}

	AppendLine(Source, TEXT("\treturn Trace * 10 + EvaluationCount;"));
	AppendLine(Source, TEXT("}"));
}

TArray<FConditionCell> AllCells()
{
	TArray<FConditionCell> Cells;
	Cells.SetNum(56);
	for (int32 Index = 0; Index < 56; ++Index)
	{
		FConditionCell& Cell = Cells[Index];
		Cell.Statement = static_cast<EConditionStatement>(Index / 14);
		Cell.Condition = static_cast<EConditionExpr>((Index / 2) % 7);
		Cell.Truth = static_cast<EConditionTruth>(Index % 2);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FConditionCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FConditionCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FGeneratedCaseInfo MakeCaseInfo(const FConditionCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Condition))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
	}
	else
	{
		Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.Execution = EGeneratedCaseExecution::Standalone;
		Info.ExpectedReturn = ExpectedFromCell(Cell);
	}
	return Info;
}

FString BuildOwnedSource(const FConditionParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendHelpers(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}
}

FStringView FConditionGenerator::GetProductId() const
{
	return TEXT("LANG-CF-CONDITION");
}

TArray<FGeneratedCaseInfo> FConditionGenerator::ListCases() const
{
	using namespace ConditionGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FConditionCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("Condition PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FConditionGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ConditionGeneratorDetail;
	FConditionCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FConditionGenerator::BuildDumpSource() const
{
	using namespace ConditionGeneratorDetail;
	const TArray<FConditionCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CF-CONDITION"));
	AppendLine(Source, TEXT("// Cases: 56 total, 48 normal, 8 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendHelpers(Source);

	bool bNeedBlank = false;
	for (const FConditionCell& Cell : Cells)
	{
		if (IsReject(Cell.Condition))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
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
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}

	for (const FConditionCell& Cell : Cells)
	{
		if (!IsReject(Cell.Condition))
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
		AppendHelpers(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FConditionGenerator::BuildConditionSource(const FConditionParams& Params) const
{
	using namespace ConditionGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Condition))
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

FString FConditionGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ConditionGeneratorDetail;
	const TArray<FConditionCell> Cells = AllCells();
	FString Source;
	AppendHelpers(Source);
	int32 Emitted = 0;
	for (const FConditionCell& Cell : Cells)
	{
		if (IsReject(Cell.Condition))
		{
			continue;
		}
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

int32 FConditionGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ConditionGeneratorDetail;
	FConditionCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Condition))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}

FString FConditionGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace ConditionGeneratorDetail;
	FConditionCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Condition))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FConditionGenerator::ListRejectCaseIds() const
{
	using namespace ConditionGeneratorDetail;
	TArray<FString> Ids;
	for (const FConditionCell& Cell : AllCells())
	{
		if (IsReject(Cell.Condition))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
