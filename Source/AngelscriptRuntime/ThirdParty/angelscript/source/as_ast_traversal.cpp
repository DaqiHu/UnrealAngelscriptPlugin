#include "as_ast_traversal.h"

BEGIN_AS_NAMESPACE

namespace
{
asSASTTraversalEdge MakeEdge(
	const asSASTNodeRef& source,
	const asSASTNodeRef& target,
	asEASTEdgeClass edgeClass,
	asEASTEdgeRole role,
	asUINT index)
{
	asSASTTraversalEdge edge;
	edge.source = source;
	edge.target = target;
	edge.edgeClass = edgeClass;
	edge.role = role;
	edge.index = index;
	return edge;
}

bool GetDeclArrayEdge(
	const asSASTNodeRef& source,
	const asCArray<asASTDeclId>& ids,
	asEASTEdgeClass edgeClass,
	asEASTEdgeRole role,
	asUINT wanted,
	asSASTTraversalEdge& outEdge)
{
	if( wanted >= ids.GetLength() ) return false;
	outEdge = MakeEdge(source, asSASTNodeRef::FromDecl(ids[wanted]), edgeClass, role, wanted);
	return true;
}

bool GetStmtArrayEdge(
	const asSASTNodeRef& source,
	const asCArray<asASTStmtId>& ids,
	asEASTEdgeClass edgeClass,
	asEASTEdgeRole role,
	asUINT wanted,
	asSASTTraversalEdge& outEdge)
{
	if( wanted >= ids.GetLength() ) return false;
	outEdge = MakeEdge(source, asSASTNodeRef::FromStmt(ids[wanted]), edgeClass, role, wanted);
	return true;
}

bool GetExprArrayEdge(
	const asSASTNodeRef& source,
	const asCArray<asASTExprId>& ids,
	asEASTEdgeClass edgeClass,
	asEASTEdgeRole role,
	asUINT wanted,
	asSASTTraversalEdge& outEdge)
{
	if( wanted >= ids.GetLength() ) return false;
	outEdge = MakeEdge(source, asSASTNodeRef::FromExpr(ids[wanted]), edgeClass, role, wanted);
	return true;
}

struct asSTraversalState
{
	asCArray<asBYTE> declActive;
	asCArray<asBYTE> typeActive;
	asCArray<asBYTE> stmtActive;
	asCArray<asBYTE> exprActive;
	asCArray<asBYTE> declCompleted;
	asCArray<asBYTE> typeCompleted;
	asCArray<asBYTE> stmtCompleted;
	asCArray<asBYTE> exprCompleted;
	bool skipCompleted;

