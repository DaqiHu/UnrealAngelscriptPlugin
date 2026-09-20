#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EDeclFailureRecoveryForm : uint8
{
	UnbalancedClass,
	BadParameterList,
	UnclosedFunctionBody,
	MissingTypeName,
	UnexpectedHandle,
	UnknownBase,
};

enum class EDeclFailureRecoveryPlacement : uint8
{
	EntryBody,
	AfterValidFunction,
	InsideNamespace,
};

enum class EDeclFailureRecoveryLineEnding : uint8
{
	Lf,
	Crlf,
};

struct FDeclFailureRecoveryParams
{
	EDeclFailureRecoveryForm Form = EDeclFailureRecoveryForm::UnbalancedClass;
	EDeclFailureRecoveryPlacement Placement = EDeclFailureRecoveryPlacement::EntryBody;
	EDeclFailureRecoveryLineEnding LineEnding = EDeclFailureRecoveryLineEnding::Lf;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FDeclFailureRecoveryGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildInvalidSource(const FDeclFailureRecoveryParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
