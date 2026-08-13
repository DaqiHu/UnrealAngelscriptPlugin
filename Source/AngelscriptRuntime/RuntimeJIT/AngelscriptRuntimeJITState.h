#pragma once

#include "CoreMinimal.h"

#include "RuntimeJIT/AngelscriptRuntimeJITBackendRegistry.h"
#include "RuntimeJIT/AngelscriptRuntimeJITSnapshot.h"

enum class EAngelscriptRuntimeJITCompilePolicy : uint8
{
	EagerSync = 0,
	EagerBackground,
	LazyFirstCall,
};

enum class EAngelscriptRuntimeJITFunctionState : uint8
{
	Missing = 0,
	WaitingForFirstCall,
	Compiling,
	Published,
	Unsupported,
	Cancelled,
	BackendFailure,
	InvalidInput,
	Stale,
	BackendUnavailable,
};

enum class EAngelscriptRuntimeJITRequestAction : uint8
{
	Started = 0,
	Registered,
	Coalesced,
	Memoized,
	AlreadyPublished,
	Rejected,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITSelectionConfig
{
	FAngelscriptRuntimeJITBackendId BackendId;
	EAngelscriptRuntimeJITPlatform Platform =
		EAngelscriptRuntimeJITPlatform::None;
	EAngelscriptRuntimeJITBuildConfiguration Configuration =
		EAngelscriptRuntimeJITBuildConfiguration::None;
	uint64 EngineNamespace = 0;
	EAngelscriptRuntimeJITCompilePolicy CompilePolicy =
		EAngelscriptRuntimeJITCompilePolicy::EagerSync;
	uint64 BackendConfigurationGeneration = 0;
	uint64 CancellationGeneration = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITObserveResult
{
	EAngelscriptRuntimeJITRequestAction Action =
		EAngelscriptRuntimeJITRequestAction::Rejected;
	EAngelscriptRuntimeJITFunctionState State =
		EAngelscriptRuntimeJITFunctionState::InvalidInput;
	EAngelscriptRuntimeJITDiagnosticReason Reason =
		EAngelscriptRuntimeJITDiagnosticReason::MalformedSnapshot;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITLazyCallResult
{
	bool bCompileClaimed = false;
	bool bUseVmForCurrentCall = true;
	EAngelscriptRuntimeJITFunctionState State =
		EAngelscriptRuntimeJITFunctionState::Missing;
};

/**
 * Coordinator-owned publication value. The code lease is copied into every
 * immutable execution route that can still call VMEntry.
 */
struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITPublishedCode
{
	FAngelscriptRuntimeJITBackendId BackendId;
	uint64 EngineNamespace = 0;
	uint64 PublicationOrdinal = 0;
	uint64 BackendConfigurationGeneration = 0;
	uint64 CancellationGeneration = 0;
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptStableFunctionKey FunctionKey;
	FAngelscriptHash256 FunctionRevision;
	FAngelscriptHash256 EntryAbiHash;
	asJITFunction VMEntry = nullptr;
	TSharedPtr<FAngelscriptRuntimeJITBackendSessionOwner,
		ESPMode::ThreadSafe> SessionKeepAlive;
	FAngelscriptRuntimeJITCodeLease CodeLease;
	uint64 CompileLatencyMicroseconds = 0;
	uint64 CodeSize = 0;

	bool IsValid() const
	{
		return BackendId.IsValid()
			&& EngineNamespace != 0
			&& PublicationOrdinal != 0
			&& !FunctionKey.Hash.IsZero()
			&& !FunctionRevision.IsZero()
			&& !EntryAbiHash.IsZero()
			&& VMEntry != nullptr
			&& SessionKeepAlive.IsValid()
			&& CodeLease.IsValid();
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITProcessResult
{
	int32 ProcessedCount = 0;
	int32 PublishedCount = 0;
	int32 StaleCount = 0;
	int32 TerminalCount = 0;
};

#if !UE_BUILD_SHIPPING
struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITFunctionDiagnostic
{
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptStableFunctionKey FunctionKey;
	FAngelscriptHash256 FunctionRevision;
	uint64 PublicationOrdinal = 0;
	EAngelscriptRuntimeJITFunctionProfile FunctionProfile =
		EAngelscriptRuntimeJITFunctionProfile::Unknown;
	EAngelscriptRuntimeJITFunctionState State =
		EAngelscriptRuntimeJITFunctionState::Missing;
	EAngelscriptRuntimeJITDiagnosticReason Reason =
		EAngelscriptRuntimeJITDiagnosticReason::None;
	uint64 CompileAttemptCount = 0;
	uint64 CompileLatencyMicroseconds = 0;
	uint64 CodeSize = 0;
	int32 CodeLeaseReferenceCount = 0;
};

/** Pointer-free observer snapshot copied under the state-machine lock. */
struct ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITStateDiagnosticSnapshot
{
	FAngelscriptRuntimeJITSelectionConfig Selection;
	bool bFactoryAvailable = false;
	bool bSessionAvailable = false;
	EAngelscriptRuntimeJITDiagnosticReason SessionReason =
		EAngelscriptRuntimeJITDiagnosticReason::BackendUnavailable;
	uint64 TotalCompileAttempts = 0;
	uint64 TotalResults = 0;
	uint64 StaleResultCount = 0;
	uint64 CancelledResultCount = 0;
	uint64 CancellationRequestCount = 0;
	int32 ActiveOperationCount = 0;
	TArray<FAngelscriptRuntimeJITFunctionDiagnostic> Functions;
};
#endif

/**
 * Engine-local request/publication state. It never mutates an AngelScript
 * function directly; the future Coordinator consumes PublishedCode only at an
 * Engine safe point.
 */
class ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeJITRequestStateMachine final
{
public:
	explicit FAngelscriptRuntimeJITRequestStateMachine(
		const FAngelscriptRuntimeJITSelectionConfig& InSelection);
	~FAngelscriptRuntimeJITRequestStateMachine();

	FAngelscriptRuntimeJITRequestStateMachine(
		const FAngelscriptRuntimeJITRequestStateMachine&) = delete;
	FAngelscriptRuntimeJITRequestStateMachine& operator=(
		const FAngelscriptRuntimeJITRequestStateMachine&) = delete;
	FAngelscriptRuntimeJITRequestStateMachine(
		FAngelscriptRuntimeJITRequestStateMachine&&) = delete;
	FAngelscriptRuntimeJITRequestStateMachine& operator=(
		FAngelscriptRuntimeJITRequestStateMachine&&) = delete;

	bool Reconfigure(
		const FAngelscriptRuntimeJITSelectionConfig& InSelection);

	FAngelscriptRuntimeJITObserveResult ObserveFunction(
		FAngelscriptRuntimeJITCompileSnapshot Snapshot);

	FAngelscriptRuntimeJITLazyCallResult OnLazyFirstCall(
		const FAngelscriptStableFunctionKey& FunctionKey,
		const FAngelscriptHash256& FunctionRevision,
		uint64 PublicationOrdinal);

	FAngelscriptRuntimeJITProcessResult ProcessPendingResults();

	bool TryAcquirePublished(
		const FAngelscriptStableFunctionKey& FunctionKey,
		const FAngelscriptHash256& FunctionRevision,
		uint64 PublicationOrdinal,
		FAngelscriptRuntimeJITPublishedCode& OutPublished) const;

	bool RetireFunction(
		const FAngelscriptStableFunctionKey& FunctionKey,
		uint64 PublicationOrdinal);

#if !UE_BUILD_SHIPPING
	FAngelscriptRuntimeJITStateDiagnosticSnapshot CaptureDiagnostics() const;
#endif

	void Shutdown();

private:
	struct FImpl;
	TUniquePtr<FImpl> Impl;
};
