#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpLogicalContext : uint8
{
	Assignment,
	Return,
	Condition,
	Argument,
};

enum class EOpLogicalOperator : uint8
{
	And,
	Or,
	Xor,
};

enum class EOpLogicalSource : uint8
{
	BoolLiteral,
	BoolLValue,
	Comparison,
	ConversionOperator,
};

enum class EOpLogicalTruth : uint8
{
	FalseFalse,
	FalseTrue,
	TrueFalse,
	TrueTrue,
};

struct FOpLogicalParams
{
	EOpLogicalContext Context = EOpLogicalContext::Assignment;
	EOpLogicalOperator Operator = EOpLogicalOperator::And;
	EOpLogicalSource Source = EOpLogicalSource::BoolLiteral;
	EOpLogicalTruth Truth = EOpLogicalTruth::FalseFalse;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpLogicalGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildLogicalSource(const FOpLogicalParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
