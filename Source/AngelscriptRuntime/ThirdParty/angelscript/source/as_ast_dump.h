#ifndef AS_AST_DUMP_H
#define AS_AST_DUMP_H

#include "as_ast_context.h"
#include "as_ast_traversal.h"
#include "as_ast_verifier.h"
#include "as_string.h"

BEGIN_AS_NAMESPACE

// Structured dumps are a deterministic diagnostic surface for tests, tooling,
// and human inspection. They are deliberately not a Cache V2 serialization
// contract or an input to semantic identity.
enum asEASTStructuredDumpFormat : asBYTE
{
	asAST_STRUCTURED_DUMP_TREE = 0,
	asAST_STRUCTURED_DUMP_JSON,
};

struct asSASTStructuredDumpFilter
{
	asCString moduleName;
	asCString stableDeclKey;
	asEASTNodeClass nodeClass;
	asUINT nodeId;
	asCString nodeKind;
	asCString logicalSource;

	asSASTStructuredDumpFilter()
		: nodeClass(asAST_NODE_INVALID)
		, nodeId(0)
	{
	}

	bool HasNodePredicates() const
	{
		return stableDeclKey.GetLength() != 0
			|| nodeClass != asAST_NODE_INVALID
			|| nodeId != 0
			|| nodeKind.GetLength() != 0
			|| logicalSource.GetLength() != 0;
	}
};

struct asSASTStructuredDumpOptions
{
	asEASTStructuredDumpFormat format;
	asSASTStructuredDumpFilter filter;
	bool includeSourceSnippets;
	asUINT maxSnippetBytes;

	asSASTStructuredDumpOptions()
		: format(asAST_STRUCTURED_DUMP_TREE)
		, includeSourceSnippets(false)
		, maxSnippetBytes(160)
	{
	}
};

ANGELSCRIPTRUNTIME_API void asCASTDump(const asCASTContext& context, asCString& out);
ANGELSCRIPTRUNTIME_API int asCASTStructuredDump(
	const asCASTContext& context,
	const asSASTStructuredDumpOptions& options,
	asCString& out);
// Exact read-only query results. Unlike the filtered structured dump this does
// not include structural ancestors or matched-node descendants.
ANGELSCRIPTRUNTIME_API int asCASTQuery(
	const asCASTContext& context,
	const asSASTStructuredDumpOptions& options,
	asCString& out);
ANGELSCRIPTRUNTIME_API void asCASTShadowDiff(const asCASTContext& left, const asCASTContext& right, asCString& out);
// Compares one Canonical declaration's owner/type/trait/source/dependency
// identity against a Builder/Runtime function without copying either graph
// into the other. out is "match" or a deterministic "mismatch <field> ..." path.
class asCScriptFunction;
class asCObjectType;
class asCTypeInfo;
class asCModule;
class asCScriptEngine;
ANGELSCRIPTRUNTIME_API void asCASTShadowDiffBuilderFunctionIdentity(
	const asCASTContext& canonical,
	asASTDeclId canonicalDecl,
	const asCScriptFunction& builderFunction,
	asCString& out,
	bool compareDependencies = true);
ANGELSCRIPTRUNTIME_API void asCASTShadowDiffBuilderTypeIdentity(
	const asCASTContext& canonical,
	asASTDeclId canonicalDecl,
	const asCObjectType& builderType,
	asCString& out);
ANGELSCRIPTRUNTIME_API void asCASTShadowDiffBuilderTypeIdentity(
	const asCASTContext& canonical,
	asASTDeclId canonicalDecl,
	const asCTypeInfo& builderType,
	asCString& out);
// Production publication gate: sealed Canonical function Decls must match the
// candidate Runtime functions. A mismatch fails closed and does not merge graphs.
ANGELSCRIPTRUNTIME_API int asCASTValidateBuilderShadowIdentity(
	const asCASTContext& canonical,
	asCModule& candidate,
	asCScriptEngine* engine);
ANGELSCRIPTRUNTIME_API void asCASTFormatVerifyResult(const asSAstVerifyResult& result, asCString& out);
ANGELSCRIPTRUNTIME_API void asCASTDumpOnVerifyFail(const asCASTContext& context, const asSAstVerifyResult& result, asCString& out);

END_AS_NAMESPACE

#endif
