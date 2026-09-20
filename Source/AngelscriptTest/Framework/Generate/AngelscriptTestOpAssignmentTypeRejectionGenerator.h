#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpAssignmentTypeRejectionCategory : uint8
{
	Local,
	Field,
	Property,
	Alias,
};

enum class EOpAssignmentTypeRejectionOperation : uint8
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

enum class EOpAssignmentTypeRejectionType : uint8
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

struct FOpAssignmentTypeRejectionParams
{
	EOpAssignmentTypeRejectionCategory Category = EOpAssignmentTypeRejectionCategory::Local;
	EOpAssignmentTypeRejectionOperation Operation = EOpAssignmentTypeRejectionOperation::AddAssign;
	EOpAssignmentTypeRejectionType Type = EOpAssignmentTypeRejectionType::Bool;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpAssignmentTypeRejectionGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildTypeRejectionSource(const FOpAssignmentTypeRejectionParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
