#include "as_ast_lifetime.h"
#include "as_ast_context.h"

BEGIN_AS_NAMESPACE

namespace
{
static const asQWORD asAST_LIFETIME_HASH_OFFSET =
	static_cast<asQWORD>(1469598103934665603ull);
static const asQWORD asAST_LIFETIME_HASH_PRIME =
	static_cast<asQWORD>(1099511628211ull);
static const asQWORD asAST_LIFETIME_VIEW_DOMAIN =
	static_cast<asQWORD>(0x4c545631ull); // "LTV1"

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
		return false;
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

bool NodeIdLess(
	const asSASTLifetimeNodeId& left,
	const asSASTLifetimeNodeId& right)
{
	if( left.nodeClass != right.nodeClass )
		return left.nodeClass < right.nodeClass;
	if( left.value != right.value ) return left.value < right.value;
	return left.snapshotOwner < right.snapshotOwner;
}

bool IsConstructionPhase(asEASTLifetimePhase phase)
{
	return phase == asAST_LIFETIME_PHASE_CONSTRUCTOR_BASE
		|| phase == asAST_LIFETIME_PHASE_CONSTRUCTOR_MEMBER
		|| phase == asAST_LIFETIME_PHASE_CONSTRUCTOR_DELEGATING
		|| phase == asAST_LIFETIME_PHASE_COMPLETE_OBJECT;
}

bool IsConstructionRecord(const asSASTLifetimeRecord& record)
{
	return record.subjectKind
		== asAST_LIFETIME_SUBJECT_CONSTRUCTION_SUBOBJECT;
}

bool IsAggregateRecord(const asSASTLifetimeRecord& record)
{
	return record.subjectKind
		== asAST_LIFETIME_SUBJECT_AGGREGATE_ELEMENT;
}

bool IsFullExpressionRecord(const asSASTLifetimeRecord& record)
{
	return record.subjectKind == asAST_LIFETIME_SUBJECT_TEMPORARY
		&& record.phase == asAST_LIFETIME_PHASE_FULL_EXPRESSION;
}

int VerifyBasicLifetimeRecord(
	const asCASTContext& context,
	const asSASTLifetimeRecord& record,
	asUINT index)
{
	if( record.subjectKind <= asAST_LIFETIME_SUBJECT_INVALID
		|| record.subjectKind > asAST_LIFETIME_SUBJECT_DEFERRED_OUT
		|| record.actionKind <= asAST_LIFETIME_ACTION_INVALID
		|| record.actionKind > asAST_LIFETIME_ACTION_DESTROY_ELEMENT
		|| record.phase <= asAST_LIFETIME_PHASE_INVALID
		|| record.phase > asAST_LIFETIME_PHASE_AGGREGATE_ELEMENT
		|| record.supportedExitMask == asAST_LIFETIME_EXIT_NONE
		|| (record.supportedExitMask & ~asAST_LIFETIME_EXIT_ALL) != 0
		|| record.order != index
		|| !LifetimeNodeExists(context, record.subject)
		|| !LifetimeNodeExists(context, record.actionTarget)
		|| !LifetimeNodeExists(context, record.activationPoint)
		|| !LifetimeNodeExists(context, record.semanticRegion) )
	{
		return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
	}

	if( record.subjectKind == asAST_LIFETIME_SUBJECT_LOCAL )
	{
		const asCDecl* const subject = record.subject.nodeClass
			== asAST_LIFETIME_NODE_DECL
			? context.GetDecl(asASTDeclId(record.subject.value)) : 0;
		if( subject == 0 || subject->kind != asAST_DECL_VAR )
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
	}
	if( record.subjectKind == asAST_LIFETIME_SUBJECT_TEMPORARY
		&& record.subject.nodeClass != asAST_LIFETIME_NODE_EXPR )
	{
		return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
	}
	if( record.actionKind == asAST_LIFETIME_ACTION_DESTROY_VALUE
		|| record.actionKind == asAST_LIFETIME_ACTION_DESTROY_ELEMENT )
	{
		const asCDecl* const action = record.actionTarget.nodeClass
			== asAST_LIFETIME_NODE_DECL
			? context.GetDecl(asASTDeclId(record.actionTarget.value)) : 0;
		if( action == 0 || action->kind != asAST_DECL_DESTRUCTOR )
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
	}
	if( record.activationPoint.nodeClass != asAST_LIFETIME_NODE_EXPR
		&& record.activationPoint.nodeClass != asAST_LIFETIME_NODE_STMT )
	{
		return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
	}
	if( record.semanticRegion.nodeClass != asAST_LIFETIME_NODE_DECL
		&& record.semanticRegion.nodeClass != asAST_LIFETIME_NODE_STMT )
	{
		return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
	}
	if( record.completeObjectCommit
		&& record.phase != asAST_LIFETIME_PHASE_COMPLETE_OBJECT
		&& record.phase != asAST_LIFETIME_PHASE_CONSTRUCTOR_DELEGATING )
	{
		return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
	}
	return asAST_VERIFY_OK;
}

bool ContainsRegion(
	const asCArray<asSASTLifetimeNodeId>& regions,
	const asSASTLifetimeNodeId& region)
{
	for( asUINT i = 0; i < regions.GetLength(); ++i )
		if( regions[i] == region ) return true;
	return false;
}

void SortRegions(asCArray<asSASTLifetimeNodeId>& regions)
{
	for( asUINT i = 1; i < regions.GetLength(); ++i )
	{
		asSASTLifetimeNodeId value = regions[i];
		asUINT insertion = i;
		while( insertion > 0 && NodeIdLess(value, regions[insertion - 1]) )
		{
			regions[insertion] = regions[insertion - 1];
			--insertion;
		}
		regions[insertion] = value;
	}
}

int BuildStmtParents(
	const asCASTContext& context,
	asCArray<asASTStmtId>& parents)
{
	parents.SetLength(context.GetStmtCount() + 1);
	for( asUINT statementIndex = 1;
		statementIndex <= context.GetStmtCount(); ++statementIndex )
	{
		const asCStmt* const statement =
			context.GetStmt(asASTStmtId(statementIndex));
		if( statement == 0 ) return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		for( asUINT childIndex = 0;
			childIndex < statement->children.GetLength(); ++childIndex )
		{
			const asASTStmtId child = statement->children[childIndex];
			if( !child.IsValid() || child.snapshotOwner != 0
				|| child.value >= parents.GetLength() )
			{
				return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
			}
			if( parents[child.value].IsValid()
				&& parents[child.value] != statement->id )
			{
				return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
			}
			parents[child.value] = statement->id;
		}
	}
	return asAST_VERIFY_OK;
}

asUINT FindRegionIndex(
	const asCASTLifetimeView& view,
	const asSASTLifetimeNodeId& region)
{
	for( asUINT i = 0; i < view.GetScopeEdgeCount(); ++i )
	{
		const asSASTLifetimeScopeEdge* const edge = view.GetScopeEdge(i);
		if( edge != 0 && edge->region == region ) return i;
	}
	return static_cast<asUINT>(-1);
}

asSASTLifetimeNodeId FindNearestLifetimeRegionParent(
	const asCArray<asSASTLifetimeNodeId>& regions,
	const asCArray<asASTStmtId>& stmtParents,
	const asSASTLifetimeNodeId& region)
{
	if( region.nodeClass != asAST_LIFETIME_NODE_STMT
		|| region.value >= stmtParents.GetLength() )
	{
		return asSASTLifetimeNodeId();
	}
	asASTStmtId parent = stmtParents[region.value];
	for( asUINT depth = 0;
		parent.IsValid() && depth < stmtParents.GetLength(); ++depth )
	{
		const asSASTLifetimeNodeId candidate =
			asSASTLifetimeNodeId::FromStmt(parent);
		if( ContainsRegion(regions, candidate) ) return candidate;
		if( parent.value >= stmtParents.GetLength() ) break;
		parent = stmtParents[parent.value];
	}
	return asSASTLifetimeNodeId();
}

asUINT CountLifetimeRegionDepth(
	const asCArray<asSASTLifetimeNodeId>& regions,
	const asCArray<asASTStmtId>& stmtParents,
	const asSASTLifetimeNodeId& region)
{
	asUINT result = 0;
	asSASTLifetimeNodeId current = region;
	for( asUINT guard = 0; guard < regions.GetLength(); ++guard )
	{
		current = FindNearestLifetimeRegionParent(regions, stmtParents, current);
		if( !current.IsValid() ) break;
		++result;
	}
	return result;
}

bool FindBlockCommitOrdinals(
	const asCASTContext& context,
	const asSASTLifetimeRecord& record,
	asUINT& outDeclarationOrdinal,
	asUINT& outActivationOrdinal)
{
	outDeclarationOrdinal = static_cast<asUINT>(-1);
	outActivationOrdinal = static_cast<asUINT>(-1);
	const asCStmt* const region = record.semanticRegion.nodeClass
		== asAST_LIFETIME_NODE_STMT
		? context.GetStmt(asASTStmtId(record.semanticRegion.value)) : 0;
	if( region == 0 || region->kind != asAST_STMT_BLOCK ) return false;

	for( asUINT childIndex = 0;
		childIndex < region->children.GetLength(); ++childIndex )
	{
		const asCStmt* const child = context.GetStmt(region->children[childIndex]);
		if( child == 0 ) return false;
		if( child->kind == asAST_STMT_DECL
			&& child->decl.value == record.subject.value
			&& child->decl.snapshotOwner == 0 )
		{
			outDeclarationOrdinal = childIndex;
		}
		if( child->kind == asAST_STMT_EXPR
			&& record.activationPoint.nodeClass == asAST_LIFETIME_NODE_EXPR
			&& child->expr.value == record.activationPoint.value
			&& child->expr.snapshotOwner == 0 )
		{
			outActivationOrdinal = childIndex;
		}
	}
	if( outDeclarationOrdinal == static_cast<asUINT>(-1)
		|| outActivationOrdinal == static_cast<asUINT>(-1)
		|| outDeclarationOrdinal >= outActivationOrdinal )
	{
		return false;
	}
	for( asUINT childIndex = outDeclarationOrdinal + 1;
		childIndex < outActivationOrdinal; ++childIndex )
	{
		const asCStmt* const child = context.GetStmt(region->children[childIndex]);
		if( child != 0 && child->kind == asAST_STMT_DECL ) return false;
	}
	return true;
}

bool DestructorMatchesLocalValue(
	const asCASTContext& context,
	const asCDecl& local,
	const asCDecl& destructor)
{
	const asCType* const localType = local.type.IsValid()
		? context.GetType(local.type.type) : 0;
	const asCDecl* const destructorOwner = context.GetDecl(destructor.parent);
	return localType != 0
		&& localType->kind == asAST_TYPE_VALUE_OBJECT
		&& !local.type.IsReference()
		&& !local.type.IsHandle()
		&& destructorOwner != 0
		&& destructorOwner->kind == asAST_DECL_CLASS
		&& localType->stableKey.Equals(destructorOwner->stableKey);
}

bool ReleaseMatchesLocal(
	const asCASTContext& context,
	const asCDecl& local)
{
	const asCType* const localType = local.type.IsValid()
		? context.GetType(local.type.type) : 0;
	return localType != 0
		&& !local.type.IsReference()
		&& (localType->kind == asAST_TYPE_REFERENCE_OBJECT
			|| localType->kind == asAST_TYPE_FUNCDEF);
}

bool ConstructorOwnsInit(
	const asCDecl& constructor,
	asASTExprId activation)
{
	for( asUINT index = 0; index < constructor.inits.GetLength(); ++index )
		if( constructor.inits[index] == activation ) return true;
	return false;
}

bool ClassHasDirectBase(
	const asCDecl& owner,
	asASTDeclId base)
{
	for( asUINT index = 0; index < owner.bases.GetLength(); ++index )
		if( owner.bases[index] == base ) return true;
	return false;
}

bool ValidateConstructionLifetimeRecord(
	const asCASTContext& context,
	const asSASTLifetimeRecord& record,
	asUINT& outActivationOrdinal)
{
	if( !IsConstructionRecord(record)
		|| !IsConstructionPhase(record.phase)
		|| record.subject.nodeClass != asAST_LIFETIME_NODE_DECL
		|| record.actionTarget.nodeClass != asAST_LIFETIME_NODE_DECL
		|| record.semanticRegion.nodeClass != asAST_LIFETIME_NODE_DECL )
	{
		return false;
	}

	const asCDecl* const constructor = context.GetDecl(
		asASTDeclId(record.semanticRegion.value));
	const asCDecl* const owner = constructor != 0
		? context.GetDecl(constructor->parent) : 0;
	const asCDecl* const subject = context.GetDecl(
		asASTDeclId(record.subject.value));
	const asCDecl* const action = context.GetDecl(
		asASTDeclId(record.actionTarget.value));
	if( constructor == 0 || constructor->kind != asAST_DECL_CONSTRUCTOR
		|| owner == 0 || owner->kind != asAST_DECL_CLASS
		|| subject == 0 || action == 0 )
	{
		return false;
	}
	outActivationOrdinal = record.constructionStep;

	if( record.phase == asAST_LIFETIME_PHASE_CONSTRUCTOR_BASE )
	{
		const asCExpr* const call = record.activationPoint.nodeClass
			== asAST_LIFETIME_NODE_EXPR
			? context.GetExpr(asASTExprId(record.activationPoint.value)) : 0;
		const asCDecl* const targetConstructor = call != 0
			? context.GetDecl(call->resolvedDecl) : 0;
		return !record.completeObjectCommit
			&& record.supportedExitMask == asAST_LIFETIME_EXIT_EXCEPTION
			&& record.actionKind == asAST_LIFETIME_ACTION_DESTROY_VALUE
			&& subject->kind == asAST_DECL_CLASS
			&& ClassHasDirectBase(*owner, subject->id)
			&& action->kind == asAST_DECL_DESTRUCTOR
			&& action->parent == subject->id
			&& call != 0 && call->kind == asAST_EXPR_CALL
			&& call->receiver.IsValid()
			&& targetConstructor != 0
			&& targetConstructor->kind == asAST_DECL_CONSTRUCTOR
			&& targetConstructor->parent == subject->id
			&& ConstructorOwnsInit(*constructor, call->id);
	}

	if( record.phase == asAST_LIFETIME_PHASE_CONSTRUCTOR_MEMBER )
	{
		const asCExpr* const assignment = record.activationPoint.nodeClass
			== asAST_LIFETIME_NODE_EXPR
			? context.GetExpr(asASTExprId(record.activationPoint.value)) : 0;
		const asCExpr* const target = assignment != 0
			&& assignment->kind == asAST_EXPR_ASSIGN
			&& assignment->children.GetLength() == 2
			? context.GetExpr(assignment->children[0]) : 0;
		if( record.completeObjectCommit
			|| record.supportedExitMask != asAST_LIFETIME_EXIT_EXCEPTION
			|| subject->kind != asAST_DECL_VAR
			|| subject->parent != owner->id
			|| assignment == 0
			|| target == 0 || target->kind != asAST_EXPR_DECL_REF
			|| target->resolvedDecl != subject->id
			|| !ConstructorOwnsInit(*constructor, assignment->id) )
		{
			return false;
		}
		if( record.actionKind == asAST_LIFETIME_ACTION_DESTROY_VALUE )
		{
			return action->kind == asAST_DECL_DESTRUCTOR
				&& DestructorMatchesLocalValue(context, *subject, *action);
		}
		if( record.actionKind == asAST_LIFETIME_ACTION_RELEASE_REFERENCE )
		{
			return record.actionTarget == record.subject
				&& ReleaseMatchesLocal(context, *subject);
		}
		return false;
	}

	if( record.actionKind != asAST_LIFETIME_ACTION_DESTROY_VALUE
		|| subject->id != owner->id
		|| action->kind != asAST_DECL_DESTRUCTOR
		|| action->parent != owner->id
		|| !record.completeObjectCommit )
	{
		return false;
	}

	if( record.phase == asAST_LIFETIME_PHASE_COMPLETE_OBJECT )
	{
		const asCStmt* const body = record.activationPoint.nodeClass
			== asAST_LIFETIME_NODE_STMT
			? context.GetStmt(asASTStmtId(record.activationPoint.value)) : 0;
		return record.supportedExitMask == asAST_LIFETIME_EXIT_NORMAL
			&& body != 0 && body->kind == asAST_STMT_BLOCK
			&& body->owner == constructor->id
			&& constructor->body == body->id;
	}

	const asCExpr* const call = record.activationPoint.nodeClass
		== asAST_LIFETIME_NODE_EXPR
		? context.GetExpr(asASTExprId(record.activationPoint.value)) : 0;
	const asCDecl* const targetConstructor = call != 0
		? context.GetDecl(call->resolvedDecl) : 0;
	return record.phase == asAST_LIFETIME_PHASE_CONSTRUCTOR_DELEGATING
		&& record.supportedExitMask == asAST_LIFETIME_EXIT_EXCEPTION
		&& call != 0 && call->kind == asAST_EXPR_CALL
		&& call->receiver.IsValid()
		&& targetConstructor != 0
		&& targetConstructor->kind == asAST_DECL_CONSTRUCTOR
		&& targetConstructor->parent == owner->id
		&& targetConstructor->id != constructor->id
		&& ConstructorOwnsInit(*constructor, call->id);
}

bool ExprSubtreeContains(
	const asCASTContext& context,
	asASTExprId root,
	asASTExprId needle)
{
	if( !root.IsValid() || !needle.IsValid() ) return false;
	asCArray<asASTExprId> stack;
	asCArray<asBYTE> visited;
	visited.SetLength(context.GetExprCount() + 1);
	stack.PushLast(root);
	while( stack.GetLength() != 0 )
	{
		const asASTExprId currentId = stack.PopLast();
		if( currentId == needle ) return true;
		if( !currentId.IsValid() || currentId.snapshotOwner != 0
			|| currentId.value >= visited.GetLength()
			|| visited[currentId.value] )
		{
			continue;
		}
		visited[currentId.value] = 1;
		const asCExpr* const current = context.GetExpr(currentId);
		if( current == 0 ) return false;
		if( current->receiver.IsValid() ) stack.PushLast(current->receiver);
		for( asUINT childIndex = 0;
			childIndex < current->children.GetLength(); ++childIndex )
		{
			stack.PushLast(current->children[childIndex]);
		}
	}
	return false;
}

bool StmtSubtreeContainsExpr(
	const asCASTContext& context,
	asASTStmtId root,
	asASTExprId needle)
{
	if( !root.IsValid() || !needle.IsValid() ) return false;
	asCArray<asASTStmtId> stack;
	asCArray<asBYTE> visited;
	visited.SetLength(context.GetStmtCount() + 1);
	stack.PushLast(root);
	while( stack.GetLength() != 0 )
	{
		const asASTStmtId currentId = stack.PopLast();
		if( !currentId.IsValid() || currentId.snapshotOwner != 0
			|| currentId.value >= visited.GetLength()
			|| visited[currentId.value] )
		{
			continue;
		}
		visited[currentId.value] = 1;
		const asCStmt* const current = context.GetStmt(currentId);
		if( current == 0 ) return false;
		if( current->expr.IsValid()
			&& ExprSubtreeContains(context, current->expr, needle) )
		{
			return true;
		}
		if( current->decl.IsValid() )
		{
			const asCDecl* const declaration = context.GetDecl(current->decl);
			for( asUINT initIndex = 0;
				declaration != 0
					&& initIndex < declaration->inits.GetLength(); ++initIndex )
			{
				if( ExprSubtreeContains(
					context, declaration->inits[initIndex], needle) )
				{
					return true;
				}
			}
		}
		for( asUINT childIndex = 0;
			childIndex < current->children.GetLength(); ++childIndex )
		{
			stack.PushLast(current->children[childIndex]);
		}
	}
	return false;
}

bool DeclOwnsExpr(
	const asCASTContext& context,
	const asCDecl& owner,
	asASTExprId expression)
{
	for( asUINT initIndex = 0; initIndex < owner.inits.GetLength(); ++initIndex )
		if( ExprSubtreeContains(context, owner.inits[initIndex], expression) )
			return true;
	return owner.body.IsValid()
		&& StmtSubtreeContainsExpr(context, owner.body, expression);
}

bool ValueTypeMatchesClass(
	const asCASTContext& context,
	const asCQualType& type,
	const asCDecl& classDecl)
{
	const asCType* const canonicalType = type.IsValid()
		? context.GetType(type.type) : 0;
	return canonicalType != 0
		&& canonicalType->kind == asAST_TYPE_VALUE_OBJECT
		&& !type.IsReference()
		&& !type.IsHandle()
		&& classDecl.kind == asAST_DECL_CLASS
		&& canonicalType->stableKey.Equals(classDecl.stableKey);
}

bool AggregateTypeMatchesFactoryOwner(
	const asCASTContext& context,
	const asCQualType& type,
	const asCDecl& classDecl)
{
	const asCType* const canonicalType = type.IsValid()
		? context.GetType(type.type) : 0;
	return canonicalType != 0
		&& (canonicalType->kind == asAST_TYPE_VALUE_OBJECT
			|| canonicalType->kind == asAST_TYPE_REFERENCE_OBJECT)
		&& classDecl.kind == asAST_DECL_CLASS
		&& canonicalType->stableKey.Equals(classDecl.stableKey);
}

bool DestructorMatchesExprValue(
	const asCASTContext& context,
	const asCExpr& expression,
	const asCDecl& destructor)
{
	const asCDecl* const owner = context.GetDecl(destructor.parent);
	return destructor.kind == asAST_DECL_DESTRUCTOR
		&& owner != 0
		&& ValueTypeMatchesClass(context, expression.type, *owner);
}

bool ConstructorMatchesMaterializedValue(
	const asCASTContext& context,
	const asCExpr& construct,
	const asCExpr& materialize)
{
	const asCDecl* const constructor = context.GetDecl(construct.resolvedDecl);
	const asCDecl* const owner = constructor != 0
		? context.GetDecl(constructor->parent) : 0;
	return construct.kind == asAST_EXPR_CONSTRUCT
		&& construct.type == materialize.type
		&& materialize.valueCategory == asAST_VALUE_XVALUE
		&& constructor != 0
		&& constructor->kind == asAST_DECL_CONSTRUCTOR
		&& owner != 0 && owner->kind == asAST_DECL_CLASS
		&& ValueTypeMatchesClass(context, construct.type, *owner);
}

bool IsExactReferenceObjectConstructorTemporary(
	const asCASTContext& context,
	const asCExpr& cleanup,
	const asCExpr& construct,
	const asCExpr& materialize)
{
	const asCType* const canonicalType = materialize.type.IsValid()
		? context.GetType(materialize.type.type) : 0;
	const asCDecl* const constructor = context.GetDecl(construct.resolvedDecl);
	const asCDecl* const owner = constructor != 0
		? context.GetDecl(constructor->parent) : 0;
	return canonicalType != 0
		&& canonicalType->kind == asAST_TYPE_REFERENCE_OBJECT
		&& materialize.type.IsHandle() && materialize.type.IsAutoHandle()
		&& !materialize.type.IsReference()
		&& cleanup.type == materialize.type
		&& construct.type == materialize.type
		&& materialize.valueCategory == asAST_VALUE_XVALUE
		&& constructor != 0
		&& constructor->kind == asAST_DECL_CONSTRUCTOR
		&& owner != 0 && owner->kind == asAST_DECL_CLASS
		&& canonicalType->stableKey.Equals(owner->stableKey);
}

enum asETemporaryShapeClassification
{
	asTEMPORARY_SHAPE_NOT_CANDIDATE = 0,
	asTEMPORARY_SHAPE_MALFORMED,
	asTEMPORARY_SHAPE_VALID
};

asETemporaryShapeClassification ClassifySupportedConstructorTemporary(
	const asCASTContext& context,
	const asCStmt& statement,
	asASTExprId& outMaterialize,
	asASTDeclId& outDestructor)
{
	outMaterialize = asASTExprId();
	outDestructor = asASTDeclId();
	if( statement.kind != asAST_STMT_EXPR )
		return asTEMPORARY_SHAPE_NOT_CANDIDATE;
	const asCExpr* const cleanup = context.GetExpr(statement.expr);
	if( cleanup == 0 || cleanup->kind != asAST_EXPR_CLEANUP
		|| !cleanup->literal.Equals("cleanup")
		|| cleanup->children.GetLength() != 1 )
	{
		return asTEMPORARY_SHAPE_NOT_CANDIDATE;
	}
	const asCExpr* const materialize = context.GetExpr(cleanup->children[0]);
	const asCExpr* const construct = materialize != 0
		&& materialize->kind == asAST_EXPR_MATERIALIZE_TEMPORARY
		&& materialize->children.GetLength() == 1
		? context.GetExpr(materialize->children[0]) : 0;
	if( construct == 0 || construct->kind != asAST_EXPR_CONSTRUCT )
	{
		return asTEMPORARY_SHAPE_NOT_CANDIDATE;
	}
	// Reference-object construction produces a handle whose ownership protocol
	// is not DESTROY_VALUE.  CTA-S182 deliberately closes only direct value-
	// object constructor temporaries, so an authenticated class temporary is a
	// different family rather than a malformed instance of this one.
	if( IsExactReferenceObjectConstructorTemporary(
		context, *cleanup, *construct, *materialize) )
	{
		return asTEMPORARY_SHAPE_NOT_CANDIDATE;
	}
	if( cleanup->type != materialize->type
		|| !ConstructorMatchesMaterializedValue(
		context, *construct, *materialize) )
	{
		return asTEMPORARY_SHAPE_MALFORMED;
	}
	const asCDecl* const destructor = context.GetDecl(cleanup->resolvedDecl);
	if( destructor == 0
		|| !DestructorMatchesExprValue(context, *materialize, *destructor) )
	{
		return asTEMPORARY_SHAPE_MALFORMED;
	}
	outMaterialize = materialize->id;
	outDestructor = destructor->id;
	return asTEMPORARY_SHAPE_VALID;
}

bool IsAggregateSemanticOwner(const asCDecl& declaration)
{
	return declaration.kind == asAST_DECL_FUNCTION
		|| declaration.kind == asAST_DECL_METHOD
		|| declaration.kind == asAST_DECL_CONSTRUCTOR
		|| declaration.kind == asAST_DECL_DESTRUCTOR;
}

bool ValidateAggregateLifetimeRecord(
	const asCASTContext& context,
	const asSASTLifetimeRecord& record,
	asUINT& outActivationOrdinal)
{
	if( !IsAggregateRecord(record)
		|| record.subject.nodeClass != asAST_LIFETIME_NODE_EXPR
		|| record.subject != record.activationPoint
		|| record.actionKind != asAST_LIFETIME_ACTION_DESTROY_ELEMENT
		|| record.actionTarget.nodeClass != asAST_LIFETIME_NODE_DECL
		|| record.semanticRegion.nodeClass != asAST_LIFETIME_NODE_DECL
		|| record.phase != asAST_LIFETIME_PHASE_AGGREGATE_ELEMENT
		|| record.supportedExitMask != asAST_LIFETIME_EXIT_EXCEPTION
		|| record.constructionStep != 0
		|| record.completeObjectCommit )
	{
		return false;
	}

	const asCExpr* const aggregate = context.GetExpr(
		asASTExprId(record.subject.value));
	const asCDecl* const listFactory = aggregate != 0
		? context.GetDecl(aggregate->resolvedDecl) : 0;
	const asCDecl* const aggregateClass = listFactory != 0
		? context.GetDecl(listFactory->parent) : 0;
	const asCDecl* const destructor = context.GetDecl(
		asASTDeclId(record.actionTarget.value));
	const asCDecl* const semanticOwner = context.GetDecl(
		asASTDeclId(record.semanticRegion.value));
	if( aggregate == 0 || aggregate->kind != asAST_EXPR_CONSTRUCT
		|| aggregate->children.GetLength() == 0
		|| listFactory == 0
		|| listFactory->kind != asAST_DECL_CONSTRUCTOR
		|| (listFactory->traits & asAST_TRAIT_LIST_FACTORY) == 0
		|| aggregateClass == 0
		|| !AggregateTypeMatchesFactoryOwner(
			context, aggregate->type, *aggregateClass)
		|| destructor == 0
		|| semanticOwner == 0
		|| !IsAggregateSemanticOwner(*semanticOwner)
		|| !DeclOwnsExpr(context, *semanticOwner, aggregate->id) )
	{
		return false;
	}

	const asCExpr* const firstElement = context.GetExpr(
		aggregate->children[0]);
	if( firstElement == 0
		|| !DestructorMatchesExprValue(context, *firstElement, *destructor) )
	{
		return false;
	}
	for( asUINT elementIndex = 1;
		elementIndex < aggregate->children.GetLength(); ++elementIndex )
	{
		const asCExpr* const element = context.GetExpr(
			aggregate->children[elementIndex]);
		if( element == 0 || element->type != firstElement->type
			|| !DestructorMatchesExprValue(context, *element, *destructor) )
		{
			return false;
		}
	}
	outActivationOrdinal = aggregate->id.value;
	return true;
}

bool FindForEachCommit(
	const asCASTContext& context,
	const asSASTLifetimeRecord& record,
	asUINT& outActivationOrdinal)
{
	const asCStmt* const loop = context.GetStmt(
		asASTStmtId(record.semanticRegion.value));
	if( loop == 0 || loop->kind != asAST_STMT_FOREACH
		|| loop->children.GetLength() != 4 )
	{
		return false;
	}
	const asCStmt* const initializer = context.GetStmt(loop->children[0]);
	if( initializer == 0 || initializer->kind != asAST_STMT_BLOCK )
		return false;
	bool sawDeclaration = false;
	bool sawActivation = false;
	for( asUINT childIndex = 0;
		childIndex < initializer->children.GetLength(); ++childIndex )
	{
		const asCStmt* const child = context.GetStmt(
			initializer->children[childIndex]);
		if( child == 0 ) return false;
		if( child->kind == asAST_STMT_DECL
			&& child->decl.value == record.subject.value )
		{
			sawDeclaration = true;
		}
		if( child->kind == asAST_STMT_EXPR
			&& record.activationPoint.nodeClass == asAST_LIFETIME_NODE_EXPR
			&& child->expr.value == record.activationPoint.value )
		{
			sawActivation = sawDeclaration;
		}
	}
	outActivationOrdinal = 0;
	return sawDeclaration && sawActivation;
}

bool ValidateLifetimeRecord(
	const asCASTContext& context,
	const asSASTLifetimeRecord& record,
	asUINT& outActivationOrdinal)
{
	if( IsConstructionRecord(record) )
	{
		return ValidateConstructionLifetimeRecord(
			context, record, outActivationOrdinal);
	}
	if( IsAggregateRecord(record) )
	{
		return ValidateAggregateLifetimeRecord(
			context, record, outActivationOrdinal);
	}
	if( IsFullExpressionRecord(record) )
	{
		if( record.subject.nodeClass != asAST_LIFETIME_NODE_EXPR
			|| record.subject != record.activationPoint
			|| record.actionKind != asAST_LIFETIME_ACTION_DESTROY_VALUE
			|| record.actionTarget.nodeClass != asAST_LIFETIME_NODE_DECL
			|| record.semanticRegion.nodeClass != asAST_LIFETIME_NODE_STMT
			|| record.supportedExitMask
				!= (asAST_LIFETIME_EXIT_NORMAL | asAST_LIFETIME_EXIT_EXCEPTION)
			|| record.constructionStep != 0 || record.completeObjectCommit )
		{
			return false;
		}
		const asCExpr* const materialize = context.GetExpr(
			asASTExprId(record.subject.value));
		const asCExpr* const construct = materialize != 0
			&& materialize->kind == asAST_EXPR_MATERIALIZE_TEMPORARY
			&& materialize->children.GetLength() == 1
			? context.GetExpr(materialize->children[0]) : 0;
		const asCStmt* const statement = context.GetStmt(
			asASTStmtId(record.semanticRegion.value));
		const asCExpr* const cleanup = statement != 0
			&& statement->kind == asAST_STMT_EXPR
			? context.GetExpr(statement->expr) : 0;
		const asCDecl* const destructor = context.GetDecl(
			asASTDeclId(record.actionTarget.value));
		if( construct == 0
			|| !ConstructorMatchesMaterializedValue(
				context, *construct, *materialize)
			|| cleanup == 0 || cleanup->kind != asAST_EXPR_CLEANUP
			|| cleanup->type != materialize->type
			|| !cleanup->literal.Equals("cleanup")
			|| cleanup->children.GetLength() != 1
			|| cleanup->children[0] != materialize->id
			|| cleanup->resolvedDecl != destructor->id
			|| !DestructorMatchesExprValue(context, *materialize, *destructor) )
		{
			return false;
		}
		outActivationOrdinal = 0;
		return true;
	}
	if( record.constructionStep != 0
		|| record.completeObjectCommit
		|| record.subject.nodeClass != asAST_LIFETIME_NODE_DECL
		|| record.activationPoint.nodeClass != asAST_LIFETIME_NODE_EXPR
		|| record.semanticRegion.nodeClass != asAST_LIFETIME_NODE_STMT )
	{
		return false;
	}
	const asCDecl* const local =
		context.GetDecl(asASTDeclId(record.subject.value));
	const asCExpr* const assignment =
		context.GetExpr(asASTExprId(record.activationPoint.value));
	const asCExpr* const target = assignment != 0
		&& assignment->kind == asAST_EXPR_ASSIGN
		&& assignment->children.GetLength() == 2
		? context.GetExpr(assignment->children[0]) : 0;
	if( local == 0 || local->kind != asAST_DECL_VAR
		|| target == 0 || target->kind != asAST_EXPR_DECL_REF
		|| target->resolvedDecl != local->id )
	{
		return false;
	}

	if( record.subjectKind == asAST_LIFETIME_SUBJECT_FOREACH_ITERATOR )
	{
		const asCDecl* const destructor = record.actionTarget.nodeClass
			== asAST_LIFETIME_NODE_DECL
			? context.GetDecl(asASTDeclId(record.actionTarget.value)) : 0;
		return record.actionKind == asAST_LIFETIME_ACTION_DESTROY_VALUE
			&& record.phase == asAST_LIFETIME_PHASE_LOOP_ITERATION
			&& destructor != 0
			&& destructor->kind == asAST_DECL_DESTRUCTOR
			&& DestructorMatchesLocalValue(context, *local, *destructor)
			&& FindForEachCommit(context, record, outActivationOrdinal);
	}

	if( record.subjectKind != asAST_LIFETIME_SUBJECT_LOCAL
		|| record.phase != asAST_LIFETIME_PHASE_LEXICAL_SCOPE
		|| record.constructionStep != 0
		|| record.completeObjectCommit
		|| record.actionTarget.nodeClass != asAST_LIFETIME_NODE_DECL )
	{
		return false;
	}
	if( record.actionKind == asAST_LIFETIME_ACTION_DESTROY_VALUE )
	{
		const asCDecl* const destructor =
			context.GetDecl(asASTDeclId(record.actionTarget.value));
		if( destructor == 0 || destructor->kind != asAST_DECL_DESTRUCTOR
			|| !DestructorMatchesLocalValue(context, *local, *destructor) )
		{
			return false;
		}
	}
	else if( record.actionKind == asAST_LIFETIME_ACTION_RELEASE_REFERENCE )
	{
		if( record.actionTarget != record.subject
			|| !ReleaseMatchesLocal(context, *local) )
		{
			return false;
		}
	}
	else
	{
		return false;
	}
	asUINT declarationOrdinal = 0;
	return FindBlockCommitOrdinals(
		context, record, declarationOrdinal, outActivationOrdinal);
}

bool RecordsDuplicateAction(
	const asSASTLifetimeRecord& left,
	const asSASTLifetimeRecord& right)
{
	return left.subjectKind == right.subjectKind
		&& left.subject == right.subject
		&& left.actionKind == right.actionKind
		&& left.actionTarget == right.actionTarget
		&& left.activationPoint == right.activationPoint
		&& left.semanticRegion == right.semanticRegion
		&& left.phase == right.phase
		&& left.constructionStep == right.constructionStep;
}

asUINT FindDirectDescendantOrdinal(
	const asCASTContext& context,
	const asCArray<asASTStmtId>& stmtParents,
	const asSASTLifetimeNodeId& ancestorRegion,
	const asSASTLifetimeNodeId& descendantRegion)
{
	if( ancestorRegion.nodeClass != asAST_LIFETIME_NODE_STMT
		|| descendantRegion.nodeClass != asAST_LIFETIME_NODE_STMT )
	{
		return static_cast<asUINT>(-1);
	}
	asASTStmtId child(descendantRegion.value);
	asASTStmtId parent = child.value < stmtParents.GetLength()
		? stmtParents[child.value] : asASTStmtId();
	for( asUINT guard = 0;
		parent.IsValid() && guard < stmtParents.GetLength(); ++guard )
	{
		if( parent.value == ancestorRegion.value )
		{
			const asCStmt* const ancestor =
				context.GetStmt(asASTStmtId(ancestorRegion.value));
			for( asUINT index = 0;
				ancestor != 0 && index < ancestor->children.GetLength(); ++index )
			{
				if( ancestor->children[index] == child ) return index;
			}
			return static_cast<asUINT>(-1);
		}
		child = parent;
		parent = child.value < stmtParents.GetLength()
			? stmtParents[child.value] : asASTStmtId();
	}
	return static_cast<asUINT>(-1);
}

bool StatementSubtreeContains(
	const asCASTContext& context,
	asASTStmtId root,
	asASTStmtId needle)
{
	if( !root.IsValid() || !needle.IsValid() ) return false;
	asCArray<asASTStmtId> stack;
	asCArray<asBYTE> visited;
	visited.SetLength(context.GetStmtCount() + 1);
	stack.PushLast(root);
	while( stack.GetLength() != 0 )
	{
		const asASTStmtId currentId = stack.PopLast();
		if( currentId == needle ) return true;
		if( !currentId.IsValid() || currentId.value >= visited.GetLength()
			|| visited[currentId.value] )
		{
			continue;
		}
		visited[currentId.value] = 1;
		const asCStmt* const current = context.GetStmt(currentId);
		for( asUINT childIndex = 0;
			current != 0 && childIndex < current->children.GetLength();
			++childIndex )
		{
			stack.PushLast(current->children[childIndex]);
		}
	}
	return false;
}

asDWORD TransferExitMask(const asCStmt* statement)
{
	if( statement == 0 ) return asAST_LIFETIME_EXIT_NONE;
	switch( statement->kind )
	{
	case asAST_STMT_RETURN: return asAST_LIFETIME_EXIT_RETURN;
	case asAST_STMT_BREAK: return asAST_LIFETIME_EXIT_BREAK;
	case asAST_STMT_CONTINUE: return asAST_LIFETIME_EXIT_CONTINUE;
	case asAST_STMT_FALLTHROUGH: return asAST_LIFETIME_EXIT_FALLTHROUGH;
	default: return asAST_LIFETIME_EXIT_NONE;
	}
}

bool DecodeCompatibilityCleanup(
	const asCASTContext& context,
	asASTStmtId statementId,
	asEASTLifetimeActionKind& outActionKind,
	asASTDeclId& outSubject,
	asASTDeclId& outActionTarget,
	asASTExprId& outExpression)
{
	outActionKind = asAST_LIFETIME_ACTION_INVALID;
	outSubject = asASTDeclId();
	outActionTarget = asASTDeclId();
	outExpression = asASTExprId();
	const asCStmt* const statement = context.GetStmt(statementId);
	const asCExpr* const cleanup = statement != 0
		&& statement->kind == asAST_STMT_EXPR
		? context.GetExpr(statement->expr) : 0;
	if( cleanup == 0 || cleanup->kind != asAST_EXPR_CLEANUP
		|| cleanup->children.GetLength() != 1 )
	{
		return false;
	}
	if( !cleanup->literal.Equals("scope-exit")
		&& !cleanup->literal.Equals("scope-release") )
	{
		return false;
	}
	const asCExpr* const target = context.GetExpr(cleanup->children[0]);
	if( target == 0 || target->kind != asAST_EXPR_DECL_REF
		|| !target->resolvedDecl.IsValid() )
	{
		return false;
	}
	outExpression = cleanup->id;
	outSubject = target->resolvedDecl;
	if( cleanup->literal.Equals("scope-release") )
	{
		if( cleanup->resolvedDecl.IsValid() ) return false;
		outActionKind = asAST_LIFETIME_ACTION_RELEASE_REFERENCE;
		outActionTarget = outSubject;
		return true;
	}
	const asCDecl* const destructor = context.GetDecl(cleanup->resolvedDecl);
	if( destructor == 0 || destructor->kind != asAST_DECL_DESTRUCTOR )
		return false;
	outActionKind = asAST_LIFETIME_ACTION_DESTROY_VALUE;
	outActionTarget = destructor->id;
	return true;
}

asUINT FindMatchingRecordIndex(
	const asCASTLifetimeView& view,
	asEASTLifetimeActionKind actionKind,
	asASTDeclId subject,
	asASTDeclId actionTarget)
{
	asUINT result = static_cast<asUINT>(-1);
	for( asUINT index = 0; index < view.GetRecordCount(); ++index )
	{
		const asSASTLifetimeViewRecord* const candidate = view.GetRecord(index);
		if( candidate == 0
			|| IsConstructionRecord(candidate->record)
			|| candidate->record.subject
				!= asSASTLifetimeNodeId::FromDecl(subject)
			|| candidate->record.actionKind != actionKind
			|| candidate->record.actionTarget
				!= asSASTLifetimeNodeId::FromDecl(actionTarget) )
		{
			continue;
		}
		if( result != static_cast<asUINT>(-1) )
			return static_cast<asUINT>(-1);
		result = index;
	}
	return result;
}

bool TransferExitsRecordRegion(
	const asCASTContext& context,
	const asSASTLifetimeViewRecord& record,
	const asCStmt& transfer)
{
	if( transfer.kind == asAST_STMT_RETURN ) return true;
	if( transfer.kind != asAST_STMT_BREAK
		&& transfer.kind != asAST_STMT_CONTINUE
		&& transfer.kind != asAST_STMT_FALLTHROUGH )
	{
		return false;
	}
	return !transfer.target.IsValid()
		|| !StatementSubtreeContains(
			context,
			asASTStmtId(record.record.semanticRegion.value),
			transfer.target);
}

bool ExitPlanRecordComesBefore(
	const asCASTLifetimeView& view,
	asUINT leftIndex,
	asUINT rightIndex)
{
	const asSASTLifetimeViewRecord* const left = view.GetRecord(leftIndex);
	const asSASTLifetimeViewRecord* const right = view.GetRecord(rightIndex);
	if( left == 0 || right == 0 ) return left != 0;
	const asSASTLifetimeScopeEdge* const leftScope =
		view.GetScopeEdge(left->scopeEdgeIndex);
	const asSASTLifetimeScopeEdge* const rightScope =
		view.GetScopeEdge(right->scopeEdgeIndex);
	if( leftScope == 0 || rightScope == 0 ) return leftScope != 0;
	if( leftScope->depth != rightScope->depth )
		return leftScope->depth > rightScope->depth;
	if( left->activationOrdinal != right->activationOrdinal )
		return left->activationOrdinal > right->activationOrdinal;
	return left->recordIndex > right->recordIndex;
}

void AppendExitPlanRecord(
	const asCASTLifetimeView& view,
	asUINT recordIndex,
	asCArray<asUINT>& records)
{
	records.PushLast(recordIndex);
	for( asUINT index = records.GetLength() - 1;
		index > 0
			&& ExitPlanRecordComesBefore(
				view, records[index], records[index - 1]); --index )
	{
		const asUINT previous = records[index - 1];
		records[index - 1] = records[index];
		records[index] = previous;
	}
}

bool AppendLiveRecordsInRegion(
	const asCASTLifetimeView& view,
	const asSASTLifetimeNodeId& region,
	asUINT beforeOrdinal,
	asCArray<asUINT>& outRecords)
{
	asUINT previousOrdinal = 0;
	bool havePrevious = false;
	for( asUINT index = 0; index < view.GetRecordCount(); ++index )
	{
		const asSASTLifetimeViewRecord* const record = view.GetRecord(index);
		if( record == 0 || record->record.semanticRegion != region
			|| record->activationOrdinal >= beforeOrdinal )
		{
			continue;
		}
		if( havePrevious && record->activationOrdinal <= previousOrdinal )
			return false;
		outRecords.PushLast(record->recordIndex);
		previousOrdinal = record->activationOrdinal;
		havePrevious = true;
	}
	return true;
}

bool ValidateConstructionPlan(
	const asCASTContext& context,
	const asCASTLifetimeView& view,
	const asSASTLifetimeConstructionPlan& plan)
{
	const asCDecl* const constructor = context.GetDecl(plan.constructor);
	const asCDecl* const owner = context.GetDecl(plan.ownerClass);
	if( constructor == 0 || constructor->kind != asAST_DECL_CONSTRUCTOR
		|| owner == 0 || owner->kind != asAST_DECL_CLASS
		|| constructor->parent != owner->id
		|| plan.recordIndices.GetLength() == 0
		|| plan.completeRecordIndex == static_cast<asUINT>(-1)
		|| plan.completeRecordIndex
			!= plan.recordIndices[plan.recordIndices.GetLength() - 1] )
	{
		return false;
	}

	bool sawMember = false;
	for( asUINT step = 0; step < plan.recordIndices.GetLength(); ++step )
	{
		const asUINT recordIndex = plan.recordIndices[step];
		const asSASTLifetimeViewRecord* const derived =
			view.GetRecord(recordIndex);
		if( derived == 0
			|| !IsConstructionRecord(derived->record)
			|| derived->record.semanticRegion
				!= asSASTLifetimeNodeId::FromDecl(plan.constructor)
			|| derived->record.constructionStep != step )
		{
			return false;
		}

		const asSASTLifetimeRecord& record = derived->record;
		if( record.phase == asAST_LIFETIME_PHASE_CONSTRUCTOR_DELEGATING )
		{
			return step == 0 && plan.recordIndices.GetLength() == 1
				&& record.completeObjectCommit
				&& plan.completeRecordIndex == recordIndex;
		}
		if( step + 1 == plan.recordIndices.GetLength() )
		{
			return record.phase == asAST_LIFETIME_PHASE_COMPLETE_OBJECT
				&& record.completeObjectCommit
				&& plan.completeRecordIndex == recordIndex;
		}
		if( record.completeObjectCommit
			|| (record.phase != asAST_LIFETIME_PHASE_CONSTRUCTOR_BASE
				&& record.phase
					!= asAST_LIFETIME_PHASE_CONSTRUCTOR_MEMBER) )
		{
			return false;
		}
		if( record.phase == asAST_LIFETIME_PHASE_CONSTRUCTOR_MEMBER )
		{
			sawMember = true;
		}
		else if( sawMember )
		{
			return false;
		}
		for( asUINT earlier = 0; earlier < step; ++earlier )
		{
			const asSASTLifetimeViewRecord* const previous =
				view.GetRecord(plan.recordIndices[earlier]);
			if( previous != 0
				&& previous->record.subject == record.subject )
			{
				return false;
			}
		}
	}
	return false;
}

asQWORD HashIndexArray(asQWORD hash, const asCArray<asUINT>& values)
{
	hash = HashField(hash, values.GetLength());
	for( asUINT i = 0; i < values.GetLength(); ++i )
		hash = HashField(hash, values[i]);
	return hash;
}

asQWORD HashLifetimeView(const asCASTLifetimeView& view)
{
	asQWORD hash = asAST_LIFETIME_HASH_OFFSET;
	hash = HashField(hash, asAST_LIFETIME_VIEW_DOMAIN);
	hash = HashField(hash, view.GetProtocolRevision());
	hash = HashField(hash, view.GetScopeEdgeCount());
	for( asUINT i = 0; i < view.GetScopeEdgeCount(); ++i )
	{
		const asSASTLifetimeScopeEdge& edge = *view.GetScopeEdge(i);
		hash = HashField(hash, asASTHashLifetimeNodeId(edge.region));
		hash = HashField(hash, asASTHashLifetimeNodeId(edge.parentRegion));
		hash = HashField(hash, edge.depth);
		hash = HashField(hash, edge.structuralOrdinal);
	}
	hash = HashField(hash, view.GetRecordCount());
	for( asUINT i = 0; i < view.GetRecordCount(); ++i )
	{
		const asSASTLifetimeViewRecord& record = *view.GetRecord(i);
		hash = HashField(hash, asASTHashLifetimeRecord(record.record));
		hash = HashField(hash, record.recordIndex);
		hash = HashField(hash, record.scopeEdgeIndex);
		hash = HashField(hash, record.activationOrdinal);
		hash = HashField(hash, record.cleanupOrdinal);
	}
	hash = HashField(hash, view.GetCommitPointCount());
	for( asUINT i = 0; i < view.GetCommitPointCount(); ++i )
	{
		const asSASTLifetimeCommitPoint& commit = *view.GetCommitPoint(i);
		hash = HashField(hash, asASTHashLifetimeNodeId(commit.activationPoint));
		hash = HashField(hash, asASTHashLifetimeNodeId(commit.semanticRegion));
		hash = HashField(hash, commit.recordIndex);
		hash = HashIndexArray(hash, commit.liveBefore);
		hash = HashIndexArray(hash, commit.liveAfter);
		hash = HashIndexArray(hash, commit.abortCleanup);
		hash = HashIndexArray(hash, commit.reverseLiveCleanup);
	}
	hash = HashField(hash, view.GetConstructionPlanCount());
	for( asUINT i = 0; i < view.GetConstructionPlanCount(); ++i )
	{
		const asSASTLifetimeConstructionPlan& plan =
			*view.GetConstructionPlan(i);
		hash = HashField(hash, plan.constructor.value);
		hash = HashField(hash, plan.constructor.snapshotOwner);
		hash = HashField(hash, plan.ownerClass.value);
		hash = HashField(hash, plan.ownerClass.snapshotOwner);
		hash = HashIndexArray(hash, plan.recordIndices);
		hash = HashField(hash, plan.completeRecordIndex);
	}
	hash = HashField(hash, view.GetAggregatePlanCount());
	for( asUINT i = 0; i < view.GetAggregatePlanCount(); ++i )
	{
		const asSASTLifetimeAggregatePlan& plan = *view.GetAggregatePlan(i);
		hash = HashField(hash, asASTHashLifetimeNodeId(
			asSASTLifetimeNodeId::FromExpr(plan.aggregate)));
		hash = HashField(hash, asASTHashLifetimeNodeId(
			asSASTLifetimeNodeId::FromExpr(plan.parentAggregate)));
		hash = HashField(hash, plan.semanticOwner.value);
		hash = HashField(hash, plan.semanticOwner.snapshotOwner);
		hash = HashField(hash, plan.recordIndex);
		hash = HashField(hash, plan.actionKind);
		hash = HashField(hash, asASTHashLifetimeNodeId(plan.actionTarget));
		hash = HashField(hash, plan.elementCommits.GetLength());
		for( asUINT elementIndex = 0;
			elementIndex < plan.elementCommits.GetLength(); ++elementIndex )
		{
			hash = HashField(hash, asASTHashLifetimeNodeId(
				asSASTLifetimeNodeId::FromExpr(
					plan.elementCommits[elementIndex])));
		}
	}
	hash = HashField(hash, view.GetCleanupBindingCount());
	for( asUINT i = 0; i < view.GetCleanupBindingCount(); ++i )
	{
		const asSASTLifetimeCleanupBinding& binding =
			*view.GetCleanupBinding(i);
		hash = HashField(hash, binding.statement.value);
		hash = HashField(hash, binding.expression.value);
		hash = HashField(hash, asASTHashLifetimeNodeId(binding.subject));
		hash = HashField(hash, binding.actionKind);
		hash = HashField(hash, asASTHashLifetimeNodeId(binding.actionTarget));
		hash = HashField(hash, binding.role);
		hash = HashField(hash, binding.structuralOwner.value);
		hash = HashField(hash, binding.exitMask);
		hash = HashField(hash, binding.recordIndex);
	}
	hash = HashField(hash, view.GetExitPlanCount());
	for( asUINT i = 0; i < view.GetExitPlanCount(); ++i )
	{
		const asSASTLifetimeExitPlan& plan = *view.GetExitPlan(i);
		hash = HashField(hash, plan.structuralOwner.value);
		hash = HashField(hash, plan.kind);
		hash = HashField(hash, plan.exitMask);
		hash = HashIndexArray(hash, plan.recordIndices);
	}
	hash = HashField(hash, view.GetForEachPhaseCount());
	for( asUINT i = 0; i < view.GetForEachPhaseCount(); ++i )
	{
		const asSASTForEachPhases& phases = *view.GetForEachPhases(i);
		hash = HashField(hash, phases.statement.value);
		hash = HashField(hash, phases.initializer.value);
		hash = HashField(hash, phases.condition.value);
		hash = HashField(hash, phases.body.value);
		hash = HashField(hash, phases.increment.value);
		hash = HashField(hash, phases.cleanup.value);
		hash = HashField(hash, phases.lifetimeRecordIndex);
	}
	return hash;
}
}

