#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class ERefLifetimeObservation : uint8
{
	Identity,
	Refcount,
	WeakFlag,
	Destruction,
	GcStats,
};

enum class ERefLifetimeOwner : uint8
{
	OwnerLive,
	ScopeExit,
	ReturnedAlias,
	ModuleRetained,
	ModuleDiscarded,
	ContextRetained,
	ContextReleased,
	GcCycle,
};

enum class ERefLifetimeReference : uint8
{
	None,
	OneAlias,
	MultipleAliases,
	Cycle,
	WeakFlag,
};

struct FRefLifetimeParams
{
	ERefLifetimeObservation Observation = ERefLifetimeObservation::Identity;
	ERefLifetimeOwner Owner = ERefLifetimeOwner::OwnerLive;
	ERefLifetimeReference Reference = ERefLifetimeReference::None;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FRefLifetimeGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildReferenceLifetimeSource(const FRefLifetimeParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
