#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EFnOverloadDiscriminator : uint8
{
	Type,
	Arity,
	Const,
	Direction,
	Namespace,
	Default,
	Conversion,
};

enum class EFnOverloadOutcome : uint8
{
	Exact,
	Promotion,
	Ambiguous,
	Missing,
};

struct FFnOverloadParams
{
	EFnOverloadDiscriminator Discriminator = EFnOverloadDiscriminator::Type;
	EFnOverloadOutcome Outcome = EFnOverloadOutcome::Exact;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FFnOverloadGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildOverloadSource(const FFnOverloadParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
