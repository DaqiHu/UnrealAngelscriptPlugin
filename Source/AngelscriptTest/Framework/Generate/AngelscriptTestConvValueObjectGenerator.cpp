#include "Framework/Generate/AngelscriptTestConvValueObjectGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace ConvValueObjectGeneratorDetail
{
constexpr int32 AvailabilityCount = 6;
constexpr int32 TargetCount = 5;
constexpr int32 FormCount = 6;
constexpr int32 OutcomeCount = 4;
constexpr int32 TotalCells = AvailabilityCount * TargetCount * FormCount * OutcomeCount;

struct FValueObjectCell
{
	EConvValueObjectAvailability Availability = EConvValueObjectAvailability::ImplicitConstructor;
	EConvValueObjectTarget Target = EConvValueObjectTarget::SameValue;
	EConvValueObjectForm Form = EConvValueObjectForm::Assignment;
	EConvValueObjectOutcome Outcome = EConvValueObjectOutcome::Direct;
};

const TCHAR* AvailabilityToken(const EConvValueObjectAvailability Availability)
{
	switch (Availability)
	{
	case EConvValueObjectAvailability::ImplicitConstructor:
		return TEXT("IMPLICIT_CONSTRUCTOR");
	case EConvValueObjectAvailability::ExplicitConstructor:
		return TEXT("EXPLICIT_CONSTRUCTOR");
	case EConvValueObjectAvailability::ImplicitOperator:
		return TEXT("IMPLICIT_OPERATOR");
	case EConvValueObjectAvailability::ExplicitOperator:
		return TEXT("EXPLICIT_OPERATOR");
	case EConvValueObjectAvailability::ConstructorAndOperator:
		return TEXT("CONSTRUCTOR_AND_OPERATOR");
	case EConvValueObjectAvailability::None:
		return TEXT("NONE");
	default:
		return nullptr;
	}
}

const TCHAR* TargetToken(const EConvValueObjectTarget Target)
{
	switch (Target)
	{
	case EConvValueObjectTarget::SameValue:
		return TEXT("SAME_VALUE");
	case EConvValueObjectTarget::OtherValue:
		return TEXT("OTHER_VALUE");
	case EConvValueObjectTarget::Int:
		return TEXT("INT");
	case EConvValueObjectTarget::Float64:
		return TEXT("FLOAT64");
	case EConvValueObjectTarget::Bool:
		return TEXT("BOOL");
	default:
		return nullptr;
	}
}

const TCHAR* FormToken(const EConvValueObjectForm Form)
{
	switch (Form)
	{
	case EConvValueObjectForm::Assignment:
		return TEXT("ASSIGNMENT");
	case EConvValueObjectForm::Initializer:
		return TEXT("INITIALIZER");
	case EConvValueObjectForm::Argument:
		return TEXT("ARGUMENT");
	case EConvValueObjectForm::Return:
		return TEXT("RETURN");
	case EConvValueObjectForm::ExplicitCast:
		return TEXT("EXPLICIT_CAST");
	case EConvValueObjectForm::DirectConstructor:
		return TEXT("DIRECT_CONSTRUCTOR");
	default:
		return nullptr;
	}
}

const TCHAR* OutcomeToken(const EConvValueObjectOutcome Outcome)
{
	switch (Outcome)
	{
	case EConvValueObjectOutcome::Direct:
		return TEXT("DIRECT");
	case EConvValueObjectOutcome::Selected:
		return TEXT("SELECTED");
	case EConvValueObjectOutcome::Ambiguous:
		return TEXT("AMBIGUOUS");
	case EConvValueObjectOutcome::Rejected:
		return TEXT("REJECTED");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FConvValueObjectParams& Params)
{
	return AvailabilityToken(Params.Availability) != nullptr
		&& TargetToken(Params.Target) != nullptr
		&& FormToken(Params.Form) != nullptr
		&& OutcomeToken(Params.Outcome) != nullptr;
}

FString CellSuffix(const FConvValueObjectParams& Params)
{
	FString Id = FString::Printf(
		TEXT("LANG-CONV-VALUE-OBJECT-%s-%s-%s-%s"),
		AvailabilityToken(Params.Availability),
		TargetToken(Params.Target),
		FormToken(Params.Form),
		OutcomeToken(Params.Outcome));
	FString Entry = MakeCanonicalEntryName(Id);
	Entry.RemoveFromStart(TEXT("EntryLangConvValueObject"));
	return Entry;
}

FString SourceTypeName(const FConvValueObjectParams& Params)
{
	return FString::Printf(TEXT("FValueConversionSource%s"), *CellSuffix(Params));
}

FString OtherTargetTypeName(const FConvValueObjectParams& Params)
{
	return FString::Printf(TEXT("FValueConversionTarget%s"), *CellSuffix(Params));
}

FString ResolveTargetType(const FConvValueObjectParams& Params)
{
	switch (Params.Target)
	{
	case EConvValueObjectTarget::SameValue:
		return SourceTypeName(Params);
	case EConvValueObjectTarget::OtherValue:
		return OtherTargetTypeName(Params);
	case EConvValueObjectTarget::Float64:
		return TEXT("double");
	case EConvValueObjectTarget::Bool:
		return TEXT("bool");
	case EConvValueObjectTarget::Int:
	default:
		return TEXT("int");
	}
}

bool IsExplicitForm(const EConvValueObjectForm Form)
{
	return Form == EConvValueObjectForm::ExplicitCast || Form == EConvValueObjectForm::DirectConstructor;
}

bool HasImplicitSelectedPath(const FConvValueObjectParams& Params)
{
	if (Params.Target == EConvValueObjectTarget::SameValue)
	{
		return true;
	}
	if (Params.Target == EConvValueObjectTarget::OtherValue)
	{
		return Params.Availability == EConvValueObjectAvailability::ImplicitConstructor
			|| Params.Availability == EConvValueObjectAvailability::ImplicitOperator
			|| Params.Availability == EConvValueObjectAvailability::ConstructorAndOperator;
	}
	return Params.Availability == EConvValueObjectAvailability::ImplicitOperator
		|| Params.Availability == EConvValueObjectAvailability::ConstructorAndOperator;
}

bool HasExplicitSelectedPath(const FConvValueObjectParams& Params)
{
	if (Params.Target == EConvValueObjectTarget::SameValue)
	{
		return true;
	}
	if (Params.Target == EConvValueObjectTarget::OtherValue)
	{
		return Params.Availability != EConvValueObjectAvailability::None;
	}
	return Params.Availability == EConvValueObjectAvailability::ImplicitOperator
		|| Params.Availability == EConvValueObjectAvailability::ExplicitOperator
		|| Params.Availability == EConvValueObjectAvailability::ConstructorAndOperator;
}

bool ShouldCompilePositivePath(const FConvValueObjectParams& Params)
{
	return IsExplicitForm(Params.Form)
		? HasExplicitSelectedPath(Params)
		: HasImplicitSelectedPath(Params);
}

bool ShouldCompile(const FConvValueObjectParams& Params)
{
	return Params.Outcome != EConvValueObjectOutcome::Ambiguous
		&& Params.Outcome != EConvValueObjectOutcome::Rejected
		&& ShouldCompilePositivePath(Params);
}

int32 ExpectedSelectedMarker(const FConvValueObjectParams& Params)
{
	if (Params.Target == EConvValueObjectTarget::SameValue)
	{
		return 7;
	}
	if (Params.Target == EConvValueObjectTarget::OtherValue)
	{
		if (Params.Availability == EConvValueObjectAvailability::ImplicitConstructor
			|| Params.Availability == EConvValueObjectAvailability::ExplicitConstructor
			|| Params.Availability == EConvValueObjectAvailability::ConstructorAndOperator)
		{
			return 107;
		}
		return Params.Availability == EConvValueObjectAvailability::ImplicitOperator ? 207 : 307;
	}
	if (Params.Target == EConvValueObjectTarget::Bool)
	{
		return Params.Availability == EConvValueObjectAvailability::ExplicitOperator ? 0 : 1;
	}
	return Params.Availability == EConvValueObjectAvailability::ExplicitOperator ? 307 : 207;
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FValueObjectCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CONV-VALUE-OBJECT-%s-%s-%s-%s"),
		AvailabilityToken(Cell.Availability),
		TargetToken(Cell.Target),
		FormToken(Cell.Form),
		OutcomeToken(Cell.Outcome));
}

FString MakeEntryName(const FValueObjectCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FConvValueObjectParams ParamsFromCell(const FValueObjectCell& Cell)
{
	FConvValueObjectParams Params;
	Params.Availability = Cell.Availability;
	Params.Target = Cell.Target;
	Params.Form = Cell.Form;
	Params.Outcome = Cell.Outcome;
	return Params;
}

FValueObjectCell CellFromIndex(const int32 Index)
{
	FValueObjectCell Cell;
	Cell.Availability = static_cast<EConvValueObjectAvailability>(Index / (TargetCount * FormCount * OutcomeCount));
	const int32 AfterAvailability = Index % (TargetCount * FormCount * OutcomeCount);
	Cell.Target = static_cast<EConvValueObjectTarget>(AfterAvailability / (FormCount * OutcomeCount));
	const int32 AfterTarget = AfterAvailability % (FormCount * OutcomeCount);
	Cell.Form = static_cast<EConvValueObjectForm>(AfterTarget / OutcomeCount);
	Cell.Outcome = static_cast<EConvValueObjectOutcome>(AfterTarget % OutcomeCount);
	return Cell;
}

bool TryFindCell(const FStringView CaseId, FValueObjectCell& OutCell)
{
	const FString Needle(CaseId);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FValueObjectCell Cell = CellFromIndex(Index);
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendTargetValueDefinition(FString& Source, const FConvValueObjectParams& Params)
{
	const FString TargetName = OtherTargetTypeName(Params);
	const FString SourceName = SourceTypeName(Params);
	AppendLine(Source, FString::Printf(TEXT("struct %s;"), *SourceName));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("struct %s"), *TargetName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value;"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("\t%s(int InValue)"), *TargetName));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	if (Params.Availability == EConvValueObjectAvailability::ImplicitConstructor
		|| Params.Availability == EConvValueObjectAvailability::ConstructorAndOperator)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s(const %s&in InValue)"), *TargetName, *SourceName));
	}
	else if (Params.Availability == EConvValueObjectAvailability::ExplicitConstructor)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s(const %s&in InValue) explicit"), *TargetName, *SourceName));
	}
	if (Params.Availability == EConvValueObjectAvailability::ImplicitConstructor
		|| Params.Availability == EConvValueObjectAvailability::ExplicitConstructor
		|| Params.Availability == EConvValueObjectAvailability::ConstructorAndOperator)
	{
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = 107;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendSourceConversionDefinition(FString& Source, const FConvValueObjectParams& Params)
{
	const FString SourceName = SourceTypeName(Params);
	const FString TargetType = ResolveTargetType(Params);
	const bool bImplicitOperator = Params.Availability == EConvValueObjectAvailability::ImplicitOperator
		|| Params.Availability == EConvValueObjectAvailability::ConstructorAndOperator;
	const bool bExplicitOperator = Params.Availability == EConvValueObjectAvailability::ExplicitOperator;
	const FString OperatorReturnType = Params.Target == EConvValueObjectTarget::SameValue
		? FString(TEXT("int"))
		: TargetType;
	FString ImplicitReturn;
	FString ExplicitReturn;
	if (Params.Target == EConvValueObjectTarget::OtherValue)
	{
		ImplicitReturn = FString::Printf(TEXT("%s(Value + 200)"), *OtherTargetTypeName(Params));
		ExplicitReturn = FString::Printf(TEXT("%s(Value + 300)"), *OtherTargetTypeName(Params));
	}
	else if (Params.Target == EConvValueObjectTarget::Bool)
	{
		ImplicitReturn = TEXT("true");
		ExplicitReturn = TEXT("false");
	}
	else
	{
		ImplicitReturn = FString::Printf(TEXT("%s(Value + 200)"), *OperatorReturnType);
		ExplicitReturn = FString::Printf(TEXT("%s(Value + 300)"), *OperatorReturnType);
	}

	AppendLine(Source, FString::Printf(TEXT("struct %s"), *SourceName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value;"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("\t%s(int InValue)"), *SourceName));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, TEXT("\t\tValue = InValue;"));
	AppendLine(Source, TEXT("\t}"));
	AppendLine(Source);
	if (bImplicitOperator)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s opImplConv() const"), *OperatorReturnType));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn ") + ImplicitReturn + TEXT(";"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
	}
	if (bExplicitOperator)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s opConv() const"), *OperatorReturnType));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn ") + ExplicitReturn + TEXT(";"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendTargetObservationHelpers(FString& Source, const FConvValueObjectParams& Params)
{
	const FString TargetType = ResolveTargetType(Params);
	const FString Suffix = CellSuffix(Params);
	AppendLine(Source, FString::Printf(TEXT("int ObserveValueConversion%s(%s Value)"), *Suffix, *TargetType));
	AppendLine(Source, TEXT("{"));
	if (Params.Target == EConvValueObjectTarget::SameValue || Params.Target == EConvValueObjectTarget::OtherValue)
	{
		AppendLine(Source, TEXT("\treturn Value.Value;"));
	}
	else if (Params.Target == EConvValueObjectTarget::Bool)
	{
		AppendLine(Source, TEXT("\treturn Value ? 1 : 0;"));
	}
	else
	{
		AppendLine(Source, TEXT("\treturn int(Value);"));
	}
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, FString::Printf(TEXT("%s ReturnValueConversionTarget%s(%s Value)"), *TargetType, *Suffix, *TargetType));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\treturn Value;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendAmbiguousOrRejectedSource(FString& Source, const FConvValueObjectParams& Params)
{
	const FString Suffix = CellSuffix(Params);
	const FString SourceName = SourceTypeName(Params);
	if (Params.Outcome == EConvValueObjectOutcome::Ambiguous)
	{
		AppendLine(Source, FString::Printf(TEXT("struct FValueAmbiguousLeft%s"), *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tFValueAmbiguousLeft%s(const %s&in Value)"), *Suffix, *SourceName));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("struct FValueAmbiguousRight%s"), *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tFValueAmbiguousRight%s(const %s&in Value)"), *Suffix, *SourceName));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int SelectValueAmbiguity%s(FValueAmbiguousLeft%s Value)"), *Suffix, *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn 1;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int SelectValueAmbiguity%s(FValueAmbiguousRight%s Value)"), *Suffix, *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn 2;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("struct FValueRejectedTarget%s"), *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("int RejectValueConversion%s(FValueRejectedTarget%s Value)"), *Suffix, *Suffix));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\treturn Value.Value;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
	}
}

