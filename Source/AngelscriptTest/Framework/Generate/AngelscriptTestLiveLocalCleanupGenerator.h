#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class ELiveLocalScope : uint8
{
	Loop,
	BranchLoop,
	NestedLoop,
	SwitchLoop,
};

enum class ELiveLocalExit : uint8
{
	Normal,
	Break,
	Continue,
	Return,
	Exception,
};

enum class ELiveLocalDepth : uint8
{
	One,
	Two,
	Three,
};

enum class ELiveLocalCount : uint8
{
	One,
	Two,
};

enum class ELiveLocalLineEnding : uint8
{
	Lf,
	Crlf,
};

struct FLiveLocalCleanupParams
{
	ELiveLocalScope Scope = ELiveLocalScope::Loop;
	ELiveLocalExit Exit = ELiveLocalExit::Normal;
	ELiveLocalDepth Depth = ELiveLocalDepth::One;
	ELiveLocalCount LocalCount = ELiveLocalCount::One;
	ELiveLocalLineEnding LineEnding = ELiveLocalLineEnding::Lf;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FLiveLocalCleanupGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildLiveLocalSource(const FLiveLocalCleanupParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
