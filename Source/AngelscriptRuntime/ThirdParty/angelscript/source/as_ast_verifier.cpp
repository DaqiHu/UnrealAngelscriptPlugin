#include "as_ast_verifier.h"
#include "as_ast_traversal.h"

BEGIN_AS_NAMESPACE

namespace
{
const char* DiagnosticDeclKindName(asEASTDeclKind kind)
{
	switch( kind )
	{
	case asAST_DECL_TRANSLATION_UNIT: return "TranslationUnit";
	case asAST_DECL_NAMESPACE: return "Namespace";
	case asAST_DECL_FUNCTION: return "Function";
	case asAST_DECL_METHOD: return "Method";
	case asAST_DECL_CLASS: return "Class";
	case asAST_DECL_INTERFACE: return "Interface";
	case asAST_DECL_ENUM: return "Enum";
	case asAST_DECL_FUNCDEF: return "FuncDef";
	case asAST_DECL_IMPORT: return "Import";
	case asAST_DECL_VAR: return "Var";
	case asAST_DECL_PARAM: return "Param";
	case asAST_DECL_CONSTRUCTOR: return "Constructor";
	case asAST_DECL_DESTRUCTOR: return "Destructor";
	case asAST_DECL_PROPERTY: return "Property";
	case asAST_DECL_TYPEDEF: return "Typedef";
	case asAST_DECL_MIXIN: return "Mixin";
	case asAST_DECL_ACCESS_SPECIFIER: return "AccessSpecifier";
	case asAST_DECL_ACCESS_PERMISSION: return "AccessPermission";
	default: return "Invalid";
	}
}

const char* DiagnosticStmtKindName(asEASTStmtKind kind)
{
	switch( kind )
	{
	case asAST_STMT_BLOCK: return "Block";
	case asAST_STMT_DECL: return "DeclStmt";
	case asAST_STMT_EXPR: return "ExprStmt";
	case asAST_STMT_IF: return "If";
	case asAST_STMT_FOR: return "For";
	case asAST_STMT_WHILE: return "While";
	case asAST_STMT_DO_WHILE: return "DoWhile";
	case asAST_STMT_SWITCH: return "Switch";
	case asAST_STMT_CASE: return "Case";
	case asAST_STMT_BREAK: return "Break";
	case asAST_STMT_CONTINUE: return "Continue";
	case asAST_STMT_RETURN: return "Return";
	case asAST_STMT_FALLTHROUGH: return "Fallthrough";
	case asAST_STMT_FOREACH: return "ForEach";
	default: return "Invalid";
	}
}

const char* DiagnosticExprKindName(asEASTExprKind kind)
{
	switch( kind )
	{
	case asAST_EXPR_INTEGER_LITERAL: return "IntegerLiteral";
	case asAST_EXPR_FLOAT_LITERAL: return "FloatLiteral";
	case asAST_EXPR_BOOL_LITERAL: return "BoolLiteral";
	case asAST_EXPR_NULL_LITERAL: return "NullLiteral";
	case asAST_EXPR_STRING_LITERAL: return "StringLiteral";
	case asAST_EXPR_DECL_REF: return "DeclRef";
	case asAST_EXPR_MEMBER_REF: return "MemberRef";
	case asAST_EXPR_UNARY: return "Unary";
	case asAST_EXPR_BINARY: return "Binary";
	case asAST_EXPR_LOGICAL: return "Logical";
	case asAST_EXPR_CONDITIONAL: return "Conditional";
	case asAST_EXPR_ASSIGN: return "Assign";
	case asAST_EXPR_CALL: return "Call";
	case asAST_EXPR_CONSTRUCT: return "Construct";
	case asAST_EXPR_CONVERSION: return "Conversion";
	case asAST_EXPR_INDEX: return "Index";
	case asAST_EXPR_SEQUENCE: return "Sequence";
	case asAST_EXPR_MATERIALIZE_TEMPORARY: return "MaterializeTemporary";
	case asAST_EXPR_CLEANUP: return "Cleanup";
	case asAST_EXPR_OPAQUE_VALUE: return "OpaqueValue";
	case asAST_EXPR_ERROR: return "Error";
	case asAST_EXPR_THIS: return "This";
	case asAST_EXPR_DEFERRED_OUT: return "DeferredOut";
	case asAST_EXPR_CALL_REWRITE: return "CallRewrite";
	default: return "Invalid";
	}
}

const char* DiagnosticTypeKindName(asEASTTypeKind kind)
{
	switch( kind )
	{
	case asAST_TYPE_VOID: return "Void";
	case asAST_TYPE_PRIMITIVE: return "Primitive";
	case asAST_TYPE_ENUM: return "Enum";
	case asAST_TYPE_FUNCDEF: return "FuncDef";
	case asAST_TYPE_VALUE_OBJECT: return "ValueObject";
	case asAST_TYPE_REFERENCE_OBJECT: return "ReferenceObject";
	case asAST_TYPE_TEMPLATE: return "Template";
	case asAST_TYPE_ERROR: return "Error";
	case asAST_TYPE_WILDCARD: return "Wildcard";
	default: return "Invalid";
	}
}

const char* DiagnosticNodeKind(const asCASTContext& context, const asSASTNodeRef& node)
{
	switch( node.nodeClass )
	{
	case asAST_NODE_DECL:
		if( const asCDecl* decl = context.GetDecl(asASTDeclId(node.value, node.snapshotOwner)) ) return DiagnosticDeclKindName(decl->kind);
		break;
	case asAST_NODE_TYPE:
		if( const asCType* type = context.GetType(asASTTypeRef(node.value, node.snapshotOwner)) ) return DiagnosticTypeKindName(type->kind);
		break;
	case asAST_NODE_STMT:
		if( const asCStmt* stmt = context.GetStmt(asASTStmtId(node.value, node.snapshotOwner)) ) return DiagnosticStmtKindName(stmt->kind);
		break;
	case asAST_NODE_EXPR:
		if( const asCExpr* expr = context.GetExpr(asASTExprId(node.value, node.snapshotOwner)) ) return DiagnosticExprKindName(expr->kind);
		break;
	default: break;
	}
	return "Invalid";
}

const char* DiagnosticEdgeRoleName(asEASTEdgeRole role)
{
	switch( role )
	{
	case asAST_EDGE_DECL_CHILD: return "decl-child";
	case asAST_EDGE_DECL_BODY: return "decl-body";
	case asAST_EDGE_DECL_INIT: return "decl-init";
	case asAST_EDGE_STMT_CHILD: return "stmt-child";
	case asAST_EDGE_STMT_EXPR: return "stmt-expr";
	case asAST_EDGE_EXPR_RECEIVER: return "expr-receiver";
	case asAST_EDGE_EXPR_CHILD: return "expr-child";
	case asAST_EDGE_DECL_TYPE: return "decl-type";
	case asAST_EDGE_DECL_PARENT: return "decl-parent";
	case asAST_EDGE_DECL_CAPTURE: return "decl-capture";
	case asAST_EDGE_DECL_ACCESSOR_FIELD: return "decl-accessor-field";
	case asAST_EDGE_DECL_BASE: return "decl-base";
	case asAST_EDGE_STMT_OWNER: return "stmt-owner";
	case asAST_EDGE_STMT_TARGET: return "stmt-target";
	case asAST_EDGE_STMT_DECL: return "stmt-decl";
	case asAST_EDGE_EXPR_TYPE: return "expr-type";
	case asAST_EDGE_EXPR_RESOLVED_DECL: return "expr-resolved-decl";
	case asAST_EDGE_DECL_ACCESS_SPECIFIER: return "decl-access-specifier";
	case asAST_EDGE_DECL_METHOD_RELATION_IMPLEMENTATION: return "decl-method-relation-implementation";
	case asAST_EDGE_DECL_METHOD_RELATION_REQUIREMENT: return "decl-method-relation-requirement";
	default: return "none";
	}
}

asCString DiagnosticNodeLabel(const asSASTNodeRef& node)
{
	const char prefix = node.nodeClass == asAST_NODE_DECL ? 'D'
		: node.nodeClass == asAST_NODE_TYPE ? 'T'
		: node.nodeClass == asAST_NODE_STMT ? 'S'
		: node.nodeClass == asAST_NODE_EXPR ? 'E' : 'I';
	asCString label;
	if( node.IsValid() ) label.Format("%c%u", prefix, node.value);
	return label;
}

asCSourceRange DiagnosticNodeRange(const asCASTContext& context, const asSASTNodeRef& node)
{
	if( node.nodeClass == asAST_NODE_DECL )
	{
		const asCDecl* decl = context.GetDecl(asASTDeclId(node.value, node.snapshotOwner));
		return decl ? decl->range : asCSourceRange();
	}
	if( node.nodeClass == asAST_NODE_STMT )
	{
		const asCStmt* stmt = context.GetStmt(asASTStmtId(node.value, node.snapshotOwner));
		return stmt ? stmt->range : asCSourceRange();
	}
	if( node.nodeClass == asAST_NODE_EXPR )
	{
		const asCExpr* expr = context.GetExpr(asASTExprId(node.value, node.snapshotOwner));
		return expr ? expr->range : asCSourceRange();
	}
	return asCSourceRange();
}

struct asSDiagnosticMarks
{
	asCArray<asBYTE> decls;
	asCArray<asBYTE> types;
	asCArray<asBYTE> stmts;
	asCArray<asBYTE> exprs;

	void Initialize(const asCASTContext& context)
	{
		decls.SetLength(context.GetDeclCount() + 1);
		types.SetLength(context.GetTypeCount() + 1);
		stmts.SetLength(context.GetStmtCount() + 1);
		exprs.SetLength(context.GetExprCount() + 1);
		Clear(decls); Clear(types); Clear(stmts); Clear(exprs);
	}
	bool Contains(const asSASTNodeRef& node) const
	{
		const asCArray<asBYTE>* array = Get(node.nodeClass);
		return array && node.value < array->GetLength() && (*array)[node.value] != 0;
	}
	void Set(const asSASTNodeRef& node, bool value)
	{
		asCArray<asBYTE>* array = GetMutable(node.nodeClass);
		if( array && node.value < array->GetLength() ) (*array)[node.value] = value ? 1 : 0;
	}
private:
	static void Clear(asCArray<asBYTE>& array) { for( asUINT i = 0; i < array.GetLength(); ++i ) array[i] = 0; }
	asCArray<asBYTE>* GetMutable(asEASTNodeClass nodeClass)
	{
		switch( nodeClass )
		{
		case asAST_NODE_DECL: return &decls;
		case asAST_NODE_TYPE: return &types;
		case asAST_NODE_STMT: return &stmts;
		case asAST_NODE_EXPR: return &exprs;
		default: return 0;
		}
	}
	const asCArray<asBYTE>* Get(asEASTNodeClass nodeClass) const
	{
		switch( nodeClass )
		{
		case asAST_NODE_DECL: return &decls;
		case asAST_NODE_TYPE: return &types;
		case asAST_NODE_STMT: return &stmts;
		case asAST_NODE_EXPR: return &exprs;
		default: return 0;
		}
	}
};

struct asSDiagnosticPathState
{
	asSDiagnosticMarks active;
	asCArray<asSASTNodeRef> nodes;
	asCArray<asSASTTraversalEdge> edges;
	asUINT remaining;
};

bool FindDiagnosticPath(
	const asCASTContext& context,
	const asSASTNodeRef& current,
	const asSASTNodeRef& target,
	asUINT depth,
	asSDiagnosticPathState& state)
{
	if( state.remaining == 0 || depth > 256 || !asCASTNodeExists(context, current) || state.active.Contains(current) ) return false;
	--state.remaining;
	state.nodes.PushLast(current);
	if( current == target ) return true;
	state.active.Set(current, true);
	const asUINT edgeCount = asCASTGetEdgeCount(context, current);
	for( asUINT i = 0; i < edgeCount; ++i )
	{
		asSASTTraversalEdge edge;
		if( !asCASTGetEdge(context, current, i, edge) || edge.edgeClass != asAST_EDGE_STRUCTURAL || !asCASTNodeExists(context, edge.target) ) continue;
		state.edges.PushLast(edge);
		if( FindDiagnosticPath(context, edge.target, target, depth + 1, state) ) return true;
		state.edges.SetLength(state.edges.GetLength() - 1);
	}
	state.active.Set(current, false);
	state.nodes.SetLength(state.nodes.GetLength() - 1);
	return false;
}

void BuildDiagnosticRootPath(const asCASTContext& context, const asSASTNodeRef& target, asCString& out)
{
	out = "";
	if( !target.IsValid() ) return;
	asSDiagnosticPathState state;
	state.active.Initialize(context);
	state.remaining = context.GetDeclCount() + context.GetTypeCount() + context.GetStmtCount() + context.GetExprCount() + 1;
	const asSASTNodeRef root = asSASTNodeRef::FromDecl(context.GetTranslationUnit());
	if( !FindDiagnosticPath(context, root, target, 0, state) )
	{
		out = "unreachable:";
		out += DiagnosticNodeLabel(target);
		return;
	}
	for( asUINT i = 0; i < state.nodes.GetLength(); ++i )
	{
		if( i )
		{
			out += "/";
			out += DiagnosticEdgeRoleName(state.edges[i - 1].role);
			asCString index;
			index.Format("[%u]/", state.edges[i - 1].index);
			out += index;
		}
		out += DiagnosticNodeLabel(state.nodes[i]);
	}
}

struct asSLocalSubtreeState
{
	asSDiagnosticMarks visited;
	asUINT nodes;
	asUINT edges;
	bool truncated;
	asCString text;

	asSLocalSubtreeState() : nodes(0), edges(0), truncated(false) {}
};

void AppendDiagnosticIndent(asCString& out, asUINT depth)
{
	for( asUINT i = 0; i < depth; ++i ) out += "  ";
}

void AppendDiagnosticLocalNode(
	const asCASTContext& context,
	const asSASTNodeRef& node,
	asUINT depth,
	asSLocalSubtreeState& state)
{
	static const asUINT MaxNodes = 8;
	static const asUINT MaxEdges = 16;
	static const asUINT MaxDepth = 3;
	if( state.nodes >= MaxNodes || depth > MaxDepth ) { state.truncated = true; return; }
	if( state.visited.Contains(node) )
	{
		AppendDiagnosticIndent(state.text, depth);
		state.text += "node="; state.text += DiagnosticNodeLabel(node); state.text += " repeated=true\n";
		return;
	}
	state.visited.Set(node, true);
	++state.nodes;
	AppendDiagnosticIndent(state.text, depth);
	state.text += "node="; state.text += DiagnosticNodeLabel(node);
	state.text += " kind="; state.text += DiagnosticNodeKind(context, node); state.text += "\n";
	const asUINT edgeCount = asCASTGetEdgeCount(context, node);
	for( asUINT i = 0; i < edgeCount; ++i )
	{
		if( state.edges >= MaxEdges ) { state.truncated = true; break; }
		asSASTTraversalEdge edge;
		if( !asCASTGetEdge(context, node, i, edge) ) { state.truncated = true; break; }
		++state.edges;
		AppendDiagnosticIndent(state.text, depth + 1);
		state.text += "edge="; state.text += DiagnosticEdgeRoleName(edge.role);
		asCString edgeText;
		edgeText.Format("[%u] target=", edge.index); state.text += edgeText;
		state.text += DiagnosticNodeLabel(edge.target);
		state.text += " targetKind="; state.text += DiagnosticNodeKind(context, edge.target);
		state.text += edge.edgeClass == asAST_EDGE_STRUCTURAL ? " class=structural\n" : " class=reference\n";
		if( edge.edgeClass == asAST_EDGE_STRUCTURAL && asCASTNodeExists(context, edge.target) )
		{
			if( depth >= MaxDepth || state.nodes >= MaxNodes ) state.truncated = true;
			else AppendDiagnosticLocalNode(context, edge.target, depth + 1, state);
		}
	}
}

void BuildDiagnosticLocalSubtree(const asCASTContext& context, const asSASTNodeRef& root, asCString& out)
{
	out = "";
	if( !root.IsValid() || !asCASTNodeExists(context, root) ) return;
	asSLocalSubtreeState state;
	state.visited.Initialize(context);
	state.text = "DIAGNOSTIC_SUBTREE root=";
	state.text += DiagnosticNodeLabel(root);
	state.text += " maxNodes=8 maxEdges=16 maxDepth=3\n";
	AppendDiagnosticLocalNode(context, root, 0, state);
	if( state.truncated ) state.text += "truncated=true\n";
	out = state.text;
}

void EnrichVerifyResult(const asCASTContext& context, asSAstVerifyResult& result)
{
	if( result.category == asAST_VERIFY_OK ) return;
	result.offendingKind = DiagnosticNodeKind(context, result.offendingNode);
	result.relatedKind = DiagnosticNodeKind(context, result.relatedNode);
	const asCSourceManager& sourceManager = context.GetSourceManager();
	asCSourceRange sourceRange = DiagnosticNodeRange(context, result.offendingNode);
	if( !sourceManager.IsValidRange(sourceRange) ) sourceRange = result.range;
	if( sourceManager.IsValidRange(sourceRange) )
	{
		result.logicalSource = sourceManager.GetLogicalKey(sourceRange.begin.fileID);
		asSSourceLineColumn lineColumn;
		if( sourceManager.GetLineColumn(sourceRange.begin, lineColumn) )
		{
			result.line = lineColumn.row;
			result.column = lineColumn.column;
		}
	}
	BuildDiagnosticRootPath(context, result.offendingNode, result.rootPath);
	BuildDiagnosticLocalSubtree(context, result.offendingNode, result.localSubtree);
}
}

