#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class ECtorParamSelectArity : uint8
{
	One,
	Two,
	Five,
	Sixteen,
};

enum class ECtorParamSelectSelection : uint8
{
	Exact,
	Promotion,
	ExplicitConversion,
	Ambiguous,
	Missing,
};

enum class ECtorParamSelectType : uint8
{
	Int8,
	Int16,
	Int,
	Int64,
	Uint8,
	Uint16,
	Uint,
	Uint64,
	Float32,
	Float64,
	Bool,
	Enum,
	Typedef,
	ScriptValue,
	NativeValue,
};

struct FCtorParamSelectParams
{
	ECtorParamSelectArity Arity = ECtorParamSelectArity::One;
	ECtorParamSelectSelection Selection = ECtorParamSelectSelection::Exact;
	ECtorParamSelectType Type = ECtorParamSelectType::Int8;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FCtorParamSelectGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildConstructorParameterSource(const FCtorParamSelectParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
