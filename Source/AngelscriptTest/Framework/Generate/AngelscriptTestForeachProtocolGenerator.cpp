#include "Framework/Generate/AngelscriptTestForeachProtocolGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ForeachProtocolGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native FNativeCaseRange.");
constexpr int32 ProtocolAxisCount = 10;
constexpr int32 ResolutionAxisCount = 5;
constexpr int32 NestingAxisCount = 5;
constexpr int32 TotalCells = ProtocolAxisCount * ResolutionAxisCount * NestingAxisCount;

struct FProtocolCell
{
	EForeachProtocol Protocol = EForeachProtocol::Complete;
	EForeachProtocolResolution Resolution = EForeachProtocolResolution::Exact;
	EForeachProtocolNesting Nesting = EForeachProtocolNesting::Single;
};

const TCHAR* ProtocolToken(const EForeachProtocol Protocol)
{
	switch (Protocol)
	{
	case EForeachProtocol::Complete:
		return TEXT("COMPLETE");
	case EForeachProtocol::Overloaded:
		return TEXT("OVERLOADED");
	case EForeachProtocol::MissingBegin:
		return TEXT("MISSING_BEGIN");
	case EForeachProtocol::MissingNext:
		return TEXT("MISSING_NEXT");
	case EForeachProtocol::MissingValue:
		return TEXT("MISSING_VALUE");
	case EForeachProtocol::MissingEnd:
		return TEXT("MISSING_END");
	case EForeachProtocol::WrongParameter:
		return TEXT("WRONG_PARAMETER");
	case EForeachProtocol::WrongReturn:
		return TEXT("WRONG_RETURN");
	case EForeachProtocol::Inaccessible:
		return TEXT("INACCESSIBLE");
	case EForeachProtocol::Throwing:
		return TEXT("THROWING");
	default:
		return nullptr;
	}
}

const TCHAR* ResolutionToken(const EForeachProtocolResolution Resolution)
{
	switch (Resolution)
	{
	case EForeachProtocolResolution::Exact:
		return TEXT("EXACT");
	case EForeachProtocolResolution::Conversion:
		return TEXT("CONVERSION");
	case EForeachProtocolResolution::ConstOverload:
		return TEXT("CONST_OVERLOAD");
	case EForeachProtocolResolution::Ambiguous:
		return TEXT("AMBIGUOUS");
	case EForeachProtocolResolution::Missing:
		return TEXT("MISSING");
	default:
		return nullptr;
	}
}

