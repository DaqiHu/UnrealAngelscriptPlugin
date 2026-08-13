#include "StaticJIT/AngelscriptJITProvider.h"

namespace
{
	static FAngelscriptJITProviderValidationResult Failure(
		const EAngelscriptJITProviderValidationCode Code,
		const int32 EntryIndex = INDEX_NONE,
		const int32 ReferenceIndex = INDEX_NONE)
	{
		return {Code, EntryIndex, ReferenceIndex};
	}

	static int32 CompareEntryIdentity(
		const FAngelscriptJITArtifactEntry& Left,
		const FAngelscriptJITArtifactEntry& Right)
	{
		if (!(Left.ModuleKey == Right.ModuleKey))
		{
			return Left.ModuleKey.Hash < Right.ModuleKey.Hash ? -1 : 1;
		}
		if (!(Left.FunctionKey == Right.FunctionKey))
		{
			return Left.FunctionKey.Hash < Right.FunctionKey.Hash ? -1 : 1;
		}
		return 0;
	}
}

FName IAngelscriptJITArtifactProvider::FeatureName()
{
	static const FName Name(TEXT("AngelscriptJITArtifactProvider"));
	return Name;
}

FAngelscriptHash256 FAngelscriptJITProviderManifest::BuildArtifactSetDigest(
	const TConstArrayView<FAngelscriptJITArtifactEntry> Entries)
{
	FAngelscriptArtifactCanonicalWriter Writer(TEXT("jit-provider-artifact-set-v1"));
	Writer.WriteUInt32(FAngelscriptJITProviderAbi::Revision);
	Writer.WriteUInt32(static_cast<uint32>(Entries.Num()));
	for (const FAngelscriptJITArtifactEntry& Entry : Entries)
	{
		Writer.WriteHash(Entry.ModuleKey.Hash);
		Writer.WriteHash(Entry.FunctionKey.Hash);
		Writer.WriteHash(Entry.ExecutionHash);
		Writer.WriteHash(Entry.DebugHash);
		Writer.WriteHash(Entry.ArtifactProfile.Hash);
		Writer.WriteHash(Entry.EntryAbiHash);
		Writer.WriteHash(Entry.NativeEnvironmentFingerprint);
		Writer.WriteUInt32(static_cast<uint32>(Entry.Flags));
		Writer.WriteBool(Entry.EntryPoints.VMEntry != nullptr);
		Writer.WriteBool(Entry.EntryPoints.RawEntry != nullptr);
		Writer.WriteBool(Entry.EntryPoints.ParmsEntry != nullptr);
		Writer.WriteUInt32(Entry.ReferenceSlotCount);
		for (uint32 ReferenceIndex = 0;
			ReferenceIndex < Entry.ReferenceSlotCount;
			++ReferenceIndex)
		{
			const FAngelscriptJITReferenceSlot& Slot =
				Entry.ReferenceSlots[ReferenceIndex];
			Writer.WriteUInt32(Slot.SlotIndex);
			Writer.WriteUInt32(static_cast<uint32>(Slot.Flags));
			Writer.WriteUInt8(static_cast<uint8>(Slot.Reference.Kind));
			Writer.WriteHash(Slot.Reference.StableKey);
			Writer.WriteHash(Slot.Reference.ExpectedAbi);
		}
	}
	return Writer.FinalizeHash();
}

FAngelscriptJITProviderGeneration
FAngelscriptJITProviderManifest::BuildProviderGeneration(
	const FAngelscriptJITProviderView& View)
{
	FAngelscriptArtifactCanonicalWriter Writer(
		TEXT("jit-provider-generation-v1"));
	Writer.WriteUInt32(View.AbiRevision);
	Writer.WriteHash(View.ProviderId.Hash);
	Writer.WriteHash(View.ArtifactSetDigest);
	Writer.WriteHash(View.ArtifactProfile.Hash);
	Writer.WriteHash(View.NativeEnvironmentFingerprint);
	Writer.WriteUInt32(View.EntryCount);
	return FAngelscriptJITProviderGeneration{Writer.FinalizeHash()};
}

