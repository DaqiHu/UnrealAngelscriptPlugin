#include "Framework/Generate/AngelscriptTestFnParamDirectionGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace FnParamDirectionGeneratorDetail
{
struct FDirectionCell
{
	EFnParamDirectionDirection Direction = EFnParamDirectionDirection::Value;
	EFnParamDirectionType Type = EFnParamDirectionType::Int8;
};

struct FTypeInfo
{
	const TCHAR* Token = nullptr;
	const TCHAR* ScriptType = nullptr;
	const TCHAR* OneLiteral = nullptr;
	const TCHAR* ZeroLiteral = nullptr;
	bool bObject = false;
	bool bHost = false;
	const TCHAR* HostNotes = nullptr;
};

const TCHAR* DirectionToken(const EFnParamDirectionDirection Direction)
{
	switch (Direction)
	{
	case EFnParamDirectionDirection::Value:
		return TEXT("VALUE");
	case EFnParamDirectionDirection::In:
		return TEXT("IN");
	case EFnParamDirectionDirection::Out:
		return TEXT("OUT");
	case EFnParamDirectionDirection::Inout:
		return TEXT("INOUT");
	default:
		return nullptr;
	}
}

const TCHAR* DirectionSuffix(const EFnParamDirectionDirection Direction)
{
	switch (Direction)
	{
	case EFnParamDirectionDirection::In:
		return TEXT("& in");
	case EFnParamDirectionDirection::Out:
		return TEXT("& out");
	case EFnParamDirectionDirection::Inout:
		return TEXT("& inout");
	default:
		return TEXT("");
	}
}

bool DirectionWrites(const EFnParamDirectionDirection Direction)
{
	return Direction == EFnParamDirectionDirection::Out || Direction == EFnParamDirectionDirection::Inout;
}

bool DirectionReads(const EFnParamDirectionDirection Direction)
{
	return Direction != EFnParamDirectionDirection::Out;
}

const FTypeInfo* TypeInfo(const EFnParamDirectionType Type)
{
	static const FTypeInfo Infos[] = {
		{TEXT("INT8"), TEXT("int8"), TEXT("1"), TEXT("0"), false, false, nullptr},
		{TEXT("INT16"), TEXT("int16"), TEXT("1"), TEXT("0"), false, false, nullptr},
		{TEXT("INT"), TEXT("int"), TEXT("1"), TEXT("0"), false, false, nullptr},
		{TEXT("INT64"), TEXT("int64"), TEXT("1"), TEXT("0"), false, false, nullptr},
		{TEXT("UINT8"), TEXT("uint8"), TEXT("1"), TEXT("0"), false, false, nullptr},
		{TEXT("UINT16"), TEXT("uint16"), TEXT("1"), TEXT("0"), false, false, nullptr},
		{TEXT("UINT"), TEXT("uint"), TEXT("1"), TEXT("0"), false, false, nullptr},
		{TEXT("UINT64"), TEXT("uint64"), TEXT("1"), TEXT("0"), false, false, nullptr},
		{TEXT("FLOAT32"), TEXT("float32"), TEXT("1.0f"), TEXT("0.0f"), false, false, nullptr},
		{TEXT("FLOAT64"), TEXT("float64"), TEXT("1.0"), TEXT("0.0"), false, false, nullptr},
		{TEXT("BOOL"), TEXT("bool"), TEXT("true"), TEXT("false"), false, false, nullptr},
		{TEXT("ENUM"), TEXT("ENativeCaseEnum"), TEXT("ENativeCaseEnum::One"), TEXT("ENativeCaseEnum::Zero"), false, false, nullptr},
		{TEXT("TYPEDEF"), TEXT("NativeCaseAlias"), TEXT("NativeCaseAlias(1)"), TEXT("NativeCaseAlias(0)"), false, true, TEXT("Needs host RegisterTypedef NativeCaseAlias.")},
		{TEXT("SCRIPT_VALUE"), TEXT("FScriptCaseValue"), TEXT("1"), TEXT("0"), true, false, nullptr},
		{TEXT("NATIVE_VALUE"), TEXT("FNativeCaseValue"), TEXT("1"), TEXT("0"), true, true, TEXT("Needs native FNativeCaseValue.")},
	};
	const int32 Index = static_cast<int32>(Type);
	if (Index < 0 || Index >= 15)
	{
		return nullptr;
	}
	return &Infos[Index];
}

bool IsValidParams(const FFnParamDirectionParams& Params)
{
	return DirectionToken(Params.Direction) != nullptr && TypeInfo(Params.Type) != nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FDirectionCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-FN-PARAM-DIRECTION-%s-%s"),
		DirectionToken(Cell.Direction),
		TypeInfo(Cell.Type)->Token);
}