void AppendOwnedHelpers(FString& Source, const FConvValueObjectParams& Params)
{
	AppendTargetValueDefinition(Source, Params);
	AppendSourceConversionDefinition(Source, Params);
	AppendTargetObservationHelpers(Source, Params);
	if (Params.Outcome == EConvValueObjectOutcome::Ambiguous || Params.Outcome == EConvValueObjectOutcome::Rejected)
	{
		AppendAmbiguousOrRejectedSource(Source, Params);
	}
}

void AppendEntry(FString& Source, const FConvValueObjectParams& Params, const FString& FunctionName)
{
	const FString SourceName = SourceTypeName(Params);
	const FString TargetType = ResolveTargetType(Params);
	const FString Suffix = CellSuffix(Params);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, FString::Printf(TEXT("\t%s SourceValue(7);"), *SourceName));
	if (Params.Outcome == EConvValueObjectOutcome::Ambiguous)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn SelectValueAmbiguity%s(SourceValue);"), *Suffix));
	}
	else if (Params.Outcome == EConvValueObjectOutcome::Rejected)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn RejectValueConversion%s(SourceValue);"), *Suffix));
	}
	else if (Params.Form == EConvValueObjectForm::Assignment)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s TargetValue;"), *TargetType));
		AppendLine(Source, TEXT("\tTargetValue = SourceValue;"));
		AppendLine(Source, FString::Printf(TEXT("\treturn ObserveValueConversion%s(TargetValue);"), *Suffix));
	}
	else if (Params.Form == EConvValueObjectForm::Initializer)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s TargetValue = SourceValue;"), *TargetType));
		AppendLine(Source, FString::Printf(TEXT("\treturn ObserveValueConversion%s(TargetValue);"), *Suffix));
	}
	else if (Params.Form == EConvValueObjectForm::Argument)
	{
		AppendLine(Source, FString::Printf(TEXT("\treturn ObserveValueConversion%s(SourceValue);"), *Suffix));
	}
	else if (Params.Form == EConvValueObjectForm::Return)
	{
		AppendLine(Source, FString::Printf(
			TEXT("\t%s TargetValue = ReturnValueConversionTarget%s(SourceValue);"),
			*TargetType,
			*Suffix));
		AppendLine(Source, FString::Printf(TEXT("\treturn ObserveValueConversion%s(TargetValue);"), *Suffix));
	}
	else if (Params.Form == EConvValueObjectForm::ExplicitCast)
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s TargetValue = %s(SourceValue);"), *TargetType, *TargetType));
		AppendLine(Source, FString::Printf(TEXT("\treturn ObserveValueConversion%s(TargetValue);"), *Suffix));
	}
	else
	{
		AppendLine(Source, FString::Printf(TEXT("\t%s TargetValue(SourceValue);"), *TargetType));
		AppendLine(Source, FString::Printf(TEXT("\treturn ObserveValueConversion%s(TargetValue);"), *Suffix));
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FConvValueObjectParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendOwnedHelpers(Source, Params);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FValueObjectCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	const FConvValueObjectParams Params = ParamsFromCell(Cell);
	if (!ShouldCompile(Params))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::Standalone;
	Info.ExpectedReturn = ExpectedSelectedMarker(Params);
	return Info;
}
}

