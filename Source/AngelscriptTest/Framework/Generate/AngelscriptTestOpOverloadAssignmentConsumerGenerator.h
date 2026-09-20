#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpOverloadAssignmentConsumerScenario : uint8
{
	AssignmentIntParameter,
	AssignmentValueParameter,
	AssignmentAmbiguousInt8,
	AssignmentMissingAddAssign,
};

enum class EOpOverloadAssignmentConsumerContext : uint8
{
	Assignment,
	Return,
	Condition,
	OverloadArgument,
	Chain,
	SwitchIndex,
};

struct FOpOverloadAssignmentConsumerParams
{
	EOpOverloadAssignmentConsumerScenario Scenario = EOpOverloadAssignmentConsumerScenario::AssignmentIntParameter;
	EOpOverloadAssignmentConsumerContext Context = EOpOverloadAssignmentConsumerContext::Assignment;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpOverloadAssignmentConsumerGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildScenarioSource(const FOpOverloadAssignmentConsumerParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
