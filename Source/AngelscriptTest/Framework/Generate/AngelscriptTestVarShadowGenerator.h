#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EVarShadowRelation : uint8
{
	None,
	InnerOuter,
	ParameterGlobal,
	ParameterMember,
	Sibling,
};

enum class EVarShadowPath : uint8
{
	FunctionBeforeDeclaration,
	FunctionAfterDeclaration,
	FunctionSiblingCall,
	NestedBlockBefore,
	NestedBlockInside,
	NestedBlockAfter,
	IfBranchBefore,
	IfBranchInside,
	IfBranchAfter,
	SwitchCaseBefore,
	SwitchCaseInside,
	SwitchCaseAfter,
	ForInitializer,
	ForBody,
	ForAfter,
	WhileBefore,
	WhileBody,
	WhileAfter,
	NestedCallCallerBefore,
	NestedCallCallee,
	NestedCallCallerAfter,
	AfterOwner,
};

struct FVarShadowParams
{
	EVarShadowRelation Relation = EVarShadowRelation::None;
	EVarShadowPath Path = EVarShadowPath::FunctionAfterDeclaration;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FVarShadowGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildScopeSource(const FVarShadowParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
	FString BuildRejectSource(FStringView CaseId) const;
	TArray<FString> ListRejectCaseIds() const;
};
}
