#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class ERefDirectionForkGlobalState : uint8
{
	Mutable,
};

struct FRefDirectionForkGlobalParams
{
	ERefDirectionForkGlobalState State = ERefDirectionForkGlobalState::Mutable;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FRefDirectionForkGlobalGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildRefDirectionForkGlobalSource(const FRefDirectionForkGlobalParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
