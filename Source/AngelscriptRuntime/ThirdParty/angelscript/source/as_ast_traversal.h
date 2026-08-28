/*
   Maintained Unreal AngelScript fork — canonical typed AST traversal.
   Internal compiler/debugging API; not public SDK ABI.
*/

#ifndef AS_AST_TRAVERSAL_H
#define AS_AST_TRAVERSAL_H

#include "as_array.h"
#include "as_ast_context.h"

BEGIN_AS_NAMESPACE

enum asEASTNodeClass : asBYTE
{
	asAST_NODE_INVALID = 0,
	asAST_NODE_DECL,
	asAST_NODE_TYPE,
	asAST_NODE_STMT,
	asAST_NODE_EXPR,
};

struct asSASTNodeRef
{
	asEASTNodeClass nodeClass;
	asUINT value;
	asDWORD snapshotOwner;

	asSASTNodeRef()
		: nodeClass(asAST_NODE_INVALID)
		, value(0)
		, snapshotOwner(0)
	{
	}

	asSASTNodeRef(asEASTNodeClass inNodeClass, asUINT inValue, asDWORD inSnapshotOwner = 0)
		: nodeClass(inNodeClass)
		, value(inValue)
		, snapshotOwner(inSnapshotOwner)
	{
	}

	static asSASTNodeRef FromDecl(asASTDeclId id) { return asSASTNodeRef(asAST_NODE_DECL, id.value, id.snapshotOwner); }
	static asSASTNodeRef FromType(asASTTypeRef id) { return asSASTNodeRef(asAST_NODE_TYPE, id.value, id.snapshotOwner); }
	static asSASTNodeRef FromStmt(asASTStmtId id) { return asSASTNodeRef(asAST_NODE_STMT, id.value, id.snapshotOwner); }
	static asSASTNodeRef FromExpr(asASTExprId id) { return asSASTNodeRef(asAST_NODE_EXPR, id.value, id.snapshotOwner); }

	bool IsValid() const { return nodeClass != asAST_NODE_INVALID && value != 0; }
	bool operator==(const asSASTNodeRef& other) const
	{
		return nodeClass == other.nodeClass && value == other.value && snapshotOwner == other.snapshotOwner;
	}
	bool operator!=(const asSASTNodeRef& other) const { return !(*this == other); }
};

enum asEASTEdgeClass : asBYTE
{
	asAST_EDGE_STRUCTURAL = 0,
	asAST_EDGE_REFERENCE,
};

enum asEASTEdgeRole : asBYTE
{
	asAST_EDGE_INVALID = 0,

	// Decl containment.
	asAST_EDGE_DECL_CHILD,
	asAST_EDGE_DECL_BODY,
	asAST_EDGE_DECL_INIT,

	// Stmt containment.
	asAST_EDGE_STMT_CHILD,
	asAST_EDGE_STMT_EXPR,

	// Expr containment.
	asAST_EDGE_EXPR_RECEIVER,
	asAST_EDGE_EXPR_CHILD,

	// Decl references.
	asAST_EDGE_DECL_TYPE,
	asAST_EDGE_DECL_PARENT,
	asAST_EDGE_DECL_CAPTURE,
	asAST_EDGE_DECL_ACCESSOR_FIELD,
	asAST_EDGE_DECL_BASE,

	// Stmt references.
	asAST_EDGE_STMT_OWNER,
	asAST_EDGE_STMT_TARGET,
	asAST_EDGE_STMT_DECL,

	// Expr references.
	asAST_EDGE_EXPR_TYPE,
	asAST_EDGE_EXPR_RESOLVED_DECL,

	// Append-only semantic declaration reference.
	asAST_EDGE_DECL_ACCESS_SPECIFIER,
};

struct asSASTTraversalEdge
{
	asSASTNodeRef source;
	asSASTNodeRef target;
	asEASTEdgeClass edgeClass;
	asEASTEdgeRole role;
	asUINT index;

	asSASTTraversalEdge()
		: edgeClass(asAST_EDGE_REFERENCE)
		, role(asAST_EDGE_INVALID)
		, index(0)
	{
	}
};

