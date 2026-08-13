#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/AngelscriptJITProvider.h"
#include "StaticJIT/AngelscriptJITProviderLifetime.h"

#include "HAL/CriticalSection.h"
#include "Templates/SharedPointer.h"

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderCatalogEntry
{
	EAngelscriptJITArtifactEntryFlags Flags =
		EAngelscriptJITArtifactEntryFlags::None;
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptStableFunctionKey FunctionKey;
	FAngelscriptHash256 ExecutionHash;
	FAngelscriptHash256 DebugHash;
	FAngelscriptArtifactProfileKey ArtifactProfile;
	FAngelscriptHash256 EntryAbiHash;
	FAngelscriptHash256 NativeEnvironmentFingerprint;
	FAngelscriptJITEntryPoints EntryPoints;
	TArray<FAngelscriptJITReferenceSlot> ReferenceSlots;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderCatalog
{
	FAngelscriptJITProviderId ProviderId;
	FAngelscriptJITProviderGeneration ProviderGeneration;
	FAngelscriptHash256 ArtifactSetDigest;
	FAngelscriptArtifactProfileKey ArtifactProfile;
	FAngelscriptHash256 NativeEnvironmentFingerprint;
	FString ProviderName;
	FString OwnerModuleName;
	TSharedPtr<const FAngelscriptJITProviderLifetime,
		ESPMode::ThreadSafe> Lifetime;
	TArray<FAngelscriptJITProviderCatalogEntry> Entries;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderRegistrySnapshot
{
	uint64 PublicationOrdinal = 0;
	TArray<TSharedPtr<const FAngelscriptJITProviderCatalog,
		ESPMode::ThreadSafe>> Providers;
};

enum class EAngelscriptJITProviderRegistrationCode : uint8
{
	Registered = 0,
	Replaced,
	Unchanged,
	InvalidOwner,
	InvalidProvider,
	OwnerProviderIdChanged,
	ProviderIdConflict,
	AmbiguousGeneration,
	CodeLifetimeUnavailable,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderRegistrationResult
{
	EAngelscriptJITProviderRegistrationCode Code =
		EAngelscriptJITProviderRegistrationCode::InvalidProvider;
	FAngelscriptJITProviderValidationResult Validation;

	bool IsAccepted() const
	{
		return Code == EAngelscriptJITProviderRegistrationCode::Registered
			|| Code == EAngelscriptJITProviderRegistrationCode::Replaced
			|| Code == EAngelscriptJITProviderRegistrationCode::Unchanged;
	}
};

class ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderRegistry
{
public:
	FAngelscriptJITProviderRegistry();
	~FAngelscriptJITProviderRegistry();

	FAngelscriptJITProviderRegistrationResult RegisterProvider(
		const void* Owner,
		const IAngelscriptJITArtifactProvider& Provider);

	bool UnregisterOwner(const void* Owner);

	TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
		ESPMode::ThreadSafe> GetSnapshot() const;

	void StartModularFeatureDiscovery();
	void StopModularFeatureDiscovery();

	static FAngelscriptJITProviderRegistry& Get();

private:
	TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
		ESPMode::ThreadSafe> PublishSnapshotLocked();
	void OnModularFeatureRegistered(const FName& Type, IModularFeature* Feature);
	void OnModularFeatureUnregistered(const FName& Type, IModularFeature* Feature);

	mutable FCriticalSection Mutex;
	TMap<const void*, TSharedPtr<const FAngelscriptJITProviderCatalog,
		ESPMode::ThreadSafe>> CatalogsByOwner;
	TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
		ESPMode::ThreadSafe> Snapshot;
	uint64 NextPublicationOrdinal = 1;
	FDelegateHandle RegisteredHandle;
	FDelegateHandle UnregisteredHandle;
	TSet<const void*> ModularFeatureOwners;
};
