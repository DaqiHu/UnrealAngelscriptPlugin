#include "as_sema_expr.h"
#include "as_datatype.h"
#include "as_module.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_runtime_type_bridge.h"
#include "as_scriptcode.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "as_sema_lifetime.h"
#include "as_string_util.h"
#include "as_tokendef.h"
#include "as_tokenizer.h"
#include <cstdlib>
#include <cstring>

BEGIN_AS_NAMESPACE

// These declarations are an AST-local view of script symbols that have already
// been committed by another module. They deliberately carry an origin marker:
// CodeGen must bind them back to the existing runtime object rather than
// publishing a second function/property for the consumer module.
static const char* const CANONICAL_AUTOMATIC_IMPORT_SCRIPT_FUNCTION_ORIGIN = "canonical-automatic-import-script-function";
static const char* const CANONICAL_AUTOMATIC_IMPORT_SCRIPT_GLOBAL_ORIGIN = "canonical-automatic-import-script-global";
static const char* const CANONICAL_CURRENT_MODULE_SCRIPT_FUNCTION_ORIGIN = "canonical-current-module-script-function";
static const char* const CANONICAL_CURRENT_MODULE_SCRIPT_GLOBAL_ORIGIN = "canonical-current-module-script-global";
static const char* const CANONICAL_NATIVE_TYPE_VIEW_ORIGIN = "canonical-native-type-view";

static asUINT CountParams(asCASTContext& context, const asCDecl* decl);
static bool ResolveCallableResultType(
	asCSema& sema,
	asCASTContext& context,
	const asCDecl& callee,
	asCQualType& outType);

static void AddStringDecodeDiagnostic(
	asCSema& sema,
	const char* token,
	const asCSourceRange& range)
{
	sema.AddDiagnostic(token, range);
}

static void DecodeCanonicalStringBytes(
	asCSema& sema,
	const asCString& sourceBytes,
	bool processEscapeSequences,
	const asCSourceRange& range,
	asCString& outBytes)
{
	asCScriptEngine* const engine = sema.GetEngine();
	const int scanner = engine ? engine->ep.scanner : 0;
	const int stringEncoding = engine ? engine->ep.stringEncoding : 0;
	asCArray<char> decoded(static_cast<int>(sourceBytes.GetLength()));

	for( asUINT n = 0; n < sourceBytes.GetLength(); ++n )
	{
#ifdef AS_DOUBLEBYTE_CHARSET
		if( (sourceBytes[n] & 0x80) && scanner == 0 && stringEncoding != 1 )
		{
			decoded.PushLast(sourceBytes[n]);
			if( ++n < sourceBytes.GetLength() )
			{
				decoded.PushLast(sourceBytes[n]);
			}
			continue;
		}
#endif

		asUINT value = 0;
		if( processEscapeSequences && sourceBytes[n] == '\\' )
		{
			++n;
			if( n == sourceBytes.GetLength() )
			{
				AddStringDecodeDiagnostic(sema, "string-literal-trailing-escape", range);
				break;
			}

			if( sourceBytes[n] == 'x' || sourceBytes[n] == 'X' )
			{
				++n;
				if( n == sourceBytes.GetLength() )
				{
					AddStringDecodeDiagnostic(sema, "string-literal-invalid-hex-escape", range);
					break;
				}
				value = 0;
				int remaining = stringEncoding == 1 ? 4 : 2;
				for( ; remaining > 0 && n < sourceBytes.GetLength(); --remaining, ++n )
				{
					const char ch = sourceBytes[n];
					if( ch >= '0' && ch <= '9' ) value = value * 16 + ch - '0';
					else if( ch >= 'a' && ch <= 'f' ) value = value * 16 + ch - 'a' + 10;
					else if( ch >= 'A' && ch <= 'F' ) value = value * 16 + ch - 'A' + 10;
					else break;
				}
				--n;
				if( stringEncoding == 0 )
				{
					decoded.PushLast(static_cast<char>(static_cast<asBYTE>(value)));
				}
				else
				{
#ifndef AS_BIG_ENDIAN
					decoded.PushLast(static_cast<char>(static_cast<asBYTE>(value)));
					decoded.PushLast(static_cast<char>(static_cast<asBYTE>(value >> 8)));
#else
					decoded.PushLast(static_cast<char>(static_cast<asBYTE>(value >> 8)));
					decoded.PushLast(static_cast<char>(static_cast<asBYTE>(value)));
#endif
				}
				continue;
			}
			if( sourceBytes[n] == 'u' || sourceBytes[n] == 'U' )
			{
				const bool shortForm = sourceBytes[n] == 'u';
				int remaining = shortForm ? 4 : 8;
				value = 0;
				for( ; remaining > 0; --remaining )
				{
					++n;
					if( n == sourceBytes.GetLength() ) break;
					const char ch = sourceBytes[n];
					if( ch >= '0' && ch <= '9' ) value = value * 16 + ch - '0';
					else if( ch >= 'a' && ch <= 'f' ) value = value * 16 + ch - 'a' + 10;
					else if( ch >= 'A' && ch <= 'F' ) value = value * 16 + ch - 'A' + 10;
					else break;
				}
				if( remaining != 0 )
				{
					AddStringDecodeDiagnostic(
						sema,
						shortForm ? "string-literal-invalid-unicode-4" : "string-literal-invalid-unicode-8",
						range);
					continue;
				}
			}
			else if( sourceBytes[n] == '"' ) value = '"';
			else if( sourceBytes[n] == '\'' ) value = '\'';
			else if( sourceBytes[n] == 'n' ) value = '\n';
			else if( sourceBytes[n] == 'r' ) value = '\r';
			else if( sourceBytes[n] == 't' ) value = '\t';
			else if( sourceBytes[n] == '0' ) value = '\0';
			else if( sourceBytes[n] == '\\' ) value = '\\';
			else
			{
				AddStringDecodeDiagnostic(sema, "string-literal-invalid-escape", range);
				continue;
			}
		}
		else if( scanner == 1 && (sourceBytes[n] & 0x80) )
		{
			unsigned int length = 0;
			value = asStringDecodeUTF8(&sourceBytes[n], &length);
			if( value == 0xFFFFFFFF )
			{
				AddStringDecodeDiagnostic(sema, "string-literal-invalid-source-utf8", range);
				value = static_cast<unsigned char>(sourceBytes[n]);
			}
			else
			{
				n += length - 1;
			}
		}
		else
		{
			value = static_cast<unsigned char>(sourceBytes[n]);
		}

		char encodedValue[5] = {};
		int encodedLength = 1;
		if( scanner == 1 && stringEncoding == 0 )
		{
			encodedLength = asStringEncodeUTF8(value, encodedValue);
		}
		else if( stringEncoding == 1 )
		{
			encodedLength = asStringEncodeUTF16(value, encodedValue);
		}
		else
		{
			encodedValue[0] = static_cast<char>(static_cast<asBYTE>(value));
		}
		if( encodedLength < 0 )
		{
			AddStringDecodeDiagnostic(sema, "string-literal-invalid-unicode-value", range);
			continue;
		}
		decoded.Concatenate(encodedValue, encodedLength);
	}

	if( decoded.GetLength() )
	{
		outBytes.Assign(decoded.AddressOf(), decoded.GetLength());
	}
	else
	{
		outBytes = "";
	}
}

static void TrimCanonicalHeredoc(asCScriptEngine* engine, asCString& bytes)
{
	bool isMultiline = false;
	int start = 0;
	for( ; start < static_cast<int>(bytes.GetLength()); ++start )
	{
		if( bytes[start] == '\n' )
		{
			isMultiline = true;
			++start;
			break;
		}
		if( bytes[start] != ' ' && bytes[start] != '\t' && bytes[start] != '\r' )
		{
			start = 0;
			break;
		}
	}

	int end = static_cast<int>(bytes.GetLength()) - 1;
	for( ; end >= 0; --end )
	{
		if( bytes[end] == '\n' )
		{
			++end;
			break;
		}
		if( bytes[end] != ' ' && bytes[end] != '\t' && bytes[end] != '\r' )
		{
			end = static_cast<int>(bytes.GetLength());
			break;
		}
	}
	if( end < 0 ) end = 0;

	const int trimMode = engine ? engine->ep.heredocTrimMode : 0;
	asCString trimmed;
	if( end > start || trimMode != 2 )
	{
		if( trimMode == 2 || (isMultiline && trimMode == 1) )
		{
			trimmed.Assign(&bytes[start], end - start);
		}
		else
		{
			trimmed = bytes;
		}
	}
	bytes = trimmed;
}

asASTExprId asCSema::ActOnLiteralExprAction(
	const asSLiteralExprAction& action,
	asCScriptCode* script)
{
	if( !PrepareParsedSource(script) )
	{
		return asASTExprId();
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("literal-action-range-invalid", range);
		return asASTExprId();
	}

	const char* const spelling = action.spelling.AddressOf()
		? action.spelling.AddressOf() : "";
	switch( action.token )
	{
	case ttTrue:
		return ActOnBoolLiteral(true, range);
	case ttFalse:
		return ActOnBoolLiteral(false, range);
	case ttIntConstant:
	case ttBitsConstant:
		return ActOnIntegerLiteral(
			(asQWORD)strtoull(spelling, 0, 0), range);
	case ttFloat32Constant:
		return ActOnFloatLiteral(spelling, range);
	case ttFloat64Constant:
		return ActOnDoubleLiteral(spelling, range);
	case ttNull:
		return ActOnNullLiteral(range);
	case ttStringConstant:
	case ttMultilineStringConstant:
	case ttHeredocStringConstant:
		break;
	default:
		AddDiagnostic("literal-action-token-invalid", range);
		return asASTExprId();
	}

	const bool heredoc = action.token == ttHeredocStringConstant;
	const asUINT delimiterLength = heredoc ? 3 : 1;
	if( action.spelling.GetLength() < delimiterLength * 2 )
	{
		AddDiagnostic("string-literal-token-malformed", range);
		return asASTExprId();
	}
	if( action.token == ttMultilineStringConstant
		&& engine != 0 && !engine->ep.allowMultilineStrings )
	{
		AddDiagnostic("string-literal-multiline-not-allowed", range);
	}

	asCString sourceBytes;
	sourceBytes.Assign(
		action.spelling.AddressOf() + delimiterLength,
		action.spelling.GetLength() - delimiterLength * 2);
	if( heredoc )
	{
		TrimCanonicalHeredoc(engine, sourceBytes);
	}
	asCString decoded;
	DecodeCanonicalStringBytes(
		*this, sourceBytes, !heredoc, range, decoded);
	return ActOnStringLiteral(
		decoded.AddressOf(), decoded.GetLength(), range);
}

static asASTDeclId InternNamespacePath(asCSema& sema, asCASTContext& context, const asSNameSpace* nameSpace)
{
	if( nameSpace == 0 || nameSpace->name.GetLength() == 0 )
	{
		return context.GetTranslationUnit();
	}

	asASTDeclId parent = context.GetTranslationUnit();
	const char* begin = nameSpace->name.AddressOf();
	while( begin && begin[0] )
	{
		const char* end = begin;
		while( end[0] && !(end[0] == ':' && end[1] == ':') )
		{
			++end;
		}
		if( end != begin )
		{
			asCString segment;
			segment.Assign(begin, asUINT(end - begin));
			parent = sema.ActOnStartNamespaceDecl(parent, segment.AddressOf(), asCSourceRange());
			if( !parent.IsValid() )
			{
				return asASTDeclId();
			}
		}
		begin = end[0] ? end + 2 : end;
	}
	return parent;
}

static bool HasMatchingExternalScriptFunction(
	asCASTContext& context,
	asASTDeclId parent,
	const asCScriptFunction* function,
	asCRuntimeTypeBridge& bridge)
{
	const asCDecl* parentDecl = context.GetDecl(parent);
	if( parentDecl == 0 || function == 0 )
	{
		return false;
	}
	for( asUINT i = 0; i < parentDecl->children.GetLength(); ++i )
	{
		const asCDecl* candidate = context.GetDecl(parentDecl->children[i]);
		if( candidate == 0 || candidate->kind != asAST_DECL_FUNCTION
			|| !candidate->name.Equals(function->name)
			|| CountParams(context, candidate) != function->parameterTypes.GetLength() )
		{
			continue;
		}
		if( bridge.FromDataType(context, function->returnType).type.value != candidate->type.type.value )
		{
			continue;
		}
		bool sameParameters = true;
		asUINT parameterIndex = 0;
		for( asUINT c = 0; c < candidate->children.GetLength(); ++c )
		{
			const asCDecl* parameter = context.GetDecl(candidate->children[c]);
			if( parameter == 0 || parameter->kind != asAST_DECL_PARAM )
			{
				continue;
			}
			if( parameterIndex >= function->parameterTypes.GetLength()
				|| bridge.FromDataType(context, function->parameterTypes[parameterIndex]).type.value != parameter->type.type.value )
			{
				sameParameters = false;
				break;
			}
			++parameterIndex;
		}
		if( sameParameters )
		{
			return true;
		}
	}
	return false;
}

static bool HasMatchingExternalScriptGlobal(
	asCASTContext& context,
	asASTDeclId parent,
	const asCGlobalProperty* property,
	asCRuntimeTypeBridge& bridge)
{
	const asCDecl* parentDecl = context.GetDecl(parent);
	if( parentDecl == 0 || property == 0 )
	{
		return false;
	}
	const asCQualType propertyType = bridge.FromDataType(context, property->type);
	for( asUINT i = 0; i < parentDecl->children.GetLength(); ++i )
	{
		const asCDecl* candidate = context.GetDecl(parentDecl->children[i]);
		if( candidate && candidate->kind == asAST_DECL_VAR
			&& candidate->name.Equals(property->name)
			&& candidate->type.type.value == propertyType.type.value )
		{
			return true;
		}
	}
	return false;
}

void asCSema::InternCompileFunctionModuleScope(asCModule* module)
{
	if( module == 0 || engine == 0 || module->defaultNamespace == 0 )
	{
		return;
	}

	// CompileFunction's authored declaration is parsed directly beneath the
	// translation unit and CodeGen interprets that lexical root as the module's
	// current default namespace. Mirror only that exact scope here. Importing a
	// different namespace as another root would change name lookup and would no
	// longer round-trip through ResolveCanonicalDeclNamespace.
	asCASTContext& ast = GetContext();
	const asASTDeclId owner = ast.GetTranslationUnit();
	if( !owner.IsValid() )
	{
		return;
	}
	asCRuntimeTypeBridge bridge(engine);
	for( asUINT i = 0; i < module->globalFunctionList.GetLength(); ++i )
	{
		asCScriptFunction* const function = module->globalFunctionList[i];
		if( function == 0 || function->module != module || function->objectType != 0
			|| function->nameSpace != module->defaultNamespace
			|| HasMatchingExternalScriptFunction(ast, owner, function, bridge) )
		{
			continue;
		}
		const asASTDeclId declaration = ActOnFunctionDecl(
			owner, function->name.AddressOf(),
			bridge.FromDataType(ast, function->returnType), asCSourceRange());
		ast.SetOrigin(declaration, CANONICAL_CURRENT_MODULE_SCRIPT_FUNCTION_ORIGIN);
		for( asUINT p = 0; p < function->parameterTypes.GetLength(); ++p )
		{
			const char* parameterName =
				p < function->parameterNames.GetLength() && function->parameterNames[p].GetLength()
					? function->parameterNames[p].AddressOf()
					: "";
			ActOnParamDecl(declaration, parameterName,
				bridge.FromDataType(ast, function->parameterTypes[p]), asCSourceRange());
		}
		FinishDecl(declaration);
	}

	for( asUINT i = 0; i < module->scriptGlobalsList.GetLength(); ++i )
	{
		asCGlobalProperty* const property = module->scriptGlobalsList[i];
		if( property == 0 || property->module != module
			|| property->nameSpace != module->defaultNamespace
			|| HasMatchingExternalScriptGlobal(ast, owner, property, bridge) )
		{
			continue;
		}
		const asASTDeclId declaration = ActOnVarDecl(
			owner, property->name.AddressOf(),
			bridge.FromDataType(ast, property->type), asCSourceRange());
		ast.SetOrigin(declaration, CANONICAL_CURRENT_MODULE_SCRIPT_GLOBAL_ORIGIN);
	}
}

// Legacy's automatic-import lookup consumes the engine-wide maps only after a
// producer module has committed. Canonical Sema has its own declaration graph,
// so project the selected, explicitly-qualified script functions into that
// graph before scope resolution. Their origin keeps the AST projection separate
// from declarations owned and published by this compilation. Cross-module
// mutable globals use the matching projection below now that bytecode retains
// a property reference across provider-module teardown.
static void InternAutomaticImportScriptFunctions(
	asCSema& sema,
	const asCString& qualifiedScope,
	const char* name)
{
	asCScriptEngine* const engine = sema.GetEngine();
	if( engine == 0 || !engine->ep.automaticImports || name == 0 || name[0] == 0 )
	{
		return;
	}
	if( qualifiedScope.GetLength() == 0 )
	{
		// Do not broaden legacy's module-local root-namespace lookup. The
		// explicit namespace is both the language contract and the identity
		// used below to prevent an arbitrary same-named producer from binding.
		return;
	}
	asSNameSpace* const nameSpace = engine->FindNameSpace(qualifiedScope.AddressOf());
	if( nameSpace == 0 )
	{
		return;
	}

	asCASTContext& context = sema.GetContext();
	const asASTDeclId owner = InternNamespacePath(sema, context, nameSpace);
	if( !owner.IsValid() )
	{
		return;
	}
	asCRuntimeTypeBridge bridge(engine);
	engine->allScriptGlobalFunctions.FindAll(name, nameSpace, [&](asCScriptFunction* function)
	{
		if( function == 0 || function->objectType != 0
			|| function->traits.GetTrait(asTRAIT_LOCAL)
			|| HasMatchingExternalScriptFunction(context, owner, function, bridge) )
		{
			return;
		}
		const asASTDeclId declaration = sema.ActOnFunctionDecl(
			owner, function->name.AddressOf(), bridge.FromDataType(context, function->returnType), asCSourceRange());
		context.SetOrigin(declaration, CANONICAL_AUTOMATIC_IMPORT_SCRIPT_FUNCTION_ORIGIN);
		for( asUINT p = 0; p < function->parameterTypes.GetLength(); ++p )
		{
			const char* parameterName = p < function->parameterNames.GetLength() && function->parameterNames[p].GetLength()
				? function->parameterNames[p].AddressOf()
				: "";
			sema.ActOnParamDecl(declaration, parameterName,
				bridge.FromDataType(context, function->parameterTypes[p]), asCSourceRange());
		}
		sema.FinishDecl(declaration);
	});
}

// Project an explicitly qualified, already-published script global into the
// Canonical declaration graph. CodeGen recognizes the origin and binds the
// declaration to the provider property rather than allocating a consumer-owned
// global slot. The matching function bytecode owns the property reference.
static void InternAutomaticImportScriptGlobal(
	asCSema& sema,
	const asCString& qualifiedScope,
	const char* name)
{
	asCScriptEngine* const engine = sema.GetEngine();
	if( engine == 0 || !engine->ep.automaticImports || name == 0 || name[0] == 0 )
	{
		return;
	}
	if( qualifiedScope.GetLength() == 0 )
	{
		// Root-namespace lookup remains module-local, matching Legacy's
		// automatic-import visibility contract.
		return;
	}
	asSNameSpace* const nameSpace = engine->FindNameSpace(qualifiedScope.AddressOf());
	if( nameSpace == 0 )
	{
		return;
	}

	asCGlobalProperty* selected = 0;
	bool ambiguous = false;
	engine->allScriptGlobalVariables.FindAll(name, nameSpace, [&](asCGlobalProperty* property)
	{
		if( property == 0 )
		{
			return;
		}
		if( selected != 0 && selected != property )
		{
			ambiguous = true;
			return;
		}
		selected = property;
	});
	if( selected == 0 || ambiguous )
	{
		return;
	}

	asCASTContext& context = sema.GetContext();
	const asASTDeclId owner = InternNamespacePath(sema, context, nameSpace);
	if( !owner.IsValid() )
	{
		return;
	}
	asCRuntimeTypeBridge bridge(engine);
	if( HasMatchingExternalScriptGlobal(context, owner, selected, bridge) )
	{
		return;
	}
	const asASTDeclId declaration = sema.ActOnVarDecl(
		owner, selected->name.AddressOf(), bridge.FromDataType(context, selected->type), asCSourceRange());
	context.SetOrigin(declaration, CANONICAL_AUTOMATIC_IMPORT_SCRIPT_GLOBAL_ORIGIN);
}

static asASTExprId FindExistingExpr(asCASTContext& context, asEASTExprKind kind, const asCSourceRange& range)
{
	if( range.begin.fileID == 0 || range.begin.offset == 0 )
	{
		return asASTExprId();
	}
	const bool requireEnd = range.end.fileID != 0 && range.end.offset != 0;
	for( asUINT i = 1; i <= context.GetExprCount(); ++i )
	{
		const asCExpr* existing = context.GetExpr(asASTExprId(i));
		if( existing == 0
			|| existing->kind != kind
			|| existing->range.begin.fileID != range.begin.fileID
			|| existing->range.begin.offset != range.begin.offset )
		{
			continue;
		}
		if( requireEnd
			&& (existing->range.end.fileID != range.end.fileID
				|| existing->range.end.offset != range.end.offset) )
		{
			continue;
		}
		return existing->id;
	}
	return asASTExprId();
}

static asASTExprId FindExistingCallByFullRange(asCASTContext& context, const asCSourceRange& range)
{
	if( range.begin.fileID == 0 )
	{
		return asASTExprId();
	}
	asASTExprId callId;
	for( asUINT i = 1; i <= context.GetExprCount(); ++i )
	{
		const asCExpr* existing = context.GetExpr(asASTExprId(i));
		if( existing
			&& existing->kind == asAST_EXPR_CALL
			&& existing->range.begin.fileID == range.begin.fileID
			&& existing->range.begin.offset == range.begin.offset
			&& existing->range.end.fileID == range.end.fileID
			&& existing->range.end.offset == range.end.offset )
		{
			callId = existing->id;
			break;
		}
	}
	if( !callId.IsValid() )
	{
		return asASTExprId();
	}
	for( asUINT i = 1; i <= context.GetExprCount(); ++i )
	{
		const asCExpr* existing = context.GetExpr(asASTExprId(i));
		if( existing
			&& existing->kind == asAST_EXPR_MATERIALIZE_TEMPORARY
			&& existing->range.begin.fileID == range.begin.fileID
			&& existing->range.begin.offset == range.begin.offset
			&& existing->range.end.fileID == range.end.fileID
			&& existing->range.end.offset == range.end.offset
			&& existing->children.GetLength()
			&& existing->children[0] == callId )
		{
			return existing->id;
		}
	}
	return callId;
}

static bool IsQualifierScopeDecl(const asCDecl* decl)
{
	return decl != 0
		&& (decl->kind == asAST_DECL_TRANSLATION_UNIT
			|| decl->kind == asAST_DECL_NAMESPACE
			|| decl->kind == asAST_DECL_CLASS
			|| decl->kind == asAST_DECL_INTERFACE
			|| decl->kind == asAST_DECL_ENUM);
}

static asASTDeclId FindQualifierScopeCandidate(
	asCASTContext& context,
	const asCArray<asASTDeclId>& hits)
{
	asASTDeclId candidate;
	for( asUINT i = 0; i < hits.GetLength(); ++i )
	{
		const asCDecl* const decl = context.GetDecl(hits[i]);
		if( !IsQualifierScopeDecl(decl) )
		{
			continue;
		}
		if( candidate.IsValid() && candidate != hits[i] )
		{
			// A qualifier is a unique scope identity, not a first-name-wins
			// recovery. Duplicate candidates fail closed and are diagnosed by the
			// ordinary unresolved-scope publication gate.
			return asASTDeclId();
		}
		candidate = hits[i];
	}
	return candidate;
}

static asASTDeclId FindNamedScopeDecl(asCSema* sema, asASTDeclId owner, const char* name)
{
	if( sema == 0 || name == 0 )
	{
		return asASTDeclId();
	}
	asCASTContext& context = sema->GetContext();
	asASTDeclId current = owner.IsValid() ? owner : context.GetTranslationUnit();
	while( current.IsValid() )
	{
		asCArray<asASTDeclId> hits;
		sema->LookupInScope(current, name, hits);
		const asASTDeclId candidate = FindQualifierScopeCandidate(context, hits);
		if( candidate.IsValid() )
		{
			return candidate;
		}
		const asCDecl* const decl = context.GetDecl(current);
		if( decl == 0 )
		{
			break;
		}
		current = decl->parent;
	}
	return asASTDeclId();
}

