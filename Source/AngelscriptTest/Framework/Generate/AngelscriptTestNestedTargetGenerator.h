#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class ENestedTargetNesting : uint8
{
	NestedLoop,
	BranchLoop,
	ThreeLevel,
};

enum class ENestedTargetTransfer : uint8
{
	Break,
	Continue,
	Return,
};

enum class ENestedTargetSite : uint8
{
	Inner,
	Outer,
};

struct FNestedTargetParams
{
	ENestedTargetNesting Nesting = ENestedTargetNesting::NestedLoop;
	ENestedTargetTransfer Transfer = ENestedTargetTransfer::Break;
	ENestedTargetSite Target = ENestedTargetSite::Inner;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FNestedTargetGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildNestedSource(const FNestedTargetParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
