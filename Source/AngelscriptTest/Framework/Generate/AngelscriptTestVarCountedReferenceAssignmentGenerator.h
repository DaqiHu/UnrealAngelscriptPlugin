#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EVarReferenceAssignmentScenario : uint8
{
	FactoryLocal,
	Overwrite,
	NullAssignment,
	ParameterReturn,
	ExceptionFrame,
	SaveLoad,
	ParameterReturnSaveLoad,
};

struct FVarCountedReferenceAssignmentParams
{
	EVarReferenceAssignmentScenario Scenario = EVarReferenceAssignmentScenario::FactoryLocal;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FVarCountedReferenceAssignmentGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildReferenceAssignmentSource(const FVarCountedReferenceAssignmentParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
