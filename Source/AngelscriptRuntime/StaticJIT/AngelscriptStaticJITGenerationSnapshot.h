#pragma once

#include "CoreMinimal.h"

#include "Core/Artifacts/AngelscriptArtifactIdentity.h"
#include "Core/Artifacts/AngelscriptArtifactReference.h"
#include "StaticJIT/StaticJITConfig.h"

#if AS_CAN_GENERATE_JIT

class asIScriptFunction;
class asIScriptModule;
class asITypeInfo;
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

/** Pointer-free entry ABI decision shared by all Static backends. */
struct FAngelscriptStaticJITGenerationEntryPlan
{
	FAngelscriptHash256 EntryPlanHash;
	bool bHasVMEntry = false;
	bool bHasRawEntry = false;
	bool bHasParmsEntry = false;
};

struct FAngelscriptStaticJITGenerationModule
{
	asIScriptModule* EngineLocalModule = nullptr;
	FAngelscriptStableModuleKey ModuleKey;
	FString CanonicalModuleName;
	FString VirtualSourcePath;
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
	TArray<FAngelscriptArtifactReference> RequiredReferences;
	bool bHasVerifiedArtifactIdentity = false;
	bool bHasVerifiedArtifactReferences = false;
	bool bHasVMEntry = false;
	bool bHasRawEntry = false;
	bool bHasParmsEntry = false;
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
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptStableGlobalKey GlobalKey;
	FString CanonicalDeclaration;
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
	FAngelscriptStaticJITGenerationEntryPlan EntryPlan;
};

struct FAngelscriptStaticJITGenerationDependency
{
	FAngelscriptStableFunctionKey OwnerFunctionKey;
	FAngelscriptArtifactReference Reference;
	FString CanonicalDeclaration;
};

/**
 * Engine-local, immutable Static generation result. Raw AngelScript and
 * descriptor pointers are valid only while the owning generation Engine lives.
 */
struct FAngelscriptStaticJITGenerationSnapshot
{
	FAngelscriptArtifactProfileKey ArtifactProfile;
	TArray<FAngelscriptStaticJITGenerationModule> Modules;
	TArray<FAngelscriptStaticJITGenerationFunction> Functions;
	TArray<FAngelscriptStaticJITGenerationType> Types;
	TArray<FAngelscriptStaticJITGenerationGlobal> Globals;
	TArray<FAngelscriptStaticJITGenerationDescriptor> Descriptors;
	TArray<FAngelscriptStaticJITGenerationDependency> Dependencies;
	TArray<FAngelscriptStaticJITGenerationDependency> ExternalNativeCalls;
	bool bDescriptorAnalysisComplete = false;
	bool bHasVerifiedTypedHIR = false;

	bool IsComplete() const
	{
		const bool bFunctionsComplete = Functions.ContainsByPredicate(
			[](const FAngelscriptStaticJITGenerationFunction& Function)
			{
				return Function.EngineLocalFunction == nullptr
					|| Function.ModuleKey.Hash.IsZero()
					|| Function.FunctionKey.Hash.IsZero()
					|| Function.ExecutionHash.IsZero()
					|| Function.DebugHash.IsZero()
					|| Function.EntryAbiHash.IsZero();
			}) == false;
		const bool bDescriptorsComplete = Descriptors.ContainsByPredicate(
			[](const FAngelscriptStaticJITGenerationDescriptor& Descriptor)
			{
				return Descriptor.StableDescriptorKey.IsZero()
					|| Descriptor.CanonicalName.IsEmpty()
					|| Descriptor.CanonicalDeclaration.IsEmpty()
					|| (Descriptor.Kind
							== EAngelscriptStaticJITGenerationDescriptorKind::Function
						&& (Descriptor.EntryPlan.EntryPlanHash.IsZero()
							|| !Descriptor.EntryPlan.bHasVMEntry
							|| !Descriptor.EntryPlan.bHasRawEntry));
			}) == false;
		return bDescriptorAnalysisComplete
			&& !ArtifactProfile.Hash.IsZero()
			&& !Modules.IsEmpty()
			&& !Functions.IsEmpty()
			&& bFunctionsComplete
			&& bDescriptorsComplete;
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
