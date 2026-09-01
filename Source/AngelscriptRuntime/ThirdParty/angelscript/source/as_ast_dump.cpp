#include "as_ast_dump.h"
#include "as_ast_traversal.h"
#include "as_datatype.h"
#include "as_module.h"
#include "as_namespace.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"
#include <cstring>

BEGIN_AS_NAMESPACE

static const char* DeclKindName(asEASTDeclKind kind)
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
	default: return "Decl";
	}
}

static const char* StmtKindName(asEASTStmtKind kind)
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
	default: return "Stmt";
	}
}

static const char* TypeStableKey(const asCASTContext& context, const asCQualType& type)
{
	const asCType* named = context.GetType(type.type);
	if( named && named->stableKey.GetLength() )
	{
		return named->stableKey.AddressOf();
	}
	return "";
}

static const char* ExprKindName(asEASTExprKind kind)
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
	case asAST_EXPR_THIS: return "This";
	case asAST_EXPR_DEFERRED_OUT: return "DeferredOut";
	case asAST_EXPR_CALL_REWRITE: return "CallRewrite";
	default: return "Expr";
	}
}

static asCString ExprLiteralForDump(const asCExpr* expr)
{
	asCString result;
	if( expr == 0 )
	{
		return result;
	}
	if( expr->kind != asAST_EXPR_STRING_LITERAL )
	{
		return expr->literal;
	}

	// A StringLiteral is a semantic byte sequence and may contain embedded NUL.
	// Keep the diagnostic dump single-line, deterministic, and length-aware so
	// the digest/debug tooling cannot silently alias `a\0b` with `a`.
	for( asUINT i = 0; i < expr->literal.GetLength(); ++i )
	{
		const asBYTE byte = static_cast<asBYTE>(expr->literal[i]);
		switch( byte )
		{
		case 0: result += "\\0"; break;
		case '\n': result += "\\n"; break;
		case '\r': result += "\\r"; break;
		case '\t': result += "\\t"; break;
		case '\\': result += "\\\\"; break;
		case '"': result += "\\\""; break;
		default:
			if( byte < 0x20 || byte == 0x7f )
			{
				asCString escaped;
				escaped.Format("\\x%02X", static_cast<unsigned int>(byte));
				result += escaped;
			}
			else
			{
				result += static_cast<char>(byte);
			}
			break;
		}
	}
	return result;
}

static const char* SafePointName(asEASTSafePointRole role)
{
	switch( role )
	{
	case asAST_SAFEPOINT_FUNCTION_ENTRY: return "FunctionEntry";
	case asAST_SAFEPOINT_STATEMENT: return "Statement";
	case asAST_SAFEPOINT_CALL: return "Call";
	case asAST_SAFEPOINT_LOOP_ENTRY: return "LoopEntry";
	case asAST_SAFEPOINT_LOOP_BACKEDGE: return "LoopBackedge";
	case asAST_SAFEPOINT_TRANSFER: return "Transfer";
	case asAST_SAFEPOINT_SWITCH_INVALID_VALUE: return "SwitchInvalidValue";
	case asAST_SAFEPOINT_RETURN: return "Return";
	case asAST_SAFEPOINT_POSITION: return "Position";
	default: return 0;
	}
}

static const char* CallDispatchName(asEASTCallDispatch dispatch)
{
	switch( dispatch )
	{
	case asAST_CALL_DISPATCH_DIRECT: return "direct";
	case asAST_CALL_DISPATCH_VIRTUAL: return "virtual";
	case asAST_CALL_DISPATCH_INDIRECT: return "indirect";
	default: return "none";
	}
}

static const char* CallArgumentOriginName(asEASTCallArgumentOrigin origin)
{
	switch( origin )
	{
	case asAST_CALL_ARGUMENT_POSITIONAL: return "positional";
	case asAST_CALL_ARGUMENT_NAMED: return "named";
	case asAST_CALL_ARGUMENT_DEFAULT: return "default";
	case asAST_CALL_ARGUMENT_HIDDEN: return "hidden";
	case asAST_CALL_ARGUMENT_IMPLICIT_RECEIVER: return "implicit-receiver";
	case asAST_CALL_ARGUMENT_GENERATED: return "generated";
	default: return "invalid";
	}
}

static const char* TypeKindName(asEASTTypeKind kind)
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

static void AppendTypeKind(asCString& line, const asCASTContext& context, const asCQualType& type)
{
	const asCType* named = context.GetType(type.type);
	if( named == 0 )
	{
		return;
	}
	if( line.GetLength() && line[line.GetLength() - 1] == '\n' )
	{
		line.SetLength(line.GetLength() - 1);
	}
	line += " typeKind=";
	line += TypeKindName(named->kind);
	line += "\n";
}

static void AppendSafePoint(asCString& line, asEASTSafePointRole role)
{
	const char* name = SafePointName(role);
	if( name == 0 )
	{
		return;
	}
	if( line.GetLength() && line[line.GetLength() - 1] == '\n' )
	{
		line.SetLength(line.GetLength() - 1);
	}
	line += " safepoint=";
	line += name;
	line += "\n";
}

class asCASTDumpInventoryVisitor : public asIASTConstVisitor
{
public:
	asCArray<asASTDeclId> decls;
	asCArray<asASTStmtId> stmts;
	asCArray<asASTExprId> exprs;

	bool EnterNode(const asCASTContext&, const asSASTNodeRef& node, const asSASTTraversalEdge*, asUINT) override
	{
		switch( node.nodeClass )
		{
		case asAST_NODE_DECL: decls.PushLast(asASTDeclId(node.value, node.snapshotOwner)); break;
		case asAST_NODE_STMT: stmts.PushLast(asASTStmtId(node.value, node.snapshotOwner)); break;
		case asAST_NODE_EXPR: exprs.PushLast(asASTExprId(node.value, node.snapshotOwner)); break;
		default: break;
		}
		return true;
	}
	bool VisitEdge(const asCASTContext&, const asSASTTraversalEdge&, asUINT) override { return true; }
	bool LeaveNode(const asCASTContext&, const asSASTNodeRef&, const asSASTTraversalEdge*, asUINT) override { return true; }
};

namespace
{
const char* StructuredNodeClassName(asEASTNodeClass nodeClass)
{
	switch( nodeClass )
	{
	case asAST_NODE_DECL: return "Decl";
	case asAST_NODE_TYPE: return "Type";
	case asAST_NODE_STMT: return "Stmt";
	case asAST_NODE_EXPR: return "Expr";
	default: return "Invalid";
	}
}

const char* StructuredEdgeRoleName(asEASTEdgeRole role)
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
	default: return "invalid";
	}
}

const char* MethodRelationKindName(asEASTMethodRelationKind kind)
{
	switch( kind )
	{
	case asAST_METHOD_RELATION_BASE_OVERRIDE: return "base-override";
	case asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION: return "interface-implementation";
	default: return "invalid";
	}
}

const char* StructuredNodeKindName(const asCASTContext& context, const asSASTNodeRef& node)
{
	switch( node.nodeClass )
	{
	case asAST_NODE_DECL:
		if( const asCDecl* decl = context.GetDecl(asASTDeclId(node.value, node.snapshotOwner)) )
			return DeclKindName(decl->kind);
		break;
	case asAST_NODE_TYPE:
		if( const asCType* type = context.GetType(asASTTypeRef(node.value, node.snapshotOwner)) )
			return TypeKindName(type->kind);
		break;
	case asAST_NODE_STMT:
		if( const asCStmt* stmt = context.GetStmt(asASTStmtId(node.value, node.snapshotOwner)) )
			return StmtKindName(stmt->kind);
		break;
	case asAST_NODE_EXPR:
		if( const asCExpr* expr = context.GetExpr(asASTExprId(node.value, node.snapshotOwner)) )
			return ExprKindName(expr->kind);
		break;
	default: break;
	}
	return "Invalid";
}

asCString StructuredNodeLabel(const asSASTNodeRef& node)
{
	const char prefix = node.nodeClass == asAST_NODE_DECL ? 'D'
		: node.nodeClass == asAST_NODE_TYPE ? 'T'
		: node.nodeClass == asAST_NODE_STMT ? 'S'
		: node.nodeClass == asAST_NODE_EXPR ? 'E' : 'I';
	asCString result;
	result.Format("%c%u", prefix, node.value);
	return result;
}

const char* StructuredNodeName(const asCASTContext& context, const asSASTNodeRef& node)
{
	if( node.nodeClass == asAST_NODE_DECL )
	{
		const asCDecl* decl = context.GetDecl(asASTDeclId(node.value, node.snapshotOwner));
		return decl ? decl->name.AddressOf() : "";
	}
	return "";
}

const char* StructuredNodeStableKey(const asCASTContext& context, const asSASTNodeRef& node)
{
	if( node.nodeClass == asAST_NODE_DECL )
	{
		const asCDecl* decl = context.GetDecl(asASTDeclId(node.value, node.snapshotOwner));
		return decl ? decl->stableKey.AddressOf() : "";
	}
	if( node.nodeClass == asAST_NODE_TYPE )
	{
		const asCType* type = context.GetType(asASTTypeRef(node.value, node.snapshotOwner));
		return type ? type->stableKey.AddressOf() : "";
	}
	return "";
}

asCQualType StructuredNodeQualType(const asCASTContext& context, const asSASTNodeRef& node)
{
	if( node.nodeClass == asAST_NODE_DECL )
	{
		const asCDecl* decl = context.GetDecl(asASTDeclId(node.value, node.snapshotOwner));
		return decl ? decl->type : asCQualType();
	}
	if( node.nodeClass == asAST_NODE_TYPE )
		return asCQualType(asASTTypeRef(node.value, node.snapshotOwner), 0);
	if( node.nodeClass == asAST_NODE_EXPR )
	{
		const asCExpr* expr = context.GetExpr(asASTExprId(node.value, node.snapshotOwner));
		return expr ? expr->type : asCQualType();
	}
	return asCQualType();
}

asDWORD StructuredNodeTraits(const asCASTContext& context, const asSASTNodeRef& node)
{
	if( node.nodeClass != asAST_NODE_DECL )
		return 0;
	const asCDecl* decl = context.GetDecl(asASTDeclId(node.value, node.snapshotOwner));
	return decl ? decl->traits : 0;
}

asCSourceRange StructuredNodeRange(const asCASTContext& context, const asSASTNodeRef& node)
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

void AppendStructuredEscaped(asCString& out, const char* value)
{
	out += '"';
	const unsigned char* cursor = reinterpret_cast<const unsigned char*>(value ? value : "");
	for( ; *cursor; ++cursor )
	{
		switch( *cursor )
		{
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\b': out += "\\b"; break;
		case '\f': out += "\\f"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if( *cursor < 0x20 )
			{
				asCString escaped;
				escaped.Format("\\u%04x", static_cast<unsigned int>(*cursor));
				out += escaped;
			}
			else
			{
				out += static_cast<char>(*cursor);
			}
			break;
		}
	}
	out += '"';
}

void AppendStructuredMethodRelations(
	asCString& out,
	const asCDecl& decl,
	bool json)
{
	if( json )
	{
		out += ",\"methodRelations\":[";
		for( asUINT index = 0;
			index < decl.methodRelations.GetLength(); ++index )
		{
			if( index != 0 ) out += ",";
			const asSASTMethodRelation& relation =
				decl.methodRelations[index];
			out += "{\"kind\":";
			AppendStructuredEscaped(out, MethodRelationKindName(relation.kind));
			asCString fields;
			fields.Format(
				",\"implementation\":\"D%u\",\"requirement\":\"D%u\"}",
				relation.implementation.value,
				relation.requirement.value);
			out += fields;
		}
		out += "]";
		return;
	}

	out += " methodRelations=[";
	for( asUINT index = 0;
		index < decl.methodRelations.GetLength(); ++index )
	{
		if( index != 0 ) out += ",";
		const asSASTMethodRelation& relation = decl.methodRelations[index];
		out += MethodRelationKindName(relation.kind);
		asCString fields;
		fields.Format(":D%u>D%u",
			relation.implementation.value,
			relation.requirement.value);
		out += fields;
	}
	out += "]";
}

struct asSStructuredSourceFacts
{
	const char* logical;
	int line;
	int column;
	int endLine;
	int endColumn;
	asCString snippet;

	asSStructuredSourceFacts()
		: logical("")
		, line(0)
		, column(0)
		, endLine(0)
		, endColumn(0)
	{
	}
};

