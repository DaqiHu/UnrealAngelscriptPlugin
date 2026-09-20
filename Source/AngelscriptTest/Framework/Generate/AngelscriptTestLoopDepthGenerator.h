#pragma once

#include "Framework/Generate/AngelscriptTestCodeGenerator.h"
#include "Framework/Generate/AngelscriptTestGeneratedCase.h"

namespace AngelscriptTest::Generate
{
enum class ELoopDepthLoop : uint8
{
	While,
	DoWhile,
	For,
};

enum class ELoopDepthCount : uint8
{
	Zero,
	One,
	Two,
	Many,
};

enum class ELoopDepthTransfer : uint8
{
	None,
	Break,
	Continue,
	Return,
};

struct FLoopDepthParams
{
	ELoopDepthLoop Loop = ELoopDepthLoop::While;
	ELoopDepthCount Count = ELoopDepthCount::Zero;
	ELoopDepthTransfer Transfer = ELoopDepthTransfer::None;
	FString FunctionName;
};

class ANGELSCRIPTTEST_API FLoopDepthGenerator final : public FCodeGenerator
{
public:
	FStringView GetProductId() const;
	TArray<FGeneratedCaseInfo> ListCases() const;
	FString BuildCaseSource(FStringView CaseId) const;
	FString BuildDumpSource() const;
	FString BuildAllSource(int32& OutCaseCount) const;
	FString BuildLoopSource(const FLoopDepthParams& Params) const;
	int32 GetExpected(FStringView CaseId) const;
};
}
