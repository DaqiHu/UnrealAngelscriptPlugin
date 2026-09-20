#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpNumericBinaryType : uint8
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

enum class EOpNumericBinaryOperator : uint8
{
	Add,
	Subtract,
	Multiply,
	Divide,
	Modulo,
	Less,
	LessEqual,
	Greater,
	GreaterEqual,
	Equal,
	NotEqual,
};

enum class EOpNumericBinaryValue : uint8
{
	Zero,
	One,
	Negative,
	NearMin,
	NearMax,
};

struct FOpNumericBinaryParams
{
	EOpNumericBinaryType LeftType = EOpNumericBinaryType::Int;
	EOpNumericBinaryOperator Operator = EOpNumericBinaryOperator::Add;
	EOpNumericBinaryType RightType = EOpNumericBinaryType::Int;
	EOpNumericBinaryValue Value = EOpNumericBinaryValue::Zero;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpNumericBinaryGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildNumericBinarySource(const FOpNumericBinaryParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
