#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpOverloadBooleanConsumerScenario : uint8
{
	ComparisonIntParameter,
	ComparisonValueParameter,
	ComparisonAmbiguousInt8,
	ComparisonMissingLess,
};

enum class EOpOverloadBooleanConsumerContext : uint8
{
	Assignment,
	Return,
	Condition,
	OverloadArgument,
	Chain,
};

struct FOpOverloadBooleanConsumerParams
{
	EOpOverloadBooleanConsumerScenario Scenario = EOpOverloadBooleanConsumerScenario::ComparisonIntParameter;
	EOpOverloadBooleanConsumerContext Context = EOpOverloadBooleanConsumerContext::Assignment;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpOverloadBooleanConsumerGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildScenarioSource(const FOpOverloadBooleanConsumerParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
