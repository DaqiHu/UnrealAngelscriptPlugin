/*
   Maintained Unreal AngelScript fork — Clang SourceManager analogue.
*/

#ifndef AS_SOURCE_MANAGER_H
#define AS_SOURCE_MANAGER_H

#include "as_array.h"
#include "as_source_location.h"
#include "as_source_provenance.h"
#include "as_string.h"

BEGIN_AS_NAMESPACE

struct asSSourceLineColumn
{
	int row;
	int column;
};

class ANGELSCRIPTRUNTIME_API asCSourceManager
{
public:
	asCSourceManager();
	~asCSourceManager();

	asASTFileID AddSection(
		const char* logicalKey,
		asEASTSourceOrigin origin,
		const char* bytes,
		size_t byteCount,
		int lineOffset = 0,
		const asSSourceProvenanceRange* provenanceRanges = 0,
		asUINT provenanceRangeCount = 0);

	asUINT GetSectionCount() const { return sections.GetLength(); }
	asASTFileID GetFileIDByIndex(asUINT index) const { return index + 1; }

	const char* GetLogicalKey(asASTFileID fileID) const;
	asEASTSourceOrigin GetOrigin(asASTFileID fileID) const;
	const char* GetBuffer(asASTFileID fileID) const;
	asUINT GetBufferLength(asASTFileID fileID) const;
	int GetLineOffset(asASTFileID fileID) const;
	asUINT GetSourceProvenanceRangeCount(asASTFileID fileID) const;
	bool GetSourceProvenanceRange(
		asASTFileID fileID,
		asUINT index,
		asSSourceProvenanceRange& outRange) const;

	asCSourceLocation MakeLocation(asASTFileID fileID, asUINT offset) const;
	asCSourceRange MakeRange(asASTFileID fileID, asUINT beginOffset, asUINT endOffset) const;
	bool IsValidRange(const asCSourceRange& range) const;
	bool ResolveSourceSpan(
		const asCSourceRange& range,
		asSSourceSpan& outSpan) const;

	bool GetLineColumn(const asCSourceLocation& loc, asSSourceLineColumn& out) const;

	// Restore path: same logical key + origin can reuse a snapshot-local FileID only
	// when the source bytes and line mapping are identical. A mismatch is rejected
	// so one snapshot cannot assign stale coordinates to changed source.
	asASTFileID RemapLogical(
		const char* logicalKey,
		asEASTSourceOrigin origin,
		const char* bytes,
		size_t byteCount,
		int lineOffset = 0,
		const asSSourceProvenanceRange* provenanceRanges = 0,
		asUINT provenanceRangeCount = 0);

	asASTFileID FindFile(const char* logicalKey, asEASTSourceOrigin origin) const;

private:
	struct asSSection
	{
		asCString logicalKey;
		asEASTSourceOrigin origin;
		asCString buffer;
		int lineOffset;
		asCArray<asUINT> lineStarts;
		asCArray<asSSourceProvenanceRange> provenanceRanges;
	};

	asSSection* GetSection(asASTFileID fileID);
	const asSSection* GetSection(asASTFileID fileID) const;
	void BuildLineStarts(asSSection& section);
	bool ValidateProvenanceRanges(
		const asSSourceProvenanceRange* ranges,
		asUINT rangeCount,
		size_t byteCount) const;
	bool HasSameProvenanceRanges(
		const asSSection& section,
		const asSSourceProvenanceRange* ranges,
		asUINT rangeCount) const;
	void ApplySourceProvenance(
		const asSSection& section,
		asSSourceSpan& span) const;

	asCArray<asSSection*> sections;
};

END_AS_NAMESPACE

#endif
