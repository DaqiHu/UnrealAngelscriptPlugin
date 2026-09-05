#include "StaticJIT/AngelscriptStaticJITGenerationSnapshot.h"

#if AS_CAN_GENERATE_JIT

#include "AngelscriptEngine.h"
#include "Misc/ScopeLock.h"

FAngelscriptStaticJITGenerationASTLease::
	FAngelscriptStaticJITGenerationASTLease(asIASTSnapshot* InSnapshot)
	: Snapshot(InSnapshot)
{
}

FAngelscriptStaticJITGenerationASTLease::
	~FAngelscriptStaticJITGenerationASTLease()
{
	ReleaseBeforeEngineShutdown();
}

const void* FAngelscriptStaticJITGenerationASTLease::GetContext() const
{
	return nullptr;
}

void FAngelscriptStaticJITGenerationASTLease::ReleaseBeforeEngineShutdown()
{
	asIASTSnapshot* SnapshotToRelease = nullptr;
	{
		FScopeLock Lock(&LeaseMutex);
		SnapshotToRelease = Snapshot;
		Snapshot = nullptr;
		SealedContext = nullptr;
	}
	if (SnapshotToRelease != nullptr)
	{
		SnapshotToRelease->Release();
	}
}

bool ValidateAngelscriptStaticJITInstalledNativeCallDispatch(
	const FAngelscriptStaticJITNativeCallInventoryEntry& Entry,
	FString* OutError)
{
	(void)Entry;
	if (OutError)
	{
		*OutError = TEXT("StaticJIT Canonical AST generation is isolated");
	}
	return false;
}

bool BuildAngelscriptStaticJITGenerationSnapshot(
	FAngelscriptEngine& Engine,
	TConstArrayView<TSharedRef<FAngelscriptModuleDesc>> Modules,
	TConstArrayView<FAngelscriptCacheCleanModuleArtifacts> Artifacts,
	const FAngelscriptArtifactProfileKey& ArtifactProfile,
	TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
		ESPMode::ThreadSafe>& OutSnapshot,
	FString* OutError)
{
	(void)Engine;
	(void)Modules;
	(void)Artifacts;
	(void)ArtifactProfile;
	OutSnapshot.Reset();
	if (OutError)
	{
		*OutError = TEXT("StaticJIT Canonical AST generation is isolated");
	}
	return false;
}

#endif
