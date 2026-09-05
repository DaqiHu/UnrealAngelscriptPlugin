#include "Dump/AngelscriptASTDiagnostics.h"

#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAngelscriptASTDiagnostics, Log, All);

namespace
{
	void ExecuteAngelscriptASTDiagnostics(const TArray<FString>& Args)
	{
		(void)Args;
		UE_LOG(LogAngelscriptASTDiagnostics, Error,
			TEXT("Canonical AST diagnostics are isolated from the active compile graph."));
	}

	FAutoConsoleCommand GAngelscriptASTDiagnosticsCommand(
		FAngelscriptASTDiagnostics::GetCommandName(),
		TEXT("Read-only Canonical AST diagnostics (isolated)."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ExecuteAngelscriptASTDiagnostics));
}

const TCHAR* FAngelscriptASTDiagnostics::GetCommandName()
{
	return TEXT("as.AST");
}

int32 FAngelscriptASTDiagnostics::Run(
	asIScriptEngine* ScriptEngine,
	const TArray<FString>& Args,
	FString& OutOutput,
	FString& OutError)
{
	(void)ScriptEngine;
	(void)Args;
	OutOutput.Reset();
	OutError = TEXT("Canonical AST diagnostics are isolated from the active compile graph.");
	return 1;
}
