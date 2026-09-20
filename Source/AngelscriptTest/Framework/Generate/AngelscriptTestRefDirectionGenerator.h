#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class ERefDirectionKind : uint8
{
	Value,
	In,
	Out,
	InOut,
};

enum class ERefDirectionNull : uint8
{
	NonNull,
	NullInput,
	NullOutput,
	NullReturn,
};

enum class ERefDirectionRelation : uint8
{
	SameTwoNames,
	Distinct,
	SelfAssignment,
	BaseDerivedViews,
	OutReplacement,
	InOutMutation,
};

struct FRefDirectionParams
{
	ERefDirectionKind Direction = ERefDirectionKind::Value;
	ERefDirectionNull NullState = ERefDirectionNull::NonNull;
	ERefDirectionRelation Relation = ERefDirectionRelation::SameTwoNames;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FRefDirectionGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildReferenceDirectionSource(const FRefDirectionParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
