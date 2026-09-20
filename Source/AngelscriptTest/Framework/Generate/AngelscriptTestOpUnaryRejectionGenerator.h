#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpUnaryRejectionCategory : uint8
{
	MutableLvalue,
	ConstLvalue,
	Temporary,
	Field,
	Alias,
};

enum class EOpUnaryRejectionFailure : uint8
{
	PositiveBool,
	NegativeBool,
	BitNotBool,
	LogicalNotSigned,
	LogicalNotUnsigned,
	LogicalNotFloat,
	BitNotFloat32,
	BitNotFloat64,
};

struct FOpUnaryRejectionParams
{
	EOpUnaryRejectionCategory Category = EOpUnaryRejectionCategory::MutableLvalue;
	EOpUnaryRejectionFailure Failure = EOpUnaryRejectionFailure::PositiveBool;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpUnaryRejectionGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildUnaryFailureSource(const FOpUnaryRejectionParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
