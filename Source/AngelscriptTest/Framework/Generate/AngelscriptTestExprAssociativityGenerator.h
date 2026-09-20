#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EExprAssociativityGrouping : uint8
{
	Unparenthesized,
	LeftParenthesized,
	RightParenthesized,
};

enum class EExprAssociativityLevel : uint8
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

enum class EExprAssociativitySequence : uint8
{
	RepeatedOperator,
	MixedSameLevel,
};

struct FExprAssociativityParams
{
	EExprAssociativityGrouping Grouping = EExprAssociativityGrouping::Unparenthesized;
	EExprAssociativityLevel Level = EExprAssociativityLevel::Multiplicative;
	EExprAssociativitySequence Sequence = EExprAssociativitySequence::RepeatedOperator;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FExprAssociativityGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildPrecedenceSource(const FExprAssociativityParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
