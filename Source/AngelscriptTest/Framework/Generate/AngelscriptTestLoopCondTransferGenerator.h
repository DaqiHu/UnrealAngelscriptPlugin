#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class ELoopCondLoop : uint8
{
	While,
	DoWhile,
	For,
};

enum class ELoopCondExpr : uint8
{
	Variable,
	Comparison,
	Logical,
	Negated,
	SideEffect,
};

enum class ELoopCondCount : uint8
{
	Zero,
	One,
	Two,
};

enum class ELoopCondTransfer : uint8
{
	None,
	Break,
	Continue,
	Return,
};

struct FLoopCondTransferParams
{
	ELoopCondLoop Loop = ELoopCondLoop::While;
	ELoopCondExpr Condition = ELoopCondExpr::Variable;
	ELoopCondCount Count = ELoopCondCount::Zero;
	ELoopCondTransfer Transfer = ELoopCondTransfer::None;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FLoopCondTransferGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildLoopCondSource(const FLoopCondTransferParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