asCASTLifetimeView::asCASTLifetimeView()
	: protocolRevision(0)
	, structuralDigest(0)
{
}

void asCASTLifetimeView::Reset()
{
	protocolRevision = 0;
	structuralDigest = 0;
	scopeEdges.SetLength(0);
	records.SetLength(0);
	commitPoints.SetLength(0);
	constructionPlans.SetLength(0);
	aggregatePlans.SetLength(0);
	cleanupBindings.SetLength(0);
	exitPlans.SetLength(0);
	forEachPhases.SetLength(0);
}

const asSASTLifetimeScopeEdge* asCASTLifetimeView::GetScopeEdge(
	asUINT index) const
{
	return index < scopeEdges.GetLength() ? &scopeEdges[index] : 0;
}

const asSASTLifetimeViewRecord* asCASTLifetimeView::GetRecord(
	asUINT index) const
{
	return index < records.GetLength() ? &records[index] : 0;
}

const asSASTLifetimeCommitPoint* asCASTLifetimeView::GetCommitPoint(
	asUINT index) const
{
	return index < commitPoints.GetLength() ? &commitPoints[index] : 0;
}

const asSASTLifetimeConstructionPlan*
asCASTLifetimeView::GetConstructionPlan(asUINT index) const
{
	return index < constructionPlans.GetLength()
		? &constructionPlans[index] : 0;
}

