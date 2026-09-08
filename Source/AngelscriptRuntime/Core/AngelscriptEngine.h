#pragma once

#include "CoreMinimal.h"
#include "Misc/CoreDelegates.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/CoreNetTypes.h"
#include "ClassGenerator/AngelscriptAdditionalCompileChecks.h"
#include "Cache/AngelscriptRuntimeReload.h"

#include "AngelscriptSource.h"
#include "AngelscriptSourceProvider.h"
#include "AngelscriptType.h"
#include "AngelscriptDescriptors.h"
#include "AngelscriptMemoryTags.h"
#include "RuntimeJIT/AngelscriptJITConfiguration.h"
#include "StaticJIT/StaticJITConfig.h"

#include "AngelscriptEngine.generated.h"

#define AS_CAN_HOTRELOAD (PLATFORM_DESKTOP)
#define AS_MAX_POOLED_CONTEXTS 10
#define AS_PRINT_STATS (!UE_BUILD_SHIPPING)
#define AS_PRECOMPILED_STATS 1
#ifndef WITH_AS_DEBUGSERVER
#define WITH_AS_DEBUGSERVER (!UE_BUILD_TEST && !UE_BUILD_SHIPPING)
#endif
#define AS_USE_BIND_DB (!WITH_EDITOR)
#define AS_ITERATOR_DEBUGGING (WITH_EDITOR)
#ifndef AS_REFERENCE_DEBUGGING
#define AS_REFERENCE_DEBUGGING (WITH_EDITOR)
#endif
#define WITH_AS_COVERAGE WITH_AS_DEBUGSERVER

#ifndef AS_ENFORCE_SERVER_RPC_VALIDATION
#define AS_ENFORCE_SERVER_RPC_VALIDATION 0
#endif

ANGELSCRIPTRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(Angelscript, Log, All);

class asIScriptEngine;
class asIScriptContext;
class asIScriptModule;
class asIScriptFunction;
class asIScriptObject;
class asITypeInfo;
class asCContext;
struct asSJITFunctionBinding;
enum class EAngelscriptJITExecutionCapability : uint32;
class FAngelscriptBindDatabase;
struct FAngelscriptTypeDatabase;
struct FAngelscriptBindState;
struct FAngelscriptDocumentationState;
struct FAngelscriptNativeFormState;
struct FToStringType;

class FAngelscriptScriptTestHotReloadRunner;
class FBlueprintEventSignatureRegistry;
struct FAngelscriptEngineLifetimeToken;
struct FAngelscriptEngineContextStack;
struct FAngelscriptEngineScope;
struct FAngelscriptFunctionRouteState;
class FAngelscriptCacheService;
class FAngelscriptJITCoordinator;
class FAngelscriptJITProviderRouter;
class IAngelscriptCacheRestoreFaultInjector;
struct FAngelscriptCacheCompileCaptureContext;
class FAngelscriptCacheCompileReuseContext;
struct FAngelscriptFunctionRoute;
struct FAngelscriptFunctionRouteSnapshot;
struct FAngelscriptCachePackPolicy;
struct FAngelscriptStableFunctionKey;
struct FAngelscriptFunctionArtifactIdentity;
struct FAngelscriptStaticJITGenerationSnapshot;

struct FStaticJITDiagnostics;
struct FAngelscriptStateSnapshotBuilder;

// Hook-surface forward decls. Previously these lived in a separate
// hooks header along with a small container struct. The struct was
// inlined onto FAngelscriptEngine so that hook subscribers can write
// `Engine.GetOnPostReload()` directly. The container existed only as a
// stylistic wrapper -- there were never any consumers that used it
// beyond the accessor -- so removing it eliminated 133 indirect call
// sites without losing any encapsulation property.
class UActorComponent;
class ULevel;
class UASClass;
class UClass;
class UEnum;
class UScriptStruct;
class UDelegateFunction;

struct FAngelscriptClassDesc;
struct FAngelscriptModuleDesc;

typedef TArray<FName> FAngelscriptDebugBreakOptions;
typedef TMap<FName, FString> FAngelscriptDebugBreakFilters;
typedef const TArray<TPair<FName, int64>>& EnumNameList;

DECLARE_DELEGATE_RetVal(class ULevel*, FAngelscriptGetDynamicSpawnLevel);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FAngelscriptDebugCheckBreakOptions, const FAngelscriptDebugBreakOptions&, UObject*);
DECLARE_DELEGATE_OneParam(FAngelscriptGetDebugBreakFilters, FAngelscriptDebugBreakFilters&);
DECLARE_DELEGATE_TwoParams(FAngelscriptDebugObjectSuffix, UObject*, FString&);
DECLARE_DELEGATE_OneParam(FAngelscriptComponentCreated, class UActorComponent*);
DECLARE_DELEGATE_ThreeParams(FAngelscriptClassAnalyzeDelegate, FString&, TSharedPtr<struct FAngelscriptClassDesc>, bool&);
DECLARE_MULTICAST_DELEGATE_OneParam(FAngelscriptPostCompileClassCollection, const TArray<TSharedRef<struct FAngelscriptModuleDesc>>&);
DECLARE_MULTICAST_DELEGATE_OneParam(FAngelscriptPreGenerateClasses, const TArray<TSharedRef<struct FAngelscriptModuleDesc>>&);
DECLARE_MULTICAST_DELEGATE(FAngelscriptCompilationDelegate);
DECLARE_MULTICAST_DELEGATE_TwoParams(FAngelscriptLiteralAssetCreated, UObject*, const FString&);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnAngelscriptPostReload, bool);
DECLARE_MULTICAST_DELEGATE(FOnAngelscriptFullReload);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAngelscriptLiteralAssetReload, UObject*, UObject*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAngelscriptClassReload, UClass*, UClass*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAngelscriptEnumCreated, UEnum*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAngelscriptEnumChanged, UEnum*, EnumNameList);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAngelscriptStructReload, UScriptStruct*, UScriptStruct*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAngelscriptDelegateReload, UDelegateFunction*, UDelegateFunction*);

ANGELSCRIPTRUNTIME_API bool PrepareAngelscriptContextWithLog(class asIScriptContext* Context, class asIScriptFunction* ScriptFunction, const TCHAR* Callsite);

struct FInterfaceMethodSignature
{
	FName FunctionName;
};

UENUM()
enum class EAngelscriptEnginePurpose : uint8
{
	Runtime = 0,
	StaticJITGeneration,
};

USTRUCT()
struct ANGELSCRIPTRUNTIME_API FAngelscriptEngineConfig
{
	GENERATED_BODY()

	/** Ordinary Runtime/Editor behavior remains the default. */
	UPROPERTY()
	EAngelscriptEnginePurpose Purpose = EAngelscriptEnginePurpose::Runtime;

	/** Frozen before Engine creation; typed generation requires a retained sealed Canonical AST. */
	bool bRequireCanonicalASTForStaticJITGeneration = false;

	/** 0 = discard AST after CodeGen, 1 = retain sealed snapshot. Applied to the initial compile. */
	int32 InitialASTRetentionPolicy = 0;

	/**
	 * Select the canonical Parser/Sema/Bytecode pipeline for every source module
	 * owned by this Engine. The UE host remains on the LEGACY pipeline until the
	 * staged production cutover matrix is complete; focused migration fixtures
	 * opt in here. This is deliberately an Engine-wide selection and never a
	 * per-declaration fallback switch.
	 */
	bool bUseCanonicalStagedCompiler = false;

	EAngelscriptJITExecutionMode JITExecutionMode =
		EAngelscriptJITExecutionMode::Auto;
	FAngelscriptRuntimeJITBackendId RuntimeJITBackend;
	EAngelscriptRuntimeJITCompilePolicy RuntimeJITCompilePolicy =
		EAngelscriptRuntimeJITCompilePolicy::EagerSync;
	bool bJITExecutionModeValid = true;
	bool bRuntimeJITConfigurationValid = true;
	FString JITConfigurationError;

	UPROPERTY()
	bool bForceThreadedInitialize = false;

	UPROPERTY()
	bool bSkipThreadedInitialize = false;

