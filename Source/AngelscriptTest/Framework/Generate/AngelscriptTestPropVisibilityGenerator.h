#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EPropVisibilityAccessPath : uint8
{
	OwnerMethod,
	OwnerConstructor,
	OwnerDestructor,
	AccessorBody,
	DirectDerived,
	DeepDerived,
	UnrelatedType,
	GlobalSameModule,
	BaseViewFromDerived,
	DerivedViewFromGlobal,
};

enum class EPropVisibilityOperation : uint8
{
	Read,
	Write,
};

enum class EPropVisibilityVisibility : uint8
{
	Default,
	Private,
	Protected,
};

struct FPropVisibilityParams
{
	EPropVisibilityAccessPath AccessPath = EPropVisibilityAccessPath::OwnerMethod;
	EPropVisibilityOperation Operation = EPropVisibilityOperation::Read;
	EPropVisibilityVisibility Visibility = EPropVisibilityVisibility::Default;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FPropVisibilityGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildPropertyVisibilitySource(const FPropVisibilityParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
