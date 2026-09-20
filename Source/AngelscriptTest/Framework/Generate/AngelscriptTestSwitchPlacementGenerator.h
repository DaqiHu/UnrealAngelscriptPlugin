#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class ESwitchPlacementScope : uint8
{
	Function,
	Branch,
	Loop,
	AfterSwitch,
};

enum class ESwitchPlacementKind : uint8
{
	CaseOutside,
	DefaultOutside,
	DuplicateDefault,
	CaseAfterDefault,
};

struct FSwitchPlacementParams
{
	ESwitchPlacementScope Scope = ESwitchPlacementScope::Function;
	ESwitchPlacementKind Kind = ESwitchPlacementKind::CaseOutside;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FSwitchPlacementGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildSwitchPlacementSource(const FSwitchPlacementParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
