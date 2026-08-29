#include "as_sema.h"
#include "as_datatype.h"
#include "as_scriptcode.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "as_property.h"
#include "as_runtime_type_bridge.h"
#include "as_tokendef.h"
#include "as_typeinfo.h"
#include <cstring>
#include <cstdlib>

BEGIN_AS_NAMESPACE

void InternNativeMethods(asCSema& sema, asCScriptEngine* engine, asCASTContext& context, const asCQualType& recvType, const char* name);

asCSema::asCSema(
	asCScriptEngine* inEngine,
	asCASTContext& inContext,
	asCModule* inModule)
	: engine(inEngine)
	, buildModule(inModule)
	, context(inContext)
	, lastActedDecl()
	, parsedFile(0)
	, parsedLineOffset(0)
{
	(void)engine;
}

bool asCSema::PrepareParsedSource(asCScriptCode* script)
{
	const char* sectionName = (script && script->name.GetLength())
		? script->name.AddressOf()
		: "<memory>";
	if( script == 0 || script->code == 0 )
	{
		parsedFile = 0;
		parsedSection = sectionName;
		parsedSourceBytes.SetLength(0);
		parsedLineOffset = 0;
		AddDiagnostic("source-section-unavailable");
		return false;
	}

	const bool sameSection = parsedSection.Equals(sectionName);
	const bool sameLineOffset = parsedLineOffset == script->lineOffset;
	const bool sameByteCount = (size_t)parsedSourceBytes.GetLength() == script->codeLength;
	const bool sameBytes = sameByteCount
		&& (script->codeLength == 0
			|| std::memcmp(parsedSourceBytes.AddressOf(), script->code, script->codeLength) == 0);
	if( sameSection && sameLineOffset && sameBytes )
	{
		// The source bytes alone do not define diagnostic provenance. Ask the
		// snapshot-owned SourceManager to verify the complete provenance record
		// before reusing this FileID.
		parsedFile = context.RemapSourceSection(
			sectionName,
			asAST_SOURCE_AUTHORED,
			script->code,
			script->codeLength,
			script->lineOffset,
			script->sourceProvenanceRanges.GetLength()
				? script->sourceProvenanceRanges.AddressOf() : 0,
			script->sourceProvenanceRanges.GetLength());
		if( parsedFile == 0 )
		{
			AddDiagnostic("source-remap-mismatch");
			return false;
		}
		return true;
	}

	// Source IDs are snapshot-local identities. Do not keep using the previous
	// ID merely because a later parser callback reused its logical section name.
	parsedFile = 0;
	parsedSection = sectionName;
	parsedSourceBytes.Assign(script->code, (asUINT)script->codeLength);
	parsedLineOffset = script->lineOffset;
	parsedFile = context.RemapSourceSection(
		sectionName,
		asAST_SOURCE_AUTHORED,
		script->code,
		script->codeLength,
		script->lineOffset,
		script->sourceProvenanceRanges.GetLength()
			? script->sourceProvenanceRanges.AddressOf() : 0,
		script->sourceProvenanceRanges.GetLength());
	if( parsedFile == 0 )
	{
		AddDiagnostic("source-remap-mismatch");
		return false;
	}
	return true;
}

void asCSema::PushControl(asASTStmtId id)
{
	if( id.IsValid() )
	{
		controlStack.PushLast(id);
	}
}

void asCSema::PopControl()
{
	if( controlStack.GetLength() > 0 )
	{
		controlStack.SetLength(controlStack.GetLength() - 1);
	}
}

void asCSema::PushDeclContext(asASTDeclId id)
{
	if( id.IsValid() )
	{
		declContextStack.PushLast(id);
	}
}

void asCSema::PopDeclContext()
{
	if( declContextStack.GetLength() > 0 )
	{
		declContextStack.PopLast();
	}
}

asASTDeclId asCSema::CurrentDeclContext() const
{
	if( declContextStack.GetLength() == 0 )
	{
		return asASTDeclId();
	}
	return declContextStack[declContextStack.GetLength() - 1];
}

asASTStmtId asCSema::CurrentSwitchControl() const
{
	for( asUINT i = controlStack.GetLength(); i > 0; --i )
	{
		const asASTStmtId control = controlStack[i - 1];
		const asCStmt* const statement = context.GetStmt(control);
		if( statement != 0 && statement->kind == asAST_STMT_SWITCH )
		{
			return control;
		}
	}
	return asASTStmtId();
}

asASTDeclId asCSema::LastActedDecl() const
{
	return lastActedDecl;
}

static bool ParseActionIdentityEquals(
	const asSParseActionIdentity& lhs,
	const asSParseActionIdentity& rhs)
{
	return lhs.nodeKind == rhs.nodeKind
		&& lhs.offset == rhs.offset
		&& lhs.length == rhs.length
		&& lhs.section.Equals(rhs.section);
}

static bool ParseActionIdentityIsValid(
	const asSParseActionIdentity& identity)
{
	return identity.section.GetLength() != 0;
}

asSParseActionIdentity asCSema::MakeParseActionIdentity(
	asUINT nodeKind,
	asUINT offset,
	asUINT length) const
{
	asSParseActionIdentity identity;
	identity.section = parsedSection.GetLength()
		? parsedSection : asCString("<memory>");
	identity.nodeKind = nodeKind;
	identity.offset = offset;
	identity.length = length;
	return identity;
}

asASTDeclId asCSema::FindParsedDeclaration(
	const asSParseActionIdentity& identity) const
{
	if( !ParseActionIdentityIsValid(identity) )
	{
		return asASTDeclId();
	}
	for( asUINT i = 0; i < parsedDeclBindings.GetLength(); ++i )
	{
		if( ParseActionIdentityEquals(
			parsedDeclBindings[i].identity, identity) )
		{
			return context.GetDecl(parsedDeclBindings[i].declaration) != 0
				? parsedDeclBindings[i].declaration : asASTDeclId();
		}
	}
	return asASTDeclId();
}

bool asCSema::BindParsedDeclarationIdentity(
	const asSParseActionIdentity& identity,
	asASTDeclId declaration)
{
	if( !ParseActionIdentityIsValid(identity)
		|| context.GetDecl(declaration) == 0 )
	{
		AddDiagnostic("declaration-identity-bind-invalid");
		return false;
	}
	for( asUINT i = 0; i < parsedDeclBindings.GetLength(); ++i )
	{
		if( ParseActionIdentityEquals(
			parsedDeclBindings[i].identity, identity) )
		{
			if( parsedDeclBindings[i].declaration == declaration )
			{
				return true;
			}
			AddDiagnostic("declaration-identity-bind-conflict");
			return false;
		}
	}
	asSParsedDeclBinding binding;
	binding.identity = identity;
	binding.declaration = declaration;
	parsedDeclBindings.PushLast(binding);
	return true;
}

bool asCSema::BindParsedExpressionType(
	const asSParseActionIdentity& identity,
	const asCQualType& type)
{
	if( !ParseActionIdentityIsValid(identity) || !type.IsValid() )
	{
		AddDiagnostic("expression-target-type-bind-invalid");
		return false;
	}
	for( asUINT i = 0; i < parsedExprTypeBindings.GetLength(); ++i )
	{
		if( ParseActionIdentityEquals(
			parsedExprTypeBindings[i].identity, identity) )
		{
			if( parsedExprTypeBindings[i].type == type )
			{
				return true;
			}
			AddDiagnostic("expression-target-type-bind-conflict");
			return false;
		}
	}
	asSParsedExprTypeBinding binding;
	binding.identity = identity;
	binding.type = type;
	parsedExprTypeBindings.PushLast(binding);
	return true;
}

bool asCSema::FindParsedExpressionType(
	const asSParseActionIdentity& identity,
	asCQualType& outType) const
{
	outType = asCQualType();
	if( !ParseActionIdentityIsValid(identity) )
	{
		return false;
	}
	for( asUINT i = 0; i < parsedExprTypeBindings.GetLength(); ++i )
	{
		if( ParseActionIdentityEquals(
			parsedExprTypeBindings[i].identity, identity) )
		{
			outType = parsedExprTypeBindings[i].type;
			return outType.IsValid();
		}
	}
	return false;
}

bool asCSema::BindParsedExpressionIdentity(
	const asSParseActionIdentity& identity,
	asASTExprId expression)
{
	if( !ParseActionIdentityIsValid(identity)
		|| context.GetExpr(expression) == 0 )
	{
		AddDiagnostic("expression-identity-bind-invalid");
		return false;
	}
	for( asUINT i = 0; i < parsedExprBindings.GetLength(); ++i )
	{
		if( ParseActionIdentityEquals(
			parsedExprBindings[i].identity, identity) )
		{
			if( parsedExprBindings[i].expression == expression )
			{
				return true;
			}
			AddDiagnostic("expression-identity-bind-conflict");
			return false;
		}
	}
	asSParsedExprBinding binding;
	binding.identity = identity;
	binding.expression = expression;
	parsedExprBindings.PushLast(binding);
	return true;
}

asASTExprId asCSema::FindExactParsedExpressionIdentity(
	const asSParseActionIdentity& identity) const
{
	return FindParsedExpressionIdentity(identity);
}

asASTExprId asCSema::FindParsedExpressionIdentity(
	const asSParseActionIdentity& identity) const
{
	if( !ParseActionIdentityIsValid(identity) )
	{
		return asASTExprId();
	}
	for( asUINT i = 0; i < parsedExprBindings.GetLength(); ++i )
	{
		if( ParseActionIdentityEquals(
			parsedExprBindings[i].identity, identity) )
		{
			return context.GetExpr(parsedExprBindings[i].expression) != 0
				? parsedExprBindings[i].expression : asASTExprId();
		}
	}
	return asASTExprId();
}