FStringView FConvValueObjectGenerator::GetProductId() const
{
	return TEXT("LANG-CONV-VALUE-OBJECT");
}

TArray<FGeneratedCaseInfo> FConvValueObjectGenerator::ListCases() const
{
	using namespace ConvValueObjectGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	Cases.Reserve(TotalCells);
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FValueObjectCell Cell = CellFromIndex(Index);
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("ConvValueObject PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FConvValueObjectGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace ConvValueObjectGeneratorDetail;
	FValueObjectCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FConvValueObjectGenerator::BuildDumpSource() const
{
	using namespace ConvValueObjectGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CONV-VALUE-OBJECT"));
	AppendLine(Source, TEXT("// Cases: 720 total, 200 normal, 520 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FValueObjectCell Cell = CellFromIndex(Index);
		const FConvValueObjectParams Params = ParamsFromCell(Cell);
		if (!ShouldCompile(Params))
		{
			continue;
		}
		const FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("// Case: %s"), *Info.CaseId));
		AppendLine(Source, FString::Printf(TEXT("// EntryDeclaration: %s"), *Info.EntryDeclaration));
		AppendLine(Source, TEXT("// Outcome: ReturnValue"));
		AppendLine(Source, FString::Printf(TEXT("// ExpectedReturn: %d"), Info.ExpectedReturn.Get(0)));
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendOwnedHelpers(Source, Params);
		AppendEntry(Source, Params, MakeEntryName(Cell));
	}
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FValueObjectCell Cell = CellFromIndex(Index);
		const FConvValueObjectParams Params = ParamsFromCell(Cell);
		if (ShouldCompile(Params))
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
		AppendOwnedHelpers(Source, Params);
		AppendEntry(Source, Params, MakeEntryName(Cell));
	}
	return Source;
}

