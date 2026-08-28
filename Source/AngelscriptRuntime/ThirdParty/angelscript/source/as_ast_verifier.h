#ifndef AS_AST_VERIFIER_H
#define AS_AST_VERIFIER_H

#include "as_ast_context.h"
#include "as_ast_traversal.h"
#include "as_string.h"

BEGIN_AS_NAMESPACE

struct asSAstVerifyResult
{
	asEASTVerifyCategory category;
	asCSourceRange range;
	asCString detail;
	asSASTNodeRef offendingNode;
	asCString offendingKind;
	asEASTEdgeRole edgeRole;
	asSASTNodeRef relatedNode;
	asCString relatedKind;
	asCString logicalSource;
	int line;
	int column;
	asCString rootPath;
	asCString localSubtree;

	asSAstVerifyResult()
		: category(asAST_VERIFY_OK)
		, edgeRole(asAST_EDGE_INVALID)
		, line(0)
		, column(0)
	{
	}
};

ANGELSCRIPTRUNTIME_API int asCASTVerify(const asCASTContext& context, asSAstVerifyResult& result);
ANGELSCRIPTRUNTIME_API int asCASTVerifyPublication(const asCASTContext& context, asSAstVerifyResult& result);

END_AS_NAMESPACE

#endif
