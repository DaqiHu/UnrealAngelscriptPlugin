#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpFailureFamily : uint8
{
	UnsupportedOperand,
	DivideZero,
	ModuloZero,
	InvalidShiftCount,
	SignedOverflow,
	SignedRemainderOverflow,
	PowerOverflow,
	InvalidLValue,
	ConstMutation,
	MissingOverload,
	AmbiguousOverload,
	InvalidSignature,
	DuplicateOperator,
	NullReceiver,
	LeftOperandException,
	RightOperandException,
	AssignmentException,
};

enum class EOpFailureObservation : uint8
{
	DiagnosticOrException,
	Cleanup,
	RecoveryResult,
};

enum class EOpFailureRecovery : uint8
{
	FreshModule,
	SameModuleOrContext,
};

struct FOpFailureParams
{
	EOpFailureFamily Family = EOpFailureFamily::InvalidShiftCount;
	EOpFailureObservation Observation = EOpFailureObservation::DiagnosticOrException;
	EOpFailureRecovery Recovery = EOpFailureRecovery::FreshModule;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpFailureGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildFailureSource(const FOpFailureParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
