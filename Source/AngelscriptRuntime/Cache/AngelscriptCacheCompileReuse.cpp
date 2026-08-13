#include "Cache/AngelscriptCacheCompileReuse.h"

#include "Cache/AngelscriptCacheCompilerBridge.h"
#include "Cache/AngelscriptCacheCurrentModuleAuthority.h"
#include "Cache/AngelscriptCacheEnvironment.h"
#include "Cache/AngelscriptCacheService.h"
#include "Cache/AngelscriptCacheStableSymbolIdentity.h"
#include "Cache/AngelscriptFunctionArtifactCodec.h"
#include "Core/AngelscriptEngine.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"

#include "as_module.h"
#include "as_scriptengine.h"

namespace AngelscriptCacheCompileReuse_Private
{
	static FAngelscriptCacheCompileReusePrepareResult Failure(
		const EAngelscriptCacheCompileReusePrepareError Error,
		FString Detail,
		const TOptional<FAngelscriptCacheValidationResult>& Validation = {})
	{
		FAngelscriptCacheCompileReusePrepareResult Result;
		Result.Error = Error;
		Result.Detail = MoveTemp(Detail);
		Result.Validation = Validation;
		return Result;
	}

	static const FAngelscriptDecodedCacheRecord* FindRecord(
		const FAngelscriptValidatedGeneration& Generation,
		const FAngelscriptCacheRecordId& RecordId)
	{
		for (const FAngelscriptDecodedCacheRecordHandle& Record
			: Generation.ReachableRecords)
		{
			if (Record->GetRecordId() == RecordId)
			{
				return &Record.Get();
			}
		}
		return nullptr;
	}

	static EAngelscriptCacheDecisionOutcome MapOutcome(
		const EAngelscriptCacheFunctionCandidateLookupStatus Status)
	{
		switch (Status)
		{
		case EAngelscriptCacheFunctionCandidateLookupStatus::Restored:
			return EAngelscriptCacheDecisionOutcome::Restored;
		case EAngelscriptCacheFunctionCandidateLookupStatus::RejectedCorrupt:
			return EAngelscriptCacheDecisionOutcome::Rejected;
		case EAngelscriptCacheFunctionCandidateLookupStatus::NotCacheable:
			return EAngelscriptCacheDecisionOutcome::NotCacheable;
		default:
			return EAngelscriptCacheDecisionOutcome::Miss;
		}
	}

	static FString BuildInvocationDetail(
		const asSBuildArtifactInvocation& Invocation,
		const asCScriptFunction& Function,
		const FString& OutcomeDetail)
	{
		return FString::Printf(
			TEXT("InvocationKind=%u Generated=%d Declaration=%s; %s"),
			static_cast<uint32>(Invocation.kind),
			Invocation.isGenerated ? 1 : 0,
			UTF8_TO_TCHAR(Function.GetDeclaration(false, false, false)),
			*OutcomeDetail);
	}

	static bool IsDerivedStaticClassHelper(
		const asSBuildArtifactInvocation& Invocation,
		const asCScriptFunction& Function)
	{
		return Invocation.kind
				== asBUILD_ARTIFACT_INVOCATION_GLOBAL_FUNCTION
			&& Function.GetObjectType() == nullptr
			&& FCStringAnsi::Strcmp(Function.GetName(), "StaticClass") == 0
			&& FCStringAnsi::Strcmp(
				Function.GetDeclaration(false, false, false),
				"UClass StaticClass()") == 0
			&& Function.traits.GetTrait(asTRAIT_GENERATED_FUNCTION);
	}

	static bool AreArtifactAliasParameterTypesEquivalent(
		const asCDataType& Left,
		const asCDataType& Right)
	{
		if (Left == Right)
		{
			return true;
		}
		// Top-level const on a by-value parameter does not change the callable
		// contract. AngelScript's full-module bytecode restore can retain it on
		// the previous module generation even when the replacement source
		// function prints the unqualified value type. Const on references or
		// handles remains semantic and must not be normalized here.
		return !Left.IsReference() && !Right.IsReference()
			&& !Left.IsObjectHandle() && !Right.IsObjectHandle()
			&& Left.IsEqualExceptConst(Right);
	}

	static bool AreArtifactAliasSignaturesEquivalent(
		const asCScriptFunction& Left,
		const asCScriptFunction& Right)
	{
		if (Left.name != Right.name
			|| Left.returnType != Right.returnType
			|| Left.IsReadOnly() != Right.IsReadOnly()
			|| Left.inOutFlags != Right.inOutFlags
			|| Left.parameterTypes.GetLength()
				!= Right.parameterTypes.GetLength()
			|| (Left.objectType != nullptr) != (Right.objectType != nullptr))
		{
			return false;
		}
		if (Left.objectType != nullptr
			&& (FCStringAnsi::Strcmp(Left.objectType->GetName(),
					Right.objectType->GetName()) != 0
				|| FCStringAnsi::Strcmp(Left.objectType->GetNamespace(),
					Right.objectType->GetNamespace()) != 0))
		{
			return false;
		}
		for (asUINT ParameterIndex = 0;
			ParameterIndex < Left.parameterTypes.GetLength();
			++ParameterIndex)
		{
			if (!AreArtifactAliasParameterTypesEquivalent(
				Left.parameterTypes[ParameterIndex],
				Right.parameterTypes[ParameterIndex]))
			{
				return false;
			}
		}
		return true;
	}
}

