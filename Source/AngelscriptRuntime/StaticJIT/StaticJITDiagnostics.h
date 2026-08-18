#pragma once

#include "CoreMinimal.h"
#include "Core/AngelscriptEngine.h"
#include "Artifacts/AngelscriptFunctionRoute.h"
#include "StaticJIT/AngelscriptJITGeneration.h"
#include "StaticJIT/AngelscriptJITProviderRegistry.h"
#include "StaticJIT/AngelscriptJITReferenceResolver.h"
#include "StaticJIT/StaticJITConfig.h"

class asCScriptFunction;
#if AS_CAN_GENERATE_JIT
struct FAngelscriptStaticJITGeneratorOutput;
#endif

struct ANGELSCRIPTRUNTIME_API FStaticJITDiagnostics
{
	enum class EArtifactRequestKind : uint8
	{
		StaticJITArtifact = 0,
	};

	enum class EPrePublicationFailureCategory : uint8
	{
		Invalid = 0,
		AuthoritativeEngineStale,
		GenerationContainmentFailure,
	};

	enum class EPrePublicationBoundary : uint8
	{
		Invalid = 0,
		PrimarySourceAuthority,
		PrimaryPackageOwnership,
		ProviderRegistry,
		PrimaryCache,
		PrimaryRoute,
		UObjectState,
		DelegateState,
		WorldState,
		PooledContext,
	};

	/** Failure observed before any generated Provider can be published. */
	struct FPrePublicationFailureRecord
	{
		EPrePublicationFailureCategory Category =
			EPrePublicationFailureCategory::Invalid;
		EPrePublicationBoundary AffectedBoundary =
			EPrePublicationBoundary::Invalid;
		FString Detail;
		bool bGenerationEngineCreated = false;
		bool bProviderPackagingStarted = false;
		bool bProviderPublished = false;
	};

	enum class EArtifactCaptureState : uint8
	{
		Bytecode = 0,
		VerifiedTypedHIR,
	};

	enum class EArtifactFunctionDisposition : uint8
	{
		Emitted = 0,
		Unsupported,
		Failed,
	};

	struct FProcessedSourceRecord
	{
		FString Section;
		uint32 Row = 0;
		uint32 Column = 0;
	};

	struct FTypedSemanticFunctionDiagnosticRecord
	{
		FAngelscriptStableFunctionKey FunctionKey;
		FString Role;
		uint32 RawTraitBits = 0;
		uint32 UnknownTraitBits = 0;
		FString InvocationKind;
		FString ReceiverKind;
		int32 ReceiverParameterIndex = INDEX_NONE;
		FString HIRState;
		FString CleanupPlanState;
		bool bHasSuspendState = false;
		bool bHasExceptionCleanup = false;
		bool bCleanupPlanCoversAllTransfers = false;
		int32 ComponentIndex = INDEX_NONE;
		bool bRecursiveComponent = false;
		FString EligibilityReason;
		FString EligibilityDetail;
		FProcessedSourceRecord EligibilitySource;
	};

	struct FTypedCallDiagnosticRecord
	{
		FAngelscriptStableFunctionKey CallerFunctionKey;
		FAngelscriptStableFunctionKey CalleeFunctionKey;
		uint32 ExpressionIndex = MAX_uint32;
		FProcessedSourceRecord ProcessedSource;
		TOptional<FProcessedSourceRecord> AuthoredSource;
		TOptional<FProcessedSourceRecord> GeneratedSource;
		FString TargetKind;
		FString Disposition;
		FString CalleeEligibilityReason;
		FString CalleeEligibilityDetail;
		FProcessedSourceRecord CalleeEligibilitySource;
		FString NativeLinkageValidation;
		FString NativeLinkageValidationDetail;
		FString CanonicalDeclaration;
		FString EmittedRoute;
		FString EmittedCppCallee;
		FString RuntimeDllCore;
		FString MetadataRowSymbol;
		FString RegisteredTarget;
		FString DirectCppSymbol;
		FAngelscriptHash256 StableTargetKey;
		FAngelscriptHash256 ExpectedAbi;
		int32 ReferenceSlotIndex = INDEX_NONE;
	};

	struct FTypedCallClosureDiagnosticRecord
	{
		bool bEligible = false;
		FString RequiredCapabilities;
		FString AvailableCapabilities;
		FString ExecutionRequirementsIdentity;
		FString AvailableCapabilityProfileIdentity;
		FString ProfileKind;
		FString ExceptionControlState;
		FString ExceptionPayloadState;
		bool bRecursionGuardAvailable = false;
		TArray<FTypedSemanticFunctionDiagnosticRecord> Functions;
		TArray<FTypedCallDiagnosticRecord> Calls;
	};

