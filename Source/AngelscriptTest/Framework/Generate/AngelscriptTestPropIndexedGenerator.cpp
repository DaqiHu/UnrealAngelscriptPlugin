#include "Framework/Generate/AngelscriptTestPropIndexedGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace PropIndexedGeneratorDetail
{
constexpr int32 CandidateCount = 6;
constexpr int32 IndexTypeCount = 13;
constexpr int32 OperationCount = 3;
constexpr int32 ReceiverCount = 2;
constexpr int32 TotalCells = CandidateCount * IndexTypeCount * OperationCount * ReceiverCount;
constexpr const TCHAR* HostNotes = TEXT("Needs native CreateIndexedProperty and ObserveIndexedProperty.");

struct FIndexedCell
{
	EPropIndexedCandidateSet CandidateSet = EPropIndexedCandidateSet::SameType;
	EPropIndexedIndexType IndexType = EPropIndexedIndexType::Int8;
	EPropIndexedOperation Operation = EPropIndexedOperation::Read;
	EPropIndexedReceiver Receiver = EPropIndexedReceiver::Mutable;
};

const TCHAR* CandidateToken(const EPropIndexedCandidateSet CandidateSet)
{
	switch (CandidateSet)
	{
	case EPropIndexedCandidateSet::SameType:
		return TEXT("SAME_TYPE");
	case EPropIndexedCandidateSet::AdjacentNumeric:
		return TEXT("ADJACENT_NUMERIC");
	case EPropIndexedCandidateSet::CrossFamily:
		return TEXT("CROSS_FAMILY");
	case EPropIndexedCandidateSet::CompetingPrimaryFirst:
		return TEXT("COMPETING_PRIMARY_FIRST");
	case EPropIndexedCandidateSet::CompetingSecondaryFirst:
		return TEXT("COMPETING_SECONDARY_FIRST");
	case EPropIndexedCandidateSet::Unrelated:
		return TEXT("UNRELATED");
	default:
		return nullptr;
	}
}

const TCHAR* IndexTypeToken(const EPropIndexedIndexType IndexType)
{
	switch (IndexType)
	{
	case EPropIndexedIndexType::Int8:
		return TEXT("INT8");
	case EPropIndexedIndexType::Int16:
		return TEXT("INT16");
	case EPropIndexedIndexType::Int:
		return TEXT("INT");
	case EPropIndexedIndexType::Int64:
		return TEXT("INT64");
	case EPropIndexedIndexType::UInt8:
		return TEXT("UINT8");
	case EPropIndexedIndexType::UInt16:
		return TEXT("UINT16");
	case EPropIndexedIndexType::UInt:
		return TEXT("UINT");
	case EPropIndexedIndexType::UInt64:
		return TEXT("UINT64");
	case EPropIndexedIndexType::Float32:
		return TEXT("FLOAT32");
	case EPropIndexedIndexType::Float64:
		return TEXT("FLOAT64");
	case EPropIndexedIndexType::Bool:
		return TEXT("BOOL");
	case EPropIndexedIndexType::Enum:
		return TEXT("ENUM");
	case EPropIndexedIndexType::Typedef:
		return TEXT("TYPEDEF");
	default:
		return nullptr;
	}
}

const TCHAR* OperationToken(const EPropIndexedOperation Operation)
{
	switch (Operation)
	{
	case EPropIndexedOperation::Read:
		return TEXT("READ");
	case EPropIndexedOperation::Write:
		return TEXT("WRITE");
	case EPropIndexedOperation::Compound:
		return TEXT("COMPOUND");
	default:
		return nullptr;
	}
}

const TCHAR* ReceiverToken(const EPropIndexedReceiver Receiver)
{
	switch (Receiver)
	{
	case EPropIndexedReceiver::Mutable:
		return TEXT("MUTABLE");
	case EPropIndexedReceiver::Const:
		return TEXT("CONST");
	default:
		return nullptr;
	}
}

const TCHAR* ScriptType(const EPropIndexedIndexType IndexType)
{
	switch (IndexType)
	{
	case EPropIndexedIndexType::Int8:
		return TEXT("int8");
	case EPropIndexedIndexType::Int16:
		return TEXT("int16");
	case EPropIndexedIndexType::Int:
		return TEXT("int");
	case EPropIndexedIndexType::Int64:
		return TEXT("int64");
	case EPropIndexedIndexType::UInt8:
		return TEXT("uint8");
	case EPropIndexedIndexType::UInt16:
		return TEXT("uint16");
	case EPropIndexedIndexType::UInt:
		return TEXT("uint");
	case EPropIndexedIndexType::UInt64:
		return TEXT("uint64");
	case EPropIndexedIndexType::Float32:
		return TEXT("float");
	case EPropIndexedIndexType::Float64:
		return TEXT("double");
	case EPropIndexedIndexType::Bool:
		return TEXT("bool");
	case EPropIndexedIndexType::Enum:
		return TEXT("ERegisteredIndex");
	case EPropIndexedIndexType::Typedef:
		return TEXT("RegisteredIndexAlias");
	default:
		return nullptr;
	}
}

const TCHAR* ScriptLiteral(const EPropIndexedIndexType IndexType)
{
	switch (IndexType)
	{
	case EPropIndexedIndexType::Float32:
		return TEXT("3.0f");
	case EPropIndexedIndexType::Float64:
		return TEXT("3.0");
	case EPropIndexedIndexType::Bool:
		return TEXT("true");
	case EPropIndexedIndexType::Enum:
		return TEXT("ERegisteredIndex::Three");
	default:
		return TEXT("3");
	}
}

int32 ArgumentValue(const EPropIndexedIndexType IndexType)
{
	return IndexType == EPropIndexedIndexType::Bool ? 1 : 3;
}

int32 CandidateMarker(const EPropIndexedCandidateSet CandidateSet)
{
	switch (CandidateSet)
	{
	case EPropIndexedCandidateSet::SameType:
		return 101;
	case EPropIndexedCandidateSet::AdjacentNumeric:
		return 201;
	case EPropIndexedCandidateSet::CrossFamily:
		return 301;
	case EPropIndexedCandidateSet::CompetingPrimaryFirst:
		return 401;
	case EPropIndexedCandidateSet::CompetingSecondaryFirst:
		return 402;
	default:
		return 0;
	}
}

bool FirstMatches(const EPropIndexedCandidateSet CandidateSet, const EPropIndexedIndexType IndexType)
{
	if (CandidateSet == EPropIndexedCandidateSet::Unrelated)
	{
		return false;
	}
	if (CandidateSet == EPropIndexedCandidateSet::SameType
		|| CandidateSet == EPropIndexedCandidateSet::CompetingPrimaryFirst)
	{
		return true;
	}
	return IndexType != EPropIndexedIndexType::Bool && IndexType != EPropIndexedIndexType::Enum;
}

bool IsCompeting(const EPropIndexedCandidateSet CandidateSet)
{
	return CandidateSet == EPropIndexedCandidateSet::CompetingPrimaryFirst
		|| CandidateSet == EPropIndexedCandidateSet::CompetingSecondaryFirst;
}

bool IsReject(const FPropIndexedParams& Params)
{
	if (Params.Operation == EPropIndexedOperation::Compound)
	{
		return true;
	}
	if (Params.Operation == EPropIndexedOperation::Read && IsCompeting(Params.CandidateSet))
	{
		return true;
	}
	if (!FirstMatches(Params.CandidateSet, Params.IndexType))
	{
		return true;
	}
	return Params.Operation == EPropIndexedOperation::Write && Params.Receiver == EPropIndexedReceiver::Const;
}

int32 ExpectedFromParams(const FPropIndexedParams& Params)
{
	const int32 Marker = CandidateMarker(Params.CandidateSet);
	const int32 Argument = ArgumentValue(Params.IndexType);
	if (Params.Operation == EPropIndexedOperation::Read)
	{
		return Marker * 100 + Argument;
	}
	return Marker * 10000 + Argument * 100 + 73;
}

bool IsValidParams(const FPropIndexedParams& Params)
{
	return CandidateToken(Params.CandidateSet) != nullptr
		&& IndexTypeToken(Params.IndexType) != nullptr
		&& OperationToken(Params.Operation) != nullptr
		&& ReceiverToken(Params.Receiver) != nullptr;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FIndexedCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-PROP-INDEXED-%s-%s-%s-%s"),
		CandidateToken(Cell.CandidateSet),
		IndexTypeToken(Cell.IndexType),
		OperationToken(Cell.Operation),
		ReceiverToken(Cell.Receiver));
}