struct FAngelscriptCacheCompileReuseContext::FImpl
{
	struct FPreparedExternalFunction final
	{
		const asCScriptFunction* Function = nullptr;
		FAngelscriptArtifactReference Reference;
		FAngelscriptHash256 ExecutionContent;
	};

	struct FModuleState final
	{
		FImpl* Owner = nullptr;
		FAngelscriptStableModuleKey ModuleKey;
		FAngelscriptValidatedModuleGraph Graph;
		FAngelscriptCachedModuleInterface CurrentInterface;
		TArray<FAngelscriptCachedTypeSchema> CurrentTypes;
		FAngelscriptCachedModuleState CurrentState;
		TArray<FAngelscriptCachedFunctionBody> CurrentFunctions;
		TArray<FPreparedExternalFunction> PreparedExternalFunctions;
		TUniquePtr<FAngelscriptCacheEngineEnvironmentResolver> ExternalSymbols;
		FAngelscriptCacheService* Service = nullptr;
		mutable FCriticalSection RestoredDependenciesMutex;
		TMap<FBlake3Hash, TArray<FAngelscriptCacheSemanticDependency>>
			RestoredActualDependencies;
	};

	TUniquePtr<FAngelscriptCacheReadSession> Session;
	FAngelscriptHash256 GenerationId;
	FAngelscriptCacheCleanCaptureOptions Options;
	FAngelscriptCacheReadLimits Limits;
	FAngelscriptCacheReadBudget Budget;
	TArray<TUniquePtr<FModuleState>> Modules;
	mutable FCriticalSection SummaryMutex;
	FAngelscriptCacheFunctionReuseSummary Summary;

	// Combines immutable application registrations with exact-source script
	// functions prepared earlier in this compile-reuse transaction. The
	// compiler prepares dependency modules before their consumers, so a
	// consumer can validate and relocate a foreign ScriptFunction without
	// publishing any partially compiled module as global Engine authority.
	class FPreparedFunctionAuthority final
		: public IAngelscriptCacheCurrentSymbolResolver
		, public IAngelscriptFunctionArtifactExternalFunctionResolver
	{
	public:
		FPreparedFunctionAuthority(
			const FAngelscriptCacheEngineEnvironmentResolver& InEnvironment,
			const FImpl& InOwner)
			: Environment(InEnvironment)
			, Owner(InOwner)
		{
		}

		virtual TOptional<FAngelscriptCacheCurrentSymbol> Resolve(
			const EAngelscriptArtifactReferenceKind ReferenceKind,
			const FAngelscriptHash256& StableKey) const override
		{
			if (ReferenceKind
				== EAngelscriptArtifactReferenceKind::ScriptFunction)
			{
				const FPreparedExternalFunction* Match = nullptr;
				for (const TUniquePtr<FModuleState>& State : Owner.Modules)
				{
					if (!State.IsValid())
					{
						continue;
					}
					for (const FPreparedExternalFunction& Function
						: State->PreparedExternalFunctions)
					{
						if (Function.Reference.StableKey != StableKey)
						{
							continue;
						}
						if (Match != nullptr
							&& (Match->Reference != Function.Reference
								|| Match->ExecutionContent
									!= Function.ExecutionContent))
						{
							return {};
						}
						Match = &Function;
					}
				}
				if (Match != nullptr)
				{
					return FAngelscriptCacheCurrentSymbol{
						Match->Reference.ExpectedAbi,
						Match->ExecutionContent,
						{}};
				}
			}
			return Environment.Resolve(ReferenceKind, StableKey);
		}

		virtual bool TryResolveExternalScriptFunction(
			const asCScriptFunction& Function,
			FAngelscriptArtifactReference& OutReference) const override
		{
			OutReference = {};
			const FPreparedExternalFunction* Match = nullptr;
			auto MatchPreparedFunction = [this, &Match](
				const asCScriptFunction* Target) -> bool
			{
				if (Target == nullptr)
				{
					return true;
				}
				for (const TUniquePtr<FModuleState>& State : Owner.Modules)
				{
					if (!State.IsValid())
					{
						continue;
					}
					for (const FPreparedExternalFunction& Candidate
						: State->PreparedExternalFunctions)
					{
						if (Candidate.Function != Target)
						{
							continue;
						}
						if (Match != nullptr
							&& Match->Reference != Candidate.Reference)
						{
							return false;
						}
						Match = &Candidate;
					}
				}
				return true;
			};

			if (!MatchPreparedFunction(&Function))
			{
				return false;
			}

			// VM bytecode refers to an imported function through the import
			// signature object. That object is intentionally distinct from the
			// provider's scriptFunctions-table entry and may print harmless
			// declaration normalization differences such as a top-level const.
			// Follow the current Engine's exact boundFunctionId instead of
			// guessing declaration equivalence, then require that provider target
			// to be one prepared exact-source authority in this transaction.
			if (Match == nullptr && (Function.id & FUNC_IMPORTED) != 0
				&& Function.engine != nullptr)
			{
				const int32 ImportIndex = Function.id & ~FUNC_IMPORTED;
				if (ImportIndex < 0
					|| ImportIndex >= static_cast<int32>(
						Function.engine->importedFunctions.GetLength()))
				{
					return false;
				}
				const sBindInfo* BindInfo =
					Function.engine->importedFunctions[ImportIndex];
				if (BindInfo == nullptr
					|| BindInfo->importedFunctionSignature != &Function
					|| BindInfo->boundFunctionId < 0)
				{
					return false;
				}
				if (!MatchPreparedFunction(Function.engine->GetScriptFunction(
					BindInfo->boundFunctionId)))
				{
					return false;
				}
			}

			// Function-artifact restore resolves an `x` reference against the
			// Engine's currently published module. During compile reuse that can
			// still be the previous exact-source module object, while the prepared
			// authority belongs to the replacement module staged by this
			// transaction. Match those generations by the same semantic module and
			// exact provider-local stable key first. The narrow structural fallback
			// handles only full-bytecode normalization such as top-level const on a
			// by-value parameter. The final stable key/ABI always comes from the one
			// prepared candidate; duplicate matches are rejected rather than selected
			// by order.
			if (Match == nullptr && Function.module != nullptr
				&& Function.module->baseModuleName.GetLength() != 0)
			{
				for (const TUniquePtr<FModuleState>& State : Owner.Modules)
				{
					if (!State.IsValid())
					{
						continue;
					}
					FAngelscriptStableFunctionKey AliasKey;
					const bool bHasExactAliasKey =
						FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
							State->ModuleKey, Function, AliasKey);
					for (const FPreparedExternalFunction& Candidate
						: State->PreparedExternalFunctions)
					{
						if (Candidate.Function == nullptr
							|| Candidate.Function->module == nullptr
							|| Candidate.Function->module->baseModuleName
								!= Function.module->baseModuleName
							|| (!(bHasExactAliasKey
									&& Candidate.Reference.StableKey
										== AliasKey.Hash)
								&& (Candidate.Function->funcType != Function.funcType
									|| Candidate.Function->artifactInvocationKind
										!= Function.artifactInvocationKind
									|| FCStringAnsi::Strcmp(
										Candidate.Function->GetNamespace(),
										Function.GetNamespace()) != 0
									|| !AngelscriptCacheCompileReuse_Private::
										AreArtifactAliasSignaturesEquivalent(
											Function, *Candidate.Function))))
						{
							continue;
						}
						if (Match != nullptr
							&& Match->Reference != Candidate.Reference)
						{
							return false;
						}
						Match = &Candidate;
					}
				}
			}
			if (Match == nullptr
				|| Match->Reference.Kind
					!= EAngelscriptArtifactReferenceKind::ScriptFunction
				|| Match->Reference.StableKey.IsZero()
				|| Match->Reference.ExpectedAbi.IsZero())
			{
				return false;
			}
			OutReference = Match->Reference;
			return true;
		}

