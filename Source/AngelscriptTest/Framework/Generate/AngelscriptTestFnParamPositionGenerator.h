#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EFnParamPositionDirection : uint8
{
	Value,
	In,
	Out,
	Inout,
};

enum class EFnParamPositionPosition : uint8
{
	First,
	Middle,
	Last,
};

enum class EFnParamPositionType : uint8
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

struct FFnParamPositionParams
{
	EFnParamPositionDirection Direction = EFnParamPositionDirection::Value;
	EFnParamPositionPosition Position = EFnParamPositionPosition::First;
	EFnParamPositionType Type = EFnParamPositionType::Int8;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FFnParamPositionGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildParameterPositionSource(const FFnParamPositionParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
