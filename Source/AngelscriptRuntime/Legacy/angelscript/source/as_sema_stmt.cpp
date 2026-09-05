#include "as_sema_stmt.h"
#include "as_sema_lifetime.h"
#include "as_context.h"
#include "as_scriptcode.h"
#include "as_scriptengine.h"
#include "as_tokendef.h"

#include <cstring>
#include <math.h>

BEGIN_AS_NAMESPACE

asASTStmtId asCSema::FindExistingStmt(asEASTStmtKind kind, const asCSourceRange& range) const
{
	if( range.begin.offset == 0 || range.begin.fileID == 0 )
	{
		return asASTStmtId();
	}
	for( asUINT i = 1; i <= context.GetStmtCount(); ++i )
	{
		const asCStmt* existing = context.GetStmt(asASTStmtId(i));
		if( existing
			&& existing->kind == kind
			&& existing->range.begin.fileID == range.begin.fileID
			&& existing->range.begin.offset == range.begin.offset )
		{
			return existing->id;
		}
	}
	return asASTStmtId();
}

static asASTStmtId FindExactStatementAction(
	const asCASTContext& context,
	asEASTStmtKind kind,
	asASTDeclId owner,
	const asCSourceRange& range)
{
	for( asUINT i = 1; i <= context.GetStmtCount(); ++i )
	{
		const asCStmt* const statement = context.GetStmt(asASTStmtId(i));
		if( statement != 0 && statement->kind == kind
			&& statement->owner == owner && statement->range == range )
		{
			return statement->id;
		}
	}
	return asASTStmtId();
}

asASTStmtId asCSema::ActOnReturnStmt(asASTDeclId owner, asASTExprId value, const asCSourceRange& range)
{
	const asASTStmtId existing = FindExistingStmt(asAST_STMT_RETURN, range);
	if( existing.IsValid() )
	{
		context.SetStmtSafePointRole(existing, asAST_SAFEPOINT_RETURN);
		return existing;
	}
	const asASTStmtId id = context.CreateStmt(asAST_STMT_RETURN, owner, range);
	context.SetStmtExpr(id, value);
	context.SetStmtSafePointRole(id, asAST_SAFEPOINT_RETURN);
	return id;
}

static asASTDeclId FindExistingVar(
	asCSema& sema,
	asASTDeclId owner,
	const char* name,
	const asCSourceRange& range)
{
	if( name == 0 || *name == 0 )
	{
		return asASTDeclId();
	}
	asCArray<asASTDeclId> hits;
	sema.LookupInScope(owner, name, hits);
	asCASTContext& context = sema.GetContext();
	for( asUINT i = 0; i < hits.GetLength(); ++i )
	{
		const asCDecl* decl = context.GetDecl(hits[i]);
		const bool sameSourceDeclaration = decl != 0
			&& decl->range.IsValid() && range.IsValid()
			&& decl->range.begin.fileID == range.begin.fileID
			&& ((decl->range.begin.offset <= range.begin.offset
					&& decl->range.end.offset >= range.end.offset)
				|| (range.begin.offset <= decl->range.begin.offset
					&& range.end.offset >= decl->range.end.offset));
		if( decl && decl->kind == asAST_DECL_VAR
			&& (decl->range == range || sameSourceDeclaration) )
		{
			return decl->id;
		}
	}
	return asASTDeclId();
}

static bool QualTypeNeedsDefaultConstruct(asCASTContext& context, const asCQualType& type)
{
	if( !type.IsValid() )
	{
		return false;
	}
	const asCType* named = context.GetType(type.type);
	if( named == 0 )
	{
		return false;
	}
	if( named->kind == asAST_TYPE_VALUE_OBJECT )
	{
		return !type.IsHandle();
	}
	return named->kind == asAST_TYPE_TEMPLATE
		|| named->kind == asAST_TYPE_REFERENCE_OBJECT;
}

static const asCDecl* FindLocalNominalTypeDeclaration(
	asCASTContext& context,
	const asCType* nominalType)
{
	if( nominalType == 0 || nominalType->stableKey.GetLength() == 0 )
	{
		return 0;
	}
	const asCDecl* nativeView = 0;
	for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
	{
		const asCDecl* const candidate = context.GetDecl(asASTDeclId(i));
		if( candidate == 0
			|| (candidate->kind != asAST_DECL_CLASS
				&& candidate->kind != asAST_DECL_INTERFACE)
			|| (!candidate->stableKey.Equals(nominalType->stableKey)
				&& !candidate->name.Equals(nominalType->stableKey)) )
		{
			continue;
		}
		if( !candidate->origin.Equals("canonical-native-type-view") )
		{
			return candidate;
		}
		nativeView = candidate;
	}
	return nativeView;
}

static bool HasLocalZeroArgConstructor(
	asCASTContext& context,
	const asCDecl* declaration)
{
	if( declaration == 0 )
	{
		return false;
	}
	for( asUINT childIndex = 0;
		childIndex < declaration->children.GetLength(); ++childIndex )
	{
		const asCDecl* const child =
			context.GetDecl(declaration->children[childIndex]);
		if( child != 0 && child->kind == asAST_DECL_CONSTRUCTOR
			&& context.GetFormalDeclCount(child->id) == 0 )
		{
			return true;
		}
	}
	return false;
}

static bool QualTypeNeedsImplicitNullInitialization(
	asCSema& sema,
	const asCQualType& type,
	const asCType* nominalType)
{
	asCASTContext& context = sema.GetContext();
	if( nominalType == 0 || type.IsReference() )
	{
		return false;
	}
	if( nominalType->kind == asAST_TYPE_FUNCDEF )
	{
		return true;
	}
	if( nominalType->kind != asAST_TYPE_REFERENCE_OBJECT )
	{
		return false;
	}

	const asCDecl* declaration =
		FindLocalNominalTypeDeclaration(context, nominalType);
	if( declaration != 0
		&& declaration->origin.Equals("canonical-native-type-view") )
	{
		// Native REF types are not uniformly null-initialized: an implicit-handle
		// type may publish a real zero-argument factory. Project that exact Runtime
		// callable first, then distinguish "factory default construction" from the
		// pointer-only/no-factory case by declaration identity rather than by type
		// kind alone.
		sema.InternNativeZeroArgCallablesForType(type);
		declaration = FindLocalNominalTypeDeclaration(context, nominalType);
		return !HasLocalZeroArgConstructor(context, declaration);
	}
	// Authored script classes own a real default constructor even though this
	// fork represents their values as implicit handles. Interfaces and unresolved
	// external reference shapes are pointer-only local storage and start null.
	return declaration == 0 || declaration->kind == asAST_DECL_INTERFACE;
}

static const asCType* ResolveStmtTypedefValueType(const asCASTContext& context, const asCType* nominalType)
{
	const asCType* current = nominalType;
	for( asUINT depth = 0; current != 0 && depth < 8; ++depth )
	{
		if( current->kind == asAST_TYPE_PRIMITIVE || current->kind == asAST_TYPE_ENUM )
		{
			return current;
		}
		const asCDecl* aliasDecl = 0;
		for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
		{
			const asCDecl* candidate = context.GetDecl(asASTDeclId(i));
			if( candidate && candidate->kind == asAST_DECL_TYPEDEF
				&& (candidate->name.Equals(current->stableKey)
					|| candidate->stableKey.Equals(current->stableKey)) )
			{
				aliasDecl = candidate;
				break;
			}
		}
		if( aliasDecl == 0 || !aliasDecl->type.IsValid() )
		{
			return current;
		}
		const asCType* underlying = context.GetType(aliasDecl->type.type);
		if( underlying == 0 || underlying == current )
		{
			return current;
		}
		current = underlying;
	}
	return current;
}

static bool IsNumericScalar(asCASTContext& context, const asCType* nominalType)
{
	const asCType* valueType = ResolveStmtTypedefValueType(context, nominalType);
	if( valueType == 0 )
	{
		return false;
	}
	if( valueType->kind == asAST_TYPE_ENUM )
	{
		return true;
	}
	if( valueType->kind != asAST_TYPE_PRIMITIVE )
	{
		return false;
	}
	return valueType->primitiveToken != ttVoid && valueType->primitiveToken != ttBool;
}

static bool IsScalarValue(asCASTContext& context, const asCType* nominalType)
{
	const asCType* valueType = ResolveStmtTypedefValueType(context, nominalType);
	return valueType != 0
		&& (valueType->kind == asAST_TYPE_ENUM
			|| (valueType->kind == asAST_TYPE_PRIMITIVE
				&& valueType->primitiveToken != ttVoid));
}

static asASTExprId DecayReturnedScalarReferenceToRValue(
	asCSema& sema,
	asCASTContext& context,
	asASTExprId value,
	const asCSourceRange& range)
{
	const asCExpr* const returned = context.GetExpr(value);
	if( returned == 0 || !returned->type.IsReference()
		|| returned->type.IsHandle()
		|| !IsScalarValue(context, context.GetType(returned->type.type)) )
	{
		return value;
	}
	asCQualType destination = returned->type;
	destination.quals &= ~(asAST_QUAL_REFERENCE | asAST_QUAL_PARAM_DIR_MASK);
	return sema.ActOnConversion(value, destination, range);
}

static void AppendImplicitLocalInitialization(
	asCSema& sema,
	asASTDeclId owner,
	asASTDeclId var,
	const asCQualType& type,
	const asCSourceRange& range,
	asCArray<asASTStmtId>& children)
{
	asCASTContext& context = sema.GetContext();
	const asCType* const namedType = type.IsValid()
		? context.GetType(type.type) : 0;
	if( QualTypeNeedsImplicitNullInitialization(sema, type, namedType) )
	{
		// Funcdefs, interfaces and native reference objects without a zero-argument
		// factory use owning pointer storage. The VM frame starts zeroed, but the
		// sealed AST still carries that live-state fact explicitly so cleanup
		// planning and every backend consume the same lifetime contract.
		const asASTExprId value = sema.ActOnNullLiteral(range);
		const asASTExprId ref = sema.ActOnDeclRef(var, type, range);
		context.AddDeclInit(var, value);
		children.PushLast(sema.ActOnExprStmt(
			owner, sema.ActOnAssign(ref, value, type, range), range));
		return;
	}
	if( !QualTypeNeedsDefaultConstruct(context, type) )
	{
		return;
	}
	asCArray<asASTExprId> args;
	const asASTExprId value = sema.ActOnConstruct(type, args, range);
	const asASTExprId ref = sema.ActOnDeclRef(var, type, range);
	context.AddDeclInit(var, value);
	children.PushLast(sema.ActOnExprStmt(owner, sema.ActOnAssign(ref, value, type, range), range));
}

static bool ControlNeedsFill(const asCStmt* stmt)
{
	return stmt != 0 && !stmt->expr.IsValid();
}

static void WireFallthroughTargets(asCASTContext& context, asASTStmtId sw)
{
	const asCStmt* switchStmt = context.GetStmt(sw);
	asCArray<asASTStmtId> cases;
	if( switchStmt )
	{
		for( asUINT i = 0; i < switchStmt->children.GetLength(); ++i )
		{
			const asCStmt* childStmt = context.GetStmt(switchStmt->children[i]);
			if( childStmt && childStmt->kind == asAST_STMT_CASE )
			{
				cases.PushLast(childStmt->id);
			}
		}
	}
	for( asUINT i = 0; i < cases.GetLength(); ++i )
	{
		const asCStmt* caseStmt = context.GetStmt(cases[i]);
		if( caseStmt == 0 )
		{
			continue;
		}
		for( asUINT c = 0; c < caseStmt->children.GetLength(); ++c )
		{
			const asCStmt* inner = context.GetStmt(caseStmt->children[c]);
			if( inner && inner->kind == asAST_STMT_FALLTHROUGH && i + 1 < cases.GetLength() )
			{
				context.SetTarget(inner->id, cases[i + 1]);
			}
		}
	}
}

static void EmitTypedLocalVariableStmts(
	asCSema& sema,
	asASTDeclId owner,
	asASTDeclId var,
	const asCQualType& type,
	asASTExprId init,
	const asCSourceRange& range,
	bool defaultConstruct,
	asCArray<asASTStmtId>& out)
{
	asCASTContext& context = sema.GetContext();
	if( const asCDecl* const existing = context.GetDecl(var) )
	{
		if( range.IsValid() && range.Length() > existing->range.Length() )
		{
			context.SetDeclRange(var, range);
		}
	}
	const asASTStmtId declStmt = context.CreateStmt(asAST_STMT_DECL, owner, range);
	context.SetStmtDecl(declStmt, var);
	out.PushLast(declStmt);
	if( init.IsValid() )
	{
		const asASTExprId rewritten = TryRewriteObjectConvOperator(
			sema, context, sema.GetEngine(), init, type, range, false);
		if( rewritten.IsValid() )
		{
			init = rewritten;
		}
		bool convertedToFuncdef = false;
		if( const asCExpr* const initExpr = context.GetExpr(init) )
		{
			const asCType* const destType = context.GetType(type.type);
			if( destType && destType->kind == asAST_TYPE_FUNCDEF
				&& initExpr->type.type != type.type )
			{
				// Funcdef locals own the function-to-funcdef Conversion on
				// the decl init, not a primitive DeclRef plus sibling Assign.
				init = sema.ActOnConversion(init, type, range);
				convertedToFuncdef = true;
			}
		}
		if( convertedToFuncdef )
		{
			context.PrependDeclInit(var, init);
		}
		else
		{
			context.AddDeclInit(var, init);
		}
		out.PushLast(sema.ActOnExprStmt(owner, sema.ActOnAssignExpr(sema.ActOnDeclRef(var, type, range), init, range), range));
	}
	else if( defaultConstruct )
	{
		AppendImplicitLocalInitialization(sema, owner, var, type, range, out);
	}
}

static void EmitLocalDeclStmts(
	asCSema& sema,
	asASTDeclId owner,
	const char* name,
	const asCQualType& type,
	asASTExprId init,
	const asCSourceRange& range,
	asCArray<asASTStmtId>& out)
{
	asASTDeclId var = FindExistingVar(sema, owner, name, range);
	if( !var.IsValid() )
	{
		var = sema.ActOnVarDecl(owner, name ? name : "", type, range);
	}
	EmitTypedLocalVariableStmts(
		sema, owner, var, type, init, range, true, out);
}

static void MarkAuthoredLocalDeclarationStatements(
	asCASTContext& context,
	const asCArray<asASTStmtId>& statements)
{
	for( asUINT i = 0; i < statements.GetLength(); ++i )
	{
		const asCStmt* const statement = context.GetStmt(statements[i]);
		if( statement != 0 && statement->kind == asAST_STMT_DECL )
		{
			// Only the authored declaration is a source statement. Its sibling
			// initializer ExprStmt is a synthesized semantic phase and must not
			// become a second stepping/safe-point event.
			context.SetStmtSafePointRole(
				statement->id, asAST_SAFEPOINT_STATEMENT);
		}
	}
}

asASTStmtId asCSema::ActOnLocalDeclStmt(asASTDeclId owner, const char* name, const asCQualType& type, asASTExprId init, const asCSourceRange& range)
{
	asCArray<asASTStmtId> children;
	EmitLocalDeclStmts(*this, owner, name, type, init, range, children);
	MarkAuthoredLocalDeclarationStatements(context, children);
	if( children.GetLength() == 1 )
	{
		return children[0];
	}
	return ActOnBlock(owner, children, range);
}

static bool IsTypedLocalVariableOwner(const asCDecl* owner)
{
	return owner != 0
		&& (owner->kind == asAST_DECL_FUNCTION
			|| owner->kind == asAST_DECL_METHOD
			|| owner->kind == asAST_DECL_CONSTRUCTOR
			|| owner->kind == asAST_DECL_DESTRUCTOR
			|| owner->kind == asAST_DECL_MIXIN);
}

bool asCSema::ActOnLocalVariableDeclaratorAction(
	const asSLocalVariableDeclaratorAction& action,
	asCScriptCode* script,
	asCArray<asASTStmtId>& statements)
{
	if( !action.variable.IsValid()
		|| action.kind <= asVARIABLE_INITIALIZER_INVALID
		|| action.kind > asVARIABLE_INITIALIZER_DIRECT_CONSTRUCT
		|| action.endOffset <= action.beginOffset
		|| !PrepareParsedSource(script) )
	{
		AddDiagnostic("local-variable-declarator-action-invalid");
		return false;
	}
	const asCDecl* const variable = context.GetDecl(action.variable);
	const asCDecl* const owner = variable != 0
		? context.GetDecl(variable->parent) : 0;
	if( variable == 0 || variable->kind != asAST_DECL_VAR
		|| !IsTypedLocalVariableOwner(owner) )
	{
		AddDiagnostic("local-variable-declarator-owner-invalid");
		return false;
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("local-variable-declarator-range-invalid");
		return false;
	}
	if( variable->inits.GetLength() != 0 )
	{
		AddDiagnostic("local-variable-initializer-already-bound", range);
		return false;
	}

	asASTExprId initExpr;
	if( action.kind == asVARIABLE_INITIALIZER_NONE )
	{
		if( action.expression.IsValid() || action.arguments.GetLength() != 0
			|| action.argumentNames.GetLength() != 0 )
		{
			AddDiagnostic("local-variable-declarator-action-invalid", range);
			return false;
		}
	}
	else if( action.kind == asVARIABLE_INITIALIZER_EXPRESSION )
	{
		if( !action.expression.IsValid()
			|| action.arguments.GetLength() != 0
			|| action.argumentNames.GetLength() != 0
			|| context.GetExpr(action.expression) == 0 )
		{
			AddDiagnostic("local-variable-initializer-expression-invalid", range);
			return false;
		}
		initExpr = action.expression;
	}
	else
	{
		if( action.expression.IsValid()
			|| (action.argumentNames.GetLength() != 0
				&& action.argumentNames.GetLength()
					!= action.arguments.GetLength()) )
		{
			AddDiagnostic("local-variable-initializer-construct-invalid", range);
			return false;
		}
		for( asUINT i = 0; i < action.arguments.GetLength(); ++i )
		{
			if( !action.arguments[i].IsValid()
				|| context.GetExpr(action.arguments[i]) == 0 )
			{
				AddDiagnostic("local-variable-initializer-construct-invalid", range);
				return false;
			}
		}
	}
	asCQualType committedType;
	if( !InferCanonicalAutoType(
		variable->type, initExpr, true,
		action.autoConstRef, committedType) )
	{
		AddDiagnostic("local-auto-inference-invalid", range);
		return false;
	}
	if( committedType != variable->type )
	{
		if( context.SetDeclType(action.variable, committedType)
			!= asAST_VERIFY_OK )
		{
			AddDiagnostic("local-auto-type-commit-invalid", range);
			return false;
		}
		const asCType* const named = context.GetType(committedType.type);
		if( named != 0 && named->kind != asAST_TYPE_PRIMITIVE
			&& named->kind != asAST_TYPE_VOID
			&& named->stableKey.GetLength() != 0 )
		{
			RecordDependency(
				action.variable, named->stableKey.AddressOf());
		}
	}
	if( action.kind == asVARIABLE_INITIALIZER_DIRECT_CONSTRUCT )
	{
		initExpr = ActOnConstructCall(
			variable->parent,
			committedType,
			action.arguments,
			action.argumentNames.GetLength() != 0
				? &action.argumentNames : 0,
			range);
		if( !initExpr.IsValid() )
		{
			return false;
		}
	}
	EmitTypedLocalVariableStmts(
		*this, variable->parent, action.variable, committedType,
		initExpr, range, true, statements);
	NoteActedDecl(action.variable);
	return true;
}

