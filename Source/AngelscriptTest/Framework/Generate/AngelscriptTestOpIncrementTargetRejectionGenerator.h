#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpIncrementTargetRejectionTarget : uint8
{
	ConstInvalid,
	TemporaryInvalid,
};

enum class EOpIncrementTargetRejectionOperator : uint8
{
	PreIncrement,
	PostIncrement,
	PreDecrement,
	PostDecrement,
};

enum class EOpIncrementTargetRejectionType : uint8
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
};

struct FOpIncrementTargetRejectionParams
{
	EOpIncrementTargetRejectionTarget Target = EOpIncrementTargetRejectionTarget::ConstInvalid;
	EOpIncrementTargetRejectionOperator Operator = EOpIncrementTargetRejectionOperator::PreIncrement;
	EOpIncrementTargetRejectionType Type = EOpIncrementTargetRejectionType::Int;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpIncrementTargetRejectionGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildTargetRejectionSource(const FOpIncrementTargetRejectionParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
