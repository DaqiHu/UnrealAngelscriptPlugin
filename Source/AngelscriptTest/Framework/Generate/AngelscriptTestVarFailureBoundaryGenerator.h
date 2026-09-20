#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EVarFailureRecovery : uint8
{
	FreshModule,
	SameModuleOrContext,
};

enum class EVarFailureScenario : uint8
{
	UseBeforeDeclaration,
	UseAfterScope,
	DuplicateSameScope,
	IncompatibleInitializer,
	MutableGlobal,
	ReferenceGlobal,
	UninitializedRead,
	InitializerException,
	FailedInitializerAtomicity,
	LongIdentifier,
	ManyLocalsSupported,
	ManyLocalsBoundary,
	StackFramePressure,
	ModuleDiscard,
};

struct FVarFailureBoundaryParams
{
	EVarFailureRecovery Recovery = EVarFailureRecovery::FreshModule;
	EVarFailureScenario Scenario = EVarFailureScenario::UseBeforeDeclaration;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FVarFailureBoundaryGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildBoundarySource(const FVarFailureBoundaryParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
