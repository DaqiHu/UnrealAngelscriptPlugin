#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EFnArityTargetArity : uint8
{
	Zero,
	One,
	Two,
	Three,
	Eight,
	CurrentBoundary,
	BoundaryPlusOne,
};

enum class EFnArityTargetTarget : uint8
{
	Global,
	NamespaceGlobal,
	InstanceMethod,
};

struct FFnArityTargetParams
{
	EFnArityTargetArity Arity = EFnArityTargetArity::Zero;
	EFnArityTargetTarget Target = EFnArityTargetTarget::Global;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FFnArityTargetGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildAritySource(const FFnArityTargetParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