	struct FTypedFunctionDiagnosticRecord
	{
		FString FallbackReason;
		FString Detail;
		FProcessedSourceRecord ProcessedSource;
		TOptional<FTypedSemanticFunctionDiagnosticRecord> SemanticFunction;
		TOptional<FTypedCallClosureDiagnosticRecord> CallClosure;
	};

	struct FArtifactBackendAttemptRecord
	{
		FString BackendId;
		EArtifactFunctionDisposition Disposition =
			EArtifactFunctionDisposition::Unsupported;
		FString Detail;
		TOptional<FTypedFunctionDiagnosticRecord> TypedDiagnostic;
	};

	struct FArtifactFunctionRecord
	{
		FAngelscriptStableFunctionKey FunctionKey;
		FString ActualBackendId;
		EArtifactFunctionDisposition Disposition =
			EArtifactFunctionDisposition::Unsupported;
		FString Detail;
		bool bHasVMEntry = false;
		bool bHasRawEntry = false;
		bool bHasParmsEntry = false;
		TArray<FArtifactBackendAttemptRecord> BackendAttempts;
		TOptional<FTypedFunctionDiagnosticRecord> TypedDiagnostic;
	};

	struct FArtifactCompiledSourceGraphRecord
	{
		uint32 SourceCompilePassCount = 0;
		int32 ModuleCount = 0;
		int32 FunctionCount = 0;
		int32 TypeCount = 0;
		int32 GlobalCount = 0;
		int32 DescriptorCount = 0;
		int32 DependencyCount = 0;
		int32 ExternalNativeCallCount = 0;
		int32 EmitModuleCount = 0;
		bool bHasVerifiedTypedHIR = false;
	};

	/** Pointer-free audit of one StaticJIT artifact generation request. */
	struct FArtifactRequestRecord
	{
		EArtifactRequestKind RequestKind =
			EArtifactRequestKind::StaticJITArtifact;
		FString RequestedBackendId;
		EArtifactCaptureState CaptureState =
			EArtifactCaptureState::Bytecode;
		bool bHIRCaptureEnabled = false;
		TArray<FString> FallbackChain;
		FArtifactCompiledSourceGraphRecord CompiledSourceGraph;
		FString TaskError;
		TArray<FArtifactFunctionRecord> Functions;
	};

	enum class EReferenceAdoptionState : uint8
	{
		Unavailable = 0,
		Unbound,
		ResolvedNotAdopted,
		Bound,
		Stale,
		Rejected,
	};

	struct FReferenceRecord
	{
		uint32 SlotIndex = 0;
		EAngelscriptJITReferenceSlotFlags Flags =
			EAngelscriptJITReferenceSlotFlags::None;
		FAngelscriptArtifactReference Reference;
		bool bResolved = false;
		EAngelscriptJITReferenceResolutionCode CurrentResolution =
			EAngelscriptJITReferenceResolutionCode::Missing;
		EReferenceAdoptionState AdoptionState =
			EReferenceAdoptionState::Unavailable;
		FString CurrentTargetDeclaration;
		FString CurrentTargetKind;
	};

