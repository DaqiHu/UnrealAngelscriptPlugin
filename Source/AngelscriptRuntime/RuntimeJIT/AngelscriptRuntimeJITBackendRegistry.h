#pragma once

#include "CoreMinimal.h"

#include "JIT/AngelscriptRuntimeJITBackend.h"

struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITFactoryMetadataSnapshot
{
	FAngelscriptRuntimeJITBackendId BackendId;
	FString DisplayName;
	EAngelscriptRuntimeJITPlatform SupportedPlatforms =
		EAngelscriptRuntimeJITPlatform::None;
	EAngelscriptRuntimeJITBuildConfiguration SupportedConfigurations =
		EAngelscriptRuntimeJITBuildConfiguration::None;
	EAngelscriptRuntimeJITCompileConcurrency CompileConcurrency =
		EAngelscriptRuntimeJITCompileConcurrency::SerializedPerSession;
};

enum class EAngelscriptRuntimeJITFactorySelectionCode : uint8
{
	Selected = 0,
	MissingBackend,
	DuplicateBackendId,
	InvalidFactoryMetadata,
	UnsupportedPlatform,
	UnsupportedConfiguration,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITFactorySelection
{
	EAngelscriptRuntimeJITFactorySelectionCode Code =
		EAngelscriptRuntimeJITFactorySelectionCode::MissingBackend;
	EAngelscriptRuntimeJITContractValidationCode ValidationCode =
		EAngelscriptRuntimeJITContractValidationCode::Valid;
	IAngelscriptRuntimeJITBackendFactory* Factory = nullptr;
	FAngelscriptRuntimeJITFactoryMetadataSnapshot Metadata;

	bool IsSelected() const
	{
		return Code == EAngelscriptRuntimeJITFactorySelectionCode::Selected
			&& Factory != nullptr;
	}
};

class ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITBackendRegistry
{
public:
	static FAngelscriptRuntimeJITFactorySelection Select(
		const FAngelscriptRuntimeJITBackendId& BackendId,
		EAngelscriptRuntimeJITPlatform Platform,
		EAngelscriptRuntimeJITBuildConfiguration Configuration);
};

class FAngelscriptRuntimeJITBackendSessionOwner;

struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITSessionCreateResult
{
	EAngelscriptRuntimeJITFactorySelectionCode Code =
		EAngelscriptRuntimeJITFactorySelectionCode::MissingBackend;
	EAngelscriptRuntimeJITContractValidationCode ValidationCode =
		EAngelscriptRuntimeJITContractValidationCode::Valid;
	TUniquePtr<FAngelscriptRuntimeJITBackendSessionOwner> Owner;

	bool IsCreated() const
	{
		return Code == EAngelscriptRuntimeJITFactorySelectionCode::Selected
			&& Owner.IsValid();
	}
};

class ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITBackendSessionOwner final
{
public:
	static FAngelscriptRuntimeJITSessionCreateResult Create(
		const FAngelscriptRuntimeJITBackendSessionConfig& Config);

	~FAngelscriptRuntimeJITBackendSessionOwner();

	FAngelscriptRuntimeJITCompileResult Compile(
		const FAngelscriptRuntimeJITCompileSnapshotView& Snapshot);
	void Cancel(uint64 CancellationGeneration);

	bool RequiresSerializedCompilation() const
	{
		return Metadata.CompileConcurrency
			== EAngelscriptRuntimeJITCompileConcurrency::SerializedPerSession;
	}

	const FAngelscriptRuntimeJITFactoryMetadataSnapshot& GetMetadata() const
	{
		return Metadata;
	}

	uint64 GetEngineNamespace() const
	{
		return EngineNamespace;
	}

private:
	FAngelscriptRuntimeJITBackendSessionOwner(
		FAngelscriptRuntimeJITFactoryMetadataSnapshot InMetadata,
		uint64 InEngineNamespace,
		EAngelscriptRuntimeJITPlatform InPlatform,
		EAngelscriptRuntimeJITBuildConfiguration InConfiguration,
		TUniquePtr<IAngelscriptRuntimeJITBackendSession> InSession);

	FAngelscriptRuntimeJITFactoryMetadataSnapshot Metadata;
	uint64 EngineNamespace = 0;
	EAngelscriptRuntimeJITPlatform Platform =
		EAngelscriptRuntimeJITPlatform::None;
	EAngelscriptRuntimeJITBuildConfiguration Configuration =
		EAngelscriptRuntimeJITBuildConfiguration::None;
	TUniquePtr<IAngelscriptRuntimeJITBackendSession> Session;
	FCriticalSection CompileMutex;
};
