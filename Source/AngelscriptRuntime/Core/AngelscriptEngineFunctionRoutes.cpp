#include "AngelscriptEngine.h"
#include "Cache/AngelscriptCacheDecisionTrace.h"
#include "Cache/AngelscriptCacheService.h"
#include "Cache/AngelscriptCacheStableSymbolIdentity.h"
#include "Core/Artifacts/AngelscriptArtifactIdentity.h"
#include "Core/Artifacts/AngelscriptFunctionRoute.h"

#include "StartAngelscriptHeaders.h"
#include "as_module.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

bool FAngelscriptEngine::ResolveFunctionRoute(
	const FAngelscriptStableFunctionKey& FunctionKey,
	FAngelscriptFunctionRoute& OutRoute) const
{
	OutRoute = {};
	if (!FunctionRouteState.IsValid() || !FunctionRouteState->Snapshot.IsValid())
	{
		return false;
	}
	const TArray<FAngelscriptFunctionRoute>& Routes =
		FunctionRouteState->Snapshot->FunctionRoutes;
	int32 Lower = 0;
	int32 Upper = Routes.Num();
	while (Lower < Upper)
	{
		const int32 Middle = Lower + (Upper - Lower) / 2;
		if (Routes[Middle].Identity.FunctionKey.Hash < FunctionKey.Hash)
		{
			Lower = Middle + 1;
		}
		else
		{
			Upper = Middle;
		}
	}
	if (!Routes.IsValidIndex(Lower))
	{
		return false;
	}
	const FAngelscriptFunctionRoute& Route = Routes[Lower];
	if (Route.Identity.FunctionKey != FunctionKey
		|| Route.Function == nullptr
		|| Route.Function->GetEngine() != Engine
		|| Route.NumericFunctionId != Route.Function->GetId()
		|| Engine->GetFunctionById(Route.NumericFunctionId) != Route.Function)
	{
		return false;
	}
	OutRoute = Route;
	return true;
}

bool FAngelscriptEngine::TryResolveStableFunctionKey(
	const asIScriptFunction* Function,
	FAngelscriptStableFunctionKey& OutFunctionKey)
{
	OutFunctionKey = {};
	if (Function == nullptr
		|| Function->GetEngine() != Engine
		|| static_cast<const asCScriptFunction*>(Function)->module == nullptr)
	{
		return false;
	}

	if (FunctionRouteState.IsValid()
		&& FunctionRouteState->Snapshot.IsValid())
	{
		const FAngelscriptFunctionRoute* const Existing =
			FunctionRouteState->Snapshot->FunctionRoutes.FindByPredicate(
				[Function](const FAngelscriptFunctionRoute& Route)
				{
					return Route.Function == Function;
				});
		if (Existing != nullptr
			&& !Existing->Identity.FunctionKey.Hash.IsZero())
		{
			OutFunctionKey = Existing->Identity.FunctionKey;
			return true;
		}
	}

	TSharedPtr<FAngelscriptModuleDesc> Module = GetModule(
		static_cast<const asCScriptFunction*>(Function)->module);
	if (!Module.IsValid())
	{
		return false;
	}

	FAngelscriptStableModuleKey ModuleKey;
	return FAngelscriptCacheStableSymbolIdentity::TryBuildModuleKey(
			*Module, ModuleKey)
		&& FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
			ModuleKey,
			*static_cast<const asCScriptFunction*>(Function),
			OutFunctionKey);
}

TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
	ESPMode::ThreadSafe> FAngelscriptEngine::GetFunctionRouteSnapshot() const
{
	return FunctionRouteState.IsValid()
		? FunctionRouteState->Snapshot
		: TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe>();
}

bool FAngelscriptEngine::RefreshFunctionRouteSnapshotAfterStaticJITChange(
	const bool bValidatedProviderStateWasAppliedAtSafePoint)
{
	// Provider enumeration, entry identity/ABI matching and Live Coding state
	// remain owned by the sibling StaticJIT system. A rejected upstream result
	// cannot publish even an identical replacement snapshot.
	if (!bValidatedProviderStateWasAppliedAtSafePoint
		|| !CacheService.IsValid())
	{
		return false;
	}

	FAngelscriptCacheMutationGuard RouteRefresh = CacheService->EnterMutation(
		EAngelscriptCacheMutationKind::RouteRefresh);
	if (!RouteRefresh.IsEntered())
	{
		return false;
	}

	// RebuildFunctionRouteSnapshot reads only current Engine-owned function
	// state and preserves graph-validated identities from the prior immutable
	// publication. It never consults or changes Current/Previous/Pending roots.
	RebuildFunctionRouteSnapshot();
	return true;
}