static asASTDeclId FindDirectChildNamed(asCSema* sema, asASTDeclId parent, const char* name)
{
	if( sema == 0 || name == 0 )
	{
		return asASTDeclId();
	}
	asCArray<asASTDeclId> hits;
	sema->LookupInScope(parent, name, hits);
	return FindQualifierScopeCandidate(sema->GetContext(), hits);
}

static asASTDeclId ResolveScopeSegments(
	asCSema* sema,
	asASTDeclId owner,
	const asCArray<asCString>& segments,
	bool absoluteScope,
	asCString* unresolvedSegment)
{
	if( unresolvedSegment != 0 )
	{
		*unresolvedSegment = asCString();
	}
	if( sema == 0 )
	{
		return asASTDeclId();
	}
	asASTDeclId current = absoluteScope
		? sema->GetContext().GetTranslationUnit() : asASTDeclId();
	for( asUINT segmentIndex = 0; segmentIndex < segments.GetLength(); ++segmentIndex )
	{
		const asCString& name = segments[segmentIndex];
		if( !current.IsValid() && segmentIndex == 0 && !absoluteScope )
		{
			current = FindNamedScopeDecl(sema, owner, name.AddressOf());
		}
		else
		{
			// Once the first qualifier has selected a scope, every remaining
			// segment is an exact child of that scope. Climbing to an ancestor here
			// would turn A::Missing::F into an unrelated A::F or global F.
			current = FindDirectChildNamed(sema, current, name.AddressOf());
		}
		if( !current.IsValid() )
		{
			if( unresolvedSegment != 0 )
			{
				*unresolvedSegment = name;
			}
			return asASTDeclId();
		}
	}
	return current;
}

static asCString JoinScopeSegments(
	const asCArray<asCString>& segments)
{
	asCString result;
	for( asUINT i = 0; i < segments.GetLength(); ++i )
	{
		if( segments[i].GetLength() == 0 )
		{
			continue;
		}
		if( result.GetLength() )
		{
			result += "::";
		}
		result += segments[i];
	}
	return result;
}

static asASTDeclId FindTypeMethod(asCASTContext& context, asASTDeclId typeDecl, const char* name)
{
	const asCDecl* type = context.GetDecl(typeDecl);
	if( type == 0 || name == 0 )
	{
		return asASTDeclId();
	}
	for( asUINT i = 0; i < type->children.GetLength(); ++i )
	{
		const asCDecl* child = context.GetDecl(type->children[i]);
		if( child && child->kind == asAST_DECL_METHOD && child->name.Equals(name) )
		{
			return child->id;
		}
	}
	return asASTDeclId();
}

static asASTDeclId FindNamedTypeDecl(asCASTContext& context, const asCQualType& type);

static const asCExpr* UnwrapByValueCopyPlan(
	asCASTContext& context, const asCExpr* expression)
{
	while( expression != 0
		&& (expression->kind == asAST_EXPR_CLEANUP
			|| expression->kind == asAST_EXPR_MATERIALIZE_TEMPORARY)
		&& expression->children.GetLength() == 1 )
	{
		expression = context.GetExpr(expression->children[0]);
	}
	return expression;
}

static asASTExprId SealNonPodByValueCopyPlan(
	asCSema& sema,
	asCASTContext& context,
	const asCDecl* formal,
	asASTExprId argument,
	const asCSourceRange& range)
{
	const asCExpr* const source = context.GetExpr(argument);
	if( formal == 0 || formal->kind != asAST_DECL_PARAM
		|| source == 0 || !formal->type.IsValid()
		|| formal->type.IsReference() || formal->type.IsHandle()
		|| source->valueCategory != asAST_VALUE_LVALUE
		|| sema.GetEngine() == 0 )
	{
		return argument;
	}

	// A non-POD value formal owns a distinct object. Seal the exact copy
	// constructor at the call edge while the argument is still an lvalue; the
	// backend then only chooses the VM ownership-transfer mechanics. POD/trivial
	// storage remains on the explicit storage-copy route.
	asCRuntimeTypeBridge bridge(sema.GetEngine());
	const asCDataType runtimeType = bridge.Resolve(context, formal->type);
	asCTypeInfo* const typeInfo = runtimeType.GetTypeInfo();
	if( !runtimeType.IsNonPrimitiveValueType() || typeInfo == 0
		|| (typeInfo->GetFlags() & asOBJ_VALUE) == 0
		|| (typeInfo->GetFlags() & asOBJ_POD) != 0 )
	{
		return argument;
	}

	asCArray<asASTExprId> copyArguments;
	copyArguments.PushLast(argument);
	const asASTExprId plan = sema.ActOnConstruct(
		formal->type, copyArguments, range);
	const asCExpr* const construct = UnwrapByValueCopyPlan(
		context, context.GetExpr(plan));
	if( construct == 0 || construct->kind != asAST_EXPR_CONSTRUCT
		|| construct->type != formal->type
		|| !construct->resolvedDecl.IsValid()
		|| construct->children.GetLength() != 1
		|| construct->children[0] != argument )
	{
		sema.AddDiagnostic(
			"by-value-copy-constructor-unavailable", range);
		return argument;
	}
	return plan;
}

static asASTExprId TryRewritePropertyGet(asCSema& sema, asCASTContext& context, asASTExprId base, const char* propName, const asCQualType& fallbackType, const asCSourceRange& range)
{
	const asCExpr* baseExpr = context.GetExpr(base);
	if( baseExpr == 0 || propName == 0 || *propName == 0 )
	{
		return asASTExprId();
	}
	asCString getName = "Get";
	getName += propName;
	const asASTDeclId getter = FindTypeMethod(context, FindNamedTypeDecl(context, baseExpr->type), getName.AddressOf());
	if( !getter.IsValid() )
	{
		return asASTExprId();
	}
	asCArray<asASTExprId> args;
	args.PushLast(base);
	asCQualType type = fallbackType;
	if( const asCDecl* decl = context.GetDecl(getter) )
	{
		if( decl->type.IsValid() )
		{
			type = decl->type;
		}
	}
	const asASTExprId call = sema.ActOnCall(getter, args, type, range);
	context.SetExprReceiver(call, base);
	return call;
}

static asASTExprId TryRewritePropertySet(asCSema& sema, asCASTContext& context, asASTExprId lhs, asASTExprId rhs, const asCQualType& fallbackType, const asCSourceRange& range)
{
	const asCExpr* left = context.GetExpr(lhs);
	while( left && left->kind == asAST_EXPR_SEQUENCE && left->children.GetLength() > 0 )
	{
		lhs = left->children[left->children.GetLength() - 1];
		left = context.GetExpr(lhs);
	}
	if( left == 0 )
	{
		return asASTExprId();
	}

	asCString propName;
	asASTExprId receiver;
	if( left->kind == asAST_EXPR_CALL && left->resolvedDecl.IsValid() )
	{
		if( const asCDecl* getter = context.GetDecl(left->resolvedDecl) )
		{
			if( getter->name.GetLength() > 3
				&& std::strncmp(getter->name.AddressOf(), "Get", 3) == 0 )
			{
				propName.Assign(getter->name.AddressOf() + 3, getter->name.GetLength() - 3);
			}
		}
		if( left->children.GetLength() > 0 )
		{
			receiver = left->children[left->children.GetLength() - 1];
		}
	}
	else if( left->kind == asAST_EXPR_MEMBER_REF && left->children.GetLength() > 0 )
	{
		propName = left->literal;
		receiver = left->children[0];
	}
	if( propName.GetLength() == 0 || !receiver.IsValid() )
	{
		return asASTExprId();
	}
	const asCExpr* recv = context.GetExpr(receiver);
	if( recv == 0 )
	{
		return asASTExprId();
	}
	asCString setName = "Set";
	setName += propName;
	const asASTDeclId setter = FindTypeMethod(context, FindNamedTypeDecl(context, recv->type), setName.AddressOf());
	if( !setter.IsValid() )
	{
		return asASTExprId();
	}
	if( const asCDecl* setterDecl = context.GetDecl(setter) )
	{
		for( asUINT p = 0; p < setterDecl->children.GetLength(); ++p )
		{
			const asCDecl* param = context.GetDecl(setterDecl->children[p]);
			if( param == 0 || param->kind != asAST_DECL_PARAM )
			{
				continue;
			}
			if( const asCExpr* rhsExpr = context.GetExpr(rhs) )
			{
				if( rhsExpr->type.type.value != param->type.type.value )
				{
					rhs = sema.ActOnConversion(rhs, param->type, range);
				}
			}
			rhs = SealNonPodByValueCopyPlan(
				sema, context, param, rhs, range);
			break;
		}
	}
	asCArray<asASTExprId> args;
	args.PushLast(receiver);
	args.PushLast(rhs);
	const asASTExprId call = sema.ActOnCall(setter, args, fallbackType, range);
	context.SetExprReceiver(call, receiver);
	return call;
}

static asASTDeclId FindNamedTypeDecl(asCASTContext& context, const asCQualType& type)
{
	const asCType* named = context.GetType(type.type);
	if( named == 0 || named->stableKey.GetLength() == 0
		|| named->kind == asAST_TYPE_PRIMITIVE || named->kind == asAST_TYPE_VOID )
	{
		return asASTDeclId();
	}
	for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
	{
		const asCDecl* decl = context.GetDecl(asASTDeclId(i));
		if( decl && (decl->kind == asAST_DECL_CLASS || decl->kind == asAST_DECL_INTERFACE)
			&& (decl->name.Equals(named->stableKey)
				|| (decl->stableKey.GetLength() && decl->stableKey.Equals(named->stableKey))) )
		{
			return decl->id;
		}
	}
	return asASTDeclId();
}

static asUINT CountParams(asCASTContext& context, const asCDecl* decl)
{
	asUINT count = 0;
	if( decl == 0 )
	{
		return 0;
	}
	for( asUINT i = 0; i < decl->children.GetLength(); ++i )
	{
		const asCDecl* child = context.GetDecl(decl->children[i]);
		if( child && child->kind == asAST_DECL_PARAM )
		{
			++count;
		}
	}
	return count;
}

static asASTDeclId FindBestCallee(asCASTContext& context, asASTDeclId owner, const char* name, const asCArray<asASTExprId>& args, asCSema* sema = 0, const asCQualType* receiverType = 0, bool exactScope = false);
static int RankArgument(asCASTContext& context, const asCQualType& paramType, const asCExpr* arg);
static bool IsFuncdefType(asCASTContext& context, const asCQualType& type);
void InternNativeMethods(asCSema& sema, asCScriptEngine* engine, asCASTContext& context, const asCQualType& recvType, const char* name);
static asASTDeclId FindFuncdefHandle(asCSema& sema, asCASTContext& context, asASTDeclId searchOwner, const char* name, bool exactScope = false);

static asCQualType CanonicalRuntimeParameterType(
	asCRuntimeTypeBridge& bridge,
	asCASTContext& context,
	const asCScriptFunction* function,
	asUINT parameterIndex)
{
	if( function == 0 || parameterIndex >= function->parameterTypes.GetLength() )
	{
		return asCQualType();
	}

	asCQualType type = bridge.FromDataType(
		context, function->parameterTypes[parameterIndex]);
	if( parameterIndex >= function->inOutFlags.GetLength() )
	{
		return type;
	}

	// asCDataType owns const/handle/reference representation, while the
	// parameter direction is stored separately by the Runtime declaration.
	// Project both facts into the sealed QualType so downstream consumers never
	// need to recover source semantics from an Engine-local function shell.
	type.quals &= ~asAST_QUAL_PARAM_DIR_MASK;
	switch( function->inOutFlags[parameterIndex] )
	{
	case asTM_INREF:
		type.quals |= asAST_QUAL_REFERENCE | asAST_QUAL_IN;
		break;
	case asTM_OUTREF:
		type.quals |= asAST_QUAL_REFERENCE | asAST_QUAL_OUT;
		break;
	case asTM_INOUTREF:
		type.quals |= asAST_QUAL_REFERENCE | asAST_QUAL_INOUT;
		break;
	default:
		break;
	}
	return type;
}

static void InternNativeGlobals(asCSema& sema, asCScriptEngine* engine, asCASTContext& context, const char* name)
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
	const asUINT count = engine->GetGlobalFunctionCount();
	for( asUINT i = 0; i < count; ++i )
	{
		asCScriptFunction* func = static_cast<asCScriptFunction*>(engine->GetGlobalFunctionByIndex(i));
		if( func == 0 || !func->name.Equals(name) )
		{
			continue;
		}
		const asASTDeclId declarationOwner =
			InternNamespacePath(sema, context, func->nameSpace);
		const asCDecl* const ownerDecl = context.GetDecl(declarationOwner);
		if( ownerDecl == 0 )
		{
			continue;
		}
		bool exists = false;
		for( asUINT c = 0; c < ownerDecl->children.GetLength(); ++c )
		{
			const asCDecl* child = context.GetDecl(ownerDecl->children[c]);
			if( child == 0 || child->kind != asAST_DECL_FUNCTION || !child->name.Equals(name)
				|| CountParams(context, child) != func->parameterTypes.GetLength() )
			{
				continue;
			}
			const asCQualType internedRet = bridge.FromDataType(context, func->returnType);
			if( internedRet.type.value != child->type.type.value )
			{
				continue;
			}
			bool sameTypes = true;
			asUINT paramIndex = 0;
			for( asUINT d = 0; d < child->children.GetLength() && sameTypes; ++d )
			{
				const asCDecl* param = context.GetDecl(child->children[d]);
				if( param == 0 || param->kind != asAST_DECL_PARAM )
				{
					continue;
				}
				if( paramIndex >= func->parameterTypes.GetLength() )
				{
					sameTypes = false;
					break;
				}
				const asCQualType interned = bridge.FromDataType(context, func->parameterTypes[paramIndex]);
				if( interned.type.value != param->type.type.value )
				{
					sameTypes = false;
					break;
				}
				++paramIndex;
			}
			if( sameTypes )
			{
				exists = true;
				break;
			}
		}
		if( exists )
		{
			continue;
		}
		const asCQualType ret = bridge.FromDataType(context, func->returnType);
		const asASTDeclId fn = sema.ActOnFunctionDecl(
			declarationOwner, func->name.AddressOf(), ret, asCSourceRange());
		// Preserve the exact runtime declaration spelling as the stable external
		// identity of this projected Decl.  Engine-local function ids are valid
		// only for the current process and names alone are ambiguous; downstream
		// consumers (JIT, cache validation, tooling) must be able to reconcile the
		// sealed AST with a current environment without either approximation.
		context.SetOrigin(fn, func->GetDeclaration(false, false, false));
		for( asUINT p = 0; p < func->parameterTypes.GetLength(); ++p )
		{
			const asCQualType pt = CanonicalRuntimeParameterType(
				bridge, context, func, p);
			const char* pname = (p < func->parameterNames.GetLength() && func->parameterNames[p].GetLength())
				? func->parameterNames[p].AddressOf()
				: "";
			const asASTDeclId param = sema.ActOnParamDecl(fn, pname, pt, asCSourceRange());
			if( func->hiddenArgumentIndex >= 0 && p == (asUINT)func->hiddenArgumentIndex )
			{
				context.SetOrigin(param, "hidden");
				if( func->hiddenArgumentDefault.GetLength() )
				{
					context.SetDefaultArg(param, func->hiddenArgumentDefault.AddressOf());
				}
			}
		}
		sema.FinishDecl(fn);
	}
}

static asASTDeclId FindTypeMemberVar(asCASTContext& context, asASTDeclId classDecl, const char* name)
{
	const asCDecl* cls = context.GetDecl(classDecl);
	if( cls == 0 || name == 0 || name[0] == 0 )
	{
		return asASTDeclId();
	}
	for( asUINT i = 0; i < cls->children.GetLength(); ++i )
	{
		const asCDecl* child = context.GetDecl(cls->children[i]);
		if( child && child->kind == asAST_DECL_VAR && child->name.Equals(name) )
		{
			return child->id;
		}
	}
	return asASTDeclId();
}

static void SyncImportedNativeClassKind(
	asCASTContext& context,
	asASTDeclId classDecl,
	const asCDataType& dataType)
{
	const asCTypeInfo* const typeInfo = dataType.GetTypeInfo();
	if( !classDecl.IsValid() || typeInfo == 0 )
	{
		return;
	}
	// A class-shaped declaration imported from the runtime registry is a
	// read-only projection of host ABI facts. Mark that ownership explicitly so
	// script-class layout sealing never replaces registered property offsets with
	// a source-language layout computed from the projected member subset.
	context.SetOrigin(classDecl, CANONICAL_NATIVE_TYPE_VIEW_ORIGIN);
	if( (typeInfo->GetFlags() & asOBJ_VALUE) != 0 )
	{
		// A range-less class declaration imported for native properties,
		// methods, or behaviours is an AST symbol view of the host type. Keep
		// the host VALUE/REF fact on that declaration so later source type
		// lookup cannot reinterpret a VALUE object as a script reference class.
		context.AddDeclTrait(classDecl, asAST_TRAIT_VALUE);
	}
}

static bool IsAuthoredScriptClass(
	asCASTContext& context,
	asASTDeclId classDecl)
{
	const asCDecl* const decl = context.GetDecl(classDecl);
	return decl != 0
		&& decl->kind == asAST_DECL_CLASS
		&& decl->range.begin.IsValid()
		&& !decl->origin.Equals(CANONICAL_NATIVE_TYPE_VIEW_ORIGIN);
}

static void InternNativeProperties(asCSema& sema, asCScriptEngine* engine, asCASTContext& context, const asCQualType& recvType)
{
	if( engine == 0 )
	{
		return;
	}
	asCRuntimeTypeBridge bridge(engine);
	const asCDataType dataType = bridge.Resolve(context, recvType);
	asCObjectType* const objectType = CastToObjectType(dataType.GetTypeInfo());
	if( objectType == 0 )
	{
		return;
	}
	asASTDeclId classDecl = FindNamedTypeDecl(context, recvType);
	if( classDecl.IsValid() && IsAuthoredScriptClass(context, classDecl) )
	{
		// A prior module may already have committed a Runtime type with the same
		// namespace/name. That registry entry is not the semantic owner of this
		// module's authored class and must not reclassify or populate its AST.
		return;
	}
	if( !classDecl.IsValid() )
	{
		const asCType* named = context.GetType(recvType.type);
		const char* key = named && named->stableKey.GetLength()
			? named->stableKey.AddressOf()
			: objectType->name.AddressOf();
		classDecl = sema.ActOnClassDecl(context.GetTranslationUnit(), key, asCSourceRange());
		sema.FinishDecl(classDecl);
	}
	SyncImportedNativeClassKind(context, classDecl, dataType);
	for( asUINT p = 0; p < objectType->properties.GetLength(); ++p )
	{
		asCObjectProperty* prop = objectType->properties[p];
		if( prop == 0 || prop->name.GetLength() == 0 )
		{
			continue;
		}
		if( FindTypeMemberVar(context, classDecl, prop->name.AddressOf()).IsValid() )
		{
			continue;
		}
		asCQualType pt = bridge.FromDataType(context, prop->type);
		const asASTDeclId var = sema.ActOnVarDecl(classDecl, prop->name.AddressOf(), pt, asCSourceRange());
		context.SetDeclByteOffset(var, prop->byteOffset);
		sema.FinishDecl(var);
	}
}

void InternNativeMethods(asCSema& sema, asCScriptEngine* engine, asCASTContext& context, const asCQualType& recvType, const char* name)
{
	if( engine == 0 || name == 0 || name[0] == 0 )
	{
		return;
	}
	asCRuntimeTypeBridge bridge(engine);
	const asCDataType dataType = bridge.Resolve(context, recvType);
	asCObjectType* const objectType = CastToObjectType(dataType.GetTypeInfo());
	if( objectType == 0 )
	{
		return;
	}
	asASTDeclId classDecl = FindNamedTypeDecl(context, recvType);
	// Host conversion operators are language facts of the native type. An
	// authored class with the same spelling must not hide `opImplConv`.
	if( classDecl.IsValid() && IsAuthoredScriptClass(context, classDecl)
		&& !asCString(name).Equals("opImplConv") )
	{
		return;
	}
	if( !classDecl.IsValid() )
	{
		const asCType* named = context.GetType(recvType.type);
		const char* key = named && named->stableKey.GetLength()
			? named->stableKey.AddressOf()
			: objectType->name.AddressOf();
		classDecl = sema.ActOnClassDecl(context.GetTranslationUnit(), key, asCSourceRange());
		sema.FinishDecl(classDecl);
	}
	SyncImportedNativeClassKind(context, classDecl, dataType);
	objectType->FindMethodUntil(name, [&](asCScriptFunction* method)
	{
		if( method == 0 || !method->name.Equals(name) )
		{
			return false;
		}
		asCScriptFunction* internMethod = method;
		if( objectType->templateBaseType != 0
			&& method->traits.GetTrait(asTRAIT_GENERIC_TEMPLATE_FUNCTION) )
		{
			internMethod = engine->GenerateTemplateFunction(objectType, method);
			if( internMethod == 0 )
			{
				// CompileModules parse can refuse nested template-function
				// generation. The generic signature is still the host operator
				// (UClass opImplConv() does not depend on T) and must rank.
				internMethod = method;
			}
		}
		asCDataType retDt = internMethod->returnType;
		asCArray<asCDataType> substParams;
		if( objectType->templateBaseType )
		{
			retDt = engine->DetermineTypeForTemplate(
				retDt, objectType->templateBaseType, objectType);
		}
		for( asUINT p = 0; p < internMethod->parameterTypes.GetLength(); ++p )
		{
			asCDataType paramType = internMethod->parameterTypes[p];
			if( objectType->templateBaseType )
			{
				paramType = engine->DetermineTypeForTemplate(
					paramType, objectType->templateBaseType, objectType);
			}
			substParams.PushLast(paramType);
		}
		const asCDecl* cls = context.GetDecl(classDecl);
		if( cls )
		{
			for( asUINT c = 0; c < cls->children.GetLength(); ++c )
			{
				const asCDecl* child = context.GetDecl(cls->children[c]);
				if( child == 0 || child->kind != asAST_DECL_METHOD || !child->name.Equals(name)
					|| CountParams(context, child) != substParams.GetLength() )
				{
					continue;
				}
				const asCQualType internedRet = bridge.FromDataType(context, retDt);
				if( internedRet.type.value != child->type.type.value )
				{
					// Zero-arg `opImplConv` overloads differ by return type
					// (`UClass` vs `UObject`). Name+arity alone is not identity.
					continue;
				}
				bool sameTypes = true;
				asUINT paramIndex = 0;
				for( asUINT d = 0; d < child->children.GetLength() && sameTypes; ++d )
				{
					const asCDecl* param = context.GetDecl(child->children[d]);
					if( param == 0 || param->kind != asAST_DECL_PARAM )
					{
						continue;
					}
					if( paramIndex >= substParams.GetLength() )
					{
						sameTypes = false;
						break;
					}
					const asCQualType interned = bridge.FromDataType(context, substParams[paramIndex]);
					if( interned.type.value != param->type.type.value )
					{
						sameTypes = false;
						break;
					}
					++paramIndex;
				}
				if( sameTypes )
				{
					return false;
				}
			}
		}
		const asCQualType ret = bridge.FromDataType(context, retDt);
		const asASTDeclId fn = sema.ActOnMethodDecl(classDecl, name, ret, asCSourceRange());
		context.SetOrigin(fn, internMethod->GetDeclaration(false, false, false));
		if( internMethod->IsReadOnly() )
		{
			context.AddDeclTrait(fn, asAST_TRAIT_CONST_METHOD);
		}
		for( asUINT p = 0; p < substParams.GetLength(); ++p )
		{
			asCDataType paramType = substParams[p];
			asCQualType pt = bridge.FromDataType(context, paramType);
			if( internMethod->inOutFlags.GetLength() > p )
			{
				if( internMethod->inOutFlags[p] == asTM_INREF || paramType.IsReference() )
				{
					pt.quals |= asAST_QUAL_IN | asAST_QUAL_REFERENCE;
				}
				if( internMethod->inOutFlags[p] == asTM_OUTREF )
				{
					pt.quals |= asAST_QUAL_OUT | asAST_QUAL_REFERENCE;
				}
				if( internMethod->inOutFlags[p] == asTM_INOUTREF )
				{
					pt.quals |= asAST_QUAL_INOUT | asAST_QUAL_REFERENCE;
				}
			}
			const char* pname = (p < internMethod->parameterNames.GetLength() && internMethod->parameterNames[p].GetLength())
				? internMethod->parameterNames[p].AddressOf()
				: "";
			sema.ActOnParamDecl(fn, pname, pt, asCSourceRange());
		}
		sema.FinishDecl(fn);
		return false;
	});
}