	private:
		const FAngelscriptCacheEngineEnvironmentResolver& Environment;
		const FImpl& Owner;
	};

	static bool BuildPreparedExternalFunctions(
		FModuleState& State,
		asCModule& ScriptModule,
		FString& OutFailure)
	{
		State.PreparedExternalFunctions.Reset();
		OutFailure.Reset();
		for (asUINT FunctionIndex = 0;
			FunctionIndex < ScriptModule.scriptFunctions.GetLength();
			++FunctionIndex)
		{
			asCScriptFunction* Function =
				ScriptModule.scriptFunctions[FunctionIndex];
			if (Function == nullptr || Function->module != &ScriptModule)
			{
				OutFailure = TEXT(
					"The staged module function table contains a null or foreign entry");
				return false;
			}

			FAngelscriptStableFunctionKey FunctionKey;
			FString KeyFailure;
			if (!FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				State.ModuleKey, *Function, FunctionKey, &KeyFailure))
			{
				OutFailure = FString::Printf(
					TEXT("Staged function has no stable key: %s"), *KeyFailure);
				return false;
			}

			const FAngelscriptCachedFunctionBody* MatchingBody = nullptr;
			for (const FAngelscriptCachedFunctionBody& Body
				: State.CurrentFunctions)
			{
				if (Body.Identity.FunctionKey != FunctionKey)
				{
					continue;
				}
				if (MatchingBody != nullptr)
				{
					OutFailure = FString::Printf(
						TEXT("Stable function key %s has ambiguous exact-source bodies"),
						*FunctionKey.Hash.ToHexString());
					return false;
				}
				MatchingBody = &Body;
			}
			// Generated helpers such as derived StaticClass are intentionally not
			// persisted as FunctionBody records and therefore publish no authority.
			if (MatchingBody == nullptr)
			{
				continue;
			}
			if (MatchingBody->ModuleKey != State.ModuleKey
				|| MatchingBody->ExpectedDeclarationAbi.IsZero()
				|| MatchingBody->Identity.Content.Execution.IsZero()
				|| State.PreparedExternalFunctions.ContainsByPredicate(
					[&FunctionKey](const FPreparedExternalFunction& Existing)
					{
						return Existing.Reference.StableKey
							== FunctionKey.Hash;
					}))
			{
				OutFailure = FString::Printf(
					TEXT("Stable function key %s has invalid or duplicate exact-source authority"),
					*FunctionKey.Hash.ToHexString());
				return false;
			}

			FPreparedExternalFunction& Prepared =
				State.PreparedExternalFunctions.AddDefaulted_GetRef();
			Prepared.Function = Function;
			Prepared.Reference.Kind =
				EAngelscriptArtifactReferenceKind::ScriptFunction;
			Prepared.Reference.StableKey = FunctionKey.Hash;
			Prepared.Reference.ExpectedAbi =
				MatchingBody->ExpectedDeclarationAbi;
			Prepared.ExecutionContent =
				MatchingBody->Identity.Content.Execution;
		}
		return true;
	}

	static void RecordLookup(
		FModuleState& State,
		const asSBuildArtifactInvocation& Invocation,
		const asCScriptFunction& Function,
		const FAngelscriptStableFunctionKey& FunctionKey,
		const FAngelscriptCacheFunctionCandidateLookupResult& Lookup,
		const uint64 ElapsedMicroseconds)
	{
		if (State.Owner != nullptr)
		{
			FScopeLock SummaryLock(&State.Owner->SummaryMutex);
			switch (Lookup.Status)
			{
			case EAngelscriptCacheFunctionCandidateLookupStatus::Restored:
				++State.Owner->Summary.RestoredFunctionCount;
				break;
			case EAngelscriptCacheFunctionCandidateLookupStatus::NotCacheable:
				++State.Owner->Summary.NotCacheableCount;
				break;
			case EAngelscriptCacheFunctionCandidateLookupStatus::RejectedCorrupt:
				++State.Owner->Summary.RejectedCorruptCount;
				break;
			default:
				break;
			}
		}
		if (State.Service == nullptr)
		{
			return;
		}
		FAngelscriptCacheDecisionEvent Event;
		Event.Stage = EAngelscriptCacheDecisionStage::FunctionLookup;
		Event.Outcome = AngelscriptCacheCompileReuse_Private::MapOutcome(
			Lookup.Status);
		Event.ReasonDomain =
			EAngelscriptCacheDecisionReasonDomain::FunctionLookup;
		Event.ReasonCode = static_cast<uint32>(Lookup.Status);
		Event.ModuleKeys.Add(State.ModuleKey);
		Event.FunctionKey = FunctionKey;
		Event.ExpectedCoordinate = State.Owner->GenerationId;
		if (!Lookup.CurrentInputDigest.Hash.IsZero())
		{
			Event.CurrentCoordinate = Lookup.CurrentInputDigest.Hash;
		}
		else if (!Lookup.CurrentSourceDigest.Hash.IsZero())
		{
			Event.CurrentCoordinate = Lookup.CurrentSourceDigest.Hash;
		}
		Event.Profile = State.Owner->Options.Profile;
		Event.SourceSnapshot = State.Owner->Session->GetGeneration().
			Manifest.SourceSnapshot;
		Event.ElapsedMicroseconds = ElapsedMicroseconds;
		Event.Detail = AngelscriptCacheCompileReuse_Private::
			BuildInvocationDetail(Invocation, Function, Lookup.Detail);
		State.Service->RecordDecisionEvent(MoveTemp(Event));
	}

	static asEBuildArtifactRestoreResult RestoreFunction(
		const asSBuildArtifactInvocation* Invocation,
		asCScriptFunction* Function,
		void* UserData)
	{
		if (Invocation == nullptr || Function == nullptr || UserData == nullptr)
		{
			return asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
		}
		FModuleState& State = *static_cast<FModuleState*>(UserData);
		if (State.Owner == nullptr || !State.Owner->Session.IsValid())
		{
			return asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
		}

		const double StartedSeconds = FPlatformTime::Seconds();
		FAngelscriptStableFunctionKey FunctionKey;
		FString KeyFailure;
		if (!FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
			State.ModuleKey, *Function, FunctionKey, &KeyFailure))
		{
			FAngelscriptCacheFunctionCandidateLookupResult Lookup;
			Lookup.Status =
				EAngelscriptCacheFunctionCandidateLookupStatus::NotCacheable;
			Lookup.RestoreResult = asBUILD_ARTIFACT_RESTORE_NOT_CACHEABLE;
			Lookup.Detail = MoveTemp(KeyFailure);
			RecordLookup(State, *Invocation, *Function, FunctionKey, Lookup,
				static_cast<uint64>(
				(FPlatformTime::Seconds() - StartedSeconds) * 1000000.0));
			return Lookup.RestoreResult;
		}
		if (AngelscriptCacheCompileReuse_Private::IsDerivedStaticClassHelper(
			*Invocation, *Function))
		{
			FAngelscriptCacheFunctionCandidateLookupResult Lookup;
			Lookup.Status =
				EAngelscriptCacheFunctionCandidateLookupStatus::NotCacheable;
			Lookup.RestoreResult = asBUILD_ARTIFACT_RESTORE_NOT_CACHEABLE;
			Lookup.Detail = TEXT(
				"Derived StaticClass helper is reconstructed from current type/ClassGenerator authority and has no persisted FunctionBody");
			RecordLookup(State, *Invocation, *Function, FunctionKey, Lookup,
				static_cast<uint64>(
					(FPlatformTime::Seconds() - StartedSeconds) * 1000000.0));
			return Lookup.RestoreResult;
		}

		FAngelscriptCacheFunctionInputAuthorities Authorities;
		Authorities.ModuleInterface = &State.CurrentInterface;
		Authorities.TypeSchemas = State.CurrentTypes;
		Authorities.ModuleState = &State.CurrentState;
		Authorities.FunctionBodies = State.CurrentFunctions;
		FPreparedFunctionAuthority CurrentSymbols(
			*State.ExternalSymbols, *State.Owner);
		Authorities.ExternalSymbols = &CurrentSymbols;
		Authorities.ExternalFunctions = &CurrentSymbols;
		const FAngelscriptCacheFunctionCandidateLookupResult Lookup =
			FAngelscriptCacheCompilerBridge::TryRestoreFunctionFromValidatedGraph(
				State.Graph,
				*Invocation,
				*Function,
				FunctionKey,
				State.Owner->Options.Profile,
				State.Owner->Options.CanonicalCompileOptions,
				Authorities,
				State.Owner->Limits,
				State.Owner->Budget);
		if (Lookup.Status
			== EAngelscriptCacheFunctionCandidateLookupStatus::Restored)
		{
			FScopeLock Lock(&State.RestoredDependenciesMutex);
			State.RestoredActualDependencies.Add(
				FunctionKey.Hash.Value,
				Lookup.RestoredActualDependencies);
		}
		RecordLookup(State, *Invocation, *Function, FunctionKey, Lookup,
			static_cast<uint64>(
			(FPlatformTime::Seconds() - StartedSeconds) * 1000000.0));
		return Lookup.RestoreResult;
	}

	static void ObserveCompileResult(
		const asSBuildArtifactInvocation* Invocation,
		const asSBuildArtifactCompileResult* Result,
		void* UserData)
	{
		if (Invocation == nullptr || Result == nullptr || UserData == nullptr
			|| Result->function == nullptr || !Result->compilerInvoked)
		{
			return;
		}
		FModuleState& State = *static_cast<FModuleState*>(UserData);
		if (State.Service == nullptr || State.Owner == nullptr)
		{
			return;
		}
		FAngelscriptStableFunctionKey FunctionKey;
		FString KeyFailure;
		if (!FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
			State.ModuleKey, *Result->function, FunctionKey, &KeyFailure))
		{
			return;
		}
		{
			FScopeLock SummaryLock(&State.Owner->SummaryMutex);
			++State.Owner->Summary.CompiledMissCount;
		}
		FAngelscriptCacheDecisionEvent Event;
		Event.Stage = EAngelscriptCacheDecisionStage::FunctionLookup;
		Event.Outcome = EAngelscriptCacheDecisionOutcome::Compiled;
		Event.ReasonDomain =
			EAngelscriptCacheDecisionReasonDomain::FunctionLookup;
		Event.ReasonCode = static_cast<uint32>(Result->restoreResult);
		Event.ModuleKeys.Add(State.ModuleKey);
		Event.FunctionKey = FunctionKey;
		Event.ExpectedCoordinate = State.Owner->GenerationId;
		Event.Profile = State.Owner->Options.Profile;
		Event.SourceSnapshot = State.Owner->Session->GetGeneration().
			Manifest.SourceSnapshot;
		Event.PrimaryCount = 1;
		const FString OutcomeDetail = Result->succeeded
			? TEXT("Candidate miss compiled successfully")
			: TEXT("Candidate miss compiler invocation failed");
		Event.Detail = AngelscriptCacheCompileReuse_Private::
			BuildInvocationDetail(
				*Invocation, *Result->function, OutcomeDetail);
		State.Service->RecordDecisionEvent(MoveTemp(Event));
	}
};

