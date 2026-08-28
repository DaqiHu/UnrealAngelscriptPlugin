#ifndef AS_DECL_H
#define AS_DECL_H

#include "as_array.h"
#include "as_ast_kind.h"
#include "as_ast_type.h"
#include "as_source_location.h"
#include "as_string.h"

BEGIN_AS_NAMESPACE

// Writers use asCASTContext construction APIs. Fields stay public POD for placement-new.
class ANGELSCRIPTRUNTIME_API asCDecl
{
public:
	asASTDeclId id;
	asEASTDeclKind kind;
	asASTDeclId parent;
	asCSourceRange range;
	asCQualType type;
	asCString name;
	asCString stableKey;
	asCArray<asASTDeclId> children;
	asCArray<asASTDeclId> captures;
	asASTStmtId body;
	asDWORD traits;
	asEASTAccessorKind accessorKind;
	asASTDeclId accessorField;
	asASTDeclId accessSpecifier;
	asCString origin;
	asCString defaultArg;
	asCArray<asCString> dependencies;
	asCArray<asASTDeclId> bases;
	asCArray<asASTExprId> inits;
	int byteOffset;
	int byteSize;
	asUINT byteAlignment;
	bool hasConstantValue;
	asQWORD constantValue;

	asCDecl()
		: kind(asAST_DECL_INVALID)
		, traits(0)
		, accessorKind(asAST_ACCESSOR_NONE)
		, byteOffset(-1)
		, byteSize(-1)
		, byteAlignment(0)
		, hasConstantValue(false)
		, constantValue(0)
	{
	}
};

inline bool asASTIsa(const asCDecl* decl, asEASTDeclKind kind)
{
	return decl != 0 && decl->kind == kind;
}

END_AS_NAMESPACE

#endif
