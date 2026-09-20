#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EExprPrimaryContext : uint8
{
	Initializer,
	AssignmentRhs,
	AssignmentLhs,
	Argument,
	Return,
	Condition,
	LoopClause,
	SwitchSelector,
	Index,
	PropertyAccessor,
};

enum class EExprPrimaryKind : uint8
{
	IntLiteral,
	BoolLiteral,
	EnumLiteral,
	NullLiteral,
	LocalIdentifier,
	ConstIdentifier,
	ReferenceIdentifier,
	ScopedConstant,
	ScopedEnum,
	ParenthesizedScalar,
	ParenthesizedLValue,
	GlobalFunctionCall,
	MethodCall,
	ValueConstructor,
	ReferenceConstructor,
	MemberField,
	VirtualProperty,
	IndexedProperty,
	ExplicitNumericCast,
	ObjectCast,
	BaseCast,
	DerivedCast,
};

struct FExprPrimaryContextParams
{
	EExprPrimaryContext Context = EExprPrimaryContext::Initializer;
	EExprPrimaryKind Primary = EExprPrimaryKind::IntLiteral;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FExprPrimaryContextGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildPrimaryExpressionSource(const FExprPrimaryContextParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
