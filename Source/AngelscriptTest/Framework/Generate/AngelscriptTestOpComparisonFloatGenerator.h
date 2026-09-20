#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpComparisonFloatType : uint8
{
	Float32,
	Float64,
};

enum class EOpComparisonFloatOperator : uint8
{
	Less,
	LessEqual,
	Greater,
	GreaterEqual,
	Equal,
	NotEqual,
};

enum class EOpComparisonFloatValue : uint8
{
	NegativeZero,
	PositiveZero,
	NaN,
	PositiveInfinity,
	NegativeInfinity,
	Minimum,
	Maximum,
	EqualPair,
};

enum class EOpComparisonFloatOrder : uint8
{
	LeftRight,
	RightLeft,
};

struct FOpComparisonFloatParams
{
	EOpComparisonFloatType Type = EOpComparisonFloatType::Float32;
	EOpComparisonFloatOperator Operator = EOpComparisonFloatOperator::Less;
	EOpComparisonFloatValue Value = EOpComparisonFloatValue::NegativeZero;
	EOpComparisonFloatOrder Order = EOpComparisonFloatOrder::LeftRight;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpComparisonFloatGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildFloatComparisonSource(const FOpComparisonFloatParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
