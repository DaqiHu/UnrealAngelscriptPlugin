#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EInhCastConstness : uint8
{
	Mutable,
	Const,
};

enum class EInhCastRelation : uint8
{
	Exact,
	Upcast,
	DowncastSuccess,
	DowncastFailure,
	Sibling,
	Null,
};

enum class EInhCastUse : uint8
{
	Assign,
	Argument,
	Return,
	IdentityCompare,
	MemberCall,
};

struct FInhCastParams
{
	EInhCastConstness Constness = EInhCastConstness::Mutable;
	EInhCastRelation Relation = EInhCastRelation::Exact;
	EInhCastUse Use = EInhCastUse::Assign;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FInhCastGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildInheritanceCastSource(const FInhCastParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
