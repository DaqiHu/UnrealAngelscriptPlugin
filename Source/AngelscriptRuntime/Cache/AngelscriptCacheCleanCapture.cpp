#include "Cache/AngelscriptCacheCleanCapture.h"

namespace
{
	FAngelscriptCacheCleanCaptureResult IsolatedCapture()
	{
		FAngelscriptCacheCleanCaptureResult Result;
		Result.Error = EAngelscriptCacheCleanCaptureError::NotCacheable;
		Result.Detail = TEXT("Cache Canonical AST capture is isolated");
		return Result;
	}
}

void FAngelscriptCacheCrossModuleFunctionAuthority::Reset()
{
	Entries.Reset();
}

bool FAngelscriptCacheCrossModuleFunctionAuthority::IsEmpty() const
{
	return Entries.IsEmpty();
}

TOptional<FAngelscriptCacheCurrentSymbol>
FAngelscriptCacheCrossModuleFunctionAuthority::Resolve(
	EAngelscriptArtifactReferenceKind ReferenceKind,
	const FAngelscriptHash256& StableKey) const
{
	(void)ReferenceKind;
	(void)StableKey;
	return {};
}

bool FAngelscriptCacheCrossModuleFunctionAuthority::TryResolveExternalScriptFunction(
	const asCScriptFunction& Function,
	FAngelscriptArtifactReference& OutReference) const
{
	(void)Function;
	OutReference = {};
	return false;
}

bool FAngelscriptCacheCrossModuleFunctionAuthority::TryResolveFunctionDependency(
	const asCScriptFunction& Function,
	FAngelscriptArtifactReference& OutReference,
	FAngelscriptHash256& OutExecutionContent) const
{
	(void)Function;
	OutReference = {};
	OutExecutionContent = {};
	return false;
}

FAngelscriptCacheCleanCaptureResult
BuildAngelscriptCacheCrossModuleFunctionAuthority(
	TConstArrayView<TSharedRef<FAngelscriptModuleDesc>> Modules,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	FAngelscriptCacheCrossModuleFunctionAuthority& OutAuthority)
{
	(void)Modules;
	(void)Options;
	OutAuthority.Reset();
	return IsolatedCapture();
}

FAngelscriptCacheCleanCaptureResult
CaptureAngelscriptCleanCompiledModule(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	FAngelscriptCacheCleanModuleArtifacts& OutArtifacts)
{
	(void)Module;
	(void)Options;
	OutArtifacts = {};
	return IsolatedCapture();
}

FAngelscriptCacheCleanCaptureResult
CaptureAngelscriptCurrentModuleFunctionArtifactsForDiagnostics(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	FAngelscriptCacheCleanModuleArtifacts& OutArtifacts)
{
	(void)Module;
	(void)Options;
	OutArtifacts = {};
	return IsolatedCapture();
}

FAngelscriptCacheCleanCaptureResult
CaptureAngelscriptCurrentModuleFunctionFacts(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	const FAngelscriptCachedSourceIndex& AuthoritativeSourceIndex,
	const FAngelscriptCacheCrossModuleFunctionAuthority* CrossModuleFunctions,
	FAngelscriptCacheCleanModuleArtifacts& OutArtifacts,
	const IAngelscriptCacheCurrentSymbolResolver* EnvironmentSymbols)
{
	(void)Module;
	(void)Options;
	(void)AuthoritativeSourceIndex;
	(void)CrossModuleFunctions;
	(void)EnvironmentSymbols;
	OutArtifacts = {};
	return IsolatedCapture();
}

FAngelscriptCacheCleanCaptureResult
CaptureAngelscriptCleanCompiledModule(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	const FAngelscriptCachedSourceIndex& AuthoritativeSourceIndex,
	FAngelscriptCacheCleanModuleArtifacts& OutArtifacts)
{
	(void)AuthoritativeSourceIndex;
	return CaptureAngelscriptCleanCompiledModule(Module, Options, OutArtifacts);
}

FAngelscriptCacheCleanCaptureResult
CaptureAngelscriptCleanCompiledModule(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	const FAngelscriptCachedSourceIndex& AuthoritativeSourceIndex,
	const IAngelscriptCacheRestoredFunctionDependencySource* RestoredDependencies,
	FAngelscriptCacheCleanModuleArtifacts& OutArtifacts)
{
	(void)AuthoritativeSourceIndex;
	(void)RestoredDependencies;
	return CaptureAngelscriptCleanCompiledModule(Module, Options, OutArtifacts);
}

FAngelscriptCacheCleanCaptureResult
CaptureAngelscriptCleanCompiledModule(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	const FAngelscriptCachedSourceIndex& AuthoritativeSourceIndex,
	const IAngelscriptCacheRestoredFunctionDependencySource* RestoredDependencies,
	const FAngelscriptCacheCrossModuleFunctionAuthority* CrossModuleFunctions,
	FAngelscriptCacheCleanModuleArtifacts& OutArtifacts,
	const IAngelscriptCacheCurrentSymbolResolver* EnvironmentSymbols)
{
	(void)AuthoritativeSourceIndex;
	(void)RestoredDependencies;
	(void)CrossModuleFunctions;
	(void)EnvironmentSymbols;
	return CaptureAngelscriptCleanCompiledModule(Module, Options, OutArtifacts);
}

FAngelscriptCacheCleanCaptureResult
ValidateAndPromoteAngelscriptCleanCompiledModuleArtifacts(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	FAngelscriptCacheCleanModuleArtifacts CandidateArtifacts,
	FAngelscriptCacheCleanModuleArtifacts& OutArtifacts,
	const FAngelscriptCacheCrossModuleFunctionAuthority* CrossModuleFunctions,
	const IAngelscriptCacheCurrentSymbolResolver* EnvironmentSymbols)
{
	(void)Module;
	(void)Options;
	(void)CandidateArtifacts;
	(void)CrossModuleFunctions;
	(void)EnvironmentSymbols;
	OutArtifacts = {};
	return IsolatedCapture();
}

FAngelscriptCacheCleanCaptureResult
OpenAngelscriptValidatedModuleGraphFromCleanArtifacts(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	FAngelscriptValidatedModuleGraph& OutGraph,
	const FAngelscriptCacheCrossModuleFunctionAuthority* CrossModuleFunctions)
{
	(void)Module;
	(void)Options;
	(void)Artifacts;
	(void)Limits;
	(void)Budget;
	(void)CrossModuleFunctions;
	OutGraph = {};
	return IsolatedCapture();
}

FAngelscriptCacheCleanCaptureResult
PrepareAngelscriptCacheColdGeneration(
	const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	const FAngelscriptCachePackPolicy& PackPolicy,
	IAngelscriptCacheStorageCodec& Codec,
	FAngelscriptCachePreparedColdGeneration& OutGeneration)
{
	(void)Artifacts;
	(void)Options;
	(void)PackPolicy;
	(void)Codec;
	OutGeneration = {};
	return IsolatedCapture();
}

FAngelscriptCacheCleanCaptureResult
PrepareAngelscriptCacheColdGeneration(
	TConstArrayView<FAngelscriptCacheCleanModuleArtifacts> Modules,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	const FAngelscriptCachePackPolicy& PackPolicy,
	IAngelscriptCacheStorageCodec& Codec,
	FAngelscriptCachePreparedColdGeneration& OutGeneration)
{
	(void)Modules;
	(void)Options;
	(void)PackPolicy;
	(void)Codec;
	OutGeneration = {};
	return IsolatedCapture();
}
