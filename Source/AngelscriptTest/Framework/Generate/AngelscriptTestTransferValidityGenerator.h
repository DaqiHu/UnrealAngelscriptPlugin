#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class ETransferValidityPlacement : uint8
{
	Function,
	Branch,
	Switch,
	Loop,
};

enum class ETransferValidityTransfer : uint8
{
	Break,
	Continue,
};

struct FTransferValidityParams
{
	ETransferValidityPlacement Placement = ETransferValidityPlacement::Function;
	ETransferValidityTransfer Transfer = ETransferValidityTransfer::Break;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FTransferValidityGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildValiditySource(const FTransferValidityParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