	asSTraversalState() : skipCompleted(false) {}
};

asCArray<asBYTE>* StateArrayFor(const asSASTNodeRef& node, asSTraversalState& state, bool completed)
{
	switch( node.nodeClass )
	{
	case asAST_NODE_DECL: return completed ? &state.declCompleted : &state.declActive;
	case asAST_NODE_TYPE: return completed ? &state.typeCompleted : &state.typeActive;
	case asAST_NODE_STMT: return completed ? &state.stmtCompleted : &state.stmtActive;
	case asAST_NODE_EXPR: return completed ? &state.exprCompleted : &state.exprActive;
	default: return 0;
	}
}

void InitializeState(const asCASTContext& context, bool skipCompleted, asSTraversalState& state)
{
	state.skipCompleted = skipCompleted;
	state.declActive.SetLength(context.GetDeclCount() + 1);
	state.typeActive.SetLength(context.GetTypeCount() + 1);
	state.stmtActive.SetLength(context.GetStmtCount() + 1);
	state.exprActive.SetLength(context.GetExprCount() + 1);
	state.declCompleted.SetLength(context.GetDeclCount() + 1);
	state.typeCompleted.SetLength(context.GetTypeCount() + 1);
	state.stmtCompleted.SetLength(context.GetStmtCount() + 1);
	state.exprCompleted.SetLength(context.GetExprCount() + 1);
	for( asUINT i = 0; i < state.declActive.GetLength(); ++i ) { state.declActive[i] = 0; state.declCompleted[i] = 0; }
	for( asUINT i = 0; i < state.typeActive.GetLength(); ++i ) { state.typeActive[i] = 0; state.typeCompleted[i] = 0; }
	for( asUINT i = 0; i < state.stmtActive.GetLength(); ++i ) { state.stmtActive[i] = 0; state.stmtCompleted[i] = 0; }
	for( asUINT i = 0; i < state.exprActive.GetLength(); ++i ) { state.exprActive[i] = 0; state.exprCompleted[i] = 0; }
}

int TraverseNode(
	const asCASTContext& context,
	const asSASTNodeRef& node,
	const asSASTTraversalEdge* incomingEdge,
	asUINT depth,
	asIASTConstVisitor& visitor,
	const asSASTTraversalOptions& options,
	asSASTTraversalResult& result,
	asSTraversalState& state)
{
	if( !asCASTNodeExists(context, node) )
	{
		result.status = asAST_TRAVERSAL_INVALID_NODE;
		result.offendingNode = node;
		if( incomingEdge )
			result.offendingEdge = *incomingEdge;
		return result.status;
	}
	if( depth > options.maxDepth )
	{
		result.status = asAST_TRAVERSAL_DEPTH_LIMIT_EXCEEDED;
		result.offendingNode = node;
		if( incomingEdge )
			result.offendingEdge = *incomingEdge;
		return result.status;
	}
	asCArray<asBYTE>* active = StateArrayFor(node, state, false);
	asCArray<asBYTE>* completed = StateArrayFor(node, state, true);
	if( active == 0 || completed == 0 || node.value >= active->GetLength() || node.value >= completed->GetLength() )
	{
		result.status = asAST_TRAVERSAL_INVALID_NODE;
		result.offendingNode = node;
		if( incomingEdge )
			result.offendingEdge = *incomingEdge;
		return result.status;
	}
	if( state.skipCompleted && (*completed)[node.value] != 0 )
		return asAST_TRAVERSAL_OK;
	if( result.nodesEntered >= options.maxNodes )
	{
		result.status = asAST_TRAVERSAL_NODE_BUDGET_EXCEEDED;
		result.offendingNode = node;
		if( incomingEdge )
			result.offendingEdge = *incomingEdge;
		return result.status;
	}
	if( (*active)[node.value] != 0 )
	{
		result.status = asAST_TRAVERSAL_CYCLE_DETECTED;
		result.offendingNode = node;
		if( incomingEdge )
			result.offendingEdge = *incomingEdge;
		return result.status;
	}

	(*active)[node.value] = 1;
	++result.nodesEntered;
	if( depth > result.maxDepthReached )
		result.maxDepthReached = depth;
	if( !visitor.EnterNode(context, node, incomingEdge, depth) )
	{
		(*active)[node.value] = 0;
		result.status = asAST_TRAVERSAL_CALLBACK_ABORTED;
		result.offendingNode = node;
		return result.status;
	}

	const asUINT edgeCount = asCASTGetEdgeCount(context, node);
	for( asUINT i = 0; i < edgeCount; ++i )
	{
		asSASTTraversalEdge edge;
		if( !asCASTGetEdge(context, node, i, edge) )
		{
			(*active)[node.value] = 0;
			result.status = asAST_TRAVERSAL_INVALID_NODE;
			result.offendingNode = node;
			return result.status;
		}
		++result.edgesVisited;
		if( !visitor.VisitEdge(context, edge, depth) )
		{
			(*active)[node.value] = 0;
			result.status = asAST_TRAVERSAL_CALLBACK_ABORTED;
			result.offendingNode = node;
			result.offendingEdge = edge;
			return result.status;
		}
		const bool followsEdge = ((edge.edgeClass == asAST_EDGE_STRUCTURAL && options.followStructuralEdges)
			|| (edge.edgeClass == asAST_EDGE_REFERENCE && options.followReferences))
			&& visitor.ShouldTraverseEdge(context, edge, depth);
		if( (options.validateEdgeTargets || followsEdge) && !asCASTNodeExists(context, edge.target) )
		{
			(*active)[node.value] = 0;
			result.status = asAST_TRAVERSAL_INVALID_NODE;
			result.offendingNode = edge.target;
			result.offendingEdge = edge;
			return result.status;
		}
		if( followsEdge )
		{
			const int status = TraverseNode(
				context,
				edge.target,
				&edge,
				depth + 1,
				visitor,
				options,
				result,
				state);
			if( status != asAST_TRAVERSAL_OK )
			{
				(*active)[node.value] = 0;
				return status;
			}
		}
	}

	if( !visitor.LeaveNode(context, node, incomingEdge, depth) )
	{
		(*active)[node.value] = 0;
		result.status = asAST_TRAVERSAL_CALLBACK_ABORTED;
		result.offendingNode = node;
		return result.status;
	}
	(*active)[node.value] = 0;
	(*completed)[node.value] = 1;
	return asAST_TRAVERSAL_OK;
}
}