asASTStmtId asCSema::ActOnLocalVariableDeclarationAction(
	const asCArray<asASTStmtId>& statements,
	asCScriptCode* script,
	asUINT beginOffset,
	asUINT endOffset)
{
	if( statements.GetLength() == 0 || endOffset <= beginOffset
		|| !PrepareParsedSource(script) )
	{
		AddDiagnostic("local-variable-declaration-action-invalid");
		return asASTStmtId();
	}
	const asASTDeclId owner = CurrentDeclContext();
	if( !IsTypedLocalVariableOwner(context.GetDecl(owner)) )
	{
		AddDiagnostic("local-variable-declaration-owner-invalid");
		return asASTStmtId();
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, beginOffset, endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("local-variable-declaration-range-invalid");
		return asASTStmtId();
	}
	MarkAuthoredLocalDeclarationStatements(context, statements);
	return ActOnBlock(owner, statements, range);
}

asASTStmtId asCSema::ActOnForeachVariableAction(
	asASTDeclId variableDecl,
	asCScriptCode* script,
	asUINT beginOffset,
	asUINT endOffset)
{
	if( endOffset <= beginOffset || !PrepareParsedSource(script) )
	{
		AddDiagnostic("foreach-variable-action-invalid");
		return asASTStmtId();
	}
	const asCDecl* const variable = context.GetDecl(variableDecl);
	const asCDecl* const owner = variable != 0
		? context.GetDecl(variable->parent) : 0;
	if( variable == 0 || variable->kind != asAST_DECL_VAR
		|| !IsTypedLocalVariableOwner(owner) )
	{
		AddDiagnostic("foreach-variable-owner-invalid");
		return asASTStmtId();
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, beginOffset, endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("foreach-variable-range-invalid");
		return asASTStmtId();
	}
	asCArray<asASTStmtId> statements;
	EmitTypedLocalVariableStmts(
		*this, variable->parent, variableDecl, variable->type,
		asASTExprId(), range, false, statements);
	NoteActedDecl(variableDecl);
	return statements.GetLength() != 0
		? statements[0] : asASTStmtId();
}

asASTStmtId asCSema::ActOnIfStmt(asASTDeclId owner, asASTExprId cond, asASTStmtId thenStmt, asASTStmtId elseStmt, const asCSourceRange& range)
{
	// LEGACY CompileCondition converts VALUE objects with `bool opImplConv()`
	// before the branch. `if (Object)` must be a stmt cond Call of that method,
	// not leftover DeclRef of T.
	cond = RewriteValueToBoolViaOpImplConv(
		*this, context, engine, cond, range);
	const asASTStmtId existing = FindExistingStmt(asAST_STMT_IF, range);
	if( existing.IsValid() )
	{
		if( cond.IsValid() )
		{
			context.SetStmtExpr(existing, cond);
		}
		context.ClearStmtChildren(existing);
		if( thenStmt.IsValid() )
		{
			context.AddStmtChild(existing, thenStmt);
		}
		if( elseStmt.IsValid() )
		{
			context.AddStmtChild(existing, elseStmt);
		}
		return existing;
	}
	return ActOnIf(owner, cond, thenStmt, elseStmt, range);
}

asASTStmtId asCSema::ActOnWhileStmt(asASTDeclId owner, asASTExprId cond, asASTStmtId body, const asCSourceRange& range)
{
	// LEGACY CompileCondition converts VALUE objects with `bool opImplConv()`
	// before the loop test. `while (Object)` must be a stmt cond Call of that
	// method, not leftover DeclRef of T.
	cond = RewriteValueToBoolViaOpImplConv(
		*this, context, engine, cond, range);
	asASTStmtId loop = FindExistingStmt(asAST_STMT_WHILE, range);
	if( loop.IsValid() && !ControlNeedsFill(context.GetStmt(loop)) )
	{
		if( cond.IsValid() )
		{
			context.SetStmtExpr(loop, cond);
		}
		return loop;
	}
	if( !loop.IsValid() )
	{
		loop = ActOnWhile(owner, cond, body, range);
		PushControl(loop);
		PopControl();
		return loop;
	}
	context.SetStmtExpr(loop, cond);
	context.ClearStmtChildren(loop);
	context.AddStmtChild(loop, body);
	context.SetStmtSafePointRole(loop, asAST_SAFEPOINT_LOOP_BACKEDGE);
	if( body.IsValid() )
	{
		context.SetLoopBodySafePointRole(loop, asAST_SAFEPOINT_LOOP_ENTRY);
	}
	PopControl();
	return loop;
}

asASTStmtId asCSema::ActOnForStmt(asASTDeclId owner, asASTStmtId initStmt, asASTExprId cond, asASTExprId incr, asASTStmtId body, const asCSourceRange& range, const asCSourceRange& incrStmtRange)
{
	// LEGACY CompileCondition converts VALUE objects with `bool opImplConv()`
	// before the loop test. `for (; Object; )` must be a stmt cond Call of that
	// method, not leftover DeclRef of T.
	cond = RewriteValueToBoolViaOpImplConv(
		*this, context, engine, cond, range);
	asASTStmtId loop = FindExistingStmt(asAST_STMT_FOR, range);
	if( loop.IsValid() && !ControlNeedsFill(context.GetStmt(loop)) )
	{
		if( cond.IsValid() )
		{
			context.SetStmtExpr(loop, cond);
		}
		return loop;
	}
	if( !loop.IsValid() )
	{
		loop = ActOnFor(owner, initStmt, cond, incr, body, range);
		PushControl(loop);
		PopControl();
		return loop;
	}
	context.SetStmtExpr(loop, cond);
	context.ClearStmtChildren(loop);
	context.AddStmtChild(loop, initStmt);
	context.AddStmtChild(loop, body);
	if( incr.IsValid() )
	{
		asCSourceRange incrRange = incrStmtRange;
		if( !incrRange.IsValid() )
		{
			if( const asCExpr* const incrExpr = context.GetExpr(incr) )
			{
				if( incrExpr->range.IsValid()
					&& (!range.IsValid()
						|| incrExpr->range.begin.offset != range.begin.offset) )
				{
					incrRange = incrExpr->range;
				}
			}
		}
		if( !incrRange.IsValid() )
		{
			incrRange = range;
		}
		context.AddStmtChild(loop, ActOnExprStmt(owner, incr, incrRange));
	}
	context.SetStmtSafePointRole(loop, asAST_SAFEPOINT_LOOP_BACKEDGE);
	if( body.IsValid() )
	{
		context.SetLoopBodySafePointRole(loop, asAST_SAFEPOINT_LOOP_ENTRY);
	}
	PopControl();
	return loop;
}

asASTStmtId asCSema::ActOnDoWhileStmt(asASTDeclId owner, asASTStmtId body, asASTExprId cond, const asCSourceRange& range)
{
	asASTStmtId loop = FindExistingStmt(asAST_STMT_DO_WHILE, range);
	if( loop.IsValid() && !ControlNeedsFill(context.GetStmt(loop)) )
	{
		return loop;
	}
	if( !loop.IsValid() )
	{
		loop = ActOnDoWhile(owner, body, cond, range);
		PushControl(loop);
		PopControl();
		return loop;
	}
	context.SetStmtExpr(loop, cond);
	context.ClearStmtChildren(loop);
	context.AddStmtChild(loop, body);
	context.SetStmtSafePointRole(loop, asAST_SAFEPOINT_LOOP_BACKEDGE);
	if( body.IsValid() )
	{
		context.SetLoopBodySafePointRole(loop, asAST_SAFEPOINT_LOOP_ENTRY);
	}
	PopControl();
	return loop;
}

static asASTDeclId FindFirstStmtDecl(const asCASTContext& context, asASTStmtId statement)
{
	const asCStmt* const stmt = context.GetStmt(statement);
	if( stmt == 0 )
	{
		return asASTDeclId();
	}
	if( stmt->decl.IsValid() )
	{
		return stmt->decl;
	}
	for( asUINT childIndex = 0; childIndex < stmt->children.GetLength(); ++childIndex )
	{
		const asASTDeclId childDecl = FindFirstStmtDecl(context, stmt->children[childIndex]);
		if( childDecl.IsValid() )
		{
			return childDecl;
		}
	}
	return asASTDeclId();
}

static const asCExpr* ExactResolvedCall(
	const asCASTContext& context,
	asASTExprId expression,
	const char* expectedName)
{
	const asCExpr* expr = context.GetExpr(expression);
	// ActOnCallExpr keeps object-valued call results honest by returning a
	// MaterializeTemporary wrapper.  Protocol recognition needs the sealed
	// callee underneath that Sema-authored lifetime fact, while callers retain
	// the outer expression as the actual initializer/value.
	if( expr != 0
		&& expr->kind == asAST_EXPR_MATERIALIZE_TEMPORARY
		&& expr->children.GetLength() == 1 )
	{
		expr = context.GetExpr(expr->children[0]);
	}
	if( expr == 0 || expr->kind != asAST_EXPR_CALL || !expr->resolvedDecl.IsValid() )
	{
		return 0;
	}
	const asCDecl* const callee = context.GetDecl(expr->resolvedDecl);
	return callee != 0 && callee->name.Equals(expectedName) ? expr : 0;
}

static bool CanSealForeachProtocol(
	const asCASTContext& context,
	asASTStmtId valueStmt,
	asASTStmtId keyStmt,
	asASTExprId rangeExpr)
{
	const asCDecl* const valueDecl = context.GetDecl(FindFirstStmtDecl(context, valueStmt));
	const asCDecl* const keyDecl = keyStmt.IsValid()
		? context.GetDecl(FindFirstStmtDecl(context, keyStmt)) : 0;
	const asCExpr* const authoredRange = context.GetExpr(rangeExpr);
	const asCType* const rangeType = authoredRange != 0
		? context.GetType(authoredRange->type.type) : 0;
	if( valueDecl == 0 || valueDecl->kind != asAST_DECL_VAR
		|| !valueDecl->type.IsValid() || rangeType == 0
		|| (keyStmt.IsValid()
			&& (keyDecl == 0 || keyDecl->kind != asAST_DECL_VAR
				|| !keyDecl->type.IsValid())) )
	{
		return false;
	}
	return rangeType->kind == asAST_TYPE_VALUE_OBJECT
		|| rangeType->kind == asAST_TYPE_REFERENCE_OBJECT
		|| rangeType->kind == asAST_TYPE_TEMPLATE;
}

static bool SourceRangeContains(
	const asCSourceRange& outer,
	const asCSourceRange& inner)
{
	return outer.begin.fileID != 0
		&& outer.begin.fileID == inner.begin.fileID
		&& outer.end.fileID == inner.end.fileID
		&& outer.begin.offset <= inner.begin.offset
		&& inner.end.offset <= outer.end.offset;
}

asASTStmtId asCSema::ActOnForeachStmt(
	asASTDeclId owner,
	asASTStmtId valueStmt,
	asASTExprId rangeExpr,
	asASTStmtId body,
	const asCSourceRange& range)
{
	return ActOnForeachStmt(
		owner, valueStmt, asASTStmtId(), rangeExpr, body, range);
}

