#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EPropValueOpOperation : uint8
{
	DefaultRead,
	Write,
	CompoundWrite,
	Copy,
	ReferenceMutation,
};

enum class EPropValueOpReceiver : uint8
{
	Mutable,
	Const,
	BaseView,
	DerivedView,
};

enum class EPropValueOpType : uint8
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
};

struct FPropValueOpParams
{
	EPropValueOpOperation Operation = EPropValueOpOperation::DefaultRead;
	EPropValueOpReceiver Receiver = EPropValueOpReceiver::Mutable;
	EPropValueOpType Type = EPropValueOpType::Int8;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FPropValueOpGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildStoredPropertySource(const FPropValueOpParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
