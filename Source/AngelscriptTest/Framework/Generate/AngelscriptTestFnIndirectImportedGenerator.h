#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EFnIndirectImportedScenario : uint8
{
	DeclarationMetadata,
	CompatibleDirect,
	CompatibleNested,
	NullOrUnbound,
	IncompatibleSignature,
	RebuildOrRebind,
};

struct FFnIndirectImportedParams
{
	EFnIndirectImportedScenario Scenario = EFnIndirectImportedScenario::DeclarationMetadata;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FFnIndirectImportedGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildImportProviderSource(const FFnIndirectImportedParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
