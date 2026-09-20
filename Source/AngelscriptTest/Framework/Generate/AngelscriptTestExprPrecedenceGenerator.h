#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EExprPrecedenceGrouping : uint8
{
	Unparenthesized,
	LeftParenthesized,
	RightParenthesized,
};

enum class EExprPrecedenceLevel : uint8
{
	Multiplicative,
	Additive,
	Shift,
	Relational,
	Equality,
	BitwiseAnd,
	BitwiseXor,
	BitwiseOr,
	LogicalAnd,
	LogicalOr,
	Conditional,
	Assignment,
};

struct FExprPrecedenceParams
{
	EExprPrecedenceGrouping Grouping = EExprPrecedenceGrouping::Unparenthesized;
	EExprPrecedenceLevel Left = EExprPrecedenceLevel::Multiplicative;
	EExprPrecedenceLevel Right = EExprPrecedenceLevel::Additive;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FExprPrecedenceGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildPrecedenceSource(const FExprPrecedenceParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
