#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpOverloadDuplicateDeclarationKind : uint8
{
	Duplicate,
};

enum class EOpOverloadDuplicateDeclarationFamily : uint8
{
	Unary,
	Binary,
	Comparison,
	Index,
	Call,
	Conversion,
	Assignment,
};

struct FOpOverloadDuplicateDeclarationParams
{
	EOpOverloadDuplicateDeclarationKind Kind = EOpOverloadDuplicateDeclarationKind::Duplicate;
	EOpOverloadDuplicateDeclarationFamily Family = EOpOverloadDuplicateDeclarationFamily::Unary;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpOverloadDuplicateDeclarationGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildDuplicateSource(const FOpOverloadDuplicateDeclarationParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
