#ifndef AS_SEMA_H
#define AS_SEMA_H

#include "as_ast_context.h"

BEGIN_AS_NAMESPACE

class asCScriptEngine;
class asCModule;

// Build-local identity for one Parser action result. Parser copies only the
// logical source section and exact syntax coordinate; Canonical Sema never
// retains a native Parser-node pointer or decodes the native tree through this
// bridge. The native node remains independently owned by Parser/LEGACY.
struct asSParseActionIdentity
{
	asCString section;
	asUINT nodeKind;
	asUINT offset;
	asUINT length;

	asSParseActionIdentity()
		: nodeKind(0)
		, offset(0)
		, length(0)
	{
	}
};

// Short-lived Parser -> Sema action payload for one authored namespace path.
// This is not a syntax tree and is never retained by ASTContext: Parser keeps
// building snNamespace only for the explicit LEGACY migration oracle, while
// Canonical Sema receives the recognized segment identity and source extent.
struct asSNamespaceDeclActionSegment
{
	asCString name;
	asUINT beginOffset;
	asUINT endOffset;

	asSNamespaceDeclActionSegment()
		: beginOffset(0)
		, endOffset(0)
	{
	}
};

// Short-lived recognized-name payload for enum and enumerator declaration
// actions. Declaration identity and source extent cross the Parser -> Sema
// boundary without retaining or decoding an snEnum/snIdentifier node.
struct asSEnumDeclActionName
{
	asCString name;
	asUINT beginOffset;
	asUINT endOffset;

	asSEnumDeclActionName()
		: beginOffset(0)
		, endOffset(0)
	{
	}
};

// One explicit enumerator value after expression actions have published its
// exact root. The retained native enum syntax remains Parser-owned; only
// build-local declaration/expression identities and copied source facts cross
// into declaration Sema.
struct asSEnumeratorInitializerAction
{
	asASTDeclId enumerator;
	asASTExprId expression;
	asUINT beginOffset;
	asUINT endOffset;
	bool recovered;

	asSEnumeratorInitializerAction()
		: beginOffset(0)
		, endOffset(0)
		, recovered(false)
	{
	}
};

// Short-lived action payload for the maintained fork's primitive-only typedef
// grammar. Parser resolves the primitive token (including configured float
// width) and recognized alias range; Canonical Sema never decodes snTypedef.
struct asSTypedefDeclAction
{
	asCString name;
	eTokenType primitiveToken;
	asUINT beginOffset;
	asUINT endOffset;

	asSTypedefDeclAction()
		: primitiveToken(ttUnrecognizedToken)
		, beginOffset(0)
		, endOffset(0)
	{
	}
};

// One node in a short-lived, pointer-free Parser -> Sema type-syntax payload.
// Nodes are stored in pre-order. Scope segments begin at firstScopeSegment;
// direct template arguments owned by any root or scope node begin at
// firstTemplateArgument. Each child's subtreeSize advances to its next sibling.
// absoluteScope records an authored leading `::`. Qualifiers belong only to the
// root/type node; they are never folded from a nested template argument into
// its parent.
struct asSQualTypeSyntaxNode
{
	asCString spelling;
	eTokenType primitiveToken;
	asDWORD qualifiers;
	bool absoluteScope;
	asUINT firstScopeSegment;
	asUINT scopeSegmentCount;
	asUINT firstTemplateArgument;
	asUINT templateArgumentCount;
	asUINT subtreeSize;
	asUINT beginOffset;
	asUINT endOffset;

	asSQualTypeSyntaxNode()
		: primitiveToken(ttUnrecognizedToken)
		, qualifiers(0)
		, absoluteScope(false)
		, firstScopeSegment(0)
		, scopeSegmentCount(0)
		, firstTemplateArgument(0)
		, templateArgumentCount(0)
		, subtreeSize(1)
		, beginOffset(0)
		, endOffset(0)
	{
	}
};

// Short-lived recognized type syntax. Parser copies the root, scope segments
// and every nested template argument into owned indexed values. Sema resolves
// this payload into an ASTContext-local QualType; no Parser node,
// Builder/Runtime pointer, numeric runtime type identity or snapshot-local type
// ref crosses the boundary.
struct asSQualTypeSyntaxAction
{
	asCArray<asSQualTypeSyntaxNode> nodes;
};

// Short-lived literal token recognized by Parser. The action owns the exact
// raw token spelling and half-open processed-source range so Canonical Sema can
// decode value/type facts without retaining or traversing an snConstant node.
struct asSLiteralExprAction
{
	eTokenType token;
	asCString spelling;
	asUINT beginOffset;
	asUINT endOffset;

	asSLiteralExprAction()
		: token(ttUnrecognizedToken)
		, beginOffset(0)
		, endOffset(0)
	{
	}
};

// One recognized declaration-reference expression. Parser owns grammar and
// copies the exact identifier plus each qualifier segment; Sema owns lexical /
// explicit-scope lookup, `this`, native-enum projection and deferred binding.
// The explicit absolute-scope bit keeps `::Game::X` distinct from `Game::X`.
// No Parser node, token-buffer pointer or Runtime numeric identity crosses the
// action boundary.
struct asSDeclRefExprAction
{
	asASTDeclId owner;
	asCString name;
	asCArray<asCString> scopeSegments;
	bool absoluteScope;
	asUINT beginOffset;
	asUINT endOffset;

	asSDeclRefExprAction()
		: absoluteScope(false)
		, beginOffset(0)
		, endOffset(0)
	{
	}
};

// One prefix or postfix operator recognized by Parser for a pure unary term.
// The token kind selects language semantics and the owned spelling proves the
// payload describes the token Parser consumed. Prefix/postfix phase belongs
// to the containing list, so no native syntax-node pointer crosses Sema.
struct asSUnaryExprOperatorAction
{
	int tokenType;
	asCString spelling;

	asSUnaryExprOperatorAction()
		: tokenType(0)
	{
	}
};

// One complete pure-unary EXPRTERM. Parser supplies the exact primary identity
// plus prefix operators in source order and postfix operators in source order.
// Sema applies postfix operators first, then wraps prefixes right-to-left.
// Member/index/call postfix stages require their own ordered action and are not
// represented by this payload. IDs remain build-local to this ASTContext.
struct asSUnaryExprAction
{
	asASTExprId operand;
	asCArray<asSUnaryExprOperatorAction> prefixOperators;
	asCArray<asSUnaryExprOperatorAction> postfixOperators;
	asUINT beginOffset;
	asUINT endOffset;

	asSUnaryExprAction()
		: beginOffset(0)
		, endOffset(0)
	{
	}
};

// One explicit Cast<T>(expr). Parser has already resolved T through the typed
// QualType action and supplies the exact operand identity. Both values are
// build-local ASTContext identities; no native syntax or Runtime identity is
// retained by this pointer-free payload.
struct asSCastExprAction
{
	asASTExprId operand;
	asCQualType targetType;
	asUINT beginOffset;
	asUINT endOffset;

	asSCastExprAction()
		: beginOffset(0)
		, endOffset(0)
	{
	}
};

// One complete TYPE ARGLIST expression. Parser has already resolved the
// target through the typed QualType action and supplies ordered exact argument
// identities. Sema owns the semantic split between scalar/enum conversion and
// object construction, including constructor selection and lifetime wrapping.
// All identities are build-local to this ASTContext; no native syntax or
// Runtime identity crosses the pointer-free action boundary.
struct asSConstructExprAction
{
	asCQualType targetType;
	asCArray<asASTExprId> arguments;
	asUINT beginOffset;
	asUINT endOffset;

