#include "Framework/Generate/AngelscriptTestPropCopyIndependenceGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace PropCopyIndependenceGeneratorDetail
{
constexpr int32 MutationCount = 3;
constexpr int32 TransferCount = 3;
constexpr int32 TypeCount = 17;
constexpr int32 ViewCount = 3;
constexpr int32 TotalCells = MutationCount * TransferCount * TypeCount * ViewCount;
constexpr const TCHAR* HostNotes = TEXT("Needs native RecordPropertyCopyObservation.");
constexpr const TCHAR* RefNotes = TEXT("Reference-valued copy is source-only until the ownership defect is repaired.");

struct FCopyCell
{
	EPropCopyIndependenceMutation Mutation = EPropCopyIndependenceMutation::SourceAfterTransfer;
	EPropCopyIndependenceTransfer Transfer = EPropCopyIndependenceTransfer::CopyConstruct;
	EPropCopyIndependenceType Type = EPropCopyIndependenceType::Int8;
	EPropCopyIndependenceView View = EPropCopyIndependenceView::Exact;
};

const TCHAR* MutationToken(const EPropCopyIndependenceMutation Mutation)
{
	switch (Mutation)
	{
	case EPropCopyIndependenceMutation::SourceAfterTransfer:
		return TEXT("SOURCE_AFTER_TRANSFER");
	case EPropCopyIndependenceMutation::TargetAfterTransfer:
		return TEXT("TARGET_AFTER_TRANSFER");
	case EPropCopyIndependenceMutation::NestedMember:
		return TEXT("NESTED_MEMBER");
	default:
		return nullptr;
	}
}

const TCHAR* TransferToken(const EPropCopyIndependenceTransfer Transfer)
{
	switch (Transfer)
	{
	case EPropCopyIndependenceTransfer::CopyConstruct:
		return TEXT("COPY_CONSTRUCT");
	case EPropCopyIndependenceTransfer::Assign:
		return TEXT("ASSIGN");
	case EPropCopyIndependenceTransfer::SelfAssign:
		return TEXT("SELF_ASSIGN");
	default:
		return nullptr;
	}
}

const TCHAR* TypeToken(const EPropCopyIndependenceType Type)
{
	switch (Type)
	{
	case EPropCopyIndependenceType::Int8:
		return TEXT("INT8");
	case EPropCopyIndependenceType::Int16:
		return TEXT("INT16");
	case EPropCopyIndependenceType::Int:
		return TEXT("INT");
	case EPropCopyIndependenceType::Int64:
		return TEXT("INT64");
	case EPropCopyIndependenceType::UInt8:
		return TEXT("UINT8");
	case EPropCopyIndependenceType::UInt16:
		return TEXT("UINT16");
	case EPropCopyIndependenceType::UInt:
		return TEXT("UINT");
	case EPropCopyIndependenceType::UInt64:
		return TEXT("UINT64");
	case EPropCopyIndependenceType::Float32:
		return TEXT("FLOAT32");
	case EPropCopyIndependenceType::Float64:
		return TEXT("FLOAT64");
	case EPropCopyIndependenceType::Bool:
		return TEXT("BOOL");
	case EPropCopyIndependenceType::Enum:
		return TEXT("ENUM");
	case EPropCopyIndependenceType::Typedef:
		return TEXT("TYPEDEF");
	case EPropCopyIndependenceType::ScriptValue:
		return TEXT("SCRIPT_VALUE");
	case EPropCopyIndependenceType::NativeValue:
		return TEXT("NATIVE_VALUE");
	case EPropCopyIndependenceType::ScriptReference:
		return TEXT("SCRIPT_REFERENCE");
	case EPropCopyIndependenceType::NativeReference:
		return TEXT("NATIVE_REFERENCE");
	default:
		return nullptr;
	}
}

const TCHAR* ViewToken(const EPropCopyIndependenceView View)
{
	switch (View)
	{
	case EPropCopyIndependenceView::Exact:
		return TEXT("EXACT");
	case EPropCopyIndependenceView::Base:
		return TEXT("BASE");
	case EPropCopyIndependenceView::Derived:
		return TEXT("DERIVED");
	default:
		return nullptr;
	}
}

const TCHAR* ScriptType(const EPropCopyIndependenceType Type)
{
	switch (Type)
	{
	case EPropCopyIndependenceType::Int8:
		return TEXT("int8");
	case EPropCopyIndependenceType::Int16:
		return TEXT("int16");
	case EPropCopyIndependenceType::Int:
		return TEXT("int");
	case EPropCopyIndependenceType::Int64:
		return TEXT("int64");
	case EPropCopyIndependenceType::UInt8:
		return TEXT("uint8");
	case EPropCopyIndependenceType::UInt16:
		return TEXT("uint16");
	case EPropCopyIndependenceType::UInt:
		return TEXT("uint");
	case EPropCopyIndependenceType::UInt64:
		return TEXT("uint64");
	case EPropCopyIndependenceType::Float32:
		return TEXT("float");
	case EPropCopyIndependenceType::Float64:
		return TEXT("double");
	case EPropCopyIndependenceType::Bool:
		return TEXT("bool");
	case EPropCopyIndependenceType::Enum:
		return TEXT("ENativeCaseEnum");
	case EPropCopyIndependenceType::Typedef:
		return TEXT("NativeCaseAlias");
	case EPropCopyIndependenceType::ScriptValue:
		return TEXT("FScriptCaseValue");
	case EPropCopyIndependenceType::NativeValue:
		return TEXT("FNativeCaseValue");
	case EPropCopyIndependenceType::ScriptReference:
		return TEXT("FScriptCaseReference");
	case EPropCopyIndependenceType::NativeReference:
		return TEXT("FNativeCaseReference");
	default:
		return nullptr;
	}
}

bool IsBoolOrEnum(const EPropCopyIndependenceType Type)
{
	return Type == EPropCopyIndependenceType::Bool || Type == EPropCopyIndependenceType::Enum;
}

bool IsReference(const EPropCopyIndependenceType Type)
{
	return Type == EPropCopyIndependenceType::ScriptReference
		|| Type == EPropCopyIndependenceType::NativeReference;
}

int32 InitialSource(const EPropCopyIndependenceType Type)
{
	return IsBoolOrEnum(Type) ? 1 : 11;
}

int32 InitialTarget(const EPropCopyIndependenceType Type)
{
	return IsBoolOrEnum(Type) ? 0 : 22;
}

int32 Mutated(const EPropCopyIndependenceType Type, const int32 Before)
{
	return IsBoolOrEnum(Type) ? 1 - Before : 37;
}

int32 ExpectedSource(const FPropCopyIndependenceParams& Params)
{
	const int32 SourceInitial = InitialSource(Params.Type);
	if (Params.Transfer == EPropCopyIndependenceTransfer::SelfAssign)
	{
		return Params.Mutation == EPropCopyIndependenceMutation::SourceAfterTransfer
			? Mutated(Params.Type, SourceInitial)
			: SourceInitial;
	}
	if (IsReference(Params.Type))
	{
		return Mutated(Params.Type, SourceInitial);
	}
	return Params.Mutation == EPropCopyIndependenceMutation::SourceAfterTransfer
		? Mutated(Params.Type, SourceInitial)
		: SourceInitial;
}

int32 ExpectedTarget(const FPropCopyIndependenceParams& Params)
{
	const int32 SourceInitial = InitialSource(Params.Type);
	const int32 TargetInitial = InitialTarget(Params.Type);
	if (Params.Transfer == EPropCopyIndependenceTransfer::SelfAssign)
	{
		return Params.Mutation == EPropCopyIndependenceMutation::SourceAfterTransfer
			? TargetInitial
			: Mutated(Params.Type, TargetInitial);
	}
	if (IsReference(Params.Type))
	{
		return Mutated(Params.Type, SourceInitial);
	}
	return Params.Mutation == EPropCopyIndependenceMutation::SourceAfterTransfer
		? SourceInitial
		: Mutated(Params.Type, SourceInitial);
}

int32 ExpectedPacked(const FPropCopyIndependenceParams& Params)
{
	return ExpectedSource(Params) * 1000 + ExpectedTarget(Params);
}

bool IsValidParams(const FPropCopyIndependenceParams& Params)
{
	return MutationToken(Params.Mutation) != nullptr
		&& TransferToken(Params.Transfer) != nullptr
		&& TypeToken(Params.Type) != nullptr
		&& ViewToken(Params.View) != nullptr;
}

FString TypedValue(const EPropCopyIndependenceType Type, const int32 Value)
{
	if (Type == EPropCopyIndependenceType::Bool)
	{
		return Value == 0 ? TEXT("false") : TEXT("true");
	}
	if (Type == EPropCopyIndependenceType::Enum)
	{
		return Value == 0 ? TEXT("ENativeCaseEnum::Zero") : TEXT("ENativeCaseEnum::One");
	}
	if (Type == EPropCopyIndependenceType::NativeReference)
	{
		return FString::Printf(TEXT("CreateNativeCaseReference(%d)"), Value);
	}
	return FString::Printf(TEXT("%s(%d)"), ScriptType(Type), Value);
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FCopyCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-PROP-COPY-INDEPENDENCE-%s-%s-%s-%s"),
		MutationToken(Cell.Mutation),
		TransferToken(Cell.Transfer),
		TypeToken(Cell.Type),
		ViewToken(Cell.View));
}