const TCHAR* NestingToken(const EForeachProtocolNesting Nesting)
{
	switch (Nesting)
	{
	case EForeachProtocolNesting::Single:
		return TEXT("SINGLE");
	case EForeachProtocolNesting::SameIterable:
		return TEXT("SAME_ITERABLE");
	case EForeachProtocolNesting::DistinctIterable:
		return TEXT("DISTINCT_ITERABLE");
	case EForeachProtocolNesting::InsideFor:
		return TEXT("INSIDE_FOR");
	case EForeachProtocolNesting::ContainsFor:
		return TEXT("CONTAINS_FOR");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FForeachProtocolParams& Params)
{
	return ProtocolToken(Params.Protocol) != nullptr
		&& ResolutionToken(Params.Resolution) != nullptr
		&& NestingToken(Params.Nesting) != nullptr;
}

bool HasCompleteProtocol(const EForeachProtocol Protocol)
{
	return Protocol == EForeachProtocol::Complete
		|| Protocol == EForeachProtocol::Overloaded
		|| Protocol == EForeachProtocol::Throwing;
}

bool HasResolvedValue(const EForeachProtocolResolution Resolution)
{
	return Resolution == EForeachProtocolResolution::Exact
		|| Resolution == EForeachProtocolResolution::Conversion
		|| Resolution == EForeachProtocolResolution::ConstOverload
		|| Resolution == EForeachProtocolResolution::Ambiguous;
}

bool IsReject(const FForeachProtocolParams& Params)
{
	return !HasCompleteProtocol(Params.Protocol) || !HasResolvedValue(Params.Resolution);
}

bool IsFault(const FForeachProtocolParams& Params)
{
	return !IsReject(Params)
		&& Params.Protocol == EForeachProtocol::Throwing
		&& Params.Resolution != EForeachProtocolResolution::Ambiguous;
}

int32 ResolutionContribution(const EForeachProtocolResolution Resolution)
{
	if (Resolution == EForeachProtocolResolution::Conversion)
	{
		return 2;
	}
	if (Resolution == EForeachProtocolResolution::ConstOverload)
	{
		return 3;
	}
	if (Resolution == EForeachProtocolResolution::Ambiguous)
	{
		return 20;
	}
	return 1;
}

int32 ExpectedFromParams(const FForeachProtocolParams& Params)
{
	if (IsReject(Params) || IsFault(Params))
	{
		return 0;
	}
	const int32 Contribution = ResolutionContribution(Params.Resolution);
	if (Params.Nesting == EForeachProtocolNesting::Single)
	{
		return 2 * Contribution;
	}
	if (Params.Nesting == EForeachProtocolNesting::InsideFor)
	{
		return 4 * Contribution;
	}
	return 6 * Contribution;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FProtocolCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-FE-PROTOCOL-%s-%s-%s"),
		ProtocolToken(Cell.Protocol),
		ResolutionToken(Cell.Resolution),
		NestingToken(Cell.Nesting));
}

