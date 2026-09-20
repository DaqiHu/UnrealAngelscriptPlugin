#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EConvBoolContextSource : uint8
{
	Int8,
	Int16,
	Int,
	Int64,
	Uint8,
	Uint16,
	Uint,
	Uint64,
	Float32,
	Float64,
	Bool,
	Enum,
	Typedef,
};

enum class EConvBoolContextContext : uint8
{
	If,
	While,
	TernaryCondition,
	LogicalAnd,
	LogicalOr,
	LogicalXor,
};

enum class EConvBoolContextValue : uint8
{
	Zero,
	One,
	Negative,
};

struct FConvBoolContextParams
{
	EConvBoolContextSource Source = EConvBoolContextSource::Int8;
	EConvBoolContextContext Context = EConvBoolContextContext::If;
	EConvBoolContextValue Value = EConvBoolContextValue::Zero;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FConvBoolContextGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildBoolConversionSource(const FConvBoolContextParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
