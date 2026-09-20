#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EStatementKind : uint8
{
	If,
	IfElse,
	ElseIf,
	While,
	DoWhile,
	For,
	Switch,
	NestedBlock,
};

enum class EStatementCount : uint8
{
	Zero,
	One,
	Two,
	Many,
};

enum class EStatementTransfer : uint8
{
	None,
	BreakLoop,
	BreakSwitch,
	Continue,
	EarlyReturn,
	NestedReturn,
	Fallthrough,
	Exception,
};

enum class EStatementNesting : uint8
{
	None,
	SameKind,
	MixedLoop,
	LoopSwitch,
	BranchLoop,
	ThreeLevel,
};

struct FStatementTransferParams
{
	EStatementKind Statement = EStatementKind::If;
	EStatementCount Count = EStatementCount::Zero;
	EStatementTransfer Transfer = EStatementTransfer::None;
	EStatementNesting Nesting = EStatementNesting::None;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FStatementTransferGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildStatementTransferSource(const FStatementTransferParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
