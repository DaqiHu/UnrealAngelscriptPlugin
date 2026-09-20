#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class EVarLoopExit : uint8
{
	Normal,
	Break,
	Continue,
	Return,
	Exception,
};

enum class EVarLoopIteration : uint8
{
	Zero,
	One,
	Three,
	Eight,
};

enum class EVarLoopPlacement : uint8
{
	ForInitializer,
	ForBody,
	WhileBody,
	DoBody,
	NestedBody,
};

enum class EVarLoopType : uint8
{
	ScriptValue,
	NativeValue,
};

struct FVarLoopDeclLifetimeParams
{
	EVarLoopExit Exit = EVarLoopExit::Normal;
	EVarLoopIteration Iteration = EVarLoopIteration::Zero;
	EVarLoopPlacement Placement = EVarLoopPlacement::ForInitializer;
	EVarLoopType Type = EVarLoopType::ScriptValue;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FVarLoopDeclLifetimeGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildVariableLoopSource(const FVarLoopDeclLifetimeParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