asSStructuredSourceFacts StructuredSourceFacts(
	const asCASTContext& context,
	const asSASTNodeRef& node,
	const asSASTStructuredDumpOptions& options)
{
	asSStructuredSourceFacts facts;
	const asCSourceRange range = StructuredNodeRange(context, node);
	const asCSourceManager& sourceManager = context.GetSourceManager();
	if( !sourceManager.IsValidRange(range) )
		return facts;
	facts.logical = sourceManager.GetLogicalKey(range.begin.fileID);
	asSSourceLineColumn begin;
	asSSourceLineColumn end;
	if( sourceManager.GetLineColumn(range.begin, begin) )
	{
		facts.line = begin.row;
		facts.column = begin.column;
	}
	if( sourceManager.GetLineColumn(range.end, end) )
	{
		facts.endLine = end.row;
		facts.endColumn = end.column;
	}
	if( options.includeSourceSnippets && options.maxSnippetBytes != 0 )
	{
		const char* buffer = sourceManager.GetBuffer(range.begin.fileID);
		const asUINT bufferLength = sourceManager.GetBufferLength(range.begin.fileID);
		asUINT endOffset = range.end.offset;
		if( endOffset > bufferLength ) endOffset = bufferLength;
		if( endOffset > range.begin.offset
			&& endOffset - range.begin.offset > options.maxSnippetBytes )
			endOffset = range.begin.offset + options.maxSnippetBytes;
		if( buffer && endOffset > range.begin.offset )
			facts.snippet.Assign(buffer + range.begin.offset, endOffset - range.begin.offset);
	}
	return facts;
}

class asCStructuredNodeMarks
{
public:
	void Initialize(const asCASTContext& context)
	{
		decls.SetLength(context.GetDeclCount() + 1);
		types.SetLength(context.GetTypeCount() + 1);
		stmts.SetLength(context.GetStmtCount() + 1);
		exprs.SetLength(context.GetExprCount() + 1);
		Clear(decls); Clear(types); Clear(stmts); Clear(exprs);
	}

	bool Mark(const asSASTNodeRef& node)
	{
		asCArray<asBYTE>* array = MutableArray(node.nodeClass);
		if( array == 0 || node.value >= array->GetLength() ) return false;
		if( (*array)[node.value] ) return false;
		(*array)[node.value] = 1;
		return true;
	}

	bool Contains(const asSASTNodeRef& node) const
	{
		const asCArray<asBYTE>* array = Array(node.nodeClass);
		return array != 0 && node.value < array->GetLength() && (*array)[node.value] != 0;
	}

	asUINT Count() const
	{
		return CountArray(decls) + CountArray(types) + CountArray(stmts) + CountArray(exprs);
	}

private:
	static void Clear(asCArray<asBYTE>& array)
	{
		for( asUINT i = 0; i < array.GetLength(); ++i ) array[i] = 0;
	}
	static asUINT CountArray(const asCArray<asBYTE>& array)
	{
		asUINT count = 0;
		for( asUINT i = 1; i < array.GetLength(); ++i ) if( array[i] ) ++count;
		return count;
	}
	asCArray<asBYTE>* MutableArray(asEASTNodeClass nodeClass)
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
	const asCArray<asBYTE>* Array(asEASTNodeClass nodeClass) const
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

	asCArray<asBYTE> decls;
	asCArray<asBYTE> types;
	asCArray<asBYTE> stmts;
	asCArray<asBYTE> exprs;
};

class asCStructuredMarkVisitor : public asIASTConstVisitor
{
public:
	explicit asCStructuredMarkVisitor(asCStructuredNodeMarks& inMarks) : marks(inMarks) {}
	bool EnterNode(const asCASTContext&, const asSASTNodeRef& node, const asSASTTraversalEdge*, asUINT) override { marks.Mark(node); return true; }
	bool VisitEdge(const asCASTContext&, const asSASTTraversalEdge&, asUINT) override { return true; }
	bool LeaveNode(const asCASTContext&, const asSASTNodeRef&, const asSASTTraversalEdge*, asUINT) override { return true; }
private:
	asCStructuredNodeMarks& marks;
};

asUINT StructuredTotalNodeCount(const asCASTContext& context)
{
	return context.GetDeclCount() + context.GetTypeCount() + context.GetStmtCount() + context.GetExprCount();
}

asUINT StructuredClassCount(const asCASTContext& context, asEASTNodeClass nodeClass)
{
	switch( nodeClass )
	{
	case asAST_NODE_DECL: return context.GetDeclCount();
	case asAST_NODE_TYPE: return context.GetTypeCount();
	case asAST_NODE_STMT: return context.GetStmtCount();
	case asAST_NODE_EXPR: return context.GetExprCount();
	default: return 0;
	}
}

bool StructuredNodeMatches(
	const asCASTContext& context,
	const asSASTNodeRef& node,
	const asSASTStructuredDumpFilter& filter)
{
	if( filter.nodeClass != asAST_NODE_INVALID && node.nodeClass != filter.nodeClass ) return false;
	if( filter.nodeId != 0 && node.value != filter.nodeId ) return false;
	if( filter.stableDeclKey.GetLength() )
	{
		if( node.nodeClass != asAST_NODE_DECL || filter.stableDeclKey != StructuredNodeStableKey(context, node) ) return false;
	}
	if( filter.nodeKind.GetLength() && filter.nodeKind != StructuredNodeKindName(context, node) ) return false;
	if( filter.logicalSource.GetLength() )
	{
		const asCSourceRange range = StructuredNodeRange(context, node);
		const asCSourceManager& sourceManager = context.GetSourceManager();
		if( !sourceManager.IsValidRange(range) || filter.logicalSource != sourceManager.GetLogicalKey(range.begin.fileID) ) return false;
	}
	return true;
}

int BuildStructuredMarks(
	const asCASTContext& context,
	const asSASTStructuredDumpOptions& options,
	asCStructuredNodeMarks& marks)
{
	marks.Initialize(context);
	const asCDecl* tu = context.GetDecl(context.GetTranslationUnit());
	const char* moduleName = tu ? tu->name.AddressOf() : "";
	if( options.filter.moduleName.GetLength() && options.filter.moduleName != moduleName )
		return asAST_TRAVERSAL_OK;

	if( !options.filter.HasNodePredicates() )
	{
		for( asUINT nodeClass = asAST_NODE_DECL; nodeClass <= asAST_NODE_EXPR; ++nodeClass )
			for( asUINT value = 1; value <= StructuredClassCount(context, static_cast<asEASTNodeClass>(nodeClass)); ++value )
				marks.Mark(asSASTNodeRef(static_cast<asEASTNodeClass>(nodeClass), value));
		return asAST_TRAVERSAL_OK;
	}

	asCASTParentEdgeIndex parents;
	int status = parents.Build(context);
	if( status != asAST_TRAVERSAL_OK ) return status;
	asSASTTraversalOptions traversalOptions;
	traversalOptions.followStructuralEdges = true;
	traversalOptions.followReferences = false;
	traversalOptions.maxDepth = StructuredTotalNodeCount(context) + 1;
	traversalOptions.maxNodes = StructuredTotalNodeCount(context) + 1;
	for( asUINT nodeClass = asAST_NODE_DECL; nodeClass <= asAST_NODE_EXPR; ++nodeClass )
	{
		for( asUINT value = 1; value <= StructuredClassCount(context, static_cast<asEASTNodeClass>(nodeClass)); ++value )
		{
			const asSASTNodeRef node(static_cast<asEASTNodeClass>(nodeClass), value);
			if( !StructuredNodeMatches(context, node, options.filter) ) continue;
			asCStructuredMarkVisitor visitor(marks);
			asSASTTraversalResult result;
			status = asCASTTraverse(context, node, visitor, traversalOptions, result);
			if( status != asAST_TRAVERSAL_OK ) return status;
			asSASTNodeRef ancestor = node;
			asSASTTraversalEdge incoming;
			asUINT parentBudget = StructuredTotalNodeCount(context) + 1;
			while( parents.GetStructuralParent(ancestor, incoming) )
			{
				if( parentBudget-- == 0 ) return asAST_TRAVERSAL_CYCLE_DETECTED;
				marks.Mark(incoming.source);
				ancestor = incoming.source;
			}
		}
	}
	return asAST_TRAVERSAL_OK;
}

void AppendStructuredNodeCore(
	asCString& out,
	const asCASTContext& context,
	const asSASTNodeRef& node,
	const asSASTStructuredDumpOptions& options,
	bool json)
{
	const asCString label = StructuredNodeLabel(node);
	const asCQualType qualType = StructuredNodeQualType(context, node);
	const asCType* type = context.GetType(qualType.type);
	const asSStructuredSourceFacts source = StructuredSourceFacts(context, node, options);
	if( json )
	{
		out += "{\"node\":"; AppendStructuredEscaped(out, label.AddressOf());
		out += ",\"class\":"; AppendStructuredEscaped(out, StructuredNodeClassName(node.nodeClass));
		out += ",\"kind\":"; AppendStructuredEscaped(out, StructuredNodeKindName(context, node));
		out += ",\"name\":"; AppendStructuredEscaped(out, StructuredNodeName(context, node));
		out += ",\"stableKey\":"; AppendStructuredEscaped(out, StructuredNodeStableKey(context, node));
		out += ",\"type\":{\"key\":"; AppendStructuredEscaped(out, type ? type->stableKey.AddressOf() : "");
		out += ",\"kind\":"; AppendStructuredEscaped(out, type ? TypeKindName(type->kind) : "Invalid");
		asCString number;
		number.Format(",\"quals\":%u},\"traits\":%u", qualType.quals, StructuredNodeTraits(context, node));
		out += number;
		if( node.nodeClass == asAST_NODE_DECL )
		{
			const asCDecl* const decl = context.GetDecl(asASTDeclId(node.value));
			if( decl )
			{
				number.Format(",\"layout\":{\"byteOffset\":%d,\"byteSize\":%d,\"byteAlignment\":%u}",
					decl->byteOffset, decl->byteSize, decl->byteAlignment);
				out += number;
				AppendStructuredMethodRelations(out, *decl, true);
			}
		}
		out += ",\"source\":{\"logical\":"; AppendStructuredEscaped(out, source.logical);
		number.Format(",\"line\":%d,\"column\":%d,\"endLine\":%d,\"endColumn\":%d}", source.line, source.column, source.endLine, source.endColumn);
		out += number;
		if( options.includeSourceSnippets )
		{
			out += ",\"snippet\":";
			AppendStructuredEscaped(out, source.snippet.AddressOf());
		}
	}
	else
	{
		out += "node="; out += label;
		out += " class="; out += StructuredNodeClassName(node.nodeClass);
		out += " kind="; out += StructuredNodeKindName(context, node);
		out += " name="; AppendStructuredEscaped(out, StructuredNodeName(context, node));
		out += " key="; AppendStructuredEscaped(out, StructuredNodeStableKey(context, node));
		out += " type="; AppendStructuredEscaped(out, type ? type->stableKey.AddressOf() : "");
		out += " typeKind="; out += type ? TypeKindName(type->kind) : "Invalid";
		asCString number;
		number.Format(" quals=%u traits=%u", qualType.quals, StructuredNodeTraits(context, node));
		out += number;
		if( node.nodeClass == asAST_NODE_DECL )
		{
			const asCDecl* const decl = context.GetDecl(asASTDeclId(node.value));
			if( decl )
			{
				number.Format(" byteOffset=%d byteSize=%d byteAlignment=%u",
					decl->byteOffset, decl->byteSize, decl->byteAlignment);
				out += number;
				AppendStructuredMethodRelations(out, *decl, false);
			}
		}
		out += " source="; AppendStructuredEscaped(out, source.logical);
		number.Format(" line=%d column=%d endLine=%d endColumn=%d", source.line, source.column, source.endLine, source.endColumn);
		out += number;
		if( options.includeSourceSnippets )
		{
			out += " snippet=";
			AppendStructuredEscaped(out, source.snippet.AddressOf());
		}
	}
}

void AppendStructuredEdge(
	asCString& out,
	const asCASTContext& context,
	const asSASTTraversalEdge& edge,
	bool json)
{
	const asCString targetLabel = StructuredNodeLabel(edge.target);
	asCString index;
	if( json )
	{
		out += "{\"role\":"; AppendStructuredEscaped(out, StructuredEdgeRoleName(edge.role));
		out += ",\"edgeClass\":"; AppendStructuredEscaped(out, edge.edgeClass == asAST_EDGE_STRUCTURAL ? "structural" : "reference");
		index.Format(",\"index\":%u,\"target\":", edge.index); out += index; AppendStructuredEscaped(out, targetLabel.AddressOf());
		out += ",\"targetClass\":"; AppendStructuredEscaped(out, StructuredNodeClassName(edge.target.nodeClass));
		out += ",\"targetKind\":"; AppendStructuredEscaped(out, StructuredNodeKindName(context, edge.target));
		out += ",\"targetName\":"; AppendStructuredEscaped(out, StructuredNodeName(context, edge.target));
		out += ",\"targetKey\":"; AppendStructuredEscaped(out, StructuredNodeStableKey(context, edge.target));
		out += "}";
	}
	else
	{
		out += "edge role="; out += StructuredEdgeRoleName(edge.role);
		out += " class="; out += edge.edgeClass == asAST_EDGE_STRUCTURAL ? "structural" : "reference";
		index.Format(" index=%u target=", edge.index); out += index; out += targetLabel;
		out += " targetClass="; out += StructuredNodeClassName(edge.target.nodeClass);
		out += " targetKind="; out += StructuredNodeKindName(context, edge.target);
		out += " targetName="; AppendStructuredEscaped(out, StructuredNodeName(context, edge.target));
		out += " targetKey="; AppendStructuredEscaped(out, StructuredNodeStableKey(context, edge.target));
	}
}

