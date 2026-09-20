#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EInhClassRuleScenario : uint8
{
	AbstractClassKeywordRejected,
	FinalClassKeywordRejected,
	ConcreteBaseInherit,
	ConcreteInstantiate,
	FinalMethodOverride,
	FinalMethodInherit,
	InvalidBaseName,
	InvalidBaseKind,
	DuplicateBase,
	InheritanceCycleDirect,
	InheritanceCycleIndirect,
	ImplicitOverrideWithoutKeyword,
	ExactOverride,
	OverrideWithoutBase,
	ReturnTypeMismatch,
	DeepOverride,
};

enum class EInhClassRuleObservation : uint8
{
	Compile,
	Diagnostic,
	Metadata,
	Runtime,
};

struct FInhClassRuleParams
{
	EInhClassRuleScenario Scenario = EInhClassRuleScenario::ConcreteInstantiate;
	EInhClassRuleObservation Observation = EInhClassRuleObservation::Runtime;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FInhClassRuleGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildInheritanceRuleSource(const FInhClassRuleParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
