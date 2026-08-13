#pragma once

#include "CoreMinimal.h"

#include "Artifacts/AngelscriptFunctionRoute.h"
#include "StaticJIT/AngelscriptJITProviderRegistry.h"
#include "StaticJIT/AngelscriptJITReferenceResolver.h"

enum class EAngelscriptJITRequiredEntryPoints : uint8
{
	None = 0,
	VM = 1u << 0,
	Raw = 1u << 1,
	Parms = 1u << 2,
};
ENUM_CLASS_FLAGS(EAngelscriptJITRequiredEntryPoints)

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITFunctionMatchInput
{
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptFunctionArtifactIdentity Identity;
	FAngelscriptHash256 EntryAbiHash;
	FAngelscriptHash256 NativeEnvironmentFingerprint;
	FAngelscriptHash256 CurrentArtifactSetDigest;
	EAngelscriptJITRequiredEntryPoints RequiredEntryPoints =
		EAngelscriptJITRequiredEntryPoints::VM;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITArtifactMatchCandidate
{
	FAngelscriptJITProviderId ProviderId;
	FAngelscriptJITProviderGeneration ProviderGeneration;
	int32 EntryIndex = INDEX_NONE;
	EAngelscriptArtifactMatchResult Result =
		EAngelscriptArtifactMatchResult::MissingProviderEntry;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITArtifactSelection
{
	TSharedPtr<const FAngelscriptJITProviderCatalog,
		ESPMode::ThreadSafe> Provider;
	int32 EntryIndex = INDEX_NONE;
	TSharedPtr<const FAngelscriptJITResolvedReferenceTable,
		ESPMode::ThreadSafe> ReferenceTable;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITFunctionMatchResult
{
	EAngelscriptArtifactMatchResult Result =
		EAngelscriptArtifactMatchResult::MissingProviderEntry;
	TOptional<FAngelscriptJITArtifactSelection> Selection;
	TArray<FAngelscriptJITArtifactMatchCandidate> Candidates;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderMatcher
{
public:
	static FAngelscriptJITFunctionMatchResult Match(
		const FAngelscriptJITProviderRegistrySnapshot& Providers,
		const FAngelscriptJITFunctionMatchInput& Input,
		const IAngelscriptJITReferenceResolver& ReferenceResolver);
};