FString MakeEntryName(const FIndexedCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FPropIndexedParams ParamsFromCell(const FIndexedCell& Cell)
{
	FPropIndexedParams Params;
	Params.CandidateSet = Cell.CandidateSet;
	Params.IndexType = Cell.IndexType;
	Params.Operation = Cell.Operation;
	Params.Receiver = Cell.Receiver;
	return Params;
}

TArray<FIndexedCell> AllCells()
{
	TArray<FIndexedCell> Cells;
	Cells.SetNum(TotalCells);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		FIndexedCell& Cell = Cells[Index];
		Cell.CandidateSet = static_cast<EPropIndexedCandidateSet>(
			Index / (IndexTypeCount * OperationCount * ReceiverCount));
		Cell.IndexType = static_cast<EPropIndexedIndexType>(
			(Index / (OperationCount * ReceiverCount)) % IndexTypeCount);
		Cell.Operation = static_cast<EPropIndexedOperation>((Index / ReceiverCount) % OperationCount);
		Cell.Receiver = static_cast<EPropIndexedReceiver>(Index % ReceiverCount);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FIndexedCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FIndexedCell& Cell : AllCells())
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
	AppendLine(Source, TEXT("enum ERegisteredIndex"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tThree = 3"));
	AppendLine(Source, TEXT("}"));
}

void AppendEntry(FString& Source, const FPropIndexedParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\tconst int CandidateMarker = %d;"), CandidateMarker(Params.CandidateSet)));
	AppendLine(Source, Params.Receiver == EPropIndexedReceiver::Const
		? TEXT("\tconst FIndexedProperty Receiver = CreateIndexedProperty();")
		: TEXT("\tFIndexedProperty Receiver = CreateIndexedProperty();"));
	AppendLine(Source, FString::Printf(
		TEXT("\t%s Index = %s;"),
		ScriptType(Params.IndexType),
		ScriptLiteral(Params.IndexType)));
	if (Params.Operation == EPropIndexedOperation::Read)
	{
		AppendLine(Source, TEXT("\treturn Receiver.Value[Index];"));
	}
	else if (Params.Operation == EPropIndexedOperation::Write)
	{
		AppendLine(Source, TEXT("\tReceiver.Value[Index] = 73;"));
		AppendLine(Source, TEXT("\treturn ObserveIndexedProperty(Receiver);"));
	}
	else
	{
		AppendLine(Source, TEXT("\tReceiver.Value[Index] += 5;"));
		AppendLine(Source, TEXT("\treturn ObserveIndexedProperty(Receiver);"));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FPropIndexedParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FIndexedCell& Cell)
{
	const FPropIndexedParams Params = ParamsFromCell(Cell);
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Params))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.ExpectedReturn = ExpectedFromParams(Params);
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExecutionNotes = HostNotes;
	return Info;
}
}

