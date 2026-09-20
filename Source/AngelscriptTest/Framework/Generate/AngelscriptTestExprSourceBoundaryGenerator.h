#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EExprSourceBoundaryBuild : uint8
{
	First,
	SameRebuild,
	ChangedRebuild,
};

enum class EExprSourceBoundaryContext : uint8
{
	Initializer,
	Argument,
	Return,
	Condition,
	Index,
};

enum class EExprSourceBoundaryScenario : uint8
{
	ParenthesesOne,
	ParenthesesEight,
	ParenthesesSixtyFour,
	ChainOne,
	ChainEight,
	ChainThirtyTwo,
	ArgumentsZero,
	ArgumentsOne,
	ArgumentsMany,
	NumericMinimum,
	NumericMaximum,
	Whitespace,
	Comments,
	Multiline,
};

struct FExprSourceBoundaryParams
{
	EExprSourceBoundaryBuild Build = EExprSourceBoundaryBuild::First;
	EExprSourceBoundaryContext Context = EExprSourceBoundaryContext::Initializer;
	EExprSourceBoundaryScenario Scenario = EExprSourceBoundaryScenario::ParenthesesOne;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FExprSourceBoundaryGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildExpressionBoundarySource(const FExprSourceBoundaryParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
