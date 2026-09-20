#pragma once

#include "Containers/Map.h"
#include "Containers/UnrealString.h"

namespace AngelscriptTest
{
class FAngelscriptTestSource;
class FAngelscriptTestSourceParseResult;

struct ANGELSCRIPTTEST_API FAngelscriptTestSourceRange
{
	int32 Begin = 0;
	int32 End = 0;
};

class ANGELSCRIPTTEST_API FAngelscriptTestAnnotations
{
public:
	static FAngelscriptTestSourceParseResult Parse(FAngelscriptTestSource Source);

	[[nodiscard]] int32 Num() const;
	const int32* GetPoint(FStringView Name) const &;
	const int32* GetPoint(FStringView Name) const && = delete;
	const int32* GetBreakpoint(FStringView Name) const &;
	const int32* GetBreakpoint(FStringView Name) const && = delete;
	const FAngelscriptTestSourceRange* GetRange(FStringView Name) const &;
	const FAngelscriptTestSourceRange* GetRange(FStringView Name) const && = delete;

private:
	TMap<FString, int32> Points;
	TMap<FString, int32> Breakpoints;
	TMap<FString, FAngelscriptTestSourceRange> Ranges;

	friend class FAngelscriptTestSource;
	friend class FAngelscriptTestCodeBuilder;
};
}