FString MakeEntryName(const FDirectionCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FString HelperStem(const FString& FunctionName)
{
	if (FunctionName == TEXT("Entry") || FunctionName == TEXT("ProbeEntry"))
	{
		return FString();
	}
	if (FunctionName.StartsWith(TEXT("Entry")))
	{
		return FunctionName.Mid(5);
	}
	return FunctionName;
}

FString ProbeName(const FString& FunctionName)
{
	const FString Stem = HelperStem(FunctionName);
	return Stem.IsEmpty() ? FString(TEXT("Probe")) : TEXT("Probe") + Stem;
}

FFnParamDirectionParams ParamsFromCell(const FDirectionCell& Cell)
{
	FFnParamDirectionParams Params;
	Params.Direction = Cell.Direction;
	Params.Type = Cell.Type;
	return Params;
}

TArray<FDirectionCell> AllCells()
{
	TArray<FDirectionCell> Cells;
	Cells.SetNum(60);
	for (int32 Index = 0; Index < 60; ++Index)
	{
		FDirectionCell& Cell = Cells[Index];
		Cell.Direction = static_cast<EFnParamDirectionDirection>(Index / 15);
		Cell.Type = static_cast<EFnParamDirectionType>(Index % 15);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FDirectionCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FDirectionCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendSharedHelpers(FString& Source)
{
	AppendLine(Source, TEXT("enum ENativeCaseEnum"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tMinimum = -1,"));
	AppendLine(Source, TEXT("\tZero = 0,"));
	AppendLine(Source, TEXT("\tOne = 1,"));
	AppendLine(Source, TEXT("\tNearMaximum = 126,"));
	AppendLine(Source, TEXT("\tMaximum = 127"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FScriptCaseValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFScriptCaseValue()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFScriptCaseValue(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
}

FString ReadExpression(const FTypeInfo& Info, const TCHAR* Name)
{
	if (Info.bObject)
	{
		return FString::Printf(TEXT("%s.Value == %s"), Name, Info.OneLiteral);
	}
	return FString::Printf(TEXT("%s == %s"), Name, Info.OneLiteral);
}

FString AssignStatement(const FTypeInfo& Info, const TCHAR* Name)
{
	if (Info.bObject)
	{
		return FString::Printf(TEXT("%s.Value = %s;"), Name, Info.OneLiteral);
	}
	return FString::Printf(TEXT("%s = %s;"), Name, Info.OneLiteral);
}

FString InitialStatement(const FTypeInfo& Info, const bool bReads)
{
	const TCHAR* const Literal = bReads ? Info.OneLiteral : Info.ZeroLiteral;
	if (Info.bObject)
	{
		return FString::Printf(TEXT("%s Value(%s);"), Info.ScriptType, Literal);
	}
	return FString::Printf(TEXT("%s Value = %s;"), Info.ScriptType, Literal);
}

void AppendEntry(FString& Source, const FFnParamDirectionParams& Params, const FString& FunctionName)
{
	const FTypeInfo* const Info = TypeInfo(Params.Type);
	const FString Probe = ProbeName(FunctionName);
	const TCHAR* const Suffix = DirectionSuffix(Params.Direction);
	const bool bWrites = DirectionWrites(Params.Direction);
	const bool bReads = DirectionReads(Params.Direction);
	if (bWrites)
	{
		AppendLine(Source, FString::Printf(TEXT("void %s(%s%s Value)"), *Probe, Info->ScriptType, Suffix));
		AppendLine(Source, TEXT("{"));
		if (bReads)
		{
			AppendLine(Source, FString::Printf(TEXT("\tif (!(%s))"), *ReadExpression(*Info, TEXT("Value"))));
			AppendLine(Source, TEXT("\t{"));
			AppendLine(Source, TEXT("\t\tValue = Value;"));
			AppendLine(Source, TEXT("\t}"));
		}
		AppendLine(Source, TEXT("\t") + AssignStatement(*Info, TEXT("Value")));
		AppendLine(Source, TEXT("}"));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("bool %s(%s%s Value)"), *Probe, Info->ScriptType, Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *ReadExpression(*Info, TEXT("Value"))));
		AppendLine(Source, TEXT("}"));
	}
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\t") + InitialStatement(*Info, bReads));
	if (bWrites)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s(Value);"), *Probe));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s ? 1 : 0;"), *ReadExpression(*Info, TEXT("Value"))));
	}
	else
	{
		AppendLine(Source, FString::Printf(
			TEXT("\treturn %s(Value) && (%s) ? 1 : 0;"),
			*Probe,
			*ReadExpression(*Info, TEXT("Value"))));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FFnParamDirectionParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FDirectionCell& Cell)
{
	const FTypeInfo* const Info = TypeInfo(Cell.Type);
	FGeneratedCaseInfo Case;
	Case.CaseId = MakeCaseId(Cell);
	Case.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Case.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Case.Execution = Info->bHost ? EGeneratedCaseExecution::RequiresHostSetup : EGeneratedCaseExecution::Standalone;
	Case.ExpectedReturn = 1;
	if (Info->HostNotes != nullptr)
	{
		Case.ExecutionNotes = Info->HostNotes;
	}
	return Case;
}
}

FStringView FFnParamDirectionGenerator::GetProductId() const
{
	return TEXT("LANG-FN-PARAM-DIRECTION");
}

TArray<FGeneratedCaseInfo> FFnParamDirectionGenerator::ListCases() const
{
	using namespace FnParamDirectionGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FDirectionCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("FnParamDirection PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FFnParamDirectionGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace FnParamDirectionGeneratorDetail;
	FDirectionCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FFnParamDirectionGenerator::BuildDumpSource() const
{
	using namespace FnParamDirectionGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FN-PARAM-DIRECTION"));
	AppendLine(Source, TEXT("// Cases: 60 total, 60 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	for (const FDirectionCell& Cell : AllCells())
	{
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, TEXT("// ExpectedReturn: 1"));
		AppendLine(Source, Info.Execution == EGeneratedCaseExecution::RequiresHostSetup
			? TEXT("// Execution: RequiresHostSetup")
			: TEXT("// Execution: Standalone"));
		if (!Info.ExecutionNotes.IsEmpty())
		{
			AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), *Info.ExecutionNotes));
		}
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FFnParamDirectionGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace FnParamDirectionGeneratorDetail;
	const TArray<FDirectionCell> Cells = AllCells();
	OutCaseCount = Cells.Num();
	FString Source;
	AppendSharedHelpers(Source);
	for (const FDirectionCell& Cell : Cells)
	{
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FFnParamDirectionGenerator::BuildParameterDirectionSource(const FFnParamDirectionParams& Params) const
{
	using namespace FnParamDirectionGeneratorDetail;
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

int32 FFnParamDirectionGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace FnParamDirectionGeneratorDetail;
	FDirectionCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return 1;
}
}
