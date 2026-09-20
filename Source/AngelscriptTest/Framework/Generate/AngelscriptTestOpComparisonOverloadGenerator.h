#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpComparisonOverloadOperator : uint8
{
	Less,
	LessEqual,
	Greater,
	GreaterEqual,
	Equal,
	NotEqual,
};

enum class EOpComparisonOverloadRelation : uint8
{
	Less,
	Equal,
	Greater,
	Unequal,
};

enum class EOpComparisonOverloadOrder : uint8
{
	LeftRight,
	RightLeft,
};

enum class EOpComparisonOverloadReceiver : uint8
{
	Mutable,
	Const,
};

struct FOpComparisonOverloadParams
{
	EOpComparisonOverloadOperator Operator = EOpComparisonOverloadOperator::Less;
	EOpComparisonOverloadRelation Relation = EOpComparisonOverloadRelation::Less;
	EOpComparisonOverloadOrder Order = EOpComparisonOverloadOrder::LeftRight;
	EOpComparisonOverloadReceiver Receiver = EOpComparisonOverloadReceiver::Mutable;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpComparisonOverloadGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildOverloadedComparisonSource(const FOpComparisonOverloadParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
