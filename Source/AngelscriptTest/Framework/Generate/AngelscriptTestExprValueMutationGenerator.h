#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EExprValueMutationCategory : uint8
{
	MutableLValue,
	ConstLValue,
	Temporary,
	ReferenceAlias,
	Field,
	Property,
	InvalidNonLValue,
};

enum class EExprValueMutationMutation : uint8
{
	Assign,
	CompoundAssign,
	PrefixIncrement,
	PostfixIncrement,
	OutArgument,
};

enum class EExprValueMutationPlacement : uint8
{
	Local,
	Global,
	Member,
	Indexed,
};

struct FExprValueMutationParams
{
	EExprValueMutationCategory Category = EExprValueMutationCategory::MutableLValue;
	EExprValueMutationMutation Mutation = EExprValueMutationMutation::Assign;
	EExprValueMutationPlacement Placement = EExprValueMutationPlacement::Local;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FExprValueMutationGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildExpressionValueCategorySource(const FExprValueMutationParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