asASTStmtId asCSema::ActOnForeachStmt(
	asASTDeclId owner,
	asASTStmtId valueStmt,
	asASTStmtId keyStmt,
	asASTExprId rangeExpr,
	asASTStmtId body,
	const asCSourceRange& range)
{
	asASTStmtId loop = FindExistingStmt(asAST_STMT_FOREACH, range);
	if( loop.IsValid() && !ControlNeedsFill(context.GetStmt(loop)) )
	{
		return loop;
	}
	if( !loop.IsValid() )
	{
		loop = ActOnForeach(owner, valueStmt, rangeExpr, body, range);
		PushControl(loop);
	}

	// Keep the authored ForEach identity, but seal the iteration protocol as the
	// same init / condition / body / increment phases consumed by a canonical
	// For.  This is the CXXForRangeStmt-style boundary: overload selection and
	// the single-evaluation receiver are Sema facts, never backend reconstruction.
	if( CanSealForeachProtocol(context, valueStmt, keyStmt, rangeExpr) )
	{
		const asASTDeclId valueDeclId = FindFirstStmtDecl(context, valueStmt);
		const asASTDeclId keyDeclId = keyStmt.IsValid()
			? FindFirstStmtDecl(context, keyStmt) : asASTDeclId();
		const asCDecl* const valueDecl = context.GetDecl(valueDeclId);
		const asCDecl* const keyDecl = context.GetDecl(keyDeclId);
		const asASTExprId receiver = ActOnOpaqueValueExpr(rangeExpr, range);
		asCArray<asASTExprId> noArguments;
		const asASTExprId beginCallId = ActOnCallExpr(
			owner, "opForBegin", noArguments, range, receiver);
		const asCExpr* const beginCall = ExactResolvedCall(
			context, beginCallId, "opForBegin");
		const asCType* const iteratorType = beginCall != 0
			? context.GetType(beginCall->type.type) : 0;

		// Primitive/enum iterators need no scope-exit action. A value-object
		// iterator uses the same resolved protocol, but its exact declaration and
		// destructor cleanup are sealed as an additional foreach phase below.
		if( beginCall != 0 && iteratorType != 0
			&& (iteratorType->kind == asAST_TYPE_PRIMITIVE
				|| iteratorType->kind == asAST_TYPE_ENUM
				|| iteratorType->kind == asAST_TYPE_VALUE_OBJECT) )
		{
			asCString iteratorName;
			iteratorName.Format("__foreach_iterator_%u", loop.value);
			asCArray<asASTStmtId> initChildren;
			EmitLocalDeclStmts(
				*this,
				owner,
				iteratorName.AddressOf(),
				beginCall->type,
				beginCallId,
				range,
				initChildren);
			const asASTDeclId iteratorDeclId = FindExistingVar(
				*this, owner, iteratorName.AddressOf(), range);
			context.AddDeclTrait(iteratorDeclId, asAST_TRAIT_GENERATED);

			asCArray<asASTExprId> iteratorArgument;
			iteratorArgument.PushLast(ActOnDeclRef(
				iteratorDeclId, beginCall->type, range));
			const asASTExprId endCallId = ActOnCallExpr(
				owner, "opForEnd", iteratorArgument, range, receiver);

			iteratorArgument.SetLength(0);
			iteratorArgument.PushLast(ActOnDeclRef(
				iteratorDeclId, beginCall->type, range));
			const asASTExprId nextCallId = ActOnCallExpr(
				owner, "opForNext", iteratorArgument, range, receiver);

			iteratorArgument.SetLength(0);
			iteratorArgument.PushLast(ActOnDeclRef(
				iteratorDeclId, beginCall->type, range));
			const asASTExprId valueCallId = ActOnCallExpr(
				owner, "opForValue", iteratorArgument, range, receiver);

			asASTExprId keyCallId;
			if( keyDecl != 0 )
			{
				iteratorArgument.SetLength(0);
				iteratorArgument.PushLast(ActOnDeclRef(
					iteratorDeclId, beginCall->type, range));
				keyCallId = ActOnCallExpr(
					owner, "opForKey", iteratorArgument, range, receiver);
			}

			const asCExpr* const endCall = ExactResolvedCall(
				context, endCallId, "opForEnd");
			const asCExpr* const nextCall = ExactResolvedCall(
				context, nextCallId, "opForNext");
			const asCExpr* const valueCall = ExactResolvedCall(
				context, valueCallId, "opForValue");
			const asCExpr* const keyCall = keyDecl != 0
				? ExactResolvedCall(context, keyCallId, "opForKey") : 0;
			if( endCall != 0 && nextCall != 0 && valueCall != 0
				&& (keyDecl == 0 || keyCall != 0) )
			{
				const asCStmt* const authoredBody = context.GetStmt(body);
				if( authoredBody != 0 )
				{
					// Incremental Parser callbacks can visit the authored body before
					// either foreach declaration is in scope. Rebind each exact source
					// declaration only inside this body; unrelated errors stay visible.
					auto rebindBodyDeclaration = [&](asASTDeclId declarationId)
					{
						const asCDecl* const declaration = context.GetDecl(declarationId);
						if( declaration == 0 )
						{
							return;
						}
						for( asUINT expressionIndex = 1;
							expressionIndex <= context.GetExprCount(); ++expressionIndex )
						{
							const asCExpr* const expression = context.GetExpr(
								asASTExprId(expressionIndex));
							if( expression != 0
								&& expression->kind == asAST_EXPR_DECL_REF
								&& !expression->resolvedDecl.IsValid()
								&& expression->literal.Equals(declaration->name)
								&& SourceRangeContains(
									authoredBody->range, expression->range) )
							{
								context.SetResolvedDecl(expression->id, declarationId);
								context.SetExprType(expression->id, declaration->type);
							}
						}

						asCString staleDiagnosticPrefix;
						staleDiagnosticPrefix.Format(
							"unresolved-identifier:%s:", declaration->name.AddressOf());
						for( asUINT diagnosticIndex = 0;
							diagnosticIndex < diagnostics.GetLength(); )
						{
							const bool isBodyPlaceholder =
								diagnosticIndex < diagnosticRanges.GetLength()
								&& diagnostics[diagnosticIndex].GetLength()
									>= staleDiagnosticPrefix.GetLength()
								&& std::strncmp(
									diagnostics[diagnosticIndex].AddressOf(),
									staleDiagnosticPrefix.AddressOf(),
									staleDiagnosticPrefix.GetLength()) == 0
								&& SourceRangeContains(
									authoredBody->range,
									diagnosticRanges[diagnosticIndex]);
							if( isBodyPlaceholder )
							{
								diagnostics.RemoveIndex(diagnosticIndex);
								diagnosticRanges.RemoveIndex(diagnosticIndex);
								continue;
							}
							++diagnosticIndex;
						}
					};
					rebindBodyDeclaration(valueDeclId);
					rebindBodyDeclaration(keyDeclId);
				}

				const asASTStmtId init = ActOnBlock(owner, initChildren, range);
				const asASTExprId condition = ActOnUnary(
					"!", endCallId, endCall->type, range);
				context.AddDeclInit(valueDeclId, valueCallId);
				const asASTExprId valueTarget = ActOnDeclRef(
					valueDeclId, valueDecl->type, range);
				const asASTStmtId valueInit = ActOnExprStmt(
					owner,
					ActOnAssign(valueTarget, valueCallId, valueDecl->type, range),
					range);
				asCArray<asASTStmtId> bodyChildren;
				bodyChildren.PushLast(valueStmt);
				if( keyStmt.IsValid() )
				{
					context.AddDeclInit(keyDeclId, keyCallId);
					const asASTExprId keyTarget = ActOnDeclRef(
						keyDeclId, keyDecl->type, range);
					const asASTStmtId keyInit = ActOnExprStmt(
						owner,
						ActOnAssign(keyTarget, keyCallId, keyDecl->type, range),
						range);
					bodyChildren.PushLast(keyStmt);
					bodyChildren.PushLast(valueInit);
					bodyChildren.PushLast(keyInit);
				}
				else
				{
					bodyChildren.PushLast(valueInit);
				}
				bodyChildren.PushLast(body);
				const asASTStmtId canonicalBody = ActOnBlock(
					owner, bodyChildren, range);
				const asASTStmtId increment = ActOnExprStmt(
					owner, nextCallId, range);
				asASTStmtId cleanup;
				if( iteratorType->kind == asAST_TYPE_VALUE_OBJECT )
				{
					const asASTExprId cleanupTarget = ActOnDeclRef(
						iteratorDeclId, beginCall->type, range);
					const asASTExprId cleanupExpr = ActOnCleanup(
						cleanupTarget, range);
					context.SetLiteral(cleanupExpr, "scope-exit");
					cleanup = ActOnExprStmt(owner, cleanupExpr, range);
				}

				context.SetStmtExpr(loop, condition);
				context.ClearStmtChildren(loop);
				context.AddStmtChild(loop, init);
				context.AddStmtChild(loop, canonicalBody);
				context.AddStmtChild(loop, increment);
				if( cleanup.IsValid() )
				{
					context.AddStmtChild(loop, cleanup);
					asASTExprId iteratorActivation;
					for( asUINT initIndex = 0;
						initIndex < initChildren.GetLength(); ++initIndex )
					{
						const asCStmt* const initStatement =
							context.GetStmt(initChildren[initIndex]);
						const asCExpr* const assignment = initStatement != 0
							&& initStatement->kind == asAST_STMT_EXPR
							? context.GetExpr(initStatement->expr) : 0;
						const asCExpr* const target = assignment != 0
							&& assignment->kind == asAST_EXPR_ASSIGN
							&& assignment->children.GetLength() == 2
							? context.GetExpr(assignment->children[0]) : 0;
						if( target != 0 && target->kind == asAST_EXPR_DECL_REF
							&& target->resolvedDecl == iteratorDeclId )
						{
							iteratorActivation = assignment->id;
							break;
						}
					}
					const asCStmt* const cleanupStatement =
						context.GetStmt(cleanup);
					const asCExpr* const cleanupAction = cleanupStatement != 0
						? context.GetExpr(cleanupStatement->expr) : 0;
					if( !iteratorActivation.IsValid() || cleanupAction == 0
						|| !cleanupAction->resolvedDecl.IsValid() )
					{
						AddDiagnostic("foreach-lifetime-record-invalid", range);
						return asASTStmtId();
					}
					asSASTLifetimeRecord record;
					record.subjectKind =
						asAST_LIFETIME_SUBJECT_FOREACH_ITERATOR;
					record.subject = asSASTLifetimeNodeId::FromDecl(
						iteratorDeclId);
					record.actionKind = asAST_LIFETIME_ACTION_DESTROY_VALUE;
					record.actionTarget = asSASTLifetimeNodeId::FromDecl(
						cleanupAction->resolvedDecl);
					record.activationPoint = asSASTLifetimeNodeId::FromExpr(
						iteratorActivation);
					record.semanticRegion = asSASTLifetimeNodeId::FromStmt(loop);
					record.phase = asAST_LIFETIME_PHASE_LOOP_ITERATION;
					record.supportedExitMask = asAST_LIFETIME_EXIT_ALL;
					record.order = context.GetLifetimeRecordCount();
					if( asSemaAuthorLifetimeRecord(context, record)
						!= asAST_VERIFY_OK )
					{
						AddDiagnostic("foreach-lifetime-record-invalid", range);
						return asASTStmtId();
					}
				}
				context.SetStmtSafePointRole(loop, asAST_SAFEPOINT_LOOP_BACKEDGE);
				context.SetLoopBodySafePointRole(
					loop, asAST_SAFEPOINT_LOOP_ENTRY);
				PopControl();
				return loop;
			}
		}
	}

	// Retain the syntax-shaped node for diagnostics and the LEGACY comparison
	// path when the protocol cannot yet be sealed.  Canonical verification/codegen
	// still fail closed on that shape.
	context.SetStmtExpr(loop, rangeExpr);
	context.ClearStmtChildren(loop);
	context.AddStmtChild(loop, valueStmt);
	if( keyStmt.IsValid() )
	{
		context.AddStmtChild(loop, keyStmt);
	}
	context.AddStmtChild(loop, body);
	context.SetStmtSafePointRole(loop, asAST_SAFEPOINT_LOOP_BACKEDGE);
	if( body.IsValid() )
	{
		context.SetLoopBodySafePointRole(loop, asAST_SAFEPOINT_LOOP_ENTRY);
	}
	PopControl();
	return loop;
}

asASTStmtId asCSema::ActOnSwitchStmt(asASTDeclId owner, asASTExprId cond, const asCSourceRange& range)
{
	asASTStmtId sw = FindExistingStmt(asAST_STMT_SWITCH, range);
	if( sw.IsValid() && !ControlNeedsFill(context.GetStmt(sw)) )
	{
		return sw;
	}
	if( !sw.IsValid() )
	{
		sw = ActOnSwitch(owner, cond, range);
		PushControl(sw);
		return sw;
	}
	if( cond.IsValid() )
	{
		context.SetStmtExpr(sw, cond);
	}
	return sw;
}

static const asCDecl* FindSwitchEnumDecl(
	const asCASTContext& context,
	const asCStmt* sw)
{
	const asCExpr* selector = sw != 0 ? context.GetExpr(sw->expr) : 0;
	const asCType* selectorType = selector != 0
		? context.GetType(selector->type.type) : 0;
	if( selectorType == 0
		|| selectorType->kind != asAST_TYPE_ENUM
		|| selectorType->stableKey.GetLength() == 0 )
	{
		return 0;
	}

	const asCDecl* match = 0;
	for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
	{
		const asCDecl* decl = context.GetDecl(asASTDeclId(i));
		if( decl != 0
			&& decl->kind == asAST_DECL_ENUM
			&& decl->stableKey.Equals(selectorType->stableKey) )
		{
			if( match != 0 )
			{
				return 0;
			}
			match = decl;
		}
	}
	return match;
}

static bool GetSwitchIntegerShape(
	const asCASTContext& context,
	const asCQualType& type,
	bool& outUnsigned,
	asUINT& outBitWidth)
{
	const asCType* const valueType = ResolveStmtTypedefValueType(
		context, context.GetType(type.type));
	if( valueType == 0 )
	{
		return false;
	}
	if( valueType->kind == asAST_TYPE_ENUM )
	{
		outUnsigned = false;
		outBitWidth = 32;
		return true;
	}
	if( valueType->kind != asAST_TYPE_PRIMITIVE )
	{
		return false;
	}
	switch( valueType->primitiveToken )
	{
	case ttInt:
	case ttInt8:
	case ttInt16:
		outUnsigned = false;
		outBitWidth = 32;
		return true;
	case ttUInt:
	case ttUInt8:
	case ttUInt16:
		outUnsigned = true;
		outBitWidth = 32;
		return true;
	case ttInt64:
		outUnsigned = false;
		outBitWidth = 64;
		return true;
	case ttUInt64:
		outUnsigned = true;
		outBitWidth = 64;
		return true;
	default:
		return false;
	}
}

static bool ApplySwitchIntegerConversion(
	const asCASTContext& context,
	const asCQualType& type,
	asQWORD& value)
{
	const asCType* const valueType = ResolveStmtTypedefValueType(
		context, context.GetType(type.type));
	if( valueType == 0 )
	{
		return false;
	}
	if( valueType->kind == asAST_TYPE_ENUM )
	{
		value = static_cast<asQWORD>(static_cast<asINT64>(
			static_cast<int>(static_cast<asDWORD>(value))));
		return true;
	}
	if( valueType->kind != asAST_TYPE_PRIMITIVE )
	{
		return false;
	}
	switch( valueType->primitiveToken )
	{
	case ttInt8:
		value = static_cast<asQWORD>(static_cast<asINT64>(
			static_cast<signed char>(static_cast<asBYTE>(value))));
		return true;
	case ttUInt8:
		value = static_cast<asBYTE>(value);
		return true;
	case ttInt16:
		value = static_cast<asQWORD>(static_cast<asINT64>(
			static_cast<short>(static_cast<asWORD>(value))));
		return true;
	case ttUInt16:
		value = static_cast<asWORD>(value);
		return true;
	case ttInt:
		value = static_cast<asQWORD>(static_cast<asINT64>(
			static_cast<int>(static_cast<asDWORD>(value))));
		return true;
	case ttUInt:
		value = static_cast<asDWORD>(value);
		return true;
	case ttInt64:
	case ttUInt64:
		return true;
	default:
		return false;
	}
}

enum asESwitchConstantDiagnosticKind
{
	asSWITCH_CONSTANT_DIAG_DIVIDE_BY_ZERO = 0,
	asSWITCH_CONSTANT_DIAG_POW_OVERFLOW,
};

struct asSSwitchConstantDiagnostic
{
	asESwitchConstantDiagnosticKind kind;
	asCSourceRange range;

	asSSwitchConstantDiagnostic()
		: kind(asSWITCH_CONSTANT_DIAG_DIVIDE_BY_ZERO)
	{
	}

	asSSwitchConstantDiagnostic(
		asESwitchConstantDiagnosticKind inKind,
		const asCSourceRange& inRange)
		: kind(inKind)
		, range(inRange)
	{
	}
};

// Keep value availability, exact Sema type, and diagnostics orthogonal. This
// mirrors the APValue + EvalStatus shape used by Clang: an operation such as
// integer division by zero may diagnose while still producing the maintained
// recovery value consumed by later Switch duplicate analysis.
struct asSSwitchConstantAnalysis
{
	bool hasValue;
	asQWORD bits;
	asCQualType exactType;
	asCArray<asSSwitchConstantDiagnostic> diagnostics;

	asSSwitchConstantAnalysis()
		: hasValue(false)
		, bits(0)
	{
	}
};

static bool IsSwitchFloatingType(
	const asCASTContext& context,
	const asCQualType& type,
	eTokenType& outToken)
{
	const asCType* const valueType = ResolveStmtTypedefValueType(
		context, context.GetType(type.type));
	if( valueType == 0 || valueType->kind != asAST_TYPE_PRIMITIVE )
	{
		return false;
	}
	outToken = valueType->primitiveToken;
	return outToken == ttFloat || outToken == ttFloat32
		|| outToken == ttDouble || outToken == ttFloat64;
}

static bool IsSwitchBooleanType(
	const asCASTContext& context,
	const asCQualType& type)
{
	const asCType* const valueType = ResolveStmtTypedefValueType(
		context, context.GetType(type.type));
	return valueType != 0 && valueType->kind == asAST_TYPE_PRIMITIVE
		&& valueType->primitiveToken == ttBool;
}

static void AppendSwitchConstantDiagnostics(
	asSSwitchConstantAnalysis& destination,
	const asSSwitchConstantAnalysis& source)
{
	for( asUINT i = 0; i < source.diagnostics.GetLength(); ++i )
	{
		destination.diagnostics.PushLast(source.diagnostics[i]);
	}
}

static void AddSwitchConstantDiagnostic(
	asSSwitchConstantAnalysis& analysis,
	asESwitchConstantDiagnosticKind kind,
	const asCSourceRange& range)
{
	analysis.diagnostics.PushLast(asSSwitchConstantDiagnostic(kind, range));
}

static bool StoreSwitchFloatingBits(
	const asCASTContext& context,
	const asCQualType& type,
	double value,
	asQWORD& outBits)
{
	eTokenType token = ttUnrecognizedToken;
	if( !IsSwitchFloatingType(context, type, token) )
	{
		return false;
	}
	if( token == ttFloat || token == ttFloat32 )
	{
		const float stored = static_cast<float>(value);
		asDWORD bits = 0;
		std::memcpy(&bits, &stored, sizeof(bits));
		outBits = bits;
		return true;
	}
	std::memcpy(&outBits, &value, sizeof(outBits));
	return true;
}

static bool GetSwitchNumericAsDouble(
	const asCASTContext& context,
	const asSSwitchConstantAnalysis& analysis,
	double& outValue)
{
	if( !analysis.hasValue )
	{
		return false;
	}
	eTokenType floatToken = ttUnrecognizedToken;
	if( IsSwitchFloatingType(context, analysis.exactType, floatToken) )
	{
		if( floatToken == ttFloat || floatToken == ttFloat32 )
		{
			const asDWORD bits = static_cast<asDWORD>(analysis.bits);
			float value = 0.0f;
			std::memcpy(&value, &bits, sizeof(value));
			outValue = value;
			return true;
		}
		std::memcpy(&outValue, &analysis.bits, sizeof(outValue));
		return true;
	}
	bool isUnsigned = false;
	asUINT bitWidth = 0;
	if( GetSwitchIntegerShape(
		context, analysis.exactType, isUnsigned, bitWidth) )
	{
		if( isUnsigned )
		{
			outValue = bitWidth == 32
				? static_cast<double>(static_cast<asDWORD>(analysis.bits))
				: static_cast<double>(analysis.bits);
		}
		else
		{
			outValue = bitWidth == 32
				? static_cast<double>(static_cast<int>(
					static_cast<asDWORD>(analysis.bits)))
				: static_cast<double>(static_cast<asINT64>(analysis.bits));
		}
		return true;
	}
	if( IsSwitchBooleanType(context, analysis.exactType) )
	{
		outValue = analysis.bits != 0 ? 1.0 : 0.0;
		return true;
	}
	return false;
}

static bool IsSwitchConstantZero(
	const asCASTContext& context,
	const asSSwitchConstantAnalysis& analysis)
{
	double value = 0.0;
	return GetSwitchNumericAsDouble(context, analysis, value)
		&& value == 0.0;
}

static bool IsSwitchPureConstantDecl(
	const asCASTContext& context,
	const asCDecl* target)
{
	if( target == 0 || target->kind != asAST_DECL_VAR )
	{
		return false;
	}
	const asCDecl* const parent = context.GetDecl(target->parent);
	if( parent != 0 && parent->kind == asAST_DECL_ENUM )
	{
		return target->hasConstantValue;
	}
	if( IsTypedLocalVariableOwner(parent) )
	{
		// Readonly locals may either consume an already sealed value or recurse
		// through their initializer below. Mutable local storage is never a Case
		// constant even when an initializer happened to fold.
		return target->type.IsConst();
	}
	// Global constants must arrive with Sema-sealed bits. Class fields and other
	// readonly storage are not source-language Case constants, and mutable global
	// storage must not become one merely because its initializer folded.
	const bool isGlobal = parent != 0
		&& (parent->kind == asAST_DECL_TRANSLATION_UNIT
			|| parent->kind == asAST_DECL_NAMESPACE);
	return isGlobal && target->type.IsConst() && target->hasConstantValue;
}

static bool IsSwitchComparisonOperator(const asCString& op)
{
	return op.Equals("==") || op.Equals("!=")
		|| op.Equals("<") || op.Equals("<=")
		|| op.Equals(">") || op.Equals(">=");
}