	struct FNativeCallSiteRecord
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
		bool bResolved = false;
		EAngelscriptJITReferenceResolutionCode CurrentResolution =
			EAngelscriptJITReferenceResolutionCode::Missing;
		EReferenceAdoptionState AdoptionState =
			EReferenceAdoptionState::Unavailable;
		FString CurrentTargetDeclaration;
		FString CurrentTargetKind;
	};

	struct FProviderEntryRecord
	{
		int32 ProviderEntryIndex = INDEX_NONE;
		EAngelscriptJITArtifactEntryFlags Flags =
			EAngelscriptJITArtifactEntryFlags::None;
		FAngelscriptStableModuleKey ModuleKey;
		FAngelscriptStableFunctionKey FunctionKey;
		FAngelscriptHash256 ExecutionHash;
		FAngelscriptHash256 DebugHash;
		FAngelscriptArtifactProfileKey ArtifactProfile;
		FAngelscriptHash256 EntryAbiHash;
		FAngelscriptHash256 NativeEnvironmentFingerprint;
		bool bHasVMEntry = false;
		bool bHasRawEntry = false;
		bool bHasParmsEntry = false;
		FString GeneratedModuleSource;
		TArray<FReferenceRecord> References;
		TArray<FNativeCallSiteRecord> NativeCallSites;
	};

	struct FProviderModuleRecord
	{
		FAngelscriptStableModuleKey ModuleKey;
		FString GeneratedModuleSource;
		FAngelscriptHash256 ModuleArtifactDigest;
		uint32 FunctionCount = 0;
	};

	struct FProviderRecord
	{
		FAngelscriptJITProviderId ProviderId;
		FAngelscriptJITProviderGeneration ProviderGeneration;
		FAngelscriptHash256 ArtifactSetDigest;
		FAngelscriptArtifactProfileKey ArtifactProfile;
		FAngelscriptHash256 NativeEnvironmentFingerprint;
		FString ProviderName;
		FString OwnerModuleName;
		uint32 CompatibleEntryCount = 0;
		uint32 RejectedEntryCount = 0;
		uint32 ConflictingEntryCount = 0;
		TOptional<FAngelscriptJITProviderCatalogDiagnostics>
			GenerationDiagnostics;
		TArray<FProviderModuleRecord> Modules;
		TArray<FProviderEntryRecord> Entries;
	};

	struct FRouteCandidateRecord
	{
		FAngelscriptJITProviderId ProviderId;
		FAngelscriptJITProviderGeneration ProviderGeneration;
		int32 ProviderEntryIndex = INDEX_NONE;
		EAngelscriptArtifactMatchResult MatchResult =
			EAngelscriptArtifactMatchResult::MissingProviderEntry;
	};

	struct FRouteRecord
	{
		FAngelscriptStableModuleKey ModuleKey;
		FAngelscriptFunctionArtifactIdentity Identity;
		FString CanonicalDeclaration;
		int32 NumericFunctionId = -1;
		EAngelscriptFunctionExecutionRoute SelectedExecutionRoute =
			EAngelscriptFunctionExecutionRoute::Vm;
		EAngelscriptArtifactMatchResult MatchResult =
			EAngelscriptArtifactMatchResult::MissingProviderEntry;
		bool bHasVerifiedArtifactIdentity = false;
		bool bHasVMEntry = false;
		bool bHasRawEntry = false;
		bool bHasParmsEntry = false;
		bool bImmutableCookedDirectDispatch = false;
		FAngelscriptJITProviderId ProviderId;
		FAngelscriptJITProviderGeneration ProviderGeneration;
		int32 ProviderEntryIndex = INDEX_NONE;
		uint32 ReferenceSlotCount = 0;
		uint32 ResolvedReferenceSlotCount = 0;
		uint64 VMExecutionCount = 0;
		uint64 RawExecutionCount = 0;
		uint64 ParmsExecutionCount = 0;
		TArray<FRouteCandidateRecord> Candidates;
	};

	struct FSnapshot
	{
		static constexpr uint32 CurrentSchemaVersion = 9;

		uint32 SchemaVersion = CurrentSchemaVersion;
		uint32 ProviderAbiRevision = FAngelscriptJITProviderAbi::Revision;
		bool bHasCurrentEngine = false;
		bool bHasScriptEngine = false;
		FString Query;
		bool bQueryMatched = false;
		EAngelscriptJITTargetProfile TargetProfile =
			EAngelscriptJITTargetProfile::Invalid;
		uint64 ProviderPublicationOrdinal = 0;
		uint64 RouteGeneration = 0;
		uint32 VmRouteCount = 0;
		uint32 NativeRouteCount = 0;
		TOptional<FPrePublicationFailureRecord> PrePublicationFailure;
		TOptional<FArtifactRequestRecord> ArtifactRequest;
		TArray<FProviderRecord> Providers;
		TArray<FRouteRecord> Routes;
	};

	struct FJsonResult
	{
		FString Json;
		FString Detail;

		bool IsSuccess() const
		{
			return !Json.IsEmpty() && Detail.IsEmpty();
		}
	};

	static bool HasJitFunction(asCScriptFunction* Function);
#if AS_CAN_GENERATE_JIT
	static FArtifactRequestRecord BuildArtifactRequestRecord(
		const FAngelscriptStaticJITGeneratorOutput& GeneratorOutput);
#endif
	static FSnapshot CaptureSnapshot(FAngelscriptEngine* Engine = nullptr);
	static FJsonResult Serialize(const FSnapshot& Snapshot);
	static FJsonResult CaptureJson(FAngelscriptEngine* Engine = nullptr);
	static bool WriteJson(const FString& Filename, FAngelscriptEngine* Engine = nullptr, FString* OutError = nullptr);
	static void DumpDiagnostics(const TArray<FString>& Args);
};