	// When true, FAngelscriptEngine::Create routes to InitializeWithoutInitialCompile()
	// (binds + runtime services, but no on-disk script scan / initial compile).
	// Default false reflects the production path (full Initialize). Test fixtures
	// set this via FAngelscriptTestEngine::Create; AngelscriptEditor tests set it
	// inline before calling FAngelscriptEngine::Create. See OpenSpec
	// `refactor-as-engine-clone-removal` D8 / Section 7.
	UPROPERTY()
	bool bSkipInitialCompile = false;

	UPROPERTY()
	bool bSimulateCooked = false;

	UPROPERTY()
	bool bTestErrors = false;

	UPROPERTY()
	bool bForcePreprocessEditorCode = false;

	UPROPERTY()
	bool bDevelopmentMode = false;

	// Tool-owned isolated target override. This is deliberately four explicit
	// facts so an Editor commandlet can compile Development and Shipping script
	// surfaces without inheriting the host executable's UE_BUILD_* macros.
	UPROPERTY()
	bool bOverrideTargetPreprocessorFlags = false;

	UPROPERTY()
	bool bTargetEditor = false;

	UPROPERTY()
	bool bTargetEditorOnlyData = false;

	UPROPERTY()
	bool bTargetRelease = false;

	UPROPERTY()
	bool bTargetTest = false;

	// Isolated-test switch used to collect native-form bindings without publishing
	// them into the live Engine route snapshot.
	UPROPERTY()
	bool bCollectStaticJITCompatibilityBinds = false;

	UPROPERTY()
	bool bSkipWriteBindDB = false;

	UPROPERTY()
	bool bWriteBindDB = false;

	UPROPERTY()
	bool bExitOnError = false;

	UPROPERTY()
	bool bDumpDocumentation = false;

	UPROPERTY()
	int32 DebugServerPort = 27099;

	UPROPERTY()
	bool bIsEditor = false;

	UPROPERTY()
	bool bRunningCommandlet = false;

#if WITH_DEV_AUTOMATION_TESTS
	// Test-only fault injection used to prove binding failures cannot publish a
	// partially initialized engine. Production configuration has no equivalent.
	bool bInjectDirectBindFailureForTesting = false;
	// Removes one generation-owned, otherwise verified Canonical AST snapshot
	// immediately before generation snapshot validation. This proves the real
	// missing-input failure path without making an invalid AST a supported state.
	bool bInjectMissingCanonicalASTSnapshotForTesting = false;
#endif
	// Captured once from FApp::IsUnattended() for startup-failure policy. Tests
	// set this explicitly so the policy can be verified without process globals.
	UPROPERTY()
	bool bIsUnattended = false;

	// Host/test override for the Cache V2 base root. Production command lines use
	// -as-cache-root; an empty value selects Saved/Angelscript/CacheV2.
	UPROPERTY()
	FString CacheV2RootOverride;

	// Host/test override for the pointer-free process session report. Production
	// command lines use -as-cache-report=<absolute-json-path>; an empty value
	// disables automatic report emission.
	UPROPERTY()
	FString CacheV2ReportPathOverride;

	// Explicit package-smoke lifecycle hook parsed from
	// -as-cache-exit-after-startup. Shipping compiles out ExecCmds, so the
	// acceptance harness requests a normal Engine shutdown after successful
	// startup and lets the production shutdown path flush/report the Cache.
	UPROPERTY()
	bool bExitAfterStartupForCacheSmoke = false;

	// Explicit diagnostic opt-in parsed from -as-cache-trace. Unlike the console
	// command, this is applied while the Engine is constructed so startup
	// selection and exact restore decisions are retained in the bounded journal.
	UPROPERTY()
	bool bForceEnableCacheV2DecisionTrace = false;

	// Optional -as-cache-trace-capacity override. Zero keeps the configured
	// UAngelscriptCacheSettings capacity; the service owns the final 1..65536
	// clamp and bounded-eviction policy.
	UPROPERTY()
	uint32 CacheV2DecisionTraceCapacityOverride = 0;

	// Optional benchmark/host overrides. Zero retains the project setting. These
	// affect only physical writer preparation and never Compatibility/Profile or
	// stable semantic identity.
	UPROPERTY()
	uint32 CacheV2PackTargetMiBOverride = 0;

	UPROPERTY()
	uint32 CacheV2PreparationWorkerCountOverride = 0;

	UPROPERTY()
	bool bForceSerialCacheV2Preparation = false;

	// Explicit host/test override for the project Cache V2 enablement setting.
	// Production leaves this false and resolves UAngelscriptCacheSettings once
	// when the Engine is constructed.
	UPROPERTY()
	bool bOverrideCacheV2Enablement = false;

	UPROPERTY()
	bool bEnableCacheV2 = false;

	// Test/commandlet isolation switch. Product enablement remains owned by
	// UAngelscriptCacheSettings rather than this per-Engine construction input.
	UPROPERTY()
	bool bDisableCacheV2Persistence = false;

	// Explicit host/test override. Production resolves the project setting.
	UPROPERTY()
	bool bOverridePackagedRuntimeReloadMode = false;

	UPROPERTY()
	EAngelscriptPackagedRuntimeReloadMode PackagedRuntimeReloadMode =
		EAngelscriptPackagedRuntimeReloadMode::Disabled;

	UPROPERTY()
	float PackagedRuntimeReloadScanIntervalSeconds = 1.0f;

	static FAngelscriptEngineConfig FromCurrentProcess();
};

ANGELSCRIPTRUNTIME_API FAngelscriptCachePackPolicy
ResolveAngelscriptCacheWriterPolicy(
	const FAngelscriptEngineConfig& Config,
	uint32 ConfiguredPackTargetMiB,
	bool bConfiguredParallelPreparation,
	uint32 ConfiguredPreparationWorkerCount);

enum class EAngelscriptStartupCompileFailureResponse : uint8
{
	RequestExit,
	InteractiveRetry,
};

// Pure startup policy seam. The caller owns diagnostics and the concrete exit or
// Slate action; unattended/commandlet/explicit-exit hosts never select a modal.
ANGELSCRIPTRUNTIME_API EAngelscriptStartupCompileFailureResponse
ResolveAngelscriptStartupCompileFailureResponse(
	const FAngelscriptEngineConfig& Config,
	bool bInteractiveRetryAvailable);

ANGELSCRIPTRUNTIME_API bool ShouldRequestAngelscriptCachePackageSmokeExit(
	const FAngelscriptEngineConfig& Config,
	bool bInitialCompileSucceeded);

struct FAngelscriptStartupCompileFailureExitRequest
{
	bool bForce = true;
	bool bBeginCacheShutdownBeforeDiagnosticReport = true;
	bool bWriteRequestedDiagnosticReportBeforeExit = true;
	uint8 Status = 3;
};

ANGELSCRIPTRUNTIME_API FAngelscriptStartupCompileFailureExitRequest
ResolveAngelscriptStartupCompileFailureExitRequest(
	const FAngelscriptEngineConfig& Config);

struct ANGELSCRIPTRUNTIME_API FAngelscriptPluginScriptRoot
{
	FString PluginName;
	FString ScriptRoot;
};

#if WITH_DEV_AUTOMATION_TESTS
/** Read-only observation of process package ownership used by containment tests. */
struct ANGELSCRIPTRUNTIME_API FAngelscriptProcessPackageReferenceCounts
{
	int32 AngelscriptPackage = 0;
	int32 AngelscriptAssetsPackage = 0;
};
#endif

struct ANGELSCRIPTRUNTIME_API FAngelscriptEngineDependencies
{
	TFunction<FString()> GetProjectDir;
	TFunction<FString(const FString&)> ConvertRelativePathToFull;
	TFunction<bool(const FString&)> DirectoryExists;
	TFunction<bool(const FString&, bool)> MakeDirectory;
	TFunction<TArray<FString>()> GetEnabledPluginScriptRoots;
	TFunction<TArray<FAngelscriptPluginScriptRoot>()> GetEnabledPluginScriptRootDescriptors;
	TSharedPtr<IAngelscriptSourceProvider> SourceProvider;
	// Owns the process-level side effect for a deterministic startup failure.
	// Tests may replace it to observe the request without terminating their host.
	TFunction<void(const FAngelscriptStartupCompileFailureExitRequest&)>
		HandleStartupCompileFailureExit;

#if WITH_ANGELSCRIPT_UNITTESTS
	// Per-Engine, caller-owned exact-restore fault seam. Production dependencies
	// leave this null; it is never copied into persisted or diagnostic state.
	IAngelscriptCacheRestoreFaultInjector* CacheRestoreFaultInjector = nullptr;
#endif

