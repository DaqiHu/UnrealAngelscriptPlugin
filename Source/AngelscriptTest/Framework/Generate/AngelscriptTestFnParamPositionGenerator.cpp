#include "Framework/Generate/AngelscriptTestFnParamPositionGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace FnParamPositionGeneratorDetail
{
struct FPositionCell
{
	EFnParamPositionDirection Direction = EFnParamPositionDirection::Value;
	EFnParamPositionPosition Position = EFnParamPositionPosition::First;
	EFnParamPositionType Type = EFnParamPositionType::Int8;
};

struct FTypeInfo
{
	const TCHAR* Token = nullptr;
	const TCHAR* ScriptType = nullptr;
	const TCHAR* OneLiteral = nullptr;
	const TCHAR* ZeroLiteral = nullptr;
	const TCHAR* NearLiteral = nullptr;
	bool bObject = false;
	bool bBool = false;
	bool bHost = false;
	const TCHAR* HostNotes = nullptr;
};

const TCHAR* DirectionToken(const EFnParamPositionDirection Direction)
{
	switch (Direction)
	{
	case EFnParamPositionDirection::Value:
		return TEXT("VALUE");
	case EFnParamPositionDirection::In:
		return TEXT("IN");
	case EFnParamPositionDirection::Out:
		return TEXT("OUT");
	case EFnParamPositionDirection::Inout:
		return TEXT("INOUT");
	default:
		return nullptr;
	}
}

const TCHAR* PositionToken(const EFnParamPositionPosition Position)
{
	switch (Position)
	{
	case EFnParamPositionPosition::First:
		return TEXT("FIRST");
	case EFnParamPositionPosition::Middle:
		return TEXT("MIDDLE");
	case EFnParamPositionPosition::Last:
		return TEXT("LAST");
	default:
		return nullptr;
	}
}

const TCHAR* DirectionSuffix(const EFnParamPositionDirection Direction)
{
	switch (Direction)
	{
	case EFnParamPositionDirection::In:
		return TEXT("& in");
	case EFnParamPositionDirection::Out:
		return TEXT("& out");
	case EFnParamPositionDirection::Inout:
		return TEXT("& inout");
	default:
		return TEXT("");
	}
}

bool DirectionWrites(const EFnParamPositionDirection Direction)
{
	return Direction == EFnParamPositionDirection::Out || Direction == EFnParamPositionDirection::Inout;
}

bool DirectionReads(const EFnParamPositionDirection Direction)
{
	return Direction != EFnParamPositionDirection::Out;
}

int32 PositionIndex(const EFnParamPositionPosition Position)
{
	return static_cast<int32>(Position);
}

const FTypeInfo* TypeInfo(const EFnParamPositionType Type)
{
	static const FTypeInfo Infos[] = {
		{TEXT("INT8"), TEXT("int8"), TEXT("1"), TEXT("0"), TEXT("126"), false, false, false, nullptr},
		{TEXT("INT16"), TEXT("int16"), TEXT("1"), TEXT("0"), TEXT("32766"), false, false, false, nullptr},
		{TEXT("INT"), TEXT("int"), TEXT("1"), TEXT("0"), TEXT("2147483646"), false, false, false, nullptr},
		{TEXT("INT64"), TEXT("int64"), TEXT("1"), TEXT("0"), TEXT("9223372036854775806"), false, false, false, nullptr},
		{TEXT("UINT8"), TEXT("uint8"), TEXT("1"), TEXT("0"), TEXT("254"), false, false, false, nullptr},
		{TEXT("UINT16"), TEXT("uint16"), TEXT("1"), TEXT("0"), TEXT("65534"), false, false, false, nullptr},
		{TEXT("UINT"), TEXT("uint"), TEXT("1"), TEXT("0"), TEXT("4294967294"), false, false, false, nullptr},
		{TEXT("UINT64"), TEXT("uint64"), TEXT("1"), TEXT("0"), TEXT("18446744073709551614"), false, false, false, nullptr},
		{TEXT("FLOAT32"), TEXT("float32"), TEXT("1.0f"), TEXT("0.0f"), TEXT("3.402823263e+38f"), false, false, false, nullptr},
		{TEXT("FLOAT64"), TEXT("float64"), TEXT("1.0"), TEXT("0.0"), TEXT("1.7976931348623155e+308"), false, false, false, nullptr},
		{TEXT("BOOL"), TEXT("bool"), TEXT("true"), TEXT("false"), TEXT("true"), false, true, false, nullptr},
		{TEXT("ENUM"), TEXT("ENativeCaseEnum"), TEXT("ENativeCaseEnum::One"), TEXT("ENativeCaseEnum::Zero"), TEXT("ENativeCaseEnum::NearMaximum"), false, false, false, nullptr},
		{TEXT("TYPEDEF"), TEXT("NativeCaseAlias"), TEXT("NativeCaseAlias(1)"), TEXT("NativeCaseAlias(0)"), TEXT("NativeCaseAlias(2147483646)"), false, false, true, TEXT("Needs host RegisterTypedef NativeCaseAlias.")},
		{TEXT("SCRIPT_VALUE"), TEXT("FScriptCaseValue"), TEXT("1"), TEXT("0"), TEXT("1"), true, false, false, nullptr},
		{TEXT("NATIVE_VALUE"), TEXT("FNativeCaseValue"), TEXT("1"), TEXT("0"), TEXT("1"), true, false, true, TEXT("Needs native FNativeCaseValue.")},
	};
	const int32 Index = static_cast<int32>(Type);
	if (Index < 0 || Index >= 15)
	{
		return nullptr;
	}
	return &Infos[Index];
}

bool IsValidParams(const FFnParamPositionParams& Params)
{
	return DirectionToken(Params.Direction) != nullptr
		&& PositionToken(Params.Position) != nullptr
		&& TypeInfo(Params.Type) != nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FPositionCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-FN-PARAM-POSITION-%s-%s-%s"),
		DirectionToken(Cell.Direction),
		PositionToken(Cell.Position),
		TypeInfo(Cell.Type)->Token);
}

