#include "Framework/Generate/AngelscriptTestFnReturnGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace FnReturnGeneratorDetail
{
struct FReturnCell
{
	EFnReturnPath Path = EFnReturnPath::Direct;
	EFnReturnType Type = EFnReturnType::Void;
};

struct FTypeInfo
{
	const TCHAR* Token = nullptr;
	const TCHAR* ScriptType = nullptr;
	const TCHAR* ValueExpression = nullptr;
	const TCHAR* IncompatibleExpression = nullptr;
	int32 ExpectedInteger = 0;
	bool bVoid = false;
	bool bFloat = false;
	bool bBool = false;
	bool bObject = false;
	bool bReference = false;
	bool bHost = false;
	const TCHAR* HostNotes = nullptr;
};

const TCHAR* PathToken(const EFnReturnPath Path)
{
	switch (Path)
	{
	case EFnReturnPath::Direct:
		return TEXT("DIRECT");
	case EFnReturnPath::IfElse:
		return TEXT("IF_ELSE");
	case EFnReturnPath::Switch:
		return TEXT("SWITCH");
	case EFnReturnPath::Early:
		return TEXT("EARLY");
	case EFnReturnPath::RecursiveBase:
		return TEXT("RECURSIVE_BASE");
	case EFnReturnPath::Exception:
		return TEXT("EXCEPTION");
	case EFnReturnPath::MissingInvalid:
		return TEXT("MISSING_INVALID");
	case EFnReturnPath::IncompatibleInvalid:
		return TEXT("INCOMPATIBLE_INVALID");
	default:
		return nullptr;
	}
}

const FTypeInfo* TypeInfo(const EFnReturnType Type)
{
	static const FTypeInfo Infos[] = {
		{TEXT("VOID"), TEXT("void"), TEXT(""), TEXT("1"), 0, true, false, false, false, false, false, nullptr},
		{TEXT("INT8"), TEXT("int8"), TEXT("int8(7)"), TEXT("FScriptCaseValue(7)"), 7, false, false, false, false, false, false, nullptr},
		{TEXT("INT16"), TEXT("int16"), TEXT("int16(7)"), TEXT("FScriptCaseValue(7)"), 7, false, false, false, false, false, false, nullptr},
		{TEXT("INT"), TEXT("int"), TEXT("7"), TEXT("FScriptCaseValue(7)"), 7, false, false, false, false, false, false, nullptr},
		{TEXT("INT64"), TEXT("int64"), TEXT("int64(7)"), TEXT("FScriptCaseValue(7)"), 7, false, false, false, false, false, false, nullptr},
		{TEXT("UINT8"), TEXT("uint8"), TEXT("uint8(7)"), TEXT("FScriptCaseValue(7)"), 7, false, false, false, false, false, false, nullptr},
		{TEXT("UINT16"), TEXT("uint16"), TEXT("uint16(7)"), TEXT("FScriptCaseValue(7)"), 7, false, false, false, false, false, false, nullptr},
		{TEXT("UINT"), TEXT("uint"), TEXT("uint(7)"), TEXT("FScriptCaseValue(7)"), 7, false, false, false, false, false, false, nullptr},
		{TEXT("UINT64"), TEXT("uint64"), TEXT("uint64(7)"), TEXT("FScriptCaseValue(7)"), 7, false, false, false, false, false, false, nullptr},
		{TEXT("FLOAT32"), TEXT("float32"), TEXT("7.25f"), TEXT("FScriptCaseValue(7)"), 0, false, true, false, false, false, false, nullptr},
		{TEXT("FLOAT64"), TEXT("double"), TEXT("7.5"), TEXT("FScriptCaseValue(7)"), 0, false, true, false, false, false, false, nullptr},
		{TEXT("BOOL"), TEXT("bool"), TEXT("true"), TEXT("FScriptCaseValue(7)"), 1, false, false, true, false, false, false, nullptr},
		{TEXT("ENUM"), TEXT("ENativeCaseEnum"), TEXT("ENativeCaseEnum::One"), TEXT("FScriptCaseValue(7)"), 1, false, false, false, false, false, false, nullptr},
		{TEXT("TYPEDEF"), TEXT("NativeCaseAlias"), TEXT("NativeCaseAlias(7)"), TEXT("FScriptCaseValue(7)"), 7, false, false, false, false, false, true, TEXT("Needs host RegisterTypedef NativeCaseAlias.")},
		{TEXT("SCRIPT_VALUE"), TEXT("FScriptCaseValue"), TEXT("FScriptCaseValue(7)"), TEXT("7"), 7, false, false, false, true, false, false, nullptr},
		{TEXT("SCRIPT_REFERENCE"), TEXT("FScriptCaseReference"), TEXT("FScriptCaseReference()"), TEXT("7"), 7, false, false, false, false, true, false, nullptr},
		{TEXT("NULL_REFERENCE"), TEXT("FScriptCaseReference"), TEXT("nullptr"), TEXT("7"), 0, false, false, false, false, true, false, nullptr},
	};
	const int32 Index = static_cast<int32>(Type);
	if (Index < 0 || Index >= 17)
	{
		return nullptr;
	}
	return &Infos[Index];
}

bool IsValidParams(const FFnReturnParams& Params)
{
	return PathToken(Params.Path) != nullptr && TypeInfo(Params.Type) != nullptr;
}

bool IsReject(const EFnReturnPath Path, const EFnReturnType Type)
{
	if (Path == EFnReturnPath::IncompatibleInvalid)
	{
		return true;
	}
	return Path == EFnReturnPath::MissingInvalid && Type != EFnReturnType::Void;
}

bool IsFault(const EFnReturnPath Path, const EFnReturnType Type)
{
	return !IsReject(Path, Type) && Path == EFnReturnPath::Exception;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FReturnCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-FN-RETURN-%s-%s"),
		PathToken(Cell.Path),
		TypeInfo(Cell.Type)->Token);
}