	static FAngelscriptEngineDependencies CreateDefault();
};

enum class ECompileType : uint8
{
	Initial,
	SoftReloadOnly,
	FullReload,
};

/**
 * Controls whether a compile request may consume persisted execution artifacts.
 * This is intentionally independent of ECompileType, which controls reload and
 * activation behavior rather than compiler-input authority.
 */
enum class EAngelscriptCompileCachePolicy : uint8
{
	Default,
	ForceClean,
};

struct FAngelscriptCompileOptions
{
	EAngelscriptCompileCachePolicy CachePolicy =
		EAngelscriptCompileCachePolicy::Default;

	// Packaged Runtime cannot safely expose a new AS module while retaining an
	// old Unreal class layout. Reject even "suggested" full reloads before swap.
	bool bRejectStructuralChanges = false;

	bool IsForcedClean() const
	{
		return CachePolicy == EAngelscriptCompileCachePolicy::ForceClean;
	}
};

enum class ECompileResult : uint8
{
	Error,
	ErrorNeedFullReload,
	PartiallyHandled,
	FullyHandled,
};

USTRUCT()
struct ANGELSCRIPTRUNTIME_API FAngelscriptEngine
{
	GENERATED_BODY()

	FAngelscriptEngine();
	explicit FAngelscriptEngine(const FAngelscriptEngineConfig& InConfig, const FAngelscriptEngineDependencies& InDependencies);
	~FAngelscriptEngine();

	static TUniquePtr<FAngelscriptEngine> Create(const FAngelscriptEngineConfig& InConfig, const FAngelscriptEngineDependencies& InDependencies);
	static FAngelscriptEngine* TryGetCurrentEngine();

	/**
	 * Resolve the Runtime owner of one exact AngelScript engine. This lookup is
	 * Engine-local and does not consult the ambient/current Engine context.
	 */
	static FAngelscriptEngine* TryGetByScriptEngine(
		const asIScriptEngine* ScriptEngine);
	static FAngelscriptEngine& Get();
	static bool IsInitialized();
	static FString GetScriptRootDirectory();
	static UPackage* GetPackage();
	static UObject* TryGetCurrentWorldContextObject();
	static UObject* GetAmbientWorldContext();
	static bool ShouldUseEditorScriptsForCurrentContext();
	static bool ShouldUseAutomaticImportMethodForCurrentContext();

	// True once an owned AngelScript engine has been released during process exit.
	// After this point the UObject system may still purge script-backed structs whose
	// owning asITypeInfo/engine is gone; callers (e.g. UASStruct destruction) must skip
	// running script destructors to avoid dereferencing a freed engine.
	static bool AreEnginesReleasedForExit();
	static class asCThreadLocalData* GameThreadTLD;

	// Hook accessors. Previously these lived on a separate hooks
	// container exposed via an accessor; inlined here because the
	// container had no consumers beyond that accessor and its only
	// effect was forcing every subscriber through a redundant indirection.
	//
	// Subscription pattern: most callers should still use the
	// IAngelscriptExtension / FAngelscriptEngineExtensionRegistry pattern so
	// that subscriptions attach/detach with engine lifetime; direct AddLambda
	// calls remain valid for short-lived test fixtures.

	FAngelscriptGetDynamicSpawnLevel& GetDynamicSpawnLevel() { return DynamicSpawnLevel; }
	const FAngelscriptGetDynamicSpawnLevel& GetDynamicSpawnLevel() const { return DynamicSpawnLevel; }

	FAngelscriptDebugCheckBreakOptions& GetDebugCheckBreakOptions() { return DebugCheckBreakOptions; }
	const FAngelscriptDebugCheckBreakOptions& GetDebugCheckBreakOptions() const { return DebugCheckBreakOptions; }

	FAngelscriptGetDebugBreakFilters& GetDebugBreakFilters() { return DebugBreakFilters; }
	const FAngelscriptGetDebugBreakFilters& GetDebugBreakFilters() const { return DebugBreakFilters; }

	FAngelscriptDebugObjectSuffix& GetDebugObjectSuffix() { return DebugObjectSuffix; }
	const FAngelscriptDebugObjectSuffix& GetDebugObjectSuffix() const { return DebugObjectSuffix; }

	FAngelscriptComponentCreated& GetComponentCreated() { return ComponentCreated; }
	const FAngelscriptComponentCreated& GetComponentCreated() const { return ComponentCreated; }

	FAngelscriptCompilationDelegate& GetPreCompile() { return PreCompile; }
	const FAngelscriptCompilationDelegate& GetPreCompile() const { return PreCompile; }

	FAngelscriptCompilationDelegate& GetPostCompile() { return PostCompile; }
	const FAngelscriptCompilationDelegate& GetPostCompile() const { return PostCompile; }

	FAngelscriptCompilationDelegate& GetOnInitialCompileFinished() { return OnInitialCompileFinished; }
	const FAngelscriptCompilationDelegate& GetOnInitialCompileFinished() const { return OnInitialCompileFinished; }

	FAngelscriptClassAnalyzeDelegate& GetClassAnalyze() { return ClassAnalyze; }
	const FAngelscriptClassAnalyzeDelegate& GetClassAnalyze() const { return ClassAnalyze; }

	FAngelscriptPreGenerateClasses& GetPreGenerateClasses() { return PreGenerateClasses; }
	const FAngelscriptPreGenerateClasses& GetPreGenerateClasses() const { return PreGenerateClasses; }

	FAngelscriptPostCompileClassCollection& GetPostCompileClassCollection() { return PostCompileClassCollection; }
	const FAngelscriptPostCompileClassCollection& GetPostCompileClassCollection() const { return PostCompileClassCollection; }

	FAngelscriptLiteralAssetCreated& GetOnLiteralAssetCreated() { return OnLiteralAssetCreated; }
	const FAngelscriptLiteralAssetCreated& GetOnLiteralAssetCreated() const { return OnLiteralAssetCreated; }

	FAngelscriptLiteralAssetCreated& GetPostLiteralAssetSetup() { return PostLiteralAssetSetup; }
	const FAngelscriptLiteralAssetCreated& GetPostLiteralAssetSetup() const { return PostLiteralAssetSetup; }

	FOnAngelscriptClassReload& GetOnClassReload() { return OnClassReload; }
	const FOnAngelscriptClassReload& GetOnClassReload() const { return OnClassReload; }

	FOnAngelscriptEnumCreated& GetOnEnumCreated() { return OnEnumCreated; }
	const FOnAngelscriptEnumCreated& GetOnEnumCreated() const { return OnEnumCreated; }

	FOnAngelscriptEnumChanged& GetOnEnumChanged() { return OnEnumChanged; }
	const FOnAngelscriptEnumChanged& GetOnEnumChanged() const { return OnEnumChanged; }

	FOnAngelscriptStructReload& GetOnStructReload() { return OnStructReload; }
	const FOnAngelscriptStructReload& GetOnStructReload() const { return OnStructReload; }

	FOnAngelscriptDelegateReload& GetOnDelegateReload() { return OnDelegateReload; }
	const FOnAngelscriptDelegateReload& GetOnDelegateReload() const { return OnDelegateReload; }

	FOnAngelscriptFullReload& GetOnFullReload() { return OnFullReload; }
	const FOnAngelscriptFullReload& GetOnFullReload() const { return OnFullReload; }

	FOnAngelscriptPostReload& GetOnPostReload() { return OnPostReload; }
	const FOnAngelscriptPostReload& GetOnPostReload() const { return OnPostReload; }

