#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EDtorPartialTopology : uint8
{
	Independent,
	NestedMember,
	BaseDerived,
	CopyTransfer,
	AssignmentTransfer,
	SelfAssignment,
	ReferenceAlias,
};

enum class EDtorPartialBoundary : uint8
{
	BeforeRoot,
	RootStarted,
	FirstOwned,
	MiddleOwned,
	AllOwned,
	Complete,
};

enum class EDtorPartialExit : uint8
{
	Normal,
	Exception,
	Abort,
	Unprepare,
};

struct FDtorPartialParams
{
	EDtorPartialTopology Topology = EDtorPartialTopology::Independent;
	EDtorPartialBoundary Boundary = EDtorPartialBoundary::BeforeRoot;
	EDtorPartialExit Exit = EDtorPartialExit::Normal;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FDtorPartialGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildDestructorPartialSource(const FDtorPartialParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