bool asCASTNodeExists(const asCASTContext& context, const asSASTNodeRef& node)
{
	switch( node.nodeClass )
	{
	case asAST_NODE_DECL: return context.GetDecl(asASTDeclId(node.value, node.snapshotOwner)) != 0;
	case asAST_NODE_TYPE: return context.GetType(asASTTypeRef(node.value, node.snapshotOwner)) != 0;
	case asAST_NODE_STMT: return context.GetStmt(asASTStmtId(node.value, node.snapshotOwner)) != 0;
	case asAST_NODE_EXPR: return context.GetExpr(asASTExprId(node.value, node.snapshotOwner)) != 0;
	default: return false;
	}
}

asUINT asCASTGetEdgeCount(const asCASTContext& context, const asSASTNodeRef& node)
{
	if( node.nodeClass == asAST_NODE_DECL )
	{
		const asCDecl* decl = context.GetDecl(asASTDeclId(node.value, node.snapshotOwner));
		if( decl == 0 ) return 0;
		return decl->children.GetLength()
			+ (decl->body.IsValid() ? 1u : 0u)
			+ decl->inits.GetLength()
			+ (decl->type.IsValid() ? 1u : 0u)
			+ (decl->parent.IsValid() ? 1u : 0u)
			+ decl->captures.GetLength()
			+ (decl->accessorField.IsValid() ? 1u : 0u)
			+ decl->bases.GetLength()
			+ (decl->accessSpecifier.IsValid() ? 1u : 0u)
			+ decl->methodRelations.GetLength() * 2u;
	}
	if( node.nodeClass == asAST_NODE_STMT )
	{
		const asCStmt* stmt = context.GetStmt(asASTStmtId(node.value, node.snapshotOwner));
		if( stmt == 0 ) return 0;
		return stmt->children.GetLength()
			+ (stmt->expr.IsValid() ? 1u : 0u)
			+ (stmt->owner.IsValid() ? 1u : 0u)
			+ (stmt->target.IsValid() ? 1u : 0u)
			+ (stmt->decl.IsValid() ? 1u : 0u);
	}
	if( node.nodeClass == asAST_NODE_EXPR )
	{
		const asCExpr* expr = context.GetExpr(asASTExprId(node.value, node.snapshotOwner));
		if( expr == 0 ) return 0;
		return (expr->receiver.IsValid() ? 1u : 0u)
			+ expr->children.GetLength()
			+ (expr->type.IsValid() ? 1u : 0u)
			+ (expr->resolvedDecl.IsValid() ? 1u : 0u);
	}
	return 0;
}