void asCSema::NoteActedDecl(asASTDeclId id)
{
	lastActedDecl = id;
}

void asCSema::InsertSymbol(asASTDeclId id)
{
	const asCDecl* decl = context.GetDecl(id);
	if( decl == 0 || decl->name.GetLength() == 0 )
	{
		return;
	}
	asSSemaSymbol symbol;
	symbol.scope = decl->parent;
	symbol.name = decl->name;
	symbol.id = id;
	symbols.PushLast(symbol);
}

asUINT asCSema::LookupInScope(asASTDeclId scope, const char* name, asCArray<asASTDeclId>& out) const
{
	if( name == 0 || *name == 0 )
	{
		return 0;
	}
	const asUINT before = out.GetLength();
	const asCDecl* scopeDecl = context.GetDecl(scope);
	if( scopeDecl )
	{
		for( asUINT i = 0; i < scopeDecl->children.GetLength(); ++i )
		{
			const asCDecl* child = context.GetDecl(scopeDecl->children[i]);
			if( child == 0 || child->name.GetLength() == 0 )
			{
				continue;
			}
			if( child->name.Equals(name) )
			{
				out.PushLast(child->id);
			}
		}
	}
	return out.GetLength() - before;
}

asUINT asCSema::LookupCandidatesFrom(asASTDeclId start, const char* name, asCArray<asASTDeclId>& out) const
{
	out.SetLength(0);
	asASTDeclId current = start;
	if( !current.IsValid() )
	{
		current = context.GetTranslationUnit();
	}
	while( current.IsValid() )
	{
		if( LookupInScope(current, name, out) > 0 )
		{
			return out.GetLength();
		}
		const asCDecl* decl = context.GetDecl(current);
		if( decl == 0 )
		{
			break;
		}
		if( decl->kind == asAST_DECL_CLASS || decl->kind == asAST_DECL_INTERFACE )
		{
			// Ordinary lexical lookup stops at the first scope that owns the
			// requested name.  A class scope, however, includes its inherited
			// declaration graph.  Search that graph breadth-first so a declaration
			// in a nearer base hides a declaration with the same name in a more
			// distant base, while siblings at the same depth remain one overload
			// set and can be diagnosed as ambiguous by the caller.
			asCArray<asASTDeclId> frontier = decl->bases;
			asCArray<asASTDeclId> visited;
			while( frontier.GetLength() != 0 )
			{
				asCArray<asASTDeclId> next;
				const asUINT beforeDepth = out.GetLength();
				for( asUINT baseIndex = 0; baseIndex < frontier.GetLength(); ++baseIndex )
				{
					const asASTDeclId base = frontier[baseIndex];
					bool alreadyVisited = false;
					for( asUINT visitedIndex = 0; visitedIndex < visited.GetLength(); ++visitedIndex )
					{
						if( visited[visitedIndex] == base )
						{
							alreadyVisited = true;
							break;
						}
					}
					if( alreadyVisited )
					{
						continue;
					}
					visited.PushLast(base);

					LookupInScope(base, name, out);
					const asCDecl* const baseDecl = context.GetDecl(base);
					if( baseDecl == 0
						|| (baseDecl->kind != asAST_DECL_CLASS
							&& baseDecl->kind != asAST_DECL_INTERFACE) )
					{
						continue;
					}
					for( asUINT inheritedIndex = 0;
						inheritedIndex < baseDecl->bases.GetLength(); ++inheritedIndex )
					{
						next.PushLast(baseDecl->bases[inheritedIndex]);
					}
				}
				if( out.GetLength() != beforeDepth )
				{
					return out.GetLength();
				}
				frontier = next;
			}
		}
		current = decl->parent;
	}
	return 0;
}

asUINT asCSema::LookupCandidates(const char* name, asCArray<asASTDeclId>& out) const
{
	asASTDeclId start = CurrentDeclContext();
	if( !start.IsValid() )
	{
		start = context.GetTranslationUnit();
	}
	return LookupCandidatesFrom(start, name, out);
}

void asCSema::FinishDecl(asASTDeclId id)
{
	const asCDecl* decl = context.GetDecl(id);
	if( decl == 0 )
	{
		return;
	}
	const asCDecl* parent = context.GetDecl(decl->parent);
	asCString key;
	// Translation units are module containers, not lexical namespaces.
	const bool qualifyWithParent =
		parent != 0
		&& parent->stableKey.GetLength() != 0
		&& parent->kind != asAST_DECL_TRANSLATION_UNIT
		&& decl->kind != asAST_DECL_PARAM;
	asCString namePart = decl->name;
	if( decl->kind == asAST_DECL_DESTRUCTOR )
	{
		namePart = "~";
		namePart += decl->name;
	}
	if( qualifyWithParent )
	{
		key = parent->stableKey;
		key += "::";
		key += namePart;
	}
	else
	{
		key = namePart;
	}
	if( decl->kind == asAST_DECL_FUNCTION
		|| decl->kind == asAST_DECL_METHOD
		|| decl->kind == asAST_DECL_CONSTRUCTOR
		|| decl->kind == asAST_DECL_DESTRUCTOR
		|| decl->kind == asAST_DECL_MIXIN
		|| decl->kind == asAST_DECL_IMPORT )
	{
		key += "(";
		bool first = true;
		for( asUINT i = 0; i < decl->children.GetLength(); ++i )
		{
			const asCDecl* child = context.GetDecl(decl->children[i]);
			if( child == 0 || child->kind != asAST_DECL_PARAM )
			{
				continue;
			}
			if( !first )
			{
				key += ",";
			}
			first = false;
			if( child->type.IsConst() )
			{
				key += "const ";
			}
			if( const asCType* type = context.GetType(child->type.type) )
			{
				key += type->stableKey;
			}
			if( child->type.IsHandle() )
			{
				key += "@";
			}
			if( child->type.IsReference() )
			{
				key += "&";
			}
			if( (child->type.quals & asAST_QUAL_PARAM_DIR_MASK) == asAST_QUAL_INOUT )
			{
				key += "inout";
			}
			else if( child->type.quals & asAST_QUAL_OUT )
			{
				key += "out";
			}
			else if( child->type.quals & asAST_QUAL_IN )
			{
				key += "in";
			}
		}
		key += ")";
	}
	if( decl->traits & asAST_TRAIT_CONST_METHOD )
	{
		key += " const";
	}
	if( decl->traits & asAST_TRAIT_LAMBDA )
	{
		asCString loc;
		loc.Format("@%u", decl->range.begin.offset);
		key += loc;
	}
	context.SetStableKey(id, key.AddressOf());
	if( decl->kind == asAST_DECL_MIXIN && decl->origin.GetLength() == 0 )
	{
		for( asUINT i = 0; i < decl->children.GetLength(); ++i )
		{
			const asCDecl* const param = context.GetDecl(decl->children[i]);
			if( param == 0 || param->kind != asAST_DECL_PARAM )
			{
				continue;
			}
			const asCType* const named = context.GetType(param->type.type);
			if( named == 0 || named->kind == asAST_TYPE_PRIMITIVE
				|| named->kind == asAST_TYPE_VOID
				|| named->stableKey.GetLength() == 0 )
			{
				continue;
			}
			context.SetOrigin(id, named->stableKey.AddressOf());
			break;
		}
	}
}

void asCSema::RecordDependency(asASTDeclId owner, const char* key)
{
	const asCDecl* decl = context.GetDecl(owner);
	if( decl == 0 || key == 0 || *key == 0 )
	{
		return;
	}
	for( asUINT i = 0; i < decl->dependencies.GetLength(); ++i )
	{
		if( decl->dependencies[i].Equals(key) )
		{
			return;
		}
	}
	context.AddDependency(owner, key);
}

asASTDeclId asCSema::ActOnTranslationUnit(const char* moduleName)
{
	return context.CreateTranslationUnit(moduleName);
}

asASTDeclId asCSema::ActOnFunctionDecl(asASTDeclId parent, const char* name, const asCQualType& returnType, const asCSourceRange& range)
{
	const asASTDeclId id = context.CreateDecl(asAST_DECL_FUNCTION, parent, range, name);
	context.SetDeclType(id, returnType);
	FinishDecl(id);
	InsertSymbol(id);
	return id;
}

asASTDeclId asCSema::ActOnStartFunctionDecl(asASTDeclId parent, const char* name, const asCQualType& returnType, const asCSourceRange& range)
{
	// Parser/Sema declaration actions are incremental. Re-entering the start
	// action for the exact same authored declaration completes the existing
	// identity; overloads remain distinct because their source ranges differ.
	// Empty/generated ranges carry no such identity and must never collapse.
	if( range.IsValid() && name && *name )
	{
		const asCDecl* owner = context.GetDecl(parent);
		if( owner )
		{
			for( asUINT i = 0; i < owner->children.GetLength(); ++i )
			{
				const asCDecl* child = context.GetDecl(owner->children[i]);
				if( child
					&& child->kind == asAST_DECL_FUNCTION
					&& child->name.Equals(name)
					&& child->range == range )
				{
					context.SetDeclType(child->id, returnType);
					FinishDecl(child->id);
					NoteActedDecl(child->id);
					return child->id;
				}
			}
		}
	}
	// Overloads must not collapse by name. Distinct source identities reach here.
	const asASTDeclId id = ActOnFunctionDecl(parent, name, returnType, range);
	NoteActedDecl(id);
	return id;
}

asASTExprId asCSema::ActOnLambdaExpr(asASTDeclId owner, const asCQualType& returnType, const asCSourceRange& range)
{
	const asASTDeclId fn = ActOnFunctionDecl(owner, "<lambda>", returnType, range);
	context.AddDeclTrait(fn, asAST_TRAIT_LAMBDA);
	context.SetDeclName(fn, "<lambda>");
	FinishDecl(fn);
	return ActOnDeclRef(fn, returnType, range);
}

