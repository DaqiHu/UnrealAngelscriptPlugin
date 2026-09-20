#include "Framework/Generate/AngelscriptTestCtorParamSelectGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace CtorParamSelectGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native constructor-parameter host markers.");

struct FCtorParamSelectCell
{
	ECtorParamSelectArity Arity = ECtorParamSelectArity::One;
	ECtorParamSelectSelection Selection = ECtorParamSelectSelection::Exact;
	ECtorParamSelectType Type = ECtorParamSelectType::Int8;
};

const TCHAR* ArityToken(const ECtorParamSelectArity Arity)
{
	switch (Arity)
	{
	case ECtorParamSelectArity::One:
		return TEXT("ONE");
	case ECtorParamSelectArity::Two:
		return TEXT("TWO");
	case ECtorParamSelectArity::Five:
		return TEXT("FIVE");
	case ECtorParamSelectArity::Sixteen:
		return TEXT("SIXTEEN");
	default:
		return nullptr;
	}
}

const TCHAR* SelectionToken(const ECtorParamSelectSelection Selection)
{
	switch (Selection)
	{
	case ECtorParamSelectSelection::Exact:
		return TEXT("EXACT");
	case ECtorParamSelectSelection::Promotion:
		return TEXT("PROMOTION");
	case ECtorParamSelectSelection::ExplicitConversion:
		return TEXT("EXPLICIT_CONVERSION");
	case ECtorParamSelectSelection::Ambiguous:
		return TEXT("AMBIGUOUS");
	case ECtorParamSelectSelection::Missing:
		return TEXT("MISSING");
	default:
		return nullptr;
	}
}

const TCHAR* TypeToken(const ECtorParamSelectType Type)
{
	switch (Type)
	{
	case ECtorParamSelectType::Int8:
		return TEXT("INT8");
	case ECtorParamSelectType::Int16:
		return TEXT("INT16");
	case ECtorParamSelectType::Int:
		return TEXT("INT");
	case ECtorParamSelectType::Int64:
		return TEXT("INT64");
	case ECtorParamSelectType::Uint8:
		return TEXT("UINT8");
	case ECtorParamSelectType::Uint16:
		return TEXT("UINT16");
	case ECtorParamSelectType::Uint:
		return TEXT("UINT");
	case ECtorParamSelectType::Uint64:
		return TEXT("UINT64");
	case ECtorParamSelectType::Float32:
		return TEXT("FLOAT32");
	case ECtorParamSelectType::Float64:
		return TEXT("FLOAT64");
	case ECtorParamSelectType::Bool:
		return TEXT("BOOL");
	case ECtorParamSelectType::Enum:
		return TEXT("ENUM");
	case ECtorParamSelectType::Typedef:
		return TEXT("TYPEDEF");
	case ECtorParamSelectType::ScriptValue:
		return TEXT("SCRIPT_VALUE");
	case ECtorParamSelectType::NativeValue:
		return TEXT("NATIVE_VALUE");
	default:
		return nullptr;
	}
}

