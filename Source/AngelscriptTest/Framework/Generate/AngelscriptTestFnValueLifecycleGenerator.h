#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EFnValueLifecycleFailure : uint8
{
	None,
	ArgumentEvaluation,
	Body,
	ReturnConstruction,
};

enum class EFnValueLifecycleInitialized : uint8
{
	Zero,
	One,
	Many,
};

enum class EFnValueLifecycleTransfer : uint8
{
	ValueArgument,
	ValueReturn,
};

struct FFnValueLifecycleParams
{
	EFnValueLifecycleFailure Failure = EFnValueLifecycleFailure::None;
	EFnValueLifecycleInitialized Initialized = EFnValueLifecycleInitialized::Zero;
	EFnValueLifecycleTransfer Transfer = EFnValueLifecycleTransfer::ValueArgument;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FFnValueLifecycleGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildSource(const FFnValueLifecycleParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
