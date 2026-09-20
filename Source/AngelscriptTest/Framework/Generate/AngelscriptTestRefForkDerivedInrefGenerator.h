#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class ERefForkDerivedInrefSource : uint8
{
	Derived,
};

enum class ERefForkDerivedInrefTarget : uint8
{
	BaseInref,
};

struct FRefForkDerivedInrefParams
{
	ERefForkDerivedInrefSource Source = ERefForkDerivedInrefSource::Derived;
	ERefForkDerivedInrefTarget Target = ERefForkDerivedInrefTarget::BaseInref;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FRefForkDerivedInrefGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildRefForkDerivedInrefSource(const FRefForkDerivedInrefParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
