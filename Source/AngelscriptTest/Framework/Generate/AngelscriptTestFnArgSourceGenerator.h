#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EFnArgSourceDirection : uint8
{
	Value,
	In,
	Out,
	Inout,
};

enum class EFnArgSourceSource : uint8
{
	Literal,
	LocalLvalue,
	ConstLocal,
	GlobalConst,
	Field,
	FunctionReturn,
	ArithmeticExpression,
	ConditionalExpression,
	Null,
	BaseView,
	DerivedView,
};

struct FFnArgSourceParams
{
	EFnArgSourceDirection Direction = EFnArgSourceDirection::Value;
	EFnArgSourceSource Source = EFnArgSourceSource::Literal;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FFnArgSourceGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildArgumentSource(const FFnArgSourceParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