static asASTDeclId ResolveNativeIndexOp(
	asCSema& sema,
	asCASTContext& context,
	asASTExprId base,
	const asCArray<asASTExprId>& indexArgs)
{
	const asCExpr* baseExpr = context.GetExpr(base);
	if( baseExpr == 0 || indexArgs.GetLength() == 0 )
	{
		return asASTDeclId();
	}
	if( asCScriptEngine* engine = sema.GetEngine() )
	{
		InternNativeMethods(sema, engine, context, baseExpr->type, "opIndex");
	}
	const asASTDeclId typeDecl = FindNamedTypeDecl(context, baseExpr->type);
	return FindBestCallee(
		context, typeDecl, "opIndex", indexArgs, &sema);
}

static bool NativeBehaviourParamsMatch(
	asCASTContext& context,
	asCRuntimeTypeBridge& bridge,
	const asCDecl* child,
	asCScriptFunction* func,
	const asCArray<asCDataType>& substParams)
{
	if( child == 0 || func == 0 || CountParams(context, child) != substParams.GetLength() )
	{
		return false;
	}
	asUINT paramIndex = 0;
	for( asUINT d = 0; d < child->children.GetLength(); ++d )
	{
		const asCDecl* param = context.GetDecl(child->children[d]);
		if( param == 0 || param->kind != asAST_DECL_PARAM )
		{
			continue;
		}
		if( paramIndex >= substParams.GetLength() )
		{
			return false;
		}
		const asCQualType interned = bridge.FromDataType(context, substParams[paramIndex]);
		if( interned.type.value != param->type.type.value )
		{
			return false;
		}
		++paramIndex;
	}
	return true;
}

static asASTDeclId InternNativeBehaviourList(
	asCSema& sema,
	asCScriptEngine* engine,
	asCASTContext& context,
	const asCQualType& recvType,
	asCObjectType* objectType,
	asASTDeclId classDecl,
	const asCArray<int>& ids,
	bool isFactory,
	bool zeroArgOnly,
	asDWORD traits = asAST_TRAIT_NONE)
{
	if( engine == 0 || objectType == 0 || !classDecl.IsValid() )
	{
		return asASTDeclId();
	}
	asCRuntimeTypeBridge bridge(engine);
	asASTDeclId selected;
	for( asUINT i = 0; i < ids.GetLength(); ++i )
	{
		const int id = ids[i];
		asCScriptFunction* func = (id >= 0 && (asUINT)id < engine->scriptFunctions.GetLength())
			? engine->scriptFunctions[id]
			: 0;
		if( func == 0 )
		{
			continue;
		}
		if( objectType->templateBaseType && func->objectType == objectType->templateBaseType )
		{
			// A non-dependent template behaviour may remain owned by the template
			// base even though the concrete instance's behaviour list references it.
			// Canonical relocations require the concrete owner, so materialize the
			// same instance-local function shell used by native template methods.
			// Importing the generic base directly would resolve Sema but later record
			// a bare TArray<T> owner instead of the source TArray<int> identity.
			asCScriptFunction* const instantiated =
				engine->GenerateTemplateFunction(objectType, func);
			if( instantiated == 0 )
			{
				continue;
			}
			func = instantiated;
		}
		asCArray<asCDataType> substParams;
		asUINT start = 0;
		if( func->hiddenArgumentIndex >= 0 )
		{
			start = (asUINT)func->hiddenArgumentIndex + 1;
			if( start > func->parameterTypes.GetLength() )
			{
				start = func->parameterTypes.GetLength();
			}
		}
		else if( isFactory && func->parameterTypes.GetLength() > 0
			&& func->parameterTypes[0].IsReference()
			&& !func->parameterTypes[0].IsPrimitive() )
		{
			start = 1;
		}
		else if( isFactory && objectType->templateBaseType && func->parameterTypes.GetLength() > 0 )
		{
			start = 1;
		}
		for( asUINT p = start; p < func->parameterTypes.GetLength(); ++p )
		{
			asCDataType paramType = func->parameterTypes[p];
			if( objectType->templateBaseType )
			{
				paramType = engine->DetermineTypeForTemplate(paramType, objectType->templateBaseType, objectType);
			}
			substParams.PushLast(paramType);
		}
		if( zeroArgOnly && substParams.GetLength() != 0 )
		{
			continue;
		}
		const char* ctorName = func->name.GetLength() ? func->name.AddressOf() : objectType->name.AddressOf();
		bool exists = false;
		const asCDecl* cls = context.GetDecl(classDecl);
		if( cls )
		{
			for( asUINT c = 0; c < cls->children.GetLength(); ++c )
			{
				const asCDecl* child = context.GetDecl(cls->children[c]);
				if( child && child->kind == asAST_DECL_CONSTRUCTOR
					&& (traits == asAST_TRAIT_NONE
						|| (child->traits & traits) == traits)
					&& (traits == asAST_TRAIT_NONE || child->name.Equals(ctorName))
					&& NativeBehaviourParamsMatch(context, bridge, child, func, substParams) )
				{
					exists = true;
					if( !selected.IsValid() )
					{
						selected = child->id;
					}
					break;
				}
			}
		}
		if( exists )
		{
			continue;
		}
		const asASTDeclId ctor = sema.ActOnConstructorDecl(classDecl, ctorName, asCSourceRange());
		if( traits != asAST_TRAIT_NONE )
		{
			context.AddDeclTrait(ctor, traits);
		}
		for( asUINT p = 0; p < substParams.GetLength(); ++p )
		{
			asCQualType pt = bridge.FromDataType(context, substParams[p]);
			const asUINT src = start + p;
			if( src < func->inOutFlags.GetLength() )
			{
				if( func->inOutFlags[src] == asTM_INREF || substParams[p].IsReference() )
				{
					pt.quals |= asAST_QUAL_IN | asAST_QUAL_REFERENCE;
				}
				if( func->inOutFlags[src] == asTM_OUTREF )
				{
					pt.quals |= asAST_QUAL_OUT | asAST_QUAL_REFERENCE;
				}
				if( func->inOutFlags[src] == asTM_INOUTREF )
				{
					pt.quals |= asAST_QUAL_INOUT | asAST_QUAL_REFERENCE;
				}
			}
			const char* pname = (src < func->parameterNames.GetLength() && func->parameterNames[src].GetLength())
				? func->parameterNames[src].AddressOf()
				: "";
			sema.ActOnParamDecl(ctor, pname, pt, asCSourceRange());
		}
		sema.FinishDecl(ctor);
		if( !selected.IsValid() )
		{
			selected = ctor;
		}
	}
	(void)recvType;
	return selected;
}

static asASTDeclId InternNativeDestructor(
	asCSema& sema,
	asCScriptEngine* engine,
	asCASTContext& context,
	asCObjectType* objectType,
	asASTDeclId classDecl)
{
	if( engine == 0 || objectType == 0 || !classDecl.IsValid()
		|| objectType->beh.destruct <= 0
		|| static_cast<asUINT>(objectType->beh.destruct)
			>= engine->scriptFunctions.GetLength() )
	{
		return asASTDeclId();
	}

	const asCDecl* const cls = context.GetDecl(classDecl);
	for( asUINT childIndex = 0;
		cls != 0 && childIndex < cls->children.GetLength(); ++childIndex )
	{
		const asCDecl* const child = context.GetDecl(cls->children[childIndex]);
		if( child != 0 && child->kind == asAST_DECL_DESTRUCTOR )
		{
			return child->id;
		}
	}

	asCScriptFunction* destructor =
		engine->scriptFunctions[objectType->beh.destruct];
	if( destructor == 0 )
	{
		return asASTDeclId();
	}
	if( objectType->templateBaseType != 0
		&& destructor->objectType == objectType->templateBaseType )
	{
		destructor = engine->GenerateTemplateFunction(objectType, destructor);
	}
	if( destructor == 0 || destructor->objectType != objectType )
	{
		return asASTDeclId();
	}

	const asASTDeclId declaration = sema.ActOnDestructorDecl(
		classDecl,
		objectType->name.AddressOf(),
		asCSourceRange());
	if( declaration.IsValid() )
	{
		context.AddDeclTrait(declaration, asAST_TRAIT_EXTERNAL);
		context.SetOrigin(declaration, "canonical-native-destructor");
	}
	return declaration;
}

void asCSema::InternNativeCallablesForType(const asCQualType& type)
{
	if( engine == 0 )
	{
		return;
	}
	asCRuntimeTypeBridge bridge(engine);
	const asCDataType dataType = bridge.Resolve(context, type);
	asCObjectType* const objectType = CastToObjectType(dataType.GetTypeInfo());
	if( objectType == 0 )
	{
		return;
	}
	asASTDeclId classDecl = FindNamedTypeDecl(context, type);
	if( classDecl.IsValid() && IsAuthoredScriptClass(context, classDecl) )
	{
		return;
	}
	if( !classDecl.IsValid() )
	{
		const asCType* named = context.GetType(type.type);
		const char* key = named && named->stableKey.GetLength()
			? named->stableKey.AddressOf()
			: objectType->name.AddressOf();
		classDecl = ActOnClassDecl(context.GetTranslationUnit(), key, asCSourceRange());
		FinishDecl(classDecl);
	}
	SyncImportedNativeClassKind(context, classDecl, dataType);
	InternNativeBehaviourList(*this, engine, context, type, objectType, classDecl, objectType->beh.constructors, false, false);
	InternNativeBehaviourList(*this, engine, context, type, objectType, classDecl, objectType->beh.factories, true, false);
	InternNativeDestructor(*this, engine, context, objectType, classDecl);
}

void asCSema::InternNativeZeroArgCallablesForType(const asCQualType& type)
{
	if( engine == 0 )
	{
		return;
	}
	asCRuntimeTypeBridge bridge(engine);
	const asCDataType dataType = bridge.Resolve(context, type);
	asCObjectType* const objectType = CastToObjectType(dataType.GetTypeInfo());
	if( objectType == 0 )
	{
		return;
	}
	asASTDeclId classDecl = FindNamedTypeDecl(context, type);
	if( classDecl.IsValid() && IsAuthoredScriptClass(context, classDecl) )
	{
		return;
	}
	if( !classDecl.IsValid() )
	{
		const asCType* named = context.GetType(type.type);
		const char* key = named && named->stableKey.GetLength()
			? named->stableKey.AddressOf()
			: objectType->name.AddressOf();
		classDecl = ActOnClassDecl(context.GetTranslationUnit(), key, asCSourceRange());
		FinishDecl(classDecl);
	}
	SyncImportedNativeClassKind(context, classDecl, dataType);
	InternNativeBehaviourList(*this, engine, context, type, objectType, classDecl, objectType->beh.constructors, false, true);
	InternNativeDestructor(*this, engine, context, objectType, classDecl);
	if( objectType->beh.factory > 0 )
	{
		asCArray<int> defaultFactory;
		defaultFactory.PushLast(objectType->beh.factory);
		InternNativeBehaviourList(*this, engine, context, type, objectType, classDecl, defaultFactory, true, true);
	}
	else
	{
		InternNativeBehaviourList(*this, engine, context, type, objectType, classDecl, objectType->beh.factories, true, true);
	}
}

asASTDeclId asCSema::InternNativeListFactoryForType(const asCQualType& type)
{
	if( engine == 0 )
	{
		return asASTDeclId();
	}
	asCRuntimeTypeBridge bridge(engine);
	const asCDataType dataType = bridge.Resolve(context, type);
	asCObjectType* const objectType = CastToObjectType(dataType.GetTypeInfo());
	if( objectType == 0 || objectType->beh.listFactory <= 0 )
	{
		return asASTDeclId();
	}
	asASTDeclId classDecl = FindNamedTypeDecl(context, type);
	if( classDecl.IsValid() && IsAuthoredScriptClass(context, classDecl) )
	{
		return asASTDeclId();
	}
	if( !classDecl.IsValid() )
	{
		const asCType* named = context.GetType(type.type);
		const char* key = named && named->stableKey.GetLength()
			? named->stableKey.AddressOf()
			: objectType->name.AddressOf();
		classDecl = ActOnClassDecl(context.GetTranslationUnit(), key, asCSourceRange());
		FinishDecl(classDecl);
	}
	SyncImportedNativeClassKind(context, classDecl, dataType);
	asCArray<int> listFactory;
	listFactory.PushLast(objectType->beh.listFactory);
	return InternNativeBehaviourList(
		*this,
		engine,
		context,
		type,
		objectType,
		classDecl,
		listFactory,
		true,
		false,
		asAST_TRAIT_LIST_FACTORY);
}

static asASTDeclId ResolveCallee(asCSema& sema, asCASTContext& context, asASTDeclId searchOwner, asASTExprId implicitReceiver, const char* name, const asCArray<asASTExprId>& args, bool exactScope = false)
{
	if( implicitReceiver.IsValid() )
	{
		if( const asCExpr* recv = context.GetExpr(implicitReceiver) )
		{
			if( asCScriptEngine* engine = sema.GetEngine() )
			{
				InternNativeMethods(sema, engine, context, recv->type, name);
			}
			const asASTDeclId typeDecl = FindNamedTypeDecl(context, recv->type);
			if( typeDecl.IsValid() )
			{
				asCArray<asASTExprId> withoutReceiver;
				for( asUINT i = 1; i < args.GetLength(); ++i )
				{
					withoutReceiver.PushLast(args[i]);
				}
				const asASTDeclId method = FindBestCallee(context, typeDecl, name, withoutReceiver, &sema, &recv->type);
				if( method.IsValid() )
				{
					return method;
				}
				if( const asCDecl* cls = context.GetDecl(typeDecl) )
				{
					for( asUINT c = 0; c < cls->children.GetLength(); ++c )
					{
						const asCDecl* child = context.GetDecl(cls->children[c]);
						if( child && child->kind == asAST_DECL_METHOD && child->name.Equals(name)
							&& child->range.begin.fileID == 0
							&& CountParams(context, child) == withoutReceiver.GetLength() )
						{
							return child->id;
						}
					}
				}
				// Member miss is fail-closed for script methods. Fall through
				// with the full arg list (receiver included) so mixin
				// MixHelper(T, int) can bind v.MixHelper(3). Free Get(int)
				// cannot, because arity includes the receiver.
			}
		}
	}
	if( asCScriptEngine* engine = sema.GetEngine() )
	{
		InternNativeGlobals(sema, engine, context, name);
	}
	const asASTDeclId direct = FindBestCallee(
		context, searchOwner, name, args, &sema, 0, exactScope);
	if( direct.IsValid() )
	{
		return direct;
	}
	return FindFuncdefHandle(sema, context, searchOwner, name, exactScope);
}

static asCQualType QualTypeFromClassDecl(asCASTContext& context, const asCDecl* classDecl)
{
	const bool isValue = classDecl && (classDecl->traits & asAST_TRAIT_VALUE) != 0;
	const asEASTTypeKind kind = isValue ? asAST_TYPE_VALUE_OBJECT : asAST_TYPE_REFERENCE_OBJECT;
	const asDWORD quals = isValue ? 0 : (asAST_QUAL_HANDLE | asAST_QUAL_AUTO_HANDLE);
	if( classDecl && classDecl->type.IsValid() )
	{
		if( const asCType* named = context.GetType(classDecl->type.type) )
		{
			if( named->kind == kind )
			{
				return asCQualType(named->id, quals);
			}
		}
	}
	const char* name = (classDecl && classDecl->name.GetLength()) ? classDecl->name.AddressOf() : "";
	return context.InternNamedType(kind, name, quals);
}

static const asCDecl* FindEnclosingClass(asCASTContext& context, asASTDeclId owner)
{
	asASTDeclId current = owner;
	while( current.IsValid() )
	{
		const asCDecl* const decl = context.GetDecl(current);
		if( decl == 0 )
		{
			return 0;
		}
		if( decl->kind == asAST_DECL_CLASS || decl->kind == asAST_DECL_INTERFACE )
		{
			return decl;
		}
		current = decl->parent;
	}
	return 0;
}

static bool FindNamedEnumConversionType(asCSema& sema,
	asCASTContext& context,
	asASTDeclId owner,
	const char* name,
	asCQualType& outType,
	bool exactScope)
{
	if( name == 0 || name[0] == 0 )
	{
		return false;
	}
	asCArray<asASTDeclId> candidates;
	if( exactScope )
	{
		sema.LookupInScope(owner, name, candidates);
	}
	else
	{
		sema.LookupCandidatesFrom(owner, name, candidates);
	}
	const asCDecl* enumDecl = 0;
	for( asUINT i = 0; i < candidates.GetLength(); ++i )
	{
		const asCDecl* candidate = context.GetDecl(candidates[i]);
		if( candidate == 0 || candidate->kind != asAST_DECL_ENUM )
		{
			continue;
		}
		// A functional cast is a type-id expression, not an overload call.
		// Refuse an ambiguous type lookup rather than picking whichever enum was
		// interned first; successful source compilation must have one authority.
		if( enumDecl != 0 && enumDecl->id != candidate->id )
		{
			return false;
		}
		enumDecl = candidate;
	}
	if( enumDecl == 0 )
	{
		return false;
	}
	const char* const stableKey = enumDecl->stableKey.GetLength() != 0
		? enumDecl->stableKey.AddressOf()
		: enumDecl->name.AddressOf();
	outType = context.InternNamedType(asAST_TYPE_ENUM, stableKey, 0);
	return outType.IsValid();
}

static asASTExprId FindExistingExactConversion(asCASTContext& context,
	const asCSourceRange& range,
	const asCQualType& destination,
	asASTExprId source)
{
	for( asUINT i = 1; i <= context.GetExprCount(); ++i )
	{
		const asCExpr* existing = context.GetExpr(asASTExprId(i));
		if( existing == 0 || existing->kind != asAST_EXPR_CONVERSION
			|| existing->type.type != destination.type
			|| existing->type.quals != destination.quals
			|| existing->children.GetLength() != 1
			|| existing->children[0] != source
			|| existing->range.begin.fileID != range.begin.fileID
			|| existing->range.begin.offset != range.begin.offset
			|| existing->range.end.fileID != range.end.fileID
			|| existing->range.end.offset != range.end.offset )
		{
			continue;
		}
		return existing->id;
	}
	return asASTExprId();
}

static asASTExprId ConstructFromCallee(asCSema& sema, asCASTContext& context, asASTDeclId callee, const asCArray<asASTExprId>& args, const asCSourceRange& range)
{
	const asCDecl* decl = context.GetDecl(callee);
	if( decl == 0 )
	{
		return asASTExprId();
	}
	const asCDecl* classDecl = decl;
	if( decl->kind == asAST_DECL_CONSTRUCTOR )
	{
		classDecl = context.GetDecl(decl->parent);
	}
	if( classDecl == 0 || classDecl->kind != asAST_DECL_CLASS )
	{
		return asASTExprId();
	}
	const asCQualType type = QualTypeFromClassDecl(context, classDecl);
	return sema.ActOnConstruct(type, args, range);
}

static bool QualTypesMatch(const asCQualType& left, const asCQualType& right);
static void ArrangeCallArguments(asCSema& sema, asCASTContext& context,
	asASTDeclId callee, asCArray<asASTExprId>& args,
	asCArray<asCString>& names, const asCSourceRange& range,
	asASTExprId implicitReceiver);
static void ConvertCallArgumentsToFormalTypes(asCSema& sema, asCASTContext& context,
	const asCDecl& callee, asCArray<asASTExprId>& args,
	const asCSourceRange& range, asASTExprId implicitReceiver);

asASTExprId asCSema::ActOnCallExpr(asASTDeclId owner, const char* name, const asCArray<asASTExprId>& inArgs, const asCSourceRange& range, asASTExprId implicitReceiver, const asCArray<asCString>* argumentNames, bool exactScope)
{
	asCArray<asASTExprId> args;
	if( implicitReceiver.IsValid() )
	{
		const bool alreadyHasReceiver = inArgs.GetLength() && inArgs[0] == implicitReceiver;
		if( !alreadyHasReceiver )
		{
			args.PushLast(implicitReceiver);
		}
	}
	for( asUINT i = 0; i < inArgs.GetLength(); ++i )
	{
		args.PushLast(inArgs[i]);
	}
	if( !implicitReceiver.IsValid() && args.GetLength() == 1 )
	{
		asCQualType enumType;
		if( FindNamedEnumConversionType(
				*this, context, owner, name, enumType, exactScope) )
		{
			const asASTExprId existing = FindExistingExactConversion(
				context, range, enumType, args[0]);
			if( existing.IsValid() )
			{
				return existing;
			}
			return ActOnConversion(args[0], enumType, range);
		}
	}
	const asASTDeclId callee = ResolveCallee(
		*this, context, owner, implicitReceiver, name, args, exactScope);
	if( callee.IsValid() )
	{
		const asCDecl* const calleeDecl = context.GetDecl(callee);
		if( !implicitReceiver.IsValid() && calleeDecl != 0
			&& calleeDecl->kind == asAST_DECL_METHOD )
		{
			const asCDecl* const enclosingClass = FindEnclosingClass(context, owner);
			if( enclosingClass != 0 )
			{
				implicitReceiver = ActOnThisExpr(
					QualTypeFromClassDecl(context, enclosingClass), range);
				asCArray<asASTExprId> withReceiver;
				withReceiver.PushLast(implicitReceiver);
				for( asUINT argIndex = 0; argIndex < args.GetLength(); ++argIndex )
				{
					withReceiver.PushLast(args[argIndex]);
				}
				args = withReceiver;
			}
		}
		asCArray<asCString> names;
		if( argumentNames != 0 )
		{
			names = *argumentNames;
		}
		ArrangeCallArguments(*this, context, callee, args, names, range, implicitReceiver);
	}
	const asASTExprId constructed = ConstructFromCallee(*this, context, callee, args, range);
	if( constructed.IsValid() )
	{
		return constructed;
	}
	if( !callee.IsValid() )
	{
		asCString msg = "unresolved-callee:";
		if( name && name[0] )
		{
			msg += name;
		}
		asCArray<asASTDeclId> unresolvedHits;
		if( exactScope )
		{
			LookupInScope(owner, name ? name : "", unresolvedHits);
		}
		else
		{
			LookupCandidatesFrom(owner, name ? name : "", unresolvedHits);
		}
		asCString extra;
		extra.Format(" nargs=%u hits=%u", args.GetLength(), unresolvedHits.GetLength());
		msg += extra;
		for( asUINT hitIndex = 0; hitIndex < unresolvedHits.GetLength(); ++hitIndex )
		{
			const asCDecl* const hit = context.GetDecl(unresolvedHits[hitIndex]);
			if( hit == 0 )
			{
				continue;
			}
			extra.Format(" hit%u kind=%d params=", hitIndex, (int)hit->kind);
			msg += extra;
			asUINT hitParams = 0;
			for( asUINT childIndex = 0; childIndex < hit->children.GetLength(); ++childIndex )
			{
				const asCDecl* const param = context.GetDecl(hit->children[childIndex]);
				if( param == 0 || param->kind != asAST_DECL_PARAM )
				{
					continue;
				}
				if( hitParams )
				{
					msg += ",";
				}
				if( const asCType* const paramType = context.GetType(param->type.type) )
				{
					msg += paramType->stableKey;
				}
				else
				{
					msg += "<untyped>";
				}
				++hitParams;
			}
			extra.Format("(%u)", hitParams);
			msg += extra;
		}
		for( asUINT argIndex = 0; argIndex < args.GetLength(); ++argIndex )
		{
			extra.Format(" arg%u=", argIndex);
			msg += extra;
			const asCExpr* const arg = context.GetExpr(args[argIndex]);
			if( arg == 0 )
			{
				msg += "<null>";
				continue;
			}
			if( const asCType* const argType = context.GetType(arg->type.type) )
			{
				if( argType->stableKey.GetLength() )
				{
					msg += argType->stableKey;
				}
				else
				{
					msg += "<type>";
				}
			}
			else
			{
				msg += "<untyped>";
			}
			if( const asCDecl* const argDecl = context.GetDecl(arg->resolvedDecl) )
			{
				msg += "/";
				msg += argDecl->name;
			}
			const asASTDeclId typeDecl = FindNamedTypeDecl(context, arg->type);
			const asCDecl* const typeCls = context.GetDecl(typeDecl);
			asUINT implConv = 0;
			if( typeCls )
			{
				for( asUINT childIndex = 0; childIndex < typeCls->children.GetLength(); ++childIndex )
				{
					const asCDecl* const method = context.GetDecl(typeCls->children[childIndex]);
					if( method && method->kind == asAST_DECL_METHOD && method->name.Equals("opImplConv") )
					{
						++implConv;
					}
				}
			}
			extra.Format(" typeDecl=%s authored=%d implConv=%u",
				typeCls && typeCls->name.GetLength() ? typeCls->name.AddressOf() : "<none>",
				typeCls && IsAuthoredScriptClass(context, typeDecl) ? 1 : 0,
				implConv);
			msg += extra;
			if( asCScriptEngine* const resolvedEngine = GetEngine() )
			{
				asCRuntimeTypeBridge bridge(resolvedEngine);
				const asCDataType resolved = bridge.Resolve(context, arg->type);
				const asCTypeInfo* const resolvedInfo = resolved.GetTypeInfo();
				extra.Format(" resolve=%s",
					resolvedInfo && resolvedInfo->GetName() ? resolvedInfo->GetName() : "miss");
				msg += extra;
			}
			asUINT paramCursor = 0;
			const asCDecl* rankParam = 0;
			for( asUINT hitIndex = 0; hitIndex < unresolvedHits.GetLength() && rankParam == 0; ++hitIndex )
			{
				const asCDecl* const hit = context.GetDecl(unresolvedHits[hitIndex]);
				if( hit == 0 )
				{
					continue;
				}
				for( asUINT childIndex = 0; childIndex < hit->children.GetLength(); ++childIndex )
				{
					const asCDecl* const param = context.GetDecl(hit->children[childIndex]);
					if( param == 0 || param->kind != asAST_DECL_PARAM )
					{
						continue;
					}
					if( paramCursor == argIndex )
					{
						rankParam = param;
						break;
					}
					++paramCursor;
				}
			}
			if( rankParam )
			{
				extra.Format(" rank%u=%d", argIndex, RankArgument(context, rankParam->type, arg));
				msg += extra;
			}
		}
		AddDiagnostic(msg.AddressOf(), range);
		const asCQualType errorType = context.InternNamedType(asAST_TYPE_ERROR, "<unresolved>", 0);
		const asASTExprId call = ActOnCall(asASTDeclId(), args, errorType, range);
		if( name && name[0] )
		{
			context.SetLiteral(call, name);
		}
		if( implicitReceiver.IsValid() )
		{
			context.SetExprReceiver(call, implicitReceiver);
		}
		asSDeferredCall deferred;
		deferred.expression = call;
		deferred.owner = owner;
		deferred.name = name ? name : "";
		deferred.arguments = args;
		if( argumentNames != 0 )
		{
			deferred.argumentNames = *argumentNames;
		}
		deferred.implicitReceiver = implicitReceiver;
		deferred.range = range;
		deferred.absoluteScope = false;
		deferred.exactScope = exactScope;
		deferredCalls.PushLast(deferred);
		return call;
	}
	if( const asCDecl* decl = context.GetDecl(callee) )
	{
		ConvertCallArgumentsToFormalTypes(
			*this, context, *decl, args, range, implicitReceiver);
	}
	asCQualType type = context.InternPrimitive(ttInt, 0);
	if( const asCDecl* decl = context.GetDecl(callee) )
	{
		if( !ResolveCallableResultType(*this, context, *decl, type) )
		{
			AddDiagnostic("unresolved-funcdef-signature", range);
			type = context.InternNamedType(asAST_TYPE_ERROR, "<funcdef-signature>", 0);
		}
	}
	const asASTExprId call = ActOnCall(callee, args, type, range);
	if( implicitReceiver.IsValid() )
	{
		context.SetExprReceiver(call, implicitReceiver);
	}
	if( const asCType* named = context.GetType(type.type) )
	{
		if( named && named->kind != asAST_TYPE_PRIMITIVE && named->kind != asAST_TYPE_VOID )
		{
			return ActOnMaterializeTemporary(call, range);
		}
	}
	return call;
}