bool asCASTGetEdge(const asCASTContext& context, const asSASTNodeRef& node, asUINT edgeIndex, asSASTTraversalEdge& outEdge)
{
	asUINT base = 0;
	if( node.nodeClass == asAST_NODE_DECL )
	{
		const asCDecl* decl = context.GetDecl(asASTDeclId(node.value, node.snapshotOwner));
		if( decl == 0 ) return false;
		asUINT count = decl->children.GetLength();
		if( edgeIndex < base + count ) return GetDeclArrayEdge(node, decl->children, asAST_EDGE_STRUCTURAL, asAST_EDGE_DECL_CHILD, edgeIndex - base, outEdge);
		base += count;
		if( decl->body.IsValid() )
		{
			if( edgeIndex == base ) { outEdge = MakeEdge(node, asSASTNodeRef::FromStmt(decl->body), asAST_EDGE_STRUCTURAL, asAST_EDGE_DECL_BODY, 0); return true; }
			++base;
		}
		count = decl->inits.GetLength();
		if( edgeIndex < base + count ) return GetExprArrayEdge(node, decl->inits, asAST_EDGE_STRUCTURAL, asAST_EDGE_DECL_INIT, edgeIndex - base, outEdge);
		base += count;
		if( decl->type.IsValid() )
		{
			if( edgeIndex == base ) { outEdge = MakeEdge(node, asSASTNodeRef::FromType(decl->type.type), asAST_EDGE_REFERENCE, asAST_EDGE_DECL_TYPE, 0); return true; }
			++base;
		}
		if( decl->parent.IsValid() )
		{
			if( edgeIndex == base ) { outEdge = MakeEdge(node, asSASTNodeRef::FromDecl(decl->parent), asAST_EDGE_REFERENCE, asAST_EDGE_DECL_PARENT, 0); return true; }
			++base;
		}
		count = decl->captures.GetLength();
		if( edgeIndex < base + count ) return GetDeclArrayEdge(node, decl->captures, asAST_EDGE_REFERENCE, asAST_EDGE_DECL_CAPTURE, edgeIndex - base, outEdge);
		base += count;
		if( decl->accessorField.IsValid() )
		{
			if( edgeIndex == base ) { outEdge = MakeEdge(node, asSASTNodeRef::FromDecl(decl->accessorField), asAST_EDGE_REFERENCE, asAST_EDGE_DECL_ACCESSOR_FIELD, 0); return true; }
			++base;
		}
		count = decl->bases.GetLength();
		if( edgeIndex < base + count ) return GetDeclArrayEdge(node, decl->bases, asAST_EDGE_REFERENCE, asAST_EDGE_DECL_BASE, edgeIndex - base, outEdge);
		base += count;
		if( decl->accessSpecifier.IsValid() )
		{
			if( edgeIndex == base ) { outEdge = MakeEdge(node, asSASTNodeRef::FromDecl(decl->accessSpecifier), asAST_EDGE_REFERENCE, asAST_EDGE_DECL_ACCESS_SPECIFIER, 0); return true; }
			++base;
		}
		count = decl->methodRelations.GetLength() * 2u;
		if( edgeIndex < base + count )
		{
			const asUINT relationOffset = edgeIndex - base;
			const asUINT relationIndex = relationOffset / 2u;
			const asSASTMethodRelation& relation =
				decl->methodRelations[relationIndex];
			if( (relationOffset & 1u) == 0 )
			{
				outEdge = MakeEdge(
					node,
					asSASTNodeRef::FromDecl(relation.implementation),
					asAST_EDGE_REFERENCE,
					asAST_EDGE_DECL_METHOD_RELATION_IMPLEMENTATION,
					relationIndex);
			}
			else
			{
				outEdge = MakeEdge(
					node,
					asSASTNodeRef::FromDecl(relation.requirement),
					asAST_EDGE_REFERENCE,
					asAST_EDGE_DECL_METHOD_RELATION_REQUIREMENT,
					relationIndex);
			}
			return true;
		}
		return false;
	}

	if( node.nodeClass == asAST_NODE_STMT )
	{
		const asCStmt* stmt = context.GetStmt(asASTStmtId(node.value, node.snapshotOwner));
		if( stmt == 0 ) return false;
		if( stmt->expr.IsValid() )
		{
			if( edgeIndex == base ) { outEdge = MakeEdge(node, asSASTNodeRef::FromExpr(stmt->expr), asAST_EDGE_STRUCTURAL, asAST_EDGE_STMT_EXPR, 0); return true; }
			++base;
		}
		asUINT count = stmt->children.GetLength();
		if( edgeIndex < base + count ) return GetStmtArrayEdge(node, stmt->children, asAST_EDGE_STRUCTURAL, asAST_EDGE_STMT_CHILD, edgeIndex - base, outEdge);
		base += count;
		if( stmt->owner.IsValid() )
		{
			if( edgeIndex == base ) { outEdge = MakeEdge(node, asSASTNodeRef::FromDecl(stmt->owner), asAST_EDGE_REFERENCE, asAST_EDGE_STMT_OWNER, 0); return true; }
			++base;
		}
		if( stmt->target.IsValid() )
		{
			if( edgeIndex == base ) { outEdge = MakeEdge(node, asSASTNodeRef::FromStmt(stmt->target), asAST_EDGE_REFERENCE, asAST_EDGE_STMT_TARGET, 0); return true; }
			++base;
		}
		if( stmt->decl.IsValid() )
		{
			if( edgeIndex == base ) { outEdge = MakeEdge(node, asSASTNodeRef::FromDecl(stmt->decl), asAST_EDGE_REFERENCE, asAST_EDGE_STMT_DECL, 0); return true; }
		}
		return false;
	}

	if( node.nodeClass == asAST_NODE_EXPR )
	{
		const asCExpr* expr = context.GetExpr(asASTExprId(node.value, node.snapshotOwner));
		if( expr == 0 ) return false;
		if( expr->receiver.IsValid() )
		{
			if( edgeIndex == base ) { outEdge = MakeEdge(node, asSASTNodeRef::FromExpr(expr->receiver), asAST_EDGE_STRUCTURAL, asAST_EDGE_EXPR_RECEIVER, 0); return true; }
			++base;
		}
		asUINT count = expr->children.GetLength();
		if( edgeIndex < base + count ) return GetExprArrayEdge(node, expr->children, asAST_EDGE_STRUCTURAL, asAST_EDGE_EXPR_CHILD, edgeIndex - base, outEdge);
		base += count;
		if( expr->type.IsValid() )
		{
			if( edgeIndex == base ) { outEdge = MakeEdge(node, asSASTNodeRef::FromType(expr->type.type), asAST_EDGE_REFERENCE, asAST_EDGE_EXPR_TYPE, 0); return true; }
			++base;
		}
		if( expr->resolvedDecl.IsValid() )
		{
			if( edgeIndex == base ) { outEdge = MakeEdge(node, asSASTNodeRef::FromDecl(expr->resolvedDecl), asAST_EDGE_REFERENCE, asAST_EDGE_EXPR_RESOLVED_DECL, 0); return true; }
		}
	}
	return false;
}

