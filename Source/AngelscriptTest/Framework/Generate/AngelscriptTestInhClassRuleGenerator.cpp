#include "Framework/Generate/AngelscriptTestInhClassRuleGenerator.h"

#include "Containers/Set.h"

namespace AngelscriptTest::Generate
{
namespace InhClassRuleGeneratorDetail
{
struct FClassRuleCell
{
	EInhClassRuleScenario Scenario = EInhClassRuleScenario::ConcreteInstantiate;
	EInhClassRuleObservation Observation = EInhClassRuleObservation::Runtime;
};

const TCHAR* ScenarioToken(const EInhClassRuleScenario Scenario)
{
	switch (Scenario)
	{
	case EInhClassRuleScenario::AbstractClassKeywordRejected:
		return TEXT("ABSTRACT_CLASS_KEYWORD_REJECTED");
	case EInhClassRuleScenario::FinalClassKeywordRejected:
		return TEXT("FINAL_CLASS_KEYWORD_REJECTED");
	case EInhClassRuleScenario::ConcreteBaseInherit:
		return TEXT("CONCRETE_BASE_INHERIT");
	case EInhClassRuleScenario::ConcreteInstantiate:
		return TEXT("CONCRETE_INSTANTIATE");
	case EInhClassRuleScenario::FinalMethodOverride:
		return TEXT("FINAL_METHOD_OVERRIDE");
	case EInhClassRuleScenario::FinalMethodInherit:
		return TEXT("FINAL_METHOD_INHERIT");
	case EInhClassRuleScenario::InvalidBaseName:
		return TEXT("INVALID_BASE_NAME");
	case EInhClassRuleScenario::InvalidBaseKind:
		return TEXT("INVALID_BASE_KIND");
	case EInhClassRuleScenario::DuplicateBase:
		return TEXT("DUPLICATE_BASE");
	case EInhClassRuleScenario::InheritanceCycleDirect:
		return TEXT("INHERITANCE_CYCLE_DIRECT");
	case EInhClassRuleScenario::InheritanceCycleIndirect:
		return TEXT("INHERITANCE_CYCLE_INDIRECT");
	case EInhClassRuleScenario::ImplicitOverrideWithoutKeyword:
		return TEXT("IMPLICIT_OVERRIDE_WITHOUT_KEYWORD");
	case EInhClassRuleScenario::ExactOverride:
		return TEXT("EXACT_OVERRIDE");
	case EInhClassRuleScenario::OverrideWithoutBase:
		return TEXT("OVERRIDE_WITHOUT_BASE");
	case EInhClassRuleScenario::ReturnTypeMismatch:
		return TEXT("RETURN_TYPE_MISMATCH");
	case EInhClassRuleScenario::DeepOverride:
		return TEXT("DEEP_OVERRIDE");
	default:
		return nullptr;
	}
}

const TCHAR* ObservationToken(const EInhClassRuleObservation Observation)
{
	switch (Observation)
	{
	case EInhClassRuleObservation::Compile:
		return TEXT("COMPILE");
	case EInhClassRuleObservation::Diagnostic:
		return TEXT("DIAGNOSTIC");
	case EInhClassRuleObservation::Metadata:
		return TEXT("METADATA");
	case EInhClassRuleObservation::Runtime:
		return TEXT("RUNTIME");
	default:
		return nullptr;
	}
}

bool IsValidParams(const FInhClassRuleParams& Params)
{
	return ScenarioToken(Params.Scenario) != nullptr && ObservationToken(Params.Observation) != nullptr;
}

bool IsReject(const EInhClassRuleScenario Scenario)
{
	switch (Scenario)
	{
	case EInhClassRuleScenario::ConcreteBaseInherit:
	case EInhClassRuleScenario::ConcreteInstantiate:
	case EInhClassRuleScenario::FinalMethodInherit:
	case EInhClassRuleScenario::ImplicitOverrideWithoutKeyword:
	case EInhClassRuleScenario::ExactOverride:
	case EInhClassRuleScenario::DeepOverride:
		return false;
	default:
		return true;
	}
}

int32 ExpectedFromScenario(const EInhClassRuleScenario Scenario)
{
	switch (Scenario)
	{
	case EInhClassRuleScenario::ConcreteBaseInherit:
		return 31;
	case EInhClassRuleScenario::ConcreteInstantiate:
		return 41;
	case EInhClassRuleScenario::FinalMethodInherit:
		return 61;
	case EInhClassRuleScenario::ImplicitOverrideWithoutKeyword:
		return 122;
	case EInhClassRuleScenario::ExactOverride:
		return 132;
	case EInhClassRuleScenario::DeepOverride:
		return 163;
	default:
		return 0;
	}
}

void AppendLine(FString& Source, const FString& Line = FString())
{
	Source += Line;
	Source.AppendChar(TEXT('\n'));
}

FString MakeCaseId(const FClassRuleCell& Cell)
{
	return FString::Printf(
		TEXT("LANG-INH-CLASS-RULE-%s-%s"),
		ScenarioToken(Cell.Scenario),
		ObservationToken(Cell.Observation));
}

FString MakeEntryName(const FClassRuleCell& Cell)
{
	return MakeCanonicalEntryName(MakeCaseId(Cell));
}

FString TypeSuffix(const FClassRuleCell& Cell)
{
	return MakeCanonicalEntryName(
		FString::Printf(
			TEXT("%s-%s"),
			ScenarioToken(Cell.Scenario),
			ObservationToken(Cell.Observation)))
		.Mid(5);
}

FInhClassRuleParams ParamsFromCell(const FClassRuleCell& Cell)
{
	FInhClassRuleParams Params;
	Params.Scenario = Cell.Scenario;
	Params.Observation = Cell.Observation;
	return Params;
}

TArray<FClassRuleCell> AllCells()
{
	TArray<FClassRuleCell> Cells;
	Cells.SetNum(64);
	for (int32 Index = 0; Index < 64; ++Index)
	{
		FClassRuleCell& Cell = Cells[Index];
		Cell.Scenario = static_cast<EInhClassRuleScenario>(Index / 4);
		Cell.Observation = static_cast<EInhClassRuleObservation>(Index % 4);
	}
	return Cells;
}

bool TryFindCell(const FStringView CaseId, FClassRuleCell& OutCell)
{
	const FString Needle(CaseId);
	for (const FClassRuleCell& Cell : AllCells())
	{
		if (MakeCaseId(Cell) == Needle)
		{
			OutCell = Cell;
			return true;
		}
	}
	return false;
}

void AppendValueMethod(FString& Source, const int32 ReturnValue, const TCHAR* Attribute)
{
	AppendLine(
		Source,
		Attribute == nullptr
			? FString(TEXT("\tint Value()"))
			: FString::Printf(TEXT("\tint Value() %s"), Attribute));
	AppendLine(Source, TEXT("\t{"));
	AppendLine(Source, FString::Printf(TEXT("\t\treturn %d;"), ReturnValue));
	AppendLine(Source, TEXT("\t}"));
}

void AppendScenarioSource(FString& Source, const FClassRuleCell& Cell, const FString& FunctionName)
{
	const FString Suffix = TypeSuffix(Cell);
	const FString BaseName = TEXT("FRuleBase") + Suffix;
	const FString PrimaryName = TEXT("FRulePrimary") + Suffix;
	const FString MiddleName = TEXT("FRuleMiddle") + Suffix;
	const FString SecondName = TEXT("FRuleSecondBase") + Suffix;

	auto AppendRuntime = [&]()
	{
		AppendLine(Source, FString::Printf(TEXT("int %s()"), *FunctionName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, FString::Printf(TEXT("\t%s Object;"), *PrimaryName));
		AppendLine(Source, TEXT("\treturn Object.Value();"));
		AppendLine(Source, TEXT("}"));
	};

	switch (Cell.Scenario)
	{
	case EInhClassRuleScenario::AbstractClassKeywordRejected:
		AppendLine(Source, FString::Printf(TEXT("abstract class %s"), *PrimaryName));
		AppendLine(Source, TEXT("{"));
		AppendValueMethod(Source, 11, nullptr);
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendRuntime();
		break;
	case EInhClassRuleScenario::FinalClassKeywordRejected:
		AppendLine(Source, FString::Printf(TEXT("final class %s"), *PrimaryName));
		AppendLine(Source, TEXT("{"));
		AppendValueMethod(Source, 12, nullptr);
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendRuntime();
		break;
	case EInhClassRuleScenario::ConcreteBaseInherit:
		AppendLine(Source, FString::Printf(TEXT("class %s"), *BaseName));
		AppendLine(Source, TEXT("{"));
		AppendValueMethod(Source, 31, nullptr);
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("class %s : %s"), *PrimaryName, *BaseName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendRuntime();
		break;
	case EInhClassRuleScenario::ConcreteInstantiate:
		AppendLine(Source, FString::Printf(TEXT("class %s"), *PrimaryName));
		AppendLine(Source, TEXT("{"));
		AppendValueMethod(Source, 41, nullptr);
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendRuntime();
		break;
	case EInhClassRuleScenario::FinalMethodOverride:
		AppendLine(Source, FString::Printf(TEXT("class %s"), *BaseName));
		AppendLine(Source, TEXT("{"));
		AppendValueMethod(Source, 51, TEXT("final"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("class %s : %s"), *PrimaryName, *BaseName));
		AppendLine(Source, TEXT("{"));
		AppendValueMethod(Source, 52, TEXT("override"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendRuntime();
		break;
	case EInhClassRuleScenario::FinalMethodInherit:
		AppendLine(Source, FString::Printf(TEXT("class %s"), *BaseName));
		AppendLine(Source, TEXT("{"));
		AppendValueMethod(Source, 61, TEXT("final"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("class %s : %s"), *PrimaryName, *BaseName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendRuntime();
		break;
	case EInhClassRuleScenario::InvalidBaseName:
		AppendLine(Source, FString::Printf(TEXT("class %s : FMissingRuleBase"), *PrimaryName));
		AppendLine(Source, TEXT("{"));
		AppendValueMethod(Source, 71, nullptr);
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendRuntime();
		break;
	case EInhClassRuleScenario::InvalidBaseKind:
		AppendLine(Source, FString::Printf(TEXT("struct %s"), *BaseName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tint Value = 81;"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("class %s : %s"), *PrimaryName, *BaseName));
		AppendLine(Source, TEXT("{"));
		AppendValueMethod(Source, 82, nullptr);
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendRuntime();
		break;
	case EInhClassRuleScenario::DuplicateBase:
		AppendLine(Source, FString::Printf(TEXT("class %s"), *BaseName));
		AppendLine(Source, TEXT("{"));
		AppendValueMethod(Source, 91, nullptr);
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("class %s"), *SecondName));
		AppendLine(Source, TEXT("{"));
		AppendValueMethod(Source, 92, nullptr);
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("class %s : %s, %s"), *PrimaryName, *BaseName, *SecondName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendRuntime();
		break;
	case EInhClassRuleScenario::InheritanceCycleDirect:
		AppendLine(Source, FString::Printf(TEXT("class %s : %s"), *PrimaryName, *PrimaryName));
		AppendLine(Source, TEXT("{"));
		AppendValueMethod(Source, 101, nullptr);
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendRuntime();
		break;
	case EInhClassRuleScenario::InheritanceCycleIndirect:
		AppendLine(Source, FString::Printf(TEXT("class %s : %s"), *BaseName, *PrimaryName));
		AppendLine(Source, TEXT("{"));
		AppendValueMethod(Source, 111, nullptr);
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("class %s : %s"), *PrimaryName, *BaseName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendRuntime();
		break;
	case EInhClassRuleScenario::ImplicitOverrideWithoutKeyword:
		AppendLine(Source, FString::Printf(TEXT("class %s"), *BaseName));
		AppendLine(Source, TEXT("{"));
		AppendValueMethod(Source, 121, nullptr);
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("class %s : %s"), *PrimaryName, *BaseName));
		AppendLine(Source, TEXT("{"));
		AppendValueMethod(Source, 122, nullptr);
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendRuntime();
		break;
	case EInhClassRuleScenario::ExactOverride:
		AppendLine(Source, FString::Printf(TEXT("class %s"), *BaseName));
		AppendLine(Source, TEXT("{"));
		AppendValueMethod(Source, 131, nullptr);
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("class %s : %s"), *PrimaryName, *BaseName));
		AppendLine(Source, TEXT("{"));
		AppendValueMethod(Source, 132, TEXT("override"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendRuntime();
		break;
	case EInhClassRuleScenario::OverrideWithoutBase:
		AppendLine(Source, FString::Printf(TEXT("class %s"), *PrimaryName));
		AppendLine(Source, TEXT("{"));
		AppendValueMethod(Source, 141, TEXT("override"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendRuntime();
		break;
	case EInhClassRuleScenario::ReturnTypeMismatch:
		AppendLine(Source, FString::Printf(TEXT("class %s"), *BaseName));
		AppendLine(Source, TEXT("{"));
		AppendValueMethod(Source, 151, nullptr);
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("class %s : %s"), *PrimaryName, *BaseName));
		AppendLine(Source, TEXT("{"));
		AppendLine(Source, TEXT("\tfloat Value() override"));
		AppendLine(Source, TEXT("\t{"));
		AppendLine(Source, TEXT("\t\treturn 152.0f;"));
		AppendLine(Source, TEXT("\t}"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendRuntime();
		break;
	case EInhClassRuleScenario::DeepOverride:
	default:
		AppendLine(Source, FString::Printf(TEXT("class %s"), *BaseName));
		AppendLine(Source, TEXT("{"));
		AppendValueMethod(Source, 161, nullptr);
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("class %s : %s"), *MiddleName, *BaseName));
		AppendLine(Source, TEXT("{"));
		AppendValueMethod(Source, 162, TEXT("override"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendLine(Source, FString::Printf(TEXT("class %s : %s"), *PrimaryName, *MiddleName));
		AppendLine(Source, TEXT("{"));
		AppendValueMethod(Source, 163, TEXT("override"));
		AppendLine(Source, TEXT("}"));
		AppendLine(Source);
		AppendRuntime();
		break;
	}
}

FString BuildOwnedSource(const FClassRuleCell& Cell, const FString& FunctionName)
{
	FString Source;
	AppendScenarioSource(Source, Cell, FunctionName);
	return Source;
}

FGeneratedCaseInfo MakeCaseInfo(const FClassRuleCell& Cell)
{
	FGeneratedCaseInfo Info;
	Info.CaseId = MakeCaseId(Cell);
	if (IsReject(Cell.Scenario))
	{
		Info.Outcome = EGeneratedCaseOutcome::CompileReject;
		Info.Execution = EGeneratedCaseExecution::SourceOnly;
	}
	else
	{
		Info.EntryDeclaration = FString::Printf(TEXT("int %s()"), *MakeEntryName(Cell));
		Info.Outcome = EGeneratedCaseOutcome::ReturnValue;
		Info.Execution = EGeneratedCaseExecution::Standalone;
		Info.ExpectedReturn = ExpectedFromScenario(Cell.Scenario);
	}
	return Info;
}
}

FStringView FInhClassRuleGenerator::GetProductId() const
{
	return TEXT("LANG-INH-CLASS-RULE");
}

TArray<FGeneratedCaseInfo> FInhClassRuleGenerator::ListCases() const
{
	using namespace InhClassRuleGeneratorDetail;
	TArray<FGeneratedCaseInfo> Cases;
	TSet<FString> EntryNames;
	for (const FClassRuleCell& Cell : AllCells())
	{
		FGeneratedCaseInfo Info = MakeCaseInfo(Cell);
		if (!Info.EntryDeclaration.IsEmpty())
		{
			const FString EntryName = MakeEntryName(Cell);
			checkf(!EntryNames.Contains(EntryName), TEXT("InhClassRule PascalCase collision: %s"), *EntryName);
			EntryNames.Add(EntryName);
		}
		Cases.Add(MoveTemp(Info));
	}
	return Cases;
}

FString FInhClassRuleGenerator::BuildCaseSource(const FStringView CaseId) const
{
	using namespace InhClassRuleGeneratorDetail;
	FClassRuleCell Cell;
	if (!TryFindCell(CaseId, Cell))
	{
		return FString();
	}
	return BuildOwnedSource(Cell, MakeEntryName(Cell));
}

FString FInhClassRuleGenerator::BuildDumpSource() const
{
	using namespace InhClassRuleGeneratorDetail;
	FString Source;
	AppendLine(Source, TEXT("// Product: LANG-INH-CLASS-RULE"));
	AppendLine(Source, TEXT("// Cases: 64 total, 24 normal, 40 reject, 0 fault"));
	AppendLine(Source, TEXT("// Source-inspection dump. This file is not a compiled AngelScript module."));

	bool bNeedBlank = true;
	for (const FClassRuleCell& Cell : AllCells())
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
		AppendLine(Source, TEXT("// Execution: Standalone"));
		AppendLine(Source);
		AppendScenarioSource(Source, Cell, MakeEntryName(Cell));
		bNeedBlank = true;
	}

	for (const FClassRuleCell& Cell : AllCells())
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
		AppendScenarioSource(Source, Cell, MakeEntryName(Cell));
	}
	return Source;
}

FString FInhClassRuleGenerator::BuildInheritanceRuleSource(const FInhClassRuleParams& Params) const
{
	using namespace InhClassRuleGeneratorDetail;
	if (!IsValidParams(Params) || IsReject(Params.Scenario))
	{
		return FString();
	}
	if (!Params.FunctionName.IsEmpty() && !IsValidGeneratedFunctionName(Params.FunctionName))
	{
		return FString();
	}
	FClassRuleCell Cell;
	Cell.Scenario = Params.Scenario;
	Cell.Observation = Params.Observation;
	const FString FunctionName = Params.FunctionName.IsEmpty() ? FString(TEXT("Entry")) : Params.FunctionName;
	return BuildOwnedSource(Cell, FunctionName);
}

FString FInhClassRuleGenerator::BuildAllSource(int32& OutCaseCount) const
{
	using namespace InhClassRuleGeneratorDetail;
	FString Source;
	int32 Emitted = 0;
	for (const FClassRuleCell& Cell : AllCells())
	{
		if (IsReject(Cell.Scenario))
		{
			continue;
		}
		if (Emitted > 0)
		{
			AppendLine(Source);
		}
		AppendScenarioSource(Source, Cell, MakeEntryName(Cell));
		++Emitted;
	}
	OutCaseCount = Emitted;
	return Source;
}

int32 FInhClassRuleGenerator::GetExpected(const FStringView CaseId) const
{
	using namespace InhClassRuleGeneratorDetail;
	FClassRuleCell Cell;
	if (!TryFindCell(CaseId, Cell) || IsReject(Cell.Scenario))
	{
		return 0;
	}
	return ExpectedFromScenario(Cell.Scenario);
}

FString FInhClassRuleGenerator::BuildRejectSource(const FStringView CaseId) const
{
	using namespace InhClassRuleGeneratorDetail;
	FClassRuleCell Cell;
	if (!TryFindCell(CaseId, Cell) || !IsReject(Cell.Scenario))
	{
		return FString();
	}
	return BuildOwnedSource(Cell, MakeEntryName(Cell));
}

TArray<FString> FInhClassRuleGenerator::ListRejectCaseIds() const
{
	using namespace InhClassRuleGeneratorDetail;
	TArray<FString> Ids;
	for (const FClassRuleCell& Cell : AllCells())
	{
		if (IsReject(Cell.Scenario))
		{
			Ids.Add(MakeCaseId(Cell));
		}
	}
	return Ids;
}
}
