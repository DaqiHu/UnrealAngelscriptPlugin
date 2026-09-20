#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EPropAccessorScenario : uint8
{
	GetterReadMutable,
	GetterReadConst,
	GetterNonconstReadMutable,
	GetterNonconstReadConstRejected,
	GetterWriteMissing,
	GetterCompoundMissingSet,
	SetterWriteMutable,
	SetterWriteConstRejected,
	SetterReadMissing,
	SetterCompoundMissingGet,
	BothReadMutable,
	BothWriteMutable,
	BothCompoundMutable,
	BothCompoundConstRejected,
	RecursiveGetter,
	RecursiveSetter,
	ThrowingGetter,
	ThrowingSetter,
};

enum class EPropAccessorSourceShape : uint8
{
	SingleLine,
	Multiline,
	Parenthesized,
	HelperCall,
};

struct FPropAccessorParams
{
	EPropAccessorScenario Scenario = EPropAccessorScenario::GetterReadMutable;
	EPropAccessorSourceShape SourceShape = EPropAccessorSourceShape::SingleLine;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FPropAccessorGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildRegisteredPropertySource(const FPropAccessorParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
