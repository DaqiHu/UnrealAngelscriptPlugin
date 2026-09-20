#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"

namespace AngelscriptTest
{
class ANGELSCRIPTTEST_API FAngelscriptTestSourceOriginMap
{
public:
	FStringView GetHostFile() const &;
	FStringView GetHostFile() const && = delete;
	[[nodiscard]] int32 GetHostLine() const;
	[[nodiscard]] int32 MapToOriginalByteOffset(int32 SourceByteOffset) const;

private:
	friend class FAngelscriptTestSource;
	friend class FAngelscriptTestAnnotations;
	friend class FAngelscriptTestSourceParser;
	friend class FAngelscriptTestCodeBuilder;

	FString HostFile;
	int32 HostLine = 0;
	TArray<int32> OriginalByteOffsets;
};
}