FString MakeEntryName(const FReturnCell& Cell)
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

FFnReturnParams ParamsFromCell(const FReturnCell& Cell)
{
	FFnReturnParams Params;
	Params.Path = Cell.Path;
	Params.Type = Cell.Type;
	return Params;
}

TArray<FReturnCell> AllCells()
{
	TArray<FReturnCell> Cells;
	Cells.SetNum(136);
	for (int32 Index = 0; Index < 136; ++Index)
	{
		FReturnCell& Cell = Cells[Index];
		Cell.Path = static_cast<EFnReturnPath>(Index / 17);
		Cell.Type = static_cast<EFnReturnType>(Index % 17);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FReturnCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FReturnCell& Cell : AllCells())
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
	AppendLine(Source);
	AppendLine(Source, TEXT("class FScriptCaseReference"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 7;"));
	AppendLine(Source, TEXT("}"));
}

void AppendReturnStatement(FString& Source, const FTypeInfo& Info, const TCHAR* Indent)
{
	if (Info.bVoid)
	{
		AppendLine(Source, FString::Printf(TEXT("%sreturn;"), Indent));
		return;
	}
	AppendLine(Source, FString::Printf(TEXT("%sreturn %s;"), Indent, Info.ValueExpression));
}

void AppendProbeBody(FString& Source, const FFnReturnParams& Params, const FString& Probe)
{
	const FTypeInfo* const Info = TypeInfo(Params.Type);
	if (Params.Path == EFnReturnPath::Direct)
	{
		AppendReturnStatement(Source, *Info, TEXT("\t"));
		return;
	}
	if (Params.Path == EFnReturnPath::IfElse)
	{
		AppendLine(Source, TEXT("\tbool bTakeFirst = true;"));
		AppendLine(Source, TEXT("\tif (bTakeFirst)"));
		AppendLine(Source, TEXT("\t{"));
		AppendReturnStatement(Source, *Info, TEXT("\t\t"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\telse"));
		AppendLine(Source, TEXT("\t{"));
		AppendReturnStatement(Source, *Info, TEXT("\t\t"));
		AppendLine(Source, TEXT("\t}"));
		return;
	}
	if (Params.Path == EFnReturnPath::Switch)
	{
		AppendLine(Source, TEXT("\tint Selector = 1;"));
		AppendLine(Source, TEXT("\tswitch (Selector)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\tcase 1:"));
		AppendReturnStatement(Source, *Info, TEXT("\t\t"));
		AppendLine(Source, TEXT("\tdefault:"));
		AppendReturnStatement(Source, *Info, TEXT("\t\t"));
		AppendLine(Source, TEXT("\t}"));
		return;
	}
	if (Params.Path == EFnReturnPath::Early)
	{
		AppendLine(Source, TEXT("\tif (true)"));
		AppendLine(Source, TEXT("\t{"));
		AppendReturnStatement(Source, *Info, TEXT("\t\t"));
		AppendLine(Source, TEXT("\t}"));
		AppendReturnStatement(Source, *Info, TEXT("\t"));
		return;
	}
	if (Params.Path == EFnReturnPath::Exception)
	{
		AppendLine(Source, TEXT("\tint Zero = 0;"));
		AppendLine(Source, TEXT("\tint Crash = 1 / Zero;"));
		AppendLine(Source, TEXT("\tif (Crash == 0)"));
		AppendLine(Source, TEXT("\t{"));
		AppendReturnStatement(Source, *Info, TEXT("\t\t"));
		AppendLine(Source, TEXT("\t}"));
		AppendReturnStatement(Source, *Info, TEXT("\t"));
		return;
	}
	if (Params.Path == EFnReturnPath::MissingInvalid)
	{
		AppendLine(Source, TEXT("\tint Marker = 1;"));
		AppendLine(Source, TEXT("\tMarker += 1;"));
		return;
	}
	if (Params.Path == EFnReturnPath::IncompatibleInvalid)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), Info->IncompatibleExpression));
		return;
	}
	if (Info->bVoid)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s_Recursive(3);"), *Probe));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn %s_Recursive(3);"), *Probe));
	}
}