const asSASTLifetimeConstructionPlan*
asCASTLifetimeView::FindConstructionPlan(asASTDeclId constructor) const
{
	for( asUINT index = 0; index < constructionPlans.GetLength(); ++index )
		if( constructionPlans[index].constructor == constructor )
			return &constructionPlans[index];
	return 0;
}

const asSASTLifetimeAggregatePlan* asCASTLifetimeView::GetAggregatePlan(
	asUINT index) const
{
	return index < aggregatePlans.GetLength() ? &aggregatePlans[index] : 0;
}

const asSASTLifetimeAggregatePlan* asCASTLifetimeView::FindAggregatePlan(
	asASTExprId aggregate) const
{
	for( asUINT index = 0; index < aggregatePlans.GetLength(); ++index )
		if( aggregatePlans[index].aggregate == aggregate )
			return &aggregatePlans[index];
	return 0;
}

int asCASTLifetimeView::DeriveAggregateAbortCleanup(
	asASTExprId aggregate,
	asUINT committedCount,
	asCArray<asSASTLifetimeAggregateCleanupAction>& outActions) const
{
	outActions.SetLength(0);
	const asSASTLifetimeAggregatePlan* const plan =
		FindAggregatePlan(aggregate);
	if( plan == 0
		|| plan->actionKind != asAST_LIFETIME_ACTION_DESTROY_ELEMENT
		|| !plan->actionTarget.IsValid()
		|| committedCount > plan->elementCommits.GetLength() )
	{
		return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
	}
	for( asUINT cursor = committedCount; cursor > 0; --cursor )
	{
		asSASTLifetimeAggregateCleanupAction action;
		action.recordIndex = plan->recordIndex;
		action.elementOrdinal = cursor - 1;
		action.element = plan->elementCommits[cursor - 1];
		action.actionKind = plan->actionKind;
		action.actionTarget = plan->actionTarget;
		outActions.PushLast(action);
	}
	return asAST_VERIFY_OK;
}

