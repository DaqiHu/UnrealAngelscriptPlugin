/*
   Maintained Unreal AngelScript fork — immutable frontend source snapshot.
*/

#pragma once

#include "frontend/Basic/as_source_location.h"
#include "frontend/Basic/as_source_provenance.h"

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/StringView.h"
#include "Templates/UniquePtr.h"

BEGIN_AS_NAMESPACE

struct asSSourceLineColumn
	{
		asUINT Line = 0;
		asUINT Column = 0;
	};

	class ANGELSCRIPTRUNTIME_API asCSourceSnapshot
	{
	public:
		asCSourceSnapshot();
		~asCSourceSnapshot();

		asCSourceSnapshot(const asCSourceSnapshot&) = delete;
		asCSourceSnapshot& operator=(const asCSourceSnapshot&) = delete;

		asCSourceFileID AddFile(FStringView LogicalSourceKey, TConstArrayView<uint8> Utf8Bytes);
		bool Freeze();

		uint64 GetSnapshotID() const { return SnapshotID; }
		bool IsFrozen() const;
		asUINT GetFileCount() const;

		bool IsValidFileID(asCSourceFileID FileID) const;
		bool TryGetBytes(asCSourceFileID FileID, TConstArrayView<uint8>& OutBytes) const;
		bool TryGetLogicalSourceKey(asCSourceFileID FileID, FStringView& OutLogicalSourceKey) const;
		bool TryGetContentRevision(asCSourceFileID FileID, uint64& OutContentRevision) const;
		asCSourceFileID FindFile(FStringView LogicalSourceKey, uint64 ContentRevision) const;

		bool HasMaterializedLineMap(asCSourceFileID FileID) const;
		bool TryGetLineColumn(const asCSourceLocation& Location, asSSourceLineColumn& OutLineColumn) const;

		asCSourceOriginID AddOrigin(const asSSourceOriginRecord& Record);
		bool FindOrigins(const asCSourceRange& Range, TArray<asSSourceOriginRecord>& OutRecords) const;
		bool TryResolveOriginChain(
			asCSourceOriginID OriginID,
			TArray<asSSourceOriginRecord>& OutChain) const;

	private:
		struct asSImpl;
		TUniquePtr<asSImpl> Impl;
		uint64 SnapshotID = 0;
	};

END_AS_NAMESPACE