const TCHAR* ScriptType(const ECtorParamSelectType Type)
{
	switch (Type)
	{
	case ECtorParamSelectType::Int8:
		return TEXT("int8");
	case ECtorParamSelectType::Int16:
		return TEXT("int16");
	case ECtorParamSelectType::Int:
		return TEXT("int");
	case ECtorParamSelectType::Int64:
		return TEXT("int64");
	case ECtorParamSelectType::Uint8:
		return TEXT("uint8");
	case ECtorParamSelectType::Uint16:
		return TEXT("uint16");
	case ECtorParamSelectType::Uint:
		return TEXT("uint");
	case ECtorParamSelectType::Uint64:
		return TEXT("uint64");
	case ECtorParamSelectType::Float32:
		return TEXT("float");
	case ECtorParamSelectType::Float64:
		return TEXT("double");
	case ECtorParamSelectType::Bool:
		return TEXT("bool");
	case ECtorParamSelectType::Enum:
		return TEXT("ENativeCaseEnum");
	case ECtorParamSelectType::Typedef:
		return TEXT("NativeCaseAlias");
	case ECtorParamSelectType::ScriptValue:
		return TEXT("FScriptCaseValue");
	case ECtorParamSelectType::NativeValue:
		return TEXT("FNativeCaseValue");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FCtorParamSelectParams& Params)
{
	return ArityToken(Params.Arity) != nullptr
		&& SelectionToken(Params.Selection) != nullptr
		&& TypeToken(Params.Type) != nullptr;
}

bool SupportsPromotion(const ECtorParamSelectType Type)
{
	switch (Type)
	{
	case ECtorParamSelectType::Int8:
	case ECtorParamSelectType::Int16:
	case ECtorParamSelectType::Int:
	case ECtorParamSelectType::Uint8:
	case ECtorParamSelectType::Uint16:
	case ECtorParamSelectType::Uint:
	case ECtorParamSelectType::Float32:
	case ECtorParamSelectType::Typedef:
		return true;
	default:
		return false;
	}
}

bool IsReject(const ECtorParamSelectSelection Selection, const ECtorParamSelectType Type)
{
	if (Selection == ECtorParamSelectSelection::Ambiguous || Selection == ECtorParamSelectSelection::Missing)
	{
		return true;
	}
	return Selection == ECtorParamSelectSelection::Promotion && !SupportsPromotion(Type);
}

bool IsObjectValue(const ECtorParamSelectType Type)
{
	return Type == ECtorParamSelectType::ScriptValue || Type == ECtorParamSelectType::NativeValue;
}

int32 ArityCount(const ECtorParamSelectArity Arity)
{
	switch (Arity)
	{
	case ECtorParamSelectArity::Two:
		return 2;
	case ECtorParamSelectArity::Five:
		return 5;
	case ECtorParamSelectArity::Sixteen:
		return 16;
	default:
		return 1;
	}
}

int32 ExpectedArgumentValue(const ECtorParamSelectType Type, const int32 Index)
{
	return Type == ECtorParamSelectType::Bool ? ((Index % 2 == 0) ? 1 : 0) : Index + 1;
}

int32 ExpectedChecksum(const ECtorParamSelectType Type, const ECtorParamSelectArity Arity)
{
	int32 Result = 0;
	const int32 Count = ArityCount(Arity);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Result += ExpectedArgumentValue(Type, Index);
	}
	return Result;
}

const TCHAR* PromotionType(const ECtorParamSelectType Type)
{
	switch (Type)
	{
	case ECtorParamSelectType::Int8:
		return TEXT("int16");
	case ECtorParamSelectType::Int16:
		return TEXT("int");
	case ECtorParamSelectType::Int:
		return TEXT("int64");
	case ECtorParamSelectType::Uint8:
		return TEXT("uint16");
	case ECtorParamSelectType::Uint16:
		return TEXT("uint");
	case ECtorParamSelectType::Uint:
		return TEXT("uint64");
	case ECtorParamSelectType::Float32:
		return TEXT("double");
	case ECtorParamSelectType::Typedef:
		return TEXT("int64");
	default:
		return TEXT("FNoPromotion");
	}
}

const TCHAR* ExplicitTargetType(const ECtorParamSelectType Type)
{
	switch (Type)
	{
	case ECtorParamSelectType::Int8:
		return TEXT("uint8");
	case ECtorParamSelectType::Int16:
		return TEXT("uint16");
	case ECtorParamSelectType::Int:
		return TEXT("uint");
	case ECtorParamSelectType::Int64:
		return TEXT("uint64");
	case ECtorParamSelectType::Uint8:
		return TEXT("int8");
	case ECtorParamSelectType::Uint16:
		return TEXT("int16");
	case ECtorParamSelectType::Uint:
		return TEXT("int");
	case ECtorParamSelectType::Uint64:
		return TEXT("int64");
	case ECtorParamSelectType::Float64:
	case ECtorParamSelectType::Typedef:
		return TEXT("int64");
	default:
		return TEXT("int");
	}
}

const TCHAR* CandidateType(const ECtorParamSelectType Type, const ECtorParamSelectSelection Selection)
{
	if (Selection == ECtorParamSelectSelection::Exact || Selection == ECtorParamSelectSelection::Ambiguous)
	{
		return ScriptType(Type);
	}
	if (Selection == ECtorParamSelectSelection::Promotion)
	{
		return PromotionType(Type);
	}
	if (Selection == ECtorParamSelectSelection::ExplicitConversion)
	{
		return ExplicitTargetType(Type);
	}
	return TEXT("FNoMatch");
}

