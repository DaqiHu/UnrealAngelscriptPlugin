/*
   Maintained Unreal AngelScript fork — immutable frontend source queries.
*/

#include "frontend/Basic/as_source_manager.h"

BEGIN_AS_NAMESPACE

asCSourceManager::asCSourceManager(
		TSharedRef<asCSourceSnapshot, ESPMode::ThreadSafe> InSnapshot)
		: Snapshot(MoveTemp(InSnapshot))
	{
	}

	uint64 asCSourceManager::GetSnapshotID() const
	{
		return Snapshot->GetSnapshotID();
	}

	bool asCSourceManager::IsReady() const
	{
		return Snapshot->IsFrozen();
	}

	asCSourceLocation asCSourceManager::MakeLocation(
		asCSourceFileID FileID,
		asUINT ByteOffset) const
	{
		return asCSourceLocation(GetSnapshotID(), FileID, ByteOffset);
	}

	asCSourceRange asCSourceManager::MakeRange(
		asCSourceFileID FileID,
		asUINT BeginByteOffset,
		asUINT EndByteOffset) const
	{
		return asCSourceRange(
			MakeLocation(FileID, BeginByteOffset),
			MakeLocation(FileID, EndByteOffset));
	}

	bool asCSourceManager::IsValidLocation(const asCSourceLocation& Location) const
	{
		if (!IsReady()
			|| !Location.IsValid()
			|| Location.SnapshotID != GetSnapshotID())
		{
			return false;
		}

		TConstArrayView<uint8> Bytes;
		return Snapshot->TryGetBytes(Location.FileID, Bytes)
			&& Location.ByteOffset <= static_cast<asUINT>(Bytes.Num());
	}

	bool asCSourceManager::IsValidRange(const asCSourceRange& Range) const
	{
		return Range.IsOrdered()
			&& IsValidLocation(Range.Begin)
			&& IsValidLocation(Range.End);
	}

	bool asCSourceManager::TryGetBytes(
		const asCSourceRange& Range,
		TConstArrayView<uint8>& OutBytes) const
	{
		OutBytes = TConstArrayView<uint8>();
		if (!IsValidRange(Range))
		{
			return false;
		}

		TConstArrayView<uint8> FileBytes;
		if (!Snapshot->TryGetBytes(Range.Begin.FileID, FileBytes))
		{
			return false;
		}
		OutBytes = FileBytes.Slice(
			static_cast<int32>(Range.Begin.ByteOffset),
			static_cast<int32>(Range.Length()));
		return true;
	}

	bool asCSourceManager::HasMaterializedLineMap(asCSourceFileID FileID) const
	{
		return Snapshot->HasMaterializedLineMap(FileID);
	}

	bool asCSourceManager::TryGetLineColumn(
		const asCSourceLocation& Location,
		asSSourceLineColumn& OutLineColumn) const
	{
		return IsValidLocation(Location)
			&& Snapshot->TryGetLineColumn(Location, OutLineColumn);
	}

	bool asCSourceManager::FindOrigins(
		const asCSourceRange& Range,
		TArray<asSSourceOriginRecord>& OutRecords) const
	{
		OutRecords.Reset();
		return IsValidRange(Range) && Snapshot->FindOrigins(Range, OutRecords);
	}

	bool asCSourceManager::TryResolveOriginChain(
		asCSourceOriginID OriginID,
		TArray<asSSourceOriginRecord>& OutChain) const
	{
		OutChain.Reset();
		return OriginID.SnapshotID == GetSnapshotID()
			&& Snapshot->TryResolveOriginChain(OriginID, OutChain);
	}

	bool asCSourceManager::TryCreateStableAnchor(
		const asCSourceRange& Range,
		asSStableSourceAnchor& OutAnchor) const
	{
		OutAnchor = asSStableSourceAnchor();
		if (!IsValidRange(Range))
		{
			return false;
		}

		FStringView LogicalSourceKey;
		uint64 ContentRevision = 0;
		if (!Snapshot->TryGetLogicalSourceKey(Range.Begin.FileID, LogicalSourceKey)
			|| !Snapshot->TryGetContentRevision(Range.Begin.FileID, ContentRevision))
		{
			return false;
		}

		OutAnchor.LogicalSourceKey = FString(LogicalSourceKey);
		OutAnchor.ContentRevision = ContentRevision;
		OutAnchor.BeginByteOffset = Range.Begin.ByteOffset;
		OutAnchor.EndByteOffset = Range.End.ByteOffset;
		return true;
	}

	bool asCSourceManager::TryRelocateStableAnchor(
		const asSStableSourceAnchor& Anchor,
		asCSourceRange& OutRange) const
	{
		OutRange = asCSourceRange();
		if (!IsReady() || !Anchor.IsValid())
		{
			return false;
		}

		const asCSourceFileID FileID = Snapshot->FindFile(
			Anchor.LogicalSourceKey,
			Anchor.ContentRevision);
		if (!FileID.IsValid())
		{
			return false;
		}

		const asCSourceRange Candidate = MakeRange(
			FileID,
			Anchor.BeginByteOffset,
			Anchor.EndByteOffset);
		if (!IsValidRange(Candidate))
		{
			return false;
		}
		OutRange = Candidate;
		return true;
	}

END_AS_NAMESPACE
