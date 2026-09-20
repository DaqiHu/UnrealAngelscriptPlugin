#include "Framework/Generate/AngelscriptTestConvBoolContextGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ConvBoolContextGeneratorDetail
{
struct FConvBoolContextCell
{
	EConvBoolContextSource Source = EConvBoolContextSource::Int8;
	EConvBoolContextContext Context = EConvBoolContextContext::If;
	EConvBoolContextValue Value = EConvBoolContextValue::Zero;
};

const TCHAR* SourceToken(const EConvBoolContextSource Source)
{
	switch (Source)
	{
	case EConvBoolContextSource::Int8:
		return TEXT("INT8");
	case EConvBoolContextSource::Int16:
		return TEXT("INT16");
	case EConvBoolContextSource::Int:
		return TEXT("INT");
	case EConvBoolContextSource::Int64:
		return TEXT("INT64");
	case EConvBoolContextSource::Uint8:
		return TEXT("UINT8");
	case EConvBoolContextSource::Uint16:
		return TEXT("UINT16");
	case EConvBoolContextSource::Uint:
		return TEXT("UINT");
	case EConvBoolContextSource::Uint64:
		return TEXT("UINT64");
	case EConvBoolContextSource::Float32:
		return TEXT("FLOAT32");
	case EConvBoolContextSource::Float64:
		return TEXT("FLOAT64");
	case EConvBoolContextSource::Bool:
		return TEXT("BOOL");
	case EConvBoolContextSource::Enum:
		return TEXT("ENUM");
	case EConvBoolContextSource::Typedef:
		return TEXT("TYPEDEF");
	default:
		return nullptr;
	}
}

const TCHAR* ContextToken(const EConvBoolContextContext Context)
{
	switch (Context)
	{
	case EConvBoolContextContext::If:
		return TEXT("IF");
	case EConvBoolContextContext::While:
		return TEXT("WHILE");
	case EConvBoolContextContext::TernaryCondition:
		return TEXT("TERNARY_CONDITION");
	case EConvBoolContextContext::LogicalAnd:
		return TEXT("LOGICAL_AND");
	case EConvBoolContextContext::LogicalOr:
		return TEXT("LOGICAL_OR");
	case EConvBoolContextContext::LogicalXor:
		return TEXT("LOGICAL_XOR");
	default:
		return nullptr;
	}
}

const TCHAR* ValueToken(const EConvBoolContextValue Value)
{
	switch (Value)
	{
	case EConvBoolContextValue::Zero:
		return TEXT("ZERO");
	case EConvBoolContextValue::One:
		return TEXT("ONE");
	case EConvBoolContextValue::Negative:
		return TEXT("NEGATIVE");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FConvBoolContextParams& Params)
{
	return SourceToken(Params.Source) != nullptr
		&& ContextToken(Params.Context) != nullptr
		&& ValueToken(Params.Value) != nullptr;
}

bool IsReject(const EConvBoolContextSource Source)
{
	return Source != EConvBoolContextSource::Bool;
}

bool BranchValue(const EConvBoolContextValue Value)
{
	return Value != EConvBoolContextValue::Zero;
}

const TCHAR* SourceScriptType(const EConvBoolContextSource Source)
{
	switch (Source)
	{
	case EConvBoolContextSource::Int8:
		return TEXT("int8");
	case EConvBoolContextSource::Int16:
		return TEXT("int16");
	case EConvBoolContextSource::Int:
		return TEXT("int");
	case EConvBoolContextSource::Int64:
		return TEXT("int64");
	case EConvBoolContextSource::Uint8:
		return TEXT("uint8");
	case EConvBoolContextSource::Uint16:
		return TEXT("uint16");
	case EConvBoolContextSource::Uint:
		return TEXT("uint");
	case EConvBoolContextSource::Uint64:
		return TEXT("uint64");
	case EConvBoolContextSource::Float32:
		return TEXT("float32");
	case EConvBoolContextSource::Float64:
		return TEXT("float");
	case EConvBoolContextSource::Bool:
		return TEXT("bool");
	case EConvBoolContextSource::Enum:
		return TEXT("EBoolConversion");
	case EConvBoolContextSource::Typedef:
		return TEXT("BoolAlias");
	default:
		return TEXT("int");
	}
}

const TCHAR* SourceLiteral(const EConvBoolContextSource Source, const EConvBoolContextValue Value)
{
	if (Source == EConvBoolContextSource::Bool)
	{
		return BranchValue(Value) ? TEXT("true") : TEXT("false");
	}
	if (Source == EConvBoolContextSource::Enum)
	{
		if (Value == EConvBoolContextValue::Zero)
		{
			return TEXT("EBoolConversion::Zero");
		}
		return Value == EConvBoolContextValue::One ? TEXT("EBoolConversion::One") : TEXT("EBoolConversion::Negative");
	}
	if (Source == EConvBoolContextSource::Typedef)
	{
		if (Value == EConvBoolContextValue::Zero)
		{
			return TEXT("BoolAlias(0)");
		}
		return Value == EConvBoolContextValue::One ? TEXT("BoolAlias(1)") : TEXT("BoolAlias(-1)");
	}
	if (Value == EConvBoolContextValue::Zero)
	{
		return TEXT("0");
	}
	return Value == EConvBoolContextValue::One ? TEXT("1") : TEXT("-1");
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FConvBoolContextCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CONV-BOOL-CONTEXT-%s-%s-%s"),
		SourceToken(Cell.Source),
		ContextToken(Cell.Context),
		ValueToken(Cell.Value));
}

FString MakeEntryName(const FConvBoolContextCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FConvBoolContextParams ParamsFromCell(const FConvBoolContextCell& Cell)
{
	FConvBoolContextParams Params;
	Params.Source = Cell.Source;
	Params.Context = Cell.Context;
	Params.Value = Cell.Value;
	return Params;
}

TArray<FConvBoolContextCell> AllCells()
{
	TArray<FConvBoolContextCell> Cells;
	Cells.SetNum(234);
	for (int32 Index = 0; Index < 234; ++Index)
	{
		FConvBoolContextCell& Cell = Cells[Index];
		Cell.Source = static_cast<EConvBoolContextSource>(Index / 18);
		Cell.Context = static_cast<EConvBoolContextContext>((Index / 3) % 6);
		Cell.Value = static_cast<EConvBoolContextValue>(Index % 3);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FConvBoolContextCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FConvBoolContextCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendSourceHelpers(FString& Source, const EConvBoolContextSource SourceKind)
{
	if (SourceKind == EConvBoolContextSource::Enum)
	{
		AppendLine(Source, TEXT("enum EBoolConversion"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tNegative = -1,"));
		AppendLine(Source, TEXT("\tZero = 0,"));
		AppendLine(Source, TEXT("\tOne = 1"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (SourceKind == EConvBoolContextSource::Typedef)
	{
		AppendLine(Source, TEXT("typedef int BoolAlias;"));
		AppendLine(Source);
	}
}

void AppendEntry(FString& Source, const FConvBoolContextParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(
		TEXT("\t%s SourceValue = %s;"),
		SourceScriptType(Params.Source),
		SourceLiteral(Params.Source, Params.Value)));
	if (Params.Context == EConvBoolContextContext::If)
	{
		AppendLine(Source, TEXT("\tif (SourceValue)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 1;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn 0;"));
	}
	else if (Params.Context == EConvBoolContextContext::While)
	{
		AppendLine(Source, TEXT("\twhile (SourceValue)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 1;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn 0;"));
	}
	else if (Params.Context == EConvBoolContextContext::TernaryCondition)
	{
		AppendLine(Source, TEXT("\treturn SourceValue ? 1 : 0;"));
	}
	else if (Params.Context == EConvBoolContextContext::LogicalAnd)
	{
		AppendLine(Source, TEXT("\treturn SourceValue && true ? 1 : 0;"));
	}
	else if (Params.Context == EConvBoolContextContext::LogicalOr)
	{
		AppendLine(Source, TEXT("\treturn SourceValue || false ? 1 : 0;"));
	}
	else
	{
		AppendLine(Source, TEXT("\treturn SourceValue ^^ false ? 1 : 0;"));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FConvBoolContextParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSourceHelpers(Source, Params.Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

int32 ExpectedFromCell(const FConvBoolContextCell& Cell)
{
	if (IsReject(Cell.Source))
	{
		return 0;
	}
	return BranchValue(Cell.Value) ? 1 : 0;
}

FGeneratedCaseInfo MakeCaseInfo(const FConvBoolContextCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Source))
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

FStringView FConvBoolContextGenerator::GetProductId() const
{
	return TEXT("LANG-CONV-BOOL-CONTEXT");
}

TArray<FGeneratedCaseInfo> FConvBoolContextGenerator::ListCases() const
{
	using namespace ConvBoolContextGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FConvBoolContextCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("ConvBoolContext PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FConvBoolContextGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ConvBoolContextGeneratorDetail;
	FConvBoolContextCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FConvBoolContextGenerator::BuildDumpSource() const
{
	using namespace ConvBoolContextGeneratorDetail;
	const TArray<FConvBoolContextCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CONV-BOOL-CONTEXT"));
	AppendLine(Source, TEXT("// Cases: 234 total, 18 normal, 216 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	bool bNeedBlank = false;
	for (const FConvBoolContextCell& Cell : Cells)
	{
		if (IsReject(Cell.Source))
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
	for (const FConvBoolContextCell& Cell : Cells)
	{
		if (!IsReject(Cell.Source))
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
		AppendSourceHelpers(Source, Cell.Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FConvBoolContextGenerator::BuildBoolConversionSource(const FConvBoolContextParams& Params) const
{
	using namespace ConvBoolContextGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Source))
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

FString FConvBoolContextGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ConvBoolContextGeneratorDetail;
	const TArray<FConvBoolContextCell> Cells = AllCells();
	FString Source;
	int32 Emitted = 0;
	for (const FConvBoolContextCell& Cell : Cells)
	{
		if (IsReject(Cell.Source))
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

int32 FConvBoolContextGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ConvBoolContextGeneratorDetail;
	FConvBoolContextCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Source))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}

FString FConvBoolContextGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace ConvBoolContextGeneratorDetail;
	FConvBoolContextCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Source))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FConvBoolContextGenerator::ListRejectCaseIds() const
{
	using namespace ConvBoolContextGeneratorDetail;
	TArray<FString> Ids;
	for (const FConvBoolContextCell& Cell : AllCells())
	{
		if (IsReject(Cell.Source))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