asASTDeclId asCSema::ActOnLambdaCapture(asASTDeclId lambda, asASTDeclId captured)
{
	const asCDecl* fn = context.GetDecl(lambda);
	if( fn == 0 || (fn->traits & asAST_TRAIT_LAMBDA) == 0 )
	{
		return asASTDeclId();
	}
	const asCDecl* cap = context.GetDecl(captured);
	if( cap == 0 || (cap->kind != asAST_DECL_VAR && cap->kind != asAST_DECL_PARAM) )
	{
		return asASTDeclId();
	}
	for( asUINT i = 0; i < fn->captures.GetLength(); ++i )
	{
		if( fn->captures[i] == captured )
		{
			return captured;
		}
	}
	if( context.AddCapture(lambda, captured) != asAST_VERIFY_OK )
	{
		return asASTDeclId();
	}
	return captured;
}

static asASTDeclId CreateTypedDecl(asCASTContext& context, asEASTDeclKind kind, asASTDeclId parent, const char* name, const asCQualType& type, const asCSourceRange& range)
{
	const asASTDeclId id = context.CreateDecl(kind, parent, range, name);
	context.SetDeclType(id, type);
	return id;
}

asASTDeclId asCSema::ActOnMethodDecl(asASTDeclId parent, const char* name, const asCQualType& returnType, const asCSourceRange& range)
{
	const asASTDeclId id = CreateTypedDecl(context, asAST_DECL_METHOD, parent, name, returnType, range);
	FinishDecl(id);
	InsertSymbol(id);
	return id;
}

asASTDeclId asCSema::ActOnStartMethodDecl(asASTDeclId parent, const char* name, const asCQualType& returnType, const asCSourceRange& range)
{
	const asASTDeclId id = ActOnMethodDecl(parent, name, returnType, range);
	NoteActedDecl(id);
	return id;
}

asASTDeclId asCSema::ActOnConstructorDecl(asASTDeclId parent, const char* name, const asCSourceRange& range)
{
	const asASTDeclId id = context.CreateDecl(asAST_DECL_CONSTRUCTOR, parent, range, name);
	// Lifecycle declarations are special function declarations, but they still
	// need a complete canonical function type. Consumers must not independently
	// reinterpret an invalid QualType as Runtime void.
	context.SetDeclType(id, context.InternPrimitive(ttVoid, 0));
	FinishDecl(id);
	InsertSymbol(id);
	return id;
}

asASTDeclId asCSema::ActOnStartConstructorDecl(asASTDeclId parent, const char* name, const asCSourceRange& range)
{
	const asASTDeclId id = ActOnConstructorDecl(parent, name, range);
	NoteActedDecl(id);
	return id;
}

asASTDeclId asCSema::ActOnDestructorDecl(asASTDeclId parent, const char* name, const asCSourceRange& range)
{
	const asASTDeclId id = context.CreateDecl(asAST_DECL_DESTRUCTOR, parent, range, name);
	context.SetDeclType(id, context.InternPrimitive(ttVoid, 0));
	FinishDecl(id);
	InsertSymbol(id);
	return id;
}

asASTDeclId asCSema::ActOnStartDestructorDecl(asASTDeclId parent, const char* name, const asCSourceRange& range)
{
	const asASTDeclId id = ActOnDestructorDecl(parent, name, range);
	NoteActedDecl(id);
	return id;
}

asASTDeclId asCSema::ActOnParamDecl(asASTDeclId parent, const char* name, const asCQualType& type, const asCSourceRange& range)
{
	if( name && *name )
	{
		if( const asCDecl* owner = context.GetDecl(parent) )
		{
			for( asUINT i = 0; i < owner->children.GetLength(); ++i )
			{
				const asCDecl* sibling = context.GetDecl(owner->children[i]);
				if( sibling && sibling->kind == asAST_DECL_PARAM && sibling->name.Equals(name) )
				{
					asCString msg;
					msg.Format("duplicate-param:%s", name);
					diagnostics.PushLast(msg);
					break;
				}
			}
		}
	}
	const asASTDeclId id = CreateTypedDecl(context, asAST_DECL_PARAM, parent, name, type, range);
	FinishDecl(id);
	FinishDecl(parent);
	InsertSymbol(id);
	return id;
}

asASTDeclId asCSema::ActOnStartParamDecl(asASTDeclId parent, const char* name, const asCQualType& type, const asCSourceRange& range)
{
	if( const asCDecl* owner = context.GetDecl(parent) )
	{
		const bool named = name && *name;
		asASTDeclId replay;
		for( asUINT i = 0; i < owner->children.GetLength(); ++i )
		{
			const asCDecl* sibling = context.GetDecl(owner->children[i]);
			if( sibling == 0 || sibling->kind != asAST_DECL_PARAM )
			{
				continue;
			}
			if( named )
			{
				if( !sibling->name.Equals(name) )
				{
					continue;
				}
				if( sibling->range == range )
				{
					replay = sibling->id;
					break;
				}
			}
			else if( sibling->name.GetLength() == 0
				&& range.begin.fileID != 0
				&& sibling->range == range )
			{
				return sibling->id;
			}
		}
		if( replay.IsValid() )
		{
			return replay;
		}
	}
	return ActOnParamDecl(parent, name, type, range);
}

asASTDeclId asCSema::ActOnStartEnumeratorDecl(asASTDeclId parent, const char* name, const asCSourceRange& range)
{
	if( name && *name )
	{
		if( const asCDecl* owner = context.GetDecl(parent) )
		{
			asASTDeclId replay;
			bool sawDistinctSameName = false;
			for( asUINT i = 0; i < owner->children.GetLength(); ++i )
			{
				const asCDecl* sibling = context.GetDecl(owner->children[i]);
				if( sibling == 0 || sibling->kind != asAST_DECL_VAR || !sibling->name.Equals(name) )
				{
					continue;
				}
				if( sibling->range == range )
				{
					replay = sibling->id;
					break;
				}
				sawDistinctSameName = true;
			}
			if( replay.IsValid() )
			{
				return replay;
			}
			if( sawDistinctSameName )
			{
				asCString msg;
				msg.Format("duplicate-enumerator:%s", name);
				diagnostics.PushLast(msg);
			}
		}
	}
	// Match Clang's EnumConstantDecl model: the constant is a Var-like value
	// declaration, but its semantic type is the owning enum, not a generic int.
	// The enum's runtime ABI carrier is a separate backend concern (currently
	// signed byte in this fork) and can then be selected from an explicit Enum
	// QualType instead of rediscovered from parent spelling.
	asCQualType enumeratorType;
	if( const asCDecl* const owner = context.GetDecl(parent) )
	{
		if( owner->type.IsValid() )
		{
			enumeratorType = asCQualType(
				owner->type.type, asAST_QUAL_CONST);
		}
		else
		{
			const char* const typeKey = owner->stableKey.GetLength() != 0
				? owner->stableKey.AddressOf()
				: owner->name.AddressOf();
			enumeratorType = context.InternNamedType(
				asAST_TYPE_ENUM, typeKey, asAST_QUAL_CONST);
		}
	}
	if( !enumeratorType.IsValid() )
	{
		enumeratorType = context.InternPrimitive(
			ttInt, asAST_QUAL_CONST);
	}
	const asASTDeclId enumerator = ActOnVarDecl(
		parent, name, enumeratorType, range);
	if( !enumerator.IsValid() )
	{
		return enumerator;
	}

	// An enum member is represented as a const VarDecl below its EnumDecl. Freeze
	// its implicit value immediately so every successfully parsed prefix already
	// has a self-contained semantic graph. An explicit initializer may replace
	// this provisional value through ActOnFinishEnumeratorDecl below.
	asINT64 value = 0;
	if( const asCDecl* owner = context.GetDecl(parent) )
	{
		for( asUINT i = owner->children.GetLength(); i > 0; --i )
		{
			const asASTDeclId siblingId = owner->children[i - 1];
			if( siblingId == enumerator )
			{
				continue;
			}
			const asCDecl* sibling = context.GetDecl(siblingId);
			if( sibling != 0 && sibling->kind == asAST_DECL_VAR && sibling->hasConstantValue )
			{
				value = static_cast<asINT64>(sibling->constantValue) + 1;
				break;
			}
		}
	}
	asCString text;
	text.Format("%lld", static_cast<long long>(value));
	context.SetDefaultArg(enumerator, text.AddressOf());
	context.SetDeclConstantValue(enumerator, static_cast<asQWORD>(value));
	return enumerator;
}

asASTDeclId asCSema::ActOnPropertyDecl(asASTDeclId parent, const char* name, const asCQualType& type, const asCSourceRange& range)
{
	const asASTDeclId id = CreateTypedDecl(context, asAST_DECL_PROPERTY, parent, name, type, range);
	FinishDecl(id);
	InsertSymbol(id);
	return id;
}

asASTDeclId asCSema::ActOnTypedefDecl(asASTDeclId parent, const char* name, const asCSourceRange& range)
{
	const asASTDeclId id = context.CreateDecl(asAST_DECL_TYPEDEF, parent, range, name);
	FinishDecl(id);
	InsertSymbol(id);
	return id;
}

asASTDeclId asCSema::ActOnMixinDecl(asASTDeclId parent, const char* name, const asCSourceRange& range)
{
	const asASTDeclId id = context.CreateDecl(asAST_DECL_MIXIN, parent, range, name);
	FinishDecl(id);
	InsertSymbol(id);
	return id;
}

asASTDeclId asCSema::ActOnStartMixinDecl(asASTDeclId parent, const char* name, const asCSourceRange& range)
{
	const asASTDeclId id = ActOnMixinDecl(parent, name, range);
	NoteActedDecl(id);
	return id;
}

