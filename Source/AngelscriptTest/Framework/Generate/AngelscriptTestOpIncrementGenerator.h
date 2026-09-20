#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpIncrementCategory : uint8
{
	Local,
	Field,
	Property,
	Alias,
};

enum class EOpIncrementObservation : uint8
{
	Before,
	ExpressionResult,
	After,
};

enum class EOpIncrementOperator : uint8
{
	PreIncrement,
	PostIncrement,
	PreDecrement,
	PostDecrement,
};

enum class EOpIncrementType : uint8
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

struct FOpIncrementParams
{
	EOpIncrementCategory Category = EOpIncrementCategory::Local;
	EOpIncrementObservation Observation = EOpIncrementObservation::Before;
	EOpIncrementOperator Operator = EOpIncrementOperator::PreIncrement;
	EOpIncrementType Type = EOpIncrementType::Int;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpIncrementGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildLegalSource(const FOpIncrementParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
