#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class ECtorBoundaryScenario : uint8
{
	DuplicateDefaultSignature,
	DuplicateParameterSignature,
	MismatchedConstructorName,
	ConstructorValueReturn,
	SuperOutsideConstructor,
	SuperAfterStatement,
	RepeatedSuper,
	MissingBaseArgument,
	AmbiguousBaseArgument,
	InaccessibleBase,
	RecursiveValueFieldDirect,
	RecursiveValueFieldIndirect,
	RecursiveConstructorDirect,
	RecursiveConstructorIndirect,
	MutableReferenceGlobal,
	ModuleDiscardConstValueGlobal,
	EngineShutdownConstValueGlobal,
};

enum class ECtorBoundaryObservation : uint8
{
	CompileOrExecutionState,
	DiagnosticOrMetadata,
	LifecycleCleanup,
	RecoveryOrTeardown,
};

struct FCtorBoundaryParams
{
	ECtorBoundaryScenario Scenario = ECtorBoundaryScenario::DuplicateDefaultSignature;
	ECtorBoundaryObservation Observation = ECtorBoundaryObservation::CompileOrExecutionState;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FCtorBoundaryGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildConstructorBoundarySource(const FCtorBoundaryParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
