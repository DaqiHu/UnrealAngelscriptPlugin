#pragma once

#include "CoreMinimal.h"

#include "Artifacts/AngelscriptFunctionRoute.h"
#include "StaticJIT/AngelscriptJITGeneration.h"
#include "StaticJIT/AngelscriptJITProvider.h"

struct FAngelscriptEngine;
struct FAngelscriptEngineConfig;

enum class EAngelscriptJITProviderRouteRefreshCode : uint8
{
	Applied = 0,
	NoRoutes,
	InvalidEngine,
	MissingStaticJITCompiler,
	MissingRouteSnapshot,
	MissingProviderSnapshot,
	ReferenceResolutionUnavailable,
	RoutePublicationFailed,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderRouteRecord
{
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptStableFunctionKey FunctionKey;
	FAngelscriptJITProviderId ProviderId;
	FAngelscriptJITProviderGeneration ProviderGeneration;
	EAngelscriptArtifactMatchResult MatchResult =
		EAngelscriptArtifactMatchResult::MissingProviderEntry;
	bool bBindingPublished = false;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderRouteRefreshResult
{
	EAngelscriptJITProviderRouteRefreshCode Code =
		EAngelscriptJITProviderRouteRefreshCode::InvalidEngine;
	EAngelscriptJITTargetProfile TargetProfile =
		EAngelscriptJITTargetProfile::Invalid;
	uint64 ProviderPublicationOrdinal = 0;
	FAngelscriptFunctionRouteGeneration SourceRouteGeneration;
	FAngelscriptFunctionRouteGeneration PublishedRouteGeneration;
	uint32 VerifiedRouteCount = 0;
	uint32 ExactMatchCount = 0;
	uint32 PublishedNativeCount = 0;
	uint32 VmFallbackCount = 0;
	uint32 RequestedReferenceCount = 0;
	uint32 ResolvedReferenceCount = 0;
	double ProviderCatalogSnapshotMs = 0.0;
	double ReferenceDiscoveryMs = 0.0;
	double StableReferenceResolutionMs = 0.0;
	double RouteMatchAndPublicationMs = 0.0;
	double TotalDurationMs = 0.0;
	TArray<FAngelscriptJITProviderRouteRecord> Routes;
	FString Detail;

	bool IsApplied() const
	{
		return Code == EAngelscriptJITProviderRouteRefreshCode::Applied
			|| Code == EAngelscriptJITProviderRouteRefreshCode::NoRoutes;
	}
};

/**
 * Matches copied Provider catalogs against one Engine's authoritative current
 * routes, resolves Engine-local references and publishes complete bindings.
 * The caller must invoke this only after a successful compile/restore safe
 * point; no generated pointer or numeric FunctionId is accepted as identity.
 */
class ANGELSCRIPTRUNTIME_API FAngelscriptJITProviderRouter
{
public:
	static EAngelscriptJITTargetProfile ResolveTargetProfile(
		const FAngelscriptEngineConfig& Config);

	static FAngelscriptJITProviderRouteRefreshResult Refresh(
		FAngelscriptEngine& Engine);

	static FAngelscriptJITProviderRouteRefreshResult Refresh(
		FAngelscriptEngine& Engine,
		EAngelscriptJITTargetProfile TargetProfile);
};
