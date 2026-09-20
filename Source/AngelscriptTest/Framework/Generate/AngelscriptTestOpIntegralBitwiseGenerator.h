#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpIntegralBitwiseCategory : uint8
{
	MutableLValue,
	ConstLValue,
	Temporary,
	Field,
	Alias,
};

enum class EOpIntegralBitwiseOperator : uint8
{
	BitAnd,
	BitOr,
	BitXor,
	ShiftLeft,
	ShiftRightLogical,
	ShiftRightArithmetic,
};

enum class EOpIntegralBitwiseRight : uint8
{
	Zero,
	One,
	SourceWidthMinusOne,
	SourceWidth,
	SourceWidthPlusOne,
	Large,
	Negative,
};

enum class EOpIntegralBitwiseType : uint8
{
	Int8,
	Int16,
	Int,
	Int64,
	Uint8,
	Uint16,
	Uint,
	Uint64,
};

struct FOpIntegralBitwiseParams
{
	EOpIntegralBitwiseCategory Category = EOpIntegralBitwiseCategory::MutableLValue;
	EOpIntegralBitwiseOperator Operator = EOpIntegralBitwiseOperator::BitAnd;
	EOpIntegralBitwiseRight Right = EOpIntegralBitwiseRight::Zero;
	EOpIntegralBitwiseType Type = EOpIntegralBitwiseType::Int;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpIntegralBitwiseGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildBitwiseSource(const FOpIntegralBitwiseParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
