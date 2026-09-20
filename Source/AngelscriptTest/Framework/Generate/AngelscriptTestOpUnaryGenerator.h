#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpUnaryCategory : uint8
{
	MutableLvalue,
	ConstLvalue,
	Temporary,
	Field,
	Alias,
};

enum class EOpUnaryOperation : uint8
{
	PositiveInt8,
	PositiveInt16,
	PositiveInt,
	PositiveInt64,
	PositiveUint8,
	PositiveUint16,
	PositiveUint,
	PositiveUint64,
	PositiveFloat32,
	PositiveFloat64,
	NegativeInt8,
	NegativeInt16,
	NegativeInt,
	NegativeInt64,
	NegativeUint8,
	NegativeUint16,
	NegativeUint,
	NegativeUint64,
	NegativeFloat32,
	NegativeFloat64,
	BitNotInt8,
	BitNotInt16,
	BitNotInt,
	BitNotInt64,
	BitNotUint8,
	BitNotUint16,
	BitNotUint,
	BitNotUint64,
};

enum class EOpUnaryValue : uint8
{
	Zero,
	One,
	Negative,
	NearMin,
	NearMax,
};

struct FOpUnaryParams
{
	EOpUnaryCategory Category = EOpUnaryCategory::MutableLvalue;
	EOpUnaryOperation Operation = EOpUnaryOperation::PositiveInt8;
	EOpUnaryValue Value = EOpUnaryValue::Zero;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpUnaryGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildUnarySource(const FOpUnaryParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