	asSConstructExprAction()
		: beginOffset(0)
		, endOffset(0)
	{
	}
};

// One element in an expression-form initializer list. Parser distinguishes an
// authored expression (including a nested list already published as one exact
// ExprId) from an omitted/default slot. The copied offsets are relative to the
// processed source section and no native syntax pointer crosses this payload.
enum asEInitListElementActionKind
{
	asINIT_LIST_ELEMENT_INVALID = 0,
	asINIT_LIST_ELEMENT_EXPRESSION,
	asINIT_LIST_ELEMENT_OMITTED
};

struct asSInitListElementAction
{
	asEInitListElementActionKind kind;
	asASTExprId expression;
	asUINT beginOffset;
	asUINT endOffset;

	asSInitListElementAction()
		: kind(asINIT_LIST_ELEMENT_INVALID)
		, beginOffset(0)
		, endOffset(0)
	{
	}
};

// One complete `[TYPE '='] INITLIST` expression. An explicit target has already
// crossed Parser's QualType action; anonymous lists remain contextual until an
// assignment/initializer supplies their target. Ordered nested/expression and
// omitted slots are copied facts. `hasTrailingSeparator` distinguishes the
// native Parser's final snUndefined sentinel from a semantic default element,
// while `recovered` permits a completed prefix to be published after a syntax
// error such as a missing closing brace. All identities are build-local.
struct asSInitListExprAction
{
	asASTDeclId owner;
	asCQualType targetType;
	bool hasExplicitTarget;
	bool hasTrailingSeparator;
	bool recovered;
	asCArray<asSInitListElementAction> elements;
	asUINT beginOffset;
	asUINT endOffset;

	asSInitListExprAction()
		: hasExplicitTarget(false)
		, hasTrailingSeparator(false)
		, recovered(false)
		, beginOffset(0)
		, endOffset(0)
	{
	}
};

// One authored call argument. The optional copied name is empty for a
// positional source argument. Sema maps these source-order facts to formal
// slots and owns every synthesized default/hidden argument.
struct asSCallArgumentAction
{
	asASTExprId expression;
	asCString name;
};

// One complete ordinary or receiver-aware call request. Parser owns only
// syntax extraction; Sema owns scope lookup, overload selection, argument
// arrangement/conversion, dispatch, deferred lookup and lifetime wrapping.
// All IDs are build-local and all strings/ranges are copied values.
struct asSCallExprAction
{
	asASTDeclId owner;
	asCString name;
	asCArray<asCString> scopeSegments;
	bool absoluteScope;
	asASTExprId implicitReceiver;
	asCArray<asSCallArgumentAction> arguments;
	asUINT beginOffset;
	asUINT endOffset;

	asSCallExprAction()
		: absoluteScope(false)
		, beginOffset(0)
		, endOffset(0)
	{
	}
};

// One ordered structural stage after an exact EXPRTERM primary. The explicit
// kind prevents Sema from inferring grammar from optional fields. Member/call
// names, qualified scope, argument names/identities and operator spelling are
// copied values; source offsets are relative to the same processed section as
// the containing action. No Parser node or token pointer crosses this payload.
enum asEStructuralPostfixExprActionKind
{
	asSTRUCTURAL_POSTFIX_INVALID = 0,
	asSTRUCTURAL_POSTFIX_MEMBER,
	asSTRUCTURAL_POSTFIX_MEMBER_CALL,
	asSTRUCTURAL_POSTFIX_INDEX,
	asSTRUCTURAL_POSTFIX_CALL,
	asSTRUCTURAL_POSTFIX_UNARY
};

struct asSStructuralPostfixExprActionStep
{
	asEStructuralPostfixExprActionKind kind;
	asCString name;
	asCArray<asCString> scopeSegments;
	bool absoluteScope;
	asCArray<asSCallArgumentAction> arguments;
	asSUnaryExprOperatorAction op;
	asUINT beginOffset;
	asUINT endOffset;

	asSStructuralPostfixExprActionStep()
		: kind(asSTRUCTURAL_POSTFIX_INVALID)
		, absoluteScope(false)
		, beginOffset(0)
		, endOffset(0)
	{
	}
};

// One complete structural EXPRTERM. Parser supplies one exact primary, every
// structural/postfix stage in source order, and prefixes in source order.
// Sema threads the immediately preceding result through the stages, then wraps
// prefixes right-to-left. This is a short-lived build-local action, not an AST
// node and not a durable serialization format.
struct asSStructuralPostfixExprAction
{
	asASTDeclId owner;
	asASTExprId base;
	asCArray<asSUnaryExprOperatorAction> prefixOperators;
	asCArray<asSStructuralPostfixExprActionStep> steps;
	asUINT beginOffset;
	asUINT endOffset;

	asSStructuralPostfixExprAction()
		: beginOffset(0)
		, endOffset(0)
	{
	}
};

// One complete conditional expression. Parser supplies only the already-
// constructed child identities and the exact source span; Sema owns result
// type/conversion selection and creates the composite expression. Child IDs
// are build-local identities in this Sema's ASTContext, not durable values.
// No Parser node, token-buffer pointer or Runtime numeric identity crosses the
// action boundary.
struct asSConditionalExprAction
{
	asASTExprId condition;
	asASTExprId thenExpression;
	asASTExprId elseExpression;
	asUINT beginOffset;
	asUINT endOffset;

	asSConditionalExprAction()
		: beginOffset(0)
		, endOffset(0)
	{
	}
};

// One complete assignment expression. Parser supplies only exact child
// identities, an owned operator spelling and the exact source span. Sema owns
// assignability, conversion, compound/property rewrite and typed construction.
// Child IDs are build-local to this Sema's ASTContext. No Parser node,
// token-buffer pointer or Runtime numeric identity crosses the boundary.
struct asSAssignExprAction
{
	asASTExprId lhs;
	asASTExprId rhs;
	asCString op;
	asUINT beginOffset;
	asUINT endOffset;

	asSAssignExprAction()
		: beginOffset(0)
		, endOffset(0)
	{
	}
};

// One operator in a complete flat binary-expression grammar sequence. The
// token kind selects language semantics while the owned spelling proves the
// action describes the source token that Parser just consumed. No token or
// Parser-node pointer crosses the action boundary.
struct asSBinaryExprOperatorAction
{
	int tokenType;
	asCString spelling;

	asSBinaryExprOperatorAction()
		: tokenType(0)
	{
	}
};

// One complete EXPR ::= EXPRTERM {EXPROP EXPRTERM}. Parser supplies ordered,
// exact operand identities and owned operator facts. Sema owns operator
// validation, precedence/associativity folding, logical short-circuit
// selection, overload resolution and typed construction. IDs are build-local
// to this ASTContext; no native syntax or Runtime identity is retained.
struct asSBinaryExprAction
{
	asCArray<asASTExprId> operands;
	asCArray<asSBinaryExprOperatorAction> operators;
	asUINT beginOffset;
	asUINT endOffset;

	asSBinaryExprAction()
		: beginOffset(0)
		, endOffset(0)
	{
	}
};

// Import declaration identity is recognized before the parameter list so
// incremental parameter actions have an exact owner. The canonical return
// type is short-lived and belongs to the same ASTContext; no Parser node or
// durable Runtime identity crosses this action payload.
struct asSImportSignatureAction
{
	asCString name;
	asCQualType returnType;
	asUINT beginOffset;
	asUINT endOffset;

