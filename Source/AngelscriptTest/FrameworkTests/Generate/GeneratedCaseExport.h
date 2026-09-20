#pragma once

#include "CoreMinimal.h"

namespace AngelscriptTest
{
class FGeneratedCaseExport
{
public:
	static FString ResolveOutputFile(FStringView GeneratorClass);
	static bool WriteDump(FStringView OutputFile, FStringView Source, FString& OutError);
};
}
