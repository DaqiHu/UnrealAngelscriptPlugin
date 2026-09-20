#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EDeclCollisionOrder : uint8
{
	LeftThenRight,
	RightThenLeft,
};

enum class EDeclCollisionRelation : uint8
{
	Same,
	Different,
	Nested,
};

enum class EDeclCollisionPair : uint8
{
	FunctionOverload,
	FunctionDuplicate,
	FunctionType,
	FunctionEnum,
	TypeDuplicate,
	TypeEnum,
	EnumDuplicate,
	MethodOverload,
	MethodDuplicate,
	MethodField,
	MethodProperty,
	FieldDuplicate,
	FieldProperty,
	PropertyGetSet,
	PropertyDuplicateGet,
	PropertyDuplicateSet,
};

struct FDeclCollisionParams
{
	EDeclCollisionOrder InsertionOrder = EDeclCollisionOrder::LeftThenRight;
	EDeclCollisionRelation NamespaceRelation = EDeclCollisionRelation::Same;
	EDeclCollisionPair Pair = EDeclCollisionPair::FunctionOverload;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FDeclCollisionGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildCollisionSource(const FDeclCollisionParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
