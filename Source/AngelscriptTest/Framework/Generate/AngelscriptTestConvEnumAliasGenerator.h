#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EConvEnumAliasSource : uint8
{
	Enum,
	AliasInt8,
	AliasInt,
	AliasInt64,
	AliasUint,
	AliasUint64,
};

enum class EConvEnumAliasTarget : uint8
{
	Enum,
	Int8,
	Int,
	Int64,
	Uint,
	Uint64,
	Float64,
};

enum class EConvEnumAliasForm : uint8
{
	Assignment,
	Initializer,
	Argument,
	Return,
	Promotion,
	ExplicitCast,
};

enum class EConvEnumAliasValue : uint8
{
	Zero,
	One,
	Negative,
	NearMin,
	NearMax,
};

struct FConvEnumAliasParams
{
	EConvEnumAliasSource Source = EConvEnumAliasSource::Enum;
	EConvEnumAliasTarget Target = EConvEnumAliasTarget::Enum;
	EConvEnumAliasForm Form = EConvEnumAliasForm::Assignment;
	EConvEnumAliasValue Value = EConvEnumAliasValue::Zero;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FConvEnumAliasGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildEnumAliasConversionSource(const FConvEnumAliasParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
