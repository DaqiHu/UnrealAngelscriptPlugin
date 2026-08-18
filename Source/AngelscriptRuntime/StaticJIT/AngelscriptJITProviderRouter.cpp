#include "StaticJIT/AngelscriptJITProviderRouter.h"

#include "AngelscriptEngine.h"
#include "StaticJIT/AngelscriptJITProviderMatcher.h"
#include "StaticJIT/AngelscriptJITProviderRegistry.h"
#include "StaticJIT/AngelscriptJITReferenceResolver.h"
#include "RuntimeJIT/AngelscriptJITCoordinator.h"
#include "StaticJIT/BytecodeJIT/AngelscriptBytecodeJIT.h"

#include "HAL/PlatformTime.h"

namespace
{
	static bool IsCandidateForRoute(
		const FAngelscriptJITProviderCatalogEntry& Entry,
		const FAngelscriptFunctionRoute& Route)
	{
		return Entry.ModuleKey == Route.ModuleKey
			&& Entry.FunctionKey == Route.Identity.FunctionKey;
	}

	static FString BuildDetail(
		const FAngelscriptJITProviderRouteRefreshResult& Result,
		const int32 ProviderCount)
	{
		TArray<uint32> MatchResultCounts;
		MatchResultCounts.Init(
			0,
			static_cast<int32>(
				EAngelscriptArtifactMatchResult::SemanticDependencyMismatch) + 1);
		for (const FAngelscriptJITProviderRouteRecord& Route : Result.Routes)
		{
			const int32 MatchResultIndex = static_cast<int32>(Route.MatchResult);
			if (MatchResultCounts.IsValidIndex(MatchResultIndex))
			{
				++MatchResultCounts[MatchResultIndex];
			}
		}

		TArray<FString> MatchResultParts;
		for (int32 MatchResultIndex = 0;
			MatchResultIndex < MatchResultCounts.Num();
			++MatchResultIndex)
		{
			if (MatchResultCounts[MatchResultIndex] != 0)
			{
				MatchResultParts.Add(FString::Printf(
					TEXT("%d:%u"),
					MatchResultIndex,
					MatchResultCounts[MatchResultIndex]));
			}
		}

		return FString::Printf(
			TEXT("StaticJIT Provider routing: code=%u profile=%s publication=%llu providers=%d verified=%u exact=%u native=%u vm=%u references=%u/%u sourceRoute=%llu publishedRoute=%llu matchResults=[%s] timings_ms=[catalog=%.3f,discovery=%.3f,references=%.3f,routes=%.3f,total=%.3f]"),
			static_cast<uint32>(Result.Code),
			FAngelscriptJITTargetProfiles::ToString(Result.TargetProfile),
			Result.ProviderPublicationOrdinal,
			ProviderCount,
			Result.VerifiedRouteCount,
			Result.ExactMatchCount,
			Result.PublishedNativeCount,
			Result.VmFallbackCount,
			Result.ResolvedReferenceCount,
			Result.RequestedReferenceCount,
			Result.SourceRouteGeneration.Value,
			Result.PublishedRouteGeneration.Value,
			*FString::Join(MatchResultParts, TEXT(",")),
			Result.ProviderCatalogSnapshotMs,
			Result.ReferenceDiscoveryMs,
			Result.StableReferenceResolutionMs,
			Result.RouteMatchAndPublicationMs,
			Result.TotalDurationMs);
	}
}

EAngelscriptJITTargetProfile
FAngelscriptJITProviderRouter::ResolveTargetProfile(
	const FAngelscriptEngineConfig& Config)
{
	if (Config.bOverrideTargetPreprocessorFlags)
	{
		if (Config.bTargetEditor)
		{
			return EAngelscriptJITTargetProfile::EditorDevelopment;
		}
		return Config.bTargetRelease
			? EAngelscriptJITTargetProfile::GameShipping
			: EAngelscriptJITTargetProfile::GameDevelopment;
	}
	if (Config.bIsEditor)
	{
		return EAngelscriptJITTargetProfile::EditorDevelopment;
	}
#if UE_BUILD_SHIPPING
	return EAngelscriptJITTargetProfile::GameShipping;
#else
	return EAngelscriptJITTargetProfile::GameDevelopment;
#endif
}

FAngelscriptJITProviderRouteRefreshResult
FAngelscriptJITProviderRouter::Refresh(FAngelscriptEngine& Engine)
{
	return Refresh(Engine,
		ResolveTargetProfile(Engine.GetRuntimeConfig()));
}

