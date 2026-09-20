#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EFnTypedDefaultsArity : uint8
{
	One,
	Two,
	Three,
	Four,
};

enum class EFnTypedDefaultsTypePattern : uint8
{
	HomogeneousInt,
	HomogeneousBool,
	AlternatingIntBool,
	AlternatingBoolInt,
};

enum class EFnTypedDefaultsTarget : uint8
{
	Global,
	NamespaceGlobal,
	InstanceMethod,
};

struct FFnTypedDefaultsParams
{
	EFnTypedDefaultsArity Arity = EFnTypedDefaultsArity::One;
	EFnTypedDefaultsTypePattern Type = EFnTypedDefaultsTypePattern::HomogeneousInt;
	EFnTypedDefaultsTarget Target = EFnTypedDefaultsTarget::Global;
	int32 OmittedCount = 0;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FFnTypedDefaultsGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildSource(const FFnTypedDefaultsParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
