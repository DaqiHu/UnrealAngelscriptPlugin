#ifndef AS_STMT_H
#define AS_STMT_H

#include "as_array.h"
#include "as_ast_kind.h"
#include "as_ast_type.h"
#include "as_source_location.h"

BEGIN_AS_NAMESPACE

class ANGELSCRIPTRUNTIME_API asCStmt
{
public:
	asASTStmtId id;
	asEASTStmtKind kind;
	asASTDeclId owner;
	asCSourceRange range;
	asASTStmtId target; // break/continue/return structured target
	asASTExprId expr;
	asASTDeclId decl;
	asEASTSafePointRole safePointRole;
	asCArray<asASTStmtId> children;

	asCStmt()
		: kind(asAST_STMT_INVALID)
		, safePointRole(asAST_SAFEPOINT_NONE)
	{
	}
};

inline bool asASTIsa(const asCStmt* stmt, asEASTStmtKind kind)
{
	return stmt != 0 && stmt->kind == kind;
}

END_AS_NAMESPACE

#endif