	FOnAngelscriptLiteralAssetReload& GetOnLiteralAssetReload() { return OnLiteralAssetReload; }
	const FOnAngelscriptLiteralAssetReload& GetOnLiteralAssetReload() const { return OnLiteralAssetReload; }

	bool bSimulateCooked = false;
	bool bTestErrors = false;
	bool bIsHotReloading = false;
	int32 HotReloadApplicationFreezeCount = 0;
	bool bForcePreprocessEditorCode = false;
	bool bUseEditorScripts = false;
	bool bUseAutomaticImportMethod = false;
	bool bCollectStaticJITCompatibilityBinds = false;
	bool bOverrideTargetPreprocessorFlags = false;
	bool bTargetEditor = false;
	bool bTargetEditorOnlyData = false;
	bool bTargetRelease = false;
	bool bTargetTest = false;

	static bool IsSimulatingCookedForCurrentContext();
	static bool IsTestingErrorsForCurrentContext();

#if WITH_DEV_AUTOMATION_TESTS
	void EnsureScriptTestHotReloadRunnerForTesting();
	bool ShouldInjectDirectBindFailureForTesting() const
	{
		return RuntimeConfig.bInjectDirectBindFailureForTesting;
	}

	FAngelscriptScriptTestHotReloadRunner*
		GetScriptTestHotReloadRunnerForTesting() const
	{
		return ScriptTestHotReloadRunner;
	}
#endif
	static bool IsHotReloadingForCurrentContext();
	static bool IsForcingPreprocessEditorCodeForCurrentContext();
	static bool IsScriptDevelopmentModeForCurrentContext();

	bool Initialize();
	/* Initialize bindings and runtime services without scanning Script roots or compiling disk scripts. */
	bool InitializeWithoutInitialCompile();
	bool IsReadyForPublication() const { return bReadyForPublication; }
	void Shutdown();
	FInterfaceMethodSignature* RegisterInterfaceMethodSignature(FName FunctionName);
	void ReleaseInterfaceMethodSignature(FInterfaceMethodSignature* Signature);
	TArray<FString> DiscoverScriptRoots(bool bOnlyProjectRoot = false) const;
	TArray<FAngelscriptSourceRoot> DiscoverScriptRootDescriptors(bool bOnlyProjectRoot = false) const;
	TArray<FAngelscriptSourceRoot> GetEffectiveScriptRootDescriptors() const;

	/** Discard a named script module from the engine. Returns true if the module was found and discarded. */
	bool DiscardModule(const TCHAR* ModuleName);

	/* Initially bind all engine types to angelscript. */
	bool BindScriptTypes();

	/* Initially compile all script files in global folders. */
	void InitialCompile();

	/* Add the listed set of modules into the angelscript engine. 
	 * Modules should already be pre-processed.
	 * Modules array should already be sorted in dependency order. */
	ECompileResult CompileModules(
		ECompileType CompileType,
		const TArray<TSharedRef<struct FAngelscriptModuleDesc>>& Modules,
		TArray<TSharedRef<struct FAngelscriptModuleDesc>>& OutCompiledModules,
		FAngelscriptCompileOptions CompileOptions = {},
		const FAngelscriptCacheCompileCaptureContext* CacheCaptureContext = nullptr,
		FAngelscriptCacheCompileReuseContext* CacheReuseContext = nullptr);
	void CompileModule_Types_Stage1(
		ECompileType CompileType,
		TSharedRef<struct FAngelscriptModuleDesc> Module,
		const TArray<TSharedRef<struct FAngelscriptModuleDesc>>& ImportedModules,
		const FAngelscriptCompileOptions& CompileOptions);
	void CompileModule_Functions_Stage2(ECompileType CompileType, TSharedRef<struct FAngelscriptModuleDesc> Module);
	void CompileModule_Code_Stage3(ECompileType CompileType, TSharedRef<struct FAngelscriptModuleDesc> Module);
	void CompileModule_Globals_Stage4(ECompileType CompileType, TSharedRef<struct FAngelscriptModuleDesc> Module);

	/* Perform a hot reload of the specified type if necessary. */
	void CheckForHotReload(ECompileType CompileType);

	/** Queue a loose-source reload for a non-editor Runtime engine. */
	EAngelscriptRuntimeReloadRequestStatus RequestPackagedRuntimeReload();

	/** Consume the most recent completed request exactly once. */
	bool ConsumePackagedRuntimeReloadResult(
		FAngelscriptRuntimeReloadResult& OutResult);

	/** Recompile selected live modules through the authoritative forced-clean
	 *  hot-reload transaction. Empty selection means all active modules. */
	bool ForceCleanCacheModules(
		TConstArrayView<FString> CanonicalModuleNames,
		ECompileResult& OutCompileResult);

	/* Verify Unreal Property specifiers in a module. */
	bool VerifyPropertySpecifiers(const TArray<TSharedRef<struct FAngelscriptModuleDesc>>& Modules);

	bool VerifyRepFunc(FString* FuncDesc, const TSharedRef<struct FAngelscriptPropertyDesc>& Property,
		const TSharedRef<struct FAngelscriptClassDesc>& Class,
		const TSharedRef<struct FAngelscriptModuleDesc>& Module);




	void Tick(float DeltaTime);
	bool ShouldTick() const;

	/* Functions can have user data specified at bind-time that can be looked up here. */
	template<typename T>
	static T* GetCurrentFunctionUserData()
	{
		return (T*)GetCurrentFunctionUserDataPtr();
	}

	static void* GetCurrentFunctionUserDataPtr();
	static asITypeInfo* GetCurrentFunctionObjectType();
	static class asCContext* GetCurrentScriptContext();
	static class asCContext* GetPreviousScriptContext();

	/* Outdated flag marking for modules and functions. */
	bool IsOutdated(asIScriptFunction* Function);

	/* Converting from angelscript-visible pointers to uobject pointers. */
	static FORCEINLINE UObject* AngelscriptToUObject(asIScriptObject* Object)
	{
		return (UObject*)Object;
	}

	static FORCEINLINE asIScriptObject* UObjectToAngelscript(UObject* Object)
	{
		return (asIScriptObject*)Object;
	}

	static bool CanCastScriptObjectToUnrealInterface(asITypeInfo* RuntimeType, asITypeInfo* TargetType, void* ObjectPtr);

	static FORCEINLINE bool CanUseGameThreadData()
	{
		if (FAngelscriptEngine* CurrentEngine = TryGetCurrentEngine())
		{
			return IsInGameThread() || !CurrentEngine->bIsInitialCompileFinished;
		}

		return IsInGameThread();
	}

	/* Throw an exception to the angelscript VM. */
	static void Throw(const ANSICHAR* Exception);

	/* Show an error with a script stack trace, but continue the script flow without throwing an exception. */
	static void TraceError(const ANSICHAR* Error);

	/* Get string representations of all levels of the current angelscript callstack. */
	static TArray<FString> GetAngelscriptCallstack();

	/* Get a string representation of the current angelscript callstack. */
	static FString FormatAngelscriptCallstack();

	/* Get a string representation of the current location of angelscript execution (ie the top most stack frame's position). */
	static FString GetAngelscriptExecutionPosition();

	/* Get the file and line number of the current location of angelscript execution (ie the top most stack frame's position). */
	static void GetAngelscriptExecutionFileAndLine(FString& OutFilename, int& OutLineNumber);

	/* Get the UObject that the current angelscript callstack is operating on. */
	static UObject* GetAngelscriptExecutionThisObject(int32 StackFrame = 0);

	/* If the angelscript debugger is attached, do an angelscript breakpoint. Returns whether we broke in AS debugging. */
	static bool TryBreakpointAngelscriptDebugging(const TCHAR* Message = nullptr);
	UObject* GetCurrentWorldContextObject() const { return WorldContextObject; }
	bool ShouldUseEditorScripts() const { return bUseEditorScripts; }
	bool ShouldUseAutomaticImportMethod() const { return bUseAutomaticImportMethod; }

