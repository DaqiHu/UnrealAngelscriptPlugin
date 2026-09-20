#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EPropFailureKind : uint8
{
	RemovedPropertyDecorator,
	RemovedVirtualProperty,
	MissingGetter,
	MissingSetter,
	RegisteredMismatchedTypes,
	RegisteredDuplicateGetter,
	RegisteredDuplicateSetter,
	RecursiveGetter,
	RecursiveSetter,
	ThrowingGetter,
	ThrowingSetter,
	NullReceiver,
	InaccessibleField,
	CompoundValueReceiver,
};

enum class EPropFailureProbe : uint8
{
	Direct,
	AlternatePath,
};

enum class EPropFailureRecovery : uint8
{
	FreshModule,
	SameModuleOrContext,
};

struct FPropFailureParams
{
	EPropFailureKind Failure = EPropFailureKind::RemovedPropertyDecorator;
	EPropFailureProbe Probe = EPropFailureProbe::Direct;
	EPropFailureRecovery Recovery = EPropFailureRecovery::FreshModule;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FPropFailureGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildPropertyFailureSource(const FPropFailureParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