void FAngelscriptEngine::RebuildFunctionRouteSnapshot(
	const TConstArrayView<FAngelscriptFunctionRoute>
		VerifiedArtifactRoutes,
	const TConstArrayView<FAngelscriptFunctionArtifactIdentity>
		ValidatedArtifactIdentities,
	const TConstArrayView<asIScriptModule*> RebuiltModules,
	const TConstArrayView<asIScriptModule*> ArtifactInvalidatedModules,
	const TConstArrayView<FAngelscriptFunctionArtifactReferenceSet>
		ValidatedArtifactReferenceSets)
{
	if (!FunctionRouteState.IsValid())
	{
		FunctionRouteState = MakeUnique<FAngelscriptFunctionRouteState>();
	}

	TArray<FAngelscriptFunctionRoute> CandidateRoutes;
	const TSharedPtr<const FAngelscriptFunctionRouteSnapshot,
		ESPMode::ThreadSafe> PreviousSnapshot =
		FunctionRouteState->Snapshot;
	const auto ContainsModule = [](
		const TConstArrayView<asIScriptModule*> Modules,
		const asIScriptModule* Candidate)
	{
		for (const asIScriptModule* Module : Modules)
		{
			if (Module == Candidate)
			{
				return true;
			}
		}
		return false;
	};
	TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = GetActiveModules();
	Modules.Sort([](
		const TSharedRef<FAngelscriptModuleDesc>& Left,
		const TSharedRef<FAngelscriptModuleDesc>& Right)
	{
		return FAngelscriptArtifactCanonicalWriter::CompareCanonicalUtf8Strings(
			Left->ModuleName, Right->ModuleName) < 0;
	});

	for (const TSharedRef<FAngelscriptModuleDesc>& Module : Modules)
	{
		asCModule* ScriptModule = Module->ScriptModule;
		if (ScriptModule == nullptr)
		{
			continue;
		}

		FAngelscriptStableModuleKey ModuleKey;
		for (const FAngelscriptFunctionRoute& Verified
			: VerifiedArtifactRoutes)
		{
			if (Verified.Function != nullptr
				&& static_cast<asCScriptFunction*>(Verified.Function)->module == ScriptModule)
			{
				ModuleKey = Verified.ModuleKey;
				break;
			}
		}
		if (ModuleKey.Hash.IsZero())
		{
			FString ModuleKeyFailure;
			if (!FAngelscriptCacheStableSymbolIdentity::TryBuildModuleKey(
				*Module, ModuleKey, &ModuleKeyFailure))
			{
				UE_LOG(Angelscript, Verbose,
					TEXT("[CacheV2][StableRoute] Module=%s skipped: %s"),
					*Module->ModuleName, *ModuleKeyFailure);
				continue;
			}
		}
		const bool bModuleWasRebuilt = ContainsModule(
			RebuiltModules, ScriptModule);
		const bool bModuleArtifactWasInvalidated = ContainsModule(
			ArtifactInvalidatedModules, ScriptModule);

		for (asUINT FunctionIndex = 0;
			FunctionIndex < ScriptModule->scriptFunctions.GetLength();
			++FunctionIndex)
		{
			asCScriptFunction* Function =
				ScriptModule->scriptFunctions[FunctionIndex];
			if (Function == nullptr || Function->module != ScriptModule)
			{
				continue;
			}

			// A graph-validated restored route is already the identity authority for
			// this exact current function. Reuse it without formatting live AS type
			// metadata, which may not be enumerable during reconstruction.
			const FAngelscriptFunctionRoute* VerifiedByFunction =
				VerifiedArtifactRoutes.FindByPredicate(
					[Function](
						const FAngelscriptFunctionRoute& Entry)
					{
						return Entry.Function == Function
							&& !Entry.Identity.FunctionKey.Hash.IsZero();
					});
			if (VerifiedByFunction != nullptr)
			{
				FAngelscriptFunctionRoute Route = *VerifiedByFunction;
				Route.ModuleKey = ModuleKey;
				Route.Function = Function;
				Route.NumericFunctionId = Function->GetId();
				Route.SelectedExecutionRoute = Function->GetJITBinding().VMEntry != nullptr
					? EAngelscriptFunctionExecutionRoute::Native
					: EAngelscriptFunctionExecutionRoute::Vm;
				Route.MatchResult = Route.SelectedExecutionRoute
						== EAngelscriptFunctionExecutionRoute::Native
					? EAngelscriptArtifactMatchResult::Exact
					: EAngelscriptArtifactMatchResult::MissingProviderEntry;
				Route.bHasVerifiedArtifactIdentity = true;
				CandidateRoutes.Add(MoveTemp(Route));
				continue;
			}

			// Modules not compiled in this transaction may temporarily retain
			// functions whose signature types were retargeted by a dependency full
			// reload. Their previous stable declaration/key is owned value data and
			// remains safe; calling GetDeclaration() here can traverse an already
			// released old asCTypeInfo. A rebuilt module must derive its current key
			// from the new compiler output instead of taking this reuse path.
			const FAngelscriptFunctionRoute* PreviousByFunction =
				PreviousSnapshot.IsValid()
				? PreviousSnapshot->FunctionRoutes.FindByPredicate(
					[Function](
						const FAngelscriptFunctionRoute& Entry)
					{
						return Entry.Function == Function;
					})
				: nullptr;
			if (!bModuleWasRebuilt && PreviousByFunction != nullptr)
			{
				FAngelscriptFunctionRoute Route = *PreviousByFunction;
				Route.ModuleKey = ModuleKey;
				Route.Function = Function;
				Route.NumericFunctionId = Function->GetId();
				Route.SelectedExecutionRoute = Function->GetJITBinding().VMEntry != nullptr
					? EAngelscriptFunctionExecutionRoute::Native
					: EAngelscriptFunctionExecutionRoute::Vm;
				Route.MatchResult = Route.SelectedExecutionRoute
						== EAngelscriptFunctionExecutionRoute::Native
					? EAngelscriptArtifactMatchResult::Exact
					: EAngelscriptArtifactMatchResult::MissingProviderEntry;
				if (bModuleArtifactWasInvalidated)
				{
					const FAngelscriptStableFunctionKey StableKey =
						Route.Identity.FunctionKey;
					Route.Identity = {};
					Route.Identity.FunctionKey = StableKey;
					Route.bHasVerifiedArtifactIdentity = false;
					Route.SelectedExecutionRoute =
						EAngelscriptFunctionExecutionRoute::Vm;
					Route.MatchResult =
						EAngelscriptArtifactMatchResult::MissingArtifactIdentity;
				}
				CandidateRoutes.Add(MoveTemp(Route));
				continue;
			}
			if (bModuleArtifactWasInvalidated)
			{
				UE_LOG(Angelscript, Verbose,
					TEXT("[CacheV2][StableRoute] Function in dependency-invalidated module %s skipped until authoritative recompile"),
					*Module->ModuleName);
				continue;
			}
			FAngelscriptStableFunctionKey FunctionKey;
			if (!FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				ModuleKey, *Function, FunctionKey))
			{
				continue;
			}

			FAngelscriptFunctionRoute Route;
			Route.ModuleKey = ModuleKey;
			Route.Identity.FunctionKey = FunctionKey;
			Route.CanonicalDeclaration = UTF8_TO_TCHAR(
				Function->GetDeclaration(false, false, false));
			Route.Function = Function;
			Route.NumericFunctionId = Function->GetId();
			Route.SelectedExecutionRoute = Function->GetJITBinding().VMEntry != nullptr
				? EAngelscriptFunctionExecutionRoute::Native
				: EAngelscriptFunctionExecutionRoute::Vm;
			Route.MatchResult = Route.SelectedExecutionRoute
					== EAngelscriptFunctionExecutionRoute::Native
				? EAngelscriptArtifactMatchResult::Exact
				: EAngelscriptArtifactMatchResult::MissingProviderEntry;

			const FAngelscriptFunctionRoute* Verified =
				VerifiedArtifactRoutes.FindByPredicate(
					[&Route](const FAngelscriptFunctionRoute& Entry)
					{
						return Entry.Function == Route.Function
							&& Entry.Identity.FunctionKey
								== Route.Identity.FunctionKey;
					});
			if (Verified != nullptr
				&& Verified->bHasVerifiedArtifactIdentity)
			{
				Route.Identity = Verified->Identity;
				Route.bHasVerifiedArtifactIdentity = true;
			}
			else if (FunctionRouteState->Snapshot.IsValid())
			{
				const FAngelscriptFunctionRoute* Previous =
					FunctionRouteState->Snapshot->FunctionRoutes.
						FindByPredicate(
							[&Route](
								const FAngelscriptFunctionRoute& Entry)
							{
								return Entry.Function == Route.Function
									&& Entry.Identity.FunctionKey
										== Route.Identity.FunctionKey;
							});
				if (Previous != nullptr
					&& Previous->bHasVerifiedArtifactIdentity)
				{
					Route.Identity = Previous->Identity;
					Route.bHasVerifiedArtifactIdentity = true;
				}
			}
			CandidateRoutes.Add(MoveTemp(Route));
		}
	}

	// A graph-validated restore may materialize a function whose maintained-fork
	// declaration metadata is not yet enumerable through the normal compiler
	// identity path. The restore DTO is already the authoritative stable
	// identity; merge it only after proving that its transient route belongs to
	// this Engine and to one of the newly active modules.
	for (const FAngelscriptFunctionRoute& Verified
		: VerifiedArtifactRoutes)
	{
		if (Verified.Function == nullptr
			|| Verified.Function->GetEngine() != Engine
			|| static_cast<asCScriptFunction*>(Verified.Function)->module == nullptr
			|| Verified.NumericFunctionId != Verified.Function->GetId()
			|| !ModulesByScriptModule.Contains(static_cast<asCScriptFunction*>(Verified.Function)->module)
			|| Verified.Identity.FunctionKey.Hash.IsZero())
		{
			continue;
		}
		CandidateRoutes.RemoveAll(
			[&Verified](const FAngelscriptFunctionRoute& Candidate)
			{
				return Candidate.Function == Verified.Function;
		});
		FAngelscriptFunctionRoute Route = Verified;
		Route.NumericFunctionId = Route.Function->GetId();
		asCScriptFunction* LiveFunction =
			static_cast<asCScriptFunction*>(Route.Function);
		Route.SelectedExecutionRoute = LiveFunction->GetJITBinding().VMEntry != nullptr
			? EAngelscriptFunctionExecutionRoute::Native
			: EAngelscriptFunctionExecutionRoute::Vm;
		Route.MatchResult = Route.SelectedExecutionRoute
				== EAngelscriptFunctionExecutionRoute::Native
			? EAngelscriptArtifactMatchResult::Exact
			: EAngelscriptArtifactMatchResult::MissingProviderEntry;
		Route.bHasVerifiedArtifactIdentity = true;
		if (Route.CanonicalDeclaration.IsEmpty())
		{
			Route.CanonicalDeclaration = UTF8_TO_TCHAR(
				Route.Function->GetDeclaration(false, false, false));
		}
		CandidateRoutes.Add(MoveTemp(Route));
	}

	// Normal compile capture derives these tuples from the sole validated
	// ModuleSnapshot graph. Join them to the already-proven current-Engine route
	// by the full StableFunctionKey; never recompute content/profile from live
	// pointers and never publish an identity for a missing current function.
	for (const FAngelscriptFunctionArtifactIdentity& ValidatedIdentity
		: ValidatedArtifactIdentities)
	{
		if (ValidatedIdentity.FunctionKey.Hash.IsZero()
			|| ValidatedIdentity.Content.Execution.IsZero()
			|| ValidatedIdentity.Content.Debug.IsZero()
			|| ValidatedIdentity.Profile.Hash.IsZero())
		{
			continue;
		}
		FAngelscriptFunctionRoute* Route =
			CandidateRoutes.FindByPredicate(
				[&ValidatedIdentity](
					FAngelscriptFunctionRoute& Candidate)
				{
					return Candidate.Identity.FunctionKey
						== ValidatedIdentity.FunctionKey;
				});
		if (Route != nullptr)
		{
			Route->Identity = ValidatedIdentity;
			Route->bHasVerifiedArtifactIdentity = true;
		}
	}

	CandidateRoutes.Sort([](
		const FAngelscriptFunctionRoute& Left,
		const FAngelscriptFunctionRoute& Right)
	{
		return Left.Identity.FunctionKey.Hash
			< Right.Identity.FunctionKey.Hash;
	});

	TSharedRef<FAngelscriptFunctionRouteSnapshot,
		ESPMode::ThreadSafe> Published =
		MakeShared<FAngelscriptFunctionRouteSnapshot,
			ESPMode::ThreadSafe>();
	Published->Generation = FAngelscriptFunctionRouteGeneration{
		FunctionRouteState->NextGenerationValue++};
	const bool bRouteOnlyRefresh = VerifiedArtifactRoutes.IsEmpty()
		&& ValidatedArtifactIdentities.IsEmpty()
		&& RebuiltModules.IsEmpty()
		&& ArtifactInvalidatedModules.IsEmpty()
		&& ValidatedArtifactReferenceSets.IsEmpty();
	if (bRouteOnlyRefresh && PreviousSnapshot.IsValid())
	{
		Published->ValidatedFunctionArtifactReferenceSets =
			PreviousSnapshot->ValidatedFunctionArtifactReferenceSets;
	}
	else
	{
		for (const FAngelscriptFunctionArtifactReferenceSet& ReferenceSet
			: ValidatedArtifactReferenceSets)
		{
			const FAngelscriptFunctionRoute* const VerifiedRoute =
				CandidateRoutes.FindByPredicate(
					[&ReferenceSet](const FAngelscriptFunctionRoute& Candidate)
					{
						return Candidate.bHasVerifiedArtifactIdentity
							&& Candidate.Identity.FunctionKey
								== ReferenceSet.FunctionKey;
					});
			if (VerifiedRoute != nullptr)
			{
				Published->ValidatedFunctionArtifactReferenceSets.Add(
					ReferenceSet);
			}
		}
		Published->ValidatedFunctionArtifactReferenceSets.Sort([](
			const FAngelscriptFunctionArtifactReferenceSet& Left,
			const FAngelscriptFunctionArtifactReferenceSet& Right)
		{
			return Left.FunctionKey.Hash < Right.FunctionKey.Hash;
		});
	}
	for (int32 Index = 0; Index < CandidateRoutes.Num();)
	{
		int32 DuplicateEnd = Index + 1;
		while (DuplicateEnd < CandidateRoutes.Num()
			&& CandidateRoutes[DuplicateEnd].Identity.FunctionKey
				== CandidateRoutes[Index].Identity.FunctionKey)
		{
			++DuplicateEnd;
		}
		if (DuplicateEnd == Index + 1)
		{
			FAngelscriptFunctionRoute& Route =
				Published->FunctionRoutes.Add_GetRef(
					MoveTemp(CandidateRoutes[Index]));
			Route.Generation = Published->Generation;
			if (Route.SelectedExecutionRoute
				== EAngelscriptFunctionExecutionRoute::Native)
			{
				++Published->NativeRouteCount;
			}
			else
			{
				++Published->VmRouteCount;
			}
		}
		else
		{
			UE_LOG(Angelscript, Error,
				TEXT("[CacheV2][StableRoute] Rejected %d duplicate live routes for StableFunctionKey=%s"),
				DuplicateEnd - Index,
				*CandidateRoutes[Index].Identity.FunctionKey.Hash.ToHexString());
		}
		Index = DuplicateEnd;
	}

	FunctionRouteState->Snapshot = Published;
	if (CacheService.IsValid())
	{
		for (const FAngelscriptFunctionRoute& Route
			: Published->FunctionRoutes)
		{
			FAngelscriptCacheDecisionEvent Event;
			Event.TransactionOrdinal = Published->Generation.Value;
			Event.Stage = EAngelscriptCacheDecisionStage::StableRoute;
			Event.Outcome = EAngelscriptCacheDecisionOutcome::Published;
			Event.ReasonDomain =
				EAngelscriptCacheDecisionReasonDomain::StableRoute;
			Event.ReasonCode =
				static_cast<uint32>(Route.SelectedExecutionRoute);
			Event.ModuleKeys.Add(Route.ModuleKey);
			Event.FunctionKey = Route.Identity.FunctionKey;
			if (Route.bHasVerifiedArtifactIdentity)
			{
				Event.CurrentCoordinate = Route.Identity.Content.Execution;
				Event.Profile = Route.Identity.Profile;
			}
			Event.PrimaryCount = 1;
			Event.SecondaryCount = Route.bHasVerifiedArtifactIdentity ? 1 : 0;
			CacheService->RecordDecisionEvent(MoveTemp(Event));
		}
	}
	UE_LOG(Angelscript, Verbose,
		TEXT("[CacheV2][StableRoute] Published Engine=%p Revision=%llu Routes=%d VM=%u Native=%u"),
		this,
		Published->Generation.Value,
		Published->FunctionRoutes.Num(),
		Published->VmRouteCount,
		Published->NativeRouteCount);
}