class asCStructuredTreeVisitor : public asIASTConstVisitor
{
public:
	asCStructuredTreeVisitor(
		const asCStructuredNodeMarks& inMarks,
		const asSASTStructuredDumpOptions& inOptions,
		asCString& inOut)
		: marks(inMarks), options(inOptions), out(inOut) {}

	bool EnterNode(const asCASTContext& context, const asSASTNodeRef& node, const asSASTTraversalEdge*, asUINT depth) override
	{
		if( marks.Contains(node) )
		{
			AppendIndent(depth);
			AppendStructuredNodeCore(out, context, node, options, false);
			out += "\n";
		}
		return true;
	}
	bool VisitEdge(const asCASTContext& context, const asSASTTraversalEdge& edge, asUINT depth) override
	{
		if( marks.Contains(edge.source) && (edge.edgeClass == asAST_EDGE_REFERENCE || marks.Contains(edge.target)) )
		{
			AppendIndent(depth + 1);
			AppendStructuredEdge(out, context, edge, false);
			out += "\n";
		}
		return true;
	}
	bool ShouldTraverseEdge(const asCASTContext&, const asSASTTraversalEdge& edge, asUINT) override
	{
		return edge.edgeClass == asAST_EDGE_STRUCTURAL && marks.Contains(edge.source) && marks.Contains(edge.target);
	}
	bool LeaveNode(const asCASTContext&, const asSASTNodeRef&, const asSASTTraversalEdge*, asUINT) override { return true; }

private:
	void AppendIndent(asUINT depth)
	{
		for( asUINT i = 0; i < depth; ++i ) out += "  ";
	}
	const asCStructuredNodeMarks& marks;
	const asSASTStructuredDumpOptions& options;
	asCString& out;
};

int RenderStructuredTree(
	const asCASTContext& context,
	const asSASTStructuredDumpOptions& options,
	const asCStructuredNodeMarks& marks,
	asCString& out)
{
	const asCDecl* tu = context.GetDecl(context.GetTranslationUnit());
	out = "AST_TREE diagnosticOnly=true cacheInput=false schema=as-canonical-ast-diagnostic-v1 module=";
	AppendStructuredEscaped(out, tu ? tu->name.AddressOf() : "");
	asCString count;
	count.Format(" nodes=%u\n", marks.Count());
	out += count;
	asCStructuredTreeVisitor visitor(marks, options, out);
	asSASTTraversalOptions traversalOptions;
	traversalOptions.followStructuralEdges = true;
	traversalOptions.followReferences = false;
	traversalOptions.validateEdgeTargets = true;
	traversalOptions.maxDepth = StructuredTotalNodeCount(context) + 1;
	traversalOptions.maxNodes = StructuredTotalNodeCount(context) + 1;
	asSASTTraversalResult result;
	return asCASTTraverseAll(context, visitor, traversalOptions, result);
}

void RenderStructuredJson(
	const asCASTContext& context,
	const asSASTStructuredDumpOptions& options,
	const asCStructuredNodeMarks& marks,
	asCString& out)
{
	const asCDecl* tu = context.GetDecl(context.GetTranslationUnit());
	out = "{\"schema\":\"as-canonical-ast-diagnostic-v1\",\"diagnosticOnly\":true,\"cacheInput\":false,\"module\":";
	AppendStructuredEscaped(out, tu ? tu->name.AddressOf() : "");
	out += ",\"nodes\":[";
	bool firstNode = true;
	for( asUINT nodeClass = asAST_NODE_DECL; nodeClass <= asAST_NODE_EXPR; ++nodeClass )
	{
		for( asUINT value = 1; value <= StructuredClassCount(context, static_cast<asEASTNodeClass>(nodeClass)); ++value )
		{
			const asSASTNodeRef node(static_cast<asEASTNodeClass>(nodeClass), value);
			if( !marks.Contains(node) ) continue;
			if( !firstNode ) out += ",";
			firstNode = false;
			AppendStructuredNodeCore(out, context, node, options, true);
			out += ",\"edges\":[";
			bool firstEdge = true;
			const asUINT edgeCount = asCASTGetEdgeCount(context, node);
			for( asUINT edgeIndex = 0; edgeIndex < edgeCount; ++edgeIndex )
			{
				asSASTTraversalEdge edge;
				if( !asCASTGetEdge(context, node, edgeIndex, edge) ) continue;
				if( edge.edgeClass == asAST_EDGE_STRUCTURAL && !marks.Contains(edge.target) ) continue;
				if( !firstEdge ) out += ",";
				firstEdge = false;
				AppendStructuredEdge(out, context, edge, true);
			}
			out += "]}";
		}
	}
	out += "]}";
}
}

int asCASTStructuredDump(
	const asCASTContext& context,
	const asSASTStructuredDumpOptions& options,
	asCString& out)
{
	out = "";
	if( !context.IsPublishable() ) return asAST_TRAVERSAL_UNSEALED_CONTEXT;
	asCStructuredNodeMarks marks;
	const int status = BuildStructuredMarks(context, options, marks);
	if( status != asAST_TRAVERSAL_OK ) return status;
	if( options.format == asAST_STRUCTURED_DUMP_JSON )
	{
		RenderStructuredJson(context, options, marks, out);
		return asAST_TRAVERSAL_OK;
	}
	return RenderStructuredTree(context, options, marks, out);
}

int asCASTQuery(
	const asCASTContext& context,
	const asSASTStructuredDumpOptions& options,
	asCString& out)
{
	out = "";
	if( !context.IsPublishable() ) return asAST_TRAVERSAL_UNSEALED_CONTEXT;
	const asCDecl* tu = context.GetDecl(context.GetTranslationUnit());
	const char* moduleName = tu ? tu->name.AddressOf() : "";
	const bool moduleMatches = !options.filter.moduleName.GetLength()
		|| options.filter.moduleName == moduleName;
	asUINT matchCount = 0;
	if( moduleMatches )
	{
		for( asUINT nodeClass = asAST_NODE_DECL; nodeClass <= asAST_NODE_EXPR; ++nodeClass )
		{
			for( asUINT value = 1;
				value <= StructuredClassCount(context, static_cast<asEASTNodeClass>(nodeClass));
				++value )
			{
				const asSASTNodeRef node(static_cast<asEASTNodeClass>(nodeClass), value);
				if( StructuredNodeMatches(context, node, options.filter) ) ++matchCount;
			}
		}
	}

	if( options.format == asAST_STRUCTURED_DUMP_JSON )
	{
		out = "{\"schema\":\"as-canonical-ast-query-v1\",\"diagnosticOnly\":true,\"cacheInput\":false,\"module\":";
		AppendStructuredEscaped(out, moduleName);
		asCString count;
		count.Format(",\"matchCount\":%u,\"matches\":[", matchCount);
		out += count;
	}
	else
	{
		out = "AST_QUERY diagnosticOnly=true cacheInput=false schema=as-canonical-ast-query-v1 module=";
		AppendStructuredEscaped(out, moduleName);
		asCString count;
		count.Format(" matchCount=%u\n", matchCount);
		out += count;
	}

	bool first = true;
	if( moduleMatches )
	{
		for( asUINT nodeClass = asAST_NODE_DECL; nodeClass <= asAST_NODE_EXPR; ++nodeClass )
		{
			for( asUINT value = 1;
				value <= StructuredClassCount(context, static_cast<asEASTNodeClass>(nodeClass));
				++value )
			{
				const asSASTNodeRef node(static_cast<asEASTNodeClass>(nodeClass), value);
				if( !StructuredNodeMatches(context, node, options.filter) ) continue;
				if( options.format == asAST_STRUCTURED_DUMP_JSON )
				{
					if( !first ) out += ",";
					AppendStructuredNodeCore(out, context, node, options, true);
					out += "}";
				}
				else
				{
					AppendStructuredNodeCore(out, context, node, options, false);
					out += "\n";
				}
				first = false;
			}
		}
	}
	if( options.format == asAST_STRUCTURED_DUMP_JSON ) out += "]}";
	return asAST_TRAVERSAL_OK;
}

