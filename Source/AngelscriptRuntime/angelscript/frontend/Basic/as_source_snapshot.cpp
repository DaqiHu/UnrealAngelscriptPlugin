/*
   Maintained Unreal AngelScript fork — immutable frontend source snapshot.
*/

#include "frontend/Basic/as_source_snapshot.h"

#include <atomic>
#include <mutex>

BEGIN_AS_NAMESPACE

namespace
	{
		std::atomic<uint64> GNextSourceSnapshotID{1};

		uint64 ComputeContentRevision(TConstArrayView<uint8> Bytes)
		{
			// FNV-1a is used as a deterministic content revision, not as a security boundary.
			uint64 Revision = 14695981039346656037ull;
			for (const uint8 Byte : Bytes)
			{
				Revision ^= Byte;
				Revision *= 1099511628211ull;
			}
			return Revision == 0 ? 1 : Revision;
		}
	}

	struct asCSourceSnapshot::asSImpl
	{
		struct asSFile
		{
			FString LogicalSourceKey;
			TArray<uint8> Utf8Bytes;
			uint64 ContentRevision = 0;
			mutable std::once_flag LineMapOnce;
			mutable TArray<asUINT> LineStarts;
			mutable std::atomic<bool> bLineMapMaterialized{false};
		};

		TArray<TUniquePtr<asSFile>> Files;
		TArray<asSSourceOriginRecord> Origins;
		std::atomic<bool> bFrozen{false};
	};

	asCSourceSnapshot::asCSourceSnapshot()
		: Impl(MakeUnique<asSImpl>())
		, SnapshotID(GNextSourceSnapshotID.fetch_add(1, std::memory_order_relaxed))
	{
		if (SnapshotID == 0)
		{
			SnapshotID = GNextSourceSnapshotID.fetch_add(1, std::memory_order_relaxed);
		}
	}

	asCSourceSnapshot::~asCSourceSnapshot() = default;

	asCSourceFileID asCSourceSnapshot::AddFile(
		FStringView LogicalSourceKey,
		TConstArrayView<uint8> Utf8Bytes)
	{
		if (Impl->bFrozen.load(std::memory_order_acquire) || LogicalSourceKey.IsEmpty())
		{
			return asCSourceFileID();
		}

		for (const TUniquePtr<asSImpl::asSFile>& Existing : Impl->Files)
		{
			if (FStringView(Existing->LogicalSourceKey).Equals(LogicalSourceKey, ESearchCase::CaseSensitive))
			{
				return asCSourceFileID();
			}
		}

		TUniquePtr<asSImpl::asSFile> File = MakeUnique<asSImpl::asSFile>();
		File->LogicalSourceKey = FString(LogicalSourceKey);
		File->Utf8Bytes.Append(Utf8Bytes.GetData(), Utf8Bytes.Num());
		File->ContentRevision = ComputeContentRevision(File->Utf8Bytes);
		Impl->Files.Add(MoveTemp(File));
		return asCSourceFileID(static_cast<asUINT>(Impl->Files.Num()));
	}

	bool asCSourceSnapshot::Freeze()
	{
		bool Expected = false;
		return Impl->bFrozen.compare_exchange_strong(
			Expected,
			true,
			std::memory_order_release,
			std::memory_order_relaxed);
	}

	bool asCSourceSnapshot::IsFrozen() const
	{
		return Impl->bFrozen.load(std::memory_order_acquire);
	}

	asUINT asCSourceSnapshot::GetFileCount() const
	{
		return static_cast<asUINT>(Impl->Files.Num());
	}

	bool asCSourceSnapshot::IsValidFileID(asCSourceFileID FileID) const
	{
		return FileID.IsValid() && FileID.Value <= static_cast<asUINT>(Impl->Files.Num());
	}

	bool asCSourceSnapshot::TryGetBytes(
		asCSourceFileID FileID,
		TConstArrayView<uint8>& OutBytes) const
	{
		OutBytes = TConstArrayView<uint8>();
		if (!IsFrozen() || !IsValidFileID(FileID))
		{
			return false;
		}
		OutBytes = MakeArrayView(Impl->Files[FileID.Value - 1]->Utf8Bytes);
		return true;
	}

	bool asCSourceSnapshot::TryGetLogicalSourceKey(
		asCSourceFileID FileID,
		FStringView& OutLogicalSourceKey) const
	{
		OutLogicalSourceKey = FStringView();
		if (!IsFrozen() || !IsValidFileID(FileID))
		{
			return false;
		}
		OutLogicalSourceKey = FStringView(Impl->Files[FileID.Value - 1]->LogicalSourceKey);
		return true;
	}

	bool asCSourceSnapshot::TryGetContentRevision(
		asCSourceFileID FileID,
		uint64& OutContentRevision) const
	{
		OutContentRevision = 0;
		if (!IsFrozen() || !IsValidFileID(FileID))
		{
			return false;
		}
		OutContentRevision = Impl->Files[FileID.Value - 1]->ContentRevision;
		return true;
	}

	asCSourceFileID asCSourceSnapshot::FindFile(
		FStringView LogicalSourceKey,
		uint64 ContentRevision) const
	{
		if (!IsFrozen() || LogicalSourceKey.IsEmpty() || ContentRevision == 0)
		{
			return asCSourceFileID();
		}
		for (int32 Index = 0; Index < Impl->Files.Num(); ++Index)
		{
			const asSImpl::asSFile& File = *Impl->Files[Index];
			if (File.ContentRevision == ContentRevision
				&& FStringView(File.LogicalSourceKey).Equals(LogicalSourceKey, ESearchCase::CaseSensitive))
			{
				return asCSourceFileID(static_cast<asUINT>(Index + 1));
			}
		}
		return asCSourceFileID();
	}

	bool asCSourceSnapshot::HasMaterializedLineMap(asCSourceFileID FileID) const
	{
		return IsValidFileID(FileID)
			&& Impl->Files[FileID.Value - 1]->bLineMapMaterialized.load(std::memory_order_acquire);
	}

	bool asCSourceSnapshot::TryGetLineColumn(
		const asCSourceLocation& Location,
		asSSourceLineColumn& OutLineColumn) const
	{
		OutLineColumn = asSSourceLineColumn();
		if (!IsFrozen()
			|| !Location.IsValid()
			|| Location.SnapshotID != SnapshotID
			|| !IsValidFileID(Location.FileID))
		{
			return false;
		}

		const asSImpl::asSFile& File = *Impl->Files[Location.FileID.Value - 1];
		if (Location.ByteOffset > static_cast<asUINT>(File.Utf8Bytes.Num()))
		{
			return false;
		}

		std::call_once(File.LineMapOnce, [&File]()
		{
			File.LineStarts.Add(0);
			for (int32 Index = 0; Index < File.Utf8Bytes.Num(); ++Index)
			{
				if (File.Utf8Bytes[Index] == static_cast<uint8>('\n'))
				{
					File.LineStarts.Add(static_cast<asUINT>(Index + 1));
				}
			}
			File.bLineMapMaterialized.store(true, std::memory_order_release);
		});

		int32 Low = 0;
		int32 High = File.LineStarts.Num();
		while (Low < High)
		{
			const int32 Mid = Low + (High - Low) / 2;
			if (File.LineStarts[Mid] <= Location.ByteOffset)
			{
				Low = Mid + 1;
			}
			else
			{
				High = Mid;
			}
		}
		const int32 LineIndex = FMath::Max(0, Low - 1);
		OutLineColumn.Line = static_cast<asUINT>(LineIndex + 1);
		OutLineColumn.Column = Location.ByteOffset - File.LineStarts[LineIndex] + 1;
		return true;
	}

	asCSourceOriginID asCSourceSnapshot::AddOrigin(const asSSourceOriginRecord& Record)
	{
		if (IsFrozen()
			|| !Record.ProducedRange.IsOrdered()
			|| Record.ProducedRange.Begin.SnapshotID != SnapshotID
			|| !IsValidFileID(Record.ProducedRange.Begin.FileID))
		{
			return asCSourceOriginID();
		}

		const asSImpl::asSFile& ProducedFile = *Impl->Files[Record.ProducedRange.Begin.FileID.Value - 1];
		if (Record.ProducedRange.End.ByteOffset > static_cast<asUINT>(ProducedFile.Utf8Bytes.Num()))
		{
			return asCSourceOriginID();
		}

		if (Record.ParentOrigin.IsValid())
		{
			if (Record.ParentOrigin.SnapshotID != SnapshotID
				|| Record.ParentOrigin.Value > static_cast<asUINT>(Impl->Origins.Num())
				|| !Record.ParentRange.IsOrdered()
				|| Record.ParentRange.Begin.SnapshotID != SnapshotID
				|| !IsValidFileID(Record.ParentRange.Begin.FileID))
			{
				return asCSourceOriginID();
			}
			const asSImpl::asSFile& ParentFile = *Impl->Files[Record.ParentRange.Begin.FileID.Value - 1];
			if (Record.ParentRange.End.ByteOffset > static_cast<asUINT>(ParentFile.Utf8Bytes.Num()))
			{
				return asCSourceOriginID();
			}
		}

		asSSourceOriginRecord Stored = Record;
		Stored.ID = asCSourceOriginID(SnapshotID, static_cast<asUINT>(Impl->Origins.Num() + 1));
		Impl->Origins.Add(MoveTemp(Stored));
		return Impl->Origins.Last().ID;
	}

	bool asCSourceSnapshot::FindOrigins(
		const asCSourceRange& Range,
		TArray<asSSourceOriginRecord>& OutRecords) const
	{
		OutRecords.Reset();
		if (!IsFrozen()
			|| !Range.IsOrdered()
			|| Range.Begin.SnapshotID != SnapshotID
			|| !IsValidFileID(Range.Begin.FileID))
		{
			return false;
		}

		for (const asSSourceOriginRecord& Record : Impl->Origins)
		{
			if (asSourceRangesOverlap(Record.ProducedRange, Range))
			{
				OutRecords.Add(Record);
			}
		}
		OutRecords.Sort([](const asSSourceOriginRecord& Left, const asSSourceOriginRecord& Right)
		{
			if (Left.ProducedRange.Begin.ByteOffset != Right.ProducedRange.Begin.ByteOffset)
			{
				return Left.ProducedRange.Begin.ByteOffset < Right.ProducedRange.Begin.ByteOffset;
			}
			if (Left.ProducedRange.End.ByteOffset != Right.ProducedRange.End.ByteOffset)
			{
				return Left.ProducedRange.End.ByteOffset > Right.ProducedRange.End.ByteOffset;
			}
			return Left.ID.Value < Right.ID.Value;
		});
		return true;
	}

	bool asCSourceSnapshot::TryResolveOriginChain(
		asCSourceOriginID OriginID,
		TArray<asSSourceOriginRecord>& OutChain) const
	{
		OutChain.Reset();
		if (!IsFrozen()
			|| !OriginID.IsValid()
			|| OriginID.SnapshotID != SnapshotID
			|| OriginID.Value > static_cast<asUINT>(Impl->Origins.Num()))
		{
			return false;
		}

		asCSourceOriginID Current = OriginID;
		for (int32 Depth = 0; Depth < Impl->Origins.Num(); ++Depth)
		{
			const asSSourceOriginRecord& Record = Impl->Origins[Current.Value - 1];
			OutChain.Add(Record);
			if (!Record.ParentOrigin.IsValid())
			{
				return true;
			}
			if (Record.ParentOrigin.SnapshotID != SnapshotID
				|| Record.ParentOrigin.Value >= Current.Value)
			{
				OutChain.Reset();
				return false;
			}
			Current = Record.ParentOrigin;
		}

		OutChain.Reset();
		return false;
	}

END_AS_NAMESPACE