	asSImportSignatureAction()
		: beginOffset(0)
		, endOffset(0)
	{
	}
};

// Second phase of an import action. Parser recognizes the unquoted module
// origin before ';'; Sema owns dependency recording and final stable identity.
struct asSImportOriginAction
{
	asCString origin;
	asUINT beginOffset;
	asUINT endOffset;

	asSImportOriginAction()
		: beginOffset(0)
		, endOffset(0)
	{
	}
};

// Custom access policy is a retained semantic declaration, not a Runtime
// pointer. Parser copies one ordered permission payload per term; Sema owns
// record-local identity, traits, diagnostics, and member reference binding.
struct asSAccessPermissionAction
{
	asCString name;
	asDWORD traits;
	asUINT beginOffset;
	asUINT endOffset;

	asSAccessPermissionAction()
		: traits(0)
		, beginOffset(0)
		, endOffset(0)
	{
	}
};

struct asSAccessSpecifierAction
{
	asCString name;
	asDWORD traits;
	asUINT beginOffset;
	asUINT endOffset;
	asCArray<asSAccessPermissionAction> permissions;

	asSAccessSpecifierAction()
		: traits(0)
		, beginOffset(0)
		, endOffset(0)
	{
	}
};

// Function declaration identity is published as soon as Parser has recognized
// the authored name. The bounded kind hint describes syntax that Parser alone
// knows; Sema remains responsible for selecting constructors from owner/name
// identity and for creating the authoritative declaration kind.
enum asEFunctionActionKind
{
	asFUNCTION_ACTION_FUNCTION = 0,
	asFUNCTION_ACTION_METHOD,
	asFUNCTION_ACTION_MIXIN,
	asFUNCTION_ACTION_DESTRUCTOR
};

struct asSFunctionSignatureAction
{
	asCString name;
	asCString accessSpecifierName;
	asCQualType returnType;
	asEFunctionActionKind kind;
	asUINT beginOffset;
	asUINT endOffset;

	asSFunctionSignatureAction()
		: kind(asFUNCTION_ACTION_FUNCTION)
		, beginOffset(0)
		, endOffset(0)
	{
	}
};

// A completed callable body is attached by exact Canonical statement identity.
// Parser copies the authored range and passes the Block StmtId that its
// statement action already published. No native body node crosses this action.
struct asSFunctionBodyAction
{
	asASTDeclId function;
	asASTStmtId body;
	asUINT beginOffset;
	asUINT endOffset;
	bool recovered;

	asSFunctionBodyAction()
		: beginOffset(0)
		, endOffset(0)
		, recovered(false)
	{
	}
};

// Funcdef is a callable type declaration rather than an executable function,
// so it has a dedicated action contract. Parser publishes the recognized name
// and canonical return type before parameters; Sema owns declaration identity,
// stable signature construction, dependency recording, and lexical scope.
struct asSFuncDefSignatureAction
{
	asCString name;
	asCQualType returnType;
	asUINT beginOffset;
	asUINT endOffset;

	asSFuncDefSignatureAction()
		: beginOffset(0)
		, endOffset(0)
	{
	}
};

// One ordinary callable parameter header. Parser publishes this immediately
// after the type/modifier/optional name are complete and before parsing an
// optional default expression. The explicit callable DeclId prevents
// lastActedDecl recovery from binding a parameter to the previous parameter.
struct asSParameterDeclAction
{
	asASTDeclId callable;
	asCString name;
	asCQualType type;
	asUINT beginOffset;
	asUINT endOffset;
	// Lambda grammar permits an omitted formal type. Keep that syntax fact
	// explicit instead of manufacturing a durable ERROR type or treating a
	// missing ParamDecl as semantic absence. Sema accepts an invalid QualType
	// only for this construction-only Lambda action and must replace it before
	// the graph can become publishable.
	bool typeOmitted;

	asSParameterDeclAction()
		: beginOffset(0)
		, endOffset(0)
		, typeOmitted(false)
	{
	}
};

// The optional default is published only after its complete expression action
// has produced an exact ExprId. Parser copies the authored text/range while it
// owns transient syntax; Sema validates and binds those durable facts to the
// exact parameter. No Parser node or Runtime identity crosses this boundary.
struct asSParameterDefaultAction
{
	asASTDeclId parameter;
	asASTExprId expression;
	asCString authoredText;
	asUINT beginOffset;
	asUINT endOffset;
	bool recovered;

	asSParameterDefaultAction()
		: beginOffset(0)
		, endOffset(0)
		, recovered(false)
	{
	}
};

// Lambda syntax has no authored return type. Parser publishes the exact
// owner/header range, authored parameter count, explicitly-typed parameter
// count, and a recovery type used only until a typed conversion supplies the
// target funcdef. Sema owns contextual signature binding; omitted-formal
// inference remains deferred until it can re-type the complete body. This
// payload is short-lived AST construction state and never crosses a snapshot,
// Provider, relocation, or Cache boundary.
struct asSLambdaHeaderAction
{
	asASTDeclId owner;
	asCQualType recoveryReturnType;
	asUINT parameterCount;
	asUINT explicitlyTypedParameterCount;
	asUINT beginOffset;
	asUINT endOffset;

	asSLambdaHeaderAction()
		: parameterCount(0)
		, explicitlyTypedParameterCount(0)
		, beginOffset(0)
		, endOffset(0)
	{
	}
};

// The completed lambda expression references the exact declaration created by
// asSLambdaHeaderAction. Parser publishes only that stable construction
// identity and the full expression range; Sema must not inspect an snFunction,
// rediscover the declaration, or attach its body again. This payload is local
// to one mutable ASTContext and never crosses a durable boundary.
struct asSLambdaExpressionAction
{
	asASTDeclId lambdaDecl;
	asUINT beginOffset;
	asUINT endOffset;

	asSLambdaExpressionAction()
		: beginOffset(0)
		, endOffset(0)
	{
	}
};

// Class, struct, and interface identity is published immediately after the
// authored name. Parser owns only the bounded syntax-kind hint; Sema owns the
// declaration kind, nominal canonical type, traits, scope, and stable key.
enum asERecordActionKind
{
	asRECORD_ACTION_CLASS = 0,
	asRECORD_ACTION_STRUCT,
	asRECORD_ACTION_INTERFACE
};

struct asSRecordHeaderAction
{
	asCString name;
	asERecordActionKind kind;
	asUINT beginOffset;
	asUINT endOffset;

	asSRecordHeaderAction()
		: kind(asRECORD_ACTION_CLASS)
		, beginOffset(0)
		, endOffset(0)
	{
	}
};

// One authored base spelling in the record header. The Parser copies the
// ordered token spelling and exact range; Sema resolves lexical/qualified
// identity and never retains or decodes the snClass/snInterface node.
struct asSRecordBaseSpecifierAction
{
	asCString qualifiedName;
	asUINT beginOffset;
	asUINT endOffset;

	asSRecordBaseSpecifierAction()
		: beginOffset(0)
		, endOffset(0)
	{
	}
};

// One authored variable declarator. Parser may recognize several names
// after one shared type spelling; each name crosses independently so Sema
// never rediscovers the first identifier from a completed snDeclaration.
// The QualType is short-lived construction-Context state, not durable ABI
// identity. Only class fields may carry private/protected traits.
struct asSVariableHeaderAction
{
	asCString name;
	asCString accessSpecifierName;
	asCQualType type;
	asDWORD traits;
	asUINT beginOffset;
	asUINT endOffset;

