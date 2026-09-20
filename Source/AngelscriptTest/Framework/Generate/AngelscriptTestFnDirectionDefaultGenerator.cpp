#include "Framework/Generate/AngelscriptTestFnDirectionDefaultGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace FnDirectionDefaultGeneratorDetail
{
struct FDirectionDefaultCell
{
	EFnDirectionDefaultType Type = EFnDirectionDefaultType::Int8;
	EFnDirectionDefaultDirection Direction = EFnDirectionDefaultDirection::Value;
	EFnDirectionDefaultState DefaultState = EFnDirectionDefaultState::NoneExplicit;
	EFnDirectionDefaultTarget Target = EFnDirectionDefaultTarget::Global;
};

const TCHAR* TypeToken(const EFnDirectionDefaultType Type)
{
	switch (Type)
	{
	case EFnDirectionDefaultType::Int8:
		return TEXT("INT8");
	case EFnDirectionDefaultType::Int16:
		return TEXT("INT16");
	case EFnDirectionDefaultType::Int:
		return TEXT("INT");
	case EFnDirectionDefaultType::Int64:
		return TEXT("INT64");
	case EFnDirectionDefaultType::Uint8:
		return TEXT("UINT8");
	case EFnDirectionDefaultType::Uint16:
		return TEXT("UINT16");
	case EFnDirectionDefaultType::Uint:
		return TEXT("UINT");
	case EFnDirectionDefaultType::Uint64:
		return TEXT("UINT64");
	case EFnDirectionDefaultType::Float32:
		return TEXT("FLOAT32");
	case EFnDirectionDefaultType::Float64:
		return TEXT("FLOAT64");
	case EFnDirectionDefaultType::Bool:
		return TEXT("BOOL");
	default:
		return nullptr;
	}
}

const TCHAR* DirectionToken(const EFnDirectionDefaultDirection Direction)
{
	switch (Direction)
	{
	case EFnDirectionDefaultDirection::Value:
		return TEXT("VALUE");
	case EFnDirectionDefaultDirection::In:
		return TEXT("IN");
	case EFnDirectionDefaultDirection::Out:
		return TEXT("OUT");
	case EFnDirectionDefaultDirection::Inout:
		return TEXT("INOUT");
	default:
		return nullptr;
	}
}

const TCHAR* StateToken(const EFnDirectionDefaultState State)
{
	switch (State)
	{
	case EFnDirectionDefaultState::NoneExplicit:
		return TEXT("NONE_EXPLICIT");
	case EFnDirectionDefaultState::NoneOmitted:
		return TEXT("NONE_OMITTED");
	case EFnDirectionDefaultState::PresentExplicit:
		return TEXT("PRESENT_EXPLICIT");
	case EFnDirectionDefaultState::PresentOmitted:
		return TEXT("PRESENT_OMITTED");
	default:
		return nullptr;
	}
}

const TCHAR* TargetToken(const EFnDirectionDefaultTarget Target)
{
	switch (Target)
	{
	case EFnDirectionDefaultTarget::Global:
		return TEXT("GLOBAL");
	case EFnDirectionDefaultTarget::NamespaceGlobal:
		return TEXT("NAMESPACE_GLOBAL");
	case EFnDirectionDefaultTarget::InstanceMethod:
		return TEXT("INSTANCE_METHOD");
	default:
		return nullptr;
	}
}

const TCHAR* TypeName(const EFnDirectionDefaultType Type)
{
	switch (Type)
	{
	case EFnDirectionDefaultType::Int8:
		return TEXT("int8");
	case EFnDirectionDefaultType::Int16:
		return TEXT("int16");
	case EFnDirectionDefaultType::Int:
		return TEXT("int");
	case EFnDirectionDefaultType::Int64:
		return TEXT("int64");
	case EFnDirectionDefaultType::Uint8:
		return TEXT("uint8");
	case EFnDirectionDefaultType::Uint16:
		return TEXT("uint16");
	case EFnDirectionDefaultType::Uint:
		return TEXT("uint");
	case EFnDirectionDefaultType::Uint64:
		return TEXT("uint64");
	case EFnDirectionDefaultType::Float32:
		return TEXT("float32");
	case EFnDirectionDefaultType::Float64:
		return TEXT("float64");
	case EFnDirectionDefaultType::Bool:
		return TEXT("bool");
	default:
		return nullptr;
	}
}

const TCHAR* DirectionSuffix(const EFnDirectionDefaultDirection Direction)
{
	switch (Direction)
	{
	case EFnDirectionDefaultDirection::Value:
		return TEXT("");
	case EFnDirectionDefaultDirection::In:
		return TEXT("& in");
	case EFnDirectionDefaultDirection::Out:
		return TEXT("& out");
	case EFnDirectionDefaultDirection::Inout:
		return TEXT("& inout");
	default:
		return nullptr;
	}
}

const TCHAR* ZeroLiteral(const EFnDirectionDefaultType Type)
{
	switch (Type)
	{
	case EFnDirectionDefaultType::Float32:
		return TEXT("0.0f");
	case EFnDirectionDefaultType::Float64:
		return TEXT("0.0");
	case EFnDirectionDefaultType::Bool:
		return TEXT("false");
	default:
		return TEXT("0");
	}
}

const TCHAR* OneLiteral(const EFnDirectionDefaultType Type)
{
	switch (Type)
	{
	case EFnDirectionDefaultType::Float32:
		return TEXT("1.0f");
	case EFnDirectionDefaultType::Float64:
		return TEXT("1.0");
	case EFnDirectionDefaultType::Bool:
		return TEXT("true");
	default:
		return TEXT("1");
	}
}

bool ReadsValue(const EFnDirectionDefaultDirection Direction)
{
	return Direction != EFnDirectionDefaultDirection::Out;
}

bool WritesValue(const EFnDirectionDefaultDirection Direction)
{
	return Direction == EFnDirectionDefaultDirection::Out || Direction == EFnDirectionDefaultDirection::Inout;
}

bool HasDefault(const EFnDirectionDefaultState State)
{
	return State == EFnDirectionDefaultState::PresentExplicit || State == EFnDirectionDefaultState::PresentOmitted;
}

bool OmitsCall(const EFnDirectionDefaultState State)
{
	return State == EFnDirectionDefaultState::NoneOmitted || State == EFnDirectionDefaultState::PresentOmitted;
}

bool ExactOmittedDefaultType(const EFnDirectionDefaultType Type)
{
	return Type == EFnDirectionDefaultType::Int
		|| Type == EFnDirectionDefaultType::Float32
		|| Type == EFnDirectionDefaultType::Float64
		|| Type == EFnDirectionDefaultType::Bool;
}

bool IsValidParams(const FFnDirectionDefaultParams& Params)
{
	return TypeToken(Params.Type) != nullptr
		&& DirectionToken(Params.Direction) != nullptr
		&& StateToken(Params.DefaultState) != nullptr
		&& TargetToken(Params.Target) != nullptr;
}

bool IsReject(
	const EFnDirectionDefaultType Type,
	const EFnDirectionDefaultDirection Direction,
	const EFnDirectionDefaultState State)
{
	if (State == EFnDirectionDefaultState::NoneOmitted)
	{
		return true;
	}
	if (!OmitsCall(State) || !HasDefault(State))
	{
		return false;
	}
	if (Direction == EFnDirectionDefaultDirection::Value)
	{
		return false;
	}
	if (Direction != EFnDirectionDefaultDirection::In)
	{
		return true;
	}
	return !ExactOmittedDefaultType(Type);
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FDirectionDefaultCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-FN-DIRECTION-DEFAULT-%s-%s-%s-%s"),
		TypeToken(Cell.Type),
		DirectionToken(Cell.Direction),
		StateToken(Cell.DefaultState),
		TargetToken(Cell.Target));
}

