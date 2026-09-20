#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EFnMixinFreeCallRejectionNamespace : uint8
{
	Global,
	Nested,
};

struct FFnMixinFreeCallRejectionParams
{
	EFnMixinFreeCallRejectionNamespace Namespace = EFnMixinFreeCallRejectionNamespace::Global;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FFnMixinFreeCallRejectionGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildFnMixinFreeCallRejectionSource(const FFnMixinFreeCallRejectionParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
