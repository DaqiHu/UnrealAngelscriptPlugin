#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpAssignmentTargetRejectionTarget : uint8
{
	ConstInvalid,
	TemporaryInvalid,
};

enum class EOpAssignmentTargetRejectionOperation : uint8
{
	Assign,
	AddAssign,
	SubtractAssign,
	MultiplyAssign,
	DivideAssign,
	ModuloAssign,
	PowerAssign,
	AndAssign,
	OrAssign,
	XorAssign,
	ShiftLeftAssign,
	ShiftRightLogicalAssign,
	ShiftRightArithmeticAssign,
};

enum class EOpAssignmentTargetRejectionType : uint8
{
	Int8,
	Int16,
	Int,
	Int64,
	Uint8,
	Uint16,
	Uint,
	Uint64,
	Float32,
	Float64,
	Bool,
};

struct FOpAssignmentTargetRejectionParams
{
	EOpAssignmentTargetRejectionTarget Target = EOpAssignmentTargetRejectionTarget::ConstInvalid;
	EOpAssignmentTargetRejectionOperation Operation = EOpAssignmentTargetRejectionOperation::Assign;
	EOpAssignmentTargetRejectionType Type = EOpAssignmentTargetRejectionType::Int;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpAssignmentTargetRejectionGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildTargetRejectionSource(const FOpAssignmentTargetRejectionParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
