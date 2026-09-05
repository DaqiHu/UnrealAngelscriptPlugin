#include "Cache/AngelscriptCacheRestore.h"

#include "Core/AngelscriptEngine.h"

namespace
{
	FAngelscriptCacheRestoreResult IsolatedRestore()
	{
		FAngelscriptCacheRestoreResult Result;
		Result.Error = EAngelscriptCacheRestoreError::CanonicalAstRestoreFailed;
		Result.Detail = TEXT("Cache Canonical AST restore is isolated");
		return Result;
	}
}

FAngelscriptCacheRestoreResult RestoreAngelscriptCacheModules(
	FAngelscriptEngine& TargetEngine,
	const FAngelscriptValidatedGeneration& Generation,
	TConstArrayView<FAngelscriptStableModuleKey> ModuleKeys,
	const FAngelscriptCacheReadLimits& Limits,
	IAngelscriptCacheRestoreFaultInjector* FaultInjector)
{
	(void)TargetEngine;
	(void)Generation;
	(void)ModuleKeys;
	(void)Limits;
	(void)FaultInjector;
	return IsolatedRestore();
}

FAngelscriptCacheRestoreResult RestoreAngelscriptCacheModules(
	FAngelscriptEngine& TargetEngine,
	const FAngelscriptValidatedGeneration& Generation,
	TConstArrayView<FAngelscriptStableModuleKey> ModuleKeys,
	TConstArrayView<FAngelscriptCacheCurrentSourceProjection> CurrentSources,
	const FAngelscriptCacheReadLimits& Limits,
	IAngelscriptCacheRestoreFaultInjector* FaultInjector)
{
	(void)CurrentSources;
	return RestoreAngelscriptCacheModules(
		TargetEngine, Generation, ModuleKeys, Limits, FaultInjector);
}

FAngelscriptCacheRestoreResult RestoreAngelscriptCacheModule(
	FAngelscriptEngine& TargetEngine,
	const FAngelscriptValidatedGeneration& Generation,
	const FAngelscriptStableModuleKey& ModuleKey,
	const FAngelscriptCacheReadLimits& Limits,
	IAngelscriptCacheRestoreFaultInjector* FaultInjector)
{
	return RestoreAngelscriptCacheModules(
		TargetEngine, Generation, MakeArrayView(&ModuleKey, 1), Limits, FaultInjector);
}

FAngelscriptCacheRestoreResult RestoreAngelscriptCacheModule(
	FAngelscriptEngine& TargetEngine,
	const FAngelscriptValidatedGeneration& Generation,
	const FAngelscriptStableModuleKey& ModuleKey,
	const TConstArrayView<FAngelscriptCacheCurrentSourceProjection> CurrentSources,
	const FAngelscriptCacheReadLimits& Limits,
	IAngelscriptCacheRestoreFaultInjector* FaultInjector)
{
	(void)CurrentSources;
	return RestoreAngelscriptCacheModule(
		TargetEngine, Generation, ModuleKey, Limits, FaultInjector);
}
