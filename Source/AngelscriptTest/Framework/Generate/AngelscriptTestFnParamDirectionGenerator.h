#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EFnParamDirectionDirection : uint8
{
	Value,
	In,
	Out,
	Inout,
};

enum class EFnParamDirectionType : uint8
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

struct FFnParamDirectionParams
{
	EFnParamDirectionDirection Direction = EFnParamDirectionDirection::Value;
	EFnParamDirectionType Type = EFnParamDirectionType::Int8;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FFnParamDirectionGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildParameterDirectionSource(const FFnParamDirectionParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
