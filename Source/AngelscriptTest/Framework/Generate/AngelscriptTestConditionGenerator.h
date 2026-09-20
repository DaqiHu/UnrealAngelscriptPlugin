#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EConditionStatement : uint8
{
	If,
	While,
	DoWhile,
	For,
};

enum class EConditionExpr : uint8
{
	BoolLiteral,
	Variable,
	Comparison,
	Logical,
	SideEffectCall,
	OverloadedConversion,
	InvalidType,
};

enum class EConditionTruth : uint8
{
	False,
	True,
};

struct FConditionParams
{
	EConditionStatement Statement = EConditionStatement::If;
	EConditionExpr Condition = EConditionExpr::BoolLiteral;
	EConditionTruth Truth = EConditionTruth::True;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FConditionGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildConditionSource(const FConditionParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