	asSVariableHeaderAction()
		: traits(0)
		, beginOffset(0)
		, endOffset(0)
	{
	}
};

enum asEVariableInitializerActionKind
{
	asVARIABLE_INITIALIZER_INVALID = 0,
	asVARIABLE_INITIALIZER_NONE,
	asVARIABLE_INITIALIZER_EXPRESSION,
	asVARIABLE_INITIALIZER_DIRECT_CONSTRUCT
};

// One global/field declarator after its typed header has created the exact
// VarDecl. Direct construction carries ordered already-published argument
// identities; ordinary assignment/list forms carry one exact expression root.
// The range and recovery state are copied Parser facts.
struct asSVariableInitializerAction
{
	asASTDeclId variable;
	asEVariableInitializerActionKind kind;
	asASTExprId expression;
	asCArray<asASTExprId> arguments;
	asUINT beginOffset;
	asUINT endOffset;
	bool recovered;

	asSVariableInitializerAction()
		: kind(asVARIABLE_INITIALIZER_INVALID)
		, beginOffset(0)
		, endOffset(0)
		, recovered(false)
	{
	}
};

// One local declarator after its typed header has created the exact VarDecl.
// Like the global/field initializer action, this owns no Parser node: ordinary
// initialization carries one exact expression identity and direct construction
// carries ordered already-published argument identities. Sema publishes the
// corresponding DeclStmt and initialization effects into `statements`.
struct asSLocalVariableDeclaratorAction
{
	asASTDeclId variable;
	asEVariableInitializerActionKind kind;
	asASTExprId expression;
	asCArray<asASTExprId> arguments;
	asUINT beginOffset;
	asUINT endOffset;
	bool recovered;

	asSLocalVariableDeclaratorAction()
		: kind(asVARIABLE_INITIALIZER_INVALID)
		, beginOffset(0)
		, endOffset(0)
		, recovered(false)
	{
	}
};

// Leaf statements have no nested statement ownership. Parser publishes their
// exact callable owner, optional already-bound expression, copied source range,
// and bounded syntax/recovery kind. Sema alone freezes return conversions,
// cleanup and active control-transfer targets; no native syntax node crosses
// this action boundary.
enum asELeafStatementActionKind
{
	asLEAF_STATEMENT_INVALID = 0,
	asLEAF_STATEMENT_EXPRESSION,
	asLEAF_STATEMENT_RETURN,
	asLEAF_STATEMENT_BREAK,
	asLEAF_STATEMENT_CONTINUE,
	asLEAF_STATEMENT_FALLTHROUGH
};

struct asSLeafStatementAction
{
	asELeafStatementActionKind kind;
	asASTDeclId owner;
	asASTExprId expression;
	asUINT beginOffset;
	asUINT endOffset;
	bool recovered;

	asSLeafStatementAction()
		: kind(asLEAF_STATEMENT_INVALID)
		, beginOffset(0)
		, endOffset(0)
		, recovered(false)
	{
	}
};

// Structured statements own already-published statement identities. Native
// Parser nodes remain available to LEGACY compilation/recovery, but never
// cross either action boundary. A local declaration sequence is an explicit
// construction carrier: Sema flattens only the listed carriers into the
// authored lexical block and relinquishes their temporary ownership edges.
struct asSBlockStatementAction
{
	asASTDeclId owner;
	asCArray<asASTStmtId> children;
	asCArray<asASTStmtId> declarationSequences;
	asUINT beginOffset;
	asUINT endOffset;
	bool recovered;

	asSBlockStatementAction()
		: beginOffset(0)
		, endOffset(0)
		, recovered(false)
	{
	}
};

struct asSIfStatementAction
{
	asASTDeclId owner;
	asASTExprId condition;
	asASTStmtId thenStatement;
	asASTStmtId elseStatement;
	asUINT beginOffset;
	asUINT endOffset;
	bool recovered;

	asSIfStatementAction()
		: beginOffset(0)
		, endOffset(0)
		, recovered(false)
	{
	}
};

// Structured controls are necessarily two-phase: the header publishes an
// exact StmtId before body parsing so break/continue can bind immediately;
// the typed finish action then fills that same identity with copied semantic
// children and the complete authored range. No native Parser node crosses
// either phase.
enum asEControlStatementActionKind
{
	asCONTROL_STATEMENT_INVALID = 0,
	asCONTROL_STATEMENT_WHILE,
	asCONTROL_STATEMENT_DO_WHILE,
	asCONTROL_STATEMENT_FOR,
	asCONTROL_STATEMENT_FOREACH,
	asCONTROL_STATEMENT_SWITCH
};

struct asSControlStatementHeaderAction
{
	asEControlStatementActionKind kind;
	asASTDeclId owner;
	asUINT beginOffset;
	asUINT endOffset;
	bool recovered;

	asSControlStatementHeaderAction()
		: kind(asCONTROL_STATEMENT_INVALID)
		, beginOffset(0)
		, endOffset(0)
		, recovered(false)
	{
	}
};

enum asEWhileStatementActionKind
{
	asWHILE_STATEMENT_INVALID = 0,
	asWHILE_STATEMENT_WHILE,
	asWHILE_STATEMENT_DO_WHILE
};

struct asSWhileStatementAction
{
	asEWhileStatementActionKind kind;
	asASTStmtId control;
	asASTDeclId owner;
	asASTExprId condition;
	asASTStmtId body;
	asUINT beginOffset;
	asUINT endOffset;
	bool recovered;

	asSWhileStatementAction()
		: kind(asWHILE_STATEMENT_INVALID)
		, beginOffset(0)
		, endOffset(0)
		, recovered(false)
	{
	}
};

// A completed C-style for loop. Parser supplies the exact init statement,
// optional condition, ordered increment expressions and exact body identity.
// Sema alone constructs the increment SequenceExpr/ExprStmt phase and fills
// the pre-published control identity.
struct asSForStatementAction
{
	asASTStmtId control;
	asASTDeclId owner;
	asASTStmtId initStatement;
	asASTExprId condition;
	asCArray<asASTExprId> increments;
	asASTStmtId body;
	asUINT beginOffset;
	asUINT endOffset;
	bool recovered;

	asSForStatementAction()
		: beginOffset(0)
		, endOffset(0)
		, recovered(false)
	{
	}
};

// A completed stock foreach loop. The two declaration statements, authored
// range expression and body are already-published Canonical identities. Sema
// owns protocol selection, single-evaluation lowering and lifetime cleanup;
// no Parser node or Runtime type identity crosses this action.
struct asSForeachStatementAction
{
	asASTStmtId control;
	asASTDeclId owner;
	asASTStmtId valueStatement;
	asASTStmtId keyStatement;
	asASTExprId rangeExpression;
	asASTStmtId body;
	asUINT beginOffset;
	asUINT endOffset;
	bool recovered;

	asSForeachStatementAction()
		: beginOffset(0)
		, endOffset(0)
		, recovered(false)
	{
	}
};

enum asECaseStatementActionKind
{
	asCASE_STATEMENT_INVALID = 0,
	asCASE_STATEMENT_CASE,
	asCASE_STATEMENT_DEFAULT
};

// One completed case/default clause. Parser supplies the exact parent switch,
// optional case value and ordered statement identities. Sema creates the Case
// node and parent edge without re-reading the native case shell.
struct asSCaseStatementAction
{
	asECaseStatementActionKind kind;
	asASTStmtId switchStatement;
	asASTDeclId owner;
	asASTExprId value;
	asCArray<asASTStmtId> children;
	asUINT beginOffset;
	asUINT endOffset;
	bool recovered;

