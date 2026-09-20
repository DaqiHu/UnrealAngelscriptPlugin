#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EExprLazyForm : uint8
{
	LogicalAnd,
	LogicalOr,
	Conditional,
};

enum class EExprLazyOperandOutcome : uint8
{
	Value,
	SideEffect,
	Exception,
};

enum class EExprLazySelector : uint8
{
	False,
	True,
};

enum class EExprLazySourceShape : uint8
{
	SingleLine,
	Comments,
	Multiline,
	Parenthesized,
};

struct FExprLazyEvaluationParams
{
	EExprLazyForm Form = EExprLazyForm::LogicalAnd;
	EExprLazyOperandOutcome OperandOutcome = EExprLazyOperandOutcome::Value;
	EExprLazySelector Selector = EExprLazySelector::False;
	EExprLazySourceShape SourceShape = EExprLazySourceShape::SingleLine;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FExprLazyEvaluationGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildLazyEvaluationSource(const FExprLazyEvaluationParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