	/* Checks if the character is a valid alphanumeric character or an underscore. */
	FORCEINLINE static bool IsValidIdentifierCharacter(TCHAR Character)
	{
		return (Character >= 'A' && Character <= 'Z')
				|| (Character >= 'a' && Character <= 'z')
				|| (Character >= '0' && Character <= '9')
				|| Character == '_';
	}

	/* The root angelscript UPackage everything should belong to. */
	UPROPERTY()
	UPackage* AngelscriptPackage = nullptr;
	/* The package that all literal assets are put into. */
	UPROPERTY()
	UPackage* AssetsPackage = nullptr;

	/* Root paths where all scripts are loaded from. */
	TArray<FString> AllRootPaths;
	TArray<FAngelscriptSourceRoot> AllScriptRoots;

	/* Internal script data. */
	class asCScriptEngine* Engine = nullptr;

	asIScriptEngine* GetScriptEngine() const
	{
		return (asIScriptEngine*)Engine;
	}

	// 0 = asAST_DISCARD_AFTER_CODEGEN, 1 = asAST_RETAIN_SNAPSHOT.
	// Applied to the next compiled asCModule (including Hot Reload replacements).
	int32 NextCompileASTRetentionPolicy = 0;

#if AS_CAN_GENERATE_JIT
	FAngelscriptJITCoordinator* GetJITCoordinator() const
	{
		return JITCoordinator;
	}
#endif

	UPackage* GetPackageInstance() const
	{
		return AngelscriptPackage;
	}

	TSharedPtr<struct FAngelscriptModuleDesc> GetModule(const FString& ModuleName)
	{
		auto* ModRef = ActiveModules.Find(MakeModuleName(ModuleName));
		if (ModRef == nullptr)
			return nullptr;
		else
			return *ModRef;
	}

	TArray<TSharedRef<struct FAngelscriptModuleDesc>> GetActiveModules() const
	{
		TArray<TSharedRef<struct FAngelscriptModuleDesc>> Result;
		for (auto It : ActiveModules)
		{
			Result.Add(It.Value);
		}
		return Result;
	}

	TSharedPtr<struct FAngelscriptModuleDesc> GetModule(asIScriptModule* Module);
	TSharedPtr<struct FAngelscriptModuleDesc> GetModuleByFilename(const FString& Filename);

	TSharedPtr<struct FAngelscriptClassDesc> GetClass(const FString& ClassName, TSharedPtr<struct FAngelscriptModuleDesc>* FoundInModule = nullptr);
	TSharedPtr<struct FAngelscriptEnumDesc> GetEnum(const FString& EnumName, TSharedPtr<struct FAngelscriptModuleDesc>* FoundInModule = nullptr);
	TSharedPtr<struct FAngelscriptDelegateDesc> GetDelegate(const FString& DelegateName, TSharedPtr<struct FAngelscriptModuleDesc>* FoundInModule = nullptr);

	TSharedPtr<struct FAngelscriptModuleDesc> GetModuleByModuleName(const FString& ModuleName);

	TSharedPtr<struct FAngelscriptModuleDesc> GetModuleByFilenameOrModuleName(const FString& Filename, const FString& ModuleName);

	/** Resolve a stable key only through this Engine's neutral live route table. */
	bool ResolveFunctionRoute(
		const FAngelscriptStableFunctionKey& FunctionKey,
		FAngelscriptFunctionRoute& OutRoute) const;

	/** Resolve/build the stable key for one exact live function in this Engine. */
	bool TryResolveStableFunctionKey(
		const asIScriptFunction* Function,
		FAngelscriptStableFunctionKey& OutFunctionKey);

	/** Capture the current immutable per-Engine StableFunctionKey route map. */
	TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
		ESPMode::ThreadSafe> GetFunctionRouteSnapshot() const;

	/**
	 * Republish the transient route snapshot after StaticJIT has already
	 * validated and applied its selected live entries at an Engine safe point.
	 * This is an isolation/compatibility seam, not a Provider matcher: false
	 * retains the exact prior route publication, and neither outcome mutates
	 * Cache lifecycle publications or persisted generations.
	 */
	bool RefreshFunctionRouteSnapshotAfterStaticJITChange(
		bool bValidatedProviderStateWasAppliedAtSafePoint);

	/** The sole Cache V2 lifecycle/mutation owner for this Engine. */
	FAngelscriptCacheService* GetCacheService() const
	{
		return CacheService.Get();
	}

	/** Frozen per-Engine Cache V2 enablement resolved at construction. */
	bool IsCacheV2Enabled() const
	{
		return bCacheV2Enabled;
	}

	// Captured diagnostic messages during compilation
	struct FDiagnostic
	{
		FString Message;
		int32 Row;
		int32 Column;
		bool bIsError;
		bool bIsInfo;
	};

	void ScriptCompileError(const FString& AbsoluteFilename, const FDiagnostic& Diagnostic);
	void ScriptCompileError(TSharedPtr<FAngelscriptModuleDesc> Module, int32 LineNumber, const FString& Message, bool bIsError = true);
	void ScriptCompileError(UClass* InsideClass, const FString& FunctionName, const FString& Message, bool bIsError = true);

	UStruct* GetUnrealStructFromAngelscriptTypeId(int TypeId);

	// Can be filled by the game module to provide additional compile checks in editor
	// depending on what code class is being compiled.
	TMap<UClass*, TSharedPtr<FAngelscriptAdditionalCompileChecks>> AdditionalCompileChecks;

	struct FFilenamePair
	{
		FString AbsolutePath;
		FString RelativePath;
		FString VirtualPath;
	};

private:
	FString MakeModuleName(const FString& ModuleName) const;
	bool ShouldInitializeThreaded();
	void AcquireProcessPackages();
	void ReleaseProcessPackages();
	#if WITH_DEV_AUTOMATION_TESTS
	#endif
	void PreInitialize_GameThread();
	bool Initialize_AnyThread();
	void PostInitialize_GameThread();

	void SetOutdated(asIScriptModule* OldModule);

	/* Internal state of active modules. */
	TMap<FString, TSharedRef<struct FAngelscriptModuleDesc>> ActiveModules;
	TMap<asIScriptModule*, TSharedPtr<struct FAngelscriptModuleDesc>> ModulesByScriptModule;

#if AS_CAN_HOTRELOAD
	TMap<FStringView, TPair<TSharedPtr<struct FAngelscriptModuleDesc>, TSharedPtr<struct FAngelscriptClassDesc>>> ActiveClassesByName;
	TMap<FStringView, TPair<TSharedPtr<struct FAngelscriptModuleDesc>, TSharedPtr<struct FAngelscriptEnumDesc>>> ActiveEnumsByName;
	TMap<FStringView, TPair<TSharedPtr<struct FAngelscriptModuleDesc>, TSharedPtr<struct FAngelscriptDelegateDesc>>> ActiveDelegatesByName;
#endif

	/* Global context pool of contexts that don't belong to a thread right now. */
	friend struct FAngelscriptPooledContextBase;
	friend struct FAngelscriptContextPool;
	TArray<asCContext*> GlobalContextPool;
	FCriticalSection GlobalContextPoolLock;

	/* Hot reload watch state, maps script files to required data for detecting reloads. */
	struct FHotReloadState
	{
		FDateTime LastChange;
		uint64 ContentHash = 0;
		bool bHasContentHash = false;
		FFilenamePair Filename;
	};

	bool bUseHotReloadCheckerThread = false;

	TMap<FString, FHotReloadState> FileHotReloadState;

	volatile bool bWaitingForHotReloadResults = false;

	/* Asynchronous reflected script-test work selected after hot reload. */
	FAngelscriptScriptTestHotReloadRunner*
		ScriptTestHotReloadRunner = nullptr;

	/* Files that we tried to reload before, but failed to compile, that we should retry later. */
	TSet<FFilenamePair> PreviouslyFailedReloadFiles;

	/* Files that we soft reloaded but that we need to do a full reload on once we are able. */
	TSet<FFilenamePair> QueuedFullReloadFiles;
	TArray<TUniquePtr<FInterfaceMethodSignature>> InterfaceMethodSignatures;

	double NextHotReloadCheck = -1.0;