FAngelscriptCacheCompileReuseContext::FAngelscriptCacheCompileReuseContext()
	: Impl(MakeUnique<FImpl>())
{
}

FAngelscriptCacheCompileReuseContext::~FAngelscriptCacheCompileReuseContext() =
	default;

TUniquePtr<FAngelscriptCacheCompileReuseContext>
FAngelscriptCacheCompileReuseContext::Create(
	TUniquePtr<FAngelscriptCacheReadSession>&& Session,
	const FAngelscriptHash256& GenerationId,
	const FAngelscriptCacheCleanCaptureOptions& CurrentOptions)
{
	if (!Session.IsValid() || GenerationId.IsZero()
		|| Session->GetGenerationId() != GenerationId
		|| CurrentOptions.Profile.Hash.IsZero()
		|| Session->GetGeneration().Manifest.Profile.Hash
			!= CurrentOptions.Profile.Hash)
	{
		return nullptr;
	}
	TUniquePtr<FAngelscriptCacheCompileReuseContext> Context(
		new FAngelscriptCacheCompileReuseContext());
	Context->Impl->Session = MoveTemp(Session);
	Context->Impl->GenerationId = GenerationId;
	Context->Impl->Options = CurrentOptions;
	Context->Impl->Summary.bPresent = true;
	Context->Impl->Summary.CandidateGenerationId = GenerationId;
	return Context;
}