FStringView FPropIndexedGenerator::GetProductId() const
{
	return TEXT("LANG-PROP-INDEXED");
}

TArray<FGeneratedCaseInfo> FPropIndexedGenerator::ListCases() const
{
	using namespace PropIndexedGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FIndexedCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("PropIndexed PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FPropIndexedGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace PropIndexedGeneratorDetail;
	FIndexedCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FPropIndexedGenerator::BuildDumpSource() const
{
	using namespace PropIndexedGeneratorDetail;
	const TArray<FIndexedCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-PROP-INDEXED"));
	AppendLine(Source, TEXT("// Cases: 468 total, 129 normal, 339 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	bool bNeedBlank = true;
	for (const FIndexedCell& Cell : Cells)
	{
		if (IsReject(ParamsFromCell(Cell)))
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
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}
	for (const FIndexedCell& Cell : Cells)
	{
		if (!IsReject(ParamsFromCell(Cell)))
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

FString FPropIndexedGenerator::BuildIndexedPropertySource(const FPropIndexedParams& Params) const
{
	using namespace PropIndexedGeneratorDetail;
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

FString FPropIndexedGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace PropIndexedGeneratorDetail;
	const TArray<FIndexedCell> Cells = AllCells();
	FString Source;
	AppendSharedHelpers(Source);
	int32 Emitted = 0;
	for (const FIndexedCell& Cell : Cells)
	{
		if (IsReject(ParamsFromCell(Cell)))
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

int32 FPropIndexedGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace PropIndexedGeneratorDetail;
	FIndexedCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(ParamsFromCell(Cell)))
	{
		return 0;
	}
	return ExpectedFromParams(ParamsFromCell(Cell));
}

FString FPropIndexedGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace PropIndexedGeneratorDetail;
	FIndexedCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(ParamsFromCell(Cell)))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FPropIndexedGenerator::ListRejectCaseIds() const
{
	using namespace PropIndexedGeneratorDetail;
	TArray<FString> Ids;
	for (const FIndexedCell& Cell : AllCells())
	{
		if (IsReject(ParamsFromCell(Cell)))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
