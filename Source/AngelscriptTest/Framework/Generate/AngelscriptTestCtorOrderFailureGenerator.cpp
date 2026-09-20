#include "Framework/Generate/AngelscriptTestCtorOrderFailureGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace CtorOrderFailureGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native constructor-failure host markers.");

struct FCtorOrderFailureCell
{
	ECtorOrderFailureFailure Failure = ECtorOrderFailureFailure::None;
	ECtorOrderFailureDepth Depth = ECtorOrderFailureDepth::FlatMembers;
	ECtorOrderFailureObservation Observation = ECtorOrderFailureObservation::Values;
};

const TCHAR* FailureToken(const ECtorOrderFailureFailure Failure)
{
	switch (Failure)
	{
	case ECtorOrderFailureFailure::None:
		return TEXT("NONE");
	case ECtorOrderFailureFailure::Base:
		return TEXT("BASE");
	case ECtorOrderFailureFailure::MemberFirst:
		return TEXT("MEMBER_FIRST");
	case ECtorOrderFailureFailure::MemberMiddle:
		return TEXT("MEMBER_MIDDLE");
	case ECtorOrderFailureFailure::MemberLast:
		return TEXT("MEMBER_LAST");
	case ECtorOrderFailureFailure::DerivedBody:
		return TEXT("DERIVED_BODY");
	case ECtorOrderFailureFailure::Copy:
		return TEXT("COPY");
	case ECtorOrderFailureFailure::Conversion:
		return TEXT("CONVERSION");
	default:
		return nullptr;
	}
}

const TCHAR* DepthToken(const ECtorOrderFailureDepth Depth)
{
	switch (Depth)
	{
	case ECtorOrderFailureDepth::FlatMembers:
		return TEXT("FLAT_MEMBERS");
	case ECtorOrderFailureDepth::NestedMembers:
		return TEXT("NESTED_MEMBERS");
	case ECtorOrderFailureDepth::DeepNestedMembers:
		return TEXT("DEEP_NESTED_MEMBERS");
	case ECtorOrderFailureDepth::BaseAndDerivedMembers:
		return TEXT("BASE_AND_DERIVED_MEMBERS");
	default:
		return nullptr;
	}
}

const TCHAR* ObservationToken(const ECtorOrderFailureObservation Observation)
{
	switch (Observation)
	{
	case ECtorOrderFailureObservation::Values:
		return TEXT("VALUES");
	case ECtorOrderFailureObservation::EventOrder:
		return TEXT("EVENT_ORDER");
	case ECtorOrderFailureObservation::Cleanup:
		return TEXT("CLEANUP");
	case ECtorOrderFailureObservation::ContextReuse:
		return TEXT("CONTEXT_REUSE");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FCtorOrderFailureParams& Params)
{
	return FailureToken(Params.Failure) != nullptr
		&& DepthToken(Params.Depth) != nullptr
		&& ObservationToken(Params.Observation) != nullptr;
}

bool IsFault(const ECtorOrderFailureFailure Failure)
{
	return Failure != ECtorOrderFailureFailure::None;
}

int32 FailureStage(const ECtorOrderFailureFailure Failure)
{
	return static_cast<int32>(Failure);
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FCtorOrderFailureCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CTOR-ORDER-FAILURE-%s-%s-%s"),
		FailureToken(Cell.Failure),
		DepthToken(Cell.Depth),
		ObservationToken(Cell.Observation));
}