static int Fail(
	asSAstVerifyResult& result,
	asEASTVerifyCategory category,
	const asCSourceRange& range,
	const char* detail,
	const asSASTNodeRef& offendingNode = asSASTNodeRef(),
	asEASTEdgeRole edgeRole = asAST_EDGE_INVALID,
	const asSASTNodeRef& relatedNode = asSASTNodeRef())
{
	result.category = category;
	result.range = range;
	result.detail = detail ? detail : "";
	result.offendingNode = offendingNode;
	result.edgeRole = edgeRole;
	result.relatedNode = relatedNode;
	return (int)category;
}

static bool DeclListsChild(const asCDecl* parent, asASTDeclId child)
{
	if( parent == 0 )
	{
		return false;
	}
	for( asUINT i = 0; i < parent->children.GetLength(); ++i )
	{
		if( parent->children[i] == child )
		{
			return true;
		}
	}
	return false;
}

static bool StmtContains(const asCASTContext& context, asASTStmtId root, asASTStmtId needle)
{
	if( !root.IsValid() || !needle.IsValid() || root == needle )
	{
		return false;
	}

	asCArray<asASTStmtId> stack;
	asCArray<asBYTE> visiting;
	visiting.SetLength(context.GetStmtCount() + 1);
	for( asUINT i = 0; i < visiting.GetLength(); ++i )
	{
		visiting[i] = 0;
	}

	stack.PushLast(root);
	while( stack.GetLength() > 0 )
	{
		const asASTStmtId current = stack[stack.GetLength() - 1];
		stack.SetLength(stack.GetLength() - 1);
		if( !current.IsValid() || current.value >= visiting.GetLength() || visiting[current.value] )
		{
			continue;
		}
		visiting[current.value] = 1;
		const asCStmt* stmt = context.GetStmt(current);
		if( stmt == 0 )
		{
			continue;
		}
		for( asUINT c = 0; c < stmt->children.GetLength(); ++c )
		{
			const asASTStmtId child = stmt->children[c];
			if( child == needle )
			{
				return true;
			}
			stack.PushLast(child);
		}
	}
	return false;
}

static bool StmtSubtreeDeclares(
	const asCASTContext& context, asASTStmtId root, asASTDeclId declaration)
{
	if( !root.IsValid() || !declaration.IsValid() )
	{
		return false;
	}
	asCArray<asASTStmtId> stack;
	stack.PushLast(root);
	asCArray<asBYTE> visited;
	visited.SetLength(context.GetStmtCount() + 1);
	for( asUINT index = 0; index < visited.GetLength(); ++index )
	{
		visited[index] = 0;
	}
	while( stack.GetLength() )
	{
		const asASTStmtId currentId = stack.PopLast();
		if( !currentId.IsValid() || currentId.value >= visited.GetLength()
			|| visited[currentId.value] )
		{
			continue;
		}
		visited[currentId.value] = 1;
		const asCStmt* const current = context.GetStmt(currentId);
		if( current == 0 )
		{
			continue;
		}
		if( current->kind == asAST_STMT_DECL && current->decl == declaration )
		{
			return true;
		}
		for( asUINT childIndex = 0;
			childIndex < current->children.GetLength(); ++childIndex )
		{
			stack.PushLast(current->children[childIndex]);
		}
	}
	return false;
}

class asCASTCycleVisitor : public asIASTConstVisitor
{
public:
	explicit asCASTCycleVisitor(bool inStmtMode) : stmtMode(inStmtMode) {}

	bool EnterNode(const asCASTContext&, const asSASTNodeRef&, const asSASTTraversalEdge*, asUINT) override { return true; }
	bool VisitEdge(const asCASTContext&, const asSASTTraversalEdge&, asUINT) override { return true; }
	bool LeaveNode(const asCASTContext&, const asSASTNodeRef&, const asSASTTraversalEdge*, asUINT) override { return true; }
	bool ShouldTraverseEdge(const asCASTContext&, const asSASTTraversalEdge& edge, asUINT) override
	{
		if( stmtMode )
			return edge.role == asAST_EDGE_STMT_CHILD;
		return edge.role == asAST_EDGE_EXPR_CHILD || edge.role == asAST_EDGE_EXPR_RECEIVER;
	}

private:
	bool stmtMode;
};

static int VerifyStructuralCycles(const asCASTContext& context, bool stmtMode, asSAstVerifyResult& result)
{
	asCASTCycleVisitor visitor(stmtMode);
	asSASTTraversalOptions options;
	options.validateEdgeTargets = false;
	options.maxNodes = context.GetDeclCount() + context.GetTypeCount() + context.GetStmtCount() + context.GetExprCount() + 1;
	options.maxDepth = (stmtMode ? context.GetStmtCount() : context.GetExprCount()) + 1;
	asSASTTraversalResult traversal;
	const int status = asCASTTraverseAll(context, visitor, options, traversal);
	if( status == asAST_TRAVERSAL_OK )
		return asAST_VERIFY_OK;

	asCSourceRange range;
	if( traversal.offendingNode.nodeClass == asAST_NODE_STMT )
	{
		if( const asCStmt* stmt = context.GetStmt(asASTStmtId(traversal.offendingNode.value, traversal.offendingNode.snapshotOwner)) )
			range = stmt->range;
	}
	else if( traversal.offendingNode.nodeClass == asAST_NODE_EXPR )
	{
		if( const asCExpr* expr = context.GetExpr(asASTExprId(traversal.offendingNode.value, traversal.offendingNode.snapshotOwner)) )
			range = expr->range;
	}
	return Fail(
		result,
		asAST_VERIFY_INVALID_CHILD,
		range,
		stmtMode ? "stmt-cycle" : "expr-cycle",
		traversal.offendingNode,
		traversal.offendingEdge.role,
		traversal.offendingEdge.source);
}

static int ExpectedExprChildCount(asEASTExprKind kind)
{
	switch( kind )
	{
	case asAST_EXPR_UNARY:
	case asAST_EXPR_CONVERSION:
	case asAST_EXPR_MATERIALIZE_TEMPORARY:
	case asAST_EXPR_CLEANUP:
	case asAST_EXPR_OPAQUE_VALUE:
	case asAST_EXPR_MEMBER_REF:
	case asAST_EXPR_DEFERRED_OUT:
		return 1;
	case asAST_EXPR_BINARY:
	case asAST_EXPR_LOGICAL:
	case asAST_EXPR_ASSIGN:
		return 2;
	case asAST_EXPR_CONDITIONAL:
		return 3;
	default:
		return -1;
	}
}

static bool IsVerifiedNullLiteral(
	const asCASTContext& context, const asCExpr* expression)
{
	if( expression == 0 || expression->kind != asAST_EXPR_NULL_LITERAL
		|| !expression->type.IsHandle() || expression->type.IsReference() )
	{
		return false;
	}
	const asCType* const type = context.GetType(expression->type.type);
	return type != 0 && type->kind == asAST_TYPE_VOID
		&& type->primitiveToken == ttVoid;
}

static bool IsVerifiedNullableTarget(
	const asCASTContext& context, const asCQualType& target)
{
	const asCType* const type = context.GetType(target.type);
	return type != 0
		&& !target.IsReference()
		// An asOBJ_IMPLICIT_HANDLE source spelling intentionally carries no
		// explicit HANDLE qualifier. REFERENCE_OBJECT is the nominal authority for
		// that nullable representation; requiring the syntactic bit rejects legal
		// `CObj Value = null`. Funcdefs remain explicit owning handles.
		&& (type->kind == asAST_TYPE_REFERENCE_OBJECT
			|| (type->kind == asAST_TYPE_FUNCDEF && target.IsHandle()));
}

static bool IsExactGeneratedGetterCopyPlan(
	const asCASTContext& context,
	const asCDecl* getter,
	const asCDecl* field)
{
	if( getter == 0 || field == 0 || getter->inits.GetLength() != 1 )
	{
		return false;
	}

	const asCExpr* plan = context.GetExpr(getter->inits[0]);
	while( plan != 0
		&& (plan->kind == asAST_EXPR_CLEANUP
			|| plan->kind == asAST_EXPR_MATERIALIZE_TEMPORARY) )
	{
		// These wrappers are a monadic lifetime route. Accepting an extra child
		// would let a backend choose evaluation or cleanup semantics that Sema did
		// not make explicit on the generated accessor declaration.
		if( plan->children.GetLength() != 1 || plan->type != field->type )
		{
			return false;
		}
		plan = context.GetExpr(plan->children[0]);
	}

	if( plan == 0 || plan->kind != asAST_EXPR_CONSTRUCT
		|| plan->type != field->type
		|| plan->valueCategory != asAST_VALUE_PRVALUE
		|| plan->children.GetLength() != 1
		|| !plan->resolvedDecl.IsValid() )
	{
		return false;
	}

	const asCExpr* const source = context.GetExpr(plan->children[0]);
	const asCDecl* const constructor = context.GetDecl(plan->resolvedDecl);
	if( source == 0 || source->kind != asAST_EXPR_DECL_REF
		|| source->resolvedDecl != field->id
		|| source->type != field->type
		|| source->valueCategory != asAST_VALUE_LVALUE
		|| source->children.GetLength() != 0
		|| source->receiver.IsValid()
		|| constructor == 0
		|| constructor->kind != asAST_DECL_CONSTRUCTOR )
	{
		return false;
	}

	// The selected constructor must belong to the canonical field type rather
	// than merely accepting it as an argument. Native type projections may not
	// have a declaration QualType, so compare their stable declaration identity
	// to the canonical type key instead of consulting Runtime pointers.
	const asCDecl* const constructorOwner = context.GetDecl(constructor->parent);
	const asCType* const fieldType = context.GetType(field->type.type);
	if( constructorOwner == 0 || fieldType == 0
		|| fieldType->stableKey.GetLength() == 0
		|| (!constructorOwner->stableKey.Equals(fieldType->stableKey)
			&& !constructorOwner->name.Equals(fieldType->stableKey)) )
	{
		return false;
	}

	const asCDecl* copyParameter = 0;
	asUINT parameterCount = 0;
	for( asUINT childIndex = 0;
		childIndex < constructor->children.GetLength(); ++childIndex )
	{
		const asCDecl* const child =
			context.GetDecl(constructor->children[childIndex]);
		if( child != 0 && child->kind == asAST_DECL_PARAM )
		{
			++parameterCount;
			copyParameter = child;
		}
	}
	return parameterCount == 1 && copyParameter != 0
		&& copyParameter->type.type == field->type.type
		&& copyParameter->type.IsConst()
		&& copyParameter->type.IsReference();
}

static bool IsExactGeneratedSetterAssignmentPlan(
	const asCASTContext& context,
	const asCDecl* setter,
	const asCDecl* field,
	const asCDecl* parameter)
{
	if( setter == 0 || field == 0 || parameter == 0
		|| setter->inits.GetLength() != 1 )
	{
		return false;
	}

	const asCExpr* const assign = context.GetExpr(setter->inits[0]);
	if( assign == 0 || assign->kind != asAST_EXPR_ASSIGN
		|| assign->children.GetLength() != 2
		|| !assign->resolvedDecl.IsValid() )
	{
		return false;
	}

	const asCExpr* const lhs = context.GetExpr(assign->children[0]);
	const asCExpr* const rhs = context.GetExpr(assign->children[1]);
	if( lhs == 0 || rhs == 0
		|| lhs->kind != asAST_EXPR_DECL_REF
		|| rhs->kind != asAST_EXPR_DECL_REF
		|| lhs->resolvedDecl != field->id
		|| rhs->resolvedDecl != parameter->id
		|| lhs->type != field->type
		|| rhs->type != parameter->type
		|| lhs->valueCategory != asAST_VALUE_LVALUE
		|| rhs->valueCategory != asAST_VALUE_LVALUE
		|| lhs->children.GetLength() != 0
		|| rhs->children.GetLength() != 0
		|| lhs->receiver.IsValid() || rhs->receiver.IsValid() )
	{
		return false;
	}

	const asCDecl* const assignmentOperator =
		context.GetDecl(assign->resolvedDecl);
	const asCDecl* const operatorOwner = assignmentOperator
		? context.GetDecl(assignmentOperator->parent) : 0;
	const asCType* const fieldType = context.GetType(field->type.type);
	if( assignmentOperator == 0
		|| assignmentOperator->kind != asAST_DECL_METHOD
		|| !assignmentOperator->name.Equals("opAssign")
		|| operatorOwner == 0 || fieldType == 0
		|| fieldType->stableKey.GetLength() == 0
		|| (!operatorOwner->stableKey.Equals(fieldType->stableKey)
			&& !operatorOwner->name.Equals(fieldType->stableKey)) )
	{
		return false;
	}

	const asCDecl* operatorParameter = 0;
	asUINT operatorParameterCount = 0;
	for( asUINT childIndex = 0;
		childIndex < assignmentOperator->children.GetLength(); ++childIndex )
	{
		const asCDecl* const child =
			context.GetDecl(assignmentOperator->children[childIndex]);
		if( child != 0 && child->kind == asAST_DECL_PARAM )
		{
			++operatorParameterCount;
			operatorParameter = child;
		}
	}
	const asDWORD direction = operatorParameter != 0
		? (operatorParameter->type.quals & asAST_QUAL_PARAM_DIR_MASK) : 0;
	return operatorParameterCount == 1 && operatorParameter != 0
		&& operatorParameter->type.type == field->type.type
		&& operatorParameter->type.IsConst()
		&& operatorParameter->type.IsReference()
		// `const T&` without an authored direction is the maintained source
		// spelling used by generated Delegates. Preserve that absence in the
		// Canonical signature; Runtime may normalize its current ABI to inout.
		// Explicit in/inout are also valid, while a pure out operator is not a
		// source-value assignment route.
		&& (direction == 0 || (direction & asAST_QUAL_IN) != 0);
}

