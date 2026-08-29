#ifndef AS_AST_SIDECAR_H
#define AS_AST_SIDECAR_H

#include "as_array.h"
#include "as_ast_context.h"

BEGIN_AS_NAMESPACE

enum asEASTSidecarError : int
{
	asAST_SIDECAR_OK = 0,
	asAST_SIDECAR_EMPTY_NOT_ABSENCE = 1,
	asAST_SIDECAR_UNKNOWN_KIND = 2,
	asAST_SIDECAR_FUNCTION_KEY_MISMATCH = 3,
	asAST_SIDECAR_PROFILE_MISMATCH = 4,
	asAST_SIDECAR_BUDGET = 5,
	asAST_SIDECAR_POINTER = 6,
	asAST_SIDECAR_MALFORMED = 7,
	asAST_SIDECAR_UNSUPPORTED_INPUT = 8,
};

// V2 is the first pointer-free DTO layout. V1 persisted a dump plus a
// declaration skeleton and is intentionally a safe restore miss. V8 appends
// pointer-free record-owned method-relation triples. V7 appended the
// pointer-free Sema-authored Canonical lifetime protocol. V6 appended
// snapshot-owned neutral source-provenance ranges; V5 appended the
// member-to-access-specifier declaration reference.
static const asUINT asAST_SIDECAR_SCHEMA_VERSION = 8;

struct asSASTSidecarHeader
{
	asCString functionKey;
	asCString profile;
	asUINT version;

	asSASTSidecarHeader()
		: version(asAST_SIDECAR_SCHEMA_VERSION)
	{
	}
};

struct asSASTSidecarRecordId
{
	asCString functionKey;
	asCString contentHash;
	asCArray<asCString> dependencies;
};

ANGELSCRIPTRUNTIME_API int asCASTEncodeSidecar(const asCASTContext& context, const asSASTSidecarHeader& header, asCArray<asBYTE>& out);
// Reads only the durable V2 owner/profile identity.  Cache graph admission
// uses this before a full DTO restore so an empty expected profile never acts
// as a production wildcard.
ANGELSCRIPTRUNTIME_API int asCASTReadSidecarHeader(const asBYTE* bytes, asUINT length, asSASTSidecarHeader& outHeader);
ANGELSCRIPTRUNTIME_API int asCASTDecodeSidecar(asCASTContext& context, const asSASTSidecarHeader& expected, const asBYTE* bytes, asUINT length);
ANGELSCRIPTRUNTIME_API int asCASTContentHash(const asBYTE* bytes, asUINT length, asCString& outHex);
ANGELSCRIPTRUNTIME_API int asCASTCollectFunctionRecords(const asCASTContext& context, const char* profile, asCArray<asSASTSidecarRecordId>& records);
ANGELSCRIPTRUNTIME_API int asCASTPlanIncrementalSidecars(
	const asCArray<asSASTSidecarRecordId>& previous,
	const asCArray<asSASTSidecarRecordId>& current,
	asCArray<asCString>& reusedKeys,
	asCArray<asCString>& rebuiltKeys,
	asCArray<asCString>& retiredKeys);
ANGELSCRIPTRUNTIME_API int asCASTCommitSidecarSet(asCArray<asSASTSidecarRecordId>& live, const asCArray<asSASTSidecarRecordId>& next);
ANGELSCRIPTRUNTIME_API int asCASTRejectNonSidecarInput(const asBYTE* bytes, asUINT length);

END_AS_NAMESPACE

#endif