FString FConvValueObjectGenerator::BuildValueObjectConversionSource(const FConvValueObjectParams& Params) const
{
	using namespace ConvValueObjectGeneratorDetail;
	if (!IsValidParams(Params) || !ShouldCompile(Params))
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

FString FConvValueObjectGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace ConvValueObjectGeneratorDetail;
	FString Source;
	int32 Emitted = 0;
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FValueObjectCell Cell = CellFromIndex(Index);
		const FConvValueObjectParams Params = ParamsFromCell(Cell);
		if (!ShouldCompile(Params))
		{
			continue;
		}
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		AppendOwnedHelpers(Source, Params);
		AppendEntry(Source, Params, MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

int32 FConvValueObjectGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace ConvValueObjectGeneratorDetail;
	FValueObjectCell Cell;
	if (!TryFindCell(CaseId, Cell) || !ShouldCompile(ParamsFromCell(Cell)))
	{
		return 0;
	}
	return ExpectedSelectedMarker(ParamsFromCell(Cell));
}

FString FConvValueObjectGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace ConvValueObjectGeneratorDetail;
	FValueObjectCell Cell;
	if (!TryFindCell(CaseId, Cell) || ShouldCompile(ParamsFromCell(Cell)))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FConvValueObjectGenerator::ListRejectCaseIds() const
{
	using namespace ConvValueObjectGeneratorDetail;
	TArray<FString> Ids;
	for (int32 Index = 0; Index < TotalCells; ++Index)
	{
		const FValueObjectCell Cell = CellFromIndex(Index);
		if (!ShouldCompile(ParamsFromCell(Cell)))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
