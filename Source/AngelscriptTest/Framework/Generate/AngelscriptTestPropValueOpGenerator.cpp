#include "Framework/Generate/AngelscriptTestPropValueOpGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace PropValueOpGeneratorDetail
{
constexpr int32 OperationCount = 5;
constexpr int32 ReceiverCount = 4;
constexpr int32 TypeCount = 15;
constexpr int32 TotalCells = OperationCount * ReceiverCount * TypeCount;
constexpr const TCHAR* HostNotes = TEXT("Needs native NativeCaseAlias or FNativeCaseValue.");

struct FValueOpCell
{
	EPropValueOpOperation Operation = EPropValueOpOperation::DefaultRead;
	EPropValueOpReceiver Receiver = EPropValueOpReceiver::Mutable;
	EPropValueOpType Type = EPropValueOpType::Int8;
};

const TCHAR* OperationToken(const EPropValueOpOperation Operation)
{
	switch (Operation)
	{
	case EPropValueOpOperation::DefaultRead:
		return TEXT("DEFAULT_READ");
	case EPropValueOpOperation::Write:
		return TEXT("WRITE");
	case EPropValueOpOperation::CompoundWrite:
		return TEXT("COMPOUND_WRITE");
	case EPropValueOpOperation::Copy:
		return TEXT("COPY");
	case EPropValueOpOperation::ReferenceMutation:
		return TEXT("REFERENCE_MUTATION");
	default:
		return nullptr;
	}
}

const TCHAR* ReceiverToken(const EPropValueOpReceiver Receiver)
{
	switch (Receiver)
	{
	case EPropValueOpReceiver::Mutable:
		return TEXT("MUTABLE");
	case EPropValueOpReceiver::Const:
		return TEXT("CONST");
	case EPropValueOpReceiver::BaseView:
		return TEXT("BASE_VIEW");
	case EPropValueOpReceiver::DerivedView:
		return TEXT("DERIVED_VIEW");
	default:
		return nullptr;
	}
}

const TCHAR* TypeToken(const EPropValueOpType Type)
{
	switch (Type)
	{
	case EPropValueOpType::Int8:
		return TEXT("INT8");
	case EPropValueOpType::Int16:
		return TEXT("INT16");
	case EPropValueOpType::Int:
		return TEXT("INT");
	case EPropValueOpType::Int64:
		return TEXT("INT64");
	case EPropValueOpType::UInt8:
		return TEXT("UINT8");
	case EPropValueOpType::UInt16:
		return TEXT("UINT16");
	case EPropValueOpType::UInt:
		return TEXT("UINT");
	case EPropValueOpType::UInt64:
		return TEXT("UINT64");
	case EPropValueOpType::Float32:
		return TEXT("FLOAT32");
	case EPropValueOpType::Float64:
		return TEXT("FLOAT64");
	case EPropValueOpType::Bool:
		return TEXT("BOOL");
	case EPropValueOpType::Enum:
		return TEXT("ENUM");
	case EPropValueOpType::Typedef:
		return TEXT("TYPEDEF");
	case EPropValueOpType::ScriptValue:
		return TEXT("SCRIPT_VALUE");
	case EPropValueOpType::NativeValue:
		return TEXT("NATIVE_VALUE");
	default:
		return nullptr;
	}
}

const TCHAR* ScriptType(const EPropValueOpType Type)
{
	switch (Type)
	{
	case EPropValueOpType::Int8:
		return TEXT("int8");
	case EPropValueOpType::Int16:
		return TEXT("int16");
	case EPropValueOpType::Int:
		return TEXT("int");
	case EPropValueOpType::Int64:
		return TEXT("int64");
	case EPropValueOpType::UInt8:
		return TEXT("uint8");
	case EPropValueOpType::UInt16:
		return TEXT("uint16");
	case EPropValueOpType::UInt:
		return TEXT("uint");
	case EPropValueOpType::UInt64:
		return TEXT("uint64");
	case EPropValueOpType::Float32:
		return TEXT("float");
	case EPropValueOpType::Float64:
		return TEXT("double");
	case EPropValueOpType::Bool:
		return TEXT("bool");
	case EPropValueOpType::Enum:
		return TEXT("ENativeCaseEnum");
	case EPropValueOpType::Typedef:
		return TEXT("NativeCaseAlias");
	case EPropValueOpType::ScriptValue:
		return TEXT("FScriptCaseValue");
	case EPropValueOpType::NativeValue:
		return TEXT("FNativeCaseValue");
	default:
		return nullptr;
	}
}

bool IsBoolOrEnum(const EPropValueOpType Type)
{
	return Type == EPropValueOpType::Bool || Type == EPropValueOpType::Enum;
}

bool SupportsCompound(const EPropValueOpType Type)
{
	return Type != EPropValueOpType::Bool
		&& Type != EPropValueOpType::Enum
		&& Type != EPropValueOpType::ScriptValue
		&& Type != EPropValueOpType::NativeValue;
}

bool IsReject(const FPropValueOpParams& Params)
{
	if (Params.Receiver == EPropValueOpReceiver::Const
		&& Params.Operation != EPropValueOpOperation::DefaultRead
		&& Params.Operation != EPropValueOpOperation::Copy)
	{
		return true;
	}
	return Params.Operation == EPropValueOpOperation::CompoundWrite && !SupportsCompound(Params.Type);
}

int32 ExpectedCore(const EPropValueOpType Type, const EPropValueOpOperation Operation)
{
	if (Operation == EPropValueOpOperation::DefaultRead)
	{
		return 0;
	}
	if (Operation == EPropValueOpOperation::Copy)
	{
		return IsBoolOrEnum(Type) ? 1 : 37;
	}
	return IsBoolOrEnum(Type) ? 1 : 29;
}

int32 ReceiverMarker(const EPropValueOpReceiver Receiver)
{
	return (Receiver == EPropValueOpReceiver::BaseView || Receiver == EPropValueOpReceiver::DerivedView) ? 22 : 11;
}

int32 ExpectedFromParams(const FPropValueOpParams& Params)
{
	return ExpectedCore(Params.Type, Params.Operation) * 100 + ReceiverMarker(Params.Receiver);
}

bool IsValidParams(const FPropValueOpParams& Params)
{
	return OperationToken(Params.Operation) != nullptr
		&& ReceiverToken(Params.Receiver) != nullptr
		&& TypeToken(Params.Type) != nullptr;
}

FString TypedValue(const EPropValueOpType Type, const int32 Value)
{
	if (Type == EPropValueOpType::Bool)
	{
		return Value == 0 ? TEXT("false") : TEXT("true");
	}
	if (Type == EPropValueOpType::Enum)
	{
		return Value == 0 ? TEXT("ENativeCaseEnum::Zero") : TEXT("ENativeCaseEnum::One");
	}
	return FString::Printf(TEXT("%s(%d)"), ScriptType(Type), Value);
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FValueOpCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-PROP-VALUE-OP-%s-%s-%s"),
		OperationToken(Cell.Operation),
		ReceiverToken(Cell.Receiver),
		TypeToken(Cell.Type));
}