	void DiscoverTests();
	bool PerformHotReload(
		ECompileType CompileType,
		const TArray<FFilenamePair>& FileList,
		ECompileResult* OutCompileResult = nullptr,
		bool bRejectStructuralChanges = false);
	bool ProcessQueuedHotReload(
		ECompileType CompileType,
		ECompileResult* OutCompileResult,
		TArray<FFilenamePair>* OutConsumedFiles,
		bool bRejectStructuralChanges = false);
	void CheckForFileChanges();
	FString MakeSourceStateKey(const FFilenamePair& Filename) const;
	void PrimePackagedRuntimeReloadState();
	void TickPackagedRuntimeReload();
	void CollectChangedModuleNames(
		const TArray<FFilenamePair>& Files,
		TArray<FString>& OutModuleNames) const;

	EAngelscriptPackagedRuntimeReloadMode PackagedRuntimeReloadMode =
		EAngelscriptPackagedRuntimeReloadMode::Disabled;
	float PackagedRuntimeReloadScanIntervalSeconds = 1.0f;
	bool bPackagedRuntimeReloadPrimed = false;
	bool bPackagedRuntimeReloadQueued = false;
	double NextPackagedRuntimeReloadScan = -1.0;
	TOptional<FAngelscriptRuntimeReloadResult>
		CompletedPackagedRuntimeReloadResult;

	void ImportIntoModule(class asIScriptModule* IntoModule, class asIScriptModule* FromModule);

	bool CheckFunctionImportsForNewModules(const TArray<TSharedRef<struct FAngelscriptModuleDesc>>& Modules);
	void UpdateScriptReferencesInUnrealData(struct asModuleReferenceUpdateMap& UpdateMap, TSharedRef<FAngelscriptModuleDesc> Module);

	void ResolveAllDeclaredImports();
	void ResolveDeclaredImports(class asIScriptModule* Module);
	void RebuildFunctionRouteSnapshot(
		TConstArrayView<FAngelscriptFunctionRoute>
			VerifiedArtifactRoutes = {},
		TConstArrayView<FAngelscriptFunctionArtifactIdentity>
			ValidatedArtifactIdentities = {},
		TConstArrayView<asIScriptModule*> RebuiltModules = {},
		TConstArrayView<asIScriptModule*> ArtifactInvalidatedModules = {},
		TConstArrayView<FAngelscriptFunctionArtifactReferenceSet>
			ValidatedArtifactReferenceSets = {});

#if WITH_EDITOR
	void CheckUsageRestrictions(const TArray<TSharedRef<struct FAngelscriptModuleDesc>>& Modules);
#endif

	void SwapInModules(const TArray<TSharedRef<struct FAngelscriptModuleDesc>>& Modules, TArray<TSharedRef<struct FAngelscriptModuleDesc>>& DiscardedModules);

#if !UE_BUILD_SHIPPING
	void GetOnScreenMessages(TMultiMap<FCoreDelegates::EOnScreenMessageSeverity, FText>& OutMessages);
	FDelegateHandle OnScreenMessagesHandle;
#endif

	// Counter for temporary generated module names
	int32 TempNameIndex = 0;

	// Owned engine state. The engine is the sole owner; each TUniquePtr is
	// empty before Initialize*() runs (Get() returns nullptr) and is
	// MakeUnique-d during initialization. Teardown releases these in
	// Shutdown().
	TUniquePtr<FAngelscriptTypeDatabase> TypeDatabase;
	TUniquePtr<FAngelscriptBindState> BindState;
	TUniquePtr<TArray<FToStringType>> ToStringList;
	TUniquePtr<FAngelscriptBindDatabase> BindDatabase;
	TUniquePtr<FBlueprintEventSignatureRegistry> BlueprintEventSignatureRegistry;
	TUniquePtr<FAngelscriptDocumentationState> DocumentationState;
#if AS_CAN_GENERATE_JIT
	TUniquePtr<FAngelscriptNativeFormState> NativeFormState;
#endif
	TUniquePtr<FAngelscriptFunctionRouteState> FunctionRouteState;
	TUniquePtr<FAngelscriptCacheService> CacheService;
	bool bCacheV2Enabled = false;
	TArray<FName> StaticNames;
	TMap<FName, int32> StaticNamesByIndex;

	// Per-engine lookup from script-defined UEnum name to its bound asITypeInfo*.
	// Populated during Bind_Enums, consumed by property-type resolution paths in
	// Bind_UEnum. Engine-owned so that two engines bound concurrently never see
	// each other's `asITypeInfo*` for same-named script enums.
	TMap<FName, class asITypeInfo*> ScriptEnumTypeLookupByName;

	TSharedPtr<FAngelscriptEngineLifetimeToken> LifetimeToken;
	bool bHoldsProcessPackageReference = false;
	bool bReadyForPublication = false;
	bool bExtensionsAttached = false;
	UPROPERTY()
	UObject* WorldContextObject = nullptr;

	friend class UAngelscriptTestCommandlet;

	static FAngelscriptEngine* TryGetGlobalEngine();
	static void SetGlobalEngine(FAngelscriptEngine* InEngine);
	static FAngelscriptEngine& GetOrCreate();
	static bool DestroyGlobal();
	friend class FAngelscriptRuntimeModule;
	friend struct FAngelscriptBindConfigTestAccess;
	friend struct FAngelscriptDependencyInjectionTestAccess;
	friend struct FAngelscriptEngineIsolationTestAccess;
	friend struct FAngelscriptMultiEngineTestAccess;
	friend struct FAngelscriptSubsystemOwnershipTestAccess;
	friend struct FAngelscriptTickBehaviorTestAccess;
	friend struct FAngelscriptHotReloadTestAccess;
	friend struct FAngelscriptCacheChangedModuleTestAccess;
	friend struct FAngelscriptEngineScope;
	friend class FAngelscriptCacheModuleRestorer;
	friend class FAngelscriptJITProviderRouter;
	friend struct FAngelscriptTestEngineScopeAccess;
	friend struct FStaticJITDiagnostics;
	friend struct FAngelscriptStateSnapshotBuilder;
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FAngelscriptInterfaceSignatureTestAccess;
#endif

public:
	void PushHotReloadApplicationFreeze();
	void PopHotReloadApplicationFreeze();
	bool IsHotReloadApplicationFrozen() const;
	bool TryProcessQueuedHotReload();

	TArray<FFilenamePair> FileChangesDetectedForReload;
	TArray<FFilenamePair> FileDeletionsDetectedForReload;
	double LastFileChangeDetectedTime = -1.0;

	bool bDidInitialCompileSucceed = true;
	bool bIsInitialCompileFinished = false;
	
	bool bCompletedAssetScan = false;

	bool IsInitialCompileFinished() const
	{
		return bIsInitialCompileFinished;
	}

	bool DidInitialCompileSucceed() const
	{
		return bDidInitialCompileSucceed;
	}

	struct FAngelscriptDebugFrame
	{
		const char* File = nullptr;
		const char* Function = nullptr;
		const char* Class = nullptr;
		int32 LineNumber = -1;
		UObject* This = nullptr;
		struct FDebugValues* Variables = nullptr;
		struct FDebugValuePrototype* Prototype = nullptr;
		asIScriptFunction* ScriptFunction = nullptr;

		~FAngelscriptDebugFrame();
	};

	struct FAngelscriptDebugStack
	{
		TArray<FAngelscriptDebugFrame> Frames;
	};

	struct FDiagnostics
	{
		FString Filename;
		TArray<FDiagnostic> Diagnostics;
		bool bHasEmittedAny = false;
		bool bIsCompiling = false;
	};

#if WITH_AS_DEBUGSERVER
	class FAngelscriptDebugServer* DebugServer = nullptr;
	bool IsEvaluatingDebuggerWatch();
#endif

	FCriticalSection CompilationLock;

	TMap<FString, FDiagnostics> Diagnostics;
	TMap<FString, FDiagnostics> LastEmittedDiagnostics;

	bool bDiagnosticsDirty = false;
	bool bIgnoreCompileErrorDiagnostics = false;

	FString FormatDiagnostics();
	void ResetDiagnostics();
	void EmitDiagnostics(class FSocket* Client = nullptr);
	void EmitDiagnostics(FDiagnostics& Diag, class FSocket* Client = nullptr);

