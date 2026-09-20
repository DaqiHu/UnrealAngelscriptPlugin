#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EVarLifetimeExit : uint8
{
	BlockEnd,
	Return,
	Break,
	Continue,
	Exception,
};

enum class EVarLifetimeNesting : uint8
{
	One,
	Sequential,
	NestedScopes,
	Loop,
	NestedCall,
};

enum class EVarLifetimeOwner : uint8
{
	LocalValue,
	NestedValue,
	Field,
	Reference,
};

struct FVarLifetimeParams
{
	EVarLifetimeExit Exit = EVarLifetimeExit::BlockEnd;
	EVarLifetimeNesting Nesting = EVarLifetimeNesting::One;
	EVarLifetimeOwner Owner = EVarLifetimeOwner::LocalValue;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FVarLifetimeGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildVariableLifetimeSource(const FVarLifetimeParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
