#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpPowerFractionalExponentBase : uint8
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

enum class EOpPowerFractionalExponentExponent : uint8
{
	Float32,
	Float64,
};

enum class EOpPowerFractionalExponentShape : uint8
{
	Constant,
	MutableLvalue,
	ConstLvalue,
	FunctionReturn,
};

enum class EOpPowerFractionalExponentScenario : uint8
{
	FractionalExponent,
};

struct FOpPowerFractionalExponentParams
{
	EOpPowerFractionalExponentBase Base = EOpPowerFractionalExponentBase::Int;
	EOpPowerFractionalExponentExponent Exponent = EOpPowerFractionalExponentExponent::Float32;
	EOpPowerFractionalExponentShape Shape = EOpPowerFractionalExponentShape::Constant;
	EOpPowerFractionalExponentScenario Scenario = EOpPowerFractionalExponentScenario::FractionalExponent;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpPowerFractionalExponentGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildPowerSource(const FOpPowerFractionalExponentParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
