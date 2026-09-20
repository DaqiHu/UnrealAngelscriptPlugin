#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EForeachProtocol : uint8
{
	Complete,
	Overloaded,
	MissingBegin,
	MissingNext,
	MissingValue,
	MissingEnd,
	WrongParameter,
	WrongReturn,
	Inaccessible,
	Throwing,
};

enum class EForeachProtocolResolution : uint8
{
	Exact,
	Conversion,
	ConstOverload,
	Ambiguous,
	Missing,
};

enum class EForeachProtocolNesting : uint8
{
	Single,
	SameIterable,
	DistinctIterable,
	InsideFor,
	ContainsFor,
};

struct FForeachProtocolParams
{
	EForeachProtocol Protocol = EForeachProtocol::Complete;
	EForeachProtocolResolution Resolution = EForeachProtocolResolution::Exact;
	EForeachProtocolNesting Nesting = EForeachProtocolNesting::Single;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FForeachProtocolGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildForeachProtocolSource(const FForeachProtocolParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
