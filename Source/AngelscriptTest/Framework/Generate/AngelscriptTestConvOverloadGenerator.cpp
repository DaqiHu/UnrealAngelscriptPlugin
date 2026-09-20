#include "Framework/Generate/AngelscriptTestConvOverloadGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ConvOverloadGeneratorDetail
{
constexpr int32 ContextCount = 6;
constexpr int32 ConversionCount = 6;
constexpr int32 OutcomeCount = 4;
constexpr int32 TotalCells = ContextCount * ConversionCount * OutcomeCount;

struct FOverloadCell
{
	EConvOverloadContext Context = EConvOverloadContext::Overload;
	EConvOverloadConversion Conversion = EConvOverloadConversion::Identity;
	EConvOverloadOutcome Outcome = EConvOverloadOutcome::Exact;
};

const TCHAR* ContextToken(const EConvOverloadContext Context)
{
	switch (Context)
	{
	case EConvOverloadContext::Overload:
		return TEXT("OVERLOAD");
	case EConvOverloadContext::Operator:
		return TEXT("OPERATOR");
	case EConvOverloadContext::DefaultArgument:
		return TEXT("DEFAULT_ARGUMENT");
	case EConvOverloadContext::Property:
		return TEXT("PROPERTY");
	case EConvOverloadContext::Index:
		return TEXT("INDEX");
	case EConvOverloadContext::Conditional:
		return TEXT("CONDITIONAL");
	default:
		return nullptr;
	}
}

const TCHAR* ConversionToken(const EConvOverloadConversion Conversion)
{
	switch (Conversion)
	{
	case EConvOverloadConversion::Identity:
		return TEXT("IDENTITY");
	case EConvOverloadConversion::Promotion:
		return TEXT("PROMOTION");
	case EConvOverloadConversion::Narrowing:
		return TEXT("NARROWING");
	case EConvOverloadConversion::Constructor:
		return TEXT("CONSTRUCTOR");
	case EConvOverloadConversion::Operator:
		return TEXT("OPERATOR");
	case EConvOverloadConversion::ReferenceCast:
		return TEXT("REFERENCE_CAST");
	default:
		return nullptr;
	}
}

const TCHAR* OutcomeToken(const EConvOverloadOutcome Outcome)
{
	switch (Outcome)
	{
	case EConvOverloadOutcome::Exact:
		return TEXT("EXACT");
	case EConvOverloadOutcome::SelectedConversion:
		return TEXT("SELECTED_CONVERSION");
	case EConvOverloadOutcome::Ambiguous:
		return TEXT("AMBIGUOUS");
	case EConvOverloadOutcome::Rejected:
		return TEXT("REJECTED");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FConvOverloadParams& Params)
{
	return ContextToken(Params.Context) != nullptr
		&& ConversionToken(Params.Conversion) != nullptr
		&& OutcomeToken(Params.Outcome) != nullptr;
}

bool IsReject(const EConvOverloadOutcome Outcome)
{
	return Outcome == EConvOverloadOutcome::Ambiguous || Outcome == EConvOverloadOutcome::Rejected;
}

FString SourceType(const EConvOverloadConversion Conversion, const FString& Suffix)
{
	switch (Conversion)
	{
	case EConvOverloadConversion::Identity:
	case EConvOverloadConversion::Constructor:
		return TEXT("int");
	case EConvOverloadConversion::Promotion:
		return TEXT("int8");
	case EConvOverloadConversion::Narrowing:
		return TEXT("int64");
	case EConvOverloadConversion::Operator:
		return FString::Printf(TEXT("FResolutionOperatorSource%s"), *Suffix);
	case EConvOverloadConversion::ReferenceCast:
	default:
		return FString::Printf(TEXT("FResolutionReferenceDerived%s@"), *Suffix);
	}
}

FString TargetType(const EConvOverloadConversion Conversion, const EConvOverloadOutcome Outcome, const FString& Suffix)
{
	if (Outcome == EConvOverloadOutcome::Exact)
	{
		return SourceType(Conversion, Suffix);
	}
	if (Conversion == EConvOverloadConversion::Constructor)
	{
		return FString::Printf(TEXT("FResolutionConstructed%s"), *Suffix);
	}
	if (Conversion == EConvOverloadConversion::ReferenceCast)
	{
		return FString::Printf(TEXT("FResolutionReferenceBase%s@"), *Suffix);
	}
	return TEXT("int");
}

int32 ExpectedSourceMarker(const EConvOverloadConversion Conversion)
{
	switch (Conversion)
	{
	case EConvOverloadConversion::Identity:
		return 11;
	case EConvOverloadConversion::Promotion:
		return 12;
	case EConvOverloadConversion::Narrowing:
		return 13;
	case EConvOverloadConversion::Constructor:
		return 14;
	case EConvOverloadConversion::Operator:
		return 15;
	case EConvOverloadConversion::ReferenceCast:
	default:
		return -1;
	}
}

int32 ExpectedSelectedMarker(const EConvOverloadConversion Conversion, const EConvOverloadOutcome Outcome)
{
	if (Outcome == EConvOverloadOutcome::Exact)
	{
		return ExpectedSourceMarker(Conversion);
	}
	if (Conversion == EConvOverloadConversion::Constructor)
	{
		return 114;
	}
	if (Conversion == EConvOverloadConversion::Operator)
	{
		return 215;
	}
	return ExpectedSourceMarker(Conversion);
}

int32 ExpectedContextResult(const FOverloadCell& Cell)
{
	const int32 Marker = ExpectedSelectedMarker(Cell.Conversion, Cell.Outcome);
	switch (Cell.Context)
	{
	case EConvOverloadContext::Overload:
		return Marker + 100;
	case EConvOverloadContext::Operator:
		return Marker + 7;
	case EConvOverloadContext::DefaultArgument:
		return Marker + 5;
	case EConvOverloadContext::Index:
		return Marker + 1;
	default:
		return Marker;
	}
}

FString ConditionalFallback(const FString& Type)
{
	if (Type.StartsWith(TEXT("FResolutionConstructed")))
	{
		return Type + TEXT("(0)");
	}
	if (Type.StartsWith(TEXT("FResolutionOperatorSource")))
	{
		return Type + TEXT("(0)");
	}
	if (Type.EndsWith(TEXT("@")))
	{
		return TEXT("nullptr");
	}
	return TEXT("0");
}

FString CellSuffix(const FConvOverloadParams& Params)
{
	FString Id = FString::Printf(
		TEXT("LANG-CONV-OVERLOAD-%s-%s-%s"),
		ContextToken(Params.Context),
		ConversionToken(Params.Conversion),
		OutcomeToken(Params.Outcome));
	FString Entry = MakeCanonicalEntryName(Id);
	Entry.RemoveFromStart(TEXT("EntryLangConvOverload"));
	return Entry;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FOverloadCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CONV-OVERLOAD-%s-%s-%s"),
		ContextToken(Cell.Context),
		ConversionToken(Cell.Conversion),
		OutcomeToken(Cell.Outcome));
}

FString MakeEntryName(const FOverloadCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FConvOverloadParams ParamsFromCell(const FOverloadCell& Cell)
{
	FConvOverloadParams Params;
	Params.Context = Cell.Context;
	Params.Conversion = Cell.Conversion;
	Params.Outcome = Cell.Outcome;
	return Params;
}

FOverloadCell CellFromIndex(const int32 Index)
{
	FOverloadCell Cell;
	Cell.Context = static_cast<EConvOverloadContext>(Index / (ConversionCount * OutcomeCount));
	const int32 AfterContext = Index % (ConversionCount * OutcomeCount);
	Cell.Conversion = static_cast<EConvOverloadConversion>(AfterContext / OutcomeCount);
	Cell.Outcome = static_cast<EConvOverloadOutcome>(AfterContext % OutcomeCount);
	return Cell;
}

bool TryFindCell(const FStringView CaseId, FOverloadCell& OutCell)
{
	const FString Needle(CaseId);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FOverloadCell Cell = CellFromIndex(Index);
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendConversionDefinitions(FString& Source, const FConvOverloadParams& Params, const FString& Suffix)
{
	if (Params.Conversion == EConvOverloadConversion::Constructor && Params.Outcome != EConvOverloadOutcome::Exact)
	{
		AppendLine(Source, FString::Printf(TEXT("struct FResolutionConstructed%s"), *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Marker;"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFResolutionConstructed%s(int InValue)"), *Suffix));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tMarker = InValue + 100;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	if (Params.Conversion == EConvOverloadConversion::Operator)
	{
		AppendLine(Source, FString::Printf(TEXT("struct FResolutionOperatorSource%s"), *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Marker;"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFResolutionOperatorSource%s(int InMarker)"), *Suffix));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tMarker = InMarker;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, TEXT("\tint opImplConv() const"));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\t\treturn Marker + 200;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	if (Params.Conversion == EConvOverloadConversion::ReferenceCast)
	{
		AppendLine(Source, FString::Printf(TEXT("class FResolutionReferenceBase%s"), *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Marker;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("class FResolutionReferenceDerived%s : FResolutionReferenceBase%s"), *Suffix, *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
}

void AppendSourceValue(FString& Source, const EConvOverloadConversion Conversion, const FString& Suffix)
{
	switch (Conversion)
	{
	case EConvOverloadConversion::Identity:
		AppendLine(Source, TEXT("\tint SourceValue = 11;"));
		break;
	case EConvOverloadConversion::Promotion:
		AppendLine(Source, TEXT("\tint8 SourceValue = 12;"));
		break;
	case EConvOverloadConversion::Narrowing:
		AppendLine(Source, TEXT("\tint64 SourceValue = 13;"));
		break;
	case EConvOverloadConversion::Constructor:
		AppendLine(Source, TEXT("\tint SourceValue = 14;"));
		break;
	case EConvOverloadConversion::Operator:
		AppendLine(Source, FString::Printf(TEXT("\tFResolutionOperatorSource%s SourceValue(15);"), *Suffix));
		break;
	case EConvOverloadConversion::ReferenceCast:
	default:
		AppendLine(Source, FString::Printf(TEXT("\tFResolutionReferenceDerived%s@ SourceValue = nullptr;"), *Suffix));
		break;
	}
}

void AppendObservationHelper(FString& Source, const FString& Type, const FString& Suffix)
{
	AppendLine(Source, FString::Printf(TEXT("int ObserveResolution%s(%s Value)"), *Suffix, *Type));
	AppendLine(Source, TEXT("{"));
	if (Type.StartsWith(TEXT("FResolutionConstructed")) || Type.StartsWith(TEXT("FResolutionOperatorSource")))
	{
		AppendLine(Source, TEXT("\treturn Value.Marker;"));
	}
	else if (Type.EndsWith(TEXT("@")))
	{
		AppendLine(Source, TEXT("\treturn Value == nullptr ? -1 : Value.Marker;"));
	}
	else
	{
		AppendLine(Source, TEXT("\treturn int(Value);"));
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendNormalContextSurface(FString& Source, const FConvOverloadParams& Params, const FString& Type)
{
	const FString Suffix = CellSuffix(Params);
	AppendLine(Source, FString::Printf(TEXT("struct FResolutionAlternative%s"), *Suffix));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	if (Params.Context == EConvOverloadContext::Overload)
	{
		AppendLine(Source, FString::Printf(TEXT("int SelectResolution%s(%s Value)"), *Suffix, *Type));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn ObserveResolution%s(Value) + 100;"), *Suffix));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int SelectResolution%s(FResolutionAlternative%s Value)"), *Suffix, *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn 999;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Params.Context == EConvOverloadContext::Operator)
	{
		AppendLine(Source, FString::Printf(TEXT("struct FResolutionOperatorContext%s"), *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tint opAdd(%s Value)"), *Type));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\treturn ObserveResolution%s(Value) + 7;"), *Suffix));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tint opAdd(FResolutionAlternative%s Value)"), *Suffix));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 999;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Params.Context == EConvOverloadContext::DefaultArgument)
	{
		AppendLine(Source, FString::Printf(TEXT("int DefaultResolution%s(%s Value, int Offset = 5)"), *Suffix, *Type));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\treturn ObserveResolution%s(Value) + Offset;"), *Suffix));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(
			TEXT("int DefaultResolution%s(FResolutionAlternative%s Value, int Offset = 5)"),
			*Suffix,
			*Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn 999 + Offset;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Params.Context == EConvOverloadContext::Property)
	{
		AppendLine(Source, FString::Printf(TEXT("struct FResolutionHolder%s"), *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Result;"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tvoid set_Value(%s Value) property"), *Type));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\tResult = ObserveResolution%s(Value);"), *Suffix));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tvoid set_Value(FResolutionAlternative%s Value) property"), *Suffix));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tResult = 999;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Params.Context == EConvOverloadContext::Index)
	{
		AppendLine(Source, FString::Printf(TEXT("struct FResolutionIndex%s"), *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tint opIndex(%s Value)"), *Type));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\treturn ObserveResolution%s(Value) + 1;"), *Suffix));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tint opIndex(FResolutionAlternative%s Value)"), *Suffix));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 999;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
}

void AppendAmbiguousContextSurface(FString& Source, const FConvOverloadParams& Params)
{
	const FString Suffix = CellSuffix(Params);
	const FString ValueType = SourceType(Params.Conversion, Suffix);
	AppendLine(Source, FString::Printf(TEXT("struct FResolutionAmbiguousLeft%s"), *Suffix));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\tFResolutionAmbiguousLeft%s(%s Value)"), *Suffix, *ValueType));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("struct FResolutionAmbiguousRight%s"), *Suffix));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\tFResolutionAmbiguousRight%s(%s Value)"), *Suffix, *ValueType));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	if (Params.Context == EConvOverloadContext::Overload || Params.Context == EConvOverloadContext::Conditional)
	{
		AppendLine(Source, FString::Printf(TEXT("int SelectAmbiguousResolution%s(FResolutionAmbiguousLeft%s Value)"), *Suffix, *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn 1;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int SelectAmbiguousResolution%s(FResolutionAmbiguousRight%s Value)"), *Suffix, *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn 2;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Params.Context == EConvOverloadContext::Operator)
	{
		AppendLine(Source, FString::Printf(TEXT("struct FResolutionAmbiguousOperator%s"), *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tint opAdd(FResolutionAmbiguousLeft%s Value)"), *Suffix));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 1;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tint opAdd(FResolutionAmbiguousRight%s Value)"), *Suffix));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 2;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Params.Context == EConvOverloadContext::DefaultArgument)
	{
		AppendLine(Source, FString::Printf(
			TEXT("int DefaultAmbiguous%s(FResolutionAmbiguousLeft%s Value, int Offset = 5)"),
			*Suffix,
			*Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn Offset;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(
			TEXT("int DefaultAmbiguous%s(FResolutionAmbiguousRight%s Value, int Offset = 5)"),
			*Suffix,
			*Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn Offset;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Params.Context == EConvOverloadContext::Property)
	{
		AppendLine(Source, FString::Printf(TEXT("struct FResolutionAmbiguousHolder%s"), *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tvoid set_Value(FResolutionAmbiguousLeft%s Value) property"), *Suffix));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tvoid set_Value(FResolutionAmbiguousRight%s Value) property"), *Suffix));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("struct FResolutionAmbiguousIndex%s"), *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tint opIndex(FResolutionAmbiguousLeft%s Value)"), *Suffix));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 1;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tint opIndex(FResolutionAmbiguousRight%s Value)"), *Suffix));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 2;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
}

void AppendRejectedContextSurface(FString& Source, const FConvOverloadParams& Params)
{
	const FString Suffix = CellSuffix(Params);
	AppendLine(Source, FString::Printf(TEXT("struct FResolutionRejected%s"), *Suffix));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	if (Params.Context == EConvOverloadContext::Overload)
	{
		AppendLine(Source, FString::Printf(TEXT("int SelectRejectedResolution%s(FResolutionRejected%s Value)"), *Suffix, *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn 0;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Params.Context == EConvOverloadContext::Operator)
	{
		AppendLine(Source, FString::Printf(TEXT("struct FResolutionRejectedOperator%s"), *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tint opAdd(FResolutionRejected%s Value)"), *Suffix));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 0;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Params.Context == EConvOverloadContext::DefaultArgument)
	{
		AppendLine(Source, FString::Printf(
			TEXT("int DefaultRejectedResolution%s(FResolutionRejected%s Value, int Offset = 5)"),
			*Suffix,
			*Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn Offset;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Params.Context == EConvOverloadContext::Property)
	{
		AppendLine(Source, FString::Printf(TEXT("struct FResolutionRejectedHolder%s"), *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tvoid set_Value(FResolutionRejected%s Value) property"), *Suffix));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else if (Params.Context == EConvOverloadContext::Index)
	{
		AppendLine(Source, FString::Printf(TEXT("struct FResolutionRejectedIndex%s"), *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tint opIndex(FResolutionRejected%s Value)"), *Suffix));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 0;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
}

void AppendNormalRunBody(FString& Source, const FConvOverloadParams& Params)
{
	const FString Suffix = CellSuffix(Params);
	const FString Type = TargetType(Params.Conversion, Params.Outcome, Suffix);
	AppendSourceValue(Source, Params.Conversion, Suffix);
	if (Params.Context == EConvOverloadContext::Overload)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn SelectResolution%s(SourceValue);"), *Suffix));
	}
	else if (Params.Context == EConvOverloadContext::Operator)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFResolutionOperatorContext%s Context;"), *Suffix));
		AppendLine(Source, TEXT("\treturn Context + SourceValue;"));
	}
	else if (Params.Context == EConvOverloadContext::DefaultArgument)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn DefaultResolution%s(SourceValue);"), *Suffix));
	}
	else if (Params.Context == EConvOverloadContext::Property)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFResolutionHolder%s Holder;"), *Suffix));
		AppendLine(Source, TEXT("\tHolder.Value = SourceValue;"));
		AppendLine(Source, TEXT("\treturn Holder.Result;"));
	}
	else if (Params.Context == EConvOverloadContext::Index)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFResolutionIndex%s Index;"), *Suffix));
		AppendLine(Source, TEXT("\treturn Index[SourceValue];"));
	}
	else
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s TargetValue = true ? SourceValue : %s;"),
			*Type,
			*ConditionalFallback(Type)));
		AppendLine(Source, FString::Printf(TEXT("\treturn ObserveResolution%s(TargetValue);"), *Suffix));
	}
}

void AppendAmbiguousRunBody(FString& Source, const FConvOverloadParams& Params)
{
	const FString Suffix = CellSuffix(Params);
	AppendSourceValue(Source, Params.Conversion, Suffix);
	if (Params.Context == EConvOverloadContext::Overload)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn SelectAmbiguousResolution%s(SourceValue);"), *Suffix));
	}
	else if (Params.Context == EConvOverloadContext::Operator)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFResolutionAmbiguousOperator%s Context;"), *Suffix));
		AppendLine(Source, TEXT("\treturn Context + SourceValue;"));
	}
	else if (Params.Context == EConvOverloadContext::DefaultArgument)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn DefaultAmbiguous%s(SourceValue);"), *Suffix));
	}
	else if (Params.Context == EConvOverloadContext::Property)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFResolutionAmbiguousHolder%s Holder;"), *Suffix));
		AppendLine(Source, TEXT("\tHolder.Value = SourceValue;"));
		AppendLine(Source, TEXT("\treturn 0;"));
	}
	else if (Params.Context == EConvOverloadContext::Index)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFResolutionAmbiguousIndex%s Index;"), *Suffix));
		AppendLine(Source, TEXT("\treturn Index[SourceValue];"));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn SelectAmbiguousResolution%s(true ? SourceValue : SourceValue);"), *Suffix));
	}
}

void AppendRejectedRunBody(FString& Source, const FConvOverloadParams& Params)
{
	const FString Suffix = CellSuffix(Params);
	AppendSourceValue(Source, Params.Conversion, Suffix);
	if (Params.Context == EConvOverloadContext::Overload)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn SelectRejectedResolution%s(SourceValue);"), *Suffix));
	}
	else if (Params.Context == EConvOverloadContext::Operator)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFResolutionRejectedOperator%s Context;"), *Suffix));
		AppendLine(Source, TEXT("\treturn Context + SourceValue;"));
	}
	else if (Params.Context == EConvOverloadContext::DefaultArgument)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn DefaultRejectedResolution%s(SourceValue);"), *Suffix));
	}
	else if (Params.Context == EConvOverloadContext::Property)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFResolutionRejectedHolder%s Holder;"), *Suffix));
		AppendLine(Source, TEXT("\tHolder.Value = SourceValue;"));
		AppendLine(Source, TEXT("\treturn 0;"));
	}
	else if (Params.Context == EConvOverloadContext::Index)
	{
		AppendLine(Source, FString::Printf(TEXT("\tFResolutionRejectedIndex%s Index;"), *Suffix));
		AppendLine(Source, TEXT("\treturn Index[SourceValue];"));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\tFResolutionRejected%s TargetValue;"), *Suffix));
		AppendLine(Source, TEXT("\tTargetValue = true ? SourceValue : TargetValue;"));
		AppendLine(Source, TEXT("\treturn 0;"));
	}
}