FAngelscriptJITProviderValidationResult
FAngelscriptJITProviderManifest::Validate(
	const FAngelscriptJITProviderView& View)
{
	if (View.StructSize != sizeof(FAngelscriptJITProviderView))
	{
		return Failure(EAngelscriptJITProviderValidationCode::InvalidViewSize);
	}
	if (View.AbiRevision != FAngelscriptJITProviderAbi::Revision)
	{
		return Failure(
			EAngelscriptJITProviderValidationCode::UnsupportedAbiRevision);
	}
	if (!View.ProviderId.IsValid())
	{
		return Failure(EAngelscriptJITProviderValidationCode::InvalidProviderId);
	}
	if (!View.ProviderGeneration.IsValid())
	{
		return Failure(
			EAngelscriptJITProviderValidationCode::InvalidProviderGeneration);
	}
	if (View.ProviderName == nullptr || View.ProviderName[0] == TEXT('\0'))
	{
		return Failure(EAngelscriptJITProviderValidationCode::MissingProviderName);
	}
	if (View.OwnerModuleName == nullptr
		|| View.OwnerModuleName[0] == TEXT('\0'))
	{
		return Failure(
			EAngelscriptJITProviderValidationCode::MissingOwnerModuleName);
	}
	if (View.ArtifactSetDigest.IsZero())
	{
		return Failure(
			EAngelscriptJITProviderValidationCode::InvalidArtifactSetDigest);
	}
	if (View.ArtifactProfile.Hash.IsZero())
	{
		return Failure(
			EAngelscriptJITProviderValidationCode::InvalidArtifactProfile);
	}
	if (View.NativeEnvironmentFingerprint.IsZero())
	{
		return Failure(
			EAngelscriptJITProviderValidationCode::InvalidEnvironmentFingerprint);
	}
	if (View.EntryCount == 0
		|| View.EntryCount > FAngelscriptJITProviderAbi::MaximumEntryCount)
	{
		return Failure(EAngelscriptJITProviderValidationCode::InvalidEntryCount);
	}
	if (View.Entries == nullptr)
	{
		return Failure(EAngelscriptJITProviderValidationCode::MissingEntryTable);
	}
	static constexpr uint32 ValidEntryFlags =
		static_cast<uint32>(
			EAngelscriptJITArtifactEntryFlags::RequireExactDebugIdentity)
		| static_cast<uint32>(
			EAngelscriptJITArtifactEntryFlags::ImmutableDirectCallSet);
	static constexpr uint32 ValidReferenceFlags =
		static_cast<uint32>(EAngelscriptJITReferenceSlotFlags::Optional);

	for (uint32 EntryIndex = 0; EntryIndex < View.EntryCount; ++EntryIndex)
	{
		const FAngelscriptJITArtifactEntry& Entry = View.Entries[EntryIndex];
		const int32 SignedEntryIndex = static_cast<int32>(EntryIndex);
		if (Entry.StructSize != sizeof(FAngelscriptJITArtifactEntry))
		{
			return Failure(EAngelscriptJITProviderValidationCode::InvalidEntrySize,
				SignedEntryIndex);
		}
		if ((static_cast<uint32>(Entry.Flags) & ~ValidEntryFlags) != 0)
		{
			return Failure(EAngelscriptJITProviderValidationCode::InvalidEntryFlags,
				SignedEntryIndex);
		}
		if (Entry.ModuleKey.Hash.IsZero())
		{
			return Failure(EAngelscriptJITProviderValidationCode::InvalidModuleKey,
				SignedEntryIndex);
		}
		if (Entry.FunctionKey.Hash.IsZero())
		{
			return Failure(EAngelscriptJITProviderValidationCode::InvalidFunctionKey,
				SignedEntryIndex);
		}
		if (Entry.ExecutionHash.IsZero())
		{
			return Failure(EAngelscriptJITProviderValidationCode::InvalidExecutionHash,
				SignedEntryIndex);
		}
		if (Entry.DebugHash.IsZero())
		{
			return Failure(EAngelscriptJITProviderValidationCode::InvalidDebugHash,
				SignedEntryIndex);
		}
		if (!(Entry.ArtifactProfile.Hash == View.ArtifactProfile.Hash))
		{
			return Failure(EAngelscriptJITProviderValidationCode::InvalidEntryProfile,
				SignedEntryIndex);
		}
		if (!(Entry.NativeEnvironmentFingerprint
			== View.NativeEnvironmentFingerprint))
		{
			return Failure(
				EAngelscriptJITProviderValidationCode::InvalidEntryEnvironment,
				SignedEntryIndex);
		}
		if (Entry.EntryAbiHash.IsZero())
		{
			return Failure(EAngelscriptJITProviderValidationCode::InvalidEntryAbi,
				SignedEntryIndex);
		}
		if (!Entry.EntryPoints.HasAny())
		{
			return Failure(EAngelscriptJITProviderValidationCode::MissingEntryPoints,
				SignedEntryIndex);
		}
		if (Entry.ReferenceSlotCount
			> FAngelscriptJITProviderAbi::MaximumReferenceSlotCount)
		{
			return Failure(EAngelscriptJITProviderValidationCode::InvalidReferenceCount,
				SignedEntryIndex);
		}
		if ((Entry.ReferenceSlotCount == 0) != (Entry.ReferenceSlots == nullptr))
		{
			return Failure(EAngelscriptJITProviderValidationCode::MissingReferenceTable,
				SignedEntryIndex);
		}
		for (uint32 ReferenceIndex = 0;
			ReferenceIndex < Entry.ReferenceSlotCount;
			++ReferenceIndex)
		{
			const FAngelscriptJITReferenceSlot& Slot =
				Entry.ReferenceSlots[ReferenceIndex];
			const int32 SignedReferenceIndex =
				static_cast<int32>(ReferenceIndex);
			if (Slot.StructSize != sizeof(FAngelscriptJITReferenceSlot))
			{
				return Failure(
					EAngelscriptJITProviderValidationCode::InvalidReferenceSize,
					SignedEntryIndex, SignedReferenceIndex);
			}
			if ((static_cast<uint32>(Slot.Flags) & ~ValidReferenceFlags) != 0)
			{
				return Failure(
					EAngelscriptJITProviderValidationCode::InvalidReferenceFlags,
					SignedEntryIndex, SignedReferenceIndex);
			}
			if (!Slot.Reference.IsValid())
			{
				return Failure(EAngelscriptJITProviderValidationCode::InvalidReference,
					SignedEntryIndex, SignedReferenceIndex);
			}
			if (Slot.Reference.ExpectedAbi.IsZero())
			{
				return Failure(EAngelscriptJITProviderValidationCode::InvalidReferenceAbi,
					SignedEntryIndex, SignedReferenceIndex);
			}
			if (Slot.SlotIndex != ReferenceIndex)
			{
				return Failure(EAngelscriptJITProviderValidationCode::InvalidReferenceOrder,
					SignedEntryIndex, SignedReferenceIndex);
			}
		}

		if (EntryIndex > 0)
		{
			const int32 Comparison =
				CompareEntryIdentity(View.Entries[EntryIndex - 1], Entry);
			if (Comparison == 0)
			{
				return Failure(EAngelscriptJITProviderValidationCode::DuplicateEntry,
					SignedEntryIndex);
			}
			if (Comparison > 0)
			{
				return Failure(EAngelscriptJITProviderValidationCode::UnsortedEntries,
					SignedEntryIndex);
			}
		}
	}

	const TConstArrayView<FAngelscriptJITArtifactEntry> Entries(
		View.Entries, static_cast<int32>(View.EntryCount));
	if (!(BuildArtifactSetDigest(Entries) == View.ArtifactSetDigest))
	{
		return Failure(
			EAngelscriptJITProviderValidationCode::ArtifactSetDigestMismatch);
	}
	if (BuildProviderGeneration(View) != View.ProviderGeneration)
	{
		return Failure(
			EAngelscriptJITProviderValidationCode::ProviderGenerationDigestMismatch);
	}

	return {};
}