static bool DiagnosticMatchesUnresolvedCallee(const asCString& text, const asCString& name)
{
	asCString prefix = "unresolved-callee:";
	prefix += name;
	if( !text.StartsWith(prefix.AddressOf()) )
	{
		return false;
	}
	if( text.GetLength() == prefix.GetLength() )
	{
		return true;
	}
	// ActOnCallExpr appends ranking facts (" nargs=… hits=…") after the name.
	// "unresolved-callee:Helper2" must not match name Helper.
	const char next = text.AddressOf()[prefix.GetLength()];
	return next == ' ' || next == '\t';
}

static asASTDeclId ResolveExactDeclRef(
	asCSema& sema,
	asCASTContext& context,
	asASTDeclId scope,
	const char* name)
{
	if( !scope.IsValid() || name == 0 || name[0] == 0 )
	{
		return asASTDeclId();
	}
	asCArray<asASTDeclId> hits;
	sema.LookupInScope(scope, name, hits);
	for( asUINT hitIndex = 0; hitIndex < hits.GetLength(); ++hitIndex )
	{
		const asCDecl* const hit = context.GetDecl(hits[hitIndex]);
		if( hit != 0 && hit->kind == asAST_DECL_VAR )
		{
			return hits[hitIndex];
		}
	}
	return asASTDeclId();
}

static bool IsIntegerLike(const asCType* type);
static const asCType* ResolveTypedefValueType(
	asCASTContext& context, const asCType* nominalType);
static bool IsFloatPrimitive(const asCType* type);
static bool IsFloat64Primitive(const asCType* type);
static eTokenType CommonIntegerToken(const asCType* lhs, const asCType* rhs);
static bool IsComparisonOperator(const char* op);

static bool QualTypeIsPrimitiveNumeric(
	asCASTContext& context,
	const asCQualType& type,
	const asCType*& resolvedType)
{
	resolvedType = type.IsValid()
		? ResolveTypedefValueType(context, context.GetType(type.type))
		: 0;
	return resolvedType != 0
		&& (IsFloatPrimitive(resolvedType) || IsIntegerLike(resolvedType));
}

static void ReconcileDeferredPrimitiveBinaryParents(
	asCSema& sema,
	asCASTContext& context)
{
	// A deferred name first enters the graph as an ERROR-typed recovery child.
	// Binary parents are nevertheless created immediately so source topology is
	// retained. Once the exact name resolves, recompute the built-in primitive
	// arithmetic facts from the AST children themselves. This is a Sema pass;
	// it neither revisits asCScriptNode nor accepts a stale provisional result as
	// CodeGen authority.
	bool changed = true;
	while( changed )
	{
		changed = false;
		const asUINT expressionCount = context.GetExprCount();
		for( asUINT expressionIndex = 1;
			expressionIndex <= expressionCount; ++expressionIndex )
		{
			const asCExpr* const binary = context.GetExpr(
				asASTExprId(expressionIndex));
			if( binary == 0 || binary->kind != asAST_EXPR_BINARY
				|| binary->children.GetLength() != 2 )
			{
				continue;
			}

			asASTExprId lhs = binary->children[0];
			asASTExprId rhs = binary->children[1];
			const asCExpr* lhsExpr = context.GetExpr(lhs);
			const asCExpr* rhsExpr = context.GetExpr(rhs);
			const asCType* lhsType = 0;
			const asCType* rhsType = 0;
			if( lhsExpr == 0 || rhsExpr == 0
				|| !QualTypeIsPrimitiveNumeric(context, lhsExpr->type, lhsType)
				|| !QualTypeIsPrimitiveNumeric(context, rhsExpr->type, rhsType) )
			{
				continue;
			}

			asCQualType commonType;
			if( IsFloatPrimitive(lhsType) || IsFloatPrimitive(rhsType) )
			{
				if( !(IsFloatPrimitive(lhsType) || IsIntegerLike(lhsType))
					|| !(IsFloatPrimitive(rhsType) || IsIntegerLike(rhsType)) )
				{
					continue;
				}
				commonType = context.InternPrimitive(
					(IsFloat64Primitive(lhsType) || IsFloat64Primitive(rhsType))
						? ttFloat64 : ttFloat32,
					0);
			}
			else
			{
				commonType = context.InternPrimitive(
					CommonIntegerToken(lhsType, rhsType), 0);
			}

			const asCQualType resultType = IsComparisonOperator(
				binary->literal.AddressOf())
				? context.InternPrimitive(ttBool, 0)
				: commonType;
			bool childrenChanged = false;
			if( lhsExpr->type.type != commonType.type )
			{
				lhs = sema.ActOnConversion(lhs, commonType, binary->range);
				childrenChanged = true;
			}
			if( rhsExpr->type.type != commonType.type )
			{
				rhs = sema.ActOnConversion(rhs, commonType, binary->range);
				childrenChanged = true;
			}
			const bool typeChanged = binary->type != resultType;
			if( !childrenChanged && !typeChanged )
			{
				continue;
			}

			const asASTExprId binaryId = binary->id;
			if( childrenChanged )
			{
				context.ClearExprChildren(binaryId);
				context.AddExprChild(binaryId, lhs);
				context.AddExprChild(binaryId, rhs);
			}
			context.SetExprType(binaryId, resultType);
			changed = true;
		}
	}

	// Return conversion and similar recovery wrappers may have been created from
	// the provisional binary type. Disconnect only conversions that are now
	// exact type no-ops; keeping their arena nodes is harmless, but reachable
	// semantic traversal must consume the recomputed binary directly.
	const asUINT expressionCount = context.GetExprCount();
	for( asUINT expressionIndex = 1;
		expressionIndex <= expressionCount; ++expressionIndex )
	{
		const asCExpr* const conversion = context.GetExpr(
			asASTExprId(expressionIndex));
		if( conversion == 0 || conversion->kind != asAST_EXPR_CONVERSION
			|| conversion->children.GetLength() != 1 )
		{
			continue;
		}
		const asCExpr* const child = context.GetExpr(conversion->children[0]);
		if( child != 0 && child->kind == asAST_EXPR_BINARY
			&& child->type == conversion->type )
		{
			context.ReplaceExprUses(conversion->id, child->id);
		}
	}
}

void asCSema::ResolveDeferredNames()
{
	asCArray<asSDeferredCall> unresolved;
	for( asUINT deferredIndex = 0; deferredIndex < deferredCalls.GetLength(); ++deferredIndex )
	{
		asSDeferredCall& deferred = deferredCalls[deferredIndex];
		const asCExpr* existing = context.GetExpr(deferred.expression);
		if( existing == 0 || existing->kind != asAST_EXPR_CALL || existing->resolvedDecl.IsValid() )
		{
			continue;
		}

		asCArray<asASTExprId> args = deferred.arguments;
		asASTDeclId lookupOwner = deferred.owner;
		if( deferred.scopeSegments.GetLength() != 0 || deferred.absoluteScope )
		{
			asCString unresolvedScope;
			lookupOwner = ResolveScopeSegments(
				this, deferred.owner, deferred.scopeSegments,
				deferred.absoluteScope, &unresolvedScope);
			if( !lookupOwner.IsValid() )
			{
				if( !unresolvedScope.Equals(deferred.unresolvedScopeSegment) )
				{
					for( asUINT diagnosticIndex = diagnostics.GetLength(); diagnosticIndex > 0; --diagnosticIndex )
					{
						const asUINT index = diagnosticIndex - 1;
						if( diagnostics[index].StartsWith("unresolved-scope:")
							&& index < diagnosticRanges.GetLength()
							&& diagnosticRanges[index].begin.fileID == deferred.range.begin.fileID
							&& diagnosticRanges[index].begin.offset == deferred.range.begin.offset
							&& diagnosticRanges[index].end.fileID == deferred.range.end.fileID
							&& diagnosticRanges[index].end.offset == deferred.range.end.offset )
						{
							diagnostics.RemoveIndex(index);
							diagnosticRanges.RemoveIndex(index);
						}
					}
					asCString msg = "unresolved-scope:";
					msg += unresolvedScope;
					AddDiagnostic(msg.AddressOf(), deferred.range);
					deferred.unresolvedScopeSegment = unresolvedScope;
				}
				unresolved.PushLast(deferred);
				continue;
			}
		}
		const bool exactScope = deferred.exactScope
			|| deferred.scopeSegments.GetLength() != 0
			|| deferred.absoluteScope;
		const asASTDeclId callee = ResolveCallee(
			*this, context, lookupOwner, deferred.implicitReceiver,
			deferred.name.AddressOf(), args, exactScope);
		const asCDecl* const decl = context.GetDecl(callee);
		if( decl == 0
			|| (decl->kind != asAST_DECL_FUNCTION
				&& decl->kind != asAST_DECL_METHOD
				&& decl->kind != asAST_DECL_MIXIN
				&& decl->kind != asAST_DECL_IMPORT) )
		{
			// Declarations from later script sections are not visible yet. Keep
			// the original call and source facts alive so the next section (or
			// the final builder pass) can bind it. Seal remains fail-closed if no
			// later declaration ever satisfies the call.
			unresolved.PushLast(deferred);
			continue;
		}

		asASTExprId effectiveReceiver = deferred.implicitReceiver;
		if( !effectiveReceiver.IsValid() && decl->kind == asAST_DECL_METHOD )
		{
			const asCDecl* const enclosingClass =
				FindEnclosingClass(context, deferred.owner);
			if( enclosingClass != 0 )
			{
				effectiveReceiver = ActOnThisExpr(
					QualTypeFromClassDecl(context, enclosingClass), deferred.range);
				asCArray<asASTExprId> withReceiver;
				withReceiver.PushLast(effectiveReceiver);
				for( asUINT argIndex = 0; argIndex < args.GetLength(); ++argIndex )
				{
					withReceiver.PushLast(args[argIndex]);
				}
				args = withReceiver;
			}
		}

		asCArray<asCString> argumentNames = deferred.argumentNames;
		ArrangeCallArguments(*this, context, callee, args, argumentNames,
			deferred.range, effectiveReceiver);

		ConvertCallArgumentsToFormalTypes(
			*this, context, *decl, args, deferred.range,
			effectiveReceiver);

		context.SetResolvedDecl(deferred.expression, callee);
		context.SetExprType(deferred.expression, decl->type);
		if( existing->callDispatch == asAST_CALL_DISPATCH_NONE )
		{
			context.SetExprCallDispatch(
				deferred.expression,
				exactScope
					? asAST_CALL_DISPATCH_DIRECT
					: ClassifyCallDispatch(callee));
		}
		asCString literal = decl->name;
		literal += ":reverse-formal";
		context.SetLiteral(deferred.expression, literal.AddressOf());
		context.ClearExprChildren(deferred.expression);
		for( asUINT i = args.GetLength(); i > 0; --i )
		{
			context.AddExprChild(deferred.expression, args[i - 1]);
		}
		if( effectiveReceiver.IsValid() )
		{
			context.SetExprReceiver(
				deferred.expression, effectiveReceiver);
		}

		const asCType* const resolvedType = context.GetType(decl->type.type);
		if( resolvedType != 0
			&& (resolvedType->kind == asAST_TYPE_PRIMITIVE
				|| resolvedType->kind == asAST_TYPE_VOID) )
		{
			// Parser-time recovery deliberately gives an unresolved call an ERROR
			// type. Statements parsed before a later section may consequently have
			// wrapped the call in type-dependent lifetime nodes. Once the exact
			// primitive result is known those wrappers are not semantic facts: make
			// them harmless for whole-arena validation, then disconnect every owning
			// edge from the stale recovery nodes.
			bool removedWrapper = true;
			while( removedWrapper )
			{
				removedWrapper = false;
				for( asUINT expressionIndex = 1;
					expressionIndex <= context.GetExprCount(); ++expressionIndex )
				{
					const asCExpr* const wrapper = context.GetExpr(
						asASTExprId(expressionIndex));
					if( wrapper == 0
						|| (wrapper->kind != asAST_EXPR_MATERIALIZE_TEMPORARY
							&& wrapper->kind != asAST_EXPR_CLEANUP)
						|| wrapper->children.GetLength() != 1
						|| wrapper->children[0] != deferred.expression )
					{
						continue;
					}
					const asCType* const wrapperType = context.GetType(
						wrapper->type.type);
					if( wrapperType == 0 || wrapperType->kind != asAST_TYPE_ERROR )
					{
						continue;
					}
					const asASTExprId wrapperId = wrapper->id;
					context.SetExprType(wrapperId, decl->type);
					context.ReplaceExprUses(wrapperId, deferred.expression);
					removedWrapper = true;
					break;
				}
			}
		}

		for( asUINT diagnosticIndex = diagnostics.GetLength(); diagnosticIndex > 0; --diagnosticIndex )
		{
			const asUINT index = diagnosticIndex - 1;
			const bool unresolvedCallee =
				DiagnosticMatchesUnresolvedCallee(diagnostics[index], deferred.name);
			const bool unresolvedScope = diagnostics[index].StartsWith("unresolved-scope:")
				&& index < diagnosticRanges.GetLength()
				&& diagnosticRanges[index].begin.fileID == deferred.range.begin.fileID
				&& diagnosticRanges[index].begin.offset == deferred.range.begin.offset
				&& diagnosticRanges[index].end.fileID == deferred.range.end.fileID
				&& diagnosticRanges[index].end.offset == deferred.range.end.offset;
			if( !unresolvedCallee && !unresolvedScope )
			{
				continue;
			}
			diagnostics.RemoveIndex(index);
			if( index < diagnosticRanges.GetLength() )
			{
				diagnosticRanges.RemoveIndex(index);
			}
		}
	}
	deferredCalls = unresolved;

	// Explicitly-qualified variable references share the same declaration-order
	// problem as calls. Their qualifier is syntax-owned only while parsing; the
	// copied path below is the complete semantic action payload used for later
	// exact resolution. Never retry the identifier from an enclosing lexical
	// scope when that path is missing or incomplete.
	asCArray<asSDeferredDeclRef> unresolvedRefs;
	for( asUINT deferredIndex = 0; deferredIndex < deferredDeclRefs.GetLength(); ++deferredIndex )
	{
		asSDeferredDeclRef& deferred = deferredDeclRefs[deferredIndex];
		const asCExpr* const existing = context.GetExpr(deferred.expression);
		if( existing == 0 || existing->kind != asAST_EXPR_DECL_REF )
		{
			continue;
		}
		if( existing->resolvedDecl.IsValid() )
		{
			continue;
		}

		asCString unresolvedScope;
		const asASTDeclId lookupOwner = ResolveScopeSegments(
			this, deferred.owner, deferred.scopeSegments,
			deferred.absoluteScope, &unresolvedScope);
		if( !lookupOwner.IsValid() )
		{
			if( !unresolvedScope.Equals(deferred.unresolvedScopeSegment) )
			{
				for( asUINT diagnosticIndex = diagnostics.GetLength(); diagnosticIndex > 0; --diagnosticIndex )
				{
					const asUINT index = diagnosticIndex - 1;
					if( index < diagnosticRanges.GetLength()
						&& diagnosticRanges[index].begin.fileID == deferred.range.begin.fileID
						&& diagnosticRanges[index].begin.offset == deferred.range.begin.offset
						&& diagnosticRanges[index].end.fileID == deferred.range.end.fileID
						&& diagnosticRanges[index].end.offset == deferred.range.end.offset
						&& (diagnostics[index].StartsWith("unresolved-scope:")
							|| diagnostics[index].StartsWith("unresolved-identifier:")) )
					{
						diagnostics.RemoveIndex(index);
						diagnosticRanges.RemoveIndex(index);
					}
				}
				asCString msg = "unresolved-scope:";
				msg += unresolvedScope;
				AddDiagnostic(msg.AddressOf(), deferred.range);
				deferred.unresolvedScopeSegment = unresolvedScope;
			}
			unresolvedRefs.PushLast(deferred);
			continue;
		}

		const asASTDeclId target = ResolveExactDeclRef(
			*this, context, lookupOwner, deferred.name.AddressOf());
		const asCDecl* const declaration = context.GetDecl(target);
		if( declaration == 0 )
		{
			bool hasIdentifierDiagnostic = false;
			for( asUINT diagnosticIndex = diagnostics.GetLength(); diagnosticIndex > 0; --diagnosticIndex )
			{
				const asUINT index = diagnosticIndex - 1;
				if( index >= diagnosticRanges.GetLength()
					|| diagnosticRanges[index].begin.fileID != deferred.range.begin.fileID
					|| diagnosticRanges[index].begin.offset != deferred.range.begin.offset
					|| diagnosticRanges[index].end.fileID != deferred.range.end.fileID
					|| diagnosticRanges[index].end.offset != deferred.range.end.offset )
				{
					continue;
				}
				if( diagnostics[index].StartsWith("unresolved-identifier:") )
				{
					hasIdentifierDiagnostic = true;
					continue;
				}
				if( diagnostics[index].StartsWith("unresolved-scope:") )
				{
					diagnostics.RemoveIndex(index);
					diagnosticRanges.RemoveIndex(index);
				}
			}
			if( !hasIdentifierDiagnostic )
			{
				asCString msg;
				msg.Format("unresolved-identifier:%s: '%s' is not declared in the explicit scope",
					deferred.name.AddressOf(), deferred.name.AddressOf());
				AddDiagnostic(msg.AddressOf(), deferred.range);
			}
			deferred.unresolvedScopeSegment = asCString();
			unresolvedRefs.PushLast(deferred);
			continue;
		}

		context.SetResolvedDecl(deferred.expression, target);
		context.SetExprType(deferred.expression, declaration->type);
		context.SetLiteral(deferred.expression, deferred.name.AddressOf());
		for( asUINT diagnosticIndex = diagnostics.GetLength(); diagnosticIndex > 0; --diagnosticIndex )
		{
			const asUINT index = diagnosticIndex - 1;
			if( index < diagnosticRanges.GetLength()
				&& diagnosticRanges[index].begin.fileID == deferred.range.begin.fileID
				&& diagnosticRanges[index].begin.offset == deferred.range.begin.offset
				&& diagnosticRanges[index].end.fileID == deferred.range.end.fileID
				&& diagnosticRanges[index].end.offset == deferred.range.end.offset
				&& (diagnostics[index].StartsWith("unresolved-scope:")
					|| diagnostics[index].StartsWith("unresolved-identifier:")) )
			{
				diagnostics.RemoveIndex(index);
				diagnosticRanges.RemoveIndex(index);
			}
		}
	}
	deferredDeclRefs = unresolvedRefs;
	ReconcileDeferredPrimitiveBinaryParents(*this, context);
}

asASTExprId asCSema::ActOnPostfixCallExpr(asASTDeclId owner, asASTExprId calleeExpr, const asCArray<asASTExprId>& args, const asCSourceRange& range, const asCArray<asCString>* argumentNames)
{
	asCString name;
	asASTExprId implicitReceiver;
	if( const asCExpr* lhs = context.GetExpr(calleeExpr) )
	{
		if( lhs->resolvedDecl.IsValid() )
		{
			if( const asCDecl* callee = context.GetDecl(lhs->resolvedDecl) )
			{
				if( (callee->traits & asAST_TRAIT_LAMBDA) != 0
					&& (callee->kind == asAST_DECL_FUNCTION || callee->kind == asAST_DECL_METHOD) )
				{
					asCArray<asASTExprId> callArgs = args;
					asCArray<asCString> names;
					if( argumentNames != 0 )
					{
						names = *argumentNames;
					}
					ArrangeCallArguments(
						*this, context, callee->id, callArgs, names,
						range, asASTExprId());
					ConvertCallArgumentsToFormalTypes(
						*this, context, *callee, callArgs,
						range, asASTExprId());
					asCQualType type = context.InternPrimitive(ttInt, 0);
					if( callee->type.IsValid() )
					{
						type = callee->type;
					}
					return ActOnCall(callee->id, callArgs, type, range);
				}
			}
		}
		if( lhs->literal.GetLength() )
		{
			name = lhs->literal;
		}
		else if( const asCDecl* decl = context.GetDecl(lhs->resolvedDecl) )
		{
			name = decl->name;
		}
		if( lhs->kind == asAST_EXPR_CALL && lhs->receiver.IsValid() )
		{
			implicitReceiver = lhs->receiver;
		}
		else if( lhs->kind == asAST_EXPR_MEMBER_REF && lhs->children.GetLength() )
		{
			implicitReceiver = lhs->children[0];
		}
	}
	if( name.GetLength() == 0 )
	{
		return asASTExprId();
	}
	return ActOnCallExpr(
		owner, name.AddressOf(), args, range,
		implicitReceiver, argumentNames);
}

asASTExprId asCSema::ActOnSequenceExpr(const asCArray<asASTExprId>& parts, const asCSourceRange& range)
{
	if( parts.GetLength() == 0 )
	{
		return asASTExprId();
	}
	if( parts.GetLength() == 1 )
	{
		return parts[0];
	}
	const asASTExprId existing = FindExistingExpr(context, asAST_EXPR_SEQUENCE, range);
	if( existing.IsValid() )
	{
		const asCExpr* seq = context.GetExpr(existing);
		if( seq && seq->children.GetLength() == parts.GetLength() )
		{
			return existing;
		}
	}
	asCQualType type = context.InternPrimitive(ttInt, 0);
	if( const asCExpr* last = context.GetExpr(parts[parts.GetLength() - 1]) )
	{
		if( last->type.IsValid() )
		{
			type = last->type;
		}
	}
	return ActOnSequence(parts, type, range);
}

