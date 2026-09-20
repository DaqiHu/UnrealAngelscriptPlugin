#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EFnRecursionDepth : uint8
{
	Zero,
	One,
	Eight,
	ConfiguredLimit,
};

enum class EFnRecursionOutcome : uint8
{
	Return,
	Exception,
};

enum class EFnRecursionType : uint8
{
	Primitive,
	ValueObject,
	ReferenceObject,
};

struct FFnRecursionParams
{
	EFnRecursionDepth Depth = EFnRecursionDepth::Zero;
	EFnRecursionOutcome Outcome = EFnRecursionOutcome::Return;
	EFnRecursionType Type = EFnRecursionType::Primitive;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FFnRecursionGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildRecursionSource(const FFnRecursionParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