int32 SelectionMarker(const ECtorParamSelectSelection Selection)
{
	switch (Selection)
	{
	case ECtorParamSelectSelection::Exact:
		return 101;
	case ECtorParamSelectSelection::Promotion:
		return 201;
	case ECtorParamSelectSelection::ExplicitConversion:
		return 301;
	case ECtorParamSelectSelection::Ambiguous:
		return 401;
	default:
		return 501;
	}
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FCtorParamSelectCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CTOR-PARAM-SELECT-%s-%s-%s"),
		ArityToken(Cell.Arity),
		SelectionToken(Cell.Selection),
		TypeToken(Cell.Type));
}

FString MakeEntryName(const FCtorParamSelectCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FString Compact(const TCHAR* Token)
{
	FString Text(Token);
	Text.ReplaceInline(TEXT("_"), TEXT(""));
	return Text;
}

FString ProbeName(const FCtorParamSelectCell& Cell)
{
	return FString::Printf(
		TEXT("FConstructorProbe%s%s%s"),
		*Compact(ArityToken(Cell.Arity)),
		*Compact(SelectionToken(Cell.Selection)),
		*Compact(TypeToken(Cell.Type)));
}

FCtorParamSelectParams ParamsFromCell(const FCtorParamSelectCell& Cell)
{
	FCtorParamSelectParams Params;
	Params.Arity = Cell.Arity;
	Params.Selection = Cell.Selection;
	Params.Type = Cell.Type;
	return Params;
}

TArray<FCtorParamSelectCell> AllCells()
{
	TArray<FCtorParamSelectCell> Cells;
	Cells.SetNum(300);
	for (int32 Index = 0; Index < 300; ++Index)
	{
		FCtorParamSelectCell& Cell = Cells[Index];
		Cell.Arity = static_cast<ECtorParamSelectArity>(Index / 75);
		Cell.Selection = static_cast<ECtorParamSelectSelection>((Index / 15) % 5);
		Cell.Type = static_cast<ECtorParamSelectType>(Index % 15);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FCtorParamSelectCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FCtorParamSelectCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

FString SourceLiteral(const ECtorParamSelectType Type, const int32 Index)
{
	const int32 Value = ExpectedArgumentValue(Type, Index);
	if (Type == ECtorParamSelectType::Bool)
	{
		return Value != 0 ? FString(TEXT("true")) : FString(TEXT("false"));
	}
	if (Type == ECtorParamSelectType::Enum)
	{
		return FString::Printf(TEXT("ENativeCaseEnum::Value%d"), Value);
	}
	return FString::Printf(TEXT("%s(%d)"), ScriptType(Type), Value);
}

FString ReadExpression(const TCHAR* Candidate, const FString& Expression)
{
	if (FCString::Strcmp(Candidate, TEXT("FNoPromotion")) == 0
		|| FCString::Strcmp(Candidate, TEXT("FNoMatch")) == 0
		|| FCString::Strcmp(Candidate, TEXT("FScriptCaseValue")) == 0
		|| FCString::Strcmp(Candidate, TEXT("FNativeCaseValue")) == 0)
	{
		return Expression + TEXT(".Value");
	}
	if (FCString::Strcmp(Candidate, TEXT("bool")) == 0)
	{
		return FString::Printf(TEXT("(%s ? 1 : 0)"), *Expression);
	}
	return FString::Printf(TEXT("int(%s)"), *Expression);
}

FString ObservedArgument(const FCtorParamSelectParams& Params, const int32 Index)
{
	const FString Observed = FString::Printf(
		TEXT("Observe%s(%d, %s)"),
		*Compact(TypeToken(Params.Type)),
		Index,
		*SourceLiteral(Params.Type, Index));
	if (Params.Selection != ECtorParamSelectSelection::ExplicitConversion)
	{
		return Observed;
	}
	const TCHAR* const Target = ExplicitTargetType(Params.Type);
	if (IsObjectValue(Params.Type))
	{
		return FString::Printf(TEXT("%s((%s).Value)"), Target, *Observed);
	}
	return FString::Printf(TEXT("%s(%s)"), Target, *Observed);
}

void AppendHelpers(FString& Source)
{
	AppendLine(Source, TEXT("void RecordConstructorArgument(int Index, int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("void RecordConstructorParameterConsumed(int Index, int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("void RecordConstructorSelected(int Marker)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("enum ENativeCaseEnum"));
	AppendLine(Source, TEXT("{"));
	for (int32 Index = 0; Index <= 16; ++Index)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\tValue%d = %d%s"),
			Index,
			Index,
			Index < 16 ? TEXT(",") : TEXT("")));
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("typedef int NativeCaseAlias;"));
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
	AppendLine(Source, TEXT("struct FNativeCaseValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseValue()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseValue(int InValue)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FNoPromotion"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FNoMatch"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendObservers(FString& Source)
{
	for (int32 TypeIndex = 0; TypeIndex < 15; ++TypeIndex)
	{
		const ECtorParamSelectType Type = static_cast<ECtorParamSelectType>(TypeIndex);
		AppendLine(Source, FString::Printf(
			TEXT("%s Observe%s(int Index, %s Value)"),
			ScriptType(Type),
			*Compact(TypeToken(Type)),
			ScriptType(Type)));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(
			TEXT("\tRecordConstructorArgument(Index, %s);"),
			*ReadExpression(ScriptType(Type), TEXT("Value"))));
		AppendLine(Source, TEXT("\treturn Value;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
}

void AppendConstructorBody(
	FString& Source,
	const TCHAR* Candidate,
	const int32 Count,
	const int32 Marker,
	const int32 ExtraValue)
{
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, FString::Printf(TEXT("\t\tMarker = %d;"), Marker));
	AppendLine(Source, FString::Printf(TEXT("\t\tRecordConstructorSelected(%d);"), Marker));
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FString Parameter = FString::Printf(TEXT("P%d"), Index);
		const FString Read = ReadExpression(Candidate, Parameter);
		AppendLine(Source, FString::Printf(TEXT("\t\tRecordConstructorParameterConsumed(%d, %s);"), Index, *Read));
		AppendLine(Source, FString::Printf(TEXT("\t\tChecksum += %s;"), *Read));
	}
	if (ExtraValue != 0)
	{
		AppendLine(Source, FString::Printf(TEXT("\t\tChecksum += %d;"), ExtraValue));
	}
	AppendLine(Source, TEXT("\t}"));
}

FString ParameterList(const TCHAR* Candidate, const int32 Count, const TCHAR* Extra = nullptr)
{
	FString List;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Index > 0)
		{
			List += TEXT(", ");
		}
		List += FString::Printf(TEXT("%s P%d"), Candidate, Index);
	}
	if (Extra != nullptr)
	{
		List += TEXT(", ");
		List += Extra;
	}
	return List;
}

void AppendProbeType(FString& Source, const FCtorParamSelectParams& Params, const FString& Probe)
{
	const TCHAR* const Candidate = CandidateType(Params.Type, Params.Selection);
	const int32 Count = ArityCount(Params.Arity);
	AppendLine(Source, FString::Printf(TEXT("struct %s"), *Probe));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Marker = 0;"));
	AppendLine(Source, TEXT("\tint Checksum = 0;"));
	AppendLine(Source);
	if (Params.Selection == ECtorParamSelectSelection::Ambiguous)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s(%s)"), *Probe, *ParameterList(Candidate, Count, TEXT("int Extra = 1"))));
		AppendConstructorBody(Source, Candidate, Count, 401, 1);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\t%s(%s)"), *Probe, *ParameterList(Candidate, Count, TEXT("float Extra = 1.0f"))));
		AppendConstructorBody(Source, Candidate, Count, 402, 2);
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s(%s)"), *Probe, *ParameterList(Candidate, Count)));
		AppendConstructorBody(Source, Candidate, Count, SelectionMarker(Params.Selection), 0);
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendEntry(FString& Source, const FCtorParamSelectParams& Params, const FString& FunctionName, const FString& Probe)
{
	const int32 Count = ArityCount(Params.Arity);
	FString Arguments;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (Index > 0)
		{
			Arguments += TEXT(", ");
		}
		Arguments += ObservedArgument(Params, Index);
	}
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Value(%s);"), *Probe, *Arguments));
	AppendLine(Source, TEXT("\treturn Value.Checksum;"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FCtorParamSelectParams& Params, const FString& FunctionName)
{
	FCtorParamSelectCell Cell;
	Cell.Arity = Params.Arity;
	Cell.Selection = Params.Selection;
	Cell.Type = Params.Type;
	const FString Probe = ProbeName(Cell);
	FString Source;
	AppendHelpers(Source);
	AppendObservers(Source);
	AppendProbeType(Source, Params, Probe);
	AppendEntry(Source, Params, FunctionName, Probe);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FCtorParamSelectCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Selection, Cell.Type))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExpectedReturn = ExpectedChecksum(Cell.Type, Cell.Arity);
	Info.ExecutionNotes = HostNotes;
	return Info;
}
}

FStringView FCtorParamSelectGenerator::GetProductId() const
{
	return TEXT("LANG-CTOR-PARAM-SELECT");
}

TArray<FGeneratedCaseInfo> FCtorParamSelectGenerator::ListCases() const
{
	using namespace CtorParamSelectGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FCtorParamSelectCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("CtorParamSelect PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FCtorParamSelectGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace CtorParamSelectGeneratorDetail;
	FCtorParamSelectCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FCtorParamSelectGenerator::BuildDumpSource() const
{
	using namespace CtorParamSelectGeneratorDetail;
	const TArray<FCtorParamSelectCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CTOR-PARAM-SELECT"));
	AppendLine(Source, TEXT("// Cases: 300 total, 152 normal, 148 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendHelpers(Source);
	AppendObservers(Source);
	for (const FCtorParamSelectCell& Cell : Cells)
	{
		if (!IsReject(Cell.Selection, Cell.Type))
		{
			AppendProbeType(Source, ParamsFromCell(Cell), ProbeName(Cell));
		}
	}
	bool bNeedBlank = false;
	for (const FCtorParamSelectCell& Cell : Cells)
	{
		if (IsReject(Cell.Selection, Cell.Type))
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
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell), ProbeName(Cell));
		bNeedBlank = true;
	}
	for (const FCtorParamSelectCell& Cell : Cells)
	{
		if (!IsReject(Cell.Selection, Cell.Type))
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
		AppendHelpers(Source);
		AppendObservers(Source);
		AppendProbeType(Source, ParamsFromCell(Cell), ProbeName(Cell));
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell), ProbeName(Cell));
	}
	return Source;
}

FString FCtorParamSelectGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace CtorParamSelectGeneratorDetail;
	const TArray<FCtorParamSelectCell> Cells = AllCells();
	FString Source;
	AppendHelpers(Source);
	AppendObservers(Source);
	for (const FCtorParamSelectCell& Cell : Cells)
	{
		if (!IsReject(Cell.Selection, Cell.Type))
		{
			AppendProbeType(Source, ParamsFromCell(Cell), ProbeName(Cell));
		}
	}
	int32 Emitted = 0;
	for (const FCtorParamSelectCell& Cell : Cells)
	{
		if (IsReject(Cell.Selection, Cell.Type))
		{
			continue;
		}
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell), ProbeName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

FString FCtorParamSelectGenerator::BuildConstructorParameterSource(const FCtorParamSelectParams& Params) const
{
	using namespace CtorParamSelectGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Selection, Params.Type))
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

int32 FCtorParamSelectGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace CtorParamSelectGeneratorDetail;
	FCtorParamSelectCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Selection, Cell.Type))
	{
		return 0;
	}
	return ExpectedChecksum(Cell.Type, Cell.Arity);
}

FString FCtorParamSelectGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace CtorParamSelectGeneratorDetail;
	FCtorParamSelectCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Selection, Cell.Type))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FCtorParamSelectGenerator::ListRejectCaseIds() const
{
	using namespace CtorParamSelectGeneratorDetail;
	TArray<FString> Ids;
	for (const FCtorParamSelectCell& Cell : AllCells())
	{
		if (IsReject(Cell.Selection, Cell.Type))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
