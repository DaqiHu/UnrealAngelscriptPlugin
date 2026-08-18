#include "StaticJIT/AngelscriptJITProviderMatcher.h"

#include "StaticJIT/AngelscriptJITGeneration.h"
#include "StaticJIT/BytecodeJIT/AngelscriptBytecodeJIT.h"

namespace
{
	struct FEvaluatedCandidate
	{
		FAngelscriptJITArtifactMatchCandidate Diagnostic;
		TOptional<FAngelscriptJITArtifactSelection> Selection;
		int32 MatchDepth = 0;
	};

	static FAngelscriptHash256 MakeRouteEntryAbi(
		const FAngelscriptFunctionRoute& Route)
	{
		const bool bHasParmsEntry =
			FAngelscriptBytecodeJIT::CanGenerateParmsEntry(Route.Function);
		return FAngelscriptJITGeneration::BuildEntryAbiHash(
			Route.CanonicalDeclaration,
			true,
			true,
			bHasParmsEntry);
	}

	static EAngelscriptArtifactMatchResult EvaluateSemanticDependency(
		const FAngelscriptJITSemanticDependencyRow& Row,
		const FAngelscriptFunctionRouteSnapshot* const Routes,
		const IAngelscriptJITReferenceResolver* const Resolver)
	{
		if (Routes == nullptr)
		{
			return EAngelscriptArtifactMatchResult::SemanticDependencyMismatch;
		}

		if (Row.TargetKind == EAngelscriptArtifactReferenceKind::ScriptFunction)
		{
			TArray<const FAngelscriptFunctionRoute*, TInlineAllocator<2>> Matches;
			for (const FAngelscriptFunctionRoute& Route : Routes->FunctionRoutes)
			{
				if (Route.bHasVerifiedArtifactIdentity
					&& Route.Identity.FunctionKey.Hash == Row.TargetStableKey)
				{
					Matches.Add(&Route);
				}
			}
			if (Matches.Num() != 1)
			{
				return EAngelscriptArtifactMatchResult::SemanticDependencyMismatch;
			}
			const FAngelscriptFunctionRoute& Target = *Matches[0];
			if (!(MakeRouteEntryAbi(Target) == Row.ExpectedAbi))
			{
				return EAngelscriptArtifactMatchResult::SemanticDependencyMismatch;
			}
			if (Row.Kind == EAngelscriptArtifactSemanticDependencyKind::FunctionContent
				&& !(Target.Identity.Content.Execution == Row.ExpectedContentOrValue))
			{
				return EAngelscriptArtifactMatchResult::SemanticDependencyMismatch;
			}
			return EAngelscriptArtifactMatchResult::Exact;
		}

		int32 Hits = 0;
		TOptional<FAngelscriptHash256> CurrentValue;
		FAngelscriptHash256 CurrentAbi;
		bool bConflict = false;
		for (const FAngelscriptFunctionArtifactReferenceSet& Set
			: Routes->ValidatedFunctionArtifactReferenceSets)
		{
			for (const FAngelscriptArtifactSemanticDependency& Dependency
				: Set.SemanticDependencies)
			{
				if (Dependency.Kind != Row.Kind
					|| Dependency.Target.Kind != Row.TargetKind
					|| !(Dependency.Target.StableKey == Row.TargetStableKey))
				{
					continue;
				}
				++Hits;
				if (Hits == 1)
				{
					CurrentAbi = Dependency.Target.ExpectedAbi;
					CurrentValue = Dependency.ExpectedContentOrValue;
				}
				else if (!(CurrentAbi == Dependency.Target.ExpectedAbi)
					|| CurrentValue != Dependency.ExpectedContentOrValue)
				{
					bConflict = true;
				}
			}
		}
		if (Hits == 0
			&& !bConflict
			&& Resolver != nullptr
			&& Row.TargetKind
				== EAngelscriptArtifactReferenceKind::EnvironmentSymbol)
		{
			FAngelscriptArtifactReference Reference;
			Reference.Kind = Row.TargetKind;
			Reference.StableKey = Row.TargetStableKey;
			Reference.ExpectedAbi = Row.ExpectedAbi;
			const FAngelscriptJITReferenceResolution Resolution =
				Resolver->Resolve(Reference);
			if (Resolution.Code == EAngelscriptJITReferenceResolutionCode::Exact
				&& Resolution.Value != nullptr)
			{
				return EAngelscriptArtifactMatchResult::Exact;
			}
			return EAngelscriptArtifactMatchResult::SemanticDependencyMismatch;
		}
		if (Hits == 0 || bConflict)
		{
			return EAngelscriptArtifactMatchResult::SemanticDependencyMismatch;
		}
		if (!(CurrentAbi == Row.ExpectedAbi))
		{
			return EAngelscriptArtifactMatchResult::SemanticDependencyMismatch;
		}
		if (FAngelscriptJITProviderManifest::SemanticDependencyRequiresExpectedContent(
				Row.Kind)
			&& (!CurrentValue.IsSet()
				|| !(CurrentValue.GetValue() == Row.ExpectedContentOrValue)))
		{
			return EAngelscriptArtifactMatchResult::SemanticDependencyMismatch;
		}
		return EAngelscriptArtifactMatchResult::Exact;
	}

