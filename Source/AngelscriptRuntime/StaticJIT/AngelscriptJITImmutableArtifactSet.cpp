#include "StaticJIT/AngelscriptJITImmutableArtifactSet.h"

namespace
{
	using FValidationCode =
		EAngelscriptJITImmutableArtifactSetValidationCode;

	static bool HasCompleteEntryPoints(
		const FAngelscriptJITEntryPoints& EntryPoints)
	{
		return EntryPoints.VMEntry != nullptr
			&& EntryPoints.RawEntry != nullptr
			&& EntryPoints.ParmsEntry != nullptr;
	}

	static bool HasAllRequiredReferences(
		const FAngelscriptJITProviderCatalogEntry& Entry,
		const FAngelscriptJITResolvedReferenceTable& Table)
	{
		if (Table.Slots.Num() != Entry.ReferenceSlots.Num())
		{
			return false;
		}

		for (int32 ReferenceIndex = 0;
			ReferenceIndex < Entry.ReferenceSlots.Num();
			++ReferenceIndex)
		{
			const FAngelscriptJITReferenceSlot& Reference =
				Entry.ReferenceSlots[ReferenceIndex];
			if (!EnumHasAnyFlags(
					Reference.Flags,
					EAngelscriptJITReferenceSlotFlags::Optional)
				&& Table.Slots[ReferenceIndex].Value == nullptr)
			{
				return false;
			}
		}
		return true;
	}

}

bool FAngelscriptJITImmutableArtifactSetValidationResult::
	AllowsDirectSelection(
		const FAngelscriptJITArtifactSelection& Selection) const
{
	return IsEligible()
		&& ValidatedProvider.IsValid()
		&& Selection.Provider.Get() == ValidatedProvider.Get()
		&& DirectEntryIndices.Contains(Selection.EntryIndex);
}

FAngelscriptJITImmutableArtifactSetValidationResult
FAngelscriptJITImmutableArtifactSetValidator::Validate(
	const FAngelscriptJITImmutableArtifactSetValidationInput& Input)
{
	const auto Reject = [](
		const FValidationCode Code,
		const int32 FailedEntry = INDEX_NONE)
	{
		FAngelscriptJITImmutableArtifactSetValidationResult Result;
		Result.ValidationCode = Code;
		Result.FailedEntry = FailedEntry;
		return Result;
	};

	if (!Input.Provider.IsValid())
	{
		return Reject(FValidationCode::MissingProvider);
	}
	if (Input.RuntimeProfile
		!= EAngelscriptJITArtifactSetRuntimeProfile::ImmutableCooked)
	{
		return Reject(FValidationCode::ReloadableProfile);
	}

	const FAngelscriptJITProviderCatalog& Provider = *Input.Provider;
	TSet<int32> DirectEntries;
	for (int32 EntryIndex = 0;
		EntryIndex < Provider.Entries.Num();
		++EntryIndex)
	{
		if (EnumHasAnyFlags(
			Provider.Entries[EntryIndex].Flags,
			EAngelscriptJITArtifactEntryFlags::ImmutableDirectCallSet))
		{
			DirectEntries.Add(EntryIndex);
		}
	}
	if (DirectEntries.IsEmpty())
	{
		return Reject(FValidationCode::MissingImmutableDirectEntries);
	}
	if (!(Input.CurrentArtifactSetDigest == Provider.ArtifactSetDigest))
	{
		return Reject(FValidationCode::ArtifactSetMismatch);
	}
	if (!(Input.CurrentArtifactProfile.Hash == Provider.ArtifactProfile.Hash))
	{
		return Reject(FValidationCode::ProfileMismatch);
	}
	if (!(Input.CurrentNativeEnvironmentFingerprint
		== Provider.NativeEnvironmentFingerprint))
	{
		return Reject(FValidationCode::EnvironmentMismatch);
	}

	TMap<int32, const FAngelscriptJITImmutableArtifactEntryMatch*>
		MatchesByEntry;
	for (const FAngelscriptJITImmutableArtifactEntryMatch& EntryMatch :
		Input.EntryMatches)
	{
		if (MatchesByEntry.Contains(EntryMatch.ProviderEntryIndex))
		{
			return Reject(
				FValidationCode::DuplicateEntryMatch,
				EntryMatch.ProviderEntryIndex);
		}
		MatchesByEntry.Add(EntryMatch.ProviderEntryIndex, &EntryMatch);
	}

	for (int32 EntryIndex = 0;
		EntryIndex < Provider.Entries.Num();
		++EntryIndex)
	{
		const FAngelscriptJITImmutableArtifactEntryMatch* const* MatchPtr =
			MatchesByEntry.Find(EntryIndex);
		if (MatchPtr == nullptr || *MatchPtr == nullptr)
		{
			return Reject(FValidationCode::MissingEntryMatch, EntryIndex);
		}

		const FAngelscriptJITFunctionMatchResult& Match = (*MatchPtr)->Match;
		if (Match.Result != EAngelscriptArtifactMatchResult::Exact
			|| !Match.Selection.IsSet())
		{
			return Reject(FValidationCode::EntryNotExact, EntryIndex);
		}

		const FAngelscriptJITArtifactSelection& Selection =
			Match.Selection.GetValue();
		if (Selection.Provider.Get() != Input.Provider.Get()
			|| Selection.EntryIndex != EntryIndex)
		{
			return Reject(FValidationCode::SelectionMismatch, EntryIndex);
		}

		const FAngelscriptJITProviderCatalogEntry& Entry =
			Provider.Entries[EntryIndex];
		if (!HasCompleteEntryPoints(Entry.EntryPoints))
		{
			return Reject(FValidationCode::MissingEntryPoint, EntryIndex);
		}
		if (!Selection.ReferenceTable.IsValid())
		{
			return Reject(FValidationCode::MissingReferenceTable, EntryIndex);
		}
		if (!HasAllRequiredReferences(Entry, *Selection.ReferenceTable))
		{
			return Reject(
				FValidationCode::MissingRequiredReference,
				EntryIndex);
		}
	}

	FAngelscriptJITImmutableArtifactSetValidationResult Result;
	Result.ValidationCode = FValidationCode::Eligible;
	Result.ValidatedProvider = Input.Provider;
	Result.DirectEntryIndices = MoveTemp(DirectEntries);
	return Result;
}