bool FAngelscriptCacheCompileReuseContext::IsValid() const
{
	return Impl.IsValid() && Impl->Session.IsValid()
		&& !Impl->GenerationId.IsZero();
}

FAngelscriptCacheFunctionReuseSummary
FAngelscriptCacheCompileReuseContext::CaptureSummary() const
{
	if (!Impl.IsValid())
	{
		return {};
	}
	FScopeLock SummaryLock(&Impl->SummaryMutex);
	return Impl->Summary;
}

bool FAngelscriptCacheCompileReuseContext::
TryBuildDependencyOrderedModuleKeys(
	const TConstArrayView<FAngelscriptStableModuleKey> SelectedKeys,
	TArray<FAngelscriptStableModuleKey>& OutOrderedKeys,
	FString& OutFailure) const
{
	struct FFunctionOwner final
	{
		FAngelscriptHash256 FunctionKey;
		FAngelscriptStableModuleKey ModuleKey;
	};
	struct FModuleOrderNode final
	{
		FAngelscriptStableModuleKey ModuleKey;
		TArray<int32> ProviderIndices;
		bool bEmitted = false;
	};

	OutOrderedKeys.Reset();
	OutFailure.Reset();
	if (!IsValid())
	{
		OutFailure = TEXT("Compile-reuse ordering requires a selected Generation");
		return false;
	}

	TArray<FModuleOrderNode> Nodes;
	Nodes.Reserve(SelectedKeys.Num());
	for (const FAngelscriptStableModuleKey& Key : SelectedKeys)
	{
		if (Key.Hash.IsZero()
			|| Nodes.ContainsByPredicate(
				[&Key](const FModuleOrderNode& Existing)
				{
					return Existing.ModuleKey == Key;
				}))
		{
			OutFailure = TEXT(
				"Compile-reuse ordering received a zero or duplicate module key");
			return false;
		}
		Nodes.Add({Key, {}, false});
	}
	const auto FindSelectedModuleIndex = [&Nodes](
		const FAngelscriptStableModuleKey& Key) -> int32
	{
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			if (Nodes[Index].ModuleKey == Key)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	};

	const FAngelscriptValidatedGeneration& Generation =
		Impl->Session->GetGeneration();
	TArray<FFunctionOwner> Owners;
	for (const FAngelscriptDecodedCacheRecordHandle& Record
		: Generation.ReachableRecords)
	{
		const FAngelscriptCachedFunctionBody* Body =
			Record->TryGetFunctionBody();
		if (Body == nullptr)
		{
			continue;
		}
		const FFunctionOwner* Existing = Owners.FindByPredicate(
			[Body](const FFunctionOwner& Owner)
			{
				return Owner.FunctionKey
					== Body->Identity.FunctionKey.Hash;
			});
		if (Existing != nullptr && Existing->ModuleKey != Body->ModuleKey)
		{
			OutFailure = TEXT(
				"A stable function key has more than one module owner");
			return false;
		}
		if (Existing == nullptr)
		{
			Owners.Add({Body->Identity.FunctionKey.Hash, Body->ModuleKey});
		}
	}

	for (const FAngelscriptDecodedCacheRecordHandle& Record
		: Generation.ReachableRecords)
	{
		const FAngelscriptCachedFunctionBody* Body =
			Record->TryGetFunctionBody();
		const int32 ConsumerIndex = Body != nullptr
			? FindSelectedModuleIndex(Body->ModuleKey) : INDEX_NONE;
		if (Body == nullptr || ConsumerIndex == INDEX_NONE)
		{
			continue;
		}
		for (const FAngelscriptCacheSemanticDependency& Dependency
			: Body->ActualDependencies)
		{
			if (Dependency.Target.Kind
				!= EAngelscriptArtifactReferenceKind::ScriptFunction)
			{
				continue;
			}
			const FFunctionOwner* Provider = Owners.FindByPredicate(
				[&Dependency](const FFunctionOwner& Owner)
				{
					return Owner.FunctionKey
						== Dependency.Target.StableKey;
				});
			if (Provider == nullptr || Provider->ModuleKey == Body->ModuleKey)
			{
				continue;
			}
			const int32 ProviderIndex =
				FindSelectedModuleIndex(Provider->ModuleKey);
			if (ProviderIndex == INDEX_NONE)
			{
				OutFailure = TEXT(
					"A selected module requires an unselected script-function provider");
				return false;
			}
			Nodes[ConsumerIndex].ProviderIndices.AddUnique(ProviderIndex);
		}
	}

	OutOrderedKeys.Reserve(Nodes.Num());
	while (OutOrderedKeys.Num() < Nodes.Num())
	{
		int32 ReadyIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			if (Nodes[Index].bEmitted)
			{
				continue;
			}
			bool bProvidersReady = true;
			for (const int32 ProviderIndex : Nodes[Index].ProviderIndices)
			{
				bProvidersReady &= Nodes[ProviderIndex].bEmitted;
			}
			if (bProvidersReady)
			{
				ReadyIndex = Index;
				break;
			}
		}
		if (ReadyIndex == INDEX_NONE)
		{
			OutOrderedKeys.Reset();
			OutFailure = TEXT(
				"Cross-module function dependencies contain a compile-reuse cycle");
			return false;
		}
		Nodes[ReadyIndex].bEmitted = true;
		OutOrderedKeys.Add(Nodes[ReadyIndex].ModuleKey);
	}
	return true;
}