int asCASTTraverse(
	const asCASTContext& context,
	const asSASTNodeRef& root,
	asIASTConstVisitor& visitor,
	const asSASTTraversalOptions& options,
	asSASTTraversalResult& outResult)
{
	outResult = asSASTTraversalResult();
	asSTraversalState state;
	InitializeState(context, false, state);

	const int status = TraverseNode(
		context,
		root,
		0,
		0,
		visitor,
		options,
		outResult,
		state);
	outResult.status = static_cast<asEASTTraversalStatus>(status);
	return status;
}

int asCASTTraverseAll(
	const asCASTContext& context,
	asIASTConstVisitor& visitor,
	const asSASTTraversalOptions& options,
	asSASTTraversalResult& outResult)
{
	outResult = asSASTTraversalResult();
	asSTraversalState state;
	InitializeState(context, true, state);
	for( asUINT nodeClass = asAST_NODE_DECL; nodeClass <= asAST_NODE_EXPR; ++nodeClass )
	{
		asUINT nodeCount = 0;
		switch( nodeClass )
		{
		case asAST_NODE_DECL: nodeCount = context.GetDeclCount(); break;
		case asAST_NODE_TYPE: nodeCount = context.GetTypeCount(); break;
		case asAST_NODE_STMT: nodeCount = context.GetStmtCount(); break;
		case asAST_NODE_EXPR: nodeCount = context.GetExprCount(); break;
		default: break;
		}
		for( asUINT value = 1; value <= nodeCount; ++value )
		{
			const int status = TraverseNode(
				context,
				asSASTNodeRef(static_cast<asEASTNodeClass>(nodeClass), value),
				0,
				0,
				visitor,
				options,
				outResult,
				state);
			if( status != asAST_TRAVERSAL_OK )
			{
				outResult.status = static_cast<asEASTTraversalStatus>(status);
				return status;
			}
		}
	}
	outResult.status = asAST_TRAVERSAL_OK;
	return asAST_TRAVERSAL_OK;
}