static int VerifyPublishedCallArguments(
	const asCASTContext& context,
	const asCExpr& call,
	asSAstVerifyResult& result)
{
	const asCDecl* const callee = context.GetDecl(call.resolvedDecl);
	if( callee == 0 )
	{
		// The enclosing publication pass owns the stable call-decl diagnostic.
		return asAST_VERIFY_OK;
	}

	asCArray<asASTDeclId> directFormals;
	if( call.callDispatch != asAST_CALL_DISPATCH_INDIRECT )
	{
		const asUINT formalCount = context.GetFormalDeclCount(callee->id);
		for( asUINT formalIndex = 0; formalIndex < formalCount; ++formalIndex )
		{
			directFormals.PushLast(asASTDeclId());
		}
		for( asUINT childIndex = 0;
			childIndex < callee->children.GetLength(); ++childIndex )
		{
			const asCDecl* const child =
				context.GetDecl(callee->children[childIndex]);
			if( child != 0 && child->kind == asAST_DECL_PARAM )
			{
				if( child->formalIndex >= directFormals.GetLength()
					|| directFormals[child->formalIndex].IsValid() )
				{
					return Fail(result, asAST_VERIFY_INVALID_CHILD,
						child->range, "call-formal-index",
						asSASTNodeRef::FromExpr(call.id),
						asAST_EDGE_EXPR_RESOLVED_DECL,
						asSASTNodeRef::FromDecl(child->id));
				}
				directFormals[child->formalIndex] = child->id;
			}
		}
	}

	// Canonical CALL/CONSTRUCT arguments cover source-level formal slots exactly.
	// CONSTRUCT stores its evaluated arguments in forward-formal order; CALL
	// stores them in reverse-formal order. In both cases each relation record is
	// aligned with the child at the same storage index and authenticates the
	// exact formal ordinal independently of that physical order. An
	// ordinary method may retain its effective receiver as the trailing reverse
	// child, but that receiver is not a formal.  A mixin/free-function receiver
	// is a declared formal and therefore stays in both arrays.  Lambda captures
	// and hidden return pointers are Runtime ABI operands, not CALL children.
	const bool hasTrailingNonFormalReceiver = call.receiver.IsValid()
		&& call.children.GetLength() != 0
		&& call.children[call.children.GetLength() - 1] == call.receiver
		&& (call.callDispatch == asAST_CALL_DISPATCH_INDIRECT
			|| callee->kind == asAST_DECL_METHOD
			// Base/delegating constructor plans carry the destination `this`
			// through the receiver edge and retain it as the trailing reverse
			// child. It is an ABI receiver, not a declared constructor formal.
			// Destructor lifetime plans use CLEANUP nodes instead of CALL, so do
			// not broaden this exception beyond the produced call shape.
			|| callee->kind == asAST_DECL_CONSTRUCTOR);
	const asUINT formalChildCount = call.children.GetLength()
		- (hasTrailingNonFormalReceiver ? 1u : 0u);
	const asUINT expectedArgumentCount =
		call.callDispatch == asAST_CALL_DISPATCH_INDIRECT
			? formalChildCount : directFormals.GetLength();
	if( call.callArguments.GetLength() != expectedArgumentCount
		|| formalChildCount != expectedArgumentCount )
	{
		return Fail(result, asAST_VERIFY_INVALID_CHILD,
			call.range, "call-argument-count",
			asSASTNodeRef::FromExpr(call.id),
			asAST_EDGE_EXPR_CHILD);
	}
	if( expectedArgumentCount == 0 )
	{
		return asAST_VERIFY_OK;
	}

	asUINT authoredArgumentCount = 0;
	for( asUINT argumentIndex = 0;
		argumentIndex < call.callArguments.GetLength(); ++argumentIndex )
	{
		const asSASTCallArgument& argument =
			call.callArguments[argumentIndex];
		if( argumentIndex >= call.children.GetLength()
			|| !argument.expression.IsValid()
			|| context.GetExpr(argument.expression) == 0
			|| argument.expression != call.children[argumentIndex] )
		{
			return Fail(result, asAST_VERIFY_INVALID_CHILD,
				argument.range, "call-argument-expression",
				asSASTNodeRef::FromExpr(call.id),
				asAST_EDGE_EXPR_CHILD,
				asSASTNodeRef::FromExpr(argument.expression));
		}
		if( (argument.range.begin.IsValid() || argument.range.end.IsValid())
			&& !context.GetSourceManager().IsValidRange(argument.range) )
		{
			return Fail(result, asAST_VERIFY_INVALID_RANGE,
				argument.range, "call-argument-range",
				asSASTNodeRef::FromExpr(call.id),
				asAST_EDGE_EXPR_CHILD,
				asSASTNodeRef::FromExpr(argument.expression));
		}
		if( argument.origin < asAST_CALL_ARGUMENT_POSITIONAL
			|| argument.origin > asAST_CALL_ARGUMENT_GENERATED )
		{
			return Fail(result, asAST_VERIFY_INVALID_CHILD,
				argument.range, "call-argument-origin",
				asSASTNodeRef::FromExpr(call.id),
				asAST_EDGE_EXPR_CHILD,
				asSASTNodeRef::FromExpr(argument.expression));
		}

		const bool sourceBacked =
			argument.origin == asAST_CALL_ARGUMENT_POSITIONAL
			|| argument.origin == asAST_CALL_ARGUMENT_NAMED;
		const bool validSourceProvenance = sourceBacked
			? argument.sourceOrdinal != asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL
				&& (argument.origin != asAST_CALL_ARGUMENT_NAMED
					|| argument.authoredName.GetLength() != 0)
				&& (argument.origin != asAST_CALL_ARGUMENT_POSITIONAL
					|| argument.authoredName.GetLength() == 0)
			: argument.sourceOrdinal == asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL
				&& argument.authoredName.GetLength() == 0;
		if( !validSourceProvenance )
		{
			return Fail(result, asAST_VERIFY_INVALID_CHILD,
				argument.range, "call-argument-provenance",
				asSASTNodeRef::FromExpr(call.id),
				asAST_EDGE_EXPR_CHILD,
				asSASTNodeRef::FromExpr(argument.expression));
		}
		if( sourceBacked )
		{
			for( asUINT previousIndex = 0;
				previousIndex < argumentIndex; ++previousIndex )
			{
				const asSASTCallArgument& previous =
					call.callArguments[previousIndex];
				const bool previousSourceBacked =
					previous.origin == asAST_CALL_ARGUMENT_POSITIONAL
					|| previous.origin == asAST_CALL_ARGUMENT_NAMED;
				if( previousSourceBacked
					&& previous.sourceOrdinal == argument.sourceOrdinal )
				{
					return Fail(result, asAST_VERIFY_INVALID_CHILD,
						argument.range, "call-argument-source-order",
						asSASTNodeRef::FromExpr(call.id),
						asAST_EDGE_EXPR_CHILD,
						asSASTNodeRef::FromExpr(argument.expression));
				}
			}
			++authoredArgumentCount;
		}

		if( call.callDispatch == asAST_CALL_DISPATCH_INDIRECT )
		{
			// An indirect funcdef invocation has no ParamDecl owned by the
			// VAR/PARAM callee. Its durable authority is the pointer-free
			// formal ordinal and canonical formal type captured by Sema.
			if( argument.formal.IsValid() )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD,
					argument.range, "call-argument-formal",
					asSASTNodeRef::FromExpr(call.id),
					asAST_EDGE_EXPR_RESOLVED_DECL,
					asSASTNodeRef::FromDecl(argument.formal));
			}
			if( argument.formalIndex >= expectedArgumentCount )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD,
					argument.range, "call-argument-formal",
					asSASTNodeRef::FromExpr(call.id),
					asAST_EDGE_EXPR_RESOLVED_DECL,
					asSASTNodeRef::FromDecl(argument.formal));
			}
		}
		else
		{
			const asASTDeclId expectedFormal =
				argument.formalIndex < directFormals.GetLength()
					? directFormals[argument.formalIndex]
					: asASTDeclId();
			if( !expectedFormal.IsValid()
				|| !argument.formal.IsValid()
				|| argument.formal != expectedFormal )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD,
					argument.range, "call-argument-formal",
					asSASTNodeRef::FromExpr(call.id),
					asAST_EDGE_EXPR_RESOLVED_DECL,
					asSASTNodeRef::FromDecl(argument.formal));
			}
		}

		for( asUINT previousIndex = 0;
			previousIndex < argumentIndex; ++previousIndex )
		{
			const asSASTCallArgument& previous =
				call.callArguments[previousIndex];
			if( previous.formalIndex == argument.formalIndex
				|| (argument.formal.IsValid()
					&& previous.formal == argument.formal) )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD,
					argument.range, "call-argument-duplicate-formal",
					asSASTNodeRef::FromExpr(call.id),
					asAST_EDGE_EXPR_RESOLVED_DECL,
					asSASTNodeRef::FromDecl(argument.formal));
			}
		}

		if( !argument.formalType.IsValid()
			|| argument.formalType.type.snapshotOwner != 0
			|| context.GetType(argument.formalType.type) == 0
			|| !asASTQualifiersAreValid(argument.formalType.quals) )
		{
			return Fail(result, asAST_VERIFY_INVALID_TYPE,
				argument.range, "call-argument-formal-type",
				asSASTNodeRef::FromExpr(call.id),
				asAST_EDGE_EXPR_TYPE,
				asSASTNodeRef::FromType(argument.formalType.type));
		}
		if( argument.formal.IsValid() )
		{
			const asCDecl* const formal = context.GetDecl(argument.formal);
			if( formal == 0 || formal->kind != asAST_DECL_PARAM
				|| formal->formalIndex != argument.formalIndex
				|| formal->type.type != argument.formalType.type
				|| formal->type.quals != argument.formalType.quals )
			{
				return Fail(result, asAST_VERIFY_INVALID_TYPE,
					argument.range, "call-argument-formal-type",
					asSASTNodeRef::FromExpr(call.id),
					asAST_EDGE_EXPR_TYPE,
					asSASTNodeRef::FromType(argument.formalType.type));
			}
			const bool formalIsHidden = formal->origin.Equals("hidden");
			if( argument.origin == asAST_CALL_ARGUMENT_NAMED
				&& !formal->name.Equals(argument.authoredName.AddressOf()) )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD,
					argument.range, "call-argument-provenance",
					asSASTNodeRef::FromExpr(call.id),
					asAST_EDGE_EXPR_RESOLVED_DECL,
					asSASTNodeRef::FromDecl(argument.formal));
			}
			if( (argument.origin == asAST_CALL_ARGUMENT_HIDDEN
					&& !formalIsHidden)
				|| (formalIsHidden
					&& argument.origin != asAST_CALL_ARGUMENT_HIDDEN
					&& argument.origin != asAST_CALL_ARGUMENT_GENERATED) )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD,
					argument.range, "call-argument-hidden",
					asSASTNodeRef::FromExpr(call.id),
					asAST_EDGE_EXPR_RESOLVED_DECL,
					asSASTNodeRef::FromDecl(argument.formal));
			}
			if( argument.origin == asAST_CALL_ARGUMENT_DEFAULT
				&& formal->defaultArg.GetLength() == 0
				&& formal->inits.GetLength() == 0 )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD,
					argument.range, "call-argument-default",
					asSASTNodeRef::FromExpr(call.id),
					asAST_EDGE_EXPR_RESOLVED_DECL,
					asSASTNodeRef::FromDecl(argument.formal));
			}
		}
		if( argument.origin == asAST_CALL_ARGUMENT_IMPLICIT_RECEIVER
			&& (call.callDispatch == asAST_CALL_DISPATCH_INDIRECT
				|| argument.formalIndex != 0
				|| !call.receiver.IsValid()
				|| argument.expression != call.receiver) )
		{
			return Fail(result, asAST_VERIFY_INVALID_CHILD,
				argument.range, "call-argument-receiver",
				asSASTNodeRef::FromExpr(call.id),
				asAST_EDGE_EXPR_RECEIVER,
				asSASTNodeRef::FromExpr(argument.expression));
		}
	}
	for( asUINT argumentIndex = 0;
		argumentIndex < call.callArguments.GetLength(); ++argumentIndex )
	{
		const asSASTCallArgument& argument =
			call.callArguments[argumentIndex];
		if( (argument.origin == asAST_CALL_ARGUMENT_POSITIONAL
				|| argument.origin == asAST_CALL_ARGUMENT_NAMED)
			&& argument.sourceOrdinal >= authoredArgumentCount )
		{
			return Fail(result, asAST_VERIFY_INVALID_CHILD,
				argument.range, "call-argument-source-order",
				asSASTNodeRef::FromExpr(call.id),
				asAST_EDGE_EXPR_CHILD,
				asSASTNodeRef::FromExpr(argument.expression));
		}
	}
	return asAST_VERIFY_OK;
}

static int VerifyPublishedCallRewrite(
	const asCASTContext& context,
	const asCExpr& rewrite,
	asSAstVerifyResult& result)
{
	const asCDecl* const callee = context.GetDecl(rewrite.resolvedDecl);
	const asDWORD dispositionMask =
		asAST_TRAIT_COMPILE_OUT_ENTIRELY
		| asAST_TRAIT_REPLACE_WITH_FIRST_PARAM
		| asAST_TRAIT_COMPILE_OUT_AS_METHOD_CHAIN;
	if( callee == 0
		|| (callee->kind != asAST_DECL_FUNCTION
			&& callee->kind != asAST_DECL_METHOD
			&& callee->kind != asAST_DECL_MIXIN) )
	{
		return Fail(result, asAST_VERIFY_INVALID_CHILD,
			rewrite.range, "call-rewrite-callee",
			asSASTNodeRef::FromExpr(rewrite.id),
			asAST_EDGE_EXPR_RESOLVED_DECL,
			asSASTNodeRef::FromDecl(rewrite.resolvedDecl));
	}
	const asDWORD disposition = callee->traits & dispositionMask;
	const bool entirely = rewrite.literal.Equals("compile-out-entirely");
	const bool first = rewrite.literal.Equals("compile-out-first-param");
	const bool methodChain = rewrite.literal.Equals("compile-out-method-chain");
	const asDWORD expectedDisposition = entirely
		? asAST_TRAIT_COMPILE_OUT_ENTIRELY
		: (first ? asAST_TRAIT_REPLACE_WITH_FIRST_PARAM
			: (methodChain ? asAST_TRAIT_COMPILE_OUT_AS_METHOD_CHAIN : 0));
	if( expectedDisposition == 0 || disposition != expectedDisposition
		|| rewrite.callDispatch != asAST_CALL_DISPATCH_NONE
		|| rewrite.safePointRole != asAST_SAFEPOINT_NONE )
	{
		return Fail(result, asAST_VERIFY_INVALID_CHILD,
			rewrite.range, "call-rewrite-disposition",
			asSASTNodeRef::FromExpr(rewrite.id),
			asAST_EDGE_EXPR_RESOLVED_DECL,
			asSASTNodeRef::FromDecl(rewrite.resolvedDecl));
	}

	if( entirely )
	{
		const asCType* const resultType = context.GetType(rewrite.type.type);
		if( resultType == 0 || resultType->kind != asAST_TYPE_VOID
			|| rewrite.type.quals != 0
			|| rewrite.valueCategory != asAST_VALUE_PRVALUE
			|| rewrite.receiver.IsValid()
			|| rewrite.children.GetLength() != 0
			|| rewrite.callArguments.GetLength() != 0 )
		{
			return Fail(result, asAST_VERIFY_INVALID_CHILD,
				rewrite.range, "call-rewrite-entirely-shape",
				asSASTNodeRef::FromExpr(rewrite.id));
		}
		return asAST_VERIFY_OK;
	}

	if( rewrite.children.GetLength() != 1
		|| rewrite.callArguments.GetLength() != 1 )
	{
		return Fail(result, asAST_VERIFY_INVALID_CHILD,
			rewrite.range, "call-rewrite-value-shape",
			asSASTNodeRef::FromExpr(rewrite.id),
			asAST_EDGE_EXPR_CHILD);
	}
	const asCExpr* const retained = context.GetExpr(rewrite.children[0]);
	const asSASTCallArgument& argument = rewrite.callArguments[0];
	if( retained == 0 || argument.expression != retained->id
		|| rewrite.type != retained->type
		|| rewrite.valueCategory != retained->valueCategory
		|| !argument.formalType.IsValid()
		|| context.GetType(argument.formalType.type) == 0 )
	{
		return Fail(result, asAST_VERIFY_INVALID_TYPE,
			rewrite.range, "call-rewrite-retained-value",
			asSASTNodeRef::FromExpr(rewrite.id),
			asAST_EDGE_EXPR_CHILD,
			asSASTNodeRef::FromExpr(rewrite.children[0]));
	}

	if( first )
	{
		const asCDecl* const formal = context.GetFormalDecl(callee->id, 0);
		const bool sourceOrigin =
			argument.origin == asAST_CALL_ARGUMENT_POSITIONAL
			|| argument.origin == asAST_CALL_ARGUMENT_NAMED;
		if( rewrite.receiver.IsValid() || formal == 0
			|| argument.formal != formal->id
			|| argument.formalIndex != 0
			|| argument.formalType != formal->type
			|| !sourceOrigin
			|| argument.sourceOrdinal
				== asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL
			|| (argument.origin == asAST_CALL_ARGUMENT_NAMED
				&& (!formal->name.Equals(argument.authoredName)
					|| argument.authoredName.GetLength() == 0))
			|| (argument.origin == asAST_CALL_ARGUMENT_POSITIONAL
				&& argument.authoredName.GetLength() != 0) )
		{
			return Fail(result, asAST_VERIFY_INVALID_CHILD,
				rewrite.range, "call-rewrite-first-param",
				asSASTNodeRef::FromExpr(rewrite.id),
				asAST_EDGE_EXPR_RESOLVED_DECL,
				asSASTNodeRef::FromDecl(argument.formal));
		}
		return asAST_VERIFY_OK;
	}

	if( callee->kind != asAST_DECL_METHOD
		|| !rewrite.receiver.IsValid()
		|| rewrite.receiver != retained->id
		|| argument.formal.IsValid()
		|| argument.formalIndex != asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL
		|| argument.sourceOrdinal != asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL
		|| argument.origin != asAST_CALL_ARGUMENT_IMPLICIT_RECEIVER
		|| argument.authoredName.GetLength() != 0
		|| argument.formalType != retained->type )
	{
		return Fail(result, asAST_VERIFY_INVALID_CHILD,
			rewrite.range, "call-rewrite-method-chain",
			asSASTNodeRef::FromExpr(rewrite.id),
			asAST_EDGE_EXPR_RECEIVER,
			asSASTNodeRef::FromExpr(rewrite.receiver));
	}
	return asAST_VERIFY_OK;
}