FString MakeEntryName(const FCtorOrderFailureCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FString Compact(const TCHAR* Token)
{
	FString Text(Token);
	Text.ReplaceInline(TEXT("_"), TEXT(""));
	return Text;
}

FString GraphName(const ECtorOrderFailureFailure Failure, const ECtorOrderFailureDepth Depth)
{
	return FString::Printf(TEXT("FCtorFailGraph%s%s"), *Compact(FailureToken(Failure)), *Compact(DepthToken(Depth)));
}

FCtorOrderFailureParams ParamsFromCell(const FCtorOrderFailureCell& Cell)
{
	FCtorOrderFailureParams Params;
	Params.Failure = Cell.Failure;
	Params.Depth = Cell.Depth;
	Params.Observation = Cell.Observation;
	return Params;
}

TArray<FCtorOrderFailureCell> AllCells()
{
	TArray<FCtorOrderFailureCell> Cells;
	Cells.SetNum(128);
	for (int32 Index = 0; Index < 128; ++Index)
	{
		FCtorOrderFailureCell& Cell = Cells[Index];
		Cell.Failure = static_cast<ECtorOrderFailureFailure>(Index / 16);
		Cell.Depth = static_cast<ECtorOrderFailureDepth>((Index / 4) % 4);
		Cell.Observation = static_cast<ECtorOrderFailureObservation>(Index % 4);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FCtorOrderFailureCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FCtorOrderFailureCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendMaybeFail(FString& Source, const int32 Stage, const ECtorOrderFailureFailure Failure, const TCHAR* Indent)
{
	AppendLine(Source, FString::Printf(TEXT("%sRecordConstructorFailureBegin(%d);"), Indent, Stage));
	if (FailureStage(Failure) == Stage)
	{
		AppendLine(Source, FString::Printf(TEXT("%sint Trigger = 1 / 0;"), Indent));
	}
	AppendLine(Source, FString::Printf(TEXT("%sRecordConstructorFailureComplete(%d, %d);"), Indent, Stage, Stage * 10));
}

FString MemberType(
	const ECtorOrderFailureFailure Failure,
	const ECtorOrderFailureDepth Depth,
	const TCHAR* StageName)
{
	const FString FailureTag = Compact(FailureToken(Failure));
	const FString DepthTag = Compact(DepthToken(Depth));
	if (Depth == ECtorOrderFailureDepth::FlatMembers)
	{
		return FString::Printf(TEXT("F%sFailureValue%s%s"), StageName, *DepthTag, *FailureTag);
	}
	if (Depth == ECtorOrderFailureDepth::DeepNestedMembers)
	{
		return FString::Printf(TEXT("FDeep%sFailureWrapper%s%s"), StageName, *DepthTag, *FailureTag);
	}
	return FString::Printf(TEXT("F%sFailureWrapper%s%s"), StageName, *DepthTag, *FailureTag);
}

FString MemberValue(const ECtorOrderFailureDepth Depth, const TCHAR* PropertyName)
{
	if (Depth == ECtorOrderFailureDepth::FlatMembers)
	{
		return FString::Printf(TEXT("%s.Value"), PropertyName);
	}
	if (Depth == ECtorOrderFailureDepth::DeepNestedMembers)
	{
		return FString::Printf(TEXT("%s.Inner.Value.Value"), PropertyName);
	}
	return FString::Printf(TEXT("%s.Value.Value"), PropertyName);
}

void AppendHelpers(FString& Source)
{
	AppendLine(Source, TEXT("void RecordConstructorFailureBegin(int Stage)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("void RecordConstructorFailureComplete(int Stage, int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("void RecordConstructorFailureDestroy(int Stage)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("int ConvertConstructorFailureInput(int Value)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FNativeCaseValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendStageValueType(FString& Source, const TCHAR* TypeName, const int32 Stage, const int32 Value, const ECtorOrderFailureFailure Failure)
{
	AppendLine(Source, FString::Printf(TEXT("struct %s"), TypeName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\tint Value = %d;"), Value));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("\t%s()"), TypeName));
	AppendLine(Source, TEXT("\t{"));
	AppendMaybeFail(Source, Stage, Failure, TEXT("\t\t"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("\t~%s()"), TypeName));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, FString::Printf(TEXT("\t\tRecordConstructorFailureDestroy(%d);"), Stage));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendSharedDepthTypes(FString& Source, const ECtorOrderFailureFailure Failure, const ECtorOrderFailureDepth Depth)
{
	if (Depth == ECtorOrderFailureDepth::FlatMembers)
	{
		return;
	}
	const FString FailureTag = Compact(FailureToken(Failure));
	const FString DepthTag = Compact(DepthToken(Depth));
	const bool bTracked = Depth == ECtorOrderFailureDepth::BaseAndDerivedMembers;
	auto AppendWrapper = [&Source, bTracked](const FString& WrapperName, const FString& InnerType, const TCHAR* PropertyName)
	{
		AppendLine(Source, FString::Printf(TEXT("struct %s"), *WrapperName));
		AppendLine(Source, TEXT("{"));
		if (bTracked)
		{
			AppendLine(Source, TEXT("\tFNativeCaseValue Context;"));
		}
		AppendLine(Source, FString::Printf(TEXT("\t%s %s;"), *InnerType, PropertyName));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	};
	AppendWrapper(
		FString::Printf(TEXT("FFirstFailureWrapper%s%s"), *DepthTag, *FailureTag),
		FString::Printf(TEXT("FFirstFailureValue%s%s"), *DepthTag, *FailureTag),
		TEXT("Value"));
	AppendWrapper(
		FString::Printf(TEXT("FMiddleFailureWrapper%s%s"), *DepthTag, *FailureTag),
		FString::Printf(TEXT("FMiddleFailureValue%s%s"), *DepthTag, *FailureTag),
		TEXT("Value"));
	AppendWrapper(
		FString::Printf(TEXT("FLastFailureWrapper%s%s"), *DepthTag, *FailureTag),
		FString::Printf(TEXT("FLastFailureValue%s%s"), *DepthTag, *FailureTag),
		TEXT("Value"));
	if (Depth == ECtorOrderFailureDepth::DeepNestedMembers)
	{
		AppendWrapper(
			FString::Printf(TEXT("FDeepFirstFailureWrapper%s%s"), *DepthTag, *FailureTag),
			FString::Printf(TEXT("FFirstFailureWrapper%s%s"), *DepthTag, *FailureTag),
			TEXT("Inner"));
		AppendWrapper(
			FString::Printf(TEXT("FDeepMiddleFailureWrapper%s%s"), *DepthTag, *FailureTag),
			FString::Printf(TEXT("FMiddleFailureWrapper%s%s"), *DepthTag, *FailureTag),
			TEXT("Inner"));
		AppendWrapper(
			FString::Printf(TEXT("FDeepLastFailureWrapper%s%s"), *DepthTag, *FailureTag),
			FString::Printf(TEXT("FLastFailureWrapper%s%s"), *DepthTag, *FailureTag),
			TEXT("Inner"));
	}
}

void AppendGraph(FString& Source, const ECtorOrderFailureFailure Failure, const ECtorOrderFailureDepth Depth)
{
	const FString Name = GraphName(Failure, Depth);
	AppendLine(Source, FString::Printf(TEXT("class %s"), *Name));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint BaseValue = 10;"));
	if (Depth == ECtorOrderFailureDepth::BaseAndDerivedMembers)
	{
		AppendLine(Source, TEXT("\tFNativeCaseValue DerivedPrefix;"));
	}
	AppendLine(Source, FString::Printf(TEXT("\t%s First;"), *MemberType(Failure, Depth, TEXT("First"))));
	AppendLine(Source, FString::Printf(TEXT("\t%s Middle;"), *MemberType(Failure, Depth, TEXT("Middle"))));
	AppendLine(Source, FString::Printf(TEXT("\t%s Last;"), *MemberType(Failure, Depth, TEXT("Last"))));
	if (Depth == ECtorOrderFailureDepth::BaseAndDerivedMembers)
	{
		AppendLine(Source, TEXT("\tFNativeCaseValue DerivedSuffix;"));
	}
	AppendLine(Source, TEXT("\tint DerivedValue = 50;"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("\t%s()"), *Name));
	AppendLine(Source, TEXT("\t{"));
	AppendMaybeFail(Source, 1, Failure, TEXT("\t\t"));
	AppendMaybeFail(Source, 5, Failure, TEXT("\t\t"));
	if (Failure == ECtorOrderFailureFailure::Copy)
	{
		AppendLine(Source, TEXT("\t\tRecordConstructorFailureBegin(6);"));
		AppendLine(Source, TEXT("\t\tFNativeCaseValue Source;"));
		AppendLine(Source, TEXT("\t\tint Trigger = 1 / 0;"));
		AppendLine(Source, TEXT("\t\tRecordConstructorFailureComplete(6, 60);"));
	}
	else if (Failure == ECtorOrderFailureFailure::Conversion)
	{
		AppendLine(Source, TEXT("\t\tRecordConstructorFailureBegin(7);"));
		AppendLine(Source, TEXT("\t\tint Converted = ConvertConstructorFailureInput(7);"));
		AppendLine(Source, TEXT("\t\tint Trigger = 1 / 0;"));
		AppendLine(Source, TEXT("\t\tRecordConstructorFailureComplete(7, Converted);"));
	}
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("\tint Total()"));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, FString::Printf(
		TEXT("\t\treturn BaseValue + %s + %s + %s + DerivedValue;"),
		*MemberValue(Depth, TEXT("First")),
		*MemberValue(Depth, TEXT("Middle")),
		*MemberValue(Depth, TEXT("Last"))));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendOwnedTypes(FString& Source, const ECtorOrderFailureFailure Failure, const ECtorOrderFailureDepth Depth)
{
	const FString FailureTag = Compact(FailureToken(Failure));
	const FString DepthTag = Compact(DepthToken(Depth));
	const FString FirstType = FString::Printf(TEXT("FFirstFailureValue%s%s"), *DepthTag, *FailureTag);
	const FString MiddleType = FString::Printf(TEXT("FMiddleFailureValue%s%s"), *DepthTag, *FailureTag);
	const FString LastType = FString::Printf(TEXT("FLastFailureValue%s%s"), *DepthTag, *FailureTag);
	AppendStageValueType(Source, *FirstType, 2, 20, Failure);
	AppendStageValueType(Source, *MiddleType, 3, 30, Failure);
	AppendStageValueType(Source, *LastType, 4, 40, Failure);
	AppendSharedDepthTypes(Source, Failure, Depth);
	AppendGraph(Source, Failure, Depth);
}

void AppendEntry(FString& Source, const FCtorOrderFailureParams& Params, const FString& FunctionName)
{
	const FString Name = GraphName(Params.Failure, Params.Depth);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s Graph = %s();"), *Name, *Name));
	AppendLine(Source, TEXT("\treturn Graph.Total();"));
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FCtorOrderFailureParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendHelpers(Source);
	AppendOwnedTypes(Source, Params.Failure, Params.Depth);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FCtorOrderFailureCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExecutionNotes = HostNotes;
	Info.bLimitedObservation = Cell.Observation != ECtorOrderFailureObservation::Values;
	if (IsFault(Cell.Failure))
	{
		Info.Outcome = EGeneratedCaseOutcome::RuntimeException;
		Info.ExpectedException = TEXT("Divide by zero");
		return Info;
	}
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.ExpectedReturn = 150;
	return Info;
}
}

FStringView FCtorOrderFailureGenerator::GetProductId() const
{
	return TEXT("LANG-CTOR-ORDER-FAILURE");
}

TArray<FGeneratedCaseInfo> FCtorOrderFailureGenerator::ListCases() const
{
	using namespace CtorOrderFailureGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FCtorOrderFailureCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		const FString EntryName = MakeEntryName(Cell);
		checkf(!EntryNames.Contains(EntryName), TEXT("CtorOrderFailure PascalCase collision: %s"), *EntryName);
		EntryNames.Add(EntryName);
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FCtorOrderFailureGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace CtorOrderFailureGeneratorDetail;
	FCtorOrderFailureCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FCtorOrderFailureGenerator::BuildDumpSource() const
{
	using namespace CtorOrderFailureGeneratorDetail;
	const TArray<FCtorOrderFailureCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CTOR-ORDER-FAILURE"));
	AppendLine(Source, TEXT("// Cases: 128 total, 16 normal, 0 reject, 112 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendHelpers(Source);
	TSet<FString> EmittedGraphs;
	for (const FCtorOrderFailureCell& Cell : Cells)
	{
		const FString Key = GraphName(Cell.Failure, Cell.Depth);
		if (!EmittedGraphs.Contains(Key))
		{
			AppendOwnedTypes(Source, Cell.Failure, Cell.Depth);
			EmittedGraphs.Add(Key);
		}
	}
	bool bNeedBlank = false;
	for (const FCtorOrderFailureCell& Cell : Cells)
	{
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (bNeedBlank)
		{
			AppendLine(Source);
		}
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		if (IsFault(Cell.Failure))
		{
			AppendLine(Source, TEXT("// Outcome: RuntimeException"));
			AppendLine(Source, TEXT("// ExpectedException: Divide by zero"));
		}
		else
		{
			AppendLine(Source, TEXT("// Outcome: ReturnValue"));
			AppendLine(Source, TEXT("// ExpectedReturn: 150"));
		}
		AppendLine(Source, TEXT("// Execution: RequiresHostSetup"));
		AppendLine(Source);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
		bNeedBlank = true;
	}
	return Source;
}

FString FCtorOrderFailureGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace CtorOrderFailureGeneratorDetail;
	const TArray<FCtorOrderFailureCell> Cells = AllCells();
	FString Source;
	AppendHelpers(Source);
	TSet<FString> EmittedGraphs;
	for (const FCtorOrderFailureCell& Cell : Cells)
	{
		const FString Key = GraphName(Cell.Failure, Cell.Depth);
		if (!EmittedGraphs.Contains(Key))
		{
			AppendOwnedTypes(Source, Cell.Failure, Cell.Depth);
			EmittedGraphs.Add(Key);
		}
	}
	int32 Emitted = 0;
	for (const FCtorOrderFailureCell& Cell : Cells)
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

FString FCtorOrderFailureGenerator::BuildConstructorFailureSource(const FCtorOrderFailureParams& Params) const
{
	using namespace CtorOrderFailureGeneratorDetail;
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

int32 FCtorOrderFailureGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace CtorOrderFailureGeneratorDetail;
	FCtorOrderFailureCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsFault(Cell.Failure))
	{
		return 0;
	}
	return 150;
}
}