void asCASTDump(const asCASTContext& context, asCString& out)
{
	out = "AST\n";
	const asCDecl* tu = context.GetDecl(context.GetTranslationUnit());
	if( tu )
	{
		out += "TU name=";
		out += tu->name;
		out += "\n";
	}
	asCASTDumpInventoryVisitor inventory;
	asSASTTraversalOptions inventoryOptions;
	inventoryOptions.followStructuralEdges = false;
	inventoryOptions.validateEdgeTargets = false;
	inventoryOptions.maxDepth = 0;
	inventoryOptions.maxNodes = context.GetDeclCount() + context.GetTypeCount() + context.GetStmtCount() + context.GetExprCount() + 1;
	asSASTTraversalResult inventoryResult;
	(void)asCASTTraverseAll(context, inventory, inventoryOptions, inventoryResult);

	for( asUINT i = 0; i < inventory.decls.GetLength(); ++i )
	{
		const asCDecl* decl = context.GetDecl(inventory.decls[i]);
		if( decl == 0 )
		{
			continue;
		}
		const char* typeKey = "";
		if( const asCType* type = context.GetType(decl->type.type) )
		{
			typeKey = type->stableKey.AddressOf() ? type->stableKey.AddressOf() : "";
		}
		asCString deps;
		for( asUINT d = 0; d < decl->dependencies.GetLength(); ++d )
		{
			if( d )
			{
				deps += ",";
			}
			deps += decl->dependencies[d];
		}
		asCString bases;
		for( asUINT b = 0; b < decl->bases.GetLength(); ++b )
		{
			if( b )
			{
				bases += ",";
			}
			asCString idText;
			idText.Format("%u", decl->bases[b].value);
			bases += idText;
		}
		asCString capturesDump;
		for( asUINT c = 0; c < decl->captures.GetLength(); ++c )
		{
			if( c )
			{
				capturesDump += ",";
			}
			if( const asCDecl* captured = context.GetDecl(decl->captures[c]) )
			{
				capturesDump += captured->name.AddressOf() ? captured->name : "";
			}
		}
		asCString initsDump;
		for( asUINT n = 0; n < decl->inits.GetLength(); ++n )
		{
			if( n )
			{
				initsDump += ",";
			}
			asCString idText;
			idText.Format("%u", decl->inits[n].value);
			initsDump += idText;
		}
		asCString accessorDump;
		if( decl->accessorKind != asAST_ACCESSOR_NONE )
		{
			accessorDump = " accessor=";
			accessorDump += decl->accessorKind == asAST_ACCESSOR_GET ? "Get:" : "Set:";
			if( const asCDecl* field = context.GetDecl(decl->accessorField) )
			{
				accessorDump += field->name.AddressOf() ? field->name.AddressOf() : "";
			}
			else
			{
				accessorDump += "<dangling>";
			}
			asCString idText;
			idText.Format("#%u", decl->accessorField.value);
			accessorDump += idText;
		}
		asCString accessSpecifierDump;
		if( decl->accessSpecifier.IsValid() )
		{
			accessSpecifierDump = " access=";
			if( const asCDecl* specifier = context.GetDecl(decl->accessSpecifier) )
			{
				accessSpecifierDump += specifier->stableKey.GetLength()
					? specifier->stableKey : specifier->name;
			}
			else
			{
				accessSpecifierDump += "<dangling>";
			}
			asCString idText;
			idText.Format("#%u", decl->accessSpecifier.value);
			accessSpecifierDump += idText;
		}
		asCString methodRelationsDump;
		for( asUINT relationIndex = 0;
			relationIndex < decl->methodRelations.GetLength();
			++relationIndex )
		{
			if( relationIndex != 0 ) methodRelationsDump += ",";
			const asSASTMethodRelation& relation =
				decl->methodRelations[relationIndex];
			methodRelationsDump += MethodRelationKindName(relation.kind);
			asCString relationText;
			relationText.Format(":D%u>D%u",
				relation.implementation.value,
				relation.requirement.value);
			methodRelationsDump += relationText;
		}
		asCString line;
		if( capturesDump.GetLength() && initsDump.GetLength() )
		{
			line.Format("DECL id=%u kind=%s name=%s parent=%u key=%s type=%s quals=%u traits=%u origin=%s default=%s deps=%s bases=%s captures=%s init=%s span=%u:%u\n",
				decl->id.value,
				DeclKindName(decl->kind),
				decl->name.AddressOf() ? decl->name.AddressOf() : "",
				decl->parent.value,
				decl->stableKey.AddressOf() ? decl->stableKey.AddressOf() : "",
				typeKey,
				decl->type.quals,
				decl->traits,
				decl->origin.AddressOf() ? decl->origin.AddressOf() : "",
				decl->defaultArg.AddressOf() ? decl->defaultArg.AddressOf() : "",
				deps.AddressOf() ? deps.AddressOf() : "",
				bases.AddressOf() ? bases.AddressOf() : "",
				capturesDump.AddressOf(),
				initsDump.AddressOf(),
				decl->range.begin.offset,
				decl->range.end.offset);
		}
		else if( capturesDump.GetLength() )
		{
			line.Format("DECL id=%u kind=%s name=%s parent=%u key=%s type=%s quals=%u traits=%u origin=%s default=%s deps=%s bases=%s captures=%s span=%u:%u\n",
				decl->id.value,
				DeclKindName(decl->kind),
				decl->name.AddressOf() ? decl->name.AddressOf() : "",
				decl->parent.value,
				decl->stableKey.AddressOf() ? decl->stableKey.AddressOf() : "",
				typeKey,
				decl->type.quals,
				decl->traits,
				decl->origin.AddressOf() ? decl->origin.AddressOf() : "",
				decl->defaultArg.AddressOf() ? decl->defaultArg.AddressOf() : "",
				deps.AddressOf() ? deps.AddressOf() : "",
				bases.AddressOf() ? bases.AddressOf() : "",
				capturesDump.AddressOf(),
				decl->range.begin.offset,
				decl->range.end.offset);
		}
		else if( initsDump.GetLength() )
		{
			line.Format("DECL id=%u kind=%s name=%s parent=%u key=%s type=%s quals=%u traits=%u origin=%s default=%s deps=%s bases=%s init=%s span=%u:%u\n",
				decl->id.value,
				DeclKindName(decl->kind),
				decl->name.AddressOf() ? decl->name.AddressOf() : "",
				decl->parent.value,
				decl->stableKey.AddressOf() ? decl->stableKey.AddressOf() : "",
				typeKey,
				decl->type.quals,
				decl->traits,
				decl->origin.AddressOf() ? decl->origin.AddressOf() : "",
				decl->defaultArg.AddressOf() ? decl->defaultArg.AddressOf() : "",
				deps.AddressOf() ? deps.AddressOf() : "",
				bases.AddressOf() ? bases.AddressOf() : "",
				initsDump.AddressOf(),
				decl->range.begin.offset,
				decl->range.end.offset);
		}
		else if( decl->byteOffset >= 0 )
		{
			line.Format("DECL id=%u kind=%s name=%s parent=%u key=%s type=%s quals=%u traits=%u origin=%s default=%s deps=%s bases=%s offset=%d span=%u:%u\n",
				decl->id.value,
				DeclKindName(decl->kind),
				decl->name.AddressOf() ? decl->name.AddressOf() : "",
				decl->parent.value,
				decl->stableKey.AddressOf() ? decl->stableKey.AddressOf() : "",
				typeKey,
				decl->type.quals,
				decl->traits,
				decl->origin.AddressOf() ? decl->origin.AddressOf() : "",
				decl->defaultArg.AddressOf() ? decl->defaultArg.AddressOf() : "",
				deps.AddressOf() ? deps.AddressOf() : "",
				bases.AddressOf() ? bases.AddressOf() : "",
				decl->byteOffset,
				decl->range.begin.offset,
				decl->range.end.offset);
		}
		else
		{
			line.Format("DECL id=%u kind=%s name=%s parent=%u key=%s type=%s quals=%u traits=%u origin=%s default=%s deps=%s bases=%s span=%u:%u\n",
				decl->id.value,
				DeclKindName(decl->kind),
				decl->name.AddressOf() ? decl->name.AddressOf() : "",
				decl->parent.value,
				decl->stableKey.AddressOf() ? decl->stableKey.AddressOf() : "",
				typeKey,
				decl->type.quals,
				decl->traits,
				decl->origin.AddressOf() ? decl->origin.AddressOf() : "",
				decl->defaultArg.AddressOf() ? decl->defaultArg.AddressOf() : "",
				deps.AddressOf() ? deps.AddressOf() : "",
				bases.AddressOf() ? bases.AddressOf() : "",
				decl->range.begin.offset,
				decl->range.end.offset);
		}
		if( methodRelationsDump.GetLength() )
		{
			line.SetLength(line.GetLength() - 1);
			line += " method-relations=";
			line += methodRelationsDump;
			line += "\n";
		}
		if( accessorDump.GetLength() )
		{
			line.SetLength(line.GetLength() - 1);
			line += accessorDump;
			line += "\n";
		}
		if( accessSpecifierDump.GetLength() )
		{
			line.SetLength(line.GetLength() - 1);
			line += accessSpecifierDump;
			line += "\n";
		}
		if( decl->hasConstantValue )
		{
			line.SetLength(line.GetLength() - 1);
			asCString constantDump;
			constantDump.Format(" constant=%llu", (unsigned long long)decl->constantValue);
			line += constantDump;
			line += "\n";
		}
		if( decl->byteSize >= 0 || decl->byteAlignment != 0 )
		{
			line.SetLength(line.GetLength() - 1);
			asCString layoutDump;
			layoutDump.Format(" layout=%d:%u", decl->byteSize, decl->byteAlignment);
			line += layoutDump;
			line += "\n";
		}
		if( decl->kind == asAST_DECL_PARAM )
		{
			line.SetLength(line.GetLength() - 1);
			asCString formalDump;
			formalDump.Format(" formal=%u", decl->formalIndex);
			line += formalDump;
			line += "\n";
		}
		out += line;
	}

	for( asUINT i = 0; i < inventory.stmts.GetLength(); ++i )
	{
		const asCStmt* stmt = context.GetStmt(inventory.stmts[i]);
		if( stmt == 0 )
		{
			continue;
		}
		asCString childrenDump;
		for( asUINT c = 0; c < stmt->children.GetLength(); ++c )
		{
			if( c )
			{
				childrenDump += ",";
			}
			asCString childId;
			childId.Format("%u", stmt->children[c].value);
			childrenDump += childId;
		}
		asCString line;
		if( stmt->target.IsValid() )
		{
			line.Format("STMT id=%u kind=%s expr=%u children=%s target=%u\n",
				stmt->id.value,
				StmtKindName(stmt->kind),
				stmt->expr.value,
				childrenDump.AddressOf() ? childrenDump.AddressOf() : "",
				stmt->target.value);
		}
		else
		{
			line.Format("STMT id=%u kind=%s expr=%u children=%s\n",
				stmt->id.value,
				StmtKindName(stmt->kind),
				stmt->expr.value,
				childrenDump.AddressOf() ? childrenDump.AddressOf() : "");
		}
		asCString phases;
		if( stmt->kind == asAST_STMT_FOR
			|| (stmt->kind == asAST_STMT_FOREACH
				&& stmt->children.GetLength() >= 3) )
		{
			const asUINT initId = stmt->children.GetLength() > 0 ? stmt->children[0].value : 0;
			const asUINT bodyId = stmt->children.GetLength() > 1 ? stmt->children[1].value : 0;
			const asUINT incrId = stmt->children.GetLength() > 2 ? stmt->children[2].value : 0;
			const asUINT cleanupId = stmt->children.GetLength() > 3 ? stmt->children[3].value : 0;
			if( cleanupId != 0 )
			{
				phases.Format(" init=%u body=%u incr=%u cleanup=%u", initId, bodyId, incrId, cleanupId);
			}
			else
			{
				phases.Format(" init=%u body=%u incr=%u", initId, bodyId, incrId);
			}
		}
		else if( stmt->kind == asAST_STMT_IF )
		{
			const asUINT thenId = stmt->children.GetLength() > 0 ? stmt->children[0].value : 0;
			const asUINT elseId = stmt->children.GetLength() > 1 ? stmt->children[1].value : 0;
			phases.Format(" then=%u else=%u", thenId, elseId);
		}
		else if( stmt->kind == asAST_STMT_WHILE || stmt->kind == asAST_STMT_DO_WHILE )
		{
			const asUINT condId = stmt->expr.value;
			const asUINT bodyId = stmt->children.GetLength() > 0 ? stmt->children[0].value : 0;
			phases.Format(" cond=%u body=%u", condId, bodyId);
		}
		else if( stmt->kind == asAST_STMT_SWITCH )
		{
			phases.Format(" cond=%u", stmt->expr.value);
		}
		else if( stmt->kind == asAST_STMT_FOREACH )
		{
			const asUINT varsId = stmt->children.GetLength() > 0 ? stmt->children[0].value : 0;
			const asUINT bodyId = stmt->children.GetLength() > 1 ? stmt->children[1].value : 0;
			phases.Format(" vars=%u body=%u", varsId, bodyId);
		}
		if( phases.GetLength() )
		{
			if( line.GetLength() && line[line.GetLength() - 1] == '\n' )
			{
				line.SetLength(line.GetLength() - 1);
			}
			line += phases;
			line += "\n";
		}
		AppendSafePoint(line, stmt->safePointRole);
		if( const char* const bodySafePoint =
			SafePointName(stmt->loopBodySafePointRole) )
		{
			if( line.GetLength() && line[line.GetLength() - 1] == '\n' )
			{
				line.SetLength(line.GetLength() - 1);
			}
			line += " body-safepoint=";
			line += bodySafePoint;
			line += "\n";
		}
		out += line;
	}

	for( asUINT i = 0; i < inventory.exprs.GetLength(); ++i )
	{
		const asCExpr* expr = context.GetExpr(inventory.exprs[i]);
		if( expr == 0 )
		{
			continue;
		}
		const asCString literalDump = ExprLiteralForDump(expr);
		asCString callee;
		if( expr->resolvedDecl.IsValid() )
		{
			if( const asCDecl* target = context.GetDecl(expr->resolvedDecl) )
			{
				const bool handleCallee = (expr->kind == asAST_EXPR_CALL
						|| expr->kind == asAST_EXPR_CONSTRUCT
						|| expr->kind == asAST_EXPR_CALL_REWRITE)
					&& (target->kind == asAST_DECL_PARAM || target->kind == asAST_DECL_VAR)
					&& target->name.GetLength() != 0;
				callee = handleCallee
					? target->name
					: (target->stableKey.GetLength() ? target->stableKey : target->name);
			}
		}
		asCString line;
		if( expr->kind == asAST_EXPR_CALL
			|| expr->kind == asAST_EXPR_CONSTRUCT
			|| expr->kind == asAST_EXPR_CALL_REWRITE )
		{
			asCString argsDump;
			for( asUINT c = 0; c < expr->children.GetLength(); ++c )
			{
				if( c )
				{
					argsDump += ",";
				}
				if( const asCExpr* child = context.GetExpr(expr->children[c]) )
				{
					argsDump += ExprLiteralForDump(child);
				}
			}
			const char* route = "";
			if( const asCDecl* target = expr->resolvedDecl.IsValid() ? context.GetDecl(expr->resolvedDecl) : 0 )
			{
				if( target->kind == asAST_DECL_IMPORT )
				{
					route = "import";
				}
			}
			const char* typeKey = TypeStableKey(context, expr->type);
			if( route[0] && expr->receiver.IsValid() )
			{
				line.Format("EXPR id=%u kind=%s type=%s quals=%u literal=%s callee=%s receiver=%u nargs=%u args=%s route=%s\n",
					expr->id.value,
					ExprKindName(expr->kind),
					typeKey,
					expr->type.quals,
					literalDump.AddressOf() ? literalDump.AddressOf() : "",
					callee.AddressOf() ? callee.AddressOf() : "",
					expr->receiver.value,
					expr->children.GetLength(),
					argsDump.AddressOf() ? argsDump.AddressOf() : "",
					route);
			}
			else if( route[0] )
			{
				line.Format("EXPR id=%u kind=%s type=%s quals=%u literal=%s callee=%s nargs=%u args=%s route=%s\n",
					expr->id.value,
					ExprKindName(expr->kind),
					typeKey,
					expr->type.quals,
					literalDump.AddressOf() ? literalDump.AddressOf() : "",
					callee.AddressOf() ? callee.AddressOf() : "",
					expr->children.GetLength(),
					argsDump.AddressOf() ? argsDump.AddressOf() : "",
					route);
			}
			else if( expr->receiver.IsValid() )
			{
				line.Format("EXPR id=%u kind=%s type=%s quals=%u literal=%s callee=%s receiver=%u nargs=%u args=%s\n",
					expr->id.value,
					ExprKindName(expr->kind),
					typeKey,
					expr->type.quals,
					literalDump.AddressOf() ? literalDump.AddressOf() : "",
					callee.AddressOf() ? callee.AddressOf() : "",
					expr->receiver.value,
					expr->children.GetLength(),
					argsDump.AddressOf() ? argsDump.AddressOf() : "");
			}
			else
			{
				line.Format("EXPR id=%u kind=%s type=%s quals=%u literal=%s callee=%s nargs=%u args=%s\n",
					expr->id.value,
					ExprKindName(expr->kind),
					typeKey,
					expr->type.quals,
					literalDump.AddressOf() ? literalDump.AddressOf() : "",
					callee.AddressOf() ? callee.AddressOf() : "",
					expr->children.GetLength(),
					argsDump.AddressOf() ? argsDump.AddressOf() : "");
			}
			if( expr->kind == asAST_EXPR_CALL
				|| expr->kind == asAST_EXPR_CONSTRUCT
				|| expr->kind == asAST_EXPR_CALL_REWRITE )
			{
				if( line.GetLength() && line[line.GetLength() - 1] == '\n' )
				{
					line.SetLength(line.GetLength() - 1);
				}
				if( expr->kind == asAST_EXPR_CALL )
				{
					line += " dispatch=";
					line += CallDispatchName(expr->callDispatch);
				}
				asCString callArgumentCount;
				callArgumentCount.Format(" callArgs=%u", expr->callArguments.GetLength());
				line += callArgumentCount;
				for( asUINT argumentIndex = 0;
					argumentIndex < expr->callArguments.GetLength(); ++argumentIndex )
				{
					const asSASTCallArgument& argument =
						expr->callArguments[argumentIndex];
					const asCDecl* const formal = argument.formal.IsValid()
						? context.GetDecl(argument.formal) : 0;
					asCString record;
					record.Format(
						" callArg%u={expr=%u formal=%s formalId=%u formalIndex=%u formalType=%s origin=%s sourceOrdinal=",
						argumentIndex,
						argument.expression.value,
						formal && formal->name.GetLength()
							? formal->name.AddressOf() : "<indirect>",
						argument.formal.value,
						argument.formalIndex,
						TypeStableKey(context, argument.formalType),
						CallArgumentOriginName(argument.origin));
					line += record;
					if( argument.sourceOrdinal
						== asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL )
					{
						line += "none";
					}
					else
					{
						record.Format("%u", argument.sourceOrdinal);
						line += record;
					}
					line += " authoredName=";
					line += argument.authoredName;
					line += "}";
				}
				line += "\n";
			}
			if( const asCDecl* target = expr->resolvedDecl.IsValid() ? context.GetDecl(expr->resolvedDecl) : 0 )
			{
				if( target->captures.GetLength() )
				{
					asCString capturesDump;
					for( asUINT c = 0; c < target->captures.GetLength(); ++c )
					{
						if( c )
						{
							capturesDump += ",";
						}
						if( const asCDecl* captured = context.GetDecl(target->captures[c]) )
						{
							capturesDump += captured->name.AddressOf() ? captured->name : "";
						}
					}
					if( line.GetLength() && line[line.GetLength() - 1] == '\n' )
					{
						line.SetLength(line.GetLength() - 1);
					}
					line += " captures=";
					line += capturesDump;
					line += "\n";
				}
			}
		}
		else if( expr->kind == asAST_EXPR_CONVERSION )
		{
			const char* destKey = TypeStableKey(context, expr->type);
			const char* srcKey = "";
			if( expr->children.GetLength() )
			{
				if( const asCExpr* child = context.GetExpr(expr->children[0]) )
				{
					srcKey = TypeStableKey(context, child->type);
				}
			}
			line.Format("EXPR id=%u kind=%s type=%s quals=%u dest=%s src=%s literal=%s callee=%s\n",
				expr->id.value,
				ExprKindName(expr->kind),
				destKey,
				expr->type.quals,
				destKey,
				srcKey,
				literalDump.AddressOf() ? literalDump.AddressOf() : "",
				callee.AddressOf() ? callee.AddressOf() : "");
		}
		else
		{
			const char* typeKey = TypeStableKey(context, expr->type);
			const char* lit = literalDump.AddressOf() ? literalDump.AddressOf() : "";
			const char* cal = callee.AddressOf() ? callee.AddressOf() : "";
			if( expr->kind == asAST_EXPR_LOGICAL && expr->children.GetLength() >= 2 )
			{
				line.Format("EXPR id=%u kind=%s type=%s quals=%u literal=%s callee=%s lhs=%u rhs=%u\n",
					expr->id.value,
					ExprKindName(expr->kind),
					typeKey,
					expr->type.quals,
					lit,
					cal,
					expr->children[0].value,
					expr->children[1].value);
			}
			else if( expr->kind == asAST_EXPR_CONDITIONAL && expr->children.GetLength() >= 3 )
			{
				line.Format("EXPR id=%u kind=%s type=%s quals=%u literal=%s callee=%s cond=%u then=%u else=%u\n",
					expr->id.value,
					ExprKindName(expr->kind),
					typeKey,
					expr->type.quals,
					lit,
					cal,
					expr->children[0].value,
					expr->children[1].value,
					expr->children[2].value);
			}
			else if( expr->kind == asAST_EXPR_OPAQUE_VALUE && expr->children.GetLength() >= 1 )
			{
				line.Format("EXPR id=%u kind=%s type=%s quals=%u literal=%s callee=%s source=%u\n",
					expr->id.value,
					ExprKindName(expr->kind),
					typeKey,
					expr->type.quals,
					lit,
					cal,
					expr->children[0].value);
			}
			else if( expr->kind == asAST_EXPR_DEFERRED_OUT )
			{
				line.Format("EXPR id=%u kind=%s type=%s quals=%u literal=%s setter=%s receiver=%u\n",
					expr->id.value,
					ExprKindName(expr->kind),
					typeKey,
					expr->type.quals,
					lit,
					cal,
					expr->receiver.value);
			}
			else if( expr->kind == asAST_EXPR_SEQUENCE )
			{
				asCString parts;
				for( asUINT c = 0; c < expr->children.GetLength(); ++c )
				{
					if( c )
					{
						parts += ",";
					}
					asCString idText;
					idText.Format("%u", expr->children[c].value);
					parts += idText;
				}
				line.Format("EXPR id=%u kind=%s type=%s quals=%u literal=%s callee=%s parts=%s\n",
					expr->id.value,
					ExprKindName(expr->kind),
					typeKey,
					expr->type.quals,
					lit,
					cal,
					parts.AddressOf() ? parts.AddressOf() : "");
			}
			else
			{
				line.Format("EXPR id=%u kind=%s type=%s quals=%u literal=%s callee=%s\n",
					expr->id.value,
					ExprKindName(expr->kind),
					typeKey,
					expr->type.quals,
					lit,
					cal);
			}
		}
		AppendTypeKind(line, context, expr->type);
		AppendSafePoint(line, expr->safePointRole);
		out += line;
	}
}

