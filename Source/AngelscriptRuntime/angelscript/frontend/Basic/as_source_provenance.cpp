/*
   Maintained Unreal AngelScript fork — snapshot-owned source provenance.
*/

#include "frontend/Basic/as_source_provenance.h"

BEGIN_AS_NAMESPACE

bool asSourceRangesOverlap(
		const asCSourceRange& Left,
		const asCSourceRange& Right)
	{
		return Left.IsOrdered()
			&& Right.IsOrdered()
			&& Left.Begin.SnapshotID == Right.Begin.SnapshotID
			&& Left.Begin.FileID == Right.Begin.FileID
			&& Left.Begin.ByteOffset < Right.End.ByteOffset
			&& Right.Begin.ByteOffset < Left.End.ByteOffset;
	}

END_AS_NAMESPACE

