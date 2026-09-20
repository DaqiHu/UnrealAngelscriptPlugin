#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EOpAssignmentCategory : uint8
{
	Local,
	Field,
	Property,
	Alias,
};

enum class EOpAssignmentOperation : uint8
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

enum class EOpAssignmentType : uint8
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

struct FOpAssignmentParams
{
	EOpAssignmentCategory Category = EOpAssignmentCategory::Local;
	EOpAssignmentOperation Operation = EOpAssignmentOperation::Assign;
	EOpAssignmentType Type = EOpAssignmentType::Int;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FOpAssignmentGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildLegalSource(const FOpAssignmentParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
