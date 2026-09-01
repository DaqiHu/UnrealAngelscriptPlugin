#ifndef AS_EXPR_H
#define AS_EXPR_H

#include "as_array.h"
#include "as_ast_kind.h"
#include "as_ast_type.h"
#include "as_source_location.h"
#include "as_string.h"

BEGIN_AS_NAMESPACE

enum asEASTCallArgumentOrigin : asBYTE
{
	asAST_CALL_ARGUMENT_INVALID = 0,
	asAST_CALL_ARGUMENT_POSITIONAL,
	asAST_CALL_ARGUMENT_NAMED,
	asAST_CALL_ARGUMENT_DEFAULT,
	asAST_CALL_ARGUMENT_HIDDEN,
	asAST_CALL_ARGUMENT_IMPLICIT_RECEIVER,
	asAST_CALL_ARGUMENT_GENERATED,
};

static const asUINT asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL = asUINT(-1);

struct ANGELSCRIPTRUNTIME_API asSASTCallArgument
{
	asASTExprId expression;
	asASTDeclId formal;
	asCQualType formalType;
	asCSourceRange range;
	asCString authoredName;
	asUINT formalIndex;
	asUINT sourceOrdinal;
	asEASTCallArgumentOrigin origin;

	asSASTCallArgument()
		: formalIndex(asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL)
		, sourceOrdinal(asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL)
		, origin(asAST_CALL_ARGUMENT_INVALID)
	{
	}
};

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
	asCArray<asSASTCallArgument> callArguments;

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
