#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EDtorDeclarationScenario : uint8
{
	ImplicitScriptValue,
	DeclaredScriptValue,
	ImplicitScriptReference,
	DeclaredScriptReference,
	NativeValue,
	NativeReference,
	EmptyDestructor,
	FieldDestructor,
	BaseDerivedDestructor,
	PrivateDestructor,
	ThrowingDestructor,
	ThrowingDerivedMembersBase,
	MalformedDestructor,
};

enum class EDtorDeclarationObservation : uint8
{
	Compile,
	Metadata,
	Runtime,
	Cleanup,
};

struct FDtorDeclarationParams
{
	EDtorDeclarationScenario Scenario = EDtorDeclarationScenario::ImplicitScriptValue;
	EDtorDeclarationObservation Observation = EDtorDeclarationObservation::Compile;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FDtorDeclarationGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildDestructorDeclarationSource(const FDtorDeclarationParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
