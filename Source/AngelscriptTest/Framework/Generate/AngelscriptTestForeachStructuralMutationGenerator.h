#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EForeachMutationSize : uint8
{
	One,
	Two,
	Many,
};

enum class EForeachMutation : uint8
{
	Stable,
	ShrinkFirst,
	ShrinkMiddle,
	ClearAfterFirst,
};

enum class EForeachMutationElement : uint8
{
	PrimitiveValue,
	ValueObjectCopy,
	ValueObjectConstRef,
};

struct FForeachStructuralMutationParams
{
	EForeachMutationSize Size = EForeachMutationSize::One;
	EForeachMutation Mutation = EForeachMutation::Stable;
	EForeachMutationElement Element = EForeachMutationElement::PrimitiveValue;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FForeachStructuralMutationGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildStructuralMutationSource(const FForeachStructuralMutationParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
