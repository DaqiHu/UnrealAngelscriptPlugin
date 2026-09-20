/*
   Maintained Unreal AngelScript fork — immutable frontend source queries.
*/

#include "frontend/Basic/as_source_manager.h"

#include "Math/NumericLimits.h"
#include "Math/UnrealMathUtility.h"

#include <atomic>
#include <mutex>

BEGIN_AS_NAMESPACE

namespace
{
	std::atomic<uint64> GNextSourceInputID{1};

	uint64 AllocateSourceInputID()
	{
		uint64 SnapshotID = GNextSourceInputID.fetch_add(1, std::memory_order_relaxed);
		if (SnapshotID == 0)
		{
			SnapshotID = GNextSourceInputID.fetch_add(1, std::memory_order_relaxed);
		}
		return SnapshotID;
	}

	TConstArrayView<uint8> ViewSourceBytes(const FAngelscriptSource& Source)
	{
		const FUtf8String& Text = Source.GetSourceText();
		return TConstArrayView<uint8>(
			reinterpret_cast<const uint8*>(*Text),
			Text.Len());
	}

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

class asSSourceIndex
{
public:
	struct asSFile
	{
		TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe> Source;
		FString LogicalSourceKey;
		uint64 ContentRevision = 0;
		mutable std::once_flag LineMapOnce;
		mutable TArray<asUINT> LineStarts;
		mutable std::atomic<bool> bLineMapMaterialized{false};

		explicit asSFile(TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe> InSource)
			: Source(MoveTemp(InSource))
			, LogicalSourceKey(Source->GetPath())
			, ContentRevision(ComputeContentRevision(ViewSourceBytes(*Source)))
		{
		}

		TConstArrayView<uint8> GetBytes() const
		{
			return ViewSourceBytes(*Source);
		}
	};

	TArray<TUniquePtr<asSFile>> Files;
	TArray<asSSourceOriginRecord> Origins;
	uint64 SnapshotID = 0;
	bool bReady = false;

