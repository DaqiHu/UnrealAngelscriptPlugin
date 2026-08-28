#pragma once

#include "CoreMinimal.h"

#include "StaticJIT/AngelscriptJITProviderDiagnostics.h"
#include "StaticJIT/AngelscriptJITProviderLifetime.h"

#include "HAL/CriticalSection.h"
#include "Templates/SharedPointer.h"

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderCatalogNativeCallSite
{
	uint32 CallSiteIndex = 0;
	EAngelscriptJITNativeCallSiteRoute Route =
		EAngelscriptJITNativeCallSiteRoute::Invalid;
	FString CanonicalDeclaration;
	FString EmittedCppCallee;
	FString RuntimeDllCore;
	FString MetadataRowSymbol;
	FString RegisteredTarget;
	FAngelscriptHash256 StableTargetKey;
	FAngelscriptHash256 ExpectedAbi;
	int32 ReferenceSlotIndex = INDEX_NONE;
};

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
	EAngelscriptJITExecutionCapability AvailableExecutionCapabilities =
		EAngelscriptJITExecutionCapability::None;
	FAngelscriptHash256 ExecutionCapabilityProfileHash;
	EAngelscriptJITExecutionCapability RuntimeExecutionRequirementTriggers =
		EAngelscriptJITExecutionCapability::None;
	TArray<FAngelscriptJITReferenceSlot> ReferenceSlots;
	TArray<FAngelscriptJITProviderCatalogNativeCallSite> NativeCallSites;
	TArray<FAngelscriptJITSemanticDependencyRow> SemanticDependencies;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderCatalogDiagnosticSource
{
	FString Section;
	uint32 Row = 0;
	uint32 Column = 0;
};

struct ANGELSCRIPTRUNTIME_API
FAngelscriptJITProviderCatalogCanonicalFunctionDiagnostic
{
	FAngelscriptStableFunctionKey FunctionKey;
	FString Role;
	uint32 RawTraitBits = 0;
	uint32 UnknownTraitBits = 0;
	FString InvocationKind;
	FString ReceiverKind;
	int32 ReceiverParameterIndex = INDEX_NONE;
	FString CanonicalASTState;
	FString CleanupPlanState;
	uint32 LifetimeProtocolRevision = 0;
	FAngelscriptHash256 LifetimeStableKey;
	FAngelscriptHash256 LifetimeABIKey;
	uint32 LifetimeActionFlags = 0;
	uint32 LifetimeExitPlanFlags = 0;
	uint32 LifetimeSupportedExitMask = 0;
	uint32 LifetimeRecordCount = 0;
	uint32 LifetimeExitPlanCount = 0;
	bool bNativeObjectFrameABIAvailable = false;
	bool bHasSuspendState = false;
	bool bHasExceptionCleanup = false;
	bool bCleanupPlanCoversAllTransfers = false;
	int32 ComponentIndex = INDEX_NONE;
	bool bRecursiveComponent = false;
	FString EligibilityReason;
	FString EligibilityDetail;
	FAngelscriptJITProviderCatalogDiagnosticSource EligibilitySource;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderCatalogTypedCallDiagnostic
{
	FAngelscriptStableFunctionKey CallerFunctionKey;
	FAngelscriptStableFunctionKey CalleeFunctionKey;
	uint32 ExpressionIndex = MAX_uint32;
	EAngelscriptJITProviderDiagnosticSourceFlags SourceFlags =
		EAngelscriptJITProviderDiagnosticSourceFlags::None;
	FAngelscriptJITProviderCatalogDiagnosticSource ProcessedSource;
	FAngelscriptJITProviderCatalogDiagnosticSource AuthoredSource;
	FAngelscriptJITProviderCatalogDiagnosticSource GeneratedSource;
	FString TargetKind;
	FString Disposition;
	FString CalleeEligibilityReason;
	FString CalleeEligibilityDetail;
	FAngelscriptJITProviderCatalogDiagnosticSource CalleeEligibilitySource;
	bool bHasNativeLinkageValidation = false;
	FString NativeLinkageValidation;
	FString NativeLinkageValidationDetail;
	FString CanonicalDeclaration;
	EAngelscriptJITNativeCallSiteRoute EmittedRoute =
		EAngelscriptJITNativeCallSiteRoute::Invalid;
	FString EmittedCppCallee;
	FString RuntimeDllCore;
	FString MetadataRowSymbol;
	FString RegisteredTarget;
	FString DirectCppSymbol;
	FAngelscriptHash256 StableTargetKey;
	FAngelscriptHash256 ExpectedAbi;
	int32 ReferenceSlotIndex = INDEX_NONE;
};

struct ANGELSCRIPTRUNTIME_API
FAngelscriptJITProviderCatalogBackendAttemptDiagnostic
{
	FString BackendId;
	FString Disposition;
	FString Detail;
	EAngelscriptJITProviderDiagnosticAttemptFlags Flags =
		EAngelscriptJITProviderDiagnosticAttemptFlags::None;
	FString FallbackReason;
	FString TypedDetail;
	FAngelscriptJITProviderCatalogDiagnosticSource ProcessedSource;
	int32 PrimarySemanticFunctionIndex = INDEX_NONE;
	uint32 SemanticFunctionStart = 0;
	uint32 SemanticFunctionCount = 0;
	uint32 CallStart = 0;
	uint32 CallCount = 0;
	EAngelscriptJITExecutionCapability RequiredCapabilities =
		EAngelscriptJITExecutionCapability::None;
	EAngelscriptJITExecutionCapability AvailableCapabilities =
		EAngelscriptJITExecutionCapability::None;
	FAngelscriptHash256 ExecutionRequirementsIdentity;
	FAngelscriptHash256 AvailableCapabilityProfileIdentity;
	FString ProfileKind;
	FString ExceptionControlState;
	FString ExceptionPayloadState;
	bool bRecursionGuardAvailable = false;
};

struct ANGELSCRIPTRUNTIME_API
FAngelscriptJITProviderCatalogFunctionDiagnostic
{
	FAngelscriptStableFunctionKey FunctionKey;
	FString ActualBackendId;
	FString Disposition;
	FString Detail;
	uint32 BackendAttemptStart = 0;
	uint32 BackendAttemptCount = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderCatalogDiagnostics
{
	uint32 SchemaRevision = 0;
	FAngelscriptHash256 DiagnosticDigest;
	FString RequestKind;
	FString RequestedBackendId;
	FString CaptureState;
	TArray<FString> FallbackChain;
	uint32 SourceCompilePassCount = 0;
	uint32 CompiledModuleCount = 0;
	uint32 CompiledFunctionCount = 0;
	uint32 CompiledTypeCount = 0;
	uint32 CompiledGlobalCount = 0;
	uint32 CompiledDescriptorCount = 0;
	uint32 CompiledDependencyCount = 0;
	uint32 CompiledExternalNativeCallCount = 0;
	uint32 EmitModuleCount = 0;
	bool bHasVerifiedCanonicalAST = false;
	FString TaskError;
	TArray<FAngelscriptJITProviderCatalogFunctionDiagnostic> Functions;
	TArray<FAngelscriptJITProviderCatalogBackendAttemptDiagnostic>
		BackendAttempts;
	TArray<FAngelscriptJITProviderCatalogCanonicalFunctionDiagnostic>
		SemanticFunctions;
	TArray<FAngelscriptJITProviderCatalogTypedCallDiagnostic> Calls;
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
	TOptional<FAngelscriptJITProviderCatalogDiagnostics> Diagnostics;
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
