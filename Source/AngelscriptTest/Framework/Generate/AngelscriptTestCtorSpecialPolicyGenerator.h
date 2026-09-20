#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class ECtorSpecialPolicyObservation : uint8
{
	Compile,
	Metadata,
	Runtime,
	Lifecycle,
};

enum class ECtorSpecialPolicyScenario : uint8
{
	ImplicitStructDefault,
	DeclaredStructDefault,
	ParameterPreservesGeneratedDefault,
	ParameterSuppressesDefaultOptionOff,
	ImplicitStructCopy,
	DeclaredStructCopy,
	ImplicitStructAssignment,
	DeclaredStructAssignment,
	UserDestructorCopy,
	ClassFactoryDefault,
	ClassParameterFactory,
	DerivedGeneratedDefault,
	DerivedExplicitSuper,
	MissingBaseDefaultOptionOff,
	CopyAfterUserConstructor,
	AssignmentSelfStability,
};

struct FCtorSpecialPolicyParams
{
	ECtorSpecialPolicyObservation Observation = ECtorSpecialPolicyObservation::Compile;
	ECtorSpecialPolicyScenario Scenario = ECtorSpecialPolicyScenario::ImplicitStructDefault;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FCtorSpecialPolicyGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildConstructorPolicySource(const FCtorSpecialPolicyParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
