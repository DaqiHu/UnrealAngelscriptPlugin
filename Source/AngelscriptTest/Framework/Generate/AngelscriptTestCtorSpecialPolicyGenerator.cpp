#include "Framework/Generate/AngelscriptTestCtorSpecialPolicyGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace CtorSpecialPolicyGeneratorDetail
{
constexpr const TCHAR* HostNotes = TEXT("Needs native constructor-policy host markers.");

struct FCtorSpecialPolicyCell
{
	ECtorSpecialPolicyObservation Observation = ECtorSpecialPolicyObservation::Compile;
	ECtorSpecialPolicyScenario Scenario = ECtorSpecialPolicyScenario::ImplicitStructDefault;
};

const TCHAR* ObservationToken(const ECtorSpecialPolicyObservation Observation)
{
	switch (Observation)
	{
	case ECtorSpecialPolicyObservation::Compile:
		return TEXT("COMPILE");
	case ECtorSpecialPolicyObservation::Metadata:
		return TEXT("METADATA");
	case ECtorSpecialPolicyObservation::Runtime:
		return TEXT("RUNTIME");
	case ECtorSpecialPolicyObservation::Lifecycle:
		return TEXT("LIFECYCLE");
	default:
		return nullptr;
	}
}

const TCHAR* ScenarioToken(const ECtorSpecialPolicyScenario Scenario)
{
	switch (Scenario)
	{
	case ECtorSpecialPolicyScenario::ImplicitStructDefault:
		return TEXT("IMPLICIT_STRUCT_DEFAULT");
	case ECtorSpecialPolicyScenario::DeclaredStructDefault:
		return TEXT("DECLARED_STRUCT_DEFAULT");
	case ECtorSpecialPolicyScenario::ParameterPreservesGeneratedDefault:
		return TEXT("PARAMETER_PRESERVES_GENERATED_DEFAULT");
	case ECtorSpecialPolicyScenario::ParameterSuppressesDefaultOptionOff:
		return TEXT("PARAMETER_SUPPRESSES_DEFAULT_OPTION_OFF");
	case ECtorSpecialPolicyScenario::ImplicitStructCopy:
		return TEXT("IMPLICIT_STRUCT_COPY");
	case ECtorSpecialPolicyScenario::DeclaredStructCopy:
		return TEXT("DECLARED_STRUCT_COPY");
	case ECtorSpecialPolicyScenario::ImplicitStructAssignment:
		return TEXT("IMPLICIT_STRUCT_ASSIGNMENT");
	case ECtorSpecialPolicyScenario::DeclaredStructAssignment:
		return TEXT("DECLARED_STRUCT_ASSIGNMENT");
	case ECtorSpecialPolicyScenario::UserDestructorCopy:
		return TEXT("USER_DESTRUCTOR_COPY");
	case ECtorSpecialPolicyScenario::ClassFactoryDefault:
		return TEXT("CLASS_FACTORY_DEFAULT");
	case ECtorSpecialPolicyScenario::ClassParameterFactory:
		return TEXT("CLASS_PARAMETER_FACTORY");
	case ECtorSpecialPolicyScenario::DerivedGeneratedDefault:
		return TEXT("DERIVED_GENERATED_DEFAULT");
	case ECtorSpecialPolicyScenario::DerivedExplicitSuper:
		return TEXT("DERIVED_EXPLICIT_SUPER");
	case ECtorSpecialPolicyScenario::MissingBaseDefaultOptionOff:
		return TEXT("MISSING_BASE_DEFAULT_OPTION_OFF");
	case ECtorSpecialPolicyScenario::CopyAfterUserConstructor:
		return TEXT("COPY_AFTER_USER_CONSTRUCTOR");
	case ECtorSpecialPolicyScenario::AssignmentSelfStability:
		return TEXT("ASSIGNMENT_SELF_STABILITY");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FCtorSpecialPolicyParams& Params)
{
	return ObservationToken(Params.Observation) != nullptr && ScenarioToken(Params.Scenario) != nullptr;
}

bool IsReject(const ECtorSpecialPolicyScenario Scenario)
{
	switch (Scenario)
	{
	case ECtorSpecialPolicyScenario::ParameterSuppressesDefaultOptionOff:
	case ECtorSpecialPolicyScenario::ImplicitStructCopy:
	case ECtorSpecialPolicyScenario::UserDestructorCopy:
	case ECtorSpecialPolicyScenario::MissingBaseDefaultOptionOff:
	case ECtorSpecialPolicyScenario::CopyAfterUserConstructor:
		return true;
	default:
		return false;
	}
}

int32 ExpectedFromScenario(const ECtorSpecialPolicyScenario Scenario)
{
	switch (Scenario)
	{
	case ECtorSpecialPolicyScenario::DeclaredStructDefault:
		return 12;
	case ECtorSpecialPolicyScenario::ParameterPreservesGeneratedDefault:
		return 13;
	case ECtorSpecialPolicyScenario::DeclaredStructCopy:
		return 32;
	case ECtorSpecialPolicyScenario::ImplicitStructAssignment:
		return 34;
	case ECtorSpecialPolicyScenario::DeclaredStructAssignment:
		return 36;
	case ECtorSpecialPolicyScenario::ClassFactoryDefault:
		return 20;
	case ECtorSpecialPolicyScenario::ClassParameterFactory:
		return 21;
	case ECtorSpecialPolicyScenario::DerivedGeneratedDefault:
		return 34;
	case ECtorSpecialPolicyScenario::DerivedExplicitSuper:
		return 26;
	case ECtorSpecialPolicyScenario::AssignmentSelfStability:
		return 84;
	default:
		return 0;
	}
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FCtorSpecialPolicyCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-CTOR-SPECIAL-POLICY-%s-%s"),
		ObservationToken(Cell.Observation),
		ScenarioToken(Cell.Scenario));
}

FString MakeEntryName(const FCtorSpecialPolicyCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FString TypeTag(const ECtorSpecialPolicyScenario Scenario)
{
	FString Tag(ScenarioToken(Scenario));
	Tag.ReplaceInline(TEXT("_"), TEXT(""));
	return Tag;
}

FCtorSpecialPolicyParams ParamsFromCell(const FCtorSpecialPolicyCell& Cell)
{
	FCtorSpecialPolicyParams Params;
	Params.Observation = Cell.Observation;
	Params.Scenario = Cell.Scenario;
	return Params;
}

TArray<FCtorSpecialPolicyCell> AllCells()
{
	TArray<FCtorSpecialPolicyCell> Cells;
	Cells.SetNum(64);
	for (int32 Index = 0; Index < 64; ++Index)
	{
		FCtorSpecialPolicyCell& Cell = Cells[Index];
		Cell.Observation = static_cast<ECtorSpecialPolicyObservation>(Index / 16);
		Cell.Scenario = static_cast<ECtorSpecialPolicyScenario>(Index % 16);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FCtorSpecialPolicyCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FCtorSpecialPolicyCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendHelpers(FString& Source)
{
	AppendLine(Source, TEXT("void RecordConstructorPolicyMarker(int Marker)"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
	AppendLine(Source, TEXT("struct FNativeCaseValue"));
	AppendLine(Source, TEXT("{"));
	AppendLine(Source, TEXT("\tint Value = 0;"));
	AppendLine(Source, TEXT("}"));
	AppendLine(Source);
}

void AppendScenarioTypes(FString& Source, const ECtorSpecialPolicyScenario Scenario)
{
	const FString Tag = TypeTag(Scenario);
	switch (Scenario)
	{
	case ECtorSpecialPolicyScenario::ImplicitStructDefault:
		AppendLine(Source, FString::Printf(TEXT("struct FPolicyValue%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 0;"));
		AppendLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorSpecialPolicyScenario::DeclaredStructDefault:
		AppendLine(Source, FString::Printf(TEXT("struct FPolicyValue%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 0;"));
		AppendLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = 12;"));
		AppendLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(201);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorSpecialPolicyScenario::ParameterPreservesGeneratedDefault:
		AppendLine(Source, FString::Printf(TEXT("struct FPolicyValue%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 0;"));
		AppendLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s(int InValue)"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = InValue;"));
		AppendLine(Source, TEXT("\t\tPayload.Value = InValue;"));
		AppendLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(301);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorSpecialPolicyScenario::ParameterSuppressesDefaultOptionOff:
		AppendLine(Source, FString::Printf(TEXT("struct FPolicyValue%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 0;"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s(int InValue)"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = InValue;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorSpecialPolicyScenario::ImplicitStructCopy:
		AppendLine(Source, FString::Printf(TEXT("struct FPolicyValue%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 0;"));
		AppendLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorSpecialPolicyScenario::DeclaredStructCopy:
		AppendLine(Source, FString::Printf(TEXT("struct FPolicyValue%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 0;"));
		AppendLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(601);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s(const FPolicyValue%s& Other)"), *Tag, *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = Other.Value;"));
		AppendLine(Source, TEXT("\t\tPayload = Other.Payload;"));
		AppendLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(602);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorSpecialPolicyScenario::ImplicitStructAssignment:
		AppendLine(Source, FString::Printf(TEXT("struct FPolicyValue%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 0;"));
		AppendLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorSpecialPolicyScenario::DeclaredStructAssignment:
		AppendLine(Source, FString::Printf(TEXT("struct FPolicyValue%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 0;"));
		AppendLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s& opAssign(const FPolicyValue%s& Other)"), *Tag, *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = Other.Value;"));
		AppendLine(Source, TEXT("\t\tPayload = Other.Payload;"));
		AppendLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(801);"));
		AppendLine(Source, TEXT("\t\treturn this;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorSpecialPolicyScenario::UserDestructorCopy:
		AppendLine(Source, FString::Printf(TEXT("struct FPolicyValue%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 0;"));
		AppendLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\t~FPolicyValue%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(902);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorSpecialPolicyScenario::ClassFactoryDefault:
		AppendLine(Source, FString::Printf(TEXT("class FPolicyClass%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 0;"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyClass%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = 20;"));
		AppendLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(1001);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorSpecialPolicyScenario::ClassParameterFactory:
		AppendLine(Source, FString::Printf(TEXT("class FPolicyClass%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 0;"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyClass%s(int InValue)"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = InValue;"));
		AppendLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(1101);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorSpecialPolicyScenario::DerivedGeneratedDefault:
		AppendLine(Source, FString::Printf(TEXT("class FPolicyBase%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint BaseValue = 0;"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyBase%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tBaseValue = 22;"));
		AppendLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(1201);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("class FPolicyDerived%s : FPolicyBase%s"), *Tag, *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint DerivedValue = 12;"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorSpecialPolicyScenario::DerivedExplicitSuper:
		AppendLine(Source, FString::Printf(TEXT("class FPolicyBase%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint BaseValue = 0;"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyBase%s(int InValue)"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tBaseValue = InValue;"));
		AppendLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(1301);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("class FPolicyDerived%s : FPolicyBase%s"), *Tag, *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint DerivedValue = 13;"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyDerived%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tsuper(13);"));
		AppendLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(1302);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorSpecialPolicyScenario::MissingBaseDefaultOptionOff:
		AppendLine(Source, FString::Printf(TEXT("class FPolicyBase%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyBase%s(int InValue)"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("class FPolicyDerived%s : FPolicyBase%s"), *Tag, *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyDerived%s()"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorSpecialPolicyScenario::CopyAfterUserConstructor:
		AppendLine(Source, FString::Printf(TEXT("struct FPolicyValue%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 0;"));
		AppendLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s(int InValue)"), *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = InValue;"));
		AppendLine(Source, TEXT("\t\tPayload.Value = InValue;"));
		AppendLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(1501);"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	case ECtorSpecialPolicyScenario::AssignmentSelfStability:
		AppendLine(Source, FString::Printf(TEXT("struct FPolicyValue%s"), *Tag));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 0;"));
		AppendLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s& opAssign(const FPolicyValue%s& Other)"), *Tag, *Tag));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\tValue = Other.Value;"));
		AppendLine(Source, TEXT("\t\tPayload = Other.Payload;"));
		AppendLine(Source, TEXT("\t\tRecordConstructorPolicyMarker(1601);"));
		AppendLine(Source, TEXT("\t\treturn this;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		break;
	default:
		break;
	}
	AppendLine(Source);
}

void AppendEntry(FString& Source, const FCtorSpecialPolicyParams& Params, const FString& FunctionName)
{
	const FString Tag = TypeTag(Params.Scenario);
	AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
	AppendLine(Source, TEXT("{"));
	switch (Params.Scenario)
	{
	case ECtorSpecialPolicyScenario::ImplicitStructDefault:
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s Value;"), *Tag));
		AppendLine(Source, TEXT("\tRecordConstructorPolicyMarker(101);"));
		AppendLine(Source, TEXT("\treturn Value.Value + Value.Payload.Value;"));
		break;
	case ECtorSpecialPolicyScenario::DeclaredStructDefault:
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s Value;"), *Tag));
		AppendLine(Source, TEXT("\tRecordConstructorPolicyMarker(202);"));
		AppendLine(Source, TEXT("\treturn Value.Value + Value.Payload.Value;"));
		break;
	case ECtorSpecialPolicyScenario::ParameterPreservesGeneratedDefault:
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s DefaultValue;"), *Tag));
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s ParameterValue(13);"), *Tag));
		AppendLine(Source, TEXT("\tRecordConstructorPolicyMarker(302);"));
		AppendLine(Source, TEXT("\treturn DefaultValue.Value + ParameterValue.Value;"));
		break;
	case ECtorSpecialPolicyScenario::ParameterSuppressesDefaultOptionOff:
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s Value;"), *Tag));
		AppendLine(Source, TEXT("\treturn Value.Value;"));
		break;
	case ECtorSpecialPolicyScenario::ImplicitStructCopy:
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s Source;"), *Tag));
		AppendLine(Source, TEXT("\tSource.Value = 15;"));
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s Target(Source);"), *Tag));
		AppendLine(Source, TEXT("\tRecordConstructorPolicyMarker(501);"));
		AppendLine(Source, TEXT("\treturn Target.Value + Target.Payload.Value;"));
		break;
	case ECtorSpecialPolicyScenario::DeclaredStructCopy:
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s Source;"), *Tag));
		AppendLine(Source, TEXT("\tSource.Value = 16;"));
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s Target(Source);"), *Tag));
		AppendLine(Source, TEXT("\tRecordConstructorPolicyMarker(603);"));
		AppendLine(Source, TEXT("\treturn Target.Value + Target.Payload.Value;"));
		break;
	case ECtorSpecialPolicyScenario::ImplicitStructAssignment:
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s Source;"), *Tag));
		AppendLine(Source, TEXT("\tSource.Value = 17;"));
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s Target;"), *Tag));
		AppendLine(Source, TEXT("\tTarget = Source;"));
		AppendLine(Source, TEXT("\tRecordConstructorPolicyMarker(701);"));
		AppendLine(Source, TEXT("\treturn Target.Value + Target.Payload.Value;"));
		break;
	case ECtorSpecialPolicyScenario::DeclaredStructAssignment:
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s Source;"), *Tag));
		AppendLine(Source, TEXT("\tSource.Value = 18;"));
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s Target;"), *Tag));
		AppendLine(Source, TEXT("\tTarget = Source;"));
		AppendLine(Source, TEXT("\tRecordConstructorPolicyMarker(802);"));
		AppendLine(Source, TEXT("\treturn Target.Value + Target.Payload.Value;"));
		break;
	case ECtorSpecialPolicyScenario::UserDestructorCopy:
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s Source;"), *Tag));
		AppendLine(Source, TEXT("\tSource.Value = 19;"));
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s Target(Source);"), *Tag));
		AppendLine(Source, TEXT("\tRecordConstructorPolicyMarker(901);"));
		AppendLine(Source, TEXT("\treturn Target.Value + Target.Payload.Value;"));
		break;
	case ECtorSpecialPolicyScenario::ClassFactoryDefault:
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyClass%s Value = FPolicyClass%s();"), *Tag, *Tag));
		AppendLine(Source, TEXT("\tRecordConstructorPolicyMarker(1002);"));
		AppendLine(Source, TEXT("\treturn Value.Value;"));
		break;
	case ECtorSpecialPolicyScenario::ClassParameterFactory:
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyClass%s Value = FPolicyClass%s(21);"), *Tag, *Tag));
		AppendLine(Source, TEXT("\tRecordConstructorPolicyMarker(1102);"));
		AppendLine(Source, TEXT("\treturn Value.Value;"));
		break;
	case ECtorSpecialPolicyScenario::DerivedGeneratedDefault:
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyDerived%s Value = FPolicyDerived%s();"), *Tag, *Tag));
		AppendLine(Source, TEXT("\tRecordConstructorPolicyMarker(1202);"));
		AppendLine(Source, TEXT("\treturn Value.BaseValue + Value.DerivedValue;"));
		break;
	case ECtorSpecialPolicyScenario::DerivedExplicitSuper:
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyDerived%s Value = FPolicyDerived%s();"), *Tag, *Tag));
		AppendLine(Source, TEXT("\tRecordConstructorPolicyMarker(1303);"));
		AppendLine(Source, TEXT("\treturn Value.BaseValue + Value.DerivedValue;"));
		break;
	case ECtorSpecialPolicyScenario::MissingBaseDefaultOptionOff:
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyDerived%s Value = FPolicyDerived%s();"), *Tag, *Tag));
		AppendLine(Source, TEXT("\treturn 1;"));
		break;
	case ECtorSpecialPolicyScenario::CopyAfterUserConstructor:
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s Source(15);"), *Tag));
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s Target(Source);"), *Tag));
		AppendLine(Source, TEXT("\tRecordConstructorPolicyMarker(1502);"));
		AppendLine(Source, TEXT("\treturn Source.Value + Target.Value;"));
		break;
	case ECtorSpecialPolicyScenario::AssignmentSelfStability:
		AppendLine(Source, FString::Printf(TEXT("\tFPolicyValue%s Value;"), *Tag));
		AppendLine(Source, TEXT("\tValue.Value = 42;"));
		AppendLine(Source, TEXT("\tValue.Payload.Value = 42;"));
		AppendLine(Source, TEXT("\tValue = Value;"));
		AppendLine(Source, TEXT("\tRecordConstructorPolicyMarker(1602);"));
		AppendLine(Source, TEXT("\treturn Value.Value + Value.Payload.Value;"));
		break;
	default:
		AppendLine(Source, TEXT("\treturn 0;"));
		break;
	}
	AppendLine(Source, TEXT("}"));
}

FString BuildOwnedSource(const FCtorSpecialPolicyParams& Params, const FString& FunctionName)
{
	FString Source;
	AppendHelpers(Source);
	AppendScenarioTypes(Source, Params.Scenario);
	AppendEntry(Source, Params, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FCtorSpecialPolicyCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Scenario))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
		return Info;
	}
	Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
	Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
	Info.Execution = EGeneratedCaseExecution::RequiresHostSetup;
	Info.ExpectedReturn = ExpectedFromScenario(Cell.Scenario);
	Info.ExecutionNotes = HostNotes;
	Info.bLimitedObservation = Cell.Observation != ECtorSpecialPolicyObservation::Runtime;
	return Info;
}
}

FStringView FCtorSpecialPolicyGenerator::GetProductId() const
{
	return TEXT("LANG-CTOR-SPECIAL-POLICY");
}

TArray<FGeneratedCaseInfo> FCtorSpecialPolicyGenerator::ListCases() const
{
	using namespace CtorSpecialPolicyGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FCtorSpecialPolicyCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("CtorSpecialPolicy PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FCtorSpecialPolicyGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace CtorSpecialPolicyGeneratorDetail;
	FCtorSpecialPolicyCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

FString FCtorSpecialPolicyGenerator::BuildDumpSource() const
{
	using namespace CtorSpecialPolicyGeneratorDetail;
	const TArray<FCtorSpecialPolicyCell> Cells = AllCells();
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-CTOR-SPECIAL-POLICY"));
	AppendLine(Source, TEXT("// Cases: 64 total, 44 normal, 20 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));
	AppendLine(Source);
	AppendHelpers(Source);
	TSet<uint8> EmittedScenarios;
	for (const FCtorSpecialPolicyCell& Cell : Cells)
	{
		if (IsReject(Cell.Scenario))
		{
			continue;
		}
		const uint8 Key = static_cast<uint8>(Cell.Scenario);
		if (!EmittedScenarios.Contains(Key))
		{
			AppendScenarioTypes(Source, Cell.Scenario);
			EmittedScenarios.Add(Key);
		}
	}
	bool bNeedBlank = false;
	for (const FCtorSpecialPolicyCell& Cell : Cells)
	{
		if (IsReject(Cell.Scenario))
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
	for (const FCtorSpecialPolicyCell& Cell : Cells)
	{
		if (!IsReject(Cell.Scenario))
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
		AppendScenarioTypes(Source, Cell.Scenario);
		AppendEntry(Source, ParamsFromCell(Cell), MakeEntryName(Cell));
	}
	return Source;
}

FString FCtorSpecialPolicyGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace CtorSpecialPolicyGeneratorDetail;
	const TArray<FCtorSpecialPolicyCell> Cells = AllCells();
	FString Source;
	AppendHelpers(Source);
	TSet<uint8> EmittedScenarios;
	for (const FCtorSpecialPolicyCell& Cell : Cells)
	{
		if (IsReject(Cell.Scenario))
		{
			continue;
		}
		const uint8 Key = static_cast<uint8>(Cell.Scenario);
		if (!EmittedScenarios.Contains(Key))
		{
			AppendScenarioTypes(Source, Cell.Scenario);
			EmittedScenarios.Add(Key);
		}
	}
	int32 Emitted = 0;
	for (const FCtorSpecialPolicyCell& Cell : Cells)
	{
		if (IsReject(Cell.Scenario))
		{
			continue;
		}
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

FString FCtorSpecialPolicyGenerator::BuildConstructorPolicySource(const FCtorSpecialPolicyParams& Params) const
{
	using namespace CtorSpecialPolicyGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Scenario))
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

int32 FCtorSpecialPolicyGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace CtorSpecialPolicyGeneratorDetail;
	FCtorSpecialPolicyCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Scenario))
	{
		return 0;
	}
	return ExpectedFromScenario(Cell.Scenario);
}

FString FCtorSpecialPolicyGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace CtorSpecialPolicyGeneratorDetail;
	FCtorSpecialPolicyCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Scenario))
	{
		return FString();
	}
	return BuildOwnedSource(ParamsFromCell(Cell), MakeEntryName(Cell));
}

TArray<FString> FCtorSpecialPolicyGenerator::ListRejectCaseIds() const
{
	using namespace CtorSpecialPolicyGeneratorDetail;
	TArray<FString> Ids;
	for (const FCtorSpecialPolicyCell& Cell : AllCells())
	{
		if (IsReject(Cell.Scenario))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
