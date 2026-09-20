#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class ECtorTransferSource : uint8
{
	ScriptValueLocal,
	ScriptValueTemporary,
	ScriptValueReturn,
	NativeValueLocal,
	NativeValueTemporary,
	NativeValueReturn,
	ScriptReferenceLocal,
	ScriptReferenceTemporary,
	ScriptReferenceReturn,
	NativeReferenceLocal,
	NativeReferenceTemporary,
	NativeReferenceReturn,
	DerivedReferenceExact,
	DerivedReferenceBaseView,
};

enum class ECtorTransferWorkflow : uint8
{
	CopyDeclarationLocal,
	FieldConstructorTransfer,
	AssignmentLocal,
	FieldAssignmentAfterDefault,
	ArgumentTransfer,
	ReturnTransfer,
	SelfAssignment,
	ChainedAssignment,
};

enum class ECtorTransferObservation : uint8
{
	InitialIdentityValue,
	SourceOrTemporaryState,
	TargetMutationRelation,
	LifecycleCleanup,
};

struct FCtorTransferParams
{
	ECtorTransferSource Source = ECtorTransferSource::ScriptValueLocal;
	ECtorTransferWorkflow Workflow = ECtorTransferWorkflow::CopyDeclarationLocal;
	ECtorTransferObservation Observation = ECtorTransferObservation::InitialIdentityValue;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FCtorTransferGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildConstructorTransferSource(const FCtorTransferParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
