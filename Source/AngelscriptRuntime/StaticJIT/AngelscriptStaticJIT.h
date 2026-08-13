#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/BytecodeJIT/AngelscriptBytecodeJIT.h"

#if AS_WITH_STATIC_JIT_DIAGNOSTICS && AS_CAN_GENERATE_JIT
ANGELSCRIPTRUNTIME_API bool GenerateStaticJITSourceTextForDiagnostics(
	class asIScriptModule* Module,
	FString& OutSourceText,
	bool bEmitDebugMetadata,
	FString* OutError = nullptr);
#endif
