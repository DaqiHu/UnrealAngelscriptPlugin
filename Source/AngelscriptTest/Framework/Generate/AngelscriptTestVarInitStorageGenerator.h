#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EVarInitKind : uint8
{
	Default,
	Literal,
	Expression,
	Copy,
	Constructor,
	FunctionReturn,
	Conditional,
};

enum class EVarInitStorage : uint8
{
	Local,
	ConstLocal,
	Auto,
	LoopInitializer,
	BranchLocal,
	ConstGlobal,
	FieldLinkage,
};

enum class EVarInitType : uint8
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

struct FVarInitStorageParams
{
	EVarInitKind Initializer = EVarInitKind::Default;
	EVarInitStorage Storage = EVarInitStorage::Local;
	EVarInitType Type = EVarInitType::Int8;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FVarInitStorageGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildVariableSource(const FVarInitStorageParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