asASTDeclId asCSema::ActOnInterfaceDecl(asASTDeclId parent, const char* name, const asCSourceRange& range)
{
	const asASTDeclId id = context.CreateDecl(asAST_DECL_INTERFACE, parent, range, name);
	FinishDecl(id);
	InsertSymbol(id);
	return id;
}

asASTDeclId asCSema::ActOnFuncDefDecl(asASTDeclId parent, const char* name, const asCSourceRange& range)
{
	const asASTDeclId id = context.CreateDecl(asAST_DECL_FUNCDEF, parent, range, name);
	FinishDecl(id);
	InsertSymbol(id);
	return id;
}

asASTExprId asCSema::ActOnIntegerLiteral(asQWORD value, const asCSourceRange& range)
{
	// Match AngelScript's decimal literal classification: values using bit 31
	// need a signed 64-bit type to avoid changing sign during assignment, while
	// only values using bit 63 become uint64. The former int-only placeholder
	// made 4294967295 become -1 before Canonical numeric conversion.
	eTokenType token = ttInt;
	if( value >> 31 )
	{
		token = (value >> 63) ? ttUInt64 : ttInt64;
	}
	const asCQualType literalType = context.InternPrimitive(token, 0);
	const asASTExprId id = context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, literalType, asAST_VALUE_PRVALUE, range);
	asCString text;
	text.Format("%llu", (unsigned long long)value);
	context.SetLiteralBits(id, value);
	context.SetLiteral(id, text.AddressOf());
	return id;
}

static bool EvalIntegerConst(asCASTContext& context, asASTExprId id, asINT64& out)
{
	const asCExpr* expr = context.GetExpr(id);
	if( expr == 0 )
	{
		return false;
	}
	if( expr->kind == asAST_EXPR_INTEGER_LITERAL )
	{
		out = (asINT64)expr->literalBits;
		return true;
	}
	if( expr->kind == asAST_EXPR_DECL_REF )
	{
		const asCDecl* const target = context.GetDecl(expr->resolvedDecl);
		if( target != 0 && target->hasConstantValue )
		{
			out = static_cast<asINT64>(target->constantValue);
			return true;
		}
		return false;
	}
	if( expr->kind == asAST_EXPR_UNARY && expr->children.GetLength() == 1 )
	{
		asINT64 inner = 0;
		if( !EvalIntegerConst(context, expr->children[0], inner) )
		{
			return false;
		}
		if( expr->literal.Equals("-") )
		{
			out = -inner;
			return true;
		}
		if( expr->literal.Equals("+") )
		{
			out = inner;
			return true;
		}
		return false;
	}
	if( expr->kind == asAST_EXPR_BINARY && expr->children.GetLength() == 2 )
	{
		asINT64 lhs = 0;
		asINT64 rhs = 0;
		if( !EvalIntegerConst(context, expr->children[0], lhs) || !EvalIntegerConst(context, expr->children[1], rhs) )
		{
			return false;
		}
		if( expr->literal.Equals("+") )
		{
			out = lhs + rhs;
			return true;
		}
		if( expr->literal.Equals("-") )
		{
			out = lhs - rhs;
			return true;
		}
		if( expr->literal.Equals("*") )
		{
			out = lhs * rhs;
			return true;
		}
		if( expr->literal.Equals("/") )
		{
			if( rhs == 0 )
			{
				return false;
			}
			out = lhs / rhs;
			return true;
		}
		return false;
	}
	if( expr->kind == asAST_EXPR_ASSIGN && expr->children.GetLength() >= 1 )
	{
		return EvalIntegerConst(context, expr->children[expr->children.GetLength() - 1], out);
	}
	if( expr->kind == asAST_EXPR_CONVERSION && expr->children.GetLength() == 1 )
	{
		return EvalIntegerConst(context, expr->children[0], out);
	}
	if( expr->kind == asAST_EXPR_SEQUENCE && expr->children.GetLength() )
	{
		return EvalIntegerConst(context, expr->children[expr->children.GetLength() - 1], out);
	}
	return false;
}

void asCSema::ActOnFinishEnumeratorDecl(asASTDeclId enumerator, asASTExprId initializer)
{
	const asCDecl* const decl = context.GetDecl(enumerator);
	const asCDecl* const owner = decl != 0 ? context.GetDecl(decl->parent) : 0;
	if( decl == 0 || decl->kind != asAST_DECL_VAR
		|| owner == 0 || owner->kind != asAST_DECL_ENUM )
	{
		AddDiagnostic("enumerator-owner-invalid");
		return;
	}
	if( !initializer.IsValid() )
	{
		AddDiagnostic("enumerator-init-not-constant");
		return;
	}

	asINT64 value = 0;
	if( !EvalIntegerConst(context, initializer, value) )
	{
		AddDiagnostic("enumerator-init-not-constant");
		return;
	}
	context.AddDeclInit(enumerator, initializer);
	asCString text;
	text.Format("%lld", static_cast<long long>(value));
	context.SetDefaultArg(enumerator, text.AddressOf());
	context.SetDeclConstantValue(enumerator, static_cast<asQWORD>(value));
}

// Parser resolves the source spelling `float` according to the engine policy
// before Sema sees the declaration: it is ttFloat32 or ttFloat64, rather than
// the spelling token ttFloat. Keep the legacy spelling tokens here too because
// direct Sema clients can still construct those primitive types explicitly.
static bool IsFloat32PrimitiveToken(eTokenType token)
{
	return token == ttFloat || token == ttFloat32;
}

static bool IsFloat64PrimitiveToken(eTokenType token)
{
	return token == ttDouble || token == ttFloat64;
}

static bool IsFloatingPrimitiveToken(eTokenType token)
{
	return IsFloat32PrimitiveToken(token) || IsFloat64PrimitiveToken(token);
}

// Global scalar initializers are persisted as the raw storage bits of their
// declared type. Keep the evaluation in Sema so CodeGen never has to recover
// a value by parsing presentation text. Integer values are accepted here as
// the source expression for a float/double declaration; the final cast and
// bit representation are chosen from the resolved declared global type below.
static bool EvalFloatingConst(asCASTContext& context, asASTExprId id, double& out)
{
	const asCExpr* const expr = context.GetExpr(id);
	if( expr == 0 )
	{
		return false;
	}
	if( expr->kind == asAST_EXPR_INTEGER_LITERAL )
	{
		out = (double)(asINT64)expr->literalBits;
		return true;
	}
	if( expr->kind == asAST_EXPR_FLOAT_LITERAL )
	{
		const asCType* const type = context.GetType(expr->type.type);
		if( type == 0 )
		{
			return false;
		}
		if( IsFloat32PrimitiveToken(type->primitiveToken) )
		{
			const asDWORD bits = (asDWORD)expr->literalBits;
			float value = 0.0f;
			std::memcpy(&value, &bits, sizeof(value));
			out = value;
			return true;
		}
		if( IsFloat64PrimitiveToken(type->primitiveToken) )
		{
			const asQWORD bits = expr->literalBits;
			double value = 0.0;
			std::memcpy(&value, &bits, sizeof(value));
			out = value;
			return true;
		}
		return false;
	}
	if( expr->kind == asAST_EXPR_UNARY && expr->children.GetLength() == 1 )
	{
		double inner = 0.0;
		if( !EvalFloatingConst(context, expr->children[0], inner) )
		{
			return false;
		}
		if( expr->literal.Equals("-") )
		{
			out = -inner;
			return true;
		}
		if( expr->literal.Equals("+") )
		{
			out = inner;
			return true;
		}
		return false;
	}
	if( expr->kind == asAST_EXPR_BINARY && expr->children.GetLength() == 2 )
	{
		double lhs = 0.0;
		double rhs = 0.0;
		if( !EvalFloatingConst(context, expr->children[0], lhs)
			|| !EvalFloatingConst(context, expr->children[1], rhs) )
		{
			return false;
		}
		if( expr->literal.Equals("+") )
		{
			out = lhs + rhs;
			return true;
		}
		if( expr->literal.Equals("-") )
		{
			out = lhs - rhs;
			return true;
		}
		if( expr->literal.Equals("*") )
		{
			out = lhs * rhs;
			return true;
		}
		if( expr->literal.Equals("/") )
		{
			if( rhs == 0.0 )
			{
				return false;
			}
			out = lhs / rhs;
			return true;
		}
		return false;
	}
	if( expr->kind == asAST_EXPR_ASSIGN && expr->children.GetLength() >= 1 )
	{
		return EvalFloatingConst(context, expr->children[expr->children.GetLength() - 1], out);
	}
	if( expr->kind == asAST_EXPR_CONVERSION && expr->children.GetLength() == 1 )
	{
		return EvalFloatingConst(context, expr->children[0], out);
	}
	if( expr->kind == asAST_EXPR_SEQUENCE && expr->children.GetLength() )
	{
		return EvalFloatingConst(context, expr->children[expr->children.GetLength() - 1], out);
	}
	return false;
}

