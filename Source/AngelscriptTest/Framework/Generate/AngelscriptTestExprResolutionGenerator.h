#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EExprResolutionContext : uint8
{
	Initializer,
	Assignment,
	Argument,
	Return,
	Condition,
	Index,
	MemberReceiver,
};

enum class EExprResolutionShape : uint8
{
	Identifier,
	Call,
	Member,
	ScopedName,
};

enum class EExprResolutionState : uint8
{
	Exact,
	NamespaceQualified,
	Overload,
	Conversion,
	Missing,
	Ambiguous,
	Inaccessible,
	WrongType,
};

struct FExprResolutionParams
{
	EExprResolutionContext Context = EExprResolutionContext::Initializer;
	EExprResolutionShape Shape = EExprResolutionShape::Identifier;
	EExprResolutionState State = EExprResolutionState::Exact;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FExprResolutionGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildExpressionResolutionSource(const FExprResolutionParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