static bool ConvertSwitchConstantValue(
	const asCASTContext& context,
	const asSSwitchConstantAnalysis& source,
	const asCQualType& destination,
	asQWORD& outBits)
{
	bool destinationUnsigned = false;
	asUINT destinationWidth = 0;
	if( GetSwitchIntegerShape(
		context, destination, destinationUnsigned, destinationWidth) )
	{
		eTokenType sourceFloat = ttUnrecognizedToken;
		if( IsSwitchFloatingType(context, source.exactType, sourceFloat) )
		{
			double floating = 0.0;
			if( !GetSwitchNumericAsDouble(context, source, floating) )
			{
				return false;
			}
			outBits = destinationUnsigned
				? (destinationWidth == 32
					? static_cast<asDWORD>(floating)
					: static_cast<asQWORD>(static_cast<asINT64>(floating)))
				: static_cast<asQWORD>(destinationWidth == 32
					? static_cast<asINT64>(static_cast<int>(floating))
					: static_cast<asINT64>(floating));
			return ApplySwitchIntegerConversion(context, destination, outBits);
		}
		if( IsSwitchBooleanType(context, source.exactType) )
		{
			outBits = source.bits != 0 ? 1 : 0;
			return ApplySwitchIntegerConversion(context, destination, outBits);
		}
		bool sourceUnsigned = false;
		asUINT sourceWidth = 0;
		if( !GetSwitchIntegerShape(
			context, source.exactType, sourceUnsigned, sourceWidth) )
		{
			return false;
		}
		outBits = source.bits;
		return ApplySwitchIntegerConversion(context, destination, outBits);
	}
	eTokenType destinationFloat = ttUnrecognizedToken;
	if( IsSwitchFloatingType(context, destination, destinationFloat) )
	{
		double value = 0.0;
		return GetSwitchNumericAsDouble(context, source, value)
			&& StoreSwitchFloatingBits(context, destination, value, outBits);
	}
	if( IsSwitchBooleanType(context, destination) )
	{
		double value = 0.0;
		if( !GetSwitchNumericAsDouble(context, source, value) )
		{
			return false;
		}
		outBits = value != 0.0 ? 1 : 0;
		return true;
	}
	return false;
}

static bool CompareSwitchIntegerConstants(
	const asCASTContext& context,
	const asSSwitchConstantAnalysis& lhs,
	const asSSwitchConstantAnalysis& rhs,
	const asCString& op,
	bool& outValue)
{
	bool isUnsigned = false;
	asUINT bitWidth = 0;
	if( !GetSwitchIntegerShape(context, lhs.exactType, isUnsigned, bitWidth) )
	{
		return false;
	}
	if( bitWidth == 32 && isUnsigned )
	{
		const asDWORD left = static_cast<asDWORD>(lhs.bits);
		const asDWORD right = static_cast<asDWORD>(rhs.bits);
		outValue = op.Equals("==") ? left == right
			: op.Equals("!=") ? left != right
			: op.Equals("<") ? left < right
			: op.Equals("<=") ? left <= right
			: op.Equals(">") ? left > right : left >= right;
		return true;
	}
	if( bitWidth == 32 )
	{
		const int left = static_cast<int>(static_cast<asDWORD>(lhs.bits));
		const int right = static_cast<int>(static_cast<asDWORD>(rhs.bits));
		outValue = op.Equals("==") ? left == right
			: op.Equals("!=") ? left != right
			: op.Equals("<") ? left < right
			: op.Equals("<=") ? left <= right
			: op.Equals(">") ? left > right : left >= right;
		return true;
	}
	if( isUnsigned )
	{
		const asQWORD left = lhs.bits;
		const asQWORD right = rhs.bits;
		outValue = op.Equals("==") ? left == right
			: op.Equals("!=") ? left != right
			: op.Equals("<") ? left < right
			: op.Equals("<=") ? left <= right
			: op.Equals(">") ? left > right : left >= right;
		return true;
	}
	const asINT64 left = static_cast<asINT64>(lhs.bits);
	const asINT64 right = static_cast<asINT64>(rhs.bits);
	outValue = op.Equals("==") ? left == right
		: op.Equals("!=") ? left != right
		: op.Equals("<") ? left < right
		: op.Equals("<=") ? left <= right
		: op.Equals(">") ? left > right : left >= right;
	return true;
}

static void EvaluateSwitchConstant(
	const asCASTContext& context,
	asASTExprId expression,
	asSSwitchConstantAnalysis& out,
	asCArray<asASTDeclId>& activeDecls,
	asUINT depth)
{
	out.hasValue = false;
	out.bits = 0;
	out.exactType = asCQualType();
	out.diagnostics.SetLength(0);
	if( depth > 64 )
	{
		return;
	}
	const asCExpr* const expr = context.GetExpr(expression);
	if( expr == 0 )
	{
		return;
	}
	out.exactType = expr->type;

	if( expr->kind == asAST_EXPR_INTEGER_LITERAL )
	{
		out.bits = expr->literalBits;
		out.hasValue = ApplySwitchIntegerConversion(
			context, expr->type, out.bits);
		return;
	}
	if( expr->kind == asAST_EXPR_FLOAT_LITERAL )
	{
		eTokenType token = ttUnrecognizedToken;
		out.bits = expr->literalBits;
		out.hasValue = IsSwitchFloatingType(context, expr->type, token);
		return;
	}
	if( expr->kind == asAST_EXPR_BOOL_LITERAL )
	{
		out.bits = expr->literalBits != 0 ? 1 : 0;
		out.hasValue = IsSwitchBooleanType(context, expr->type);
		return;
	}
	if( expr->kind == asAST_EXPR_DECL_REF )
	{
		const asCDecl* const target = context.GetDecl(expr->resolvedDecl);
		if( !IsSwitchPureConstantDecl(context, target) )
		{
			return;
		}
		asSSwitchConstantAnalysis source;
		if( target->hasConstantValue )
		{
			source.hasValue = true;
			source.bits = target->constantValue;
			source.exactType = target->type;
		}
		else if( target->inits.GetLength() != 0
			&& activeDecls.IndexOf(target->id) < 0 )
		{
			activeDecls.PushLast(target->id);
			EvaluateSwitchConstant(
				context, target->inits[0], source, activeDecls, depth + 1);
			activeDecls.PopLast();
		}
		AppendSwitchConstantDiagnostics(out, source);
		out.hasValue = source.hasValue && ConvertSwitchConstantValue(
			context, source, expr->type, out.bits);
		return;
	}
	if( expr->kind == asAST_EXPR_UNARY
		&& expr->children.GetLength() == 1 )
	{
		asSSwitchConstantAnalysis inner;
		EvaluateSwitchConstant(
			context, expr->children[0], inner, activeDecls, depth + 1);
		AppendSwitchConstantDiagnostics(out, inner);
		if( !inner.hasValue )
		{
			return;
		}
		if( expr->literal.Equals("!")
			&& IsSwitchBooleanType(context, expr->type) )
		{
			out.bits = inner.bits == 0 ? 1 : 0;
			out.hasValue = true;
			return;
		}
		eTokenType floatToken = ttUnrecognizedToken;
		if( IsSwitchFloatingType(context, expr->type, floatToken) )
		{
			double value = 0.0;
			if( !GetSwitchNumericAsDouble(context, inner, value)
				|| (!expr->literal.Equals("+")
					&& !expr->literal.Equals("-")) )
			{
				return;
			}
			if( expr->literal.Equals("-") )
			{
				value = -value;
			}
			out.hasValue = StoreSwitchFloatingBits(
				context, expr->type, value, out.bits);
			return;
		}
		bool isUnsigned = false;
		asUINT bitWidth = 0;
		if( !GetSwitchIntegerShape(
			context, expr->type, isUnsigned, bitWidth) )
		{
			return;
		}
		out.bits = inner.bits;
		if( expr->literal.Equals("-") )
		{
			out.bits = asQWORD(0) - out.bits;
		}
		else if( expr->literal.Equals("~") )
		{
			out.bits = ~out.bits;
		}
		else if( !expr->literal.Equals("+") )
		{
			return;
		}
		out.hasValue = ApplySwitchIntegerConversion(
			context, expr->type, out.bits);
		return;
	}
	if( (expr->kind == asAST_EXPR_BINARY
			|| expr->kind == asAST_EXPR_LOGICAL)
		&& expr->children.GetLength() == 2 )
	{
		asSSwitchConstantAnalysis lhs;
		asSSwitchConstantAnalysis rhs;
		EvaluateSwitchConstant(
			context, expr->children[0], lhs, activeDecls, depth + 1);
		EvaluateSwitchConstant(
			context, expr->children[1], rhs, activeDecls, depth + 1);
		AppendSwitchConstantDiagnostics(out, lhs);
		AppendSwitchConstantDiagnostics(out, rhs);
		if( !lhs.hasValue || !rhs.hasValue )
		{
			// Keep diagnostics orthogonal to value availability. Legacy diagnoses a
			// constant zero divisor even when the dividend is runtime/nonconstant;
			// Switch analysis then adds its independent nonconstant-Case diagnostic.
			if( (expr->literal.Equals("/") || expr->literal.Equals("%"))
				&& rhs.hasValue && IsSwitchConstantZero(context, rhs) )
			{
				AddSwitchConstantDiagnostic(out,
					asSWITCH_CONSTANT_DIAG_DIVIDE_BY_ZERO, expr->range);
			}
			return;
		}
		if( expr->kind == asAST_EXPR_LOGICAL
			|| expr->literal.Equals("&&") || expr->literal.Equals("||")
			|| expr->literal.Equals("^^") )
		{
			const bool left = lhs.bits != 0;
			const bool right = rhs.bits != 0;
			out.bits = expr->literal.Equals("&&") ? left && right
				: expr->literal.Equals("||") ? left || right
				: left != right;
			out.hasValue = IsSwitchBooleanType(context, expr->type);
			return;
		}
		if( IsSwitchComparisonOperator(expr->literal) )
		{
			bool value = false;
			eTokenType lhsFloat = ttUnrecognizedToken;
			if( IsSwitchFloatingType(context, lhs.exactType, lhsFloat) )
			{
				double left = 0.0;
				double right = 0.0;
				if( !GetSwitchNumericAsDouble(context, lhs, left)
					|| !GetSwitchNumericAsDouble(context, rhs, right) )
				{
					return;
				}
				value = expr->literal.Equals("==") ? left == right
					: expr->literal.Equals("!=") ? left != right
					: expr->literal.Equals("<") ? left < right
					: expr->literal.Equals("<=") ? left <= right
					: expr->literal.Equals(">") ? left > right : left >= right;
			}
			else if( IsSwitchBooleanType(context, lhs.exactType) )
			{
				const bool left = lhs.bits != 0;
				const bool right = rhs.bits != 0;
				value = expr->literal.Equals("==") ? left == right
					: expr->literal.Equals("!=") ? left != right
					: expr->literal.Equals("<") ? left < right
					: expr->literal.Equals("<=") ? left <= right
					: expr->literal.Equals(">") ? left > right : left >= right;
			}
			else if( !CompareSwitchIntegerConstants(
				context, lhs, rhs, expr->literal, value) )
			{
				return;
			}
			out.bits = value ? 1 : 0;
			out.hasValue = IsSwitchBooleanType(context, expr->type);
			return;
		}

		eTokenType resultFloat = ttUnrecognizedToken;
		if( IsSwitchFloatingType(context, expr->type, resultFloat) )
		{
			double left = 0.0;
			double right = 0.0;
			if( !GetSwitchNumericAsDouble(context, lhs, left)
				|| !GetSwitchNumericAsDouble(context, rhs, right) )
			{
				return;
			}
			double value = 0.0;
			if( resultFloat == ttFloat || resultFloat == ttFloat32 )
			{
				const float typedLeft = static_cast<float>(left);
				const float typedRight = static_cast<float>(right);
				float typedValue = 0.0f;
				if( expr->literal.Equals("+") ) typedValue = typedLeft + typedRight;
				else if( expr->literal.Equals("-") ) typedValue = typedLeft - typedRight;
				else if( expr->literal.Equals("*") ) typedValue = typedLeft * typedRight;
				else if( expr->literal.Equals("/") || expr->literal.Equals("%") )
				{
					if( typedRight == 0.0f )
					{
						AddSwitchConstantDiagnostic(out,
							asSWITCH_CONSTANT_DIAG_DIVIDE_BY_ZERO, expr->range);
						typedValue = 0.0f;
					}
					else
					{
						typedValue = expr->literal.Equals("/")
							? typedLeft / typedRight
							: fmodf(typedLeft, typedRight);
					}
				}
				else if( expr->literal.Equals("**") )
				{
					typedValue = powf(typedLeft, typedRight);
					if( typedValue == HUGE_VAL )
					{
						AddSwitchConstantDiagnostic(out,
							asSWITCH_CONSTANT_DIAG_POW_OVERFLOW, expr->range);
					}
				}
				else return;
				value = typedValue;
			}
			else
			{
				if( expr->literal.Equals("+") ) value = left + right;
				else if( expr->literal.Equals("-") ) value = left - right;
				else if( expr->literal.Equals("*") ) value = left * right;
				else if( expr->literal.Equals("/") || expr->literal.Equals("%") )
				{
					if( right == 0.0 )
					{
						AddSwitchConstantDiagnostic(out,
							asSWITCH_CONSTANT_DIAG_DIVIDE_BY_ZERO, expr->range);
						value = 0.0;
					}
					else
					{
						value = expr->literal.Equals("/")
							? left / right : fmod(left, right);
					}
				}
				else if( expr->literal.Equals("**") )
				{
					value = pow(left, right);
					if( value == HUGE_VAL )
					{
						AddSwitchConstantDiagnostic(out,
							asSWITCH_CONSTANT_DIAG_POW_OVERFLOW, expr->range);
					}
				}
				else return;
			}
			out.hasValue = StoreSwitchFloatingBits(
				context, expr->type, value, out.bits);
			return;
		}

		bool isUnsigned = false;
		asUINT bitWidth = 0;
		if( !GetSwitchIntegerShape(
			context, expr->type, isUnsigned, bitWidth) )
		{
			return;
		}
		const asQWORD left = lhs.bits;
		const asQWORD right = rhs.bits;
		if( expr->literal.Equals("+") ) out.bits = left + right;
		else if( expr->literal.Equals("-") ) out.bits = left - right;
		else if( expr->literal.Equals("*") ) out.bits = left * right;
		else if( expr->literal.Equals("/") || expr->literal.Equals("%") )
		{
			if( right == 0 )
			{
				out.bits = 0;
				AddSwitchConstantDiagnostic(out,
					asSWITCH_CONSTANT_DIAG_DIVIDE_BY_ZERO, expr->range);
			}
			else
			{
				const bool minMinusOne = bitWidth == 32
					? static_cast<asDWORD>(left) == 0x80000000u
						&& static_cast<asDWORD>(right) == 0xFFFFFFFFu
					: left == (asQWORD(1) << 63) && right == ~asQWORD(0);
				if( minMinusOne ) out.bits = 0;
				else if( bitWidth == 32 && isUnsigned )
					out.bits = expr->literal.Equals("/")
						? static_cast<asDWORD>(left) / static_cast<asDWORD>(right)
						: static_cast<asDWORD>(left) % static_cast<asDWORD>(right);
				else if( bitWidth == 32 )
					out.bits = static_cast<asQWORD>(expr->literal.Equals("/")
						? static_cast<int>(static_cast<asDWORD>(left))
							/ static_cast<int>(static_cast<asDWORD>(right))
						: static_cast<int>(static_cast<asDWORD>(left))
							% static_cast<int>(static_cast<asDWORD>(right)));
				else if( isUnsigned )
					out.bits = expr->literal.Equals("/") ? left / right : left % right;
				else
					out.bits = static_cast<asQWORD>(expr->literal.Equals("/")
						? static_cast<asINT64>(left) / static_cast<asINT64>(right)
						: static_cast<asINT64>(left) % static_cast<asINT64>(right));
			}
		}
		else if( expr->literal.Equals("**") )
		{
			bool overflow = false;
			if( bitWidth == 32 && isUnsigned )
				out.bits = as_powu(static_cast<asDWORD>(left),
					static_cast<asDWORD>(right), overflow);
			else if( bitWidth == 32 )
				out.bits = static_cast<asQWORD>(as_powi(
					static_cast<int>(static_cast<asDWORD>(left)),
					static_cast<int>(static_cast<asDWORD>(right)), overflow));
			else if( isUnsigned ) out.bits = as_powu64(left, right, overflow);
			else out.bits = static_cast<asQWORD>(as_powi64(
				static_cast<asINT64>(left), static_cast<asINT64>(right), overflow));
			if( overflow ) AddSwitchConstantDiagnostic(out,
				asSWITCH_CONSTANT_DIAG_POW_OVERFLOW, expr->range);
		}
		else if( expr->literal.Equals("&") ) out.bits = left & right;
		else if( expr->literal.Equals("|") ) out.bits = left | right;
		else if( expr->literal.Equals("^") ) out.bits = left ^ right;
		else if( expr->literal.Equals("<<") )
		{
			const asUINT mask = bitWidth == 32 ? 31u : 63u;
			out.bits = bitWidth == 32
				? static_cast<asDWORD>(left) << (right & mask)
				: left << (right & mask);
		}
		else if( expr->literal.Equals(">>") )
		{
			const asUINT mask = bitWidth == 32 ? 31u : 63u;
			out.bits = bitWidth == 32
				? static_cast<asDWORD>(left) >> (right & mask)
				: left >> (right & mask);
		}
		else if( expr->literal.Equals(">>>") )
		{
			const asUINT mask = bitWidth == 32 ? 31u : 63u;
			out.bits = bitWidth == 32
				? static_cast<asQWORD>(static_cast<int>(
					static_cast<asDWORD>(left)) >> (right & mask))
				: static_cast<asQWORD>(static_cast<asINT64>(left) >> (right & mask));
		}
		else return;
		out.hasValue = ApplySwitchIntegerConversion(
			context, expr->type, out.bits);
		return;
	}
	if( (expr->kind == asAST_EXPR_CONSTRUCT
			|| expr->kind == asAST_EXPR_CONVERSION
			|| expr->kind == asAST_EXPR_MATERIALIZE_TEMPORARY
			|| expr->kind == asAST_EXPR_CLEANUP)
		&& expr->children.GetLength() == 1 )
	{
		asSSwitchConstantAnalysis child;
		EvaluateSwitchConstant(
			context, expr->children[0], child, activeDecls, depth + 1);
		AppendSwitchConstantDiagnostics(out, child);
		out.hasValue = child.hasValue && ConvertSwitchConstantValue(
			context, child, expr->type, out.bits);
	}
}