static int VerifySealedRequiredTargets(const asCASTContext& context, asSAstVerifyResult& result)
{
	for( asUINT i = 1; i <= context.GetExprCount(); ++i )
	{
		const asCExpr* const expr = context.GetExpr(asASTExprId(i));
		if( expr == 0 )
		{
			// asCASTVerify() owns generic table-integrity diagnostics. This helper
			// only makes the sealed-publication semantic requirement explicit.
			continue;
		}
		const char* detail = 0;
		switch( expr->kind )
		{
		case asAST_EXPR_CALL:
			detail = "call-decl";
			break;
		case asAST_EXPR_CONSTRUCT:
			detail = "construct-decl";
			break;
		case asAST_EXPR_CALL_REWRITE:
			detail = "call-rewrite-decl";
			break;
		case asAST_EXPR_DECL_REF:
			detail = "declref-decl";
			break;
		default:
			break;
		}
		if( detail && (!expr->resolvedDecl.IsValid() || context.GetDecl(expr->resolvedDecl) == 0) )
		{
			return Fail(
				result,
				asAST_VERIFY_DANGLING_ID,
				expr->range,
				detail,
				asSASTNodeRef::FromExpr(expr->id),
				asAST_EDGE_EXPR_RESOLVED_DECL,
				asSASTNodeRef::FromDecl(expr->resolvedDecl));
		}
		if( expr->kind == asAST_EXPR_CALL
			&& expr->callDispatch == asAST_CALL_DISPATCH_NONE )
		{
			return Fail(
				result,
				asAST_VERIFY_INVALID_CHILD,
				expr->range,
				"call-dispatch",
				asSASTNodeRef::FromExpr(expr->id));
		}
		if( expr->kind == asAST_EXPR_CALL_REWRITE )
		{
			const int rewrite =
				VerifyPublishedCallRewrite(context, *expr, result);
			if( rewrite != asAST_VERIFY_OK )
			{
				return rewrite;
			}
		}
		if( expr->kind == asAST_EXPR_CALL
			|| (expr->kind == asAST_EXPR_CONSTRUCT
				&& !expr->literal.Equals("list-pattern")) )
		{
			const int callArguments =
				VerifyPublishedCallArguments(context, *expr, result);
			if( callArguments != asAST_VERIFY_OK )
			{
				return callArguments;
			}
		}
	}
	return asAST_VERIFY_OK;
}

static bool MethodRelationContainsDecl(
	const asCArray<asASTDeclId>& declarations,
	asASTDeclId declaration)
{
	for( asUINT index = 0; index < declarations.GetLength(); ++index )
	{
		if( declarations[index] == declaration )
		{
			return true;
		}
	}
	return false;
}

static bool IsCanonicalNativeTypeView(const asCDecl* declaration)
{
	return declaration != 0
		&& declaration->kind == asAST_DECL_CLASS
		&& declaration->origin.Equals("canonical-native-type-view");
}

static bool MethodRelationQualTypeIsValid(
	const asCASTContext& context,
	const asCQualType& type)
{
	return type.IsValid()
		&& type.type.snapshotOwner == 0
		&& context.GetType(type.type) != 0
		&& asASTQualifiersAreValid(type.quals);
}

static bool MethodRelationSignatureTypesAreValid(
	const asCASTContext& context,
	const asCDecl* method)
{
	if( method == 0 || method->kind != asAST_DECL_METHOD
		|| !MethodRelationQualTypeIsValid(context, method->type) )
	{
		return false;
	}
	for( asUINT childIndex = 0;
		childIndex < method->children.GetLength(); ++childIndex )
	{
		const asCDecl* const child =
			context.GetDecl(method->children[childIndex]);
		if( child != 0 && child->kind == asAST_DECL_PARAM
			&& !MethodRelationQualTypeIsValid(context, child->type) )
		{
			return false;
		}
	}
	return true;
}

static bool MethodRelationSignaturesEqual(
	const asCASTContext& context,
	const asCDecl* lhs,
	const asCDecl* rhs)
{
	if( lhs == 0 || rhs == 0
		|| lhs->kind != asAST_DECL_METHOD
		|| rhs->kind != asAST_DECL_METHOD
		|| !lhs->name.Equals(rhs->name)
		|| lhs->type != rhs->type
		|| ((lhs->traits & asAST_TRAIT_CONST_METHOD) != 0)
			!= ((rhs->traits & asAST_TRAIT_CONST_METHOD) != 0) )
	{
		return false;
	}

	const asUINT lhsCount = context.GetFormalDeclCount(lhs->id);
	const asUINT rhsCount = context.GetFormalDeclCount(rhs->id);
	if( lhsCount != rhsCount )
	{
		return false;
	}
	for( asUINT formalIndex = 0; formalIndex < lhsCount; ++formalIndex )
	{
		const asCDecl* const lhsParameter =
			context.GetFormalDecl(lhs->id, formalIndex);
		const asCDecl* const rhsParameter =
			context.GetFormalDecl(rhs->id, formalIndex);
		if( lhsParameter == 0 || rhsParameter == 0 )
		{
			return false;
		}
		if( lhsParameter->type != rhsParameter->type )
		{
			return false;
		}
	}
	return true;
}

static bool MethodRelationInterfaceReaches(
	const asCASTContext& context,
	asASTDeclId start,
	asASTDeclId target)
{
	asCArray<asASTDeclId> pending;
	asCArray<asASTDeclId> visited;
	pending.PushLast(start);
	for( asUINT pendingIndex = 0;
		pendingIndex < pending.GetLength(); ++pendingIndex )
	{
		const asCDecl* const candidate =
			context.GetDecl(pending[pendingIndex]);
		if( candidate == 0 || candidate->kind != asAST_DECL_INTERFACE )
		{
			continue;
		}
		if( candidate->id == target )
		{
			return true;
		}
		if( MethodRelationContainsDecl(visited, candidate->id) )
		{
			continue;
		}
		visited.PushLast(candidate->id);
		for( asUINT baseIndex = 0;
			baseIndex < candidate->bases.GetLength(); ++baseIndex )
		{
			pending.PushLast(candidate->bases[baseIndex]);
		}
	}
	return false;
}

static bool MethodRelationClassReaches(
	const asCASTContext& context,
	asASTDeclId start,
	asASTDeclId target)
{
	asCArray<asASTDeclId> pending;
	asCArray<asASTDeclId> visited;
	pending.PushLast(start);
	for( asUINT pendingIndex = 0;
		pendingIndex < pending.GetLength(); ++pendingIndex )
	{
		const asCDecl* const candidate =
			context.GetDecl(pending[pendingIndex]);
		if( candidate == 0 || candidate->kind != asAST_DECL_CLASS
			|| IsCanonicalNativeTypeView(candidate) )
		{
			continue;
		}
		if( candidate->id == target )
		{
			return true;
		}
		if( MethodRelationContainsDecl(visited, candidate->id) )
		{
			continue;
		}
		visited.PushLast(candidate->id);
		for( asUINT baseIndex = 0;
			baseIndex < candidate->bases.GetLength(); ++baseIndex )
		{
			const asCDecl* const base =
				context.GetDecl(candidate->bases[baseIndex]);
			if( base != 0 && base->kind == asAST_DECL_CLASS
				&& !IsCanonicalNativeTypeView(base) )
			{
				pending.PushLast(base->id);
			}
		}
	}
	return false;
}

static int VerifyMethodRelationClassAncestry(
	const asCASTContext& context,
	const asCDecl& owner,
	asSAstVerifyResult& result)
{
	if( owner.kind != asAST_DECL_CLASS
		&& owner.kind != asAST_DECL_INTERFACE )
	{
		return asAST_VERIFY_OK;
	}

	asASTDeclId directClass;
	for( asUINT baseIndex = 0;
		baseIndex < owner.bases.GetLength(); ++baseIndex )
	{
		const asCDecl* const base = context.GetDecl(owner.bases[baseIndex]);
		if( base == 0 || base->kind != asAST_DECL_CLASS
			|| IsCanonicalNativeTypeView(base) )
		{
			continue;
		}
		if( owner.kind == asAST_DECL_INTERFACE )
		{
			return Fail(result, asAST_VERIFY_WRONG_KIND, owner.range,
				"method-relation-interface-base-kind",
				asSASTNodeRef::FromDecl(owner.id),
				asAST_EDGE_DECL_BASE,
				asSASTNodeRef::FromDecl(base->id));
		}
		if( directClass.IsValid() )
		{
			return Fail(result, asAST_VERIFY_INVALID_CHILD, owner.range,
				"method-relation-class-ancestry-multiple-bases",
				asSASTNodeRef::FromDecl(owner.id),
				asAST_EDGE_DECL_BASE,
				asSASTNodeRef::FromDecl(base->id));
		}
		directClass = base->id;
	}

	if( owner.kind == asAST_DECL_CLASS
		&& directClass.IsValid()
		&& MethodRelationClassReaches(context, directClass, owner.id) )
	{
		return Fail(result, asAST_VERIFY_INVALID_CHILD, owner.range,
			"method-relation-class-ancestry-cycle",
			asSASTNodeRef::FromDecl(owner.id),
			asAST_EDGE_DECL_BASE,
			asSASTNodeRef::FromDecl(directClass));
	}
	return asAST_VERIFY_OK;
}

static int VerifyMethodRelationInterfaceAncestry(
	const asCASTContext& context,
	const asCDecl& owner,
	asSAstVerifyResult& result)
{
	if( owner.kind != asAST_DECL_CLASS
		&& owner.kind != asAST_DECL_INTERFACE )
	{
		return asAST_VERIFY_OK;
	}
	asCArray<asASTDeclId> directInterfaces;
	for( asUINT baseIndex = 0;
		baseIndex < owner.bases.GetLength(); ++baseIndex )
	{
		const asCDecl* const base = context.GetDecl(owner.bases[baseIndex]);
		if( base == 0 || base->kind != asAST_DECL_INTERFACE )
		{
			continue;
		}
		if( MethodRelationContainsDecl(directInterfaces, base->id) )
		{
			return Fail(result, asAST_VERIFY_INVALID_CHILD, owner.range,
				"method-relation-interface-ancestry-duplicate",
				asSASTNodeRef::FromDecl(owner.id),
				asAST_EDGE_DECL_BASE,
				asSASTNodeRef::FromDecl(base->id));
		}
		directInterfaces.PushLast(base->id);
		if( owner.kind == asAST_DECL_INTERFACE
			&& MethodRelationInterfaceReaches(
				context, base->id, owner.id) )
		{
			return Fail(result, asAST_VERIFY_INVALID_CHILD, owner.range,
				"method-relation-interface-ancestry-cycle",
				asSASTNodeRef::FromDecl(owner.id),
				asAST_EDGE_DECL_BASE,
				asSASTNodeRef::FromDecl(base->id));
		}
	}
	return asAST_VERIFY_OK;
}

static int VerifyInterfaceMethodsAreDeclarationOnly(
	const asCASTContext& context,
	const asCDecl& owner,
	asSAstVerifyResult& result)
{
	if( owner.kind != asAST_DECL_INTERFACE )
	{
		return asAST_VERIFY_OK;
	}
	for( asUINT childIndex = 0;
		childIndex < owner.children.GetLength(); ++childIndex )
	{
		const asCDecl* const method =
			context.GetDecl(owner.children[childIndex]);
		if( method == 0 || method->kind != asAST_DECL_METHOD )
		{
			continue;
		}
		if( method->body.IsValid() )
		{
			return Fail(result, asAST_VERIFY_INVALID_CHILD, method->range,
				"method-relation-interface-method-body",
				asSASTNodeRef::FromDecl(method->id),
				asAST_EDGE_DECL_BODY,
				asSASTNodeRef::FromStmt(method->body));
		}
		// Declaration initializers are executable plans. Runtime-only function
		// data has no Canonical AST field and therefore cannot be persisted here.
		if( method->inits.GetLength() != 0 )
		{
			return Fail(result, asAST_VERIFY_INVALID_CHILD, method->range,
				"method-relation-interface-method-backend-state",
				asSASTNodeRef::FromDecl(method->id));
		}
	}
	return asAST_VERIFY_OK;
}

static void CollectMethodRelationClassAncestors(
	const asCASTContext& context,
	const asCDecl& owner,
	asCArray<asASTDeclId>& classes)
{
	asCArray<asASTDeclId> pending;
	asCArray<asASTDeclId> visited;
	visited.PushLast(owner.id);
	for( asUINT baseIndex = 0; baseIndex < owner.bases.GetLength(); ++baseIndex )
	{
		pending.PushLast(owner.bases[baseIndex]);
	}
	for( asUINT pendingIndex = 0;
		pendingIndex < pending.GetLength(); ++pendingIndex )
	{
		const asCDecl* const candidate =
			context.GetDecl(pending[pendingIndex]);
		if( candidate == 0 || candidate->kind != asAST_DECL_CLASS
			|| IsCanonicalNativeTypeView(candidate)
			|| MethodRelationContainsDecl(visited, candidate->id) )
		{
			continue;
		}
		visited.PushLast(candidate->id);
		classes.PushLast(candidate->id);
		for( asUINT baseIndex = 0;
			baseIndex < candidate->bases.GetLength(); ++baseIndex )
		{
			pending.PushLast(candidate->bases[baseIndex]);
		}
	}
}

