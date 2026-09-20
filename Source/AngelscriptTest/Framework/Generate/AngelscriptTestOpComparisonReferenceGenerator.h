#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpComparisonReferenceOperator : uint8
{
	Less,
	LessEqual,
	Greater,
	GreaterEqual,
	Equal,
	NotEqual,
};

enum class EOpComparisonReferenceRelation : uint8
{
	SameNonNull,
	DifferentNonNull,
	LeftNull,
	RightNull,
	BothNull,
	DerivedBaseSame,
	SiblingDifferent,
	ConstSame,
};

enum class EOpComparisonReferenceOrder : uint8
{
	LeftRight,
	RightLeft,
};

struct FOpComparisonReferenceParams
{
	EOpComparisonReferenceOperator Operator = EOpComparisonReferenceOperator::Equal;
	EOpComparisonReferenceRelation Relation = EOpComparisonReferenceRelation::SameNonNull;
	EOpComparisonReferenceOrder Order = EOpComparisonReferenceOrder::LeftRight;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpComparisonReferenceGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildReferenceComparisonSource(const FOpComparisonReferenceParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
