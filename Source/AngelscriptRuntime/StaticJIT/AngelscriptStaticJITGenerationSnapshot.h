#pragma once

#include "CoreMinimal.h"

#include "Core/Artifacts/AngelscriptArtifactIdentity.h"
#include "Core/Artifacts/AngelscriptArtifactReference.h"
#include "Core/AngelscriptBinds.h"
#include "StaticJIT/AngelscriptStaticJITCaptureProfile.h"
#include "StaticJIT/AngelscriptStaticJITEntryPlan.h"
#include "StaticJIT/AngelscriptStaticJITFunctionMetadata.h"
#include "StaticJIT/AngelscriptStaticJITRuntimeTypeBinding.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallTarget.h"
#include "StaticJIT/StaticJITConfig.h"

#if AS_CAN_GENERATE_JIT

class asIScriptFunction;
class asIScriptModule;
class asIASTSnapshot;
class asITypeInfo;
class asCASTContext;
struct FAngelscriptCacheCleanModuleArtifacts;
struct FAngelscriptEngine;
struct FAngelscriptModuleDesc;

enum class EAngelscriptStaticJITGenerationDescriptorKind : uint8
{
	Class = 0,
	Struct,
	Enum,
	Delegate,
	Property,
	Function,
};

struct FAngelscriptStaticJITGenerationModule
{
	asIScriptModule* EngineLocalModule = nullptr;
	const FAngelscriptModuleDesc* EngineLocalDescriptor = nullptr;
	FAngelscriptStableModuleKey ModuleKey;
	FString CanonicalModuleName;
	FString VirtualSourcePath;
};

/**
 * Owns one public V1 AST lease captured for an immutable StaticJIT generation
 * snapshot. The caller transfers the single reference returned by
 * AcquireASTSnapshot(). The owning generation Engine must release that lease
 * before replacing its snapshot or shutting the script Engine down; external
 * snapshot owners may retain pointer-free authentication data afterwards, but
 * GetContext() then returns null. Function rows borrow only GetContext() from
 * this owner and are never valid beyond the generation Engine lifetime.
 */
class ANGELSCRIPTRUNTIME_API FAngelscriptStaticJITGenerationASTLease final
{
public:
	explicit FAngelscriptStaticJITGenerationASTLease(asIASTSnapshot* InSnapshot);
	~FAngelscriptStaticJITGenerationASTLease();

	FAngelscriptStaticJITGenerationASTLease(
		const FAngelscriptStaticJITGenerationASTLease&) = delete;
	FAngelscriptStaticJITGenerationASTLease& operator=(
		const FAngelscriptStaticJITGenerationASTLease&) = delete;

	const asCASTContext* GetContext() const;

	/**
	 * Drops the public AST reference while its Runtime Engine is still alive.
	 * Safe to call repeatedly; the destructor uses the same path.
	 */
	void ReleaseBeforeEngineShutdown();

private:
	mutable FCriticalSection LeaseMutex;
	asIASTSnapshot* Snapshot = nullptr;
	const asCASTContext* SealedContext = nullptr;
};

struct FAngelscriptStaticJITGenerationFunction
{
	asIScriptFunction* EngineLocalFunction = nullptr;
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptStableFunctionKey FunctionKey;
	FAngelscriptHash256 ExecutionHash;
	FAngelscriptHash256 DebugHash;
	FAngelscriptHash256 EntryAbiHash;
	FString CanonicalDeclaration;
	TArray<uint8> Bytecode;
	const class asCASTContext* SealedAST = nullptr;
	asUINT CanonicalFunctionDeclValue = 0;
	/** This source function was mapped to an exact canonical body at freeze time. */
	bool bRequiresCanonicalAST = false;
	TArray<FAngelscriptArtifactReference> RequiredReferences;
	bool bHasVerifiedArtifactIdentity = false;
	bool bHasVerifiedArtifactReferences = false;
	bool bHasVMEntry = false;
	bool bHasRawEntry = false;
	bool bHasParmsEntry = false;
	FStaticJITEntryPlan EntryPlan;
	EAngelscriptStaticJITLiteralAssetFunctionRole LiteralAssetRole =
		EAngelscriptStaticJITLiteralAssetFunctionRole::None;
};