FString MakeEntryName(const FValueOpCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FPropValueOpParams ParamsFromCell(const FValueOpCell& Cell)
{
	FPropValueOpParams Params;
	Params.Operation = Cell.Operation;
	Params.Receiver = Cell.Receiver;
	Params.Type = Cell.Type;
	return Params;
}

TArray<FValueOpCell> AllCells()
{
	TArray<FValueOpCell> Cells;
	Cells.SetNum(TotalCells);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		FValueOpCell& Cell = Cells[Index];
		Cell.Operation = static_cast<EPropValueOpOperation>(Index / (ReceiverCount * TypeCount));
		Cell.Receiver = static_cast<EPropValueOpReceiver>((Index / TypeCount) % ReceiverCount);
		Cell.Type = static_cast<EPropValueOpType>(Index % TypeCount);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FValueOpCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FValueOpCell& Cell : AllCells())
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
	AppendLine(Source, TEXT("\tZero = 0,"));
	AppendLine(Source, TEXT("\tOne = 1"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FScriptCaseValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source, TEXT("\tFScriptCaseValue(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
}

void AppendTypeHelpers(FString& Source, const FPropValueOpParams& Params, const FString& Qualifier)
{
	const TCHAR* const TypeName = ScriptType(Params.Type);
	AppendLine(Source, FString::Printf(TEXT("int ObserveStoredProperty_%s(%s Value)"), *Qualifier, TypeName));
	AppendLine(Source, TEXT("{"));
	if (Params.Type == EPropValueOpType::Bool)
	{
		AppendLine(Source, TEXT("\treturn Value ? 1 : 0;"));
	}
	else if (Params.Type == EPropValueOpType::ScriptValue || Params.Type == EPropValueOpType::NativeValue)
	{
		AppendLine(Source, TEXT("\treturn Value.Value;"));
	}
	else
	{
		AppendLine(Source, TEXT("\treturn int(Value);"));
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("class FStoredPropertyBase_%s"), *Qualifier));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Stored;"), TypeName));
	AppendLine(Source, TEXT("\tint ReceiverMarker = 11;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(
		TEXT("class FStoredPropertyDerived_%s : FStoredPropertyBase_%s"),
		*Qualifier,
		*Qualifier));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\tFStoredPropertyDerived_%s()"), *Qualifier));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tReceiverMarker = 22;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	if (Params.Operation == EPropValueOpOperation::ReferenceMutation)
	{
		AppendLine(Source, FString::Printf(TEXT("void MutateStoredProperty_%s(%s& inout Value)"), *Qualifier, TypeName));
		AppendLine(Source, TEXT("{"));
		if (Params.Type == EPropValueOpType::ScriptValue || Params.Type == EPropValueOpType::NativeValue)
		{
			AppendLine(Source, FString::Printf(TEXT("\tValue.Value = %d;"), ExpectedCore(Params.Type, Params.Operation)));
		}
		else
		{
			AppendLine(Source, FString::Printf(TEXT("\tValue = %s;"), *TypedValue(Params.Type, ExpectedCore(Params.Type, Params.Operation))));
		}
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
}

void AppendEntry(FString& Source, const FPropValueOpParams& Params, const FString& FunctionName)
{
	const FString Qualifier = FunctionName;
	AppendTypeHelpers(Source, Params, Qualifier);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Receiver == EPropValueOpReceiver::Mutable)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFStoredPropertyBase_%s Receiver = FStoredPropertyBase_%s();"), *Qualifier, *Qualifier));
	}
	else if (Params.Receiver == EPropValueOpReceiver::Const)
	{
		AppendLine(Source, FString::Printf(TEXT("\tconst FStoredPropertyBase_%s Receiver = FStoredPropertyBase_%s();"), *Qualifier, *Qualifier));
	}
	else if (Params.Receiver == EPropValueOpReceiver::BaseView)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFStoredPropertyDerived_%s DerivedReceiver = FStoredPropertyDerived_%s();"), *Qualifier, *Qualifier));
		AppendLine(Source, FString::Printf(TEXT("\tFStoredPropertyBase_%s Receiver = DerivedReceiver;"), *Qualifier));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\tFStoredPropertyDerived_%s Receiver = FStoredPropertyDerived_%s();"), *Qualifier, *Qualifier));
	}

	const int32 Core = ExpectedCore(Params.Type, Params.Operation);
	if (Params.Operation == EPropValueOpOperation::Write)
	{
		AppendLine(Source, FString::Printf(TEXT("\tReceiver.Stored = %s;"), *TypedValue(Params.Type, Core)));
	}
	else if (Params.Operation == EPropValueOpOperation::CompoundWrite)
	{
		AppendLine(Source, FString::Printf(TEXT("\tReceiver.Stored += %s;"), *TypedValue(Params.Type, Core)));
	}
	else if (Params.Operation == EPropValueOpOperation::Copy)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s CopyValue = Receiver.Stored;"), ScriptType(Params.Type)));
		if (Params.Type == EPropValueOpType::ScriptValue || Params.Type == EPropValueOpType::NativeValue)
		{
			AppendLine(Source, FString::Printf(TEXT("\tCopyValue.Value = %d;"), Core));
		}
		else
		{
			AppendLine(Source, FString::Printf(TEXT("\tCopyValue = %s;"), *TypedValue(Params.Type, Core)));
		}
	}
	else if (Params.Operation == EPropValueOpOperation::ReferenceMutation)
	{
		AppendLine(Source, FString::Printf(TEXT("\tMutateStoredProperty_%s(Receiver.Stored);"), *Qualifier));
	}

	if (Params.Operation == EPropValueOpOperation::Copy)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\tint CoreResult = ObserveStoredProperty_%s(Receiver.Stored) * 100 + ObserveStoredProperty_%s(CopyValue);"),
			*Qualifier,
			*Qualifier));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\tint CoreResult = ObserveStoredProperty_%s(Receiver.Stored);"), *Qualifier));
	}
	AppendLine(Source, TEXT("\treturn CoreResult * 100 + Receiver.ReceiverMarker;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FPropValueOpParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FValueOpCell& Cell)
{
	const FPropValueOpParams Params = ParamsFromCell(Cell);
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
	if (Cell.Type == EPropValueOpType::Typedef || Cell.Type == EPropValueOpType::NativeValue)
	{
		Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
		Info.ExecutionNotes = HostNotes;
		Info.bLimitedObservation = true;
	}
	else if (Cell.Type == EPropValueOpType::Float32 || Cell.Type == EPropValueOpType::Float64
		|| Cell.Type == EPropValueOpType::Int64 || Cell.Type == EPropValueOpType::UInt64)
	{
		Info.Execution = EGeneratedCaseExecution::Standalone;
		Info.bLimitedObservation = true;
		Info.ExecutionNotes = TEXT("Int32 observation is a type marker, not exact wide/float arithmetic.");
	}
	else
	{
		Info.Execution = EGeneratedCaseExecution::Standalone;
	}
	return Info;
}
}

FStringView FPropValueOpGenerator::GetProductId() const
{
	return TEXT("LANG-PROP-VALUE-OP");
}

TArray<FGeneratedCaseInfo> FPropValueOpGenerator::ListCases() const
{
	using namespace PropValueOpGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FValueOpCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("PropValueOp PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FPropValueOpGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace PropValueOpGeneratorDetail;
	FValueOpCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FPropValueOpGenerator::BuildDumpSource() const
{
	using namespace PropValueOpGeneratorDetail;
	const TArray<FValueOpCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-PROP-VALUE-OP"));
	AppendLine(Source, TEXT("// Cases: 300 total, 243 normal, 57 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	bool bNeedBlank = true;
	for (const FValueOpCell& Cell : Cells)
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
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}
	for (const FValueOpCell& Cell : Cells)
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

FString FPropValueOpGenerator::BuildStoredPropertySource(const FPropValueOpParams& Params) const
{
	using namespace PropValueOpGeneratorDetail;
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

FString FPropValueOpGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace PropValueOpGeneratorDetail;
	const TArray<FValueOpCell> Cells = AllCells();
	FString Source;
	AppendSharedHelpers(Source);
	int32 Emitted = 0;
	for (const FValueOpCell& Cell : Cells)
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

int32 FPropValueOpGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace PropValueOpGeneratorDetail;
	FValueOpCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(ParamsFromCell(Cell)))
	{
		return 0;
	}
	return ExpectedFromParams(ParamsFromCell(Cell));
}

FString FPropValueOpGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace PropValueOpGeneratorDetail;
	FValueOpCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(ParamsFromCell(Cell)))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FPropValueOpGenerator::ListRejectCaseIds() const
{
	using namespace PropValueOpGeneratorDetail;
	TArray<FString> Ids;
	for (const FValueOpCell& Cell : AllCells())
	{
		if (IsReject(ParamsFromCell(Cell)))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
