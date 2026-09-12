/*
   Maintained Unreal AngelScript fork — snapshot-owned source provenance.
*/

#pragma once

#include "frontend/Basic/as_source_location.h"

#include "Containers/UnrealString.h"

BEGIN_AS_NAMESPACE

enum class asESourceOriginKind : uint8
	{
		Direct,
		Directive,
		Transformed,
		Synthetic,
	};

	struct asCSourceOriginID
	{
		uint64 SnapshotID = 0;
		asUINT Value = 0;

		asCSourceOriginID() = default;
		asCSourceOriginID(uint64 InSnapshotID, asUINT InValue)
			: SnapshotID(InSnapshotID)
			, Value(InValue)
		{
		}

		bool IsValid() const { return SnapshotID != 0 && Value != 0; }
		bool operator==(const asCSourceOriginID& Other) const
		{
			return SnapshotID == Other.SnapshotID && Value == Other.Value;
		}
		bool operator!=(const asCSourceOriginID& Other) const { return !(*this == Other); }
	};

	struct asSSourceOriginRecord
	{
		asCSourceOriginID ID;
		asESourceOriginKind Kind = asESourceOriginKind::Direct;
		asCSourceRange ProducedRange;
		asCSourceRange ParentRange;
		asCSourceOriginID ParentOrigin;
		FString Producer;
	};

	ANGELSCRIPTRUNTIME_API bool asSourceRangesOverlap(
		const asCSourceRange& Left,
		const asCSourceRange& Right);

END_AS_NAMESPACE

