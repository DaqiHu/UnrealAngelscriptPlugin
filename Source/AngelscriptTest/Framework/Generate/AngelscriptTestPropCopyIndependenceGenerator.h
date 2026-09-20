#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EPropCopyIndependenceMutation : uint8
{
	SourceAfterTransfer,
	TargetAfterTransfer,
	NestedMember,
};

enum class EPropCopyIndependenceTransfer : uint8
{
	CopyConstruct,
	Assign,
	SelfAssign,
};

enum class EPropCopyIndependenceType : uint8
{
	Int8,
	Int16,
	Int,
	Int64,
	UInt8,
	UInt16,
	UInt,
	UInt64,
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

enum class EPropCopyIndependenceView : uint8
{
	Exact,
	Base,
	Derived,
};

struct FPropCopyIndependenceParams
{
	EPropCopyIndependenceMutation Mutation = EPropCopyIndependenceMutation::SourceAfterTransfer;
	EPropCopyIndependenceTransfer Transfer = EPropCopyIndependenceTransfer::CopyConstruct;
	EPropCopyIndependenceType Type = EPropCopyIndependenceType::Int8;
	EPropCopyIndependenceView View = EPropCopyIndependenceView::Exact;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FPropCopyIndependenceGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildPropertyCopySource(const FPropCopyIndependenceParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
