#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EFnDirectionDefaultType : uint8
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
};

enum class EFnDirectionDefaultDirection : uint8
{
	Value,
	In,
	Out,
	Inout,
};

enum class EFnDirectionDefaultState : uint8
{
	NoneExplicit,
	NoneOmitted,
	PresentExplicit,
	PresentOmitted,
};

enum class EFnDirectionDefaultTarget : uint8
{
	Global,
	NamespaceGlobal,
	InstanceMethod,
};

struct FFnDirectionDefaultParams
{
	EFnDirectionDefaultType Type = EFnDirectionDefaultType::Int8;
	EFnDirectionDefaultDirection Direction = EFnDirectionDefaultDirection::Value;
	EFnDirectionDefaultState DefaultState = EFnDirectionDefaultState::NoneExplicit;
	EFnDirectionDefaultTarget Target = EFnDirectionDefaultTarget::Global;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FFnDirectionDefaultGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildSource(const FFnDirectionDefaultParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
