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
	if( record.actionKind == asAST_LIFETIME_ACTION_DESTROY_VALUE )
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
		&& record.phase != asAST_LIFETIME_PHASE_COMPLETE_OBJECT )
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
		&& (localType->stableKey.Equals(destructorOwner->stableKey)
			|| localType->stableKey.Equals(destructorOwner->name));
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
	cleanupBindings.SetLength(0);
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
		if( !ContainsRegion(regions, record->semanticRegion) )
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
		derived.scopeEdgeIndex = FindRegionIndex(
			outView, source.semanticRegion);
		if( derived.scopeEdgeIndex == static_cast<asUINT>(-1)
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
				|| (previous.record.semanticRegion == source.semanticRegion
					&& previous.activationOrdinal
						>= derived.activationOrdinal) )
			{
				outView.Reset();
				return asAST_VERIFY_INVALID_LIFETIME_PROTOCOL;
			}
		}
		outView.records.PushLast(derived);
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

	for( asUINT index = 0; index < outView.records.GetLength(); ++index )
	{
		const asSASTLifetimeViewRecord& current = outView.records[index];
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
