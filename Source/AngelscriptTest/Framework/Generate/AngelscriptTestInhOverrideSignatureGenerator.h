#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EInhOverrideSignatureDimension : uint8
{
	ParameterType,
	ParameterCount,
	ReturnType,
	Constness,
	Visibility,
	NameHiding,
};

enum class EInhOverrideSignatureVariant : uint8
{
	Exact,
	CompatibleOverload,
	Incompatible,
};

enum class EInhOverrideSignatureView : uint8
{
	Base,
	Derived,
	ExplicitBase,
};

struct FInhOverrideSignatureParams
{
	EInhOverrideSignatureDimension Dimension = EInhOverrideSignatureDimension::ParameterType;
	EInhOverrideSignatureVariant Variant = EInhOverrideSignatureVariant::Exact;
	EInhOverrideSignatureView View = EInhOverrideSignatureView::Base;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FInhOverrideSignatureGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildOverrideSignatureSource(const FInhOverrideSignatureParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