const asSASTLifetimeCleanupBinding* asCASTLifetimeView::GetCleanupBinding(
	asUINT index) const
{
	return index < cleanupBindings.GetLength() ? &cleanupBindings[index] : 0;
}

const asSASTLifetimeCleanupBinding* asCASTLifetimeView::FindCleanupBinding(
	asASTStmtId statement) const
{
	for( asUINT index = 0; index < cleanupBindings.GetLength(); ++index )
		if( cleanupBindings[index].statement == statement )
			return &cleanupBindings[index];
	return 0;
}

const asSASTLifetimeExitPlan* asCASTLifetimeView::GetExitPlan(
	asUINT index) const
{
	return index < exitPlans.GetLength() ? &exitPlans[index] : 0;
}

const asSASTLifetimeExitPlan* asCASTLifetimeView::FindExitPlan(
	asASTStmtId structuralOwner,
	asEASTLifetimeExitPlanKind kind) const
{
	for( asUINT index = 0; index < exitPlans.GetLength(); ++index )
		if( exitPlans[index].structuralOwner == structuralOwner
			&& exitPlans[index].kind == kind )
		{
			return &exitPlans[index];
		}
	return 0;
}

const asSASTForEachPhases* asCASTLifetimeView::GetForEachPhases(
	asUINT index) const
{
	return index < forEachPhases.GetLength() ? &forEachPhases[index] : 0;
}

