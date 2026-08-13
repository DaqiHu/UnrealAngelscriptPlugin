#pragma once

#include "CoreMinimal.h"
#include "StaticJIT/AngelscriptJITProvider.h"

namespace AngelscriptTestJITProbes
{
	ANGELSCRIPTTESTJIT_API const FAngelscriptJITProviderId&
		GetStableProviderId();
	ANGELSCRIPTTESTJIT_API void Reset();
	ANGELSCRIPTTESTJIT_API void Record(const int32 Value);
	ANGELSCRIPTTESTJIT_API int32 GetCallCount();
	ANGELSCRIPTTESTJIT_API int32 GetLastValue();
}