asCASTParentEdgeIndex::asCASTParentEdgeIndex()
	: indexedContext(0)
	, built(false)
{
}

void asCASTParentEdgeIndex::Reset()
{
	indexedContext = 0;
	declRanges.SetLength(0);
	typeRanges.SetLength(0);
	stmtRanges.SetLength(0);
	exprRanges.SetLength(0);
	incomingEdges.SetLength(0);
	built = false;
}

asCASTParentEdgeIndex::asSEdgeRange* asCASTParentEdgeIndex::MutableRange(const asSASTNodeRef& target)
{
	if( !target.IsValid() ) return 0;
	switch( target.nodeClass )
	{
	case asAST_NODE_DECL: return target.value < declRanges.GetLength() ? &declRanges[target.value] : 0;
	case asAST_NODE_TYPE: return target.value < typeRanges.GetLength() ? &typeRanges[target.value] : 0;
	case asAST_NODE_STMT: return target.value < stmtRanges.GetLength() ? &stmtRanges[target.value] : 0;
	case asAST_NODE_EXPR: return target.value < exprRanges.GetLength() ? &exprRanges[target.value] : 0;
	default: return 0;
	}
}

const asCASTParentEdgeIndex::asSEdgeRange* asCASTParentEdgeIndex::GetRange(const asSASTNodeRef& target) const
{
	if( !built || indexedContext == 0 || !asCASTNodeExists(*indexedContext, target) ) return 0;
	if( !target.IsValid() ) return 0;
	switch( target.nodeClass )
	{
	case asAST_NODE_DECL: return target.value < declRanges.GetLength() ? &declRanges[target.value] : 0;
	case asAST_NODE_TYPE: return target.value < typeRanges.GetLength() ? &typeRanges[target.value] : 0;
	case asAST_NODE_STMT: return target.value < stmtRanges.GetLength() ? &stmtRanges[target.value] : 0;
	case asAST_NODE_EXPR: return target.value < exprRanges.GetLength() ? &exprRanges[target.value] : 0;
	default: return 0;
	}
}

