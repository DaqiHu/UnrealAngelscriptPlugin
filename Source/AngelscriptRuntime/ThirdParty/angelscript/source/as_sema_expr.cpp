#include "as_sema_expr.h"
#include "as_datatype.h"
#include "as_module.h"
#include "as_objecttype.h"
#include "as_parser.h"
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

static bool ShouldMaterializeCallResult(
	asCASTContext& context,
	const asCQualType& type)
{
	if( !type.IsValid() || type.IsReference() )
	{
		return false;
	}
	const asCType* const named = context.GetType(type.type);
	return named != 0
		&& named->kind != asAST_TYPE_PRIMITIVE
		&& named->kind != asAST_TYPE_VOID;
}

asASTExprId asCSema::ActOnProjectedRuntimeDefaultArgument(
	const char* authoredText,
	const char* stableCallableKey,
	asUINT runtimeParameterIndex,
	asASTDeclId callable)
{
	if( engine == 0 || authoredText == 0 || authoredText[0] == 0
		|| stableCallableKey == 0 || stableCallableKey[0] == 0
		|| context.GetDecl(callable) == 0 )
	{
		AddDiagnostic("runtime-default-argument-input-invalid");
		return asASTExprId();
	}

	// Expression actions update the active parsed source. Runtime projections
	// can occur recursively while the outer authored Parser is still building a
	// call, so save and restore the complete source/action context. The generated
	// default expression remains in the AST and SourceManager, but it cannot
	// steal the outer Parser's section identity.
	const asASTFileID savedParsedFile = parsedFile;
	const asCString savedParsedSection = parsedSection;
	const asCString savedParsedSourceBytes = parsedSourceBytes;
	const int savedParsedLineOffset = parsedLineOffset;
	const asEASTSourceOrigin savedParsedSourceOrigin = parsedSourceOrigin;
	const asASTDeclId savedLastActedDecl = lastActedDecl;
	const asCArray<asASTDeclId> savedDeclContextStack = declContextStack;

	asCString sectionName;
	sectionName.Format(
		"<canonical-runtime-default:%s#%u>",
		stableCallableKey,
		runtimeParameterIndex);
	asCScriptCode source;
	const int sourceResult = source.SetCode(
		sectionName.AddressOf(), authoredText, true);
	asASTExprId expression;
	if( sourceResult >= 0 )
	{
		parsedSourceOrigin = asAST_SOURCE_GENERATED;
		PushDeclContext(callable);
		asCParser parser(engine);
		parser.SetSema(this);
		if( parser.ParseExpression(&source) >= 0 )
		{
			expression = parser.GetParsedExpressionIdentity();
		}
	}

	declContextStack = savedDeclContextStack;
	lastActedDecl = savedLastActedDecl;
	parsedFile = savedParsedFile;
	parsedSection = savedParsedSection;
	parsedSourceBytes = savedParsedSourceBytes;
	parsedLineOffset = savedParsedLineOffset;
	parsedSourceOrigin = savedParsedSourceOrigin;

	if( sourceResult < 0 )
	{
		AddDiagnostic("runtime-default-argument-source-unavailable");
		return asASTExprId();
	}
	if( !expression.IsValid() || context.GetExpr(expression) == 0 )
	{
		asCString detail;
		detail.Format(
			"runtime-default-argument-expression-invalid:callable=%s parameter=%u",
			stableCallableKey,
			runtimeParameterIndex);
		AddDiagnostic(detail.AddressOf());
		return asASTExprId();
	}
	return expression;
}

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

static asCQualType CanonicalScriptParameterType(
	asCRuntimeTypeBridge& bridge,
	asCASTContext& context,
	const asCScriptFunction* function,
	asUINT parameterIndex)
{
	if( function == 0 || parameterIndex >= function->parameterTypes.GetLength() )
	{
		return asCQualType();
	}
	return bridge.FromScriptFunctionParameterABI(
		context, *function, parameterIndex, parameterIndex);
}