	void FindAllScriptFilenames(TArray<FFilenamePair>& OutFilenames);
	void FindAllScriptSources(TArray<FAngelscriptSource>& OutSources);

	bool HasAnyDebugServerClients();
	void ReplaceScriptAssetContent(FString AssetName, TArray<FString> AssetContent);

	friend struct FAngelscriptPrecompiledData;
	class FAngelscriptJITCoordinator* JITCoordinator = nullptr;
	bool bScriptDevelopmentMode = false;

	static bool IsCollectingStaticJITCompatibilityBinds();

	// Argument type specializations that were bound by Bind_BlueprintEvent.cpp,
	// the preprocessor uses this list to look up what push argument function to call
	TSet<FString> BoundBlueprintEventArgumentSpecializations;

	void StartHotReloadThread();
	bool bHotReloadThreadStarted = false;

	UPROPERTY()
	class UAngelscriptSettings* ConfigSettings = nullptr;

	static void HandleExceptionFromJIT(const ANSICHAR* ExceptionString);
	asCContext* CreateContext();
	void UpdateLineCallbackState();
	EAngelscriptJITExecutionCapability CaptureCurrentJITExecutionRequirements();
	static bool AllowsJITBindingExecution(
		asIScriptEngine* ScriptEngine,
		const asSJITFunctionBinding& Binding);

	FAngelscriptTypeDatabase* GetTypeDatabase() const;
	FAngelscriptBindState* GetBindState() const;
	TArray<FToStringType>* GetToStringList() const;
	FAngelscriptBindDatabase* GetBindDatabase() const;
	FBlueprintEventSignatureRegistry* GetBlueprintEventSignatureRegistry() const;
	FAngelscriptDocumentationState* GetDocumentationState() const;
#if AS_CAN_GENERATE_JIT
	FAngelscriptNativeFormState* GetNativeFormState() const;
#endif

	TMap<FName, class asITypeInfo*>& GetScriptEnumTypeLookup() { return ScriptEnumTypeLookupByName; }
	const TMap<FName, class asITypeInfo*>& GetScriptEnumTypeLookup() const { return ScriptEnumTypeLookupByName; }

#if WITH_DEV_AUTOMATION_TESTS
	int32 GetToStringEntryCountForTesting() const;
	FAngelscriptBindDatabase& GetBindDatabaseForTesting() const;
	static int32 GetLocalPooledContextCountForTesting(asIScriptEngine* ScriptEngine);
	static FAngelscriptProcessPackageReferenceCounts
	GetProcessPackageReferenceCountsForTesting();
	bool HasDebugServerForTesting() const
	{
#if WITH_AS_DEBUGSERVER
		return DebugServer != nullptr;
#else
		return false;
#endif
	}
	bool HasAttachedExtensionsForTesting() const
	{
		return bExtensionsAttached;
	}
	bool HasHotReloadThreadStartedForTesting() const
	{
		return bHotReloadThreadStarted;
	}
	void SetUseEditorScriptsForTesting(bool bEnabled);
	void SetAutomaticImportMethodForTesting(bool bEnabled);
	void SetBlueprintLibraryNamespaceSettingsForTesting(bool bUseScriptName, TArray<FString> PrefixesToStrip, TArray<FString> SuffixesToStrip);
#endif

	static const FName& GetStaticName(int32 Index);
	static FName ResolveStaticName(int32 Index, const FString& CanonicalName);
	static bool TryGetStaticName(int32 Index, FName& OutName);
	static int32 GetOrAddStaticName(FName Name);
	static int32 GetStaticNameCount();
	static void ReserveStaticNames(int32 Count);
	static void ResetStaticNames();
	static void AddStaticNameFromPrecompiled(FName Name);
	static const TArray<FName>& GetStaticNames();

	static void AssignWorldContext(UObject* NewWorldContext);

	static void FindScriptFiles(
		IFileManager& FileManager,
		const FString& RelativeRoot,
		const FString& SearchDirectory,
		const TCHAR* Pattern,
		TArray<FFilenamePair>& OutFilenames,
		bool bSkipDevelopmentScripts,
		bool bSkipEditorScripts);

	static TArray<FString> MakeAllScriptRoots(bool bOnlyProjectRoot = false);

	const FAngelscriptEngineConfig& GetRuntimeConfig() const
	{
		return RuntimeConfig;
	}

	EAngelscriptEnginePurpose GetPurpose() const
	{
		return RuntimeConfig.Purpose;
	}

#if AS_CAN_GENERATE_JIT
	TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
		ESPMode::ThreadSafe> GetStaticJITGenerationSnapshot() const
	{
		return StaticJITGenerationSnapshot;
	}

	uint32 GetStaticJITGenerationCompilePassCount() const
	{
		return StaticJITGenerationCompilePassCount;
	}
#endif

	// Runtime-owned source authority used by Cache V2 startup discovery and
	// read-only diagnostics. Callers may observe through the interface but do not
	// own or replace the configured provider.
	IAngelscriptSourceProvider* GetSourceProvider() const
	{
		return Dependencies.SourceProvider.Get();
	}

#if WITH_ANGELSCRIPT_UNITTESTS
	IAngelscriptCacheRestoreFaultInjector*
	GetCacheRestoreFaultInjectorForTests() const
	{
		return Dependencies.CacheRestoreFaultInjector;
	}

	static int32 GetStaticJITGenerationEngineCreateCountForTests();
	static int32 GetStaticJITGenerationEngineLiveCountForTests();
	static int32 GetStaticJITGenerationEnginePeakLiveCountForTests();
	static void ResetStaticJITGenerationEngineCreateCountForTests();
#endif

private:
	void WriteRequestedCacheV2ProcessReport() const;
	FAngelscriptEngineConfig RuntimeConfig;
	FAngelscriptEngineDependencies Dependencies;
#if AS_CAN_GENERATE_JIT
	TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
		ESPMode::ThreadSafe> StaticJITGenerationSnapshot;
	uint32 StaticJITGenerationCompilePassCount = 0;
#endif
	bool bCacheV2ShutdownFlushAttempted = false;

	// Hook delegate fields. Field order/grouping kept identical to the
	// previous hooks container for git-blame and diff
	// readability. Categories: runtime helpers, compile lifecycle, class
	// generation, asset lifecycle, reload lifecycle.
	FAngelscriptGetDynamicSpawnLevel DynamicSpawnLevel;
	FAngelscriptDebugCheckBreakOptions DebugCheckBreakOptions;
	FAngelscriptGetDebugBreakFilters DebugBreakFilters;
	FAngelscriptDebugObjectSuffix DebugObjectSuffix;
	FAngelscriptComponentCreated ComponentCreated;
	FAngelscriptCompilationDelegate PreCompile;
	FAngelscriptCompilationDelegate PostCompile;
	FAngelscriptCompilationDelegate OnInitialCompileFinished;
	FAngelscriptClassAnalyzeDelegate ClassAnalyze;
	FAngelscriptPreGenerateClasses PreGenerateClasses;
	FAngelscriptPostCompileClassCollection PostCompileClassCollection;
	FAngelscriptLiteralAssetCreated OnLiteralAssetCreated;
	FAngelscriptLiteralAssetCreated PostLiteralAssetSetup;

	FOnAngelscriptClassReload OnClassReload;
	FOnAngelscriptEnumCreated OnEnumCreated;
	FOnAngelscriptEnumChanged OnEnumChanged;
	FOnAngelscriptStructReload OnStructReload;
	FOnAngelscriptDelegateReload OnDelegateReload;
	FOnAngelscriptFullReload OnFullReload;
	FOnAngelscriptPostReload OnPostReload;
	FOnAngelscriptLiteralAssetReload OnLiteralAssetReload;
};

