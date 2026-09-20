#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EFnArityTypeStressArity : uint8
{
	Zero,
	One,
	Two,
	Four,
	Eight,
	Sixteen,
	ThirtyTwo,
	SixtyFour,
};

enum class EFnArityTypeStressPattern : uint8
{
	HomogeneousInt,
	HomogeneousBool,
	AlternatingIntBool,
	AlternatingBoolInt,
};

enum class EFnArityTypeStressTarget : uint8
{
	Global,
	NamespaceGlobal,
	InstanceMethod,
};

struct FFnArityTypeStressParams
{
	EFnArityTypeStressArity Arity = EFnArityTypeStressArity::Zero;
	EFnArityTypeStressPattern Pattern = EFnArityTypeStressPattern::HomogeneousInt;
	EFnArityTypeStressTarget Target = EFnArityTypeStressTarget::Global;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FFnArityTypeStressGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildSource(const FFnArityTypeStressParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