void asCSema::ActOnGlobalVarInit(asASTDeclId var, asASTExprId init)
{
	if( !var.IsValid() )
	{
		return;
	}
	if( !init.IsValid() )
	{
		AddDiagnostic("global-init-not-constant");
		return;
	}
	const asCDecl* const decl = context.GetDecl(var);
	const asCType* const declType = decl != 0 ? context.GetType(decl->type.type) : 0;
	if( declType != 0 && IsFloatingPrimitiveToken(declType->primitiveToken) )
	{
		double value = 0.0;
		if( !EvalFloatingConst(context, init, value) )
		{
			AddDiagnostic("global-init-not-constant");
			return;
		}

		asQWORD bits = 0;
		asCString text;
		if( IsFloat32PrimitiveToken(declType->primitiveToken) )
		{
			const float stored = (float)value;
			asDWORD storedBits = 0;
			std::memcpy(&storedBits, &stored, sizeof(storedBits));
			bits = storedBits;
			text.Format("%.9g", (double)stored);
		}
		else
		{
			const double stored = value;
			std::memcpy(&bits, &stored, sizeof(bits));
			text.Format("%.17g", stored);
		}
		context.SetDefaultArg(var, text.AddressOf());
		context.SetDeclConstantValue(var, bits);
		return;
	}

	asINT64 value = 0;
	if( !EvalIntegerConst(context, init, value) )
	{
		AddDiagnostic("global-init-not-constant");
		return;
	}
	const bool isUnsigned = declType != 0
		&& (declType->primitiveToken == ttUInt
			|| declType->primitiveToken == ttUInt8
			|| declType->primitiveToken == ttUInt16
			|| declType->primitiveToken == ttUInt64);
	asCString text;
	if( isUnsigned )
	{
		text.Format("%llu", (unsigned long long)(asQWORD)value);
	}
	else
	{
		text.Format("%lld", (long long)value);
	}
	context.SetDefaultArg(var, text.AddressOf());
	context.SetDeclConstantValue(var, (asQWORD)value);
}

void asCSema::AddDiagnostic(const char* text)
{
	if( text && *text )
	{
		diagnostics.PushLast(text);
		diagnosticRanges.PushLast(asCSourceRange());
	}
}

void asCSema::AddDiagnostic(const char* text, const asCSourceRange& range)
{
	if( text && *text )
	{
		// Parser lookahead can visit the same semantic expression through more
		// than one grammar probe. Diagnostics are facts keyed by message and
		// source range, so publish that fact once instead of leaking parser probe
		// count into the user-visible compiler contract.
		for( asUINT i = 0; i < diagnostics.GetLength()
			&& i < diagnosticRanges.GetLength(); ++i )
		{
			const asCSourceRange& existing = diagnosticRanges[i];
			if( diagnostics[i] == text
				&& existing.begin.fileID == range.begin.fileID
				&& existing.begin.offset == range.begin.offset
				&& existing.end.fileID == range.end.fileID
				&& existing.end.offset == range.end.offset )
			{
				return;
			}
		}
		diagnostics.PushLast(text);
		diagnosticRanges.PushLast(range);
	}
}

static asASTExprId MakeExpr(asCASTContext& context, asEASTExprKind kind, const asCQualType& type, asEASTValueCategory category, const asCSourceRange& range, const char* literal = 0)
{
	const asASTExprId id = context.CreateExpr(kind, type, category, range);
	if( literal )
	{
		context.SetLiteral(id, literal);
	}
	return id;
}

asASTExprId asCSema::ActOnBoolLiteral(bool value, const asCSourceRange& range)
{
	const asASTExprId id = MakeExpr(context, asAST_EXPR_BOOL_LITERAL, context.InternPrimitive(ttBool, 0), asAST_VALUE_PRVALUE, range, value ? "true" : "false");
	context.SetLiteralBits(id, value ? 1 : 0);
	return id;
}

asASTExprId asCSema::ActOnFloatLiteral(const char* text, const asCSourceRange& range)
{
	const asASTExprId id = context.CreateExpr(
		asAST_EXPR_FLOAT_LITERAL,
		context.InternPrimitive(ttFloat32, 0),
		asAST_VALUE_PRVALUE,
		range);
	context.SetLiteral(id, text ? text : "");
	const float value = text ? std::strtof(text, 0) : 0.0f;
	asDWORD bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	context.SetLiteralBits(id, bits);
	return id;
}

asASTExprId asCSema::ActOnDoubleLiteral(const char* text, const asCSourceRange& range)
{
	const asASTExprId id = context.CreateExpr(
		asAST_EXPR_FLOAT_LITERAL,
		context.InternPrimitive(ttFloat64, 0),
		asAST_VALUE_PRVALUE,
		range);
	context.SetLiteral(id, text ? text : "");
	const double value = text ? std::strtod(text, 0) : 0.0;
	asQWORD bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	context.SetLiteralBits(id, bits);
	return id;
}

asASTExprId asCSema::ActOnStringLiteral(const char* text, const asCSourceRange& range)
{
	return ActOnStringLiteral(
		text ? text : "",
		text ? static_cast<asUINT>(std::strlen(text)) : 0,
		range);
}

asASTExprId asCSema::ActOnStringLiteral(
	const char* bytes,
	asUINT length,
	const asCSourceRange& range)
{
	asCQualType stringType;
	if( engine != 0 && engine->stringType.IsValid() )
	{
		asCRuntimeTypeBridge bridge(engine);
		stringType = bridge.FromDataType(context, engine->stringType);
	}
	if( !stringType.IsValid() )
	{
		// Construction-only Sema contexts may intentionally have no registered
		// string factory. Preserve the standalone spelling there, but never let
		// it override an Engine's authoritative string type.
		stringType = context.InternNamedType(asAST_TYPE_VALUE_OBJECT, "string", 0);
	}
	const asASTExprId id = context.CreateExpr(asAST_EXPR_STRING_LITERAL, stringType, asAST_VALUE_PRVALUE, range);
	context.SetLiteralBytes(id, bytes, length);
	// literalBits is kind-discriminated. For StringLiteral it exposes the exact
	// semantic byte length through the existing immutable/public V1 view.
	context.SetLiteralBits(id, length);
	return id;
}

asASTExprId asCSema::ActOnNullLiteral(const asCSourceRange& range)
{
	return context.CreateExpr(
		asAST_EXPR_NULL_LITERAL,
		context.InternPrimitive(ttVoid, asAST_QUAL_HANDLE),
		asAST_VALUE_PRVALUE,
		range);
}

asASTExprId asCSema::ActOnDeclRef(asASTDeclId target, const asCQualType& type, const asCSourceRange& range)
{
	const asASTExprId id = MakeExpr(context, asAST_EXPR_DECL_REF, type, asAST_VALUE_LVALUE, range);
	context.SetResolvedDecl(id, target);
	if( const asCDecl* decl = context.GetDecl(target) )
	{
		context.SetLiteral(id, decl->name.AddressOf());
	}
	return id;
}

static const char* const CANONICAL_NATIVE_GLOBAL_PROPERTY_ORIGIN =
	"canonical-native-global-property";

static void InternNativeGlobalProperties(
	asCSema& sema,
	asCScriptEngine* engine,
	asCASTContext& context,
	const char* name)
{
	if( engine == 0 || name == 0 || name[0] == 0 )
	{
		return;
	}
	if( !context.GetTranslationUnit().IsValid() )
	{
		return;
	}
	asCRuntimeTypeBridge bridge(engine);
	const asUINT count = engine->GetGlobalPropertyCount();
	for( asUINT i = 0; i < count; ++i )
	{
		const char* propName = 0;
		const char* nameSpaceName = 0;
		int typeId = 0;
		bool isConst = false;
		if( engine->GetGlobalPropertyByIndex(
				i, &propName, &nameSpaceName, &typeId, &isConst) < 0
			|| propName == 0 || !asCString(propName).Equals(name) )
		{
			continue;
		}
		(void)nameSpaceName;
		const asASTDeclId owner = context.GetTranslationUnit();
		asCArray<asASTDeclId> existing;
		sema.LookupCandidatesFrom(owner, name, existing);
		bool exists = false;
		for( asUINT h = 0; h < existing.GetLength(); ++h )
		{
			const asCDecl* const child = context.GetDecl(existing[h]);
			if( child && child->kind == asAST_DECL_VAR && child->name.Equals(name) )
			{
				exists = true;
				break;
			}
		}
		if( exists )
		{
			continue;
		}
		asCDataType dataType = engine->GetDataTypeFromTypeId(typeId);
		if( !dataType.IsValid() )
		{
			asCTypeInfo* const typeInfo = static_cast<asCTypeInfo*>(
				engine->GetTypeInfoById(typeId));
			if( typeInfo != 0 )
			{
				dataType = asCDataType::CreateType(typeInfo, false);
			}
		}
		if( isConst )
		{
			dataType.MakeReadOnly(true);
		}
		const asCQualType type = bridge.FromDataType(context, dataType);
		if( !type.IsValid() )
		{
			continue;
		}
		const asASTDeclId var = sema.ActOnVarDecl(owner, name, type, asCSourceRange());
		context.SetOrigin(var, CANONICAL_NATIVE_GLOBAL_PROPERTY_ORIGIN);
		InternNativeMethods(sema, engine, context, type, "opImplConv");
	}
}

