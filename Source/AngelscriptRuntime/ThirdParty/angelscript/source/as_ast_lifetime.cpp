#include "as_ast_lifetime.h"
#include "as_ast_context.h"

BEGIN_AS_NAMESPACE

namespace
{
static const asQWORD asAST_LIFETIME_HASH_OFFSET =
	static_cast<asQWORD>(1469598103934665603ull);
static const asQWORD asAST_LIFETIME_HASH_PRIME =
	static_cast<asQWORD>(1099511628211ull);

asQWORD HashField(asQWORD hash, asQWORD value)
{
	hash ^= value;
	hash *= asAST_LIFETIME_HASH_PRIME;
	return hash;
}

bool LifetimeNodeExists(
	const asCASTContext& context,
	const asSASTLifetimeNodeId& node)
{
	if( !node.IsValid() || node.snapshotOwner != 0 )
	{
		return false;
	}
	switch( node.nodeClass )
	{
	case asAST_LIFETIME_NODE_DECL:
		return context.GetDecl(asASTDeclId(node.value)) != 0;
	case asAST_LIFETIME_NODE_STMT:
		return context.GetStmt(asASTStmtId(node.value)) != 0;
	case asAST_LIFETIME_NODE_EXPR:
		return context.GetExpr(asASTExprId(node.value)) != 0;
	default:
		return false;
	}
}
}

asQWORD asASTHashLifetimeNodeId(const asSASTLifetimeNodeId& id)
{
	asQWORD hash = asAST_LIFETIME_HASH_OFFSET;
	hash = HashField(hash, static_cast<asQWORD>(id.nodeClass));
	hash = HashField(hash, static_cast<asQWORD>(id.value));
	return HashField(hash, static_cast<asQWORD>(id.snapshotOwner));
}

asQWORD asASTHashLifetimeRecord(const asSASTLifetimeRecord& record)
{
	asQWORD hash = asAST_LIFETIME_HASH_OFFSET;
	hash = HashField(hash, static_cast<asQWORD>(record.subjectKind));
	hash = HashField(hash, asASTHashLifetimeNodeId(record.subject));
	hash = HashField(hash, static_cast<asQWORD>(record.actionKind));
	hash = HashField(hash, asASTHashLifetimeNodeId(record.actionTarget));
	hash = HashField(hash, asASTHashLifetimeNodeId(record.activationPoint));
	hash = HashField(hash, asASTHashLifetimeNodeId(record.semanticRegion));
	hash = HashField(hash, static_cast<asQWORD>(record.phase));
	hash = HashField(hash, static_cast<asQWORD>(record.supportedExitMask));
	hash = HashField(hash, static_cast<asQWORD>(record.constructionStep));
	hash = HashField(hash, static_cast<asQWORD>(record.order));
	return HashField(hash, record.completeObjectCommit ? 1u : 0u);
}

asQWORD asASTHashLifetimeProtocol(
	asDWORD revision,
	const asSASTLifetimeRecord* records,
	asUINT recordCount)
{
	asQWORD hash = asAST_LIFETIME_HASH_OFFSET;
	hash = HashField(hash, static_cast<asQWORD>(revision));
	hash = HashField(hash, static_cast<asQWORD>(recordCount));
	for( asUINT i = 0; i < recordCount; ++i )
	{
		hash = HashField(hash, asASTHashLifetimeRecord(records[i]));
	}
	return hash;
}

int asCASTVerifyLifetimeProtocol(const asCASTContext& context)
{
	if( context.GetLifetimeProtocolRevision()
		!= asAST_LIFETIME_PROTOCOL_REVISION )
	{
		return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
	}

	for( asUINT index = 0;
		index < context.GetLifetimeRecordCount(); ++index )
	{
		const asSASTLifetimeRecord* const record =
			context.GetLifetimeRecord(index);
		if( record == 0
			|| record->subjectKind <= asAST_LIFETIME_SUBJECT_INVALID
			|| record->subjectKind > asAST_LIFETIME_SUBJECT_DEFERRED_OUT
			|| record->actionKind <= asAST_LIFETIME_ACTION_INVALID
			|| record->actionKind > asAST_LIFETIME_ACTION_DESTROY_ELEMENT
			|| record->phase <= asAST_LIFETIME_PHASE_INVALID
			|| record->phase > asAST_LIFETIME_PHASE_AGGREGATE_ELEMENT
			|| record->supportedExitMask == asAST_LIFETIME_EXIT_NONE
			|| (record->supportedExitMask & ~asAST_LIFETIME_EXIT_ALL) != 0
			|| record->order != index
			|| !LifetimeNodeExists(context, record->subject)
			|| !LifetimeNodeExists(context, record->actionTarget)
			|| !LifetimeNodeExists(context, record->activationPoint)
			|| !LifetimeNodeExists(context, record->semanticRegion) )
		{
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}

		if( record->subjectKind == asAST_LIFETIME_SUBJECT_LOCAL )
		{
			const asCDecl* const subject = record->subject.nodeClass
				== asAST_LIFETIME_NODE_DECL
				? context.GetDecl(asASTDeclId(record->subject.value)) : 0;
			if( subject == 0 || subject->kind != asAST_DECL_VAR )
			{
				return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
			}
		}
		if( record->subjectKind == asAST_LIFETIME_SUBJECT_TEMPORARY
			&& record->subject.nodeClass != asAST_LIFETIME_NODE_EXPR )
		{
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}
		if( record->actionKind == asAST_LIFETIME_ACTION_DESTROY_VALUE )
		{
			const asCDecl* const action = record->actionTarget.nodeClass
				== asAST_LIFETIME_NODE_DECL
				? context.GetDecl(asASTDeclId(record->actionTarget.value)) : 0;
			if( action == 0 || action->kind != asAST_DECL_DESTRUCTOR )
			{
				return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
			}
		}
		if( record->activationPoint.nodeClass
			!= asAST_LIFETIME_NODE_EXPR
			&& record->activationPoint.nodeClass
				!= asAST_LIFETIME_NODE_STMT )
		{
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}
		if( record->semanticRegion.nodeClass
			!= asAST_LIFETIME_NODE_DECL
			&& record->semanticRegion.nodeClass
				!= asAST_LIFETIME_NODE_STMT )
		{
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}
		if( record->completeObjectCommit
			&& record->phase != asAST_LIFETIME_PHASE_COMPLETE_OBJECT )
		{
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}
	}
	return asAST_VERIFY_OK;
}

END_AS_NAMESPACE
