#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpResultContextFamily : uint8
{
	Unary,
	Arithmetic,
	Power,
	Bitwise,
	Shift,
	Comparison,
	Logical,
	Assignment,
	Increment,
	Overloaded,
};

enum class EOpResultContextContext : uint8
{
	Assignment,
	Return,
	Condition,
	OverloadArgument,
	Chain,
	SwitchOrIndex,
};

enum class EOpResultContextOutcome : uint8
{
	Exact,
	Converted,
	Ambiguous,
	Rejected,
};

struct FOpResultContextParams
{
	EOpResultContextFamily Family = EOpResultContextFamily::Unary;
	EOpResultContextContext Context = EOpResultContextContext::Assignment;
	EOpResultContextOutcome Outcome = EOpResultContextOutcome::Exact;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpResultContextGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildSource(const FOpResultContextParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
