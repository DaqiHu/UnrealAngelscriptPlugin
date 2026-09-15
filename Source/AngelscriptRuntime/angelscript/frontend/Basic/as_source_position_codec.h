/*
   Maintained Unreal AngelScript fork — snapshot-bound UTF-8/UTF-16 positions.
*/

#pragma once

#include "frontend/Basic/as_source_manager.h"

BEGIN_AS_NAMESPACE

class ANGELSCRIPTRUNTIME_API asCSourcePositionCodec
{
public:
	explicit asCSourcePositionCodec(const asCSourceManager& Snapshot);

	bool ByteToPosition(asCSourceLocation Byte, bool bUtf16, int32& Line, int32& Column) const;
	bool PositionToByte(
		asCSourceFileID File,
		int32 Line,
		int32 Column,
		bool bUtf16,
		asCSourceLocation& Out) const;

private:
	const asCSourceManager* Snapshot = nullptr;
};

END_AS_NAMESPACE