static asASTExprId FindExistingExactConversion(
	asCASTContext& context,
	const asCSourceRange& range,
	asASTExprId inner,
	const asCQualType& dest)
{
	if( range.begin.fileID == 0 )
	{
		return asASTExprId();
	}
	for( asUINT i = 1; i <= context.GetExprCount(); ++i )
	{
		const asCExpr* const existing = context.GetExpr(asASTExprId(i));
		if( existing != 0
			&& existing->kind == asAST_EXPR_CONVERSION
			&& existing->type == dest
			&& existing->children.GetLength() == 1
			&& existing->children[0] == inner
			&& existing->range.begin.fileID == range.begin.fileID
			&& existing->range.begin.offset == range.begin.offset
			&& existing->range.end.fileID == range.end.fileID
			&& existing->range.end.offset == range.end.offset )
		{
			return existing->id;
		}
	}
	return asASTExprId();
}

asASTExprId asCSema::ActOnCastExpr(asASTExprId inner, const asCQualType& dest, const asCSourceRange& range)
{
	if( !inner.IsValid() || !dest.IsValid() )
	{
		return asASTExprId();
	}
	const asASTExprId existing = FindExistingExactConversion(
		context, range, inner, dest);
	if( existing.IsValid() )
	{
		return existing;
	}
	return ActOnConversion(inner, dest, range);
}

asASTExprId asCSema::ActOnCastExprAction(
	const asSCastExprAction& action,
	asCScriptCode* script)
{
	if( !PrepareParsedSource(script) )
	{
		return asASTExprId();
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("cast-expression-action-range-invalid", range);
		return asASTExprId();
	}
	if( !action.operand.IsValid()
		|| context.GetExpr(action.operand) == 0 )
	{
		AddDiagnostic("cast-expression-action-operand-invalid", range);
		return asASTExprId();
	}
	if( !action.targetType.IsValid()
		|| context.GetType(action.targetType.type) == 0 )
	{
		AddDiagnostic("cast-expression-action-target-invalid", range);
		return asASTExprId();
	}
	const asASTExprId expression = ActOnCastExpr(
		action.operand, action.targetType, range);
	if( !expression.IsValid() || context.GetExpr(expression) == 0 )
	{
		AddDiagnostic("cast-expression-action-result-invalid", range);
		return asASTExprId();
	}
	return expression;
}

asASTExprId asCSema::ActOnConstructExprAction(
	const asSConstructExprAction& action,
	asCScriptCode* script)
{
	if( !PrepareParsedSource(script) )
	{
		return asASTExprId();
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("construct-expression-action-range-invalid", range);
		return asASTExprId();
	}
	if( !action.targetType.IsValid()
		|| context.GetType(action.targetType.type) == 0 )
	{
		AddDiagnostic("construct-expression-action-target-invalid", range);
		return asASTExprId();
	}
	for( asUINT i = 0; i < action.arguments.GetLength(); ++i )
	{
		if( !action.arguments[i].IsValid()
			|| context.GetExpr(action.arguments[i]) == 0 )
		{
			AddDiagnostic("construct-expression-action-argument-invalid", range);
			return asASTExprId();
		}
	}

	const asCType* const target = context.GetType(action.targetType.type);
	asASTExprId expression;
	if( target != 0
		&& (target->kind == asAST_TYPE_PRIMITIVE
			|| target->kind == asAST_TYPE_ENUM) )
	{
		if( action.arguments.GetLength() != 1 )
		{
			AddDiagnostic("construct-expression-action-scalar-arity-invalid", range);
			return asASTExprId();
		}
		expression = ActOnCastExpr(
			action.arguments[0], action.targetType, range);
	}
	else
	{
		expression = ActOnConstruct(
			action.targetType, action.arguments, range);
	}
	if( !expression.IsValid() || context.GetExpr(expression) == 0 )
	{
		AddDiagnostic("construct-expression-action-result-invalid", range);
		return asASTExprId();
	}
	return expression;
}

asASTExprId asCSema::ActOnInitListExprAction(
	const asSInitListExprAction& action,
	asCScriptCode* script)
{
	if( !PrepareParsedSource(script) )
	{
		return asASTExprId();
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() || action.endOffset <= action.beginOffset )
	{
		AddDiagnostic("initializer-list-expression-action-range-invalid", range);
		return asASTExprId();
	}
	if( action.owner.IsValid() && context.GetDecl(action.owner) == 0 )
	{
		AddDiagnostic("initializer-list-expression-action-owner-invalid", range);
		return asASTExprId();
	}
	if( action.hasExplicitTarget )
	{
		if( !action.targetType.IsValid()
			|| context.GetType(action.targetType.type) == 0 )
		{
			AddDiagnostic("initializer-list-expression-action-target-invalid", range);
			return asASTExprId();
		}
	}
	else if( action.targetType.IsValid() )
	{
		AddDiagnostic("initializer-list-expression-action-target-unexpected", range);
		return asASTExprId();
	}

	asCArray<asASTExprId> elements;
	for( asUINT i = 0; i < action.elements.GetLength(); ++i )
	{
		const asSInitListElementAction& element = action.elements[i];
		const asCSourceRange elementRange =
			context.GetSourceManager().MakeRange(
				parsedFile, element.beginOffset, element.endOffset);
		if( !elementRange.IsValid()
			|| element.endOffset <= element.beginOffset
			|| element.beginOffset < action.beginOffset
			|| element.endOffset > action.endOffset )
		{
			AddDiagnostic(
				"initializer-list-expression-action-element-range-invalid",
				range);
			return asASTExprId();
		}
		if( element.kind == asINIT_LIST_ELEMENT_OMITTED )
		{
			// The native compiler supports default list elements. Canonical AST
			// does not yet have a durable default-element node and CodeGen cannot
			// lower it faithfully, so preserve LEGACY syntax support but fail this
			// action closed instead of dropping an invalid ExprId.
			AddDiagnostic(
				"initializer-list-omitted-element-unsupported",
				elementRange);
			return asASTExprId();
		}
		if( element.kind != asINIT_LIST_ELEMENT_EXPRESSION
			|| !element.expression.IsValid()
			|| context.GetExpr(element.expression) == 0 )
		{
			AddDiagnostic(
				"initializer-list-expression-action-element-invalid",
				elementRange);
			return asASTExprId();
		}
		elements.PushLast(element.expression);
	}

	const asASTExprId expression = ActOnInitList(
		elements,
		range,
		action.hasExplicitTarget ? action.targetType : asCQualType());
	if( !expression.IsValid() || context.GetExpr(expression) == 0 )
	{
		AddDiagnostic("initializer-list-expression-action-result-invalid", range);
		return asASTExprId();
	}
	return expression;
}

static void ResolvePlainAssignmentOperator(
	asCSema& sema,
	asCScriptEngine* engine,
	asCASTContext& context,
	asASTExprId assign)
{
	const asCExpr* const assignExpr = context.GetExpr(assign);
	const asCExpr* const leftExpr = assignExpr
		&& assignExpr->children.GetLength() == 2
		? context.GetExpr(assignExpr->children[0]) : 0;
	if( leftExpr == 0 || !leftExpr->type.IsValid() )
	{
		return;
	}

	const asCType* const leftType = context.GetType(leftExpr->type.type);
	if( leftType == 0
		|| leftType->kind == asAST_TYPE_PRIMITIVE
		|| leftType->kind == asAST_TYPE_ENUM
		|| leftType->kind == asAST_TYPE_VOID )
	{
		return;
	}

	if( engine != 0 )
	{
		InternNativeMethods(sema, engine, context,
			leftExpr->type, "opAssign");
	}
	asCArray<asASTExprId> args;
	args.PushLast(assignExpr->children[1]);
	const asASTDeclId selected = FindBestCallee(
		context,
		FindNamedTypeDecl(context, leftExpr->type),
		"opAssign",
		args,
		&sema,
		&leftExpr->type);
	if( selected.IsValid() )
	{
		context.SetResolvedDecl(assign, selected);
	}
}

asASTExprId asCSema::ActOnGeneratedAssign(
	asASTExprId lhs,
	asASTExprId rhs,
	const asCQualType& type,
	const asCSourceRange& range)
{
	const asASTExprId assign = ActOnAssign(lhs, rhs, type, range);
	if( assign.IsValid() )
	{
		ResolvePlainAssignmentOperator(*this, engine, context, assign);
	}
	return assign;
}

static bool IsAssignmentOperatorText(const asCString& op)
{
	static const char* const supported[] = {
		"=", "+=", "-=", "*=", "/=", "%=", "**=", "&=", "|=", "^=",
		"<<=", ">>=", ">>>="
	};
	for( asUINT i = 0; i < sizeof(supported) / sizeof(supported[0]); ++i )
	{
		if( op.Equals(supported[i]) )
		{
			return true;
		}
	}
	return false;
}

asASTExprId asCSema::ActOnAssignExprAction(
	const asSAssignExprAction& action,
	asCScriptCode* script)
{
	if( !PrepareParsedSource(script) )
	{
		return asASTExprId();
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("assignment-expression-action-range-invalid", range);
		return asASTExprId();
	}
	if( !action.lhs.IsValid() || context.GetExpr(action.lhs) == 0 )
	{
		AddDiagnostic("assignment-expression-action-lhs-invalid", range);
		return asASTExprId();
	}
	if( !action.rhs.IsValid() || context.GetExpr(action.rhs) == 0 )
	{
		AddDiagnostic("assignment-expression-action-rhs-invalid", range);
		return asASTExprId();
	}
	if( !IsAssignmentOperatorText(action.op) )
	{
		AddDiagnostic("assignment-expression-action-operator-invalid", range);
		return asASTExprId();
	}
	return ActOnAssignExpr(action.lhs, action.rhs, range, action.op.AddressOf());
}

static bool BinaryActionOperatorInfo(
	int tokenType,
	const char*& canonicalSpelling,
	int& precedence,
	bool& logical)
{
	logical = false;
	switch( tokenType )
	{
	case ttStarStar:
		canonicalSpelling = "**"; precedence = 0; return true;
	case ttStar:
		canonicalSpelling = "*"; precedence = -1; return true;
	case ttSlash:
		canonicalSpelling = "/"; precedence = -1; return true;
	case ttPercent:
		canonicalSpelling = "%"; precedence = -1; return true;
	case ttPlus:
		canonicalSpelling = "+"; precedence = -2; return true;
	case ttMinus:
		canonicalSpelling = "-"; precedence = -2; return true;
	case ttBitShiftLeft:
		canonicalSpelling = "<<"; precedence = -3; return true;
	case ttBitShiftRight:
		canonicalSpelling = ">>"; precedence = -3; return true;
	case ttBitShiftRightArith:
		canonicalSpelling = ">>>"; precedence = -3; return true;
	case ttAmp:
		canonicalSpelling = "&"; precedence = -4; return true;
	case ttBitXor:
		canonicalSpelling = "^"; precedence = -5; return true;
	case ttBitOr:
		canonicalSpelling = "|"; precedence = -6; return true;
	case ttLessThanOrEqual:
		canonicalSpelling = "<="; precedence = -7; return true;
	case ttLessThan:
		canonicalSpelling = "<"; precedence = -7; return true;
	case ttGreaterThanOrEqual:
		canonicalSpelling = ">="; precedence = -7; return true;
	case ttGreaterThan:
		canonicalSpelling = ">"; precedence = -7; return true;
	case ttEqual:
		canonicalSpelling = "=="; precedence = -8; return true;
	case ttNotEqual:
		canonicalSpelling = "!="; precedence = -8; return true;
	case ttXor:
		canonicalSpelling = "^^"; precedence = -8; logical = true; return true;
	case ttIs:
		canonicalSpelling = "is"; precedence = -8; return true;
	case ttNotIs:
		canonicalSpelling = "!is"; precedence = -8; return true;
	case ttAnd:
		canonicalSpelling = "&&"; precedence = -9; logical = true; return true;
	case ttOr:
		canonicalSpelling = "||"; precedence = -10; logical = true; return true;
	default:
		canonicalSpelling = 0; precedence = 0; return false;
	}
}

static bool BinaryActionSpellingMatches(
	int tokenType,
	const asCString& spelling,
	const char* canonicalSpelling)
{
	if( canonicalSpelling != 0 && spelling.Equals(canonicalSpelling) )
	{
		return true;
	}
	return (tokenType == ttAnd && spelling.Equals("and"))
		|| (tokenType == ttOr && spelling.Equals("or"))
		|| (tokenType == ttXor && spelling.Equals("xor"));
}

static bool ApplyBinaryActionOperator(
	asCSema& sema,
	asCASTContext& context,
	asCArray<asASTExprId>& values,
	const asSBinaryExprOperatorAction& action,
	const asCSourceRange& range)
{
	if( values.GetLength() < 2 )
	{
		return false;
	}
	const char* canonicalSpelling = 0;
	int precedence = 0;
	bool logical = false;
	if( !BinaryActionOperatorInfo(
		action.tokenType, canonicalSpelling, precedence, logical) )
	{
		return false;
	}
	const asASTExprId rhs = values.PopLast();
	const asASTExprId lhs = values.PopLast();
	const asASTExprId folded = logical
		? sema.ActOnLogicalExpr(lhs, canonicalSpelling, rhs, range)
		: sema.ActOnBinaryExpr(lhs, canonicalSpelling, rhs, range);
	if( !folded.IsValid() || context.GetExpr(folded) == 0 )
	{
		return false;
	}
	values.PushLast(folded);
	return true;
}

asASTExprId asCSema::ActOnBinaryExprAction(
	const asSBinaryExprAction& action,
	asCScriptCode* script)
{
	if( !PrepareParsedSource(script) )
	{
		return asASTExprId();
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("binary-expression-action-range-invalid", range);
		return asASTExprId();
	}
	if( action.operands.GetLength() < 2
		|| action.operators.GetLength() + 1 != action.operands.GetLength() )
	{
		AddDiagnostic("binary-expression-action-shape-invalid", range);
		return asASTExprId();
	}
	for( asUINT i = 0; i < action.operands.GetLength(); ++i )
	{
		if( !action.operands[i].IsValid()
			|| context.GetExpr(action.operands[i]) == 0 )
		{
			AddDiagnostic("binary-expression-action-operand-invalid", range);
			return asASTExprId();
		}
	}
	for( asUINT i = 0; i < action.operators.GetLength(); ++i )
	{
		const char* canonicalSpelling = 0;
		int precedence = 0;
		bool logical = false;
		if( !BinaryActionOperatorInfo(
			action.operators[i].tokenType,
			canonicalSpelling,
			precedence,
			logical)
			|| !BinaryActionSpellingMatches(
				action.operators[i].tokenType,
				action.operators[i].spelling,
				canonicalSpelling) )
		{
			AddDiagnostic("binary-expression-action-operator-invalid", range);
			return asASTExprId();
		}
	}

	asCArray<asASTExprId> values;
	asCArray<asSBinaryExprOperatorAction> operators;
	values.PushLast(action.operands[0]);
	for( asUINT i = 0; i < action.operators.GetLength(); ++i )
	{
		const char* currentSpelling = 0;
		int currentPrecedence = 0;
		bool currentLogical = false;
		BinaryActionOperatorInfo(
			action.operators[i].tokenType,
			currentSpelling,
			currentPrecedence,
			currentLogical);
		while( operators.GetLength() > 0 )
		{
			const char* stackedSpelling = 0;
			int stackedPrecedence = 0;
			bool stackedLogical = false;
			BinaryActionOperatorInfo(
				operators[operators.GetLength() - 1].tokenType,
				stackedSpelling,
				stackedPrecedence,
				stackedLogical);
			if( currentPrecedence > stackedPrecedence )
			{
				break;
			}
			const asSBinaryExprOperatorAction stacked = operators.PopLast();
			if( !ApplyBinaryActionOperator(
				*this, context, values, stacked, range) )
			{
				AddDiagnostic("binary-expression-action-fold-invalid", range);
				return asASTExprId();
			}
		}
		operators.PushLast(action.operators[i]);
		values.PushLast(action.operands[i + 1]);
	}
	while( operators.GetLength() > 0 )
	{
		const asSBinaryExprOperatorAction stacked = operators.PopLast();
		if( !ApplyBinaryActionOperator(*this, context, values, stacked, range) )
		{
			AddDiagnostic("binary-expression-action-fold-invalid", range);
			return asASTExprId();
		}
	}
	if( values.GetLength() != 1 )
	{
		AddDiagnostic("binary-expression-action-fold-invalid", range);
		return asASTExprId();
	}
	return values[0];
}

asASTExprId asCSema::ActOnAssignExpr(asASTExprId lhs, asASTExprId rhs, const asCSourceRange& range, const char* op)
{
	if( op == 0 || op[0] == 0 )
	{
		op = "=";
	}
	const asASTExprId existingSeq = FindExistingExpr(context, asAST_EXPR_SEQUENCE, range);
	if( existingSeq.IsValid() )
	{
		if( const asCExpr* seq = context.GetExpr(existingSeq) )
		{
			if( seq->literal.Equals("opaque") )
			{
				return existingSeq;
			}
		}
	}
	const bool addAssign = op[0] == '+' && op[1] == '=' && op[2] == 0;
	if( !addAssign )
	{
		const asASTExprId existing = FindExistingExpr(context, asAST_EXPR_ASSIGN, range);
		if( existing.IsValid() )
		{
			return existing;
		}
	}
	if( addAssign )
	{
		asASTExprId target = lhs;
		for( ;; )
		{
			const asCExpr* wrap = context.GetExpr(target);
			if( wrap == 0 )
			{
				break;
			}
			if( wrap->kind == asAST_EXPR_SEQUENCE && wrap->children.GetLength() > 0 )
			{
				target = wrap->children[wrap->children.GetLength() - 1];
				continue;
			}
			if( (wrap->kind == asAST_EXPR_MATERIALIZE_TEMPORARY || wrap->kind == asAST_EXPR_CLEANUP)
				&& wrap->children.GetLength() > 0 )
			{
				target = wrap->children[0];
				continue;
			}
			break;
		}
		if( const asCExpr* left = context.GetExpr(target) )
		{
			if( left->kind == asAST_EXPR_INDEX && left->children.GetLength() >= 2 )
			{
				const asASTExprId base = left->children[0];
				const asASTExprId index = left->children[1];
				asCSourceRange ovRange = range;
				if( const asCExpr* baseExpr = context.GetExpr(base) )
				{
					ovRange = baseExpr->range;
				}
				asASTExprId ov = FindExistingExpr(context, asAST_EXPR_OPAQUE_VALUE, ovRange);
				if( !ov.IsValid() )
				{
					ov = ActOnOpaqueValueExpr(base, ovRange);
				}
				asCQualType idxType = left->type.IsValid() ? left->type : context.InternPrimitive(ttInt, 0);
				const asASTExprId wrappedIndex = ActOnIndex(ov, index, idxType, left->range);
				if( left->resolvedDecl.IsValid() )
				{
					context.SetResolvedDecl(wrappedIndex, left->resolvedDecl);
				}
				const asASTExprId add = ActOnBinaryExpr(wrappedIndex, "+", rhs, rhs.IsValid() && context.GetExpr(rhs) ? context.GetExpr(rhs)->range : range);
				const asASTExprId write = ActOnAssign(wrappedIndex, add, idxType, range);
				asCArray<asASTExprId> parts;
				parts.PushLast(ov);
				parts.PushLast(wrappedIndex);
				parts.PushLast(add);
				parts.PushLast(write);
				const asASTExprId seq = ActOnSequenceExpr(parts, range);
				if( seq.IsValid() )
				{
					context.SetLiteral(seq, "opaque");
				}
				return seq;
			}
			if( left->kind == asAST_EXPR_CALL && left->resolvedDecl.IsValid() && left->children.GetLength() > 0 )
			{
				const asCDecl* getter = context.GetDecl(left->resolvedDecl);
				if( getter
					&& getter->name.GetLength() > 3
					&& std::strncmp(getter->name.AddressOf(), "Get", 3) == 0 )
				{
					const asASTExprId receiver = left->children[left->children.GetLength() - 1];
					asCSourceRange ovRange = range;
					if( const asCExpr* recv = context.GetExpr(receiver) )
					{
						ovRange = recv->range;
					}
					asASTExprId ov = FindExistingExpr(context, asAST_EXPR_OPAQUE_VALUE, ovRange);
					if( !ov.IsValid() )
					{
						ov = ActOnOpaqueValueExpr(receiver, ovRange);
					}
					asCArray<asASTExprId> getArgs;
					getArgs.PushLast(ov);
					asCQualType getType = left->type.IsValid() ? left->type : context.InternPrimitive(ttInt, 0);
					const asASTExprId get = ActOnCall(left->resolvedDecl, getArgs, getType, left->range);
					context.SetExprReceiver(get, ov);
					const asASTExprId add = ActOnBinaryExpr(get, "+", rhs, rhs.IsValid() && context.GetExpr(rhs) ? context.GetExpr(rhs)->range : range);
					asCQualType fallback = context.InternPrimitive(ttInt, 0);
					const asASTExprId set = TryRewritePropertySet(*this, context, get, add, fallback, range);
					asCArray<asASTExprId> parts;
					parts.PushLast(ov);
					parts.PushLast(get);
					parts.PushLast(add);
					if( set.IsValid() )
					{
						parts.PushLast(set);
					}
					const asASTExprId seq = ActOnSequenceExpr(parts, range);
					if( seq.IsValid() )
					{
						context.SetLiteral(seq, "opaque");
					}
					return seq;
				}
			}
		}
	}
	asCQualType fallback = context.InternPrimitive(ttInt, 0);
	const asASTExprId rewritten = TryRewritePropertySet(*this, context, lhs, rhs, fallback, range);
	if( rewritten.IsValid() )
	{
		return rewritten;
	}
	if( !lhs.IsValid() )
	{
		return asASTExprId();
	}
	const asASTExprId assign = ActOnAssign(lhs, rhs, fallback, range);
	if( assign.IsValid() && op[0] == '=' && op[1] == 0 )
	{
		// Overloaded value assignment is a Sema decision, not a storage-width
		// decision for Bytecode CodeGen. Project the current Runtime method set,
		// rank the exact rhs against opAssign, and retain the selected declaration
		// on the sealed Assign node. A missing/ambiguous operator deliberately
		// remains unresolved so non-trivial consumers fail closed instead of
		// guessing by method name or raw-copying application storage.
		ResolvePlainAssignmentOperator(*this, engine, context, assign);
	}
	else if( assign.IsValid() )
	{
		// Keep the selected source mutation operator as an immutable semantic
		// fact.  A backend must not infer +=/-=/*= from parser nodes, and it
		// must not silently turn a compound mutation into a plain store.
		context.SetLiteral(assign, op);
	}
	return assign;
}

asASTExprId asCSema::ActOnMemberExpr(asASTExprId base, const char* name, const asCSourceRange& range)
{
	const asCQualType intType = context.InternPrimitive(ttInt, 0);
	const asASTExprId rewritten = TryRewritePropertyGet(*this, context, base, name, intType, range);
	if( rewritten.IsValid() )
	{
		return rewritten;
	}
	const asCExpr* baseExpr = context.GetExpr(base);
	if( baseExpr && engine )
	{
		InternNativeProperties(*this, engine, context, baseExpr->type);
	}
	asCQualType fieldType = intType;
	asASTDeclId member;
	if( baseExpr )
	{
		member = FindTypeMemberVar(context, FindNamedTypeDecl(context, baseExpr->type), name);
		if( const asCDecl* field = context.GetDecl(member) )
		{
			if( field->type.IsValid() )
			{
				fieldType = field->type;
			}
		}
	}
	const asASTExprId id = ActOnMemberRef(base, name ? name : "", fieldType, range);
	if( member.IsValid() )
	{
		context.SetResolvedDecl(id, member);
	}
	return id;
}

static asASTExprId MaterializeOwningIndexResult(
	asCSema& sema,
	asCASTContext& context,
	asASTExprId indexExpr,
	const asCSourceRange& range)
{
	const asCExpr* expr = context.GetExpr(indexExpr);
	if( expr == 0 || (expr->type.quals & asAST_QUAL_HANDLE) == 0 )
	{
		return indexExpr;
	}
	for( asUINT i = 1; i <= context.GetExprCount(); ++i )
	{
		const asCExpr* existing = context.GetExpr(asASTExprId(i));
		if( existing
			&& existing->kind == asAST_EXPR_MATERIALIZE_TEMPORARY
			&& existing->children.GetLength() == 1
			&& existing->children[0] == indexExpr )
		{
			return existing->id;
		}
	}
	// A handle-returning opIndex is an owning prvalue just like a normal
	// handle-returning call. Make that lifetime explicit so a following member
	// or call reuses the same result instead of executing opIndex twice.
	return sema.ActOnMaterializeTemporary(indexExpr, range);
}