FString MakeEntryName(const FProtocolCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FForeachProtocolParams ParamsFromCell(const FProtocolCell& Cell)
{
	FForeachProtocolParams Params;
	Params.Protocol = Cell.Protocol;
	Params.Resolution = Cell.Resolution;
	Params.Nesting = Cell.Nesting;
	return Params;
}

FString RangeTypeName(const FForeachProtocolParams& Params, const FString& FunctionName)
{
	return (HasCompleteProtocol(Params.Protocol) ? FString(TEXT("FForeachProtocolRange_")) : FString(TEXT("FBrokenProtocol_")))
		+ FunctionName;
}

FString ForeachVariableDeclaration(const EForeachProtocolResolution Resolution, const TCHAR* VariableName)
{
	if (Resolution == EForeachProtocolResolution::Conversion)
	{
		return FString::Printf(TEXT("int64 %s"), VariableName);
	}
	if (Resolution == EForeachProtocolResolution::ConstOverload)
	{
		return FString::Printf(TEXT("const int %s"), VariableName);
	}
	return FString::Printf(TEXT("int %s"), VariableName);
}

void AppendBrokenProtocol(FString& Source, const FForeachProtocolParams& Params, const FString& TypeName)
{
	AppendLine(Source, FString::Printf(TEXT("struct %s"), *TypeName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseRange Native;"));
	if (Params.Protocol == EForeachProtocol::Inaccessible)
	{
		AppendLine(Source, TEXT("private:"));
	}
	if (Params.Protocol != EForeachProtocol::MissingBegin)
	{
		if (Params.Protocol == EForeachProtocol::WrongReturn)
		{
			AppendLine(Source, TEXT("\tvoid opForBegin()"));
			AppendLine(Source, TEXT("\t{"));
			AppendLine(Source, TEXT("\t\tNative.opForBegin();"));
			AppendLine(Source, TEXT("\t}"));
		}
		else
		{
			AppendLine(Source, TEXT("\tint opForBegin()"));
			AppendLine(Source, TEXT("\t{"));
			AppendLine(Source, TEXT("\t\treturn Native.opForBegin();"));
			AppendLine(Source, TEXT("\t}"));
		}
		AppendLine(Source);
	}
	if (Params.Protocol != EForeachProtocol::MissingEnd)
	{
		AppendLine(Source, TEXT("\tbool opForEnd(const int Iterator)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn Iterator < 0;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
	}
	if (Params.Protocol != EForeachProtocol::MissingNext)
	{
		if (Params.Protocol == EForeachProtocol::WrongParameter)
		{
			AppendLine(Source, TEXT("\tvoid opForNext(int64& inout Iterator)"));
		}
		else
		{
			AppendLine(Source, TEXT("\tvoid opForNext(int& inout Iterator)"));
		}
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tNative.opForNext(Iterator);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
	}
	if (Params.Protocol != EForeachProtocol::MissingValue)
	{
		AppendLine(Source, TEXT("\tint opForValue(const int Iterator)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tNative.opForValue(Iterator);"));
		AppendLine(Source, TEXT("\t\treturn 1;"));
		AppendLine(Source, TEXT("\t}"));
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendProtocolValueCallbacks(FString& Source, const FForeachProtocolParams& Params)
{
	if (Params.Resolution == EForeachProtocolResolution::Ambiguous)
	{
		AppendLine(Source, TEXT("\tint opForValue(int64 Iterator)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tNative.opForValue(int(Iterator));"));
		AppendLine(Source, TEXT("\t\treturn 20;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\tint opForValue(uint64 Iterator)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tNative.opForValue(int(Iterator));"));
		AppendLine(Source, TEXT("\t\treturn 21;"));
		AppendLine(Source, TEXT("\t}"));
		return;
	}
	if (Params.Resolution == EForeachProtocolResolution::Missing)
	{
		AppendLine(Source, TEXT("\tint opForValue(const int Iterator, const int Unused)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tNative.opForValue(Iterator);"));
		AppendLine(Source, TEXT("\t\treturn Unused;"));
		AppendLine(Source, TEXT("\t}"));
		return;
	}

	const bool bConversion = Params.Resolution == EForeachProtocolResolution::Conversion;
	AppendLine(Source, bConversion ? TEXT("\tint8 opForValue(const int Iterator)") : TEXT("\tint opForValue(const int Iterator)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tNative.opForValue(Iterator);"));
	if (Params.Protocol == EForeachProtocol::Throwing)
	{
		AppendLine(Source, TEXT("\t\tint Zero = 0;"));
		AppendLine(Source, bConversion ? TEXT("\t\treturn int8(1 / Zero);") : TEXT("\t\treturn 1 / Zero;"));
	}
	else if (bConversion)
	{
		AppendLine(Source, TEXT("\t\treturn int8(2);"));
	}
	else if (Params.Resolution == EForeachProtocolResolution::ConstOverload)
	{
		AppendLine(Source, TEXT("\t\treturn 3;"));
	}
	else
	{
		AppendLine(Source, TEXT("\t\treturn 1;"));
	}
	AppendLine(Source, TEXT("\t}"));
	if (Params.Protocol == EForeachProtocol::Overloaded
		|| Params.Resolution == EForeachProtocolResolution::ConstOverload)
	{
		AppendLine(Source);
		AppendLine(Source, TEXT("\tint opForValue(int8 Iterator)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 100 + int(Iterator);"));
		AppendLine(Source, TEXT("\t}"));
	}
}

void AppendCompleteProtocol(FString& Source, const FForeachProtocolParams& Params, const FString& TypeName)
{
	AppendLine(Source, FString::Printf(TEXT("struct %s"), *TypeName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseRange Native;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint opForBegin()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn Native.opForBegin();"));
	AppendLine(Source, TEXT("\t}"));
	if (Params.Protocol == EForeachProtocol::Overloaded)
	{
		AppendLine(Source);
		AppendLine(Source, TEXT("\tint opForBegin(const int Ignored)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 100 + Ignored;"));
		AppendLine(Source, TEXT("\t}"));
	}
	AppendLine(Source);
	AppendLine(Source, TEXT("\tbool opForEnd(const int Iterator)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn Iterator < 0;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tvoid opForNext(int& inout Iterator)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tNative.opForNext(Iterator);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendProtocolValueCallbacks(Source, Params);
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendLoopBody(
	FString& Source,
	const FString& RangeName,
	const FString& Indent,
	const EForeachProtocolResolution Resolution,
	const bool bContainsFor)
{
	AppendLine(Source, Indent + TEXT("foreach (") + ForeachVariableDeclaration(Resolution, TEXT("Value")) + TEXT(" : ") + RangeName + TEXT(")"));
	AppendLine(Source, Indent + TEXT("{"));
	AppendLine(Source, Indent + TEXT("\tTrace += Value;"));
	if (bContainsFor)
	{
		AppendLine(Source, Indent + TEXT("\tfor (int Index = 0; Index < 2; ++Index)"));
		AppendLine(Source, Indent + TEXT("\t{"));
		AppendLine(Source, Indent + TEXT("\t\tTrace += Value;"));
		AppendLine(Source, Indent + TEXT("\t}"));
	}
	AppendLine(Source, Indent + TEXT("}"));
}

void AppendOwnedTypes(FString& Source, const FForeachProtocolParams& Params, const FString& FunctionName)
{
	const FString TypeName = RangeTypeName(Params, FunctionName);
	if (HasCompleteProtocol(Params.Protocol))
	{
		AppendCompleteProtocol(Source, Params, TypeName);
	}
	else
	{
		AppendBrokenProtocol(Source, Params, TypeName);
	}
}

void AppendEntry(FString& Source, const FForeachProtocolParams& Params, const FString& FunctionName)
{
	const FString TypeName = RangeTypeName(Params, FunctionName);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Range;"), *TypeName));
	AppendLine(Source, TEXT("\tRange.Native.Count = 2;"));
	if (Params.Nesting == EForeachProtocolNesting::DistinctIterable)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s OtherRange;"), *TypeName));
		AppendLine(Source, TEXT("\tOtherRange.Native.Count = 2;"));
	}
	AppendLine(Source, TEXT("\tint Trace = 0;"));
	if (IsFault(Params))
	{
		AppendLine(Source, TEXT("\tint Zero = 0;"));
		AppendLine(Source, TEXT("\tTrace += 1 / Zero;"));
	}
	if (Params.Nesting == EForeachProtocolNesting::SameIterable)
	{
		AppendLine(Source, TEXT("\tforeach (") + ForeachVariableDeclaration(Params.Resolution, TEXT("OuterValue")) + TEXT(" : Range)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tTrace += OuterValue;"));
		AppendLoopBody(Source, TEXT("Range"), TEXT("\t\t"), Params.Resolution, false);
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Params.Nesting == EForeachProtocolNesting::DistinctIterable)
	{
		AppendLine(Source, TEXT("\tforeach (") + ForeachVariableDeclaration(Params.Resolution, TEXT("OuterValue")) + TEXT(" : Range)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tTrace += OuterValue;"));
		AppendLoopBody(Source, TEXT("OtherRange"), TEXT("\t\t"), Params.Resolution, false);
		AppendLine(Source, TEXT("\t}"));
	}
	else if (Params.Nesting == EForeachProtocolNesting::InsideFor)
	{
		AppendLine(Source, TEXT("\tfor (int OuterIndex = 0; OuterIndex < 2; ++OuterIndex)"));
		AppendLine(Source, TEXT("\t{"));
		AppendLoopBody(Source, TEXT("Range"), TEXT("\t\t"), Params.Resolution, false);
		AppendLine(Source, TEXT("\t}"));
	}
	else
	{
		AppendLoopBody(
			Source,
			TEXT("Range"),
			TEXT("\t"),
			Params.Resolution,
			Params.Nesting == EForeachProtocolNesting::ContainsFor);
	}
	AppendLine(Source, TEXT("\treturn Trace;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FForeachProtocolParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOwnedTypes(Source, Params, FunctionName);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

TArray<FProtocolCell> AllCells()
{
	TArray<FProtocolCell> Cells;
	Cells.SetNum(TotalCells);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		FProtocolCell& Cell = Cells[Index];
		Cell.Protocol = static_cast<EForeachProtocol>(Index / (ResolutionAxisCount * NestingAxisCount));
		const int32 Remainder = Index % (ResolutionAxisCount * NestingAxisCount);
		Cell.Resolution = static_cast<EForeachProtocolResolution>(Remainder / NestingAxisCount);
		Cell.Nesting = static_cast<EForeachProtocolNesting>(Remainder % NestingAxisCount);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FProtocolCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FProtocolCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FGeneratedCaseInfo MakeCaseInfo(const FProtocolCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	const FForeachProtocolParams Params = ParamsFromCell(Cell);
	if (IsReject(Params))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExecutionNotes = HostNotes;
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
	return Info;
}
}

FStringView FForeachProtocolGenerator::GetProductId() const
{
	return TEXT("LANG-FE-PROTOCOL");
}

TArray<FGeneratedCaseInfo> FForeachProtocolGenerator::ListCases() const
{
	using namespace ForeachProtocolGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FProtocolCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("ForeachProtocol PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FForeachProtocolGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ForeachProtocolGeneratorDetail;
	FProtocolCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FForeachProtocolGenerator::BuildDumpSource() const
{
	using namespace ForeachProtocolGeneratorDetail;
	const TArray<FProtocolCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-FE-PROTOCOL"));
	AppendLine(Source, TEXT("// Cases: 250 total, 45 normal, 190 reject, 15 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));

	bool bNeedBlank = true;
	for (const FProtocolCell& Cell : Cells)
	{
		const FForeachProtocolParams Params = ParamsFromCell(Cell);
		if (IsReject(Params))
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
		if (IsFault(Params))
		{
			AppendLine(Source, TEXT("// Outcome: RuntimeException"));
			AppendLine(Source, TEXT("// ExpectedException: Divide by zero"));
		}
		else
		{
			AppendLine(Source, TEXT("// Outcome: ReturnValue"));
			AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		}
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source, FString::Printf(TEXT("// ExecutionNotes: %s"), HostNotes));
		AppendLine(Source);
		AppendOwnedTypes(Source, Params, MakeEntryName(Cell));
		AppendEntry(Source, Params, MakeEntryName(Cell));
		bNeedBlank = true;
	}

	for (const FProtocolCell& Cell : Cells)
	{
		const FForeachProtocolParams Params = ParamsFromCell(Cell);
		if (!IsReject(Params))
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
		AppendOwnedTypes(Source, Params, MakeEntryName(Cell));
		AppendEntry(Source, Params, MakeEntryName(Cell));
	}
	return Source;
}

FString FForeachProtocolGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ForeachProtocolGeneratorDetail;
	FString Source;
	int32 Emitted = 0;
	for (const FProtocolCell& Cell : AllCells())
	{
		const FForeachProtocolParams Params = ParamsFromCell(Cell);
		if (IsReject(Params))
		{
			continue;
		}
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		AppendOwnedTypes(Source, Params, MakeEntryName(Cell));
		AppendEntry(Source, Params, MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

FString FForeachProtocolGenerator::BuildForeachProtocolSource(const FForeachProtocolParams& Params) const
{
	using namespace ForeachProtocolGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params))
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

int32 FForeachProtocolGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ForeachProtocolGeneratorDetail;
	FProtocolCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedFromParams(ParamsFromCell(Cell));
}

FString FForeachProtocolGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace ForeachProtocolGeneratorDetail;
	FProtocolCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(ParamsFromCell(Cell)))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FForeachProtocolGenerator::ListRejectCaseIds() const
{
	using namespace ForeachProtocolGeneratorDetail;
	TArray<FString> Ids;
	for (const FProtocolCell& Cell : AllCells())
	{
		if (IsReject(ParamsFromCell(Cell)))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
