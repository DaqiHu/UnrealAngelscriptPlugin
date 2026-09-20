#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class ECtorOrderFailureFailure : uint8
{
	None,
	Base,
	MemberFirst,
	MemberMiddle,
	MemberLast,
	DerivedBody,
	Copy,
	Conversion,
};

enum class ECtorOrderFailureDepth : uint8
{
	FlatMembers,
	NestedMembers,
	DeepNestedMembers,
	BaseAndDerivedMembers,
};

enum class ECtorOrderFailureObservation : uint8
{
	Values,
	EventOrder,
	Cleanup,
	ContextReuse,
};

struct FCtorOrderFailureParams
{
	ECtorOrderFailureFailure Failure = ECtorOrderFailureFailure::None;
	ECtorOrderFailureDepth Depth = ECtorOrderFailureDepth::FlatMembers;
	ECtorOrderFailureObservation Observation = ECtorOrderFailureObservation::Values;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FCtorOrderFailureGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildConstructorFailureSource(const FCtorOrderFailureParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