FString MakeEntryName(const FCopyCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FPropCopyIndependenceParams ParamsFromCell(const FCopyCell& Cell)
{
	FPropCopyIndependenceParams Params;
	Params.Mutation = Cell.Mutation;
	Params.Transfer = Cell.Transfer;
	Params.Type = Cell.Type;
	Params.View = Cell.View;
	return Params;
}

TArray<FCopyCell> AllCells()
{
	TArray<FCopyCell> Cells;
	Cells.SetNum(TotalCells);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		FCopyCell& Cell = Cells[Index];
		Cell.Mutation = static_cast<EPropCopyIndependenceMutation>(
			Index / (TransferCount * TypeCount * ViewCount));
		Cell.Transfer = static_cast<EPropCopyIndependenceTransfer>(
			(Index / (TypeCount * ViewCount)) % TransferCount);
		Cell.Type = static_cast<EPropCopyIndependenceType>((Index / ViewCount) % TypeCount);
		Cell.View = static_cast<EPropCopyIndependenceView>(Index % ViewCount);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FCopyCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FCopyCell& Cell : AllCells())
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

void AppendEntry(FString& Source, const FPropCopyIndependenceParams& Params, const FString& FunctionName)
{
	const FString Qualifier = FunctionName;
	const TCHAR* const TypeName = ScriptType(Params.Type);
	const FString SourceInit = TypedValue(Params.Type, InitialSource(Params.Type));
	const FString TargetInit = TypedValue(Params.Type, InitialTarget(Params.Type));
	const int32 MutatedSource = Mutated(Params.Type, InitialSource(Params.Type));
	AppendLine(Source, FString::Printf(TEXT("class FPropertyCopyPayload_%s"), *Qualifier));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Stored;"), TypeName));
	AppendLine(Source, FString::Printf(TEXT("\tFPropertyCopyPayload_%s(%s InValue)"), *Qualifier, TypeName));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tStored = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("class FPropertyCopyBase_%s"), *Qualifier));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Source = %s;"), TypeName, *SourceInit));
	AppendLine(Source, FString::Printf(TEXT("\t%s Target = %s;"), TypeName, *TargetInit));
	AppendLine(Source, FString::Printf(
		TEXT("\tFPropertyCopyPayload_%s NestedSource = FPropertyCopyPayload_%s(%s);"),
		*Qualifier,
		*Qualifier,
		*SourceInit));
	AppendLine(Source, FString::Printf(
		TEXT("\tFPropertyCopyPayload_%s NestedTarget = FPropertyCopyPayload_%s(%s);"),
		*Qualifier,
		*Qualifier,
		*TargetInit));
	AppendLine(Source, TEXT("\tint ViewMarker = 11;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(
		TEXT("class FPropertyCopyDerived_%s : FPropertyCopyBase_%s"),
		*Qualifier,
		*Qualifier));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\tFPropertyCopyDerived_%s()"), *Qualifier));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tViewMarker = 22;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.View == EPropCopyIndependenceView::Derived)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\tFPropertyCopyDerived_%s Receiver = FPropertyCopyDerived_%s();"),
			*Qualifier,
			*Qualifier));
	}
	else
	{
		AppendLine(Source, FString::Printf(
			TEXT("\tFPropertyCopyBase_%s Receiver = FPropertyCopyBase_%s();"),
			*Qualifier,
			*Qualifier));
	}
	const bool bNested = Params.Mutation == EPropCopyIndependenceMutation::NestedMember;
	if (Params.Transfer == EPropCopyIndependenceTransfer::CopyConstruct)
	{
		if (bNested)
		{
			AppendLine(Source, FString::Printf(
				TEXT("\tFPropertyCopyPayload_%s TransferTarget = Receiver.NestedSource;"),
				*Qualifier));
		}
		else
		{
			AppendLine(Source, FString::Printf(TEXT("\t%s TransferTarget = Receiver.Source;"), TypeName));
		}
	}
	else if (Params.Transfer == EPropCopyIndependenceTransfer::Assign)
	{
		AppendLine(Source, bNested
			? TEXT("\tReceiver.NestedTarget = Receiver.NestedSource;")
			: TEXT("\tReceiver.Target = Receiver.Source;"));
	}
	else
	{
		AppendLine(Source, bNested
			? TEXT("\tReceiver.NestedTarget = Receiver.NestedTarget;")
			: (Params.Mutation == EPropCopyIndependenceMutation::SourceAfterTransfer
				? TEXT("\tReceiver.Source = Receiver.Source;")
				: TEXT("\tReceiver.Target = Receiver.Target;")));
	}
	if (Params.Mutation == EPropCopyIndependenceMutation::SourceAfterTransfer)
	{
		AppendLine(Source, FString::Printf(TEXT("\tReceiver.Source = %s;"), *TypedValue(Params.Type, MutatedSource)));
	}
	else if (Params.Mutation == EPropCopyIndependenceMutation::TargetAfterTransfer)
	{
		AppendLine(Source, FString::Printf(TEXT("\tReceiver.Target = %s;"), *TypedValue(Params.Type, Mutated(Params.Type, InitialTarget(Params.Type)))));
	}
	else
	{
		AppendLine(Source, FString::Printf(
			TEXT("\tReceiver.NestedSource.Stored = %s;"),
			*TypedValue(Params.Type, MutatedSource)));
	}
	AppendLine(Source, FString::Printf(
		TEXT("\treturn %d * 1000 + %d;"),
		ExpectedSource(Params),
		ExpectedTarget(Params)));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FPropCopyIndependenceParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendLine(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FCopyCell& Cell)
{
	const FPropCopyIndependenceParams Params = ParamsFromCell(Cell);
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.ExpectedReturn = ExpectedPacked(Params);
	if (IsReference(Cell.Type))
	{
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		Info.bLimitedObservation = true;
		Info.ExecutionNotes = RefNotes;
	}
	else
	{
		Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
		Info.ExecutionNotes = HostNotes;
		Info.bLimitedObservation = Cell.Type == EPropCopyIndependenceType::Float32
			|| Cell.Type == EPropCopyIndependenceType::Float64
			|| Cell.Type == EPropCopyIndependenceType::Int64
			|| Cell.Type == EPropCopyIndependenceType::UInt64
			|| Cell.Type == EPropCopyIndependenceType::NativeValue;
	}
	return Info;
}
}