static void CollectMethodRelationInterfaceClosure(
	const asCASTContext& context,
	const asCDecl& owner,
	const asCArray<asASTDeclId>& classes,
	asCArray<asASTDeclId>& interfaces)
{
	asCArray<asASTDeclId> pending;
	asCArray<asASTDeclId> visited;
	if( owner.kind == asAST_DECL_INTERFACE )
	{
		visited.PushLast(owner.id);
	}
	for( asUINT baseIndex = 0; baseIndex < owner.bases.GetLength(); ++baseIndex )
	{
		pending.PushLast(owner.bases[baseIndex]);
	}
	for( asUINT classIndex = 0; classIndex < classes.GetLength(); ++classIndex )
	{
		const asCDecl* const cls = context.GetDecl(classes[classIndex]);
		for( asUINT baseIndex = 0;
			cls != 0 && baseIndex < cls->bases.GetLength(); ++baseIndex )
		{
			pending.PushLast(cls->bases[baseIndex]);
		}
	}
	for( asUINT pendingIndex = 0;
		pendingIndex < pending.GetLength(); ++pendingIndex )
	{
		const asCDecl* const candidate =
			context.GetDecl(pending[pendingIndex]);
		if( candidate == 0 || candidate->kind != asAST_DECL_INTERFACE
			|| MethodRelationContainsDecl(visited, candidate->id) )
		{
			continue;
		}
		visited.PushLast(candidate->id);
		interfaces.PushLast(candidate->id);
		for( asUINT baseIndex = 0;
			baseIndex < candidate->bases.GetLength(); ++baseIndex )
		{
			pending.PushLast(candidate->bases[baseIndex]);
		}
	}
}

static int VerifyExistingMethodRelations(
	const asCASTContext& context,
	const asCDecl& owner,
	asSAstVerifyResult& result)
{
	if( owner.methodRelations.GetLength() == 0 )
	{
		return asAST_VERIFY_OK;
	}
	if( owner.kind != asAST_DECL_CLASS
		&& owner.kind != asAST_DECL_INTERFACE )
	{
		return Fail(result, asAST_VERIFY_WRONG_KIND, owner.range,
			"method-relation-owner-kind",
			asSASTNodeRef::FromDecl(owner.id));
	}

	asCArray<asASTDeclId> classes;
	CollectMethodRelationClassAncestors(context, owner, classes);
	asCArray<asASTDeclId> interfaces;
	CollectMethodRelationInterfaceClosure(
		context, owner, classes, interfaces);

	for( asUINT relationIndex = 0;
		relationIndex < owner.methodRelations.GetLength(); ++relationIndex )
	{
		const asSASTMethodRelation& relation =
			owner.methodRelations[relationIndex];
		if( relation.kind != asAST_METHOD_RELATION_BASE_OVERRIDE
			&& relation.kind != asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION )
		{
			return Fail(result, asAST_VERIFY_WRONG_KIND, owner.range,
				"method-relation-kind",
				asSASTNodeRef::FromDecl(owner.id));
		}
		if( relation.implementation.snapshotOwner != 0 )
		{
			return Fail(result, asAST_VERIFY_FOREIGN_ID, owner.range,
				"method-relation-implementation-foreign",
				asSASTNodeRef::FromDecl(owner.id),
				asAST_EDGE_DECL_METHOD_RELATION_IMPLEMENTATION,
				asSASTNodeRef::FromDecl(relation.implementation));
		}
		if( relation.requirement.snapshotOwner != 0 )
		{
			return Fail(result, asAST_VERIFY_FOREIGN_ID, owner.range,
				"method-relation-requirement-foreign",
				asSASTNodeRef::FromDecl(owner.id),
				asAST_EDGE_DECL_METHOD_RELATION_REQUIREMENT,
				asSASTNodeRef::FromDecl(relation.requirement));
		}
		const asCDecl* const implementation =
			context.GetDecl(relation.implementation);
		if( implementation == 0 )
		{
			return Fail(result, asAST_VERIFY_DANGLING_ID, owner.range,
				"method-relation-implementation-dangling",
				asSASTNodeRef::FromDecl(owner.id),
				asAST_EDGE_DECL_METHOD_RELATION_IMPLEMENTATION,
				asSASTNodeRef::FromDecl(relation.implementation));
		}
		const asCDecl* const requirement =
			context.GetDecl(relation.requirement);
		if( requirement == 0 )
		{
			return Fail(result, asAST_VERIFY_DANGLING_ID, owner.range,
				"method-relation-requirement-dangling",
				asSASTNodeRef::FromDecl(owner.id),
				asAST_EDGE_DECL_METHOD_RELATION_REQUIREMENT,
				asSASTNodeRef::FromDecl(relation.requirement));
		}
		if( implementation->kind != asAST_DECL_METHOD
			|| requirement->kind != asAST_DECL_METHOD )
		{
			return Fail(result, asAST_VERIFY_WRONG_KIND, owner.range,
				"method-relation-endpoint-kind",
				asSASTNodeRef::FromDecl(owner.id));
		}
		if( implementation->id == requirement->id )
		{
			return Fail(result, asAST_VERIFY_INVALID_CHILD, owner.range,
				"method-relation-self",
				asSASTNodeRef::FromDecl(owner.id));
		}
		if( !MethodRelationSignatureTypesAreValid(context, implementation)
			|| !MethodRelationSignatureTypesAreValid(context, requirement) )
		{
			return Fail(result, asAST_VERIFY_INVALID_TYPE, owner.range,
				"method-relation-signature-type",
				asSASTNodeRef::FromDecl(owner.id));
		}
		if( !MethodRelationSignaturesEqual(
			context, implementation, requirement) )
		{
			return Fail(result, asAST_VERIFY_INVALID_TYPE, owner.range,
				"method-relation-signature",
				asSASTNodeRef::FromDecl(owner.id),
				asAST_EDGE_DECL_METHOD_RELATION_REQUIREMENT,
				asSASTNodeRef::FromDecl(requirement->id));
		}

		const asCDecl* const implementationOwner =
			context.GetDecl(implementation->parent);
		const bool implementationInOwnerChain = implementationOwner != 0
			&& (implementationOwner->id == owner.id
				|| MethodRelationContainsDecl(
					classes, implementationOwner->id));
		if( !implementationInOwnerChain )
		{
			return Fail(result, asAST_VERIFY_INVALID_CHILD, owner.range,
				"method-relation-implementation-owner",
				asSASTNodeRef::FromDecl(owner.id),
				asAST_EDGE_DECL_METHOD_RELATION_IMPLEMENTATION,
				asSASTNodeRef::FromDecl(implementation->id));
		}

		const asCDecl* const requirementOwner =
			context.GetDecl(requirement->parent);
		if( relation.kind == asAST_METHOD_RELATION_BASE_OVERRIDE )
		{
			if( implementationOwner->id != owner.id
				|| requirementOwner == 0
				|| requirementOwner->kind != asAST_DECL_CLASS
				|| !MethodRelationContainsDecl(
					classes, requirementOwner->id) )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, owner.range,
					"method-relation-base-ancestry",
					asSASTNodeRef::FromDecl(owner.id),
					asAST_EDGE_DECL_METHOD_RELATION_REQUIREMENT,
					asSASTNodeRef::FromDecl(requirement->id));
			}
		}
		else if( requirementOwner == 0
			|| requirementOwner->kind != asAST_DECL_INTERFACE
			|| !MethodRelationContainsDecl(
				interfaces, requirementOwner->id) )
		{
			return Fail(result, asAST_VERIFY_INVALID_CHILD, owner.range,
				"method-relation-interface-closure",
				asSASTNodeRef::FromDecl(owner.id),
				asAST_EDGE_DECL_METHOD_RELATION_REQUIREMENT,
				asSASTNodeRef::FromDecl(requirement->id));
		}

		for( asUINT previousIndex = 0;
			previousIndex < relationIndex; ++previousIndex )
		{
			const asSASTMethodRelation& previous =
				owner.methodRelations[previousIndex];
			if( previous.kind == relation.kind
				&& previous.implementation == relation.implementation
				&& previous.requirement == relation.requirement )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, owner.range,
					"method-relation-duplicate",
					asSASTNodeRef::FromDecl(owner.id));
			}
			if( previous.requirement == relation.requirement )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, owner.range,
					"method-relation-requirement-duplicate",
					asSASTNodeRef::FromDecl(owner.id),
					asAST_EDGE_DECL_METHOD_RELATION_REQUIREMENT,
					asSASTNodeRef::FromDecl(requirement->id));
			}
		}
	}
	return asAST_VERIFY_OK;
}

static int VerifyMethodRelationCompleteness(
	const asCASTContext& context,
	const asCDecl& owner,
	asSAstVerifyResult& result)
{
	if( owner.kind != asAST_DECL_CLASS )
	{
		return asAST_VERIFY_OK;
	}
	asCArray<asASTDeclId> classes;
	CollectMethodRelationClassAncestors(context, owner, classes);
	asCArray<asASTDeclId> interfaces;
	CollectMethodRelationInterfaceClosure(
		context, owner, classes, interfaces);
	for( asUINT interfaceIndex = 0;
		interfaceIndex < interfaces.GetLength(); ++interfaceIndex )
	{
		const asCDecl* const interfaceDecl =
			context.GetDecl(interfaces[interfaceIndex]);
		for( asUINT childIndex = 0;
			interfaceDecl != 0
				&& childIndex < interfaceDecl->children.GetLength();
			++childIndex )
		{
			const asCDecl* const requirement =
				context.GetDecl(interfaceDecl->children[childIndex]);
			if( requirement == 0 || requirement->kind != asAST_DECL_METHOD )
			{
				continue;
			}
			asUINT relationCount = 0;
			for( asUINT relationIndex = 0;
				relationIndex < owner.methodRelations.GetLength();
				++relationIndex )
			{
				const asSASTMethodRelation& relation =
					owner.methodRelations[relationIndex];
				if( relation.kind
						== asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION
					&& relation.requirement == requirement->id )
				{
					++relationCount;
				}
			}
			if( relationCount != 1 )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, owner.range,
					"method-relation-interface-incomplete",
					asSASTNodeRef::FromDecl(owner.id),
					asAST_EDGE_DECL_METHOD_RELATION_REQUIREMENT,
					asSASTNodeRef::FromDecl(requirement->id));
			}
		}
	}
	return asAST_VERIFY_OK;
}

