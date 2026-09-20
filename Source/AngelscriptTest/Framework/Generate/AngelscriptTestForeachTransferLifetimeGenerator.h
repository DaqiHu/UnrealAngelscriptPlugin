#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EForeachLifetimeTransfer : uint8
{
	Complete,
	BreakFirst,
	BreakMiddle,
	ContinueFirst,
	ContinueMiddle,
	Return,
	Exception,
};

enum class EForeachLifetimeNesting : uint8
{
	Single,
	NestedSame,
	NestedDistinct,
	InsideFor,
};

enum class EForeachLifetimeElement : uint8
{
	PrimitiveValue,
	ValueObjectCopy,
	ValueObjectConstRef,
};

struct FForeachTransferLifetimeParams
{
	EForeachLifetimeTransfer Transfer = EForeachLifetimeTransfer::Complete;
	EForeachLifetimeNesting Nesting = EForeachLifetimeNesting::Single;
	EForeachLifetimeElement Element = EForeachLifetimeElement::PrimitiveValue;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FForeachTransferLifetimeGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildTransferSource(const FForeachTransferLifetimeParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