asASTExprId asCSema::ActOnIndexExpr(asASTExprId base, asASTExprId index, const asCSourceRange& range)
{
	asCArray<asASTExprId> indices;
	indices.PushLast(index);
	return ActOnIndexExpr(base, indices, range, 0);
}

static asASTExprId FindExistingExactIndex(
	asCASTContext& context,
	asASTExprId base,
	const asCArray<asASTExprId>& indices,
	const asCSourceRange& range)
{
	for( asUINT expressionIndex = 1;
		expressionIndex <= context.GetExprCount(); ++expressionIndex )
	{
		const asCExpr* const existing = context.GetExpr(
			asASTExprId(expressionIndex));
		if( existing == 0 || existing->kind != asAST_EXPR_INDEX
			|| existing->range.begin.fileID != range.begin.fileID
			|| existing->range.begin.offset != range.begin.offset
			|| existing->range.end.fileID != range.end.fileID
			|| existing->range.end.offset != range.end.offset
			|| existing->children.GetLength() != indices.GetLength() + 1
			|| existing->children[0] != base )
		{
			continue;
		}
		bool sameArguments = true;
		for( asUINT index = 0; index < indices.GetLength(); ++index )
		{
			if( existing->children[index + 1] != indices[index] )
			{
				sameArguments = false;
				break;
			}
		}
		if( sameArguments )
		{
			return existing->id;
		}
	}
	return asASTExprId();
}

asASTExprId asCSema::ActOnIndexExpr(
	asASTExprId base,
	const asCArray<asASTExprId>& inIndices,
	const asCSourceRange& range,
	const asCArray<asCString>* argumentNames)
{
	if( !base.IsValid() || context.GetExpr(base) == 0
		|| inIndices.GetLength() == 0 )
	{
		return asASTExprId();
	}
	asCArray<asASTExprId> indices = inIndices;
	for( asUINT index = 0; index < indices.GetLength(); ++index )
	{
		if( !indices[index].IsValid()
			|| context.GetExpr(indices[index]) == 0 )
		{
			return asASTExprId();
		}
	}

	const asCQualType intType = context.InternPrimitive(ttInt, 0);
	const asASTDeclId opIndex = ResolveNativeIndexOp(
		*this, context, base, indices);
	asCQualType resultType = intType;
	if( const asCDecl* method = context.GetDecl(opIndex) )
	{
		asCArray<asCString> names;
		if( argumentNames != 0 )
		{
			names = *argumentNames;
		}
		ArrangeCallArguments(
			*this, context, opIndex, indices, names,
			range, asASTExprId());
		ConvertCallArgumentsToFormalTypes(
			*this, context, *method, indices,
			range, asASTExprId());
		if( method->type.IsValid() )
		{
			resultType = method->type;
		}
	}
	const asASTExprId existing = FindExistingExactIndex(
		context, base, indices, range);
	if( existing.IsValid() )
	{
		if( opIndex.IsValid() )
		{
			context.SetResolvedDecl(existing, opIndex);
			context.SetExprType(existing, resultType);
		}
		return MaterializeOwningIndexResult(
			*this, context, existing, range);
	}
	asASTExprId value = ActOnIndex(base, indices, resultType, range);
	if( opIndex.IsValid() )
	{
		context.SetResolvedDecl(value, opIndex);
	}
	return MaterializeOwningIndexResult(*this, context, value, range);
}

static bool UnaryActionOperatorInfo(
	int tokenType,
	bool postfix,
	const char*& canonicalSpelling)
{
	canonicalSpelling = 0;
	if( postfix )
	{
		if( tokenType == ttInc )
		{
			canonicalSpelling = "++";
			return true;
		}
		if( tokenType == ttDec )
		{
			canonicalSpelling = "--";
			return true;
		}
		return false;
	}

	switch( tokenType )
	{
	case ttMinus:  canonicalSpelling = "-";  return true;
	case ttPlus:   canonicalSpelling = "+";  return true;
	case ttNot:    canonicalSpelling = "!";  return true;
	case ttInc:    canonicalSpelling = "++"; return true;
	case ttDec:    canonicalSpelling = "--"; return true;
	case ttBitNot: canonicalSpelling = "~";  return true;
	case ttHandle: canonicalSpelling = "@";  return true;
	default: return false;
	}
}

static bool UnaryActionOperatorMatches(
	const asSUnaryExprOperatorAction& action,
	bool postfix,
	const char*& canonicalSpelling)
{
	return UnaryActionOperatorInfo(
		action.tokenType, postfix, canonicalSpelling)
		&& action.spelling.Equals(canonicalSpelling);
}

asASTExprId asCSema::ActOnUnaryExprAction(
	const asSUnaryExprAction& action,
	asCScriptCode* script)
{
	if( !PrepareParsedSource(script) )
	{
		return asASTExprId();
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("unary-expression-action-range-invalid", range);
		return asASTExprId();
	}
	if( !action.operand.IsValid()
		|| context.GetExpr(action.operand) == 0 )
	{
		AddDiagnostic("unary-expression-action-operand-invalid", range);
		return asASTExprId();
	}
	if( action.prefixOperators.GetLength() == 0
		&& action.postfixOperators.GetLength() == 0 )
	{
		AddDiagnostic("unary-expression-action-shape-invalid", range);
		return asASTExprId();
	}
	for( asUINT i = 0; i < action.prefixOperators.GetLength(); ++i )
	{
		const char* canonicalSpelling = 0;
		if( !UnaryActionOperatorMatches(
			action.prefixOperators[i], false, canonicalSpelling) )
		{
			AddDiagnostic("unary-expression-action-operator-invalid", range);
			return asASTExprId();
		}
	}
	for( asUINT i = 0; i < action.postfixOperators.GetLength(); ++i )
	{
		const char* canonicalSpelling = 0;
		if( !UnaryActionOperatorMatches(
			action.postfixOperators[i], true, canonicalSpelling) )
		{
			AddDiagnostic("unary-expression-action-operator-invalid", range);
			return asASTExprId();
		}
	}

	asASTExprId value = action.operand;
	for( asUINT i = 0; i < action.postfixOperators.GetLength(); ++i )
	{
		const char* canonicalSpelling = 0;
		UnaryActionOperatorInfo(
			action.postfixOperators[i].tokenType, true, canonicalSpelling);
		value = ActOnUnaryExpr(canonicalSpelling, value, range, true);
		if( !value.IsValid() || context.GetExpr(value) == 0 )
		{
			AddDiagnostic("unary-expression-action-fold-invalid", range);
			return asASTExprId();
		}
	}
	for( asUINT i = action.prefixOperators.GetLength(); i > 0; --i )
	{
		const char* canonicalSpelling = 0;
		UnaryActionOperatorInfo(
			action.prefixOperators[i - 1].tokenType,
			false,
			canonicalSpelling);
		value = ActOnUnaryExpr(canonicalSpelling, value, range, false);
		if( !value.IsValid() || context.GetExpr(value) == 0 )
		{
			AddDiagnostic("unary-expression-action-fold-invalid", range);
			return asASTExprId();
		}
	}
	return value;
}

static bool UnaryExprRangeMatches(
	const asCExpr* expression,
	const asCSourceRange& range)
{
	return expression != 0
		&& range.begin.fileID != 0
		&& expression->range.begin.fileID == range.begin.fileID
		&& expression->range.begin.offset == range.begin.offset
		&& expression->range.end.fileID == range.end.fileID
		&& expression->range.end.offset == range.end.offset;
}

static asASTExprId FindExistingExactUnary(
	asCASTContext& context,
	const asCSourceRange& range,
	const char* canonicalOp,
	asASTExprId inner)
{
	for( asUINT i = 1; i <= context.GetExprCount(); ++i )
	{
		const asCExpr* const existing = context.GetExpr(asASTExprId(i));
		if( UnaryExprRangeMatches(existing, range)
			&& existing->kind == asAST_EXPR_UNARY
			&& existing->literal.Equals(canonicalOp ? canonicalOp : "")
			&& existing->children.GetLength() == 1
			&& existing->children[0] == inner )
		{
			return existing->id;
		}
	}
	return asASTExprId();
}

static asASTExprId FindExistingExactUnaryCall(
	asCASTContext& context,
	const asCSourceRange& range,
	asASTDeclId callee,
	asASTExprId inner)
{
	for( asUINT i = 1; i <= context.GetExprCount(); ++i )
	{
		const asCExpr* const existing = context.GetExpr(asASTExprId(i));
		if( UnaryExprRangeMatches(existing, range)
			&& existing->kind == asAST_EXPR_CALL
			&& existing->resolvedDecl == callee
			&& existing->receiver == inner
			&& existing->children.GetLength() == 1
			&& existing->children[0] == inner )
		{
			return existing->id;
		}
	}
	return asASTExprId();
}

asASTExprId asCSema::ActOnUnaryExpr(const char* op, asASTExprId inner, const asCSourceRange& range, bool postfix)
{
	const asCQualType intType = context.InternPrimitive(ttInt, 0);
	const char* methodName = 0;
	if( op && op[0] == '-' && op[1] == 0 )
	{
		methodName = "opNeg";
	}
	else if( op && op[0] == '+' && op[1] == '+' && op[2] == 0 )
	{
		methodName = postfix ? "opPostInc" : "opPreInc";
	}
	else if( op && op[0] == '-' && op[1] == '-' && op[2] == 0 )
	{
		methodName = postfix ? "opPostDec" : "opPreDec";
	}
	if( methodName )
	{
		if( const asCExpr* innerExpr = context.GetExpr(inner) )
		{
			if( engine != 0 )
			{
				// Registered VALUE operators are lazy semantic projections just like
				// ordinary native methods.  Looking only at the current AST class view
				// makes a native opNeg/opInc silently fall back to a builtin unary node.
				InternNativeMethods(*this, engine, context,
					innerExpr->type, methodName);
			}
			const asASTDeclId typeDecl = FindNamedTypeDecl(context, innerExpr->type);
			asCArray<asASTExprId> opArgs;
			const asASTDeclId callee = FindBestCallee(context, typeDecl, methodName, opArgs, this);
			if( const asCDecl* method = context.GetDecl(callee) )
			{
				if( method->kind == asAST_DECL_METHOD && method->name.Equals(methodName) )
				{
					asCQualType type = intType;
					if( method->type.IsValid() )
					{
						type = method->type;
					}
					asCArray<asASTExprId> callArgs;
					callArgs.PushLast(inner);
					asASTExprId call = FindExistingExactUnaryCall(
						context, range, callee, inner);
					if( !call.IsValid() )
					{
						call = ActOnCall(callee, callArgs, type, range);
						context.SetExprReceiver(call, inner);
					}
					const asCType* const resultType = context.GetType(type.type);
					return resultType != 0
						&& resultType->kind != asAST_TYPE_PRIMITIVE
						&& resultType->kind != asAST_TYPE_VOID
						? ActOnMaterializeTemporary(call, range) : call;
				}
			}
		}
	}
	// A raw source spelling is not enough for primitive ++/--. Unlike a class
	// operator call (which already resolves to opPreInc/opPostInc), the result
	// value of the builtin operation depends on whether it was prefix or
	// postfix. Keep that canonical semantic fact in the sealed AST so CodeGen,
	// sidecar restore, dumps, and the AST content digest all agree on it.
	const char* canonicalOp = op ? op : "";
	if( op && op[0] == '+' && op[1] == '+' && op[2] == 0 )
	{
		canonicalOp = postfix ? "post++" : "pre++";
	}
	else if( op && op[0] == '-' && op[1] == '-' && op[2] == 0 )
	{
		canonicalOp = postfix ? "post--" : "pre--";
	}

	// Builtin increment/decrement have the type of their operand. Preserve
	// that here rather than silently defaulting unsigned expressions to int.
	asCQualType resultType = intType;
	if( (canonicalOp[0] == 'p' && canonicalOp[1] != 0) )
	{
		if( const asCExpr* innerExpr = context.GetExpr(inner) )
		{
			if( innerExpr->type.IsValid() )
			{
				resultType = innerExpr->type;
			}
		}
	}
	const asASTExprId existing = FindExistingExactUnary(
		context, range, canonicalOp, inner);
	if( existing.IsValid() )
	{
		return existing;
	}
	return ActOnUnary(canonicalOp, inner, resultType, range);
}

static asASTExprId FindExistingExactInitList(
	asCASTContext& context,
	const asCArray<asASTExprId>& args,
	const asCSourceRange& range,
	const asCQualType& explicitTarget)
{
	if( range.begin.fileID == 0 )
	{
		return asASTExprId();
	}
	for( asUINT i = 1; i <= context.GetExprCount(); ++i )
	{
		const asCExpr* const existing = context.GetExpr(asASTExprId(i));
		if( existing == 0
			|| existing->kind != asAST_EXPR_CONSTRUCT
			|| !existing->literal.Equals("list-pattern")
			|| existing->range.begin.fileID != range.begin.fileID
			|| existing->range.begin.offset != range.begin.offset
			|| existing->range.end.fileID != range.end.fileID
			|| existing->range.end.offset != range.end.offset
			|| existing->children.GetLength() != args.GetLength()
			|| (explicitTarget.IsValid()
				&& existing->type != explicitTarget) )
		{
			continue;
		}
		bool sameChildren = true;
		for( asUINT child = 0; child < args.GetLength(); ++child )
		{
			if( existing->children[child] != args[child] )
			{
				sameChildren = false;
				break;
			}
		}
		if( sameChildren )
		{
			return existing->id;
		}
	}
	return asASTExprId();
}

static bool IsAggregateLifetimeSemanticOwner(const asCDecl* owner)
{
	return owner != 0
		&& (owner->kind == asAST_DECL_FUNCTION
			|| owner->kind == asAST_DECL_METHOD
			|| owner->kind == asAST_DECL_CONSTRUCTOR);
}

static asASTDeclId FindAggregateElementDestructor(
	asCASTContext& context,
	const asCQualType& elementType)
{
	const asCType* const type = elementType.IsValid()
		? context.GetType(elementType.type) : 0;
	if( type == 0 || type->kind != asAST_TYPE_VALUE_OBJECT
		|| elementType.IsReference() || elementType.IsHandle()
		|| type->stableKey.GetLength() == 0 )
	{
		return asASTDeclId();
	}

	for( asUINT index = 1; index <= context.GetDeclCount(); ++index )
	{
		const asCDecl* const destructor =
			context.GetDecl(asASTDeclId(index));
		const asCDecl* const owner = destructor != 0
			&& destructor->kind == asAST_DECL_DESTRUCTOR
			? context.GetDecl(destructor->parent) : 0;
		if( owner != 0 && owner->kind == asAST_DECL_CLASS
			&& (owner->stableKey.Equals(type->stableKey)
				|| owner->name.Equals(type->stableKey)) )
		{
			return destructor->id;
		}
	}
	return asASTDeclId();
}

static bool HasAggregateElementLifetimeRecord(
	const asCASTContext& context,
	asASTExprId aggregate)
{
	for( asUINT index = 0;
		index < context.GetLifetimeRecordCount(); ++index )
	{
		const asSASTLifetimeRecord* const record =
			context.GetLifetimeRecord(index);
		if( record != 0
			&& record->subjectKind
				== asAST_LIFETIME_SUBJECT_AGGREGATE_ELEMENT
			&& record->subject
				== asSASTLifetimeNodeId::FromExpr(aggregate) )
		{
			return true;
		}
	}
	return false;
}

// Return false only when the expression is a non-trivial aggregate that must
// own a progress record but cannot author an exact, verifier-ready record.
// Trivial/empty/non-list aggregates require no dynamic committed cursor.
static bool AuthorAggregateElementLifetime(
	asCSema& sema,
	asASTExprId aggregate)
{
	asCASTContext& context = sema.GetContext();
	const asCExpr* const expression = context.GetExpr(aggregate);
	const asCDecl* const listFactory = expression != 0
		? context.GetDecl(expression->resolvedDecl) : 0;
	if( expression == 0 || expression->kind != asAST_EXPR_CONSTRUCT
		|| !expression->literal.Equals("list-pattern")
		|| listFactory == 0
		|| (listFactory->traits & asAST_TRAIT_LIST_FACTORY) == 0
		|| expression->children.GetLength() == 0 )
	{
		return true;
	}
	if( HasAggregateElementLifetimeRecord(context, aggregate) )
	{
		return true;
	}

	const asCExpr* const first =
		context.GetExpr(expression->children[0]);
	const asCType* const firstType = first != 0 && first->type.IsValid()
		? context.GetType(first->type.type) : 0;
	if( first == 0 || firstType == 0
		|| firstType->kind != asAST_TYPE_VALUE_OBJECT
		|| first->type.IsReference() || first->type.IsHandle() )
	{
		return true;
	}
	for( asUINT childIndex = 1;
		childIndex < expression->children.GetLength(); ++childIndex )
	{
		const asCExpr* const child =
			context.GetExpr(expression->children[childIndex]);
		if( child == 0 || !child->type.IsValid()
			|| child->type.type != first->type.type
			|| child->type.IsReference() || child->type.IsHandle() )
		{
			sema.AddDiagnostic(
				"aggregate-lifetime-element-type-mismatch",
				expression->range);
			return false;
		}
	}

	// Native type views are lazy. Import lifecycle callables before selecting
	// the exact destructor; the record stores the Decl identity and no backend
	// is permitted to repeat name/type lookup.
	sema.InternNativeCallablesForType(first->type);
	const asASTDeclId destructor = FindAggregateElementDestructor(
		context, first->type);
	if( !destructor.IsValid() )
	{
		// A value type with no destructor has no partial-construction cleanup.
		return true;
	}

	const asASTDeclId semanticOwner = sema.CurrentDeclContext();
	const asCDecl* const owner = context.GetDecl(semanticOwner);
	if( !IsAggregateLifetimeSemanticOwner(owner) )
	{
		sema.AddDiagnostic(
			"aggregate-lifetime-semantic-owner-unsupported",
			expression->range);
		return false;
	}

	asSASTLifetimeRecord record;
	record.subjectKind = asAST_LIFETIME_SUBJECT_AGGREGATE_ELEMENT;
	record.subject = asSASTLifetimeNodeId::FromExpr(aggregate);
	record.actionKind = asAST_LIFETIME_ACTION_DESTROY_ELEMENT;
	record.actionTarget = asSASTLifetimeNodeId::FromDecl(destructor);
	record.activationPoint = asSASTLifetimeNodeId::FromExpr(aggregate);
	record.semanticRegion = asSASTLifetimeNodeId::FromDecl(semanticOwner);
	record.phase = asAST_LIFETIME_PHASE_AGGREGATE_ELEMENT;
	record.supportedExitMask = asAST_LIFETIME_EXIT_EXCEPTION;
	record.constructionStep = 0;
	record.order = context.GetLifetimeRecordCount();
	record.completeObjectCommit = false;
	if( asSemaAuthorLifetimeRecord(context, record) != asAST_VERIFY_OK )
	{
		sema.AddDiagnostic(
			"aggregate-lifetime-record-invalid",
			expression->range);
		return false;
	}
	return true;
}

asASTExprId asCSema::ActOnInitList(
	const asCArray<asASTExprId>& args,
	const asCSourceRange& range,
	const asCQualType& explicitTarget)
{
	const asASTExprId existingList = FindExistingExactInitList(
		context, args, range, explicitTarget);
	if( existingList.IsValid() )
	{
		return AuthorAggregateElementLifetime(*this, existingList)
			? existingList : asASTExprId();
	}
	const asCQualType type = explicitTarget.IsValid()
		? explicitTarget : context.InternPrimitive(ttInt, 0);
	const asASTExprId id = context.CreateExpr(
		asAST_EXPR_CONSTRUCT, type, asAST_VALUE_PRVALUE, range);
	context.SetLiteral(id, "list-pattern");
	for( asUINT i = 0; i < args.GetLength(); ++i )
	{
		context.AddExprChild(id, args[i]);
	}
	if( explicitTarget.IsValid() )
	{
		const asASTDeclId listFactory =
			InternNativeListFactoryForType(explicitTarget);
		if( listFactory.IsValid() )
		{
			context.SetResolvedDecl(id, listFactory);
		}
	}
	return AuthorAggregateElementLifetime(*this, id)
		? id : asASTExprId();
}

static const char* OperatorMethodNameFromText(const char* op)
{
	if( op == 0 || op[0] == 0 )
	{
		return 0;
	}
	if( std::strcmp(op, "+") == 0 )
	{
		return "opAdd";
	}
	if( std::strcmp(op, "-") == 0 )
	{
		return "opSub";
	}
	if( std::strcmp(op, "*") == 0 )
	{
		return "opMul";
	}
	if( std::strcmp(op, "/") == 0 )
	{
		return "opDiv";
	}
	if( std::strcmp(op, "%") == 0 )
	{
		return "opMod";
	}
	if( std::strcmp(op, "==") == 0 || std::strcmp(op, "!=") == 0 )
	{
		return "opEquals";
	}
	if( std::strcmp(op, "<") == 0 || std::strcmp(op, ">") == 0
		|| std::strcmp(op, "<=") == 0 || std::strcmp(op, ">=") == 0 )
	{
		return "opCmp";
	}
	return 0;
}

asASTExprId asCSema::ActOnConditionalExpr(asASTExprId cond, asASTExprId thenExpr, asASTExprId elseExpr, const asCSourceRange& range)
{
	const asASTExprId existing = FindExistingExpr(context, asAST_EXPR_CONDITIONAL, range);
	if( existing.IsValid() )
	{
		return existing;
	}
	asCQualType type;
	if( const asCExpr* thenNode = context.GetExpr(thenExpr) )
	{
		type = thenNode->type;
	}
	if( !type.IsValid() )
	{
		if( const asCExpr* elseNode = context.GetExpr(elseExpr) )
		{
			type = elseNode->type;
		}
	}
	if( !type.IsValid() )
	{
		type = context.InternPrimitive(ttInt, 0);
	}
	else if( const asCExpr* elseNode = context.GetExpr(elseExpr) )
	{
		if( elseNode->type.IsValid() && elseNode->type.type != type.type )
		{
			elseExpr = ActOnConversion(elseExpr, type, range);
		}
	}
	return ActOnConditional(cond, thenExpr, elseExpr, type, range);
}

asASTExprId asCSema::ActOnConditionalExprAction(
	const asSConditionalExprAction& action,
	asCScriptCode* script)
{
	if( !PrepareParsedSource(script) )
	{
		return asASTExprId();
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("conditional-expression-action-range-invalid", range);
		return asASTExprId();
	}
	if( !action.condition.IsValid() || context.GetExpr(action.condition) == 0 )
	{
		AddDiagnostic("conditional-expression-action-condition-invalid", range);
		return asASTExprId();
	}
	if( !action.thenExpression.IsValid() || context.GetExpr(action.thenExpression) == 0 )
	{
		AddDiagnostic("conditional-expression-action-then-invalid", range);
		return asASTExprId();
	}
	if( !action.elseExpression.IsValid() || context.GetExpr(action.elseExpression) == 0 )
	{
		AddDiagnostic("conditional-expression-action-else-invalid", range);
		return asASTExprId();
	}
	return ActOnConditionalExpr(
		action.condition,
		action.thenExpression,
		action.elseExpression,
		range);
}

static asCSourceRange SpanExprRange(
	asCASTContext& context,
	asASTExprId lhs,
	asASTExprId rhs,
	const asCSourceRange& fallback)
{
	asCSourceRange span = fallback;
	if( const asCExpr* left = context.GetExpr(lhs) )
	{
		if( left->range.begin.fileID != 0 )
		{
			span.begin = left->range.begin;
		}
		if( left->range.end.fileID != 0 && left->range.end.offset != 0 )
		{
			span.end = left->range.end;
		}
	}
	if( const asCExpr* right = context.GetExpr(rhs) )
	{
		if( right->range.end.fileID != 0 && right->range.end.offset != 0 )
		{
			span.end = right->range.end;
		}
		else if( right->range.begin.fileID != 0 && right->range.begin.offset != 0 )
		{
			span.end = right->range.begin;
		}
	}
	return span;
}

static bool IsIntegerPrimitive(const asCType* type)
{
	return type && type->kind == asAST_TYPE_PRIMITIVE
		&& (type->primitiveToken == ttInt || type->primitiveToken == ttInt8
			|| type->primitiveToken == ttInt16 || type->primitiveToken == ttInt64
			|| type->primitiveToken == ttUInt || type->primitiveToken == ttUInt8
			|| type->primitiveToken == ttUInt16 || type->primitiveToken == ttUInt64);
}

