#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EFnMixinDirectDispatchNamespace : uint8
{
	Global,
	Nested,
};

struct FFnMixinDirectDispatchParams
{
	EFnMixinDirectDispatchNamespace Namespace = EFnMixinDirectDispatchNamespace::Global;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FFnMixinDirectDispatchGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildFnMixinDirectDispatchSource(const FFnMixinDirectDispatchParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
