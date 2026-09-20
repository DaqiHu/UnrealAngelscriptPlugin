#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpPowerUniversalBase : uint8
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

enum class EOpPowerUniversalExponent : uint8
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

enum class EOpPowerUniversalShape : uint8
{
	Constant,
	MutableLvalue,
	ConstLvalue,
	FunctionReturn,
};

enum class EOpPowerUniversalScenario : uint8
{
	ZeroExponent,
	OneExponent,
	NearLimit,
	Overflow,
};

struct FOpPowerUniversalParams
{
	EOpPowerUniversalBase Base = EOpPowerUniversalBase::Int8;
	EOpPowerUniversalExponent Exponent = EOpPowerUniversalExponent::Int8;
	EOpPowerUniversalShape Shape = EOpPowerUniversalShape::Constant;
	EOpPowerUniversalScenario Scenario = EOpPowerUniversalScenario::ZeroExponent;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpPowerUniversalGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildPowerSource(const FOpPowerUniversalParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