	static bool HasEntryPoint(
		const FAngelscriptJITEntryPoints& EntryPoints,
		const EAngelscriptJITRequiredEntryPoints Required)
	{
		if (EnumHasAnyFlags(Required, EAngelscriptJITRequiredEntryPoints::VM)
			&& EntryPoints.VMEntry == nullptr)
		{
			return false;
		}
		if (EnumHasAnyFlags(Required, EAngelscriptJITRequiredEntryPoints::Raw)
			&& EntryPoints.RawEntry == nullptr)
		{
			return false;
		}
		return !EnumHasAnyFlags(
				Required, EAngelscriptJITRequiredEntryPoints::Parms)
			|| EntryPoints.ParmsEntry != nullptr;
	}

	static FEvaluatedCandidate Evaluate(
		const TSharedPtr<const FAngelscriptJITProviderCatalog,
			ESPMode::ThreadSafe>& Provider,
		const int32 EntryIndex,
		const FAngelscriptJITFunctionMatchInput& Input,
		const IAngelscriptJITReferenceResolver& ReferenceResolver)
	{
		FEvaluatedCandidate Result;
		Result.Diagnostic.ProviderId = Provider->ProviderId;
		Result.Diagnostic.ProviderGeneration = Provider->ProviderGeneration;
		Result.Diagnostic.EntryIndex = EntryIndex;
		const FAngelscriptJITProviderCatalogEntry& Entry =
			Provider->Entries[EntryIndex];

		if (!(Entry.ExecutionHash == Input.Identity.Content.Execution))
		{
			Result.Diagnostic.Result =
				EAngelscriptArtifactMatchResult::ContentMismatch;
			return Result;
		}
		Result.MatchDepth = 1;
		if (EnumHasAnyFlags(Entry.Flags,
			EAngelscriptJITArtifactEntryFlags::RequireExactDebugIdentity)
			&& !(Entry.DebugHash == Input.Identity.Content.Debug))
		{
			Result.Diagnostic.Result =
				EAngelscriptArtifactMatchResult::DebugMismatch;
			return Result;
		}
		Result.MatchDepth = 2;
		if (!(Entry.ArtifactProfile.Hash == Input.Identity.Profile.Hash))
		{
			Result.Diagnostic.Result =
				EAngelscriptArtifactMatchResult::ProfileMismatch;
			return Result;
		}
		Result.MatchDepth = 3;
		if (!(Entry.NativeEnvironmentFingerprint
			== Input.NativeEnvironmentFingerprint))
		{
			Result.Diagnostic.Result =
				EAngelscriptArtifactMatchResult::EnvironmentMismatch;
			return Result;
		}
		Result.MatchDepth = 4;
		if (!(Entry.EntryAbiHash == Input.EntryAbiHash))
		{
			Result.Diagnostic.Result =
				EAngelscriptArtifactMatchResult::EntryAbiMismatch;
			return Result;
		}
		Result.MatchDepth = 5;
		if (!HasEntryPoint(Entry.EntryPoints, Input.RequiredEntryPoints))
		{
			Result.Diagnostic.Result =
				EAngelscriptArtifactMatchResult::MissingEntryPoint;
			return Result;
		}
		Result.MatchDepth = 6;
		if (EnumHasAnyFlags(Entry.Flags,
			EAngelscriptJITArtifactEntryFlags::ImmutableDirectCallSet)
			&& !(Provider->ArtifactSetDigest
				== Input.CurrentArtifactSetDigest))
		{
			Result.Diagnostic.Result =
				EAngelscriptArtifactMatchResult::ArtifactSetMismatch;
			return Result;
		}
		Result.MatchDepth = 7;

		for (const FAngelscriptJITSemanticDependencyRow& Row
			: Entry.SemanticDependencies)
		{
			const EAngelscriptArtifactMatchResult DependencyResult =
				EvaluateSemanticDependency(
					Row, Input.CurrentRoutes.Get(), &ReferenceResolver);
			if (DependencyResult != EAngelscriptArtifactMatchResult::Exact)
			{
				Result.Diagnostic.Result = DependencyResult;
				return Result;
			}
		}
		Result.MatchDepth = 8;

		const FAngelscriptJITReferenceTableBuildResult ReferenceResult =
			FAngelscriptJITResolvedReferenceTableBuilder::Build(
				Entry.ReferenceSlots, ReferenceResolver);
		if (!ReferenceResult.IsExact())
		{
			Result.Diagnostic.Result = ReferenceResult.Result;
			Result.MatchDepth += FMath::Max(0, ReferenceResult.FailedSlotIndex);
			return Result;
		}

		Result.Diagnostic.Result = EAngelscriptArtifactMatchResult::Exact;
		FAngelscriptJITArtifactSelection Selection;
		Selection.Provider = Provider;
		Selection.EntryIndex = EntryIndex;
		Selection.ReferenceTable = ReferenceResult.Table;
		Result.Selection = MoveTemp(Selection);
		Result.MatchDepth = 1000 + Entry.ReferenceSlots.Num();
		return Result;
	}
}

