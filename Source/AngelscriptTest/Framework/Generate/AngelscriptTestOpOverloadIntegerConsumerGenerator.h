#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpOverloadIntegerConsumerScenario : uint8
{
	UnaryMember,
	UnaryConstMember,
	BinaryIntMember,
	BinaryIntConstMember,
	BinaryInt64Promotion,
	BinaryValueParameter,
	IndexIntParameter,
	IndexInt64Promotion,
	CallIntMember,
	CallInt64Promotion,
	ConversionIntTarget,
	BinaryAmbiguousInt8,
	IndexAmbiguousInt8,
	CallAmbiguousInt8,
	ConversionAmbiguousTarget,
	UnaryMissingComplement,
	BinaryMissingSubtract,
	IndexMissingSecondArgument,
	CallMissingSecondArgument,
	ConversionMissingTarget,
};

enum class EOpOverloadIntegerConsumerContext : uint8
{
	Assignment,
	Return,
	Condition,
	OverloadArgument,
	Chain,
	SwitchIndex,
};

struct FOpOverloadIntegerConsumerParams
{
	EOpOverloadIntegerConsumerScenario Scenario = EOpOverloadIntegerConsumerScenario::UnaryMember;
	EOpOverloadIntegerConsumerContext Context = EOpOverloadIntegerConsumerContext::Assignment;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpOverloadIntegerConsumerGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildScenarioSource(const FOpOverloadIntegerConsumerParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
