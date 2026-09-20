#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EExprChainContext : uint8
{
	Initializer,
	Argument,
	Return,
	Condition,
	Assignment,
};

enum class EExprChainDepth : uint8
{
	Two,
	Three,
	Eight,
	DeepBoundary,
};

enum class EExprChainShape : uint8
{
	CallMember,
	MemberCall,
	IndexMember,
	MemberIndex,
	CastMember,
	CallIndexCast,
	MemberCallIndex,
	CastCallMemberIndex,
};

enum class EExprChainState : uint8
{
	Valid,
	NullReceiver,
	InvalidIntermediate,
	ExceptionIntermediate,
	MissingTerminal,
};

struct FExprChainParams
{
	EExprChainContext Context = EExprChainContext::Initializer;
	EExprChainDepth Depth = EExprChainDepth::Two;
	EExprChainShape Shape = EExprChainShape::CallMember;
	EExprChainState State = EExprChainState::Valid;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FExprChainGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildExpressionChainSource(const FExprChainParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
