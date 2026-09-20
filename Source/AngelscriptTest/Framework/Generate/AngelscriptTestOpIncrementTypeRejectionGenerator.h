#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpIncrementTypeRejectionCategory : uint8
{
	Local,
	Field,
	Property,
	Alias,
};

enum class EOpIncrementTypeRejectionOperator : uint8
{
	PreIncrement,
	PostIncrement,
	PreDecrement,
	PostDecrement,
};

struct FOpIncrementTypeRejectionParams
{
	EOpIncrementTypeRejectionCategory Category = EOpIncrementTypeRejectionCategory::Local;
	EOpIncrementTypeRejectionOperator Operator = EOpIncrementTypeRejectionOperator::PreIncrement;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpIncrementTypeRejectionGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildBoolRejectionSource(const FOpIncrementTypeRejectionParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
