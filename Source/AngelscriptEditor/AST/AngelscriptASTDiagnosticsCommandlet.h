#pragma once

#include "Commandlets/Commandlet.h"

#include "AngelscriptASTDiagnosticsCommandlet.generated.h"

// Commandlet host for the Runtime-owned read-only canonical AST diagnostics
// parser. It intentionally has no direct AST/context mutation API.
UCLASS()
class ANGELSCRIPTEDITOR_API UAngelscriptASTDiagnosticsCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