asASTExprId asCSema::ActOnDeclRefExpr(asASTDeclId owner, const char* name, const asCSourceRange& range)
{
	asCArray<asASTDeclId> hits;
	LookupCandidatesFrom(owner, name, hits);
	asASTDeclId target;
	auto FindVarHit = [&](const asCArray<asASTDeclId>& candidates) -> asASTDeclId
	{
		for( asUINT i = 0; i < candidates.GetLength(); ++i )
		{
			const asCDecl* hit = context.GetDecl(candidates[i]);
			if( hit && hit->kind == asAST_DECL_VAR )
			{
				return candidates[i];
			}
		}
		return asASTDeclId();
	};
	target = FindVarHit(hits);
	if( !target.IsValid() && name && name[0] && engine != 0 )
	{
		// Host globals such as `__StaticType_<Name>` are not script declarations.
		// Intern the requested name first so an explicit native global is visible
		// without going through the type-identifier rewrite.
		InternNativeGlobalProperties(*this, engine, context, name);
		hits.SetLength(0);
		LookupCandidatesFrom(owner, name, hits);
		target = FindVarHit(hits);
	}
	if( !target.IsValid() && name && name[0] && engine != 0 )
	{
		// Legacy CompileVariableAccess rewrites a type identifier used as a
		// value to the host `__StaticType_<Name>` global (UClass / TSubclassOf).
		asCString staticName = "__StaticType_";
		staticName += name;
		InternNativeGlobalProperties(*this, engine, context, staticName.AddressOf());
		asCArray<asASTDeclId> staticHits;
		LookupCandidatesFrom(owner, staticName.AddressOf(), staticHits);
		target = FindVarHit(staticHits);
	}
	if( !target.IsValid() && hits.GetLength() )
	{
		target = hits[0];
	}
	if( !target.IsValid() )
	{
		const char* const identifier = name && name[0] ? name : "<identifier>";
		asCString msg;
		// Keep a stable category/name prefix for AST tooling while retaining the
		// legacy user-facing explanation for diagnostics surfaced by the builder.
		msg.Format("unresolved-identifier:%s: '%s' is not declared", identifier, identifier);
		AddDiagnostic(msg.AddressOf(), range);
		if( range.begin.fileID == 0 )
		{
			const asCQualType errorType = context.InternNamedType(asAST_TYPE_ERROR, "<unresolved>", 0);
			const asASTExprId ref = ActOnDeclRef(asASTDeclId(), errorType, range);
			if( name && name[0] )
			{
				context.SetLiteral(ref, name);
			}
			return ref;
		}
		asCQualType type = context.InternPrimitive(ttInt, 0);
		const asASTExprId ref = ActOnDeclRef(asASTDeclId(), type, range);
		if( name && name[0] )
		{
			context.SetLiteral(ref, name);
		}
		return ref;
	}
	asCQualType type = context.InternPrimitive(ttInt, 0);
	if( const asCDecl* decl = context.GetDecl(target) )
	{
		if( decl->type.IsValid() )
		{
			type = decl->type;
		}
	}
	const asASTExprId ref = ActOnDeclRef(target, type, range);
	if( name && name[0] )
	{
		context.SetLiteral(ref, name);
	}
	return ref;
}

asASTExprId asCSema::ActOnUnary(const char* op, asASTExprId inner, const asCQualType& type, const asCSourceRange& range)
{
	const asASTExprId id = MakeExpr(context, asAST_EXPR_UNARY, type, asAST_VALUE_PRVALUE, range, op);
	context.AddExprChild(id, inner);
	return id;
}

asASTExprId asCSema::ActOnBinary(const char* op, asASTExprId lhs, asASTExprId rhs, const asCQualType& type, const asCSourceRange& range)
{
	const asASTExprId id = MakeExpr(context, asAST_EXPR_BINARY, type, asAST_VALUE_PRVALUE, range, op);
	context.AddExprChild(id, lhs);
	context.AddExprChild(id, rhs);
	return id;
}

asASTExprId asCSema::ActOnLogical(const char* op, asASTExprId lhs, asASTExprId rhs, const asCSourceRange& range)
{
	const asASTExprId id = MakeExpr(context, asAST_EXPR_LOGICAL, context.InternPrimitive(ttBool, 0), asAST_VALUE_PRVALUE, range, op);
	context.AddExprChild(id, lhs);
	context.AddExprChild(id, rhs);
	return id;
}

asASTExprId asCSema::ActOnConditional(asASTExprId cond, asASTExprId thenExpr, asASTExprId elseExpr, const asCQualType& type, const asCSourceRange& range)
{
	const asASTExprId id = MakeExpr(context, asAST_EXPR_CONDITIONAL, type, asAST_VALUE_PRVALUE, range, "?:");
	context.AddExprChild(id, cond);
	context.AddExprChild(id, thenExpr);
	context.AddExprChild(id, elseExpr);
	return id;
}

asASTExprId asCSema::ActOnAssign(asASTExprId lhs, asASTExprId rhs, const asCQualType& type, const asCSourceRange& range)
{
	asCQualType assignType = type;
	if( const asCExpr* lhsExpr = context.GetExpr(lhs) )
	{
		if( lhsExpr->type.IsValid() )
		{
			assignType = lhsExpr->type;
		}
	}
	if( const asCExpr* rhsExpr = context.GetExpr(rhs) )
	{
		if( rhsExpr->kind == asAST_EXPR_CONSTRUCT
			&& rhsExpr->literal.Equals("list-pattern")
			&& assignType.IsValid() )
		{
			context.SetExprType(rhs, assignType);
			const asASTDeclId listFactory = InternNativeListFactoryForType(assignType);
			if( listFactory.IsValid() )
			{
				context.SetResolvedDecl(rhs, listFactory);
			}
		}
		else if( assignType.IsValid() && rhsExpr->type.IsValid() && assignType.type != rhsExpr->type.type )
		{
			rhs = ActOnConversion(rhs, assignType, range);
		}
	}
	const asASTExprId id = MakeExpr(context, asAST_EXPR_ASSIGN, assignType.IsValid() ? assignType : type, asAST_VALUE_LVALUE, range, "=");
	context.AddExprChild(id, lhs);
	if( rhs.IsValid() )
	{
		context.AddExprChild(id, rhs);
	}
	return id;
}

asEASTCallDispatch asCSema::ClassifyCallDispatch(asASTDeclId callee) const
{
	const asCDecl* const declaration = context.GetDecl(callee);
	if( declaration == 0 )
	{
		return asAST_CALL_DISPATCH_NONE;
	}
	if( declaration->kind == asAST_DECL_PARAM
		|| declaration->kind == asAST_DECL_VAR )
	{
		return asAST_CALL_DISPATCH_INDIRECT;
	}
	if( declaration->kind == asAST_DECL_METHOD )
	{
		const asCDecl* const owner = context.GetDecl(declaration->parent);
		if( owner && (owner->traits & asAST_TRAIT_VALUE) != 0 )
		{
			// Value objects have no runtime virtual table. Seal their member calls
			// as direct even though they share MethodDecl with reference classes.
			return asAST_CALL_DISPATCH_DIRECT;
		}
		// An ordinary member call remains virtual-eligible even when lookup has
		// already selected a base declaration. Parser-owned qualification may
		// replace this with DIRECT before sealing.
		return asAST_CALL_DISPATCH_VIRTUAL;
	}
	return asAST_CALL_DISPATCH_DIRECT;
}

asASTExprId asCSema::ActOnCall(asASTDeclId callee, const asCArray<asASTExprId>& args, const asCQualType& type, const asCSourceRange& range)
{
	asCString literal = "call:reverse-formal";
	if( const asCDecl* decl = context.GetDecl(callee) )
	{
		literal = decl->name;
		literal += ":reverse-formal";
	}
	// The callee declaration does not always carry the call result type. A
	// function declaration currently does, but a callable variable/parameter
	// carries its funcdef handle type instead. The expression-building path has
	// already resolved the signature and passes the authoritative result type.
	const asASTExprId id = MakeExpr(context, asAST_EXPR_CALL, type, asAST_VALUE_PRVALUE, range, literal.AddressOf());
	context.SetResolvedDecl(id, callee);
	context.SetLiteral(id, literal.AddressOf());
	context.SetExprSafePointRole(id, asAST_SAFEPOINT_CALL);
	context.SetExprCallDispatch(id, ClassifyCallDispatch(callee));
	for( asUINT i = args.GetLength(); i > 0; --i )
	{
		context.AddExprChild(id, args[i - 1]);
	}
	return id;
}

static asETypeModifiers CanonicalLambdaParameterPassing(asDWORD qualifiers)
{
	if( (qualifiers & asAST_QUAL_PARAM_DIR_MASK) == asAST_QUAL_INOUT )
	{
		return asTM_INOUTREF;
	}
	if( qualifiers & asAST_QUAL_OUT )
	{
		return asTM_OUTREF;
	}
	if( qualifiers & asAST_QUAL_IN )
	{
		return asTM_INREF;
	}
	if( qualifiers & asAST_QUAL_REFERENCE )
	{
		return asTM_INOUTREF;
	}
	return asTM_NONE;
}

