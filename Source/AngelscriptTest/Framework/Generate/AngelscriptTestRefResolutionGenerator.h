#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class ERefResolutionCandidate : uint8
{
	MutableExact,
	ConstPair,
	ValueVsIn,
	BaseVsDerived,
	ReturnCovariance,
	NullPair,
	NumericConversion,
	CompetingConversions,
	MissingCandidate,
	IncompatibleCandidate,
};

enum class ERefResolutionSite : uint8
{
	Direct,
	Helper,
};

enum class ERefResolutionSource : uint8
{
	MutableLValue,
	ConstLValue,
	Temporary,
	Field,
	Parameter,
	BaseView,
	DerivedView,
	Null,
};

struct FRefResolutionParams
{
	ERefResolutionCandidate Candidate = ERefResolutionCandidate::MutableExact;
	ERefResolutionSite Site = ERefResolutionSite::Direct;
	ERefResolutionSource Source = ERefResolutionSource::MutableLValue;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FRefResolutionGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildReferenceResolutionSource(const FRefResolutionParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
