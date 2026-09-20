#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpLogicalNotCategory : uint8
{
	MutableLValue,
	ConstLValue,
	Temporary,
	Field,
	Alias,
};

enum class EOpLogicalNotValue : uint8
{
	False,
	True,
};

struct FOpLogicalNotParams
{
	EOpLogicalNotCategory Category = EOpLogicalNotCategory::MutableLValue;
	EOpLogicalNotValue Value = EOpLogicalNotValue::False;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpLogicalNotGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildLogicalNotSource(const FOpLogicalNotParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