void asCSema::ContextualizeLambdaToFuncdef(
	asASTExprId inner,
	const asCQualType& destination,
	const asCSourceRange& range)
{
	const asCType* const targetType = context.GetType(destination.type);
	const asCExpr* const innerExpression = context.GetExpr(inner);
	if( targetType == 0 || targetType->kind != asAST_TYPE_FUNCDEF
		|| targetType->stableKey.GetLength() == 0
		|| innerExpression == 0
		|| innerExpression->kind != asAST_EXPR_DECL_REF
		|| !innerExpression->resolvedDecl.IsValid() )
	{
		return;
	}

	const asASTDeclId lambdaId = innerExpression->resolvedDecl;
	const asCDecl* const lambda = context.GetDecl(lambdaId);
	if( lambda == 0 || lambda->kind != asAST_DECL_FUNCTION
		|| (lambda->traits & asAST_TRAIT_LAMBDA) == 0 )
	{
		return;
	}

	asCArray<asCQualType> authoredParameters;
	for( asUINT i = 0; i < lambda->children.GetLength(); ++i )
	{
		const asCDecl* const child = context.GetDecl(lambda->children[i]);
		if( child != 0 && child->kind == asAST_DECL_PARAM )
		{
			authoredParameters.PushLast(child->type);
		}
	}

	asSLambdaSyntaxFact* syntax = 0;
	for( asUINT i = 0; i < lambdaSyntaxFacts.GetLength(); ++i )
	{
		if( lambdaSyntaxFacts[i].lambda == lambdaId )
		{
			syntax = &lambdaSyntaxFacts[i];
			break;
		}
	}
	if( syntax == 0 )
	{
		// Direct Sema construction has no Parser action. Every authored ParamDecl
		// is therefore explicit by construction; retain the same conflict guard
		// and idempotence as the Parser path without publishing transient state.
		asSLambdaSyntaxFact fact;
		fact.lambda = lambdaId;
		fact.parameterCount = authoredParameters.GetLength();
		fact.explicitlyTypedParameterCount = authoredParameters.GetLength();
		lambdaSyntaxFacts.PushLast(fact);
		syntax = &lambdaSyntaxFacts[lambdaSyntaxFacts.GetLength() - 1];
	}

	if( syntax->explicitlyTypedParameterCount < syntax->parameterCount )
	{
		// Untyped formals require deferred body re-typing. Do not misdiagnose
		// their absent ParamDecls as an explicit signature mismatch in this gate.
		return;
	}
	if( syntax->boundFuncdefKey.GetLength() != 0
		&& !syntax->boundFuncdefKey.Equals(targetType->stableKey) )
	{
		AddDiagnostic("lambda-funcdef-context-conflict", range);
		return;
	}

	asCQualType expectedReturn;
	asCArray<asCQualType> expectedParameters;
	const asCScriptFunction* runtimeFuncdef = 0;
	asASTDeclId canonicalFuncdef;
	for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
	{
		const asCDecl* const declaration = context.GetDecl(asASTDeclId(i));
		if( declaration == 0 || declaration->kind != asAST_DECL_FUNCDEF
			|| !declaration->stableKey.Equals(targetType->stableKey) )
		{
			continue;
		}
		if( canonicalFuncdef.IsValid() )
		{
			AddDiagnostic("lambda-funcdef-target-ambiguous", range);
			return;
		}
		canonicalFuncdef = declaration->id;
		expectedReturn = declaration->type;
		for( asUINT childIndex = 0;
			childIndex < declaration->children.GetLength();
			++childIndex )
		{
			const asCDecl* const child = context.GetDecl(
				declaration->children[childIndex]);
			if( child != 0 && child->kind == asAST_DECL_PARAM )
			{
				expectedParameters.PushLast(child->type);
			}
		}
	}

	if( !canonicalFuncdef.IsValid() )
	{
		if( engine == 0 )
		{
			AddDiagnostic("lambda-funcdef-target-unresolved", range);
			return;
		}
		asCRuntimeTypeBridge bridge(engine);
		const asCDataType runtimeType = bridge.Resolve(context, destination);
		asCFuncdefType* const funcdefType = runtimeType.IsFuncdef()
			? CastToFuncdefType(runtimeType.GetTypeInfo()) : 0;
		const asCScriptFunction* const function =
			funcdefType != 0 ? funcdefType->funcdef : 0;
		if( function == 0 )
		{
			AddDiagnostic("lambda-funcdef-target-unresolved", range);
			return;
		}
		runtimeFuncdef = function;
		expectedReturn = bridge.FromDataType(context, function->returnType);
	}

	if( !expectedReturn.IsValid() )
	{
		AddDiagnostic("lambda-funcdef-return-type-invalid", range);
		return;
	}
	const asUINT expectedParameterCount = runtimeFuncdef != 0
		? runtimeFuncdef->parameterTypes.GetLength()
		: expectedParameters.GetLength();
	if( syntax->parameterCount != expectedParameterCount
		|| authoredParameters.GetLength() != expectedParameterCount )
	{
		AddDiagnostic("lambda-funcdef-parameter-count-mismatch", range);
		return;
	}
	if( runtimeFuncdef == 0 )
	{
		for( asUINT parameterIndex = 0;
			parameterIndex < expectedParameters.GetLength();
			++parameterIndex )
		{
			if( !expectedParameters[parameterIndex].IsValid()
				|| authoredParameters[parameterIndex]
					!= expectedParameters[parameterIndex] )
			{
				AddDiagnostic("lambda-funcdef-parameter-type-mismatch", range);
				return;
			}
		}
	}
	else
	{
		// A host funcdef stores its live Runtime ABI, not necessarily its authored
		// surface. Compare after the same deterministic lowering used by Runtime
		// declaration binding, but retain the source-level lambda ParamDecl types.
		asCRuntimeTypeBridge bridge(engine);
		for( asUINT parameterIndex = 0;
			parameterIndex < runtimeFuncdef->parameterTypes.GetLength();
			++parameterIndex )
		{
			asCDataType lowered = bridge.Resolve(
				context, authoredParameters[parameterIndex]);
			asETypeModifiers passing = CanonicalLambdaParameterPassing(
				authoredParameters[parameterIndex].quals);
			if( lowered.IsValid() )
			{
				asCRuntimeTypeBridge::NormalizeScriptParameterABI(
					lowered, passing);
			}
			const asETypeModifiers expectedPassing =
				parameterIndex < runtimeFuncdef->inOutFlags.GetLength()
					? runtimeFuncdef->inOutFlags[parameterIndex]
					: asTM_NONE;
			if( !lowered.IsValid()
				|| lowered != runtimeFuncdef->parameterTypes[parameterIndex]
				|| passing != expectedPassing )
			{
				AddDiagnostic("lambda-funcdef-parameter-type-mismatch", range);
				return;
			}
		}
	}

	context.SetDeclType(lambdaId, expectedReturn);
	context.SetExprType(inner, expectedReturn);
	FinishDecl(lambdaId);
	RecordDependency(lambdaId, targetType->stableKey.AddressOf());
	syntax->boundFuncdefKey = targetType->stableKey;
}

asASTExprId asCSema::ActOnConversion(asASTExprId inner, const asCQualType& dest, const asCSourceRange& range)
{
	ContextualizeLambdaToFuncdef(inner, dest, range);
	const asASTExprId id = MakeExpr(context, asAST_EXPR_CONVERSION, dest, asAST_VALUE_PRVALUE, range, "conv");
	context.AddExprChild(id, inner);
	return id;
}

asASTExprId asCSema::ActOnSequence(const asCArray<asASTExprId>& parts, const asCQualType& type, const asCSourceRange& range)
{
	const asASTExprId id = MakeExpr(context, asAST_EXPR_SEQUENCE, type, asAST_VALUE_PRVALUE, range, "seq");
	for( asUINT i = 0; i < parts.GetLength(); ++i )
	{
		context.AddExprChild(id, parts[i]);
	}
	return id;
}

asASTExprId asCSema::ActOnOpaqueValueExpr(asASTExprId source, const asCSourceRange& range)
{
	if( !source.IsValid() )
	{
		return asASTExprId();
	}
	asCQualType type = context.InternPrimitive(ttInt, 0);
	asEASTValueCategory category = asAST_VALUE_PRVALUE;
	if( const asCExpr* src = context.GetExpr(source) )
	{
		if( src->type.IsValid() )
		{
			type = src->type;
		}
		category = src->valueCategory;
	}
	const asASTExprId id = MakeExpr(context, asAST_EXPR_OPAQUE_VALUE, type, category, range, "opaque");
	context.AddExprChild(id, source);
	return id;
}

asASTExprId asCSema::ActOnThisExpr(const asCQualType& type, const asCSourceRange& range)
{
	return MakeExpr(context, asAST_EXPR_THIS, type, asAST_VALUE_LVALUE, range, "this");
}

asASTExprId asCSema::ActOnIndex(asASTExprId base, asASTExprId index, const asCQualType& type, const asCSourceRange& range)
{
	asCArray<asASTExprId> indices;
	indices.PushLast(index);
	return ActOnIndex(base, indices, type, range);
}

asASTExprId asCSema::ActOnIndex(asASTExprId base, const asCArray<asASTExprId>& indices, const asCQualType& type, const asCSourceRange& range)
{
	const asASTExprId id = MakeExpr(context, asAST_EXPR_INDEX, type, asAST_VALUE_LVALUE, range, "[]");
	context.AddExprChild(id, base);
	for( asUINT i = 0; i < indices.GetLength(); ++i )
	{
		context.AddExprChild(id, indices[i]);
	}
	return id;
}

asASTExprId asCSema::ActOnMemberRef(asASTExprId base, const char* name, const asCQualType& type, const asCSourceRange& range)
{
	const asASTExprId id = MakeExpr(context, asAST_EXPR_MEMBER_REF, type, asAST_VALUE_LVALUE, range, name);
	context.AddExprChild(id, base);
	return id;
}

static bool IsIntegerPrimitiveType(const asCType* type)
{
	return type && type->kind == asAST_TYPE_PRIMITIVE
		&& (type->primitiveToken == ttInt || type->primitiveToken == ttInt8
			|| type->primitiveToken == ttInt16 || type->primitiveToken == ttInt64
			|| type->primitiveToken == ttUInt || type->primitiveToken == ttUInt8
			|| type->primitiveToken == ttUInt16 || type->primitiveToken == ttUInt64);
}

static bool IsFloatPrimitiveType(const asCType* type)
{
	return type && type->kind == asAST_TYPE_PRIMITIVE
		&& (type->primitiveToken == ttFloat || type->primitiveToken == ttDouble
			|| type->primitiveToken == ttFloat32 || type->primitiveToken == ttFloat64);
}

