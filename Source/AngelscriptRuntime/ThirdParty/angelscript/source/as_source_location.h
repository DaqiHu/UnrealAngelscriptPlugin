/*
   Maintained Unreal AngelScript fork — Clang-style compact source coords.
*/

#ifndef AS_SOURCE_LOCATION_H
#define AS_SOURCE_LOCATION_H

#include "as_ast_fwd.h"

BEGIN_AS_NAMESPACE

struct asCSourceLocation
{
	asASTFileID fileID;
	asUINT offset;

	asCSourceLocation() : fileID(0), offset(0) {}
	asCSourceLocation(asASTFileID InFile, asUINT InOffset) : fileID(InFile), offset(InOffset) {}

	bool IsValid() const { return fileID != 0; }

	bool operator==(const asCSourceLocation& Other) const
	{
		return fileID == Other.fileID && offset == Other.offset;
	}

	bool operator!=(const asCSourceLocation& Other) const
	{
		return !(*this == Other);
	}
};

struct asCSourceRange
{
	asCSourceLocation begin;
	asCSourceLocation end;

	asCSourceRange() {}
	asCSourceRange(asCSourceLocation Begin, asCSourceLocation End) : begin(Begin), end(End) {}

	bool IsValid() const
	{
		return begin.IsValid() && end.IsValid()
			&& begin.fileID == end.fileID
			&& begin.offset <= end.offset;
	}

	asUINT Length() const
	{
		return IsValid() ? (end.offset - begin.offset) : 0;
	}

	bool operator==(const asCSourceRange& Other) const
	{
		return begin == Other.begin && end == Other.end;
	}
};

END_AS_NAMESPACE

#endif
