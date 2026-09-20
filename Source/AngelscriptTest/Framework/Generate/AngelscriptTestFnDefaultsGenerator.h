#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EFnDefaultsOmission : uint8
{
	None,
	One,
	Many,
};

enum class EFnDefaultsPattern : uint8
{
	FinalOne,
	FinalMany,
	AllOptional,
	ExplicitOverride,
	MixedOmittedProvided,
	NonTrailingInvalid,
	TypeInvalid,
	EarlierParameterReferenceInvalid,
};

enum class EFnDefaultsTarget : uint8
{
	Global,
	NamespaceGlobal,
	InstanceMethod,
};

struct FFnDefaultsParams
{
	EFnDefaultsOmission Omission = EFnDefaultsOmission::None;
	EFnDefaultsPattern Pattern = EFnDefaultsPattern::FinalOne;
	EFnDefaultsTarget Target = EFnDefaultsTarget::Global;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FFnDefaultsGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildDefaultArgumentSource(const FFnDefaultsParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