static asASTDeclId SelectConstructor(asCASTContext& context, const asCQualType& type, const asCArray<asASTExprId>& args)
{
	const asCType* named = context.GetType(type.type);
	if( named == 0 || named->stableKey.GetLength() == 0 )
	{
		return asASTDeclId();
	}
	asASTDeclId classId;
	for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
	{
		const asCDecl* decl = context.GetDecl(asASTDeclId(i));
		if( decl && decl->kind == asAST_DECL_CLASS
			&& (decl->name.Equals(named->stableKey)
				|| (decl->stableKey.GetLength() && decl->stableKey.Equals(named->stableKey))) )
		{
			classId = decl->id;
			break;
		}
	}
	const asCDecl* cls = context.GetDecl(classId);
	if( cls == 0 )
	{
		return asASTDeclId();
	}
	asASTDeclId best;
	int bestScore = -1;
	for( asUINT i = 0; i < cls->children.GetLength(); ++i )
	{
		const asCDecl* child = context.GetDecl(cls->children[i]);
		if( child == 0 || child->kind != asAST_DECL_CONSTRUCTOR )
		{
			continue;
		}
		asUINT paramCount = 0;
		for( asUINT p = 0; p < child->children.GetLength(); ++p )
		{
			const asCDecl* param = context.GetDecl(child->children[p]);
			if( param && param->kind == asAST_DECL_PARAM )
			{
				++paramCount;
			}
		}
		if( paramCount != args.GetLength() )
		{
			continue;
		}
		int score = 0;
		asUINT paramIndex = 0;
		bool ok = true;
		for( asUINT p = 0; p < child->children.GetLength(); ++p )
		{
			const asCDecl* param = context.GetDecl(child->children[p]);
			if( param == 0 || param->kind != asAST_DECL_PARAM )
			{
				continue;
			}
			const asCExpr* arg = context.GetExpr(args[paramIndex]);
			++paramIndex;
			if( arg == 0 )
			{
				ok = false;
				break;
			}
			int rank = -1;
			if( param->type.type.value == arg->type.type.value )
			{
				rank = 2;
			}
			else if( IsFloatPrimitiveType(context.GetType(param->type.type))
				&& IsIntegerPrimitiveType(context.GetType(arg->type.type)) )
			{
				rank = 1;
			}
			if( rank < 0 )
			{
				ok = false;
				break;
			}
			score += rank;
		}
		if( ok && score > bestScore )
		{
			bestScore = score;
			best = child->id;
		}
	}
	return best;
}

asASTExprId asCSema::ActOnConstruct(const asCQualType& type, const asCArray<asASTExprId>& args, const asCSourceRange& range)
{
	if( args.GetLength() > 0 )
	{
		InternNativeCallablesForType(type);
	}
	else
	{
		InternNativeZeroArgCallablesForType(type);
	}
	asCArray<asASTExprId> converted;
	for( asUINT i = 0; i < args.GetLength(); ++i )
	{
		if( args[i].IsValid() && context.GetExpr(args[i]) )
		{
			converted.PushLast(args[i]);
		}
	}
	asASTDeclId ctor = SelectConstructor(context, type, converted);
	if( !ctor.IsValid() && converted.GetLength() > 0 )
	{
		bool allUnparsed = true;
		for( asUINT i = 0; i < converted.GetLength(); ++i )
		{
			const asCExpr* arg = context.GetExpr(converted[i]);
			if( arg && (arg->literal.GetLength() != 0 || arg->resolvedDecl.IsValid() || arg->children.GetLength() != 0) )
			{
				allUnparsed = false;
				break;
			}
		}
		if( allUnparsed )
		{
			converted.SetLength(0);
			ctor = SelectConstructor(context, type, converted);
		}
	}
	if( const asCDecl* decl = context.GetDecl(ctor) )
	{
		asUINT paramIndex = 0;
		for( asUINT p = 0; p < decl->children.GetLength(); ++p )
		{
			const asCDecl* param = context.GetDecl(decl->children[p]);
			if( param == 0 || param->kind != asAST_DECL_PARAM )
			{
				continue;
			}
			if( paramIndex >= converted.GetLength() )
			{
				break;
			}
			const asCExpr* arg = context.GetExpr(converted[paramIndex]);
			if( arg && param->type.type.value != arg->type.type.value )
			{
				converted[paramIndex] = ActOnConversion(converted[paramIndex], param->type, range);
			}
			++paramIndex;
		}
	}
	const asASTExprId id = MakeExpr(context, asAST_EXPR_CONSTRUCT, type, asAST_VALUE_PRVALUE, range, "ctor");
	context.SetResolvedDecl(id, ctor);
	if( const asCDecl* decl = context.GetDecl(ctor) )
	{
		context.SetLiteral(id, decl->stableKey.GetLength() ? decl->stableKey.AddressOf() : decl->name.AddressOf());
	}
	for( asUINT i = 0; i < converted.GetLength(); ++i )
	{
		context.AddExprChild(id, converted[i]);
	}
	return ActOnCleanup(ActOnMaterializeTemporary(id, range), range);
}

asASTStmtId asCSema::ActOnBlock(asASTDeclId owner, const asCArray<asASTStmtId>& children, const asCSourceRange& range)
{
	const asASTStmtId id = context.CreateStmt(asAST_STMT_BLOCK, owner, range);
	for( asUINT i = 0; i < children.GetLength(); ++i )
	{
		context.AddStmtChild(id, children[i]);
	}
	return id;
}

asASTStmtId asCSema::ActOnExprStmt(asASTDeclId owner, asASTExprId expr, const asCSourceRange& range)
{
	const asASTStmtId id = context.CreateStmt(asAST_STMT_EXPR, owner, range);
	context.SetStmtExpr(id, expr);
	return id;
}

asASTStmtId asCSema::ActOnIf(asASTDeclId owner, asASTExprId cond, asASTStmtId thenStmt, asASTStmtId elseStmt, const asCSourceRange& range)
{
	const asASTStmtId id = context.CreateStmt(asAST_STMT_IF, owner, range);
	context.SetStmtExpr(id, cond);
	context.AddStmtChild(id, thenStmt);
	if( elseStmt.IsValid() )
	{
		context.AddStmtChild(id, elseStmt);
	}
	return id;
}

static void ApplyLoopSafePoints(asCASTContext& context, asASTStmtId loop, asASTStmtId body)
{
	context.SetStmtSafePointRole(loop, asAST_SAFEPOINT_LOOP_BACKEDGE);
	if( body.IsValid() )
	{
		context.SetStmtSafePointRole(body, asAST_SAFEPOINT_LOOP_ENTRY);
	}
}

asASTStmtId asCSema::ActOnWhile(asASTDeclId owner, asASTExprId cond, asASTStmtId body, const asCSourceRange& range)
{
	const asASTStmtId id = context.CreateStmt(asAST_STMT_WHILE, owner, range);
	context.SetStmtExpr(id, cond);
	context.AddStmtChild(id, body);
	ApplyLoopSafePoints(context, id, body);
	return id;
}

asASTStmtId asCSema::ActOnDoWhile(asASTDeclId owner, asASTStmtId body, asASTExprId cond, const asCSourceRange& range)
{
	const asASTStmtId id = context.CreateStmt(asAST_STMT_DO_WHILE, owner, range);
	context.SetStmtExpr(id, cond);
	context.AddStmtChild(id, body);
	ApplyLoopSafePoints(context, id, body);
	return id;
}

asASTStmtId asCSema::ActOnFor(asASTDeclId owner, asASTStmtId initStmt, asASTExprId cond, asASTExprId incr, asASTStmtId body, const asCSourceRange& range)
{
	const asASTStmtId id = context.CreateStmt(asAST_STMT_FOR, owner, range);
	context.SetStmtExpr(id, cond);
	context.AddStmtChild(id, initStmt);
	context.AddStmtChild(id, body);
	if( incr.IsValid() )
	{
		asCSourceRange incrRange = range;
		if( const asCExpr* const incrExpr = context.GetExpr(incr) )
		{
			if( incrExpr->range.IsValid() )
			{
				incrRange = incrExpr->range;
			}
		}
		const asASTStmtId incrStmt = context.CreateStmt(asAST_STMT_EXPR, owner, incrRange);
		context.SetStmtExpr(incrStmt, incr);
		context.AddStmtChild(id, incrStmt);
	}
	ApplyLoopSafePoints(context, id, body);
	return id;
}

asASTStmtId asCSema::ActOnForeach(asASTDeclId owner, asASTStmtId varStmt, asASTExprId rangeExpr, asASTStmtId body, const asCSourceRange& range)
{
	const asASTStmtId id = context.CreateStmt(asAST_STMT_FOREACH, owner, range);
	context.SetStmtExpr(id, rangeExpr);
	context.AddStmtChild(id, varStmt);
	context.AddStmtChild(id, body);
	ApplyLoopSafePoints(context, id, body);
	return id;
}

asASTStmtId asCSema::ActOnBreak(asASTDeclId owner, asASTStmtId target, const asCSourceRange& range)
{
	const asASTStmtId id = context.CreateStmt(asAST_STMT_BREAK, owner, range);
	context.SetTarget(id, target);
	context.SetStmtSafePointRole(id, asAST_SAFEPOINT_TRANSFER);
	return id;
}

asASTStmtId asCSema::ActOnContinue(asASTDeclId owner, asASTStmtId target, const asCSourceRange& range)
{
	const asASTStmtId id = context.CreateStmt(asAST_STMT_CONTINUE, owner, range);
	context.SetTarget(id, target);
	context.SetStmtSafePointRole(id, asAST_SAFEPOINT_TRANSFER);
	return id;
}

asASTStmtId asCSema::ActOnSwitch(asASTDeclId owner, asASTExprId cond, const asCSourceRange& range)
{
	const asASTStmtId id = context.CreateStmt(asAST_STMT_SWITCH, owner, range);
	context.SetStmtExpr(id, cond);
	return id;
}

asASTStmtId asCSema::ActOnCase(asASTDeclId owner, asASTStmtId switchStmt, asASTExprId value, const asCSourceRange& range)
{
	const asASTStmtId id = context.CreateStmt(asAST_STMT_CASE, owner, range);
	context.SetStmtExpr(id, value);
	context.SetTarget(id, switchStmt);
	context.AddStmtChild(switchStmt, id);
	return id;
}

asASTStmtId asCSema::ActOnFallthrough(asASTDeclId owner, const asCSourceRange& range)
{
	return context.CreateStmt(asAST_STMT_FALLTHROUGH, owner, range);
}

END_AS_NAMESPACE