FAngelscriptJITFunctionMatchResult FAngelscriptJITProviderMatcher::Match(
	const FAngelscriptJITProviderRegistrySnapshot& Providers,
	const FAngelscriptJITFunctionMatchInput& Input,
	const IAngelscriptJITReferenceResolver& ReferenceResolver)
{
	FAngelscriptJITFunctionMatchResult Result;
	TArray<FEvaluatedCandidate> Evaluated;
	bool bHasDuplicateProviderEntry = false;

	for (const TSharedPtr<const FAngelscriptJITProviderCatalog,
		ESPMode::ThreadSafe>& Provider : Providers.Providers)
	{
		if (!Provider.IsValid())
		{
			continue;
		}
		TArray<int32, TInlineAllocator<2>> MatchingEntryIndices;
		for (int32 EntryIndex = 0;
			EntryIndex < Provider->Entries.Num();
			++EntryIndex)
		{
			const FAngelscriptJITProviderCatalogEntry& Entry =
				Provider->Entries[EntryIndex];
			if (Entry.ModuleKey == Input.ModuleKey
				&& Entry.FunctionKey == Input.Identity.FunctionKey)
			{
				MatchingEntryIndices.Add(EntryIndex);
			}
		}

		if (MatchingEntryIndices.Num() > 1)
		{
			FEvaluatedCandidate Duplicate;
			Duplicate.Diagnostic.ProviderId = Provider->ProviderId;
			Duplicate.Diagnostic.ProviderGeneration =
				Provider->ProviderGeneration;
			Duplicate.Diagnostic.EntryIndex = MatchingEntryIndices[0];
			Duplicate.Diagnostic.Result =
				EAngelscriptArtifactMatchResult::DuplicateProviderEntry;
			Duplicate.MatchDepth = 2000;
			Evaluated.Add(MoveTemp(Duplicate));
			bHasDuplicateProviderEntry = true;
			continue;
		}
		if (MatchingEntryIndices.Num() == 1)
		{
			Evaluated.Add(Evaluate(
				Provider,
				MatchingEntryIndices[0],
				Input,
				ReferenceResolver));
		}
	}

	for (const FEvaluatedCandidate& Candidate : Evaluated)
	{
		Result.Candidates.Add(Candidate.Diagnostic);
	}
	if (bHasDuplicateProviderEntry)
	{
		Result.Result = EAngelscriptArtifactMatchResult::DuplicateProviderEntry;
		return Result;
	}

	TArray<const FEvaluatedCandidate*, TInlineAllocator<2>> Exact;
	for (const FEvaluatedCandidate& Candidate : Evaluated)
	{
		if (Candidate.Diagnostic.Result == EAngelscriptArtifactMatchResult::Exact)
		{
			Exact.Add(&Candidate);
		}
	}
	if (Exact.Num() > 1)
	{
		Result.Result = EAngelscriptArtifactMatchResult::AmbiguousExactProvider;
		return Result;
	}
	if (Exact.Num() == 1)
	{
		Result.Result = EAngelscriptArtifactMatchResult::Exact;
		Result.Selection = Exact[0]->Selection;
		return Result;
	}
	if (Evaluated.IsEmpty())
	{
		return Result;
	}

	const FEvaluatedCandidate* Best = &Evaluated[0];
	for (int32 Index = 1; Index < Evaluated.Num(); ++Index)
	{
		const FEvaluatedCandidate& Candidate = Evaluated[Index];
		if (Candidate.MatchDepth > Best->MatchDepth
			|| (Candidate.MatchDepth == Best->MatchDepth
				&& static_cast<uint8>(Candidate.Diagnostic.Result)
					< static_cast<uint8>(Best->Diagnostic.Result)))
		{
			Best = &Candidate;
		}
	}
	Result.Result = Best->Diagnostic.Result;
	return Result;
}

FAngelscriptJITFunctionMatchResult
FAngelscriptJITProviderMatcher::MatchPreferredThenFallback(
	const FAngelscriptJITProviderRegistrySnapshot& Providers,
	const FAngelscriptJITFunctionMatchInput& PreferredInput,
	const FAngelscriptJITFunctionMatchInput& FallbackInput,
	const IAngelscriptJITReferenceResolver& ReferenceResolver)
{
	FAngelscriptJITFunctionMatchResult Preferred = Match(
		Providers, PreferredInput, ReferenceResolver);
	if (Preferred.Result == EAngelscriptArtifactMatchResult::Exact
		|| Preferred.Result
			== EAngelscriptArtifactMatchResult::AmbiguousExactProvider
		|| Preferred.Result
			== EAngelscriptArtifactMatchResult::DuplicateProviderEntry)
	{
		return Preferred;
	}

	FAngelscriptJITFunctionMatchResult Fallback = Match(
		Providers, FallbackInput, ReferenceResolver);
	if (Fallback.Result == EAngelscriptArtifactMatchResult::Exact
		|| Fallback.Result
			== EAngelscriptArtifactMatchResult::AmbiguousExactProvider)
	{
		return Fallback;
	}
	return Preferred;
}
