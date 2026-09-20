#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EConvFloat64ToFloat32RangeForm : uint8
{
	Assignment,
	Argument,
	Return,
	ExplicitCast,
};

enum class EConvFloat64ToFloat32RangeBoundaryDirection : uint8
{
	AboveTargetMax,
	BelowTargetMin,
};

struct FConvFloat64ToFloat32RangeParams
{
	EConvFloat64ToFloat32RangeForm Form = EConvFloat64ToFloat32RangeForm::Assignment;
	EConvFloat64ToFloat32RangeBoundaryDirection BoundaryDirection =
		EConvFloat64ToFloat32RangeBoundaryDirection::AboveTargetMax;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FConvFloat64ToFloat32RangeGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildNumericBoundaryConversionSource(const FConvFloat64ToFloat32RangeParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
