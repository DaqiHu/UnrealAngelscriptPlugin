#include "as_sema_lifetime.h"

BEGIN_AS_NAMESPACE

int asSemaAuthorLifetimeRecord(
	asCASTContext& context,
	const asSASTLifetimeRecord& record)
{
	return context.AddLifetimeRecord(record);
}

asASTExprId asCSema::ActOnMaterializeTemporary(asASTExprId inner, const asCSourceRange& range)
{
	asCQualType type;
	if( const asCExpr* innerExpr = context.GetExpr(inner) )
	{
		type = innerExpr->type;
	}
	const asASTExprId id = context.CreateExpr(asAST_EXPR_MATERIALIZE_TEMPORARY, type, asAST_VALUE_XVALUE, range);
	context.SetLiteral(id, "materialize");
	context.AddExprChild(id, inner);
	return id;
}

asASTExprId asCSema::ActOnCleanup(asASTExprId inner, const asCSourceRange& range)
{
	asCQualType type;
	if( const asCExpr* innerExpr = context.GetExpr(inner) )
	{
		type = innerExpr->type;
	}
	const asASTExprId id = context.CreateExpr(asAST_EXPR_CLEANUP, type, asAST_VALUE_PRVALUE, range);
	context.SetLiteral(id, "cleanup");
	context.AddExprChild(id, inner);
	if( const asCType* named = context.GetType(type.type) )
	{
		if( named->kind != asAST_TYPE_PRIMITIVE && named->kind != asAST_TYPE_VOID
			&& named->stableKey.GetLength() != 0 )
		{
			for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
			{
				const asCDecl* decl = context.GetDecl(asASTDeclId(i));
				if( decl == 0 || decl->kind != asAST_DECL_DESTRUCTOR )
				{
					continue;
				}
				const asCDecl* parent = context.GetDecl(decl->parent);
				if( parent && parent->kind == asAST_DECL_CLASS
					&& parent->stableKey.Equals(named->stableKey) )
				{
					context.SetResolvedDecl(id, decl->id);
					break;
				}
			}
		}
	}
	return id;
}

asASTExprId asCSema::ActOnDeferredOutArgument(
	asASTExprId receiver,
	const asCQualType& formalType,
	asASTDeclId setter,
	const asCSourceRange& range)
{
	if( !receiver.IsValid() || !setter.IsValid() || !formalType.IsValid()
		|| !formalType.IsReference()
		|| (formalType.quals & asAST_QUAL_OUT) == 0
		|| (formalType.quals & asAST_QUAL_IN) != 0 )
	{
		return asASTExprId();
	}

	const asASTExprId opaqueReceiver = ActOnOpaqueValueExpr(receiver, range);
	if( !opaqueReceiver.IsValid() )
	{
		return asASTExprId();
	}

	const asASTExprId plan = context.CreateExpr(
		asAST_EXPR_DEFERRED_OUT,
		formalType,
		asAST_VALUE_LVALUE,
		range);
	context.SetLiteral(plan, "deferred-out");
	context.SetResolvedDecl(plan, setter);
	context.SetExprReceiver(plan, opaqueReceiver);
	context.AddExprChild(plan, opaqueReceiver);
	return plan;
}

END_AS_NAMESPACE
