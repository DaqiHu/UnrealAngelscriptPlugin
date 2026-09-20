#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EInhAccessKind : uint8
{
	Default,
	Protected,
	Private,
};

enum class EInhAccessMember : uint8
{
	Field,
	Method,
	GetterSetterMethod,
	Constructor,
};

enum class EInhAccessSite : uint8
{
	Owner,
	DirectDerived,
	DeepDerived,
	Unrelated,
	Global,
};

struct FInhAccessParams
{
	EInhAccessKind Access = EInhAccessKind::Default;
	EInhAccessMember Member = EInhAccessMember::Field;
	EInhAccessSite Site = EInhAccessSite::Owner;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FInhAccessGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildInheritanceAccessSource(const FInhAccessParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
