/*
   Maintained Unreal AngelScript fork — snapshot-bound frontend coordinates.
*/

#pragma once

#include "as_config.h"

#include "Containers/UnrealString.h"

BEGIN_AS_NAMESPACE

struct asCSourceFileID
	{
		asUINT Value = 0;

		asCSourceFileID() = default;
		explicit asCSourceFileID(asUINT InValue)
			: Value(InValue)
		{
		}

		bool IsValid() const { return Value != 0; }
		bool operator==(const asCSourceFileID& Other) const { return Value == Other.Value; }
		bool operator!=(const asCSourceFileID& Other) const { return !(*this == Other); }
	};

	struct asCSourceLocation
	{
		uint64 SnapshotID = 0;
		asCSourceFileID FileID;
		asUINT ByteOffset = 0;

		asCSourceLocation() = default;
		asCSourceLocation(uint64 InSnapshotID, asCSourceFileID InFileID, asUINT InByteOffset)
			: SnapshotID(InSnapshotID)
			, FileID(InFileID)
			, ByteOffset(InByteOffset)
		{
		}

		bool IsValid() const { return SnapshotID != 0 && FileID.IsValid(); }
		bool operator==(const asCSourceLocation& Other) const
		{
			return SnapshotID == Other.SnapshotID
				&& FileID == Other.FileID
				&& ByteOffset == Other.ByteOffset;
		}
		bool operator!=(const asCSourceLocation& Other) const { return !(*this == Other); }
	};

	struct asCSourceRange
	{
		asCSourceLocation Begin;
		asCSourceLocation End;

		asCSourceRange() = default;
		asCSourceRange(asCSourceLocation InBegin, asCSourceLocation InEnd)
			: Begin(InBegin)
			, End(InEnd)
		{
		}

		bool HasConsistentOwner() const
		{
			return Begin.IsValid()
				&& End.IsValid()
				&& Begin.SnapshotID == End.SnapshotID
				&& Begin.FileID == End.FileID;
		}

		bool IsOrdered() const { return HasConsistentOwner() && Begin.ByteOffset <= End.ByteOffset; }
		asUINT Length() const { return IsOrdered() ? End.ByteOffset - Begin.ByteOffset : 0; }
		bool operator==(const asCSourceRange& Other) const { return Begin == Other.Begin && End == Other.End; }
	};

	struct asSStableSourceAnchor
	{
		FString LogicalSourceKey;
		uint64 ContentRevision = 0;
		asUINT BeginByteOffset = 0;
		asUINT EndByteOffset = 0;

		bool IsValid() const
		{
			return !LogicalSourceKey.IsEmpty()
				&& ContentRevision != 0
				&& BeginByteOffset <= EndByteOffset;
		}
	};

END_AS_NAMESPACE