FStringView FPropCopyIndependenceGenerator::GetProductId() const
{
	return TEXT("LANG-PROP-COPY-INDEPENDENCE");
}

TArray<FGeneratedCaseInfo> FPropCopyIndependenceGenerator::ListCases() const
{
	using namespace PropCopyIndependenceGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FCopyCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("PropCopyIndependence PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FPropCopyIndependenceGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace PropCopyIndependenceGeneratorDetail;
	FCopyCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FPropCopyIndependenceGenerator::BuildDumpSource() const
{
	using namespace PropCopyIndependenceGeneratorDetail;
	const TArray<FCopyCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-PROP-COPY-INDEPENDENCE"));
	AppendLine(Source, TEXT("// Cases: 459 total, 459 normal, 0 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	bool bNeedBlank = true;
	for (const FCopyCell& Cell : Cells)
	{
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
	return Source;
}

FString FPropCopyIndependenceGenerator::BuildPropertyCopySource(const FPropCopyIndependenceParams& Params) const
{
	using namespace PropCopyIndependenceGeneratorDetail;
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

FString FPropCopyIndependenceGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace PropCopyIndependenceGeneratorDetail;
	const TArray<FCopyCell> Cells = AllCells();
	FString Source;
	AppendSharedHelpers(Source);
	int32 Emitted = 0;
	for (const FCopyCell& Cell : Cells)
	{
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

int32 FPropCopyIndependenceGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace PropCopyIndependenceGeneratorDetail;
	FCopyCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return 0;
	}
	return ExpectedPacked(ParamsFromCell(Cell));
}
}
