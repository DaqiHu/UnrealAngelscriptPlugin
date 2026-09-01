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

int asCSema::ActOnFinishFullExpression(
	asASTExprId expression,
	asASTStmtId semanticRegion)
{
	const asCStmt* const statement = context.GetStmt(semanticRegion);
	const asCExpr* const cleanup = context.GetExpr(expression);
	if( statement == 0 || statement->kind != asAST_STMT_EXPR
		|| statement->expr != expression || cleanup == 0 )
	{
		AddDiagnostic("full-expression-owner-invalid");
		return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
	}

	// CTA-S182 deliberately closes the first exact constructor-temporary shape.
	// Call-result and conditionally evaluated temporaries need their own Sema
	// cleanup-selection/evaluation-order protocol and are not inferred here.
	if( cleanup->kind != asAST_EXPR_CLEANUP
		|| !cleanup->literal.Equals("cleanup")
		|| cleanup->children.GetLength() != 1 )
	{
		return asAST_VERIFY_OK;
	}
	const asCExpr* const materialize = context.GetExpr(cleanup->children[0]);
	const asCExpr* const construct = materialize != 0
		&& materialize->kind == asAST_EXPR_MATERIALIZE_TEMPORARY
		&& materialize->children.GetLength() == 1
		? context.GetExpr(materialize->children[0]) : 0;
	const asCDecl* const destructor = context.GetDecl(cleanup->resolvedDecl);
	const asCDecl* const destructorOwner = destructor != 0
		? context.GetDecl(destructor->parent) : 0;
	const asCDecl* const constructor = construct != 0
		? context.GetDecl(construct->resolvedDecl) : 0;
	const asCDecl* const constructorOwner = constructor != 0
		? context.GetDecl(constructor->parent) : 0;
	const asCType* const valueType = materialize != 0
		? context.GetType(materialize->type.type) : 0;
	if( construct == 0 || construct->kind != asAST_EXPR_CONSTRUCT )
	{
		return asAST_VERIFY_OK;
	}
	// Class construction materializes a reference-object handle.  Its release
	// semantics are a separate lifetime family; this first full-expression
	// slice authors DESTROY_VALUE only for exact value-object temporaries.
	const bool exactReferenceObjectTemporary = valueType != 0
		&& valueType->kind == asAST_TYPE_REFERENCE_OBJECT
		&& materialize->type.IsHandle() && materialize->type.IsAutoHandle()
		&& !materialize->type.IsReference()
		&& cleanup->type == materialize->type
		&& construct->type == materialize->type
		&& materialize->valueCategory == asAST_VALUE_XVALUE
		&& constructor != 0 && constructor->kind == asAST_DECL_CONSTRUCTOR
		&& constructorOwner != 0
		&& constructorOwner->kind == asAST_DECL_CLASS
		&& valueType->stableKey.Equals(constructorOwner->stableKey);
	if( exactReferenceObjectTemporary )
	{
		return asAST_VERIFY_OK;
	}
	if( destructor == 0 || destructor->kind != asAST_DECL_DESTRUCTOR
		|| destructorOwner == 0 || destructorOwner->kind != asAST_DECL_CLASS
		|| constructor == 0 || constructor->kind != asAST_DECL_CONSTRUCTOR
		|| constructorOwner == 0 || constructorOwner->kind != asAST_DECL_CLASS
		|| valueType == 0 || valueType->kind != asAST_TYPE_VALUE_OBJECT
		|| cleanup->type != materialize->type
		|| construct->type != materialize->type
		|| materialize->valueCategory != asAST_VALUE_XVALUE
		|| materialize->type.IsReference() || materialize->type.IsHandle()
		|| !valueType->stableKey.Equals(destructorOwner->stableKey)
		|| !valueType->stableKey.Equals(constructorOwner->stableKey) )
	{
		AddDiagnostic("temporary-full-expression-destructor-invalid");
		return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
	}

	const asSASTLifetimeNodeId subject =
		asSASTLifetimeNodeId::FromExpr(materialize->id);
	for( asUINT index = 0; index < context.GetLifetimeRecordCount(); ++index )
	{
		const asSASTLifetimeRecord* const existing =
			context.GetLifetimeRecord(index);
		if( existing == 0 || existing->subjectKind
			!= asAST_LIFETIME_SUBJECT_TEMPORARY
			|| existing->subject != subject )
		{
			continue;
		}
		const bool exact = existing->actionKind
				== asAST_LIFETIME_ACTION_DESTROY_VALUE
			&& existing->actionTarget
				== asSASTLifetimeNodeId::FromDecl(destructor->id)
			&& existing->activationPoint == subject
			&& existing->semanticRegion
				== asSASTLifetimeNodeId::FromStmt(semanticRegion)
			&& existing->phase == asAST_LIFETIME_PHASE_FULL_EXPRESSION
			&& existing->supportedExitMask
				== (asAST_LIFETIME_EXIT_NORMAL | asAST_LIFETIME_EXIT_EXCEPTION)
			&& existing->constructionStep == 0
			&& !existing->completeObjectCommit;
		if( exact ) return asAST_VERIFY_OK;
		AddDiagnostic("temporary-full-expression-record-conflict");
		return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
	}

	asSASTLifetimeRecord record;
	record.subjectKind = asAST_LIFETIME_SUBJECT_TEMPORARY;
	record.subject = subject;
	record.actionKind = asAST_LIFETIME_ACTION_DESTROY_VALUE;
	record.actionTarget = asSASTLifetimeNodeId::FromDecl(destructor->id);
	record.activationPoint = subject;
	record.semanticRegion = asSASTLifetimeNodeId::FromStmt(semanticRegion);
	record.phase = asAST_LIFETIME_PHASE_FULL_EXPRESSION;
	record.supportedExitMask = asAST_LIFETIME_EXIT_NORMAL
		| asAST_LIFETIME_EXIT_EXCEPTION;
	record.constructionStep = 0;
	record.order = context.GetLifetimeRecordCount();
	record.completeObjectCommit = false;
	const int status = asSemaAuthorLifetimeRecord(context, record);
	if( status != asAST_VERIFY_OK )
	{
		AddDiagnostic("temporary-full-expression-record-invalid");
	}
	return status;
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