static int VerifyImpl(const asCASTContext& context, asSAstVerifyResult& result)
{
	result = asSAstVerifyResult();

	const asASTDeclId tu = context.GetTranslationUnit();
	if( !tu.IsValid() || context.GetDecl(tu) == 0 )
	{
		return Fail(result, asAST_VERIFY_DANGLING_ID, asCSourceRange(), "translation-unit", asSASTNodeRef::FromDecl(tu));
	}

	for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
	{
		const asASTDeclId id(i);
		const asCDecl* decl = context.GetDecl(id);
		if( decl == 0 )
		{
			return Fail(result, asAST_VERIFY_DANGLING_ID, asCSourceRange(), "decl", asSASTNodeRef::FromDecl(id));
		}
		if( decl->id != id )
		{
			return Fail(result, asAST_VERIFY_FOREIGN_ID, decl->range, "decl-id", asSASTNodeRef::FromDecl(id));
		}
		if( decl->kind == asAST_DECL_INVALID )
		{
			return Fail(result, asAST_VERIFY_WRONG_KIND, decl->range, "decl-kind", asSASTNodeRef::FromDecl(id));
		}
		if( decl->type.IsValid() )
		{
			const asCType* const exactType = context.GetType(decl->type.type);
			if( exactType != 0 && exactType->stableKey.Equals("auto") )
			{
				return Fail(result, asAST_VERIFY_WRONG_KIND, decl->range,
					"decl-auto-placeholder", asSASTNodeRef::FromDecl(id));
			}
		}
		if( decl->parent.IsValid() )
		{
			const asCDecl* parent = context.GetDecl(decl->parent);
			if( parent == 0 )
			{
				return Fail(result, asAST_VERIFY_DANGLING_ID, decl->range, "decl-parent", asSASTNodeRef::FromDecl(id), asAST_EDGE_DECL_PARENT, asSASTNodeRef::FromDecl(decl->parent));
			}
			if( !DeclListsChild(parent, id) )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, decl->range, "decl-parent-child", asSASTNodeRef::FromDecl(id), asAST_EDGE_DECL_PARENT, asSASTNodeRef::FromDecl(decl->parent));
			}

			asASTDeclId walk = decl->parent;
			asUINT guard = 0;
			while( walk.IsValid() && guard++ < context.GetDeclCount() + 1 )
			{
				if( walk == id )
				{
					return Fail(result, asAST_VERIFY_INVALID_CHILD, decl->range, "decl-cycle", asSASTNodeRef::FromDecl(id), asAST_EDGE_DECL_PARENT, asSASTNodeRef::FromDecl(walk));
				}
				const asCDecl* ancestor = context.GetDecl(walk);
				if( ancestor == 0 )
				{
					break;
				}
				walk = ancestor->parent;
			}
		}
		if( decl->kind == asAST_DECL_PARAM )
		{
			const asCDecl* const owner = context.GetDecl(decl->parent);
			const bool callableOwner = owner != 0
				&& (owner->kind == asAST_DECL_FUNCDEF
					|| owner->kind == asAST_DECL_FUNCTION
					|| owner->kind == asAST_DECL_METHOD
					|| owner->kind == asAST_DECL_CONSTRUCTOR
					|| owner->kind == asAST_DECL_DESTRUCTOR
					|| owner->kind == asAST_DECL_IMPORT
					|| owner->kind == asAST_DECL_MIXIN);
			const asUINT formalCount = owner != 0
				? context.GetFormalDeclCount(owner->id) : 0;
			if( !callableOwner
				|| decl->formalIndex == asAST_FORMAL_INDEX_NONE
				|| decl->formalIndex >= formalCount
				|| context.GetFormalDecl(owner->id, decl->formalIndex) != decl )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, decl->range,
					"param-formal-index", asSASTNodeRef::FromDecl(id),
					asAST_EDGE_DECL_PARENT,
					asSASTNodeRef::FromDecl(decl->parent));
			}
		}
		else if( decl->formalIndex != asAST_FORMAL_INDEX_NONE )
		{
			return Fail(result, asAST_VERIFY_INVALID_CHILD, decl->range,
				"non-param-formal-index", asSASTNodeRef::FromDecl(id));
		}
		const asDWORD compileOutDisposition = decl->traits
			& (asAST_TRAIT_COMPILE_OUT_ENTIRELY
				| asAST_TRAIT_REPLACE_WITH_FIRST_PARAM
				| asAST_TRAIT_COMPILE_OUT_AS_METHOD_CHAIN);
		const bool compileOutCallable = decl->kind == asAST_DECL_FUNCTION
			|| decl->kind == asAST_DECL_METHOD
			|| decl->kind == asAST_DECL_MIXIN;
		if( compileOutDisposition != 0
			&& (!compileOutCallable
				|| (compileOutDisposition & (compileOutDisposition - 1)) != 0) )
		{
			return Fail(result, asAST_VERIFY_WRONG_KIND, decl->range,
				"decl-compile-out-disposition", asSASTNodeRef::FromDecl(id));
		}
		if( decl->range.begin.IsValid() && !context.GetSourceManager().IsValidRange(decl->range) )
		{
			return Fail(result, asAST_VERIFY_INVALID_RANGE, decl->range, "decl-range", asSASTNodeRef::FromDecl(id));
		}
		for( asUINT c = 0; c < decl->children.GetLength(); ++c )
		{
			const asCDecl* child = context.GetDecl(decl->children[c]);
			if( child == 0 )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, decl->range, "decl-child", asSASTNodeRef::FromDecl(id), asAST_EDGE_DECL_CHILD, asSASTNodeRef::FromDecl(decl->children[c]));
			}
			if( child->parent != id )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, decl->range, "decl-parent-child", asSASTNodeRef::FromDecl(id), asAST_EDGE_DECL_CHILD, asSASTNodeRef::FromDecl(decl->children[c]));
			}
		}
		for( asUINT baseIndex = 0; baseIndex < decl->bases.GetLength(); ++baseIndex )
		{
			const asCDecl* const baseDecl = context.GetDecl(decl->bases[baseIndex]);
			if( baseDecl == 0 )
			{
				return Fail(result, asAST_VERIFY_DANGLING_ID, decl->range, "decl-base", asSASTNodeRef::FromDecl(id), asAST_EDGE_DECL_BASE, asSASTNodeRef::FromDecl(decl->bases[baseIndex]));
			}
			if( baseDecl->id == id || (baseDecl->kind != asAST_DECL_CLASS && baseDecl->kind != asAST_DECL_INTERFACE) )
			{
				return Fail(result, asAST_VERIFY_WRONG_KIND, decl->range, "decl-base-kind", asSASTNodeRef::FromDecl(id), asAST_EDGE_DECL_BASE, asSASTNodeRef::FromDecl(decl->bases[baseIndex]));
			}
		}
		for( asUINT initIndex = 0; initIndex < decl->inits.GetLength(); ++initIndex )
		{
			if( context.GetExpr(decl->inits[initIndex]) == 0 )
			{
				return Fail(result, asAST_VERIFY_DANGLING_ID, decl->range, "decl-init", asSASTNodeRef::FromDecl(id), asAST_EDGE_DECL_INIT, asSASTNodeRef::FromExpr(decl->inits[initIndex]));
			}
		}
		if( decl->kind == asAST_DECL_ACCESS_SPECIFIER )
		{
			const asCDecl* const owner = context.GetDecl(decl->parent);
			if( owner == 0 || (owner->kind != asAST_DECL_CLASS
				&& owner->kind != asAST_DECL_INTERFACE
				&& owner->kind != asAST_DECL_MIXIN) )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, decl->range,
					"access-specifier-owner", asSASTNodeRef::FromDecl(id),
					asAST_EDGE_DECL_PARENT, asSASTNodeRef::FromDecl(decl->parent));
			}
			const asDWORD baseTraits = decl->traits
				& (asAST_TRAIT_PRIVATE | asAST_TRAIT_PROTECTED);
			if( (decl->traits & ~(asAST_TRAIT_PRIVATE | asAST_TRAIT_PROTECTED)) != 0
				|| (baseTraits != asAST_TRAIT_PRIVATE
					&& baseTraits != asAST_TRAIT_PROTECTED) )
			{
				return Fail(result, asAST_VERIFY_WRONG_KIND, decl->range,
					"access-specifier-traits", asSASTNodeRef::FromDecl(id));
			}
			for( asUINT childIndex = 0;
				childIndex < decl->children.GetLength(); ++childIndex )
			{
				const asCDecl* const permission =
					context.GetDecl(decl->children[childIndex]);
				if( permission == 0
					|| permission->kind != asAST_DECL_ACCESS_PERMISSION )
				{
					return Fail(result, asAST_VERIFY_INVALID_CHILD, decl->range,
						"access-specifier-permission", asSASTNodeRef::FromDecl(id),
						asAST_EDGE_DECL_CHILD,
						asSASTNodeRef::FromDecl(decl->children[childIndex]));
				}
			}
		}
		else if( decl->kind == asAST_DECL_ACCESS_PERMISSION )
		{
			const asCDecl* const owner = context.GetDecl(decl->parent);
			const asDWORD allowedTraits = asAST_TRAIT_ACCESS_WILDCARD
				| asAST_TRAIT_ACCESS_READONLY
				| asAST_TRAIT_ACCESS_EDITDEFAULTS
				| asAST_TRAIT_ACCESS_INHERITED;
			const bool wildcard = decl->name.Equals("*");
			if( owner == 0 || owner->kind != asAST_DECL_ACCESS_SPECIFIER
				|| decl->name.GetLength() == 0
				|| (decl->traits & ~allowedTraits) != 0
				|| (((decl->traits & asAST_TRAIT_ACCESS_WILDCARD) != 0) != wildcard)
				|| (wildcard
					&& (decl->traits & asAST_TRAIT_ACCESS_INHERITED) != 0) )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, decl->range,
					"access-permission-shape", asSASTNodeRef::FromDecl(id),
					asAST_EDGE_DECL_PARENT, asSASTNodeRef::FromDecl(decl->parent));
			}
		}
		if( decl->accessSpecifier.IsValid() )
		{
			const asCDecl* const specifier =
				context.GetDecl(decl->accessSpecifier);
			if( specifier == 0 )
			{
				return Fail(result, asAST_VERIFY_DANGLING_ID, decl->range,
					"decl-access-specifier", asSASTNodeRef::FromDecl(id),
					asAST_EDGE_DECL_ACCESS_SPECIFIER,
					asSASTNodeRef::FromDecl(decl->accessSpecifier));
			}
			const bool memberKind = decl->kind == asAST_DECL_VAR
				|| decl->kind == asAST_DECL_PROPERTY
				|| decl->kind == asAST_DECL_METHOD
				|| decl->kind == asAST_DECL_CONSTRUCTOR
				|| decl->kind == asAST_DECL_DESTRUCTOR;
			if( !memberKind
				|| specifier->kind != asAST_DECL_ACCESS_SPECIFIER
				|| !decl->parent.IsValid()
				|| specifier->parent != decl->parent )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, decl->range,
					"decl-access-specifier-owner", asSASTNodeRef::FromDecl(id),
					asAST_EDGE_DECL_ACCESS_SPECIFIER,
					asSASTNodeRef::FromDecl(decl->accessSpecifier));
			}
		}
		if( decl->kind == asAST_DECL_CLASS && (decl->byteSize >= 0 || decl->byteAlignment != 0) )
		{
			if( decl->byteSize < 0 || decl->byteAlignment == 0
				|| (decl->byteAlignment & (decl->byteAlignment - 1)) != 0 )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, decl->range, "class-layout", asSASTNodeRef::FromDecl(id));
			}
			const asCDecl* layoutBase = 0;
			for( asUINT baseIndex = 0; baseIndex < decl->bases.GetLength(); ++baseIndex )
			{
				const asCDecl* const candidate = context.GetDecl(decl->bases[baseIndex]);
				if( candidate && candidate->kind == asAST_DECL_CLASS
					&& !IsCanonicalNativeTypeView(candidate) )
				{
					if( layoutBase != 0 )
					{
						return Fail(result, asAST_VERIFY_INVALID_CHILD, decl->range, "class-layout-multiple-bases", asSASTNodeRef::FromDecl(id), asAST_EDGE_DECL_BASE, asSASTNodeRef::FromDecl(candidate->id));
					}
					layoutBase = candidate;
				}
			}
			// For a root class, class byteOffset is the sealed PreClassData storage
			// prefix. Derived classes follow the maintained ABI and begin local
			// fields after their source base, while retaining their own embedding
			// metadata separately on the declaration.
			const int baseSize = layoutBase
				? layoutBase->byteSize
				: (decl->byteOffset >= 0 ? decl->byteOffset : 0);
			if( layoutBase && (baseSize < 0 || layoutBase->byteAlignment == 0
				|| baseSize > decl->byteSize || layoutBase->byteAlignment > decl->byteAlignment) )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, decl->range, "class-layout-base", asSASTNodeRef::FromDecl(id), asAST_EDGE_DECL_BASE, asSASTNodeRef::FromDecl(layoutBase->id));
			}
			for( asUINT childIndex = 0; childIndex < decl->children.GetLength(); ++childIndex )
			{
				const asCDecl* const member = context.GetDecl(decl->children[childIndex]);
				if( member && member->kind == asAST_DECL_VAR
					&& (member->byteOffset < baseSize || member->byteOffset < 0
						|| (decl->byteSize == 0 ? member->byteOffset != 0 : member->byteOffset >= decl->byteSize)) )
				{
					return Fail(result, asAST_VERIFY_INVALID_CHILD, member->range, "class-field-layout", asSASTNodeRef::FromDecl(member->id), asAST_EDGE_DECL_PARENT, asSASTNodeRef::FromDecl(id));
				}
			}
			if( layoutBase )
			{
				const asCDecl* baseConstructor = 0;
				for( asUINT childIndex = 0; childIndex < layoutBase->children.GetLength(); ++childIndex )
				{
					const asCDecl* const candidate = context.GetDecl(layoutBase->children[childIndex]);
					if( candidate == 0 || candidate->kind != asAST_DECL_CONSTRUCTOR )
					{
						continue;
					}
					asUINT parameterCount = 0;
					for( asUINT nestedIndex = 0; nestedIndex < candidate->children.GetLength(); ++nestedIndex )
					{
						const asCDecl* const nested = context.GetDecl(candidate->children[nestedIndex]);
						if( nested && nested->kind == asAST_DECL_PARAM )
						{
							++parameterCount;
						}
					}
					if( parameterCount == 0 )
					{
						baseConstructor = candidate;
						if( (candidate->traits & asAST_TRAIT_GENERATED) != 0 )
						{
							break;
						}
					}
				}
				if( baseConstructor == 0 )
				{
					return Fail(result, asAST_VERIFY_INVALID_CHILD, decl->range, "class-base-default-constructor", asSASTNodeRef::FromDecl(id), asAST_EDGE_DECL_BASE, asSASTNodeRef::FromDecl(layoutBase->id));
				}
				for( asUINT childIndex = 0; childIndex < decl->children.GetLength(); ++childIndex )
				{
					const asCDecl* const constructor = context.GetDecl(decl->children[childIndex]);
					if( constructor == 0 || constructor->kind != asAST_DECL_CONSTRUCTOR )
					{
						continue;
					}
					const asCExpr* const firstInit = constructor->inits.GetLength()
						? context.GetExpr(constructor->inits[0])
						: 0;
					if( firstInit == 0 || firstInit->kind != asAST_EXPR_CALL
						|| firstInit->resolvedDecl != baseConstructor->id
						|| !firstInit->receiver.IsValid() )
					{
						return Fail(result, asAST_VERIFY_INVALID_CHILD, constructor->range, "constructor-base-init", asSASTNodeRef::FromDecl(constructor->id), asAST_EDGE_DECL_INIT, firstInit ? asSASTNodeRef::FromExpr(firstInit->id) : asSASTNodeRef());
					}
				}
			}
		}
		if( decl->body.IsValid() )
		{
			const asCStmt* body = context.GetStmt(decl->body);
			if( body == 0 )
			{
				return Fail(result, asAST_VERIFY_DANGLING_ID, decl->range, "decl-body", asSASTNodeRef::FromDecl(id), asAST_EDGE_DECL_BODY, asSASTNodeRef::FromStmt(decl->body));
			}
			if( body->owner != id )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, decl->range, "decl-body", asSASTNodeRef::FromDecl(id), asAST_EDGE_DECL_BODY, asSASTNodeRef::FromStmt(decl->body));
			}
		}
		if( decl->accessorKind == asAST_ACCESSOR_NONE )
		{
			if( decl->accessorField.IsValid() )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, decl->range, "accessor-field-without-kind", asSASTNodeRef::FromDecl(id), asAST_EDGE_DECL_ACCESSOR_FIELD, asSASTNodeRef::FromDecl(decl->accessorField));
			}
		}
		else
		{
			if( decl->accessorKind != asAST_ACCESSOR_GET && decl->accessorKind != asAST_ACCESSOR_SET )
			{
				return Fail(result, asAST_VERIFY_WRONG_KIND, decl->range, "accessor-kind", asSASTNodeRef::FromDecl(id));
			}
			if( decl->kind != asAST_DECL_METHOD || (decl->traits & asAST_TRAIT_GENERATED) == 0 )
			{
				return Fail(result, asAST_VERIFY_WRONG_KIND, decl->range, "accessor-decl", asSASTNodeRef::FromDecl(id));
			}
			const asCDecl* field = context.GetDecl(decl->accessorField);
			if( field == 0 )
			{
				return Fail(result, asAST_VERIFY_DANGLING_ID, decl->range, "accessor-field", asSASTNodeRef::FromDecl(id), asAST_EDGE_DECL_ACCESSOR_FIELD, asSASTNodeRef::FromDecl(decl->accessorField));
			}
			if( field->kind != asAST_DECL_VAR || field->parent != decl->parent )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, decl->range, "accessor-owner", asSASTNodeRef::FromDecl(id), asAST_EDGE_DECL_ACCESSOR_FIELD, asSASTNodeRef::FromDecl(decl->accessorField));
			}

			asUINT paramCount = 0;
			const asCDecl* param = 0;
			for( asUINT c = 0; c < decl->children.GetLength(); ++c )
			{
				const asCDecl* child = context.GetDecl(decl->children[c]);
				if( child && child->kind == asAST_DECL_PARAM )
				{
					++paramCount;
					param = child;
				}
			}
			if( decl->accessorKind == asAST_ACCESSOR_GET )
			{
				if( decl->type != field->type || paramCount != 0 )
				{
					return Fail(result, asAST_VERIFY_WRONG_KIND, decl->range, "accessor-get-signature", asSASTNodeRef::FromDecl(id), asAST_EDGE_DECL_ACCESSOR_FIELD, asSASTNodeRef::FromDecl(decl->accessorField));
				}
				// Scalar/POD accessors legitimately have no declaration init because
				// their storage read is enough. Once Sema records a by-value object
				// plan, however, it must be the one exact copy-construction route; an
				// arbitrary initializer cannot be deferred to backend interpretation.
				if( decl->inits.GetLength() != 0
					&& !IsExactGeneratedGetterCopyPlan(context, decl, field) )
				{
					const asCExpr* const plan = decl->inits.GetLength()
						? context.GetExpr(decl->inits[0]) : 0;
					return Fail(result, asAST_VERIFY_INVALID_CHILD, decl->range,
						"accessor-get-copy-plan", asSASTNodeRef::FromDecl(id),
						asAST_EDGE_DECL_INIT,
						plan ? asSASTNodeRef::FromExpr(plan->id) : asSASTNodeRef());
				}
			}
			else
			{
				const asCType* returnType = context.GetType(decl->type.type);
				if( returnType == 0 || returnType->kind != asAST_TYPE_VOID || decl->type.quals != 0
					|| paramCount != 1 || param == 0 || param->type != field->type )
				{
					return Fail(result, asAST_VERIFY_WRONG_KIND, decl->range, "accessor-set-signature", asSASTNodeRef::FromDecl(id), asAST_EDGE_DECL_ACCESSOR_FIELD, asSASTNodeRef::FromDecl(decl->accessorField));
				}
				// Scalar/POD setters need no declaration init. If Sema records a
				// non-POD plan, it must be the exact field = parameter assignment and
				// exact opAssign overload; a backend may not infer either fact later.
				if( decl->inits.GetLength() != 0
					&& !IsExactGeneratedSetterAssignmentPlan(
						context, decl, field, param) )
				{
					const asCExpr* const plan = decl->inits.GetLength()
						? context.GetExpr(decl->inits[0]) : 0;
					return Fail(result, asAST_VERIFY_INVALID_CHILD, decl->range,
						"accessor-set-assignment-plan", asSASTNodeRef::FromDecl(id),
						asAST_EDGE_DECL_INIT,
						plan ? asSASTNodeRef::FromExpr(plan->id) : asSASTNodeRef());
				}
			}
		}
	}

	// The first declaration pass above authenticates the entire generic graph.
	// Only then authenticate interface shape and every authored relation so a
	// malformed later endpoint cannot be masked by an earlier relation result.
	for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
	{
		const asCDecl* const decl = context.GetDecl(asASTDeclId(i));
		if( decl == 0 )
		{
			continue;
		}
		int methodRelationStatus =
			VerifyMethodRelationClassAncestry(context, *decl, result);
		if( methodRelationStatus == asAST_VERIFY_OK )
		{
			methodRelationStatus =
				VerifyMethodRelationInterfaceAncestry(
					context, *decl, result);
		}
		if( methodRelationStatus == asAST_VERIFY_OK )
		{
			methodRelationStatus =
				VerifyInterfaceMethodsAreDeclarationOnly(
					context, *decl, result);
		}
		if( methodRelationStatus == asAST_VERIFY_OK )
		{
			methodRelationStatus =
				VerifyExistingMethodRelations(context, *decl, result);
		}
		if( methodRelationStatus != asAST_VERIFY_OK )
		{
			return methodRelationStatus;
		}
	}

	// Coverage is the final relation phase. This keeps a valid missing-method
	// diagnostic from hiding any forged owner, ancestry, endpoint or signature.
	for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
	{
		const asCDecl* const decl = context.GetDecl(asASTDeclId(i));
		if( decl == 0 )
		{
			continue;
		}
		const int completenessStatus =
			VerifyMethodRelationCompleteness(context, *decl, result);
		if( completenessStatus != asAST_VERIFY_OK )
		{
			return completenessStatus;
		}
	}

	asCArray<asASTStmtId> stmtParent;
	stmtParent.SetLength(context.GetStmtCount() + 1);
	for( asUINT p = 0; p < stmtParent.GetLength(); ++p )
	{
		stmtParent[p] = asASTStmtId();
	}

	for( asUINT i = 1; i <= context.GetStmtCount(); ++i )
	{
		const asASTStmtId id(i);
		const asCStmt* stmt = context.GetStmt(id);
		if( stmt == 0 )
		{
			return Fail(result, asAST_VERIFY_DANGLING_ID, asCSourceRange(), "stmt", asSASTNodeRef::FromStmt(id));
		}
		if( stmt->id != id )
		{
			return Fail(result, asAST_VERIFY_FOREIGN_ID, stmt->range, "stmt-id", asSASTNodeRef::FromStmt(id));
		}
		if( stmt->kind == asAST_STMT_INVALID )
		{
			return Fail(result, asAST_VERIFY_WRONG_KIND, stmt->range, "stmt-kind", asSASTNodeRef::FromStmt(id));
		}
		if( stmt->owner.IsValid() && context.GetDecl(stmt->owner) == 0 )
		{
			return Fail(result, asAST_VERIFY_FOREIGN_ID, stmt->range, "stmt-owner", asSASTNodeRef::FromStmt(id), asAST_EDGE_STMT_OWNER, asSASTNodeRef::FromDecl(stmt->owner));
		}
		if( stmt->target.IsValid() && context.GetStmt(stmt->target) == 0 )
		{
			return Fail(result, asAST_VERIFY_DANGLING_ID, stmt->range, "stmt-target", asSASTNodeRef::FromStmt(id), asAST_EDGE_STMT_TARGET, asSASTNodeRef::FromStmt(stmt->target));
		}
		if( stmt->expr.IsValid() && context.GetExpr(stmt->expr) == 0 )
		{
			return Fail(result, asAST_VERIFY_DANGLING_ID, stmt->range, "stmt-expr", asSASTNodeRef::FromStmt(id), asAST_EDGE_STMT_EXPR, asSASTNodeRef::FromExpr(stmt->expr));
		}
		if( stmt->range.begin.IsValid() && !context.GetSourceManager().IsValidRange(stmt->range) )
		{
			return Fail(result, asAST_VERIFY_INVALID_RANGE, stmt->range, "stmt-range", asSASTNodeRef::FromStmt(id));
		}
		for( asUINT c = 0; c < stmt->children.GetLength(); ++c )
		{
			const asASTStmtId child = stmt->children[c];
			if( context.GetStmt(child) == 0 )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, stmt->range, "stmt-child", asSASTNodeRef::FromStmt(id), asAST_EDGE_STMT_CHILD, asSASTNodeRef::FromStmt(child));
			}
			if( child.IsValid() && child.value < stmtParent.GetLength() )
			{
				if( stmtParent[child.value].IsValid() && stmtParent[child.value] != id )
				{
					return Fail(result, asAST_VERIFY_INVALID_CHILD, stmt->range, "stmt-multi-owner", asSASTNodeRef::FromStmt(child), asAST_EDGE_STMT_CHILD, asSASTNodeRef::FromStmt(id));
				}
				stmtParent[child.value] = id;
			}
		}
		if( stmt->kind == asAST_STMT_FOREACH )
		{
			if( stmt->children.GetLength() < 3 || stmt->children.GetLength() > 4 )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, stmt->range,
					"foreach-phase-count", asSASTNodeRef::FromStmt(id),
					asAST_EDGE_STMT_CHILD);
			}
			if( stmt->children.GetLength() == 4 )
			{
				const asASTStmtId cleanupStmtId = stmt->children[3];
				const asCStmt* const cleanupStmt = context.GetStmt(cleanupStmtId);
				if( cleanupStmt == 0 || cleanupStmt->kind != asAST_STMT_EXPR )
				{
					return Fail(result, asAST_VERIFY_WRONG_KIND, stmt->range,
						"foreach-cleanup-stmt", asSASTNodeRef::FromStmt(cleanupStmtId),
						asAST_EDGE_STMT_CHILD, asSASTNodeRef::FromStmt(id));
				}
				const asCExpr* const cleanup = context.GetExpr(cleanupStmt->expr);
				if( cleanup == 0 || cleanup->kind != asAST_EXPR_CLEANUP )
				{
					return Fail(result, asAST_VERIFY_WRONG_KIND, cleanupStmt->range,
						"foreach-cleanup-expr", asSASTNodeRef::FromStmt(cleanupStmtId),
						asAST_EDGE_STMT_EXPR,
						asSASTNodeRef::FromExpr(cleanupStmt->expr));
				}
				if( !cleanup->literal.Equals("scope-exit") )
				{
					return Fail(result, asAST_VERIFY_INVALID_CHILD, cleanup->range,
						"foreach-cleanup-literal", asSASTNodeRef::FromExpr(cleanup->id));
				}
				const asCDecl* const destructor =
					context.GetDecl(cleanup->resolvedDecl);
				if( destructor == 0 || destructor->kind != asAST_DECL_DESTRUCTOR )
				{
					return Fail(result, asAST_VERIFY_WRONG_KIND, cleanup->range,
						"foreach-cleanup-dtor", asSASTNodeRef::FromExpr(cleanup->id),
						asAST_EDGE_EXPR_RESOLVED_DECL,
						asSASTNodeRef::FromDecl(cleanup->resolvedDecl));
				}
				const asCExpr* const target = cleanup->children.GetLength() == 1
					? context.GetExpr(cleanup->children[0]) : 0;
				const asCDecl* const targetDecl = target
					&& target->kind == asAST_EXPR_DECL_REF
					? context.GetDecl(target->resolvedDecl) : 0;
				if( targetDecl == 0 || targetDecl->kind != asAST_DECL_VAR
					|| (targetDecl->traits & asAST_TRAIT_GENERATED) == 0
					|| !StmtSubtreeDeclares(
						context, stmt->children[0], targetDecl->id) )
				{
					return Fail(result, asAST_VERIFY_INVALID_CHILD, cleanup->range,
						"foreach-cleanup-target", asSASTNodeRef::FromExpr(cleanup->id),
						asAST_EDGE_EXPR_CHILD,
						target ? asSASTNodeRef::FromExpr(target->id) : asSASTNodeRef());
				}
				const asCDecl* const destructorClass =
					context.GetDecl(destructor->parent);
				const asCType* const targetType =
					context.GetType(targetDecl->type.type);
				if( destructorClass == 0 || destructorClass->kind != asAST_DECL_CLASS
					|| targetType == 0
					|| !targetType->stableKey.Equals(destructorClass->stableKey) )
				{
					return Fail(result, asAST_VERIFY_INVALID_TYPE, cleanup->range,
						"foreach-cleanup-type", asSASTNodeRef::FromExpr(cleanup->id),
						asAST_EDGE_EXPR_TYPE,
						asSASTNodeRef::FromDecl(destructorClass
							? destructorClass->id : asASTDeclId()));
				}
			}
		}
		if( stmt->kind == asAST_STMT_BREAK || stmt->kind == asAST_STMT_CONTINUE )
		{
			if( stmt->target.IsValid() )
			{
				const asCStmt* target = context.GetStmt(stmt->target);
				if( target == 0 )
				{
					return Fail(result, asAST_VERIFY_DANGLING_ID, stmt->range, "ctrl-target", asSASTNodeRef::FromStmt(id), asAST_EDGE_STMT_TARGET, asSASTNodeRef::FromStmt(stmt->target));
				}
				const bool loopTarget = target->kind == asAST_STMT_WHILE || target->kind == asAST_STMT_DO_WHILE || target->kind == asAST_STMT_FOR || target->kind == asAST_STMT_FOREACH;
				const bool switchTarget = target->kind == asAST_STMT_SWITCH;
				if( stmt->kind == asAST_STMT_CONTINUE && !loopTarget )
				{
					return Fail(result, asAST_VERIFY_WRONG_KIND, stmt->range, "continue-target", asSASTNodeRef::FromStmt(id), asAST_EDGE_STMT_TARGET, asSASTNodeRef::FromStmt(stmt->target));
				}
				if( stmt->kind == asAST_STMT_BREAK && !loopTarget && !switchTarget )
				{
					return Fail(result, asAST_VERIFY_WRONG_KIND, stmt->range, "break-target", asSASTNodeRef::FromStmt(id), asAST_EDGE_STMT_TARGET, asSASTNodeRef::FromStmt(stmt->target));
				}
				if( !StmtContains(context, stmt->target, id) )
				{
					const char* detail = stmt->kind == asAST_STMT_CONTINUE ? "continue-ancestor" : "break-ancestor";
					return Fail(result, asAST_VERIFY_WRONG_KIND, stmt->range, detail, asSASTNodeRef::FromStmt(id), asAST_EDGE_STMT_TARGET, asSASTNodeRef::FromStmt(stmt->target));
				}
				asASTStmtId nearest;
				asASTStmtId walk = (id.value < stmtParent.GetLength()) ? stmtParent[id.value] : asASTStmtId();
				asUINT nearerGuard = 0;
				while( walk.IsValid() && nearerGuard++ < context.GetStmtCount() + 1 )
				{
					const asCStmt* ancestor = context.GetStmt(walk);
					if( ancestor )
					{
						const bool loop = ancestor->kind == asAST_STMT_WHILE
							|| ancestor->kind == asAST_STMT_DO_WHILE
							|| ancestor->kind == asAST_STMT_FOR
							|| ancestor->kind == asAST_STMT_FOREACH;
						const bool sw = ancestor->kind == asAST_STMT_SWITCH;
						if( stmt->kind == asAST_STMT_CONTINUE && loop )
						{
							nearest = walk;
							break;
						}
						if( stmt->kind == asAST_STMT_BREAK && (loop || sw) )
						{
							nearest = walk;
							break;
						}
					}
					if( walk.value >= stmtParent.GetLength() )
					{
						break;
					}
					walk = stmtParent[walk.value];
				}
				if( nearest.IsValid() && nearest != stmt->target )
				{
					const char* detail = stmt->kind == asAST_STMT_CONTINUE ? "continue-skipped-nearer" : "break-skipped-nearer";
					return Fail(result, asAST_VERIFY_WRONG_KIND, stmt->range, detail, asSASTNodeRef::FromStmt(id), asAST_EDGE_STMT_TARGET, asSASTNodeRef::FromStmt(stmt->target));
				}
			}
		}
		if( stmt->kind == asAST_STMT_SWITCH )
		{
			bool sawDefault = false;
			asUINT lastDefaultCaseIndex = ~0u;
			asUINT lastCaseIndex = ~0u;
			for( asUINT c = 0; c < stmt->children.GetLength(); ++c )
			{
				const asCStmt* cs = context.GetStmt(stmt->children[c]);
				if( cs == 0 || cs->kind != asAST_STMT_CASE )
				{
					continue;
				}
				lastCaseIndex = c;
				if( !cs->expr.IsValid() )
				{
					if( sawDefault )
					{
						return Fail(result, asAST_VERIFY_INVALID_CHILD, stmt->range, "duplicate-default", asSASTNodeRef::FromStmt(id), asAST_EDGE_STMT_CHILD, asSASTNodeRef::FromStmt(stmt->children[c]));
					}
					sawDefault = true;
					lastDefaultCaseIndex = c;
				}
				for( asUINT d = c + 1; d < stmt->children.GetLength(); ++d )
				{
					const asCStmt* other = context.GetStmt(stmt->children[d]);
					if( other && other->kind == asAST_STMT_CASE && cs->expr.IsValid() && other->expr.IsValid() )
					{
						const asCExpr* left = context.GetExpr(cs->expr);
						const asCExpr* right = context.GetExpr(other->expr);
						if( left && right && left->literalBits == right->literalBits && left->literal.Equals(right->literal) )
						{
							return Fail(result, asAST_VERIFY_INVALID_CHILD, stmt->range, "duplicate-case", asSASTNodeRef::FromStmt(id), asAST_EDGE_STMT_CHILD, asSASTNodeRef::FromStmt(stmt->children[d]));
						}
					}
				}
			}
			if( lastDefaultCaseIndex != ~0u && lastDefaultCaseIndex != lastCaseIndex )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, stmt->range, "default-order", asSASTNodeRef::FromStmt(id), asAST_EDGE_STMT_CHILD, asSASTNodeRef::FromStmt(stmt->children[lastDefaultCaseIndex]));
			}
		}
		if( stmt->kind == asAST_STMT_FALLTHROUGH )
		{
			if( !stmt->owner.IsValid() )
			{
				return Fail(result, asAST_VERIFY_DANGLING_ID, stmt->range, "fallthrough-owner", asSASTNodeRef::FromStmt(id), asAST_EDGE_STMT_OWNER, asSASTNodeRef::FromDecl(stmt->owner));
			}
		}
	}

	const int stmtCycleStatus = VerifyStructuralCycles(context, true, result);
	if( stmtCycleStatus != asAST_VERIFY_OK )
		return stmtCycleStatus;

	for( asUINT i = 1; i <= context.GetStmtCount(); ++i )
	{
		const asASTStmtId id(i);
		const asCStmt* stmt = context.GetStmt(id);
		if( stmt == 0 || stmt->kind != asAST_STMT_FALLTHROUGH )
		{
			continue;
		}
		bool underSwitch = false;
		asASTStmtId walk = (id.value < stmtParent.GetLength()) ? stmtParent[id.value] : asASTStmtId();
		asUINT guard = 0;
		while( walk.IsValid() && guard++ < context.GetStmtCount() + 1 )
		{
			const asCStmt* ancestor = context.GetStmt(walk);
			if( ancestor && ancestor->kind == asAST_STMT_SWITCH )
			{
				underSwitch = true;
				break;
			}
			if( walk.value >= stmtParent.GetLength() )
			{
				break;
			}
			walk = stmtParent[walk.value];
		}
		if( !underSwitch )
		{
			return Fail(result, asAST_VERIFY_WRONG_KIND, stmt->range, "fallthrough-switch", asSASTNodeRef::FromStmt(id));
		}
		if( stmt->target.IsValid() )
		{
			asASTStmtId switchId;
			asASTStmtId switchWalk = (id.value < stmtParent.GetLength()) ? stmtParent[id.value] : asASTStmtId();
			asUINT switchGuard = 0;
			while( switchWalk.IsValid() && switchGuard++ < context.GetStmtCount() + 1 )
			{
				const asCStmt* ancestor = context.GetStmt(switchWalk);
				if( ancestor && ancestor->kind == asAST_STMT_SWITCH )
				{
					switchId = switchWalk;
					break;
				}
				if( switchWalk.value >= stmtParent.GetLength() )
				{
					break;
				}
				switchWalk = stmtParent[switchWalk.value];
			}
			const asCStmt* sw = context.GetStmt(switchId);
			if( sw )
			{
				asCArray<asASTStmtId> cases;
				for( asUINT c = 0; c < sw->children.GetLength(); ++c )
				{
					const asCStmt* cs = context.GetStmt(sw->children[c]);
					if( cs && cs->kind == asAST_STMT_CASE )
					{
						cases.PushLast(sw->children[c]);
					}
				}
				asASTStmtId ownerCase;
				for( asUINT c = 0; c < cases.GetLength(); ++c )
				{
					if( cases[c] == ((id.value < stmtParent.GetLength()) ? stmtParent[id.value] : asASTStmtId())
						|| StmtContains(context, cases[c], id) )
					{
						ownerCase = cases[c];
						asASTStmtId nextCase = (c + 1 < cases.GetLength()) ? cases[c + 1] : asASTStmtId();
						if( nextCase != stmt->target )
						{
							return Fail(result, asAST_VERIFY_WRONG_KIND, stmt->range, "fallthrough-target", asSASTNodeRef::FromStmt(id), asAST_EDGE_STMT_TARGET, asSASTNodeRef::FromStmt(stmt->target));
						}
						break;
					}
				}
				(void)ownerCase;
			}
		}
	}

	for( asUINT i = 1; i <= context.GetExprCount(); ++i )
	{
		const asASTExprId id(i);
		const asCExpr* expr = context.GetExpr(id);
		if( expr == 0 )
		{
			return Fail(result, asAST_VERIFY_DANGLING_ID, asCSourceRange(), "expr", asSASTNodeRef::FromExpr(id));
		}
		if( expr->id != id )
		{
			return Fail(result, asAST_VERIFY_FOREIGN_ID, expr->range, "expr-id", asSASTNodeRef::FromExpr(id));
		}
		if( expr->kind == asAST_EXPR_INVALID )
		{
			return Fail(result, asAST_VERIFY_WRONG_KIND, expr->range, "expr-kind", asSASTNodeRef::FromExpr(id));
		}
		if( expr->kind != asAST_EXPR_ERROR )
		{
			if( !expr->type.IsValid() || context.GetType(expr->type.type) == 0 )
			{
				return Fail(result, asAST_VERIFY_INVALID_TYPE, expr->range, "expr-type", asSASTNodeRef::FromExpr(id), asAST_EDGE_EXPR_TYPE, asSASTNodeRef::FromType(expr->type.type));
			}
			if( expr->valueCategory != asAST_VALUE_PRVALUE
				&& expr->valueCategory != asAST_VALUE_LVALUE
				&& expr->valueCategory != asAST_VALUE_XVALUE )
			{
				return Fail(result, asAST_VERIFY_INVALID_TYPE, expr->range, "expr-value-category", asSASTNodeRef::FromExpr(id));
			}
		}
		else if( expr->type.IsValid() && context.GetType(expr->type.type) == 0 )
		{
			return Fail(result, asAST_VERIFY_INVALID_TYPE, expr->range, "expr-type", asSASTNodeRef::FromExpr(id), asAST_EDGE_EXPR_TYPE, asSASTNodeRef::FromType(expr->type.type));
		}
		if( !asASTQualifiersAreValid(expr->type.quals) )
		{
			return Fail(result, asAST_VERIFY_INVALID_TYPE, expr->range, "expr-quals", asSASTNodeRef::FromExpr(id), asAST_EDGE_EXPR_TYPE, asSASTNodeRef::FromType(expr->type.type));
		}
		if( expr->resolvedDecl.IsValid() && context.GetDecl(expr->resolvedDecl) == 0 )
		{
			return Fail(result, asAST_VERIFY_DANGLING_ID, expr->range, "expr-decl", asSASTNodeRef::FromExpr(id), asAST_EDGE_EXPR_RESOLVED_DECL, asSASTNodeRef::FromDecl(expr->resolvedDecl));
		}
		if( expr->callDispatch > asAST_CALL_DISPATCH_INDIRECT )
		{
			return Fail(result, asAST_VERIFY_INVALID_CHILD, expr->range, "call-dispatch-value", asSASTNodeRef::FromExpr(id));
		}
		if( expr->kind != asAST_EXPR_CALL
			&& expr->callDispatch != asAST_CALL_DISPATCH_NONE )
		{
			return Fail(result, asAST_VERIFY_WRONG_KIND, expr->range, "call-dispatch-kind", asSASTNodeRef::FromExpr(id));
		}
		if( expr->receiver.IsValid() )
		{
			if( expr->kind != asAST_EXPR_CALL
				&& expr->kind != asAST_EXPR_DEFERRED_OUT
				&& expr->kind != asAST_EXPR_CALL_REWRITE )
			{
				return Fail(result, asAST_VERIFY_WRONG_KIND, expr->range, "expr-receiver-kind", asSASTNodeRef::FromExpr(id), asAST_EDGE_EXPR_RECEIVER, asSASTNodeRef::FromExpr(expr->receiver));
			}
			if( expr->receiver == id )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, expr->range, "expr-receiver-self", asSASTNodeRef::FromExpr(id), asAST_EDGE_EXPR_RECEIVER, asSASTNodeRef::FromExpr(expr->receiver));
			}
			if( context.GetExpr(expr->receiver) == 0 )
			{
				return Fail(result, asAST_VERIFY_DANGLING_ID, expr->range, "expr-receiver", asSASTNodeRef::FromExpr(id), asAST_EDGE_EXPR_RECEIVER, asSASTNodeRef::FromExpr(expr->receiver));
			}
		}
		if( expr->kind == asAST_EXPR_DEFERRED_OUT )
		{
			const asCType* const outType = context.GetType(expr->type.type);
			const asCDecl* const setter = context.GetDecl(expr->resolvedDecl);
			const asCExpr* const receiver = context.GetExpr(expr->receiver);
			if( !expr->literal.Equals("deferred-out")
				|| expr->valueCategory != asAST_VALUE_LVALUE
				|| outType == 0 || outType->kind != asAST_TYPE_PRIMITIVE
				|| !expr->type.IsReference()
				|| (expr->type.quals & asAST_QUAL_OUT) == 0
				|| (expr->type.quals & asAST_QUAL_IN) != 0 )
			{
				return Fail(result, asAST_VERIFY_INVALID_TYPE, expr->range,
					"deferred-out-type", asSASTNodeRef::FromExpr(id),
					asAST_EDGE_EXPR_TYPE,
					asSASTNodeRef::FromType(expr->type.type));
			}
			if( expr->children.GetLength() != 1
				|| expr->children[0] != expr->receiver
				|| receiver == 0
				|| receiver->kind != asAST_EXPR_OPAQUE_VALUE
				|| receiver->children.GetLength() != 1 )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, expr->range,
					"deferred-out-receiver", asSASTNodeRef::FromExpr(id),
					asAST_EDGE_EXPR_RECEIVER,
					receiver ? asSASTNodeRef::FromExpr(receiver->id)
						: asSASTNodeRef());
			}
			if( setter == 0 || setter->kind != asAST_DECL_METHOD
				|| setter->accessorKind != asAST_ACCESSOR_SET
				|| !setter->accessorField.IsValid() )
			{
				return Fail(result, asAST_VERIFY_WRONG_KIND, expr->range,
					"deferred-out-setter", asSASTNodeRef::FromExpr(id),
					asAST_EDGE_EXPR_RESOLVED_DECL,
					setter ? asSASTNodeRef::FromDecl(setter->id)
						: asSASTNodeRef());
			}

			asUINT setterParamCount = 0;
			const asCDecl* setterParam = 0;
			for( asUINT childIndex = 0;
				childIndex < setter->children.GetLength(); ++childIndex )
			{
				const asCDecl* const child =
					context.GetDecl(setter->children[childIndex]);
				if( child != 0 && child->kind == asAST_DECL_PARAM )
				{
					++setterParamCount;
					setterParam = child;
				}
			}
			const asCDecl* const setterOwner = context.GetDecl(setter->parent);
			const asCType* const receiverType = receiver != 0
				? context.GetType(receiver->type.type) : 0;
			if( setterParamCount != 1 || setterParam == 0
				|| setterParam->type.type != expr->type.type
				|| setterParam->type.quals != 0
				|| setterOwner == 0 || receiverType == 0
				|| (!receiverType->stableKey.Equals(setterOwner->stableKey)
					&& !receiverType->stableKey.Equals(setterOwner->name)) )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, expr->range,
					"deferred-out-plan", asSASTNodeRef::FromExpr(id),
					asAST_EDGE_EXPR_RESOLVED_DECL,
					asSASTNodeRef::FromDecl(setter->id));
			}
		}
		if( expr->kind == asAST_EXPR_CLEANUP && expr->resolvedDecl.IsValid() )
		{
			const asCDecl* cleanupDecl = context.GetDecl(expr->resolvedDecl);
			if( cleanupDecl == 0 || cleanupDecl->kind != asAST_DECL_DESTRUCTOR )
			{
				return Fail(result, asAST_VERIFY_WRONG_KIND, expr->range, "cleanup-dtor", asSASTNodeRef::FromExpr(id), asAST_EDGE_EXPR_RESOLVED_DECL, asSASTNodeRef::FromDecl(expr->resolvedDecl));
			}
		}
		if( expr->kind == asAST_EXPR_CLEANUP
			&& expr->literal.Equals("scope-exit") )
		{
			const asCDecl* const destructor =
				context.GetDecl(expr->resolvedDecl);
			if( destructor == 0 || destructor->kind != asAST_DECL_DESTRUCTOR )
			{
				return Fail(result, asAST_VERIFY_WRONG_KIND, expr->range,
					"scope-exit-cleanup-dtor", asSASTNodeRef::FromExpr(id),
					asAST_EDGE_EXPR_RESOLVED_DECL,
					asSASTNodeRef::FromDecl(expr->resolvedDecl));
			}
			const asCExpr* const target = expr->children.GetLength() == 1
				? context.GetExpr(expr->children[0]) : 0;
			const asCDecl* const targetDecl = target != 0
				&& target->kind == asAST_EXPR_DECL_REF
				? context.GetDecl(target->resolvedDecl) : 0;
			if( targetDecl == 0 || targetDecl->kind != asAST_DECL_VAR )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, expr->range,
					"scope-exit-cleanup-target", asSASTNodeRef::FromExpr(id),
					asAST_EDGE_EXPR_CHILD,
					target ? asSASTNodeRef::FromExpr(target->id) : asSASTNodeRef());
			}
			const asCDecl* const destructorClass =
				context.GetDecl(destructor->parent);
			const asCType* const targetType =
				context.GetType(targetDecl->type.type);
			if( destructorClass == 0
				|| destructorClass->kind != asAST_DECL_CLASS
				|| targetType == 0
				|| targetType->kind != asAST_TYPE_VALUE_OBJECT
				|| !targetType->stableKey.Equals(destructorClass->stableKey) )
			{
				return Fail(result, asAST_VERIFY_INVALID_TYPE, expr->range,
					"scope-exit-cleanup-type", asSASTNodeRef::FromExpr(id),
					asAST_EDGE_EXPR_TYPE,
					asSASTNodeRef::FromDecl(destructorClass
						? destructorClass->id : asASTDeclId()));
			}
		}
		if( expr->kind == asAST_EXPR_CLEANUP
			&& expr->literal.Equals("scope-release") )
		{
			if( expr->resolvedDecl.IsValid() )
			{
				return Fail(result, asAST_VERIFY_WRONG_KIND, expr->range,
					"scope-release-cleanup-decl", asSASTNodeRef::FromExpr(id),
					asAST_EDGE_EXPR_RESOLVED_DECL,
					asSASTNodeRef::FromDecl(expr->resolvedDecl));
			}
			const asCExpr* const target = expr->children.GetLength() == 1
				? context.GetExpr(expr->children[0]) : 0;
			const asCDecl* const targetDecl = target != 0
				&& target->kind == asAST_EXPR_DECL_REF
				? context.GetDecl(target->resolvedDecl) : 0;
			if( targetDecl == 0 || targetDecl->kind != asAST_DECL_VAR )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, expr->range,
					"scope-release-cleanup-target", asSASTNodeRef::FromExpr(id),
					asAST_EDGE_EXPR_CHILD,
					target ? asSASTNodeRef::FromExpr(target->id) : asSASTNodeRef());
			}
			const asCType* const targetType =
				context.GetType(targetDecl->type.type);
			if( targetDecl->type.IsReference() || targetType == 0
				|| (targetType->kind != asAST_TYPE_REFERENCE_OBJECT
					&& targetType->kind != asAST_TYPE_FUNCDEF) )
			{
				return Fail(result, asAST_VERIFY_INVALID_TYPE, expr->range,
					"scope-release-cleanup-type", asSASTNodeRef::FromExpr(id),
					asAST_EDGE_EXPR_TYPE,
					asSASTNodeRef::FromType(targetDecl->type.type));
			}
			if( targetDecl->inits.GetLength() == 0 )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, expr->range,
					"scope-release-cleanup-uninitialized",
					asSASTNodeRef::FromExpr(id), asAST_EDGE_DECL_INIT,
					asSASTNodeRef::FromDecl(targetDecl->id));
			}
		}
		if( expr->range.begin.IsValid() && !context.GetSourceManager().IsValidRange(expr->range) )
		{
			return Fail(result, asAST_VERIFY_INVALID_RANGE, expr->range, "expr-range", asSASTNodeRef::FromExpr(id));
		}

		const int expectedChildren = ExpectedExprChildCount(expr->kind);
		if( expectedChildren >= 0 && (int)expr->children.GetLength() != expectedChildren )
		{
			return Fail(result, asAST_VERIFY_INVALID_CHILD, expr->range, "expr-operand", asSASTNodeRef::FromExpr(id), asAST_EDGE_EXPR_CHILD);
		}
		if( expr->kind == asAST_EXPR_CONVERSION
			&& expr->children.GetLength() == 1 )
		{
			const asCExpr* const source =
				context.GetExpr(expr->children[0]);
			if( source != 0 && source->kind == asAST_EXPR_NULL_LITERAL
				&& (!IsVerifiedNullLiteral(context, source)
					|| !IsVerifiedNullableTarget(context, expr->type)
					|| expr->resolvedDecl.IsValid()) )
			{
				return Fail(result, asAST_VERIFY_INVALID_TYPE, expr->range,
					"null-conversion-target",
					asSASTNodeRef::FromExpr(id), asAST_EDGE_EXPR_TYPE,
					asSASTNodeRef::FromType(expr->type.type));
			}
		}
		if( expr->kind == asAST_EXPR_INDEX
			&& expr->children.GetLength() < 2 )
		{
			return Fail(result, asAST_VERIFY_INVALID_CHILD, expr->range, "index-operands", asSASTNodeRef::FromExpr(id), asAST_EDGE_EXPR_CHILD);
		}
		for( asUINT c = 0; c < expr->children.GetLength(); ++c )
		{
			const asCExpr* child = context.GetExpr(expr->children[c]);
			if( child == 0 || child->kind == asAST_EXPR_INVALID )
			{
				return Fail(result, asAST_VERIFY_INVALID_CHILD, expr->range, "expr-operand", asSASTNodeRef::FromExpr(id), asAST_EDGE_EXPR_CHILD, asSASTNodeRef::FromExpr(expr->children[c]));
			}
		}
	}

	const int exprCycleStatus = VerifyStructuralCycles(context, false, result);
	if( exprCycleStatus != asAST_VERIFY_OK )
		return exprCycleStatus;

	if( context.GetLifecycleState() >= asAST_CONTEXT_LIFETIME_PLANNED )
	{
		const int lifetimeStatus = asCASTVerifyLifetimeView(context, result);
		if( lifetimeStatus != asAST_VERIFY_OK ) return lifetimeStatus;
	}

	return asAST_VERIFY_OK;
}