static asCString ShadowUnsigned(asQWORD value)
{
	asCString result;
	result.Format("%llu", static_cast<unsigned long long>(value));
	return result;
}

static asCString ShadowSigned(int value)
{
	asCString result;
	result.Format("%d", value);
	return result;
}

static asCString ShadowString(const char* value)
{
	asCString result;
	AppendStructuredEscaped(result, value ? value : "");
	return result;
}

static asCString ShadowId(asUINT value)
{
	if( value == 0 )
		return asCString("-");
	return ShadowUnsigned(value);
}

static asCString ShadowRange(const asCSourceRange& range)
{
	asCString result;
	result.Format(
		"%u:%u-%u:%u",
		range.begin.fileID,
		range.begin.offset,
		range.end.fileID,
		range.end.offset);
	return result;
}

static asCString ShadowQualType(const asCQualType& type)
{
	asCString result;
	result.Format("T%u/q%u", type.type.value, type.quals);
	return result;
}

static asCString ShadowNodePath(const asCASTContext& context, const asSASTNodeRef& node)
{
	const asCString fallback = StructuredNodeLabel(node);
	if( !context.IsPublishable() || !node.IsValid() )
		return fallback;

	asCASTParentEdgeIndex index;
	if( index.Build(context) < 0 )
		return fallback;

	asCArray<asSASTNodeRef> nodes;
	asCArray<asSASTTraversalEdge> edges;
	asSASTNodeRef current = node;
	nodes.PushLast(current);
	for( asUINT guard = 0; guard < 1024; ++guard )
	{
		asSASTTraversalEdge parent;
		if( !index.GetStructuralParent(current, parent) )
			break;
		edges.PushLast(parent);
		current = parent.source;
		nodes.PushLast(current);
	}

	asCString result = StructuredNodeLabel(nodes[nodes.GetLength() - 1]);
	for( asUINT reverse = edges.GetLength(); reverse > 0; --reverse )
	{
		const asUINT edgeIndex = reverse - 1;
		const asSASTTraversalEdge& edge = edges[edgeIndex];
		asCString segment;
		segment.Format("/%s[%u]/", StructuredEdgeRoleName(edge.role), edge.index);
		result += segment;
		result += StructuredNodeLabel(nodes[edgeIndex]);
	}
	return result;
}

static bool ShadowMismatch(
	asCString& out,
	const asCString& path,
	const char* field,
	const asCString& leftValue,
	const asCString& rightValue)
{
	out = "mismatch path=";
	out += path;
	out += ".";
	out += field;
	out += " left=";
	out += leftValue;
	out += " right=";
	out += rightValue;
	return true;
}

static bool ShadowUnsignedMismatch(
	asCString& out,
	const asCString& path,
	const char* field,
	asQWORD leftValue,
	asQWORD rightValue)
{
	return leftValue != rightValue
		&& ShadowMismatch(out, path, field, ShadowUnsigned(leftValue), ShadowUnsigned(rightValue));
}

static bool ShadowSignedMismatch(
	asCString& out,
	const asCString& path,
	const char* field,
	int leftValue,
	int rightValue)
{
	return leftValue != rightValue
		&& ShadowMismatch(out, path, field, ShadowSigned(leftValue), ShadowSigned(rightValue));
}

static bool ShadowStringMismatch(
	asCString& out,
	const asCString& path,
	const char* field,
	const asCString& leftValue,
	const asCString& rightValue)
{
	return !leftValue.Equals(rightValue)
		&& ShadowMismatch(out, path, field, ShadowString(leftValue.AddressOf()), ShadowString(rightValue.AddressOf()));
}

static bool ShadowIdMismatch(
	asCString& out,
	const asCString& path,
	const char* field,
	asUINT leftValue,
	asUINT rightValue)
{
	return leftValue != rightValue
		&& ShadowMismatch(out, path, field, ShadowId(leftValue), ShadowId(rightValue));
}

static bool ShadowRangeMismatch(
	asCString& out,
	const asCString& path,
	const asCSourceRange& leftRange,
	const asCSourceRange& rightRange)
{
	return !(leftRange == rightRange)
		&& ShadowMismatch(out, path, "range", ShadowRange(leftRange), ShadowRange(rightRange));
}

static bool ShadowQualTypeMismatch(
	asCString& out,
	const asCString& path,
	const char* field,
	const asCQualType& leftType,
	const asCQualType& rightType)
{
	return leftType != rightType
		&& ShadowMismatch(out, path, field, ShadowQualType(leftType), ShadowQualType(rightType));
}