	asSSourceIndex()
		: SnapshotID(AllocateSourceInputID())
	{
	}
};

bool asTryConvertSourceByteLength(uint64 ByteCount, asUINT& OutLength)
{
	if (ByteCount > static_cast<uint64>(TNumericLimits<asUINT>::Max()))
	{
		OutLength = 0;
		return false;
	}
	OutLength = static_cast<asUINT>(ByteCount);
	return true;
}

asCSourceManager::asCSourceManager()
	: Index(MakeShared<asSSourceIndex, ESPMode::ThreadSafe>())
{
}

asCSourceManager::asCSourceManager(
	TConstArrayView<TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>> Sources)
	: Index(MakeShared<asSSourceIndex, ESPMode::ThreadSafe>())
{
	if (Sources.Num() == 0)
	{
		return;
	}

	TArray<TUniquePtr<asSSourceIndex::asSFile>> Files;
	Files.Reserve(Sources.Num());
	for (const TSharedRef<const FAngelscriptSource, ESPMode::ThreadSafe>& Source : Sources)
	{
		if (!Source->bHasSourceText || Source->GetPath().IsEmpty())
		{
			return;
		}

		asUINT Length = 0;
		if (!asTryConvertSourceByteLength(
			static_cast<uint64>(Source->GetSourceText().Len()),
			Length))
		{
			return;
		}

		for (const TUniquePtr<asSSourceIndex::asSFile>& Existing : Files)
		{
			if (FStringView(Existing->LogicalSourceKey).Equals(Source->GetPath(), ESearchCase::CaseSensitive))
			{
				return;
			}
		}

		Files.Add(MakeUnique<asSSourceIndex::asSFile>(Source));
	}

	Index->Files = MoveTemp(Files);
	Index->bReady = true;
}

asCSourceManager::asCSourceManager(const asCSourceManager& Other) = default;
asCSourceManager& asCSourceManager::operator=(const asCSourceManager& Other) = default;
asCSourceManager::asCSourceManager(asCSourceManager&& Other) noexcept = default;
asCSourceManager& asCSourceManager::operator=(asCSourceManager&& Other) noexcept = default;
asCSourceManager::~asCSourceManager() = default;

uint64 asCSourceManager::GetSnapshotID() const
{
	return Index.IsValid() ? Index->SnapshotID : 0;
}

bool asCSourceManager::IsReady() const
{
	return Index.IsValid() && Index->bReady;
}

asUINT asCSourceManager::GetFileCount() const
{
	return Index.IsValid() ? static_cast<asUINT>(Index->Files.Num()) : 0;
}

bool asCSourceManager::IsValidFileID(asCSourceFileID FileID) const
{
	return IsReady()
		&& FileID.IsValid()
		&& FileID.Value <= static_cast<asUINT>(Index->Files.Num());
}

bool asCSourceManager::TryGetBytes(
	asCSourceFileID FileID,
	TConstArrayView<uint8>& OutBytes) const
{
	OutBytes = TConstArrayView<uint8>();
	if (!IsValidFileID(FileID))
	{
		return false;
	}
	OutBytes = Index->Files[FileID.Value - 1]->GetBytes();
	return true;
}

bool asCSourceManager::TryGetLogicalSourceKey(
	asCSourceFileID FileID,
	FStringView& OutLogicalSourceKey) const
{
	OutLogicalSourceKey = FStringView();
	if (!IsValidFileID(FileID))
	{
		return false;
	}
	OutLogicalSourceKey = FStringView(Index->Files[FileID.Value - 1]->LogicalSourceKey);
	return true;
}

bool asCSourceManager::TryGetContentRevision(
	asCSourceFileID FileID,
	uint64& OutContentRevision) const
{
	OutContentRevision = 0;
	if (!IsValidFileID(FileID))
	{
		return false;
	}
	OutContentRevision = Index->Files[FileID.Value - 1]->ContentRevision;
	return true;
}

asCSourceFileID asCSourceManager::FindFile(
	FStringView LogicalSourceKey,
	uint64 ContentRevision) const
{
	if (!IsReady() || LogicalSourceKey.IsEmpty() || ContentRevision == 0)
	{
		return asCSourceFileID();
	}
	for (int32 FileIndex = 0; FileIndex < Index->Files.Num(); ++FileIndex)
	{
		const asSSourceIndex::asSFile& File = *Index->Files[FileIndex];
		if (File.ContentRevision == ContentRevision
			&& FStringView(File.LogicalSourceKey).Equals(LogicalSourceKey, ESearchCase::CaseSensitive))
		{
			return asCSourceFileID(static_cast<asUINT>(FileIndex + 1));
		}
	}
	return asCSourceFileID();
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
	return TryGetBytes(Location.FileID, Bytes)
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
	if (!TryGetBytes(Range.Begin.FileID, FileBytes))
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
	return IsValidFileID(FileID)
		&& Index->Files[FileID.Value - 1]->bLineMapMaterialized.load(std::memory_order_acquire);
}

bool asCSourceManager::TryGetLineColumn(
	const asCSourceLocation& Location,
	asSSourceLineColumn& OutLineColumn) const
{
	OutLineColumn = asSSourceLineColumn();
	if (!IsValidLocation(Location))
	{
		return false;
	}

	const asSSourceIndex::asSFile& File = *Index->Files[Location.FileID.Value - 1];
	const TConstArrayView<uint8> Bytes = File.GetBytes();
	std::call_once(File.LineMapOnce, [&File, Bytes]()
	{
		File.LineStarts.Add(0);
		for (int32 ByteIndex = 0; ByteIndex < Bytes.Num(); ++ByteIndex)
		{
			if (Bytes[ByteIndex] == static_cast<uint8>('\n'))
			{
				File.LineStarts.Add(static_cast<asUINT>(ByteIndex + 1));
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

asCSourceOriginID asCSourceManager::AddOrigin(const asSSourceOriginRecord& Record)
{
	if (!IsReady()
		|| !Record.ProducedRange.IsOrdered()
		|| Record.ProducedRange.Begin.SnapshotID != GetSnapshotID()
		|| !IsValidFileID(Record.ProducedRange.Begin.FileID))
	{
		return asCSourceOriginID();
	}

	TConstArrayView<uint8> ProducedBytes;
	if (!TryGetBytes(Record.ProducedRange.Begin.FileID, ProducedBytes)
		|| Record.ProducedRange.End.ByteOffset > static_cast<asUINT>(ProducedBytes.Num()))
	{
		return asCSourceOriginID();
	}

	if (Record.ParentOrigin.IsValid())
	{
		if (Record.ParentOrigin.SnapshotID != GetSnapshotID()
			|| Record.ParentOrigin.Value > static_cast<asUINT>(Index->Origins.Num())
			|| !Record.ParentRange.IsOrdered()
			|| Record.ParentRange.Begin.SnapshotID != GetSnapshotID()
			|| !IsValidFileID(Record.ParentRange.Begin.FileID))
		{
			return asCSourceOriginID();
		}
		TConstArrayView<uint8> ParentBytes;
		if (!TryGetBytes(Record.ParentRange.Begin.FileID, ParentBytes)
			|| Record.ParentRange.End.ByteOffset > static_cast<asUINT>(ParentBytes.Num()))
		{
			return asCSourceOriginID();
		}
	}

	asSSourceOriginRecord Stored = Record;
	Stored.ID = asCSourceOriginID(GetSnapshotID(), static_cast<asUINT>(Index->Origins.Num() + 1));
	Index->Origins.Add(MoveTemp(Stored));
	return Index->Origins.Last().ID;
}

bool asCSourceManager::FindOrigins(
	const asCSourceRange& Range,
	TArray<asSSourceOriginRecord>& OutRecords) const
{
	OutRecords.Reset();
	if (!IsReady()
		|| !Range.IsOrdered()
		|| Range.Begin.SnapshotID != GetSnapshotID()
		|| !IsValidFileID(Range.Begin.FileID))
	{
		return false;
	}

	for (const asSSourceOriginRecord& Record : Index->Origins)
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

bool asCSourceManager::TryResolveOriginChain(
	asCSourceOriginID OriginID,
	TArray<asSSourceOriginRecord>& OutChain) const
{
	OutChain.Reset();
	if (!IsReady()
		|| !OriginID.IsValid()
		|| OriginID.SnapshotID != GetSnapshotID()
		|| OriginID.Value > static_cast<asUINT>(Index->Origins.Num()))
	{
		return false;
	}

	asCSourceOriginID Current = OriginID;
	for (int32 Depth = 0; Depth < Index->Origins.Num(); ++Depth)
	{
		const asSSourceOriginRecord& Record = Index->Origins[Current.Value - 1];
		OutChain.Add(Record);
		if (!Record.ParentOrigin.IsValid())
		{
			return true;
		}
		if (Record.ParentOrigin.SnapshotID != GetSnapshotID()
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
	if (!TryGetLogicalSourceKey(Range.Begin.FileID, LogicalSourceKey)
		|| !TryGetContentRevision(Range.Begin.FileID, ContentRevision))
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

	const asCSourceFileID FileID = FindFile(
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
