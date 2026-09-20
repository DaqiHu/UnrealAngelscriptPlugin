#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EConvFailureKind : uint8
{
	ImplicitNarrowing,
	NumericToEnum,
	UnrelatedReference,
	BadDowncast,
	NullValueTarget,
	AmbiguousConstructor,
	AmbiguousOperator,
	ExplicitOnlyImplicitUse,
	ConversionException,
	ConstructorException,
	AbiMismatch,
	ConditionalNoCommonType,
};

enum class EConvFailureRecovery : uint8
{
	FreshModule,
	SameModuleOrContext,
};

struct FConvFailureParams
{
	EConvFailureKind Failure = EConvFailureKind::ImplicitNarrowing;
	EConvFailureRecovery Recovery = EConvFailureRecovery::FreshModule;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FConvFailureGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildFailureSource(const FConvFailureParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
