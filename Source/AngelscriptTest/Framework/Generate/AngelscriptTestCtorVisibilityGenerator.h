#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class ECtorVisibilitySelection : uint8
{
	Exact,
	Promotion,
	ExplicitCast,
	ImplicitRejected,
	Ambiguous,
	Missing,
};

enum class ECtorVisibilitySite : uint8
{
	Owner,
	Derived,
	Unrelated,
	Global,
};

enum class ECtorVisibilityVisibility : uint8
{
	Default,
	Protected,
	Private,
};

struct FCtorVisibilityParams
{
	ECtorVisibilitySelection Selection = ECtorVisibilitySelection::Exact;
	ECtorVisibilitySite Site = ECtorVisibilitySite::Owner;
	ECtorVisibilityVisibility Visibility = ECtorVisibilityVisibility::Default;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FCtorVisibilityGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildConstructorVisibilitySource(const FCtorVisibilityParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