static bool IsIntegerLike(const asCType* type)
{
	return IsIntegerPrimitive(type) || (type && type->kind == asAST_TYPE_ENUM);
}

static const asCType* ResolveTypedefValueType(asCASTContext& context, const asCType* nominalType)
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

static bool IsFloatPrimitive(const asCType* type)
{
	return type && type->kind == asAST_TYPE_PRIMITIVE
		&& (type->primitiveToken == ttFloat || type->primitiveToken == ttFloat32
			|| type->primitiveToken == ttDouble || type->primitiveToken == ttFloat64);
}

static bool IsFloat64Primitive(const asCType* type)
{
	return type && (type->primitiveToken == ttDouble || type->primitiveToken == ttFloat64);
}

static int PromotedIntegerBits(const asCType* type)
{
	if( type == 0 || type->kind == asAST_TYPE_ENUM )
	{
		return 32;
	}
	switch( type->primitiveToken )
	{
	case ttInt64:
	case ttUInt64:
		return 64;
	default:
		return 32;
	}
}

static bool IsPromotedUnsigned(const asCType* type)
{
	if( type == 0 || type->kind == asAST_TYPE_ENUM )
	{
		return false;
	}
	return type->primitiveToken == ttUInt || type->primitiveToken == ttUInt64;
}

static eTokenType CommonIntegerToken(const asCType* lhs, const asCType* rhs)
{
	const int lhsBits = PromotedIntegerBits(lhs);
	const int rhsBits = PromotedIntegerBits(rhs);
	const bool lhsUnsigned = IsPromotedUnsigned(lhs);
	const bool rhsUnsigned = IsPromotedUnsigned(rhs);
	if( lhsBits == rhsBits )
	{
		const bool useUnsigned = lhsUnsigned || rhsUnsigned;
		return lhsBits == 64
			? (useUnsigned ? ttUInt64 : ttInt64)
			: (useUnsigned ? ttUInt : ttInt);
	}
	const asCType* wider = lhsBits > rhsBits ? lhs : rhs;
	const bool widerUnsigned = lhsBits > rhsBits ? lhsUnsigned : rhsUnsigned;
	const bool narrowerUnsigned = lhsBits > rhsBits ? rhsUnsigned : lhsUnsigned;
	// A wider signed 64-bit value represents the complete uint32 range. In all
	// other mixed-sign cases the unsigned type at the highest rank wins.
	const bool useUnsigned = widerUnsigned || (narrowerUnsigned && PromotedIntegerBits(wider) <= 32);
	return useUnsigned ? ttUInt64 : ttInt64;
}

static bool IsComparisonOperator(const char* op)
{
	return op && ((op[0] == '=' && op[1] == '=')
		|| (op[0] == '!' && op[1] == '=')
		|| (op[0] == '<' && (op[1] == 0 || op[1] == '='))
		|| (op[0] == '>' && (op[1] == 0 || op[1] == '=')));
}

asASTExprId asCSema::ActOnLogicalExpr(asASTExprId lhs, const char* op, asASTExprId rhs, const asCSourceRange& range)
{
	const asCSourceRange ident = SpanExprRange(context, lhs, rhs, range);
	const asASTExprId existing = FindExistingExpr(context, asAST_EXPR_LOGICAL, ident);
	if( existing.IsValid() )
	{
		const asCExpr* existingExpr = context.GetExpr(existing);
		if( existingExpr && existingExpr->literal.Equals(op ? op : "") )
		{
			return existing;
		}
	}
	if( !lhs.IsValid() )
	{
		return asASTExprId();
	}
	return ActOnLogical(op ? op : "", lhs, rhs, ident);
}

asASTExprId asCSema::ActOnBinaryExpr(asASTExprId lhs, const char* op, asASTExprId rhs, const asCSourceRange& range)
{
	const asCSourceRange ident = SpanExprRange(context, lhs, rhs, range);
	asCQualType intType = context.InternPrimitive(ttInt, 0);
	const char* methodName = OperatorMethodNameFromText(op);
	if( methodName )
	{
		if( const asCExpr* lhsExpr = context.GetExpr(lhs) )
		{
			if( engine != 0 )
			{
				// Native operators are not eagerly copied into every Canonical AST.
				// Project only the requested operator before overload resolution, as
				// the ordinary member-call path does.  Otherwise `FString + FString`
				// is misclassified as builtin scalar arithmetic and later acquires a
				// meaningless conversion back to FString.
				InternNativeMethods(*this, engine, context,
					lhsExpr->type, methodName);
			}
			const asASTDeclId typeDecl = FindNamedTypeDecl(context, lhsExpr->type);
			if( typeDecl.IsValid() )
			{
				asCArray<asASTExprId> opArgs;
				if( rhs.IsValid() )
				{
					opArgs.PushLast(rhs);
				}
				const asASTDeclId callee = FindBestCallee(context, typeDecl, methodName, opArgs, this);
				if( const asCDecl* method = context.GetDecl(callee) )
				{
					if( method->kind == asAST_DECL_METHOD && method->name.Equals(methodName) )
					{
						asUINT paramIndex = 0;
						for( asUINT p = 0; p < method->children.GetLength(); ++p )
						{
							const asCDecl* param = context.GetDecl(method->children[p]);
							if( param == 0 || param->kind != asAST_DECL_PARAM )
							{
								continue;
							}
							if( paramIndex >= opArgs.GetLength() )
							{
								break;
							}
							const asCExpr* arg = context.GetExpr(opArgs[paramIndex]);
							if( arg && !QualTypesMatch(param->type, arg->type) )
							{
								opArgs[paramIndex] = ActOnConversion(opArgs[paramIndex], param->type, ident);
							}
							++paramIndex;
						}
						asCQualType type = intType;
						if( method->type.IsValid() )
						{
							type = method->type;
						}
						asCArray<asASTExprId> callArgs;
						callArgs.PushLast(lhs);
						for( asUINT argumentIndex = 0;
							argumentIndex < opArgs.GetLength(); ++argumentIndex )
						{
							callArgs.PushLast(opArgs[argumentIndex]);
						}
						const asASTExprId call = ActOnCall(
							callee, callArgs, type, ident);
						context.SetExprReceiver(call, lhs);
						const asCType* const resultType =
							context.GetType(type.type);
						return resultType != 0
							&& resultType->kind != asAST_TYPE_PRIMITIVE
							&& resultType->kind != asAST_TYPE_VOID
							? ActOnMaterializeTemporary(call, ident) : call;
					}
				}
			}
		}
	}
	if( !lhs.IsValid() )
	{
		return asASTExprId();
	}

	// The parser resolves the source spelling `float` according to the engine
	// policy before reaching Sema, while a literal with an `f` suffix remains
	// float32.  Preserve that authority in the canonical AST: a mixed numeric
	// expression has one explicit common operand type, and comparisons produce
	// bool rather than the historical int placeholder.
	asCQualType resultType = intType;
	const asCExpr* lhsExpr = context.GetExpr(lhs);
	const asCExpr* rhsExpr = context.GetExpr(rhs);
	const asCType* lhsType = lhsExpr
		? ResolveTypedefValueType(context, context.GetType(lhsExpr->type.type))
		: 0;
	const asCType* rhsType = rhsExpr
		? ResolveTypedefValueType(context, context.GetType(rhsExpr->type.type))
		: 0;
	if( lhsType && rhsType && (IsFloatPrimitive(lhsType) || IsFloatPrimitive(rhsType))
		&& (IsFloatPrimitive(lhsType) || IsIntegerLike(lhsType))
		&& (IsFloatPrimitive(rhsType) || IsIntegerLike(rhsType)) )
	{
		const eTokenType commonToken = (IsFloat64Primitive(lhsType) || IsFloat64Primitive(rhsType))
			? ttFloat64
			: ttFloat32;
		const asCQualType commonType = context.InternPrimitive(commonToken, 0);
		if( lhsExpr->type.type != commonType.type )
		{
			lhs = ActOnConversion(lhs, commonType, ident);
		}
		if( rhsExpr->type.type != commonType.type )
		{
			rhs = ActOnConversion(rhs, commonType, ident);
		}
		resultType = commonType;
	}
	else if( lhsType && rhsType && IsIntegerLike(lhsType) && IsIntegerLike(rhsType) )
	{
		// Materialize integer promotions in Canonical AST. A result type label
		// alone is insufficient: compact enum/typedef inputs must be extended
		// before VM arithmetic, otherwise -1 is observed as 0xFF.
		const asCQualType commonType = context.InternPrimitive(
			CommonIntegerToken(lhsType, rhsType), 0);
		if( lhsExpr->type.type != commonType.type )
		{
			lhs = ActOnConversion(lhs, commonType, ident);
		}
		if( rhsExpr->type.type != commonType.type )
		{
			rhs = ActOnConversion(rhs, commonType, ident);
		}
		resultType = commonType;
	}
	if( IsComparisonOperator(op) )
	{
		resultType = context.InternPrimitive(ttBool, 0);
	}
	// Range and spelling alone are not a complete binary-expression identity.
	// A flat action may fold more than one equal-precedence operator and, for a
	// synthetic caller whose operands have no ranges, both folds legitimately
	// share the action-wide fallback range. Reuse only after conversions have
	// been normalized and only when both exact children also match. This keeps
	// replay idempotent without allowing an outer `a + b + c` fold to alias the
	// inner `a + b` node.
	const asASTExprId existingBinary =
		FindExistingExpr(context, asAST_EXPR_BINARY, ident);
	if( existingBinary.IsValid() )
	{
		const asCExpr* existing = context.GetExpr(existingBinary);
		if( existing && existing->literal.Equals(op ? op : "")
			&& existing->children.GetLength() == 2
			&& existing->children[0].value == lhs.value
			&& existing->children[1].value == rhs.value )
		{
			return existingBinary;
		}
	}
	return ActOnBinary(op ? op : "", lhs, rhs, resultType, ident);
}

static asASTDeclId FindFuncdefHandle(asCSema& sema, asCASTContext& context, asASTDeclId searchOwner, const char* name, bool exactScope)
{
	if( name == 0 || name[0] == 0 )
	{
		return asASTDeclId();
	}
	asCArray<asASTDeclId> hits;
	if( exactScope )
	{
		sema.LookupInScope(searchOwner, name, hits);
	}
	else
	{
		sema.LookupCandidatesFrom(searchOwner, name, hits);
	}
	for( asUINT i = 0; i < hits.GetLength(); ++i )
	{
		const asCDecl* decl = context.GetDecl(hits[i]);
		if( decl == 0 || (decl->kind != asAST_DECL_PARAM && decl->kind != asAST_DECL_VAR) )
		{
			continue;
		}
		if( IsFuncdefType(context, decl->type) )
		{
			return hits[i];
		}
	}
	return asASTDeclId();
}

static bool QualTypesMatch(const asCQualType& left, const asCQualType& right)
{
	return left.type.value == right.type.value;
}

static bool IsFuncdefType(asCASTContext& context, const asCQualType& type)
{
	const asCType* named = context.GetType(type.type);
	return named && named->kind == asAST_TYPE_FUNCDEF;
}

static bool ResolveCallableResultType(
	asCSema& sema,
	asCASTContext& context,
	const asCDecl& callee,
	asCQualType& outType)
{
	if( !callee.type.IsValid() )
	{
		return false;
	}
	if( !IsFuncdefType(context, callee.type) )
	{
		outType = callee.type;
		return true;
	}

	asCScriptEngine* const engine = sema.GetEngine();
	if( engine == 0 )
	{
		return false;
	}
	asCRuntimeTypeBridge bridge(engine);
	const asCDataType callableType = bridge.Resolve(context, callee.type);
	asCFuncdefType* const funcdefType = callableType.IsFuncdef()
		? CastToFuncdefType(callableType.GetTypeInfo())
		: 0;
	if( funcdefType == 0 || funcdefType->funcdef == 0 )
	{
		return false;
	}

	outType = bridge.FromDataType(context, funcdefType->funcdef->returnType);
	return outType.IsValid();
}

static bool IsFunctionValuedDecl(const asCDecl* decl)
{
	return decl && (decl->kind == asAST_DECL_FUNCTION
		|| decl->kind == asAST_DECL_METHOD
		|| decl->kind == asAST_DECL_IMPORT);
}

static bool DeclRepresentsType(
	asCASTContext& context,
	asASTDeclId declaration,
	const asCQualType& type)
{
	const asCDecl* const decl = context.GetDecl(declaration);
	const asCType* const canonicalType = context.GetType(type.type);
	return decl != 0 && canonicalType != 0
		&& canonicalType->stableKey.GetLength() != 0
		&& (decl->stableKey.Equals(canonicalType->stableKey)
			|| decl->name.Equals(canonicalType->stableKey));
}

static bool IsDerivedReferenceArgument(
	asCASTContext& context,
	const asCQualType& parameterType,
	const asCQualType& argumentType)
{
	const asCType* const parameter = context.GetType(parameterType.type);
	const asCType* const argument = context.GetType(argumentType.type);
	if( parameter == 0 || argument == 0
		|| parameter->kind != asAST_TYPE_REFERENCE_OBJECT
		|| argument->kind != asAST_TYPE_REFERENCE_OBJECT )
	{
		return false;
	}

	const asASTDeclId argumentDecl = FindNamedTypeDecl(context, argumentType);
	if( !argumentDecl.IsValid() )
	{
		return false;
	}
	const asASTDeclId parameterDecl = FindNamedTypeDecl(context, parameterType);
	asCArray<asASTDeclId> frontier;
	frontier.PushLast(argumentDecl);
	asCArray<asASTDeclId> visited;
	while( frontier.GetLength() != 0 )
	{
		const asASTDeclId current = frontier[frontier.GetLength() - 1];
		frontier.PopLast();
		bool alreadyVisited = false;
		for( asUINT visitedIndex = 0; visitedIndex < visited.GetLength(); ++visitedIndex )
		{
			if( visited[visitedIndex] == current )
			{
				alreadyVisited = true;
				break;
			}
		}
		if( alreadyVisited )
		{
			continue;
		}
		visited.PushLast(current);

		if( current != argumentDecl
			&& ((parameterDecl.IsValid() && current == parameterDecl)
				|| DeclRepresentsType(context, current, parameterType)) )
		{
			return true;
		}
		const asCDecl* const currentDecl = context.GetDecl(current);
		if( currentDecl == 0 )
		{
			continue;
		}
		for( asUINT baseIndex = 0; baseIndex < currentDecl->bases.GetLength(); ++baseIndex )
		{
			frontier.PushLast(currentDecl->bases[baseIndex]);
		}
	}
	return false;
}

static bool ImplConvResultMatchesFormal(
	asCASTContext& context,
	const asCQualType& resultType,
	const asCQualType& formalType)
{
	if( QualTypesMatch(resultType, formalType) )
	{
		return true;
	}
	if( IsDerivedReferenceArgument(context, formalType, resultType) )
	{
		return true;
	}
	const asCType* const result = ResolveTypedefValueType(
		context, context.GetType(resultType.type));
	const asCType* const formal = ResolveTypedefValueType(
		context, context.GetType(formalType.type));
	return result != 0 && formal != 0
		&& result->stableKey.GetLength() != 0
		&& result->stableKey.Equals(formal->stableKey);
}

// AngelScript's implicit value conversion is the zero-arg `opImplConv` method
// registered on the argument type. Host bindings such as `TSubclassOf<T>` use
// that operator to produce `UClass`; Sema must rank the operator, not the
// host type names.
static asASTDeclId FindZeroArgImplConv(
	asCASTContext& context,
	const asCQualType& fromType,
	const asCQualType& toType)
{
	if( !fromType.IsValid() || !toType.IsValid() )
	{
		return asASTDeclId();
	}
	const asASTDeclId typeDecl = FindNamedTypeDecl(context, fromType);
	const asCDecl* const cls = context.GetDecl(typeDecl);
	if( cls == 0 )
	{
		return asASTDeclId();
	}
	for( asUINT c = 0; c < cls->children.GetLength(); ++c )
	{
		const asCDecl* const method = context.GetDecl(cls->children[c]);
		if( method == 0 || method->kind != asAST_DECL_METHOD
			|| !method->name.Equals("opImplConv")
			|| CountParams(context, method) != 0 )
		{
			continue;
		}
		if( ImplConvResultMatchesFormal(context, method->type, toType) )
		{
			return method->id;
		}
	}
	return asASTDeclId();
}

static bool HasZeroArgImplConvTo(
	asCASTContext& context,
	const asCQualType& fromType,
	const asCQualType& toType)
{
	return FindZeroArgImplConv(context, fromType, toType).IsValid();
}

static int RankArgument(asCASTContext& context, const asCQualType& paramType, const asCExpr* arg)
{
	if( arg == 0 )
	{
		return -1;
	}
	if( QualTypesMatch(paramType, arg->type) )
	{
		return 2;
	}
	const asCType* const directParamType = context.GetType(paramType.type);
	if( directParamType != 0 && directParamType->kind == asAST_TYPE_WILDCARD )
	{
		// A registered `?` formal accepts the call-site value's exact runtime
		// type.  Keep it viable but rank it below a concrete exact overload.
		return 0;
	}
	if( IsFuncdefType(context, paramType) )
	{
		const asCDecl* value = arg->resolvedDecl.IsValid() ? context.GetDecl(arg->resolvedDecl) : 0;
		if( IsFunctionValuedDecl(value) )
		{
			return 1;
		}
	}
	if( IsDerivedReferenceArgument(context, paramType, arg->type) )
	{
		// A derived script/host reference is a viable base or interface formal.
		// Keep it below an exact overload; ConvertCallArgumentsToFormalTypes will
		// seal the explicit reference conversion after overload selection.
		return 1;
	}
	if( HasZeroArgImplConvTo(context, arg->type, paramType) )
	{
		return 1;
	}
	const asCType* param = ResolveTypedefValueType(context, context.GetType(paramType.type));
	const asCType* argType = ResolveTypedefValueType(context, context.GetType(arg->type.type));
	if( IsIntegerLike(param) && IsIntegerLike(argType) )
	{
		return 1;
	}
	if( IsFloatPrimitive(param) && IsFloatPrimitive(argType) )
	{
		// AngelScript's authored `float` spelling and the UE binding-facing
		// `float32` spelling may intentionally denote different primitive
		// widths. Legacy overload resolution accepts the ordinary numeric
		// conversion and CodeGen materializes it; Canonical Sema must not drop
		// an otherwise exact native target merely because the widths differ.
		return 1;
	}
	if( IsFloatPrimitive(param) && IsIntegerLike(argType) )
	{
		return 1;
	}
	if( IsIntegerLike(param) && IsFloatPrimitive(argType) )
	{
		return 0;
	}
	return -1;
}

static asASTDeclId FindBestCallee(asCASTContext& context, asASTDeclId owner, const char* name, const asCArray<asASTExprId>& args, asCSema* sema, const asCQualType* receiverType, bool exactScope)
{
	asASTDeclId best;
	int bestScore = -1;
	bool ambiguous = false;
	asCArray<asASTDeclId> hits;
	if( sema )
	{
		if( exactScope )
		{
			sema->LookupInScope(owner, name, hits);
		}
		else
		{
			sema->LookupCandidatesFrom(owner, name, hits);
		}
		if( asCScriptEngine* const engine = sema->GetEngine() )
		{
			// Intern conversion operators before ranking so a VALUE argument
			// can match a REF formal through `opImplConv`. Do this before the
			// candidate walk: intern may grow the decl table.
			for( asUINT a = 0; a < args.GetLength(); ++a )
			{
				if( const asCExpr* const arg = context.GetExpr(args[a]) )
				{
					InternNativeMethods(*sema, engine, context, arg->type, "opImplConv");
				}
			}
		}
	}
	for( asUINT i = 0; i < hits.GetLength(); ++i )
	{
		const asCDecl* child = context.GetDecl(hits[i]);
		if( child == 0 || name == 0 || !child->name.Equals(name) )
		{
			continue;
		}
		if( child->kind != asAST_DECL_FUNCTION && child->kind != asAST_DECL_METHOD
			&& child->kind != asAST_DECL_CONSTRUCTOR && child->kind != asAST_DECL_MIXIN
			&& child->kind != asAST_DECL_IMPORT )
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
		if( paramCount < args.GetLength() )
		{
			continue;
		}
		int score = 0;
		if( receiverType && child->kind == asAST_DECL_METHOD )
		{
			const bool methodIsConst = (child->traits & asAST_TRAIT_CONST_METHOD) != 0;
			if( receiverType->IsConst() )
			{
				if( !methodIsConst )
				{
					continue;
				}
			}
			else if( !methodIsConst )
			{
				// A mutable receiver may call either overload, but the mutable
				// member is the exact receiver-qualified match.
				++score;
			}
		}
		asUINT paramIndex = 0;
		bool ok = true;
		for( asUINT p = 0; p < child->children.GetLength(); ++p )
		{
			const asCDecl* param = context.GetDecl(child->children[p]);
			if( param == 0 || param->kind != asAST_DECL_PARAM )
			{
				continue;
			}
			if( paramIndex < args.GetLength() )
			{
				const asCExpr* arg = context.GetExpr(args[paramIndex]);
				if( arg == 0 )
				{
					ok = false;
					break;
				}
				const int rank = RankArgument(context, param->type, arg);
				if( rank < 0 )
				{
					ok = false;
					break;
				}
				score += rank;
			}
			else if( param->defaultArg.GetLength() == 0 && param->inits.GetLength() == 0 )
			{
				ok = false;
				break;
			}
			++paramIndex;
		}
		if( !ok )
		{
			continue;
		}
		if( score > bestScore )
		{
			bestScore = score;
			best = child->id;
			ambiguous = false;
		}
		else if( score == bestScore && best.IsValid() )
		{
			ambiguous = true;
		}
	}
	if( ambiguous )
	{
		if( sema )
		{
			sema->AddDiagnostic("ambiguous-overload");
		}
		return asASTDeclId();
	}
	if( best.IsValid() )
	{
		return best;
	}
	for( asUINT i = 0; i < hits.GetLength(); ++i )
	{
		if( const asCDecl* const decl = context.GetDecl(hits[i]) )
		{
			if( decl->kind == asAST_DECL_CLASS )
			{
				return hits[i];
			}
		}
	}
	return asASTDeclId();
}

static void ArrangeCallArguments(asCSema& sema, asCASTContext& context,
	asASTDeclId callee, asCArray<asASTExprId>& args,
	asCArray<asCString>& names, const asCSourceRange& range,
	asASTExprId implicitReceiver)
{
	const asCDecl* decl = context.GetDecl(callee);
	if( decl == 0 )
	{
		return;
	}
	asCArray<const asCDecl*> params;
	for( asUINT p = 0; p < decl->children.GetLength(); ++p )
	{
		const asCDecl* const param = context.GetDecl(decl->children[p]);
		if( param && param->kind == asAST_DECL_PARAM )
		{
			params.PushLast(param);
		}
	}
	if( params.GetLength() == 0 )
	{
		return;
	}

	const bool hasReceiver = implicitReceiver.IsValid()
		&& args.GetLength() != 0 && args[0] == implicitReceiver;
	const bool receiverIsFormal = hasReceiver && decl->kind == asAST_DECL_MIXIN;
	const asUINT sourceBegin = hasReceiver ? 1 : 0;
	const bool namesIncludeReceiver = hasReceiver && names.GetLength() == args.GetLength();

	asCArray<asASTExprId> formal;
	asCArray<bool> filled;
	formal.SetLength(params.GetLength());
	filled.SetLength(params.GetLength());
	for( asUINT p = 0; p < filled.GetLength(); ++p )
	{
		filled[p] = false;
	}

	asUINT nextPositional = 0;
	if( receiverIsFormal && formal.GetLength() != 0 )
	{
		// Member syntax for a mixin is sugar for a free function whose first
		// declared parameter is the receiver.  Keep receiver metadata on the
		// CALL, but reserve formal zero before arranging authored/named/default
		// arguments.  Ordinary methods instead carry their receiver outside the
		// formal parameter list.
		formal[0] = implicitReceiver;
		filled[0] = true;
		nextPositional = 1;
	}
	for( asUINT i = sourceBegin; i < args.GetLength(); ++i )
	{
		const asUINT nameIndex = namesIncludeReceiver ? i : i - sourceBegin;
		const asCString* const argName = nameIndex < names.GetLength() ? &names[nameIndex] : 0;
		if( argName && argName->GetLength() != 0 )
		{
			for( asUINT p = 0; p < params.GetLength(); ++p )
			{
				if( filled[p] || !params[p]->name.Equals(argName->AddressOf()) )
				{
					continue;
				}
				formal[p] = args[i];
				filled[p] = true;
				if( const asCExpr* expr = context.GetExpr(args[i]) )
				{
					asCString lit = "named:";
					lit += *argName;
					lit += "=";
					lit += expr->literal;
					context.SetLiteral(args[i], lit.AddressOf());
				}
				break;
			}
			continue;
		}
		while( nextPositional < filled.GetLength() && filled[nextPositional] )
		{
			++nextPositional;
		}
		if( nextPositional < formal.GetLength() )
		{
			formal[nextPositional] = args[i];
			filled[nextPositional] = true;
			++nextPositional;
		}
	}

	for( asUINT p = 0; p < params.GetLength(); ++p )
	{
		if( filled[p] )
		{
			continue;
		}
		asASTExprId syn;
		if( params[p]->inits.GetLength() )
		{
			syn = sema.ActOnConversion(params[p]->inits[0], params[p]->type, range);
		}
		else if( params[p]->defaultArg.GetLength() == 0 )
		{
			continue;
		}
		else
		{
			asCString text = params[p]->defaultArg;
			const asQWORD value = (asQWORD)strtoull(text.AddressOf() ? text.AddressOf() : "0", 0, 0);
			syn = sema.ActOnIntegerLiteral(value, range);
		}
		asCString lit = params[p]->origin.Equals("hidden") ? "hidden:" : "default:";
		if( params[p]->defaultArg.GetLength() )
		{
			lit += params[p]->defaultArg;
		}
		context.SetLiteral(syn, lit.AddressOf());
		formal[p] = syn;
		filled[p] = true;
	}

	args.SetLength(0);
	if( hasReceiver && !receiverIsFormal )
	{
		args.PushLast(implicitReceiver);
	}
	for( asUINT p = 0; p < formal.GetLength(); ++p )
	{
		if( filled[p] )
		{
			args.PushLast(formal[p]);
		}
	}
}

