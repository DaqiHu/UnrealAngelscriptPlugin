#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EVarAssignKind : uint8
{
	Simple,
	CopySource,
	SelfAssignment,
	Compound,
	ReferenceRebind,
};

enum class EVarAssignTarget : uint8
{
	MutableLocal,
	ConstLocal,
	MutableField,
	ConstField,
	ReferenceAlias,
	Temporary,
	ExpressionResult,
};

enum class EVarAssignType : uint8
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
	ScriptReference,
	NativeReference,
};

struct FVarAssignTargetParams
{
	EVarAssignKind Assignment = EVarAssignKind::Simple;
	EVarAssignTarget Target = EVarAssignTarget::MutableLocal;
	EVarAssignType Type = EVarAssignType::Int8;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FVarAssignTargetGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildAssignmentSource(const FVarAssignTargetParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