void asCASTShadowDiff(const asCASTContext& left, const asCASTContext& right, asCString& out)
{
	out = "match";
	const asCString contextPath("context");

	const asCSourceManager& leftSources = left.GetSourceManager();
	const asCSourceManager& rightSources = right.GetSourceManager();
	if( ShadowUnsignedMismatch(out, contextPath, "sourceCount", leftSources.GetSectionCount(), rightSources.GetSectionCount()) ) return;
	for( asUINT i = 0; i < leftSources.GetSectionCount(); ++i )
	{
		const asASTFileID leftFile = leftSources.GetFileIDByIndex(i);
		const asASTFileID rightFile = rightSources.GetFileIDByIndex(i);
		asCString path;
		path.Format("source[%u]", i);
		const asCString leftLogical(leftSources.GetLogicalKey(leftFile));
		const asCString rightLogical(rightSources.GetLogicalKey(rightFile));
		if( ShadowStringMismatch(out, path, "logicalKey", leftLogical, rightLogical) ) return;
		if( ShadowUnsignedMismatch(out, path, "origin", leftSources.GetOrigin(leftFile), rightSources.GetOrigin(rightFile)) ) return;
		if( ShadowSignedMismatch(out, path, "lineOffset", leftSources.GetLineOffset(leftFile), rightSources.GetLineOffset(rightFile)) ) return;
		const asUINT leftLength = leftSources.GetBufferLength(leftFile);
		const asUINT rightLength = rightSources.GetBufferLength(rightFile);
		if( ShadowUnsignedMismatch(out, path, "bufferLength", leftLength, rightLength) ) return;
		const char* leftBuffer = leftSources.GetBuffer(leftFile);
		const char* rightBuffer = rightSources.GetBuffer(rightFile);
		for( asUINT byte = 0; byte < leftLength; ++byte )
		{
			const unsigned char leftByte = leftBuffer ? static_cast<unsigned char>(leftBuffer[byte]) : 0;
			const unsigned char rightByte = rightBuffer ? static_cast<unsigned char>(rightBuffer[byte]) : 0;
			if( leftByte != rightByte )
			{
				asCString bytePath;
				bytePath.Format("source[%u]/byte[%u]", i, byte);
				ShadowUnsignedMismatch(out, bytePath, "value", leftByte, rightByte);
				return;
			}
		}
	}

	if( ShadowIdMismatch(out, contextPath, "translationUnit", left.GetTranslationUnit().value, right.GetTranslationUnit().value) ) return;
	if( ShadowUnsignedMismatch(out, contextPath, "typeCount", left.GetTypeCount(), right.GetTypeCount()) ) return;
	if( ShadowUnsignedMismatch(out, contextPath, "declCount", left.GetDeclCount(), right.GetDeclCount()) ) return;
	if( ShadowUnsignedMismatch(out, contextPath, "stmtCount", left.GetStmtCount(), right.GetStmtCount()) ) return;
	if( ShadowUnsignedMismatch(out, contextPath, "exprCount", left.GetExprCount(), right.GetExprCount()) ) return;

	for( asUINT i = 1; i <= left.GetTypeCount(); ++i )
	{
		const asSASTNodeRef node = asSASTNodeRef::FromType(asASTTypeRef(i));
		const asCString path = ShadowNodePath(left, node);
		const asCType* leftType = left.GetType(asASTTypeRef(i));
		const asCType* rightType = right.GetType(asASTTypeRef(i));
		if( leftType == 0 || rightType == 0 )
		{
			ShadowMismatch(out, path, "present", ShadowUnsigned(leftType != 0), ShadowUnsigned(rightType != 0));
			return;
		}
		if( ShadowIdMismatch(out, path, "id", leftType->id.value, rightType->id.value) ) return;
		if( ShadowUnsignedMismatch(out, path, "kind", leftType->kind, rightType->kind) ) return;
		if( ShadowUnsignedMismatch(out, path, "primitiveToken", leftType->primitiveToken, rightType->primitiveToken) ) return;
		if( ShadowStringMismatch(out, path, "stableKey", leftType->stableKey, rightType->stableKey) ) return;
		if( ShadowUnsignedMismatch(out, path, "requiresRuntimeValidation",
			leftType->requiresRuntimeValidation ? 1u : 0u,
			rightType->requiresRuntimeValidation ? 1u : 0u) ) return;
	}

	for( asUINT i = 1; i <= left.GetDeclCount(); ++i )
	{
		const asSASTNodeRef node = asSASTNodeRef::FromDecl(asASTDeclId(i));
		const asCString path = ShadowNodePath(left, node);
		const asCDecl* leftDecl = left.GetDecl(asASTDeclId(i));
		const asCDecl* rightDecl = right.GetDecl(asASTDeclId(i));
		if( leftDecl == 0 || rightDecl == 0 )
		{
			ShadowMismatch(out, path, "present", ShadowUnsigned(leftDecl != 0), ShadowUnsigned(rightDecl != 0));
			return;
		}
		if( ShadowIdMismatch(out, path, "id", leftDecl->id.value, rightDecl->id.value) ) return;
		if( ShadowUnsignedMismatch(out, path, "kind", leftDecl->kind, rightDecl->kind) ) return;
		if( ShadowIdMismatch(out, path, "parent", leftDecl->parent.value, rightDecl->parent.value) ) return;
		if( ShadowRangeMismatch(out, path, leftDecl->range, rightDecl->range) ) return;
		if( ShadowQualTypeMismatch(out, path, "type", leftDecl->type, rightDecl->type) ) return;
		if( ShadowStringMismatch(out, path, "name", leftDecl->name, rightDecl->name) ) return;
		if( ShadowStringMismatch(out, path, "stableKey", leftDecl->stableKey, rightDecl->stableKey) ) return;
		if( ShadowIdMismatch(out, path, "body", leftDecl->body.value, rightDecl->body.value) ) return;
		if( ShadowUnsignedMismatch(out, path, "traits", leftDecl->traits, rightDecl->traits) ) return;
		if( ShadowUnsignedMismatch(out, path, "accessorKind", leftDecl->accessorKind, rightDecl->accessorKind) ) return;
		if( ShadowIdMismatch(out, path, "accessorField", leftDecl->accessorField.value, rightDecl->accessorField.value) ) return;
		if( ShadowIdMismatch(out, path, "accessSpecifier", leftDecl->accessSpecifier.value, rightDecl->accessSpecifier.value) ) return;
		if( ShadowUnsignedMismatch(out, path, "formalIndex", leftDecl->formalIndex, rightDecl->formalIndex) ) return;
		if( ShadowStringMismatch(out, path, "origin", leftDecl->origin, rightDecl->origin) ) return;
		if( ShadowStringMismatch(out, path, "defaultArg", leftDecl->defaultArg, rightDecl->defaultArg) ) return;
		if( ShadowSignedMismatch(out, path, "byteOffset", leftDecl->byteOffset, rightDecl->byteOffset) ) return;
		if( ShadowSignedMismatch(out, path, "byteSize", leftDecl->byteSize, rightDecl->byteSize) ) return;
		if( ShadowUnsignedMismatch(out, path, "byteAlignment", leftDecl->byteAlignment, rightDecl->byteAlignment) ) return;
		if( ShadowUnsignedMismatch(out, path, "hasConstantValue", leftDecl->hasConstantValue, rightDecl->hasConstantValue) ) return;
		if( ShadowUnsignedMismatch(out, path, "constantValue", leftDecl->constantValue, rightDecl->constantValue) ) return;

		if( ShadowUnsignedMismatch(out, path, "children.count", leftDecl->children.GetLength(), rightDecl->children.GetLength()) ) return;
		for( asUINT n = 0; n < leftDecl->children.GetLength(); ++n )
		{
			asCString field; field.Format("children[%u]", n);
			if( ShadowIdMismatch(out, path, field.AddressOf(), leftDecl->children[n].value, rightDecl->children[n].value) ) return;
		}
		if( ShadowUnsignedMismatch(out, path, "captures.count", leftDecl->captures.GetLength(), rightDecl->captures.GetLength()) ) return;
		for( asUINT n = 0; n < leftDecl->captures.GetLength(); ++n )
		{
			asCString field; field.Format("captures[%u]", n);
			if( ShadowIdMismatch(out, path, field.AddressOf(), leftDecl->captures[n].value, rightDecl->captures[n].value) ) return;
		}
		if( ShadowUnsignedMismatch(out, path, "dependencies.count", leftDecl->dependencies.GetLength(), rightDecl->dependencies.GetLength()) ) return;
		for( asUINT n = 0; n < leftDecl->dependencies.GetLength(); ++n )
		{
			asCString field; field.Format("dependencies[%u]", n);
			if( ShadowStringMismatch(out, path, field.AddressOf(), leftDecl->dependencies[n], rightDecl->dependencies[n]) ) return;
		}
		if( ShadowUnsignedMismatch(out, path, "bases.count", leftDecl->bases.GetLength(), rightDecl->bases.GetLength()) ) return;
		for( asUINT n = 0; n < leftDecl->bases.GetLength(); ++n )
		{
			asCString field; field.Format("bases[%u]", n);
			if( ShadowIdMismatch(out, path, field.AddressOf(), leftDecl->bases[n].value, rightDecl->bases[n].value) ) return;
		}
		if( ShadowUnsignedMismatch(out, path, "methodRelations.count", leftDecl->methodRelations.GetLength(), rightDecl->methodRelations.GetLength()) ) return;
		for( asUINT n = 0; n < leftDecl->methodRelations.GetLength(); ++n )
		{
			asCString field;
			field.Format("methodRelations[%u].kind", n);
			if( ShadowUnsignedMismatch(out, path, field.AddressOf(), leftDecl->methodRelations[n].kind, rightDecl->methodRelations[n].kind) ) return;
			field.Format("methodRelations[%u].implementation", n);
			if( ShadowIdMismatch(out, path, field.AddressOf(), leftDecl->methodRelations[n].implementation.value, rightDecl->methodRelations[n].implementation.value) ) return;
			field.Format("methodRelations[%u].requirement", n);
			if( ShadowIdMismatch(out, path, field.AddressOf(), leftDecl->methodRelations[n].requirement.value, rightDecl->methodRelations[n].requirement.value) ) return;
		}
		if( ShadowUnsignedMismatch(out, path, "inits.count", leftDecl->inits.GetLength(), rightDecl->inits.GetLength()) ) return;
		for( asUINT n = 0; n < leftDecl->inits.GetLength(); ++n )
		{
			asCString field; field.Format("inits[%u]", n);
			if( ShadowIdMismatch(out, path, field.AddressOf(), leftDecl->inits[n].value, rightDecl->inits[n].value) ) return;
		}
	}

	for( asUINT i = 1; i <= left.GetStmtCount(); ++i )
	{
		const asSASTNodeRef node = asSASTNodeRef::FromStmt(asASTStmtId(i));
		const asCString path = ShadowNodePath(left, node);
		const asCStmt* leftStmt = left.GetStmt(asASTStmtId(i));
		const asCStmt* rightStmt = right.GetStmt(asASTStmtId(i));
		if( leftStmt == 0 || rightStmt == 0 )
		{
			ShadowMismatch(out, path, "present", ShadowUnsigned(leftStmt != 0), ShadowUnsigned(rightStmt != 0));
			return;
		}
		if( ShadowIdMismatch(out, path, "id", leftStmt->id.value, rightStmt->id.value) ) return;
		if( ShadowUnsignedMismatch(out, path, "kind", leftStmt->kind, rightStmt->kind) ) return;
		if( ShadowIdMismatch(out, path, "owner", leftStmt->owner.value, rightStmt->owner.value) ) return;
		if( ShadowRangeMismatch(out, path, leftStmt->range, rightStmt->range) ) return;
		if( ShadowIdMismatch(out, path, "target", leftStmt->target.value, rightStmt->target.value) ) return;
		if( ShadowIdMismatch(out, path, "expr", leftStmt->expr.value, rightStmt->expr.value) ) return;
		if( ShadowIdMismatch(out, path, "decl", leftStmt->decl.value, rightStmt->decl.value) ) return;
		if( ShadowUnsignedMismatch(out, path, "safePointRole", leftStmt->safePointRole, rightStmt->safePointRole) ) return;
		if( ShadowUnsignedMismatch(out, path, "loopBodySafePointRole", leftStmt->loopBodySafePointRole, rightStmt->loopBodySafePointRole) ) return;
		if( ShadowUnsignedMismatch(out, path, "children.count", leftStmt->children.GetLength(), rightStmt->children.GetLength()) ) return;
		for( asUINT n = 0; n < leftStmt->children.GetLength(); ++n )
		{
			asCString field; field.Format("children[%u]", n);
			if( ShadowIdMismatch(out, path, field.AddressOf(), leftStmt->children[n].value, rightStmt->children[n].value) ) return;
		}
	}

	for( asUINT i = 1; i <= left.GetExprCount(); ++i )
	{
		const asSASTNodeRef node = asSASTNodeRef::FromExpr(asASTExprId(i));
		const asCString path = ShadowNodePath(left, node);
		const asCExpr* leftExpr = left.GetExpr(asASTExprId(i));
		const asCExpr* rightExpr = right.GetExpr(asASTExprId(i));
		if( leftExpr == 0 || rightExpr == 0 )
		{
			ShadowMismatch(out, path, "present", ShadowUnsigned(leftExpr != 0), ShadowUnsigned(rightExpr != 0));
			return;
		}
		if( ShadowIdMismatch(out, path, "id", leftExpr->id.value, rightExpr->id.value) ) return;
		if( ShadowUnsignedMismatch(out, path, "kind", leftExpr->kind, rightExpr->kind) ) return;
		if( ShadowRangeMismatch(out, path, leftExpr->range, rightExpr->range) ) return;
		if( ShadowQualTypeMismatch(out, path, "type", leftExpr->type, rightExpr->type) ) return;
		if( ShadowUnsignedMismatch(out, path, "valueCategory", leftExpr->valueCategory, rightExpr->valueCategory) ) return;
		if( ShadowIdMismatch(out, path, "resolvedDecl", leftExpr->resolvedDecl.value, rightExpr->resolvedDecl.value) ) return;
		if( ShadowIdMismatch(out, path, "receiver", leftExpr->receiver.value, rightExpr->receiver.value) ) return;
		if( ShadowStringMismatch(out, path, "literal", leftExpr->literal, rightExpr->literal) ) return;
		if( ShadowUnsignedMismatch(out, path, "literalBits", leftExpr->literalBits, rightExpr->literalBits) ) return;
		if( ShadowUnsignedMismatch(out, path, "safePointRole", leftExpr->safePointRole, rightExpr->safePointRole) ) return;
		if( ShadowUnsignedMismatch(out, path, "callDispatch", leftExpr->callDispatch, rightExpr->callDispatch) ) return;
		if( ShadowUnsignedMismatch(out, path, "children.count", leftExpr->children.GetLength(), rightExpr->children.GetLength()) ) return;
		for( asUINT n = 0; n < leftExpr->children.GetLength(); ++n )
		{
			asCString field; field.Format("children[%u]", n);
			if( ShadowIdMismatch(out, path, field.AddressOf(), leftExpr->children[n].value, rightExpr->children[n].value) ) return;
		}
	}
}