void AppendEntry(FString& Source, const FConvOverloadParams& Params, const FString& FunctionName)
{
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (!IsReject(Params.Outcome))
	{
		AppendNormalRunBody(Source, Params);
	}
	else if (Params.Outcome == EConvOverloadOutcome::Ambiguous)
	{
		AppendAmbiguousRunBody(Source, Params);
	}
	else
	{
		AppendRejectedRunBody(Source, Params);
	}
	AppendLine(Source, TEXT("}"));
}

void AppendOwnedHelpers(FString& Source, const FConvOverloadParams& Params)
{
	const FString Suffix = CellSuffix(Params);
	AppendConversionDefinitions(Source, Params, Suffix);
	if (!IsReject(Params.Outcome))
	{
		const FString Type = TargetType(Params.Conversion, Params.Outcome, Suffix);
		AppendObservationHelper(Source, Type, CellSuffix(Params));
		AppendNormalContextSurface(Source, Params, Type);
	}
	else if (Params.Outcome == EConvOverloadOutcome::Ambiguous)
	{
		AppendAmbiguousContextSurface(Source, Params);
	}
	else
	{
		AppendRejectedContextSurface(Source, Params);
	}
}

FString BuildOwnedSource(const FConvOverloadParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOwnedHelpers(Source, Params);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FOverloadCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Outcome))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = ExpectedContextResult(Cell);
	return Info;
}
}

