#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class ERefFailureKind : uint8
{
	ExplicitHandle,
	ConstRemoval,
	TemporaryOut,
	ExpiredLocalReturn,
	UnrelatedAssignment,
	UnrelatedCast,
	NullMemberAccess,
	StaleModuleObject,
	AmbiguousOverload,
	IncompatibleInOut,
};

enum class ERefFailureRecovery : uint8
{
	FreshModule,
	SameModuleOrContext,
};

struct FRefFailureParams
{
	ERefFailureKind Failure = ERefFailureKind::ExplicitHandle;
	ERefFailureRecovery Recovery = ERefFailureRecovery::FreshModule;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FRefFailureGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildReferenceFailureSource(const FRefFailureParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