static bool ProjectRuntimeDefaultArgument(
	asCSema& sema,
	asCASTContext& context,
	asCScriptFunction* function,
	asUINT runtimeParameterIndex,
	asASTDeclId parameter)
{
	const asCDecl* const parameterDecl = context.GetDecl(parameter);
	if( function == 0 || parameterDecl == 0
		|| parameterDecl->kind != asAST_DECL_PARAM )
	{
		sema.AddDiagnostic("runtime-default-argument-owner-invalid");
		return false;
	}

	const char* authoredText = 0;
	if( runtimeParameterIndex < function->defaultArgs.GetLength()
		&& function->defaultArgs[runtimeParameterIndex] != 0
		&& function->defaultArgs[runtimeParameterIndex]->GetLength() != 0 )
	{
		authoredText = function->defaultArgs[runtimeParameterIndex]->AddressOf();
	}
	else if( function->hiddenArgumentIndex >= 0
		&& runtimeParameterIndex == (asUINT)function->hiddenArgumentIndex
		&& function->hiddenArgumentDefault.GetLength() != 0 )
	{
		authoredText = function->hiddenArgumentDefault.AddressOf();
	}
	if( authoredText == 0 || authoredText[0] == 0 )
	{
		return true;
	}

	asCString stableCallableKey = function->canonicalASTStableDeclKey;
	if( stableCallableKey.GetLength() == 0 )
	{
		stableCallableKey = function->GetDeclaration(true, true, true);
	}
	const asASTExprId expression =
		sema.ActOnProjectedRuntimeDefaultArgument(
			authoredText,
			stableCallableKey.AddressOf(),
			runtimeParameterIndex,
			parameterDecl->parent);
	if( !expression.IsValid() || context.GetExpr(expression) == 0 )
	{
		return false;
	}
	context.SetDefaultArg(parameter, authoredText);
	context.AddDeclInit(parameter, expression);
	return true;
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
		const asCQualType runtimeReturn =
			bridge.FromDataType(context, function->returnType);
		if( runtimeReturn.type.value != candidate->type.type.value
			|| runtimeReturn.quals != candidate->type.quals )
		{
			continue;
		}
		bool sameParameters = true;
		for( asUINT parameterIndex = 0;
			parameterIndex < function->parameterTypes.GetLength(); ++parameterIndex )
		{
			const asCDecl* parameter =
				context.GetFormalDecl(candidate->id, parameterIndex);
			const asCQualType runtimeParameter =
				CanonicalScriptParameterType(
					bridge, context, function, parameterIndex);
			if( parameter == 0
				|| runtimeParameter.type.value != parameter->type.type.value
				|| runtimeParameter.quals != parameter->type.quals )
			{
				sameParameters = false;
				break;
			}
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
			const asASTDeclId parameter = ActOnParamDecl(declaration, parameterName,
				CanonicalScriptParameterType(bridge, ast, function, p),
				asCSourceRange());
			ProjectRuntimeDefaultArgument(
				*this, ast, function, p, parameter);
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
			const asASTDeclId parameter = sema.ActOnParamDecl(declaration, parameterName,
				CanonicalScriptParameterType(
					bridge, context, function, p),
				asCSourceRange());
			ProjectRuntimeDefaultArgument(
				sema, context, function, p, parameter);
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

static bool IsCompanionTypeScopeDecl(const asCDecl* decl)
{
	return decl != 0
		&& (decl->kind == asAST_DECL_CLASS
			|| decl->kind == asAST_DECL_INTERFACE
			|| decl->kind == asAST_DECL_ENUM);
}

static bool AreTypeAndCompanionNamespace(
	const asCDecl* left,
	const asCDecl* right)
{
	if( left == 0 || right == 0
		|| left->parent != right->parent
		|| !left->stableKey.Equals(right->stableKey) )
	{
		return false;
	}
	return (left->kind == asAST_DECL_NAMESPACE
			&& IsCompanionTypeScopeDecl(right))
		|| (right->kind == asAST_DECL_NAMESPACE
			&& IsCompanionTypeScopeDecl(left));
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
			const asCDecl* const selected = context.GetDecl(candidate);
			if( AreTypeAndCompanionNamespace(selected, decl) )
			{
				// UE's AngelScript surface deliberately uses one source qualifier
				// for a type and a same-key namespace of static-looking helpers.
				// Keep the type as the representative identity; exact member lookup
				// consults its companion namespace after the type scope itself.
				if( selected != 0
					&& selected->kind == asAST_DECL_NAMESPACE )
				{
					candidate = hits[i];
				}
				continue;
			}
			// Any other duplicate remains a real ambiguous scope. Never select
			// an arena-order winner for two namespaces or two type declarations.
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

static asASTDeclId FindCompanionNamespace(
	asCASTContext& context,
	asASTDeclId typeScope)
{
	const asCDecl* const type = context.GetDecl(typeScope);
	if( !IsCompanionTypeScopeDecl(type) )
	{
		return asASTDeclId();
	}
	const asCDecl* const parent = context.GetDecl(type->parent);
	if( parent == 0 )
	{
		return asASTDeclId();
	}
	asASTDeclId companion;
	for( asUINT childIndex = 0;
		childIndex < parent->children.GetLength(); ++childIndex )
	{
		const asCDecl* const child =
			context.GetDecl(parent->children[childIndex]);
		if( child == 0 || child->kind != asAST_DECL_NAMESPACE
			|| !child->stableKey.Equals(type->stableKey) )
		{
			continue;
		}
		if( companion.IsValid() && companion != child->id )
		{
			return asASTDeclId();
		}
		companion = child->id;
	}
	return companion;
}

static const asCDecl* FindEnclosingClass(
	asCASTContext& context, asASTDeclId owner);

static asASTDeclId FindDirectBaseClass(
	asCASTContext& context, asASTDeclId owner)
{
	const asCDecl* const enclosingClass =
		FindEnclosingClass(context, owner);
	asASTDeclId directBase;
	for( asUINT baseIndex = 0;
		enclosingClass != 0
			&& baseIndex < enclosingClass->bases.GetLength(); ++baseIndex )
	{
		const asCDecl* const base =
			context.GetDecl(enclosingClass->bases[baseIndex]);
		if( base == 0 || base->kind != asAST_DECL_CLASS )
		{
			continue;
		}
		if( base->origin.Equals(CANONICAL_NATIVE_TYPE_VIEW_ORIGIN) )
		{
			// A UE PreClass may publish both its authored script superclass and
			// an embedding-owned native shadow as base relations.  The shadow is
			// a layout/type projection, not the source-language target of Super::.
			// Ignore it here just as script-class layout does, while preserving the
			// fail-closed rule below for multiple authored concrete bases.
			continue;
		}
		if( directBase.IsValid() && directBase != base->id )
		{
			// AngelScript has one concrete base class. Preserve fail-closed
			// behavior if a malformed graph presents more than one rather than
			// selecting an arena-order winner for Super::.
			return asASTDeclId();
		}
		directBase = base->id;
	}
	return directBase;
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
		if( !absoluteScope && segmentIndex == 0 && name.Equals("Super") )
		{
			// Super is a source-language base qualifier, not a lexical class or
			// namespace declaration. Project it to the exact direct ClassDecl so
			// the resulting CallExpr owns stable declaration identity and DIRECT
			// dispatch without retaining Parser or Runtime object pointers.
			current = FindDirectBaseClass(sema->GetContext(), owner);
		}
		else if( !current.IsValid() && segmentIndex == 0 && !absoluteScope )
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
	const asCQualType& formalType,
	asASTExprId argument,
	const asCSourceRange& range)
{
	const asCExpr* const source = context.GetExpr(argument);
	if( source == 0 || !formalType.IsValid()
		|| formalType.IsReference() || formalType.IsHandle()
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
	const asCDataType runtimeType = bridge.Resolve(context, formalType);
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
		formalType, copyArguments, range);
	const asCExpr* const construct = UnwrapByValueCopyPlan(
		context, context.GetExpr(plan));
	if( construct == 0 || construct->kind != asAST_EXPR_CONSTRUCT
		|| construct->type != formalType
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

static bool IsExactValueInPlaceProtocolMethod(const char* name)
{
	if( name == 0 || name[0] == 0 )
		return false;
	return std::strcmp(name, "Iterator") == 0
		|| std::strcmp(name, "opForBegin") == 0
		|| std::strcmp(name, "opForEnd") == 0
		|| std::strcmp(name, "opForNext") == 0
		|| std::strcmp(name, "opForValue") == 0
		|| std::strcmp(name, "opForKey") == 0;
}

static asASTExprId RewriteGeneratedPropertyGetForRvalueCallReceiver(
	asCSema& sema,
	asCASTContext& context,
	asASTExprId receiver,
	const char* calleeName)
{
	if( IsExactValueInPlaceProtocolMethod(calleeName) )
		return receiver;
	const asCExpr* expr = context.GetExpr(receiver);
	if( expr == 0 || expr->kind != asAST_EXPR_MEMBER_REF
		|| expr->children.GetLength() == 0
		|| !expr->resolvedDecl.IsValid() )
	{
		return receiver;
	}
	const asCDecl* field = context.GetDecl(expr->resolvedDecl);
	if( field == 0 || field->kind != asAST_DECL_VAR
		|| field->name.GetLength() == 0
		|| !sema.RequiresExactValueObjectPlan(field->type) )
	{
		return receiver;
	}
	const asASTExprId rewritten = TryRewritePropertyGet(
		sema, context, expr->children[0], field->name.AddressOf(),
		field->type, expr->range);
	return rewritten.IsValid() ? rewritten : receiver;
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
		const asCDecl* const param = context.GetFormalDecl(setterDecl->id, 0);
		if( param != 0 && context.GetFormalDeclCount(setterDecl->id) == 1 )
		{
			if( const asCExpr* rhsExpr = context.GetExpr(rhs) )
			{
				if( rhsExpr->type.type.value != param->type.type.value )
				{
					rhs = sema.ActOnConversion(rhs, param->type, range);
				}
			}
			rhs = SealNonPodByValueCopyPlan(
				sema, context, param->type, rhs, range);
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
			&& decl->stableKey.GetLength()
			&& decl->stableKey.Equals(named->stableKey) )
		{
			return decl->id;
		}
	}
	return asASTDeclId();
}

static asUINT CountParams(asCASTContext& context, const asCDecl* decl)
{
	return decl != 0 ? context.GetFormalDeclCount(decl->id) : 0;
}

static asASTDeclId FindBestCallee(asCASTContext& context, asASTDeclId owner,
	const char* name, const asCArray<asASTExprId>& args, asCSema* sema = 0,
	const asCQualType* receiverType = 0, bool exactScope = false,
	const asCArray<asCString>* argumentNames = 0,
	asASTExprId implicitReceiver = asASTExprId());
static asASTDeclId FindBestConstructor(asCASTContext& context,
	asASTDeclId classDecl, const asCArray<asASTExprId>& args,
	asCSema* sema, const asCArray<asCString>* argumentNames = 0);
static int RankArgument(asCASTContext& context, const asCQualType& paramType, const asCExpr* arg, asCSema* sema);
static bool IsFuncdefType(asCASTContext& context, const asCQualType& type);
void InternNativeMethods(asCSema& sema, asCScriptEngine* engine, asCASTContext& context, const asCQualType& recvType, const char* name);
static asCQualType QualTypeFromClassDecl(
	asCASTContext& context, const asCDecl* classDecl);
static asASTDeclId FindFuncdefHandle(asCSema& sema, asCASTContext& context, asASTDeclId searchOwner, const char* name, bool exactScope = false);
static asASTExprId DecayScalarLValueReferenceToRValue(
	asCSema& sema,
	asCASTContext& context,
	asASTExprId expr,
	const asCSourceRange& range);

static asCQualType CanonicalRuntimeParameterType(
	asCRuntimeTypeBridge& bridge,
	asCASTContext& context,
	const asCDataType& runtimeType,
	asETypeModifiers passing)
{
	asCQualType type = bridge.FromDataType(
		context, runtimeType);

	// asCDataType owns const/handle/reference representation, while the
	// parameter direction is stored separately by the Runtime declaration.
	// Project both facts into the sealed QualType so downstream consumers never
	// need to recover source semantics from an Engine-local function shell.
	type.quals &= ~asAST_QUAL_PARAM_DIR_MASK;
	switch( passing )
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
	const asETypeModifiers passing =
		parameterIndex < function->inOutFlags.GetLength()
			? function->inOutFlags[parameterIndex] : asTM_NONE;
	return CanonicalRuntimeParameterType(
		bridge, context, function->parameterTypes[parameterIndex], passing);
}

static bool PublishNativeFunctionDeclarationIdentity(
	asCSema& sema,
	asCASTContext& context,
	asASTDeclId declaration,
	asCScriptFunction* function)
{
	const asCDecl* const decl = context.GetDecl(declaration);
	if( decl == 0 || decl->stableKey.GetLength() == 0 || function == 0
		|| function->engine != sema.GetEngine()
		|| function->funcType != asFUNC_SYSTEM )
	{
		sema.AddDiagnostic("native-function-canonical-identity-invalid");
		return false;
	}
	if( function->canonicalASTStableDeclKey.GetLength() != 0
		&& !function->canonicalASTStableDeclKey.Equals(decl->stableKey) )
	{
		asCString detail;
		detail.Format(
			"native-function-canonical-identity-conflict:function=%s existing=%s projected=%s",
			function->GetDeclaration(false, true, false),
			function->canonicalASTStableDeclKey.AddressOf(),
			decl->stableKey.AddressOf());
		sema.AddDiagnostic(detail.AddressOf());
		return false;
	}
	function->canonicalASTStableDeclKey = decl->stableKey;
	return true;
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
		asASTDeclId existingDeclaration;
		for( asUINT c = 0; c < ownerDecl->children.GetLength(); ++c )
		{
			const asCDecl* child = context.GetDecl(ownerDecl->children[c]);
			if( child == 0 || child->kind != asAST_DECL_FUNCTION || !child->name.Equals(name)
				|| CountParams(context, child) != func->parameterTypes.GetLength() )
			{
				continue;
			}
			const asCQualType internedRet = bridge.FromDataType(context, func->returnType);
			if( internedRet.type.value != child->type.type.value
				|| internedRet.quals != child->type.quals )
			{
				continue;
			}
			bool sameTypes = true;
			for( asUINT paramIndex = 0;
				paramIndex < func->parameterTypes.GetLength() && sameTypes;
				++paramIndex )
			{
				const asCDecl* param =
					context.GetFormalDecl(child->id, paramIndex);
				if( param == 0 )
				{
					sameTypes = false;
					break;
				}
				const asCQualType interned = CanonicalRuntimeParameterType(
					bridge, context, func, paramIndex);
				if( interned.type.value != param->type.type.value
					|| interned.quals != param->type.quals )
				{
					sameTypes = false;
					break;
				}
			}
			if( sameTypes )
			{
				existingDeclaration = child->id;
				break;
			}
		}
		if( existingDeclaration.IsValid() )
		{
			PublishNativeFunctionDeclarationIdentity(
				sema, context, existingDeclaration, func);
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
			}
			ProjectRuntimeDefaultArgument(
				sema, context, func, p, param);
		}
		sema.FinishDecl(fn);
		PublishNativeFunctionDeclarationIdentity(sema, context, fn, func);
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

static asASTDeclId FindTypeMemberVarFromBases(
	asCASTContext& context,
	asASTDeclId classDecl,
	const char* name)
{
	const asASTDeclId direct = FindTypeMemberVar(context, classDecl, name);
	if( direct.IsValid() )
	{
		return direct;
	}
	const asCDecl* const record = context.GetDecl(classDecl);
	if( record == 0 )
	{
		return asASTDeclId();
	}

	asCArray<asASTDeclId> frontier = record->bases;
	asCArray<asASTDeclId> visited;
	while( frontier.GetLength() != 0 )
	{
		asCArray<asASTDeclId> next;
		asASTDeclId selected;
		for( asUINT baseIndex = 0; baseIndex < frontier.GetLength(); ++baseIndex )
		{
			const asASTDeclId base = frontier[baseIndex];
			bool alreadyVisited = false;
			for( asUINT visitedIndex = 0;
				visitedIndex < visited.GetLength(); ++visitedIndex )
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

			const asASTDeclId property =
				FindTypeMemberVar(context, base, name);
			if( property.IsValid() )
			{
				// Two distinct properties at the same inheritance depth are not a
				// deterministic member identity. Leave the reference unresolved so
				// the ordinary diagnostic path fails closed.
				if( selected.IsValid() && selected != property )
				{
					return asASTDeclId();
				}
				selected = property;
			}

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
		if( selected.IsValid() )
		{
			return selected;
		}
		frontier = next;
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

static bool ContainsNativeMethodBase(
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

static void InternNativeMethodsFromCanonicalBases(
	asCSema& sema,
	asCScriptEngine* engine,
	asCASTContext& context,
	asASTDeclId record,
	const char* name)
{
	const asCDecl* const recordDecl = context.GetDecl(record);
	if( recordDecl == 0 )
	{
		return;
	}

	// Runtime ancestry may intentionally remain empty for UE host classes. Walk
	// the pointer-free Canonical base graph instead and project each method onto
	// the native declaration that owns it. LookupCandidatesFrom then applies the
	// ordinary nearest-base hiding/overload rules without copying a method onto
	// the authored derived class or changing LEGACY Runtime semantics.
	asCArray<asASTDeclId> frontier = recordDecl->bases;
	asCArray<asASTDeclId> visited;
	while( frontier.GetLength() != 0 )
	{
		asCArray<asASTDeclId> next;
		for( asUINT baseIndex = 0;
			baseIndex < frontier.GetLength(); ++baseIndex )
		{
			const asASTDeclId base = frontier[baseIndex];
			if( !base.IsValid()
				|| ContainsNativeMethodBase(visited, base) )
			{
				continue;
			}
			visited.PushLast(base);

			const asCDecl* const baseDecl = context.GetDecl(base);
			if( baseDecl == 0
				|| (baseDecl->kind != asAST_DECL_CLASS
					&& baseDecl->kind != asAST_DECL_INTERFACE) )
			{
				continue;
			}

			// Interning can grow the Decl table. Copy every input needed for the
			// next breadth before calling it so no retained Decl pointer can be
			// invalidated by projection.
			const asCQualType baseType =
				QualTypeFromClassDecl(context, baseDecl);
			const asCArray<asASTDeclId> inherited = baseDecl->bases;
			if( baseType.IsValid() )
			{
				InternNativeMethods(
					sema, engine, context, baseType, name);
			}
			for( asUINT inheritedIndex = 0;
				inheritedIndex < inherited.GetLength(); ++inheritedIndex )
			{
				next.PushLast(inherited[inheritedIndex]);
			}
		}
		frontier = next;
	}
}

static void InternNativeProperties(
	asCSema& sema,
	asCScriptEngine* engine,
	asCASTContext& context,
	const asCQualType& recvType,
	const char* requestedName)
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

	auto internPropertiesOf = [&](asCObjectType* declaringType, asASTDeclId ownerDecl)
	{
		if( declaringType == 0 || !ownerDecl.IsValid() )
		{
			return;
		}
		for( asUINT p = 0; p < declaringType->properties.GetLength(); ++p )
		{
			asCObjectProperty* prop = declaringType->properties[p];
			if( prop == 0 || prop->name.GetLength() == 0
				|| (requestedName != 0 && requestedName[0] != 0
					&& !prop->name.Equals(requestedName)) )
			{
				continue;
			}
			if( FindTypeMemberVar(context, ownerDecl, prop->name.AddressOf()).IsValid() )
			{
				continue;
			}
			asCQualType pt = bridge.FromDataType(context, prop->type);
			const asASTDeclId var = sema.ActOnVarDecl(
				ownerDecl, prop->name.AddressOf(), pt, asCSourceRange());
			context.SetDeclByteOffset(var, prop->byteOffset);
			sema.FinishDecl(var);
		}
	};

	internPropertiesOf(objectType, classDecl);
	if( objectType->templateBaseType != 0
		&& objectType->templateBaseType != objectType )
	{
		// Specialized template instances keep the registered property set on
		// the generic object type. Project those names onto the current CLASS
		// view so member-ref lookup does not depend on Runtime copying them.
		internPropertiesOf(objectType->templateBaseType, classDecl);
		// Generic template methods/constructors still carry the unspecialized
		// object type. Intern that Format key so final bytecode relocation can
		// authenticate FREE/CALLSYS against the same Canonical type.
		const asCDataType genericDt =
			asCDataType::CreateType(objectType->templateBaseType, false);
		bridge.FromDataType(context, genericDt);
	}

	// Native UObject properties are registered on the declaring ancestor
	// (`derivedFrom` / `shadowType`), not copied onto every derived AS type.
	// Class-default lookup only sees Canonical bases, so project those
	// ancestors into the same native-view graph and intern the requested field
	// on its exact declaring owner.
	asCObjectType* ancestor = objectType->derivedFrom;
	if( ancestor == 0 )
	{
		ancestor = static_cast<asCObjectType*>(objectType->shadowType);
	}
	asCArray<asCObjectType*> visitedAncestors;
	while( ancestor != 0 )
	{
		bool alreadyVisited = false;
		for( asUINT visitedIndex = 0;
			visitedIndex < visitedAncestors.GetLength(); ++visitedIndex )
		{
			if( visitedAncestors[visitedIndex] == ancestor )
			{
				alreadyVisited = true;
				break;
			}
		}
		if( alreadyVisited )
		{
			break;
		}
		visitedAncestors.PushLast(ancestor);

		const asCDataType ancestorDt = asCDataType::CreateType(ancestor, false);
		const asCQualType ancestorType = bridge.FromDataType(context, ancestorDt);
		asASTDeclId ancestorDecl = FindNamedTypeDecl(context, ancestorType);
		if( !ancestorDecl.IsValid() )
		{
			const char* const ancestorKey = ancestor->name.AddressOf();
			ancestorDecl = sema.ActOnClassDecl(
				context.GetTranslationUnit(), ancestorKey, asCSourceRange());
			sema.FinishDecl(ancestorDecl);
		}
		SyncImportedNativeClassKind(context, ancestorDecl, ancestorDt);
		internPropertiesOf(ancestor, ancestorDecl);

		const asCDecl* const owner = context.GetDecl(classDecl);
		bool alreadyBase = false;
		for( asUINT baseIndex = 0;
			owner != 0 && baseIndex < owner->bases.GetLength(); ++baseIndex )
		{
			if( owner->bases[baseIndex] == ancestorDecl )
			{
				alreadyBase = true;
				break;
			}
		}
		if( !alreadyBase )
		{
			context.AddBase(classDecl, ancestorDecl);
		}

		asCObjectType* const next = ancestor->derivedFrom != 0
			? ancestor->derivedFrom
			: static_cast<asCObjectType*>(ancestor->shadowType);
		ancestor = next;
	}
}

void asCSema::InternNativePropertyCandidates(
	asASTDeclId owner,
	const char* name)
{
	if( engine == 0 || name == 0 || name[0] == 0 )
	{
		return;
	}

	asASTDeclId record = owner;
	while( record.IsValid() )
	{
		const asCDecl* const declaration = context.GetDecl(record);
		if( declaration == 0 )
		{
			return;
		}
		if( declaration->kind == asAST_DECL_CLASS
			|| declaration->kind == asAST_DECL_INTERFACE )
		{
			break;
		}
		record = declaration->parent;
	}
	const asCDecl* const recordDecl = context.GetDecl(record);
	if( recordDecl == 0 )
	{
		return;
	}

	// A native receiver can own the requested property directly. For an authored
	// script record this call deliberately performs no projection; only its exact
	// Canonical bases are allowed to contribute host declarations.
	const asCQualType recordType = QualTypeFromClassDecl(context, recordDecl);
	if( recordType.IsValid() )
	{
		InternNativeProperties(
			*this, engine, context, recordType, name);
	}

	// Interning can grow the declaration table. Retain only ids and copied base
	// arrays across projection calls, never pointers into the mutable table.
	asCArray<asASTDeclId> frontier = recordDecl->bases;
	asCArray<asASTDeclId> visited;
	while( frontier.GetLength() != 0 )
	{
		asCArray<asASTDeclId> next;
		for( asUINT baseIndex = 0;
			baseIndex < frontier.GetLength(); ++baseIndex )
		{
			const asASTDeclId base = frontier[baseIndex];
			bool alreadyVisited = false;
			for( asUINT visitedIndex = 0;
				visitedIndex < visited.GetLength(); ++visitedIndex )
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

			const asCDecl* const baseDecl = context.GetDecl(base);
			if( baseDecl == 0
				|| (baseDecl->kind != asAST_DECL_CLASS
					&& baseDecl->kind != asAST_DECL_INTERFACE) )
			{
				continue;
			}
			const asCQualType baseType =
				QualTypeFromClassDecl(context, baseDecl);
			const asCArray<asASTDeclId> inherited = baseDecl->bases;
			if( baseType.IsValid() )
			{
				InternNativeProperties(
					*this, engine, context, baseType, name);
			}
			for( asUINT inheritedIndex = 0;
				inheritedIndex < inherited.GetLength(); ++inheritedIndex )
			{
				next.PushLast(inherited[inheritedIndex]);
			}
		}
		frontier = next;
	}
}

void asCSema::InternNativePropertyCandidates(
	const asCQualType& recvType,
	const char* name)
{
	if( engine == 0 || name == 0 || name[0] == 0 || !recvType.IsValid() )
	{
		return;
	}
	InternNativeProperties(*this, engine, context, recvType, name);
	InternNativePropertyCandidates(
		FindNamedTypeDecl(context, recvType), name);
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
	if( objectType->templateBaseType != 0
		&& objectType->templateBaseType != objectType )
	{
		const asCDataType genericDt =
			asCDataType::CreateType(objectType->templateBaseType, false);
		bridge.FromDataType(context, genericDt);
	}
	asASTDeclId classDecl = FindNamedTypeDecl(context, recvType);
	// Runtime methods are language facts of the native declaration that owns
	// them, never of a current-module script class that happens to resolve to a
	// Runtime type with the same stable spelling.  This boundary also applies to
	// `opImplConv`: overload ranking asks for it eagerly on every argument type,
	// so projecting it onto an authored value class would reclassify that class as
	// a native type view and append ABI-normalized callables beside its source
	// declarations.  Native-base conversion operators remain visible by
	// projecting them onto the exact Canonical base declarations below.
	if( classDecl.IsValid() && IsAuthoredScriptClass(context, classDecl) )
	{
		InternNativeMethodsFromCanonicalBases(
			sema, engine, context, classDecl, name);
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
				if( internedRet.type.value != child->type.type.value
					|| internedRet.quals != child->type.quals
					|| internMethod->IsReadOnly()
						!= ((child->traits & asAST_TRAIT_CONST_METHOD) != 0) )
				{
					// Zero-arg `opImplConv` overloads differ by return type
					// (`UClass` vs `UObject`). Name+arity alone is not identity.
					continue;
				}
				bool sameTypes = true;
				for( asUINT paramIndex = 0;
					paramIndex < substParams.GetLength() && sameTypes;
					++paramIndex )
				{
					const asCDecl* param =
						context.GetFormalDecl(child->id, paramIndex);
					if( param == 0 )
					{
						sameTypes = false;
						break;
					}
					const asETypeModifiers passing =
						paramIndex < internMethod->inOutFlags.GetLength()
							? internMethod->inOutFlags[paramIndex] : asTM_NONE;
					const asCQualType interned = CanonicalRuntimeParameterType(
						bridge, context, substParams[paramIndex], passing);
					if( interned.type.value != param->type.type.value
						|| interned.quals != param->type.quals )
					{
						sameTypes = false;
						break;
					}
				}
				if( sameTypes )
				{
					context.AddDeclTrait(child->id, asAST_TRAIT_EXTERNAL);
					if( internMethod->objectType == objectType )
					{
						PublishNativeFunctionDeclarationIdentity(
							sema, context, child->id, internMethod);
					}
					return false;
				}
			}
		}
		const asCQualType ret = bridge.FromDataType(context, retDt);
		const asASTDeclId fn = sema.ActOnMethodDecl(classDecl, name, ret, asCSourceRange());
		context.SetOrigin(fn, internMethod->GetDeclaration(false, false, false));
		context.AddDeclTrait(fn, asAST_TRAIT_EXTERNAL);
		if( internMethod->IsReadOnly() )
		{
			context.AddDeclTrait(fn, asAST_TRAIT_CONST_METHOD);
		}
		if( internMethod->traits.GetTrait(asTRAIT_EDITOR_ONLY) )
		{
			context.AddDeclTrait(fn, asAST_TRAIT_EDITOR_ONLY);
		}
		for( asUINT p = 0; p < substParams.GetLength(); ++p )
		{
			const asETypeModifiers passing =
				p < internMethod->inOutFlags.GetLength()
					? internMethod->inOutFlags[p] : asTM_NONE;
			const asCQualType pt = CanonicalRuntimeParameterType(
				bridge, context, substParams[p], passing);
			const char* pname = (p < internMethod->parameterNames.GetLength() && internMethod->parameterNames[p].GetLength())
				? internMethod->parameterNames[p].AddressOf()
				: "";
			const asASTDeclId parameter =
				sema.ActOnParamDecl(fn, pname, pt, asCSourceRange());
			ProjectRuntimeDefaultArgument(
				sema, context, internMethod, p, parameter);
		}
		sema.FinishDecl(fn);
		if( internMethod->objectType == objectType )
		{
			PublishNativeFunctionDeclarationIdentity(
				sema, context, fn, internMethod);
		}
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
		context, typeDecl, "opIndex", indexArgs, &sema, &baseExpr->type);
}

static bool NativeBehaviourParamsMatch(
	asCASTContext& context,
	asCRuntimeTypeBridge& bridge,
	const asCDecl* child,
	asCScriptFunction* func,
	const asCArray<asCDataType>& substParams,
	asUINT sourceParameterStart)
{
	if( child == 0 || func == 0 || CountParams(context, child) != substParams.GetLength() )
	{
		return false;
	}
	for( asUINT paramIndex = 0;
		paramIndex < substParams.GetLength(); ++paramIndex )
	{
		const asCDecl* param = context.GetFormalDecl(child->id, paramIndex);
		if( param == 0 )
		{
			return false;
		}
		const asUINT sourceParameterIndex = sourceParameterStart + paramIndex;
		const asETypeModifiers passing =
			sourceParameterIndex < func->inOutFlags.GetLength()
				? func->inOutFlags[sourceParameterIndex] : asTM_NONE;
		const asCQualType interned = CanonicalRuntimeParameterType(
			bridge, context, substParams[paramIndex], passing);
		if( interned.type.value != param->type.type.value
			|| interned.quals != param->type.quals )
		{
			return false;
		}
	}
	return true;
}

static asCString CanonicalListPatternOrigin(const asCScriptFunction* function)
{
	asCString origin;
	if( function == 0 || function->listPattern == 0 )
	{
		return origin;
	}
	for( const asSListPatternNode* node = function->listPattern;
		node != 0; node = node->next )
	{
		asCString token;
		if( node->type == asLPT_REPEAT )
			token = "repeat";
		else if( node->type == asLPT_REPEAT_SAME )
			token = "repeat_same";
		else if( node->type == asLPT_TYPE )
		{
			const asSListPatternDataTypeNode* const typed =
				static_cast<const asSListPatternDataTypeNode*>(node);
			token = typed->dataType.Format(0);
		}
		else
			continue;
		if( token.GetLength() == 0 )
			continue;
		if( origin.GetLength() != 0 )
			origin += " ";
		origin += token;
	}
	return origin;
}

static void PublishCanonicalListFactoryOrigin(
	asCASTContext& context,
	asASTDeclId declaration,
	const asCScriptFunction* function,
	asDWORD traits)
{
	if( (traits & asAST_TRAIT_LIST_FACTORY) == 0 || !declaration.IsValid() )
	{
		return;
	}
	const asCString origin = CanonicalListPatternOrigin(function);
	if( origin.GetLength() != 0 )
	{
		context.SetOrigin(declaration, origin.AddressOf());
	}
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
		else if( isFactory && func->listPattern != 0
			&& func->parameterTypes.GetLength() > 0 )
		{
			start = 1;
		}
		else if( isFactory && func->parameterTypes.GetLength() > 0
			&& func->parameterTypes[0].IsReference()
			&& !func->parameterTypes[0].IsPrimitive() )
		{
			start = 1;
		}
		else if( isFactory && objectType->templateBaseType
			&& func->funcType != asFUNC_SCRIPT
			&& func->parameterTypes.GetLength() > 0 )
		{
			// A registered template factory still carries the leading type-info
			// formal. GenerateTemplateFactoryStub removes that hidden formal from
			// the instance-local script stub, so skipping again would discard its
			// first authored argument.
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
		asDWORD effectiveTraits = traits;
		if( func->traits.GetTrait(asEFuncTrait::asTRAIT_IMPLICITCONSTRUCTOR) )
		{
			effectiveTraits |= asAST_TRAIT_IMPLICIT_CONSTRUCTOR;
		}
		bool exists = false;
		const asCDecl* cls = context.GetDecl(classDecl);
		if( cls )
		{
			for( asUINT c = 0; c < cls->children.GetLength(); ++c )
			{
				const asCDecl* child = context.GetDecl(cls->children[c]);
				if( child && child->kind == asAST_DECL_CONSTRUCTOR
					&& (effectiveTraits == asAST_TRAIT_NONE
						|| (child->traits & effectiveTraits) == effectiveTraits)
					&& (effectiveTraits == asAST_TRAIT_NONE || child->name.Equals(ctorName))
					&& NativeBehaviourParamsMatch(
						context, bridge, child, func, substParams, start) )
				{
					exists = true;
					if( !selected.IsValid() )
					{
						selected = child->id;
					}
					if( func->funcType == asFUNC_SYSTEM
						&& (func->objectType == objectType
							|| (isFactory && func->objectType == 0)) )
					{
						PublishNativeFunctionDeclarationIdentity(
							sema, context, child->id, func);
					}
					PublishCanonicalListFactoryOrigin(
						context, child->id, func, effectiveTraits);
					break;
				}
			}
		}
		if( exists )
		{
			continue;
		}
		const asASTDeclId ctor = sema.ActOnConstructorDecl(classDecl, ctorName, asCSourceRange());
		if( effectiveTraits != asAST_TRAIT_NONE )
		{
			context.AddDeclTrait(ctor, effectiveTraits);
		}
		for( asUINT p = 0; p < substParams.GetLength(); ++p )
		{
			const asUINT src = start + p;
			const asETypeModifiers passing =
				src < func->inOutFlags.GetLength()
					? func->inOutFlags[src] : asTM_NONE;
			const asCQualType pt = CanonicalRuntimeParameterType(
				bridge, context, substParams[p], passing);
			const char* pname = (src < func->parameterNames.GetLength() && func->parameterNames[src].GetLength())
				? func->parameterNames[src].AddressOf()
				: "";
			const asASTDeclId parameter =
				sema.ActOnParamDecl(ctor, pname, pt, asCSourceRange());
			ProjectRuntimeDefaultArgument(
				sema, context, func, src, parameter);
		}
		sema.FinishDecl(ctor);
		PublishCanonicalListFactoryOrigin(
			context, ctor, func, effectiveTraits);
		if( func->funcType == asFUNC_SYSTEM
			&& (func->objectType == objectType
				|| (isFactory && func->objectType == 0)) )
		{
			PublishNativeFunctionDeclarationIdentity(
				sema, context, ctor, func);
		}
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

static asASTDeclId ResolveCallee(asCSema& sema, asCASTContext& context,
	asASTDeclId searchOwner, asASTExprId implicitReceiver, const char* name,
	const asCArray<asASTExprId>& args,
	const asCArray<asCString>* argumentNames = 0,
	bool exactScope = false)
{
	if( !implicitReceiver.IsValid() && !exactScope )
	{
		// An unqualified class-body call receives its implicit `this` only after
		// overload selection has identified a MethodDecl. Native methods inherited
		// through a pointer-free host base relation must therefore be projected
		// before that first lookup; waiting for the receiver path below creates a
		// cycle (no projected method -> no selected method -> no implicit `this`).
		// Import onto the exact native base declarations only. Ordinary lexical
		// lookup still decides whether the name denotes a method, global, or local,
		// and ActOnCallExpr remains the sole owner of implicit-receiver creation.
		const asCDecl* const enclosingClass =
			FindEnclosingClass(context, searchOwner);
		if( enclosingClass != 0
			&& IsAuthoredScriptClass(context, enclosingClass->id) )
		{
			if( asCScriptEngine* const engine = sema.GetEngine() )
			{
				InternNativeMethodsFromCanonicalBases(
					sema, engine, context, enclosingClass->id, name);
			}
		}
	}
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
				asCArray<asCString> withoutReceiverNames;
				if( argumentNames != 0 )
				{
					const bool namesIncludeReceiver =
						argumentNames->GetLength() == args.GetLength();
					const asUINT nameBegin = namesIncludeReceiver ? 1 : 0;
					for( asUINT i = nameBegin;
						i < argumentNames->GetLength(); ++i )
					{
						withoutReceiverNames.PushLast((*argumentNames)[i]);
					}
				}
				const asASTDeclId method = FindBestCallee(
					context, typeDecl, name, withoutReceiver, &sema, &recv->type,
					false, argumentNames != 0 ? &withoutReceiverNames : 0);
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
				if( asCScriptEngine* const engine = sema.GetEngine() )
				{
					// Extra-module compiles may not project PreClass native
					// ancestry onto an authored script class. Native methods such
					// as UAngelscriptTestSuite::AssertNotNull still live on the
					// registered type views; intern and select them by argument
					// ranking when Canonical bases were not copied onto the
					// derived class.
					InternNativeMethodsFromCanonicalBases(
						sema, engine, context, typeDecl, name);
					const asCQualType objectType = context.InternNamedType(
						asAST_TYPE_REFERENCE_OBJECT,
						"UObject",
						asAST_QUAL_HANDLE | asAST_QUAL_AUTO_HANDLE);
					const asCQualType suiteType = context.InternNamedType(
						asAST_TYPE_REFERENCE_OBJECT,
						"UAngelscriptTestSuite",
						asAST_QUAL_HANDLE | asAST_QUAL_AUTO_HANDLE);
					InternNativeMethods(sema, engine, context, objectType, name);
					InternNativeMethods(sema, engine, context, suiteType, name);
					asASTDeclId inherited = FindBestCallee(
						context, typeDecl, name, withoutReceiver, &sema,
						&recv->type, false,
						argumentNames != 0 ? &withoutReceiverNames : 0);
					if( inherited.IsValid() )
					{
						return inherited;
					}
					asASTDeclId selectedNative;
					for( asUINT declarationIndex = 1;
						declarationIndex <= context.GetDeclCount();
						++declarationIndex )
					{
						const asCDecl* const nativeView =
							context.GetDecl(asASTDeclId(declarationIndex));
						if( nativeView == 0
							|| (nativeView->kind != asAST_DECL_CLASS
								&& nativeView->kind != asAST_DECL_INTERFACE)
							|| !nativeView->origin.Equals(
								CANONICAL_NATIVE_TYPE_VIEW_ORIGIN) )
						{
							continue;
						}
						InternNativeMethods(
							sema, engine, context,
							QualTypeFromClassDecl(context, nativeView), name);
						const asASTDeclId nativeMethod = FindBestCallee(
							context, nativeView->id, name, withoutReceiver,
							&sema, &recv->type, true,
							argumentNames != 0 ? &withoutReceiverNames : 0);
						if( !nativeMethod.IsValid() )
						{
							continue;
						}
						if( selectedNative.IsValid()
							&& selectedNative != nativeMethod )
						{
							selectedNative = asASTDeclId();
							break;
						}
						selectedNative = nativeMethod;
					}
					if( selectedNative.IsValid() )
					{
						return selectedNative;
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
	const asCDecl* const explicitOwner = exactScope
		? context.GetDecl(searchOwner) : 0;
	if( explicitOwner != 0
		&& (explicitOwner->kind == asAST_DECL_CLASS
			|| explicitOwner->kind == asAST_DECL_INTERFACE) )
	{
		// The maintained UE language surface publishes BlueprintEvent/
		// BlueprintOverride bodies as <Name>_Implementation. An explicitly
		// qualified base call must select that authored implementation before
		// the event wrapper, matching the source compiler's established route.
		// This is Sema-owned overload selection: CodeGen receives only the exact
		// stable MethodDecl and DIRECT dispatch fact.
		asCString implementationName = name ? name : "";
		implementationName += "_Implementation";
		const asASTDeclId implementation = FindBestCallee(
			context, searchOwner, implementationName.AddressOf(), args,
			&sema, 0, true, argumentNames, implicitReceiver);
		if( implementation.IsValid() )
		{
			return implementation;
		}
	}
	const asASTDeclId direct = FindBestCallee(
		context, searchOwner, name, args, &sema, 0, exactScope,
		argumentNames, implicitReceiver);
	if( direct.IsValid() )
	{
		return direct;
	}
	if( exactScope )
	{
		const asASTDeclId companion =
			FindCompanionNamespace(context, searchOwner);
		if( companion.IsValid() )
		{
			const asASTDeclId staticHelper = FindBestCallee(
				context, companion, name, args, &sema, 0, true,
				argumentNames, implicitReceiver);
			if( staticHelper.IsValid() )
			{
				return staticHelper;
			}
		}
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

static asCQualType QualTypeForConstructExpression(
	asCASTContext& context,
	const asCQualType& exactTargetType)
{
	if( !exactTargetType.IsValid() )
	{
		return exactTargetType;
	}
	const asCType* const target = context.GetType(exactTargetType.type);
	if( target == 0 || target->kind != asAST_TYPE_REFERENCE_OBJECT )
	{
		return exactTargetType;
	}

	// The typed target identifies the exact nominal/template specialization,
	// while construction owns the result-value qualifier. A reference-object
	// type name can carry only its intrinsic implicit-handle bit; the temporary
	// produced by C(...) additionally owns AUTO_HANDLE so lifetime transfer and
	// cleanup are explicit. Preserve every authored qualifier and the exact TypeId
	// while adding only the construct-result semantics.
	return asCQualType(
		exactTargetType.type,
		exactTargetType.quals | asAST_QUAL_HANDLE | asAST_QUAL_AUTO_HANDLE);
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

static asASTExprId ConstructFromCallee(
	asCSema& sema,
	asCASTContext& context,
	asASTDeclId callee,
	const asCArray<asASTExprId>& args,
	const asCSourceRange& range,
	const asCArray<asSASTCallArgument>* callArguments,
	const asCQualType* exactTargetType)
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
	// A type-name call already owns the exact source target. Rebuilding it from
	// the selected ClassDecl can lose a template specialization or handle
	// qualifier, so keep the Sema-authored QualType when it is available.
	const asCQualType exactType = exactTargetType != 0
		&& exactTargetType->IsValid()
		&& context.GetType(exactTargetType->type) != 0
		? *exactTargetType : QualTypeFromClassDecl(context, classDecl);
	const asCQualType type = QualTypeForConstructExpression(context, exactType);
	return sema.ActOnConstruct(
		type, args, range,
		decl->kind == asAST_DECL_CONSTRUCTOR ? decl->id : asASTDeclId(),
		callArguments);
}

static bool QualTypesMatch(const asCQualType& left, const asCQualType& right);
static void ArrangeCallArguments(asCSema& sema, asCASTContext& context,
	asASTDeclId callee, asCArray<asASTExprId>& args,
	asCArray<asCString>& names, const asCSourceRange& range,
	asASTExprId implicitReceiver,
	asCArray<asSASTCallArgument>& callArguments);
static void ConvertCallArgumentsToFormalTypes(asCSema& sema, asCASTContext& context,
	const asCDecl& callee, asCArray<asASTExprId>& args,
	const asCSourceRange& range, asASTExprId implicitReceiver,
	asCArray<asSASTCallArgument>& callArguments);

asASTExprId asCSema::ActOnCallExpr(asASTDeclId owner, const char* name, const asCArray<asASTExprId>& inArgs, const asCSourceRange& range, asASTExprId implicitReceiver, const asCArray<asCString>* argumentNames, bool exactScope, const asCQualType* typeNameTarget)
{
	const asASTExprId originalReceiver = implicitReceiver;
	if( implicitReceiver.IsValid() )
	{
		implicitReceiver = RewriteGeneratedPropertyGetForRvalueCallReceiver(
			*this, context, implicitReceiver, name);
	}
	asCArray<asASTExprId> args;
	if( implicitReceiver.IsValid() )
	{
		const bool alreadyHasReceiver = inArgs.GetLength()
			&& (inArgs[0] == originalReceiver || inArgs[0] == implicitReceiver);
		if( !alreadyHasReceiver )
		{
			args.PushLast(implicitReceiver);
		}
	}
	for( asUINT i = 0; i < inArgs.GetLength(); ++i )
	{
		args.PushLast(inArgs[i] == originalReceiver ? implicitReceiver : inArgs[i]);
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
	asCArray<asSASTCallArgument> callArguments;
	asASTDeclId callee = ResolveCallee(
		*this, context, owner, implicitReceiver, name, args,
		argumentNames, exactScope);
	const asCDecl* calleeDecl = context.GetDecl(callee);
	if( !implicitReceiver.IsValid()
		&& ((calleeDecl != 0 && calleeDecl->kind == asAST_DECL_CLASS)
			|| (!callee.IsValid() && typeNameTarget != 0
				&& typeNameTarget->IsValid()
				&& context.GetType(typeNameTarget->type) != 0)) )
	{
		const asCQualType constructType = calleeDecl != 0
			? QualTypeFromClassDecl(context, calleeDecl) : *typeNameTarget;
		InternNativeCallablesForType(constructType);
		const asASTDeclId classDecl = FindNamedTypeDecl(context, constructType);
		const asASTDeclId selectedConstructor = FindBestConstructor(
			context, classDecl, args, this, argumentNames);
		bool hasConstructorDeclaration = false;
		if( const asCDecl* const constructClass = context.GetDecl(classDecl) )
		{
			for( asUINT childIndex = 0;
				childIndex < constructClass->children.GetLength(); ++childIndex )
			{
				const asCDecl* const child =
					context.GetDecl(constructClass->children[childIndex]);
				if( child != 0 && child->kind == asAST_DECL_CONSTRUCTOR )
				{
					hasConstructorDeclaration = true;
					break;
				}
			}
		}
		bool hasNamedArgument = false;
		for( asUINT argumentIndex = 0;
			argumentNames != 0
				&& argumentIndex < argumentNames->GetLength(); ++argumentIndex )
		{
			if( (*argumentNames)[argumentIndex].GetLength() != 0 )
			{
				hasNamedArgument = true;
				break;
			}
		}
		callee = selectedConstructor.IsValid()
			? selectedConstructor
			: (!hasConstructorDeclaration && args.GetLength() == 0
				&& !hasNamedArgument ? classDecl : asASTDeclId());
		calleeDecl = context.GetDecl(callee);
	}
	if( callee.IsValid() )
	{
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
		// A type-name invocation may deliberately resolve to the ClassDecl when
		// lookup has not selected one constructor declaration. ActOnConstruct is
		// the authority that selects a constructor from the class children. A
		// ClassDecl has no callable formal list of its own, so do not manufacture
		// CALL records or reject its source arguments here.
		if( calleeDecl != 0 && calleeDecl->kind != asAST_DECL_CLASS )
		{
			ArrangeCallArguments(*this, context, callee, args, names, range,
				implicitReceiver, callArguments);
		}
	}
	const asCArray<asSASTCallArgument>* const constructArguments =
		calleeDecl != 0 && calleeDecl->kind == asAST_DECL_CONSTRUCTOR
			? &callArguments : 0;
	const asASTExprId constructed = ConstructFromCallee(
		*this, context, callee, args, range, constructArguments,
		typeNameTarget);
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
			const asUINT hitParams = context.GetFormalDeclCount(hit->id);
			for( asUINT formalIndex = 0; formalIndex < hitParams; ++formalIndex )
			{
				const asCDecl* const param =
					context.GetFormalDecl(hit->id, formalIndex);
				if( param == 0 )
				{
					continue;
				}
				if( formalIndex )
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
			const asCType* const argType = context.GetType(arg->type.type);
			if( argType )
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
			if( typeCls == 0 && argType != 0
				&& argType->stableKey.GetLength() != 0 )
			{
				asUINT sameNameCount = 0;
				asUINT recordCount = 0;
				for( asUINT declarationIndex = 1;
					declarationIndex <= context.GetDeclCount();
					++declarationIndex )
				{
					const asCDecl* const declaration =
						context.GetDecl(asASTDeclId(declarationIndex));
					if( declaration == 0
						|| (declaration->kind != asAST_DECL_CLASS
							&& declaration->kind != asAST_DECL_INTERFACE) )
					{
						continue;
					}
					if( recordCount < 8 )
					{
						extra.Format(" record%u=%s/%s",
							recordCount,
							declaration->name.GetLength() != 0
								? declaration->name.AddressOf() : "<empty>",
							declaration->stableKey.GetLength() != 0
								? declaration->stableKey.AddressOf() : "<empty>");
						msg += extra;
					}
					++recordCount;
					if( !declaration->name.Equals(argType->stableKey) )
					{
						continue;
					}
					++sameNameCount;
					extra.Format(" sameName%u=%s",
						sameNameCount - 1,
						declaration->stableKey.GetLength() != 0
							? declaration->stableKey.AddressOf() : "<empty>");
					msg += extra;
				}
				extra.Format(" sameNameCount=%u recordCount=%u",
					sameNameCount, recordCount);
				msg += extra;
			}
			if( asCScriptEngine* const resolvedEngine = GetEngine() )
			{
				asCRuntimeTypeBridge bridge(resolvedEngine);
				const asCDataType resolved = bridge.Resolve(context, arg->type);
				const asCTypeInfo* const resolvedInfo = resolved.GetTypeInfo();
				extra.Format(" resolve=%s",
					resolvedInfo && resolvedInfo->GetName() ? resolvedInfo->GetName() : "miss");
				msg += extra;
			}
			const asCDecl* rankParam = 0;
			for( asUINT hitIndex = 0; hitIndex < unresolvedHits.GetLength() && rankParam == 0; ++hitIndex )
			{
				const asCDecl* const hit = context.GetDecl(unresolvedHits[hitIndex]);
				if( hit == 0 )
				{
					continue;
				}
				rankParam = context.GetFormalDecl(hit->id, argIndex);
			}
			if( rankParam )
			{
				extra.Format(" rank%u=%d", argIndex, RankArgument(context, rankParam->type, arg, this));
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
			*this, context, *decl, args, range, implicitReceiver,
			callArguments);
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
	const asASTExprId call = ActOnCall(
		callee, args, type, range, &callArguments);
	if( implicitReceiver.IsValid() )
	{
		context.SetExprReceiver(call, implicitReceiver);
	}
	return ShouldMaterializeCallResult(context, type)
		? ActOnMaterializeTemporary(call, range) : call;
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
	asCASTContext& context,
	const asCArray<asBYTE>* allowedExpressions)
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
			if( allowedExpressions != 0
				&& (expressionIndex >= allowedExpressions->GetLength()
					|| (*allowedExpressions)[expressionIndex] == 0) )
			{
				continue;
			}
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
		if( allowedExpressions != 0
			&& (expressionIndex >= allowedExpressions->GetLength()
				|| (*allowedExpressions)[expressionIndex] == 0) )
		{
			continue;
		}
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

bool asCSema::ReconcileContextualLambdaBody(
	asASTDeclId lambdaId,
	const asCQualType& expectedReturn,
	const asCSourceRange& range)
{
	const asCDecl* const lambda = context.GetDecl(lambdaId);
	if( lambda == 0 || (lambda->traits & asAST_TRAIT_LAMBDA) == 0
		|| !lambda->body.IsValid() || !expectedReturn.IsValid() )
	{
		AddDiagnostic("lambda-contextual-body-invalid", range);
		return false;
	}

	// Build a bounded reachability mask from this Lambda's exact body. Nested
	// Lambda bodies are separate declarations and are intentionally not entered.
	// Contextual inference therefore cannot re-type an unrelated function or a
	// later parser section as a side effect.
	asCArray<asBYTE> reachableStatements;
	reachableStatements.SetLength(context.GetStmtCount() + 1);
	for( asUINT i = 0; i < reachableStatements.GetLength(); ++i )
	{
		reachableStatements[i] = 0;
	}
	asCArray<asBYTE> reachableExpressions;
	reachableExpressions.SetLength(context.GetExprCount() + 1);
	for( asUINT i = 0; i < reachableExpressions.GetLength(); ++i )
	{
		reachableExpressions[i] = 0;
	}
	asCArray<asASTStmtId> statementStack;
	asCArray<asASTExprId> expressionStack;
	statementStack.PushLast(lambda->body);
	while( statementStack.GetLength() != 0 )
	{
		const asASTStmtId statementId = statementStack.PopLast();
		if( !statementId.IsValid()
			|| statementId.value >= reachableStatements.GetLength()
			|| reachableStatements[statementId.value] != 0 )
		{
			continue;
		}
		const asCStmt* const statement = context.GetStmt(statementId);
		if( statement == 0 || statement->owner != lambdaId )
		{
			AddDiagnostic("lambda-contextual-body-owner-invalid", range);
			return false;
		}
		reachableStatements[statementId.value] = 1;
		if( statement->expr.IsValid() )
		{
			expressionStack.PushLast(statement->expr);
		}
		if( const asCDecl* const declaration = context.GetDecl(statement->decl) )
		{
			for( asUINT initIndex = 0;
				initIndex < declaration->inits.GetLength(); ++initIndex )
			{
				expressionStack.PushLast(declaration->inits[initIndex]);
			}
		}
		for( asUINT childIndex = 0;
			childIndex < statement->children.GetLength(); ++childIndex )
		{
			statementStack.PushLast(statement->children[childIndex]);
		}
	}
	while( expressionStack.GetLength() != 0 )
	{
		const asASTExprId expressionId = expressionStack.PopLast();
		if( !expressionId.IsValid()
			|| expressionId.value >= reachableExpressions.GetLength()
			|| reachableExpressions[expressionId.value] != 0 )
		{
			continue;
		}
		const asCExpr* const expression = context.GetExpr(expressionId);
		if( expression == 0 )
		{
			AddDiagnostic("lambda-contextual-expression-invalid", range);
			return false;
		}
		reachableExpressions[expressionId.value] = 1;
		if( expression->receiver.IsValid() )
		{
			expressionStack.PushLast(expression->receiver);
		}
		for( asUINT childIndex = 0;
			childIndex < expression->children.GetLength(); ++childIndex )
		{
			expressionStack.PushLast(expression->children[childIndex]);
		}
	}

	// Parameter identity already existed while parsing the body. Replace only
	// references to this Lambda's own ParamDecls, preserving lexical shadowing
	// and every unrelated declaration binding.
	for( asUINT expressionIndex = 1;
		expressionIndex < reachableExpressions.GetLength(); ++expressionIndex )
	{
		if( reachableExpressions[expressionIndex] == 0 )
		{
			continue;
		}
		const asCExpr* const expression = context.GetExpr(
			asASTExprId(expressionIndex));
		if( expression == 0 || expression->kind != asAST_EXPR_DECL_REF )
		{
			continue;
		}
		const asCDecl* const declaration = context.GetDecl(
			expression->resolvedDecl);
		if( declaration == 0 || declaration->kind != asAST_DECL_PARAM
			|| declaration->parent != lambdaId || !declaration->type.IsValid() )
		{
			continue;
		}
		context.SetExprType(expression->id, declaration->type);
		for( asUINT diagnosticIndex = diagnostics.GetLength();
			diagnosticIndex > 0; --diagnosticIndex )
		{
			const asUINT index = diagnosticIndex - 1;
			if( index < diagnosticRanges.GetLength()
				&& diagnosticRanges[index] == expression->range
				&& diagnostics[index].StartsWith("unresolved-identifier:") )
			{
				diagnostics.RemoveIndex(index);
				diagnosticRanges.RemoveIndex(index);
			}
		}
	}

	ReconcileDeferredPrimitiveBinaryParents(
		*this, context, &reachableExpressions);

	// Return conversions were provisionally selected before the contextual
	// signature existed. Reconcile numeric returns from the exact body result;
	// all other mismatches fail Sema rather than becoming backend inference.
	const asCType* const expectedReturnType = context.GetType(expectedReturn.type);
	for( asUINT statementIndex = 1;
		statementIndex < reachableStatements.GetLength(); ++statementIndex )
	{
		if( reachableStatements[statementIndex] == 0 )
		{
			continue;
		}
		const asCStmt* const statement = context.GetStmt(
			asASTStmtId(statementIndex));
		if( statement == 0 || statement->kind != asAST_STMT_RETURN )
		{
			continue;
		}
		const asCExpr* const returned = context.GetExpr(statement->expr);
		if( returned == 0 )
		{
			if( expectedReturnType == 0
				|| expectedReturnType->kind != asAST_TYPE_VOID )
			{
				AddDiagnostic("lambda-funcdef-return-type-mismatch", statement->range);
				return false;
			}
			continue;
		}
		if( returned->type == expectedReturn )
		{
			continue;
		}
		const asCType* returnedType = 0;
		const asCType* resolvedExpected = 0;
		if( QualTypeIsPrimitiveNumeric(context, returned->type, returnedType)
			&& QualTypeIsPrimitiveNumeric(
				context, expectedReturn, resolvedExpected) )
		{
			const asASTExprId conversion = ActOnConversion(
				returned->id, expectedReturn, statement->range);
			context.SetStmtExpr(statement->id, conversion);
			continue;
		}
		AddDiagnostic("lambda-funcdef-return-type-mismatch", statement->range);
		return false;
	}
	return true;
}

void asCSema::ReconcileDeferredByValueCopyPlans()
{
	// Iterate only expressions that existed when finalization began. A newly
	// authored copy ConstructExpr has a const-reference formal and therefore
	// cannot itself require another by-value copy, but the fixed bound also
	// makes the one-pass ownership transition explicit.
	const asUINT expressionCount = context.GetExprCount();
	for( asUINT expressionIndex = 1;
		expressionIndex <= expressionCount; ++expressionIndex )
	{
		const asCExpr* const call = context.GetExpr(
			asASTExprId(expressionIndex));
		if( call == 0 || (call->kind != asAST_EXPR_CALL
			&& call->kind != asAST_EXPR_CONSTRUCT) )
		{
			continue;
		}

		const asUINT argumentCount = call->callArguments.GetLength();
		for( asUINT argumentIndex = 0;
			argumentIndex < argumentCount; ++argumentIndex )
		{
			const asSASTCallArgument argument =
				call->callArguments[argumentIndex];
			if( argument.origin == asAST_CALL_ARGUMENT_IMPLICIT_RECEIVER
				|| !RequiresExactValueObjectPlan(argument.formalType) )
			{
				continue;
			}

			const asCExpr* const source = context.GetExpr(argument.expression);
			if( source == 0
				|| source->valueCategory != asAST_VALUE_LVALUE )
			{
				// Existing Construct/Materialize/Cleanup plans are prvalues and must
				// remain single-evaluation. Only the raw lvalue shape is incomplete.
				continue;
			}

			asCArray<asASTExprId> copyArguments;
			copyArguments.PushLast(argument.expression);
			const asCSourceRange planRange =
				argument.range.begin.IsValid() ? argument.range : call->range;
			const asASTExprId plan = ActOnConstruct(
				argument.formalType, copyArguments, planRange);
			const asCExpr* const construct = UnwrapByValueCopyPlan(
				context, context.GetExpr(plan));
			const asCDecl* const constructor = construct != 0
				? context.GetDecl(construct->resolvedDecl) : 0;
			const asCDecl* const copyParameter = constructor != 0
				? context.GetFormalDecl(constructor->id, 0) : 0;
			const asASTDeclId valueClass =
				FindNamedTypeDecl(context, argument.formalType);
			const bool exactCopyPlan = construct != 0
				&& construct->kind == asAST_EXPR_CONSTRUCT
				&& construct->type == argument.formalType
				&& constructor != 0
				&& constructor->kind == asAST_DECL_CONSTRUCTOR
				&& constructor->parent == valueClass
				&& context.GetFormalDeclCount(constructor->id) == 1
				&& copyParameter != 0
				&& copyParameter->type.type == argument.formalType.type
				&& copyParameter->type.IsConst()
				&& copyParameter->type.IsReference()
				&& construct->children.GetLength() == 1
				&& construct->children[0] == argument.expression;
			if( !exactCopyPlan )
			{
				AddDiagnostic(
					"by-value-copy-constructor-unavailable", planRange);
				continue;
			}

			const int replacement = context.ReplaceCallArgumentExpression(
				call->id, argumentIndex, plan);
			if( replacement != asAST_VERIFY_OK )
			{
				AddDiagnostic(
					"by-value-copy-relation-rewrite-failed", planRange);
			}
		}
	}
}

void asCSema::ResolveDeferredNames()
{
	// Forward-referenced script classes intern as VALUE_OBJECT before the
	// matching ClassDecl exists. Once the record is finished, rebind those
	// named types onto the authored reference QualType so deferred method
	// lookup and derived-to-base ranking see the completed declaration.
	for( asUINT expressionIndex = 1;
		expressionIndex <= context.GetExprCount(); ++expressionIndex )
	{
		const asCExpr* const expr = context.GetExpr(asASTExprId(expressionIndex));
		if( expr == 0 || !expr->type.IsValid() )
		{
			continue;
		}
		const asCType* const named = context.GetType(expr->type.type);
		if( named == 0 || named->kind != asAST_TYPE_VALUE_OBJECT
			|| named->stableKey.GetLength() == 0 )
		{
			continue;
		}
		const asASTDeclId record = FindNamedTypeDecl(context, expr->type);
		const asCDecl* const declaration = context.GetDecl(record);
		if( declaration == 0
			|| (declaration->kind != asAST_DECL_CLASS
				&& declaration->kind != asAST_DECL_INTERFACE)
			|| (declaration->traits & asAST_TRAIT_VALUE) != 0
			|| !declaration->type.IsValid() )
		{
			continue;
		}
		context.SetExprType(expr->id, declaration->type);
	}

	// A generated script record can be appended by the UE preprocessor after an
	// earlier source section has already default-constructed that exact type.
	// ActOnConstruct deliberately leaves the constructor identity invalid when
	// no declaration exists yet; retry only the zero-argument shape once later
	// record finalization has published its generated default constructor. The
	// expression's exact QualType remains the lookup authority, so this does not
	// manufacture a backend fallback or pick a same-spelled declaration.
	for( asUINT expressionIndex = 1;
		expressionIndex <= context.GetExprCount(); ++expressionIndex )
	{
		const asCExpr* const construct = context.GetExpr(
			asASTExprId(expressionIndex));
		if( construct == 0 || construct->kind != asAST_EXPR_CONSTRUCT
			|| construct->resolvedDecl.IsValid()
			|| construct->children.GetLength() != 0
			|| construct->callArguments.GetLength() != 0 )
		{
			continue;
		}

		const asASTDeclId record = FindNamedTypeDecl(
			context, construct->type);
		asCArray<asASTExprId> noArguments;
		const asASTDeclId constructor = FindBestConstructor(
			context, record, noArguments, this, 0);
		const asCDecl* const declaration = context.GetDecl(constructor);
		if( declaration == 0
			|| declaration->kind != asAST_DECL_CONSTRUCTOR
			|| context.GetFormalDeclCount(constructor) != 0 )
		{
			continue;
		}

		context.SetResolvedDecl(construct->id, constructor);
		context.SetLiteral(
			construct->id,
			declaration->stableKey.GetLength() != 0
				? declaration->stableKey.AddressOf()
				: declaration->name.AddressOf());
	}

	// AngelScript lets an earlier method body name a later script class field.
	// ActOnMemberExpr records an unbound MEMBER_REF when the record body has
	// not been parsed yet. Rebind those edges after every class in the
	// translation unit has published its fields, matching Clang's delayed
	// lookup after a complete type. Immediate ActOnMemberExpr rewrites
	// primitive/POD reads to GetX and keeps exact-value (TMap/TArray) fields
	// as MEMBER_REF. This pass only fills resolvedDecl on leftover MEMBER_REF
	// (non-POD fields and forward-referenced names).
	for( asUINT expressionIndex = 1;
		expressionIndex <= context.GetExprCount(); ++expressionIndex )
	{
		const asCExpr* const memberRef = context.GetExpr(
			asASTExprId(expressionIndex));
		if( memberRef == 0
			|| memberRef->kind != asAST_EXPR_MEMBER_REF
			|| memberRef->resolvedDecl.IsValid()
			|| memberRef->children.GetLength() == 0
			|| memberRef->literal.GetLength() == 0 )
		{
			continue;
		}

		const asCExpr* const baseExpr =
			context.GetExpr(memberRef->children[0]);
		if( baseExpr == 0 || !baseExpr->type.IsValid() )
		{
			continue;
		}

		InternNativePropertyCandidates(
			baseExpr->type, memberRef->literal.AddressOf());
		const asASTDeclId field = FindTypeMemberVarFromBases(
			context,
			FindNamedTypeDecl(context, baseExpr->type),
			memberRef->literal.AddressOf());
		const asCDecl* const fieldDecl = context.GetDecl(field);
		if( fieldDecl == 0 || fieldDecl->kind != asAST_DECL_VAR )
		{
			continue;
		}

		context.SetResolvedDecl(memberRef->id, field);
		if( fieldDecl->type.IsValid() )
		{
			context.SetExprType(memberRef->id, fieldDecl->type);
		}
	}

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
			deferred.name.AddressOf(), args, &deferred.argumentNames,
			exactScope);
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
		asCArray<asSASTCallArgument> callArguments;
		ArrangeCallArguments(*this, context, callee, args, argumentNames,
			deferred.range, effectiveReceiver, callArguments);

		ConvertCallArgumentsToFormalTypes(
			*this, context, *decl, args, deferred.range,
			effectiveReceiver, callArguments);

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
		context.ClearCallArguments(deferred.expression);
		for( asUINT i = args.GetLength(); i > 0; --i )
		{
			context.AddExprChild(deferred.expression, args[i - 1]);
		}
		for( asUINT i = callArguments.GetLength(); i > 0; --i )
		{
			context.AddCallArgument(
				deferred.expression, callArguments[i - 1]);
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
	ReconcileDeferredPrimitiveBinaryParents(*this, context, 0);
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
					asCArray<asSASTCallArgument> callArguments;
					if( argumentNames != 0 )
					{
						names = *argumentNames;
					}
					ArrangeCallArguments(
						*this, context, callee->id, callArgs, names,
						range, asASTExprId(), callArguments);
					ConvertCallArgumentsToFormalTypes(
						*this, context, *callee, callArgs,
						range, asASTExprId(), callArguments);
					asCQualType type = context.InternPrimitive(ttInt, 0);
					if( callee->type.IsValid() )
					{
						type = callee->type;
					}
					return ActOnCall(
						callee->id, callArgs, type, range, &callArguments);
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

asASTExprId asCSema::ActOnConstructCall(
	asASTDeclId owner,
	const asCQualType& type,
	const asCArray<asASTExprId>& args,
	const asCArray<asCString>* argumentNames,
	const asCSourceRange& range)
{
	const asCQualType constructType =
		QualTypeForConstructExpression(context, type);
	const asCType* const target = context.GetType(constructType.type);
	if( !constructType.IsValid() || target == 0
		|| target->stableKey.GetLength() == 0
		|| (argumentNames != 0
			&& argumentNames->GetLength() != 0
			&& argumentNames->GetLength() != args.GetLength()) )
	{
		AddDiagnostic("construct-call-action-invalid", range);
		return asASTExprId();
	}
	const asCArray<asCString>* const names = argumentNames != 0
		&& argumentNames->GetLength() != 0 ? argumentNames : 0;
	bool hasNamedArgument = false;
	for( asUINT argumentIndex = 0;
		names != 0 && argumentIndex < names->GetLength(); ++argumentIndex )
	{
		if( (*names)[argumentIndex].GetLength() != 0 )
		{
			hasNamedArgument = true;
			break;
		}
	}
	InternNativeCallablesForType(constructType);
	const asASTDeclId classDecl = FindNamedTypeDecl(context, constructType);
	const asASTDeclId constructor = FindBestConstructor(
		context, classDecl, args, this, names);
	if( constructor.IsValid() )
	{
		asCArray<asASTExprId> arrangedArgs = args;
		asCArray<asCString> arrangedNames;
		if( names != 0 )
		{
			arrangedNames = *names;
		}
		asCArray<asSASTCallArgument> callArguments;
		ArrangeCallArguments(
			*this, context, constructor, arrangedArgs, arrangedNames, range,
			asASTExprId(), callArguments);
		return ActOnConstruct(
			constructType, arrangedArgs, range, constructor, &callArguments);
	}

	if( hasNamedArgument )
	{
		// Preserve the ordinary call diagnostic for an invalid named plan. This
		// route may also resolve a declaration that is not indexed by the exact
		// nominal-type relation yet, but it must retain the source target type.
		return ActOnCallExpr(
			owner,
			target->stableKey.AddressOf(),
			args,
			range,
			asASTExprId(),
			names,
			false,
			&constructType);
	}

	// Retain the existing positional constructor-selection semantics for
	// compatibility shapes such as maintained-fork template factories whose
	// Runtime hidden-formal view differs from the authored source fixture. The
	// fallback still publishes one complete exact relation; reclassify only the
	// source-backed generated placeholders as positional provenance.
	const asASTExprId result = ActOnConstruct(constructType, args, range);
	asCExpr* construct = const_cast<asCExpr*>(context.GetExpr(result));
	while( construct != 0
		&& (construct->kind == asAST_EXPR_CLEANUP
			|| construct->kind == asAST_EXPR_MATERIALIZE_TEMPORARY)
		&& construct->children.GetLength() == 1 )
	{
		construct = const_cast<asCExpr*>(
			context.GetExpr(construct->children[0]));
	}
	if( construct != 0 && construct->kind == asAST_EXPR_CONSTRUCT
		&& construct->children.GetLength() == args.GetLength()
		&& construct->callArguments.GetLength() == args.GetLength() )
	{
		for( asUINT argumentIndex = 0;
			argumentIndex < construct->callArguments.GetLength(); ++argumentIndex )
		{
			asSASTCallArgument& argument =
				construct->callArguments[argumentIndex];
			if( argument.origin == asAST_CALL_ARGUMENT_GENERATED )
			{
				argument.origin = asAST_CALL_ARGUMENT_POSITIONAL;
				argument.sourceOrdinal = argumentIndex;
			}
		}
	}
	return result;
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
	if( action.argumentNames.GetLength() != 0
		&& action.argumentNames.GetLength() != action.arguments.GetLength() )
	{
		AddDiagnostic("construct-expression-action-argument-names-invalid", range);
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
		for( asUINT argumentIndex = 0;
			argumentIndex < action.argumentNames.GetLength(); ++argumentIndex )
		{
			if( action.argumentNames[argumentIndex].GetLength() != 0 )
			{
				AddDiagnostic(
					"construct-expression-action-named-scalar-invalid", range);
				return asASTExprId();
			}
		}
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
		expression = ActOnConstructCall(
			CurrentDeclContext(),
			action.targetType,
			action.arguments,
			action.argumentNames.GetLength() != 0
				? &action.argumentNames : 0,
			range);
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

static const char* AssignOperatorMethodName(const char* op)
{
	if( op == 0 || op[0] == 0 )
	{
		return 0;
	}
	if( op[0] == '=' && op[1] == 0 )
	{
		return "opAssign";
	}
	if( std::strcmp(op, "+=") == 0 )
	{
		return "opAddAssign";
	}
	if( std::strcmp(op, "-=") == 0 )
	{
		return "opSubAssign";
	}
	if( std::strcmp(op, "*=") == 0 )
	{
		return "opMulAssign";
	}
	if( std::strcmp(op, "/=") == 0 )
	{
		return "opDivAssign";
	}
	if( std::strcmp(op, "%=") == 0 )
	{
		return "opModAssign";
	}
	if( std::strcmp(op, "**=") == 0 )
	{
		return "opPowAssign";
	}
	if( std::strcmp(op, "&=") == 0 )
	{
		return "opAndAssign";
	}
	if( std::strcmp(op, "|=") == 0 )
	{
		return "opOrAssign";
	}
	if( std::strcmp(op, "^=") == 0 )
	{
		return "opXorAssign";
	}
	if( std::strcmp(op, "<<=") == 0 )
	{
		return "opShlAssign";
	}
	if( std::strcmp(op, ">>=") == 0 )
	{
		return "opShrAssign";
	}
	if( std::strcmp(op, ">>>=") == 0 )
	{
		return "opUShrAssign";
	}
	return 0;
}

static asASTExprId TryRewriteOverloadedAssignmentToCall(
	asCSema& sema,
	asCScriptEngine* engine,
	asCASTContext& context,
	asASTExprId lhs,
	asASTExprId rhs,
	const asCSourceRange& range,
	const char* methodName)
{
	const asCExpr* const leftExpr = context.GetExpr(lhs);
	const asCExpr* const rightExpr = context.GetExpr(rhs);
	if( leftExpr == 0 || rightExpr == 0 || !leftExpr->type.IsValid()
		|| methodName == 0 || methodName[0] == 0 )
	{
		return asASTExprId();
	}

	const asCType* const leftType = context.GetType(leftExpr->type.type);
	if( leftType == 0
		|| leftType->kind == asAST_TYPE_PRIMITIVE
		|| leftType->kind == asAST_TYPE_ENUM
		|| leftType->kind == asAST_TYPE_VOID )
	{
		return asASTExprId();
	}

	if( engine != 0 )
	{
		InternNativeMethods(sema, engine, context,
			leftExpr->type, methodName);
	}
	asCArray<asASTExprId> args;
	args.PushLast(rhs);
	const asASTDeclId selected = FindBestCallee(
		context,
		FindNamedTypeDecl(context, leftExpr->type),
		methodName,
		args,
		&sema,
		&leftExpr->type);
	const asCDecl* const method = context.GetDecl(selected);
	if( method == 0
		|| method->kind != asAST_DECL_METHOD
		|| !method->name.Equals(methodName)
		|| (method->traits & (asAST_TRAIT_GENERATED | asAST_TRAIT_EXTERNAL)) != 0 )
	{
		return asASTExprId();
	}

	// Clang publishes overloaded `=` / `+=` as calls. Rank against the authored
	// rhs before ActOnAssign converts it to the lhs type; otherwise
	// `Object += 7` becomes int→T conversion and CodeGen reinterprets leftover
	// Assign the same way `Object = 7` did.
	asASTExprId receiver = lhs;
	if( leftExpr->valueCategory != asAST_VALUE_LVALUE )
	{
		// Make() += 7 mutates a VALUE temporary. Materialize it so CodeGen
		// owns storage for `this` instead of calling through a discarded
		// prvalue.
		receiver = sema.ActOnMaterializeTemporary(lhs, range);
		if( !receiver.IsValid() )
		{
			return asASTExprId();
		}
	}
	asCArray<asCString> names;
	asCArray<asSASTCallArgument> callArguments;
	ArrangeCallArguments(
		sema, context, selected, args, names, range, receiver, callArguments);
	ConvertCallArgumentsToFormalTypes(
		sema, context, *method, args, range, receiver, callArguments);
	asCQualType resultType = method->type.IsValid()
		? method->type : leftExpr->type;
	const asASTExprId call = sema.ActOnCall(
		selected, args, resultType, range, &callArguments);
	if( call.IsValid() && receiver.IsValid() )
	{
		context.SetExprReceiver(call, receiver);
	}
	return call;
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

static bool IsDeclarationInitializerAssignment(
	const asCASTContext& context,
	asASTExprId assign)
{
	const asCExpr* const assignExpr = context.GetExpr(assign);
	if( assignExpr == 0 || assignExpr->kind != asAST_EXPR_ASSIGN
		|| assignExpr->children.GetLength() != 2 )
	{
		return false;
	}

	const asCExpr* const target = context.GetExpr(assignExpr->children[0]);
	const asCDecl* const variable = target != 0
		&& target->kind == asAST_EXPR_DECL_REF
		? context.GetDecl(target->resolvedDecl) : 0;
	if( variable == 0 || variable->kind != asAST_DECL_VAR )
	{
		return false;
	}

	// A typed local declaration is lowered to a sibling Assign so statement
	// consumers have an explicit execution edge, while VarDecl::inits remains
	// the authoritative initialization relation. ActOnAssign may insert a
	// representation conversion around that exact initializer, so compare the
	// wrapper and its conversion source. This identity test cannot classify a
	// later authored assignment, whose rhs owns a distinct expression id.
	asASTExprId candidate = assignExpr->children[1];
	for( ;; )
	{
		for( asUINT initIndex = 0;
			initIndex < variable->inits.GetLength(); ++initIndex )
		{
			if( variable->inits[initIndex] == candidate )
			{
				return true;
			}
		}

		const asCExpr* const candidateExpr = context.GetExpr(candidate);
		if( candidateExpr == 0
			|| candidateExpr->kind != asAST_EXPR_CONVERSION
			|| candidateExpr->children.GetLength() != 1 )
		{
			return false;
		}
		candidate = candidateExpr->children[0];
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
			const asCDecl* const leftCallee = left->resolvedDecl.IsValid()
				? context.GetDecl(left->resolvedDecl) : 0;
			if( left->kind == asAST_EXPR_CALL
				&& leftCallee != 0
				&& leftCallee->name.Equals("opIndex")
				&& left->receiver.IsValid() )
			{
				const asASTExprId indexBase = left->receiver;
				asCSourceRange ovRange = range;
				if( const asCExpr* baseExpr = context.GetExpr(indexBase) )
				{
					ovRange = baseExpr->range;
				}
				asASTExprId ov = FindExistingExpr(
					context, asAST_EXPR_OPAQUE_VALUE, ovRange);
				if( !ov.IsValid() )
				{
					ov = ActOnOpaqueValueExpr(indexBase, ovRange);
				}
				asCArray<asASTExprId> indexArgs;
				indexArgs.SetLength(left->callArguments.GetLength());
				for( asUINT argumentIndex = 0;
					argumentIndex < left->callArguments.GetLength();
					++argumentIndex )
				{
					const asSASTCallArgument& argument =
						left->callArguments[argumentIndex];
					if( argument.formalIndex >= indexArgs.GetLength() )
					{
						indexArgs.SetLength(argument.formalIndex + 1);
					}
					indexArgs[argument.formalIndex] = argument.expression;
				}
				asCArray<asCString> names;
				asCArray<asSASTCallArgument> callArguments;
				ArrangeCallArguments(
					*this, context, left->resolvedDecl, indexArgs, names,
					left->range, ov, callArguments);
				asCQualType idxType = left->type.IsValid()
					? left->type : context.InternPrimitive(ttInt, 0);
				const asASTExprId rewritten = ActOnCall(
					left->resolvedDecl, indexArgs, idxType, left->range,
					&callArguments);
				context.SetExprReceiver(rewritten, ov);
				const asASTExprId add = ActOnBinaryExpr(
					rewritten, "+", rhs,
					rhs.IsValid() && context.GetExpr(rhs)
						? context.GetExpr(rhs)->range : range);
				const asASTExprId write = ActOnAssign(
					rewritten, add, idxType, range);
				asCArray<asASTExprId> parts;
				parts.PushLast(ov);
				parts.PushLast(rewritten);
				parts.PushLast(add);
				parts.PushLast(write);
				const asASTExprId seq = ActOnSequenceExpr(parts, range);
				if( seq.IsValid() )
				{
					context.SetLiteral(seq, "opaque");
				}
				return seq;
			}
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
	if( const char* const assignMethod = AssignOperatorMethodName(op) )
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
			if( (wrap->kind == asAST_EXPR_MATERIALIZE_TEMPORARY
					|| wrap->kind == asAST_EXPR_CLEANUP)
				&& wrap->children.GetLength() > 0 )
			{
				target = wrap->children[0];
				continue;
			}
			break;
		}
		const asCExpr* const left = context.GetExpr(target);
		if( left != 0 && !left->type.IsConst() )
		{
			// Overloaded += is a method Call, not a store. VALUE temporaries
			// such as Make() += 7 are valid receivers, matching LEGACY
			// CompileOverloadedDualOperator. The leftover Assign path below
			// still rejects non-lvalue primitive stores.
			const asASTExprId rewrittenCall = TryRewriteOverloadedAssignmentToCall(
				*this, engine, context, target, rhs, range, assignMethod);
			if( rewrittenCall.IsValid() )
			{
				return rewrittenCall;
			}
		}
	}
	const asASTExprId assign = ActOnAssign(lhs, rhs, fallback, range);
	if( assign.IsValid()
		&& !IsDeclarationInitializerAssignment(context, assign) )
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
			if( (wrap->kind == asAST_EXPR_MATERIALIZE_TEMPORARY
					|| wrap->kind == asAST_EXPR_CLEANUP)
				&& wrap->children.GetLength() > 0 )
			{
				target = wrap->children[0];
				continue;
			}
			break;
		}
		const asCExpr* const left = context.GetExpr(target);
		if( left == 0
			|| left->valueCategory != asAST_VALUE_LVALUE
			|| left->type.IsConst() )
		{
			AddDiagnostic("expression-not-assignable", range);
		}
	}
	if( assign.IsValid() && op[0] == '=' && op[1] == 0
		&& !IsDeclarationInitializerAssignment(context, assign) )
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
	const asCExpr* baseExpr = context.GetExpr(base);
	if( baseExpr && engine )
	{
		InternNativePropertyCandidates(baseExpr->type, name);
	}
	asCQualType fieldType = intType;
	asASTDeclId member;
	if( baseExpr )
	{
		member = FindTypeMemberVarFromBases(
			context, FindNamedTypeDecl(context, baseExpr->type), name);
		if( const asCDecl* field = context.GetDecl(member) )
		{
			if( field->type.IsValid() )
			{
				fieldType = field->type;
			}
		}
	}
	// Clang MemberExpr is storage/lvalue. Generated GetX is by-value sugar.
	// A stored non-POD / TMap / TArray field used as an Iterator or other
	// exact-value receiver must stay MEMBER_REF; GetActorMap() would copy.
	// Primitive/POD reads still rewrite to GetX when a getter binds.
	if( member.IsValid() && RequiresExactValueObjectPlan(fieldType) )
	{
		const asASTExprId id = ActOnMemberRef(base, name ? name : "", fieldType, range);
		context.SetResolvedDecl(id, member);
		return id;
	}
	const asASTExprId rewritten = TryRewritePropertyGet(
		*this, context, base, name,
		fieldType.IsValid() ? fieldType : intType, range);
	if( rewritten.IsValid() )
	{
		return rewritten;
	}
	if( member.IsValid() )
	{
		const asASTExprId id = ActOnMemberRef(base, name ? name : "", fieldType, range);
		context.SetResolvedDecl(id, member);
		return id;
	}
	return ActOnMemberRef(base, name ? name : "", fieldType, range);
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
		asCArray<asASTExprId> args = indices;
		asCArray<asCString> names;
		asCArray<asSASTCallArgument> callArguments;
		if( argumentNames != 0 )
		{
			names = *argumentNames;
		}
		// Clang publishes overloaded [] as a call with an exact callee,
		// receiver and argument plan. Keep a leftover Index only when no
		// opIndex declaration exists (builtin array indexing).
		ArrangeCallArguments(
			*this, context, opIndex, args, names, range, base, callArguments);
		ConvertCallArgumentsToFormalTypes(
			*this, context, *method, args, range, base, callArguments);
		if( method->type.IsValid() )
		{
			resultType = method->type;
		}
		const asASTExprId call = ActOnCall(
			opIndex, args, resultType, range, &callArguments);
		if( base.IsValid() )
		{
			context.SetExprReceiver(call, base);
		}
		return MaterializeOwningIndexResult(*this, context, call, range);
	}
	const asASTExprId existing = FindExistingExactIndex(
		context, base, indices, range);
	if( existing.IsValid() )
	{
		return MaterializeOwningIndexResult(
			*this, context, existing, range);
	}
	const asASTExprId value = ActOnIndex(base, indices, resultType, range);
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

static asASTExprId UnwrapMutationTargetExpr(asCASTContext& context, asASTExprId target)
{
	for( ;; )
	{
		const asCExpr* const wrap = context.GetExpr(target);
		if( wrap == 0 )
		{
			break;
		}
		if( wrap->kind == asAST_EXPR_SEQUENCE && wrap->children.GetLength() > 0 )
		{
			target = wrap->children[wrap->children.GetLength() - 1];
			continue;
		}
		if( (wrap->kind == asAST_EXPR_MATERIALIZE_TEMPORARY
				|| wrap->kind == asAST_EXPR_CLEANUP)
			&& wrap->children.GetLength() > 0 )
		{
			target = wrap->children[0];
			continue;
		}
		break;
	}
	return target;
}

// Clang prefix/postfix increment of a pseudo-object: OpaqueValue of the base,
// load, increment, store. Postfix yields the loaded value; prefix yields the
// incremented value. Compound += already does this for Get/opIndex; leftover
// Unary pre++/post++ is not an lvalue for CodeGen.
static asASTExprId RewritePostfixPropertyOrIndexMutation(
	asCSema& sema,
	asCASTContext& context,
	asASTExprId inner,
	bool decrement,
	const asCSourceRange& range,
	bool yieldOriginal)
{
	const asASTExprId target = UnwrapMutationTargetExpr(context, inner);
	const asCExpr* const left = context.GetExpr(target);
	const asCDecl* const leftCallee = left != 0 && left->resolvedDecl.IsValid()
		? context.GetDecl(left->resolvedDecl) : 0;
	if( left == 0 )
	{
		return asASTExprId();
	}

	const asASTExprId one = sema.ActOnIntegerLiteral(1, range);
	const char* const binOp = decrement ? "-" : "+";
	if( !one.IsValid() )
	{
		return asASTExprId();
	}

	if( left->kind == asAST_EXPR_CALL && leftCallee != 0
		&& leftCallee->name.GetLength() > 3
		&& std::strncmp(leftCallee->name.AddressOf(), "Get", 3) == 0 )
	{
		asASTExprId receiver = left->receiver.IsValid()
			? left->receiver
			: (left->children.GetLength() > 0
				? left->children[left->children.GetLength() - 1]
				: asASTExprId());
		if( !receiver.IsValid() )
		{
			return asASTExprId();
		}
		asCSourceRange ovRange = range;
		if( const asCExpr* const recv = context.GetExpr(receiver) )
		{
			ovRange = recv->range;
		}
		asASTExprId ov = FindExistingExpr(
			context, asAST_EXPR_OPAQUE_VALUE, ovRange);
		if( !ov.IsValid() )
		{
			ov = sema.ActOnOpaqueValueExpr(receiver, ovRange);
		}
		asCArray<asASTExprId> getArgs;
		getArgs.PushLast(ov);
		asCQualType getType = left->type.IsValid()
			? left->type : context.InternPrimitive(ttInt, 0);
		const asASTExprId get = sema.ActOnCall(
			left->resolvedDecl, getArgs, getType, left->range);
		context.SetExprReceiver(get, ov);
		const asASTExprId orig = sema.ActOnOpaqueValueExpr(get, range);
		const asASTExprId add = sema.ActOnBinaryExpr(
			orig, binOp, one, range);
		asCQualType fallback = context.InternPrimitive(ttInt, 0);
		const asASTExprId set = TryRewritePropertySet(
			sema, context, get, add, fallback, range);
		if( !set.IsValid() )
		{
			return asASTExprId();
		}
		asCArray<asASTExprId> parts;
		parts.PushLast(ov);
		parts.PushLast(orig);
		parts.PushLast(add);
		parts.PushLast(set);
		parts.PushLast(yieldOriginal ? orig : add);
		return sema.ActOnSequenceExpr(parts, range);
	}

	if( left->kind == asAST_EXPR_CALL && leftCallee != 0
		&& leftCallee->name.Equals("opIndex")
		&& left->receiver.IsValid()
		&& left->valueCategory == asAST_VALUE_LVALUE )
	{
		const asASTExprId indexBase = left->receiver;
		asCSourceRange ovRange = range;
		if( const asCExpr* const baseExpr = context.GetExpr(indexBase) )
		{
			ovRange = baseExpr->range;
		}
		asASTExprId ov = FindExistingExpr(
			context, asAST_EXPR_OPAQUE_VALUE, ovRange);
		if( !ov.IsValid() )
		{
			ov = sema.ActOnOpaqueValueExpr(indexBase, ovRange);
		}
		asCArray<asASTExprId> indexArgs;
		indexArgs.SetLength(left->callArguments.GetLength());
		for( asUINT argumentIndex = 0;
			argumentIndex < left->callArguments.GetLength();
			++argumentIndex )
		{
			const asSASTCallArgument& argument =
				left->callArguments[argumentIndex];
			if( argument.formalIndex >= indexArgs.GetLength() )
			{
				indexArgs.SetLength(argument.formalIndex + 1);
			}
			indexArgs[argument.formalIndex] = argument.expression;
		}
		asCArray<asCString> names;
		asCArray<asSASTCallArgument> callArguments;
		ArrangeCallArguments(
			sema, context, left->resolvedDecl, indexArgs, names,
			left->range, ov, callArguments);
		asCQualType idxType = left->type.IsValid()
			? left->type : context.InternPrimitive(ttInt, 0);
		const asASTExprId rewritten = sema.ActOnCall(
			left->resolvedDecl, indexArgs, idxType, left->range,
			&callArguments);
		context.SetExprReceiver(rewritten, ov);
		const asASTExprId loaded = DecayScalarLValueReferenceToRValue(
			sema, context, rewritten, range);
		const asASTExprId orig = sema.ActOnOpaqueValueExpr(loaded, range);
		const asASTExprId add = sema.ActOnBinaryExpr(
			orig, binOp, one, range);
		const asASTExprId write = sema.ActOnAssign(
			rewritten, add, idxType, range);
		if( !write.IsValid() )
		{
			return asASTExprId();
		}
		asCArray<asASTExprId> parts;
		parts.PushLast(ov);
		parts.PushLast(rewritten);
		parts.PushLast(orig);
		parts.PushLast(add);
		parts.PushLast(write);
		parts.PushLast(yieldOriginal ? orig : add);
		return sema.ActOnSequenceExpr(parts, range);
	}

	return asASTExprId();
}

// LEGACY CompileUnaryOperator / CompileCondition convert VALUE objects with
// `bool opImplConv()` before treating the value as a boolean. Canonical must
// seal that Call rather than leftover Unary/Conditional of T.
asASTExprId RewriteValueToBoolViaOpImplConv(
	asCSema& sema,
	asCASTContext& context,
	asCScriptEngine* engine,
	asASTExprId inner,
	const asCSourceRange& range)
{
	const asCExpr* innerExpr = context.GetExpr(inner);
	if( innerExpr == 0 )
	{
		return inner;
	}
	const asCType* const innerType = context.GetType(innerExpr->type.type);
	if( innerType != 0
		&& innerType->kind == asAST_TYPE_PRIMITIVE
		&& innerType->primitiveToken == ttBool )
	{
		return inner;
	}
	if( engine != 0 )
	{
		InternNativeMethods(sema, engine, context, innerExpr->type, "opImplConv");
	}
	const asCQualType boolType = context.InternPrimitive(ttBool, 0);
	const asASTDeclId typeDecl = FindNamedTypeDecl(context, innerExpr->type);
	asCArray<asASTExprId> opArgs;
	const asASTDeclId callee = FindBestCallee(
		context, typeDecl, "opImplConv", opArgs, &sema);
	const asCDecl* method = context.GetDecl(callee);
	if( method == 0
		|| method->kind != asAST_DECL_METHOD
		|| !method->name.Equals("opImplConv") )
	{
		return inner;
	}
	asCQualType convType = method->type.IsValid() ? method->type : boolType;
	asCArray<asASTExprId> callArgs;
	asCArray<asCString> names;
	asCArray<asSASTCallArgument> callArguments;
	ArrangeCallArguments(
		sema, context, callee, callArgs, names, range, inner, callArguments);
	ConvertCallArgumentsToFormalTypes(
		sema, context, *method, callArgs, range, inner, callArguments);
	asASTExprId conv = sema.ActOnCall(
		callee, callArgs, convType, range, &callArguments);
	if( conv.IsValid() && inner.IsValid() )
	{
		context.SetExprReceiver(conv, inner);
	}
	if( ShouldMaterializeCallResult(context, convType) )
	{
		conv = sema.ActOnMaterializeTemporary(conv, range);
	}
	return conv.IsValid() ? conv : inner;
}

asASTExprId asCSema::ActOnUnaryExpr(const char* op, asASTExprId inner, const asCSourceRange& range, bool postfix)
{
	const asCQualType intType = context.InternPrimitive(ttInt, 0);
	const char* methodName = 0;
	if( op && op[0] == '-' && op[1] == 0 )
	{
		methodName = "opNeg";
	}
	else if( op && op[0] == '~' && op[1] == 0 )
	{
		// LEGACY CompileUnaryOperator maps ttBitNot on objects to opCom.
		// `~Object` must be a Sema Call, not leftover Unary ~ of T typed as int.
		methodName = "opCom";
	}
	else if( op && op[0] == '+' && op[1] == '+' && op[2] == 0 )
	{
		methodName = postfix ? "opPostInc" : "opPreInc";
	}
	else if( op && op[0] == '-' && op[1] == '-' && op[2] == 0 )
	{
		methodName = postfix ? "opPostDec" : "opPreDec";
	}
	if( methodName
		&& (std::strcmp(methodName, "opPostInc") == 0
			|| std::strcmp(methodName, "opPostDec") == 0
			|| std::strcmp(methodName, "opPreInc") == 0
			|| std::strcmp(methodName, "opPreDec") == 0) )
	{
		const asASTExprId rewritten = RewritePostfixPropertyOrIndexMutation(
			*this, context, inner,
			std::strcmp(methodName, "opPostDec") == 0
				|| std::strcmp(methodName, "opPreDec") == 0,
			range,
			postfix);
		if( rewritten.IsValid() )
		{
			return rewritten;
		}
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
					// Unary operator methods take the operand as receiver.
					// Pushing it as a positional child made 0-arg opCom look
					// like a one-argument Call.
					asCArray<asASTExprId> callArgs;
					asCArray<asCString> names;
					asCArray<asSASTCallArgument> callArguments;
					ArrangeCallArguments(
						*this, context, callee, callArgs, names, range,
						inner, callArguments);
					ConvertCallArgumentsToFormalTypes(
						*this, context, *method, callArgs, range, inner,
						callArguments);
					asASTExprId call = FindExistingExactUnaryCall(
						context, range, callee, inner);
					if( !call.IsValid() )
					{
						call = ActOnCall(
							callee, callArgs, type, range, &callArguments);
						if( call.IsValid() && inner.IsValid() )
						{
							context.SetExprReceiver(call, inner);
						}
					}
					return ShouldMaterializeCallResult(context, type)
						? ActOnMaterializeTemporary(call, range) : call;
				}
			}
		}
	}
	if( op && op[0] == '!' && op[1] == 0 )
	{
		// LEGACY CompileUnaryOperator converts VALUE objects with
		// `bool opImplConv()` then emits NOT. `!Object` must be Unary ! of
		// that Call, not leftover Unary ! of T typed as int.
		const asASTExprId conv = RewriteValueToBoolViaOpImplConv(
			*this, context, engine, inner, range);
		if( conv.IsValid() && conv.value != inner.value )
		{
			const asCQualType boolType = context.InternPrimitive(ttBool, 0);
			const asASTExprId existingNot =
				FindExistingExactUnary(context, range, "!", conv);
			if( existingNot.IsValid() )
			{
				return existingNot;
			}
			return ActOnUnary("!", conv, boolType, range);
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
	// Logical not is always bool: already-bool `!Flag` must not leftover
	// Unary ! typed as int after the VALUE opImplConv rewrite returns the
	// same inner expression.
	asCQualType resultType = intType;
	if( op && op[0] == '!' && op[1] == 0 )
	{
		resultType = context.InternPrimitive(ttBool, 0);
	}
	else if( (canonicalOp[0] == 'p' && canonicalOp[1] != 0) )
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
			&& owner->stableKey.Equals(type->stableKey) )
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
	if( std::strcmp(op, "**") == 0 )
	{
		return "opPow";
	}
	if( std::strcmp(op, "|") == 0 )
	{
		return "opOr";
	}
	if( std::strcmp(op, "&") == 0 )
	{
		return "opAnd";
	}
	if( std::strcmp(op, "^") == 0 )
	{
		return "opXor";
	}
	if( std::strcmp(op, "<<") == 0 )
	{
		return "opShl";
	}
	if( std::strcmp(op, ">>") == 0 )
	{
		return "opShr";
	}
	if( std::strcmp(op, ">>>") == 0 )
	{
		return "opUShr";
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

static const char* InvertComparisonOperator(const char* op)
{
	if( op == 0 )
	{
		return 0;
	}
	if( std::strcmp(op, "<") == 0 )
	{
		return ">";
	}
	if( std::strcmp(op, ">") == 0 )
	{
		return "<";
	}
	if( std::strcmp(op, "<=") == 0 )
	{
		return ">=";
	}
	if( std::strcmp(op, ">=") == 0 )
	{
		return "<=";
	}
	return op;
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
	// LEGACY CompileCondition converts VALUE objects with `bool opImplConv()`
	// before the branch. `Object ? 42 : 0` must be Conditional of that Call,
	// not leftover cond DeclRef of T.
	cond = RewriteValueToBoolViaOpImplConv(
		*this, context, engine, cond, range);
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

static asASTExprId DecayScalarLValueReferenceToRValue(
	asCSema& sema,
	asCASTContext& context,
	asASTExprId expr,
	const asCSourceRange& range)
{
	const asCExpr* const value = context.GetExpr(expr);
	if( value == 0 || !value->type.IsReference() || value->type.IsHandle() )
	{
		return expr;
	}
	// EmitCall/EmitIndex of T& leave a pointer slot. EmitDeclRef of a
	// parameter or local T& already loads the pointee, so wrapping that
	// DeclRef in LValueToRValue double-dereferences and crashes
	// (`Iterator = Iterator + 1` in foreach opForNext).
	if( value->kind != asAST_EXPR_CALL && value->kind != asAST_EXPR_INDEX )
	{
		return expr;
	}
	const asCType* const named = context.GetType(value->type.type);
	if( named == 0
		|| (named->kind != asAST_TYPE_PRIMITIVE
			&& named->kind != asAST_TYPE_ENUM) )
	{
		return expr;
	}
	asCQualType dest = value->type;
	// LValueToRValue is a value, not a parameter. Stripping only REFERENCE
	// leaves IN/OUT/INOUT on a non-reference QualType, which
	// asASTQualifiersAreValid rejects (`const T&in` Call results).
	dest.quals &= ~(asAST_QUAL_REFERENCE | asAST_QUAL_PARAM_DIR_MASK);
	return sema.ActOnConversion(expr, dest, range);
}

static asASTExprId TryRewriteOverloadedBinaryToCall(
	asCSema& sema,
	asCScriptEngine* engine,
	asCASTContext& context,
	asASTExprId receiver,
	asASTExprId argument,
	const char* methodName,
	const asCSourceRange& range)
{
	const asCExpr* const receiverExpr = context.GetExpr(receiver);
	if( receiverExpr == 0 || !receiverExpr->type.IsValid()
		|| methodName == 0 || methodName[0] == 0 )
	{
		return asASTExprId();
	}

	const asCType* const receiverType = context.GetType(receiverExpr->type.type);
	if( receiverType == 0
		|| receiverType->kind == asAST_TYPE_PRIMITIVE
		|| receiverType->kind == asAST_TYPE_ENUM
		|| receiverType->kind == asAST_TYPE_VOID )
	{
		return asASTExprId();
	}

	if( engine != 0 )
	{
		InternNativeMethods(sema, engine, context,
			receiverExpr->type, methodName);
	}

	asCArray<asASTExprId> args;
	if( argument.IsValid() )
	{
		args.PushLast(argument);
	}
	const asASTDeclId selected = FindBestCallee(
		context,
		FindNamedTypeDecl(context, receiverExpr->type),
		methodName,
		args,
		&sema,
		&receiverExpr->type);
	const asCDecl* const method = context.GetDecl(selected);
	if( method == 0
		|| method->kind != asAST_DECL_METHOD
		|| !method->name.Equals(methodName) )
	{
		return asASTExprId();
	}

	asCArray<asCString> names;
	asCArray<asSASTCallArgument> callArguments;
	ArrangeCallArguments(
		sema, context, selected, args, names, range, receiver, callArguments);
	ConvertCallArgumentsToFormalTypes(
		sema, context, *method, args, range, receiver, callArguments);
	asCQualType resultType = method->type.IsValid()
		? method->type : context.InternPrimitive(ttInt, 0);
	const asASTExprId call = sema.ActOnCall(
		selected, args, resultType, range, &callArguments);
	if( call.IsValid() && receiver.IsValid() )
	{
		context.SetExprReceiver(call, receiver);
	}
	return ShouldMaterializeCallResult(context, resultType)
		? sema.ActOnMaterializeTemporary(call, range) : call;
}

asASTExprId asCSema::ActOnBinaryExpr(asASTExprId lhs, const char* op, asASTExprId rhs, const asCSourceRange& range)
{
	const asCSourceRange ident = SpanExprRange(context, lhs, rhs, range);
	asCQualType intType = context.InternPrimitive(ttInt, 0);
	const char* methodName = OperatorMethodNameFromText(op);
	if( methodName )
	{
		const asASTExprId rewrittenCall = TryRewriteOverloadedBinaryToCall(
			*this, engine, context, lhs, rhs, methodName, ident);
		if( rewrittenCall.IsValid() )
		{
			asASTExprId result = rewrittenCall;
			// `==` and `!=` both rank `opEquals`. Inequality is a
			// Sema fact: wrap the equality Call in Unary `!` so
			// `Left != Right` cannot execute as a bare equality.
			if( op && std::strcmp(op, "!=") == 0 )
			{
				result = ActOnUnaryExpr("!", result, ident, false);
			}
			// `<`/`>`/`<=`/`>=` all rank `opCmp`, which returns
			// int. AngelScript compares that ranking against 0;
			// a leftover Call would treat any non-zero result as
			// true (`3 < 1` became truthy because opCmp returned 2).
			else if( op
				&& (std::strcmp(op, "<") == 0
					|| std::strcmp(op, ">") == 0
					|| std::strcmp(op, "<=") == 0
					|| std::strcmp(op, ">=") == 0) )
			{
				const asCQualType boolType =
					context.InternPrimitive(ttBool, 0);
				const asASTExprId zero =
					ActOnIntegerLiteral(0, ident);
				result = ActOnBinary(op, result, zero, boolType, ident);
			}
			return result;
		}
		if( rhs.IsValid() )
		{
			// `opEquals`/`opCmp` have no `_r` names. When the lhs cannot host
			// the method, AngelScript retries the same name on the rhs and
			// inverts comparison operators (`3 < Object` is Object.opCmp(3) > 0).
			if( std::strcmp(methodName, "opEquals") == 0
				|| std::strcmp(methodName, "opCmp") == 0 )
			{
				const asASTExprId swappedCall = TryRewriteOverloadedBinaryToCall(
					*this, engine, context, rhs, lhs, methodName, ident);
				if( swappedCall.IsValid() )
				{
					asASTExprId result = swappedCall;
					if( op && std::strcmp(op, "!=") == 0 )
					{
						result = ActOnUnaryExpr("!", result, ident, false);
					}
					else if( op
						&& (std::strcmp(op, "<") == 0
							|| std::strcmp(op, ">") == 0
							|| std::strcmp(op, "<=") == 0
							|| std::strcmp(op, ">=") == 0) )
					{
						const asCQualType boolType =
							context.InternPrimitive(ttBool, 0);
						const asASTExprId zero =
							ActOnIntegerLiteral(0, ident);
						result = ActOnBinary(
							InvertComparisonOperator(op), result, zero,
							boolType, ident);
					}
					return result;
				}
			}
			else
			{
				// AngelScript reverse operators (`opAdd_r`) live on the rhs when
				// the lhs has no matching `opAdd`. `40 + Object` must become a
				// Call of `T::opAdd_r(int)` with receiver=Object.
				asCString reverseName(methodName);
				reverseName += "_r";
				const asASTExprId reverseCall = TryRewriteOverloadedBinaryToCall(
					*this, engine, context, rhs, lhs,
					reverseName.AddressOf(), ident);
				if( reverseCall.IsValid() )
				{
					return reverseCall;
				}
			}
		}
	}
	if( !lhs.IsValid() )
	{
		return asASTExprId();
	}

	// Clang UsualUnaryConversions includes LValueToRValue. A builtin arithmetic
	// operand whose QualType is a scalar reference (array opIndex returning
	// T&) must decay before the Binary node is sealed. Otherwise CodeGen keeps
	// the pointer slot and `Values[0] + 1` adds 1 to an address.
	lhs = DecayScalarLValueReferenceToRValue(*this, context, lhs, ident);
	if( rhs.IsValid() )
	{
		rhs = DecayScalarLValueReferenceToRValue(*this, context, rhs, ident);
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
	if( parameter == 0 || argument == 0 )
	{
		return false;
	}

	const asASTDeclId argumentDecl = FindNamedTypeDecl(context, argumentType);
	const asASTDeclId parameterDecl = FindNamedTypeDecl(context, parameterType);
	const asCDecl* const argumentRecord = context.GetDecl(argumentDecl);
	const asCDecl* const parameterRecord = context.GetDecl(parameterDecl);
	const bool argumentIsReferenceRecord =
		argument->kind == asAST_TYPE_REFERENCE_OBJECT
		|| (argumentRecord != 0
			&& (argumentRecord->kind == asAST_DECL_CLASS
				|| argumentRecord->kind == asAST_DECL_INTERFACE)
			&& (argumentRecord->traits & asAST_TRAIT_VALUE) == 0);
	const bool parameterIsReferenceRecord =
		parameter->kind == asAST_TYPE_REFERENCE_OBJECT
		|| (parameterRecord != 0
			&& (parameterRecord->kind == asAST_DECL_CLASS
				|| parameterRecord->kind == asAST_DECL_INTERFACE)
			&& (parameterRecord->traits & asAST_TRAIT_VALUE) == 0);
	if( !argumentIsReferenceRecord || !parameterIsReferenceRecord )
	{
		return false;
	}
	if( !argumentDecl.IsValid() )
	{
		return false;
	}
	if( argumentRecord != 0
		&& IsAuthoredScriptClass(context, argumentDecl)
		&& parameter != 0
		&& parameter->stableKey.Equals("UObject") )
	{
		// UE script classes are UObject-derived even when extra-module
		// compile has not yet projected PreClass native ancestry onto Decl::bases.
		return true;
	}
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

static bool IsCanonicalNullLiteral(const asCASTContext& context, const asCExpr* expr)
{
	if( expr == 0 || expr->kind != asAST_EXPR_NULL_LITERAL
		|| !expr->type.IsHandle() || expr->type.IsReference() )
	{
		return false;
	}
	const asCType* const type = context.GetType(expr->type.type);
	return type != 0 && type->kind == asAST_TYPE_VOID
		&& type->primitiveToken == ttVoid;
}

static bool IsCanonicalNullableFormal(
	const asCASTContext& context, const asCQualType& formalType)
{
	const asCType* const type = context.GetType(formalType.type);
	// Match legacy ImplicitConvObjectRef exactly for the supported Canonical
	// surface: null converts to a non-reference REFERENCE_OBJECT, including an
	// asOBJ_IMPLICIT_HANDLE spelling that intentionally has no explicit HANDLE
	// qualifier. A HANDLE bit on a value/template shape is not sufficient, and
	// funcdefs need their own independently sealed owning-handle contract.
	return type != 0
		&& !formalType.IsReference()
		&& type->kind == asAST_TYPE_REFERENCE_OBJECT;
}

// Rank only exact and language-standard conversions. User-defined conversions
// are layered on top by RankArgument so one call cannot silently chain
// opImplConv with a target-side implicit constructor.
static int RankStandardArgument(asCASTContext& context,
	const asCQualType& paramType, const asCExpr* arg, asCSema* sema)
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
	if( (directParamType != 0 && directParamType->kind == asAST_TYPE_WILDCARD)
		|| (directParamType != 0 && directParamType->stableKey.Equals("?")) )
	{
		// A registered `?` / `const ?&` formal accepts the call-site value,
		// including enum literals and other rvalues. Keep it viable but rank it
		// below a concrete exact overload.
		return 0;
	}
	if( IsCanonicalNullLiteral(context, arg) )
	{
		// Match legacy ImplicitConvObjectRef: null is a reference conversion only
		// to a non-reference object handle. Keep it below an exact overload and
		// equal across unrelated nullable overloads so
		// ambiguity remains deterministic rather than registration-order driven.
		return IsCanonicalNullableFormal(context, paramType) ? 1 : -1;
	}
	if( IsFuncdefType(context, paramType) )
	{
		const asCDecl* value = arg->resolvedDecl.IsValid() ? context.GetDecl(arg->resolvedDecl) : 0;
		if( value != 0 && value->kind == asAST_DECL_FUNCTION
			&& (value->traits & asAST_TRAIT_LAMBDA) != 0 )
		{
			return sema != 0
				&& sema->IsLambdaViableForFuncdef(arg->id, paramType)
				? 1 : -1;
		}
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
	if( sema != 0 )
	{
		asCRuntimeTypeBridge derivedBridge(sema->GetEngine());
		const asCDataType paramDt = derivedBridge.Resolve(context, paramType);
		const asCDataType argDt = derivedBridge.Resolve(context, arg->type);
		asCObjectType* const paramObject = CastToObjectType(paramDt.GetTypeInfo());
		asCObjectType* const argObject = CastToObjectType(argDt.GetTypeInfo());
		if( paramObject != 0 && argObject != 0
			&& (paramObject->GetFlags() & asOBJ_REF) != 0
			&& (argObject->GetFlags() & asOBJ_REF) != 0
			&& (argObject == paramObject
				|| argObject->DerivesOrShadows(paramObject)
				|| argObject->Implements(paramObject)) )
		{
			return 1;
		}
	}
	if( sema != 0 )
	{
		// Host templates such as TSubclassOf<T> are value objects. A derived
		// script/native T is accepted by TSubclassOf<Base> through the same
		// covariant subtype relationship LEGACY implements with DerivesOrShadows.
		asCRuntimeTypeBridge bridge(sema->GetEngine());
		const asCDataType paramDt = bridge.Resolve(context, paramType);
		const asCDataType argDt = bridge.Resolve(context, arg->type);
		asCObjectType* const paramObject = CastToObjectType(paramDt.GetTypeInfo());
		asCObjectType* const argObject = CastToObjectType(argDt.GetTypeInfo());
		const bool covariantTemplates = paramObject != 0 && argObject != 0
			&& (paramObject->GetFlags() & asOBJ_TEMPLATE_SUBTYPE_COVARIANT) != 0
			&& (argObject->GetFlags() & asOBJ_TEMPLATE_SUBTYPE_COVARIANT) != 0
			&& paramObject->templateBaseType != 0
			&& paramObject->templateBaseType == argObject->templateBaseType
			&& paramObject->templateSubTypes.GetLength()
				== argObject->templateSubTypes.GetLength()
			&& paramObject->templateSubTypes.GetLength() != 0;
		if( covariantTemplates )
		{
			bool covariant = true;
			for( asUINT subIndex = 0;
				covariant && subIndex < paramObject->templateSubTypes.GetLength();
				++subIndex )
			{
				asCObjectType* const paramSub = CastToObjectType(
					paramObject->templateSubTypes[subIndex].GetTypeInfo());
				asCObjectType* const argSub = CastToObjectType(
					argObject->templateSubTypes[subIndex].GetTypeInfo());
				if( paramSub == 0 || argSub == 0 )
				{
					covariant = false;
					break;
				}
				if( argSub != paramSub
					&& !argSub->DerivesOrShadows(paramSub)
					&& !argSub->Implements(paramSub) )
				{
					covariant = false;
				}
			}
			if( covariant )
			{
				return 1;
			}
		}

		const asCType* const paramNamed = context.GetType(paramType.type);
		const asCType* const argNamed = context.GetType(arg->type.type);
		if( paramNamed != 0 && argNamed != 0
			&& paramNamed->stableKey.GetLength() != 0
			&& argNamed->stableKey.GetLength() != 0 )
		{
			const char* const paramKey = paramNamed->stableKey.AddressOf();
			const char* const argKey = argNamed->stableKey.AddressOf();
			const char* const paramOpen = strchr(paramKey, '<');
			const char* const argOpen = strchr(argKey, '<');
			const asUINT paramLen = paramNamed->stableKey.GetLength();
			const asUINT argLen = argNamed->stableKey.GetLength();
			if( paramOpen != 0 && argOpen != 0
				&& paramKey[paramLen - 1] == '>'
				&& argKey[argLen - 1] == '>'
				&& (paramOpen - paramKey) == (argOpen - argKey)
				&& strncmp(paramKey, argKey, paramOpen - paramKey) == 0 )
			{
				asCString paramInner;
				asCString argInner;
				paramInner.Assign(
					paramOpen + 1,
					static_cast<asUINT>((paramKey + paramLen - 1) - (paramOpen + 1)));
				argInner.Assign(
					argOpen + 1,
					static_cast<asUINT>((argKey + argLen - 1) - (argOpen + 1)));
				const asCQualType paramSubType = context.InternNamedType(
					asAST_TYPE_REFERENCE_OBJECT, paramInner.AddressOf(), 0);
				const asCQualType argSubType = context.InternNamedType(
					asAST_TYPE_REFERENCE_OBJECT, argInner.AddressOf(), 0);
				if( IsDerivedReferenceArgument(context, paramSubType, argSubType) )
				{
					return 1;
				}
			}
		}
	}
	const asCType* param = ResolveTypedefValueType(context, context.GetType(paramType.type));
	const asCType* argType = ResolveTypedefValueType(context, context.GetType(arg->type.type));
	if( IsIntegerPrimitive(param) && IsIntegerPrimitive(argType) )
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
	if( IsFloatPrimitive(param) && IsIntegerPrimitive(argType) )
	{
		return 1;
	}
	if( IsIntegerPrimitive(param) && IsFloatPrimitive(argType) )
	{
		return 0;
	}
	return -1;
}

static asASTDeclId FindTargetImplicitConstructor(
	asCASTContext& context,
	const asCQualType& targetType,
	const asCExpr* argument,
	asCSema* sema)
{
	const asCDecl* const target =
		context.GetDecl(FindNamedTypeDecl(context, targetType));
	if( target == 0 || target->kind != asAST_DECL_CLASS || argument == 0 )
	{
		return asASTDeclId();
	}

	asASTDeclId best;
	int bestRank = -1;
	bool ambiguous = false;
	for( asUINT childIndex = 0;
		childIndex < target->children.GetLength(); ++childIndex )
	{
		const asCDecl* const constructor =
			context.GetDecl(target->children[childIndex]);
		if( constructor == 0
			|| constructor->kind != asAST_DECL_CONSTRUCTOR
			|| CountParams(context, constructor) != 1 )
		{
			continue;
		}
		// Native constructors convert only when the host marked them implicit.
		// Script one-parameter constructors are converting constructors, matching
		// Clang's non-explicit CXXConstructorDecl used as CXXConstructExpr.
		if( (constructor->traits & asAST_TRAIT_IMPLICIT_CONSTRUCTOR) == 0
			&& (constructor->traits & asAST_TRAIT_EXTERNAL) != 0 )
		{
			continue;
		}
		const asCDecl* const parameter =
			context.GetFormalDecl(constructor->id, 0);
		const int rank = parameter != 0
			? RankStandardArgument(context, parameter->type, argument, sema)
			: -1;
		if( rank < 0 )
		{
			continue;
		}
		if( rank > bestRank )
		{
			bestRank = rank;
			best = constructor->id;
			ambiguous = false;
		}
		else if( rank == bestRank && best.IsValid() )
		{
			ambiguous = true;
		}
	}
	return ambiguous ? asASTDeclId() : best;
}

static int RankArgument(asCASTContext& context, const asCQualType& paramType,
	const asCExpr* arg, asCSema* sema)
{
	const int standardRank =
		RankStandardArgument(context, paramType, arg, sema);
	if( standardRank >= 0 )
	{
		return standardRank;
	}
	if( HasZeroArgImplConvTo(context, arg->type, paramType) )
	{
		return 1;
	}
	// A target implicit constructor is one user-defined conversion and ranks
	// below a source-owned opImplConv. Its constructor argument is deliberately
	// tested with RankStandardArgument, so the two user conversions cannot chain.
	return FindTargetImplicitConstructor(
		context, paramType, arg, sema).IsValid() ? 0 : -1;
}

struct asSCallArgumentPlanSlot
{
	asASTDeclId formal;
	asCQualType formalType;
	asCString formalName;
	asUINT formalIndex;
	asUINT argumentIndex;
	asUINT sourceOrdinal;
	asEASTCallArgumentOrigin origin;
	asCString authoredName;

	asSCallArgumentPlanSlot()
		: formalIndex(asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL)
		, argumentIndex(asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL)
		, sourceOrdinal(asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL)
		, origin(asAST_CALL_ARGUMENT_INVALID)
	{
	}
};

static bool AppendCallArgumentFormal(
	asCASTContext& context,
	const asCDecl& formal,
	asUINT formalIndex,
	asCArray<asSCallArgumentPlanSlot>& plan)
{
	if( formal.kind != asAST_DECL_PARAM
		|| formal.formalIndex != formalIndex )
	{
		return false;
	}
	asSCallArgumentPlanSlot slot;
	slot.formal = formal.id;
	slot.formalType = formal.type;
	slot.formalName = formal.name;
	slot.formalIndex = formalIndex;
	if( formal.origin.Equals("hidden") )
	{
		slot.origin = asAST_CALL_ARGUMENT_HIDDEN;
	}
	plan.PushLast(slot);
	return true;
}

static bool BuildCallableFormalView(
	asCSema* sema,
	asCASTContext& context,
	const asCDecl& callee,
	asCArray<asSCallArgumentPlanSlot>& plan)
{
	plan.SetLength(0);
	if( callee.kind != asAST_DECL_PARAM && callee.kind != asAST_DECL_VAR )
	{
		const asUINT formalCount = context.GetFormalDeclCount(callee.id);
		for( asUINT formalIndex = 0; formalIndex < formalCount; ++formalIndex )
		{
			const asCDecl* const param =
				context.GetFormalDecl(callee.id, formalIndex);
			if( param == 0
				|| !AppendCallArgumentFormal(
					context, *param, formalIndex, plan) )
			{
				plan.SetLength(0);
				return false;
			}
		}
		return true;
	}

	const asCType* const callableType = context.GetType(callee.type.type);
	if( callableType == 0 || callableType->kind != asAST_TYPE_FUNCDEF
		|| callableType->stableKey.GetLength() == 0 )
	{
		return false;
	}

	const asCDecl* canonicalFuncdef = 0;
	for( asUINT declarationIndex = 1;
		declarationIndex <= context.GetDeclCount(); ++declarationIndex )
	{
		const asCDecl* const declaration =
			context.GetDecl(asASTDeclId(declarationIndex));
		if( declaration == 0 || declaration->kind != asAST_DECL_FUNCDEF
			|| !declaration->stableKey.Equals(callableType->stableKey) )
		{
			continue;
		}
		if( canonicalFuncdef != 0 )
		{
			return false;
		}
		canonicalFuncdef = declaration;
	}
	if( canonicalFuncdef == 0 && sema != 0 )
	{
		const asASTDeclId projected =
			sema->ProjectCanonicalRegisteredFuncdef(callableType->stableKey);
		canonicalFuncdef = context.GetDecl(projected);
	}
	if( canonicalFuncdef == 0 )
	{
		return false;
	}

	// Indirect VAR/PARAM calls have no callee-owned ParamDecl. Verifier
	// requires pointer-free formalIndex + QualType only.
	const asUINT formalCount =
		context.GetFormalDeclCount(canonicalFuncdef->id);
	for( asUINT formalIndex = 0; formalIndex < formalCount; ++formalIndex )
	{
		const asCDecl* const param =
			context.GetFormalDecl(canonicalFuncdef->id, formalIndex);
		if( param == 0 || !param->type.IsValid() )
		{
			plan.SetLength(0);
			return false;
		}
		asSCallArgumentPlanSlot slot;
		slot.formalType = param->type;
		slot.formalName = param->name;
		slot.formalIndex = formalIndex;
		plan.PushLast(slot);
	}
	return true;
}

// Build the source-to-formal binding without creating or rewriting AST nodes.
// Overload viability and the selected-callee arrangement pass must agree on
// named/default/hidden slots before any synthesized expression exists.
static bool TryBuildCallArgumentPlan(
	asCSema* sema,
	asCASTContext& context,
	const asCDecl& callee,
	const asCArray<asASTExprId>& args,
	const asCArray<asCString>* argumentNames,
	asASTExprId implicitReceiver,
	asCArray<asSCallArgumentPlanSlot>& plan)
{
	if( !BuildCallableFormalView(sema, context, callee, plan) )
	{
		return false;
	}

	const bool hasReceiver = implicitReceiver.IsValid()
		&& args.GetLength() != 0 && args[0] == implicitReceiver;
	const bool receiverIsFormal = hasReceiver
		&& callee.kind == asAST_DECL_MIXIN;
	const bool receiverBelongsToCallee = hasReceiver
		&& (callee.kind == asAST_DECL_METHOD
			|| callee.kind == asAST_DECL_MIXIN);
	const asUINT sourceBegin = receiverBelongsToCallee ? 1 : 0;
	const asUINT sourceCount = args.GetLength() - sourceBegin;
	const asUINT nameCount = argumentNames != 0
		? argumentNames->GetLength() : 0;
	const bool namesIncludeReceiver = receiverBelongsToCallee
		&& nameCount == args.GetLength();
	if( nameCount != 0 && nameCount != sourceCount
		&& !namesIncludeReceiver )
	{
		return false;
	}

	if( receiverIsFormal )
	{
		if( plan.GetLength() == 0
			|| plan[0].origin != asAST_CALL_ARGUMENT_INVALID )
		{
			return false;
		}
		plan[0].argumentIndex = 0;
		plan[0].origin = asAST_CALL_ARGUMENT_IMPLICIT_RECEIVER;
	}

	asUINT nextPositional = receiverIsFormal ? 1 : 0;
	for( asUINT argumentIndex = sourceBegin;
		argumentIndex < args.GetLength(); ++argumentIndex )
	{
		if( context.GetExpr(args[argumentIndex]) == 0 )
		{
			return false;
		}
		const asUINT sourceOrdinal = argumentIndex - sourceBegin;
		const asUINT nameIndex = namesIncludeReceiver
			? argumentIndex : sourceOrdinal;
		const asCString* const authoredName =
			argumentNames != 0 && nameIndex < nameCount
				? &(*argumentNames)[nameIndex] : 0;
		if( authoredName != 0 && authoredName->GetLength() != 0 )
		{
			bool matched = false;
			for( asUINT formalIndex = 0;
				formalIndex < plan.GetLength(); ++formalIndex )
			{
				asSCallArgumentPlanSlot& slot = plan[formalIndex];
				if( slot.origin != asAST_CALL_ARGUMENT_INVALID
					|| slot.formalName.GetLength() == 0
					|| !slot.formalName.Equals(authoredName->AddressOf()) )
				{
					continue;
				}
				slot.argumentIndex = argumentIndex;
				slot.sourceOrdinal = sourceOrdinal;
				slot.origin = asAST_CALL_ARGUMENT_NAMED;
				slot.authoredName = *authoredName;
				matched = true;
				break;
			}
			if( !matched )
			{
				return false;
			}
			continue;
		}

		while( nextPositional < plan.GetLength()
			&& plan[nextPositional].origin
				!= asAST_CALL_ARGUMENT_INVALID )
		{
			++nextPositional;
		}
		if( nextPositional >= plan.GetLength() )
		{
			return false;
		}
		asSCallArgumentPlanSlot& slot = plan[nextPositional];
		slot.argumentIndex = argumentIndex;
		slot.sourceOrdinal = sourceOrdinal;
		slot.origin = asAST_CALL_ARGUMENT_POSITIONAL;
		++nextPositional;
	}

	for( asUINT formalIndex = 0;
		formalIndex < plan.GetLength(); ++formalIndex )
	{
		asSCallArgumentPlanSlot& slot = plan[formalIndex];
		if( slot.origin != asAST_CALL_ARGUMENT_INVALID )
		{
			continue;
		}
		const asCDecl* const formal = context.GetDecl(slot.formal);
		if( formal == 0 || formal->inits.GetLength() == 0 )
		{
			return false;
		}
		slot.origin = asAST_CALL_ARGUMENT_DEFAULT;
	}
	return true;
}

static asCSourceRange DiagnosticRangeFromCallArguments(
	asCASTContext& context,
	const asCArray<asASTExprId>& args,
	asASTExprId implicitReceiver)
{
	asCSourceRange range;
	if( implicitReceiver.IsValid() )
	{
		if( const asCExpr* const receiver = context.GetExpr(implicitReceiver) )
		{
			if( receiver->range.IsValid() )
			{
				range = receiver->range;
			}
		}
	}
	for( asUINT i = 0; i < args.GetLength(); ++i )
	{
		const asCExpr* const arg = context.GetExpr(args[i]);
		if( arg == 0 || !arg->range.IsValid() )
		{
			continue;
		}
		if( !range.IsValid() )
		{
			range = arg->range;
			continue;
		}
		if( arg->range.begin.fileID != range.begin.fileID )
		{
			continue;
		}
		if( arg->range.begin.offset < range.begin.offset )
		{
			range.begin = arg->range.begin;
		}
		if( arg->range.end.offset > range.end.offset )
		{
			range.end = arg->range.end;
		}
	}
	return range;
}

static asUINT CountWildcardFormals(asCASTContext& context, const asCDecl& callee)
{
	asUINT count = 0;
	const asUINT formalCount = context.GetFormalDeclCount(callee.id);
	for( asUINT i = 0; i < formalCount; ++i )
	{
		const asCDecl* const formal = context.GetFormalDecl(callee.id, i);
		if( formal == 0 )
		{
			continue;
		}
		const asCType* const type = context.GetType(formal->type.type);
		if( type != 0
			&& (type->kind == asAST_TYPE_WILDCARD
				|| type->stableKey.Equals("?")) )
		{
			++count;
		}
	}
	return count;
}

static asASTDeclId FindBestCallee(asCASTContext& context, asASTDeclId owner,
	const char* name, const asCArray<asASTExprId>& args, asCSema* sema,
	const asCQualType* receiverType, bool exactScope,
	const asCArray<asCString>* argumentNames,
	asASTExprId implicitReceiver)
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
		// Target-side implicit constructors are registration facts of each
		// formal type. Import them before walking candidates: interning may grow
		// the declaration table, so doing it from RankArgument would invalidate
		// a candidate pointer held by this loop.
		for( asUINT hitIndex = 0; hitIndex < hits.GetLength(); ++hitIndex )
		{
			const asCDecl* const candidate = context.GetDecl(hits[hitIndex]);
			if( candidate == 0 )
			{
				continue;
			}
			const asUINT formalCount =
				context.GetFormalDeclCount(candidate->id);
			for( asUINT formalIndex = 0;
				formalIndex < formalCount; ++formalIndex )
			{
				const asCDecl* const formal =
					context.GetFormalDecl(candidate->id, formalIndex);
				if( formal != 0 )
				{
					const asCQualType formalType = formal->type;
					sema->InternNativeCallablesForType(formalType);
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
		asCArray<asSCallArgumentPlanSlot> plan;
		if( !TryBuildCallArgumentPlan(
				sema, context, *child, args, argumentNames,
				implicitReceiver, plan) )
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
		bool ok = true;
		for( asUINT formalIndex = 0;
			formalIndex < plan.GetLength(); ++formalIndex )
		{
			const asSCallArgumentPlanSlot& slot = plan[formalIndex];
			if( slot.argumentIndex
				== asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL )
			{
				continue;
			}
			const asCExpr* const arg =
				context.GetExpr(args[slot.argumentIndex]);
			if( !slot.formalType.IsValid()
				|| context.GetType(slot.formalType.type) == 0 || arg == 0 )
			{
				ok = false;
				break;
			}
			const int rank = RankArgument(context, slot.formalType, arg, sema);
			if( rank < 0 )
			{
				ok = false;
				break;
			}
			score += rank;
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
			// LEGACY costs asCC_TO_OBJECT_CONV below asCC_VARIABLE_CONV, so a
			// concrete formal that matches through an implicit constructor
			// (`FString(FName)`) beats `const ?&`. Equal numeric ranks must
			// not become ambiguous-overload for ApplyFormat(GetName(), spec).
			const asCDecl* const bestDecl = context.GetDecl(best);
			const asUINT bestWild = bestDecl != 0
				? CountWildcardFormals(context, *bestDecl) : 0;
			const asUINT childWild = CountWildcardFormals(context, *child);
			if( childWild < bestWild )
			{
				best = child->id;
				ambiguous = false;
			}
			else if( childWild > bestWild )
			{
				ambiguous = false;
			}
			else
			{
				ambiguous = true;
			}
		}
	}
	if( ambiguous )
	{
		if( sema )
		{
			const asCSourceRange range = DiagnosticRangeFromCallArguments(
				context, args, implicitReceiver);
			if( range.IsValid() )
			{
				sema->AddDiagnostic("ambiguous-overload", range);
			}
			else
			{
				sema->AddDiagnostic("ambiguous-overload");
			}
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

static asASTDeclId FindBestConstructor(asCASTContext& context,
	asASTDeclId classDecl, const asCArray<asASTExprId>& args,
	asCSema* sema, const asCArray<asCString>* argumentNames)
{
	const asCDecl* const owner = context.GetDecl(classDecl);
	if( owner == 0 || owner->kind != asAST_DECL_CLASS )
	{
		return asASTDeclId();
	}
	if( sema != 0 )
	{
		if( asCScriptEngine* const engine = sema->GetEngine() )
		{
			for( asUINT argumentIndex = 0;
				argumentIndex < args.GetLength(); ++argumentIndex )
			{
				if( const asCExpr* const argument =
					context.GetExpr(args[argumentIndex]) )
				{
					InternNativeMethods(
						*sema, engine, context, argument->type, "opImplConv");
				}
			}
		}
	}

	asASTDeclId best;
	int bestScore = -1;
	bool ambiguous = false;
	for( asUINT childIndex = 0;
		childIndex < owner->children.GetLength(); ++childIndex )
	{
		const asCDecl* const candidate =
			context.GetDecl(owner->children[childIndex]);
		if( candidate == 0 || candidate->kind != asAST_DECL_CONSTRUCTOR )
		{
			continue;
		}
		asCArray<asSCallArgumentPlanSlot> plan;
		if( !TryBuildCallArgumentPlan(
				sema, context, *candidate, args, argumentNames,
				asASTExprId(), plan) )
		{
			continue;
		}
		int score = 0;
		bool viable = true;
		for( asUINT formalIndex = 0;
			formalIndex < plan.GetLength(); ++formalIndex )
		{
			const asSCallArgumentPlanSlot& slot = plan[formalIndex];
			if( slot.argumentIndex == asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL )
			{
				continue;
			}
			const asCExpr* const argument =
				context.GetExpr(args[slot.argumentIndex]);
			if( !slot.formalType.IsValid()
				|| context.GetType(slot.formalType.type) == 0
				|| argument == 0 )
			{
				viable = false;
				break;
			}
			const int rank = RankArgument(
				context, slot.formalType, argument, sema);
			if( rank < 0 )
			{
				viable = false;
				break;
			}
			score += rank;
		}
		if( !viable )
		{
			continue;
		}
		if( score > bestScore )
		{
			bestScore = score;
			best = candidate->id;
			ambiguous = false;
		}
		else if( score == bestScore && best.IsValid() )
		{
			ambiguous = true;
		}
	}
	if( ambiguous )
	{
		if( sema != 0 )
		{
			sema->AddDiagnostic("ambiguous-constructor-overload");
		}
		return asASTDeclId();
	}
	return best;
}

static void ArrangeCallArguments(asCSema& sema, asCASTContext& context,
	asASTDeclId callee, asCArray<asASTExprId>& args,
	asCArray<asCString>& names, const asCSourceRange& range,
	asASTExprId implicitReceiver,
	asCArray<asSASTCallArgument>& callArguments)
{
	callArguments.SetLength(0);
	const asCDecl* const decl = context.GetDecl(callee);
	if( decl == 0 )
	{
		return;
	}
	const bool hasReceiver = implicitReceiver.IsValid()
		&& args.GetLength() != 0 && args[0] == implicitReceiver;
	const bool receiverIsFormal = hasReceiver && decl->kind == asAST_DECL_MIXIN;
	asCArray<asSCallArgumentPlanSlot> plan;
	if( !TryBuildCallArgumentPlan(
			&sema, context, *decl, args, &names, implicitReceiver, plan) )
	{
		asCString detail;
		detail.Format(
			"call-argument-plan-invalid:callee=%s kind=%d args=%u names=%u receiver=%u formals=%u",
			decl->stableKey.GetLength() != 0
				? decl->stableKey.AddressOf() : decl->name.AddressOf(),
			static_cast<int>(decl->kind),
			args.GetLength(), names.GetLength(), implicitReceiver.value,
			plan.GetLength());
		for( asUINT formalIndex = 0;
			formalIndex < plan.GetLength(); ++formalIndex )
		{
			asCString slot;
			slot.Format(
				" formal%u{name=%s,arg=%u,source=%u,origin=%d}",
				formalIndex,
				plan[formalIndex].formalName.GetLength() != 0
					? plan[formalIndex].formalName.AddressOf() : "<unnamed>",
				plan[formalIndex].argumentIndex,
				plan[formalIndex].sourceOrdinal,
				static_cast<int>(plan[formalIndex].origin));
			detail += slot;
		}
		sema.AddDiagnostic(detail.AddressOf(), range);
		return;
	}

	asCArray<asASTExprId> arranged;
	if( hasReceiver && !receiverIsFormal )
	{
		arranged.PushLast(implicitReceiver);
	}
	for( asUINT formalIndex = 0;
		formalIndex < plan.GetLength(); ++formalIndex )
	{
		const asSCallArgumentPlanSlot& slot = plan[formalIndex];
		const asCDecl* const formal = slot.formal.IsValid()
			? context.GetDecl(slot.formal) : 0;
		if( !slot.formalType.IsValid()
			|| context.GetType(slot.formalType.type) == 0
			|| (slot.formal.IsValid() && formal == 0) )
		{
			sema.AddDiagnostic("call-argument-formal-invalid", range);
			return;
		}

		asASTExprId expression;
		if( slot.argumentIndex != asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL )
		{
			expression = args[slot.argumentIndex];
		}
		else if( formal->inits.GetLength() != 0 )
		{
			expression = sema.ActOnConversion(
				formal->inits[0], formal->type, range);
		}
		if( !expression.IsValid() || context.GetExpr(expression) == 0 )
		{
			sema.AddDiagnostic("call-argument-expression-invalid", range);
			return;
		}

		asSASTCallArgument argument;
		argument.expression = expression;
		argument.formal = slot.formal;
		argument.formalType = slot.formalType;
		argument.formalIndex = slot.formalIndex;
		argument.sourceOrdinal = slot.sourceOrdinal;
		argument.origin = slot.origin;
		argument.authoredName = slot.authoredName;
		const asCExpr* const expressionNode = context.GetExpr(expression);
		argument.range = slot.argumentIndex
			!= asAST_CALL_ARGUMENT_NO_SOURCE_ORDINAL
				? expressionNode->range : range;
		arranged.PushLast(expression);
		callArguments.PushLast(argument);
	}
	args = arranged;
}

static void ConvertCallArgumentsToFormalTypes(asCSema& sema, asCASTContext& context,
	const asCDecl& callee, asCArray<asASTExprId>& args,
	const asCSourceRange& range, asASTExprId implicitReceiver,
	asCArray<asSASTCallArgument>& callArguments)
{
	// CALL argument arrays are still in forward-formal order here.  A normal
	// method keeps its receiver in the leading non-formal slot, while a mixin
	// uses that same expression as declared formal zero.  Model that distinction
	// once so immediate and deferred lookup cannot drift apart.
	const bool receiverIsFormal = implicitReceiver.IsValid()
		&& callee.kind == asAST_DECL_MIXIN;
	asUINT argIndex = implicitReceiver.IsValid() && !receiverIsFormal ? 1 : 0;
	for( asUINT formalIndex = 0;
		formalIndex < callArguments.GetLength(); ++formalIndex )
	{
		if( argIndex >= args.GetLength() )
		{
			break;
		}
		asSASTCallArgument& callArgument = callArguments[formalIndex];
		const asCDecl* const param = callArgument.formal.IsValid()
			? context.GetDecl(callArgument.formal) : 0;
		asCQualType formalType = callArgument.formalType;
		if( !formalType.IsValid() && param != 0 )
		{
			formalType = param->type;
			callArgument.formalType = formalType;
		}
		if( !formalType.IsValid() || context.GetType(formalType.type) == 0 )
		{
			sema.AddDiagnostic("call-argument-formal-type-invalid", range);
			break;
		}

		// A generated property getter is not an addressable out target. Seal the
		// exact generated setter and one opaque receiver before ordinary argument
		// conversion can erase that property identity. Direct locals/references
		// stay on the normal lvalue-address route.
		const bool isExactOutOnly = formalType.IsReference()
			&& (formalType.quals & asAST_QUAL_OUT) != 0
			&& (formalType.quals & asAST_QUAL_IN) == 0;
		const asCType* const outValueType = context.GetType(formalType.type);
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
				&& getter->type.type == formalType.type )
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

					const asUINT valueParamCount =
						context.GetFormalDeclCount(candidate->id);
					const asCDecl* const valueParam = valueParamCount == 1
						? context.GetFormalDecl(candidate->id, 0) : 0;
					if( valueParamCount == 1 && valueParam != 0
						&& valueParam->type.type == formalType.type
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
							formalType,
							setter,
							range);
					if( deferred.IsValid() )
					{
						args[argIndex] = deferred;
						if( formalIndex < callArguments.GetLength() )
						{
							callArgument.expression = deferred;
						}
						++argIndex;
						continue;
					}
				}
			}
		}
		const asCExpr* const arg = context.GetExpr(args[argIndex]);
		const asCType* const paramType = context.GetType(formalType.type);
		if( arg != 0
			&& (paramType == 0 || paramType->kind != asAST_TYPE_WILDCARD)
			&& !QualTypesMatch(formalType, arg->type) )
		{
			const asASTDeclId implConv = FindZeroArgImplConv(
				context, arg->type, formalType);
			const asASTDeclId implicitConstructor = !implConv.IsValid()
				? FindTargetImplicitConstructor(
					context, formalType, arg, &sema)
				: asASTDeclId();
			if( implicitConstructor.IsValid() )
			{
				asCArray<asASTExprId> constructorArgs;
				constructorArgs.PushLast(args[argIndex]);
				asCArray<asCString> constructorNames;
				asCArray<asSASTCallArgument> constructorArguments;
				ArrangeCallArguments(
					sema, context, implicitConstructor,
					constructorArgs, constructorNames, range,
					asASTExprId(), constructorArguments);
				asCQualType constructedValueType = formalType;
				// The formal describes how the finished temporary is passed
				// (`const T&in` for SpawnActor), not the value category produced by
				// the constructor. Keep the exact nominal/template TypeRef while
				// removing parameter-only presentation from the Construct result.
				constructedValueType.quals &= ~(
					asAST_QUAL_CONST
					| asAST_QUAL_REFERENCE
					| asAST_QUAL_PARAM_DIR_MASK);
				args[argIndex] = sema.ActOnConstruct(
					QualTypeForConstructExpression(
						context, constructedValueType),
					constructorArgs, range, implicitConstructor,
					&constructorArguments);
			}
			else
			{
				args[argIndex] = sema.ActOnConversion(
					args[argIndex], formalType, range);
				if( implConv.IsValid() )
				{
					context.SetResolvedDecl(args[argIndex], implConv);
				}
			}
		}
		args[argIndex] = SealNonPodByValueCopyPlan(
			sema, context, formalType, args[argIndex], range);
		callArgument.expression = args[argIndex];
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

// Registered enum values are not declarations in the current script AST. Keep
// them as typed literals instead of manufacturing short-lived fake decls, but
// consume only the translation-unit-local pointer-free declaration snapshot.
// A current lexical scope always wins before this registered fallback.
asASTExprId asCSema::InternCanonicalRegisteredEnumLiteral(
	asASTDeclId lookupScope,
	const asCArray<asCString>& scopeSegments,
	bool absoluteScope,
	const char* valueName,
	const asCSourceRange& range,
	bool& authoritativeFailure)
{
	authoritativeFailure = false;
	const asCString qualifiedScope = JoinScopeSegments(scopeSegments);
	if( valueName == 0 || valueName[0] == 0
		|| qualifiedScope.GetLength() == 0 )
	{
		return asASTExprId();
	}
	if( ResolveScopeSegments(
			this, lookupScope, scopeSegments, absoluteScope, 0).IsValid() )
	{
		// The scope belongs to this Canonical translation unit. Its declaration
		// and enumerator are resolved by the ordinary lexical path below.
		return asASTExprId();
	}

	asCString stableKey;
	bool ambiguous = false;
	const asSCanonicalRegisteredTypeDeclarationFact* const fact =
		ResolveCanonicalRegisteredTypeDeclaration(
			qualifiedScope,
			absoluteScope,
			lookupScope,
			stableKey,
			ambiguous);
	if( ambiguous )
	{
		AddDiagnostic("decl-ref-registered-enum-ambiguous", range);
		authoritativeFailure = true;
		return asASTExprId();
	}
	if( fact == 0 || fact->kind != asAST_TYPE_ENUM )
	{
		return asASTExprId();
	}
	for( asUINT i = 0; i < fact->enumerators.GetLength(); ++i )
	{
		const asSCanonicalRegisteredEnumeratorFact& value =
			fact->enumerators[i];
		if( !value.name.Equals(valueName) )
		{
			continue;
		}
		const asCQualType type = context.InternNamedType(
			asAST_TYPE_ENUM, stableKey.AddressOf(), 0);
		if( !type.IsValid() )
		{
			return asASTExprId();
		}
		const asASTExprId literal = context.CreateExpr(asAST_EXPR_INTEGER_LITERAL, type, asAST_VALUE_PRVALUE, range);
		context.SetLiteralBits(literal,
			static_cast<asQWORD>(static_cast<asINT64>(value.value)));
		asCString spelling = stableKey;
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
	bool registeredEnumFailure = false;
	const asASTExprId registeredEnum = InternCanonicalRegisteredEnumLiteral(
		owner,
		action.scopeSegments,
		action.absoluteScope,
		action.name.AddressOf(),
		range,
		registeredEnumFailure);
	if( registeredEnum.IsValid() )
	{
		return registeredEnum;
	}
	if( registeredEnumFailure )
	{
		return asASTExprId();
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
	asASTDeclId target = ResolveExactDeclRef(
		*this, context, searchOwner, action.name.AddressOf());
	if( !target.IsValid() && engine != 0 )
	{
		// Application globals may live in a namespace whose spelling also names a
		// registered value type (for example FLinearColor::LucBlue). Resolve the
		// exact Runtime namespace/property coordinate from the structured Parser
		// action, then publish only its pointer-free VarDecl. This also avoids
		// treating a namespace/type spelling collision as an arbitrary first hit.
		target = InternNativeGlobalProperty(
			owner,
			action.scopeSegments,
			action.absoluteScope,
			action.name.AddressOf());
	}
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
	asCString typeName = qualifiedScope;
	if( typeName.GetLength() != 0 )
	{
		typeName += "::";
	}
	typeName += action.name;
	bool typeNameAmbiguous = false;
	const asCQualType typeNameTarget = !action.implicitReceiver.IsValid()
		? ResolveQualifiedNominalType(
			typeName, action.absoluteScope, 0, typeNameAmbiguous)
		: asCQualType();
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
		action.implicitReceiver, &argumentNames, hasExplicitScope,
		typeNameTarget.IsValid() ? &typeNameTarget : 0);
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