int asCASTVerifyLifetimeView(
	const asCASTContext& context,
	asSAstVerifyResult& result)
{
	result = asSAstVerifyResult();
	asCASTLifetimeView view;
	const int status = asCASTBuildLifetimeView(context, view);
	if( status != asAST_VERIFY_OK )
	{
		return Fail(result, asAST_VERIFY_INVALID_LIFETIME_PROTOCOL,
			asCSourceRange(), "lifetime-derived-view");
	}
	return asAST_VERIFY_OK;
}

int asCASTVerify(const asCASTContext& context, asSAstVerifyResult& result)
{
	const int status = VerifyImpl(context, result);
	if( status != asAST_VERIFY_OK ) EnrichVerifyResult(context, result);
	return status;
}

int asCASTVerifyPublication(const asCASTContext& context, asSAstVerifyResult& result)
{
	result = asSAstVerifyResult();
	if( !context.IsPublishable() )
	{
		const int status = Fail(result, asAST_VERIFY_UNSEALED_PUBLICATION, asCSourceRange(), "unsealed-publication");
		EnrichVerifyResult(context, result);
		return status;
	}
	const int structural = VerifyImpl(context, result);
	if( structural != asAST_VERIFY_OK )
	{
		EnrichVerifyResult(context, result);
		return structural;
	}
	const int requiredTargets = VerifySealedRequiredTargets(context, result);
	if( requiredTargets != asAST_VERIFY_OK ) EnrichVerifyResult(context, result);
	return requiredTargets;
}

END_AS_NAMESPACE