	asSCaseStatementAction()
		: kind(asCASE_STATEMENT_INVALID)
		, beginOffset(0)
		, endOffset(0)
		, recovered(false)
	{
	}
};

// The completed switch owns one exact selector and the ordered exact Case
// identities already published under its header target. Sema owns final
// fallthrough wiring, enum safe-point selection and control-stack completion.
struct asSSwitchStatementAction
{
	asASTStmtId control;
	asASTDeclId owner;
	asASTExprId selector;
	asCArray<asASTStmtId> cases;
	asUINT beginOffset;
	asUINT endOffset;
	bool recovered;

	asSSwitchStatementAction()
		: beginOffset(0)
		, endOffset(0)
		, recovered(false)
	{
	}
};

// Clang Sema analogue: the only semantic authority for the canonical pipeline.
class ANGELSCRIPTRUNTIME_API asCSema
{
public:
	asCSema(asCScriptEngine* engine, asCASTContext& context, asCModule* module = 0);

	asASTDeclId ActOnTranslationUnit(const char* moduleName);
	asASTDeclId ActOnNamespaceDecl(asASTDeclId parent, const char* name, const asCSourceRange& range);
	asASTDeclId ActOnStartNamespaceDecl(asASTDeclId parent, const char* name, const asCSourceRange& range);
	asASTDeclId ActOnNamespacePath(
		const asCArray<asSNamespaceDeclActionSegment>& segments,
		class asCScriptCode* script);
	asASTDeclId ActOnClassDecl(asASTDeclId parent, const char* name, const asCSourceRange& range);
	asASTDeclId ActOnStartClassDecl(asASTDeclId parent, const char* name, const asCSourceRange& range);
	asASTDeclId ActOnRecordHeaderAction(
		const asSRecordHeaderAction& action,
		class asCScriptCode* script);
	void ActOnRecordBaseSpecifiersAction(
		asASTDeclId recordDecl,
		const asCArray<asSRecordBaseSpecifierAction>& actions,
		class asCScriptCode* script);
	void ActOnRecordFinishAction(asASTDeclId recordDecl);
	// Class-default syntax is lowered while parsing, not replayed from a completed
	// Parser shell. Start publishes the exact generated method so the authored
	// statement is created under its final DeclContext; finish routes that one
	// owner/range-matched statement into the method body.
	asASTDeclId ActOnClassDefaultStartAction(
		asASTDeclId ownerClass,
		class asCScriptCode* script,
		asUINT beginOffset,
		asUINT endOffset);
	asASTStmtId ActOnClassDefaultStatementAction(
		asASTDeclId initDefaults,
		class asCScriptCode* script,
		asUINT beginOffset,
		asUINT endOffset);
	asASTDeclId ActOnEnumDecl(asASTDeclId parent, const char* name, const asCSourceRange& range);
	asASTDeclId ActOnStartEnumDecl(asASTDeclId parent, const char* name, const asCSourceRange& range);
	asASTDeclId ActOnEnumName(
		const asSEnumDeclActionName& action,
		class asCScriptCode* script);
	asASTDeclId ActOnImportDecl(asASTDeclId parent, const char* name, const asCSourceRange& range);
	asASTDeclId ActOnStartImportDecl(asASTDeclId parent, const char* name, const asCSourceRange& range);
	asASTDeclId ActOnImportSignatureAction(
		const asSImportSignatureAction& action,
		class asCScriptCode* script);
	void ActOnImportOriginAction(
		asASTDeclId importDecl,
		const asSImportOriginAction& action,
		class asCScriptCode* script);
	asASTDeclId ActOnAccessSpecifierAction(
		const asSAccessSpecifierAction& action,
		class asCScriptCode* script);
	asASTDeclId ActOnDeclAccessSpecifierAction(
		asASTDeclId member,
		const char* accessSpecifierName);
	asASTDeclId ActOnVarDecl(asASTDeclId parent, const char* name, const asCQualType& type, const asCSourceRange& range);
	asASTDeclId ActOnStartVarDecl(asASTDeclId parent, const char* name, const asCQualType& type, const asCSourceRange& range);
	asASTDeclId ActOnVariableHeaderAction(
		const asSVariableHeaderAction& action,
		class asCScriptCode* script);
	bool ActOnVariableInitializerAction(
		const asSVariableInitializerAction& action,
		class asCScriptCode* script);
	// Local identity/type already crossed through ActOnVariableHeaderAction.
	// Bind the exact pointer-free initializer action and publish its
	// DeclStmt/initialization statements in order.
	bool ActOnLocalVariableDeclaratorAction(
		const asSLocalVariableDeclaratorAction& action,
		class asCScriptCode* script,
		asCArray<asASTStmtId>& statements);
	// Publish the exact source-level declaration sequence after its semicolon.
	// Normal compound assembly flattens this Block; for-init assembly retains it
	// as a sequence whose children share the enclosing loop scope.
	asASTStmtId ActOnLocalVariableDeclarationAction(
		const asCArray<asASTStmtId>& statements,
		class asCScriptCode* script,
		asUINT beginOffset,
		asUINT endOffset);
	// Foreach variables are initialized by the loop protocol, so this creates
	// only the typed DeclStmt and deliberately performs no default construction.
	asASTStmtId ActOnForeachVariableAction(
		asASTDeclId variableDecl,
		class asCScriptCode* script,
		asUINT beginOffset,
		asUINT endOffset);
	asASTDeclId ActOnListPatternDecl(asASTDeclId parent, const char* structured, const asCSourceRange& range);
	void ActOnGlobalVarInit(asASTDeclId var, asASTExprId init);
	asASTDeclId ActOnFunctionDecl(asASTDeclId parent, const char* name, const asCQualType& returnType, const asCSourceRange& range);
	asASTDeclId ActOnStartFunctionDecl(asASTDeclId parent, const char* name, const asCQualType& returnType, const asCSourceRange& range);
	asASTDeclId ActOnFunctionSignatureAction(
		const asSFunctionSignatureAction& action,
		class asCScriptCode* script);
	asASTDeclId ActOnFuncDefSignatureAction(
		const asSFuncDefSignatureAction& action,
		class asCScriptCode* script);
	void ActOnFunctionTraitsAction(asASTDeclId functionDecl, asDWORD traits);
	// Attach an exact already-published Block to its callable declaration.
	bool ActOnFunctionBodyAction(
		const asSFunctionBodyAction& action,
		class asCScriptCode* script);
	asASTDeclId ActOnMethodDecl(asASTDeclId parent, const char* name, const asCQualType& returnType, const asCSourceRange& range);
	asASTDeclId ActOnStartMethodDecl(asASTDeclId parent, const char* name, const asCQualType& returnType, const asCSourceRange& range);
	asASTDeclId ActOnConstructorDecl(asASTDeclId parent, const char* name, const asCSourceRange& range);
	asASTDeclId ActOnStartConstructorDecl(asASTDeclId parent, const char* name, const asCSourceRange& range);
	asASTDeclId ActOnDestructorDecl(asASTDeclId parent, const char* name, const asCSourceRange& range);
	asASTDeclId ActOnStartDestructorDecl(asASTDeclId parent, const char* name, const asCSourceRange& range);
	asASTDeclId ActOnParamDecl(asASTDeclId parent, const char* name, const asCQualType& type, const asCSourceRange& range);
	asASTDeclId ActOnStartParamDecl(asASTDeclId parent, const char* name, const asCQualType& type, const asCSourceRange& range);
	asASTDeclId ActOnParameterDeclAction(
		const asSParameterDeclAction& action,
		class asCScriptCode* script);
	bool ActOnParameterDefaultAction(
		const asSParameterDefaultAction& action,
		class asCScriptCode* script);
	asASTDeclId ActOnStartEnumeratorDecl(asASTDeclId parent, const char* name, const asCSourceRange& range);
	void ActOnFinishEnumeratorDecl(asASTDeclId enumerator, asASTExprId initializer);
	asASTDeclId ActOnEnumeratorName(
		asASTDeclId enumDecl,
		const asSEnumDeclActionName& action,
		class asCScriptCode* script);
	bool ActOnEnumeratorInitializerAction(
		const asSEnumeratorInitializerAction& action,
		class asCScriptCode* script);
	asASTDeclId ActOnPropertyDecl(asASTDeclId parent, const char* name, const asCQualType& type, const asCSourceRange& range);
	asASTDeclId ActOnTypedefDecl(asASTDeclId parent, const char* name, const asCSourceRange& range);
	asASTDeclId ActOnStartTypedefDecl(asASTDeclId parent, const char* name, const asCSourceRange& range);
	asASTDeclId ActOnTypedefAction(
		const asSTypedefDeclAction& action,
		class asCScriptCode* script);
	asASTDeclId ActOnMixinDecl(asASTDeclId parent, const char* name, const asCSourceRange& range);
	asASTDeclId ActOnStartMixinDecl(asASTDeclId parent, const char* name, const asCSourceRange& range);
	asASTDeclId ActOnInterfaceDecl(asASTDeclId parent, const char* name, const asCSourceRange& range);
	asASTDeclId ActOnStartInterfaceDecl(asASTDeclId parent, const char* name, const asCSourceRange& range);
	asASTDeclId ActOnFuncDefDecl(asASTDeclId parent, const char* name, const asCSourceRange& range);
	asASTDeclId ActOnStartFuncDefDecl(asASTDeclId parent, const char* name, const asCSourceRange& range);
	asASTStmtId ActOnLeafStatementAction(
		const asSLeafStatementAction& action,
		class asCScriptCode* script);
	asASTStmtId ActOnBlockStatementAction(
		const asSBlockStatementAction& action,
		class asCScriptCode* script);
	asASTStmtId ActOnIfStatementAction(
		const asSIfStatementAction& action,
		class asCScriptCode* script);
	asASTStmtId ActOnControlStatementHeaderAction(
		const asSControlStatementHeaderAction& action,
		class asCScriptCode* script);
	asASTStmtId ActOnWhileStatementAction(
		const asSWhileStatementAction& action,
		class asCScriptCode* script);
	asASTStmtId ActOnForStatementAction(
		const asSForStatementAction& action,
		class asCScriptCode* script);
	asASTStmtId ActOnForeachStatementAction(
		const asSForeachStatementAction& action,
		class asCScriptCode* script);
	asASTStmtId ActOnCaseStatementAction(
		const asSCaseStatementAction& action,
		class asCScriptCode* script);
	asASTStmtId ActOnSwitchStatementAction(
		const asSSwitchStatementAction& action,
		class asCScriptCode* script);
	asASTStmtId ActOnReturnStmt(asASTDeclId owner, asASTExprId value, const asCSourceRange& range);
	asASTExprId ActOnIntegerLiteral(asQWORD value, const asCSourceRange& range);
	asASTExprId ActOnBoolLiteral(bool value, const asCSourceRange& range);
	asASTExprId ActOnFloatLiteral(const char* text, const asCSourceRange& range);
	asASTExprId ActOnDoubleLiteral(const char* text, const asCSourceRange& range);
	asASTExprId ActOnStringLiteral(const char* text, const asCSourceRange& range);
	asASTExprId ActOnStringLiteral(const char* bytes, asUINT length, const asCSourceRange& range);
	asASTExprId ActOnNullLiteral(const asCSourceRange& range);
	asASTExprId ActOnLiteralExprAction(
		const asSLiteralExprAction& action,
		class asCScriptCode* script);
	asASTExprId ActOnDeclRefExprAction(
		const asSDeclRefExprAction& action,
		class asCScriptCode* script);
	asASTExprId ActOnUnaryExprAction(
		const asSUnaryExprAction& action,
		class asCScriptCode* script);
	asASTExprId ActOnCastExprAction(
		const asSCastExprAction& action,
		class asCScriptCode* script);
	asASTExprId ActOnConstructExprAction(
		const asSConstructExprAction& action,
		class asCScriptCode* script);
	asASTExprId ActOnInitListExprAction(
		const asSInitListExprAction& action,
		class asCScriptCode* script);
	asASTExprId ActOnCallExprAction(
		const asSCallExprAction& action,
		class asCScriptCode* script);
	asASTExprId ActOnStructuralPostfixExprAction(
		const asSStructuralPostfixExprAction& action,
		class asCScriptCode* script);
	asASTExprId ActOnConditionalExprAction(
		const asSConditionalExprAction& action,
		class asCScriptCode* script);
	asASTExprId ActOnAssignExprAction(
		const asSAssignExprAction& action,
		class asCScriptCode* script);
	asASTExprId ActOnBinaryExprAction(
		const asSBinaryExprAction& action,
		class asCScriptCode* script);
	asASTExprId ActOnDeclRef(asASTDeclId target, const asCQualType& type, const asCSourceRange& range);
	asASTExprId ActOnDeclRefExpr(asASTDeclId owner, const char* name, const asCSourceRange& range);
	asASTExprId ActOnUnary(const char* op, asASTExprId inner, const asCQualType& type, const asCSourceRange& range);
	asASTExprId ActOnUnaryExpr(const char* op, asASTExprId inner, const asCSourceRange& range, bool postfix = false);
	asASTExprId ActOnBinary(const char* op, asASTExprId lhs, asASTExprId rhs, const asCQualType& type, const asCSourceRange& range);
	asASTExprId ActOnBinaryExpr(asASTExprId lhs, const char* op, asASTExprId rhs, const asCSourceRange& range);
	asASTExprId ActOnLogical(const char* op, asASTExprId lhs, asASTExprId rhs, const asCSourceRange& range);
	asASTExprId ActOnLogicalExpr(asASTExprId lhs, const char* op, asASTExprId rhs, const asCSourceRange& range);
	asASTExprId ActOnConditional(asASTExprId cond, asASTExprId thenExpr, asASTExprId elseExpr, const asCQualType& type, const asCSourceRange& range);
	asASTExprId ActOnConditionalExpr(asASTExprId cond, asASTExprId thenExpr, asASTExprId elseExpr, const asCSourceRange& range);
	asASTExprId ActOnAssign(asASTExprId lhs, asASTExprId rhs, const asCQualType& type, const asCSourceRange& range);
	asASTExprId ActOnAssignExpr(asASTExprId lhs, asASTExprId rhs, const asCSourceRange& range, const char* op = "=");
	// Compiler-generated bodies do not participate in parser replay. Always
	// create a distinct assignment node, then bind the same exact opAssign
	// decision used by authored `=` expressions.
	asASTExprId ActOnGeneratedAssign(asASTExprId lhs, asASTExprId rhs, const asCQualType& type, const asCSourceRange& range);
	asASTExprId ActOnOpaqueValueExpr(asASTExprId source, const asCSourceRange& range);
	asASTExprId ActOnDeferredOutArgument(asASTExprId receiver, const asCQualType& formalType, asASTDeclId setter, const asCSourceRange& range);
	asASTExprId ActOnThisExpr(const asCQualType& type, const asCSourceRange& range);
	asEASTCallDispatch ClassifyCallDispatch(asASTDeclId callee) const;
	asASTExprId ActOnCall(asASTDeclId callee, const asCArray<asASTExprId>& args, const asCQualType& type, const asCSourceRange& range);
	asASTExprId ActOnCallExpr(asASTDeclId owner, const char* name, const asCArray<asASTExprId>& args, const asCSourceRange& range, asASTExprId implicitReceiver = asASTExprId(), const asCArray<asCString>* argumentNames = 0, bool exactScope = false);
	asASTExprId ActOnPostfixCallExpr(asASTDeclId owner, asASTExprId calleeExpr, const asCArray<asASTExprId>& args, const asCSourceRange& range, const asCArray<asCString>* argumentNames = 0);
	asASTExprId ActOnCastExpr(asASTExprId inner, const asCQualType& dest, const asCSourceRange& range);
	asASTExprId ActOnConversion(asASTExprId inner, const asCQualType& dest, const asCSourceRange& range);
	asASTExprId ActOnSequence(const asCArray<asASTExprId>& parts, const asCQualType& type, const asCSourceRange& range);
	asASTExprId ActOnSequenceExpr(const asCArray<asASTExprId>& parts, const asCSourceRange& range);
	asASTExprId ActOnIndex(asASTExprId base, asASTExprId index, const asCQualType& type, const asCSourceRange& range);
	asASTExprId ActOnIndex(asASTExprId base, const asCArray<asASTExprId>& indices, const asCQualType& type, const asCSourceRange& range);
	asASTExprId ActOnIndexExpr(asASTExprId base, asASTExprId index, const asCSourceRange& range);
	asASTExprId ActOnIndexExpr(asASTExprId base, const asCArray<asASTExprId>& indices, const asCSourceRange& range, const asCArray<asCString>* argumentNames = 0);
	asASTExprId ActOnMemberRef(asASTExprId base, const char* name, const asCQualType& type, const asCSourceRange& range);
	asASTExprId ActOnMemberExpr(asASTExprId base, const char* name, const asCSourceRange& range);
	asASTDeclId ActOnLambdaHeaderAction(
		const asSLambdaHeaderAction& action,
		class asCScriptCode* script);
	asASTExprId ActOnLambdaExpressionAction(
		const asSLambdaExpressionAction& action,
		class asCScriptCode* script);
	asASTExprId ActOnLambdaExpr(asASTDeclId owner, const asCQualType& returnType, const asCSourceRange& range);
	asASTDeclId ActOnLambdaCapture(asASTDeclId lambda, asASTDeclId captured);
	asASTExprId ActOnConstruct(const asCQualType& type, const asCArray<asASTExprId>& args, const asCSourceRange& range);
	void InternNativeCallablesForType(const asCQualType& type);
	void InternNativeZeroArgCallablesForType(const asCQualType& type);
	asASTDeclId InternNativeListFactoryForType(const asCQualType& type);
	// asIScriptModule::CompileFunction parses an intentionally incomplete source
	// unit. Project the already-published symbols owned by that same module into
	// its ephemeral canonical graph so Sema can resolve ordinary module-local
	// calls and globals. Full module Build must never use this view: a replacement
	// generation has to resolve only against the candidate source being built.
	void InternCompileFunctionModuleScope(asCModule* module);
	void LayoutScriptClassFields(asCModule* module = 0);
	asASTExprId ActOnInitList(
		const asCArray<asASTExprId>& args,
		const asCSourceRange& range,
		const asCQualType& explicitTarget = asCQualType());
	asASTExprId ActOnMaterializeTemporary(asASTExprId inner, const asCSourceRange& range);
	asASTExprId ActOnCleanup(asASTExprId inner, const asCSourceRange& range);
	asASTStmtId ActOnBlock(asASTDeclId owner, const asCArray<asASTStmtId>& children, const asCSourceRange& range);
	asASTStmtId ActOnCompoundStmt(asASTDeclId owner, const asCArray<asASTStmtId>& children, const asCSourceRange& range);
	asASTStmtId ActOnExprStmt(asASTDeclId owner, asASTExprId expr, const asCSourceRange& range);
	asASTStmtId ActOnExpressionStmt(asASTDeclId owner, asASTExprId expr, const asCSourceRange& range);
	asASTStmtId ActOnLocalDeclStmt(asASTDeclId owner, const char* name, const asCQualType& type, asASTExprId init, const asCSourceRange& range);
	asASTStmtId ActOnIf(asASTDeclId owner, asASTExprId cond, asASTStmtId thenStmt, asASTStmtId elseStmt, const asCSourceRange& range);
	asASTStmtId ActOnIfStmt(asASTDeclId owner, asASTExprId cond, asASTStmtId thenStmt, asASTStmtId elseStmt, const asCSourceRange& range);
	asASTStmtId ActOnWhile(asASTDeclId owner, asASTExprId cond, asASTStmtId body, const asCSourceRange& range);
	asASTStmtId ActOnWhileStmt(asASTDeclId owner, asASTExprId cond, asASTStmtId body, const asCSourceRange& range);
	asASTStmtId ActOnDoWhile(asASTDeclId owner, asASTStmtId body, asASTExprId cond, const asCSourceRange& range);
	asASTStmtId ActOnDoWhileStmt(asASTDeclId owner, asASTStmtId body, asASTExprId cond, const asCSourceRange& range);
	asASTStmtId ActOnFor(asASTDeclId owner, asASTStmtId initStmt, asASTExprId cond, asASTExprId incr, asASTStmtId body, const asCSourceRange& range);
	asASTStmtId ActOnForStmt(asASTDeclId owner, asASTStmtId initStmt, asASTExprId cond, asASTExprId incr, asASTStmtId body, const asCSourceRange& range, const asCSourceRange& incrStmtRange = asCSourceRange());
	asASTStmtId ActOnForeach(asASTDeclId owner, asASTStmtId varStmt, asASTExprId rangeExpr, asASTStmtId body, const asCSourceRange& range);
	asASTStmtId ActOnForeachStmt(asASTDeclId owner, asASTStmtId varStmt, asASTExprId rangeExpr, asASTStmtId body, const asCSourceRange& range);
	asASTStmtId ActOnForeachStmt(asASTDeclId owner, asASTStmtId valueStmt, asASTStmtId keyStmt, asASTExprId rangeExpr, asASTStmtId body, const asCSourceRange& range);
	asASTStmtId ActOnBreak(asASTDeclId owner, asASTStmtId target, const asCSourceRange& range);
	asASTStmtId ActOnBreakStmt(asASTDeclId owner, const asCSourceRange& range);
	asASTStmtId ActOnContinue(asASTDeclId owner, asASTStmtId target, const asCSourceRange& range);
	asASTStmtId ActOnContinueStmt(asASTDeclId owner, const asCSourceRange& range);
	asASTStmtId ActOnSwitch(asASTDeclId owner, asASTExprId cond, const asCSourceRange& range);
	asASTStmtId ActOnSwitchStmt(asASTDeclId owner, asASTExprId cond, const asCSourceRange& range);
	void FinishSwitchStmt(asASTStmtId sw);
	asASTStmtId ActOnCase(asASTDeclId owner, asASTStmtId switchStmt, asASTExprId value, const asCSourceRange& range);
	asASTStmtId ActOnCaseStmt(asASTDeclId owner, asASTExprId value, const asCSourceRange& range);
	asASTStmtId ActOnFallthrough(asASTDeclId owner, const asCSourceRange& range);
	asASTStmtId ActOnFallthroughStmt(asASTDeclId owner, const asCSourceRange& range);
	asCQualType ActOnQualType(const char* key, asDWORD quals = 0);
	asCQualType ActOnQualTypeAction(
		const asSQualTypeSyntaxAction& action,
		class asCScriptCode* script = 0);
	asASTStmtId FindExistingStmt(asEASTStmtKind kind, const asCSourceRange& range) const;
	void RecordDependency(asASTDeclId owner, const char* key);
	void FinishDecl(asASTDeclId id);
	// AngelScript permits a function body to call a declaration appearing later
	// in the same source. Reconcile those parser-time placeholders after the
	// complete declaration surface is known and before the AST is published.
	void ResolveDeferredNames();
	// Local declarations remain children of their function DeclContext, matching
	// Clang's AST ownership model. Their visibility is instead determined by the
	// nested Stmt scope that contains each DeclStmt. Replay those scopes after all
	// incremental Parser callbacks have completed so shadowing, scope exit, and
	// for-init lifetime are sealed independently of callback order.
	void ResolveLexicalScopes();
	// A funcdef value stores only a function identity; it has no capture
	// environment. Capturing lambdas are therefore valid only as direct calls in
	// the current language/runtime ABI. Diagnose every captured lambda that has
	// no Call node naming its exact declaration before seal/publication.
	void ValidateCapturedLambdaUses();
	void PushControl(asASTStmtId id);
	void PopControl();
	void PushDeclContext(asASTDeclId id);
	void PopDeclContext();
	asASTDeclId CurrentDeclContext() const;
	asASTStmtId CurrentSwitchControl() const;
	asASTDeclId LastActedDecl() const;
	// Pointer-free parse/build identity bridge. Parser and the prepared Builder
	// shell independently copy the same section/kind/range coordinate. A
	// conflicting coordinate fails closed instead of overwriting an earlier
	// Canonical identity.
	asASTDeclId FindParsedDeclaration(
		const asSParseActionIdentity& identity) const;
	bool BindParsedDeclarationIdentity(
		const asSParseActionIdentity& identity,
		asASTDeclId declaration);
	bool BindParsedExpressionType(
		const asSParseActionIdentity& identity,
		const asCQualType& type);
	bool FindParsedExpressionType(
		const asSParseActionIdentity& identity,
		asCQualType& outType) const;
	bool BindParsedExpressionIdentity(
		const asSParseActionIdentity& identity,
		asASTExprId expression);
	asASTExprId FindExactParsedExpressionIdentity(
		const asSParseActionIdentity& identity) const;
	asASTExprId FindParsedExpressionIdentity(
		const asSParseActionIdentity& identity) const;
	asSParseActionIdentity MakeParseActionIdentity(
		asUINT nodeKind, asUINT offset, asUINT length) const;
	void NoteActedDecl(asASTDeclId id);
	void InsertSymbol(asASTDeclId id);
	bool PrepareParsedSource(class asCScriptCode* script);
	asUINT LookupCandidates(const char* name, asCArray<asASTDeclId>& out) const;
	asUINT LookupCandidatesFrom(asASTDeclId start, const char* name, asCArray<asASTDeclId>& out) const;
	asUINT LookupInScope(asASTDeclId scope, const char* name, asCArray<asASTDeclId>& out) const;