FString MakeEntryName(const FPositionCell& Cell)
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

FFnParamPositionParams ParamsFromCell(const FPositionCell& Cell)
{
	FFnParamPositionParams Params;
	Params.Direction = Cell.Direction;
	Params.Position = Cell.Position;
	Params.Type = Cell.Type;
	return Params;
}

TArray<FPositionCell> AllCells()
{
	TArray<FPositionCell> Cells;
	Cells.SetNum(180);
	for (int32 Index = 0; Index < 180; ++Index)
	{
		FPositionCell& Cell = Cells[Index];
		Cell.Direction = static_cast<EFnParamPositionDirection>(Index / 45);
		Cell.Position = static_cast<EFnParamPositionPosition>((Index / 15) % 3);
		Cell.Type = static_cast<EFnParamPositionType>(Index % 15);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FPositionCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FPositionCell& Cell : AllCells())
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

FString ParameterDeclaration(const FFnParamPositionParams& Params)
{
	const FTypeInfo* const Info = TypeInfo(Params.Type);
	const int32 Slot = PositionIndex(Params.Position);
	const FString Target = FString::Printf(TEXT("%s%s Target"), Info->ScriptType, DirectionSuffix(Params.Direction));
	const TCHAR* const Slots[] = {TEXT("int FirstSentinel"), TEXT("int MiddleSentinel"), TEXT("int LastSentinel")};
	FString Result;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (Index > 0)
		{
			Result += TEXT(", ");
		}
		Result += Index == Slot ? Target : FString(Slots[Index]);
	}
	return Result;
}

FString CallArguments(const EFnParamPositionPosition Position)
{
	const int32 Slot = PositionIndex(Position);
	const TCHAR* const Args[] = {TEXT("101"), TEXT("202"), TEXT("303")};
	FString Result;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (Index > 0)
		{
			Result += TEXT(", ");
		}
		Result += Index == Slot ? TEXT("Target") : Args[Index];
	}
	return Result;
}

FString SentinelExpression(const EFnParamPositionPosition Position)
{
	const int32 Slot = PositionIndex(Position);
	FString Result;
	if (Slot != 0)
	{
		Result = TEXT("FirstSentinel == 101");
	}
	if (Slot != 1)
	{
		if (!Result.IsEmpty())
		{
			Result += TEXT(" && ");
		}
		Result += TEXT("MiddleSentinel == 202");
	}
	if (Slot != 2)
	{
		if (!Result.IsEmpty())
		{
			Result += TEXT(" && ");
		}
		Result += TEXT("LastSentinel == 303");
	}
	return Result;
}

