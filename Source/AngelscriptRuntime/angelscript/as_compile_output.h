#pragma once

#include "as_config.h"
#include "Core/AngelscriptDescriptors.h"
#include "frontend/Basic/as_diagnostics.h"

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
	TSharedRef<const asCDiagnosticResult, ESPMode::ThreadSafe> GetDiagnosticResult() const
	{
		return DiagnosticResult.IsValid()
			? DiagnosticResult.ToSharedRef()
			: MakeShared<asCDiagnosticResult, ESPMode::ThreadSafe>();
	}
	const asCDefinitionCompileOutput& GetDefinitionOutput() const { return DefinitionOutput; }
	asCDefinitionCompileOutput& GetDefinitionOutput() { return DefinitionOutput; }
	void ReplaceDiagnosticResult(TSharedRef<const asCDiagnosticResult, ESPMode::ThreadSafe> Result)
	{
		DiagnosticResult = Result;
		Diagnostics.Reset();
		for (const asSDiagnosticGroup& Group : Result->GetVisibleGroups())
		{
			Diagnostics.Add(Group.Primary);
		}
	}
	void ReplaceDiagnostics(TArray<asSDiagnosticRecord> InDiagnostics) { Diagnostics = MoveTemp(InDiagnostics); }

private:
	TSharedPtr<const asCDiagnosticResult, ESPMode::ThreadSafe> DiagnosticResult;
	TArray<asSDiagnosticRecord> Diagnostics;
	asCDefinitionCompileOutput DefinitionOutput;
};

END_AS_NAMESPACE