struct FAngelscriptStaticJITGenerationType
{
	asITypeInfo* EngineLocalType = nullptr;
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptStableTypeKey TypeKey;
	FString CanonicalDeclaration;
	bool bTargetMaterializesScriptReflection = false;
};

struct FAngelscriptStaticJITGenerationGlobal
{
	const void* EngineLocalGlobal = nullptr;
	int32 EngineLocalGlobalPropertyId = INDEX_NONE;
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptStableGlobalKey GlobalKey;
	FString CanonicalDeclaration;
	/** Exact sealed VarDecl key authenticated against EngineLocalGlobalPropertyId. */
	FString CanonicalASTDeclKey;
};

struct FAngelscriptStaticJITGenerationDescriptor
{
	const void* EngineLocalDescriptor = nullptr;
	EAngelscriptStaticJITGenerationDescriptorKind Kind =
		EAngelscriptStaticJITGenerationDescriptorKind::Class;
	FAngelscriptHash256 StableDescriptorKey;
	FString CanonicalName;
	FString CanonicalDeclaration;
	FString ReceiverCanonicalName;
	bool bUFunctionRoot = false;
	FStaticJITEntryPlan EntryPlan;
};

struct FAngelscriptStaticJITGenerationDependency
{
	FAngelscriptStableFunctionKey OwnerFunctionKey;
	FAngelscriptArtifactReference Reference;
	FString CanonicalDeclaration;
};

/** Cache-authoritative semantic dependency retained separately from the flat reference set. */
struct FAngelscriptStaticJITGenerationSemanticDependency
{
	FAngelscriptStableFunctionKey OwnerFunctionKey;
	EAngelscriptArtifactSemanticDependencyKind Kind =
		EAngelscriptArtifactSemanticDependencyKind::Invalid;
	FAngelscriptArtifactReference Reference;
	TOptional<FAngelscriptHash256> ExpectedContentOrValue;
	int32 EngineLocalGlobalPropertyId = INDEX_NONE;
	/** Exact producer-carried Canonical target; empty means no authenticated mapping. */
	FString CanonicalASTDeclKey;
};

enum class EAngelscriptStaticJITInstalledCallableKind : uint8
{
	Global = 0,
	Method,
	Constructor,
	Destructor,
	Behavior,
	Generic,
};

/** Current target-profile rewrite selected by the maintained compiler fork. */
enum class EAngelscriptStaticJITInstalledCompileOutKind : uint8
{
	CompileCalls = 0,
	CompileOutEntirely,
	ReplaceWithFirstParam,
	CompileOutAsMethodChain,
};

/**
 * Pointer-free description of the exact route CallSystemFunction selects for
 * the current Engine registration. FunctionCaller has precedence over the
 * registered calling convention, matching the maintained VM implementation.
 */
enum class EAngelscriptStaticJITInstalledDispatchKind : uint8
{
	Unknown = 0,
	FunctionCaller,
	GenericFunction,
	GenericMethod,
	NativeCallingConvention,
};

/** Hidden first-parameter routing installed on the system-function interface. */
enum class EAngelscriptStaticJITInstalledFirstParamMetadata : uint8
{
	None = 0,
	ScriptFunction,
	ScriptObjectType,
};

/**
 * Pointer-free, owned row for every function installed through the Bind
 * surface of one generation Engine. Missing direct/native facts are explicit
 * data and do not remove the row from the inventory.
 */
