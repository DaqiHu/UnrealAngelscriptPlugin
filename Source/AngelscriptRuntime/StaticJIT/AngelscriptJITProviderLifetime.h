#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/AngelscriptJITProvider.h"

#include "Templates/SharedPointer.h"

enum class EAngelscriptJITProviderCodeLifetime : uint8
{
	LogicalOnly = 0,
	PinnedDynamicModule,
	ProcessLifetimeImage,
};

/**
 * Runtime-owned immutable lease for one Provider generation.
 *
 * A Registry catalog retains this object together with copied provider data.
 * For a loaded dynamic provider the lease resolves every published entry-point
 * address and owns an additional platform reference for each containing code
 * image. ModuleManager and Live Coding may therefore drop their references
 * without unmapping entry points still reachable through retired Bindings. The
 * lease and its deleter live in AngelscriptRuntime, never in generated code.
 */
class ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderLifetime final
{
public:
	static TSharedRef<const FAngelscriptJITProviderLifetime,
		ESPMode::ThreadSafe> CreateLogical(
		const FAngelscriptJITProviderId& ProviderId,
		const FAngelscriptJITProviderGeneration& ProviderGeneration,
		FStringView OwnerModuleName);

	static TSharedRef<const FAngelscriptJITProviderLifetime,
		ESPMode::ThreadSafe> AcquireForLoadedProvider(
		const FAngelscriptJITProviderView& View);

	~FAngelscriptJITProviderLifetime();

	const FAngelscriptJITProviderId& GetProviderId() const
	{
		return ProviderId;
	}

	const FAngelscriptJITProviderGeneration& GetProviderGeneration() const
	{
		return ProviderGeneration;
	}

	const FString& GetOwnerModuleName() const
	{
		return OwnerModuleName;
	}

	TConstArrayView<FString> GetPinnedModuleFilenames() const
	{
		return PinnedModuleFilenames;
	}

	EAngelscriptJITProviderCodeLifetime GetCodeLifetime() const
	{
		return CodeLifetime;
	}

	bool KeepsCodeMapped() const
	{
		return bKeepsCodeMapped;
	}

private:
	FAngelscriptJITProviderLifetime(
		const FAngelscriptJITProviderId& InProviderId,
		const FAngelscriptJITProviderGeneration& InProviderGeneration,
		FStringView InOwnerModuleName);

	FAngelscriptJITProviderId ProviderId;
	FAngelscriptJITProviderGeneration ProviderGeneration;
	FString OwnerModuleName;
	TArray<FString> PinnedModuleFilenames;
	EAngelscriptJITProviderCodeLifetime CodeLifetime =
		EAngelscriptJITProviderCodeLifetime::LogicalOnly;
	TArray<void*> PinnedModuleHandles;
	bool bKeepsCodeMapped = false;
};