const TCHAR* WriteLiteral(const FTypeInfo& Info, const EFnParamPositionDirection Direction)
{
	if (Info.bObject)
	{
		return TEXT("1");
	}
	if (Direction == EFnParamPositionDirection::Inout && Info.bBool)
	{
		return Info.ZeroLiteral;
	}
	return Info.NearLiteral;
}

FString ReadExpression(const FTypeInfo& Info, const TCHAR* Name, const TCHAR* Literal)
{
	if (Info.bObject)
	{
		return FString::Printf(TEXT("%s.Value == %s"), Name, Literal);
	}
	return FString::Printf(TEXT("%s == %s"), Name, Literal);
}

void AppendEntry(FString& Source, const FFnParamPositionParams& Params, const FString& FunctionName)
{
	const FTypeInfo* const Info = TypeInfo(Params.Type);
	const FString Probe = ProbeName(FunctionName);
	const bool bWrites = DirectionWrites(Params.Direction);
	const bool bReads = DirectionReads(Params.Direction);
	const TCHAR* const Written = WriteLiteral(*Info, Params.Direction);
	// Legacy oracle always constructs object targets with the one-literal, including OUT.
	const TCHAR* const Initial = Info->bObject
		? TEXT("1")
		: (bReads ? Info->OneLiteral : Info->ZeroLiteral);
	const TCHAR* const After = (bWrites || Info->bObject) ? Written : Info->OneLiteral;

	AppendLine(Source, FString::Printf(TEXT("bool %s(%s)"), *Probe, *ParameterDeclaration(Params)));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\tbool SentinelsMatch = %s;"), *SentinelExpression(Params.Position)));
	if (bReads)
	{
		AppendLine(Source, FString::Printf(TEXT("\tbool InputMatches = %s;"), *ReadExpression(*Info, TEXT("Target"), Info->bObject ? TEXT("1") : Info->OneLiteral)));
	}
	if (bWrites)
	{
		if (Info->bObject)
		{
			AppendLine(Source, FString::Printf(TEXT("\tTarget.Value = %s;"), Written));
		}
		else
		{
			AppendLine(Source, FString::Printf(TEXT("\tTarget = %s;"), Written));
		}
	}
	AppendLine(Source, FString::Printf(TEXT("\treturn SentinelsMatch%s;"), bReads ? TEXT(" && InputMatches") : TEXT("")));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Info->bObject)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Target(%s);"), Info->ScriptType, Initial));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Target = %s;"), Info->ScriptType, Initial));
	}
	AppendLine(Source, FString::Printf(TEXT("\tbool Result = %s(%s);"), *Probe, *CallArguments(Params.Position)));
	AppendLine(Source, FString::Printf(TEXT("\treturn Result && (%s) ? 1 : 0;"), *ReadExpression(*Info, TEXT("Target"), After)));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FFnParamPositionParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FPositionCell& Cell)
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

FStringView FFnParamPositionGenerator::GetProductId() const
{
	return TEXT("LANG-FN-PARAM-POSITION");
}

TArray<FGeneratedCaseInfo> FFnParamPositionGenerator::ListCases() const
{
	using namespace FnParamPositionGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FPositionCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("FnParamPosition PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FFnParamPositionGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace FnParamPositionGeneratorDetail;
	FPositionCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FFnParamPositionGenerator::BuildDumpSource() const
{
	using namespace FnParamPositionGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FN-PARAM-POSITION"));
	AppendLine(Source, TEXT("// Cases: 180 total, 180 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	for (const FPositionCell& Cell : AllCells())
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

FString FFnParamPositionGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace FnParamPositionGeneratorDetail;
	const TArray<FPositionCell> Cells = AllCells();
	OutCaseCount = Cells.Num();
	FString Source;
	AppendSharedHelpers(Source);
	for (const FPositionCell& Cell : Cells)
	{
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FFnParamPositionGenerator::BuildParameterPositionSource(const FFnParamPositionParams& Params) const
{
	using namespace FnParamPositionGeneratorDetail;
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

int32 FFnParamPositionGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace FnParamPositionGeneratorDetail;
	FPositionCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return 1;
}
}
