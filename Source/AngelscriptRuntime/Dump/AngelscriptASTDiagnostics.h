#pragma once

#include "CoreMinimal.h"

class asIScriptEngine;

// Runtime host adapter for the standard-C++ read-only canonical AST
// diagnostics service. This parser is shared by the console command and
// Editor commandlet so neither host can obtain mutable AST access.
struct ANGELSCRIPTRUNTIME_API FAngelscriptASTDiagnostics
{
	static const TCHAR* GetCommandName();

	static int32 Run(
		asIScriptEngine* ScriptEngine,
		const TArray<FString>& Args,
		FString& OutOutput,
		FString& OutError);
};
