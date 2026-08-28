/*
   Maintained Unreal AngelScript fork - source provenance shared by native
   parsing, Canonical SourceManager/Sema, diagnostics, and generated sources.

   This contract is intentionally owned by the Canonical AST frontend. It is
   installed on asCScriptCode before parsing and remains valid after the
   function-owned HIR implementation is removed.
*/

#ifndef AS_SOURCE_PROVENANCE_H
#define AS_SOURCE_PROVENANCE_H

#include "as_config.h"
#include "as_string.h"

BEGIN_AS_NAMESPACE

/** Optional authored source coordinate. Offsets are UTF-8 byte offsets. */
struct asSSourceAnchor
{
	asCString section;
	asUINT sourceOffset = 0;
	asUINT sourceLength = 0;
	asUINT row = 0;
	asUINT column = 0;
};

enum class asEGeneratedSourceKind : asBYTE
{
	None,
	LiteralAsset,
	SubsystemAccessor,
	ClassHelper,
	StaticClassHelper,
	DelegateHelper,
	RangeBasedFor,
};

struct asSGeneratedSourceOrigin
{
	asEGeneratedSourceKind kind = asEGeneratedSourceKind::None;
	asCString generatorName;
	bool hasAuthoredAnchor = false;
	asSSourceAnchor authoredAnchor;
};

/**
 * One processed compiler span plus optional authored/generated provenance.
 * The processed tuple is authoritative and is never relabelled as authored.
 */
struct asSSourceSpan
{
	asCString section;
	asUINT sourceOffset = 0;
	asUINT sourceLength = 0;
	asUINT row = 0;
	asUINT column = 0;

	bool hasAuthoredOrigin = false;
	asSSourceAnchor authoredOrigin;

	bool hasGeneratedOrigin = false;
	asSGeneratedSourceOrigin generatedOrigin;
};

/** Source-section provenance installed before parsing. */
struct asSSourceProvenanceRange
{
	asUINT processedOffset = 0;
	asUINT processedLength = 0;
	bool hasAuthoredOrigin = false;
	asSSourceAnchor authoredOrigin;
	bool hasGeneratedOrigin = false;
	asSGeneratedSourceOrigin generatedOrigin;
};

END_AS_NAMESPACE

#endif