struct FAngelscriptStaticJITNativeCallInventoryEntry
{
	int32 EngineLocalFunctionId = INDEX_NONE;
	FString CanonicalDeclaration;
	EAngelscriptFunctionBindingOrigin Origin =
		EAngelscriptFunctionBindingOrigin::Unknown;
	FName OwnerModule;
	FName Provider;
	EAngelscriptBindPhase Phase = EAngelscriptBindPhase::TypeDeclarations;
	FString ProviderSourceFile;
	int32 ProviderSourceLine = 0;
	EAngelscriptStaticJITInstalledCallableKind CallableKind =
		EAngelscriptStaticJITInstalledCallableKind::Global;
	FString NamespaceCanonicalName;
	FString ReceiverCanonicalName;
	bool bHasStableReference = false;
	FAngelscriptArtifactReference StableReference;
	FString StableReferenceError;
	uint32 FunctionTraitBits = 0;
	EAngelscriptStaticJITInstalledCompileOutKind CompileOutKind =
		EAngelscriptStaticJITInstalledCompileOutKind::CompileCalls;
	int32 DefaultArgumentCount = 0;
	int8 HiddenArgumentIndex = -1;
	int8 DeterminesOutputTypeArgumentIndex = -1;
	EAngelscriptStaticJITInstalledFirstParamMetadata FirstParamMetadata =
		EAngelscriptStaticJITInstalledFirstParamMetadata::None;
	uint32 RegisteredCallConv = 0;
	EAngelscriptStaticJITInstalledDispatchKind DispatchKind =
		EAngelscriptStaticJITInstalledDispatchKind::Unknown;
	int32 RegisteredParameterSize = 0;
	int32 RegisteredReturnSize = 0;
	bool bReturnsOnStack = false;
	bool bUsesWorldContext = false;
	bool bHasSystemCaller = false;
	bool bHasNativeForm = false;
	EAngelscriptNativeFormKind NativeFormKind =
		EAngelscriptNativeFormKind::Unknown;
	FString NativeCallableDisplay;
	FString NativeCustomForm;
	FString NativeTargetType;
	FString NativeHeader;
	FString NativeUnrealFunctionPath;
	EAngelscriptNativeUFunctionRouteFlags NativeUFunctionRouteFlags =
		EAngelscriptNativeUFunctionRouteFlags::None;
	bool bNativeFormTrivial = false;
	bool bNativeFormGuaranteed = false;
	bool bNativeFormNeedsCompare = false;
	bool bNativeFormNeedsCopy = false;
	bool bHasExternalDescriptor = false;
	FAngelscriptStaticJITNativeCallDescriptor ExternalDescriptor;
	bool bHasExpectedNativeABI = false;
	FAngelscriptStaticJITNativeABIIdentity ExpectedNativeABI;
	FString NativeABIError;
	bool bHasExpectedScalarABI = false;
	FAngelscriptStaticJITScalarABIIdentity ExpectedScalarABI;
	FString ScalarABIError;
};

/**
 * Validates that pointer-free installed-dispatch facts cannot contradict an
 * advertised direct DLL/inline descriptor. Generic callbacks are VM ABI
 * entrypoints and must never be reinterpreted as the AS-visible C++ signature.
 */
ANGELSCRIPTRUNTIME_API bool
ValidateAngelscriptStaticJITInstalledNativeCallDispatch(
	const FAngelscriptStaticJITNativeCallInventoryEntry& Entry,
	FString* OutError = nullptr);

/**
 * Engine-local, immutable Static generation result. Raw AngelScript and
 * descriptor pointers are valid only while the owning generation Engine lives.
 * CanonicalASTLeases own retained public AST V1 snapshots only until that
 * Engine replaces this result or starts shutdown. External shared owners may
 * retain the pointer-free fields afterwards, but Engine-local pointers and
 * SealedAST borrows are invalid and every lease returns a null context. Typed
 * capture requires every compiled module to have a live lease; Bytecode capture
 * may also retain one for AST identity metadata.
 */
struct FAngelscriptStaticJITGenerationSnapshot
{
	FAngelscriptArtifactProfileKey ArtifactProfile;
	TArray<FAngelscriptStaticJITGenerationModule> Modules;
	TArray<TSharedPtr<FAngelscriptStaticJITGenerationASTLease,
		ESPMode::ThreadSafe>> CanonicalASTLeases;
	TArray<FAngelscriptStaticJITGenerationFunction> Functions;
	/**
	 * Owned view copied from the exact generation-local frozen Runtime table.
	 * Only function-signature rows are exposed to StaticJIT; numeric TypeIds and
	 * table/snapshot pointers never enter this collection.
	 */
	TArray<FAngelscriptStaticJITRuntimeTypeBinding> RuntimeTypeBindings;
	FString RuntimeTypeBindingTargetProfileKey;
	FString RuntimeTypeBindingNativeEnvironmentKey;
	TArray<FAngelscriptStaticJITGenerationType> Types;
	TArray<FAngelscriptStaticJITGenerationGlobal> Globals;
	TArray<FAngelscriptStaticJITGenerationDescriptor> Descriptors;
	TArray<FAngelscriptStaticJITGenerationDependency> Dependencies;
	TArray<FAngelscriptStaticJITGenerationSemanticDependency>
		SemanticDependencies;
	TArray<FAngelscriptStaticJITGenerationDependency> ExternalNativeCalls;
	TArray<FAngelscriptStaticJITNativeCallTarget> NativeCallTargets;
	TArray<FAngelscriptStaticJITNativeCallInventoryEntry> NativeCallInventory;
	// Number of authoritative source-compiler passes admitted by this
	// generation Engine before the immutable graph was frozen.
	uint32 SourceCompilePassCount = 0;
	// Audit-only count of Cache V2 records carried into generation. StaticJIT
	// generation consumes verified function facts, never persisted record graphs.
	uint32 InputCacheRecordCount = 0;
	bool bDescriptorAnalysisComplete = false;
	EAngelscriptStaticJITCaptureProfile CaptureProfile =
		EAngelscriptStaticJITCaptureProfile::Bytecode;
	bool bHasVerifiedCanonicalAST = false;