static void EvaluateSwitchConstant(
	const asCASTContext& context,
	asASTExprId expression,
	asSSwitchConstantAnalysis& out)
{
	asCArray<asASTDeclId> activeDecls;
	EvaluateSwitchConstant(context, expression, out, activeDecls, 0);
}

static bool GetSwitchPrimitiveComparisonDomain(
	asCASTContext& context,
	asASTExprId selector,
	asCQualType& outDomain)
{
	outDomain = asCQualType();
	const asCExpr* const selectorExpr = context.GetExpr(selector);
	const asCType* const selectorType = selectorExpr != 0
		? ResolveStmtTypedefValueType(
			context, context.GetType(selectorExpr->type.type))
		: 0;
	if( selectorType == 0 || selectorType->kind != asAST_TYPE_PRIMITIVE )
	{
		// Enum selectors retain their nominal type so enum compatibility and the
		// invalid-value safe-point fact remain explicit. Primitive selectors use
		// the Legacy language's signed/unsigned 32-bit comparison domain.
		return false;
	}

	switch( selectorType->primitiveToken )
	{
	case ttInt:
	case ttInt8:
	case ttInt16:
	case ttInt64:
		outDomain = context.InternPrimitive(ttInt, 0);
		break;
	case ttUInt:
	case ttUInt8:
	case ttUInt16:
	case ttUInt64:
		outDomain = context.InternPrimitive(ttUInt, 0);
		break;
	default:
		return false;
	}
	return outDomain.IsValid();
}

enum asESwitchDomainSealResult
{
	asSWITCH_DOMAIN_SEAL_OK = 0,
	asSWITCH_DOMAIN_SEAL_NON_INTEGRAL,
	asSWITCH_DOMAIN_SEAL_DIAGNOSED,
};

static const asCDecl* FindSwitchEnumDeclForType(
	const asCASTContext& context,
	const asCType* enumType)
{
	if( enumType == 0 || enumType->kind != asAST_TYPE_ENUM
		|| enumType->stableKey.GetLength() == 0 )
	{
		return 0;
	}
	const asCDecl* match = 0;
	for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
	{
		const asCDecl* const decl = context.GetDecl(asASTDeclId(i));
		if( decl != 0 && decl->kind == asAST_DECL_ENUM
			&& decl->stableKey.Equals(enumType->stableKey) )
		{
			if( match != 0 )
			{
				return 0;
			}
			match = decl;
		}
	}
	return match;
}

static asESwitchDomainSealResult NormalizeSwitchComparisonDomain(
	asCSema& sema,
	asCASTContext& context,
	asASTExprId& selector,
	const asCArray<asASTStmtId>& cases,
	const asCSourceRange& switchRange,
	asCSourceRange& outDiagnosticRange)
{
	const asCExpr* const authoredSelector = context.GetExpr(selector);
	const asCType* const authoredSelectorType = authoredSelector != 0
		? ResolveStmtTypedefValueType(
			context, context.GetType(authoredSelector->type.type))
		: 0;
	asCQualType domain;
	const bool primitiveDomain =
		GetSwitchPrimitiveComparisonDomain(context, selector, domain);
	const bool enumDomain = authoredSelectorType != 0
		&& authoredSelectorType->kind == asAST_TYPE_ENUM;
	if( !primitiveDomain && !enumDomain )
	{
		if( authoredSelector != 0 && authoredSelectorType != 0
			&& authoredSelectorType->kind != asAST_TYPE_INVALID
			&& authoredSelectorType->kind != asAST_TYPE_ERROR )
		{
			outDiagnosticRange = authoredSelector->range.IsValid()
				? authoredSelector->range : switchRange;
			return asSWITCH_DOMAIN_SEAL_NON_INTEGRAL;
		}
		return asSWITCH_DOMAIN_SEAL_OK;
	}
	if( enumDomain )
	{
		// Preserve nominal enum identity in the Canonical graph. The sealed Case
		// fact still stores the maintained 32-bit comparison value, while its
		// exact QualType records which enum the Switch selects.
		domain = authoredSelector->type;
	}

	// Analyse the complete ordered Case set before mutating the graph. A failed
	// duplicate/constant check therefore cannot leave a recovery Switch with a
	// converted prefix and authored suffix.
	asCArray<asDWORD> normalizedByCase;
	normalizedByCase.SetLength(cases.GetLength());
	asCArray<asBYTE> hasNormalizedValue;
	hasNormalizedValue.SetLength(cases.GetLength());
	for( asUINT i = 0; i < cases.GetLength(); ++i )
	{
		normalizedByCase[i] = 0;
		hasNormalizedValue[i] = 0;
	}

	asCArray<asDWORD> normalizedValues;
	bool diagnosed = false;
	for( asUINT i = 0; i < cases.GetLength(); ++i )
	{
		const asCStmt* caseStmt = context.GetStmt(cases[i]);
		if( caseStmt == 0 || !caseStmt->expr.IsValid() )
		{
			continue;
		}
		const asCExpr* caseExpr = context.GetExpr(caseStmt->expr);
		if( caseExpr == 0 )
		{
			const asCSourceRange diagnosticRange = caseStmt->range.IsValid()
				? caseStmt->range : switchRange;
			sema.AddDiagnostic(
				"Case expressions must be constants", diagnosticRange);
			diagnosed = true;
			continue;
		}
		if( enumDomain && sema.GetEngine() != 0
			&& sema.GetEngine()->ep.typeCheckSwitchEnums )
		{
			const asCType* const caseType = ResolveStmtTypedefValueType(
				context, context.GetType(caseExpr->type.type));
			if( caseType != 0 && caseType->kind == asAST_TYPE_ENUM
				&& !caseType->stableKey.Equals(
					authoredSelectorType->stableKey) )
			{
				const asCDecl* const selectorDecl =
					FindSwitchEnumDeclForType(context, authoredSelectorType);
				const asCDecl* const caseDecl =
					FindSwitchEnumDeclForType(context, caseType);
				const char* const selectorName = selectorDecl != 0
					? selectorDecl->name.AddressOf()
					: authoredSelectorType->stableKey.AddressOf();
				const char* const caseName = caseDecl != 0
					? caseDecl->name.AddressOf()
					: caseType->stableKey.AddressOf();
				asCString diagnostic;
				diagnostic.Format(
					"Enumeration types are not the same in switch statement ('%s' and '%s') ",
					selectorName, caseName);
				const asCSourceRange diagnosticRange = caseExpr->range.IsValid()
					? caseExpr->range
					: (caseStmt->range.IsValid()
						? caseStmt->range : switchRange);
				sema.AddDiagnostic(diagnostic.AddressOf(), diagnosticRange);
				diagnosed = true;
			}
		}

		const asCSourceRange caseDiagnosticRange = caseExpr->range.IsValid()
			? caseExpr->range
			: (caseStmt->range.IsValid() ? caseStmt->range : switchRange);
		asSSwitchConstantAnalysis analysis;
		EvaluateSwitchConstant(context, caseStmt->expr, analysis);
		for( asUINT diagnosticIndex = 0;
			diagnosticIndex < analysis.diagnostics.GetLength(); ++diagnosticIndex )
		{
			const asSSwitchConstantDiagnostic& diagnostic =
				analysis.diagnostics[diagnosticIndex];
			const asCSourceRange diagnosticRange = diagnostic.range.IsValid()
				? diagnostic.range : caseDiagnosticRange;
			sema.AddDiagnostic(
				diagnostic.kind == asSWITCH_CONSTANT_DIAG_DIVIDE_BY_ZERO
					? "Divide by zero" : "Overflow in exponent operation",
				diagnosticRange);
			diagnosed = true;
		}

		bool caseUnsigned = false;
		asUINT caseBitWidth = 0;
		const bool integralCaseType = GetSwitchIntegerShape(
			context, analysis.exactType, caseUnsigned, caseBitWidth);
		if( !analysis.hasValue )
		{
			sema.AddDiagnostic(
				"Case expressions must be constants", caseDiagnosticRange);
			diagnosed = true;
		}
		if( !integralCaseType )
		{
			sema.AddDiagnostic(
				"Switch expressions must be integral numbers", caseDiagnosticRange);
			diagnosed = true;
		}
		if( !analysis.hasValue || !integralCaseType )
		{
			continue;
		}

		asQWORD bits = analysis.bits;
		if( !ApplySwitchIntegerConversion(context, domain, bits) )
		{
			sema.AddDiagnostic(
				"Switch expressions must be integral numbers", caseDiagnosticRange);
			diagnosed = true;
			continue;
		}
		// Switch comparison is intentionally 32-bit even for int64/uint64
		// source expressions. Match Legacy GetConstantDW() after conversion.
		const asDWORD normalized = static_cast<asDWORD>(bits);
		if( normalizedValues.IndexOf(normalized) >= 0 )
		{
			sema.AddDiagnostic(
				"Duplicate switch case",
				caseStmt->range.IsValid() ? caseStmt->range : switchRange);
			diagnosed = true;
			continue;
		}
		normalizedValues.PushLast(normalized);
		normalizedByCase[i] = normalized;
		hasNormalizedValue[i] = 1;
	}
	if( diagnosed )
	{
		return asSWITCH_DOMAIN_SEAL_DIAGNOSED;
	}

	// Commit only after the whole analysis succeeds. The selector conversion
	// and every Case-domain wrapper are explicit AST facts; authored Case
	// expressions remain their unique children.
	const asCExpr* selectorExpr = context.GetExpr(selector);
	if( primitiveDomain && selectorExpr != 0
		&& selectorExpr->type != domain )
	{
		selector = sema.ActOnConversion(selector, domain,
			selectorExpr->range.IsValid() ? selectorExpr->range : switchRange);
	}
	for( asUINT i = 0; i < cases.GetLength(); ++i )
	{
		if( !hasNormalizedValue[i] )
		{
			continue;
		}
		const asCStmt* const caseStmt = context.GetStmt(cases[i]);
		const asCExpr* const authored = caseStmt != 0
			? context.GetExpr(caseStmt->expr) : 0;
		if( caseStmt == 0 || authored == 0 )
		{
			continue;
		}
		const asCSourceRange factRange = authored->range.IsValid()
			? authored->range
			: (caseStmt->range.IsValid() ? caseStmt->range : switchRange);
		const asASTExprId fact = sema.ActOnConversion(
			caseStmt->expr, domain, factRange);
		context.SetLiteral(fact, "switch-case-domain");
		context.SetLiteralBits(fact, normalizedByCase[i]);
		context.SetStmtExpr(caseStmt->id, fact);
	}
	return asSWITCH_DOMAIN_SEAL_OK;
}

static bool SwitchHasInvalidEnumValueEdge(
	const asCASTContext& context,
	const asCStmt* sw)
{
	const asCDecl* enumDecl = FindSwitchEnumDecl(context, sw);
	if( sw == 0 || enumDecl == 0 )
	{
		return false;
	}

	asCArray<asQWORD> caseValues;
	for( asUINT i = 0; i < sw->children.GetLength(); ++i )
	{
		const asCStmt* caseStmt = context.GetStmt(sw->children[i]);
		if( caseStmt == 0 || caseStmt->kind != asAST_STMT_CASE )
		{
			return false;
		}
		if( !caseStmt->expr.IsValid() )
		{
			// A source default handles every unmatched value and therefore owns no
			// implicit invalid-enum edge.
			return false;
		}
		const asCExpr* const fact = context.GetExpr(caseStmt->expr);
		if( fact == 0 || fact->kind != asAST_EXPR_CONVERSION
			|| !fact->literal.Equals("switch-case-domain") )
		{
			return false;
		}
		caseValues.PushLast(fact->literalBits);
	}

	for( asUINT i = 0; i < enumDecl->children.GetLength(); ++i )
	{
		const asCDecl* valueDecl = context.GetDecl(enumDecl->children[i]);
		if( valueDecl == 0
			|| valueDecl->kind != asAST_DECL_VAR
			|| !valueDecl->hasConstantValue )
		{
			continue;
		}
		// Match the legacy compiler's exact, case-sensitive enum sentinel rule:
		// MAX and names ending in _MAX describe the range and are not values that
		// an exhaustive source switch must spell as cases.
		if( valueDecl->name.Equals("MAX") || valueDecl->name.EndsWith("_MAX") )
		{
			continue;
		}
		bool covered = false;
		const asQWORD normalizedEnumValue = static_cast<asQWORD>(
			static_cast<asDWORD>(valueDecl->constantValue));
		for( asUINT caseIndex = 0;
			caseIndex < caseValues.GetLength(); ++caseIndex )
		{
			if( caseValues[caseIndex] == normalizedEnumValue )
			{
				covered = true;
				break;
			}
		}
		if( !covered )
		{
			return false;
		}
	}
	return enumDecl->children.GetLength() != 0;
}

void asCSema::FinishSwitchStmt(asASTStmtId sw)
{
	WireFallthroughTargets(context, sw);
	context.SetStmtSafePointRole(
		sw,
		SwitchHasInvalidEnumValueEdge(context, context.GetStmt(sw))
			? asAST_SAFEPOINT_SWITCH_INVALID_VALUE
			: asAST_SAFEPOINT_STATEMENT);
	if( controlStack.GetLength() && controlStack[controlStack.GetLength() - 1] == sw )
	{
		PopControl();
	}
}

asASTStmtId asCSema::ActOnCompoundStmt(asASTDeclId owner, const asCArray<asASTStmtId>& children, const asCSourceRange& range)
{
	const asASTStmtId existing = FindExistingStmt(asAST_STMT_BLOCK, range);
	if( existing.IsValid() )
	{
		const asCStmt* stmt = context.GetStmt(existing);
		if( stmt && stmt->owner == owner )
		{
			return existing;
		}
	}
	return ActOnBlock(owner, children, range);
}

namespace
{
	struct asSLexicalBinding
	{
		asCString name;
		asASTDeclId declaration;
	};

	static bool IsLexicalFunctionLike(asEASTDeclKind kind)
	{
		return kind == asAST_DECL_FUNCTION
			|| kind == asAST_DECL_METHOD
			|| kind == asAST_DECL_CONSTRUCTOR
			|| kind == asAST_DECL_DESTRUCTOR
			|| kind == asAST_DECL_MIXIN;
	}

	static asASTDeclId FindActiveLexicalBinding(
		const asCArray<asSLexicalBinding>& bindings,
		const char* name)
	{
		if( name == 0 || *name == 0 )
		{
			return asASTDeclId();
		}
		for( asUINT index = bindings.GetLength(); index > 0; --index )
		{
			if( bindings[index - 1].name.Equals(name) )
			{
				return bindings[index - 1].declaration;
			}
		}
		return asASTDeclId();
	}

	static asASTDeclId FindNonLocalLexicalFallback(
		asCSema& sema,
		const asCDecl& function,
		const char* name)
	{
		asCArray<asASTDeclId> candidates;
		sema.LookupCandidatesFrom(function.parent, name, candidates);
		asCASTContext& context = sema.GetContext();
		for( asUINT index = 0; index < candidates.GetLength(); ++index )
		{
			const asCDecl* const candidate = context.GetDecl(candidates[index]);
			if( candidate != 0
				&& (candidate->kind == asAST_DECL_VAR
					|| candidate->kind == asAST_DECL_PROPERTY) )
			{
				return candidate->id;
			}
		}
		return asASTDeclId();
	}

	static void ResolveLexicalExpr(
		asCSema& sema,
		const asCDecl& function,
		asASTExprId expressionId,
		const asCArray<asSLexicalBinding>& bindings)
	{
		asCASTContext& context = sema.GetContext();
		const asCExpr* const expression = context.GetExpr(expressionId);
		if( expression == 0 )
		{
			return;
		}

		if( expression->kind == asAST_EXPR_DECL_REF
			&& expression->literal.GetLength() != 0 )
		{
			const asASTDeclId active = FindActiveLexicalBinding(
				bindings, expression->literal.AddressOf());
			if( active.IsValid() )
			{
				const asCDecl* const declaration = context.GetDecl(active);
				context.SetResolvedDecl(expression->id, active);
				if( declaration != 0 && declaration->type.IsValid() )
				{
					context.SetExprType(expression->id, declaration->type);
				}
			}
			else
			{
				const asCDecl* const current = context.GetDecl(
					expression->resolvedDecl);
				const bool namesFunctionLocal = current != 0
					&& current->parent == function.id
					&& (current->kind == asAST_DECL_VAR
						|| current->kind == asAST_DECL_PARAM);
				if( namesFunctionLocal )
				{
					const asASTDeclId fallback = FindNonLocalLexicalFallback(
						sema, function, expression->literal.AddressOf());
					if( fallback.IsValid() )
					{
						const asCDecl* const declaration = context.GetDecl(fallback);
						context.SetResolvedDecl(expression->id, fallback);
						if( declaration != 0 && declaration->type.IsValid() )
						{
							context.SetExprType(expression->id, declaration->type);
						}
					}
					else
					{
						context.SetResolvedDecl(expression->id, asASTDeclId());
						asCString diagnostic;
						diagnostic.Format(
							"unresolved-identifier:%s: '%s' is not declared in this scope",
							expression->literal.AddressOf(),
							expression->literal.AddressOf());
						sema.AddDiagnostic(diagnostic.AddressOf(), expression->range);
					}
				}
			}
		}

		ResolveLexicalExpr(sema, function, expression->receiver, bindings);
		for( asUINT childIndex = 0;
			childIndex < expression->children.GetLength(); ++childIndex )
		{
			ResolveLexicalExpr(
				sema, function, expression->children[childIndex], bindings);
		}
	}

