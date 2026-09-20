#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
struct FForLoopParams
{
	bool bInit = false;
	bool bCondition = false;
	bool bIncrement = false;
	int32 Limit = 0;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FForLoopGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildForSource(const FForLoopParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