int asCASTParentEdgeIndex::Build(const asCASTContext& context)
{
	Reset();
	if( !context.IsPublishable() )
		return asAST_TRAVERSAL_UNSEALED_CONTEXT;

	indexedContext = &context;
	declRanges.SetLength(context.GetDeclCount() + 1);
	typeRanges.SetLength(context.GetTypeCount() + 1);
	stmtRanges.SetLength(context.GetStmtCount() + 1);
	exprRanges.SetLength(context.GetExprCount() + 1);

	asCArray<asSASTTraversalEdge> allEdges;
	for( asUINT nodeClass = asAST_NODE_DECL; nodeClass <= asAST_NODE_EXPR; ++nodeClass )
	{
		asUINT nodeCount = 0;
		switch( nodeClass )
		{
		case asAST_NODE_DECL: nodeCount = context.GetDeclCount(); break;
		case asAST_NODE_TYPE: nodeCount = context.GetTypeCount(); break;
		case asAST_NODE_STMT: nodeCount = context.GetStmtCount(); break;
		case asAST_NODE_EXPR: nodeCount = context.GetExprCount(); break;
		default: break;
		}
		for( asUINT value = 1; value <= nodeCount; ++value )
		{
			const asSASTNodeRef source(static_cast<asEASTNodeClass>(nodeClass), value);
			const asUINT edgeCount = asCASTGetEdgeCount(context, source);
			for( asUINT edgeIndex = 0; edgeIndex < edgeCount; ++edgeIndex )
			{
				asSASTTraversalEdge edge;
				if( !asCASTGetEdge(context, source, edgeIndex, edge) || !asCASTNodeExists(context, edge.target) )
				{
					Reset();
					return asAST_TRAVERSAL_INVALID_NODE;
				}
				asSEdgeRange* range = MutableRange(edge.target);
				if( range == 0 )
				{
					Reset();
					return asAST_TRAVERSAL_INVALID_NODE;
				}
				++range->count;
				allEdges.PushLast(edge);
			}
		}
	}

	asUINT offset = 0;
	asCArray<asSEdgeRange>* rangeTables[] = { &declRanges, &typeRanges, &stmtRanges, &exprRanges };
	for( asUINT tableIndex = 0; tableIndex < 4; ++tableIndex )
	{
		asCArray<asSEdgeRange>& table = *rangeTables[tableIndex];
		for( asUINT value = 1; value < table.GetLength(); ++value )
		{
			table[value].offset = offset;
			offset += table[value].count;
		}
	}
	incomingEdges.SetLength(offset);

	asCArray<asUINT> declCursor(context.GetDeclCount() + 1);
	asCArray<asUINT> typeCursor(context.GetTypeCount() + 1);
	asCArray<asUINT> stmtCursor(context.GetStmtCount() + 1);
	asCArray<asUINT> exprCursor(context.GetExprCount() + 1);
	declCursor.SetLength(context.GetDeclCount() + 1);
	typeCursor.SetLength(context.GetTypeCount() + 1);
	stmtCursor.SetLength(context.GetStmtCount() + 1);
	exprCursor.SetLength(context.GetExprCount() + 1);
	for( asUINT i = 0; i < declCursor.GetLength(); ++i ) declCursor[i] = 0;
	for( asUINT i = 0; i < typeCursor.GetLength(); ++i ) typeCursor[i] = 0;
	for( asUINT i = 0; i < stmtCursor.GetLength(); ++i ) stmtCursor[i] = 0;
	for( asUINT i = 0; i < exprCursor.GetLength(); ++i ) exprCursor[i] = 0;

	for( asUINT i = 0; i < allEdges.GetLength(); ++i )
	{
		const asSASTTraversalEdge& edge = allEdges[i];
		asSEdgeRange* range = MutableRange(edge.target);
		asCArray<asUINT>* cursor = 0;
		switch( edge.target.nodeClass )
		{
		case asAST_NODE_DECL: cursor = &declCursor; break;
		case asAST_NODE_TYPE: cursor = &typeCursor; break;
		case asAST_NODE_STMT: cursor = &stmtCursor; break;
		case asAST_NODE_EXPR: cursor = &exprCursor; break;
		default: break;
		}
		if( range == 0 || cursor == 0 )
		{
			Reset();
			return asAST_TRAVERSAL_INVALID_NODE;
		}
		incomingEdges[range->offset + (*cursor)[edge.target.value]++] = edge;
	}

	built = true;
	return asAST_TRAVERSAL_OK;
}

asUINT asCASTParentEdgeIndex::GetIncomingEdgeCount(const asSASTNodeRef& target) const
{
	const asSEdgeRange* range = GetRange(target);
	return range ? range->count : 0;
}

bool asCASTParentEdgeIndex::GetIncomingEdge(const asSASTNodeRef& target, asUINT incomingIndex, asSASTTraversalEdge& outEdge) const
{
	const asSEdgeRange* range = GetRange(target);
	if( range == 0 || incomingIndex >= range->count ) return false;
	outEdge = incomingEdges[range->offset + incomingIndex];
	return true;
}

bool asCASTParentEdgeIndex::GetStructuralParent(const asSASTNodeRef& target, asSASTTraversalEdge& outEdge) const
{
	const asSEdgeRange* range = GetRange(target);
	if( range == 0 ) return false;
	bool found = false;
	for( asUINT i = 0; i < range->count; ++i )
	{
		const asSASTTraversalEdge& edge = incomingEdges[range->offset + i];
		if( edge.edgeClass != asAST_EDGE_STRUCTURAL ) continue;
		if( found ) return false;
		outEdge = edge;
		found = true;
	}
	return found;
}

END_AS_NAMESPACE
