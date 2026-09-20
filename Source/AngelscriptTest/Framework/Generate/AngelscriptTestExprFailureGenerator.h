#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EExprFailureContext : uint8
{
	Initializer,
	Assignment,
	Argument,
	Return,
	Condition,
	LoopClause,
	SwitchSelector,
	Index,
};

enum class EExprFailureKind : uint8
{
	InvalidLValue,
	MissingDelimiter,
	MalformedTernary,
	MissingSymbol,
	AmbiguousSymbol,
	InaccessibleMember,
	MissingMember,
	DivideZero,
	IndexOutOfRange,
	NullAccess,
	ExceptionLeft,
	ExceptionRight,
};

enum class EExprFailureRecovery : uint8
{
	FreshModule,
	RebuildOrContextReuse,
};

struct FExprFailureParams
{
	EExprFailureContext Context = EExprFailureContext::Initializer;
	EExprFailureKind Failure = EExprFailureKind::InvalidLValue;
	EExprFailureRecovery Recovery = EExprFailureRecovery::FreshModule;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FExprFailureGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildExpressionFailureSource(const FExprFailureParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