template<>
struct TStructOpsTypeTraits<FAngelscriptEngine> : public TStructOpsTypeTraitsBase2<FAngelscriptEngine>
{
	enum
	{
		WithCopy = false,
	};
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptEngineContextStack
{
	static void Push(FAngelscriptEngine* Engine);
	static void Pop(FAngelscriptEngine* Engine);
	static FAngelscriptEngine* Peek();
	static bool IsEmpty();

#if WITH_DEV_AUTOMATION_TESTS
	static TArray<FAngelscriptEngine*> SnapshotAndClear();
	static void RestoreSnapshot(TArray<FAngelscriptEngine*>&& SavedStack);
	static void PushEngineResolutionSuppressionForTesting();
	static void PopEngineResolutionSuppressionForTesting();
	static bool IsEngineResolutionSuppressedForTesting();
#endif
};

#if WITH_DEV_AUTOMATION_TESTS
/**
 * Lets automation tests exercise legacy no-current-engine branches without
 * mutating the editor-owned primary engine subsystem.
 */
struct ANGELSCRIPTRUNTIME_API FScopedAngelscriptEngineResolutionSuppressionForTesting
{
	FScopedAngelscriptEngineResolutionSuppressionForTesting();
	~FScopedAngelscriptEngineResolutionSuppressionForTesting();

	FScopedAngelscriptEngineResolutionSuppressionForTesting(const FScopedAngelscriptEngineResolutionSuppressionForTesting&) = delete;
	FScopedAngelscriptEngineResolutionSuppressionForTesting& operator=(const FScopedAngelscriptEngineResolutionSuppressionForTesting&) = delete;
};
#endif

struct ANGELSCRIPTRUNTIME_API FAngelscriptEngineScope
{
	explicit FAngelscriptEngineScope(FAngelscriptEngine& InEngine, UObject* InWorldContext = nullptr);
	~FAngelscriptEngineScope();

	FAngelscriptEngineScope(const FAngelscriptEngineScope&) = delete;
	FAngelscriptEngineScope& operator=(const FAngelscriptEngineScope&) = delete;
	FAngelscriptEngineScope(FAngelscriptEngineScope&& Other) noexcept;
	FAngelscriptEngineScope& operator=(FAngelscriptEngineScope&& Other) noexcept;

private:
	void Reset();

	FAngelscriptEngine* Engine = nullptr;
	UObject* PreviousWorldContext = nullptr;
	UObject* PreviousEngineWorldContext = nullptr;
	bool bChangedWorldContext = false;
};

struct FAngelscriptContextPool
{
	TArray<asCContext*> FreeContexts;

	~FAngelscriptContextPool();
};

extern thread_local FAngelscriptContextPool GAngelscriptContextPool;
extern FAngelscriptEngine::FAngelscriptDebugStack* GAngelscriptStack;

/* Automatically retrieves and manages an angelscript context for the scope of this struct. */
struct ANGELSCRIPTRUNTIME_API FAngelscriptPooledContextBase
{
	FAngelscriptPooledContextBase();
	explicit FAngelscriptPooledContextBase(class asIScriptEngine* DesiredScriptEngine);

	FORCEINLINE FAngelscriptPooledContextBase(class asCThreadLocalData* tld)
	{
		Init(tld, nullptr);
	}

	FORCEINLINE FAngelscriptPooledContextBase(class asCThreadLocalData* tld, class asIScriptEngine* DesiredScriptEngine)
	{
		Init(tld, DesiredScriptEngine);
	}

	~FAngelscriptPooledContextBase();

	void Init(class asCThreadLocalData* tld, class asIScriptEngine* DesiredScriptEngine);

	FAngelscriptPooledContextBase(FAngelscriptPooledContextBase&& Other);

	FAngelscriptPooledContextBase(FAngelscriptPooledContextBase& Other) = delete;
	void operator=(FAngelscriptPooledContextBase& Other) = delete;

	bool operator==(asCContext* Ptr) const { return Context == Ptr; }
	bool operator!=(asCContext* Ptr) const { return Context != Ptr; }

	void PrepareExternal(class asIScriptFunction* Function);
	void ExecuteExternal();

	FORCEINLINE asCContext* operator->() const
	{
		return Context;
	}

	operator asCContext*() const
	{
		return Context;
	}

private:
	asCContext* Context;
	bool bWasNested;
};

struct FAngelscriptGameThreadScopeWorldContext
{
	FAngelscriptGameThreadScopeWorldContext(UObject* WorldContext)
	{
		PreviousWorldContext = FAngelscriptEngine::GetAmbientWorldContext();
		FAngelscriptEngine::AssignWorldContext(WorldContext);
	}

	~FAngelscriptGameThreadScopeWorldContext()
	{
		FAngelscriptEngine::AssignWorldContext(PreviousWorldContext);
	}
private:
	UObject* PreviousWorldContext;
};

struct FAngelscriptContext : public FAngelscriptPooledContextBase
{
	FAngelscriptContext()
	{
		bChangedWorldContext = false;
	}

	explicit FAngelscriptContext(class asIScriptEngine* DesiredScriptEngine)
		: FAngelscriptPooledContextBase(DesiredScriptEngine)
	{
		bChangedWorldContext = false;
	}

	FAngelscriptContext(UObject* WorldContext)
		: FAngelscriptContext(WorldContext, nullptr)
	{
	}

	FAngelscriptContext(UObject* WorldContext, class asIScriptEngine* DesiredScriptEngine = nullptr);

	~FAngelscriptContext()
	{
		if (bChangedWorldContext)
		{
			FAngelscriptEngine::AssignWorldContext(PreviousWorldContext);
		}
	}
private:
	UObject* PreviousWorldContext;
	bool bChangedWorldContext;
};

struct FAngelscriptGameThreadContext : public FAngelscriptPooledContextBase
{
	FAngelscriptGameThreadContext(UObject* WorldContext, class asIScriptEngine* DesiredScriptEngine = nullptr);

	~FAngelscriptGameThreadContext()
	{
		FAngelscriptEngine::AssignWorldContext(PreviousWorldContext);
	}
private:
	UObject* PreviousWorldContext;
};

/**
 * Anything running within this scope is excluded from the angelscript loop detection timeout in editor.
 */
struct ANGELSCRIPTRUNTIME_API FAngelscriptExcludeScopeFromLoopTimeout
{
#if WITH_EDITOR
	class asCContext* Context;
	double StartTime;

	FAngelscriptExcludeScopeFromLoopTimeout();
	~FAngelscriptExcludeScopeFromLoopTimeout();
#else
	FORCEINLINE FAngelscriptExcludeScopeFromLoopTimeout() {}
	FORCEINLINE ~FAngelscriptExcludeScopeFromLoopTimeout() {}
#endif
};

/**
 * Description of a script property that has been added as an unreal property.
 */
/* Helper scope struct to print out performance information. */
struct ANGELSCRIPTRUNTIME_API FAngelscriptScopeTimer
{
#if AS_PRINT_STATS
	double StartTime;
	FString Name;
public:
	FAngelscriptScopeTimer(const TCHAR* Name);
	~FAngelscriptScopeTimer();

	static void OutputTime(const TCHAR* Name, double Time);
#else
	FORCEINLINE FAngelscriptScopeTimer(const TCHAR* Name) {}
	FORCEINLINE static void OutputTime(const TCHAR* Name, double Time) {}
#endif
};

struct FAngelscriptScopeTotalTimer
{
#if AS_PRINT_STATS && AS_PRECOMPILED_STATS
	double* Timer;
	double StartTime;
public:
	FAngelscriptScopeTotalTimer(double& TotalTime);
	~FAngelscriptScopeTotalTimer();
#else
	FORCEINLINE FAngelscriptScopeTotalTimer(double& TotalTime) {}
#endif
};

inline uint32 GetTypeHash(const FAngelscriptEngine::FFilenamePair& FilenamePair)
{
	return HashCombine(
		HashCombine(GetTypeHash(FilenamePair.AbsolutePath), GetTypeHash(FilenamePair.RelativePath)),
		GetTypeHash(FilenamePair.VirtualPath));
}

inline bool operator==(const FAngelscriptEngine::FFilenamePair& A, const FAngelscriptEngine::FFilenamePair& B)
{
	return A.AbsolutePath == B.AbsolutePath
		&& A.RelativePath == B.RelativePath
		&& A.VirtualPath == B.VirtualPath;
}