bool FAngelscriptCacheCompileReuseContext::TryCopyActualDependencies(
	const FAngelscriptStableModuleKey& ModuleKey,
	const FAngelscriptStableFunctionKey& FunctionKey,
	TArray<FAngelscriptCacheSemanticDependency>& OutDependencies) const
{
	OutDependencies.Reset();
	if (!IsValid() || ModuleKey.Hash.IsZero() || FunctionKey.Hash.IsZero())
	{
		return false;
	}
	for (const TUniquePtr<FImpl::FModuleState>& State : Impl->Modules)
	{
		if (!State.IsValid() || State->ModuleKey != ModuleKey)
		{
			continue;
		}
		FScopeLock Lock(&State->RestoredDependenciesMutex);
		const TArray<FAngelscriptCacheSemanticDependency>* Dependencies =
			State->RestoredActualDependencies.Find(FunctionKey.Hash.Value);
		if (Dependencies == nullptr)
		{
			return false;
		}
		OutDependencies = *Dependencies;
		return true;
	}
	return false;
}

FAngelscriptCacheCompileReusePrepareResult
FAngelscriptCacheCompileReuseContext::PrepareModule(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptStableModuleKey& ModuleKey,
	const FAngelscriptHash256& CurrentSourceSnapshot,
	FAngelscriptCacheService* Service)
{
	using namespace AngelscriptCacheCompileReuse_Private;
	if (!IsValid() || ModuleKey.Hash.IsZero()
		|| Module->ScriptModule == nullptr
		|| Module->ScriptModule->engine == nullptr)
	{
		return Failure(EAngelscriptCacheCompileReusePrepareError::InvalidInput,
			TEXT("Compile reuse requires a live staged module and selected Generation"));
	}
	const FAngelscriptValidatedGeneration& Generation =
		Impl->Session->GetGeneration();
	if (CurrentSourceSnapshot.IsZero())
	{
		return Failure(
			EAngelscriptCacheCompileReusePrepareError::InvalidInput,
			TEXT("Compile reuse requires a nonzero current SourceSnapshot"));
	}
	if (Impl->Modules.ContainsByPredicate(
		[&ModuleKey](const TUniquePtr<FImpl::FModuleState>& Existing)
		{
			return Existing.IsValid() && Existing->ModuleKey == ModuleKey;
		}))
	{
		return Failure(EAngelscriptCacheCompileReusePrepareError::InvalidInput,
			TEXT("The same module was prepared twice for one compile reuse transaction"));
	}

	const FAngelscriptCacheModuleSnapshotLink* Link =
		Generation.Manifest.ModuleSnapshots.FindByPredicate(
			[&ModuleKey](const FAngelscriptCacheModuleSnapshotLink& Candidate)
			{
				return Candidate.ModuleKey == ModuleKey;
			});
	if (Link == nullptr)
	{
		return Failure(
			EAngelscriptCacheCompileReusePrepareError::CandidateModuleMissing,
			TEXT("The selected Generation has no candidate for the current module"));
	}
	const FAngelscriptDecodedCacheRecord* SourceIndex = FindRecord(
		Generation, Generation.Manifest.SourceIndexRecordId);
	if (SourceIndex == nullptr || SourceIndex->TryGetSourceIndex() == nullptr)
	{
		return Failure(
			EAngelscriptCacheCompileReusePrepareError::GraphValidationFailed,
			TEXT("The selected Generation has no reachable SourceIndex authority"));
	}

	asCScriptEngine& ScriptEngine = *Module->ScriptModule->engine;
	asCModule ValidationModule("__CacheV2CompileReuseValidation", &ScriptEngine);
	FAngelscriptCacheEngineEnvironmentResolver CurrentEnvironment(ScriptEngine);
	FImpl::FPreparedFunctionAuthority CurrentSymbols(
		CurrentEnvironment, *Impl);
	FAngelscriptCacheEngineLayoutResolver CurrentLayouts(ScriptEngine);
	FAngelscriptFunctionArtifactCodec OpaqueValidator(
		ValidationModule, ScriptEngine, &CurrentSymbols);
	FAngelscriptCacheModuleGraphValidationContext GraphContext;
	GraphContext.SelectedProfile = Generation.Manifest.Profile;
	GraphContext.SelectedSourceSnapshot = Generation.Manifest.SourceSnapshot;
	GraphContext.SourceIndex = SourceIndex;
	GraphContext.CurrentSymbols = &CurrentSymbols;
	GraphContext.CurrentLayouts = &CurrentLayouts;
	GraphContext.OpaquePayloads = &OpaqueValidator;

	TUniquePtr<FImpl::FModuleState> State =
		MakeUnique<FImpl::FModuleState>();
	State->Owner = Impl.Get();
	State->ModuleKey = ModuleKey;
	State->Service = Service;
	const FAngelscriptCacheValidationResult GraphResult =
		ValidateModuleSnapshotGraph(
			Link->RecordId,
			Generation.ReachableRecords,
			GraphContext,
			Impl->Limits,
			Impl->Budget,
			State->Graph);
	if (!GraphResult.IsSuccess())
	{
		return Failure(
			EAngelscriptCacheCompileReusePrepareError::GraphValidationFailed,
			FString::Printf(
				TEXT("Compile-reuse graph validation failed: Error=%u Stage=%u Offset=%llu Detail=%s"),
				static_cast<uint32>(GraphResult.Error),
				static_cast<uint32>(GraphResult.Stage),
				GraphResult.ByteOffset,
				*OpaqueValidator.GetLastExecutionFailureDetail()),
			GraphResult);
	}

	const bool bExactSource =
		Generation.Manifest.SourceSnapshot == CurrentSourceSnapshot;
	if (bExactSource)
	{
		bool bHasInterface = false;
		bool bHasState = false;
		for (const FAngelscriptDecodedCacheRecordHandle& Record
			: State->Graph.GetReachableRecords())
		{
			if (const FAngelscriptCachedModuleInterface* Interface =
				Record->TryGetModuleInterface())
			{
				if (bHasInterface)
				{
					return Failure(
						EAngelscriptCacheCompileReusePrepareError::CurrentAuthorityMissing,
						TEXT("The candidate graph contains more than one ModuleInterface"));
				}
				State->CurrentInterface = *Interface;
				bHasInterface = true;
			}
			else if (const FAngelscriptCachedTypeSchema* Type =
				Record->TryGetTypeSchema())
			{
				State->CurrentTypes.Add(*Type);
			}
			else if (const FAngelscriptCachedModuleState* ModuleState =
				Record->TryGetModuleState())
			{
				if (bHasState)
				{
					return Failure(
						EAngelscriptCacheCompileReusePrepareError::CurrentAuthorityMissing,
						TEXT("The candidate graph contains more than one ModuleState"));
				}
				State->CurrentState = *ModuleState;
				bHasState = true;
			}
			else if (const FAngelscriptCachedFunctionBody* Function =
				Record->TryGetFunctionBody())
			{
				State->CurrentFunctions.Add(*Function);
			}
		}
		if (!bHasInterface || !bHasState
			|| State->CurrentInterface.ModuleKey != ModuleKey
			|| State->CurrentState.ModuleKey != ModuleKey)
		{
			return Failure(
				EAngelscriptCacheCompileReusePrepareError::CurrentAuthorityMissing,
				TEXT("The candidate graph omitted its exact-source interface/state authority"));
		}
	}
	else
	{
		FAngelscriptCacheCurrentModuleAuthority CurrentAuthority;
		const FAngelscriptCacheCleanCaptureResult AuthorityResult =
			BuildAngelscriptCacheCurrentModuleAuthority(
				Module, Impl->Options, ModuleKey, CurrentAuthority);
		if (!AuthorityResult.IsSuccess())
		{
			return Failure(
				EAngelscriptCacheCompileReusePrepareError::CurrentAuthorityMissing,
				FString::Printf(
					TEXT("Current pre-compile authority rejected module: Error=%u Detail=%s"),
					static_cast<uint32>(AuthorityResult.Error),
					*AuthorityResult.Detail));
		}
		State->CurrentInterface = MoveTemp(CurrentAuthority.ModuleInterface);
		State->CurrentTypes = MoveTemp(CurrentAuthority.TypeSchemas);
		State->CurrentState = MoveTemp(CurrentAuthority.ModuleState);
		// Function-content authority is populated only after a current function
		// has restored or compiled. Never seed it from a changed Generation.
		State->CurrentFunctions.Reset();
	}
	State->ExternalSymbols =
		MakeUnique<FAngelscriptCacheEngineEnvironmentResolver>(ScriptEngine);
	if (bExactSource)
	{
		FString ExternalAuthorityFailure;
		if (!FImpl::BuildPreparedExternalFunctions(
			*State, *Module->ScriptModule, ExternalAuthorityFailure))
		{
			return Failure(
				EAngelscriptCacheCompileReusePrepareError::CurrentAuthorityMissing,
				FString::Printf(
					TEXT("Exact-source function authority rejected module: %s"),
					*ExternalAuthorityFailure));
		}
	}

	FImpl::FModuleState* CallbackState = State.Get();
	Impl->Modules.Add(MoveTemp(State));
	{
		FScopeLock SummaryLock(&Impl->SummaryMutex);
		++Impl->Summary.CandidateModuleCount;
	}
	Module->ScriptModule->SetBuildArtifactRestoreCallback(
		&FImpl::RestoreFunction, CallbackState);
	Module->ScriptModule->SetBuildArtifactCompileResultCallback(
		&FImpl::ObserveCompileResult, CallbackState);

	FAngelscriptCacheCompileReusePrepareResult Result;
	Result.Detail = FString::Printf(
		TEXT("Prepared %s module %s with %d graph-validated function candidates"),
		bExactSource ? TEXT("exact-source") : TEXT("changed-source"),
		*Module->ModuleName,
		CallbackState->Graph.GetFunctionOrdinals().Num());
	return Result;
}
