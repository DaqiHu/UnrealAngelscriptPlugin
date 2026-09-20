#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EBranchConditionBranch : uint8
{
	If,
	IfElse,
	ElseIfChain,
};

enum class EBranchConditionKind : uint8
{
	Variable,
	Comparison,
	Logical,
	Negated,
	SideEffect,
};

enum class EBranchConditionSelection : uint8
{
	First,
	Second,
	None,
};

enum class EBranchConditionLineEnding : uint8
{
	Lf,
	Crlf,
};

struct FBranchConditionParams
{
	EBranchConditionBranch Branch = EBranchConditionBranch::If;
	EBranchConditionKind Condition = EBranchConditionKind::Variable;
	EBranchConditionSelection Selection = EBranchConditionSelection::First;
	EBranchConditionLineEnding LineEnding = EBranchConditionLineEnding::Lf;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FBranchConditionGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildBranchSource(const FBranchConditionParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
