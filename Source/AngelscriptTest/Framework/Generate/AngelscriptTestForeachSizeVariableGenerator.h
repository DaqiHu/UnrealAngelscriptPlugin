#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EForeachSize : uint8
{
	Empty,
	One,
	Two,
	Many,
};

enum class EForeachElement : uint8
{
	Primitive,
	ValueObject,
	ReferenceObject,
	ConstElement,
};

enum class EForeachVariable : uint8
{
	Value,
	Auto,
	MutableReference,
	ConstReference,
	Incompatible,
};

enum class EForeachSizeTransfer : uint8
{
	Complete,
	BreakFirst,
	BreakMiddle,
	BreakLast,
	ContinueFirst,
	ContinueMiddle,
	Return,
	Exception,
};

struct FForeachSizeVariableParams
{
	EForeachSize Size = EForeachSize::Empty;
	EForeachElement Element = EForeachElement::Primitive;
	EForeachVariable Variable = EForeachVariable::Value;
	EForeachSizeTransfer Transfer = EForeachSizeTransfer::Complete;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FForeachSizeVariableGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildForeachIterationSource(const FForeachSizeVariableParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