	static void AddLexicalDeclaration(
		asCSema& sema,
		const asCDecl& function,
		asASTDeclId declarationId,
		asCArray<asSLexicalBinding>& bindings,
		const asCArray<asUINT>& scopeOffsets)
	{
		asCASTContext& context = sema.GetContext();
		const asCDecl* const declaration = context.GetDecl(declarationId);
		if( declaration == 0 || declaration->name.GetLength() == 0
			|| (declaration->kind != asAST_DECL_VAR
				&& declaration->kind != asAST_DECL_PARAM) )
		{
			return;
		}

		const asUINT currentScopeBegin = scopeOffsets.GetLength() != 0
			? scopeOffsets[scopeOffsets.GetLength() - 1] : 0;
		for( asUINT index = currentScopeBegin; index < bindings.GetLength(); ++index )
		{
			if( bindings[index].name.Equals(declaration->name) )
			{
				asCString diagnostic;
				diagnostic.Format(
					"redeclaration:%s: '%s' is already declared in this scope",
					declaration->name.AddressOf(), declaration->name.AddressOf());
				sema.AddDiagnostic(diagnostic.AddressOf(), declaration->range);
				break;
			}
		}

		asSLexicalBinding binding;
		binding.name = declaration->name;
		binding.declaration = declaration->id;
		bindings.PushLast(binding);
		(void)function;
	}

	static void ResolveLexicalStmt(
		asCSema& sema,
		const asCDecl& function,
		asASTStmtId statementId,
		asCArray<asSLexicalBinding>& bindings,
		asCArray<asUINT>& scopeOffsets,
		bool functionBody = false);

	static void ResolveLexicalStmtChildren(
		asCSema& sema,
		const asCDecl& function,
		const asCStmt& statement,
		asCArray<asSLexicalBinding>& bindings,
		asCArray<asUINT>& scopeOffsets)
	{
		for( asUINT childIndex = 0;
			childIndex < statement.children.GetLength(); ++childIndex )
		{
			ResolveLexicalStmt(
				sema, function, statement.children[childIndex],
				bindings, scopeOffsets);
		}
	}

	static void ResolveLexicalStmt(
		asCSema& sema,
		const asCDecl& function,
		asASTStmtId statementId,
		asCArray<asSLexicalBinding>& bindings,
		asCArray<asUINT>& scopeOffsets,
		bool functionBody)
	{
		asCASTContext& context = sema.GetContext();
		const asCStmt* const statement = context.GetStmt(statementId);
		if( statement == 0 )
		{
			return;
		}

		if( statement->kind == asAST_STMT_BLOCK )
		{
			const asUINT restoreLength = bindings.GetLength();
			if( !functionBody )
			{
				scopeOffsets.PushLast(restoreLength);
			}
			ResolveLexicalStmtChildren(
				sema, function, *statement, bindings, scopeOffsets);
			if( !functionBody )
			{
				bindings.SetLength(restoreLength);
				scopeOffsets.PopLast();
			}
			return;
		}

		if( statement->kind == asAST_STMT_DECL )
		{
			AddLexicalDeclaration(
				sema, function, statement->decl, bindings, scopeOffsets);
			return;
		}

		if( statement->kind == asAST_STMT_FOR )
		{
			const asUINT restoreLength = bindings.GetLength();
			scopeOffsets.PushLast(restoreLength);
			if( statement->children.GetLength() > 0 )
			{
				const asCStmt* const initializer = context.GetStmt(
					statement->children[0]);
				// A source-level for initializer cannot contain an authored `{}`
				// statement. ActOnLocalDeclStmt nevertheless represents
				// `int I = 0` as a synthetic Block holding the DeclStmt and its
				// explicit initializer assignment. That Block is a sequence, not
				// a nested lexical scope: I remains visible to the condition,
				// increment, and body and expires only with the enclosing For.
				if( initializer != 0
					&& initializer->kind == asAST_STMT_BLOCK )
				{
					ResolveLexicalStmtChildren(
						sema, function, *initializer, bindings, scopeOffsets);
				}
				else
				{
					ResolveLexicalStmt(sema, function, statement->children[0],
						bindings, scopeOffsets);
				}
			}
			ResolveLexicalExpr(sema, function, statement->expr, bindings);
			for( asUINT childIndex = 1;
				childIndex < statement->children.GetLength(); ++childIndex )
			{
				ResolveLexicalStmt(sema, function, statement->children[childIndex],
					bindings, scopeOffsets);
			}
			bindings.SetLength(restoreLength);
			scopeOffsets.PopLast();
			return;
		}

		if( statement->kind == asAST_STMT_FOREACH )
		{
			const asCStmt* const initializer =
				statement->children.GetLength() > 0
					? context.GetStmt(statement->children[0]) : 0;
			const asASTDeclId iteratorDeclId = initializer != 0
				? FindFirstStmtDecl(context, initializer->id) : asASTDeclId();
			const asCDecl* const iteratorDecl = context.GetDecl(iteratorDeclId);
			static const char FOREACH_ITERATOR_PREFIX[] = "__foreach_iterator_";
			const bool sealedProtocol = iteratorDecl != 0
				&& (iteratorDecl->traits & asAST_TRAIT_GENERATED) != 0
				&& iteratorDecl->name.GetLength()
					>= sizeof(FOREACH_ITERATOR_PREFIX) - 1
				&& std::strncmp(iteratorDecl->name.AddressOf(),
					FOREACH_ITERATOR_PREFIX,
					sizeof(FOREACH_ITERATOR_PREFIX) - 1) == 0;
			if( sealedProtocol )
			{
				// A sealed foreach is no longer the syntax-shaped
				// range/value/body tuple. Sema has lowered it to the same
				// init / condition / body / increment / optional-cleanup phases
				// consumed by Canonical CodeGen. The generated iterator belongs
				// to the whole loop: flatten its synthetic initializer Block,
				// then resolve every later phase before leaving that scope.
				const asUINT restoreLength = bindings.GetLength();
				scopeOffsets.PushLast(restoreLength);
				if( initializer->kind == asAST_STMT_BLOCK )
				{
					ResolveLexicalStmtChildren(
						sema, function, *initializer, bindings, scopeOffsets);
				}
				else
				{
					ResolveLexicalStmt(sema, function, initializer->id,
						bindings, scopeOffsets);
				}
				ResolveLexicalExpr(sema, function, statement->expr, bindings);
				for( asUINT childIndex = 1;
					childIndex < statement->children.GetLength(); ++childIndex )
				{
					ResolveLexicalStmt(sema, function, statement->children[childIndex],
						bindings, scopeOffsets);
				}
				bindings.SetLength(restoreLength);
				scopeOffsets.PopLast();
				return;
			}

			// The range is evaluated outside the loop-variable scope. Only after that
			// value exists do the foreach declarations shadow outer names.
			ResolveLexicalExpr(sema, function, statement->expr, bindings);
			const asUINT restoreLength = bindings.GetLength();
			scopeOffsets.PushLast(restoreLength);
			ResolveLexicalStmtChildren(
				sema, function, *statement, bindings, scopeOffsets);
			bindings.SetLength(restoreLength);
			scopeOffsets.PopLast();
			return;
		}

		if( statement->kind == asAST_STMT_SWITCH )
		{
			ResolveLexicalExpr(sema, function, statement->expr, bindings);
			const asUINT restoreLength = bindings.GetLength();
			scopeOffsets.PushLast(restoreLength);
			ResolveLexicalStmtChildren(
				sema, function, *statement, bindings, scopeOffsets);
			bindings.SetLength(restoreLength);
			scopeOffsets.PopLast();
			return;
		}

		ResolveLexicalExpr(sema, function, statement->expr, bindings);
		ResolveLexicalStmtChildren(
			sema, function, *statement, bindings, scopeOffsets);
	}
}

void asCSema::ResolveLexicalScopes()
{
	if( context.IsPublishable() )
	{
		return;
	}
	for( asUINT declarationIndex = 1;
		declarationIndex <= context.GetDeclCount(); ++declarationIndex )
	{
		const asCDecl* const function = context.GetDecl(
			asASTDeclId(declarationIndex));
		if( function == 0 || !IsLexicalFunctionLike(function->kind)
			|| !function->body.IsValid() )
		{
			continue;
		}

		asCArray<asSLexicalBinding> bindings;
		asCArray<asUINT> scopeOffsets;
		scopeOffsets.PushLast(0);
		for( asUINT childIndex = 0;
			childIndex < function->children.GetLength(); ++childIndex )
		{
			const asCDecl* const child = context.GetDecl(
				function->children[childIndex]);
			if( child != 0 && child->kind == asAST_DECL_PARAM )
			{
				AddLexicalDeclaration(
					*this, *function, child->id, bindings, scopeOffsets);
			}
		}
		for( asUINT captureIndex = 0;
			captureIndex < function->captures.GetLength(); ++captureIndex )
		{
			const asCDecl* const capture = context.GetDecl(
				function->captures[captureIndex]);
			if( capture == 0 || capture->name.GetLength() == 0 )
			{
				continue;
			}
			asSLexicalBinding binding;
			binding.name = capture->name;
			binding.declaration = capture->id;
			bindings.PushLast(binding);
		}
		ResolveLexicalStmt(
			*this, *function, function->body,
			bindings, scopeOffsets, true);
	}
}

asASTStmtId asCSema::ActOnExpressionStmt(asASTDeclId owner, asASTExprId expr, const asCSourceRange& range)
{
	const asASTStmtId existing = FindExistingStmt(asAST_STMT_EXPR, range);
	if( existing.IsValid() )
	{
		context.SetStmtSafePointRole(
			existing, asAST_SAFEPOINT_STATEMENT);
		return existing;
	}
	const asASTStmtId statement = ActOnExprStmt(owner, expr, range);
	context.SetStmtSafePointRole(
		statement, asAST_SAFEPOINT_STATEMENT);
	return statement;
}

static asASTStmtId NearestControl(asCASTContext& context, const asCArray<asASTStmtId>& stack, bool loopsOnly)
{
	for( asUINT i = stack.GetLength(); i > 0; --i )
	{
		const asCStmt* stmt = context.GetStmt(stack[i - 1]);
		if( stmt == 0 )
		{
			continue;
		}
		if( stmt->kind == asAST_STMT_WHILE || stmt->kind == asAST_STMT_DO_WHILE || stmt->kind == asAST_STMT_FOR || stmt->kind == asAST_STMT_FOREACH )
		{
			return stmt->id;
		}
		if( !loopsOnly && stmt->kind == asAST_STMT_SWITCH )
		{
			return stmt->id;
		}
	}
	return asASTStmtId();
}

asASTStmtId asCSema::ActOnCaseStmt(asASTDeclId owner, asASTExprId value, const asCSourceRange& range)
{
	const asASTStmtId existing = FindExistingStmt(asAST_STMT_CASE, range);
	if( existing.IsValid() )
	{
		if( value.IsValid() )
		{
			context.SetStmtExpr(existing, value);
		}
		return existing;
	}
	return ActOnCase(owner, NearestControl(context, controlStack, false), value, range);
}

asASTStmtId asCSema::ActOnBreakStmt(asASTDeclId owner, const asCSourceRange& range)
{
	const asASTStmtId existing = FindExistingStmt(asAST_STMT_BREAK, range);
	if( existing.IsValid() )
	{
		context.SetStmtSafePointRole(existing, asAST_SAFEPOINT_TRANSFER);
		return existing;
	}
	return ActOnBreak(owner, NearestControl(context, controlStack, false), range);
}

asASTStmtId asCSema::ActOnContinueStmt(asASTDeclId owner, const asCSourceRange& range)
{
	const asASTStmtId existing = FindExistingStmt(asAST_STMT_CONTINUE, range);
	if( existing.IsValid() )
	{
		context.SetStmtSafePointRole(existing, asAST_SAFEPOINT_TRANSFER);
		return existing;
	}
	return ActOnContinue(owner, NearestControl(context, controlStack, true), range);
}

asASTStmtId asCSema::ActOnFallthroughStmt(asASTDeclId owner, const asCSourceRange& range)
{
	const asASTStmtId existing = FindExistingStmt(asAST_STMT_FALLTHROUGH, range);
	if( existing.IsValid() )
	{
		return existing;
	}
	return ActOnFallthrough(owner, range);
}

static asEASTStmtKind LeafStatementKindForAction(
	asELeafStatementActionKind kind)
{
	switch( kind )
	{
	case asLEAF_STATEMENT_EXPRESSION: return asAST_STMT_EXPR;
	case asLEAF_STATEMENT_RETURN: return asAST_STMT_RETURN;
	case asLEAF_STATEMENT_BREAK: return asAST_STMT_BREAK;
	case asLEAF_STATEMENT_CONTINUE: return asAST_STMT_CONTINUE;
	case asLEAF_STATEMENT_FALLTHROUGH: return asAST_STMT_FALLTHROUGH;
	default: return asAST_STMT_INVALID;
	}
}

asASTStmtId asCSema::ActOnLeafStatementAction(
	const asSLeafStatementAction& action,
	asCScriptCode* script)
{
	const asEASTStmtKind statementKind = LeafStatementKindForAction(action.kind);
	const asCDecl* const owner = context.GetDecl(action.owner);
	if( statementKind == asAST_STMT_INVALID || owner == 0
		|| !IsTypedLocalVariableOwner(owner)
		|| action.endOffset <= action.beginOffset
		|| !PrepareParsedSource(script) )
	{
		AddDiagnostic("leaf-statement-action-invalid");
		return asASTStmtId();
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("leaf-statement-range-invalid");
		return asASTStmtId();
	}
	const bool acceptsExpression = action.kind == asLEAF_STATEMENT_EXPRESSION
		|| action.kind == asLEAF_STATEMENT_RETURN;
	if( (!acceptsExpression && action.expression.IsValid())
		|| (action.expression.IsValid()
			&& context.GetExpr(action.expression) == 0) )
	{
		AddDiagnostic("leaf-statement-expression-invalid", range);
		return asASTStmtId();
	}

	const asASTStmtId existing = FindExactStatementAction(
		context, statementKind, action.owner, range);
	if( existing.IsValid() )
	{
		const asCStmt* const statement = context.GetStmt(existing);
		if( action.kind == asLEAF_STATEMENT_EXPRESSION
			&& statement != 0 && statement->expr != action.expression )
		{
			AddDiagnostic("leaf-statement-action-conflict", range);
			return asASTStmtId();
		}
		return existing;
	}

	switch( action.kind )
	{
	case asLEAF_STATEMENT_EXPRESSION:
		{
			const asASTStmtId statement =
				ActOnExprStmt(action.owner, action.expression, range);
			if( action.expression.IsValid() && !action.recovered
				&& ActOnFinishFullExpression(action.expression, statement)
					!= asAST_VERIFY_OK )
			{
				return asASTStmtId();
			}
			context.SetStmtSafePointRole(
				statement, asAST_SAFEPOINT_STATEMENT);
			return statement;
		}
	case asLEAF_STATEMENT_RETURN:
		{
			asASTExprId value = action.expression;
			if( owner->type.IsValid() )
			{
				const asASTExprId rewritten = TryRewriteObjectConvOperator(
					*this, context, engine, value, owner->type, range, false);
				if( rewritten.IsValid() )
				{
					value = rewritten;
				}
			}
			if( const asCExpr* expr = context.GetExpr(value) )
			{
				const asCType* type = context.GetType(expr->type.type);
				// A value-object return owns an explicit transfer cleanup. Handles
				// and references retain their own ownership protocol.
				if( type != 0 && type->kind == asAST_TYPE_VALUE_OBJECT
					&& !expr->type.IsHandle() )
				{
					value = ActOnCleanup(value, range);
					context.SetLiteral(value, "return");
				}
			}
			const bool functionReturn = owner->kind == asAST_DECL_FUNCTION
				|| owner->kind == asAST_DECL_METHOD;
			if( functionReturn && !owner->type.IsReference() )
			{
				value = DecayReturnedScalarReferenceToRValue(
					*this, context, value, range);
			}
			const asCExpr* const returnedExpr = context.GetExpr(value);
			const asCType* const returnType = context.GetType(owner->type.type);
			const asCType* const returnedType = returnedExpr != 0
				? context.GetType(returnedExpr->type.type) : 0;
			if( functionReturn
				&& IsNumericScalar(context, returnType)
				&& IsNumericScalar(context, returnedType)
				&& owner->type.type != returnedExpr->type.type )
			{
				value = ActOnConversion(value, owner->type, range);
			}
			const asASTStmtId statement = context.CreateStmt(
				asAST_STMT_RETURN, action.owner, range);
			context.SetStmtExpr(statement, value);
			context.SetStmtSafePointRole(statement, asAST_SAFEPOINT_RETURN);
			return statement;
		}
	case asLEAF_STATEMENT_BREAK:
		return ActOnBreak(
			action.owner, NearestControl(context, controlStack, false), range);
	case asLEAF_STATEMENT_CONTINUE:
		return ActOnContinue(
			action.owner, NearestControl(context, controlStack, true), range);
	case asLEAF_STATEMENT_FALLTHROUGH:
		return ActOnFallthrough(action.owner, range);
	default:
		break;
	}
	return asASTStmtId();
}

static bool ContainsStatementIdentity(
	const asCArray<asASTStmtId>& statements,
	asASTStmtId statement)
{
	for( asUINT i = 0; i < statements.GetLength(); ++i )
	{
		if( statements[i] == statement )
		{
			return true;
		}
	}
	return false;
}

