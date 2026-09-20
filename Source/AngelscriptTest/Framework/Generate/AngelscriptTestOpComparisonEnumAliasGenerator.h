#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpComparisonEnumAliasFamily : uint8
{
	Enum,
	Alias,
};

enum class EOpComparisonEnumAliasOperator : uint8
{
	Less,
	LessEqual,
	Greater,
	GreaterEqual,
	Equal,
	NotEqual,
};

enum class EOpComparisonEnumAliasPair : uint8
{
	EqualZero,
	EqualNamed,
	BelowAdjacent,
	AboveAdjacent,
	MinimumBoundary,
	MaximumBoundary,
};

enum class EOpComparisonEnumAliasOrder : uint8
{
	LeftRight,
	RightLeft,
};

struct FOpComparisonEnumAliasParams
{
	EOpComparisonEnumAliasFamily Family = EOpComparisonEnumAliasFamily::Enum;
	EOpComparisonEnumAliasOperator Operator = EOpComparisonEnumAliasOperator::Less;
	EOpComparisonEnumAliasPair Pair = EOpComparisonEnumAliasPair::EqualZero;
	EOpComparisonEnumAliasOrder Order = EOpComparisonEnumAliasOrder::LeftRight;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpComparisonEnumAliasGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildIntegralComparisonSource(const FOpComparisonEnumAliasParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
