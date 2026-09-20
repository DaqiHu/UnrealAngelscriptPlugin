#include "Framework/Generate/AngelscriptTestExprResolutionGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ExprResolutionGeneratorDetail
{
struct FResolutionCell
{
	EExprResolutionContext Context = EExprResolutionContext::Initializer;
	EExprResolutionShape Shape = EExprResolutionShape::Identifier;
	EExprResolutionState State = EExprResolutionState::Exact;
};

const TCHAR* ContextToken(const EExprResolutionContext Context)
{
	switch (Context)
	{
	case EExprResolutionContext::Initializer:
		return TEXT("INITIALIZER");
	case EExprResolutionContext::Assignment:
		return TEXT("ASSIGNMENT");
	case EExprResolutionContext::Argument:
		return TEXT("ARGUMENT");
	case EExprResolutionContext::Return:
		return TEXT("RETURN");
	case EExprResolutionContext::Condition:
		return TEXT("CONDITION");
	case EExprResolutionContext::Index:
		return TEXT("INDEX");
	case EExprResolutionContext::MemberReceiver:
		return TEXT("MEMBER_RECEIVER");
	default:
		return nullptr;
	}
}

const TCHAR* ShapeToken(const EExprResolutionShape Shape)
{
	switch (Shape)
	{
	case EExprResolutionShape::Identifier:
		return TEXT("IDENTIFIER");
	case EExprResolutionShape::Call:
		return TEXT("CALL");
	case EExprResolutionShape::Member:
		return TEXT("MEMBER");
	case EExprResolutionShape::ScopedName:
		return TEXT("SCOPED_NAME");
	default:
		return nullptr;
	}
}

const TCHAR* StateToken(const EExprResolutionState State)
{
	switch (State)
	{
	case EExprResolutionState::Exact:
		return TEXT("EXACT");
	case EExprResolutionState::NamespaceQualified:
		return TEXT("NAMESPACE_QUALIFIED");
	case EExprResolutionState::Overload:
		return TEXT("OVERLOAD");
	case EExprResolutionState::Conversion:
		return TEXT("CONVERSION");
	case EExprResolutionState::Missing:
		return TEXT("MISSING");
	case EExprResolutionState::Ambiguous:
		return TEXT("AMBIGUOUS");
	case EExprResolutionState::Inaccessible:
		return TEXT("INACCESSIBLE");
	case EExprResolutionState::WrongType:
		return TEXT("WRONG_TYPE");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FExprResolutionParams& Params)
{
	return ContextToken(Params.Context) != nullptr
		&& ShapeToken(Params.Shape) != nullptr
		&& StateToken(Params.State) != nullptr;
}

bool IsSuccessfulState(const EExprResolutionState State)
{
	return State == EExprResolutionState::Exact
		|| State == EExprResolutionState::NamespaceQualified
		|| State == EExprResolutionState::Overload
		|| State == EExprResolutionState::Conversion;
}

bool IsReject(const EExprResolutionState State)
{
	return !IsSuccessfulState(State);
}

int32 ShapeOrdinal(const EExprResolutionShape Shape)
{
	switch (Shape)
	{
	case EExprResolutionShape::Identifier:
		return 1;
	case EExprResolutionShape::Call:
		return 2;
	case EExprResolutionShape::Member:
		return 3;
	case EExprResolutionShape::ScopedName:
		return 4;
	default:
		return 0;
	}
}

int32 ExpectedFromCell(const FResolutionCell& Cell)
{
	if (IsReject(Cell.State))
	{
		return 0;
	}
	int32 Base = 0;
	switch (Cell.State)
	{
	case EExprResolutionState::Exact:
		Base = 100;
		break;
	case EExprResolutionState::NamespaceQualified:
		Base = 200;
		break;
	case EExprResolutionState::Overload:
		Base = 300;
		break;
	case EExprResolutionState::Conversion:
		Base = 400;
		break;
	default:
		return 0;
	}
	return Base + ShapeOrdinal(Cell.Shape);
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FResolutionCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-EXPR-RESOLUTION-%s-%s-%s"),
		ContextToken(Cell.Context),
		ShapeToken(Cell.Shape),
		StateToken(Cell.State));
}

FString MakeEntryName(const FResolutionCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FExprResolutionParams ParamsFromCell(const FResolutionCell& Cell)
{
	FExprResolutionParams Params;
	Params.Context = Cell.Context;
	Params.Shape = Cell.Shape;
	Params.State = Cell.State;
	return Params;
}

TArray<FResolutionCell> AllCells()
{
	TArray<FResolutionCell> Cells;
	Cells.SetNum(224);
	for (int32 Index = 0; Index < 224; ++Index)
	{
		FResolutionCell& Cell = Cells[Index];
		Cell.Context = static_cast<EExprResolutionContext>(Index / 32);
		Cell.Shape = static_cast<EExprResolutionShape>((Index / 8) % 4);
		Cell.State = static_cast<EExprResolutionState>(Index % 8);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FResolutionCell& OutCell)
{
	const FString Needle(CaseId);
	const TArray<FResolutionCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (MakeCaseId(Cells[Index]) == Needle)
		{
			OutCell = Cells[Index];
			return true;
		}
	}
	return false;
}

FString ResolutionExpression(const EExprResolutionState State, const EExprResolutionShape Shape)
{
	if (State == EExprResolutionState::Exact)
	{
		if (Shape == EExprResolutionShape::Identifier)
		{
			return TEXT("FNativeCaseValue(RecordResolution(ExactIdentifier))");
		}
		if (Shape == EExprResolutionShape::Call)
		{
			return TEXT("ResolveExactCall()");
		}
		if (Shape == EExprResolutionShape::Member)
		{
			return TEXT("FNativeCaseValue(RecordResolution(ResolutionOwner.ExactMember))");
		}
		return TEXT("FNativeCaseValue(RecordResolution(ResolutionExact::ExactScoped))");
	}
	if (State == EExprResolutionState::NamespaceQualified)
	{
		if (Shape == EExprResolutionShape::Identifier)
		{
			return TEXT("FNativeCaseValue(RecordResolution(ResolutionQualified::QualifiedIdentifier))");
		}
		if (Shape == EExprResolutionShape::Call)
		{
			return TEXT("ResolutionQualified::ResolveCall()");
		}
		if (Shape == EExprResolutionShape::Member)
		{
			return TEXT("FNativeCaseValue(RecordResolution(QualifiedOwner.QualifiedMember))");
		}
		return TEXT("FNativeCaseValue(RecordResolution(ResolutionOuter::ResolutionInner::QualifiedScoped))");
	}
	if (State == EExprResolutionState::Overload)
	{
		if (Shape == EExprResolutionShape::Identifier)
		{
			return TEXT("OverloadIdentifier");
		}
		if (Shape == EExprResolutionShape::Call)
		{
			return TEXT("ResolveCallOverload(7)");
		}
		if (Shape == EExprResolutionShape::Member)
		{
			return TEXT("ResolutionOwner.ResolveMemberOverload(7)");
		}
		return TEXT("ResolutionOverload::Resolve(7)");
	}
	if (State == EExprResolutionState::Conversion)
	{
		if (Shape == EExprResolutionShape::Identifier)
		{
			return TEXT("ConversionIdentifier");
		}
		if (Shape == EExprResolutionShape::Call)
		{
			return TEXT("ResolveCallConversion(int8(7))");
		}
		if (Shape == EExprResolutionShape::Member)
		{
			return TEXT("ResolutionOwner.ResolveMemberConversion(int8(7))");
		}
		return TEXT("ResolutionConversion::Resolve(int8(7))");
	}
	if (State == EExprResolutionState::Missing)
	{
		if (Shape == EExprResolutionShape::Identifier)
		{
			return TEXT("FNativeCaseValue(MissingIdentifier)");
		}
		if (Shape == EExprResolutionShape::Call)
		{
			return TEXT("MissingCall()");
		}
		if (Shape == EExprResolutionShape::Member)
		{
			return TEXT("ResolutionOwner.MissingMember");
		}
		return TEXT("ResolutionExisting::MissingScoped");
	}
	if (State == EExprResolutionState::Ambiguous)
	{
		if (Shape == EExprResolutionShape::Identifier)
		{
			return TEXT("FNativeCaseValue(AmbiguousIdentifier())");
		}
		if (Shape == EExprResolutionShape::Call)
		{
			return TEXT("ResolveAmbiguousCall(nullptr)");
		}
		if (Shape == EExprResolutionShape::Member)
		{
			return TEXT("ResolutionOwner.ResolveAmbiguousMember(nullptr)");
		}
		return TEXT("ResolutionAmbiguous::Resolve(nullptr)");
	}
	if (State == EExprResolutionState::Inaccessible)
	{
		if (Shape == EExprResolutionShape::Identifier)
		{
			return TEXT("FNativeCaseValue(HiddenIdentifier)");
		}
		if (Shape == EExprResolutionShape::Call)
		{
			return TEXT("FNativeCaseValue(HiddenCall())");
		}
		if (Shape == EExprResolutionShape::Member)
		{
			return TEXT("FNativeCaseValue(HiddenOwner.HiddenMember)");
		}
		return TEXT("FNativeCaseValue(ResolutionHidden::HiddenScoped)");
	}
	if (Shape == EExprResolutionShape::Identifier)
	{
		return TEXT("WrongIdentifier");
	}
	if (Shape == EExprResolutionShape::Call)
	{
		return TEXT("ResolveWrongCall()");
	}
	if (Shape == EExprResolutionShape::Member)
	{
		return TEXT("ResolutionOwner.WrongMember");
	}
	return TEXT("ResolutionWrong::Resolve()");
}

void AppendResolutionSetup(FString& Source, const EExprResolutionState State, const EExprResolutionShape Shape)
{
	if (State == EExprResolutionState::Exact && Shape == EExprResolutionShape::Identifier)
	{
		AppendLine(Source, TEXT("\tint ExactIdentifier = 101;"));
	}
	else if (State == EExprResolutionState::Exact && Shape == EExprResolutionShape::Member)
	{
		AppendLine(Source, TEXT("\tFResolutionOwner ResolutionOwner;"));
	}
	else if (State == EExprResolutionState::NamespaceQualified && Shape == EExprResolutionShape::Member)
	{
		AppendLine(Source, TEXT("\tResolutionQualified::FOwner QualifiedOwner;"));
	}
	else if (State == EExprResolutionState::Overload && Shape == EExprResolutionShape::Identifier)
	{
		AppendLine(Source, TEXT("\tFNativeCaseValue OverloadIdentifier = ResolveIdentifierOverload(7);"));
	}
	else if (State == EExprResolutionState::Overload && Shape == EExprResolutionShape::Member)
	{
		AppendLine(Source, TEXT("\tFResolutionOwner ResolutionOwner;"));
	}
	else if (State == EExprResolutionState::Conversion && Shape == EExprResolutionShape::Identifier)
	{
		AppendLine(Source, TEXT("\tFNativeCaseValue ConversionIdentifier = ResolveIdentifierConversion(int8(7));"));
	}
	else if (State == EExprResolutionState::Conversion && Shape == EExprResolutionShape::Member)
	{
		AppendLine(Source, TEXT("\tFResolutionOwner ResolutionOwner;"));
	}
	else if ((State == EExprResolutionState::Missing || State == EExprResolutionState::Ambiguous) &&
		Shape == EExprResolutionShape::Member)
	{
		AppendLine(Source, TEXT("\tFResolutionOwner ResolutionOwner;"));
	}
	else if (State == EExprResolutionState::Inaccessible && Shape == EExprResolutionShape::Member)
	{
		AppendLine(Source, TEXT("\tFNativeCaseValue HiddenOwner(307);"));
	}
	else if (State == EExprResolutionState::WrongType && Shape == EExprResolutionShape::Identifier)
	{
		AppendLine(Source, TEXT("\tFWrongResolutionValue WrongIdentifier;"));
	}
	else if (State == EExprResolutionState::WrongType && Shape == EExprResolutionShape::Member)
	{
		AppendLine(Source, TEXT("\tFResolutionOwner ResolutionOwner;"));
	}
}

void AppendSharedHelpers(FString& Source)
{
	AppendLine(Source, TEXT("int RecordResolution(int Marker)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Marker;"));
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
	AppendLine(Source, TEXT("struct FWrongResolutionValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tbool Wrong = true;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FRefRoot"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FRefUnrelated"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FResolutionOwner"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint ExactMember = 103;"));
	AppendLine(Source, TEXT("\tFWrongResolutionValue WrongMember;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseValue ResolveMemberOverload(int Value)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn FNativeCaseValue(RecordResolution(303));"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseValue ResolveMemberOverload(double Value)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn FNativeCaseValue(RecordResolution(903));"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseValue ResolveMemberConversion(int64 Value)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn FNativeCaseValue(RecordResolution(403));"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseValue ResolveAmbiguousMember(FRefRoot Value)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn FNativeCaseValue(0);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseValue ResolveAmbiguousMember(FRefUnrelated Value)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn FNativeCaseValue(0);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FNativeCaseValue ResolveExactCall()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn FNativeCaseValue(RecordResolution(102));"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FNativeCaseValue ResolveIdentifierOverload(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn FNativeCaseValue(RecordResolution(301));"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FNativeCaseValue ResolveIdentifierOverload(double Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn FNativeCaseValue(RecordResolution(901));"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FNativeCaseValue ResolveCallOverload(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn FNativeCaseValue(RecordResolution(302));"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FNativeCaseValue ResolveCallOverload(double Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn FNativeCaseValue(RecordResolution(902));"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FNativeCaseValue ResolveIdentifierConversion(int64 Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn FNativeCaseValue(RecordResolution(401));"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FNativeCaseValue ResolveCallConversion(int64 Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn FNativeCaseValue(RecordResolution(402));"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FNativeCaseValue ResolveAmbiguousCall(FRefRoot Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn FNativeCaseValue(0);"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FNativeCaseValue ResolveAmbiguousCall(FRefUnrelated Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn FNativeCaseValue(0);"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("FWrongResolutionValue ResolveWrongCall()"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn FWrongResolutionValue();"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("namespace ResolutionExact"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tconst int ExactScoped = 104;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("namespace ResolutionQualified"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tconst int QualifiedIdentifier = 201;"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tstruct FOwner"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tint QualifiedMember = 203;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseValue ResolveCall()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn FNativeCaseValue(RecordResolution(202));"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("namespace ResolutionOuter"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tnamespace ResolutionInner"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tconst int QualifiedScoped = 204;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("namespace ResolutionOverload"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseValue Resolve(int Value)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn FNativeCaseValue(RecordResolution(304));"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseValue Resolve(double Value)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn FNativeCaseValue(RecordResolution(904));"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("namespace ResolutionConversion"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseValue Resolve(int64 Value)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn FNativeCaseValue(RecordResolution(404));"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("namespace ResolutionExisting"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tconst int ExistingValue = 1;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("namespace ResolutionAmbiguous"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFNativeCaseValue Resolve(FRefRoot Value)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn FNativeCaseValue(0);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tFNativeCaseValue Resolve(FRefUnrelated Value)"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn FNativeCaseValue(0);"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("namespace ResolutionWrong"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tFWrongResolutionValue Resolve()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn FWrongResolutionValue();"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ObserveResolution(const FNativeCaseValue& in Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value.Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FResolutionIndexProbe"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint opIndex(int Index) const"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\treturn Index;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendResolutionUse(
	FString& Source,
	const EExprResolutionContext Context,
	const FString& Expression,
	const int32 Expected)
{
	switch (Context)
	{
	case EExprResolutionContext::Initializer:
		AppendLine(Source, FString::Printf(TEXT("\tFNativeCaseValue Result = %s;"), *Expression));
		AppendLine(Source, TEXT("\treturn Result.Value;"));
		break;
	case EExprResolutionContext::Assignment:
		AppendLine(Source, TEXT("\tFNativeCaseValue Result;"));
		AppendLine(Source, FString::Printf(TEXT("\tResult = %s;"), *Expression));
		AppendLine(Source, TEXT("\treturn Result.Value;"));
		break;
	case EExprResolutionContext::Argument:
		AppendLine(Source, FString::Printf(TEXT("\treturn ObserveResolution(%s);"), *Expression));
		break;
	case EExprResolutionContext::Condition:
		AppendLine(Source, FString::Printf(TEXT("\tif ((%s).Value == %d)"), *Expression, Expected));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, FString::Printf(TEXT("\t\treturn %d;"), Expected));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("\treturn -1;"));
		break;
	case EExprResolutionContext::Index:
		AppendLine(Source, TEXT("\tFResolutionIndexProbe Probe;"));
		AppendLine(Source, FString::Printf(TEXT("\treturn Probe[(%s).Value];"), *Expression));
		break;
	case EExprResolutionContext::MemberReceiver:
		AppendLine(Source, FString::Printf(TEXT("\treturn (%s).Value;"), *Expression));
		break;
	case EExprResolutionContext::Return:
		break;
	}
}

void AppendEntry(FString& Source, const FExprResolutionParams& Params, const FString& FunctionName)
{
	const FResolutionCell Cell{Params.Context, Params.Shape, Params.State};
	const int32 Expected = ExpectedFromCell(Cell);
	const FString Expression = ResolutionExpression(Params.State, Params.Shape);
	if (Params.Context == EExprResolutionContext::Return)
	{
		AppendLine(Source, FString::Printf(TEXT("FNativeCaseValue ProduceResolution_%s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendResolutionSetup(Source, Params.State, Params.Shape);
		AppendLine(Source, FString::Printf(TEXT("\treturn %s;"), *Expression));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	if (Params.Context == EExprResolutionContext::Return)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn ProduceResolution_%s().Value;"), *FunctionName));
	}
	else
	{
		AppendResolutionSetup(Source, Params.State, Params.Shape);
		AppendResolutionUse(Source, Params.Context, Expression, Expected);
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FExprResolutionParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendSharedHelpers(Source);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FResolutionCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.State))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = ExpectedFromCell(Cell);
	return Info;
}
}

FStringView FExprResolutionGenerator::GetProductId() const
{
	return TEXT("LANG-EXPR-RESOLUTION");
}

TArray<FGeneratedCaseInfo> FExprResolutionGenerator::ListCases() const
{
	using namespace ExprResolutionGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	const TArray<FResolutionCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cells[Index]);
			checkf(!EntryNames.Contains(EntryName), TEXT("ExprResolution PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FExprResolutionGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ExprResolutionGeneratorDetail;
	FResolutionCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FExprResolutionGenerator::BuildDumpSource() const
{
	using namespace ExprResolutionGeneratorDetail;
	const TArray<FResolutionCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-EXPR-RESOLUTION"));
	AppendLine(Source, TEXT("// Cases: 224 total, 112 normal, 112 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendSharedHelpers(Source);
	bool bNeedBlank = true;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].State))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		if (bNeedBlank)
		{
			AppendLine(Source);
		}
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		bNeedBlank = true;
	}
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (!IsReject(Cells[Index].State))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cells[Index]);
		AppendLine(Source);
		AppendLine(Source, TEXT("// Reject module"));
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, TEXT("// EntryDeclaration:"));
		AppendLine(Source, TEXT("// Outcome: CompileReject"));
		AppendLine(Source, TEXT("// Execution: SourceOnly"));
		AppendLine(Source);
		AppendSharedHelpers(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
	}
	return Source;
}

FString FExprResolutionGenerator::BuildExpressionResolutionSource(const FExprResolutionParams& Params) const
{
	using namespace ExprResolutionGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.State))
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

FString FExprResolutionGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ExprResolutionGeneratorDetail;
	const TArray<FResolutionCell> Cells = AllCells();
	FString Source;
	AppendSharedHelpers(Source);
	int32 Emitted = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].State))
		{
			continue;
		}
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cells[Index]), MakeEntryName(Cells[Index]));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

int32 FExprResolutionGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ExprResolutionGeneratorDetail;
	FResolutionCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.State))
	{
		return 0;
	}
	return ExpectedFromCell(Cell);
}

FString FExprResolutionGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace ExprResolutionGeneratorDetail;
	FResolutionCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.State))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FExprResolutionGenerator::ListRejectCaseIds() const
{
	using namespace ExprResolutionGeneratorDetail;
	TArray<FString> Ids;
	const TArray<FResolutionCell> Cells = AllCells();
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (IsReject(Cells[Index].State))
		{
			Ids.Add(MakeCaseId(Cells[Index]));
		}
	}
	return Ids;
}
}