static bool StatementSubtreeContains(
	asCASTContext& context,
	asASTStmtId root,
	asASTStmtId needle)
{
	if( !root.IsValid() || !needle.IsValid() )
	{
		return false;
	}
	asCArray<asASTStmtId> stack;
	stack.PushLast(root);
	asCArray<asBYTE> visited;
	visited.SetLength(context.GetStmtCount() + 1);
	for( asUINT i = 0; i < visited.GetLength(); ++i )
	{
		visited[i] = 0;
	}
	while( stack.GetLength() != 0 )
	{
		const asASTStmtId currentId = stack.PopLast();
		if( currentId == needle )
		{
			return true;
		}
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

enum asELexicalCleanupRoute
{
	asLEXICAL_CLEANUP_NONE = 0,
	asLEXICAL_CLEANUP_VALUE_DESTRUCTOR,
	asLEXICAL_CLEANUP_OWNED_REFERENCE_RELEASE,
};

struct asSLexicalOwnedLocal
{
	asASTDeclId declaration;
	asASTExprId activationPoint;
	asELexicalCleanupRoute cleanupRoute;
	asASTDeclId actionTarget;

	asSLexicalOwnedLocal()
		: cleanupRoute(asLEXICAL_CLEANUP_NONE)
	{
	}
};

static asASTDeclId FindLexicalValueDestructor(
	asCASTContext& context,
	asASTDeclId declaration)
{
	const asCDecl* const variable = context.GetDecl(declaration);
	const asCType* const type = variable != 0 && variable->type.IsValid()
		? ResolveStmtTypedefValueType(
			context, context.GetType(variable->type.type)) : 0;
	if( variable == 0 || variable->kind != asAST_DECL_VAR
		|| type == 0 || type->kind != asAST_TYPE_VALUE_OBJECT
		|| variable->type.IsReference() || variable->type.IsHandle() )
	{
		return asASTDeclId();
	}
	for( asUINT declIndex = 1; declIndex <= context.GetDeclCount(); ++declIndex )
	{
		const asCDecl* const destructor = context.GetDecl(asASTDeclId(declIndex));
		if( destructor == 0 || destructor->kind != asAST_DECL_DESTRUCTOR )
		{
			continue;
		}
		const asCDecl* const parent = context.GetDecl(destructor->parent);
		if( parent != 0
			&& parent->stableKey.Equals(type->stableKey) )
		{
			return destructor->id;
		}
	}
	return asASTDeclId();
}

static asELexicalCleanupRoute GetLexicalCleanupRoute(
	asCASTContext& context,
	asASTDeclId declaration)
{
	const asCDecl* const variable = context.GetDecl(declaration);
	if( variable == 0 || variable->kind != asAST_DECL_VAR
		|| !variable->type.IsValid() || variable->type.IsReference()
		|| variable->inits.GetLength() == 0 )
	{
		return asLEXICAL_CLEANUP_NONE;
	}
	const asCType* const type = ResolveStmtTypedefValueType(
		context, context.GetType(variable->type.type));
	if( type == 0 )
	{
		return asLEXICAL_CLEANUP_NONE;
	}
	// Reference objects in this fork may use an implicit-handle spelling. Their
	// Canonical qualifier mask is then zero, so IsHandle() is not an ownership
	// predicate. A true reference qualifier is the non-owning case; every
	// initialized ReferenceObject/FuncDef local otherwise owns one release.
	if( type->kind == asAST_TYPE_REFERENCE_OBJECT
		|| type->kind == asAST_TYPE_FUNCDEF )
	{
		return asLEXICAL_CLEANUP_OWNED_REFERENCE_RELEASE;
	}
	if( type->kind != asAST_TYPE_VALUE_OBJECT || variable->type.IsHandle() )
	{
		return asLEXICAL_CLEANUP_NONE;
	}
	return FindLexicalValueDestructor(context, declaration).IsValid()
		? asLEXICAL_CLEANUP_VALUE_DESTRUCTOR : asLEXICAL_CLEANUP_NONE;
}

static asASTExprId FindLexicalActivationPoint(
	asCASTContext& context,
	const asCArray<asASTStmtId>& blockChildren,
	asUINT declarationIndex,
	asASTDeclId declaration)
{
	for( asUINT childIndex = declarationIndex + 1;
		childIndex < blockChildren.GetLength(); ++childIndex )
	{
		const asCStmt* const statement = context.GetStmt(blockChildren[childIndex]);
		if( statement == 0 )
		{
			continue;
		}
		if( statement->kind == asAST_STMT_DECL )
		{
			break;
		}
		const asCExpr* const assignment = statement->kind == asAST_STMT_EXPR
			? context.GetExpr(statement->expr) : 0;
		const asCExpr* const target = assignment != 0
			&& assignment->kind == asAST_EXPR_ASSIGN
			&& assignment->children.GetLength() == 2
			? context.GetExpr(assignment->children[0]) : 0;
		if( target != 0 && target->kind == asAST_EXPR_DECL_REF
			&& target->resolvedDecl == declaration )
		{
			return assignment->id;
		}
	}
	return asASTExprId();
}

static asASTStmtId CreateLexicalCleanup(
	asCSema& sema,
	asASTDeclId owner,
	asASTDeclId declaration,
	const asCSourceRange& range)
{
	asCASTContext& context = sema.GetContext();
	const asCDecl* const variable = context.GetDecl(declaration);
	const asELexicalCleanupRoute route = GetLexicalCleanupRoute(
		context, declaration);
	if( variable == 0 || route == asLEXICAL_CLEANUP_NONE )
	{
		return asASTStmtId();
	}
	const asASTExprId target = sema.ActOnDeclRef(
		declaration, variable->type, range);
	if( route == asLEXICAL_CLEANUP_OWNED_REFERENCE_RELEASE )
	{
		const asASTExprId cleanup = context.CreateExpr(
			asAST_EXPR_CLEANUP, variable->type, asAST_VALUE_PRVALUE, range);
		context.SetLiteral(cleanup, "scope-release");
		context.AddExprChild(cleanup, target);
		return sema.ActOnExprStmt(owner, cleanup, range);
	}

	const asASTDeclId destructorId = FindLexicalValueDestructor(
		context, declaration);
	const asCDecl* const destructor = context.GetDecl(destructorId);
	if( destructor == 0 || destructor->kind != asAST_DECL_DESTRUCTOR )
	{
		return asASTStmtId();
	}
	// The lifetime protocol already owns the exact destructor decision. Keep the
	// temporary scope-exit statement only as an authenticated compatibility
	// encoding of that fact; do not repeat unqualified type lookup here.
	const asASTExprId cleanup = context.CreateExpr(
		asAST_EXPR_CLEANUP, variable->type, asAST_VALUE_PRVALUE, range);
	context.SetLiteral(cleanup, "scope-exit");
	context.SetResolvedDecl(cleanup, destructorId);
	context.AddExprChild(cleanup, target);
	return sema.ActOnExprStmt(owner, cleanup, range);
}

static void AppendLexicalCleanupPlansToTransfers(
	asCSema& sema,
	asASTStmtId block,
	const asCArray<asSLexicalOwnedLocal>& locals,
	const asCSourceRange& range)
{
	if( locals.GetLength() == 0 )
	{
		return;
	}
	asCASTContext& context = sema.GetContext();
	const asCStmt* const blockStmt = context.GetStmt(block);
	if( blockStmt == 0 )
	{
		return;
	}
	asCArray<asBYTE> visited;
	visited.SetLength(context.GetStmtCount() + 1);
	for( asUINT i = 0; i < visited.GetLength(); ++i )
	{
		visited[i] = 0;
	}

	// A DeclStmt introduces the name but does not make its storage live. A local
	// becomes cleanup-eligible only after its exact initializer assignment has
	// completed successfully. Keep the active set in direct-child order; nested
	// blocks have already attached their own cleanup and this pass appends the
	// active outer actions after those inner actions.
	asCArray<asASTStmtId> blockChildren;
	for( asUINT childIndex = 0;
		childIndex < blockStmt->children.GetLength(); ++childIndex )
	{
		blockChildren.PushLast(blockStmt->children[childIndex]);
	}
	asCArray<asASTDeclId> activeDeclarations;
	for( asUINT blockChildIndex = 0;
		blockChildIndex < blockChildren.GetLength(); ++blockChildIndex )
	{
		const asASTStmtId blockChildId = blockChildren[blockChildIndex];
		asCArray<asASTStmtId> stack;
		stack.PushLast(blockChildId);
		while( stack.GetLength() != 0 )
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
			asCArray<asASTStmtId> existingChildren;
			for( asUINT childIndex = 0;
				childIndex < current->children.GetLength(); ++childIndex )
			{
				existingChildren.PushLast(current->children[childIndex]);
			}
			const bool isReturn = current->kind == asAST_STMT_RETURN;
			const bool isTargetedTransfer =
				current->kind == asAST_STMT_BREAK
				|| current->kind == asAST_STMT_CONTINUE
				|| current->kind == asAST_STMT_FALLTHROUGH;
			const bool exitsBlock = isReturn
				|| (isTargetedTransfer
					&& (!current->target.IsValid()
						|| !StatementSubtreeContains(context, block, current->target)));
			if( exitsBlock )
			{
				for( asUINT declIndex = activeDeclarations.GetLength();
					declIndex > 0; --declIndex )
				{
					const asASTStmtId cleanup = CreateLexicalCleanup(
						sema, current->owner,
						activeDeclarations[declIndex - 1], range);
					if( cleanup.IsValid() )
					{
						context.AddStmtChild(currentId, cleanup);
					}
				}
			}
			for( asUINT childIndex = 0;
				childIndex < existingChildren.GetLength(); ++childIndex )
			{
				stack.PushLast(existingChildren[childIndex]);
			}
		}

		const asCStmt* const completedChild = context.GetStmt(blockChildId);
		for( asUINT localIndex = 0; completedChild != 0
			&& completedChild->kind == asAST_STMT_EXPR
			&& localIndex < locals.GetLength(); ++localIndex )
		{
			if( locals[localIndex].activationPoint == completedChild->expr )
			{
				activeDeclarations.PushLast(locals[localIndex].declaration);
				break;
			}
		}
	}
}

static bool AuthorLexicalLifetimeRecords(
	asCSema& sema,
	asASTStmtId block,
	const asCArray<asSLexicalOwnedLocal>& locals)
{
	asCASTContext& context = sema.GetContext();
	for( asUINT localIndex = 0; localIndex < locals.GetLength(); ++localIndex )
	{
		const asSLexicalOwnedLocal& local = locals[localIndex];
		if( local.cleanupRoute == asLEXICAL_CLEANUP_NONE )
		{
			continue;
		}
		asSASTLifetimeRecord record;
		record.subjectKind = asAST_LIFETIME_SUBJECT_LOCAL;
		record.subject = asSASTLifetimeNodeId::FromDecl(local.declaration);
		record.actionKind = local.cleanupRoute
			== asLEXICAL_CLEANUP_VALUE_DESTRUCTOR
			? asAST_LIFETIME_ACTION_DESTROY_VALUE
			: asAST_LIFETIME_ACTION_RELEASE_REFERENCE;
		record.actionTarget = asSASTLifetimeNodeId::FromDecl(
			local.cleanupRoute == asLEXICAL_CLEANUP_VALUE_DESTRUCTOR
				? local.actionTarget : local.declaration);
		record.activationPoint = asSASTLifetimeNodeId::FromExpr(
			local.activationPoint);
		record.semanticRegion = asSASTLifetimeNodeId::FromStmt(block);
		record.phase = asAST_LIFETIME_PHASE_LEXICAL_SCOPE;
		record.supportedExitMask = asAST_LIFETIME_EXIT_ALL;
		record.constructionStep = 0;
		record.order = context.GetLifetimeRecordCount();
		record.completeObjectCommit = false;
		if( asSemaAuthorLifetimeRecord(context, record) != asAST_VERIFY_OK )
		{
			sema.AddDiagnostic("local-lifetime-record-invalid");
			return false;
		}
	}
	return true;
}

asASTStmtId asCSema::ActOnBlockStatementAction(
	const asSBlockStatementAction& action,
	asCScriptCode* script)
{
	const asCDecl* const owner = context.GetDecl(action.owner);
	if( owner == 0 || !IsTypedLocalVariableOwner(owner)
		|| action.endOffset <= action.beginOffset
		|| !PrepareParsedSource(script) )
	{
		AddDiagnostic("block-statement-action-invalid");
		return asASTStmtId();
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("block-statement-range-invalid");
		return asASTStmtId();
	}
	const asASTStmtId existing = FindExactStatementAction(
		context, asAST_STMT_BLOCK, action.owner, range);
	if( existing.IsValid() )
	{
		return existing;
	}

	for( asUINT i = 0; i < action.children.GetLength(); ++i )
	{
		const asCStmt* const child = context.GetStmt(action.children[i]);
		if( child == 0 || child->owner != action.owner )
		{
			AddDiagnostic("block-statement-child-invalid", range);
			return asASTStmtId();
		}
	}
	for( asUINT i = 0; i < action.declarationSequences.GetLength(); ++i )
	{
		const asASTStmtId sequenceId = action.declarationSequences[i];
		const asCStmt* const sequence = context.GetStmt(sequenceId);
		if( !ContainsStatementIdentity(action.children, sequenceId)
			|| sequence == 0 || sequence->kind != asAST_STMT_BLOCK
			|| sequence->owner != action.owner )
		{
			AddDiagnostic("block-declaration-sequence-invalid", range);
			return asASTStmtId();
		}
		for( asUINT childIndex = 0;
			childIndex < sequence->children.GetLength(); ++childIndex )
		{
			const asCStmt* const declarationPart =
				context.GetStmt(sequence->children[childIndex]);
			if( declarationPart == 0
				|| declarationPart->owner != action.owner )
			{
				AddDiagnostic("block-declaration-sequence-child-invalid", range);
				return asASTStmtId();
			}
		}
	}

	asCArray<asASTStmtId> flattenedChildren;
	for( asUINT i = 0; i < action.children.GetLength(); ++i )
	{
		const asASTStmtId childId = action.children[i];
		if( ContainsStatementIdentity(action.declarationSequences, childId) )
		{
			const asCStmt* const sequence = context.GetStmt(childId);
			for( asUINT childIndex = 0;
				sequence != 0 && childIndex < sequence->children.GetLength();
				++childIndex )
			{
				flattenedChildren.PushLast(sequence->children[childIndex]);
			}
			continue;
		}
		flattenedChildren.PushLast(childId);
	}
	asCArray<asSLexicalOwnedLocal> lexicalOwnedLocals;
	for( asUINT childIndex = 0;
		childIndex < flattenedChildren.GetLength(); ++childIndex )
	{
		const asCStmt* const child = context.GetStmt(flattenedChildren[childIndex]);
		if( child == 0 || child->kind != asAST_STMT_DECL )
		{
			continue;
		}
		asSLexicalOwnedLocal local;
		local.declaration = child->decl;
		local.cleanupRoute = GetLexicalCleanupRoute(context, child->decl);
		if( local.cleanupRoute != asLEXICAL_CLEANUP_NONE )
		{
			local.activationPoint = FindLexicalActivationPoint(
				context, flattenedChildren, childIndex, child->decl);
			local.actionTarget = local.cleanupRoute
				== asLEXICAL_CLEANUP_VALUE_DESTRUCTOR
				? FindLexicalValueDestructor(context, child->decl)
				: asASTDeclId();
			if( !local.activationPoint.IsValid()
				|| (local.cleanupRoute == asLEXICAL_CLEANUP_VALUE_DESTRUCTOR
					&& !local.actionTarget.IsValid()) )
			{
				AddDiagnostic("local-lifetime-activation-invalid", range);
				return asASTStmtId();
			}
			lexicalOwnedLocals.PushLast(local);
		}
	}
	const asASTStmtId block = ActOnCompoundStmt(
		action.owner, flattenedChildren, range);
	if( block.IsValid() )
	{
		if( !AuthorLexicalLifetimeRecords(
			*this, block, lexicalOwnedLocals) )
		{
			return asASTStmtId();
		}
		AppendLexicalCleanupPlansToTransfers(
			*this, block, lexicalOwnedLocals, range);
		for( asUINT declIndex = lexicalOwnedLocals.GetLength();
			declIndex > 0; --declIndex )
		{
			const asASTStmtId cleanup = CreateLexicalCleanup(
				*this, action.owner,
				lexicalOwnedLocals[declIndex - 1].declaration, range);
			if( cleanup.IsValid() )
			{
				context.AddStmtChild(block, cleanup);
			}
		}
		for( asUINT i = 0; i < action.declarationSequences.GetLength(); ++i )
		{
			context.ClearStmtChildren(action.declarationSequences[i]);
		}
	}
	return block;
}

asASTStmtId asCSema::ActOnIfStatementAction(
	const asSIfStatementAction& action,
	asCScriptCode* script)
{
	const asCDecl* const owner = context.GetDecl(action.owner);
	if( owner == 0 || !IsTypedLocalVariableOwner(owner)
		|| !action.condition.IsValid()
		|| context.GetExpr(action.condition) == 0
		|| (!action.recovered && !action.thenStatement.IsValid())
		|| (action.elseStatement.IsValid()
			&& !action.thenStatement.IsValid())
		|| action.endOffset <= action.beginOffset
		|| !PrepareParsedSource(script) )
	{
		AddDiagnostic("if-statement-action-invalid");
		return asASTStmtId();
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("if-statement-range-invalid");
		return asASTStmtId();
	}
	for( const asASTStmtId childId : {
		action.thenStatement, action.elseStatement} )
	{
		if( !childId.IsValid() )
		{
			continue;
		}
		const asCStmt* const child = context.GetStmt(childId);
		if( child == 0 || child->owner != action.owner )
		{
			AddDiagnostic("if-statement-child-invalid", range);
			return asASTStmtId();
		}
	}
	return ActOnIfStmt(
		action.owner, action.condition,
		action.thenStatement, action.elseStatement, range);
}

static asEASTStmtKind ControlStatementKindForAction(
	asEControlStatementActionKind kind)
{
	switch( kind )
	{
	case asCONTROL_STATEMENT_WHILE: return asAST_STMT_WHILE;
	case asCONTROL_STATEMENT_DO_WHILE: return asAST_STMT_DO_WHILE;
	case asCONTROL_STATEMENT_FOR: return asAST_STMT_FOR;
	case asCONTROL_STATEMENT_FOREACH: return asAST_STMT_FOREACH;
	case asCONTROL_STATEMENT_SWITCH: return asAST_STMT_SWITCH;
	default: return asAST_STMT_INVALID;
	}
}

static asEASTStmtKind WhileStatementKindForAction(
	asEWhileStatementActionKind kind)
{
	switch( kind )
	{
	case asWHILE_STATEMENT_WHILE: return asAST_STMT_WHILE;
	case asWHILE_STATEMENT_DO_WHILE: return asAST_STMT_DO_WHILE;
	default: return asAST_STMT_INVALID;
	}
}

asASTStmtId asCSema::ActOnControlStatementHeaderAction(
	const asSControlStatementHeaderAction& action,
	asCScriptCode* script)
{
	const asEASTStmtKind statementKind =
		ControlStatementKindForAction(action.kind);
	const asCDecl* const owner = context.GetDecl(action.owner);
	if( statementKind == asAST_STMT_INVALID || owner == 0
		|| !IsTypedLocalVariableOwner(owner)
		|| action.endOffset <= action.beginOffset
		|| !PrepareParsedSource(script) )
	{
		AddDiagnostic("control-statement-header-action-invalid");
		return asASTStmtId();
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("control-statement-header-range-invalid");
		return asASTStmtId();
	}

	// Header identity is explicit and exists before the body. Do not discover it
	// again from a completed native range: the exact returned ID is the control
	// target used by every nested transfer action and by the finish action.
	const asASTStmtId control = context.CreateStmt(
		statementKind, action.owner, range);
	if( statementKind != asAST_STMT_SWITCH )
	{
		context.SetStmtSafePointRole(
			control, asAST_SAFEPOINT_LOOP_BACKEDGE);
	}
	PushControl(control);
	return control;
}

asASTStmtId asCSema::ActOnWhileStatementAction(
	const asSWhileStatementAction& action,
	asCScriptCode* script)
{
	const asEASTStmtKind statementKind = WhileStatementKindForAction(action.kind);
	const asCDecl* const owner = context.GetDecl(action.owner);
	const bool ownsTop = action.control.IsValid()
		&& controlStack.GetLength() != 0
		&& controlStack[controlStack.GetLength() - 1] == action.control;
	auto fail = [&](const char* diagnostic, const asCSourceRange& range)
	{
		AddDiagnostic(diagnostic, range);
		if( ownsTop )
		{
			PopControl();
		}
		return asASTStmtId();
	};

	if( statementKind == asAST_STMT_INVALID || owner == 0
		|| !IsTypedLocalVariableOwner(owner)
		|| action.endOffset <= action.beginOffset
		|| !PrepareParsedSource(script) )
	{
		return fail("while-statement-action-invalid", asCSourceRange());
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		return fail("while-statement-range-invalid", range);
	}
	if( (!action.recovered && (!action.condition.IsValid()
			|| !action.body.IsValid()))
		|| (action.condition.IsValid()
			&& context.GetExpr(action.condition) == 0) )
	{
		return fail("while-statement-components-invalid", range);
	}
	if( action.body.IsValid() )
	{
		const asCStmt* const body = context.GetStmt(action.body);
		if( body == 0 || body->owner != action.owner )
		{
			return fail("while-statement-body-invalid", range);
		}
	}

	asASTStmtId control = action.control;
	if( control.IsValid() )
	{
		const asCStmt* const existing = context.GetStmt(control);
		if( existing == 0 || existing->kind != statementKind
			|| existing->owner != action.owner )
		{
			return fail("while-statement-control-invalid", range);
		}
		if( !ownsTop && ControlNeedsFill(existing) )
		{
			return fail("while-statement-control-stack-invalid", range);
		}
	}
	else
	{
		if( !action.recovered )
		{
			return fail("while-statement-control-missing", range);
		}
		// Header syntax failed before a body could be entered. Preserve the exact
		// completed prefix for recovery without perturbing an enclosing control.
		control = context.CreateStmt(statementKind, action.owner, range);
	}

	asASTExprId cond = action.condition;
	if( cond.IsValid() )
	{
		// Typed while/do-while finish writes the condition directly. LEGACY
		// CompileCondition converts VALUE objects with `bool opImplConv()`.
		cond = RewriteValueToBoolViaOpImplConv(
			*this, context, engine, cond, range);
	}
	context.SetStmtRange(control, range);
	context.SetStmtExpr(control, cond);
	context.ClearStmtChildren(control);
	if( action.body.IsValid() )
	{
		context.AddStmtChild(control, action.body);
		context.SetLoopBodySafePointRole(
			control, asAST_SAFEPOINT_LOOP_ENTRY);
	}
	context.SetStmtSafePointRole(control, asAST_SAFEPOINT_LOOP_BACKEDGE);
	if( ownsTop )
	{
		PopControl();
	}
	return control;
}

asASTStmtId asCSema::ActOnForStatementAction(
	const asSForStatementAction& action,
	asCScriptCode* script)
{
	const asCDecl* const owner = context.GetDecl(action.owner);
	const bool ownsTop = action.control.IsValid()
		&& controlStack.GetLength() != 0
		&& controlStack[controlStack.GetLength() - 1] == action.control;
	auto fail = [&](const char* diagnostic, const asCSourceRange& range)
	{
		AddDiagnostic(diagnostic, range);
		if( ownsTop )
		{
			PopControl();
		}
		return asASTStmtId();
	};

	if( owner == 0 || !IsTypedLocalVariableOwner(owner)
		|| action.endOffset <= action.beginOffset
		|| !PrepareParsedSource(script) )
	{
		return fail("for-statement-action-invalid", asCSourceRange());
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		return fail("for-statement-range-invalid", range);
	}
	if( !action.recovered && !action.body.IsValid() )
	{
		return fail("for-statement-components-invalid", range);
	}
	for( asASTStmtId statement : {action.initStatement, action.body} )
	{
		if( !statement.IsValid() )
		{
			continue;
		}
		const asCStmt* const typed = context.GetStmt(statement);
		if( typed == 0 || typed->owner != action.owner )
		{
			return fail("for-statement-child-invalid", range);
		}
	}
	if( action.condition.IsValid() && context.GetExpr(action.condition) == 0 )
	{
		return fail("for-statement-condition-invalid", range);
	}
	for( asUINT i = 0; i < action.increments.GetLength(); ++i )
	{
		if( !action.increments[i].IsValid()
			|| context.GetExpr(action.increments[i]) == 0 )
		{
			return fail("for-statement-increment-invalid", range);
		}
	}

	asASTStmtId control = action.control;
	if( control.IsValid() )
	{
		const asCStmt* const existing = context.GetStmt(control);
		if( existing == 0 || existing->kind != asAST_STMT_FOR
			|| existing->owner != action.owner )
		{
			return fail("for-statement-control-invalid", range);
		}
		if( !ownsTop && ControlNeedsFill(existing) )
		{
			return fail("for-statement-control-stack-invalid", range);
		}
	}
	else
	{
		if( !action.recovered )
		{
			return fail("for-statement-control-missing", range);
		}
		control = context.CreateStmt(asAST_STMT_FOR, action.owner, range);
	}

	asASTExprId increment;
	asCSourceRange incrementRange;
	if( action.increments.GetLength() != 0 )
	{
		const asCExpr* const first = context.GetExpr(action.increments[0]);
		const asCExpr* const last = context.GetExpr(
			action.increments[action.increments.GetLength() - 1]);
		incrementRange = first != 0 && last != 0
			? asCSourceRange(first->range.begin, last->range.end)
			: range;
		increment = action.increments.GetLength() == 1
			? action.increments[0]
			: ActOnSequenceExpr(action.increments, incrementRange);
	}

	asASTExprId cond = action.condition;
	if( cond.IsValid() )
	{
		// Typed for finish writes the condition directly. LEGACY
		// CompileCondition converts VALUE objects with `bool opImplConv()`.
		cond = RewriteValueToBoolViaOpImplConv(
			*this, context, engine, cond, range);
	}
	context.SetStmtRange(control, range);
	context.SetStmtExpr(control, cond);
	context.ClearStmtChildren(control);
	if( action.initStatement.IsValid() )
	{
		context.AddStmtChild(control, action.initStatement);
	}
	if( action.body.IsValid() )
	{
		context.AddStmtChild(control, action.body);
		context.SetLoopBodySafePointRole(
			control, asAST_SAFEPOINT_LOOP_ENTRY);
	}
	if( increment.IsValid() )
	{
		context.AddStmtChild(
			control,
			ActOnExprStmt(
				action.owner, increment,
				incrementRange.IsValid() ? incrementRange : range));
	}
	context.SetStmtSafePointRole(control, asAST_SAFEPOINT_LOOP_BACKEDGE);
	if( ownsTop )
	{
		PopControl();
	}
	return control;
}

asASTStmtId asCSema::ActOnForeachStatementAction(
	const asSForeachStatementAction& action,
	asCScriptCode* script)
{
	const asCDecl* const owner = context.GetDecl(action.owner);
	const bool ownsTop = action.control.IsValid()
		&& controlStack.GetLength() != 0
		&& controlStack[controlStack.GetLength() - 1] == action.control;
	auto fail = [&](const char* diagnostic, const asCSourceRange& range)
	{
		AddDiagnostic(diagnostic, range);
		if( ownsTop )
		{
			PopControl();
		}
		return asASTStmtId();
	};

	if( owner == 0 || !IsTypedLocalVariableOwner(owner)
		|| action.endOffset <= action.beginOffset
		|| !PrepareParsedSource(script) )
	{
		return fail("foreach-statement-action-invalid", asCSourceRange());
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		return fail("foreach-statement-range-invalid", range);
	}
	if( !action.recovered && (!action.valueStatement.IsValid()
			|| !action.rangeExpression.IsValid()
			|| !action.body.IsValid()) )
	{
		return fail("foreach-statement-components-invalid", range);
	}
	for( asASTStmtId statement : {
		action.valueStatement, action.keyStatement, action.body} )
	{
		if( !statement.IsValid() )
		{
			continue;
		}
		const asCStmt* const typed = context.GetStmt(statement);
		if( typed == 0 || typed->owner != action.owner )
		{
			return fail("foreach-statement-child-invalid", range);
		}
	}
	if( action.rangeExpression.IsValid()
		&& context.GetExpr(action.rangeExpression) == 0 )
	{
		return fail("foreach-statement-range-expression-invalid", range);
	}

	if( action.control.IsValid() )
	{
		const asCStmt* const existing = context.GetStmt(action.control);
		if( existing == 0 || existing->kind != asAST_STMT_FOREACH
			|| existing->owner != action.owner )
		{
			return fail("foreach-statement-control-invalid", range);
		}
		if( !ownsTop && ControlNeedsFill(existing) )
		{
			return fail("foreach-statement-control-stack-invalid", range);
		}
		context.SetStmtRange(action.control, range);
	}
	else if( !action.recovered )
	{
		return fail("foreach-statement-control-missing", range);
	}

	const asASTStmtId loop = ActOnForeachStmt(
		action.owner,
		action.valueStatement,
		action.keyStatement,
		action.rangeExpression,
		action.body,
		range);
	if( action.control.IsValid() && loop != action.control )
	{
		// ActOnForeachStmt owns the successful control-stack pop. A mismatch is
		// already terminal here, so diagnose without attempting a second pop.
		AddDiagnostic("foreach-statement-control-mismatch", range);
		return asASTStmtId();
	}
	return loop;
}

asASTStmtId asCSema::ActOnCaseStatementAction(
	const asSCaseStatementAction& action,
	asCScriptCode* script)
{
	const asCDecl* const owner = context.GetDecl(action.owner);
	if( action.kind != asCASE_STATEMENT_CASE
		&& action.kind != asCASE_STATEMENT_DEFAULT )
	{
		AddDiagnostic("case-statement-kind-invalid");
		return asASTStmtId();
	}
	if( owner == 0 || !IsTypedLocalVariableOwner(owner)
		|| action.endOffset <= action.beginOffset
		|| !PrepareParsedSource(script) )
	{
		AddDiagnostic("case-statement-action-invalid");
		return asASTStmtId();
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("case-statement-range-invalid", range);
		return asASTStmtId();
	}
	const asCStmt* const switchStatement =
		context.GetStmt(action.switchStatement);
	if( switchStatement == 0
		|| switchStatement->kind != asAST_STMT_SWITCH
		|| switchStatement->owner != action.owner
		|| controlStack.GetLength() == 0
		|| controlStack[controlStack.GetLength() - 1]
			!= action.switchStatement )
	{
		AddDiagnostic("case-statement-switch-invalid", range);
		return asASTStmtId();
	}
	if( action.kind == asCASE_STATEMENT_DEFAULT )
	{
		if( action.value.IsValid() )
		{
			AddDiagnostic("default-statement-value-invalid", range);
			return asASTStmtId();
		}
	}
	else if( !action.value.IsValid() )
	{
		if( !action.recovered )
		{
			AddDiagnostic("case-statement-value-missing", range);
			return asASTStmtId();
		}
	}
	else if( context.GetExpr(action.value) == 0 )
	{
		AddDiagnostic("case-statement-value-invalid", range);
		return asASTStmtId();
	}
	for( asUINT i = 0; i < action.children.GetLength(); ++i )
	{
		const asCStmt* const child = context.GetStmt(action.children[i]);
		if( child == 0 || child->owner != action.owner )
		{
			AddDiagnostic("case-statement-child-invalid", range);
			return asASTStmtId();
		}
	}

	const asASTStmtId caseStatement = ActOnCase(
		action.owner, action.switchStatement, action.value, range);
	for( asUINT i = 0; i < action.children.GetLength(); ++i )
	{
		context.AddStmtChild(caseStatement, action.children[i]);
	}
	return caseStatement;
}

asASTStmtId asCSema::ActOnSwitchStatementAction(
	const asSSwitchStatementAction& action,
	asCScriptCode* script)
{
	const asCDecl* const owner = context.GetDecl(action.owner);
	const bool ownsTop = action.control.IsValid()
		&& controlStack.GetLength() != 0
		&& controlStack[controlStack.GetLength() - 1] == action.control;
	auto fail = [&](const char* diagnostic, const asCSourceRange& range)
	{
		AddDiagnostic(diagnostic, range);
		if( ownsTop )
		{
			PopControl();
		}
		return asASTStmtId();
	};

	if( owner == 0 || !IsTypedLocalVariableOwner(owner)
		|| action.endOffset <= action.beginOffset
		|| !PrepareParsedSource(script) )
	{
		return fail("switch-statement-action-invalid", asCSourceRange());
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		return fail("switch-statement-range-invalid", range);
	}
	if( !action.recovered && !action.selector.IsValid() )
	{
		return fail("switch-statement-selector-missing", range);
	}
	if( action.selector.IsValid() && context.GetExpr(action.selector) == 0 )
	{
		return fail("switch-statement-selector-invalid", range);
	}

	asASTStmtId control = action.control;
	if( control.IsValid() )
	{
		const asCStmt* const existing = context.GetStmt(control);
		if( existing == 0 || existing->kind != asAST_STMT_SWITCH
			|| existing->owner != action.owner )
		{
			return fail("switch-statement-control-invalid", range);
		}
		if( !ownsTop )
		{
			return fail("switch-statement-control-stack-invalid", range);
		}
	}
	else
	{
		if( !action.recovered )
		{
			return fail("switch-statement-control-missing", range);
		}
		control = context.CreateStmt(asAST_STMT_SWITCH, action.owner, range);
	}

	for( asUINT i = 0; i < action.cases.GetLength(); ++i )
	{
		const asCStmt* const caseStatement = context.GetStmt(action.cases[i]);
		if( caseStatement == 0 || caseStatement->kind != asAST_STMT_CASE
			|| caseStatement->owner != action.owner
			|| caseStatement->target != control )
		{
			return fail("switch-statement-case-invalid", range);
		}
		for( asUINT previous = 0; previous < i; ++previous )
		{
			if( action.cases[previous] == action.cases[i] )
			{
				return fail("switch-statement-case-duplicate", range);
			}
		}
	}

	asASTExprId selector = action.selector;
	asCSourceRange domainDiagnosticRange;
	const asESwitchDomainSealResult domainResult =
		NormalizeSwitchComparisonDomain(
			*this, context, selector, action.cases, range,
			domainDiagnosticRange);
	if( domainResult == asSWITCH_DOMAIN_SEAL_NON_INTEGRAL )
	{
		return fail("Switch expressions must be integral numbers",
			domainDiagnosticRange);
	}
	if( domainResult == asSWITCH_DOMAIN_SEAL_DIAGNOSED )
	{
		// NormalizeSwitchComparisonDomain already published the complete ordered
		// diagnostic set. Pop only the parser-control shell; do not add a synthetic
		// terminal message or commit any partially normalized AST facts.
		if( ownsTop )
		{
			PopControl();
		}
		return asASTStmtId();
	}

	context.SetStmtRange(control, range);
	context.SetStmtExpr(control, selector);
	context.ClearStmtChildren(control);
	for( asUINT i = 0; i < action.cases.GetLength(); ++i )
	{
		context.AddStmtChild(control, action.cases[i]);
	}
	WireFallthroughTargets(context, control);
	context.SetStmtSafePointRole(
		control,
		SwitchHasInvalidEnumValueEdge(context, context.GetStmt(control))
			? asAST_SAFEPOINT_SWITCH_INVALID_VALUE
			: asAST_SAFEPOINT_STATEMENT);
	if( ownsTop )
	{
		PopControl();
	}
	return control;
}

END_AS_NAMESPACE
