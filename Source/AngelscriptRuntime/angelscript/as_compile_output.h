#pragma once

#include "as_config.h"
#include "Core/AngelscriptDescriptors.h"
#include "frontend/as_diagnostics.h"

BEGIN_AS_NAMESPACE

class ANGELSCRIPTRUNTIME_API asCDefinitionCompileOutput
{
public:
	TConstArrayView<TSharedRef<FAngelscriptModuleDesc>> GetModules() const { return MakeArrayView(Modules); }
	void ReplaceModules(TArray<TSharedRef<FAngelscriptModuleDesc>> InModules) { Modules = MoveTemp(InModules); }

private:
	TArray<TSharedRef<FAngelscriptModuleDesc>> Modules;
};

// Diagnostics plus ClassGen descriptors. Does not own TypeInfo or bytecode.
class ANGELSCRIPTRUNTIME_API asCCompileOutput
{
public:
	TConstArrayView<asSDiagnosticRecord> GetDiagnostics() const { return MakeArrayView(Diagnostics); }
	const asCDefinitionCompileOutput& GetDefinitionOutput() const { return DefinitionOutput; }
	asCDefinitionCompileOutput& GetDefinitionOutput() { return DefinitionOutput; }
	void ReplaceDiagnostics(TArray<asSDiagnosticRecord> InDiagnostics) { Diagnostics = MoveTemp(InDiagnostics); }

private:
	TArray<asSDiagnosticRecord> Diagnostics;
	asCDefinitionCompileOutput DefinitionOutput;
};

END_AS_NAMESPACE
