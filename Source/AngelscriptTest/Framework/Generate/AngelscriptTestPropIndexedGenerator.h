#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EPropIndexedCandidateSet : uint8
{
	SameType,
	AdjacentNumeric,
	CrossFamily,
	CompetingPrimaryFirst,
	CompetingSecondaryFirst,
	Unrelated,
};

enum class EPropIndexedIndexType : uint8
{
	Int8,
	Int16,
	Int,
	Int64,
	UInt8,
	UInt16,
	UInt,
	UInt64,
	Float32,
	Float64,
	Bool,
	Enum,
	Typedef,
};

enum class EPropIndexedOperation : uint8
{
	Read,
	Write,
	Compound,
};

enum class EPropIndexedReceiver : uint8
{
	Mutable,
	Const,
};

struct FPropIndexedParams
{
	EPropIndexedCandidateSet CandidateSet = EPropIndexedCandidateSet::SameType;
	EPropIndexedIndexType IndexType = EPropIndexedIndexType::Int8;
	EPropIndexedOperation Operation = EPropIndexedOperation::Read;
	EPropIndexedReceiver Receiver = EPropIndexedReceiver::Mutable;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FPropIndexedGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildIndexedPropertySource(const FPropIndexedParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
