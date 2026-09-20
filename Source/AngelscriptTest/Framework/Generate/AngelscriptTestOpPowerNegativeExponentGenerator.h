#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpPowerNegativeExponentBase : uint8
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

enum class EOpPowerNegativeExponentExponent : uint8
{
	Int8,
	Int16,
	Int,
	Int64,
	Float32,
	Float64,
};

enum class EOpPowerNegativeExponentShape : uint8
{
	Constant,
	MutableLvalue,
	ConstLvalue,
	FunctionReturn,
};

struct FOpPowerNegativeExponentParams
{
	EOpPowerNegativeExponentBase Base = EOpPowerNegativeExponentBase::Int8;
	EOpPowerNegativeExponentExponent Exponent = EOpPowerNegativeExponentExponent::Int8;
	EOpPowerNegativeExponentShape Shape = EOpPowerNegativeExponentShape::Constant;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpPowerNegativeExponentGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildPowerSource(const FOpPowerNegativeExponentParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