FStringView FConvOverloadGenerator::GetProductId() const
{
	return TEXT("LANG-CONV-OVERLOAD");
}

TArray<FGeneratedCaseInfo> FConvOverloadGenerator::ListCases() const
{
	using namespace ConvOverloadGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	Cases.Reserve(TotalCells);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FOverloadCell Cell = CellFromIndex(Index);
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("ConvOverload PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FConvOverloadGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ConvOverloadGeneratorDetail;
	FOverloadCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FConvOverloadGenerator::BuildDumpSource() const
{
	using namespace ConvOverloadGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CONV-OVERLOAD"));
	AppendLine(Source, TEXT("// Cases: 144 total, 72 normal, 72 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	bool bNeedBlank = false;
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FOverloadCell Cell = CellFromIndex(Index);
		if (IsReject(Cell.Outcome))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FConvOverloadParams Params = ParamsFromCell(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendOwnedHelpers(Source, Params);
		AppendEntry(Source, Params, MakeEntryName(Cell));
		bNeedBlank = true;
	}
	(void)bNeedBlank;
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FOverloadCell Cell = CellFromIndex(Index);
		if (!IsReject(Cell.Outcome))
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
		AppendOwnedHelpers(Source, ParamsFromCell(Cell));
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FConvOverloadGenerator::BuildResolutionSource(const FConvOverloadParams& Params) const
{
	using namespace ConvOverloadGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Outcome))
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

FString FConvOverloadGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ConvOverloadGeneratorDetail;
	FString Source;
	int32 Emitted = 0;
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FOverloadCell Cell = CellFromIndex(Index);
		if (IsReject(Cell.Outcome))
		{
			continue;
		}
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		const FConvOverloadParams Params = ParamsFromCell(Cell);
		AppendOwnedHelpers(Source, Params);
		AppendEntry(Source, Params, MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

int32 FConvOverloadGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ConvOverloadGeneratorDetail;
	FOverloadCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Outcome))
	{
		return 0;
	}
	return ExpectedContextResult(Cell);
}

FString FConvOverloadGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace ConvOverloadGeneratorDetail;
	FOverloadCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Outcome))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FConvOverloadGenerator::ListRejectCaseIds() const
{
	using namespace ConvOverloadGeneratorDetail;
	TArray<FString> Ids;
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FOverloadCell Cell = CellFromIndex(Index);
		if (IsReject(Cell.Outcome))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
