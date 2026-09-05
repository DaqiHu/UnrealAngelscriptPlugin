/*
   Maintained Unreal AngelScript fork — immutable frontend source queries.
*/

#pragma once

#include "frontend/as_source_snapshot.h"

#include "Templates/SharedPointer.h"

BEGIN_AS_NAMESPACE

class ANGELSCRIPTRUNTIME_API asCSourceManager
	{
	public:
		explicit asCSourceManager(TSharedRef<asCSourceSnapshot, ESPMode::ThreadSafe> InSnapshot);

		uint64 GetSnapshotID() const;
		bool IsReady() const;

		asCSourceLocation MakeLocation(asCSourceFileID FileID, asUINT ByteOffset) const;
		asCSourceRange MakeRange(asCSourceFileID FileID, asUINT BeginByteOffset, asUINT EndByteOffset) const;
		bool IsValidLocation(const asCSourceLocation& Location) const;
		bool IsValidRange(const asCSourceRange& Range) const;
		bool TryGetBytes(const asCSourceRange& Range, TConstArrayView<uint8>& OutBytes) const;
		bool HasMaterializedLineMap(asCSourceFileID FileID) const;
		bool TryGetLineColumn(const asCSourceLocation& Location, asSSourceLineColumn& OutLineColumn) const;
		bool FindOrigins(const asCSourceRange& Range, TArray<asSSourceOriginRecord>& OutRecords) const;
		bool TryResolveOriginChain(
			asCSourceOriginID OriginID,
			TArray<asSSourceOriginRecord>& OutChain) const;

		bool TryCreateStableAnchor(const asCSourceRange& Range, asSStableSourceAnchor& OutAnchor) const;
		bool TryRelocateStableAnchor(const asSStableSourceAnchor& Anchor, asCSourceRange& OutRange) const;

		const TSharedRef<asCSourceSnapshot, ESPMode::ThreadSafe>& GetSnapshot() const { return Snapshot; }

	private:
		TSharedRef<asCSourceSnapshot, ESPMode::ThreadSafe> Snapshot;
	};

END_AS_NAMESPACE