enum asEASTTraversalStatus : asBYTE
{
	asAST_TRAVERSAL_OK = 0,
	asAST_TRAVERSAL_INVALID_NODE,
	asAST_TRAVERSAL_DEPTH_LIMIT_EXCEEDED,
	asAST_TRAVERSAL_NODE_BUDGET_EXCEEDED,
	asAST_TRAVERSAL_CYCLE_DETECTED,
	asAST_TRAVERSAL_CALLBACK_ABORTED,
	asAST_TRAVERSAL_UNSEALED_CONTEXT,
};

struct asSASTTraversalOptions
{
	asUINT maxDepth;
	asUINT maxNodes;
	bool followStructuralEdges;
	bool followReferences;
	bool validateEdgeTargets;

	asSASTTraversalOptions()
		: maxDepth(512)
		, maxNodes(100000)
		, followStructuralEdges(true)
		, followReferences(false)
		, validateEdgeTargets(true)
	{
	}
};

struct asSASTTraversalResult
{
	asEASTTraversalStatus status;
	asSASTNodeRef offendingNode;
	asSASTTraversalEdge offendingEdge;
	asUINT nodesEntered;
	asUINT edgesVisited;
	asUINT maxDepthReached;

	asSASTTraversalResult()
		: status(asAST_TRAVERSAL_OK)
		, nodesEntered(0)
		, edgesVisited(0)
		, maxDepthReached(0)
	{
	}
};

class ANGELSCRIPTRUNTIME_API asIASTConstVisitor
{
public:
	virtual ~asIASTConstVisitor() {}
	virtual bool EnterNode(const asCASTContext& context, const asSASTNodeRef& node, const asSASTTraversalEdge* incomingEdge, asUINT depth) = 0;
	virtual bool VisitEdge(const asCASTContext& context, const asSASTTraversalEdge& edge, asUINT depth) = 0;
	virtual bool ShouldTraverseEdge(const asCASTContext&, const asSASTTraversalEdge&, asUINT) { return true; }
	virtual bool LeaveNode(const asCASTContext& context, const asSASTNodeRef& node, const asSASTTraversalEdge* incomingEdge, asUINT depth) = 0;
};

ANGELSCRIPTRUNTIME_API bool asCASTNodeExists(const asCASTContext& context, const asSASTNodeRef& node);
ANGELSCRIPTRUNTIME_API asUINT asCASTGetEdgeCount(const asCASTContext& context, const asSASTNodeRef& node);
ANGELSCRIPTRUNTIME_API bool asCASTGetEdge(const asCASTContext& context, const asSASTNodeRef& node, asUINT edgeIndex, asSASTTraversalEdge& outEdge);
ANGELSCRIPTRUNTIME_API int asCASTTraverse(
	const asCASTContext& context,
	const asSASTNodeRef& root,
	asIASTConstVisitor& visitor,
	const asSASTTraversalOptions& options,
	asSASTTraversalResult& outResult);
ANGELSCRIPTRUNTIME_API int asCASTTraverseAll(
	const asCASTContext& context,
	asIASTConstVisitor& visitor,
	const asSASTTraversalOptions& options,
	asSASTTraversalResult& outResult);

// Built only over a sealed immutable context. It records both structural
// containment and non-owning semantic references, but structural-parent
// queries intentionally ignore reference cycles such as call -> callee decl.
class ANGELSCRIPTRUNTIME_API asCASTParentEdgeIndex
{
public:
	asCASTParentEdgeIndex();

	int Build(const asCASTContext& context);
	bool IsBuilt() const { return built; }
	asUINT GetIncomingEdgeCount(const asSASTNodeRef& target) const;
	bool GetIncomingEdge(const asSASTNodeRef& target, asUINT incomingIndex, asSASTTraversalEdge& outEdge) const;
	bool GetStructuralParent(const asSASTNodeRef& target, asSASTTraversalEdge& outEdge) const;

private:
	struct asSEdgeRange
	{
		asUINT offset;
		asUINT count;
		asSEdgeRange() : offset(0), count(0) {}
	};

	asSEdgeRange* MutableRange(const asSASTNodeRef& target);
	const asSEdgeRange* GetRange(const asSASTNodeRef& target) const;
	void Reset();

	const asCASTContext* indexedContext;
	asCArray<asSEdgeRange> declRanges;
	asCArray<asSEdgeRange> typeRanges;
	asCArray<asSEdgeRange> stmtRanges;
	asCArray<asSEdgeRange> exprRanges;
	asCArray<asSASTTraversalEdge> incomingEdges;
	bool built;
};

END_AS_NAMESPACE

#endif
