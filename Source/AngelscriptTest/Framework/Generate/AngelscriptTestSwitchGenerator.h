#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class ESwitchSelector : uint8
{
	Int8,
	Int16,
	Int,
	Int64,
	Uint8,
	Uint16,
	Uint,
	Uint64,
	Enum,
	Typedef,
	Boundary,
	Unsupported,
};

enum class ESwitchCase : uint8
{
	First,
	Middle,
	Last,
	Default,
	NoMatch,
	Fallthrough,
	Grouped,
	Duplicate,
	NonConstant,
};

enum class ESwitchExit : uint8
{
	Break,
	Fallthrough,
	Return,
	Exception,
};

struct FSwitchParams
{
	ESwitchSelector Selector = ESwitchSelector::Int8;
	ESwitchCase Case = ESwitchCase::First;
	ESwitchExit Exit = ESwitchExit::Break;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FSwitchGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildSwitchSource(const FSwitchParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