FString MakeEntryName(const FDirectionDefaultCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FFnDirectionDefaultParams ParamsFromCell(const FDirectionDefaultCell& Cell)
{
	FFnDirectionDefaultParams Params;
	Params.Type = Cell.Type;
	Params.Direction = Cell.Direction;
	Params.DefaultState = Cell.DefaultState;
	Params.Target = Cell.Target;
	return Params;
}

TArray<FDirectionDefaultCell> AllCells()
{
	TArray<FDirectionDefaultCell> Cells;
	Cells.SetNum(528);
	for (int32 Index = 0; Index < 528; ++Index)
	{
		FDirectionDefaultCell& Cell = Cells[Index];
		Cell.Type = static_cast<EFnDirectionDefaultType>(Index / 48);
		Cell.Direction = static_cast<EFnDirectionDefaultDirection>((Index / 12) % 4);
		Cell.DefaultState = static_cast<EFnDirectionDefaultState>((Index / 3) % 4);
		Cell.Target = static_cast<EFnDirectionDefaultTarget>(Index % 3);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FDirectionDefaultCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FDirectionDefaultCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FString NameSuffix(const FString& FunctionName)
{
	return FunctionName == TEXT("Entry") ? FString() : FunctionName;
}

void AppendEntry(FString& Source, const FFnDirectionDefaultParams& Params, const FString& FunctionName)
{
	const FString Suffix = NameSuffix(FunctionName);
	const FString ProbeName = TEXT("Probe") + Suffix;
	const FString DefaultSuffix = HasDefault(Params.DefaultState)
		? FString::Printf(TEXT(" = %s"), OneLiteral(Params.Type))
		: FString();
	const TCHAR* const ExpectedRead = OmitsCall(Params.DefaultState)
		? OneLiteral(Params.Type)
		: ZeroLiteral(Params.Type);
	const FString Parameter = FString::Printf(
		TEXT("%s%s Value%s"),
		TypeName(Params.Type),
		DirectionSuffix(Params.Direction),
		*DefaultSuffix);
	const FString ProbeDeclaration = FString::Printf(TEXT("bool %s(%s)"), *ProbeName, *Parameter);

	if (Params.Target == EFnDirectionDefaultTarget::NamespaceGlobal)
	{
		AppendLine(Source, FString::Printf(TEXT("namespace DirectionDefaultNamespace%s"), *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\t") + ProbeDeclaration);
		AppendLine(Source, TEXT("\t{"));
		if (ReadsValue(Params.Direction))
		{
			AppendLine(Source, FString::Printf(TEXT("\t\tif (!(Value == %s))"), ExpectedRead));
			AppendLine(Source, TEXT("\t\t{"));
			AppendLine(Source, TEXT("\t\t\treturn false;"));
			AppendLine(Source, TEXT("\t\t}"));
		}
		if (WritesValue(Params.Direction))
		{
			AppendLine(Source, FString::Printf(TEXT("\t\tValue = %s;"), OneLiteral(Params.Type)));
		}
		AppendLine(Source, TEXT("\t\treturn true;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Params.Target == EFnDirectionDefaultTarget::InstanceMethod)
	{
		AppendLine(Source, FString::Printf(TEXT("struct FDirectionDefaultReceiver%s"), *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\t") + ProbeDeclaration);
		AppendLine(Source, TEXT("\t{"));
		if (ReadsValue(Params.Direction))
		{
			AppendLine(Source, FString::Printf(TEXT("\t\tif (!(Value == %s))"), ExpectedRead));
			AppendLine(Source, TEXT("\t\t{"));
			AppendLine(Source, TEXT("\t\t\treturn false;"));
			AppendLine(Source, TEXT("\t\t}"));
		}
		if (WritesValue(Params.Direction))
		{
			AppendLine(Source, FString::Printf(TEXT("\t\tValue = %s;"), OneLiteral(Params.Type)));
		}
		AppendLine(Source, TEXT("\t\treturn true;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else
	{
		AppendLine(Source, ProbeDeclaration);
		AppendLine(Source, TEXT("{"));
		if (ReadsValue(Params.Direction))
		{
			AppendLine(Source, FString::Printf(TEXT("\tif (!(Value == %s))"), ExpectedRead));
			AppendLine(Source, TEXT("\t{"));
			AppendLine(Source, TEXT("\t\treturn false;"));
			AppendLine(Source, TEXT("\t}"));
		}
		if (WritesValue(Params.Direction))
		{
			AppendLine(Source, FString::Printf(TEXT("\tValue = %s;"), OneLiteral(Params.Type)));
		}
		AppendLine(Source, TEXT("\treturn true;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}

	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Target == EFnDirectionDefaultTarget::InstanceMethod)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFDirectionDefaultReceiver%s Receiver;"), *Suffix));
	}
	if (!OmitsCall(Params.DefaultState))
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s Value = %s;"),
			TypeName(Params.Type),
			ZeroLiteral(Params.Type)));
	}
	const TCHAR* const CallArgs = OmitsCall(Params.DefaultState) ? TEXT("()") : TEXT("(Value)");
	FString CallExpression;
	if (Params.Target == EFnDirectionDefaultTarget::NamespaceGlobal)
	{
		CallExpression = FString::Printf(TEXT("DirectionDefaultNamespace%s::%s%s"), *Suffix, *ProbeName, CallArgs);
	}
	else if (Params.Target == EFnDirectionDefaultTarget::InstanceMethod)
	{
		CallExpression = FString::Printf(TEXT("Receiver.%s%s"), *ProbeName, CallArgs);
	}
	else
	{
		CallExpression = ProbeName + CallArgs;
	}
	if (!OmitsCall(Params.DefaultState) && WritesValue(Params.Direction))
	{
		AppendLine(Source, FString::Printf(
			TEXT("\treturn (%s && (Value == %s)) ? 1 : 0;"),
			*CallExpression,
			OneLiteral(Params.Type)));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn %s ? 1 : 0;"), *CallExpression));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FFnDirectionDefaultParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FDirectionDefaultCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Type, Cell.Direction, Cell.DefaultState))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = 1;
	return Info;
}
}

FStringView FFnDirectionDefaultGenerator::GetProductId() const
{
	return TEXT("LANG-FN-DIRECTION-DEFAULT");
}

TArray<FGeneratedCaseInfo> FFnDirectionDefaultGenerator::ListCases() const
{
	using namespace FnDirectionDefaultGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FDirectionDefaultCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("FnDirectionDefault PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FFnDirectionDefaultGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace FnDirectionDefaultGeneratorDetail;
	FDirectionDefaultCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FFnDirectionDefaultGenerator::BuildDumpSource() const
{
	using namespace FnDirectionDefaultGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FN-DIRECTION-DEFAULT"));
	AppendLine(Source, TEXT("// Cases: 528 total, 309 normal, 219 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (const FDirectionDefaultCell& Cell : AllCells())
	{
		if (IsReject(Cell.Type, Cell.Direction, Cell.DefaultState))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, TEXT("// ExpectedReturn: 1"));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	for (const FDirectionDefaultCell& Cell : AllCells())
	{
		if (!IsReject(Cell.Type, Cell.Direction, Cell.DefaultState))
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
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FFnDirectionDefaultGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace FnDirectionDefaultGeneratorDetail;
	FString Source;
	int32 Emitted = 0;
	for (const FDirectionDefaultCell& Cell : AllCells())
	{
		if (IsReject(Cell.Type, Cell.Direction, Cell.DefaultState))
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

FString FFnDirectionDefaultGenerator::BuildSource(const FFnDirectionDefaultParams& Params) const
{
	using namespace FnDirectionDefaultGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Type, Params.Direction, Params.DefaultState))
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

int32 FFnDirectionDefaultGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace FnDirectionDefaultGeneratorDetail;
	FDirectionDefaultCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Type, Cell.Direction, Cell.DefaultState))
	{
		return 0;
	}
	return 1;
}

FString FFnDirectionDefaultGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace FnDirectionDefaultGeneratorDetail;
	FDirectionDefaultCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Type, Cell.Direction, Cell.DefaultState))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FFnDirectionDefaultGenerator::ListRejectCaseIds() const
{
	using namespace FnDirectionDefaultGeneratorDetail;
	TArray<FString> Ids;
	for (const FDirectionDefaultCell& Cell : AllCells())
	{
		if (IsReject(Cell.Type, Cell.Direction, Cell.DefaultState))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
