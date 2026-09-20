#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EConvValueObjectAvailability : uint8
{
	ImplicitConstructor,
	ExplicitConstructor,
	ImplicitOperator,
	ExplicitOperator,
	ConstructorAndOperator,
	None,
};

enum class EConvValueObjectTarget : uint8
{
	SameValue,
	OtherValue,
	Int,
	Float64,
	Bool,
};

enum class EConvValueObjectForm : uint8
{
	Assignment,
	Initializer,
	Argument,
	Return,
	ExplicitCast,
	DirectConstructor,
};

enum class EConvValueObjectOutcome : uint8
{
	Direct,
	Selected,
	Ambiguous,
	Rejected,
};

struct FConvValueObjectParams
{
	EConvValueObjectAvailability Availability = EConvValueObjectAvailability::ImplicitConstructor;
	EConvValueObjectTarget Target = EConvValueObjectTarget::SameValue;
	EConvValueObjectForm Form = EConvValueObjectForm::Assignment;
	EConvValueObjectOutcome Outcome = EConvValueObjectOutcome::Direct;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FConvValueObjectGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildValueObjectConversionSource(const FConvValueObjectParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
