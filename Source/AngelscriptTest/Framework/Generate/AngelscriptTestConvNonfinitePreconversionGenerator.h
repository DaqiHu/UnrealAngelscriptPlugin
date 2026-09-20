#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EConvNonfiniteSource : uint8
{
	Float32,
	Float64,
};

enum class EConvNonfiniteTarget : uint8
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

enum class EConvNonfiniteForm : uint8
{
	Assignment,
	Argument,
	Return,
	ExplicitCast,
};

enum class EConvNonfiniteValue : uint8
{
	PositiveInfinity,
	NegativeInfinity,
	Nan,
};

struct FConvNonfinitePreconversionParams
{
	EConvNonfiniteSource Source = EConvNonfiniteSource::Float32;
	EConvNonfiniteTarget Target = EConvNonfiniteTarget::Int8;
	EConvNonfiniteForm Form = EConvNonfiniteForm::Assignment;
	EConvNonfiniteValue Value = EConvNonfiniteValue::PositiveInfinity;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FConvNonfinitePreconversionGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildNumericBoundaryConversionSource(const FConvNonfinitePreconversionParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
