#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EExprEvalOrderComposition : uint8
{
	Binary,
	Assignment,
	CompoundAssignment,
	CallArguments,
	ConstructorArguments,
	IndexArguments,
	CallChain,
	MemberIndexChain,
	NestedCast,
};

enum class EExprEvalOrderOperandCount : uint8
{
	Two,
	Three,
	Eight,
};

enum class EExprEvalOrderOutcome : uint8
{
	Complete,
	ExceptionFirst,
	ExceptionMiddle,
	ExceptionLast,
};

enum class EExprEvalOrderSourceShape : uint8
{
	SingleLine,
	Whitespace,
	Comments,
	Multiline,
	NestedParentheses,
};

struct FExprEvalOrderParams
{
	EExprEvalOrderComposition Composition = EExprEvalOrderComposition::Binary;
	EExprEvalOrderOperandCount OperandCount = EExprEvalOrderOperandCount::Two;
	EExprEvalOrderOutcome Outcome = EExprEvalOrderOutcome::Complete;
	EExprEvalOrderSourceShape SourceShape = EExprEvalOrderSourceShape::SingleLine;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FExprEvalOrderGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildEagerEvaluationSource(const FExprEvalOrderParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