	/**
	 * Invalidates Engine-local AST borrows before the Runtime Engine can tear
	 * down the retained type generation. Pointer-free snapshot data is preserved
	 * for external audit owners.
	 */
	void ReleaseEngineLocalASTLeases() const
	{
		for (const TSharedPtr<FAngelscriptStaticJITGenerationASTLease,
			ESPMode::ThreadSafe>& Lease : CanonicalASTLeases)
		{
			if (Lease.IsValid())
			{
				Lease->ReleaseBeforeEngineShutdown();
			}
		}
	}

	bool IsComplete() const
	{
		const bool bModulesComplete = Modules.ContainsByPredicate(
			[](const FAngelscriptStaticJITGenerationModule& Module)
			{
				return Module.EngineLocalModule == nullptr
					|| Module.EngineLocalDescriptor == nullptr
					|| Module.ModuleKey.Hash.IsZero();
			}) == false;
		const bool bFunctionsComplete = Functions.ContainsByPredicate(
			[](const FAngelscriptStaticJITGenerationFunction& Function)
			{
				return Function.EngineLocalFunction == nullptr
					|| Function.ModuleKey.Hash.IsZero()
					|| Function.FunctionKey.Hash.IsZero()
					|| Function.ExecutionHash.IsZero()
					|| Function.DebugHash.IsZero()
					|| Function.EntryAbiHash.IsZero()
					|| !Function.EntryPlan.IsStructurallyComplete();
			}) == false;
		const bool bDescriptorsComplete = Descriptors.ContainsByPredicate(
			[](const FAngelscriptStaticJITGenerationDescriptor& Descriptor)
			{
				return Descriptor.StableDescriptorKey.IsZero()
					|| Descriptor.CanonicalName.IsEmpty()
					|| Descriptor.CanonicalDeclaration.IsEmpty()
					|| (Descriptor.Kind
							== EAngelscriptStaticJITGenerationDescriptorKind::Function
						&& !Descriptor.EntryPlan.IsStructurallyComplete());
			}) == false;
		const bool bNativeCallTargetsComplete =
			NativeCallTargets.ContainsByPredicate(
				[](const FAngelscriptStaticJITNativeCallTarget& Target)
				{
					const bool bDescriptorAbsent =
						Target.Descriptor.IsAbsent();
					const bool bDescriptorBridge = !bDescriptorAbsent
						&& EnumHasAnyFlags(
							Target.Descriptor.RouteFlags,
							EAngelscriptStaticJITNativeCallRouteFlags::ScalarBridge);
					const bool bDescriptorCurrentNative = !bDescriptorAbsent
						&& EnumHasAnyFlags(
							Target.Descriptor.RouteFlags,
							EAngelscriptStaticJITNativeCallRouteFlags::CurrentNativeCall);
					return Target.EngineLocalFunctionId < 0
						|| Target.CanonicalTargetDeclKey.IsEmpty()
						|| Target.RegisteredTargetDisplay.IsEmpty()
						|| !Target.Reference.IsValid()
						|| Target.Reference.Kind
							!= EAngelscriptArtifactReferenceKind::EnvironmentSymbol
						|| Target.Reference.ExpectedAbi.IsZero()
						|| !Target.ExpectedNativeABI.IsValid()
						|| (!bDescriptorAbsent
							&& Target.Descriptor.ABIDomain
								== EAngelscriptStaticJITNativeCallABIDomain::Scalar
							&& !Target.ExpectedScalarABI.IsValid())
						|| (!bDescriptorAbsent
							&& Target.Descriptor.ABIDomain
								== EAngelscriptStaticJITNativeCallABIDomain::BorrowedManagedPOD
							&& Target.ExpectedScalarABI.IsValid())
						|| (bDescriptorAbsent
							&& (!Target.bScalarBridgeCallable
								|| !Target.ExpectedVMBridgeABI.IsValid()))
						|| (!bDescriptorAbsent
							&& Target.bScalarBridgeCallable != bDescriptorBridge)
						|| Target.bCurrentNativeCallable != bDescriptorCurrentNative
						|| (Target.bScalarBridgeCallable
							&& !Target.ExpectedVMBridgeABI.IsValid());
				}) == false;
		const bool bNativeCallInventoryComplete =
			!NativeCallInventory.IsEmpty()
			&& NativeCallInventory.ContainsByPredicate(
				[](const FAngelscriptStaticJITNativeCallInventoryEntry& Entry)
				{
					return Entry.EngineLocalFunctionId < 0
						|| Entry.CanonicalDeclaration.IsEmpty()
						|| Entry.Origin
							== EAngelscriptFunctionBindingOrigin::Unknown
						|| Entry.OwnerModule.IsNone()
						|| Entry.Provider.IsNone()
						|| Entry.ProviderSourceFile.IsEmpty()
						|| Entry.ProviderSourceLine <= 0;
				}) == false;
		const bool bCanonicalASTLeasesComplete =
			CaptureProfile != EAngelscriptStaticJITCaptureProfile::VerifiedCanonicalAST
			|| (CanonicalASTLeases.Num() == Modules.Num()
				&& CanonicalASTLeases.ContainsByPredicate(
					[](const TSharedPtr<FAngelscriptStaticJITGenerationASTLease,
						ESPMode::ThreadSafe>& Lease)
					{
						return !Lease.IsValid() || Lease->GetContext() == nullptr;
					}) == false);
		const bool bTypedFunctionContextsComplete =
			CaptureProfile != EAngelscriptStaticJITCaptureProfile::VerifiedCanonicalAST
			|| Functions.ContainsByPredicate(
				[](const FAngelscriptStaticJITGenerationFunction& Function)
				{
					return Function.SealedAST == nullptr;
				}) == false;
		const bool bRuntimeTypeBindingsComplete =
			CaptureProfile != EAngelscriptStaticJITCaptureProfile::VerifiedCanonicalAST
			|| (!RuntimeTypeBindingTargetProfileKey.IsEmpty()
				&& !RuntimeTypeBindingNativeEnvironmentKey.IsEmpty()
				&& RuntimeTypeBindings.ContainsByPredicate(
					[](const FAngelscriptStaticJITRuntimeTypeBinding& Binding)
					{
						return !Binding.IsStructurallyComplete();
					}) == false);
		return bDescriptorAnalysisComplete
			&& SourceCompilePassCount == 1
			&& !ArtifactProfile.Hash.IsZero()
			&& !Modules.IsEmpty()
			&& bModulesComplete
			&& !Functions.IsEmpty()
			&& bFunctionsComplete
			&& bDescriptorsComplete
			&& bNativeCallTargetsComplete
			&& bNativeCallInventoryComplete
			&& bCanonicalASTLeasesComplete
			&& bTypedFunctionContextsComplete
			&& bRuntimeTypeBindingsComplete;
	}
};

ANGELSCRIPTRUNTIME_API bool BuildAngelscriptStaticJITGenerationSnapshot(
	FAngelscriptEngine& Engine,
	TConstArrayView<TSharedRef<FAngelscriptModuleDesc>> Modules,
	TConstArrayView<FAngelscriptCacheCleanModuleArtifacts> Artifacts,
	const FAngelscriptArtifactProfileKey& ArtifactProfile,
	TSharedPtr<const FAngelscriptStaticJITGenerationSnapshot,
		ESPMode::ThreadSafe>& OutSnapshot,
	FString* OutError = nullptr);

#endif
