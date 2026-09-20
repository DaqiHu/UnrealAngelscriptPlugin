#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EConvFloatFiniteSpecialSource : uint8
{
	Float32,
	Float64,
};

enum class EConvFloatFiniteSpecialTarget : uint8
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

enum class EConvFloatFiniteSpecialForm : uint8
{
	Assignment,
	Argument,
	Return,
	ExplicitCast,
};

enum class EConvFloatFiniteSpecialValue : uint8
{
	PositiveZero,
	NegativeZero,
	Subnormal,
};

struct FConvFloatFiniteSpecialParams
{
	EConvFloatFiniteSpecialSource SourceType = EConvFloatFiniteSpecialSource::Float32;
	EConvFloatFiniteSpecialTarget TargetType = EConvFloatFiniteSpecialTarget::Int8;
	EConvFloatFiniteSpecialForm Form = EConvFloatFiniteSpecialForm::Assignment;
	EConvFloatFiniteSpecialValue Value = EConvFloatFiniteSpecialValue::PositiveZero;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FConvFloatFiniteSpecialGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildNumericBoundaryConversionSource(const FConvFloatFiniteSpecialParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