FAngelscriptJITProviderRouteRefreshResult
FAngelscriptJITProviderRouter::Refresh(
	FAngelscriptEngine& Engine,
	const EAngelscriptJITTargetProfile TargetProfile)
{
	FAngelscriptJITProviderRouteRefreshResult Result;
	Result.TargetProfile = TargetProfile;
	const double TotalStart = FPlatformTime::Seconds();
	const auto Finish = [&Result](
		const double InTotalStart,
		const EAngelscriptJITProviderRouteRefreshCode Code,
		const int32 ProviderCount)
	{
		Result.Code = Code;
		Result.TotalDurationMs =
			(FPlatformTime::Seconds() - InTotalStart) * 1000.0;
		Result.Detail = BuildDetail(Result, ProviderCount);
		return Result;
	};

	if (Engine.GetScriptEngine() == nullptr
		|| TargetProfile == EAngelscriptJITTargetProfile::Invalid
		|| TargetProfile == EAngelscriptJITTargetProfile::All)
	{
		return Finish(TotalStart,
			EAngelscriptJITProviderRouteRefreshCode::InvalidEngine, 0);
	}
	if (Engine.JITCoordinator == nullptr
		|| Engine.GetScriptEngine()->GetJITCompiler()
			!= Engine.JITCoordinator)
	{
		return Finish(TotalStart,
			EAngelscriptJITProviderRouteRefreshCode::
				MissingStaticJITCompiler,
			0);
	}
	const bool bAllowsStaticAOT =
		Engine.JITCoordinator->AllowsStaticAOT();

	const double ProviderCatalogStart = FPlatformTime::Seconds();
	const TSharedPtr<const FAngelscriptJITProviderRegistrySnapshot,
		ESPMode::ThreadSafe> Providers =
		FAngelscriptJITProviderRegistry::Get().GetSnapshot();
	Result.ProviderCatalogSnapshotMs =
		(FPlatformTime::Seconds() - ProviderCatalogStart) * 1000.0;
	if (!Providers.IsValid())
	{
		return Finish(TotalStart,
			EAngelscriptJITProviderRouteRefreshCode::
				MissingProviderSnapshot,
			0);
	}
	Result.ProviderPublicationOrdinal = Providers->PublicationOrdinal;
	const int32 ProviderCount = Providers->Providers.Num();

	const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
		ESPMode::ThreadSafe> SourceRoutes = Engine.GetFunctionRouteSnapshot();
	if (!SourceRoutes.IsValid())
	{
		return Finish(TotalStart,
			EAngelscriptJITProviderRouteRefreshCode::MissingRouteSnapshot,
			ProviderCount);
	}
	Result.SourceRouteGeneration = SourceRoutes->Generation;

	TArray<const FAngelscriptFunctionRoute*> VerifiedRoutes;
	TArray<FAngelscriptArtifactReference> RequiredReferences;
	const double ReferenceDiscoveryStart = FPlatformTime::Seconds();
	for (const FAngelscriptFunctionRoute& Route : SourceRoutes->FunctionRoutes)
	{
		if (Route.Function == nullptr
			|| !Route.bHasVerifiedArtifactIdentity
			|| Route.ModuleKey.Hash.IsZero()
			|| Route.Identity.FunctionKey.Hash.IsZero()
			|| Route.Identity.Content.Execution.IsZero()
			|| Route.Identity.Profile.Hash.IsZero())
		{
			continue;
		}
		VerifiedRoutes.Add(&Route);
		for (const TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe>& Provider : Providers->Providers)
		{
			if (!Provider.IsValid())
			{
				continue;
			}
			for (const FAngelscriptJITProviderCatalogEntry& Entry
				: Provider->Entries)
			{
				if (!IsCandidateForRoute(Entry, Route))
				{
					continue;
				}
				for (const FAngelscriptJITReferenceSlot& Slot
					: Entry.ReferenceSlots)
				{
					RequiredReferences.AddUnique(Slot.Reference);
				}
			}
		}
	}
	Result.ReferenceDiscoveryMs =
		(FPlatformTime::Seconds() - ReferenceDiscoveryStart) * 1000.0;
	Result.VerifiedRouteCount = static_cast<uint32>(VerifiedRoutes.Num());
	if (VerifiedRoutes.IsEmpty())
	{
		Engine.JITCoordinator->ReplaceSemanticDependents({});
		Engine.JITCoordinator->ProcessRuntimeSafePoint(Engine);
		return Finish(TotalStart,
			EAngelscriptJITProviderRouteRefreshCode::NoRoutes,
			ProviderCount);
	}
	if (!bAllowsStaticAOT)
	{
		Engine.JITCoordinator->ReplaceSemanticDependents({});
		bool bAnyBindingChanged = false;
		for (const FAngelscriptFunctionRoute* Route : VerifiedRoutes)
		{
			FAngelscriptJITProviderRouteRecord& Record =
				Result.Routes.AddDefaulted_GetRef();
			Record.ModuleKey = Route->ModuleKey;
			Record.FunctionKey = Route->Identity.FunctionKey;
			bAnyBindingChanged |=
				Engine.JITCoordinator->ClearProviderSelection(Route->Function);
			++Result.VmFallbackCount;
		}
		if (bAnyBindingChanged
			&& !Engine.RefreshFunctionRouteSnapshotAfterStaticJITChange(true))
		{
			return Finish(TotalStart,
				EAngelscriptJITProviderRouteRefreshCode::RoutePublicationFailed,
				ProviderCount);
		}
		Engine.JITCoordinator->ProcessRuntimeSafePoint(Engine);
		const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe> PublishedRoutes =
			Engine.GetFunctionRouteSnapshot();
		Result.PublishedRouteGeneration = PublishedRoutes.IsValid()
			? PublishedRoutes->Generation
			: Result.SourceRouteGeneration;
		Result.RouteMatchAndPublicationMs =
			(FPlatformTime::Seconds() - ReferenceDiscoveryStart) * 1000.0;
		return Finish(TotalStart,
			EAngelscriptJITProviderRouteRefreshCode::Applied,
			ProviderCount);
	}

	const double ReferenceResolutionStart = FPlatformTime::Seconds();
	const FAngelscriptJITCurrentEngineReferenceBuildResult ReferenceBuild =
		FAngelscriptJITCurrentEngineReferenceResolverBuilder::Build(
			Engine, RequiredReferences);
	Result.StableReferenceResolutionMs =
		(FPlatformTime::Seconds() - ReferenceResolutionStart) * 1000.0;
	Result.RequestedReferenceCount = ReferenceBuild.RequestedReferenceCount;
	Result.ResolvedReferenceCount = ReferenceBuild.ResolvedReferenceCount;
	if (!ReferenceBuild.IsUsable())
	{
		return Finish(TotalStart,
			EAngelscriptJITProviderRouteRefreshCode::
				ReferenceResolutionUnavailable,
			ProviderCount);
	}

	const FAngelscriptHash256 NativeEnvironmentFingerprint =
		FAngelscriptJITTargetProfiles::
			BuildCurrentNativeEnvironmentFingerprint(TargetProfile);
	TArray<asIScriptFunction*> PublishedFunctions;
	TArray<FAngelscriptJITCoordinator::FSemanticDependentRecord> Dependents;
	bool bAnyBindingChanged = false;
	const double RouteMatchStart = FPlatformTime::Seconds();
	for (const FAngelscriptFunctionRoute* Route : VerifiedRoutes)
	{
		FAngelscriptJITProviderRouteRecord& Record =
			Result.Routes.AddDefaulted_GetRef();
		Record.ModuleKey = Route->ModuleKey;
		Record.FunctionKey = Route->Identity.FunctionKey;

		const bool bHasParmsEntry =
			FAngelscriptBytecodeJIT::CanGenerateParmsEntry(Route->Function);
		FAngelscriptJITFunctionMatchInput Input;
		Input.ModuleKey = Route->ModuleKey;
		Input.Identity = Route->Identity;
		Input.EntryAbiHash = FAngelscriptJITGeneration::BuildEntryAbiHash(
			Route->CanonicalDeclaration,
			true,
			true,
			bHasParmsEntry);
		Input.NativeEnvironmentFingerprint = NativeEnvironmentFingerprint;
		Input.CurrentRoutes = SourceRoutes;
		Input.RequiredEntryPoints =
			EAngelscriptJITRequiredEntryPoints::VM
			| EAngelscriptJITRequiredEntryPoints::Raw;
		if (bHasParmsEntry)
		{
			Input.RequiredEntryPoints |=
				EAngelscriptJITRequiredEntryPoints::Parms;
		}

		FAngelscriptJITFunctionMatchResult Match;
		if (bAllowsStaticAOT && bHasParmsEntry)
		{
			FAngelscriptJITFunctionMatchInput VMRawFallbackInput = Input;
			VMRawFallbackInput.EntryAbiHash =
				FAngelscriptJITGeneration::BuildEntryAbiHash(
					Route->CanonicalDeclaration, true, true, false);
			VMRawFallbackInput.RequiredEntryPoints =
				EAngelscriptJITRequiredEntryPoints::VM
				| EAngelscriptJITRequiredEntryPoints::Raw;
			Match = FAngelscriptJITProviderMatcher::
				MatchPreferredThenFallback(
					*Providers,
					Input,
					VMRawFallbackInput,
					*ReferenceBuild.Resolver);
		}
		else if (bAllowsStaticAOT)
		{
			Match = FAngelscriptJITProviderMatcher::Match(
				*Providers, Input, *ReferenceBuild.Resolver);
		}
		Record.MatchResult = Match.Result;
		if (Match.Result == EAngelscriptArtifactMatchResult::Exact
			&& Match.Selection.IsSet())
		{
			++Result.ExactMatchCount;
			const FAngelscriptJITArtifactSelection& Selection =
				Match.Selection.GetValue();
			Record.ProviderId = Selection.Provider->ProviderId;
			Record.ProviderGeneration =
				Selection.Provider->ProviderGeneration;
			if (Engine.JITCoordinator->PublishProviderSelection(
					Route->Function, Selection))
			{
				Record.bBindingPublished = true;
				PublishedFunctions.Add(Route->Function);
				++Result.PublishedNativeCount;
				bAnyBindingChanged = true;
				const FAngelscriptJITProviderCatalogEntry& PublishedEntry =
					Selection.Provider->Entries[Selection.EntryIndex];
				for (const FAngelscriptJITSemanticDependencyRow& Row
					: PublishedEntry.SemanticDependencies)
				{
					FAngelscriptJITCoordinator::FSemanticDependentRecord* Existing =
						Dependents.FindByPredicate(
							[&Row](const FAngelscriptJITCoordinator::
								FSemanticDependentRecord& Candidate)
							{
								return Candidate.Kind == Row.Kind
									&& Candidate.TargetStableKey
										== Row.TargetStableKey;
							});
					if (Existing == nullptr)
					{
						Existing = &Dependents.AddDefaulted_GetRef();
						Existing->Kind = Row.Kind;
						Existing->TargetStableKey = Row.TargetStableKey;
					}
					Existing->Dependents.AddUnique(Route->Identity.FunctionKey);
				}
				continue;
			}
			Record.MatchResult =
				EAngelscriptArtifactMatchResult::InvalidProvider;
		}

		bAnyBindingChanged |= Engine.JITCoordinator->ClearProviderSelection(
			Route->Function);
		++Result.VmFallbackCount;
	}

	Engine.JITCoordinator->ReplaceSemanticDependents(MoveTemp(Dependents));

	if (bAnyBindingChanged
		&& !Engine.RefreshFunctionRouteSnapshotAfterStaticJITChange(true))
	{
		Engine.JITCoordinator->ReplaceSemanticDependents({});
		for (asIScriptFunction* Function : PublishedFunctions)
		{
			Engine.JITCoordinator->ClearProviderSelection(Function);
		}
		Result.PublishedNativeCount = 0;
		Result.VmFallbackCount = Result.VerifiedRouteCount;
		for (FAngelscriptJITProviderRouteRecord& Record : Result.Routes)
		{
			Record.bBindingPublished = false;
		}
		Result.RouteMatchAndPublicationMs =
			(FPlatformTime::Seconds() - RouteMatchStart) * 1000.0;
		return Finish(TotalStart,
			EAngelscriptJITProviderRouteRefreshCode::RoutePublicationFailed,
			ProviderCount);
	}

	// Static Provider selection is authoritative and completes first. Runtime
	// may consume only the resulting current routes, so Auto can never replace
	// an exact AOT Binding merely because a backend result is also available.
	if (!Engine.JITCoordinator->ProcessRuntimeSafePoint(Engine))
	{
		UE_LOG(Angelscript, Warning,
			TEXT("[RuntimeJIT] Runtime safe-point publication failed for Engine=%p"),
			&Engine);
	}

	const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
		ESPMode::ThreadSafe> PublishedRoutes = Engine.GetFunctionRouteSnapshot();
	Result.PublishedRouteGeneration = PublishedRoutes.IsValid()
		? PublishedRoutes->Generation
		: Result.SourceRouteGeneration;
	Result.RouteMatchAndPublicationMs =
		(FPlatformTime::Seconds() - RouteMatchStart) * 1000.0;
	return Finish(TotalStart,
		EAngelscriptJITProviderRouteRefreshCode::Applied,
		ProviderCount);
}

bool FAngelscriptJITProviderRouter::CollectSemanticDependents(
	const FAngelscriptEngine& Engine,
	const EAngelscriptArtifactSemanticDependencyKind Kind,
	const FAngelscriptHash256& TargetStableKey,
	TArray<FAngelscriptStableFunctionKey>& OutDependents)
{
	OutDependents.Reset();
	FAngelscriptJITCoordinator* const Coordinator = Engine.GetJITCoordinator();
	if (Coordinator == nullptr)
	{
		return false;
	}
	return Coordinator->CollectSemanticDependents(
		Kind, TargetStableKey, OutDependents);
}
