#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EConvObjectCastKind : uint8
{
	Base,
	Derived,
	Unrelated,
	Null,
};

enum class EConvObjectCastView : uint8
{
	Base,
	Derived,
	Unrelated,
};

enum class EConvObjectCastForm : uint8
{
	Assignment,
	Initializer,
	Argument,
	Return,
	ExplicitCast,
};

struct FConvObjectCastParams
{
	EConvObjectCastKind Kind = EConvObjectCastKind::Base;
	EConvObjectCastView Source = EConvObjectCastView::Base;
	EConvObjectCastView Target = EConvObjectCastView::Base;
	EConvObjectCastForm Form = EConvObjectCastForm::Assignment;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FConvObjectCastGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildObjectCastSource(const FConvObjectCastParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
