#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EConvNumericType : uint8
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

enum class EConvNumericForm : uint8
{
	Assignment,
	Initializer,
	Argument,
	Return,
	Promotion,
	ExplicitCast,
};

enum class EConvNumericValue : uint8
{
	Zero,
	One,
	Negative,
	Min,
	Max,
	NearBoundary,
	Fractional,
};

struct FConvNumericParams
{
	EConvNumericType Source = EConvNumericType::Int8;
	EConvNumericType Target = EConvNumericType::Int8;
	EConvNumericForm Form = EConvNumericForm::Assignment;
	EConvNumericValue Value = EConvNumericValue::Zero;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FConvNumericGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildNumericConversionSource(const FConvNumericParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
