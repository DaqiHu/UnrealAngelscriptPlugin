#ifndef AS_EXPR_H
#define AS_EXPR_H

#include "as_array.h"
#include "as_ast_kind.h"
#include "as_ast_type.h"
#include "as_source_location.h"
#include "as_string.h"

BEGIN_AS_NAMESPACE

class ANGELSCRIPTRUNTIME_API asCExpr
{
public:
	asASTExprId id;
	asEASTExprKind kind;
	asCSourceRange range;
	asCQualType type;
	asEASTValueCategory valueCategory;
	asASTDeclId resolvedDecl;
	asASTExprId receiver;
	asCString literal;
	asQWORD literalBits;
	asEASTSafePointRole safePointRole;
	asEASTCallDispatch callDispatch;
	asCArray<asASTExprId> children;

	asCExpr()
		: kind(asAST_EXPR_INVALID)
		, valueCategory(asAST_VALUE_PRVALUE)
		, literalBits(0)
		, safePointRole(asAST_SAFEPOINT_NONE)
		, callDispatch(asAST_CALL_DISPATCH_NONE)
	{
	}
};

inline bool asASTIsa(const asCExpr* expr, asEASTExprKind kind)
{
	return expr != 0 && expr->kind == kind;
}

END_AS_NAMESPACE

#endif