	asCASTContext& GetContext() { return context; }
	asCScriptEngine* GetEngine() const { return engine; }
	const asCArray<asCString>& GetDiagnostics() const { return diagnostics; }
	const asCArray<asCSourceRange>& GetDiagnosticRanges() const { return diagnosticRanges; }
	void AddDiagnostic(const char* text);
	void AddDiagnostic(const char* text, const asCSourceRange& range);
private:
	void ContextualizeLambdaToFuncdef(
		asASTExprId inner,
		const asCQualType& destination,
		const asCSourceRange& range);
	bool ReconcileContextualLambdaBody(
		asASTDeclId lambda,
		const asCQualType& expectedReturn,
		const asCSourceRange& range);
	asCQualType ActOnQualTypeSyntaxNode(
		const asSQualTypeSyntaxAction& action,
		asUINT nodeIndex,
		asUINT depth);
	asCSourceRange QualTypeSyntaxNodeRange(
		const asSQualTypeSyntaxNode& node) const;
	asCScriptEngine* engine;
	// Build-only metadata source. Sema consumes PreClassData while declarations
	// are mutable, but never publishes this Engine-local pointer into the AST.
	asCModule* buildModule;
	asCASTContext& context;
	asCArray<asCString> diagnostics;
	asCArray<asCSourceRange> diagnosticRanges;
	asCArray<asASTStmtId> controlStack;
	asCArray<asASTDeclId> declContextStack;
	asASTDeclId lastActedDecl;
	struct asSParsedDeclBinding
	{
		asSParseActionIdentity identity;
		asASTDeclId declaration;
	};
	asCArray<asSParsedDeclBinding> parsedDeclBindings;
	struct asSParsedExprTypeBinding
	{
		asSParseActionIdentity identity;
		asCQualType type;
	};
	asCArray<asSParsedExprTypeBinding> parsedExprTypeBindings;
	struct asSParsedExprBinding
	{
		asSParseActionIdentity identity;
		asASTExprId expression;
	};
	asCArray<asSParsedExprBinding> parsedExprBindings;
	struct asSLambdaSyntaxFact
	{
		asASTDeclId lambda;
		asUINT parameterCount;
		asUINT explicitlyTypedParameterCount;
		// Construction-only identities of formals whose source type was omitted.
		// The ParamDecls themselves become fully typed before publication; this
		// list is never copied to snapshots, providers, relocations, or Cache.
		asCArray<asASTDeclId> omittedParameters;
		asCString boundFuncdefKey;
	};
	asCArray<asSLambdaSyntaxFact> lambdaSyntaxFacts;
	struct asSSemaSymbol
	{
		asASTDeclId scope;
		asCString name;
		asASTDeclId id;
	};
	asCArray<asSSemaSymbol> symbols;
	struct asSDeferredCall
	{
		asASTExprId expression;
		asASTDeclId owner;
		asCString name;
		asCArray<asASTExprId> arguments;
		asCArray<asCString> argumentNames;
		asASTExprId implicitReceiver;
		asCSourceRange range;
		// An explicitly-qualified call must retain its exact syntax identity while
		// later declarations are still being parsed. An empty path denotes an
		// ordinary lexical call; it must never be used as an approximation for a
		// qualifier that failed to resolve.
		asCArray<asCString> scopeSegments;
		asCString unresolvedScopeSegment;
		bool absoluteScope;
		// A resolved explicit qualifier may have an empty copied path only after
		// its scope declaration is already known. Preserve the lookup mode
		// independently so retries never climb from that scope to an ancestor.
		bool exactScope;
	};
	asCArray<asSDeferredCall> deferredCalls;
	struct asSDeferredDeclRef
	{
		asASTExprId expression;
		asASTDeclId owner;
		asCString name;
		asCSourceRange range;
		asCArray<asCString> scopeSegments;
		asCString unresolvedScopeSegment;
		bool absoluteScope;
	};
	asCArray<asSDeferredDeclRef> deferredDeclRefs;
	asASTFileID parsedFile;
	asCString parsedSection;
	asCString parsedSourceBytes;
	int parsedLineOffset;
};

END_AS_NAMESPACE

#endif