static void ConvertCallArgumentsToFormalTypes(asCSema& sema, asCASTContext& context,
	const asCDecl& callee, asCArray<asASTExprId>& args,
	const asCSourceRange& range, asASTExprId implicitReceiver)
{
	// CALL argument arrays are still in forward-formal order here.  A normal
	// method keeps its receiver in the leading non-formal slot, while a mixin
	// uses that same expression as declared formal zero.  Model that distinction
	// once so immediate and deferred lookup cannot drift apart.
	const bool receiverIsFormal = implicitReceiver.IsValid()
		&& callee.kind == asAST_DECL_MIXIN;
	asUINT argIndex = implicitReceiver.IsValid() && !receiverIsFormal ? 1 : 0;
	for( asUINT p = 0; p < callee.children.GetLength(); ++p )
	{
		const asCDecl* const param = context.GetDecl(callee.children[p]);
		if( param == 0 || param->kind != asAST_DECL_PARAM )
		{
			continue;
		}
		if( argIndex >= args.GetLength() )
		{
			break;
		}

		// A generated property getter is not an addressable out target. Seal the
		// exact generated setter and one opaque receiver before ordinary argument
		// conversion can erase that property identity. Direct locals/references
		// stay on the normal lvalue-address route.
		const bool isExactOutOnly = param->type.IsReference()
			&& (param->type.quals & asAST_QUAL_OUT) != 0
			&& (param->type.quals & asAST_QUAL_IN) == 0;
		const asCType* const outValueType = context.GetType(param->type.type);
		const asCExpr* propertyGet = context.GetExpr(args[argIndex]);
		while( propertyGet != 0
			&& propertyGet->kind == asAST_EXPR_CONVERSION
			&& propertyGet->children.GetLength() == 1 )
		{
			propertyGet = context.GetExpr(propertyGet->children[0]);
		}
		if( isExactOutOnly && outValueType != 0
			&& outValueType->kind == asAST_TYPE_PRIMITIVE
			&& propertyGet != 0
			&& propertyGet->kind == asAST_EXPR_CALL
			&& propertyGet->receiver.IsValid()
			&& propertyGet->resolvedDecl.IsValid() )
		{
			const asCDecl* const getter =
				context.GetDecl(propertyGet->resolvedDecl);
			if( getter != 0
				&& getter->kind == asAST_DECL_METHOD
				&& getter->accessorKind == asAST_ACCESSOR_GET
				&& getter->accessorField.IsValid()
				&& getter->type.type == param->type.type )
			{
				asASTDeclId setter;
				for( asUINT candidateIndex = 1;
					candidateIndex <= context.GetDeclCount();
					++candidateIndex )
				{
					const asCDecl* const candidate =
						context.GetDecl(asASTDeclId(candidateIndex));
					if( candidate == 0
						|| candidate->kind != asAST_DECL_METHOD
						|| candidate->parent != getter->parent
						|| candidate->accessorKind != asAST_ACCESSOR_SET
						|| candidate->accessorField != getter->accessorField )
					{
						continue;
					}

					asUINT valueParamCount = 0;
					const asCDecl* valueParam = 0;
					for( asUINT childIndex = 0;
						childIndex < candidate->children.GetLength();
						++childIndex )
					{
						const asCDecl* const child = context.GetDecl(
							candidate->children[childIndex]);
						if( child != 0 && child->kind == asAST_DECL_PARAM )
						{
							++valueParamCount;
							valueParam = child;
						}
					}
					if( valueParamCount == 1 && valueParam != 0
						&& valueParam->type.type == param->type.type
						&& valueParam->type.quals == 0 )
					{
						setter = candidate->id;
						break;
					}
				}

				if( setter.IsValid() )
				{
					const asASTExprId deferred =
						sema.ActOnDeferredOutArgument(
							propertyGet->receiver,
							param->type,
							setter,
							range);
					if( deferred.IsValid() )
					{
						args[argIndex] = deferred;
						++argIndex;
						continue;
					}
				}
			}
		}
		const asCExpr* const arg = context.GetExpr(args[argIndex]);
		const asCType* const paramType = context.GetType(param->type.type);
		if( arg != 0
			&& (paramType == 0 || paramType->kind != asAST_TYPE_WILDCARD)
			&& !QualTypesMatch(param->type, arg->type) )
		{
			const asASTDeclId implConv = FindZeroArgImplConv(
				context, arg->type, param->type);
			args[argIndex] = sema.ActOnConversion(
				args[argIndex], param->type, range);
			if( implConv.IsValid() )
			{
				context.SetResolvedDecl(args[argIndex], implConv);
			}
		}
		args[argIndex] = SealNonPodByValueCopyPlan(
			sema, context, param, args[argIndex], range);
		++argIndex;
	}
}

static const char* OperatorText(eTokenType token)
{
	const char* text = asCTokenizer::GetDefinition(token);
	return text ? text : "";
}

static const char* OperatorMethodName(eTokenType token)
{
	if( token == ttPlus ) return "opAdd";
	if( token == ttMinus ) return "opSub";
	if( token == ttStar ) return "opMul";
	if( token == ttSlash ) return "opDiv";
	if( token == ttPercent ) return "opMod";
	if( token == ttEqual || token == ttNotEqual ) return "opEquals";
	if( token == ttLessThan || token == ttGreaterThan
		|| token == ttLessThanOrEqual || token == ttGreaterThanOrEqual )
	{
		return "opCmp";
	}
	return 0;
}

// Native enum values are not declarations in the canonical script AST. Keep
// them as a typed literal instead of manufacturing a short-lived fake decl:
// that preserves overload resolution and gives CodeGen a normal scalar value.
static asASTExprId InternNativeEnumLiteral(
	asCSema& sema,
	asCASTContext& context,
	const asCString& qualifiedScope,
	const char* valueName,
	const asCSourceRange& range)
{
	if( valueName == 0 || valueName[0] == 0
		|| qualifiedScope.GetLength() == 0 )
	{
		return asASTExprId();
	}
	asCScriptEngine* const engine = sema.GetEngine();
	if( engine == 0 )
	{
		return asASTExprId();
	}
	asITypeInfo* info = engine->GetTypeInfoByDecl(qualifiedScope.AddressOf());
	if( info == 0 )
	{
		info = engine->GetTypeInfoByName(qualifiedScope.AddressOf());
	}
	asCEnumType* const enumType = info ? CastToEnumType(static_cast<asCTypeInfo*>(info)) : 0;
	if( enumType == 0 )
	{
		return asASTExprId();
	}
	for( asUINT i = 0; i < enumType->enumValues.GetLength(); ++i )
	{
		const asSEnumValue* const value = enumType->enumValues[i];
		if( value == 0 || !value->name.Equals(valueName) )
		{
			continue;
		}
		asCRuntimeTypeBridge bridge(engine);
		const asCQualType type = bridge.FromDataType(context, asCDataType::CreateType(enumType, false));
		if( !type.IsValid() )
		{
			return asASTExprId();
		}
		const asASTExprId literal = context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, type, asAST_VALUE_PRVALUE, range);
		context.SetLiteralBits(literal, static_cast<asQWORD>(static_cast<asINT64>(value->value)));
		asCString spelling = qualifiedScope;
		spelling += "::";
		spelling += valueName;
		context.SetLiteral(literal, spelling.AddressOf());
		return literal;
	}
	return asASTExprId();
}

asASTExprId asCSema::ActOnDeclRefExprAction(
	const asSDeclRefExprAction& action,
	asCScriptCode* script)
{
	if( !PrepareParsedSource(script) )
	{
		return asASTExprId();
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("decl-ref-action-range-invalid", range);
		return asASTExprId();
	}
	if( action.name.GetLength() == 0 )
	{
		AddDiagnostic("decl-ref-action-name-empty", range);
		return asASTExprId();
	}
	asASTDeclId owner = action.owner;
	if( !owner.IsValid() )
	{
		owner = CurrentDeclContext();
	}
	if( !owner.IsValid() )
	{
		owner = context.GetTranslationUnit();
	}
	const asASTExprId existingRef = FindExistingExpr(context, asAST_EXPR_DECL_REF, range);
	if( existingRef.IsValid() )
	{
		const asCExpr* existing = context.GetExpr(existingRef);
		if( existing && existing->resolvedDecl.IsValid() )
		{
			return existingRef;
		}
	}
	const bool hasExplicitScope = action.absoluteScope
		|| action.scopeSegments.GetLength() != 0;
	const asCString qualifiedScope = JoinScopeSegments(action.scopeSegments);
	if( !hasExplicitScope && action.name.Equals("this") )
	{
		// The legacy parser represents the language keyword `this` through the
		// same snVariableAccess shape as an ordinary identifier.  Canonical Sema
		// must recover the receiver semantics here instead of routing it through
		// lexical Decl lookup: there is intentionally no synthetic VarDecl named
		// `this` in the public declaration graph.
		const asCDecl* const enclosingClass = FindEnclosingClass(context, owner);
		if( enclosingClass != 0 )
		{
			return ActOnThisExpr(
				QualTypeFromClassDecl(context, enclosingClass), range);
		}
	}
	const asASTExprId nativeEnum = InternNativeEnumLiteral(
		*this, context, qualifiedScope, action.name.AddressOf(), range);
	if( nativeEnum.IsValid() )
	{
		return nativeEnum;
	}
	InternAutomaticImportScriptGlobal(
		*this, qualifiedScope, action.name.AddressOf());
	if( !hasExplicitScope )
	{
		return ActOnDeclRefExpr(owner, action.name.AddressOf(), range);
	}

	asCString unresolvedScope;
	const asASTDeclId searchOwner = ResolveScopeSegments(
		this, owner, action.scopeSegments,
		action.absoluteScope, &unresolvedScope);
	const asASTDeclId target = ResolveExactDeclRef(
		*this, context, searchOwner, action.name.AddressOf());
	if( const asCDecl* const declaration = context.GetDecl(target) )
	{
		const asASTExprId ref = existingRef.IsValid()
			? existingRef : ActOnDeclRef(target, declaration->type, range);
		context.SetResolvedDecl(ref, target);
		context.SetExprType(ref, declaration->type);
		context.SetLiteral(ref, action.name.AddressOf());
		for( asUINT diagnosticIndex = diagnostics.GetLength(); diagnosticIndex > 0; --diagnosticIndex )
		{
			const asUINT index = diagnosticIndex - 1;
			if( index < diagnosticRanges.GetLength()
				&& diagnosticRanges[index].begin.fileID == range.begin.fileID
				&& diagnosticRanges[index].begin.offset == range.begin.offset
				&& diagnosticRanges[index].end.fileID == range.end.fileID
				&& diagnosticRanges[index].end.offset == range.end.offset
				&& (diagnostics[index].StartsWith("unresolved-scope:")
					|| diagnostics[index].StartsWith("unresolved-identifier:")) )
			{
				diagnostics.RemoveIndex(index);
				diagnosticRanges.RemoveIndex(index);
			}
		}
		return ref;
	}

	if( existingRef.IsValid() )
	{
		for( asUINT deferredIndex = 0; deferredIndex < deferredDeclRefs.GetLength(); ++deferredIndex )
		{
			if( deferredDeclRefs[deferredIndex].expression == existingRef )
			{
				return existingRef;
			}
		}
	}

	const asCQualType errorType = context.InternNamedType(
		asAST_TYPE_ERROR, "<unresolved>", 0);
	const asASTExprId ref = existingRef.IsValid()
		? existingRef : ActOnDeclRef(asASTDeclId(), errorType, range);
	context.SetResolvedDecl(ref, asASTDeclId());
	context.SetExprType(ref, errorType);
	context.SetLiteral(ref, action.name.AddressOf());

	if( !searchOwner.IsValid() )
	{
		asCString msg = "unresolved-scope:";
		msg += unresolvedScope;
		AddDiagnostic(msg.AddressOf(), range);
	}
	else
	{
		asCString msg;
		msg.Format("unresolved-identifier:%s: '%s' is not declared in the explicit scope",
			action.name.AddressOf(), action.name.AddressOf());
		AddDiagnostic(msg.AddressOf(), range);
	}

	asSDeferredDeclRef deferred;
	deferred.expression = ref;
	deferred.owner = owner;
	deferred.name = action.name;
	deferred.range = range;
	deferred.scopeSegments = action.scopeSegments;
	deferred.unresolvedScopeSegment = unresolvedScope;
	deferred.absoluteScope = action.absoluteScope;
	deferredDeclRefs.PushLast(deferred);
	return ref;
}

static asASTExprId FindTransparentCallExpression(
	asCASTContext& context,
	asASTExprId result)
{
	const asCExpr* expression = context.GetExpr(result);
	while( expression != 0
		&& (expression->kind == asAST_EXPR_MATERIALIZE_TEMPORARY
			|| expression->kind == asAST_EXPR_CLEANUP)
		&& expression->children.GetLength() == 1 )
	{
		expression = context.GetExpr(expression->children[0]);
	}
	return expression != 0 && expression->kind == asAST_EXPR_CALL
		? expression->id : asASTExprId();
}

asASTExprId asCSema::ActOnCallExprAction(
	const asSCallExprAction& action,
	asCScriptCode* script)
{
	if( !PrepareParsedSource(script) )
	{
		return asASTExprId();
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("call-action-range-invalid", range);
		return asASTExprId();
	}
	if( action.name.GetLength() == 0 )
	{
		AddDiagnostic("call-action-name-empty", range);
		return asASTExprId();
	}

	asASTDeclId owner = action.owner;
	if( !owner.IsValid() )
	{
		owner = CurrentDeclContext();
	}
	if( !owner.IsValid() )
	{
		owner = context.GetTranslationUnit();
	}
	if( !owner.IsValid() || context.GetDecl(owner) == 0 )
	{
		AddDiagnostic("call-action-owner-invalid", range);
		return asASTExprId();
	}
	if( action.implicitReceiver.IsValid()
		&& context.GetExpr(action.implicitReceiver) == 0 )
	{
		AddDiagnostic("call-action-receiver-invalid", range);
		return asASTExprId();
	}

	asCArray<asASTExprId> args;
	asCArray<asCString> argumentNames;
	if( action.implicitReceiver.IsValid() )
	{
		args.PushLast(action.implicitReceiver);
		argumentNames.PushLast(asCString());
	}
	for( asUINT argumentIndex = 0;
		argumentIndex < action.arguments.GetLength(); ++argumentIndex )
	{
		const asSCallArgumentAction& argument =
			action.arguments[argumentIndex];
		if( !argument.expression.IsValid()
			|| context.GetExpr(argument.expression) == 0 )
		{
			AddDiagnostic("call-action-argument-invalid", range);
			return asASTExprId();
		}
		args.PushLast(argument.expression);
		argumentNames.PushLast(argument.name);
	}

	const bool hasExplicitScope = action.absoluteScope
		|| action.scopeSegments.GetLength() != 0;
	const asCString qualifiedScope = JoinScopeSegments(action.scopeSegments);
	InternAutomaticImportScriptFunctions(
		*this, qualifiedScope, action.name.AddressOf());
	if( engine != 0 )
	{
		InternNativeGlobals(*this, engine, context, action.name.AddressOf());
	}

	asASTDeclId searchOwner = owner;
	asCString unresolvedScope;
	if( hasExplicitScope )
	{
		searchOwner = ResolveScopeSegments(
			this, owner, action.scopeSegments,
			action.absoluteScope, &unresolvedScope);
	}
	if( hasExplicitScope && !searchOwner.IsValid() )
	{
		asCString msg = "unresolved-scope:";
		msg += unresolvedScope.GetLength() ? unresolvedScope : "<scope>";
		AddDiagnostic(msg.AddressOf(), range);
		const asCQualType errorType = context.InternNamedType(
			asAST_TYPE_ERROR, "<unresolved>", 0);
		const asASTExprId call = ActOnCall(
			asASTDeclId(), args, errorType, range);
		context.SetLiteral(call, action.name.AddressOf());
		if( action.implicitReceiver.IsValid() )
		{
			context.SetExprReceiver(call, action.implicitReceiver);
		}
		asSDeferredCall deferred;
		deferred.expression = call;
		deferred.owner = owner;
		deferred.name = action.name;
		deferred.arguments = args;
		deferred.argumentNames = argumentNames;
		deferred.implicitReceiver = action.implicitReceiver;
		deferred.range = range;
		deferred.scopeSegments = action.scopeSegments;
		deferred.unresolvedScopeSegment = unresolvedScope;
		deferred.absoluteScope = action.absoluteScope;
		deferred.exactScope = true;
		deferredCalls.PushLast(deferred);
		return call;
	}

	const asASTExprId result = ActOnCallExpr(
		searchOwner, action.name.AddressOf(), args, range,
		action.implicitReceiver, &argumentNames, hasExplicitScope);
	if( hasExplicitScope )
	{
		const asASTExprId call = FindTransparentCallExpression(context, result);
		const asCExpr* const callExpr = context.GetExpr(call);
		if( callExpr != 0 && callExpr->resolvedDecl.IsValid() )
		{
			context.SetExprCallDispatch(call, asAST_CALL_DISPATCH_DIRECT);
		}
	}
	return result;
}

asASTExprId asCSema::ActOnStructuralPostfixExprAction(
	const asSStructuralPostfixExprAction& action,
	asCScriptCode* script)
{
	if( !PrepareParsedSource(script) )
	{
		return asASTExprId();
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("structural-postfix-action-range-invalid", range);
		return asASTExprId();
	}
	if( !action.base.IsValid() || context.GetExpr(action.base) == 0 )
	{
		AddDiagnostic("structural-postfix-action-base-invalid", range);
		return asASTExprId();
	}
	if( action.steps.GetLength() == 0 )
	{
		AddDiagnostic("structural-postfix-action-steps-empty", range);
		return asASTExprId();
	}

	asASTDeclId owner = action.owner;
	if( !owner.IsValid() )
	{
		owner = CurrentDeclContext();
	}
	if( !owner.IsValid() )
	{
		owner = context.GetTranslationUnit();
	}
	if( !owner.IsValid() || context.GetDecl(owner) == 0 )
	{
		AddDiagnostic("structural-postfix-action-owner-invalid", range);
		return asASTExprId();
	}

	asASTExprId value = action.base;
	asUINT previousEnd = action.beginOffset;
	for( asUINT stepIndex = 0;
		stepIndex < action.steps.GetLength(); ++stepIndex )
	{
		const asSStructuralPostfixExprActionStep& step =
			action.steps[stepIndex];
		if( step.beginOffset < action.beginOffset
			|| step.beginOffset < previousEnd
			|| step.endOffset <= step.beginOffset
			|| step.endOffset > action.endOffset )
		{
			AddDiagnostic("structural-postfix-action-step-range-invalid", range);
			return asASTExprId();
		}
		const asCSourceRange stepRange =
			context.GetSourceManager().MakeRange(
				parsedFile, action.beginOffset, step.endOffset);
		if( !stepRange.IsValid() )
		{
			AddDiagnostic("structural-postfix-action-step-range-invalid", range);
			return asASTExprId();
		}

		asCArray<asASTExprId> arguments;
		asCArray<asCString> argumentNames;
		for( asUINT argumentIndex = 0;
			argumentIndex < step.arguments.GetLength(); ++argumentIndex )
		{
			const asSCallArgumentAction& argument =
				step.arguments[argumentIndex];
			if( !argument.expression.IsValid()
				|| context.GetExpr(argument.expression) == 0 )
			{
				AddDiagnostic(
					"structural-postfix-action-argument-invalid",
					stepRange);
				return asASTExprId();
			}
			arguments.PushLast(argument.expression);
			argumentNames.PushLast(argument.name);
		}

		asASTExprId next;
		switch( step.kind )
		{
		case asSTRUCTURAL_POSTFIX_MEMBER:
			if( step.name.GetLength() == 0
				|| step.arguments.GetLength() != 0
				|| step.scopeSegments.GetLength() != 0
				|| step.absoluteScope )
			{
				AddDiagnostic(
					"structural-postfix-action-member-invalid",
					stepRange);
				return asASTExprId();
			}
			next = ActOnMemberExpr(
				value, step.name.AddressOf(), stepRange);
			break;

		case asSTRUCTURAL_POSTFIX_MEMBER_CALL:
		{
			if( step.name.GetLength() == 0 )
			{
				AddDiagnostic(
					"structural-postfix-action-member-call-invalid",
					stepRange);
				return asASTExprId();
			}
			asSCallExprAction call;
			call.owner = owner;
			call.name = step.name;
			call.scopeSegments = step.scopeSegments;
			call.absoluteScope = step.absoluteScope;
			call.implicitReceiver = value;
			call.arguments = step.arguments;
			call.beginOffset = action.beginOffset;
			call.endOffset = step.endOffset;
			next = ActOnCallExprAction(call, script);
			break;
		}

		case asSTRUCTURAL_POSTFIX_INDEX:
			if( arguments.GetLength() == 0
				|| step.name.GetLength() != 0
				|| step.scopeSegments.GetLength() != 0
				|| step.absoluteScope )
			{
				AddDiagnostic(
					"structural-postfix-action-index-invalid",
					stepRange);
				return asASTExprId();
			}
			next = ActOnIndexExpr(
				value, arguments, stepRange, &argumentNames);
			break;

		case asSTRUCTURAL_POSTFIX_CALL:
			if( step.name.GetLength() != 0
				|| step.scopeSegments.GetLength() != 0
				|| step.absoluteScope )
			{
				AddDiagnostic(
					"structural-postfix-action-call-invalid",
					stepRange);
				return asASTExprId();
			}
			next = ActOnPostfixCallExpr(
				owner, value, arguments, stepRange,
				&argumentNames);
			break;

		case asSTRUCTURAL_POSTFIX_UNARY:
		{
			const char* spelling = 0;
			if( step.arguments.GetLength() != 0
				|| step.name.GetLength() != 0
				|| !UnaryActionOperatorMatches(
					step.op, true, spelling) )
			{
				AddDiagnostic(
					"structural-postfix-action-unary-invalid",
					stepRange);
				return asASTExprId();
			}
			next = ActOnUnaryExpr(
				spelling, value, stepRange, true);
			break;
		}

		default:
			AddDiagnostic(
				"structural-postfix-action-kind-invalid",
				stepRange);
			return asASTExprId();
		}
		if( !next.IsValid() || context.GetExpr(next) == 0 )
		{
			AddDiagnostic(
				"structural-postfix-action-step-failed",
				stepRange);
			return asASTExprId();
		}
		value = next;
		previousEnd = step.endOffset;
	}

	for( asUINT prefixIndex = action.prefixOperators.GetLength();
		prefixIndex > 0; --prefixIndex )
	{
		const char* spelling = 0;
		if( !UnaryActionOperatorMatches(
			action.prefixOperators[prefixIndex - 1],
			false, spelling) )
		{
			AddDiagnostic(
				"structural-postfix-action-prefix-invalid",
				range);
			return asASTExprId();
		}
		value = ActOnUnaryExpr(spelling, value, range);
		if( !value.IsValid() )
		{
			AddDiagnostic(
				"structural-postfix-action-prefix-failed",
				range);
			return asASTExprId();
		}
	}
	return value;
}

END_AS_NAMESPACE