static const char* VerifyCategoryName(asEASTVerifyCategory category)
{
	switch( category )
	{
	case asAST_VERIFY_OK: return "OK";
	case asAST_VERIFY_DANGLING_ID: return "DANGLING_ID";
	case asAST_VERIFY_FOREIGN_ID: return "FOREIGN_ID";
	case asAST_VERIFY_WRONG_KIND: return "WRONG_KIND";
	case asAST_VERIFY_INVALID_TYPE: return "INVALID_TYPE";
	case asAST_VERIFY_INVALID_RANGE: return "INVALID_RANGE";
	case asAST_VERIFY_INVALID_CHILD: return "INVALID_CHILD";
	case asAST_VERIFY_POST_SEAL_MUTATION: return "POST_SEAL_MUTATION";
	case asAST_VERIFY_UNSEALED_PUBLICATION: return "UNSEALED_PUBLICATION";
	case asAST_VERIFY_INVALID_LIFECYCLE_TRANSITION: return "INVALID_LIFECYCLE_TRANSITION";
	case asAST_VERIFY_INVALID_LIFETIME_PROTOCOL: return "INVALID_LIFETIME_PROTOCOL";
	default: return "UNKNOWN";
	}
}

void asCASTFormatVerifyResult(const asSAstVerifyResult& result, asCString& out)
{
	const char* category = VerifyCategoryName(result.category);
	const char* detail = result.detail.AddressOf() ? result.detail.AddressOf() : "";
	asUINT beginFile = 0;
	asUINT beginOffset = 0;
	asUINT endFile = 0;
	asUINT endOffset = 0;
	if( result.range.IsValid() )
	{
		beginFile = result.range.begin.fileID;
		beginOffset = result.range.begin.offset;
		endFile = result.range.end.fileID;
		endOffset = result.range.end.offset;
	}
	out.Format("VERIFY category=%s detail=%s range=%u:%u-%u:%u\n",
		category,
		detail,
		beginFile,
		beginOffset,
		endFile,
		endOffset);
	const bool enriched = result.offendingNode.IsValid()
		|| result.relatedNode.IsValid()
		|| result.edgeRole != asAST_EDGE_INVALID
		|| result.logicalSource.GetLength()
		|| result.rootPath.GetLength()
		|| result.localSubtree.GetLength();
	if( !enriched )
		return;
	if( out.GetLength() && out[out.GetLength() - 1] == '\n' ) out.SetLength(out.GetLength() - 1);
	out += " node=";
	out += result.offendingNode.IsValid() ? StructuredNodeLabel(result.offendingNode) : asCString("-");
	out += " kind=";
	out += result.offendingKind.GetLength() ? result.offendingKind : asCString("Invalid");
	out += " edge=";
	out += result.edgeRole != asAST_EDGE_INVALID ? StructuredEdgeRoleName(result.edgeRole) : "none";
	out += " related=";
	out += result.relatedNode.IsValid() ? StructuredNodeLabel(result.relatedNode) : asCString("-");
	out += " relatedKind=";
	out += result.relatedKind.GetLength() ? result.relatedKind : asCString("Invalid");
	out += " source=";
	AppendStructuredEscaped(out, result.logicalSource.AddressOf());
	asCString position;
	position.Format(" line=%d column=%d path=", result.line, result.column);
	out += position;
	AppendStructuredEscaped(out, result.rootPath.AddressOf());
	out += "\n";
	if( result.localSubtree.GetLength() ) out += result.localSubtree;
}

void asCASTDumpOnVerifyFail(const asCASTContext& context, const asSAstVerifyResult& result, asCString& out)
{
	asCASTFormatVerifyResult(result, out);
	if( result.category == asAST_VERIFY_OK )
	{
		return;
	}
	asCString dump;
	asCASTDump(context, dump);
	out += dump;
}

static asCString BuilderOwnerIdentity(const asCScriptFunction& function)
{
	if( function.objectType != 0 && function.objectType->GetName() )
	{
		asCString owner("class:");
		owner += function.objectType->GetName();
		return owner;
	}
	if( function.nameSpace != 0 && function.nameSpace->name.GetLength() != 0 )
	{
		asCString owner("ns:");
		owner += function.nameSpace->name;
		return owner;
	}
	return asCString("tu");
}

static asCString CanonicalNamespaceQualifiedName(
	const asCASTContext& canonical,
	const asCDecl& namespaceDecl)
{
	asCArray<const asCDecl*> scopes;
	const asCDecl* scope = &namespaceDecl;
	while( scope != 0 && scope->kind == asAST_DECL_NAMESPACE )
	{
		scopes.PushLast(scope);
		scope = scope->parent.IsValid() ? canonical.GetDecl(scope->parent) : 0;
	}
	asCString qualified;
	for( asUINT index = scopes.GetLength(); index > 0; --index )
	{
		const asCDecl* const segment = scopes[index - 1];
		if( segment == 0 || segment->name.GetLength() == 0 )
		{
			continue;
		}
		if( qualified.GetLength() != 0 )
		{
			qualified += "::";
		}
		qualified += segment->name;
	}
	return qualified;
}

static asCString CanonicalOwnerIdentity(
	const asCASTContext& canonical,
	const asCDecl& declaration)
{
	const asCDecl* const parent = canonical.GetDecl(declaration.parent);
	if( parent == 0 )
	{
		return asCString();
	}
	if( parent->kind == asAST_DECL_TRANSLATION_UNIT )
	{
		return asCString("tu");
	}
	if( parent->kind == asAST_DECL_NAMESPACE )
	{
		asCString owner("ns:");
		owner += CanonicalNamespaceQualifiedName(canonical, *parent);
		return owner;
	}
	asCString owner("class:");
	owner += parent->name;
	return owner;
}

static asDWORD BuilderTraitIdentity(const asCScriptFunction& function)
{
	asDWORD traits = asAST_TRAIT_NONE;
	if( function.traits.GetTrait(asTRAIT_PRIVATE) ) traits |= asAST_TRAIT_PRIVATE;
	if( function.traits.GetTrait(asTRAIT_PROTECTED) ) traits |= asAST_TRAIT_PROTECTED;
	if( function.traits.GetTrait(asTRAIT_CONST) ) traits |= asAST_TRAIT_CONST_METHOD;
	if( function.traits.GetTrait(asTRAIT_OVERRIDE) ) traits |= asAST_TRAIT_OVERRIDE;
	if( function.traits.GetTrait(asTRAIT_FINAL) ) traits |= asAST_TRAIT_FINAL;
	if( function.traits.GetTrait(asTRAIT_SHARED) ) traits |= asAST_TRAIT_SHARED;
	if( function.traits.GetTrait(asTRAIT_EXTERNAL) ) traits |= asAST_TRAIT_EXTERNAL;
	if( function.traits.GetTrait(asTRAIT_MIXIN) ) traits |= asAST_TRAIT_MIXIN;
	if( function.traits.GetTrait(asTRAIT_GENERATED_FUNCTION) ) traits |= asAST_TRAIT_GENERATED;
	if( function.traits.GetTrait(asTRAIT_EXTERNAL_IMPLICIT_THIS) )
		traits |= asAST_TRAIT_EXTERNAL_IMPLICIT_THIS;
	return traits;
}

static asCString BuilderTypeKey(const asCScriptFunction& function, asDWORD& outQuals)
{
	outQuals = 0;
	asCDataType dataType = function.returnType;
	if( dataType.IsObjectConst() || dataType.IsReadOnly() )
	{
		outQuals |= asAST_QUAL_CONST;
	}
	if( dataType.IsObjectHandle() )
	{
		outQuals |= asAST_QUAL_HANDLE;
	}
	if( dataType.IsReference() )
	{
		outQuals |= asAST_QUAL_REFERENCE;
	}
	dataType.MakeConstValueOrConstObject(false);
	if( dataType.IsObjectHandle() )
	{
		dataType.MakeHandleToConst(false);
	}
	dataType.MakeReadOnly(false);
	dataType.MakeHandle(false);
	dataType.MakeReference(false);
	asSNameSpace* nameSpace = 0;
	if( function.engine != 0 && function.engine->nameSpaces.GetLength() != 0 )
	{
		nameSpace = function.engine->nameSpaces[0];
	}
	return dataType.Format(nameSpace, true);
}

// Product `float` is float64. Canonical intern uses the resolved token key
// (`float64`); Runtime Format() still spells the language name (`float`).
// float32 remains a distinct width and must not alias either.
static bool CanonicalFloatTypeFactAliasesBuilder(
	const asCString& canonicalFact,
	const asCString& builderFact)
{
	const char* const left = canonicalFact.AddressOf();
	const char* const right = builderFact.AddressOf();
	if( left == 0 || right == 0 )
	{
		return false;
	}
	const char* leftQual = strrchr(left, '/');
	const char* rightQual = strrchr(right, '/');
	if( leftQual == 0 || rightQual == 0 || strcmp(leftQual, rightQual) != 0 )
	{
		return false;
	}
	const size_t leftLen = static_cast<size_t>(leftQual - left);
	const size_t rightLen = static_cast<size_t>(rightQual - right);
	const auto isDefaultFloat = [](const char* key, size_t len) -> bool
	{
		return (len == 5 && strncmp(key, "float", 5) == 0)
			|| (len == 6 && strncmp(key, "double", 6) == 0)
			|| (len == 7 && strncmp(key, "float64", 7) == 0);
	};
	return isDefaultFloat(left, leftLen) && isDefaultFloat(right, rightLen);
}

static void BuilderSourceIdentity(
	const asCScriptFunction& function,
	asCString& outSection,
	int& outRow,
	int& outColumn)
{
	outSection = function.GetScriptSectionName() ? function.GetScriptSectionName() : "";
	outRow = 0;
	outColumn = 0;
	if( function.scriptData != 0 )
	{
		outRow = function.scriptData->declaredAt & 0xFFFFF;
		outColumn = (function.scriptData->declaredAt >> 20) & 0xFFF;
	}
}

static void CanonicalSourceIdentity(
	const asCASTContext& canonical,
	const asCDecl& declaration,
	asCString& outSection,
	int& outRow,
	int& outColumn)
{
	const asCSourceManager& sources = canonical.GetSourceManager();
	outSection = sources.GetLogicalKey(declaration.range.begin.fileID);
	asSSourceLineColumn lineColumn;
	if( sources.GetLineColumn(declaration.range.begin, lineColumn) )
	{
		outRow = lineColumn.row;
		outColumn = lineColumn.column;
		return;
	}
	outRow = -1;
	outColumn = -1;
}

static asCString BuilderImportOrigin(const asCScriptFunction& function)
{
	if( function.funcType != asFUNC_IMPORTED || function.engine == 0 )
	{
		return asCString();
	}
	for( asUINT index = 0; index < function.engine->importedFunctions.GetLength(); ++index )
	{
		sBindInfo* const info = function.engine->importedFunctions[index];
		if( info != 0 && info->importedFunctionSignature == &function )
		{
			return info->importFromModule;
		}
	}
	return asCString();
}

static asCString BuilderDependencyIdentity(const asCScriptFunction& function)
{
	asCString joined;
	if( function.scriptData != 0 )
	{
		for( asUINT index = 0;
			index < function.scriptData->artifactDependencies.GetLength();
			++index )
		{
			const asSBuildArtifactDependency& dependency =
				function.scriptData->artifactDependencies[index];
			asCString key;
			if( dependency.type != 0 && dependency.type->GetName() )
			{
				key = dependency.type->GetName();
			}
			else if( dependency.function != 0 && dependency.function->GetName() )
			{
				key = dependency.function->GetName();
			}
			else if( dependency.globalProperty != 0 )
			{
				key = dependency.globalProperty->name;
			}
			if( key.GetLength() == 0 )
			{
				continue;
			}
			if( joined.GetLength() != 0 )
			{
				joined += ",";
			}
			joined += key;
		}
	}
	const asCString origin = BuilderImportOrigin(function);
	if( origin.GetLength() != 0 )
	{
		if( joined.GetLength() != 0 )
		{
			joined += ",";
		}
		joined += origin;
	}
	return joined;
}

static asCString CanonicalDependencyIdentity(const asCDecl& declaration)
{
	asCString joined;
	for( asUINT index = 0; index < declaration.dependencies.GetLength(); ++index )
	{
		if( index != 0 )
		{
			joined += ",";
		}
		joined += declaration.dependencies[index];
	}
	return joined;
}

