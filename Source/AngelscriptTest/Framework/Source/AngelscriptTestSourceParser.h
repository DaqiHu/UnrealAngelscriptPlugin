#pragma once

#include "Framework/Source/AngelscriptTestCodeBuildResult.h"

#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"

namespace AngelscriptTest
{
class ANGELSCRIPTTEST_API FAngelscriptTestSourceParser
{
public:
	static FAngelscriptTestCodeBuildResult Parse(
		FStringView FileTag,
		TConstArrayView<uint8> Bytes);
};
}
