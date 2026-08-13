#pragma once

#include "CoreMinimal.h"
#include "Core/AngelscriptEngine.h"
#include "Artifacts/AngelscriptFunctionRoute.h"
#include "StaticJIT/AngelscriptJITGeneration.h"
#include "StaticJIT/AngelscriptJITProviderRegistry.h"
#include "StaticJIT/StaticJITConfig.h"

class asCScriptFunction;

struct ANGELSCRIPTRUNTIME_API FStaticJITDiagnostics
{
	struct FReferenceRecord
	{
		uint32 SlotIndex = 0;
		EAngelscriptJITReferenceSlotFlags Flags =
			EAngelscriptJITReferenceSlotFlags::None;
		FAngelscriptArtifactReference Reference;
		bool bResolved = false;
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
		static constexpr uint32 CurrentSchemaVersion = 2;

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
	static FSnapshot CaptureSnapshot(FAngelscriptEngine* Engine = nullptr);
	static FJsonResult Serialize(const FSnapshot& Snapshot);
	static FJsonResult CaptureJson(FAngelscriptEngine* Engine = nullptr);
	static bool WriteJson(const FString& Filename, FAngelscriptEngine* Engine = nullptr, FString* OutError = nullptr);
	static void DumpDiagnostics(const TArray<FString>& Args);
};