const asSASTForEachPhases* asCASTLifetimeView::FindForEachPhases(
	asASTStmtId statement) const
{
	for( asUINT index = 0; index < forEachPhases.GetLength(); ++index )
		if( forEachPhases[index].statement == statement )
			return &forEachPhases[index];
	return 0;
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
		hash = HashField(hash, asASTHashLifetimeRecord(records[i]));
	return hash;
}

int asCASTBuildLifetimeView(
	const asCASTContext& context,
	asCASTLifetimeView& outView)
{
	outView.Reset();
	if( context.GetLifecycleState() == asAST_CONTEXT_BUILDING
		|| context.GetLifetimeProtocolRevision()
			!= asAST_LIFETIME_PROTOCOL_REVISION )
	{
		return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
	}
	outView.protocolRevision = context.GetLifetimeProtocolRevision();

	asCArray<asSASTLifetimeNodeId> regions;
	for( asUINT index = 0;
		index < context.GetLifetimeRecordCount(); ++index )
	{
		const asSASTLifetimeRecord* const record =
			context.GetLifetimeRecord(index);
		if( record == 0
			|| VerifyBasicLifetimeRecord(context, *record, index)
				!= asAST_VERIFY_OK )
		{
			outView.Reset();
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}
		if( !IsConstructionRecord(*record)
			&& !IsAggregateRecord(*record)
			&& !IsFullExpressionRecord(*record)
			&& !ContainsRegion(regions, record->semanticRegion) )
			regions.PushLast(record->semanticRegion);
	}
	SortRegions(regions);

	asCArray<asASTStmtId> stmtParents;
	if( BuildStmtParents(context, stmtParents) != asAST_VERIFY_OK )
	{
		outView.Reset();
		return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
	}
	for( asUINT regionIndex = 0;
		regionIndex < regions.GetLength(); ++regionIndex )
	{
		const asSASTLifetimeNodeId& region = regions[regionIndex];
		const asCStmt* const regionStmt = region.nodeClass
			== asAST_LIFETIME_NODE_STMT
			? context.GetStmt(asASTStmtId(region.value)) : 0;
		if( regionStmt == 0
			|| (regionStmt->kind != asAST_STMT_BLOCK
				&& regionStmt->kind != asAST_STMT_FOREACH) )
		{
			outView.Reset();
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}
		asSASTLifetimeScopeEdge edge;
		edge.region = region;
		edge.parentRegion = FindNearestLifetimeRegionParent(
			regions, stmtParents, region);
		edge.depth = CountLifetimeRegionDepth(regions, stmtParents, region);
		edge.structuralOrdinal = region.value;
		outView.scopeEdges.PushLast(edge);
	}

	for( asUINT statementIndex = 1;
		statementIndex <= context.GetStmtCount(); ++statementIndex )
	{
		const asCStmt* const statement =
			context.GetStmt(asASTStmtId(statementIndex));
		if( statement == 0 || statement->kind != asAST_STMT_FOREACH )
			continue;
		if( statement->children.GetLength() < 3
			|| statement->children.GetLength() > 4
			|| !statement->expr.IsValid() )
		{
			outView.Reset();
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}
		asSASTForEachPhases phases;
		phases.statement = statement->id;
		phases.initializer = statement->children[0];
		phases.condition = statement->expr;
		phases.body = statement->children[1];
		phases.increment = statement->children[2];
		phases.cleanup = statement->children.GetLength() == 4
			? statement->children[3] : asASTStmtId();
		outView.forEachPhases.PushLast(phases);
	}

	for( asUINT index = 0;
		index < context.GetLifetimeRecordCount(); ++index )
	{
		const asSASTLifetimeRecord& source =
			*context.GetLifetimeRecord(index);
		asSASTLifetimeViewRecord derived;
		derived.record = source;
		derived.recordIndex = index;
		derived.scopeEdgeIndex = (IsConstructionRecord(source)
			|| IsAggregateRecord(source) || IsFullExpressionRecord(source))
			? static_cast<asUINT>(-1)
			: FindRegionIndex(outView, source.semanticRegion);
		if( ((!IsConstructionRecord(source) && !IsAggregateRecord(source)
				&& !IsFullExpressionRecord(source))
				&& derived.scopeEdgeIndex == static_cast<asUINT>(-1))
			|| !ValidateLifetimeRecord(
				context, source, derived.activationOrdinal) )
		{
			outView.Reset();
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}
		for( asUINT previousIndex = 0;
			previousIndex < outView.records.GetLength(); ++previousIndex )
		{
			const asSASTLifetimeViewRecord& previous =
				outView.records[previousIndex];
			if( RecordsDuplicateAction(previous.record, source)
				|| (!IsConstructionRecord(source)
					&& !IsAggregateRecord(source)
					&& previous.record.semanticRegion == source.semanticRegion
					&& previous.activationOrdinal
						>= derived.activationOrdinal) )
			{
				outView.Reset();
				return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
			}
		}
		outView.records.PushLast(derived);
	}

	// The first supported full-expression family is bidirectional: an exact
	// Cleanup(MaterializeTemporary(Construct)) source shape must have one Sema
	// record.  The compatibility wrapper alone is never lifetime authority.
	for( asUINT statementIndex = 1;
		statementIndex <= context.GetStmtCount(); ++statementIndex )
	{
		const asCStmt* const statement = context.GetStmt(
			asASTStmtId(statementIndex));
		asASTExprId materialize;
		asASTDeclId destructor;
		if( statement == 0 ) continue;
		const asETemporaryShapeClassification classification =
			ClassifySupportedConstructorTemporary(
				context, *statement, materialize, destructor);
		if( classification == asTEMPORARY_SHAPE_NOT_CANDIDATE )
		{
			continue;
		}
		if( classification == asTEMPORARY_SHAPE_MALFORMED )
		{
			outView.Reset();
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}
		asUINT matchingRecords = 0;
		for( asUINT recordIndex = 0;
			recordIndex < outView.records.GetLength(); ++recordIndex )
		{
			const asSASTLifetimeRecord& record =
				outView.records[recordIndex].record;
			if( IsFullExpressionRecord(record)
				&& record.subject
					== asSASTLifetimeNodeId::FromExpr(materialize)
				&& record.actionTarget
					== asSASTLifetimeNodeId::FromDecl(destructor)
				&& record.semanticRegion
					== asSASTLifetimeNodeId::FromStmt(statement->id) )
			{
				++matchingRecords;
			}
		}
		if( matchingRecords != 1 )
		{
			outView.Reset();
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}
	}

	for( asUINT index = 0; index < outView.records.GetLength(); ++index )
	{
		asSASTLifetimeViewRecord& record = outView.records[index];
		asUINT laterInRegion = 0;
		for( asUINT otherIndex = 0;
			otherIndex < outView.records.GetLength(); ++otherIndex )
		{
			const asSASTLifetimeViewRecord& other =
				outView.records[otherIndex];
			if( other.record.semanticRegion == record.record.semanticRegion
				&& other.activationOrdinal > record.activationOrdinal )
			{
				++laterInRegion;
			}
		}
		record.cleanupOrdinal = laterInRegion;
	}

	for( asUINT recordIndex = 0;
		recordIndex < outView.records.GetLength(); ++recordIndex )
	{
		const asSASTLifetimeViewRecord& derived =
			outView.records[recordIndex];
		if( !IsConstructionRecord(derived.record) ) continue;

		const asASTDeclId constructor(
			derived.record.semanticRegion.value);
		asSASTLifetimeConstructionPlan* plan = 0;
		for( asUINT planIndex = 0;
			planIndex < outView.constructionPlans.GetLength(); ++planIndex )
		{
			if( outView.constructionPlans[planIndex].constructor
				== constructor )
			{
				plan = &outView.constructionPlans[planIndex];
				break;
			}
		}
		if( plan == 0 )
		{
			const asCDecl* const constructorDecl =
				context.GetDecl(constructor);
			const asCDecl* const owner = constructorDecl != 0
				? context.GetDecl(constructorDecl->parent) : 0;
			if( constructorDecl == 0
				|| constructorDecl->kind != asAST_DECL_CONSTRUCTOR
				|| owner == 0 || owner->kind != asAST_DECL_CLASS )
			{
				outView.Reset();
				return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
			}
			asSASTLifetimeConstructionPlan fresh;
			fresh.constructor = constructor;
			fresh.ownerClass = owner->id;
			outView.constructionPlans.PushLast(fresh);
			plan = &outView.constructionPlans[
				outView.constructionPlans.GetLength() - 1];
		}
		if( derived.record.constructionStep
			!= plan->recordIndices.GetLength() )
		{
			outView.Reset();
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}
		plan->recordIndices.PushLast(derived.recordIndex);
		if( derived.record.completeObjectCommit )
		{
			if( plan->completeRecordIndex != static_cast<asUINT>(-1) )
			{
				outView.Reset();
				return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
			}
			plan->completeRecordIndex = derived.recordIndex;
		}
	}
	for( asUINT planIndex = 0;
		planIndex < outView.constructionPlans.GetLength(); ++planIndex )
	{
		if( !ValidateConstructionPlan(
			context, outView, outView.constructionPlans[planIndex]) )
		{
			outView.Reset();
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}
	}

	for( asUINT recordIndex = 0;
		recordIndex < outView.records.GetLength(); ++recordIndex )
	{
		const asSASTLifetimeViewRecord& derived = outView.records[recordIndex];
		if( !IsAggregateRecord(derived.record) ) continue;
		const asASTExprId aggregateId(derived.record.subject.value);
		if( outView.FindAggregatePlan(aggregateId) != 0 )
		{
			outView.Reset();
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}
		const asCExpr* const aggregate = context.GetExpr(aggregateId);
		if( aggregate == 0 )
		{
			outView.Reset();
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}
		asSASTLifetimeAggregatePlan plan;
		plan.aggregate = aggregateId;
		plan.semanticOwner = asASTDeclId(
			derived.record.semanticRegion.value);
		plan.recordIndex = derived.recordIndex;
		plan.actionKind = derived.record.actionKind;
		plan.actionTarget = derived.record.actionTarget;
		plan.elementCommits = aggregate->children;
		outView.aggregatePlans.PushLast(plan);
	}
	for( asUINT parentIndex = 0;
		parentIndex < outView.aggregatePlans.GetLength(); ++parentIndex )
	{
		const asSASTLifetimeAggregatePlan& parent =
			outView.aggregatePlans[parentIndex];
		for( asUINT elementIndex = 0;
			elementIndex < parent.elementCommits.GetLength(); ++elementIndex )
		{
			const asASTExprId elementId = parent.elementCommits[elementIndex];
			const asCExpr* const element = context.GetExpr(elementId);
			const asCDecl* const resolved = element != 0
				? context.GetDecl(element->resolvedDecl) : 0;
			const bool isNestedAggregate = element != 0
				&& element->kind == asAST_EXPR_CONSTRUCT
				&& resolved != 0
				&& (resolved->traits & asAST_TRAIT_LIST_FACTORY) != 0;
			asSASTLifetimeAggregatePlan* child = 0;
			for( asUINT childIndex = 0;
				childIndex < outView.aggregatePlans.GetLength(); ++childIndex )
			{
				if( outView.aggregatePlans[childIndex].aggregate == elementId )
				{
					child = &outView.aggregatePlans[childIndex];
					break;
				}
			}
			if( isNestedAggregate && child == 0 )
			{
				outView.Reset();
				return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
			}
			if( child == 0 ) continue;
			if( child->aggregate == parent.aggregate
				|| child->parentAggregate.IsValid()
				|| parent.semanticOwner != child->semanticOwner )
			{
				outView.Reset();
				return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
			}
			child->parentAggregate = parent.aggregate;
		}
	}
	for( asUINT planIndex = 0;
		planIndex < outView.aggregatePlans.GetLength(); ++planIndex )
	{
		asASTExprId parent = outView.aggregatePlans[planIndex].parentAggregate;
		for( asUINT guard = 0;
			parent.IsValid() && guard <= outView.aggregatePlans.GetLength(); ++guard )
		{
			if( parent == outView.aggregatePlans[planIndex].aggregate )
			{
				outView.Reset();
				return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
			}
			const asSASTLifetimeAggregatePlan* const parentPlan =
				outView.FindAggregatePlan(parent);
			if( parentPlan == 0 )
			{
				outView.Reset();
				return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
			}
			parent = parentPlan->parentAggregate;
		}
		if( parent.IsValid() )
		{
			outView.Reset();
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}
	}

	for( asUINT statementIndex = 1;
		statementIndex <= context.GetStmtCount(); ++statementIndex )
	{
		const asASTStmtId statementId(statementIndex);
		const asCStmt* const statement = context.GetStmt(statementId);
		const asCExpr* const expression = statement != 0
			&& statement->kind == asAST_STMT_EXPR
			? context.GetExpr(statement->expr) : 0;
		const bool isCompatibilityCleanup = expression != 0
			&& expression->kind == asAST_EXPR_CLEANUP
			&& (expression->literal.Equals("scope-exit")
				|| expression->literal.Equals("scope-release"));
		if( !isCompatibilityCleanup ) continue;

		asEASTLifetimeActionKind actionKind;
		asASTDeclId subject;
		asASTDeclId actionTarget;
		asASTExprId cleanupExpression;
		if( !DecodeCompatibilityCleanup(
			context, statementId, actionKind, subject,
			actionTarget, cleanupExpression) )
		{
			outView.Reset();
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}
		const asUINT recordIndex = FindMatchingRecordIndex(
			outView, actionKind, subject, actionTarget);
		if( recordIndex == static_cast<asUINT>(-1) )
		{
			outView.Reset();
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}
		const asSASTLifetimeViewRecord& record = outView.records[recordIndex];
		const asASTStmtId parentId = statementId.value < stmtParents.GetLength()
			? stmtParents[statementId.value] : asASTStmtId();
		const asCStmt* const parent = context.GetStmt(parentId);
		if( parent == 0 )
		{
			outView.Reset();
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}

		asSASTLifetimeCleanupBinding binding;
		binding.statement = statementId;
		binding.expression = cleanupExpression;
		binding.subject = record.record.subject;
		binding.actionKind = record.record.actionKind;
		binding.actionTarget = record.record.actionTarget;
		binding.structuralOwner = parentId;
		binding.recordIndex = recordIndex;
		if( record.record.subjectKind
				== asAST_LIFETIME_SUBJECT_FOREACH_ITERATOR
			&& parentId.value == record.record.semanticRegion.value
			&& parent->kind == asAST_STMT_FOREACH )
		{
			const asSASTForEachPhases* const phases =
				outView.FindForEachPhases(parentId);
			if( phases == 0 || phases->cleanup != statementId )
			{
				outView.Reset();
				return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
			}
			binding.role = asAST_LIFETIME_COMPAT_FOREACH_EXIT;
			binding.exitMask = asAST_LIFETIME_EXIT_NORMAL
				| asAST_LIFETIME_EXIT_RETURN
				| asAST_LIFETIME_EXIT_BREAK
				| asAST_LIFETIME_EXIT_EXCEPTION;
		}
		else if( parentId.value == record.record.semanticRegion.value
			&& parent->kind == asAST_STMT_BLOCK )
		{
			binding.role = asAST_LIFETIME_COMPAT_NORMAL_EXIT;
			binding.exitMask = asAST_LIFETIME_EXIT_NORMAL;
		}
		else
		{
			const asDWORD transferMask = TransferExitMask(parent);
			const asUINT directOrdinal = FindDirectDescendantOrdinal(
				context, stmtParents, record.record.semanticRegion,
				asSASTLifetimeNodeId::FromStmt(parentId));
			if( transferMask == asAST_LIFETIME_EXIT_NONE
				|| directOrdinal == static_cast<asUINT>(-1)
				|| directOrdinal <= record.activationOrdinal
				|| !TransferExitsRecordRegion(context, record, *parent) )
			{
				outView.Reset();
				return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
			}
			binding.role = asAST_LIFETIME_COMPAT_TRANSFER_EXIT;
			binding.exitMask = transferMask;
		}
		if( (record.record.supportedExitMask & binding.exitMask)
			!= binding.exitMask )
		{
			outView.Reset();
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}
		outView.cleanupBindings.PushLast(binding);
	}

	for( asUINT recordIndex = 0;
		recordIndex < outView.records.GetLength(); ++recordIndex )
	{
		const asSASTLifetimeViewRecord& record = outView.records[recordIndex];
		if( IsConstructionRecord(record.record)
			|| IsAggregateRecord(record.record)
			|| IsFullExpressionRecord(record.record) )
		{
			continue;
		}
		asUINT normalCount = 0;
		asUINT foreachCount = 0;
		for( asUINT bindingIndex = 0;
			bindingIndex < outView.cleanupBindings.GetLength(); ++bindingIndex )
		{
			const asSASTLifetimeCleanupBinding& binding =
				outView.cleanupBindings[bindingIndex];
			if( binding.recordIndex != recordIndex ) continue;
			if( binding.role == asAST_LIFETIME_COMPAT_NORMAL_EXIT )
				++normalCount;
			if( binding.role == asAST_LIFETIME_COMPAT_FOREACH_EXIT )
				++foreachCount;
		}

		if( record.record.subjectKind
			== asAST_LIFETIME_SUBJECT_FOREACH_ITERATOR )
		{
			if( foreachCount != 1 || normalCount != 0 )
			{
				outView.Reset();
				return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
			}
			for( asUINT phaseIndex = 0;
				phaseIndex < outView.forEachPhases.GetLength(); ++phaseIndex )
			{
				if( outView.forEachPhases[phaseIndex].statement.value
					== record.record.semanticRegion.value )
				{
					outView.forEachPhases[phaseIndex].lifetimeRecordIndex =
						recordIndex;
				}
			}
			continue;
		}

		if( normalCount != 1 )
		{
			outView.Reset();
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}
		const asCStmt* const region = context.GetStmt(
			asASTStmtId(record.record.semanticRegion.value));
		if( region == 0
			|| record.cleanupOrdinal >= region->children.GetLength() )
		{
			outView.Reset();
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}
		asUINT regionRecordCount = 0;
		for( asUINT otherIndex = 0;
			otherIndex < outView.records.GetLength(); ++otherIndex )
		{
			if( outView.records[otherIndex].record.semanticRegion
				== record.record.semanticRegion )
			{
				++regionRecordCount;
			}
		}
		if( regionRecordCount == 0
			|| regionRecordCount > region->children.GetLength()
			|| record.cleanupOrdinal >= regionRecordCount )
		{
			outView.Reset();
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}
		const asASTStmtId expectedNormal = region->children[
			region->children.GetLength() - regionRecordCount
				+ record.cleanupOrdinal];
		const asSASTLifetimeCleanupBinding* const normal =
			outView.FindCleanupBinding(expectedNormal);
		if( normal == 0 || normal->recordIndex != recordIndex
			|| normal->role != asAST_LIFETIME_COMPAT_NORMAL_EXIT )
		{
			outView.Reset();
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}

		for( asUINT transferIndex = 1;
			transferIndex <= context.GetStmtCount(); ++transferIndex )
		{
			const asCStmt* const transfer =
				context.GetStmt(asASTStmtId(transferIndex));
			if( TransferExitMask(transfer) == asAST_LIFETIME_EXIT_NONE
				|| !StatementSubtreeContains(
					context,
					asASTStmtId(record.record.semanticRegion.value),
					asASTStmtId(transferIndex)) )
			{
				continue;
			}
			const asUINT directOrdinal = FindDirectDescendantOrdinal(
				context, stmtParents, record.record.semanticRegion,
				asSASTLifetimeNodeId::FromStmt(asASTStmtId(transferIndex)));
			const bool expected = directOrdinal != static_cast<asUINT>(-1)
				&& directOrdinal > record.activationOrdinal
				&& TransferExitsRecordRegion(context, record, *transfer);
			asUINT found = 0;
			for( asUINT childIndex = 0;
				childIndex < transfer->children.GetLength(); ++childIndex )
			{
				const asSASTLifetimeCleanupBinding* const binding =
					outView.FindCleanupBinding(transfer->children[childIndex]);
				if( binding != 0 && binding->recordIndex == recordIndex )
					++found;
			}
			if( (expected && found != 1) || (!expected && found != 0) )
			{
				outView.Reset();
				return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
			}
		}
	}

	for( asUINT scopeIndex = 0;
		scopeIndex < outView.scopeEdges.GetLength(); ++scopeIndex )
	{
		const asSASTLifetimeScopeEdge& scope =
			outView.scopeEdges[scopeIndex];
		const asCStmt* const region = scope.region.nodeClass
			== asAST_LIFETIME_NODE_STMT
			? context.GetStmt(asASTStmtId(scope.region.value)) : 0;
		if( region == 0 || region->kind != asAST_STMT_BLOCK ) continue;

		asSASTLifetimeExitPlan plan;
		plan.structuralOwner = region->id;
		plan.kind = asAST_LIFETIME_EXIT_PLAN_NORMAL;
		plan.exitMask = asAST_LIFETIME_EXIT_NORMAL;
		for( asUINT reverseIndex = outView.records.GetLength();
			reverseIndex > 0; --reverseIndex )
		{
			const asSASTLifetimeViewRecord& record =
				outView.records[reverseIndex - 1];
			if( record.record.subjectKind == asAST_LIFETIME_SUBJECT_LOCAL
				&& record.record.semanticRegion == scope.region )
			{
				plan.recordIndices.PushLast(record.recordIndex);
			}
		}
		if( plan.recordIndices.GetLength() != 0 )
			outView.exitPlans.PushLast(plan);
	}

	for( asUINT recordIndex = 0;
		recordIndex < outView.records.GetLength(); ++recordIndex )
	{
		const asSASTLifetimeViewRecord& record =
			outView.records[recordIndex];
		if( record.record.subjectKind
			!= asAST_LIFETIME_SUBJECT_FOREACH_ITERATOR )
		{
			continue;
		}
		asSASTLifetimeExitPlan plan;
		plan.structuralOwner =
			asASTStmtId(record.record.semanticRegion.value);
		plan.kind = asAST_LIFETIME_EXIT_PLAN_FOREACH;
		plan.exitMask = asAST_LIFETIME_EXIT_NORMAL
			| asAST_LIFETIME_EXIT_RETURN
			| asAST_LIFETIME_EXIT_BREAK
			| asAST_LIFETIME_EXIT_EXCEPTION;
		plan.recordIndices.PushLast(record.recordIndex);
		outView.exitPlans.PushLast(plan);
	}

	for( asUINT recordIndex = 0;
		recordIndex < outView.records.GetLength(); ++recordIndex )
	{
		const asSASTLifetimeViewRecord& record =
			outView.records[recordIndex];
		if( !IsFullExpressionRecord(record.record) ) continue;
		bool alreadyPlanned = false;
		for( asUINT planIndex = 0;
			planIndex < outView.exitPlans.GetLength(); ++planIndex )
		{
			if( outView.exitPlans[planIndex].kind
					== asAST_LIFETIME_EXIT_PLAN_FULL_EXPRESSION
				&& outView.exitPlans[planIndex].structuralOwner.value
					== record.record.semanticRegion.value )
			{
				alreadyPlanned = true;
				break;
			}
		}
		if( alreadyPlanned ) continue;

		asSASTLifetimeExitPlan plan;
		plan.structuralOwner = asASTStmtId(
			record.record.semanticRegion.value);
		plan.kind = asAST_LIFETIME_EXIT_PLAN_FULL_EXPRESSION;
		plan.exitMask = asAST_LIFETIME_EXIT_NORMAL
			| asAST_LIFETIME_EXIT_EXCEPTION;
		for( asUINT reverseIndex = outView.records.GetLength();
			reverseIndex > 0; --reverseIndex )
		{
			const asSASTLifetimeViewRecord& candidate =
				outView.records[reverseIndex - 1];
			if( IsFullExpressionRecord(candidate.record)
				&& candidate.record.semanticRegion
					== record.record.semanticRegion )
			{
				plan.recordIndices.PushLast(candidate.recordIndex);
			}
		}
		outView.exitPlans.PushLast(plan);
	}

	for( asUINT transferIndex = 1;
		transferIndex <= context.GetStmtCount(); ++transferIndex )
	{
		const asCStmt* const transfer =
			context.GetStmt(asASTStmtId(transferIndex));
		const asDWORD exitMask = TransferExitMask(transfer);
		if( transfer == 0 || exitMask == asAST_LIFETIME_EXIT_NONE )
			continue;

		asSASTLifetimeExitPlan plan;
		plan.structuralOwner = transfer->id;
		plan.kind = asAST_LIFETIME_EXIT_PLAN_TRANSFER;
		plan.exitMask = exitMask;
		for( asUINT recordIndex = 0;
			recordIndex < outView.records.GetLength(); ++recordIndex )
		{
			const asSASTLifetimeViewRecord& record =
				outView.records[recordIndex];
			if( (record.record.subjectKind != asAST_LIFETIME_SUBJECT_LOCAL
					&& record.record.subjectKind
						!= asAST_LIFETIME_SUBJECT_FOREACH_ITERATOR)
				|| (record.record.supportedExitMask & exitMask) != exitMask )
			{
				continue;
			}
			const asUINT directOrdinal = FindDirectDescendantOrdinal(
				context, stmtParents, record.record.semanticRegion,
				asSASTLifetimeNodeId::FromStmt(transfer->id));
			if( directOrdinal == static_cast<asUINT>(-1)
				|| directOrdinal <= record.activationOrdinal
				|| !TransferExitsRecordRegion(context, record, *transfer) )
			{
				continue;
			}
			AppendExitPlanRecord(
				outView, record.recordIndex, plan.recordIndices);
		}
		if( plan.recordIndices.GetLength() != 0 )
			outView.exitPlans.PushLast(plan);
	}

	for( asUINT index = 0; index < outView.records.GetLength(); ++index )
	{
		const asSASTLifetimeViewRecord& current = outView.records[index];
		if( IsAggregateRecord(current.record) )
		{
			// Aggregate liveness is dynamic and is derived on demand from the
			// backend-owned committed count. It must not be flattened into one
			// static commit point for the complete aggregate.
			continue;
		}
		if( IsConstructionRecord(current.record) )
		{
			const asSASTLifetimeConstructionPlan* const plan =
				outView.FindConstructionPlan(
					asASTDeclId(current.record.semanticRegion.value));
			asUINT step = static_cast<asUINT>(-1);
			for( asUINT candidate = 0;
				plan != 0 && candidate < plan->recordIndices.GetLength();
				++candidate )
			{
				if( plan->recordIndices[candidate] == current.recordIndex )
				{
					step = candidate;
					break;
				}
			}
			if( plan == 0 || step == static_cast<asUINT>(-1) )
			{
				outView.Reset();
				return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
			}
			asSASTLifetimeCommitPoint commit;
			commit.activationPoint = current.record.activationPoint;
			commit.semanticRegion = current.record.semanticRegion;
			commit.recordIndex = current.recordIndex;
			for( asUINT prefix = 0; prefix < step; ++prefix )
				commit.liveBefore.PushLast(plan->recordIndices[prefix]);
			commit.liveAfter = commit.liveBefore;
			commit.liveAfter.PushLast(current.recordIndex);
			for( asUINT prefix = commit.liveBefore.GetLength();
				prefix > 0; --prefix )
			{
				commit.abortCleanup.PushLast(
					commit.liveBefore[prefix - 1]);
			}
			for( asUINT prefix = commit.liveAfter.GetLength();
				prefix > 0; --prefix )
			{
				commit.reverseLiveCleanup.PushLast(
					commit.liveAfter[prefix - 1]);
			}
			outView.commitPoints.PushLast(commit);
			continue;
		}
		if( IsFullExpressionRecord(current.record) )
		{
			asSASTLifetimeCommitPoint commit;
			commit.activationPoint = current.record.activationPoint;
			commit.semanticRegion = current.record.semanticRegion;
			commit.recordIndex = current.recordIndex;
			for( asUINT previous = 0; previous < index; ++previous )
			{
				if( IsFullExpressionRecord(outView.records[previous].record)
					&& outView.records[previous].record.semanticRegion
						== current.record.semanticRegion )
				{
					commit.liveBefore.PushLast(
						outView.records[previous].recordIndex);
				}
			}
			commit.liveAfter = commit.liveBefore;
			commit.liveAfter.PushLast(current.recordIndex);
			for( asUINT live = commit.liveBefore.GetLength(); live > 0; --live )
				commit.abortCleanup.PushLast(commit.liveBefore[live - 1]);
			for( asUINT live = commit.liveAfter.GetLength(); live > 0; --live )
				commit.reverseLiveCleanup.PushLast(commit.liveAfter[live - 1]);
			outView.commitPoints.PushLast(commit);
			continue;
		}
		asCArray<asUINT> reverseScopeChain;
		asUINT scopeIndex = current.scopeEdgeIndex;
		for( asUINT guard = 0;
			scopeIndex < outView.scopeEdges.GetLength()
				&& guard < outView.scopeEdges.GetLength(); ++guard )
		{
			reverseScopeChain.PushLast(scopeIndex);
			const asSASTLifetimeNodeId parent =
				outView.scopeEdges[scopeIndex].parentRegion;
			if( !parent.IsValid() ) break;
			scopeIndex = FindRegionIndex(outView, parent);
		}
		if( reverseScopeChain.GetLength() == 0 )
		{
			outView.Reset();
			return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
		}

		asSASTLifetimeCommitPoint commit;
		commit.activationPoint = current.record.activationPoint;
		commit.semanticRegion = current.record.semanticRegion;
		commit.recordIndex = current.recordIndex;
		for( asUINT reverseIndex = reverseScopeChain.GetLength();
			reverseIndex > 0; --reverseIndex )
		{
			const asUINT chainIndex = reverseIndex - 1;
			const asSASTLifetimeScopeEdge& scope =
				outView.scopeEdges[reverseScopeChain[chainIndex]];
			asUINT boundaryOrdinal = current.activationOrdinal;
			if( scope.region != current.record.semanticRegion )
			{
				const asSASTLifetimeNodeId descendant = chainIndex > 0
					? outView.scopeEdges[
						reverseScopeChain[chainIndex - 1]].region
					: current.record.semanticRegion;
				boundaryOrdinal = FindDirectDescendantOrdinal(
					context, stmtParents, scope.region, descendant);
			}
			if( boundaryOrdinal == static_cast<asUINT>(-1)
				|| !AppendLiveRecordsInRegion(
					outView, scope.region, boundaryOrdinal,
					commit.liveBefore) )
			{
				outView.Reset();
				return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
			}
		}
		for( asUINT liveIndex = 0;
			liveIndex < commit.liveBefore.GetLength(); ++liveIndex )
		{
			if( commit.liveBefore[liveIndex] == current.recordIndex )
			{
				outView.Reset();
				return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
			}
		}
		commit.liveAfter = commit.liveBefore;
		commit.liveAfter.PushLast(current.recordIndex);
		for( asUINT liveIndex = commit.liveBefore.GetLength();
			liveIndex > 0; --liveIndex )
		{
			commit.abortCleanup.PushLast(commit.liveBefore[liveIndex - 1]);
		}
		for( asUINT liveIndex = commit.liveAfter.GetLength();
			liveIndex > 0; --liveIndex )
		{
			commit.reverseLiveCleanup.PushLast(
				commit.liveAfter[liveIndex - 1]);
		}
		outView.commitPoints.PushLast(commit);
	}

	outView.structuralDigest = HashLifetimeView(outView);
	return asAST_VERIFY_OK;
}

int asCASTVerifyLifetimeProtocol(const asCASTContext& context)
{
	asCASTLifetimeView view;
	return asCASTBuildLifetimeView(context, view);
}

END_AS_NAMESPACE
