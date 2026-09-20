#include "Framework/Generate/AngelscriptTestFnRecursionGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace FnRecursionGeneratorDetail
{
struct FRecursionCell
{
	EFnRecursionDepth Depth = EFnRecursionDepth::Zero;
	EFnRecursionOutcome Outcome = EFnRecursionOutcome::Return;
	EFnRecursionType Type = EFnRecursionType::Primitive;
};

const TCHAR* DepthToken(const EFnRecursionDepth Depth)
{
	switch (Depth)
	{
	case EFnRecursionDepth::Zero:
		return TEXT("ZERO");
	case EFnRecursionDepth::One:
		return TEXT("ONE");
	case EFnRecursionDepth::Eight:
		return TEXT("EIGHT");
	case EFnRecursionDepth::ConfiguredLimit:
		return TEXT("CONFIGURED_LIMIT");
	default:
		return nullptr;
	}
}

const TCHAR* OutcomeToken(const EFnRecursionOutcome Outcome)
{
	switch (Outcome)
	{
	case EFnRecursionOutcome::Return:
		return TEXT("RETURN");
	case EFnRecursionOutcome::Exception:
		return TEXT("EXCEPTION");
	default:
		return nullptr;
	}
}

const TCHAR* TypeToken(const EFnRecursionType Type)
{
	switch (Type)
	{
	case EFnRecursionType::Primitive:
		return TEXT("PRIMITIVE");
	case EFnRecursionType::ValueObject:
		return TEXT("VALUE_OBJECT");
	case EFnRecursionType::ReferenceObject:
		return TEXT("REFERENCE_OBJECT");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FFnRecursionParams& Params)
{
	return DepthToken(Params.Depth) != nullptr
		&& OutcomeToken(Params.Outcome) != nullptr
		&& TypeToken(Params.Type) != nullptr;
}

bool IsReject(const EFnRecursionDepth Depth)
{
	return Depth == EFnRecursionDepth::Eight || Depth == EFnRecursionDepth::ConfiguredLimit;
}

bool IsFault(const FFnRecursionParams& Params)
{
	return !IsReject(Params.Depth) && Params.Outcome == EFnRecursionOutcome::Exception;
}

int32 OrdinaryDepth(const EFnRecursionDepth Depth)
{
	switch (Depth)
	{
	case EFnRecursionDepth::One:
		return 1;
	case EFnRecursionDepth::Eight:
		return 8;
	case EFnRecursionDepth::ConfiguredLimit:
		return 12;
	default:
		return 0;
	}
}

int32 ExecutionDepth(const EFnRecursionDepth Depth, const EFnRecursionOutcome Outcome)
{
	if (Depth != EFnRecursionDepth::ConfiguredLimit)
	{
		return OrdinaryDepth(Depth);
	}
	return Outcome == EFnRecursionOutcome::Exception ? 13 : 11;
}

int32 ExpectedFromParams(const FFnRecursionParams& Params)
{
	if (IsReject(Params.Depth) || IsFault(Params))
	{
		return 0;
	}
	return 1 + ExecutionDepth(Params.Depth, Params.Outcome);
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FRecursionCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-FN-RECURSION-%s-%s-%s"),
		DepthToken(Cell.Depth),
		OutcomeToken(Cell.Outcome),
		TypeToken(Cell.Type));
}

FString MakeEntryName(const FRecursionCell& Cell)
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

FString RecurseName(const FString& FunctionName, const TCHAR* Side)
{
	const FString Stem = HelperStem(FunctionName);
	return Stem.IsEmpty() ? FString(Side) : FString(Side) + Stem;
}

FFnRecursionParams ParamsFromCell(const FRecursionCell& Cell)
{
	FFnRecursionParams Params;
	Params.Depth = Cell.Depth;
	Params.Outcome = Cell.Outcome;
	Params.Type = Cell.Type;
	return Params;
}

TArray<FRecursionCell> AllCells()
{
	TArray<FRecursionCell> Cells;
	Cells.SetNum(24);
	for (int32 Index = 0; Index < 24; ++Index)
	{
		FRecursionCell& Cell = Cells[Index];
		Cell.Depth = static_cast<EFnRecursionDepth>(Index / 6);
		Cell.Outcome = static_cast<EFnRecursionOutcome>((Index / 3) % 2);
		Cell.Type = static_cast<EFnRecursionType>(Index % 3);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FRecursionCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FRecursionCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FString MakeParameters(const EFnRecursionType Type)
{
	if (Type == EFnRecursionType::ReferenceObject)
	{
		return TEXT("FNativeCaseReference Value, int Depth, int Accumulator");
	}
	if (Type == EFnRecursionType::ValueObject)
	{
		return TEXT("FNativeCaseValue Value, int Depth");
	}
	return TEXT("int Value, int Depth");
}

void AppendBaseReturn(FString& Source, const FFnRecursionParams& Params, const TCHAR* Indent)
{
	if (Params.Outcome == EFnRecursionOutcome::Exception)
	{
		AppendLine(Source, FString::Printf(TEXT("%sint Zero = 0;"), Indent));
		AppendLine(Source, FString::Printf(TEXT("%sreturn 1 / Zero;"), Indent));
		return;
	}
	if (Params.Type == EFnRecursionType::ValueObject)
	{
		AppendLine(Source, FString::Printf(TEXT("%sreturn Value.Value;"), Indent));
		return;
	}
	if (Params.Type == EFnRecursionType::ReferenceObject)
	{
		AppendLine(Source, FString::Printf(TEXT("%sreturn Accumulator;"), Indent));
		return;
	}
	AppendLine(Source, FString::Printf(TEXT("%sreturn Value;"), Indent));
}

void AppendRecursiveFunction(
	FString& Source,
	const FString& FunctionName,
	const FString& NextName,
	const FFnRecursionParams& Params)
{
	AppendLine(Source, FString::Printf(TEXT("int %s(%s)"), *FunctionName, *MakeParameters(Params.Type)));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tif (Depth <= 0)"));
	AppendLine(Source, TEXT("\t{"));
	AppendBaseReturn(Source, Params, TEXT("\t\t"));
	AppendLine(Source, TEXT("\t}"));
	if (Params.Type == EFnRecursionType::ValueObject)
	{
		AppendLine(Source, TEXT("\tFNativeCaseValue NextValue = Value;"));
		AppendLine(Source, TEXT("\tNextValue.Value += 1;"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s(NextValue, Depth - 1);"), *NextName));
	}
	else if (Params.Type == EFnRecursionType::ReferenceObject)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn %s(Value, Depth - 1, Accumulator + 1);"), *NextName));
	}
	else
	{
		AppendLine(Source, TEXT("\tint NextValue = Value + 1;"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s(NextValue, Depth - 1);"), *NextName));
	}
	AppendLine(Source, TEXT("}"));
}

void AppendEntry(FString& Source, const FFnRecursionParams& Params, const FString& FunctionName)
{
	const FString RecurseA = RecurseName(FunctionName, TEXT("RecurseA"));
	const FString RecurseB = RecurseName(FunctionName, TEXT("RecurseB"));
	const bool bMutual = IsReject(Params.Depth);
	if (bMutual)
	{
		AppendLine(Source, FString::Printf(TEXT("int %s(%s);"), *RecurseB, *MakeParameters(Params.Type)));
		AppendLine(Source);
		AppendRecursiveFunction(Source, RecurseA, RecurseB, Params);
		AppendLine(Source);
		AppendRecursiveFunction(Source, RecurseB, RecurseA, Params);
	}
	else
	{
		AppendRecursiveFunction(Source, RecurseA, RecurseA, Params);
	}
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	const int32 Depth = ExecutionDepth(Params.Depth, Params.Outcome);
	if (Params.Type == EFnRecursionType::ValueObject)
	{
		AppendLine(Source, TEXT("\tFNativeCaseValue Value(1);"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s(Value, %d);"), *RecurseA, Depth));
	}
	else if (Params.Type == EFnRecursionType::ReferenceObject)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn %s(nullptr, %d, 1);"), *RecurseA, Depth));
	}
	else
	{
		AppendLine(Source, TEXT("\tint Value = 1;"));
		AppendLine(Source, FString::Printf(TEXT("\treturn %s(Value, %d);"), *RecurseA, Depth));
	}
	AppendLine(Source, TEXT("}"));
}

void AppendSharedHelpers(FString& Source)
{
	AppendLine(Source, TEXT("int CleanAfterRecursion()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn 42;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FFnRecursionParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FRecursionCell& Cell)
{
	const FFnRecursionParams Params = ParamsFromCell(Cell);
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Depth))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	if (IsFault(Params))
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.ExpectedException = TEXT("Divide by zero");
	}
	else
	{
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.ExpectedReturn = ExpectedFromParams(Params);
	}
	if (Cell.Type == EFnRecursionType::Primitive)
	{
		Info.Execution = EGeneratedCaseExecution::Standalone;
	}
	else
	{
		Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
		Info.ExecutionNotes = Cell.Type == EFnRecursionType::ValueObject
			? TEXT("Needs native FNativeCaseValue.")
			: TEXT("Needs native FNativeCaseReference.");
	}
	return Info;
}
}

FStringView FFnRecursionGenerator::GetProductId() const
{
	return TEXT("LANG-FN-RECURSION");
}

TArray<FGeneratedCaseInfo> FFnRecursionGenerator::ListCases() const
{
	using namespace FnRecursionGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FRecursionCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("FnRecursion PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FFnRecursionGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace FnRecursionGeneratorDetail;
	FRecursionCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FFnRecursionGenerator::BuildDumpSource() const
{
	using namespace FnRecursionGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FN-RECURSION"));
	AppendLine(Source, TEXT("// Cases: 24 total, 6 normal, 12 reject, 6 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	for (const FRecursionCell& Cell : AllCells())
	{
		if (IsReject(Cell.Depth))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		if (IsFault(ParamsFromCell(Cell)))
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
	for (const FRecursionCell& Cell : AllCells())
	{
		if (!IsReject(Cell.Depth))
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

FString FFnRecursionGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace FnRecursionGeneratorDetail;
	FString Source;
	AppendSharedHelpers(Source);
	int32 Emitted = 0;
	for (const FRecursionCell& Cell : AllCells())
	{
		if (IsReject(Cell.Depth))
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

FString FFnRecursionGenerator::BuildRecursionSource(const FFnRecursionParams& Params) const
{
	using namespace FnRecursionGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Depth))
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

int32 FFnRecursionGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace FnRecursionGeneratorDetail;
	FRecursionCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromParams(ParamsFromCell(Cell));
}

FString FFnRecursionGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace FnRecursionGeneratorDetail;
	FRecursionCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Depth))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FFnRecursionGenerator::ListRejectCaseIds() const
{
	using namespace FnRecursionGeneratorDetail;
	TArray<FString> Ids;
	for (const FRecursionCell& Cell : AllCells())
	{
		if (IsReject(Cell.Depth))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