void asCASTShadowDiffBuilderFunctionIdentity(
	const asCASTContext& canonical,
	asASTDeclId canonicalDecl,
	const asCScriptFunction& builderFunction,
	asCString& out,
	bool compareDependencies)
{
	out = "match";
	const asCDecl* const declaration = canonical.GetDecl(canonicalDecl);
	if( declaration == 0 )
	{
		ShadowMismatch(out, asCString("decl"), "present", asCString("0"), asCString("1"));
		return;
	}

	const asCString path("decl");
	if( ShadowStringMismatch(out, path, "parent",
		CanonicalOwnerIdentity(canonical, *declaration),
		BuilderOwnerIdentity(builderFunction)) )
	{
		return;
	}

	const asCType* const canonicalType = canonical.GetType(declaration->type.type);
	asDWORD builderQuals = 0;
	const asCString builderType = BuilderTypeKey(builderFunction, builderQuals);
	asCString canonicalTypeKey = canonicalType ? canonicalType->stableKey : asCString();
	asCString canonicalTypeFact;
	canonicalTypeFact.Format("%s/q%u",
		canonicalTypeKey.AddressOf() ? canonicalTypeKey.AddressOf() : "",
		declaration->type.quals);
	asCString builderTypeFact;
	builderTypeFact.Format("%s/q%u",
		builderType.AddressOf() ? builderType.AddressOf() : "",
		builderQuals);
	if( !canonicalTypeFact.Equals(builderTypeFact)
		&& !CanonicalFloatTypeFactAliasesBuilder(canonicalTypeFact, builderTypeFact)
		&& ShadowStringMismatch(out, path, "type", canonicalTypeFact, builderTypeFact) )
	{
		return;
	}

	if( ShadowUnsignedMismatch(out, path, "traits",
		declaration->traits, BuilderTraitIdentity(builderFunction)) )
	{
		return;
	}

	asCString canonicalSection;
	int canonicalRow = 0;
	int canonicalColumn = 0;
	CanonicalSourceIdentity(canonical, *declaration, canonicalSection, canonicalRow, canonicalColumn);
	asCString builderSection;
	int builderRow = 0;
	int builderColumn = 0;
	BuilderSourceIdentity(builderFunction, builderSection, builderRow, builderColumn);
	asCString canonicalRange;
	canonicalRange.Format("%s:%d:%d",
		canonicalSection.AddressOf() ? canonicalSection.AddressOf() : "",
		canonicalRow, canonicalColumn);
	asCString builderRange;
	builderRange.Format("%s:%d:%d",
		builderSection.AddressOf() ? builderSection.AddressOf() : "",
		builderRow, builderColumn);
	if( ShadowStringMismatch(out, path, "range", canonicalRange, builderRange) )
	{
		return;
	}

	if( compareDependencies
		&& ShadowStringMismatch(out, path, "dependencies",
			CanonicalDependencyIdentity(*declaration),
			BuilderDependencyIdentity(builderFunction)) )
	{
		return;
	}
}

int asCASTValidateBuilderShadowIdentity(
	const asCASTContext& canonical,
	asCModule& candidate,
	asCScriptEngine* engine)
{
	for( asUINT index = 0; index < candidate.globalFunctionList.GetLength(); ++index )
	{
		asCScriptFunction* const function = candidate.globalFunctionList[index];
		if( function == 0
			|| function->funcType != asFUNC_SCRIPT
			|| function->objectType != 0
			|| function->name.GetLength() == 0
			|| function->name[0] == '$' )
		{
			continue;
		}
		asCString diff;
		asASTDeclId declarationId;
		bool matched = false;
		declarationId.value = 0;
		for( asUINT declIndex = 1; declIndex <= canonical.GetDeclCount(); ++declIndex )
		{
			const asCDecl* const candidateDecl =
				canonical.GetDecl(asASTDeclId(declIndex));
			if( candidateDecl == 0
				|| candidateDecl->kind != asAST_DECL_FUNCTION
				|| (candidateDecl->traits & asAST_TRAIT_GENERATED) != 0
				|| (candidateDecl->traits & asAST_TRAIT_LAMBDA) != 0
				|| !candidateDecl->name.Equals(function->name) )
			{
				continue;
			}
			asCString candidateDiff;
			asCASTShadowDiffBuilderFunctionIdentity(
				canonical, candidateDecl->id, *function, candidateDiff, false);
			if( candidateDiff.Equals("match") )
			{
				matched = true;
				declarationId = candidateDecl->id;
				diff = candidateDiff;
				break;
			}
			declarationId = candidateDecl->id;
			diff = candidateDiff;
		}
		if( !declarationId.IsValid() )
		{
			continue;
		}
		if( matched )
		{
			continue;
		}
		asCString message;
		message.Format("shadow-declaration-mismatch %s",
			diff.AddressOf() ? diff.AddressOf() : "mismatch");
		if( engine != 0 )
		{
			const asCDecl* const declaration = canonical.GetDecl(declarationId);
			asCString sectionName;
			int row = 0;
			int column = 0;
			if( declaration != 0 )
			{
				CanonicalSourceIdentity(
					canonical, *declaration, sectionName, row, column);
			}
			engine->WriteMessage(
				sectionName.AddressOf() ? sectionName.AddressOf() : "",
				row,
				column,
				asMSGTYPE_ERROR,
				message.AddressOf());
		}
		return asINVALID_DECLARATION;
	}
	return asSUCCESS;
}

static asCString BuilderTypeOwnerIdentity(const asCTypeInfo& type)
{
	if( type.nameSpace != 0 && type.nameSpace->name.GetLength() != 0 )
	{
		asCString owner("ns:");
		owner += type.nameSpace->name;
		return owner;
	}
	return asCString("tu");
}

static asDWORD BuilderTypeTraitIdentity(const asCTypeInfo& type)
{
	asDWORD traits = asAST_TRAIT_NONE;
	if( type.flags & asOBJ_SHARED ) traits |= asAST_TRAIT_SHARED;
	if( type.flags & asOBJ_ENUM )
	{
		return traits;
	}
	if( type.flags & asOBJ_VALUE ) traits |= asAST_TRAIT_VALUE;
	// Script structs always set asOBJ_NOINHERIT; that is the value-type
	// layout, not the language `final` trait. Map NOINHERIT to FINAL only
	// for reference classes.
	if( (type.flags & asOBJ_VALUE) == 0 && (type.flags & asOBJ_NOINHERIT) )
		traits |= asAST_TRAIT_FINAL;
	return traits;
}

static asCString BuilderObjectTypeKey(const asCTypeInfo& type, asDWORD& outQuals)
{
	outQuals = 0;
	if( (type.flags & asOBJ_ENUM) == 0 && (type.flags & asOBJ_VALUE) == 0 )
	{
		outQuals |= asAST_QUAL_HANDLE;
	}
	return type.name;
}

static asEASTTypeKind BuilderObjectTypeKind(const asCTypeInfo& type)
{
	if( type.flags & asOBJ_ENUM )
	{
		return asAST_TYPE_ENUM;
	}
	if( type.flags & asOBJ_VALUE )
	{
		return asAST_TYPE_VALUE_OBJECT;
	}
	return asAST_TYPE_REFERENCE_OBJECT;
}

static void BuilderTypeSourceIdentity(
	const asCTypeInfo& type,
	asCString& outSection,
	int& outRow,
	int& outColumn)
{
	outSection = "";
	if( type.engine != 0
		&& type.scriptSectionIdx >= 0
		&& type.scriptSectionIdx < (int)type.engine->scriptSectionNames.GetLength()
		&& type.engine->scriptSectionNames[type.scriptSectionIdx] != 0 )
	{
		outSection = *type.engine->scriptSectionNames[type.scriptSectionIdx];
	}
	outRow = type.declaredAt & 0xFFFFF;
	outColumn = (type.declaredAt >> 20) & 0xFFF;
}

static asCString BuilderTypeDependencyIdentity(const asCTypeInfo& type)
{
	asCString joined;
	if( type.flags & asOBJ_ENUM )
	{
		return joined;
	}
	const asCObjectType* const objectType =
		CastToObjectType(const_cast<asCTypeInfo*>(&type));
	if( objectType == 0 )
	{
		return joined;
	}
	if( objectType->derivedFrom != 0 && objectType->derivedFrom->name.GetLength() != 0 )
	{
		joined = objectType->derivedFrom->name;
	}
	for( asUINT index = 0; index < objectType->interfaces.GetLength(); ++index )
	{
		const asCObjectType* const iface = objectType->interfaces[index];
		if( iface == 0 || iface->name.GetLength() == 0 )
		{
			continue;
		}
		if( joined.GetLength() != 0 )
		{
			joined += ",";
		}
		joined += iface->name;
	}
	return joined;
}

static const char* BuilderTypeDeclKindName(const asCTypeInfo& type)
{
	if( type.flags & asOBJ_ENUM )
	{
		return "Enum";
	}
	const asCObjectType* const objectType =
		CastToObjectType(const_cast<asCTypeInfo*>(&type));
	if( objectType != 0 && objectType->IsInterface() )
	{
		return "Interface";
	}
	return "Class";
}

static bool CanonicalDeclKindMatchesBuilderType(
	const asCDecl& declaration,
	const asCTypeInfo& type)
{
	if( type.flags & asOBJ_ENUM )
	{
		return declaration.kind == asAST_DECL_ENUM;
	}
	const asCObjectType* const objectType =
		CastToObjectType(const_cast<asCTypeInfo*>(&type));
	if( objectType != 0 && objectType->IsInterface() )
	{
		return declaration.kind == asAST_DECL_INTERFACE;
	}
	return declaration.kind == asAST_DECL_CLASS;
}

void asCASTShadowDiffBuilderTypeIdentity(
	const asCASTContext& canonical,
	asASTDeclId canonicalDecl,
	const asCTypeInfo& builderType,
	asCString& out)
{
	out = "match";
	const asCDecl* const declaration = canonical.GetDecl(canonicalDecl);
	if( declaration == 0 )
	{
		ShadowMismatch(out, asCString("decl"), "present", asCString("0"), asCString("1"));
		return;
	}

	const asCString path("decl");
	if( !CanonicalDeclKindMatchesBuilderType(*declaration, builderType) )
	{
		ShadowMismatch(out, path, "kind",
			asCString(DeclKindName(declaration->kind)),
			asCString(BuilderTypeDeclKindName(builderType)));
		return;
	}

	if( ShadowStringMismatch(out, path, "parent",
		CanonicalOwnerIdentity(canonical, *declaration),
		BuilderTypeOwnerIdentity(builderType)) )
	{
		return;
	}

	const asCType* const canonicalType = canonical.GetType(declaration->type.type);
	asDWORD builderQuals = 0;
	const asCString builderTypeKey = BuilderObjectTypeKey(builderType, builderQuals);
	asCString canonicalTypeFact;
	canonicalTypeFact.Format("%u:%s/q%u",
		canonicalType ? (unsigned)canonicalType->kind : 0u,
		canonicalType && canonicalType->stableKey.AddressOf()
			? canonicalType->stableKey.AddressOf() : "",
		declaration->type.quals);
	asCString builderTypeFact;
	builderTypeFact.Format("%u:%s/q%u",
		(unsigned)BuilderObjectTypeKind(builderType),
		builderTypeKey.AddressOf() ? builderTypeKey.AddressOf() : "",
		builderQuals);
	if( ShadowStringMismatch(out, path, "type", canonicalTypeFact, builderTypeFact) )
	{
		return;
	}

	if( ShadowUnsignedMismatch(out, path, "traits",
		declaration->traits, BuilderTypeTraitIdentity(builderType)) )
	{
		return;
	}

	asCString canonicalSection;
	int canonicalRow = 0;
	int canonicalColumn = 0;
	CanonicalSourceIdentity(canonical, *declaration, canonicalSection, canonicalRow, canonicalColumn);
	asCString builderSection;
	int builderRow = 0;
	int builderColumn = 0;
	BuilderTypeSourceIdentity(builderType, builderSection, builderRow, builderColumn);
	asCString canonicalRange;
	canonicalRange.Format("%s:%d:%d",
		canonicalSection.AddressOf() ? canonicalSection.AddressOf() : "",
		canonicalRow, canonicalColumn);
	asCString builderRange;
	builderRange.Format("%s:%d:%d",
		builderSection.AddressOf() ? builderSection.AddressOf() : "",
		builderRow, builderColumn);
	if( ShadowStringMismatch(out, path, "range", canonicalRange, builderRange) )
	{
		return;
	}

	if( ShadowStringMismatch(out, path, "dependencies",
		CanonicalDependencyIdentity(*declaration),
		BuilderTypeDependencyIdentity(builderType)) )
	{
		return;
	}
}

void asCASTShadowDiffBuilderTypeIdentity(
	const asCASTContext& canonical,
	asASTDeclId canonicalDecl,
	const asCObjectType& builderType,
	asCString& out)
{
	asCASTShadowDiffBuilderTypeIdentity(
		canonical, canonicalDecl, static_cast<const asCTypeInfo&>(builderType), out);
}

END_AS_NAMESPACE
