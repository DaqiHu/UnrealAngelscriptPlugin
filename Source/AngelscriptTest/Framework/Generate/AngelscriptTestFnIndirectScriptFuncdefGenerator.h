#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EFnIndirectScriptFuncdefScenario : uint8
{
	DeclarationMetadata,
	CompatibleDirect,
	CompatibleNested,
	NullOrUnbound,
	IncompatibleSignature,
	RebuildOrRebind,
};

struct FFnIndirectScriptFuncdefParams
{
	EFnIndirectScriptFuncdefScenario Scenario = EFnIndirectScriptFuncdefScenario::DeclarationMetadata;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FFnIndirectScriptFuncdefGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildScriptFuncdefSource(const FFnIndirectScriptFuncdefParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
