#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EFnReturnPath : uint8
{
	Direct,
	IfElse,
	Switch,
	Early,
	RecursiveBase,
	Exception,
	MissingInvalid,
	IncompatibleInvalid,
};

enum class EFnReturnType : uint8
{
	Void,
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
	ScriptReference,
	NullReference,
};

struct FFnReturnParams
{
	EFnReturnPath Path = EFnReturnPath::Direct;
	EFnReturnType Type = EFnReturnType::Void;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FFnReturnGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildReturnSource(const FFnReturnParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