void AppendEntry(FString& Source, const FFnReturnParams& Params, const FString& FunctionName)
{
	const FTypeInfo* const Info = TypeInfo(Params.Type);
	const FString Probe = ProbeName(FunctionName);
	if (Params.Path == EFnReturnPath::RecursiveBase)
	{
		AppendLine(Source, FString::Printf(TEXT("%s %s_Recursive(int Depth)"), Info->ScriptType, *Probe));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tif (Depth <= 0)"));
		AppendLine(Source, TEXT("\t{"));
		AppendReturnStatement(Source, *Info, TEXT("\t\t"));
		AppendLine(Source, TEXT("\t}"));
		if (Info->bVoid)
		{
			AppendLine(Source, FString::Printf(TEXT("\t%s_Recursive(Depth - 1);"), *Probe));
		}
		else
		{
			AppendLine(Source, FString::Printf(TEXT("\treturn %s_Recursive(Depth - 1);"), *Probe));
		}
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	AppendLine(Source, FString::Printf(TEXT("%s %s()"), Info->ScriptType, *Probe));
	AppendLine(Source, TEXT("{"));
	AppendProbeBody(Source, Params, Probe);
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Info->bVoid)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s();"), *Probe));
		AppendLine(Source, TEXT("\treturn 0;"));
	}
	else if (Info->bFloat)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s Observed = %s();"), Info->ScriptType, *Probe));
		AppendLine(Source, TEXT("\treturn Observed == Observed ? 0 : 1;"));
	}
	else if (Info->bBool)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn %s() ? 1 : 0;"), *Probe));
	}
	else if (Info->bObject)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn %s().Value;"), *Probe));
	}
	else if (Info->bReference)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s@ Result = %s();"), Info->ScriptType, *Probe));
		AppendLine(Source, TEXT("\tif (Result is null)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 0;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn Result.Value;"));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn int(%s());"), *Probe));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FFnReturnParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FReturnCell& Cell)
{
	const FTypeInfo* const Info = TypeInfo(Cell.Type);
	FGeneratedCaseInfo Case;
	Case.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Path, Cell.Type))
	{
		Case.Outcome = EGeneratedCaseOutcome::CompileReject;
		Case.Execution = EGeneratedCaseExecution::SourceOnly;
		return Case;
	}
	Case.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	if (IsFault(Cell.Path, Cell.Type))
	{
		Case.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Case.ExpectedException = TEXT("Divide by zero");
	}
	else
	{
		Case.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Case.ExpectedReturn = Info->ExpectedInteger;
		Case.bLimitedObservation = Info->bFloat;
	}
	Case.Execution = Info->bHost ? EGeneratedCaseExecution::RequiresHostSetup : EGeneratedCaseExecution::Standalone;
	if (Info->HostNotes != nullptr)
	{
		Case.ExecutionNotes = Info->HostNotes;
	}
	return Case;
}
}

FStringView FFnReturnGenerator::GetProductId() const
{
	return TEXT("LANG-FN-RETURN");
}

TArray<FGeneratedCaseInfo> FFnReturnGenerator::ListCases() const
{
	using namespace FnReturnGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FReturnCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("FnReturn PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FFnReturnGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace FnReturnGeneratorDetail;
	FReturnCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FFnReturnGenerator::BuildDumpSource() const
{
	using namespace FnReturnGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FN-RETURN"));
	AppendLine(Source, TEXT("// Cases: 136 total, 86 normal, 33 reject, 17 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	for (const FReturnCell& Cell : AllCells())
	{
		if (IsReject(Cell.Path, Cell.Type))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		if (IsFault(Cell.Path, Cell.Type))
		{
			AppendLine(Source, TEXT("// Outcome: RuntimeException"));
			AppendLine(Source, TEXT("// ExpectedException: Divide by zero"));
		}
		else
		{
			AppendLine(Source, TEXT("// Outcome: ReturnValue"));
			AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		}
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
	for (const FReturnCell& Cell : AllCells())
	{
		if (!IsReject(Cell.Path, Cell.Type))
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
		AppendSharedHelpers(Source);
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FFnReturnGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace FnReturnGeneratorDetail;
	FString Source;
	AppendSharedHelpers(Source);
	int32 Emitted = 0;
	for (const FReturnCell& Cell : AllCells())
	{
		if (IsReject(Cell.Path, Cell.Type))
		{
			continue;
		}
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

FString FFnReturnGenerator::BuildReturnSource(const FFnReturnParams& Params) const
{
	using namespace FnReturnGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Path, Params.Type))
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

int32 FFnReturnGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace FnReturnGeneratorDetail;
	FReturnCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Path, Cell.Type) || IsFault(Cell.Path, Cell.Type))
	{
		return 0;
	}
	return TypeInfo(Cell.Type)->ExpectedInteger;
}

FString FFnReturnGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace FnReturnGeneratorDetail;
	FReturnCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Path, Cell.Type))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FFnReturnGenerator::ListRejectCaseIds() const
{
	using namespace FnReturnGeneratorDetail;
	TArray<FString> Ids;
	for (const FReturnCell& Cell : AllCells())
	{
		if (IsReject(Cell.Path, Cell.Type))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
