#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EConvOverloadContext : uint8
{
	Overload,
	Operator,
	DefaultArgument,
	Property,
	Index,
	Conditional,
};

enum class EConvOverloadConversion : uint8
{
	Identity,
	Promotion,
	Narrowing,
	Constructor,
	Operator,
	ReferenceCast,
};

enum class EConvOverloadOutcome : uint8
{
	Exact,
	SelectedConversion,
	Ambiguous,
	Rejected,
};

struct FConvOverloadParams
{
	EConvOverloadContext Context = EConvOverloadContext::Overload;
	EConvOverloadConversion Conversion = EConvOverloadConversion::Identity;
	EConvOverloadOutcome Outcome = EConvOverloadOutcome::Exact;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FConvOverloadGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildResolutionSource(const FConvOverloadParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
