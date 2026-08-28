#include "as_bytecode_codegen.h"
#include "as_bytecode_codegen_artifact.h"

#ifndef AS_NO_COMPILER

#include "as_bytecode.h"
#include "as_builder.h"
#include "as_module.h"
#include "as_objecttype.h"
#include "as_parser.h"
#include "as_property.h"
#include "as_runtime_type_bridge.h"
#include "as_scriptengine.h"
#include "as_ast_dump.h"
#include "as_ast_lifetime.h"
#include "as_scriptfunction.h"
#include "as_ast_type.h"
#include "as_ast_verifier.h"
#include "as_tokendef.h"
#include "as_typeinfo.h"
#include <stdlib.h>

BEGIN_AS_NAMESPACE

asCBytecodeCodeGen::asCBytecodeCodeGen()
	: failed(false)
	, error(asAST_VERIFY_OK)
	, lastFailLine(0)
	, failureRow(0)
	, failureColumn(0)
#if WITH_ANGELSCRIPT_UNITTESTS
	, testFailureBeforeFunctionEmission(asUINT(-1))
	, testFailureCode(asERROR)
	, testFailureBeforePreparedFactoryEmission(asUINT(-1))
	, testPreparedFactoryFailureCode(asERROR)
	, testFailureAfterCommitStage(asUINT(-1))
	, testCommitFailureCode(asERROR)
	, testFailureAtTypeRelocationUse(asTYPE_RELOC_USE_COUNT)
	, testTypeRelocationFailureCode(asERROR)
	, testTypeRelocationFailureInjected(false)
#endif
{
}

#if WITH_ANGELSCRIPT_UNITTESTS
void asCBytecodeCodeGen::SetTestFailureBeforeFunctionEmission(asUINT functionOrdinal, int inFailureCode)
{
	testFailureBeforeFunctionEmission = functionOrdinal;
	testFailureCode = inFailureCode != 0 ? inFailureCode : asERROR;
}

void asCBytecodeCodeGen::SetTestFailureBeforePreparedFactoryEmission(
	asUINT factoryOrdinal, int inFailureCode)
{
	testFailureBeforePreparedFactoryEmission = factoryOrdinal;
	testPreparedFactoryFailureCode = inFailureCode < 0 ? inFailureCode : asERROR;
}

void asCBytecodeCodeGen::SetTestFailureAfterCommitStage(asUINT stage, int inFailureCode)
{
	testFailureAfterCommitStage = stage;
	testCommitFailureCode = inFailureCode < 0 ? inFailureCode : asERROR;
}

void asCBytecodeCodeGen::SetTestFailureAtTypeRelocationUse(
	asERuntimeTypeRelocationUse use, int inFailureCode)
{
	testFailureAtTypeRelocationUse = static_cast<asUINT>(use);
	testTypeRelocationFailureCode = inFailureCode < 0 ? inFailureCode : asERROR;
	testTypeRelocationFailureInjected = false;
}
#endif

namespace
{
	static const char* const CANONICAL_AUTOMATIC_IMPORT_SCRIPT_FUNCTION_ORIGIN = "canonical-automatic-import-script-function";
	static const char* const CANONICAL_AUTOMATIC_IMPORT_SCRIPT_GLOBAL_ORIGIN = "canonical-automatic-import-script-global";
	static const char* const CANONICAL_CURRENT_MODULE_SCRIPT_FUNCTION_ORIGIN = "canonical-current-module-script-function";
	static const char* const CANONICAL_CURRENT_MODULE_SCRIPT_GLOBAL_ORIGIN = "canonical-current-module-script-global";
	static const char* const CANONICAL_NATIVE_GLOBAL_PROPERTY_ORIGIN = "canonical-native-global-property";

	bool IsCanonicalAutomaticImportScriptFunction(const asCDecl* decl)
	{
		return decl && decl->origin.Equals(CANONICAL_AUTOMATIC_IMPORT_SCRIPT_FUNCTION_ORIGIN);
	}

	bool IsCanonicalAutomaticImportScriptGlobal(const asCDecl* decl)
	{
		return decl && decl->origin.Equals(CANONICAL_AUTOMATIC_IMPORT_SCRIPT_GLOBAL_ORIGIN);
	}

	bool IsCanonicalCurrentModuleScriptFunction(const asCDecl* decl)
	{
		return decl && decl->origin.Equals(CANONICAL_CURRENT_MODULE_SCRIPT_FUNCTION_ORIGIN);
	}

	bool IsCanonicalCurrentModuleScriptGlobal(const asCDecl* decl)
	{
		return decl && decl->origin.Equals(CANONICAL_CURRENT_MODULE_SCRIPT_GLOBAL_ORIGIN);
	}

	bool IsCanonicalNativeGlobalProperty(const asCDecl* decl)
	{
		return decl && decl->origin.Equals(CANONICAL_NATIVE_GLOBAL_PROPERTY_ORIGIN);
	}

	bool CanonicalDeclIsFunctionLike(asEASTDeclKind kind)
	{
		return kind == asAST_DECL_FUNCTION
			|| kind == asAST_DECL_METHOD
			|| kind == asAST_DECL_CONSTRUCTOR
			|| kind == asAST_DECL_DESTRUCTOR
			|| kind == asAST_DECL_MIXIN;
	}

	const asCDecl* FindSingleCompileFunctionDecl(const asCASTContext& context)
	{
		const asCDecl* const translationUnit =
			context.GetDecl(context.GetTranslationUnit());
		if( translationUnit == 0 || translationUnit->kind != asAST_DECL_TRANSLATION_UNIT )
		{
			return 0;
		}
		const asCDecl* result = 0;
		for( asUINT i = 0; i < translationUnit->children.GetLength(); ++i )
		{
			const asCDecl* const child = context.GetDecl(translationUnit->children[i]);
			if( IsCanonicalCurrentModuleScriptFunction(child)
				|| IsCanonicalCurrentModuleScriptGlobal(child) )
			{
				continue;
			}
			if( child == 0 || child->kind != asAST_DECL_FUNCTION
				|| (child->traits & asAST_TRAIT_LAMBDA) != 0 || result != 0 )
			{
				return 0;
			}
			result = child;
		}
		return result;
	}

	// The canonical dump is explicitly address-free. Hash it rather than a
	// context pointer so provenance can be retained in diagnostics, compared
	// across capture/restore paths, and inspected without exposing node layout.
	asQWORD ComputeCanonicalAstDigest(const asCASTContext& context)
	{
		asCString dump;
		asCASTDump(context, dump);
		asQWORD digest = 1469598103934665603ULL;
		const unsigned char* cursor = reinterpret_cast<const unsigned char*>(dump.AddressOf());
		while( cursor && *cursor )
		{
			digest ^= *cursor++;
			digest *= 1099511628211ULL;
		}
		return digest;
	}

	asCString FormatCanonicalLiteralBytes(const asCString& bytes)
	{
		asCString result;
		for( asUINT i = 0; i < bytes.GetLength(); ++i )
		{
			const asBYTE byte = static_cast<asBYTE>(bytes[i]);
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
					result += static_cast<char>(byte);
				break;
			}
		}
		return result;
	}

	const char* CanonicalDeclKindName(asEASTDeclKind kind)
	{
		switch( kind )
		{
		case asAST_DECL_TRANSLATION_UNIT: return "TranslationUnit";
		case asAST_DECL_NAMESPACE: return "Namespace";
		case asAST_DECL_TYPE: return "Type";
		case asAST_DECL_CLASS: return "Class";
		case asAST_DECL_INTERFACE: return "Interface";
		case asAST_DECL_ENUM: return "Enum";
		case asAST_DECL_FUNCDEF: return "FuncDef";
		case asAST_DECL_FUNCTION: return "Function";
		case asAST_DECL_METHOD: return "Method";
		case asAST_DECL_CONSTRUCTOR: return "Constructor";
		case asAST_DECL_DESTRUCTOR: return "Destructor";
		case asAST_DECL_VAR: return "Var";
		case asAST_DECL_PARAM: return "Param";
		case asAST_DECL_PROPERTY: return "Property";
		case asAST_DECL_IMPORT: return "Import";
		case asAST_DECL_TYPEDEF: return "Typedef";
		case asAST_DECL_MIXIN: return "Mixin";
		case asAST_DECL_ERROR: return "Error";
		case asAST_DECL_ACCESS_SPECIFIER: return "AccessSpecifier";
		case asAST_DECL_ACCESS_PERMISSION: return "AccessPermission";
		default: return "Invalid";
		}
	}

	bool CanonicalDeclIsSupportedForCodeGen(asEASTDeclKind kind)
	{
		switch( kind )
		{
		case asAST_DECL_TRANSLATION_UNIT:
		case asAST_DECL_CLASS:
		case asAST_DECL_FUNCTION:
		case asAST_DECL_METHOD:
		case asAST_DECL_CONSTRUCTOR:
		case asAST_DECL_DESTRUCTOR:
		case asAST_DECL_MIXIN:
		case asAST_DECL_VAR:
		case asAST_DECL_PARAM:
		case asAST_DECL_PROPERTY:
		case asAST_DECL_IMPORT:
		case asAST_DECL_NAMESPACE:
		case asAST_DECL_ENUM:
		case asAST_DECL_TYPEDEF:
		case asAST_DECL_ACCESS_SPECIFIER:
		case asAST_DECL_ACCESS_PERMISSION:
			return true;
		default:
			return false;
		}
	}

	asETypeModifiers InOutFromQuals(asDWORD quals);

	// Canonical declarations retain lexical parentage, whereas the runtime owns
	// a flat table of fully-qualified namespaces. Derive the runtime owner from
	// the sealed declaration tree, never from presentation text or by silently
	// falling back to module->defaultNamespace. Like the legacy builder, the
	// engine keeps namespace objects for its lifetime, so they are intentionally
	// not part of the module artifact rollback.
	asSNameSpace* ResolveCanonicalDeclNamespace(
		const asCASTContext& context,
		asCScriptEngine* engine,
		asCModule* module,
		const asCDecl* decl)
	{
		if( engine == 0 || module == 0 || decl == 0 || module->defaultNamespace == 0 )
		{
			return 0;
		}

		asCArray<const asCDecl*> namespaceScopes;
		const asCDecl* scope = decl;
		while( scope )
		{
			if( scope->kind == asAST_DECL_NAMESPACE )
			{
				namespaceScopes.PushLast(scope);
			}
			scope = scope->parent.IsValid() ? context.GetDecl(scope->parent) : 0;
		}

		asSNameSpace* runtimeNamespace = module->defaultNamespace;
		asCString qualifiedName = runtimeNamespace->name;
		for( asUINT i = namespaceScopes.GetLength(); i > 0; --i )
		{
			const asCDecl* const namespaceDecl = namespaceScopes[i - 1];
			if( namespaceDecl == 0 || namespaceDecl->name.GetLength() == 0 )
			{
				return 0;
			}
			if( qualifiedName.GetLength() )
			{
				qualifiedName += "::";
			}
			qualifiedName += namespaceDecl->name;
			runtimeNamespace = engine->AddNameSpace(qualifiedName.AddressOf());
			if( runtimeNamespace == 0 )
			{
				return 0;
			}
		}
		return runtimeNamespace;
	}

	bool IsCanonicalNamespaceOrTranslationUnitMember(const asCASTContext& context, const asCDecl* decl)
	{
		if( decl == 0 )
		{
			return false;
		}
		const asCDecl* const parent = context.GetDecl(decl->parent);
		return parent != 0
			&& (parent->kind == asAST_DECL_TRANSLATION_UNIT || parent->kind == asAST_DECL_NAMESPACE);
	}

	asCTypeInfo* FindRegisteredTypeInNamespace(
		asCScriptEngine* engine,
		const asCString& name,
		asSNameSpace* nameSpace)
	{
		if( engine == 0 || name.GetLength() == 0 || nameSpace == 0 )
		{
			return 0;
		}
	asCTypeInfo* result = 0;
	engine->allRegisteredTypesByName.FindAllUntil(name.AddressOf(), [&](asCTypeInfo* candidate)
		{
			if( candidate && candidate->nameSpace == nameSpace )
			{
				result = candidate;
				return true;
			}
			return false;
		});
	if( result )
		return result;
	engine->allScriptDeclaredTypes.FindAllUntil(name.AddressOf(), [&](asCTypeInfo* candidate)
	{
		if( candidate && candidate->nameSpace == nameSpace )
		{
			result = candidate;
			return true;
		}
		return false;
	});
	return result;
}

	asCTypeInfo* FindRegisteredHostTypeInNamespace(
		asCScriptEngine* engine,
		const asCString& name,
		asSNameSpace* nameSpace)
	{
		if( engine == 0 || name.GetLength() == 0 || nameSpace == 0 )
		{
			return 0;
		}
		asCTypeInfo* result = 0;
		engine->allRegisteredTypesByName.FindAllUntil(name.AddressOf(), [&](asCTypeInfo* candidate)
		{
			if( candidate && candidate->nameSpace == nameSpace )
			{
				result = candidate;
				return true;
			}
			return false;
		});
		return result;
	}

	asCScriptFunction* FindRegisteredGlobalFunction(
		asCScriptEngine* engine,
		asCModule* module,
		asCRuntimeTypeBridge& bridge,
		const asCASTContext& context,
		const asCDecl* decl)
	{
		if( engine == 0 || decl == 0 || decl->kind != asAST_DECL_FUNCTION || decl->body.IsValid() )
		{
			return 0;
		}
		asSNameSpace* const nameSpace = ResolveCanonicalDeclNamespace(context, engine, module, decl);
		if( nameSpace == 0 )
		{
			return 0;
		}
		if( (decl->traits & asAST_TRAIT_LAMBDA) != 0 )
		{
			return 0;
		}

		asCArray<asCDataType> wantParams;
		asCArray<asETypeModifiers> wantInOut;
		for( asUINT i = 0; i < decl->children.GetLength(); ++i )
		{
			const asCDecl* child = context.GetDecl(decl->children[i]);
			if( child == 0 || child->kind != asAST_DECL_PARAM )
			{
				continue;
			}
			asCDataType paramType = bridge.Resolve(context, child->type);
			if( !paramType.IsValid() )
			{
				return 0;
			}
			wantParams.PushLast(paramType);
			wantInOut.PushLast(InOutFromQuals(child->type.quals));
		}
		asCDataType wantRet = bridge.Resolve(context, decl->type);

		asCScriptFunction* unique = 0;
		int matches = 0;
		const asUINT count = engine->GetGlobalFunctionCount();
		for( asUINT i = 0; i < count; ++i )
		{
			asCScriptFunction* func = static_cast<asCScriptFunction*>(engine->GetGlobalFunctionByIndex(i));
			if( func == 0
				|| func->funcType != asFUNC_SYSTEM
				|| !func->name.Equals(decl->name)
				|| func->nameSpace != nameSpace
				|| func->parameterTypes.GetLength() != wantParams.GetLength() )
			{
				continue;
			}
			if( func->returnType.GetTokenType() != wantRet.GetTokenType()
				|| func->returnType.GetTypeInfo() != wantRet.GetTypeInfo()
				|| func->returnType.IsReference() != wantRet.IsReference()
				|| func->returnType.IsObjectHandle() != wantRet.IsObjectHandle() )
			{
				continue;
			}
			bool sameParams = true;
			for( asUINT p = 0; p < wantParams.GetLength(); ++p )
			{
				const asCDataType& paramType = func->parameterTypes[p];
				if( paramType.GetTokenType() != wantParams[p].GetTokenType()
					|| paramType.GetTypeInfo() != wantParams[p].GetTypeInfo()
					|| paramType.IsReference() != wantParams[p].IsReference()
					|| paramType.IsObjectHandle() != wantParams[p].IsObjectHandle() )
				{
					sameParams = false;
					break;
				}
				if( func->inOutFlags.GetLength() > p
					&& wantInOut[p] != asTM_NONE
					&& func->inOutFlags[p] != wantInOut[p] )
				{
					sameParams = false;
					break;
				}
			}
			if( !sameParams )
			{
				continue;
			}
			if( unique && unique->id == func->id )
			{
				continue;
			}
			if( unique )
			{
				++matches;
				continue;
			}
			++matches;
			unique = func;
		}
		return (matches == 1) ? unique : 0;
	}

	// Stage 2 has already registered explicit imports and reserved their Engine
	// import ids. Prepared CodeGen must bind the sealed declaration to that exact
	// shell instead of allocating a duplicate slot or reconstructing identity by
	// name. Source module, namespace, complete Runtime types, parameter direction,
	// and the producer-carried stable key all participate in the match.
	void AppendPreparedImportMismatch(asCString& fields, const char* field)
	{
		if( fields.GetLength() != 0 )
			fields += ',';
		fields += field;
	}

	asCScriptFunction* FindPreparedImportedFunction(
		asCScriptEngine* engine,
		asCModule* module,
		asCRuntimeTypeBridge& bridge,
		const asCASTContext& context,
		const asCDecl* decl,
		asUINT& outMatches,
		asCString& outDiagnostic)
	{
		outMatches = 0;
		outDiagnostic = "runtime-shells=unavailable";
		if( engine == 0 || module == 0 || decl == 0
			|| decl->kind != asAST_DECL_IMPORT
			|| decl->origin.GetLength() == 0
			|| decl->stableKey.GetLength() == 0 )
		{
			return 0;
		}

		asSNameSpace* const nameSpace = ResolveCanonicalDeclNamespace(
			context, engine, module, decl);
		const asCDataType returnType = bridge.Resolve(context, decl->type);
		if( nameSpace == 0 || !returnType.IsValid() )
		{
			return 0;
		}

		asCArray<asCDataType> parameterTypes;
		asCArray<asETypeModifiers> inOutFlags;
		for( asUINT childIndex = 0;
			childIndex < decl->children.GetLength(); ++childIndex )
		{
			const asCDecl* const child = context.GetDecl(
				decl->children[childIndex]);
			if( child == 0 || child->kind != asAST_DECL_PARAM )
			{
				continue;
			}
			asCDataType parameterType = bridge.Resolve(
				context, child->type);
			asETypeModifiers passing = InOutFromQuals(child->type.quals);
			if( !parameterType.IsValid()
				|| parameterType.GetTokenType() == ttVoid )
			{
				return 0;
			}
			asCRuntimeTypeBridge::NormalizeScriptParameterABI(
				parameterType, passing);
			parameterTypes.PushLast(parameterType);
			inOutFlags.PushLast(passing);
		}

		outDiagnostic.Format(
			"runtime-shells=%u", module->bindInformations.GetLength());
		asCScriptFunction* unique = 0;
		for( asUINT importIndex = 0;
			importIndex < module->bindInformations.GetLength(); ++importIndex )
		{
			sBindInfo* const info = module->bindInformations[importIndex];
			asCScriptFunction* const function = info
				? info->importedFunctionSignature : 0;
			asCString mismatches;
			bool engineSlotValid = false;
			if( info == 0 )
				AppendPreparedImportMismatch(mismatches, "null-info");
			if( function == 0 )
			{
				AppendPreparedImportMismatch(mismatches, "null-function");
			}
			else
			{
				if( function->module != module )
					AppendPreparedImportMismatch(mismatches, "owner");
				if( function->funcType != asFUNC_IMPORTED )
					AppendPreparedImportMismatch(mismatches, "function-kind");
				if( (function->id & FUNC_IMPORTED) == 0 )
					AppendPreparedImportMismatch(mismatches, "imported-id");
				if( !function->name.Equals(decl->name) )
					AppendPreparedImportMismatch(mismatches, "name");
				if( function->nameSpace != nameSpace )
					AppendPreparedImportMismatch(mismatches, "namespace");
				if( function->canonicalASTStableDeclKey != decl->stableKey )
					AppendPreparedImportMismatch(mismatches, "stable-key");
				if( info == 0 || info->importFromModule != decl->origin )
					AppendPreparedImportMismatch(mismatches, "source-module");
				if( function->returnType != returnType )
					AppendPreparedImportMismatch(mismatches, "return-abi");
				if( function->parameterTypes.GetLength()
						!= parameterTypes.GetLength() )
					AppendPreparedImportMismatch(mismatches, "parameter-count");
				if( function->inOutFlags.GetLength() != inOutFlags.GetLength() )
					AppendPreparedImportMismatch(mismatches, "direction-count");

				const int engineImportIndex = function->id & ~FUNC_IMPORTED;
				engineSlotValid = engineImportIndex >= 0
					&& asUINT(engineImportIndex) < engine->importedFunctions.GetLength()
					&& engine->importedFunctions[engineImportIndex] == info;
				if( !engineSlotValid )
					AppendPreparedImportMismatch(mismatches, "engine-slot");

				if( function->parameterTypes.GetLength()
						== parameterTypes.GetLength()
					&& function->inOutFlags.GetLength() == inOutFlags.GetLength() )
				{
					for( asUINT parameterIndex = 0;
						parameterIndex < parameterTypes.GetLength(); ++parameterIndex )
					{
						if( function->parameterTypes[parameterIndex]
								!= parameterTypes[parameterIndex]
							|| function->inOutFlags[parameterIndex]
								!= inOutFlags[parameterIndex] )
						{
							AppendPreparedImportMismatch(
								mismatches, "parameter-abi");
							break;
						}
					}
				}
			}

			// Keep generated-module diagnostics bounded while retaining exact
			// Stage2 identity and the field-level reason for every nearby shell.
			if( importIndex < 8 )
			{
				asCString shellDetail;
				shellDetail.Format(
					" [slot=%u id=%d name=%s ns=%s key=%s from=%s owner=%s "
					"kind=%d engine-slot=%s mismatches=%s]",
					importIndex,
					function ? function->id : -1,
					function && function->name.AddressOf()
						? function->name.AddressOf() : "<missing>",
					function && function->nameSpace
						&& function->nameSpace->name.AddressOf()
						? function->nameSpace->name.AddressOf() : "<global>",
					function && function->canonicalASTStableDeclKey.AddressOf()
						? function->canonicalASTStableDeclKey.AddressOf() : "<missing>",
					info && info->importFromModule.AddressOf()
						? info->importFromModule.AddressOf() : "<missing>",
					function
						? (function->module == module ? "current" : "other")
						: "missing",
					function ? int(function->funcType) : -1,
					engineSlotValid ? "valid" : "invalid",
					mismatches.GetLength() != 0
						? mismatches.AddressOf() : "none");
				outDiagnostic += shellDetail;
			}

			if( mismatches.GetLength() != 0 )
				continue;

			++outMatches;
			unique = function;
		}
		if( module->bindInformations.GetLength() > 8 )
		{
			asCString truncated;
			truncated.Format(
				" [truncated=%u]", module->bindInformations.GetLength() - 8);
			outDiagnostic += truncated;
		}
		return outMatches == 1 ? unique : 0;
	}

	// Automatic-import mode intentionally owns no mutable CALLBND slot. In a
	// multi-module source batch every module finishes Stage2 before any body is
	// emitted, so bind the sealed explicit ImportDecl to the exact provider's
	// already-registered script-function shell. The source module, stable key,
	// complete ABI, and Engine slot are all required; a same-named overload or
	// another provider generation must never be selected by convenience lookup.
	asCScriptFunction* FindPreparedAutomaticImportedFunction(
		asCScriptEngine* engine,
		asCModule* module,
		asCRuntimeTypeBridge& bridge,
		const asCASTContext& context,
		const asCDecl* decl,
		asUINT& outMatches,
		asCString& outDiagnostic)
	{
		outMatches = 0;
		outDiagnostic = "automatic-provider-candidates=unavailable";
		if( engine == 0 || module == 0 || decl == 0
			|| !engine->ep.automaticImports
			|| decl->kind != asAST_DECL_IMPORT
			|| decl->origin.GetLength() == 0
			|| decl->stableKey.GetLength() == 0 )
		{
			return 0;
		}

		asSNameSpace* const nameSpace = ResolveCanonicalDeclNamespace(
			context, engine, module, decl);
		const asCDataType returnType = bridge.Resolve(context, decl->type);
		if( nameSpace == 0 || !returnType.IsValid() )
			return 0;

		asCArray<asCDataType> parameterTypes;
		asCArray<asETypeModifiers> inOutFlags;
		for( asUINT childIndex = 0;
			childIndex < decl->children.GetLength(); ++childIndex )
		{
			const asCDecl* const child = context.GetDecl(
				decl->children[childIndex]);
			if( child == 0 || child->kind != asAST_DECL_PARAM )
				continue;
			asCDataType parameterType = bridge.Resolve(context, child->type);
			asETypeModifiers passing = InOutFromQuals(child->type.quals);
			if( !parameterType.IsValid()
				|| parameterType.GetTokenType() == ttVoid )
			{
				return 0;
			}
			asCRuntimeTypeBridge::NormalizeScriptParameterABI(
				parameterType, passing);
			parameterTypes.PushLast(parameterType);
			inOutFlags.PushLast(passing);
		}

		asCScriptFunction* unique = 0;
		asUINT candidates = 0;
		asCString inventory;
		engine->allScriptGlobalFunctions.FindAll(
			decl->name.AddressOf(), nameSpace,
			[&](asCScriptFunction* function)
		{
			++candidates;
			asCString mismatches;
			bool engineSlotValid = false;
			if( function == 0 )
			{
				AppendPreparedImportMismatch(mismatches, "null-function");
			}
			else
			{
				const asCString* producerModuleName = 0;
				if( function->module )
				{
					producerModuleName = function->module->baseModuleName.GetLength()
						? &function->module->baseModuleName : &function->module->name;
				}
				if( function->module == 0 || function->module == module )
					AppendPreparedImportMismatch(mismatches, "owner");
				if( producerModuleName == 0
					|| *producerModuleName != decl->origin )
					AppendPreparedImportMismatch(mismatches, "source-module");
				if( function->funcType != asFUNC_SCRIPT )
					AppendPreparedImportMismatch(mismatches, "function-kind");
				if( function->objectType != 0 )
					AppendPreparedImportMismatch(mismatches, "object-owner");
				if( function->traits.GetTrait(asTRAIT_LOCAL) )
					AppendPreparedImportMismatch(mismatches, "local-visibility");
				if( function->canonicalASTStableDeclKey != decl->stableKey )
					AppendPreparedImportMismatch(mismatches, "stable-key");
				if( function->returnType != returnType )
					AppendPreparedImportMismatch(mismatches, "return-abi");
				if( function->parameterTypes.GetLength()
						!= parameterTypes.GetLength() )
					AppendPreparedImportMismatch(mismatches, "parameter-count");
				if( function->inOutFlags.GetLength() != inOutFlags.GetLength() )
					AppendPreparedImportMismatch(mismatches, "direction-count");

				engineSlotValid = function->id >= 0
					&& asUINT(function->id) < engine->scriptFunctions.GetLength()
					&& engine->scriptFunctions[function->id] == function;
				if( !engineSlotValid )
					AppendPreparedImportMismatch(mismatches, "engine-slot");

				if( function->parameterTypes.GetLength()
						== parameterTypes.GetLength()
					&& function->inOutFlags.GetLength() == inOutFlags.GetLength() )
				{
					for( asUINT parameterIndex = 0;
						parameterIndex < parameterTypes.GetLength(); ++parameterIndex )
					{
						if( function->parameterTypes[parameterIndex]
								!= parameterTypes[parameterIndex]
							|| function->inOutFlags[parameterIndex]
								!= inOutFlags[parameterIndex] )
						{
							AppendPreparedImportMismatch(
								mismatches, "parameter-abi");
							break;
						}
					}
				}
			}

			if( candidates <= 8 )
			{
				const asCString* producerModuleName = 0;
				if( function && function->module )
				{
					producerModuleName = function->module->baseModuleName.GetLength()
						? &function->module->baseModuleName : &function->module->name;
				}
				asCString candidateDetail;
				candidateDetail.Format(
					" [id=%d provider=%s key=%s kind=%d engine-slot=%s mismatches=%s]",
					function ? function->id : -1,
					producerModuleName && producerModuleName->AddressOf()
						? producerModuleName->AddressOf() : "<missing>",
					function && function->canonicalASTStableDeclKey.AddressOf()
						? function->canonicalASTStableDeclKey.AddressOf() : "<missing>",
					function ? int(function->funcType) : -1,
					engineSlotValid ? "valid" : "invalid",
					mismatches.GetLength() != 0
						? mismatches.AddressOf() : "none");
				inventory += candidateDetail;
			}

			if( mismatches.GetLength() == 0 )
			{
				++outMatches;
				unique = function;
			}
		});

		outDiagnostic.Format("automatic-provider-candidates=%u", candidates);
		outDiagnostic += inventory;
		if( candidates > 8 )
		{
			asCString truncated;
			truncated.Format(" [truncated=%u]", candidates - 8);
			outDiagnostic += truncated;
		}
		return outMatches == 1 ? unique : 0;
	}

	// Sema represents a qualified automatic import from a previously committed
	// script module as a declaration-only AST node. Bind that node to its exact
	// runtime function here; it is not a consumer-owned declaration and must
	// never allocate a second script-function id during CodeGen.
	asCScriptFunction* FindAutomaticImportedScriptGlobalFunction(
		asCScriptEngine* engine,
		asCModule* module,
		asCRuntimeTypeBridge& bridge,
		const asCASTContext& context,
		const asCDecl* decl)
	{
		if( engine == 0 || module == 0 || decl == 0 || decl->kind != asAST_DECL_FUNCTION
			|| decl->body.IsValid() || !IsCanonicalAutomaticImportScriptFunction(decl)
			|| !engine->ep.automaticImports )
		{
			return 0;
		}
		asSNameSpace* const nameSpace = ResolveCanonicalDeclNamespace(context, engine, module, decl);
		if( nameSpace == 0 )
		{
			return 0;
		}

		asCArray<asCDataType> wantParams;
		asCArray<asETypeModifiers> wantInOut;
		for( asUINT i = 0; i < decl->children.GetLength(); ++i )
		{
			const asCDecl* child = context.GetDecl(decl->children[i]);
			if( child == 0 || child->kind != asAST_DECL_PARAM )
			{
				continue;
			}
			asCDataType paramType = bridge.Resolve(context, child->type);
			if( !paramType.IsValid() )
			{
				return 0;
			}
			wantParams.PushLast(paramType);
			wantInOut.PushLast(InOutFromQuals(child->type.quals));
		}
		const asCDataType wantRet = bridge.Resolve(context, decl->type);
		if( !wantRet.IsValid() )
		{
			return 0;
		}

		asCScriptFunction* unique = 0;
		int matches = 0;
		engine->allScriptGlobalFunctions.FindAll(decl->name.AddressOf(), nameSpace, [&](asCScriptFunction* function)
		{
			if( function == 0 || function->module == module || function->objectType != 0
				|| function->traits.GetTrait(asTRAIT_LOCAL)
				|| function->parameterTypes.GetLength() != wantParams.GetLength()
				|| function->returnType.GetTokenType() != wantRet.GetTokenType()
				|| function->returnType.GetTypeInfo() != wantRet.GetTypeInfo()
				|| function->returnType.IsReference() != wantRet.IsReference()
				|| function->returnType.IsObjectHandle() != wantRet.IsObjectHandle() )
			{
				return;
			}
			for( asUINT p = 0; p < wantParams.GetLength(); ++p )
			{
				const asCDataType& parameter = function->parameterTypes[p];
				if( parameter.GetTokenType() != wantParams[p].GetTokenType()
					|| parameter.GetTypeInfo() != wantParams[p].GetTypeInfo()
					|| parameter.IsReference() != wantParams[p].IsReference()
					|| parameter.IsObjectHandle() != wantParams[p].IsObjectHandle()
					|| (function->inOutFlags.GetLength() > p && wantInOut[p] != asTM_NONE
						&& function->inOutFlags[p] != wantInOut[p]) )
				{
					return;
				}
			}
			++matches;
			unique = function;
		});
		return matches == 1 ? unique : 0;
	}

	// CompileFunction's ephemeral AST contains declaration-only views of the
	// functions already published by its own module. Resolve those views only
	// against that module's authoritative inventory. Looking through the
	// engine-wide automatic-import map here would make module-local/private
	// functions depend on automaticImports and could bind a same-named producer.
	asCScriptFunction* FindCurrentModuleScriptGlobalFunction(
		asCModule* module,
		asCRuntimeTypeBridge& bridge,
		const asCASTContext& context,
		const asCDecl* decl)
	{
		if( module == 0 || decl == 0 || decl->kind != asAST_DECL_FUNCTION
			|| decl->body.IsValid() || !IsCanonicalCurrentModuleScriptFunction(decl) )
		{
			return 0;
		}
		asSNameSpace* const nameSpace = ResolveCanonicalDeclNamespace(
			context, module->engine, module, decl);
		if( nameSpace == 0 )
		{
			return 0;
		}

		asCArray<asCDataType> wantParams;
		asCArray<asETypeModifiers> wantInOut;
		for( asUINT i = 0; i < decl->children.GetLength(); ++i )
		{
			const asCDecl* child = context.GetDecl(decl->children[i]);
			if( child == 0 || child->kind != asAST_DECL_PARAM )
			{
				continue;
			}
			const asCDataType paramType = bridge.Resolve(context, child->type);
			if( !paramType.IsValid() )
			{
				return 0;
			}
			wantParams.PushLast(paramType);
			wantInOut.PushLast(InOutFromQuals(child->type.quals));
		}
		const asCDataType wantRet = bridge.Resolve(context, decl->type);
		if( !wantRet.IsValid() )
		{
			return 0;
		}

		asCScriptFunction* unique = 0;
		int matches = 0;
		for( asUINT i = 0; i < module->globalFunctionList.GetLength(); ++i )
		{
			asCScriptFunction* const function = module->globalFunctionList[i];
			if( function == 0 || function->module != module || function->objectType != 0
				|| function->nameSpace != nameSpace || !function->name.Equals(decl->name)
				|| function->parameterTypes.GetLength() != wantParams.GetLength()
				|| function->returnType.GetTokenType() != wantRet.GetTokenType()
				|| function->returnType.GetTypeInfo() != wantRet.GetTypeInfo()
				|| function->returnType.IsReference() != wantRet.IsReference()
				|| function->returnType.IsObjectHandle() != wantRet.IsObjectHandle() )
			{
				continue;
			}
			bool sameParams = true;
			for( asUINT p = 0; p < wantParams.GetLength(); ++p )
			{
				const asCDataType& parameter = function->parameterTypes[p];
				if( parameter.GetTokenType() != wantParams[p].GetTokenType()
					|| parameter.GetTypeInfo() != wantParams[p].GetTypeInfo()
					|| parameter.IsReference() != wantParams[p].IsReference()
					|| parameter.IsObjectHandle() != wantParams[p].IsObjectHandle()
					|| (function->inOutFlags.GetLength() > p && wantInOut[p] != asTM_NONE
						&& function->inOutFlags[p] != wantInOut[p]) )
				{
					sameParams = false;
					break;
				}
			}
			if( !sameParams )
			{
				continue;
			}
			++matches;
			unique = function;
		}
		return matches == 1 ? unique : 0;
	}

	// The Sema-side declaration is only a lexical projection of an already
	// published provider global. Bind it to that precise property and leave
	// ownership with the provider/engine; the consumer function takes its own
	// property reference when its bytecode references are installed.
	asCGlobalProperty* FindAutomaticImportedScriptGlobal(
		asCScriptEngine* engine,
		asCModule* module,
		asCRuntimeTypeBridge& bridge,
		const asCASTContext& context,
		const asCDecl* decl)
	{
		if( engine == 0 || module == 0 || decl == 0 || decl->kind != asAST_DECL_VAR
			|| !IsCanonicalAutomaticImportScriptGlobal(decl) || !engine->ep.automaticImports )
		{
			return 0;
		}
		asSNameSpace* const nameSpace = ResolveCanonicalDeclNamespace(context, engine, module, decl);
		if( nameSpace == 0 )
		{
			return 0;
		}
		asCGlobalProperty* const property = engine->allScriptGlobalVariables.FindFirst(decl->name.AddressOf(), nameSpace);
		if( property == 0 || property->module == module )
		{
			return 0;
		}
		const asCDataType type = bridge.Resolve(context, decl->type);
		return type.IsValid() && type == property->type ? property : 0;
	}

	asCGlobalProperty* FindRegisteredNativeGlobalProperty(
		asCScriptEngine* engine,
		asCRuntimeTypeBridge& bridge,
		const asCASTContext& context,
		const asCDecl* decl)
	{
		if( engine == 0 || decl == 0 || decl->kind != asAST_DECL_VAR
			|| !IsCanonicalNativeGlobalProperty(decl) )
		{
			return 0;
		}
		asCGlobalProperty* unique = 0;
		int matches = 0;
		for( asUINT i = 0; i < engine->registeredGlobalProps.GetLength(); ++i )
		{
			asCGlobalProperty* const property = engine->registeredGlobalProps[i];
			if( property == 0 || !property->name.Equals(decl->name) )
			{
				continue;
			}
			const asCDataType type = bridge.Resolve(context, decl->type);
			if( !type.IsValid()
				|| (type != property->type
					&& !type.IsEqualExceptConst(property->type)) )
			{
				continue;
			}
			++matches;
			unique = property;
		}
		return matches == 1 ? unique : 0;
	}

	asCGlobalProperty* FindCurrentModuleScriptGlobal(
		asCModule* module,
		asCRuntimeTypeBridge& bridge,
		const asCASTContext& context,
		const asCDecl* decl)
	{
		if( module == 0 || decl == 0 || decl->kind != asAST_DECL_VAR
			|| !IsCanonicalCurrentModuleScriptGlobal(decl) )
		{
			return 0;
		}
		asSNameSpace* const nameSpace = ResolveCanonicalDeclNamespace(
			context, module->engine, module, decl);
		if( nameSpace == 0 )
		{
			return 0;
		}
		asCGlobalProperty* unique = 0;
		int matches = 0;
		for( asUINT i = 0; i < module->scriptGlobalsList.GetLength(); ++i )
		{
			asCGlobalProperty* const property = module->scriptGlobalsList[i];
			if( property && property->module == module
				&& property->nameSpace == nameSpace
				&& property->name.Equals(decl->name) )
			{
				++matches;
				unique = property;
			}
		}
		if( matches != 1 )
		{
			return 0;
		}
		const asCDataType type = bridge.Resolve(context, decl->type);
		return type.IsValid() && type == unique->type ? unique : 0;
	}

	asCScriptFunction* FindExactRegisteredMethod(
		asCScriptEngine* engine,
		asCRuntimeTypeBridge& bridge,
		const asCASTContext& context,
		const asCDecl* decl)
	{
		if( engine == 0 || decl == 0 || decl->kind != asAST_DECL_METHOD || decl->body.IsValid() )
		{
			return 0;
		}
		if( (decl->traits & asAST_TRAIT_GENERATED) != 0 )
		{
			return 0;
		}
		const asCDecl* cls = context.GetDecl(decl->parent);
		if( cls == 0 )
		{
			return 0;
		}
		const char* typeKey = cls->stableKey.GetLength() ? cls->stableKey.AddressOf() : cls->name.AddressOf();
		asCObjectType* objType = CastToObjectType(static_cast<asCTypeInfo*>(engine->GetTypeInfoByDecl(typeKey)));
		if( objType == 0 && cls->name.GetLength() )
		{
			objType = CastToObjectType(static_cast<asCTypeInfo*>(engine->GetTypeInfoByName(cls->name.AddressOf())));
		}
		if( objType == 0 )
		{
			return 0;
		}

		asCArray<asCDataType> wantParams;
		asCArray<asETypeModifiers> wantInOut;
		for( asUINT i = 0; i < decl->children.GetLength(); ++i )
		{
			const asCDecl* child = context.GetDecl(decl->children[i]);
			if( child == 0 || child->kind != asAST_DECL_PARAM )
			{
				continue;
			}
			asCDataType paramType = bridge.Resolve(context, child->type);
			if( !paramType.IsValid() )
			{
				return 0;
			}
			wantParams.PushLast(paramType);
			wantInOut.PushLast(InOutFromQuals(child->type.quals));
		}
		asCDataType wantRet = bridge.Resolve(context, decl->type);

		asCScriptFunction* unique = 0;
		int matches = 0;
		auto consider = [&](asCObjectType* /*search*/, asCScriptFunction* method)
		{
			if( method == 0 || !method->name.Equals(decl->name) )
			{
				return;
			}
			if( method->parameterTypes.GetLength() != wantParams.GetLength() )
			{
				return;
			}
			const bool fromTemplate = objType->templateBaseType != 0
				&& method->objectType == objType->templateBaseType;
			asCDataType retDt = method->returnType;
			if( fromTemplate )
			{
				retDt = engine->DetermineTypeForTemplate(retDt, objType->templateBaseType, objType);
			}
			if( retDt.GetTokenType() != wantRet.GetTokenType()
				|| retDt.GetTypeInfo() != wantRet.GetTypeInfo()
				|| retDt.IsReference() != wantRet.IsReference()
				|| retDt.IsObjectHandle() != wantRet.IsObjectHandle() )
			{
				return;
			}
			for( asUINT p = 0; p < wantParams.GetLength(); ++p )
			{
				asCDataType paramType = method->parameterTypes[p];
				if( fromTemplate )
				{
					paramType = engine->DetermineTypeForTemplate(paramType, objType->templateBaseType, objType);
				}
				if( paramType.GetTokenType() != wantParams[p].GetTokenType()
					|| paramType.GetTypeInfo() != wantParams[p].GetTypeInfo()
					|| paramType.IsReference() != wantParams[p].IsReference()
					|| paramType.IsObjectHandle() != wantParams[p].IsObjectHandle() )
				{
					return;
				}
				if( method->inOutFlags.GetLength() > p
					&& wantInOut[p] != asTM_NONE
					&& method->inOutFlags[p] != wantInOut[p] )
				{
					return;
				}
			}
			if( unique && unique->id == method->id )
			{
				return;
			}
			if( unique )
			{
				const bool haveInst = unique->objectType == objType;
				const bool newInst = method->objectType == objType;
				if( haveInst && !newInst )
				{
					return;
				}
				if( !haveInst && newInst )
				{
					unique = method;
					return;
				}
				++matches;
				return;
			}
			++matches;
			unique = method;
		};

		auto walk = [&](asCObjectType* search)
		{
			if( search == 0 || decl->name.GetLength() == 0 )
			{
				return;
			}
			search->FindMethodUntil(decl->name.AddressOf(), [&](asCScriptFunction* method)
			{
				consider(search, method);
				return false;
			});
		};

		walk(objType);
		if( matches == 0 && objType->templateBaseType )
		{
			walk(objType->templateBaseType);
		}
		return (matches == 1) ? unique : 0;
	}

	asCScriptFunction* FindExactRegisteredConstructor(
		asCScriptEngine* engine,
		asCRuntimeTypeBridge& bridge,
		const asCASTContext& context,
		const asCDecl* decl)
	{
		if( engine == 0 || decl == 0 || decl->kind != asAST_DECL_CONSTRUCTOR || decl->body.IsValid() )
		{
			return 0;
		}
		if( (decl->traits & (asAST_TRAIT_GENERATED | asAST_TRAIT_LIST_FACTORY)) != 0 )
		{
			return 0;
		}
		const asCDecl* cls = context.GetDecl(decl->parent);
		if( cls == 0 )
		{
			return 0;
		}
		const char* typeKey = cls->stableKey.GetLength() ? cls->stableKey.AddressOf() : cls->name.AddressOf();
		asCObjectType* objType = CastToObjectType(static_cast<asCTypeInfo*>(engine->GetTypeInfoByDecl(typeKey)));
		if( objType == 0 && cls->name.GetLength() )
		{
			objType = CastToObjectType(static_cast<asCTypeInfo*>(engine->GetTypeInfoByName(cls->name.AddressOf())));
		}
		if( objType == 0 )
		{
			return 0;
		}

		asCArray<asCDataType> wantParams;
		asCArray<asETypeModifiers> wantInOut;
		for( asUINT i = 0; i < decl->children.GetLength(); ++i )
		{
			const asCDecl* child = context.GetDecl(decl->children[i]);
			if( child == 0 || child->kind != asAST_DECL_PARAM )
			{
				continue;
			}
			asCDataType paramType = bridge.Resolve(context, child->type);
			if( !paramType.IsValid() )
			{
				return 0;
			}
			wantParams.PushLast(paramType);
			wantInOut.PushLast(InOutFromQuals(child->type.quals));
		}

		asCScriptFunction* unique = 0;
		int matches = 0;
		asCScriptFunction* namedUnique = 0;
		int namedMatches = 0;
		auto consider = [&](asCScriptFunction* func)
		{
			if( func == 0 )
			{
				return;
			}
			asUINT start = 0;
			if( func->hiddenArgumentIndex >= 0 )
			{
				start = (asUINT)func->hiddenArgumentIndex + 1;
				if( start > func->parameterTypes.GetLength() )
				{
					start = func->parameterTypes.GetLength();
				}
			}
			else
			{
				bool listedFactory = false;
				for( asUINT f = 0; f < objType->beh.factories.GetLength(); ++f )
				{
					if( objType->beh.factories[f] == func->id )
					{
						listedFactory = true;
						break;
					}
				}
				if( !listedFactory && objType->beh.factory == func->id )
				{
					listedFactory = true;
				}
				if( listedFactory && func->parameterTypes.GetLength() > 0
					&& (objType->templateBaseType
						|| (func->parameterTypes[0].IsReference() && !func->parameterTypes[0].IsPrimitive())) )
				{
					start = 1;
				}
			}
			const asUINT userCount = func->parameterTypes.GetLength() > start
				? func->parameterTypes.GetLength() - start
				: 0;
			if( userCount != wantParams.GetLength() )
			{
				return;
			}
			const bool fromTemplate = objType->templateBaseType != 0
				&& func->objectType == objType->templateBaseType;
			for( asUINT p = 0; p < wantParams.GetLength(); ++p )
			{
				asCDataType paramType = func->parameterTypes[start + p];
				if( fromTemplate )
				{
					paramType = engine->DetermineTypeForTemplate(paramType, objType->templateBaseType, objType);
				}
				if( paramType.GetTokenType() != wantParams[p].GetTokenType()
					|| paramType.GetTypeInfo() != wantParams[p].GetTypeInfo()
					|| paramType.IsReference() != wantParams[p].IsReference()
					|| paramType.IsObjectHandle() != wantParams[p].IsObjectHandle() )
				{
					return;
				}
				const asUINT flagIndex = start + p;
				if( func->inOutFlags.GetLength() > flagIndex
					&& wantInOut[p] != asTM_NONE
					&& func->inOutFlags[flagIndex] != wantInOut[p] )
				{
					return;
				}
			}
			if( unique && unique->id == func->id )
			{
				return;
			}
			++matches;
			unique = func;
			if( decl->name.GetLength() && func->name == decl->name )
			{
				if( namedUnique == 0 || namedUnique->id != func->id )
				{
					++namedMatches;
					namedUnique = func;
				}
			}
		};

		for( asUINT i = 0; i < objType->beh.constructors.GetLength(); ++i )
		{
			const int id = objType->beh.constructors[i];
			asCScriptFunction* func = (id >= 0 && (asUINT)id < engine->scriptFunctions.GetLength())
				? engine->scriptFunctions[id]
				: 0;
			consider(func);
		}
		if( objType->beh.construct > 0 )
		{
			asCScriptFunction* func = (asUINT)objType->beh.construct < engine->scriptFunctions.GetLength()
				? engine->scriptFunctions[objType->beh.construct]
				: 0;
			consider(func);
		}
		for( asUINT i = 0; i < objType->beh.factories.GetLength(); ++i )
		{
			const int id = objType->beh.factories[i];
			asCScriptFunction* func = (id >= 0 && (asUINT)id < engine->scriptFunctions.GetLength())
				? engine->scriptFunctions[id]
				: 0;
			consider(func);
		}
		if( objType->beh.factory > 0 )
		{
			asCScriptFunction* func = (asUINT)objType->beh.factory < engine->scriptFunctions.GetLength()
				? engine->scriptFunctions[objType->beh.factory]
				: 0;
			consider(func);
		}
		if( matches == 0 && objType->templateBaseType )
		{
			asCObjectType* base = objType->templateBaseType;
			for( asUINT i = 0; i < base->beh.factories.GetLength(); ++i )
			{
				const int id = base->beh.factories[i];
				asCScriptFunction* func = (id >= 0 && (asUINT)id < engine->scriptFunctions.GetLength())
					? engine->scriptFunctions[id]
					: 0;
				consider(func);
			}
		}
		// Template instances expose both the registered base factory and the
		// generated per-instance $fact stub. They have the same user-visible
		// signature, but only the stub has the canonical declaration's name and
		// already captures the concrete template type. Prefer that exact-name
		// match before falling back to the ordinary unique-signature rule.
		if( namedMatches == 1 )
		{
			return namedUnique;
		}
		return (matches == 1) ? unique : 0;
	}

	asCScriptFunction* FindExactRegisteredListFactory(
		asCScriptEngine* engine,
		asCRuntimeTypeBridge& bridge,
		const asCASTContext& context,
		const asCDecl* decl)
	{
		if( engine == 0 || decl == 0 || decl->kind != asAST_DECL_CONSTRUCTOR || decl->body.IsValid()
			|| (decl->traits & asAST_TRAIT_LIST_FACTORY) == 0 )
		{
			return 0;
		}
		const asCDecl* cls = context.GetDecl(decl->parent);
		if( cls == 0 )
		{
			return 0;
		}
		const char* typeKey = cls->stableKey.GetLength() ? cls->stableKey.AddressOf() : cls->name.AddressOf();
		asCObjectType* objType = CastToObjectType(static_cast<asCTypeInfo*>(engine->GetTypeInfoByDecl(typeKey)));
		if( objType == 0 && cls->name.GetLength() )
		{
			objType = CastToObjectType(static_cast<asCTypeInfo*>(engine->GetTypeInfoByName(cls->name.AddressOf())));
		}
		if( objType == 0 || objType->beh.listFactory <= 0
			|| (asUINT)objType->beh.listFactory >= engine->scriptFunctions.GetLength() )
		{
			return 0;
		}
		asCScriptFunction* const factory = engine->scriptFunctions[objType->beh.listFactory];
		const char* const factoryName = factory && factory->name.GetLength()
			? factory->name.AddressOf()
			: objType->name.AddressOf();
		if( factory == 0 || !decl->name.Equals(factoryName) )
		{
			return 0;
		}

		asUINT start = 0;
		if( factory->hiddenArgumentIndex >= 0 )
		{
			start = (asUINT)factory->hiddenArgumentIndex + 1;
			if( start > factory->parameterTypes.GetLength() )
			{
				start = factory->parameterTypes.GetLength();
			}
		}
		else if( factory->parameterTypes.GetLength() > 0
			&& factory->parameterTypes[0].IsReference()
			&& !factory->parameterTypes[0].IsPrimitive() )
		{
			start = 1;
		}
		else if( objType->templateBaseType && factory->parameterTypes.GetLength() > 0 )
		{
			start = 1;
		}

		asUINT paramIndex = 0;
		for( asUINT i = 0; i < decl->children.GetLength(); ++i )
		{
			const asCDecl* param = context.GetDecl(decl->children[i]);
			if( param == 0 || param->kind != asAST_DECL_PARAM )
			{
				continue;
			}
			if( start + paramIndex >= factory->parameterTypes.GetLength() )
			{
				return 0;
			}
			asCDataType actual = factory->parameterTypes[start + paramIndex];
			if( objType->templateBaseType && factory->objectType == objType->templateBaseType )
			{
				actual = engine->DetermineTypeForTemplate(actual, objType->templateBaseType, objType);
			}
			const asCDataType expected = bridge.Resolve(context, param->type);
			if( !expected.IsValid()
				|| actual.GetTokenType() != expected.GetTokenType()
				|| actual.GetTypeInfo() != expected.GetTypeInfo()
				|| actual.IsReference() != expected.IsReference()
				|| actual.IsObjectHandle() != expected.IsObjectHandle() )
			{
				return 0;
			}
			const asUINT flagIndex = start + paramIndex;
			const asETypeModifiers expectedInOut = InOutFromQuals(param->type.quals);
			if( factory->inOutFlags.GetLength() > flagIndex
				&& expectedInOut != asTM_NONE
				&& factory->inOutFlags[flagIndex] != expectedInOut )
			{
				return 0;
			}
			++paramIndex;
		}
		return start + paramIndex == factory->parameterTypes.GetLength() ? factory : 0;
	}

	asASTDeclId FindInternedZeroArgConstructor(const asCASTContext& context, const asCQualType& type)
	{
		const asCType* named = context.GetType(type.type);
		if( named == 0 )
		{
			return asASTDeclId();
		}
		for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
		{
			const asCDecl* decl = context.GetDecl(asASTDeclId(i));
			if( decl == 0 || decl->kind != asAST_DECL_CLASS )
			{
				continue;
			}
			const bool same = (named->stableKey.GetLength() && decl->stableKey.Equals(named->stableKey))
				|| (named->stableKey.GetLength() && decl->name.Equals(named->stableKey))
				|| (decl->name.GetLength() && named->stableKey.Equals(decl->name));
			if( !same )
			{
				continue;
			}
			for( asUINT c = 0; c < decl->children.GetLength(); ++c )
			{
				const asCDecl* child = context.GetDecl(decl->children[c]);
				if( child == 0 || child->kind != asAST_DECL_CONSTRUCTOR )
				{
					continue;
				}
				asUINT params = 0;
				for( asUINT p = 0; p < child->children.GetLength(); ++p )
				{
					const asCDecl* param = context.GetDecl(child->children[p]);
					if( param && param->kind == asAST_DECL_PARAM )
					{
						++params;
					}
				}
				if( params == 0 )
				{
					return child->id;
				}
			}
		}
		return asASTDeclId();
	}

	void AddUniqueCapture(asCArray<asASTDeclId>& captures, asUINT begin, asASTDeclId id)
	{
		if( !id.IsValid() )
		{
			return;
		}
		for( asUINT i = begin; i < captures.GetLength(); ++i )
		{
			if( captures[i] == id )
			{
				return;
			}
		}
		captures.PushLast(id);
	}

	void CollectLambdaCaptures(const asCASTContext& context, const asCDecl* lambda, asCArray<asASTDeclId>& captures)
	{
		if( lambda == 0 || (lambda->traits & asAST_TRAIT_LAMBDA) == 0 )
		{
			return;
		}
		const asUINT begin = captures.GetLength();
		for( asUINT i = 0; i < lambda->captures.GetLength(); ++i )
		{
			AddUniqueCapture(captures, begin, lambda->captures[i]);
		}
	}

	bool IsPrimitiveOrVoid(const asCDataType& dataType)
	{
		return dataType.IsValid() && (dataType.GetTokenType() == ttVoid || dataType.IsPrimitive());
	}

	asETypeModifiers InOutFromQuals(asDWORD quals)
	{
		if( (quals & asAST_QUAL_INOUT) == asAST_QUAL_INOUT )
			return asTM_INOUTREF;
		if( quals & asAST_QUAL_OUT )
			return asTM_OUTREF;
		if( quals & asAST_QUAL_IN )
			return asTM_INREF;
		return asTM_NONE;
	}

	struct asSCodeGenSlot
	{
		asASTDeclId decl;
		int offset;
		bool storesObjectPointer;
	};

	struct asSCodeGenOwnedParameter
	{
		int offset;
		asCObjectType* objectType;
	};

	struct asSExprSlotMemo
	{
		asASTExprId id;
		int slot;
	};

	struct asSIndexPtrMemo
	{
		asASTExprId base;
		asASTExprId index;
		int slot;
	};

	struct asSCodeGenFuncBind
	{
		asASTDeclId decl;
		asCScriptFunction* func;
		asUINT captureBegin;
		asUINT captureCount;
	};

	struct asSCodeGenGlobal
	{
		asASTDeclId decl;
		asCGlobalProperty* prop;
	};

	struct asSCodeGenObject
	{
		int offset;
		asCDataType type;
		bool live;
	};

	asCTypeInfo* FindArtifactType(
		const asSBytecodeCodeGenArtifact* artifact,
		const asCString& name,
		asSNameSpace* nameSpace)
	{
		if( artifact == 0 )
			return 0;
		for( asUINT n = 0; n < artifact->types.GetLength(); ++n )
		{
			asCTypeInfo* const type = artifact->types[n];
			if( type && type->name == name && type->nameSpace == nameSpace )
				return type;
		}
		return 0;
	}

	asCObjectType* FindArtifactObjectType(
		const asSBytecodeCodeGenArtifact* artifact,
		const asCString& name,
		asSNameSpace* nameSpace)
	{
		return CastToObjectType(FindArtifactType(artifact, name, nameSpace));
	}

	asCObjectType* FindCanonicalObjectType(
		asCScriptEngine* engine,
		asCModule* module,
		const asCString& name,
		asSNameSpace* nameSpace,
		const asSBytecodeCodeGenArtifact* artifact = 0)
	{
		if( module )
		{
			for( asUINT n = 0; n < module->classTypes.GetLength(); ++n )
			{
				asCObjectType* ot = module->classTypes[n];
				if( ot && ot->name == name && ot->nameSpace == nameSpace )
				{
					return ot;
				}
			}
		}
		if( asCObjectType* candidate = FindArtifactObjectType(artifact, name, nameSpace) )
		{
			return candidate;
		}
		if( engine == 0 || name.GetLength() == 0 || nameSpace == 0 )
		{
			return 0;
		}
		return CastToObjectType(FindRegisteredTypeInNamespace(engine, name, nameSpace));
	}

	int FindConstructorId(asCObjectType* objType, asUINT argCount)
	{
		if( objType == 0 )
		{
			return 0;
		}
		int systemId = 0;
		for( asUINT i = 0; i < objType->beh.constructors.GetLength(); ++i )
		{
			const int id = objType->beh.constructors[i];
			asCScriptFunction* ctor = objType->engine && id >= 0 && (asUINT)id < objType->engine->scriptFunctions.GetLength()
				? objType->engine->scriptFunctions[id]
				: 0;
			if( ctor == 0 || ctor->parameterTypes.GetLength() != argCount )
			{
				continue;
			}
			if( ctor->funcType == asFUNC_SCRIPT )
			{
				return id;
			}
			if( systemId == 0 )
			{
				systemId = id;
			}
		}
		if( argCount == 0 && objType->beh.construct )
		{
			asCScriptFunction* ctor = objType->engine && objType->beh.construct >= 0
				&& (asUINT)objType->beh.construct < objType->engine->scriptFunctions.GetLength()
				? objType->engine->scriptFunctions[objType->beh.construct]
				: 0;
			if( ctor && ctor->funcType == asFUNC_SCRIPT )
			{
				return objType->beh.construct;
			}
			if( systemId == 0 )
			{
				systemId = objType->beh.construct;
			}
		}
		return systemId;
	}

	int FindFactoryId(asCObjectType* objType, asUINT userArgCount)
	{
		if( objType == 0 )
		{
			return 0;
		}
		if( userArgCount == 0 && objType->beh.factory > 0 )
		{
			return objType->beh.factory;
		}
		int systemId = 0;
		for( asUINT i = 0; i < objType->beh.factories.GetLength(); ++i )
		{
			const int id = objType->beh.factories[i];
			asCScriptFunction* factory = objType->engine && id >= 0 && (asUINT)id < objType->engine->scriptFunctions.GetLength()
				? objType->engine->scriptFunctions[id]
				: 0;
			if( factory == 0 )
			{
				continue;
			}
			asUINT hidden = 0;
			if( factory->hiddenArgumentIndex >= 0 )
			{
				hidden = 1;
			}
			const asUINT factoryUserArgs = factory->parameterTypes.GetLength() > hidden
				? factory->parameterTypes.GetLength() - hidden
				: 0;
			if( factoryUserArgs != userArgCount )
			{
				continue;
			}
			if( factory->funcType == asFUNC_SCRIPT )
			{
				return id;
			}
			if( systemId == 0 )
			{
				systemId = id;
			}
		}
		return systemId;
	}

	// String factories own the opaque constant objects embedded by PGA. Keep
	// each CodeGen acquisition alive until the generated functions have either
	// established their normal AddReferences() ownership or the detached body
	// transaction has failed. The destructor balances both success and rollback
	// paths without teaching the emitter about module publication order.
	class asCCanonicalStringConstantLease
	{
	public:
		explicit asCCanonicalStringConstantLease(asCScriptEngine* inEngine)
			: engine(inEngine)
		{
		}

		~asCCanonicalStringConstantLease()
		{
			if( engine == 0 || engine->stringFactory == 0 )
				return;
			for( asUINT i = constants.GetLength(); i > 0; --i )
				engine->stringFactory->ReleaseStringConstant(constants[i - 1]);
		}

		const void* Acquire(const char* bytes, asUINT length)
		{
			if( engine == 0 || engine->stringFactory == 0
				|| (bytes == 0 && length != 0) )
			{
				return 0;
			}
			const void* const constant = engine->stringFactory->GetStringConstant(
				bytes ? bytes : "", length);
			if( constant )
				constants.PushLast(constant);
			return constant;
		}

	private:
		asCScriptEngine* engine;
		asCArray<const void*> constants;
	};

	struct asSCodeGenLoop
	{
		asASTStmtId stmt;
		int breakLabel;
		int continueLabel;
		asASTStmtId cleanup;
	};

	class asCCanonicalFunctionEmitter
	{
	public:
		asCCanonicalFunctionEmitter(
			const asCASTContext& inContext,
			asCBuilder* inBuilder,
			asCRuntimeTypeBridge* inBridge,
			asSBytecodeCodeGenArtifact& inArtifact,
			asCArray<asSCodeGenFuncBind>& inFuncs,
			asCArray<asSCodeGenGlobal>& inGlobals,
			asCArray<asASTDeclId>& inCaptures,
			asCCanonicalStringConstantLease& inStringConstants,
			int& inError,
			int& inFailLine,
			asCString& inFailureDetail)
			: context(inContext)
			, lifetimeViewValid(
				asCASTBuildLifetimeView(inContext, lifetimeView)
					== asAST_VERIFY_OK)
			, builder(inBuilder)
			, bridge(inBridge)
			, artifact(inArtifact)
			, functionArtifactIndex(0)
			, funcs(inFuncs)
			, globals(inGlobals)
			, captureDecls(inCaptures)
			, stringConstants(inStringConstants)
			, error(inError)
			, failLine(inFailLine)
			, failureDetail(inFailureDetail)
			, bc(inBuilder)
			, func(0)
			, activeDecl(0)
			, nextLocal(1)
			, nextLabel(1)
			, thisOffset(0x7fffffff)
			, returnSlot(0x7fffffff)
			, returnDwords(0)
			, ok(true)
		{
		}

		bool Emit(asCScriptFunction* outFunc, const asCDecl* decl)
		{
			if( !lifetimeViewValid )
			{
				FailAt(__LINE__, asAST_VERIFY_INVALID_LIFETIME_PROTOCOL);
				return false;
			}
			func = outFunc;
			functionArtifactIndex = artifact.GetOrAddRelocationFunction(outFunc);
			activeDecl = decl;
			nextLocal = 1;
			nextLabel = 1;
			// Method `this` uses frame offset 0 (asCCompiler
			// ThisObjectStackOffset); PushThisObject applies the VALUE-vs-REF ABI.
			thisOffset = (func && func->objectType) ? 0 : 0x7fffffff;
			ok = true;
			slots.SetLength(0);
			debugVariableDecls.SetLength(0);
			ownedParameters.SetLength(0);
			pendingTypeRelocations.SetLength(0);
			temporaryOffsets.SetLength(0);
			loops.SetLength(0);
			materializedSlots.SetLength(0);
			opaqueSlots.SetLength(0);
			indexPtrSlots.SetLength(0);
			indexPtrByArgs.SetLength(0);
			bc.ClearAll();

			objects.SetLength(0);
			returnSlot = 0x7fffffff;
			returnDwords = 0;
			// Keep the JIT entry as the function's first instruction. Parameter
			// materialization below may emit COPY bytecode for values whose sealed
			// semantic owner explicitly authorizes exact storage copying.
			bc.InstrPTR(asBC_JitEntry, 0);
			BindParameters(decl);
			if( func && func->returnType.GetTokenType() != ttVoid )
			{
				if( func->returnType.IsReference() && !func->returnType.IsObjectHandle() )
				{
					returnDwords = AS_PTR_SIZE;
				}
				else
				{
					returnDwords = SizeDwords(func->returnType);
				}
				if( returnDwords < 1 )
				{
					returnDwords = 1;
				}
				returnSlot = func->returnType.IsObjectHandle()
					|| func->returnType.IsFuncdef()
					? AllocPointerVariable()
					: AllocTemporary(func->returnType, returnDwords);
			}

			if( decl->kind == asAST_DECL_CONSTRUCTOR )
			{
				for( asUINT initIndex = 0; initIndex < decl->inits.GetLength(); ++initIndex )
				{
					EmitConstructorInit(decl->inits[initIndex]);
					if( !ok )
					{
						return false;
					}
				}
			}
			if( decl->body.IsValid() )
			{
				EmitStmt(decl->body);
			}
			else if( decl->kind == asAST_DECL_METHOD
				&& (decl->traits & asAST_TRAIT_GENERATED) != 0 )
			{
				EmitGeneratedAccessor(decl);
			}
			if( decl->kind == asAST_DECL_DESTRUCTOR )
			{
				EmitDestructorCleanup(decl);
			}

			if( !ok )
				return false;

			bc.Label(0);
			DestroyLiveObjects();
			DestroyOwnedParameters();
			if( returnSlot != 0x7fffffff )
			{
				if( func->DoesReturnOnStack() )
				{
					// Value objects are returned through the caller-owned hidden pointer,
					// not the eight-byte value register. For methods it follows `this`.
					const int returnPointerOffset = func->objectType ? -AS_PTR_SIZE : 0;
					EmitSealedStorageCopy(returnSlot, returnPointerOffset, func->returnType, true);
				}
				else if( func->returnType.IsObjectHandle() || func->returnType.IsFuncdef() )
				{
					// Reference objects and funcdefs use the VM object register. LOADOBJ
					// transfers ownership from the return slot and clears the source,
					// matching the caller-side STOREOBJ protocol and the legacy compiler.
					bc.InstrSHORT(asBC_LOADOBJ, short(returnSlot));
				}
				else
				{
					LoadReturn(returnSlot, returnDwords);
				}
			}
			if( !ok )
				return false;
			int retPop = func->GetSpaceNeededForArguments();
			if( func->objectType )
			{
				retPop += AS_PTR_SIZE;
			}
			if( func->DoesReturnOnStack() )
			{
				retPop += AS_PTR_SIZE;
			}
			bc.Ret(retPop);

			// Match the maintained compiler's two-stage bytecode pipeline. Finalize
			// performs whole-function optimization, but standard expression peepholes
			// such as LDV + INCi -> IncVi live in OptimizeLocally and remain governed
			// by asEP_OPTIMIZE_BYTECODE.  Canonical scalar temporaries now publish the
			// same offset/type directory as the maintained compiler, so BytecodeJIT and
			// RuntimeJIT can interpret RDR/convert/arithmetic operands without guessing.
			bc.OptimizeLocally(temporaryOffsets);
			bc.Finalize(temporaryOffsets);
			bc.ExtractObjectVariableInfo(func);
			asUINT bytecodeOffset = 0;
			for( asCByteInstruction* instruction = bc.GetFirstInstr();
				instruction; instruction = instruction->next )
			{
				if( instruction->relocationIndex >= 0 )
				{
					const asUINT relocationIndex =
						static_cast<asUINT>(instruction->relocationIndex);
					if( relocationIndex >= pendingTypeRelocations.GetLength() )
					{
						FailAt(__LINE__, asINVALID_CONFIGURATION);
						return false;
					}
					asSTypeRelocation relocation =
						pendingTypeRelocations[relocationIndex];
					relocation.functionArtifactIndex = functionArtifactIndex;
					relocation.bytecodeDwordOffset = bytecodeOffset;
					artifact.typeRelocations.PushLast(relocation);
				}
				if( instruction->GetSize() > 0 )
					bytecodeOffset += static_cast<asUINT>(instruction->GetSize());
			}

			const int varSize = nextLocal > 1 ? nextLocal - 1 : 0;
			func->scriptData->variableSpace = varSize;
			func->scriptData->byteCode.SetLength(bc.GetSize());
			if( bc.GetSize() == 0 )
			{
				FailAt(__LINE__, asERROR);
				return false;
			}
			bc.Output(func->scriptData->byteCode.AddressOf());
			if( !CaptureFinalBytecodeRelocations() )
			{
				return false;
			}
			func->scriptData->stackNeeded = bc.largestStackUsed + varSize;
			func->scriptData->lineNumbers = bc.lineNumbers;
			// CleanStackFrame interprets the first objVariablesOnHeap entries as
			// pointer-owning objects (handles/ref objects/funcdefs) and releases
			// them directly on exception. Inline value objects must follow those
			// entries because their liveness is instead derived from ObjInfo.
			// Preserve that VM table contract even though Canonical CodeGen keeps a
			// single allocation-order list while emitting the normal epilogue.
			for( asUINT i = 0; i < objects.GetLength(); ++i )
			{
				asCTypeInfo* info = objects[i].type.GetTypeInfo();
				if( info == 0 || info == &bc.GetEngine()->functionBehaviours )
				{
					continue;
				}
				const bool isPointerObject = objects[i].type.IsObjectHandle()
					|| objects[i].type.IsFuncdef()
					|| (info->GetFlags() & asOBJ_REF) != 0;
				if( !isPointerObject )
				{
					continue;
				}
				func->scriptData->objVariableTypes.PushLast(info);
				func->scriptData->objVariablePos.PushLast(objects[i].offset);
			}
			func->scriptData->objVariablesOnHeap =
				func->scriptData->objVariablePos.GetLength();
			for( asUINT i = 0; i < objects.GetLength(); ++i )
			{
				asCTypeInfo* info = objects[i].type.GetTypeInfo();
				if( info == 0 || info == &bc.GetEngine()->functionBehaviours )
				{
					continue;
				}
				const bool isPointerObject = objects[i].type.IsObjectHandle()
					|| objects[i].type.IsFuncdef()
					|| (info->GetFlags() & asOBJ_REF) != 0;
				if( isPointerObject )
				{
					continue;
				}
				func->scriptData->objVariableTypes.PushLast(info);
				func->scriptData->objVariablePos.PushLast(objects[i].offset);
			}
			return ok;
		}

	private:
		bool MarkLastTypeRelocation(
			const asCDataType& dataType,
			asERuntimeTypeRelocationUse use,
			const asCObjectProperty* property = 0)
		{
			asSTypeRelocation relocation;
			asCString detail;
			const int result = asCRuntimeTypeBindingTable::CaptureExpectedABI(
				bc.GetEngine(), context, dataType, bindingEnvironment,
				relocation.expectedType, &artifact.types, &detail);
			if( result != asRUNTIME_TYPE_BIND_OK )
			{
				if( failureDetail.GetLength() == 0 )
				{
					failureDetail.Format(
						"failed to capture canonical type relocation: result=%d detail=%s",
						result,
						detail.AddressOf() ? detail.AddressOf() : "");
				}
				FailAt(__LINE__, result);
				return false;
			}
			relocation.use = use;
			if( property )
			{
				relocation.stableMemberKey = relocation.expectedType.stableTypeKey;
				relocation.stableMemberKey += "::";
				relocation.stableMemberKey += property->name;
			}
			pendingTypeRelocations.PushLast(relocation);
			bc.SetLastInstructionRelocationIndex(
				static_cast<int>(pendingTypeRelocations.GetLength() - 1));
			return true;
		}

		bool MarkLastListPatternRelocation(
			const asCDataType& ownerType,
			asCScriptFunction* listFactory)
		{
			if( listFactory == 0 )
			{
				FailAt(__LINE__, asINVALID_DECLARATION);
				return false;
			}
			asSTypeRelocation relocation;
			asCString detail;
			const int result = asCRuntimeTypeBindingTable::CaptureExpectedABI(
				bc.GetEngine(), context, ownerType, bindingEnvironment,
				relocation.expectedType, &artifact.types, &detail);
			if( result != asRUNTIME_TYPE_BIND_OK )
			{
				if( failureDetail.GetLength() == 0 )
				{
					failureDetail.Format(
						"failed to capture list-pattern relocation: result=%d detail=%s",
						result, detail.AddressOf() ? detail.AddressOf() : "");
				}
				FailAt(__LINE__, result);
				return false;
			}
			relocation.stableMemberKey =
				listFactory->canonicalASTStableDeclKey;
			if( relocation.stableMemberKey.GetLength() == 0 )
			{
				const char* const declaration =
					listFactory->GetDeclaration(true, true, false);
				if( declaration )
					relocation.stableMemberKey = declaration;
			}
			if( relocation.stableMemberKey.GetLength() == 0 )
			{
				FailAt(__LINE__, asINVALID_DECLARATION);
				return false;
			}
			relocation.use = asTYPE_RELOC_LIST_PATTERN_TARGET;
			relocation.targetFunctionArtifactIndex =
				artifact.GetOrAddRelocationFunction(listFactory);
			pendingTypeRelocations.PushLast(relocation);
			bc.SetLastInstructionRelocationIndex(
				static_cast<int>(pendingTypeRelocations.GetLength() - 1));
			return true;
		}

		bool AppendFinalRelocation(
			const asCDataType& dataType,
			asERuntimeTypeRelocationUse use,
			asUINT bytecodeOffset,
			const char* stableMemberKey = 0,
			asUINT targetFunctionArtifactIndex = asUINT(-1))
		{
			asSTypeRelocation relocation;
			asCString detail;
			const int result = asCRuntimeTypeBindingTable::CaptureExpectedABI(
				bc.GetEngine(), context, dataType, bindingEnvironment,
				relocation.expectedType, &artifact.types, &detail);
			if( result != asRUNTIME_TYPE_BIND_OK )
			{
				if( failureDetail.GetLength() == 0 )
				{
					failureDetail.Format(
						"failed to capture final bytecode relocation: result=%d detail=%s",
						result,
						detail.AddressOf() ? detail.AddressOf() : "");
				}
				FailAt(__LINE__, result);
				return false;
			}
			relocation.use = use;
			relocation.functionArtifactIndex = functionArtifactIndex;
			relocation.bytecodeDwordOffset = bytecodeOffset;
			relocation.targetFunctionArtifactIndex = targetFunctionArtifactIndex;
			if( stableMemberKey )
				relocation.stableMemberKey = stableMemberKey;
			artifact.typeRelocations.PushLast(relocation);
			return true;
		}

		asCScriptFunction* FindFunctionById(int functionId) const
		{
			for( asUINT i = 0; i < funcs.GetLength(); ++i )
			{
				if( funcs[i].func && funcs[i].func->id == functionId )
					return funcs[i].func;
			}
			asCScriptEngine* const engine = bc.GetEngine();
			return engine && functionId >= 0
				&& static_cast<asUINT>(functionId) < engine->scriptFunctions.GetLength()
				? engine->scriptFunctions[functionId] : 0;
		}

		bool AppendFinalFunctionRelocation(
			asCScriptFunction* target,
			asUINT bytecodeOffset,
			asCTypeInfo* allocationType = 0)
		{
			if( target == 0 )
			{
				FailAt(__LINE__, asINVALID_DECLARATION);
				return false;
			}
			asCDataType authorityType;
			if( allocationType )
				authorityType = asCDataType::CreateType(allocationType, false);
			else if( target->objectType )
				authorityType = asCDataType::CreateType(target->objectType, false);
			else
				authorityType = target->returnType;
			if( !authorityType.IsValid() )
				authorityType = asCDataType::CreatePrimitive(ttVoid, false);

			asCString stableFunctionKey = target->canonicalASTStableDeclKey;
			if( stableFunctionKey.GetLength() == 0 )
			{
				const char* const declaration =
					target->GetDeclaration(true, true, false);
				if( declaration )
					stableFunctionKey = declaration;
			}
			if( stableFunctionKey.GetLength() == 0 )
			{
				FailAt(__LINE__, asINVALID_DECLARATION);
				return false;
			}
			// Make every detached target visible to the validate-all binding pass.
			// This is non-owning and does not publish the function to the Engine.
			const asUINT targetFunctionArtifactIndex =
				artifact.GetOrAddRelocationFunction(target);
			return AppendFinalRelocation(
				authorityType,
				asTYPE_RELOC_FUNCTION,
				bytecodeOffset,
				stableFunctionKey.AddressOf(),
				targetFunctionArtifactIndex);
		}

		bool CaptureFinalBytecodeRelocations()
		{
			if( func == 0 || func->scriptData == 0 )
			{
				FailAt(__LINE__, asINVALID_CONFIGURATION);
				return false;
			}
			asDWORD* const bytes = func->scriptData->byteCode.AddressOf();
			const asUINT length = func->scriptData->byteCode.GetLength();
			for( asUINT offset = 0; offset < length; )
			{
				asDWORD* const instruction = bytes + offset;
				const asEBCInstr opcode = static_cast<asEBCInstr>(
					*reinterpret_cast<const asBYTE*>(instruction));
				const asUINT size = static_cast<asUINT>(
					asBCTypeSize[asBCInfo[opcode].type]);
				if( size == 0 || offset + size > length )
				{
					FailAt(__LINE__, asINVALID_CONFIGURATION);
					return false;
				}

				if( opcode == asBC_CALLSYS || opcode == asBC_Thiscall1
					|| opcode == asBC_FuncPtr )
				{
					asCScriptFunction* const target =
						reinterpret_cast<asCScriptFunction*>(asBC_PTRARG(instruction));
					if( !AppendFinalFunctionRelocation(target, offset) )
						return false;
				}
				else if( opcode == asBC_CALL || opcode == asBC_CALLBND
					|| opcode == asBC_CALLINTF )
				{
					asCScriptFunction* const target =
						FindFunctionById(asBC_INTARG(instruction));
					if( !AppendFinalFunctionRelocation(target, offset) )
						return false;
				}
				else if( opcode == asBC_ALLOC )
				{
					asCTypeInfo* const type =
						reinterpret_cast<asCTypeInfo*>(asBC_PTRARG(instruction));
					const int constructorId = asBC_INTARG(
						instruction + AS_PTR_SIZE);
					if( constructorId > 0 )
					{
						if( !AppendFinalFunctionRelocation(
							FindFunctionById(constructorId), offset, type) )
							return false;
					}
					else if( type == 0 )
					{
						failureDetail.Format(
							"final ALLOC has null Runtime type target function=%s offset=%u constructorId=%d length=%u",
							activeDecl && activeDecl->stableKey.AddressOf()
								? activeDecl->stableKey.AddressOf() : "<unknown>",
							offset, constructorId, length);
						FailAt(__LINE__, asINVALID_CONFIGURATION);
						return false;
					}
					else if( !AppendFinalRelocation(
						asCDataType::CreateType(type, false),
						asTYPE_RELOC_RUNTIME_TYPE_TARGET,
						offset) )
					{
						return false;
					}
				}
				else if( opcode == asBC_FREE || opcode == asBC_REFCPY )
				{
					bool alreadyRelocated = false;
					for( asUINT relocationIndex = 0;
						relocationIndex < artifact.typeRelocations.GetLength();
						++relocationIndex )
					{
						const asSTypeRelocation& existing =
							artifact.typeRelocations[relocationIndex];
						if( existing.functionArtifactIndex == functionArtifactIndex
							&& existing.bytecodeDwordOffset == offset )
						{
							alreadyRelocated = true;
							break;
						}
					}
					if( alreadyRelocated )
					{
						offset += size;
						continue;
					}
					asCTypeInfo* const type =
						reinterpret_cast<asCTypeInfo*>(asBC_PTRARG(instruction));
					// Funcdef reference accounting uses one fixed Engine behaviour
					// sentinel, not a generation-owned type target.
					if( type == 0 )
					{
						failureDetail.Format(
							"final %s has null Runtime type target function=%s offset=%u slot=%d length=%u word1=%u word2=%u",
							opcode == asBC_FREE ? "FREE" : "REFCPY",
							activeDecl && activeDecl->stableKey.AddressOf()
								? activeDecl->stableKey.AddressOf() : "<unknown>",
							offset,
							static_cast<int>(asBC_SWORDARG0(instruction)),
							length,
							offset + 1 < length ? instruction[1] : 0,
							offset + 2 < length ? instruction[2] : 0);
						FailAt(__LINE__, asINVALID_CONFIGURATION);
						return false;
					}
					if( type != &bc.GetEngine()->functionBehaviours
						&& !AppendFinalRelocation(
							asCDataType::CreateType(type, false),
							asTYPE_RELOC_RUNTIME_TYPE_TARGET,
							offset) )
					{
						return false;
					}
				}

				offset += size;
			}
			return true;
		}

		void FailAt(int line, int code)
		{
			ok = false;
			if( error == asAST_VERIFY_OK )
			{
				error = code;
				failLine = line;
				if( failureDetail.GetLength() == 0 )
				{
					failureDetail.Format("function=%s emitterLine=%d error=%d",
						activeDecl && activeDecl->stableKey.AddressOf() ? activeDecl->stableKey.AddressOf() : "<unknown>",
						line,
						code);
				}
			}
		}

		bool PublishDebugVariable(
			const asCDecl* declaration,
			const asCDataType& dataType,
			int stackOffset)
		{
			if( declaration == 0 || !declaration->id.IsValid()
				|| declaration->name.GetLength() == 0
				|| (declaration->traits & asAST_TRAIT_GENERATED) != 0 )
			{
				return true;
			}
			for( asUINT i = 0; i < debugVariableDecls.GetLength(); ++i )
			{
				if( debugVariableDecls[i] == declaration->id )
					return true;
			}
			if( func == 0 || func->scriptData == 0 || !dataType.IsValid() )
			{
				FailAt(__LINE__, asINVALID_CONFIGURATION);
				return false;
			}
			const asUINT variableIndex = func->scriptData->variables.GetLength();
			if( variableIndex > 0xFFFFu )
			{
				FailAt(__LINE__, asOUT_OF_MEMORY);
				return false;
			}
			// Keep the public debugger table tied to source DeclIds while preserving
			// the Runtime frame ABI. Parameters use their Runtime-normalized type and
			// incoming slot; locals use the sealed source type and Canonical slot.
			// The complete ScriptFunctionData is detached during prepared emission,
			// so this record and its VarDecl marker commit or roll back with the body.
			func->AddVariable(declaration->name, dataType, stackOffset, false);
			if( func->scriptData->variables.GetLength() != variableIndex + 1 )
			{
				FailAt(__LINE__, asOUT_OF_MEMORY);
				return false;
			}
			bc.VarDecl(int(variableIndex));
			debugVariableDecls.PushLast(declaration->id);
			return true;
		}

		void FailUnsupportedConversion(const asCExpr* expr, const asCDataType& srcType, const asCDataType& destType, int line)
		{
			if( error == asAST_VERIFY_OK && failureDetail.GetLength() == 0 )
			{
				const asCString srcName = srcType.Format(0);
				const asCString destName = destType.Format(0);
				failureDetail.Format(
					"unsupported conversion function=%s expr=%u literal=%s srcType=%s srcToken=%d srcBytes=%d dstType=%s dstToken=%d dstBytes=%d",
					activeDecl && activeDecl->stableKey.AddressOf() ? activeDecl->stableKey.AddressOf() : "<unknown>",
					expr ? expr->id.value : 0,
					expr && expr->literal.AddressOf() ? expr->literal.AddressOf() : "",
					srcName.AddressOf(),
					static_cast<int>(srcType.GetTokenType()),
					srcType.GetSizeInMemoryBytes(),
					destName.AddressOf(),
					static_cast<int>(destType.GetTokenType()),
					destType.GetSizeInMemoryBytes());
			}
			FailAt(line, asNOT_SUPPORTED);
		}

		int AllocDwords(int dwords)
		{
			if( dwords < 1 )
				dwords = 1;
			int offset = nextLocal;
			if( dwords > 1 )
				offset += dwords - 1;
			nextLocal += dwords;
			return offset;
		}

		void PublishPointerVariable(int offset)
		{
			if( func == 0 || func->scriptData == 0 || offset <= 0 )
			{
				FailAt(__LINE__, asINVALID_CONFIGURATION);
				return;
			}
			if( !func->scriptData->pointerVariablePos.Exists(offset) )
			{
				func->scriptData->pointerVariablePos.PushLast(offset);
			}
		}

		int AllocPointerVariable()
		{
			const int offset = AllocDwords(AS_PTR_SIZE);
			PublishPointerVariable(offset);
			return offset;
		}

		int AllocTemporary(const asCDataType& inputType, int dwords = 0)
		{
			asCDataType valueType = inputType;
			if( valueType.IsReference() && !valueType.IsObjectHandle() )
			{
				valueType.MakeReference(false);
			}
			const int offset = AllocDwords(
				dwords > 0 ? dwords : LocalDwords(valueType));
			if( !valueType.IsValid() || valueType.GetTokenType() == ttVoid
				|| valueType.IsObject() || valueType.IsObjectHandle()
				|| valueType.IsFuncdef() )
			{
				return offset;
			}

			asCScriptFunction::ScriptFunctionData::ScriptTempVariable temporary;
			temporary.Offset = offset;
			if( valueType.IsEnumType() )
			{
				switch( valueType.GetSizeInMemoryBytes() )
				{
				case 1: temporary.Token = ttUInt8; break;
				case 2: temporary.Token = ttUInt16; break;
				case 4: temporary.Token = ttUInt; break;
				case 8: temporary.Token = ttUInt64; break;
				default: return offset;
				}
			}
			else
			{
				temporary.Token = valueType.GetTokenType();
			}
			func->scriptData->temporaryVariables.PushLast(temporary);
			temporaryOffsets.PushLast(offset);
			return offset;
		}

		int SizeDwords(const asCDataType& dataType)
		{
			if( dataType.GetTokenType() == ttVoid )
				return 0;
			const int size = dataType.GetSizeOnStackDWords();
			return size > 0 ? size : 1;
		}

		int ValueDwords(const asCDataType& dataType)
		{
			if( dataType.IsNullHandle()
				|| dataType.IsObjectHandle()
				|| dataType.IsFuncdef() )
			{
				return AS_PTR_SIZE;
			}
			if( dataType.IsObject() && dataType.GetTypeInfo()
				&& (dataType.GetTypeInfo()->GetFlags() & asOBJ_REF)
				&& (dataType.GetTypeInfo()->GetFlags() & asOBJ_VALUE) == 0 )
			{
				return AS_PTR_SIZE;
			}
			const int dwords = dataType.GetSizeInMemoryDWords();
			return dwords > 0 ? dwords : 1;
		}

		int MemoryBytes(const asCDataType& dataType) const
		{
			if( dataType.IsObjectHandle() || dataType.IsFuncdef() )
			{
				return AS_PTR_SIZE * 4;
			}
			if( asCTypeInfo* info = dataType.GetTypeInfo() )
			{
				const asDWORD flags = info->GetFlags();
				if( ((flags & asOBJ_REF) && (flags & asOBJ_VALUE) == 0) || info->size <= 0 )
				{
					return AS_PTR_SIZE * 4;
				}
			}
			const int bytes = dataType.GetSizeInMemoryBytes();
			if( bytes == 1 || bytes == 2 || bytes == 4 || bytes == 8 )
			{
				return bytes;
			}
			return 0;
		}

		void EmitReadValue(
			int dest,
			const asCDataType& dataType,
			const asCExpr* sourceExpression = 0,
			const char* readRole = 0)
		{
			const int bytes = MemoryBytes(dataType);
			if( bytes == 1 )
			{
				bc.InstrSHORT(asBC_RDR1, (short)dest);
			}
			else if( bytes == 2 )
			{
				bc.InstrSHORT(asBC_RDR2, (short)dest);
			}
			else if( bytes == 4 )
			{
				bc.InstrSHORT(asBC_RDR4, (short)dest);
			}
			else if( bytes == 8 )
			{
				bc.InstrSHORT(asBC_RDR8, (short)dest);
			}
			else
			{
				const asCString typeName = dataType.Format(0);
				asCTypeInfo* const typeInfo = dataType.GetTypeInfo();
				const asCExpr* receiverParent = 0;
				const asCExpr* childParent = 0;
				asCString allParents;
				if( sourceExpression != 0 )
				{
					for( asUINT i = 1; i <= context.GetExprCount(); ++i )
					{
						const asCExpr* const candidate =
							context.GetExpr(asASTExprId(i));
						if( candidate == 0 )
						{
							continue;
						}
						const bool isReceiverParent =
							candidate->receiver == sourceExpression->id;
						if( receiverParent == 0 && isReceiverParent )
						{
							receiverParent = candidate;
						}
						bool isChildParent = false;
						for( asUINT c = 0; c < candidate->children.GetLength(); ++c )
						{
							if( candidate->children[c] == sourceExpression->id )
							{
								isChildParent = true;
								break;
							}
						}
						if( childParent == 0 && isChildParent )
						{
							childParent = candidate;
						}
						if( (isReceiverParent || isChildParent)
							&& allParents.GetLength() < 512 )
						{
							asCString edge;
							edge.Format("%s%u:%d:%s%s",
								allParents.GetLength() ? "," : "",
								candidate->id.value,
								static_cast<int>(candidate->kind),
								isReceiverParent ? "R" : "",
								isChildParent ? "C" : "");
							allParents += edge;
						}
					}
				}
				const asCExpr* const callParent = receiverParent != 0
					&& receiverParent->kind == asAST_EXPR_CALL
					? receiverParent
					: (childParent != 0 && childParent->kind == asAST_EXPR_CALL
						? childParent : 0);
				const asCDecl* const sealedCallee = callParent != 0
					? context.GetDecl(callParent->resolvedDecl) : 0;
				asCScriptFunction* runtimeCallee = 0;
				if( callParent != 0 )
				{
					for( asUINT funcIndex = 0; funcIndex < funcs.GetLength(); ++funcIndex )
					{
						if( funcs[funcIndex].decl == callParent->resolvedDecl )
						{
							runtimeCallee = funcs[funcIndex].func;
							break;
						}
					}
				}
				const asCString runtimeDeclaration = runtimeCallee != 0
					? runtimeCallee->GetDeclaration(false, false, true)
					: asCString("<missing>");
				const asCString runtimeObjectType = runtimeCallee != 0
					&& runtimeCallee->objectType != 0
					? runtimeCallee->objectType->GetName() : asCString("<none>");
				failureDetail.Format(
					"unsupported read width function=%s type=%s token=%d bytes=%d "
					"typeSize=%d typeFlags=0x%x object=%d handle=%d reference=%d "
					"role=%s expr=%u kind=%d resolvedDecl=%u receiver=%u children=%u "
					"receiverParent=%u receiverParentKind=%d receiverParentChildren=%u "
					"childParent=%u childParentKind=%d childParentChildren=%u "
					"callTarget=%u callTargetKind=%d callTargetName=%s callTargetKey=%s "
					"runtimeCallee=%s runtimeObjectType=%s runtimeParams=%u runtimeHidden=%d "
					"allParents=%s",
					activeDecl && activeDecl->stableKey.AddressOf()
						? activeDecl->stableKey.AddressOf() : "<unknown>",
					typeName.AddressOf() ? typeName.AddressOf() : "<invalid>",
					static_cast<int>(dataType.GetTokenType()),
					dataType.GetSizeInMemoryBytes(),
					typeInfo ? typeInfo->size : -1,
					typeInfo ? typeInfo->GetFlags() : 0,
					dataType.IsObject() ? 1 : 0,
					dataType.IsObjectHandle() ? 1 : 0,
					dataType.IsReference() ? 1 : 0,
					readRole ? readRole : "unspecified",
					sourceExpression ? sourceExpression->id.value : 0,
					sourceExpression
						? static_cast<int>(sourceExpression->kind) : -1,
					sourceExpression ? sourceExpression->resolvedDecl.value : 0,
					sourceExpression ? sourceExpression->receiver.value : 0,
					sourceExpression ? sourceExpression->children.GetLength() : 0,
					receiverParent ? receiverParent->id.value : 0,
					receiverParent ? static_cast<int>(receiverParent->kind) : -1,
					receiverParent ? receiverParent->children.GetLength() : 0,
					childParent ? childParent->id.value : 0,
					childParent ? static_cast<int>(childParent->kind) : -1,
					childParent ? childParent->children.GetLength() : 0,
					callParent ? callParent->resolvedDecl.value : 0,
					sealedCallee ? static_cast<int>(sealedCallee->kind) : -1,
					sealedCallee && sealedCallee->name.AddressOf()
						? sealedCallee->name.AddressOf() : "<missing>",
					sealedCallee && sealedCallee->stableKey.AddressOf()
						? sealedCallee->stableKey.AddressOf() : "<missing>",
					runtimeDeclaration.AddressOf()
						? runtimeDeclaration.AddressOf() : "<missing>",
					runtimeObjectType.AddressOf()
						? runtimeObjectType.AddressOf() : "<none>",
					runtimeCallee ? runtimeCallee->parameterTypes.GetLength() : 0,
					runtimeCallee ? runtimeCallee->hiddenArgumentIndex : -1,
					allParents.AddressOf() ? allParents.AddressOf() : "<none>");
				FailAt(__LINE__, asNOT_SUPPORTED);
			}
		}

		void EmitWriteValue(int src, const asCDataType& dataType)
		{
			const int bytes = MemoryBytes(dataType);
			if( bytes == 1 )
			{
				bc.InstrSHORT(asBC_WRTV1, (short)src);
			}
			else if( bytes == 2 )
			{
				bc.InstrSHORT(asBC_WRTV2, (short)src);
			}
			else if( bytes == 4 )
			{
				bc.InstrSHORT(asBC_WRTV4, (short)src);
			}
			else if( bytes == 8 )
			{
				bc.InstrSHORT(asBC_WRTV8, (short)src);
			}
			else
			{
				const asCString typeName = dataType.Format(0);
				asCTypeInfo* const typeInfo = dataType.GetTypeInfo();
				failureDetail.Format(
					"unsupported write width function=%s type=%s token=%d bytes=%d "
					"typeSize=%d typeFlags=0x%x object=%d handle=%d reference=%d",
					activeDecl && activeDecl->stableKey.AddressOf()
						? activeDecl->stableKey.AddressOf() : "<unknown>",
					typeName.AddressOf() ? typeName.AddressOf() : "<invalid>",
					static_cast<int>(dataType.GetTokenType()),
					dataType.GetSizeInMemoryBytes(),
					typeInfo ? typeInfo->size : -1,
					typeInfo ? typeInfo->GetFlags() : 0,
					dataType.IsObject() ? 1 : 0,
					dataType.IsObjectHandle() ? 1 : 0,
					dataType.IsReference() ? 1 : 0);
				FailAt(__LINE__, asNOT_SUPPORTED);
			}
		}

		bool ListPatternElementType(asCScriptFunction* factory, asCDataType& outType)
		{
			if( factory == 0 || factory->listPattern == 0 )
			{
				return false;
			}
			for( asSListPatternNode* node = factory->listPattern; node; node = node->next )
			{
				if( node->type == asLPT_TYPE )
				{
					outType = static_cast<asSListPatternDataTypeNode*>(node)->dataType;
					return true;
				}
			}
			return false;
		}

		int ListElementBytes(const asCDataType& dataType)
		{
			if( dataType.IsPrimitive()
				|| (!dataType.IsNullHandle() && dataType.GetTypeInfo()
					&& (dataType.GetTypeInfo()->GetFlags() & asOBJ_VALUE)
					&& !dataType.IsObjectHandle()) )
			{
				const int bytes = dataType.GetSizeInMemoryBytes();
				return bytes > 0 ? bytes : 4;
			}
			return AS_PTR_SIZE * 4;
		}

		asCDataType TypeOf(const asCQualType& qual)
		{
			asCDataType dataType = bridge->Resolve(context, qual);
			if( !dataType.IsValid() )
			{
				if( error == asAST_VERIFY_OK && failureDetail.GetLength() == 0 )
				{
					const asCType* const canonicalType = context.GetType(qual.type);
					failureDetail.Format(
						"unresolved canonical type function=%s typeRef=%u kind=%d key=%s quals=%u",
						activeDecl && activeDecl->stableKey.AddressOf()
							? activeDecl->stableKey.AddressOf()
							: "<unknown>",
						qual.type.value,
						canonicalType ? static_cast<int>(canonicalType->kind) : -1,
						canonicalType && canonicalType->stableKey.AddressOf()
							? canonicalType->stableKey.AddressOf()
							: "<null>",
						static_cast<unsigned int>(qual.quals));
					for( asUINT exprIndex = 1; exprIndex <= context.GetExprCount(); ++exprIndex )
					{
						const asCExpr* const user = context.GetExpr(asASTExprId(exprIndex));
						if( user == 0 || user->type.type != qual.type )
						{
							continue;
						}
						asCString userDetail;
						userDetail.Format(
							" exprUser=%u/%d/%s/callee:%u",
							user->id.value,
							static_cast<int>(user->kind),
							user->literal.AddressOf() ? user->literal.AddressOf() : "",
							user->resolvedDecl.value);
						failureDetail += userDetail;
					}
				}
				FailAt(__LINE__, asINVALID_TYPE);
				return asCDataType::CreatePrimitive(ttInt, false);
			}
			// Cache V2 records the runtime type object actually selected by the
			// authoritative lowering pass. This covers signature, local, conversion,
			// and expression types without reconstructing dependencies from source
			// spelling after Sema has already resolved aliases and template instances.
			if( builder && dataType.GetTypeInfo() )
			{
				builder->MarkDependency(dataType.GetTypeInfo(), 0, 0);
			}
			return dataType;
		}

		int FindExprSlot(const asCArray<asSExprSlotMemo>& memos, asASTExprId id) const
		{
			if( !id.IsValid() )
			{
				return 0x7fffffff;
			}
			for( asUINT i = 0; i < memos.GetLength(); ++i )
			{
				if( memos[i].id == id )
				{
					return memos[i].slot;
				}
			}
			return 0x7fffffff;
		}

		void BindExprSlot(asCArray<asSExprSlotMemo>& memos, asASTExprId id, int slot)
		{
			if( !id.IsValid() )
			{
				return;
			}
			asSExprSlotMemo memo;
			memo.id = id;
			memo.slot = slot;
			memos.PushLast(memo);
		}

		asCObjectType* ObjectTypeFromExpr(const asCExpr* expr)
		{
			if( expr == 0 )
			{
				return 0;
			}
			if( expr->kind == asAST_EXPR_SEQUENCE && expr->children.GetLength() )
			{
				return ObjectTypeFromExpr(context.GetExpr(expr->children[expr->children.GetLength() - 1]));
			}
			if( (expr->kind == asAST_EXPR_MATERIALIZE_TEMPORARY
				|| expr->kind == asAST_EXPR_CLEANUP
				|| expr->kind == asAST_EXPR_OPAQUE_VALUE)
				&& expr->children.GetLength() )
			{
				return ObjectTypeFromExpr(context.GetExpr(expr->children[0]));
			}
			asCDataType dataType = bridge->Resolve(context, expr->type);
			if( asCObjectType* objType = CastToObjectType(dataType.GetTypeInfo()) )
			{
				return objType;
			}
			return 0;
		}

		void PushThisObject()
		{
			// Method frames store the receiver pointer at offset zero.  The legacy
			// compiler represents VALUE `this` as a reference variable: take the
			// frame-slot address and dereference it once to obtain the inline object
			// address.  REF methods can push the stored receiver pointer directly.
			// Treating PSF(0) itself as the VALUE object writes into the callee's
			// argument slot instead of the caller-owned object storage.
			if( func && func->objectType
				&& (func->objectType->GetFlags() & asOBJ_VALUE) != 0 )
			{
				bc.InstrSHORT(asBC_PSF, (short)thisOffset);
				bc.Instr(asBC_RDSPtr);
			}
			else
			{
				bc.InstrSHORT(asBC_PshVPtr, (short)thisOffset);
			}
		}

		void PushMemberBase(int base, const asCExpr* baseExpr)
		{
			asCDataType baseType = baseExpr ? TypeOf(baseExpr->type) : asCDataType();
			if( baseType.IsObjectHandle() || baseType.IsFuncdef()
				|| (baseType.GetTypeInfo() && (baseType.GetTypeInfo()->GetFlags() & asOBJ_REF)
					&& (baseType.GetTypeInfo()->GetFlags() & asOBJ_VALUE) == 0) )
			{
				bc.InstrSHORT(asBC_PshVPtr, (short)base);
				return;
			}
			bc.InstrSHORT(asBC_PSF, (short)base);
		}

		asCGlobalProperty* FindDirectGlobalValueObject(const asCExpr* baseExpr)
		{
			if( baseExpr == 0 || baseExpr->kind != asAST_EXPR_DECL_REF )
			{
				return 0;
			}
			const asCDataType baseType = TypeOf(baseExpr->type);
			if( !baseType.IsNonPrimitiveValueType() )
			{
				return 0;
			}
			return FindGlobal(baseExpr->resolvedDecl);
		}

		void PushDirectGlobalValueObject(asCGlobalProperty* prop)
		{
			// Non-primitive global VALUE storage contains an object pointer. PGA
			// pushes the address of that storage slot; dereference it once before
			// applying a member offset. Loading the pointer into a local and using
			// PSF would instead address the local pointer slot itself.
			bc.InstrPTR(asBC_PGA, prop->GetAddressOfValue());
			bc.Instr(asBC_RDSPtr);
		}

		const asCExpr* UnwrapAssignLhs(const asCExpr* lhs) const
		{
			while( lhs )
			{
				if( lhs->kind == asAST_EXPR_SEQUENCE && lhs->children.GetLength() > 0 )
				{
					lhs = context.GetExpr(lhs->children[lhs->children.GetLength() - 1]);
					continue;
				}
				if( (lhs->kind == asAST_EXPR_MATERIALIZE_TEMPORARY || lhs->kind == asAST_EXPR_CLEANUP)
					&& lhs->children.GetLength() > 0 )
				{
					lhs = context.GetExpr(lhs->children[0]);
					continue;
				}
				break;
			}
			return lhs;
		}

		const asCExpr* GeneratedAccessorCopyPlan(const asCDecl* decl) const
		{
			if( decl == 0 || decl->accessorKind != asAST_ACCESSOR_GET
				|| !decl->accessorField.IsValid()
				|| decl->inits.GetLength() != 1 )
			{
				return 0;
			}

			const asCExpr* plan = context.GetExpr(decl->inits[0]);
			while( plan
				&& (plan->kind == asAST_EXPR_CLEANUP
					|| plan->kind == asAST_EXPR_MATERIALIZE_TEMPORARY)
				&& plan->children.GetLength() == 1 )
			{
				plan = context.GetExpr(plan->children[0]);
			}
			if( plan == 0 || plan->kind != asAST_EXPR_CONSTRUCT
				|| !plan->resolvedDecl.IsValid()
				|| plan->children.GetLength() != 1 )
			{
				return 0;
			}
			const asCExpr* const source = context.GetExpr(plan->children[0]);
			const asCDecl* const ctor = context.GetDecl(plan->resolvedDecl);
			const asCDecl* const field = context.GetDecl(decl->accessorField);
			if( source == 0 || source->kind != asAST_EXPR_DECL_REF
				|| source->resolvedDecl != decl->accessorField
				|| ctor == 0 || ctor->kind != asAST_DECL_CONSTRUCTOR
				|| field == 0 || plan->type != field->type )
			{
				return 0;
			}
			return plan;
		}

		const asCExpr* GeneratedAccessorAssignmentPlan(
			const asCDecl* decl,
			const asCDecl* parameter) const
		{
			if( decl == 0 || parameter == 0
				|| decl->inits.GetLength() != 1 )
			{
				return 0;
			}
			const asCExpr* const plan = context.GetExpr(decl->inits[0]);
			const asCDecl* const field = context.GetDecl(decl->accessorField);
			if( plan == 0 || plan->kind != asAST_EXPR_ASSIGN
				|| !plan->resolvedDecl.IsValid()
				|| plan->children.GetLength() != 2 || field == 0 )
			{
				return 0;
			}
			const asCExpr* const lhs = context.GetExpr(plan->children[0]);
			const asCExpr* const rhs = context.GetExpr(plan->children[1]);
			if( lhs == 0 || rhs == 0
				|| lhs->kind != asAST_EXPR_DECL_REF
				|| rhs->kind != asAST_EXPR_DECL_REF
				|| lhs->resolvedDecl != field->id
				|| rhs->resolvedDecl != parameter->id
				|| lhs->type != field->type
				|| rhs->type != parameter->type )
			{
				return 0;
			}
			return plan;
		}

		void EmitGeneratedAccessor(const asCDecl* decl)
		{
			if( func == 0 || func->objectType == 0 || thisOffset == 0x7fffffff || decl == 0 )
			{
				FailAt(__LINE__, asNO_FUNCTION);
				return;
			}
			const bool isGet = decl->accessorKind == asAST_ACCESSOR_GET;
			const bool isSet = decl->accessorKind == asAST_ACCESSOR_SET;
			const asCDecl* field = context.GetDecl(decl->accessorField);
			if( (!isGet && !isSet) || field == 0 || field->kind != asAST_DECL_VAR || field->parent != decl->parent )
			{
				FailAt(__LINE__, asNO_FUNCTION);
				return;
			}
			const int fieldOffset = FindFieldOffset(field->id);
			const asCDataType fieldType = TypeOf(field->type);
			// The generated accessor embeds the selected field offset in ADDSi.
			// TypeOf records the field's type authority, but Cache V2 also needs the
			// exact property-layout authority used by that relocation.
			asCObjectProperty* const runtimeProperty =
				PropertyFromFieldDecl(field, CastToObjectType(func->objectType));
			if( fieldOffset < 0 || runtimeProperty == 0 )
			{
				FailAt(__LINE__, asNO_FUNCTION);
				return;
			}
			const asCDataType ownerDataType =
				asCDataType::CreateType(func->objectType, false);
			const int dwords = ValueDwords(fieldType);
			if( isGet )
			{
				if( func->DoesReturnOnStack()
					&& IsByValueObject(fieldType)
					&& !CanEmitSealedStorageCopy(fieldType) )
				{
					const asCExpr* const copyPlan =
						GeneratedAccessorCopyPlan(decl);
					asCScriptFunction* const copyCtor = copyPlan
						? FindFunc(copyPlan->resolvedDecl) : 0;
					if( copyPlan == 0 || copyCtor == 0
						|| copyCtor->objectType != fieldType.GetTypeInfo() )
					{
						failureDetail.Format(
							"generated non-POD getter lacks its exact sealed copy-construction route: function=%s field=%s",
							decl->stableKey.AddressOf()
								? decl->stableKey.AddressOf() : "<missing>",
							field->stableKey.AddressOf()
								? field->stableKey.AddressOf() : "<missing>");
						FailAt(__LINE__, asNO_FUNCTION);
						return;
					}

					// The one sealed copy-constructor argument is the exact backing
					// field. Leave that field address on the VM stack, then pass the
					// caller-owned hidden return pointer as the constructor receiver.
					// This constructs the detached return object once; it never reads
					// or memcpy-copies the non-POD field storage.
					PushThisObject();
					bc.InstrSHORT_DW(asBC_ADDSi,
						(short)fieldOffset, 0);
					MarkLastTypeRelocation(
						ownerDataType, asTYPE_RELOC_PROPERTY, runtimeProperty);
					const int returnPointerOffset = func->objectType
						? -AS_PTR_SIZE : 0;
					bc.InstrSHORT(asBC_PshVPtr,
						(short)returnPointerOffset);
					bc.Call(copyCtor->funcType == asFUNC_SYSTEM
							? asBC_CALLSYS : asBC_CALL,
						copyCtor->id, AS_PTR_SIZE * 2);
					// The return object is owned and tracked by the caller. Suppress
					// the scalar/storage-copy epilogue for this direct construction.
					returnSlot = 0x7fffffff;
					return;
				}
				PushThisObject();
				bc.InstrSHORT_DW(asBC_ADDSi, (short)fieldOffset, 0);
				MarkLastTypeRelocation(
					ownerDataType, asTYPE_RELOC_PROPERTY, runtimeProperty);
				bc.Instr(asBC_PopRPtr);
				const int dest = AllocTemporary(fieldType, dwords);
				EmitReadValue(dest, fieldType, 0, "generated-accessor");
				// Generated getters have no STMT_RETURN, so CopyVar never ran and
				// the shared epilogue LoadReturn(returnSlot) overwrote the register.
				if( returnSlot == 0x7fffffff )
				{
					FailAt(__LINE__, asNO_FUNCTION);
					return;
				}
				CopyVar(returnSlot, dest, returnDwords);
				return;
			}
			int src = 0x7fffffff;
			const asCDecl* parameter = 0;
			for( asUINT i = 0; i < decl->children.GetLength(); ++i )
			{
				const asCDecl* child = context.GetDecl(decl->children[i]);
				if( child && child->kind == asAST_DECL_PARAM )
				{
					parameter = child;
					src = FindSlot(child->id);
					break;
				}
			}
			if( src == 0x7fffffff )
			{
				FailAt(__LINE__, asNO_FUNCTION);
				return;
			}
			if( IsByValueObject(fieldType)
				&& !CanEmitSealedStorageCopy(fieldType) )
			{
				const asCExpr* const assignment =
					GeneratedAccessorAssignmentPlan(decl, parameter);
				if( assignment == 0 )
				{
					failureDetail.Format(
						"generated non-POD setter lacks its exact sealed assignment route: function=%s field=%s",
						decl->stableKey.AddressOf()
							? decl->stableKey.AddressOf() : "<missing>",
						field->stableKey.AddressOf()
							? field->stableKey.AddressOf() : "<missing>");
					FailAt(__LINE__, asNO_FUNCTION);
					return;
				}
				EmitResolvedAssignmentOperator(
					assignment,
					context.GetExpr(assignment->children[0]),
					context.GetExpr(assignment->children[1]));
				return;
			}
			PushThisObject();
			bc.InstrSHORT_DW(asBC_ADDSi, (short)fieldOffset, 0);
			MarkLastTypeRelocation(
				ownerDataType, asTYPE_RELOC_PROPERTY, runtimeProperty);
			bc.Instr(asBC_PopRPtr);
			EmitWriteValue(src, fieldType);
		}

		void EmitDestructorCleanup(const asCDecl* decl)
		{
			if( decl == 0 || func == 0 || func->objectType == 0
				|| thisOffset == 0x7fffffff )
			{
				FailAt(__LINE__, asNO_FUNCTION);
				return;
			}
			const asCDecl* const ownerClass = context.GetDecl(decl->parent);
			if( ownerClass == 0 || ownerClass->kind != asAST_DECL_CLASS )
			{
				FailAt(__LINE__, asINVALID_DECLARATION);
				return;
			}

			const asCDataType ownerDataType =
				asCDataType::CreateType(func->objectType, false);
			// Match the language lifetime order: local fields are destroyed in
			// reverse declaration order, followed by the base subobject.
			for( asUINT childIndex = ownerClass->children.GetLength();
				childIndex > 0; --childIndex )
			{
				const asCDecl* const field = context.GetDecl(
					ownerClass->children[childIndex - 1]);
				if( field == 0 || field->kind != asAST_DECL_VAR )
				{
					continue;
				}
				asCObjectProperty* const property =
					PropertyFromFieldDecl(field, CastToObjectType(func->objectType));
				const asCDataType fieldType = TypeOf(field->type);
				if( property == 0 || !fieldType.IsValid() )
				{
					FailAt(__LINE__, asINVALID_TYPE);
					return;
				}

				if( fieldType.IsObjectHandle() || fieldType.IsFuncdef() )
				{
					// Replace the owned field reference with null. REFCPY performs the
					// matching release and PopPtr commits the cleared slot.
					bc.Instr(asBC_PshNull);
					PushThisObject();
					bc.InstrSHORT_DW(asBC_ADDSi,
						(short)property->byteOffset, 0);
					MarkLastTypeRelocation(
						ownerDataType, asTYPE_RELOC_PROPERTY, property);
					if( fieldType.IsFuncdef() )
					{
						bc.InstrPTR(asBC_REFCPY,
							&bc.GetEngine()->functionBehaviours);
					}
					else
					{
						bc.InstrPTR(asBC_REFCPY, fieldType.GetTypeInfo());
					}
					bc.Instr(asBC_PopPtr);
					continue;
				}

				if( !fieldType.IsObject() || fieldType.IsReference() )
				{
					continue;
				}
				asCObjectType* const fieldObject =
					CastToObjectType(fieldType.GetTypeInfo());
				asCScriptFunction* fieldDestructor =
					FindPendingDestructor(fieldObject);
				if( fieldDestructor == 0 && fieldObject
					&& fieldObject->beh.destruct > 0
					&& (asUINT)fieldObject->beh.destruct
						< bc.GetEngine()->scriptFunctions.GetLength() )
				{
					fieldDestructor = bc.GetEngine()->scriptFunctions[
						fieldObject->beh.destruct];
				}
				if( fieldDestructor == 0 )
				{
					continue;
				}
				if( builder )
				{
					builder->MarkDependency(fieldDestructor, 0, 0);
				}
				PushThisObject();
				bc.InstrSHORT_DW(asBC_ADDSi,
					(short)property->byteOffset, 0);
				MarkLastTypeRelocation(
					ownerDataType, asTYPE_RELOC_PROPERTY, property);
				bc.Call(fieldDestructor->funcType == asFUNC_SYSTEM
						? asBC_CALLSYS : asBC_CALL,
					fieldDestructor->id, AS_PTR_SIZE);
			}

			for( asUINT baseIndex = 0;
				baseIndex < ownerClass->bases.GetLength(); ++baseIndex )
			{
				const asCDecl* const baseClass = context.GetDecl(
					ownerClass->bases[baseIndex]);
				if( baseClass == 0 || baseClass->kind != asAST_DECL_CLASS )
				{
					continue;
				}
				asCScriptFunction* baseDestructor = 0;
				for( asUINT baseChildIndex = 0;
					baseChildIndex < baseClass->children.GetLength(); ++baseChildIndex )
				{
					const asCDecl* const baseChild = context.GetDecl(
						baseClass->children[baseChildIndex]);
					if( baseChild && baseChild->kind == asAST_DECL_DESTRUCTOR )
					{
						baseDestructor = FindFunc(baseChild->id);
						break;
					}
				}
				if( baseDestructor )
				{
					PushThisObject();
					bc.Call(baseDestructor->funcType == asFUNC_SYSTEM
							? asBC_CALLSYS : asBC_CALL,
						baseDestructor->id, AS_PTR_SIZE);
				}
				break;
			}
		}

		int LocalDwords(const asCDataType& dataType)
		{
			if( dataType.IsNullHandle()
				|| dataType.IsObjectHandle()
				|| dataType.IsFuncdef() )
			{
				return AS_PTR_SIZE;
			}
			if( dataType.IsObject() && dataType.GetTypeInfo()
				&& (dataType.GetTypeInfo()->GetFlags() & asOBJ_REF) != 0
				&& (dataType.GetTypeInfo()->GetFlags() & asOBJ_VALUE) == 0 )
			{
				// This fork permits implicit-handle spellings for REF objects. They
				// still occupy a full pointer slot even when IsObjectHandle() is false.
				// A one-dword allocation followed by ClrVPtr/WRTV8 overlaps the method
				// frame on 64-bit targets.
				return AS_PTR_SIZE;
			}
			if( dataType.IsObject() && dataType.GetTypeInfo() && (dataType.GetTypeInfo()->GetFlags() & asOBJ_VALUE) )
			{
				const int size = dataType.GetSizeInMemoryDWords();
				return size > 0 ? size : 1;
			}
			return SizeDwords(dataType);
		}

		int AllocTyped(const asCDataType& dataType)
		{
			// A null literal has pointer-width representation but owns no Runtime
			// type/lifecycle. Tracking it in `objects` would synthesize FREE with a
			// null type operand during epilogue optimization. Keep the slot in the
			// pointer directory, but never publish cleanup metadata for it.
			const bool isNullHandle = dataType.IsNullHandle();
			const bool isObjectSlot = !isNullHandle && (dataType.IsObject()
				|| dataType.IsObjectHandle() || dataType.IsFuncdef());
			const int offset = isObjectSlot || isNullHandle
				? AllocDwords(LocalDwords(dataType))
				: AllocTemporary(dataType);
			if( isObjectSlot )
			{
				asSCodeGenObject obj;
				obj.offset = offset;
				obj.type = dataType;
				obj.live = true;
				objects.PushLast(obj);
				asCTypeInfo* const typeInfo = dataType.GetTypeInfo();
				const bool isPointerRepresentation = dataType.IsObjectHandle()
					|| dataType.IsFuncdef() || dataType.IsNullHandle()
					|| (typeInfo != 0
						&& (typeInfo->GetFlags() & asOBJ_REF) != 0
						&& (typeInfo->GetFlags() & asOBJ_VALUE) == 0);
				if( isPointerRepresentation )
				{
					// This is a representation directory only. Ownership and
					// cleanup remain exclusively in `objects`/objVariablePos.
					PublishPointerVariable(offset);
				}
				if( dataType.IsObjectHandle() || dataType.IsFuncdef() )
				{
					bc.InstrSHORT(asBC_ClrVPtr, (short)offset);
				}
			}
			else if( isNullHandle )
			{
				PublishPointerVariable(offset);
				bc.InstrSHORT(asBC_ClrVPtr, (short)offset);
			}
			return offset;
		}

		asCGlobalProperty* FindGlobal(asASTDeclId decl)
		{
			for( asUINT i = 0; i < globals.GetLength(); ++i )
			{
				if( globals[i].decl == decl )
				{
					if( builder && globals[i].prop )
					{
						builder->MarkDependency(globals[i].prop, 0, 0);
					}
					return globals[i].prop;
				}
			}
			return 0;
		}

		bool IsOwnedReferenceObject(const asCDataType& dataType) const
		{
			if( dataType.IsObjectHandle() || dataType.IsFuncdef() )
			{
				return true;
			}
			asCTypeInfo* const typeInfo = dataType.GetTypeInfo();
			return dataType.IsObject() && typeInfo != 0
				&& (typeInfo->GetFlags() & asOBJ_REF) != 0
				&& (typeInfo->GetFlags() & asOBJ_VALUE) == 0;
		}

		asSCodeGenObject* FindTrackedObject(int slot)
		{
			for( asUINT objectIndex = 0; objectIndex < objects.GetLength(); ++objectIndex )
			{
				if( objects[objectIndex].offset == slot )
				{
					return &objects[objectIndex];
				}
			}
			return 0;
		}

		bool EmitOwnedReferenceRelease(asSCodeGenObject& obj)
		{
			if( !IsOwnedReferenceObject(obj.type) )
			{
				FailAt(__LINE__, asINVALID_TYPE);
				return false;
			}
			void* const behaviour = obj.type.IsFuncdef()
				? (void*)&bc.GetEngine()->functionBehaviours
				: (void*)obj.type.GetTypeInfo();
			if( behaviour == 0 )
			{
				FailAt(__LINE__, asINVALID_TYPE);
				return false;
			}
			bc.InstrW_PTR(asBC_FREE, (short)obj.offset, behaviour);
			// FREE and UNINIT are one sealed lifecycle transition. The VM's
			// exception directory must not observe a released pointer as live.
			bc.ObjInfo(obj.offset, asOBJ_UNINIT);
			return true;
		}

		void DestroyObject(asSCodeGenObject& obj)
		{
			if( !obj.live )
			{
				return;
			}
			asCObjectType* objType = CastToObjectType(obj.type.GetTypeInfo());
			if( IsOwnedReferenceObject(obj.type) )
			{
				if( EmitOwnedReferenceRelease(obj) )
				{
					obj.live = false;
				}
				return;
			}
			if( objType )
			{
				// Canonical CodeGen keeps the runtime object-type tables detached until
				// the whole artifact has emitted successfully. In particular, a
				// Canonical destructor cannot be installed in beh.destruct while
				// another function body is still being generated: a later emission
				// failure must be able to abandon the artifact without leaving a
				// type that references a removed script function. Resolve that pending
				// declaration through the sealed CodeGen bind set instead. Registered
				// (native or already-published) destructors continue to use the runtime
				// behavior table.
				int destructorId = objType->beh.destruct;
				asCScriptFunction* dtor = 0;
				if( destructorId > 0 )
				{
					asCScriptEngine* const engine = bc.GetEngine();
					dtor = engine && (asUINT)destructorId < engine->scriptFunctions.GetLength()
						? engine->scriptFunctions[destructorId]
						: 0;
				}
				else
				{
					dtor = FindPendingDestructor(objType);
					if( dtor )
					{
						destructorId = dtor->id;
					}
				}
				if( destructorId <= 0 )
				{
					return;
				}
				// The emitted CALL/CALLSYS embeds the selected destructor function id.
				// Cache V2 must carry the matching signature authority even when the
				// destructor came from the detached canonical artifact rather than the
				// already-published Runtime behaviour table.
				if( builder && dtor )
				{
					builder->MarkDependency(dtor, 0, 0);
				}
				bc.InstrSHORT(asBC_PSF, (short)obj.offset);
				if( dtor && dtor->funcType == asFUNC_SYSTEM )
					bc.Call(asBC_CALLSYS, destructorId, AS_PTR_SIZE);
				else
					bc.Call(asBC_CALL, destructorId, AS_PTR_SIZE);
				bc.ObjInfo(obj.offset, asOBJ_UNINIT);
				obj.live = false;
			}
		}

		void DestroyLiveObjects()
		{
			for( asUINT i = objects.GetLength(); i > 0; --i )
			{
				if( !objects[i - 1].live || objects[i - 1].offset == returnSlot )
				{
					continue;
				}
				DestroyObject(objects[i - 1]);
			}
		}

		void DestroyOwnedParameters()
		{
			// AngelScript's by-value object ABI transfers one heap object pointer
			// into the callee frame. Locals retire first, then parameters in reverse
			// formal order. Exception unwinding performs the same parameter cleanup
			// from the Runtime signature, so these entries must not also appear in
			// ScriptFunctionData::objVariableTypes.
			for( asUINT i = ownedParameters.GetLength(); i > 0; --i )
			{
				const asSCodeGenOwnedParameter& parameter =
					ownedParameters[i - 1];
				if( parameter.objectType == 0 )
				{
					FailAt(__LINE__, asINVALID_TYPE);
					return;
				}
				bc.InstrW_PTR(
					asBC_FREE, (short)parameter.offset, parameter.objectType);
			}
		}

		void DetachReturnedHandle(int value)
		{
			if( func == 0 || value == 0x7fffffff || value == returnSlot )
			{
				return;
			}
			if( !func->returnType.IsObjectHandle() && !func->returnType.IsFuncdef() )
			{
				return;
			}
			for( asUINT i = 0; i < objects.GetLength(); ++i )
			{
				if( objects[i].offset != value )
				{
					continue;
				}
				if( objects[i].type.IsObjectHandle() || objects[i].type.IsFuncdef() )
				{
					bc.InstrSHORT(asBC_ClrVPtr, (short)value);
					bc.ObjInfo(value, asOBJ_UNINIT);
					objects[i].live = false;
					return;
				}
			}
		}

		void BindSlot(
			asASTDeclId decl,
			int offset,
			bool storesObjectPointer = false)
		{
			asSCodeGenSlot slot;
			slot.decl = decl;
			slot.offset = offset;
			slot.storesObjectPointer = storesObjectPointer;
			slots.PushLast(slot);
		}

		int FindSlot(asASTDeclId decl) const
		{
			for( asUINT i = 0; i < slots.GetLength(); ++i )
			{
				if( slots[i].decl == decl )
					return slots[i].offset;
			}
			return 0x7fffffff;
		}

		bool SlotStoresObjectPointer(asASTDeclId decl) const
		{
			for( asUINT i = 0; i < slots.GetLength(); ++i )
			{
				if( slots[i].decl == decl )
					return slots[i].storesObjectPointer;
			}
			return false;
		}

		asCScriptFunction* FindFunc(asASTDeclId decl)
		{
			if( !decl.IsValid() )
			{
				return 0;
			}
			for( asUINT i = 0; i < funcs.GetLength(); ++i )
			{
				if( funcs[i].decl == decl )
				{
					if( builder && funcs[i].func )
					{
						builder->MarkDependency(funcs[i].func, 0, 0);
						// Property syntax may lower to a Sema-generated accessor. The
						// callable relocation is an actual signature dependency, while
						// the accessor's selected backing field is also an actual layout
						// dependency of this caller. Preserve both edges so changing the
						// field layout cannot reuse a stale caller artifact merely because
						// the generated accessor declaration itself stayed stable.
						const asCDecl* const target = context.GetDecl(decl);
						if( target && target->accessorField.IsValid()
							&& funcs[i].func->objectType )
						{
							PropertyFromFieldDecl(
								context.GetDecl(target->accessorField),
								CastToObjectType(funcs[i].func->objectType));
						}
					}
					return funcs[i].func;
				}
			}
			return 0;
		}

		asCScriptFunction* FindPendingDestructor(asCObjectType* objType)
		{
			if( objType == 0 )
			{
				return 0;
			}

			asCScriptFunction* result = 0;
			for( asUINT i = 0; i < funcs.GetLength(); ++i )
			{
				asCScriptFunction* const candidate = funcs[i].func;
				if( candidate == 0 || candidate->objectType != objType )
				{
					continue;
				}
				const asCDecl* const decl = context.GetDecl(funcs[i].decl);
				if( decl == 0 || decl->kind != asAST_DECL_DESTRUCTOR )
				{
					continue;
				}
				if( result && result != candidate )
				{
					// A sealed class may have only one selected destructor. Do not
					// silently choose one if the semantic authority is inconsistent.
					FailAt(__LINE__, asINVALID_DECLARATION);
					return 0;
				}
				result = candidate;
			}
			return result;
		}

		int FindFieldOffset(asASTDeclId decl) const
		{
			const asCDecl* const field = context.GetDecl(decl);
			if( field == 0 || field->kind != asAST_DECL_VAR || field->byteOffset < 0 )
			{
				return -1;
			}

			// The sealed declaration is the sole semantic layout authority. CodeGen
			// must never reconstruct an absent layout fact after Sema has finished.
			return field->byteOffset;
		}

		const asSCodeGenFuncBind* FindBind(const asCScriptFunction* target) const
		{
			if( target == 0 )
			{
				return 0;
			}
			for( asUINT i = 0; i < funcs.GetLength(); ++i )
			{
				if( funcs[i].func == target )
				{
					return &funcs[i];
				}
			}
			return 0;
		}

		void BindParameters(const asCDecl* decl)
		{
			int stackPos = 0;
			asUINT parameterIndex = 0;
			if( func->objectType )
				stackPos = -AS_PTR_SIZE;
			if( func->DoesReturnOnStack() )
				stackPos -= AS_PTR_SIZE;

			for( asUINT i = 0; i < decl->children.GetLength(); ++i )
			{
				const asCDecl* child = context.GetDecl(decl->children[i]);
				if( child == 0 )
					continue;
				if( child->kind == asAST_DECL_PARAM )
				{
					if( parameterIndex >= func->parameterTypes.GetLength() )
					{
						FailAt(__LINE__, asINVALID_DECLARATION);
						return;
					}
					const asCDataType dataType = TypeOf(child->type);
					const asCDataType& runtimeParameterType =
						func->parameterTypes[parameterIndex];
					if( !PublishDebugVariable(child, runtimeParameterType, stackPos) )
						return;
					if( IsByValueObject(dataType) )
					{
						if( CanEmitSealedStorageCopy(dataType) )
						{
							const int local = AllocTyped(dataType);
							BindSlot(child->id, local);
							CopySealedValueFromPointer(stackPos, local, dataType);
						}
						else
						{
							asCObjectType* const objectType =
								CastToObjectType(dataType.GetTypeInfo());
							if( objectType == 0 )
							{
								FailAt(__LINE__, asINVALID_TYPE);
								return;
							}
							// Non-POD VALUE parameters are already unique heap objects.
							// Bind the incoming pointer directly; copying it into inline
							// storage would lose both identity and callee ownership.
							BindSlot(child->id, stackPos, true);
							asSCodeGenOwnedParameter owned;
							owned.offset = stackPos;
							owned.objectType = objectType;
							ownedParameters.PushLast(owned);
						}
					}
					else
					{
						BindSlot(child->id, stackPos);
					}
					stackPos -= SizeDwords(dataType);
					++parameterIndex;
				}
				else if( child->kind == asAST_DECL_VAR )
				{
					const asCDataType dataType = TypeOf(child->type);
					BindSlot(child->id, AllocTyped(dataType));
				}
			}
			if( const asSCodeGenFuncBind* self = FindBind(func) )
			{
				for( asUINT c = 0; c < self->captureCount; ++c )
				{
					const asCDecl* captured = context.GetDecl(captureDecls[self->captureBegin + c]);
					if( captured == 0 )
					{
						continue;
					}
					BindSlot(captured->id, stackPos);
					stackPos -= SizeDwords(TypeOf(captured->type));
				}
			}
		}

		void CopyVar(int dest, int src, int dwords)
		{
			// VM local offsets name the highest dword of an allocation. Copy from
			// that end towards the allocation base so a 12-byte scalar aggregate is
			// not silently truncated to the first CpyVtoV8.
			while( dwords >= 2 )
			{
				bc.InstrW_W(asBC_CpyVtoV8, dest, src);
				dest -= 2;
				src -= 2;
				dwords -= 2;
			}
			if( dwords == 1 )
			{
				bc.InstrW_W(asBC_CpyVtoV4, dest, src);
			}
		}

		const asCDecl* FindSealedStorageCopyClass(const asCDataType& dataType) const
		{
			asCTypeInfo* const typeInfo = dataType.GetTypeInfo();
			if( typeInfo == 0 )
			{
				return 0;
			}
			asCString runtimeKey;
			if( typeInfo->nameSpace && typeInfo->nameSpace->name.GetLength() )
			{
				runtimeKey = typeInfo->nameSpace->name;
				runtimeKey += "::";
			}
			runtimeKey += typeInfo->name;

			const asCDecl* match = 0;
			for( asUINT declIndex = 1; declIndex <= context.GetDeclCount(); ++declIndex )
			{
				const asCDecl* const candidate =
					context.GetDecl(asASTDeclId(declIndex));
				if( candidate == 0 || candidate->kind != asAST_DECL_CLASS
					|| (candidate->traits & asAST_TRAIT_VALUE) == 0 )
				{
					continue;
				}
				const bool exactKey = candidate->stableKey.Equals(runtimeKey);
				const bool unqualifiedKey = typeInfo->nameSpace != 0
					&& typeInfo->nameSpace->name.GetLength() == 0
					&& candidate->name.Equals(typeInfo->name);
				if( !exactKey && !unqualifiedKey )
				{
					continue;
				}
				if( match != 0 )
				{
					// A byte-copy permission must identify one exact semantic owner.
					return 0;
				}
				match = candidate;
			}
			return match;
		}

		bool CanEmitSealedStorageCopy(const asCDataType& dataType) const
		{
			asCTypeInfo* const typeInfo = dataType.GetTypeInfo();
			const int byteSize = dataType.GetSizeInMemoryBytes();
			const asCDecl* const sealedClass = FindSealedStorageCopyClass(dataType);
			const bool producerAuthorized = sealedClass != 0
				&& (sealedClass->traits & asAST_TRAIT_TRIVIAL_STORAGE_COPY) != 0
				&& sealedClass->byteSize == byteSize
				&& sealedClass->byteAlignment > 0;
			return dataType.IsObject()
				&& !dataType.IsObjectHandle()
				&& !dataType.IsReference()
				&& typeInfo != 0
				&& (typeInfo->GetFlags() & asOBJ_VALUE) != 0
				&& ((typeInfo->GetFlags() & asOBJ_POD) != 0 || producerAuthorized)
				&& byteSize > 0
				&& byteSize <= 0x7fff
				&& bc.GetEngine() != 0;
		}

		bool IsByValueObject(const asCDataType& dataType)
		{
			asCTypeInfo* const typeInfo = dataType.GetTypeInfo();
			return dataType.IsObject()
				&& !dataType.IsObjectHandle()
				&& !dataType.IsReference()
				&& typeInfo != 0
				&& (typeInfo->GetFlags() & asOBJ_VALUE) != 0;
		}

		bool IsWideValueObject(const asCDataType& dataType)
		{
			asCTypeInfo* const typeInfo = dataType.GetTypeInfo();
			return dataType.IsObject()
				&& !dataType.IsObjectHandle()
				&& !dataType.IsReference()
				&& typeInfo != 0
				&& (typeInfo->GetFlags() & asOBJ_VALUE) != 0
				&& LocalDwords(dataType) > AS_PTR_SIZE;
		}

		void EmitSealedStorageCopy(int src, int dest, const asCDataType& dataType, bool destIsPointer)
		{
			if( !CanEmitSealedStorageCopy(dataType) )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return;
			}

			const int byteSize = dataType.GetSizeInMemoryBytes();
			// `COPY` pops destination before source and leaves its lvalue result.
			bc.InstrSHORT(asBC_PSF, (short)src);
			if( destIsPointer )
				bc.InstrSHORT(asBC_PshVPtr, (short)dest);
			else
				bc.InstrSHORT(asBC_PSF, (short)dest);
			bc.InstrSHORT_DW(asBC_COPY, (short)byteSize, 0);
			MarkLastTypeRelocation(dataType, asTYPE_RELOC_METADATA_ONLY);
			bc.Instr(asBC_PopPtr);
		}

		void CopySealedValueFromPointer(int srcPointerSlot, int dest, const asCDataType& dataType)
		{
			if( !CanEmitSealedStorageCopy(dataType) )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return;
			}

			const int byteSize = dataType.GetSizeInMemoryBytes();
			// Incoming object arguments use AngelScript's standard pointer ABI.
			// Materialize the pointed-to sealed value into the callee's local slot so
			// canonical expressions retain normal value semantics.
			bc.InstrSHORT(asBC_PshVPtr, (short)srcPointerSlot);
			bc.InstrSHORT(asBC_PSF, (short)dest);
			bc.InstrSHORT_DW(asBC_COPY, (short)byteSize, 0);
			MarkLastTypeRelocation(dataType, asTYPE_RELOC_METADATA_ONLY);
			bc.Instr(asBC_PopPtr);
		}

		bool EmitSealedValueArgument(int src, const asCDataType& dataType)
		{
			if( !CanEmitSealedStorageCopy(dataType) )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return false;
			}

			// Preserve by-value semantics without changing asCScriptFunction's
			// public object-parameter ABI: copy into caller-owned storage, then
			// pass its pointer. This is also what external SetArgObject callers
			// already provide to the VM.
			const int temporary = AllocTyped(dataType);
			EmitSealedStorageCopy(src, temporary, dataType, false);
			if( !ok )
			{
				return false;
			}
			bc.InstrSHORT(asBC_PSF, (short)temporary);
			return true;
		}

		bool EmitOwnedSealedValueArgument(
			int src,
			const asCDataType& dataType,
			int& pointerSlot)
		{
			if( !CanEmitSealedStorageCopy(dataType) )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return false;
			}

			asCObjectType* const objectType = CastToObjectType(dataType.GetTypeInfo());
			if( objectType == 0 )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return false;
			}

			// CALLSYS/CallGeneric owns every by-value object argument and retires
			// it through sysFunc->cleanArgs. Allocate distinct heap storage, copy the
			// sealed value into it, and transfer that pointer to the call stack.
			// The local pointer slot is deliberately not an `objects` entry: after
			// the call it is only a non-owning alias to storage CallGeneric freed.
			pointerSlot = AllocPointerVariable();
			bc.InstrSHORT(asBC_ClrVPtr, (short)pointerSlot);
			bc.InstrSHORT(asBC_PSF, (short)pointerSlot);
			bc.Alloc(asBC_ALLOC, objectType, 0, AS_PTR_SIZE, false);
			EmitSealedStorageCopy(src, pointerSlot, dataType, true);
			if( !ok )
			{
				return false;
			}
			bc.InstrSHORT(asBC_PshVPtr, (short)pointerSlot);
			return true;
		}

		void CopyValue(int dest, int src, const asCDataType& dataType)
		{
			if( IsByValueObject(dataType) )
			{
				// Every value-object copy, including one-dword structs, consumes the
				// producer-carried storage-copy decision. Width must never bypass the
				// semantic firewall and silently turn assignment into memcpy.
				EmitSealedStorageCopy(src, dest, dataType, false);
				return;
			}
			CopyVar(dest, src, LocalDwords(dataType));
		}

		void LoadReturn(int src, int dwords)
		{
			if( dwords >= 2 )
				bc.InstrSHORT(asBC_CpyVtoR8, (short)src);
			else
				bc.InstrSHORT(asBC_CpyVtoR4, (short)src);
		}

		void StoreReturn(int dest, int dwords)
		{
			if( dwords >= 2 )
				bc.InstrSHORT(asBC_CpyRtoV8, (short)dest);
			else
				bc.InstrSHORT(asBC_CpyRtoV4, (short)dest);
		}

		void SetConst(int dest, const asCDataType& dataType, asQWORD bits)
		{
			const int bytes = dataType.GetSizeInMemoryBytes();
			if( bytes <= 1 )
				bc.InstrSHORT_B(asBC_SetV1, (short)dest, (asBYTE)bits);
			else if( bytes == 2 )
				bc.InstrSHORT_W(asBC_SetV2, (short)dest, (asWORD)bits);
			else if( bytes <= 4 )
				bc.InstrSHORT_DW(asBC_SetV4, (short)dest, (asDWORD)bits);
			else
				bc.InstrSHORT_QW(asBC_SetV8, (short)dest, bits);
		}

		int EmitBoolJump(int condOffset, asEBCInstr jump, int label)
		{
			bc.InstrSHORT(asBC_CpyVtoR1, (short)condOffset);
			bc.InstrINT(jump, label);
			return condOffset;
		}

		int EmitPrimitiveIncrementOrDecrement(const asCExpr* expr, bool returnsOldValue, bool decrement)
		{
			// This deliberately begins with the mutability shape the canonical AST
			// already models without ambiguity: a one-word local or value parameter.
			// Globals, fields, indexes, references, and non-32-bit scalar forms need
			// their own single-evaluation/addressing paths; failing closed here is
			// preferable to publishing bytecode that evaluates or writes them twice.
			if( expr == 0 || expr->children.GetLength() != 1 )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}

			const asCExpr* const targetExpr = context.GetExpr(expr->children[0]);
			if( targetExpr == 0 || targetExpr->kind != asAST_EXPR_DECL_REF
				|| targetExpr->valueCategory != asAST_VALUE_LVALUE )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}

			const asCDataType targetType = TypeOf(targetExpr->type);
			const eTokenType token = targetType.GetTokenType();
			if( (token != ttInt && token != ttUInt) || targetType.IsReference()
				|| targetType.IsObjectHandle() || SizeDwords(targetType) != 1 )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}

			const asCDecl* const targetDecl = context.GetDecl(targetExpr->resolvedDecl);
			if( targetDecl == 0 || (targetDecl->kind != asAST_DECL_VAR && targetDecl->kind != asAST_DECL_PARAM) )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}

			const int target = FindSlot(targetDecl->id);
			if( target == 0x7fffffff )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}

			// Postfix is a read-before-write operation. Materialize the prior value
			// before changing the sole target slot, then return that snapshot.
			const int oldValue = returnsOldValue
				? AllocTemporary(targetType, 1) : 0;
			if( returnsOldValue )
			{
				CopyVar(oldValue, target, 1);
			}


			// Use the VM's standard address-mutation sequence. With optimization
			// disabled it remains LDV + INCi/DECi; with optimization enabled,
			// asCByteCode::OptimizeInstruction folds it to IncVi/DecVi. This keeps
			// asEP_OPTIMIZE_BYTECODE observable without reconstructing a second
			// Canonical-only peephole policy.
			bc.InstrSHORT(asBC_LDV, (short)target);
			bc.Instr(decrement ? asBC_DECi : asBC_INCi);
			return returnsOldValue ? oldValue : target;
		}

		int EmitStringLiteralAddress(const asCExpr* expr)
		{
			asCScriptEngine* const engine = bc.GetEngine();
			const asCDataType literalType = expr ? TypeOf(expr->type) : asCDataType();
			if( expr == 0 || expr->kind != asAST_EXPR_STRING_LITERAL
				|| engine == 0 || engine->stringFactory == 0
				|| !engine->stringType.IsValid()
				|| literalType.GetTypeInfo() != engine->stringType.GetTypeInfo()
				|| expr->literalBits != expr->literal.GetLength() )
			{
				const asCString bytes = expr
					? FormatCanonicalLiteralBytes(expr->literal) : asCString();
				failureDetail.Format(
					"invalid sealed StringLiteral contract function=%s expr=%u "
					"length=%u literalBits=%llu bytes=%s factory=%s typeMatch=%s",
					activeDecl && activeDecl->stableKey.AddressOf()
						? activeDecl->stableKey.AddressOf() : "<unknown>",
					expr ? expr->id.value : 0,
					expr ? expr->literal.GetLength() : 0,
					static_cast<unsigned long long>(expr ? expr->literalBits : 0),
					bytes.AddressOf() ? bytes.AddressOf() : "",
					engine && engine->stringFactory ? "present" : "missing",
					engine && expr && engine->stringType.IsValid()
						&& literalType.GetTypeInfo() == engine->stringType.GetTypeInfo()
						? "yes" : "no");
				FailAt(__LINE__, asINVALID_CONFIGURATION);
				return 0;
			}

			const void* const constant = stringConstants.Acquire(
				expr->literal.AddressOf(),
				static_cast<asUINT>(expr->literal.GetLength()));
			if( constant == 0 )
			{
				failureDetail.Format(
					"Engine string factory rejected sealed StringLiteral function=%s "
					"expr=%u length=%u",
					activeDecl && activeDecl->stableKey.AddressOf()
						? activeDecl->stableKey.AddressOf() : "<unknown>",
					expr->id.value,
					expr->literal.GetLength());
				FailAt(__LINE__, asERROR);
				return 0;
			}

			// PGA is the normal VM/string-reference opcode observed by
			// asCScriptFunction::AddReferences(). Capture the opaque object identity
			// in a pointer-valued local only so the call ABI can push the identity,
			// never the address of that local slot.
			bc.InstrPTR(asBC_PGA, const_cast<void*>(constant));
			bc.Instr(asBC_PopRPtr);
			return CapturePointerRegister();
		}

		int EmitExpr(asASTExprId id)
		{
			if( !ok )
				return 0;
			const asCExpr* expr = context.GetExpr(id);
			if( expr == 0 )
			{
				FailAt(__LINE__, asAST_VERIFY_DANGLING_ID);
				return 0;
			}

			const asCDataType dataType = TypeOf(expr->type);
			const int dwords = SizeDwords(dataType);

			switch( expr->kind )
			{
			case asAST_EXPR_THIS:
				if( thisOffset == 0x7fffffff )
				{
					FailAt(__LINE__, asNO_FUNCTION);
					return 0;
				}
				return thisOffset;
			case asAST_EXPR_INTEGER_LITERAL:
			case asAST_EXPR_BOOL_LITERAL:
			case asAST_EXPR_FLOAT_LITERAL:
			{
				const int dest = AllocTemporary(dataType, dwords > 0 ? dwords : 1);
				SetConst(dest, dataType, expr->literalBits);
				return dest;
			}
			case asAST_EXPR_DECL_REF:
				return EmitDeclRef(expr, dataType, dwords, false);
			case asAST_EXPR_NULL_LITERAL:
			{
				const int dest = AllocTyped(dataType.IsValid() ? dataType : asCDataType::CreateNullHandle());
				bc.InstrSHORT(asBC_ClrVPtr, (short)dest);
				return dest;
			}
			case asAST_EXPR_CONSTRUCT:
				return EmitConstruct(expr, dataType);
			case asAST_EXPR_MATERIALIZE_TEMPORARY:
			{
				const int cached = FindExprSlot(materializedSlots, expr->id);
				if( cached != 0x7fffffff )
				{
					return cached;
				}
				if( expr->children.GetLength() == 0 )
				{
					FailAt(__LINE__, asNOT_SUPPORTED);
					return 0;
				}
				const int slot = EmitExpr(expr->children[0]);
				BindExprSlot(materializedSlots, expr->id, slot);
				return slot;
			}
			case asAST_EXPR_CLEANUP:
				if( expr->children.GetLength() == 0 )
				{
					FailAt(__LINE__, asNOT_SUPPORTED);
					return 0;
				}
				return EmitExpr(expr->children[0]);
			case asAST_EXPR_OPAQUE_VALUE:
			{
				const int cached = FindExprSlot(opaqueSlots, expr->id);
				if( cached != 0x7fffffff )
				{
					return cached;
				}
				if( expr->children.GetLength() == 0 )
				{
					return 0;
				}
				const int slot = EmitExpr(expr->children[0]);
				BindExprSlot(opaqueSlots, expr->id, slot);
				return slot;
			}
			case asAST_EXPR_MEMBER_REF:
				return EmitMember(expr, dataType, dwords, false);
			case asAST_EXPR_INDEX:
				return EmitIndex(expr, dataType, dwords, false);
			case asAST_EXPR_UNARY:
			{
				if( expr->children.GetLength() != 1 )
				{
					FailAt(__LINE__, asNOT_SUPPORTED);
					return 0;
				}
				if( expr->literal.Equals("post++") )
				{
					return EmitPrimitiveIncrementOrDecrement(expr, true, false);
				}
				if( expr->literal.Equals("pre++") )
				{
					return EmitPrimitiveIncrementOrDecrement(expr, false, false);
				}
				if( expr->literal.Equals("post--") )
				{
					return EmitPrimitiveIncrementOrDecrement(expr, true, true);
				}
				if( expr->literal.Equals("pre--") )
				{
					return EmitPrimitiveIncrementOrDecrement(expr, false, true);
				}
				const int inner = EmitExpr(expr->children[0]);
				if( !ok )
					return 0;
				const int dest = AllocTemporary(dataType, dwords > 0 ? dwords : 1);
				CopyVar(dest, inner, dwords > 0 ? dwords : 1);
				if( expr->literal.Equals("-") )
				{
					if( dataType.IsFloat64Type() )
						bc.InstrSHORT(asBC_NEGd, (short)dest);
					else if( dataType.IsFloat32Type() )
						bc.InstrSHORT(asBC_NEGf, (short)dest);
					else if( dwords >= 2 )
						bc.InstrSHORT(asBC_NEGi64, (short)dest);
					else
						bc.InstrSHORT(asBC_NEGi, (short)dest);
				}
				else if( expr->literal.Equals("!") )
				{
					bc.InstrSHORT(asBC_NOT, (short)dest);
				}
				else if( expr->literal.Equals("~") )
				{
					if( dwords >= 2 )
						bc.InstrSHORT(asBC_BNOT64, (short)dest);
					else
						bc.InstrSHORT(asBC_BNOT, (short)dest);
				}
				else
				{
					FailAt(__LINE__, asNOT_SUPPORTED);
					return 0;
				}
				return dest;
			}
			case asAST_EXPR_BINARY:
				return EmitBinary(expr, dataType, dwords);
			case asAST_EXPR_LOGICAL:
				return EmitLogical(expr);
			case asAST_EXPR_ASSIGN:
				return EmitAssign(expr, dataType, dwords);
			case asAST_EXPR_CONDITIONAL:
				return EmitConditional(expr, dataType, dwords);
			case asAST_EXPR_CONVERSION:
				return EmitConversion(expr, dataType, dwords);
			case asAST_EXPR_SEQUENCE:
			{
				if( expr->literal.Equals("opaque") && expr->children.GetLength() )
				{
					return EmitExpr(expr->children[expr->children.GetLength() - 1]);
				}
				int last = 0;
				for( asUINT i = 0; i < expr->children.GetLength(); ++i )
				{
					const asCExpr* child = context.GetExpr(expr->children[i]);
					if( child && child->kind == asAST_EXPR_CALL && !child->resolvedDecl.IsValid() )
					{
						// Parser FromNode can intern a receiver-less CALL beside the member CALL.
						continue;
					}
					if( child && child->kind == asAST_EXPR_DECL_REF )
					{
						const asCDecl* target = context.GetDecl(child->resolvedDecl);
						if( target && (CanonicalDeclIsFunctionLike(target->kind) || target->kind == asAST_DECL_CLASS) )
						{
							// IIFE / T() intern keeps a DECL_REF beside CALL/CONSTRUCT.
							continue;
						}
					}
					last = EmitExpr(expr->children[i]);
				}
				return last;
			}
			case asAST_EXPR_CALL:
				return EmitCall(expr, dataType, dwords);
			default:
			{
				const asCDecl* const target =
					context.GetDecl(expr->resolvedDecl);
				const asCString typeName = dataType.Format(0);
				const asCString literalDiagnostic =
					FormatCanonicalLiteralBytes(expr->literal);
				failureDetail.Format(
					"unsupported expression function=%s expr=%u kind=%d literal=%s "
					"literalLength=%u literalBits=%llu "
					"type=%s resolvedDecl=%u targetKind=%d targetName=%s "
					"receiver=%u children=%u",
					activeDecl && activeDecl->stableKey.AddressOf()
						? activeDecl->stableKey.AddressOf() : "<unknown>",
					expr->id.value,
					static_cast<int>(expr->kind),
					literalDiagnostic.AddressOf()
						? literalDiagnostic.AddressOf() : "",
					expr->literal.GetLength(),
					static_cast<unsigned long long>(expr->literalBits),
					typeName.AddressOf() ? typeName.AddressOf() : "<invalid>",
					expr->resolvedDecl.value,
					target ? static_cast<int>(target->kind)
						: static_cast<int>(asAST_DECL_INVALID),
					target && target->name.AddressOf()
						? target->name.AddressOf() : "<missing>",
					expr->receiver.value,
					expr->children.GetLength());
				FailAt(__LINE__, asINVALID_ARG);
				return 0;
			}
			}
		}

		int LoadGlobal(asCGlobalProperty* prop, const asCDataType& dataType)
		{
			const int dest = AllocTemporary(dataType, LocalDwords(dataType));
			void* address = prop->GetAddressOfValue();
			// CpyGtoV4 only materializes one dword. Globals share the same
			// sealed type-width contract as fields and indexed values, so load
			// through the value register and select RDR{1,2,4,8} from that type.
			bc.InstrPTR(asBC_LDG, address);
			EmitReadValue(dest, dataType, 0, "global");
			return dest;
		}

		void StoreGlobal(asCGlobalProperty* prop, int src, const asCDataType& dataType)
		{
			void* address = prop->GetAddressOfValue();
			bc.InstrPTR(asBC_LDG, address);
			EmitWriteValue(src, dataType);
		}

		bool FuncdefSignatureMatches(const asCScriptFunction* signature, const asCScriptFunction* callee) const
		{
			if( signature == 0 || callee == 0 )
			{
				return false;
			}
			if( !signature->returnType.IsEqualExceptRefAndConst(callee->returnType)
				&& !(signature->returnType.IsPrimitive() && callee->returnType.IsPrimitive()
					&& signature->returnType.GetTokenType() == callee->returnType.GetTokenType()) )
			{
				return false;
			}
			if( signature->parameterTypes.GetLength() != callee->parameterTypes.GetLength() )
			{
				return false;
			}
			for( asUINT i = 0; i < signature->parameterTypes.GetLength(); ++i )
			{
				const asCDataType& a = signature->parameterTypes[i];
				const asCDataType& b = callee->parameterTypes[i];
				if( a.IsEqualExceptRefAndConst(b) )
				{
					continue;
				}
				if( a.IsPrimitive() && b.IsPrimitive() && a.GetTokenType() == b.GetTokenType() )
				{
					continue;
				}
				return false;
			}
			return true;
		}

		asCFuncdefType* LookupExistingFuncdef(asCScriptFunction* callee) const
		{
			asCScriptEngine* engine = bc.GetEngine();
			if( callee == 0 || engine == 0 )
			{
				return 0;
			}
			for( asUINT n = 0; n < engine->funcDefs.GetLength(); ++n )
			{
				asCFuncdefType* candidate = engine->funcDefs[n];
				if( candidate && FuncdefSignatureMatches(candidate->funcdef, callee) )
				{
					return candidate;
				}
			}
			for( asUINT n = 0; n < engine->registeredFuncDefs.GetLength(); ++n )
			{
				asCFuncdefType* candidate = engine->registeredFuncDefs[n];
				if( candidate && FuncdefSignatureMatches(candidate->funcdef, callee) )
				{
					return candidate;
				}
			}
			return 0;
		}

		asCDataType FuncPtrHandleType(asCScriptFunction* callee)
		{
			asCFuncdefType* funcDef = LookupExistingFuncdef(callee);
			if( funcDef == 0 )
			{
				FailAt(__LINE__, asNO_FUNCTION);
				return asCDataType();
			}
			asCDataType handleType = asCDataType::CreateType(funcDef, false);
			handleType.MakeHandle(true);
			return handleType;
		}

		void EmitFuncPtrInto(asCScriptFunction* callee, int dest)
		{
			bc.InstrSHORT(asBC_ClrVPtr, (short)dest);
			bc.InstrPTR(asBC_FuncPtr, callee);
			bc.InstrSHORT(asBC_PSF, (short)dest);
			bc.InstrPTR(asBC_REFCPY, &bc.GetEngine()->functionBehaviours);
			bc.Instr(asBC_PopPtr);
		}

		int EmitFuncPtr(asCScriptFunction* callee)
		{
			const asCDataType handleType = FuncPtrHandleType(callee);
			if( !handleType.IsValid() )
			{
				return 0;
			}
			const int dest = AllocTyped(handleType);
			EmitFuncPtrInto(callee, dest);
			return dest;
		}

		void PushValue(int offset, int dwords)
		{
			if( dwords <= 1 )
			{
				bc.InstrSHORT(asBC_PshV4, (short)offset);
				return;
			}
			bc.InstrSHORT(asBC_PshV8, (short)offset);
		}

		int EmitDeclRef(const asCExpr* expr, const asCDataType& dataType, int dwords, bool asAddress)
		{
			const asCDecl* target = context.GetDecl(expr->resolvedDecl);
			if( target == 0 )
			{
				FailAt(__LINE__, asAST_VERIFY_DANGLING_ID);
				return 0;
			}
			if( target->kind == asAST_DECL_FUNCTION || target->kind == asAST_DECL_METHOD )
			{
				asCScriptFunction* callee = FindFunc(target->id);
				if( callee == 0 )
				{
					FailAt(__LINE__, asNOT_SUPPORTED);
					return 0;
				}
				return EmitFuncPtr(callee);
			}
			const asCDecl* const targetOwner = context.GetDecl(target->parent);
			if( target->kind == asAST_DECL_VAR
				&& targetOwner != 0 && targetOwner->kind == asAST_DECL_ENUM
				&& target->hasConstantValue )
			{
				if( asAddress )
				{
					FailAt(__LINE__, asINVALID_DECLARATION);
					return 0;
				}
				// Enum locals occupy a normal VM dword even though this fork stores
				// enum payloads in one byte in object/global memory. Preserve the
				// sign-extended semantic value in the evaluation slot.
				const int dest = AllocTemporary(dataType, 1);
				bc.InstrSHORT_DW(asBC_SetV4, (short)dest,
					static_cast<asDWORD>(static_cast<asINT32>(target->constantValue)));
				return dest;
			}
			if( asCGlobalProperty* prop = FindGlobal(target->id) )
			{
				if( prop->isPureConstant && !asAddress
					&& target->hasConstantValue )
				{
					// Match legacy CompileVariableAccess: a Sema-proven pure
					// constant is folded into the body. The HardValue edge is
					// captured by FindGlobal; do not emit LDG, or the function
					// artifact would require a GlobalStorage relocation.
					const int dest = AllocTemporary(
						dataType, dwords > 0 ? dwords : 1);
					SetConst(dest, dataType, target->constantValue);
					return dest;
				}
				return LoadGlobal(prop, dataType);
			}
			if( asCObjectProperty* member = FindThisProperty(target) )
			{
				if( thisOffset == 0x7fffffff )
				{
					FailAt(__LINE__, asNOT_SUPPORTED);
					return 0;
				}
				const asCDataType ownerDataType =
					asCDataType::CreateType(func->objectType, false);
				PushThisObject();
				bc.InstrSHORT_DW(asBC_ADDSi, (short)member->byteOffset, 0);
				MarkLastTypeRelocation(
					ownerDataType, asTYPE_RELOC_PROPERTY, member);
				bc.Instr(asBC_PopRPtr);
				const int ptr = AllocPointerVariable();
				StoreReturn(ptr, AS_PTR_SIZE);
				if( asAddress )
				{
					return ptr;
				}
				const int payload = ValueDwords(member->type);
				const int dest = AllocTemporary(
					member->type, payload > 0 ? payload : 1);
				bc.InstrSHORT(asBC_PshVPtr, (short)ptr);
				bc.Instr(asBC_PopRPtr);
				EmitReadValue(dest, member->type, expr, "decl-ref-this-property");
				return dest;
			}
			if( target->kind != asAST_DECL_PARAM && target->kind != asAST_DECL_VAR )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}
			const int offset = FindSlot(target->id);
			if( offset == 0x7fffffff )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}
			if( dataType.IsReference() && !dataType.IsObjectHandle() )
			{
				asCDataType valueType = dataType;
				valueType.MakeReference(false);
				const int valueDwords = dataType.GetSizeInMemoryDWords();
				bc.InstrSHORT(asBC_PshVPtr, (short)offset);
				bc.Instr(asBC_PopRPtr);
				const int dest = AllocTemporary(
					valueType, valueDwords > 0 ? valueDwords : 1);
				if( valueDwords >= 2 )
					bc.InstrSHORT(asBC_RDR8, (short)dest);
				else
					bc.InstrSHORT(asBC_RDR4, (short)dest);
				return dest;
			}
			(void)dwords;
			return offset;
		}

		int CapturePointerRegister()
		{
			const int pointerSlot = AllocPointerVariable();
			StoreReturn(pointerSlot, AS_PTR_SIZE);
			return pointerSlot;
		}

		int EmitLValueAddress(asASTExprId expressionId)
		{
			const asCExpr* expression = context.GetExpr(expressionId);
			if( expression == 0 )
			{
				FailAt(__LINE__, asAST_VERIFY_DANGLING_ID);
				return 0;
			}

			// Member-chain sequencing stores the base and the terminal member as
			// named children. The terminal member owns that same base edge, so
			// descending to it evaluates the address exactly once; emitting every
			// Sequence child first would evaluate the receiver twice.
			if( expression->kind == asAST_EXPR_SEQUENCE
				&& expression->children.GetLength() > 0 )
			{
				return EmitLValueAddress(
					expression->children[expression->children.GetLength() - 1]);
			}
			if( (expression->kind == asAST_EXPR_OPAQUE_VALUE
					|| expression->kind == asAST_EXPR_CLEANUP)
				&& expression->children.GetLength() == 1 )
			{
				return EmitLValueAddress(expression->children[0]);
			}
			if( expression->kind == asAST_EXPR_MATERIALIZE_TEMPORARY
				&& expression->children.GetLength() == 1 )
			{
				// MaterializeTemporary is the sealed lifetime boundary for a
				// prvalue object.  Taking the address of its Construct child would
				// discard that boundary and either emit the constructor again or
				// fail after recursively stripping the wrapper.  Emit the wrapper
				// itself so its memoized value slot is constructed exactly once,
				// then pass the address of that inline VALUE storage to the resolved
				// receiver/call.  AllocTyped already records the slot as live, and
				// the function cleanup plan destroys it in reverse order.
				const asCDataType materializedType = TypeOf(expression->type);
				if( !materializedType.IsValid()
					|| !materializedType.IsNonPrimitiveValueType()
					|| materializedType.IsReference()
					|| materializedType.IsObjectHandle() )
				{
					if( failureDetail.GetLength() == 0 )
					{
						const asCType* const canonicalType =
							context.GetType(expression->type.type);
						failureDetail.Format(
							"invalid materialized lvalue receiver function=%s "
							"expr=%u typeRef=%u typeKey=%s",
							activeDecl && activeDecl->stableKey.AddressOf()
								? activeDecl->stableKey.AddressOf() : "<unknown>",
							expression->id.value,
							expression->type.type.value,
							canonicalType && canonicalType->stableKey.AddressOf()
								? canonicalType->stableKey.AddressOf() : "<invalid>");
					}
					FailAt(__LINE__, asNOT_SUPPORTED);
					return 0;
				}
				const int materializedSlot = EmitExpr(expression->id);
				if( !ok )
				{
					return 0;
				}
				bc.InstrSHORT(asBC_PSF, (short)materializedSlot);
				bc.Instr(asBC_PopRPtr);
				return CapturePointerRegister();
			}
			if( expression->kind == asAST_EXPR_CALL )
			{
				// A by-value object call owns its result in the typed slot returned by
				// EmitCall. Hidden-return calls initialize that caller-owned storage
				// directly and mark it live; taking the receiver address must therefore
				// emit the call once and point at that exact slot. Recursing through a
				// child would either address the source object or execute the getter a
				// second time, and loading the slot as a scalar loses object identity.
				const asCDataType callResultType = TypeOf(expression->type);
				if( !callResultType.IsValid()
					|| !callResultType.IsNonPrimitiveValueType()
					|| callResultType.IsReference()
					|| callResultType.IsObjectHandle() )
				{
					if( failureDetail.GetLength() == 0 )
					{
						const asCType* const canonicalType =
							context.GetType(expression->type.type);
						failureDetail.Format(
							"invalid call-result lvalue receiver function=%s "
							"expr=%u typeRef=%u typeKey=%s reference=%d handle=%d",
							activeDecl && activeDecl->stableKey.AddressOf()
								? activeDecl->stableKey.AddressOf() : "<unknown>",
							expression->id.value,
							expression->type.type.value,
							canonicalType && canonicalType->stableKey.AddressOf()
								? canonicalType->stableKey.AddressOf() : "<invalid>",
							callResultType.IsReference() ? 1 : 0,
							callResultType.IsObjectHandle() ? 1 : 0);
					}
					FailAt(__LINE__, asNOT_SUPPORTED);
					return 0;
				}
				const int callResultSlot = EmitExpr(expression->id);
				if( !ok )
				{
					return 0;
				}
				bc.InstrSHORT(asBC_PSF, (short)callResultSlot);
				bc.Instr(asBC_PopRPtr);
				return CapturePointerRegister();
			}

			if( expression->kind == asAST_EXPR_STRING_LITERAL )
			{
				// A sealed string literal is already an Engine-owned value object.
				// Its lvalue identity is the opaque constant returned by the string
				// factory, not the address of a temporary VM scalar slot.  This route
				// is required both for an ordinary reference argument and for the rhs
				// of a Sema-selected value-type opAssign (for example
				// `FString Value = "text"`).
				return EmitStringLiteralAddress(expression);
			}

			if( expression->kind == asAST_EXPR_THIS )
			{
				if( thisOffset == 0x7fffffff )
				{
					FailAt(__LINE__, asNO_FUNCTION);
					return 0;
				}
				// Method frames store the receiver pointer at offset zero. VALUE
				// `this` is a reference variable: that slot already holds the live
				// object identity. PSF(0) would address the pointer slot itself
				// instead of the caller-owned object (see PushThisObject). REF
				// methods store the same pointer form. Returning the slot lets
				// call/member emitters PshVPtr the object address, including
				// generated `BindUFunction(..., __DelegateSignature(this))`.
				return thisOffset;
			}

			if( expression->kind == asAST_EXPR_DECL_REF )
			{
				const asCDecl* const target =
					context.GetDecl(expression->resolvedDecl);
				if( target == 0 )
				{
					FailAt(__LINE__, asAST_VERIFY_DANGLING_ID);
					return 0;
				}
				if( asCGlobalProperty* const global =
					FindGlobal(expression->resolvedDecl) )
				{
					const asCDataType globalType = TypeOf(target->type);
					if( globalType.IsNonPrimitiveValueType() )
					{
						PushDirectGlobalValueObject(global);
						bc.Instr(asBC_PopRPtr);
					}
					else
					{
						bc.InstrPTR(asBC_LDG, global->GetAddressOfValue());
					}
					return CapturePointerRegister();
				}

				if( FindThisProperty(target) != 0 )
				{
					return EmitDeclRef(
						expression,
						TypeOf(expression->type),
						SizeDwords(TypeOf(expression->type)),
						true);
				}

				const int slot = FindSlot(target->id);
				if( slot == 0x7fffffff )
				{
					FailAt(__LINE__, asNOT_SUPPORTED);
					return 0;
				}
				if( SlotStoresObjectPointer(target->id) )
				{
					// A callee-owned by-value object parameter is already an object
					// pointer. Address-of must return that pointee identity rather
					// than the address of the pointer slot itself.
					return slot;
				}
				const asCDataType valueType = TypeOf(expression->type);
				if( valueType.IsReference() && !valueType.IsObjectHandle() )
				{
					// A reference parameter/local already stores the pointee address.
					return slot;
				}
				bc.InstrSHORT(asBC_PSF, (short)slot);
				bc.Instr(asBC_PopRPtr);
				return CapturePointerRegister();
			}

			if( expression->kind == asAST_EXPR_MEMBER_REF
				&& expression->children.GetLength() == 1 )
			{
				const asCExpr* const baseExpression =
					context.GetExpr(expression->children[0]);
				const asCDecl* const field =
					context.GetDecl(expression->resolvedDecl);
				asCObjectType* const baseObject =
					ObjectTypeFromExpr(baseExpression);
				asCObjectProperty* const property =
					PropertyFromFieldDecl(field, baseObject);
				if( baseExpression == 0 || field == 0
					|| field->kind != asAST_DECL_VAR
					|| property == 0 )
				{
					FailAt(__LINE__, asINVALID_DECLARATION);
					return 0;
				}
				const int basePointer =
					EmitLValueAddress(expression->children[0]);
				if( !ok )
				{
					return 0;
				}
				const asCDataType ownerDataType =
					asCDataType::CreateType(baseObject, false);
				bc.InstrSHORT(asBC_PshVPtr, (short)basePointer);
				bc.InstrSHORT_DW(asBC_ADDSi,
					(short)property->byteOffset, 0);
				MarkLastTypeRelocation(
					ownerDataType, asTYPE_RELOC_PROPERTY, property);
				bc.Instr(asBC_PopRPtr);
				return CapturePointerRegister();
			}

			if( failureDetail.GetLength() == 0 )
			{
				const asCExpr* const firstChild = expression->children.GetLength()
					? context.GetExpr(expression->children[0]) : 0;
				const asCDecl* const target = context.GetDecl(expression->resolvedDecl);
				const asCType* const canonicalType = context.GetType(expression->type.type);
				const asCString literalDiagnostic =
					FormatCanonicalLiteralBytes(expression->literal);
				failureDetail.Format(
					"unsupported lvalue address function=%s expr=%u kind=%d "
					"literal=%s typeRef=%u typeKey=%s resolvedDecl=%u "
					"targetKind=%d targetName=%s children=%u child0=%u child0Kind=%d",
					activeDecl && activeDecl->stableKey.AddressOf()
						? activeDecl->stableKey.AddressOf() : "<unknown>",
					expression->id.value,
					static_cast<int>(expression->kind),
					literalDiagnostic.AddressOf()
						? literalDiagnostic.AddressOf() : "",
					expression->type.type.value,
					canonicalType && canonicalType->stableKey.AddressOf()
						? canonicalType->stableKey.AddressOf() : "<invalid>",
					expression->resolvedDecl.value,
					target ? static_cast<int>(target->kind)
						: static_cast<int>(asAST_DECL_INVALID),
					target && target->name.AddressOf()
						? target->name.AddressOf() : "<missing>",
					expression->children.GetLength(),
					firstChild ? firstChild->id.value : 0,
					firstChild ? static_cast<int>(firstChild->kind) : -1);
			}
			FailAt(__LINE__, asNOT_SUPPORTED);
			return 0;
		}

		int EmitResolvedAssignmentOperator(
			const asCExpr* expression,
			const asCExpr* lhs,
			const asCExpr* rhs)
		{
			const asCDecl* const sealedOperator = expression
				? context.GetDecl(expression->resolvedDecl) : 0;
			asCScriptFunction* const runtimeOperator = expression
				? FindFunc(expression->resolvedDecl) : 0;
			const asCDataType lhsType = lhs
				? TypeOf(lhs->type) : asCDataType();
			const asCDataType rhsType = rhs
				? TypeOf(rhs->type) : asCDataType();
			if( sealedOperator == 0
				|| sealedOperator->kind != asAST_DECL_METHOD
				|| !sealedOperator->name.Equals("opAssign")
				|| runtimeOperator == 0 || runtimeOperator->objectType == 0
				|| runtimeOperator->parameterTypes.GetLength() != 1
				|| !runtimeOperator->parameterTypes[0].IsReference()
				|| !lhsType.IsValid() || !rhsType.IsValid()
				|| lhsType.GetTypeInfo() != runtimeOperator->objectType
				|| rhsType.GetTypeInfo()
					!= runtimeOperator->parameterTypes[0].GetTypeInfo() )
			{
				if( failureDetail.GetLength() == 0 )
				{
					failureDetail.Format(
						"invalid sealed opAssign ABI function=%s assign=%u target=%u targetName=%s runtime=%d params=%u lhsType=%s rhsType=%s",
						activeDecl && activeDecl->stableKey.AddressOf()
							? activeDecl->stableKey.AddressOf() : "<unknown>",
						expression ? expression->id.value : 0,
						expression ? expression->resolvedDecl.value : 0,
						sealedOperator && sealedOperator->name.AddressOf()
							? sealedOperator->name.AddressOf() : "<missing>",
						runtimeOperator ? runtimeOperator->id : -1,
						runtimeOperator
							? runtimeOperator->parameterTypes.GetLength() : 0,
						lhsType.IsValid()
							? lhsType.Format(0).AddressOf() : "<invalid>",
						rhsType.IsValid()
							? rhsType.Format(0).AddressOf() : "<invalid>");
				}
				FailAt(__LINE__, asNO_FUNCTION);
				return 0;
			}

			// AngelScript assignment evaluates the rhs before acquiring the target.
			// Both helpers return pointer slots, so member/reference receivers are
			// evaluated once and neither 32-byte application object is materialized
			// through the scalar RDR/WRTV path.
			const int rhsPointer = EmitLValueAddress(expression->children[1]);
			const int lhsPointer = EmitLValueAddress(expression->children[0]);
			if( !ok )
			{
				return 0;
			}
			bc.InstrSHORT(asBC_PshVPtr, (short)rhsPointer);
			bc.InstrSHORT(asBC_PshVPtr, (short)lhsPointer);
			const int pop = AS_PTR_SIZE * 2;
			if( runtimeOperator->funcType == asFUNC_SYSTEM )
				bc.Call(asBC_CALLSYS, runtimeOperator->id, pop);
			else if( runtimeOperator->funcType == asFUNC_SCRIPT )
				bc.Call(asBC_CALL, runtimeOperator->id, pop);
			else
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}
			return lhsPointer;
		}

		const asCExpr* UnwrapMaterializedConstruct(const asCExpr* expr) const
		{
			while( expr
				&& (expr->kind == asAST_EXPR_MATERIALIZE_TEMPORARY
					|| expr->kind == asAST_EXPR_CLEANUP)
				&& expr->children.GetLength() == 1 )
			{
				expr = context.GetExpr(expr->children[0]);
			}
			return expr;
		}

		bool EmitOwnedNonPodValueArgument(
			asASTExprId argumentId,
			const asCDataType& formalType,
			int& pointerSlot)
		{
			const asCExpr* const wrapped = context.GetExpr(argumentId);
			const asCExpr* const construct =
				UnwrapMaterializedConstruct(wrapped);
			const asCDecl* const sealedCtor = construct
				? context.GetDecl(construct->resolvedDecl) : 0;
			asCScriptFunction* const copyCtor = construct
				? FindFunc(construct->resolvedDecl) : 0;
			asCObjectType* const objectType =
				CastToObjectType(formalType.GetTypeInfo());
			if( wrapped == 0 || construct == 0
				|| construct->kind != asAST_EXPR_CONSTRUCT
				|| construct->children.GetLength() != 1
				|| TypeOf(construct->type) != formalType
				|| sealedCtor == 0
				|| sealedCtor->kind != asAST_DECL_CONSTRUCTOR
				|| copyCtor == 0 || objectType == 0
				|| copyCtor->objectType != objectType
				|| copyCtor->parameterTypes.GetLength() != 1
				|| !copyCtor->parameterTypes[0].IsReference()
				|| copyCtor->parameterTypes[0].GetTypeInfo()
					!= formalType.GetTypeInfo() )
			{
				if( failureDetail.GetLength() == 0 )
				{
					failureDetail.Format(
						"invalid sealed by-value copy transfer function=%s argument=%u construct=%u ctor=%u runtime=%d type=%s",
						activeDecl && activeDecl->stableKey.AddressOf()
							? activeDecl->stableKey.AddressOf() : "<unknown>",
						argumentId.value,
						construct ? construct->id.value : 0,
						sealedCtor ? sealedCtor->id.value : 0,
						copyCtor ? copyCtor->id : -1,
						formalType.IsValid()
							? formalType.Format(0).AddressOf() : "<invalid>");
				}
				FailAt(__LINE__, asNOT_SUPPORTED);
				return false;
			}

			const int sourcePointer =
				EmitLValueAddress(construct->children[0]);
			if( !ok )
			{
				return false;
			}

			// ALLOC is the VM's exception-safe heap-construction primitive. Keep
			// the pointer slot untracked in the caller: after the call the callee
			// owns and retires this allocation, exactly like legacy GETOBJ.
			pointerSlot = AllocPointerVariable();
			bc.InstrSHORT(asBC_ClrVPtr, (short)pointerSlot);
			bc.InstrSHORT(asBC_PSF, (short)pointerSlot);
			bc.InstrSHORT(asBC_PshVPtr, (short)sourcePointer);
			bc.Alloc(
				asBC_ALLOC,
				objectType,
				copyCtor->id,
				AS_PTR_SIZE * 2,
				true);
			return true;
		}

		void EmitConstructorInit(asASTExprId initId)
		{
			const asCExpr* const init = context.GetExpr(initId);
			if( init == 0 )
			{
				FailAt(__LINE__, asAST_VERIFY_DANGLING_ID);
				return;
			}

			// Sema represents a member InitPlan as `field = Construct(...)` so the
			// exact field declaration, QualType and selected constructor all survive
			// sealing. A non-POD value member must be constructed directly in that
			// field's storage. Materializing a local and then routing it through the
			// scalar WRTV path discards the registered application lifecycle contract.
			if( init->kind == asAST_EXPR_ASSIGN
				&& init->children.GetLength() == 2
				&& func && func->objectType )
			{
				const asCExpr* const lhs = UnwrapAssignLhs(
					context.GetExpr(init->children[0]));
				const asCExpr* const rhs = UnwrapMaterializedConstruct(
					context.GetExpr(init->children[1]));
				const asCDecl* const field = lhs
					&& lhs->kind == asAST_EXPR_DECL_REF
					? context.GetDecl(lhs->resolvedDecl) : 0;
				asCObjectProperty* const property = field
					? PropertyFromFieldDecl(field, func->objectType) : 0;
				const asCDataType fieldType = field
					? TypeOf(field->type) : asCDataType();
				if( rhs && rhs->kind == asAST_EXPR_CONSTRUCT
					&& !rhs->literal.Equals("list-pattern")
					&& field && property && fieldType.IsNonPrimitiveValueType() )
				{
					const asCDataType constructType = TypeOf(rhs->type);
					asCObjectType* const fieldObject =
						CastToObjectType(fieldType.GetTypeInfo());
					asCScriptFunction* const ctor = FindFunc(rhs->resolvedDecl);
					if( !constructType.IsValid()
						|| constructType.GetTypeInfo() != fieldType.GetTypeInfo()
						|| fieldObject == 0 || ctor == 0
						|| ctor->objectType != fieldObject )
					{
						FailAt(__LINE__, asNO_FUNCTION);
						return;
					}

					asCArray<int> args;
					for( asUINT argIndex = 0;
						argIndex < rhs->children.GetLength(); ++argIndex )
					{
						args.PushLast(EmitExpr(rhs->children[argIndex]));
					}
					if( !ok )
					{
						return;
					}

					int pop = 0;
					for( asUINT argIndex = args.GetLength();
						argIndex > 0; --argIndex )
					{
						const asUINT index = argIndex - 1;
						const asCExpr* const argExpr =
							context.GetExpr(rhs->children[index]);
						const asCDataType argType = argExpr
							? TypeOf(argExpr->type) : asCDataType();
						if( !argType.IsValid() || IsWideValueObject(argType) )
						{
							FailAt(__LINE__, asNOT_SUPPORTED);
							return;
						}
						const int argDwords = SizeDwords(argType);
						PushValue(args[index], argDwords);
						pop += argDwords;
					}

					const asCDataType ownerDataType =
						asCDataType::CreateType(func->objectType, false);
					PushThisObject();
					bc.InstrSHORT_DW(asBC_ADDSi,
						(short)property->byteOffset, 0);
					MarkLastTypeRelocation(
						ownerDataType, asTYPE_RELOC_PROPERTY, property);
					pop += AS_PTR_SIZE;
					bc.Call(ctor->funcType == asFUNC_SYSTEM
							? asBC_CALLSYS : asBC_CALL,
						ctor->id, pop);
					return;
				}
			}

			// Primitive/handle initializers and already explicit constructor-body
			// expressions keep the general expression path.
			EmitExpr(initId);
		}

		int EmitAssign(const asCExpr* expr, const asCDataType& dataType, int dwords)
		{
			if( expr->children.GetLength() != 2 )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}
			const asCExpr* lhs = UnwrapAssignLhs(context.GetExpr(expr->children[0]));
			const asCExpr* rhsExpr = context.GetExpr(expr->children[1]);
			if( lhs == 0 || rhsExpr == 0 )
			{
				FailAt(__LINE__, asAST_VERIFY_DANGLING_ID);
				return 0;
			}
			if( !expr->literal.Equals("=") )
			{
				return EmitCompoundAssign(expr, lhs, dataType, dwords);
			}
			if( expr->resolvedDecl.IsValid() )
			{
				return EmitResolvedAssignmentOperator(expr, lhs, rhsExpr);
			}
			if( lhs->kind == asAST_EXPR_DECL_REF )
			{
				const asCDecl* target = context.GetDecl(lhs->resolvedDecl);
				if( asCGlobalProperty* prop = FindGlobal(lhs->resolvedDecl) )
				{
					const int rhs = EmitExpr(expr->children[1]);
					if( !ok )
						return 0;
					StoreGlobal(prop, rhs, dataType);
					return rhs;
				}
				if( const asCDecl* lhsDecl = context.GetDecl(lhs->resolvedDecl) )
				{
					if( asCObjectProperty* member = FindThisProperty(lhsDecl) )
					{
						const int rhs = EmitExpr(expr->children[1]);
						if( !ok )
							return 0;
						if( thisOffset == 0x7fffffff )
						{
							FailAt(__LINE__, asNOT_SUPPORTED);
							return 0;
						}
						const asCDataType ownerDataType =
							asCDataType::CreateType(func->objectType, false);
						PushThisObject();
						bc.InstrSHORT_DW(asBC_ADDSi, (short)member->byteOffset, 0);
						MarkLastTypeRelocation(
							ownerDataType, asTYPE_RELOC_PROPERTY, member);
						bc.Instr(asBC_PopRPtr);
						EmitWriteValue(rhs, member->type);
						return rhs;
					}
				}
				const int slot = FindSlot(lhs->resolvedDecl);
				if( slot == 0x7fffffff )
				{
					const asCDecl* const targetOwner = target
						? context.GetDecl(target->parent) : 0;
					if( failureDetail.GetLength() == 0 )
					{
						failureDetail.Format(
							"function=%s assignment-target=%s kind=%s owner=%s "
							"byteOffset=%d runtimeObject=%s runtimeProperties=%u "
							"emitterLine=%d error=%d",
							activeDecl && activeDecl->stableKey.AddressOf()
								? activeDecl->stableKey.AddressOf() : "<unknown>",
							target && target->stableKey.AddressOf()
								? target->stableKey.AddressOf() : "<unknown>",
							target ? CanonicalDeclKindName(target->kind) : "<missing>",
							targetOwner && targetOwner->stableKey.AddressOf()
								? targetOwner->stableKey.AddressOf() : "<missing>",
							target ? target->byteOffset : -1,
							func && func->objectType && func->objectType->name.AddressOf()
								? func->objectType->name.AddressOf() : "<none>",
							func && func->objectType
								? func->objectType->properties.GetLength() : 0,
							__LINE__, asNOT_SUPPORTED);
					}
					FailAt(__LINE__, asNOT_SUPPORTED);
					return 0;
				}
				const asCDataType lhsType = TypeOf(lhs->type);
				if( lhsType.IsObjectHandle() || lhsType.IsFuncdef() )
				{
					if( rhsExpr->kind == asAST_EXPR_NULL_LITERAL )
					{
						bc.InstrSHORT(asBC_ClrVPtr, (short)slot);
						return slot;
					}
					const asCExpr* construct = rhsExpr;
					while( construct
						&& (construct->kind == asAST_EXPR_MATERIALIZE_TEMPORARY
							|| construct->kind == asAST_EXPR_CLEANUP
							|| construct->kind == asAST_EXPR_CONVERSION)
						&& construct->children.GetLength() )
					{
						construct = context.GetExpr(construct->children[0]);
					}
					if( construct && construct->kind == asAST_EXPR_CONSTRUCT )
					{
						asCDataType constructType = TypeOf(construct->type);
						if( construct->literal.Equals("list-pattern") )
						{
							constructType = TypeOf(lhs->type);
						}
						return EmitConstructInto(construct, slot, constructType);
					}
					const int rhs = EmitExpr(expr->children[1]);
					if( !ok )
						return 0;
					CopyVar(slot, rhs, AS_PTR_SIZE);
					return slot;
				}
				if( lhsType.IsReference() && !lhsType.IsObjectHandle() )
				{
					const int rhs = EmitExpr(expr->children[1]);
					if( !ok )
						return 0;
					bc.InstrSHORT(asBC_PshVPtr, (short)slot);
					bc.Instr(asBC_PopRPtr);
					EmitWriteValue(rhs, lhsType);
					return rhs;
				}
				const asCExpr* construct = rhsExpr;
				while( construct && (construct->kind == asAST_EXPR_MATERIALIZE_TEMPORARY || construct->kind == asAST_EXPR_CLEANUP) && construct->children.GetLength() )
				{
					construct = context.GetExpr(construct->children[0]);
				}
				if( construct && construct->kind == asAST_EXPR_CONSTRUCT )
				{
					asCDataType constructType = TypeOf(construct->type);
					if( construct->literal.Equals("list-pattern") )
					{
						constructType = TypeOf(lhs->type);
					}
					return EmitConstructInto(construct, slot, constructType);
				}
				if( target && (target->kind == asAST_DECL_FUNCTION || target->kind == asAST_DECL_METHOD) )
				{
					FailAt(__LINE__, asNOT_SUPPORTED);
					return 0;
				}
				if( rhsExpr->kind == asAST_EXPR_DECL_REF )
				{
					const asCDecl* rhsDecl = context.GetDecl(rhsExpr->resolvedDecl);
					if( rhsDecl && (rhsDecl->kind == asAST_DECL_FUNCTION || rhsDecl->kind == asAST_DECL_METHOD) )
					{
						asCScriptFunction* callee = FindFunc(rhsDecl->id);
						if( callee == 0 )
						{
							FailAt(__LINE__, asNOT_SUPPORTED);
							return 0;
						}
						EmitFuncPtrInto(callee, slot);
						return slot;
					}
				}
				const int rhs = EmitExpr(expr->children[1]);
				if( !ok )
					return 0;
				CopyValue(slot, rhs, lhsType.IsValid() ? lhsType : dataType);
				if( ok && IsByValueObject(lhsType)
					&& IsDeclaredInitializer(lhs->resolvedDecl, expr->children[1]) )
				{
					bc.ObjInfo(slot, asOBJ_INIT);
				}
				return slot;
			}
			const int rhs = EmitExpr(expr->children[1]);
			if( !ok )
				return 0;
			if( lhs->kind == asAST_EXPR_MEMBER_REF )
			{
				EmitMemberStore(lhs, rhs);
				return rhs;
			}
			if( lhs->kind == asAST_EXPR_INDEX )
			{
				const asCDataType lhsType = TypeOf(lhs->type);
				asCDataType payloadType = lhsType;
				if( payloadType.IsReference() )
				{
					payloadType.MakeReference(false);
				}
				int payloadDwords = payloadType.GetSizeInMemoryDWords();
				if( payloadDwords < 1 )
				{
					payloadDwords = 1;
				}
				const int ptr = EmitIndex(lhs, lhsType, payloadDwords, true);
				if( !ok )
					return 0;
				bc.InstrSHORT(asBC_PshVPtr, (short)ptr);
				bc.Instr(asBC_PopRPtr);
				EmitWriteValue(rhs, payloadType);
				return rhs;
			}
			const int dest = EmitExpr(expr->children[0]);
			CopyVar(dest, rhs, dwords > 0 ? dwords : 1);
			return dest;
		}

		const char* CompoundBinaryOperator(const asCString& compound) const
		{
			if( compound.Equals("+=") ) return "+";
			if( compound.Equals("-=") ) return "-";
			if( compound.Equals("*=") ) return "*";
			if( compound.Equals("/=") ) return "/";
			if( compound.Equals("%=") ) return "%";
			if( compound.Equals("&=") ) return "&";
			if( compound.Equals("|=") ) return "|";
			if( compound.Equals("^=") ) return "^";
			if( compound.Equals("<<=") ) return "<<";
			if( compound.Equals(">>=") ) return ">>";
			if( compound.Equals(">>>=") ) return ">>>";
			return 0;
		}

		int NormalizeCompoundStorageValue(int value, const asCDataType& dataType)
		{
			if( !(dataType.IsIntegerType() || dataType.IsUnsignedType() || dataType.IsEnumType()) )
			{
				return value;
			}
			const int bytes = dataType.GetSizeInMemoryBytes();
			if( bytes != 1 && bytes != 2 )
			{
				return value;
			}
			const int narrowed = AllocTemporary(dataType, 1);
			bc.InstrW_W(bytes == 1 ? asBC_iTOb : asBC_iTOw, narrowed, value);
			return narrowed;
		}

		int EmitCompoundAssign(
			const asCExpr* expr,
			const asCExpr* lhs,
			const asCDataType& dataType,
			int dwords)
		{
			const char* const binaryOp = CompoundBinaryOperator(expr->literal);
			if( binaryOp == 0 || lhs == 0 || lhs->kind != asAST_EXPR_DECL_REF )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}

			// AngelScript compound assignment evaluates the RHS before acquiring and
			// reading the mutation target. The sealed AST keeps the original target
			// and operator, so CodeGen must not synthesize a second target evaluation.
			const int rhs = EmitExpr(expr->children[1]);
			if( !ok )
			{
				return 0;
			}

			const asCDataType lhsType = TypeOf(lhs->type);
			if( lhsType.IsReference() && !lhsType.IsObjectHandle() )
			{
				// A reference local stores the address of the value. Read through it once
				// after RHS evaluation, then use the same reference slot for write-back.
				const int referenceSlot = FindSlot(lhs->resolvedDecl);
				if( referenceSlot == 0x7fffffff )
				{
					FailAt(__LINE__, asNOT_SUPPORTED);
					return 0;
				}
				asCDataType payloadType = lhsType;
				payloadType.MakeReference(false);
				const int current = AllocTemporary(
					payloadType, ValueDwords(payloadType));
				bc.InstrSHORT(asBC_PshVPtr, (short)referenceSlot);
				bc.Instr(asBC_PopRPtr);
				EmitReadValue(current, payloadType, expr, "reference-mutation");
				const asCExpr* rhsExpr = context.GetExpr(expr->children[1]);
				const asCDataType rhsType = rhsExpr
					? TypeOf(rhsExpr->type) : payloadType;
				int result = EmitScalarBinaryValues(
					binaryOp, current, rhs, payloadType, rhsType,
					ValueDwords(payloadType));
				result = NormalizeCompoundStorageValue(result, payloadType);
				if( !ok )
				{
					return 0;
				}
				bc.InstrSHORT(asBC_PshVPtr, (short)referenceSlot);
				bc.Instr(asBC_PopRPtr);
				EmitWriteValue(result, payloadType);
				return result;
			}

			const int current = EmitDeclRef(lhs, lhsType, SizeDwords(lhsType), false);
			if( !ok )
			{
				return 0;
			}
			const asCExpr* rhsExpr = context.GetExpr(expr->children[1]);
			const asCDataType rhsType = rhsExpr
				? TypeOf(rhsExpr->type) : lhsType;
			int result = EmitScalarBinaryValues(
				binaryOp, current, rhs, lhsType, rhsType,
				dwords > 0 ? dwords : SizeDwords(lhsType));
			result = NormalizeCompoundStorageValue(result, lhsType);
			if( !ok )
			{
				return 0;
			}

			if( asCGlobalProperty* prop = FindGlobal(lhs->resolvedDecl) )
			{
				StoreGlobal(prop, result, lhsType);
				return result;
			}
			if( const asCDecl* lhsDecl = context.GetDecl(lhs->resolvedDecl) )
			{
				if( asCObjectProperty* member = FindThisProperty(lhsDecl) )
				{
					if( thisOffset == 0x7fffffff )
					{
						FailAt(__LINE__, asNOT_SUPPORTED);
						return 0;
					}
					const asCDataType ownerDataType =
						asCDataType::CreateType(func->objectType, false);
					PushThisObject();
					bc.InstrSHORT_DW(asBC_ADDSi, (short)member->byteOffset, 0);
					MarkLastTypeRelocation(
						ownerDataType, asTYPE_RELOC_PROPERTY, member);
					bc.Instr(asBC_PopRPtr);
					EmitWriteValue(result, member->type);
					return result;
				}
			}

			const int slot = FindSlot(lhs->resolvedDecl);
			if( slot == 0x7fffffff )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}
			CopyValue(slot, result, lhsType.IsValid() ? lhsType : dataType);
			return result;
		}

		int EmitListFactoryInto(const asCExpr* expr, int dest, const asCDataType& dataType)
		{
			if( expr == 0 || !expr->resolvedDecl.IsValid() )
			{
				FailAt(__LINE__, asNO_FUNCTION);
				return 0;
			}
			asCScriptFunction* const factory = FindFunc(expr->resolvedDecl);
			if( factory == 0 )
			{
				FailAt(__LINE__, asNO_FUNCTION);
				return 0;
			}
			asCObjectType* objType = CastToObjectType(dataType.GetTypeInfo());
			if( objType == 0 )
			{
				objType = ObjectTypeFromExpr(expr);
			}
			if( objType == 0 || objType->beh.listFactory <= 0 || factory->id != objType->beh.listFactory )
			{
				FailAt(__LINE__, asNO_FUNCTION);
				return 0;
			}
			const int listId = objType->beh.listFactory;
			asCArray<int> args;
			for( asUINT i = 0; i < expr->children.GetLength(); ++i )
			{
				args.PushLast(EmitExpr(expr->children[i]));
			}
			if( !ok )
			{
				return 0;
			}
			asCDataType elemType;
			if( !ListPatternElementType(factory, elemType) )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}
			const int elemBytes = ListElementBytes(elemType);
			const int elemDwords = ValueDwords(elemType);
			const asUINT count = args.GetLength();
			asDWORD bufferSize = 4;
			for( asUINT i = 0; i < count; ++i )
			{
				if( elemBytes >= 4 && (bufferSize & 0x3) )
				{
					bufferSize += 4 - (bufferSize & 0x3);
				}
				bufferSize += (asDWORD)elemBytes;
			}
			const int bufferVar = AllocPointerVariable();
			bc.InstrSHORT_DW(asBC_AllocMem, (short)bufferVar, bufferSize);
			bc.InstrSHORT_DW_DW(asBC_SetListSize, (short)bufferVar, 0, count);
			asDWORD offset = 4;
			for( asUINT i = 0; i < count; ++i )
			{
				if( elemBytes >= 4 && (offset & 0x3) )
				{
					offset += 4 - (offset & 0x3);
				}
				bc.InstrSHORT_DW(asBC_PshListElmnt, (short)bufferVar, offset);
				bc.Instr(asBC_PopRPtr);
				EmitWriteValue(args[i], elemType);
				offset += (asDWORD)elemBytes;
			}
			bc.InstrSHORT(asBC_PshVPtr, (short)bufferVar);
			if( factory->funcType == asFUNC_SYSTEM )
				bc.Call(asBC_CALLSYS, listId, AS_PTR_SIZE);
			else
				bc.Call(asBC_CALL, listId, AS_PTR_SIZE);
			bc.InstrSHORT(asBC_STOREOBJ, (short)dest);
			bc.ObjInfo(dest, asOBJ_INIT);
			// The cleanup helper is an anonymous Engine cache entry. Emit a null
			// candidate operand and carry the exact list-factory identity plus owner
			// ABI in a pointer-free relocation; the aggregate install transaction
			// materializes and patches the helper only after validate-all succeeds.
			bc.InstrW_PTR(asBC_FREE, (short)bufferVar, 0);
			if( !MarkLastListPatternRelocation(dataType, factory) )
			{
				return 0;
			}
			return dest;
		}

		int EmitConstructInto(const asCExpr* expr, int dest, const asCDataType& dataType)
		{
			if( expr && expr->literal.Equals("list-pattern") )
			{
				return EmitListFactoryInto(expr, dest, dataType);
			}
			asCObjectType* objType = CastToObjectType(dataType.GetTypeInfo());
			if( objType == 0 )
			{
				objType = ObjectTypeFromExpr(expr);
			}
			if( objType == 0 )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}
			asCArray<int> args;
			for( asUINT i = 0; i < expr->children.GetLength(); ++i )
				args.PushLast(EmitExpr(expr->children[i]));
			if( !ok )
				return 0;

			asCScriptFunction* ctor = FindFunc(expr->resolvedDecl);
			int ctorId = ctor ? ctor->id : 0;
			asCScriptFunction* factory = 0;
			int factoryId = 0;
			const bool isValue = (objType->GetFlags() & asOBJ_VALUE) != 0;
			bool listedFactory = false;
			if( ctor && objType )
			{
				if( objType->beh.factory == ctor->id )
				{
					listedFactory = true;
				}
				for( asUINT f = 0; !listedFactory && f < objType->beh.factories.GetLength(); ++f )
				{
					if( objType->beh.factories[f] == ctor->id )
					{
						listedFactory = true;
					}
				}
			}
			// REF construct is a factory (CALL/CALLSYS + STOREOBJ), including SCRIPT
			// $fact stubs. ALLOC+CALLSYS of a SCRIPT factory has null sysFuncIntf (0x38).
			const bool isTemplateFactoryStub = ctor && ctor->name == "$fact";
			if( ctor && !isValue && (ctor->funcType == asFUNC_SYSTEM || listedFactory || isTemplateFactoryStub) )
			{
				factory = ctor;
				factoryId = ctorId;
				ctor = 0;
				ctorId = 0;
			}
			if( (ctor == 0 || ctorId <= 0) && (factory == 0 || factoryId <= 0) )
			{
				FailAt(__LINE__, asNO_FUNCTION);
				return 0;
			}
			if( factory && factoryId > 0 && !isValue )
			{
				int pop = 0;
				if( factory->hiddenArgumentIndex >= 0 || factory->parameterTypes.GetLength() > args.GetLength() )
				{
					const int typeSlot = AllocPointerVariable();
					bc.InstrSHORT_QW(asBC_SetV8, (short)typeSlot, (asQWORD)(asPWORD)objType);
					bc.InstrSHORT(asBC_PSF, (short)typeSlot);
					pop += AS_PTR_SIZE;
				}
				for( asUINT i = args.GetLength(); i > 0; --i )
				{
					const asUINT index = i - 1;
					const asCExpr* const argExpr = context.GetExpr(expr->children[index]);
					const asCDataType argType = argExpr ? TypeOf(argExpr->type) : asCDataType();
					const int argDwords = SizeDwords(argType);
					// Constructor/factory parameter metadata is not yet sealed in this
					// CodeGen slice. Refuse a wide VALUE argument rather than pushing
					// its first eight bytes as though it were an object pointer.
					if( !argType.IsValid() || IsWideValueObject(argType) )
					{
						FailAt(__LINE__, asNOT_SUPPORTED);
						return 0;
					}
					PushValue(args[index], argDwords);
					pop += argDwords;
				}
				if( factory->funcType == asFUNC_SYSTEM )
					bc.Call(asBC_CALLSYS, factoryId, pop);
				else
					bc.Call(asBC_CALL, factoryId, pop);
				// REF factory returns in objectRegister. CpyRtoV* reads the value
				// register and would leave dest as the ClrVPtr null handle.
				bc.InstrSHORT(asBC_STOREOBJ, (short)dest);
				bc.ObjInfo(dest, asOBJ_INIT);
				return dest;
			}
			if( ctor == 0 || ctorId <= 0 )
			{
				FailAt(__LINE__, asNO_FUNCTION);
				return 0;
			}

			int pop = 0;
			if( !isValue )
			{
				bc.InstrSHORT(asBC_PSF, (short)dest);
				pop += AS_PTR_SIZE;
			}
			for( asUINT i = args.GetLength(); i > 0; --i )
			{
				const asUINT index = i - 1;
				const asCExpr* const argExpr = context.GetExpr(expr->children[index]);
				const asCDataType argType = argExpr ? TypeOf(argExpr->type) : asCDataType();
				const int argDwords = SizeDwords(argType);
				if( !argType.IsValid() || IsWideValueObject(argType) )
				{
					FailAt(__LINE__, asNOT_SUPPORTED);
					return 0;
				}
				PushValue(args[index], argDwords);
				pop += argDwords;
			}
			if( isValue )
			{
				bc.InstrSHORT(asBC_PSF, (short)dest);
				pop += AS_PTR_SIZE;
				if( ctor && ctor->funcType == asFUNC_SYSTEM )
					bc.Call(asBC_CALLSYS, ctorId, pop);
				else
					bc.Call(asBC_CALL, ctorId, pop);
			}
			else
			{
				bc.Alloc(asBC_ALLOC, objType, ctorId, pop, true);
			}
			bc.ObjInfo(dest, asOBJ_INIT);
			return dest;
		}

		int EmitConstruct(const asCExpr* expr, const asCDataType& dataType)
		{
			const int dest = AllocTyped(dataType);
			return EmitConstructInto(expr, dest, dataType);
		}

		int EmitMember(const asCExpr* expr, const asCDataType& dataType, int dwords, bool forStore)
		{
			if( expr->children.GetLength() == 0 )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}
			const asCExpr* baseExpr = context.GetExpr(expr->children[0]);
			asCGlobalProperty* const directGlobalValue = FindDirectGlobalValueObject(baseExpr);
			const int base = directGlobalValue ? 0 : EmitExpr(expr->children[0]);
			if( !ok )
				return 0;
			asCObjectType* objType = ObjectTypeFromExpr(baseExpr);
			const asCDecl* const field = context.GetDecl(expr->resolvedDecl);
			asCObjectProperty* const property =
				PropertyFromFieldDecl(field, objType);
			if( property == 0 )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}
			const int fieldOffset = property->byteOffset;
			const asCDataType ownerDataType =
				asCDataType::CreateType(objType, false);
			if( directGlobalValue )
				PushDirectGlobalValueObject(directGlobalValue);
			else
				PushMemberBase(base, baseExpr);
			bc.InstrSHORT_DW(asBC_ADDSi, (short)fieldOffset, 0);
			MarkLastTypeRelocation(
				ownerDataType, asTYPE_RELOC_PROPERTY, property);
			bc.Instr(asBC_PopRPtr);
			if( forStore )
			{
				return 0;
			}
			const asCDataType fieldType = TypeOf(field->type);
			const int dest = AllocTemporary(
				fieldType, dwords > 0 ? dwords : 1);
			EmitReadValue(dest, fieldType, expr, "member-ref");
			return dest;
		}

		void EmitMemberStore(const asCExpr* lhs, int src)
		{
			if( lhs->children.GetLength() == 0 )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return;
			}
			const asCExpr* baseExpr = context.GetExpr(lhs->children[0]);
			asCGlobalProperty* const directGlobalValue = FindDirectGlobalValueObject(baseExpr);
			const int base = directGlobalValue ? 0 : EmitExpr(lhs->children[0]);
			if( !ok )
				return;
			asCObjectType* objType = ObjectTypeFromExpr(baseExpr);
			const asCDecl* const field = context.GetDecl(lhs->resolvedDecl);
			asCObjectProperty* const property =
				PropertyFromFieldDecl(field, objType);
			if( property == 0 )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return;
			}
			const int fieldOffset = property->byteOffset;
			const asCDataType ownerDataType =
				asCDataType::CreateType(objType, false);
			if( directGlobalValue )
				PushDirectGlobalValueObject(directGlobalValue);
			else
				PushMemberBase(base, baseExpr);
			bc.InstrSHORT_DW(asBC_ADDSi, (short)fieldOffset, 0);
			MarkLastTypeRelocation(
				ownerDataType, asTYPE_RELOC_PROPERTY, property);
			bc.Instr(asBC_PopRPtr);
			const asCDataType fieldType = TypeOf(field->type);
			EmitWriteValue(src, fieldType);
		}

		int EmitIndex(const asCExpr* expr, const asCDataType& dataType, int dwords, bool asAddress)
		{
			// Sema retains every authored/defaulted opIndex argument. This first
			// detached ABI emitter still supports the one-argument VM form only;
			// fail closed instead of silently compiling children[1] and discarding
			// later arguments.
			if( expr->children.GetLength() != 2 )
			{
				if( failureDetail.GetLength() == 0 )
				{
					failureDetail.Format(
						"unsupported multi-argument index function=%s expr=%u children=%u",
						activeDecl && activeDecl->stableKey.AddressOf()
							? activeDecl->stableKey.AddressOf() : "<unknown>",
						expr ? expr->id.value : 0,
						expr ? expr->children.GetLength() : 0);
				}
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}
			asCDataType payloadType = dataType;
			if( payloadType.IsReference() )
			{
				payloadType.MakeReference(false);
			}
			int payloadDwords = payloadType.GetSizeInMemoryDWords();
			if( payloadDwords < 1 )
			{
				payloadDwords = 1;
			}
			int cachedPtr = FindExprSlot(indexPtrSlots, expr->id);
			if( cachedPtr == 0x7fffffff && expr->children.GetLength() >= 2 )
			{
				for( asUINT i = 0; i < indexPtrByArgs.GetLength(); ++i )
				{
					if( indexPtrByArgs[i].base == expr->children[0]
						&& indexPtrByArgs[i].index == expr->children[1] )
					{
						cachedPtr = indexPtrByArgs[i].slot;
						break;
					}
				}
			}
			if( cachedPtr != 0x7fffffff )
			{
				if( asAddress )
				{
					return cachedPtr;
				}
				const int dest = AllocTemporary(payloadType, payloadDwords);
				bc.InstrSHORT(asBC_PshVPtr, (short)cachedPtr);
				bc.Instr(asBC_PopRPtr);
				EmitReadValue(dest, payloadType, expr, "index-cached-pointer");
				return dest;
			}
			const asCExpr* baseExpr = context.GetExpr(expr->children[0]);
			asCScriptFunction* opIndex = FindFunc(expr->resolvedDecl);
			if( opIndex == 0 )
			{
				FailAt(__LINE__, asNO_FUNCTION);
				return 0;
			}
			const int base = EmitExpr(expr->children[0]);
			const int index = EmitExpr(expr->children[1]);
			if( !ok )
				return 0;
			bc.InstrSHORT(asBC_PshV4, (short)index);
			PushMemberBase(base, baseExpr);
			if( opIndex->funcType == asFUNC_SYSTEM )
				bc.Call(asBC_CALLSYS, opIndex->id, AS_PTR_SIZE + 1);
			else
				bc.Call(asBC_CALL, opIndex->id, AS_PTR_SIZE + 1);
			if( opIndex->returnType.IsReference() && !opIndex->returnType.IsObjectHandle() )
			{
				asCDataType retPayload = opIndex->returnType;
				retPayload.MakeReference(false);
				int retDwords = retPayload.GetSizeInMemoryDWords();
				if( retDwords < 1 )
				{
					retDwords = 1;
				}
				const int ptr = AllocPointerVariable();
				StoreReturn(ptr, AS_PTR_SIZE);
				BindExprSlot(indexPtrSlots, expr->id, ptr);
				asSIndexPtrMemo byArgs;
				byArgs.base = expr->children[0];
				byArgs.index = expr->children[1];
				byArgs.slot = ptr;
				indexPtrByArgs.PushLast(byArgs);
				if( asAddress )
				{
					return ptr;
				}
				const int dest = AllocTemporary(retPayload, retDwords);
				bc.InstrSHORT(asBC_PshVPtr, (short)ptr);
				bc.Instr(asBC_PopRPtr);
				EmitReadValue(dest, retPayload, expr, "index-return-reference");
				return dest;
			}
			if( asAddress )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}
			if( opIndex->returnType.IsObjectHandle() || opIndex->returnType.IsFuncdef() )
			{
				// An owning opIndex result follows the same object-register ABI as a
				// normal handle-returning call. Reading valueRegister here turns the
				// index argument/previous scalar residue into a bogus receiver pointer.
				const int dest = AllocTyped(opIndex->returnType);
				bc.InstrSHORT(asBC_STOREOBJ, (short)dest);
				bc.ObjInfo(dest, asOBJ_INIT);
				return dest;
			}
			const int dest = AllocTemporary(
				opIndex->returnType, payloadDwords);
			StoreReturn(dest, payloadDwords);
			return dest;
		}

		int EmitBinary(const asCExpr* expr, const asCDataType& dataType, int dwords)
		{
			if( expr->children.GetLength() != 2 )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}
			const int lhs = EmitExpr(expr->children[0]);
			const int rhs = EmitExpr(expr->children[1]);
			if( !ok )
				return 0;

			const char* op = expr->literal.AddressOf() ? expr->literal.AddressOf() : "";
			const asCExpr* lhsExpr = context.GetExpr(expr->children[0]);
			asCDataType lhsType = lhsExpr ? TypeOf(lhsExpr->type) : dataType;
			if( lhsType.IsReference() && !lhsType.IsObjectHandle() )
			{
				// EmitExpr has already loaded through a scalar reference. Opcode width
				// and temporary metadata therefore follow the pointee value, not the
				// pointer-sized storage used to pass the reference itself.
				lhsType.MakeReference(false);
			}
			const asCExpr* rhsExpr = context.GetExpr(expr->children[1]);
			asCDataType rhsType = rhsExpr
				? TypeOf(rhsExpr->type) : dataType;
			if( rhsType.IsReference() && !rhsType.IsObjectHandle() )
			{
				rhsType.MakeReference(false);
			}
			const bool lhsNull = lhsExpr && lhsExpr->kind == asAST_EXPR_NULL_LITERAL;
			const bool rhsNull = rhsExpr && rhsExpr->kind == asAST_EXPR_NULL_LITERAL;
			const bool identityOp = expr->literal.Equals("is")
				|| expr->literal.Equals("!is");
			const bool equalityOp = expr->literal.Equals("==")
				|| expr->literal.Equals("!=");
			const bool lhsPtr = lhsType.IsObjectHandle() || lhsType.IsFuncdef();
			const bool rhsPtr = rhsType.IsObjectHandle() || rhsType.IsFuncdef();
			// Handle/funcdef == and != are pointer identity, including this
			// fork's implicit-handle REF form (CObj without '@'). Falling
			// through to EmitCompare emits CMPi on pointer frame slots.
			if( identityOp || (equalityOp && (lhsNull || rhsNull || (lhsPtr && rhsPtr))) )
			{
				const int dest = AllocTemporary(
					asCDataType::CreatePrimitive(ttBool, false), 1);
				if( rhsNull )
				{
					bc.InstrSHORT(asBC_CmpPtrNull, (short)lhs);
				}
				else if( lhsNull )
				{
					bc.InstrSHORT(asBC_CmpPtrNull, (short)rhs);
				}
				else
				{
					bc.InstrW_W(asBC_CmpPtr, lhs, rhs);
				}
				const bool notEq = expr->literal.Equals("!is") || expr->literal.Equals("!=");
				bc.Instr(notEq ? asBC_TNZ : asBC_TZ);
				bc.InstrSHORT(asBC_CpyRtoV4, (short)dest);
				return dest;
			}
			if( op[0] == '=' || op[0] == '!' || op[0] == '<' || op[0] == '>' )
				return EmitCompare(op, lhs, rhs, lhsType);
			return EmitScalarBinaryValues(
				op, lhs, rhs, lhsType, rhsType, dwords);
		}

		int EmitScalarBinaryValues(
			const char* op,
			int lhs,
			int rhs,
			const asCDataType& lhsType,
			const asCDataType& rhsType,
			int dwords)
		{
			asEBCInstr instr = asBC_ADDi;
			if( lhsType.IsFloat64Type() )
			{
				if( std::strcmp(op, "+") == 0 ) instr = asBC_ADDd;
				else if( std::strcmp(op, "-") == 0 ) instr = asBC_SUBd;
				else if( std::strcmp(op, "*") == 0 ) instr = asBC_MULd;
				else if( std::strcmp(op, "/") == 0 ) instr = asBC_DIVd;
				else if( std::strcmp(op, "%") == 0 ) instr = asBC_MODd;
				else if( std::strcmp(op, "**") == 0 )
					instr = rhsType.IsIntegerType()
						&& rhsType.GetSizeInMemoryDWords() == 1
						? asBC_POWdi : asBC_POWd;
				else { FailAt(__LINE__, asNOT_SUPPORTED); return 0; }
			}
			else if( lhsType.IsFloat32Type() )
			{
				if( std::strcmp(op, "+") == 0 ) instr = asBC_ADDf;
				else if( std::strcmp(op, "-") == 0 ) instr = asBC_SUBf;
				else if( std::strcmp(op, "*") == 0 ) instr = asBC_MULf;
				else if( std::strcmp(op, "/") == 0 ) instr = asBC_DIVf;
				else if( std::strcmp(op, "%") == 0 ) instr = asBC_MODf;
				else if( std::strcmp(op, "**") == 0 ) instr = asBC_POWf;
				else { FailAt(__LINE__, asNOT_SUPPORTED); return 0; }
			}
			else if( SizeDwords(lhsType) >= 2 )
			{
				if( std::strcmp(op, "+") == 0 ) instr = asBC_ADDi64;
				else if( std::strcmp(op, "-") == 0 ) instr = asBC_SUBi64;
				else if( std::strcmp(op, "*") == 0 ) instr = asBC_MULi64;
				else if( std::strcmp(op, "/") == 0 ) instr = lhsType.IsUnsignedType() ? asBC_DIVu64 : asBC_DIVi64;
				else if( std::strcmp(op, "%") == 0 ) instr = lhsType.IsUnsignedType() ? asBC_MODu64 : asBC_MODi64;
				else if( std::strcmp(op, "**") == 0 ) instr = lhsType.IsUnsignedType() ? asBC_POWu64 : asBC_POWi64;
				else if( std::strcmp(op, "&") == 0 ) instr = asBC_BAND64;
				else if( std::strcmp(op, "|") == 0 ) instr = asBC_BOR64;
				else if( std::strcmp(op, "^") == 0 ) instr = asBC_BXOR64;
				else if( std::strcmp(op, "<<") == 0 ) instr = asBC_BSLL64;
				else if( std::strcmp(op, ">>") == 0 ) instr = asBC_BSRL64;
				else if( std::strcmp(op, ">>>") == 0 ) instr = asBC_BSRA64;
				else { FailAt(__LINE__, asNOT_SUPPORTED); return 0; }
			}
			else
			{
				if( std::strcmp(op, "+") == 0 ) instr = asBC_ADDi;
				else if( std::strcmp(op, "-") == 0 ) instr = asBC_SUBi;
				else if( std::strcmp(op, "*") == 0 ) instr = asBC_MULi;
				else if( std::strcmp(op, "/") == 0 ) instr = lhsType.IsUnsignedType() ? asBC_DIVu : asBC_DIVi;
				else if( std::strcmp(op, "%") == 0 ) instr = lhsType.IsUnsignedType() ? asBC_MODu : asBC_MODi;
				else if( std::strcmp(op, "**") == 0 ) instr = lhsType.IsUnsignedType() ? asBC_POWu : asBC_POWi;
				else if( std::strcmp(op, "&") == 0 ) instr = asBC_BAND;
				else if( std::strcmp(op, "|") == 0 ) instr = asBC_BOR;
				else if( std::strcmp(op, "^") == 0 ) instr = asBC_BXOR;
				else if( std::strcmp(op, "<<") == 0 ) instr = asBC_BSLL;
				else if( std::strcmp(op, ">>") == 0 ) instr = asBC_BSRL;
				else if( std::strcmp(op, ">>>") == 0 ) instr = asBC_BSRA;
				else { FailAt(__LINE__, asNOT_SUPPORTED); return 0; }
			}

			const int dest = AllocTemporary(
				lhsType, dwords > 0 ? dwords : 1);
			bc.InstrW_W_W(instr, dest, lhs, rhs);
			return dest;
		}

		int EmitCompare(const char* op, int lhs, int rhs, const asCDataType& operandType)
		{
			asEBCInstr test = asBC_TZ;
			if( op[0] == '=' && op[1] == '=' ) test = asBC_TZ;
			else if( op[0] == '!' && op[1] == '=' ) test = asBC_TNZ;
			else if( op[0] == '<' && op[1] == '=' ) test = asBC_TNP;
			else if( op[0] == '>' && op[1] == '=' ) test = asBC_TNS;
			else if( op[0] == '<' ) test = asBC_TS;
			else if( op[0] == '>' ) test = asBC_TP;
			else
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}
			const int dest = AllocTemporary(
				asCDataType::CreatePrimitive(ttBool, false), 1);
			asEBCInstr compare = asBC_CMPi;
			if( operandType.IsIntegerType() && SizeDwords(operandType) == 1 )
				compare = asBC_CMPi;
			else if( operandType.IsUnsignedType() && SizeDwords(operandType) == 1 )
				compare = asBC_CMPu;
			else if( operandType.IsIntegerType() && SizeDwords(operandType) == 2 )
				compare = asBC_CMPi64;
			else if( operandType.IsUnsignedType() && SizeDwords(operandType) == 2 )
				compare = asBC_CMPu64;
			else if( operandType.IsFloat32Type() )
				compare = asBC_CMPf;
			else if( operandType.IsFloat64Type() )
				compare = asBC_CMPd;
			bc.InstrW_W(compare, lhs, rhs);
			bc.Instr(test);
			bc.InstrSHORT(asBC_CpyRtoV4, (short)dest);
			return dest;
		}

		int EmitLogical(const asCExpr* expr)
		{
			if( expr->children.GetLength() != 2 )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}
			const bool isAnd = expr->literal.Equals("&&");
			const int lhs = EmitExpr(expr->children[0]);
			if( !ok )
				return 0;
			const int dest = AllocTemporary(
				asCDataType::CreatePrimitive(ttBool, false), 1);
			CopyVar(dest, lhs, 1);
			const int skip = nextLabel++;
			EmitBoolJump(dest, isAnd ? asBC_JLowZ : asBC_JLowNZ, skip);
			const int rhs = EmitExpr(expr->children[1]);
			if( !ok )
				return 0;
			CopyVar(dest, rhs, 1);
			bc.Label((short)skip);
			return dest;
		}

		int EmitConditional(const asCExpr* expr, const asCDataType& dataType, int dwords)
		{
			if( expr->children.GetLength() != 3 )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}
			const int cond = EmitExpr(expr->children[0]);
			if( !ok )
				return 0;
			const int elseLabel = nextLabel++;
			const int endLabel = nextLabel++;
			const int dest = AllocTemporary(
				dataType, dwords > 0 ? dwords : 1);
			EmitBoolJump(cond, asBC_JLowZ, elseLabel);
			const int thenV = EmitExpr(expr->children[1]);
			CopyVar(dest, thenV, dwords > 0 ? dwords : 1);
			bc.InstrINT(asBC_JMP, endLabel);
			bc.Label((short)elseLabel);
			const int elseV = EmitExpr(expr->children[2]);
			CopyVar(dest, elseV, dwords > 0 ? dwords : 1);
			bc.Label((short)endLabel);
			return dest;
		}

		int EmitConversion(const asCExpr* expr, const asCDataType& destType, int dwords)
		{
			if( expr->children.GetLength() != 1 )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}
			if( asCScriptFunction* const conv = FindFunc(expr->resolvedDecl) )
			{
				if( conv->name.Equals("opImplConv")
					&& conv->parameterTypes.GetLength() == 0 )
				{
					const asCExpr* const innerExpr =
						context.GetExpr(expr->children[0]);
					asCDataType srcType = innerExpr
						? TypeOf(innerExpr->type) : destType;
					int recvOffset = 0x7fffffff;
					bool recvOffsetStoresPointer = false;
					if( srcType.IsNonPrimitiveValueType()
						&& !srcType.IsObjectHandle() )
					{
						recvOffset = EmitLValueAddress(expr->children[0]);
						recvOffsetStoresPointer = true;
					}
					else
					{
						recvOffset = EmitExpr(expr->children[0]);
					}
					if( !ok )
						return 0;
					if( recvOffsetStoresPointer
						|| (conv->objectType
							&& (conv->objectType->GetFlags() & asOBJ_VALUE) == 0) )
						bc.InstrSHORT(asBC_PshVPtr, (short)recvOffset);
					else
						bc.InstrSHORT(asBC_PSF, (short)recvOffset);
					bc.Call(
						conv->funcType == asFUNC_SYSTEM ? asBC_CALLSYS : asBC_CALL,
						conv->id,
						AS_PTR_SIZE);
					if( destType.IsObjectHandle() || destType.IsFuncdef() )
					{
						const int dest = AllocTyped(destType);
						bc.InstrSHORT(asBC_STOREOBJ, (short)dest);
						bc.ObjInfo(dest, asOBJ_INIT);
						return dest;
					}
					FailUnsupportedConversion(expr, srcType, destType, __LINE__);
					return 0;
				}
			}
			const int inner = EmitExpr(expr->children[0]);
			if( !ok )
				return 0;
			const asCExpr* innerExpr = context.GetExpr(expr->children[0]);
			asCDataType srcType = innerExpr ? TypeOf(innerExpr->type) : destType;
			if( srcType.IsEqualExceptRefAndConst(destType) )
				return inner;
			// int and uint occupy the same single VM dword. Sema has already
			// selected the destination type, so this is a representation-preserving
			// conversion rather than arithmetic: preserve the slot and let later
			// operations choose signed or unsigned bytecode from their sealed type.
			if( (srcType.IsIntegerType() || srcType.IsUnsignedType())
				&& (destType.IsIntegerType() || destType.IsUnsignedType())
				&& srcType.GetSizeInMemoryBytes() == 4 && destType.GetSizeInMemoryBytes() == 4 )
			{
				return inner;
			}
			if( destType.IsFuncdef() || destType.IsObjectHandle()
				|| srcType.IsFuncdef() || srcType.IsObjectHandle() )
			{
				// A VALUE object is not a handle. Sema ranks `opImplConv` for
				// that conversion and seals the selected method on the node.
				// Passing the VALUE slot as a handle is never an identity.
				if( srcType.IsNonPrimitiveValueType() && !srcType.IsObjectHandle()
					&& destType.IsObjectHandle()
					&& !srcType.IsEqualExceptRefAndConst(destType) )
				{
					FailUnsupportedConversion(expr, srcType, destType, __LINE__);
					return 0;
				}
				return inner;
			}

			auto NumericRepresentation = [](const asCDataType& nominalType)
			{
				asCDataType representation = nominalType;
				asCTypeInfo* typeInfo = representation.GetTypeInfo();
				if( typeInfo && (typeInfo->flags & asOBJ_TYPEDEF) )
				{
					// Preserve the typedef in Canonical AST and published signatures,
					// but select VM opcodes from its aliased scalar representation.
					representation = CastToTypedefType(typeInfo)->aliasForType;
				}
				return representation;
			};
			const asCDataType sourceValueType = NumericRepresentation(srcType);
			const asCDataType targetValueType = NumericRepresentation(destType);
			int value = inner;
			int sourceBytes = sourceValueType.GetSizeInMemoryBytes();
			const int targetBytes = targetValueType.GetSizeInMemoryBytes();
			const bool sourceSigned = sourceValueType.IsIntegerType();
			const bool sourceUnsigned = sourceValueType.IsUnsignedType();
			const bool targetSigned = targetValueType.IsIntegerType();
			const bool targetUnsigned = targetValueType.IsUnsignedType();
			const bool sourceInteger = sourceSigned || sourceUnsigned;
			const bool targetInteger = targetSigned || targetUnsigned;

			auto EmitUnary = [&](asEBCInstr instruction, int resultDwords)
			{
				eTokenType resultToken = resultDwords >= 2 ? ttUInt64 : ttUInt;
				switch( instruction )
				{
				case asBC_iTOb:
					resultToken = ttUInt8;
					break;
				case asBC_iTOw:
					resultToken = ttUInt16;
					break;
				case asBC_u64TOf:
				case asBC_i64TOf:
				case asBC_uTOf:
				case asBC_iTOf:
				case asBC_dTOf:
					// ScriptFunctionData::temporaryVariables is a Runtime ABI
					// directory, so publish the normalized storage token rather
					// than the source-spelling keyword token. BytecodeJIT uses
					// ttFloat32 to distinguish this dword from an integer slot.
					resultToken = ttFloat32;
					break;
				case asBC_u64TOd:
				case asBC_i64TOd:
				case asBC_uTOd:
				case asBC_iTOd:
				case asBC_fTOd:
					resultToken = ttFloat64;
					break;
				default:
					break;
				}
				const asCDataType resultType =
					asCDataType::CreatePrimitive(resultToken, false);
				const int result = AllocTemporary(
					resultType, resultDwords > 0 ? resultDwords : 1);
				bc.InstrW_W(instruction, result, value);
				value = result;
			};

			// The VM stores every sub-dword scalar in a normal dword slot. Match
			// LEGACY's numeric conversion staging by first extending a compact
			// source whenever a later conversion consumes a 32/64-bit value.
			if( sourceInteger && sourceBytes < 4
				&& (targetBytes >= 4 || targetValueType.IsFloat32Type() || targetValueType.IsFloat64Type()) )
			{
				if( sourceBytes == 1 )
					EmitUnary(sourceUnsigned ? asBC_ubTOi : asBC_sbTOi, 1);
				else if( sourceBytes == 2 )
					EmitUnary(sourceUnsigned ? asBC_uwTOi : asBC_swTOi, 1);
				else
				{
					FailUnsupportedConversion(expr, srcType, destType, __LINE__);
					return 0;
				}
				sourceBytes = 4;
			}

			if( sourceInteger && targetInteger )
			{
				if( targetBytes < 4 )
				{
					if( sourceBytes == 8 )
					{
						EmitUnary(asBC_i64TOi, 1);
						sourceBytes = 4;
					}
					if( targetBytes == 1 )
						EmitUnary(asBC_iTOb, 1);
					else if( targetBytes == 2 )
						EmitUnary(asBC_iTOw, 1);
					else
					{
						FailUnsupportedConversion(expr, srcType, destType, __LINE__);
						return 0;
					}
				}
				else if( targetBytes == 4 )
				{
					if( sourceBytes == 8 )
						EmitUnary(asBC_i64TOi, 1);
					// All other integer sources already have their correct dword
					// representation after the compact-source normalization above.
				}
				else if( targetBytes == 8 )
				{
					if( sourceBytes < 8 )
						EmitUnary(sourceUnsigned ? asBC_uTOi64 : asBC_iTOi64, 2);
				}
				else
				{
					FailUnsupportedConversion(expr, srcType, destType, __LINE__);
					return 0;
				}
			}
			else if( sourceInteger && targetValueType.IsFloat32Type() )
			{
				if( sourceBytes == 8 )
					EmitUnary(sourceUnsigned ? asBC_u64TOf : asBC_i64TOf, 1);
				else
					EmitUnary(sourceUnsigned ? asBC_uTOf : asBC_iTOf, 1);
			}
			else if( sourceInteger && targetValueType.IsFloat64Type() )
			{
				if( sourceBytes == 8 )
					EmitUnary(sourceUnsigned ? asBC_u64TOd : asBC_i64TOd, 2);
				else
					EmitUnary(sourceUnsigned ? asBC_uTOd : asBC_iTOd, 2);
			}
			else if( (sourceValueType.IsFloat32Type() || sourceValueType.IsFloat64Type()) && targetInteger )
			{
				if( targetBytes == 8 )
				{
					if( sourceValueType.IsFloat32Type() )
						EmitUnary(targetUnsigned ? asBC_fTOu64 : asBC_fTOi64, 2);
					else
						EmitUnary(targetUnsigned ? asBC_dTOu64 : asBC_dTOi64, 2);
				}
				else
				{
					if( sourceValueType.IsFloat32Type() )
						EmitUnary(targetUnsigned ? asBC_fTOu : asBC_fTOi, 1);
					else
						EmitUnary(targetUnsigned ? asBC_dTOu : asBC_dTOi, 1);
					if( targetBytes == 1 )
						EmitUnary(asBC_iTOb, 1);
					else if( targetBytes == 2 )
						EmitUnary(asBC_iTOw, 1);
					else if( targetBytes != 4 )
					{
						FailUnsupportedConversion(expr, srcType, destType, __LINE__);
						return 0;
					}
				}
			}
			else if( sourceValueType.IsFloat32Type() && targetValueType.IsFloat64Type() )
				EmitUnary(asBC_fTOd, 2);
			else if( sourceValueType.IsFloat64Type() && targetValueType.IsFloat32Type() )
				EmitUnary(asBC_dTOf, 1);
			else
			{
				FailUnsupportedConversion(expr, srcType, destType, __LINE__);
				return 0;
			}

			if( destType.IsEnumType() )
			{
				// Enum payloads use compact signed storage in this fork, but every
				// Canonical expression/local slot uses the normal VM dword value
				// representation. Narrowing alone zero-fills the upper bytes
				// (iTOb/iTOw), which turns -1 into 255 before the next arithmetic
				// operation. Re-normalize the compact result to the signed dword
				// representation expected by enum constants and enum-to-int casts.
				// The compact source and dword result must use distinct offsets:
				// StaticJIT and RuntimeJIT require one stable representation per slot.
				const int enumBytes = destType.GetSizeInMemoryBytes();
				if( enumBytes == 1 )
				{
					const int normalized = AllocTemporary(
						asCDataType::CreatePrimitive(ttInt, false), 1);
					bc.InstrW_W(asBC_sbTOi, normalized, value);
					value = normalized;
				}
				else if( enumBytes == 2 )
				{
					const int normalized = AllocTemporary(
						asCDataType::CreatePrimitive(ttInt, false), 1);
					bc.InstrW_W(asBC_swTOi, normalized, value);
					value = normalized;
				}
			}
			return value;
		}

		int EmitCall(const asCExpr* expr, const asCDataType& dataType, int dwords)
		{
			asCScriptFunction* callee = FindFunc(expr->resolvedDecl);
			const int funcVar = callee ? 0x7fffffff : FindSlot(expr->resolvedDecl);
			if( callee == 0 && funcVar == 0x7fffffff )
			{
				if( failureDetail.GetLength() == 0 )
				{
					const asCDecl* const target =
						context.GetDecl(expr->resolvedDecl);
					failureDetail.Format(
						"missing callable relocation function=%s expr=%u resolvedDecl=%u targetKind=%d targetName=%s targetKey=%s targetOrigin=%s receiver=%u args=%u",
						activeDecl && activeDecl->stableKey.AddressOf()
							? activeDecl->stableKey.AddressOf() : "<unknown>",
						expr ? expr->id.value : 0,
						expr ? expr->resolvedDecl.value : 0,
						target ? (int)target->kind : (int)asAST_DECL_INVALID,
						target && target->name.AddressOf()
							? target->name.AddressOf() : "<missing>",
						target && target->stableKey.AddressOf()
							? target->stableKey.AddressOf() : "<missing>",
						target && target->origin.AddressOf()
							? target->origin.AddressOf() : "",
						expr ? expr->receiver.value : 0,
						expr ? expr->children.GetLength() : 0);
				}
				FailAt(__LINE__, asNO_FUNCTION);
				return 0;
			}

			asCArray<int> argOffsets;
			asCArray<asUINT> argExprIds;
			asCArray<bool> argOffsetsStorePointer;
			// Runtime wildcard formals (`?`) carry a hidden public TypeId immediately
			// before the ordinary argument. Keep the actual canonical datatype beside
			// the detached argument plan; the numeric projection is emitted as a
			// relocation and is resolved only at candidate installation.
			asCArray<asCDataType> argPublicTypeIdTypes;
			// A property passed to primitive &out is not a normal lvalue address.
			// Sema seals a DEFERRED_OUT call-edge plan containing the exact setter
			// and an opaque receiver.  Capture both the receiver address and a
			// temporary value while evaluating arguments, then execute the setter
			// only after the primary call returns.  The arrays preserve the AST's
			// reverse-formal order, matching the maintained compiler contract.
			asCArray<const asCExpr*> deferredOutPlans;
			asCArray<int> deferredOutValueOffsets;
			asCArray<int> deferredOutReceiverOffsets;
			asCArray<const asCDecl*> sealedFormalParams;
			if( const asCDecl* const sealedCallee =
				context.GetDecl(expr->resolvedDecl) )
			{
				for( asUINT i = 0; i < sealedCallee->children.GetLength(); ++i )
				{
					const asCDecl* const child =
						context.GetDecl(sealedCallee->children[i]);
					if( child != 0 && child->kind == asAST_DECL_PARAM )
					{
						sealedFormalParams.PushLast(child);
					}
				}
			}
			const bool hasImplicitReceiverChild = callee != 0
				&& callee->objectType != 0
				&& expr->receiver.IsValid()
				&& expr->children.GetLength()
					== callee->parameterTypes.GetLength() + 1
				&& expr->children[expr->children.GetLength() - 1]
					== expr->receiver;
			if( callee != 0 && callee->objectType != 0
				&& expr->receiver.IsValid() && expr->children.GetLength() > 0
				&& expr->children[expr->children.GetLength() - 1] == expr->receiver
				&& !hasImplicitReceiverChild )
			{
				failureDetail.Format(
					"receiver child/runtime arity mismatch function=%s callExpr=%u "
					"callee=%s runtimeParams=%u sealedParams=%u hiddenIndex=%d "
					"children=%u receiver=%u",
					activeDecl && activeDecl->stableKey.AddressOf()
						? activeDecl->stableKey.AddressOf() : "<unknown>",
					expr->id.value,
					callee->GetDeclaration(false, false, true),
					callee->parameterTypes.GetLength(),
					sealedFormalParams.GetLength(),
					callee->hiddenArgumentIndex,
					expr->children.GetLength(),
					expr->receiver.value);
				FailAt(__LINE__, asNOT_SUPPORTED);
				return 0;
			}
			for( asUINT i = 0; i < expr->children.GetLength(); ++i )
			{
				// Canonical method calls keep the effective receiver in the dedicated
				// receiver edge and as one trailing implicit reverse-formal child.  It
				// is not a Runtime parameter.  Evaluate it below through the receiver
				// route so VALUE objects yield their address rather than a scalar load.
				if( hasImplicitReceiverChild
					&& i + 1 == expr->children.GetLength() )
				{
					continue;
				}
				const asCExpr* const argument =
					context.GetExpr(expr->children[i]);
				const asCDecl* const sealedFormal =
					i < sealedFormalParams.GetLength()
						? sealedFormalParams[sealedFormalParams.GetLength() - 1 - i]
						: 0;
				const asUINT runtimeFormalIndex =
					callee != 0 && i < callee->parameterTypes.GetLength()
						? callee->parameterTypes.GetLength() - 1 - i
						: asUINT(-1);
				const asCDataType* const runtimeFormal =
					runtimeFormalIndex != asUINT(-1)
						? &callee->parameterTypes[runtimeFormalIndex] : 0;
				const bool runtimeFormalIsReference = runtimeFormal != 0
					&& (runtimeFormal->IsReference()
						|| (runtimeFormalIndex < callee->inOutFlags.GetLength()
							&& callee->inOutFlags[runtimeFormalIndex] != asTM_NONE));
				const asCDataType sealedFormalType = sealedFormal != 0
					? TypeOf(sealedFormal->type) : asCDataType();
				asCDataType sealedFormalValueType = sealedFormalType;
				if( sealedFormalValueType.IsReference()
					&& !sealedFormalValueType.IsObjectHandle() )
				{
					sealedFormalValueType.MakeReference(false);
				}
				asCDataType runtimeFormalValueType = runtimeFormal != 0
					? *runtimeFormal : asCDataType();
				if( runtimeFormalValueType.IsReference()
					&& !runtimeFormalValueType.IsObjectHandle() )
				{
					runtimeFormalValueType.MakeReference(false);
				}
				const bool isStringLiteral = argument != 0
					&& argument->kind == asAST_EXPR_STRING_LITERAL;
				const bool isDeferredOut = argument != 0
					&& argument->kind == asAST_EXPR_DEFERRED_OUT;
				if( isDeferredOut )
				{
					const asCType* const planType =
						context.GetType(argument->type.type);
					const asCScriptFunction* const setter =
						FindFunc(argument->resolvedDecl);
					const bool runtimeIsOut = callee != 0
						&& runtimeFormalIndex < callee->inOutFlags.GetLength()
						&& callee->inOutFlags[runtimeFormalIndex] == asTM_OUTREF;
					if( sealedFormal == 0 || runtimeFormal == 0
						|| planType == 0
						|| planType->kind != asAST_TYPE_PRIMITIVE
						|| argument->type != sealedFormal->type
						|| !argument->type.IsReference()
						|| (argument->type.quals & asAST_QUAL_OUT) == 0
						|| (argument->type.quals & asAST_QUAL_IN) != 0
						|| !runtimeFormalIsReference || !runtimeIsOut
						|| !sealedFormalValueType.IsPrimitive()
						|| !runtimeFormalValueType.IsPrimitive()
						|| !sealedFormalValueType.IsEqualExceptRef(
								runtimeFormalValueType)
						|| setter == 0 || setter->objectType == 0
						|| setter->parameterTypes.GetLength() != 1
						|| setter->inOutFlags.GetLength() != 1
						|| setter->inOutFlags[0] != asTM_NONE
						|| !setter->parameterTypes[0].IsEqualExceptRef(
								sealedFormalValueType)
						|| setter->returnType.GetTokenType() != ttVoid
						|| (setter->funcType != asFUNC_SCRIPT
							&& setter->funcType != asFUNC_SYSTEM)
						|| !argument->receiver.IsValid() )
					{
						if( failureDetail.GetLength() == 0 )
						{
							failureDetail.Format(
								"invalid deferred-out ABI function=%s call=%u child=%u "
								"plan=%u setter=%u runtimeFormal=%u",
								activeDecl && activeDecl->stableKey.AddressOf()
									? activeDecl->stableKey.AddressOf() : "<unknown>",
								expr ? expr->id.value : 0,
								argument->id.value,
								argument->resolvedDecl.value,
								setter ? setter->id : 0,
								runtimeFormalIndex);
						}
						FailAt(__LINE__, asNOT_SUPPORTED);
						return 0;
					}
				}
				const bool sealedRequiresValueAddress = sealedFormal != 0
					&& sealedFormal->type.IsReference()
					&& sealedFormalValueType.IsNonPrimitiveValueType()
					&& !sealedFormalValueType.IsObjectHandle();
				// Native projections do not always carry authored parameter children.
				// In that case the Runtime shell is still the authoritative ABI owner:
				// a VALUE-object reference argument must pass the existing lvalue address,
				// not EmitExpr() a pointer-width slice of the object into a scalar temp.
				const bool runtimeRequiresValueAddress = sealedFormal == 0
					&& runtimeFormalIsReference
					&& runtimeFormalValueType.IsNonPrimitiveValueType()
					&& !runtimeFormalValueType.IsObjectHandle();
				const bool requiresValueAddress = sealedRequiresValueAddress
					|| runtimeRequiresValueAddress;
				asCDataType byValueFormalType;
				if( sealedFormal != 0 && IsByValueObject(sealedFormalType) )
				{
					byValueFormalType = sealedFormalType;
				}
				else if( runtimeFormal != 0 && IsByValueObject(*runtimeFormal) )
				{
					byValueFormalType = *runtimeFormal;
				}
				const bool requiresValueCopy = byValueFormalType.IsValid()
					&& CanEmitSealedStorageCopy(byValueFormalType);
				const bool requiresOwnedValueTransfer = byValueFormalType.IsValid()
					&& !CanEmitSealedStorageCopy(byValueFormalType);
				asCDataType argumentValueType = argument != 0
					? TypeOf(argument->type) : asCDataType();
				if( argumentValueType.IsReference()
					&& !argumentValueType.IsObjectHandle() )
				{
					argumentValueType.MakeReference(false);
				}
				const bool argumentIsValueObject =
					argumentValueType.IsNonPrimitiveValueType()
					&& !argumentValueType.IsObjectHandle();
				const bool sealedFormalIsObjectRef = sealedFormal != 0
					&& sealedFormal->type.IsReference()
					&& !sealedFormalValueType.IsObjectHandle();
				// Generic/wildcard formals such as const ?& are not themselves
				// value objects. The argument still occupies a VALUE slot and
				// must pass its lvalue address, matching generated delegate
				// Execute(Payload) wrappers.
				const bool requiresWildcardValueAddress = argumentIsValueObject
					&& (sealedFormalIsObjectRef
						|| (sealedFormal == 0 && runtimeFormalIsReference));
				if( !isStringLiteral && argumentIsValueObject
					&& !requiresValueAddress && !requiresValueCopy
					&& !requiresOwnedValueTransfer
					&& !requiresWildcardValueAddress )
				{
					if( failureDetail.GetLength() == 0 )
					{
						const asCString argumentName = argumentValueType.Format(0);
						const asCString sealedName = sealedFormalType.IsValid()
							? sealedFormalType.Format(0) : asCString("<missing>");
						const asCString runtimeName = runtimeFormal != 0
							? runtimeFormal->Format(0) : asCString("<missing>");
						failureDetail.Format(
							"unclassified value-object call argument function=%s call=%u "
							"child=%u reverseIndex=%u argumentType=%s argumentRef=%d "
							"sealedFormal=%s sealedRef=%d runtimeFormal=%s "
							"runtimeRef=%d runtimeInOut=%d",
							activeDecl && activeDecl->stableKey.AddressOf()
								? activeDecl->stableKey.AddressOf() : "<unknown>",
							expr ? expr->id.value : 0,
							argument ? argument->id.value : 0,
							i,
							argumentName.AddressOf(),
							argument && argument->type.IsReference() ? 1 : 0,
							sealedName.AddressOf(),
							sealedFormal && sealedFormal->type.IsReference() ? 1 : 0,
							runtimeName.AddressOf(),
							runtimeFormal != 0 && runtimeFormal->IsReference() ? 1 : 0,
							callee != 0
								&& runtimeFormalIndex < callee->inOutFlags.GetLength()
								? static_cast<int>(callee->inOutFlags[runtimeFormalIndex])
								: -1);
					}
					FailAt(__LINE__, asNOT_SUPPORTED);
					return 0;
				}
				const bool storesPointer = isStringLiteral
					|| requiresValueAddress || requiresOwnedValueTransfer
					|| requiresWildcardValueAddress;
				int argumentOffset = 0x7fffffff;
				if( isDeferredOut )
				{
					asCDataType valueType = sealedFormalType;
					valueType.MakeReference(false);
					argumentOffset = AllocTemporary(
						valueType, SizeDwords(valueType));
					const int receiverOffset =
						EmitLValueAddress(argument->receiver);
					if( !ok )
						return 0;
					deferredOutPlans.PushLast(argument);
					deferredOutValueOffsets.PushLast(argumentOffset);
					deferredOutReceiverOffsets.PushLast(receiverOffset);
				}
				else if( requiresOwnedValueTransfer )
				{
					if( !EmitOwnedNonPodValueArgument(
						expr->children[i], byValueFormalType, argumentOffset) )
					{
						return 0;
					}
				}
				else if( requiresValueCopy )
				{
					const int sourcePointer =
						EmitLValueAddress(expr->children[i]);
					if( !ok )
					{
						return 0;
					}
					argumentOffset = AllocTyped(byValueFormalType);
					CopySealedValueFromPointer(
						sourcePointer, argumentOffset, byValueFormalType);
					if( !ok )
					{
						return 0;
					}
				}
				else
				{
					argumentOffset = isStringLiteral
						? EmitStringLiteralAddress(argument)
						: ((requiresValueAddress || requiresWildcardValueAddress)
							? EmitLValueAddress(expr->children[i])
							: EmitExpr(expr->children[i]));
				}
				argOffsets.PushLast(argumentOffset);
				argExprIds.PushLast(expr->children[i].value);
				argOffsetsStorePointer.PushLast(storesPointer);
				argPublicTypeIdTypes.PushLast(
					runtimeFormal != 0
						&& runtimeFormal->GetTokenType() == ttQuestion
						? argumentValueType : asCDataType());
			}
			if( !ok )
				return 0;

			int pop = 0;
			int returnDest = 0x7fffffff;
			const bool returnsOnStack = callee && callee->DoesReturnOnStack();
			if( returnsOnStack )
			{
				// A return-on-stack call constructs its result directly in the
				// caller-owned hidden-return slot; no storage copy happens here.  POD
				// and TRIVIAL_STORAGE_COPY are therefore irrelevant to this boundary.
				// Requiring them rejected normal non-POD native values such as the
				// FString returned by opAdd.  Keep the actual safety condition: the
				// Runtime ABI and sealed result must describe one by-value object. The
				// typed slot records its destructor/liveness and is marked initialized
				// only after the call returns.
				if( !IsByValueObject(callee->returnType) )
				{
					if( failureDetail.GetLength() == 0 )
					{
						failureDetail.Format(
							"invalid hidden-return ABI function=%s call=%u callee=%s return=%s",
							activeDecl && activeDecl->stableKey.AddressOf()
								? activeDecl->stableKey.AddressOf() : "<unknown>",
							expr ? expr->id.value : 0,
							callee->GetDeclaration(false, false, true),
							callee->returnType.Format(0).AddressOf());
					}
					FailAt(__LINE__, asNOT_SUPPORTED);
					return 0;
				}
				// Allocate now, but push its address only after ordinary parameters so
				// the VM entry layout is receiver, hidden return pointer, parameters.
				returnDest = AllocTyped(callee->returnType);
			}
			int recvOffset = 0x7fffffff;
			bool recvOffsetStoresPointer = false;
			if( callee && callee->objectType )
			{
				const asUINT recvId = expr->receiver.value;
				bool reused = false;
				if( recvId )
				{
					for( asUINT i = 0; i < argExprIds.GetLength(); ++i )
					{
						if( argExprIds[i] == recvId )
						{
							recvOffset = argOffsets[i];
							recvOffsetStoresPointer =
								i < argOffsetsStorePointer.GetLength()
								&& argOffsetsStorePointer[i];
							reused = true;
							break;
						}
					}
				}
				if( !reused && recvId )
				{
					const asCExpr* const receiverExpr =
						context.GetExpr(expr->receiver);
					if( receiverExpr == 0 )
					{
						FailAt(__LINE__, asAST_VERIFY_DANGLING_ID);
						return 0;
					}
					if( (callee->objectType->GetFlags() & asOBJ_VALUE) != 0 )
					{
						// A native or script VALUE method receives the address of the
						// already-live object.  The sealed receiver expression owns the
						// exact field/local identity and offset; never load or copy the
						// complete value merely to manufacture an object pointer.
						recvOffset = EmitLValueAddress(expr->receiver);
						recvOffsetStoresPointer = true;
					}
					else
					{
						recvOffset = EmitExpr(expr->receiver);
					}
				}
				else if( !reused && argOffsets.GetLength() == callee->parameterTypes.GetLength() + 1 )
				{
					recvOffset = argOffsets[argOffsets.GetLength() - 1];
				}
			}
			const asSCodeGenFuncBind* calleeBind = FindBind(callee);
			const asUINT captureCount = calleeBind ? calleeBind->captureCount : 0;
			const asUINT paramCount = callee ? callee->parameterTypes.GetLength() : argOffsets.GetLength();
			const asUINT userParamCount = paramCount > captureCount ? paramCount - captureCount : 0;
			const asUINT storedParams = (recvOffset != 0x7fffffff)
				? paramCount
				: ((captureCount > 0) ? paramCount : argOffsets.GetLength());
			asCArray<int> transferredOwnedArguments;
			// ActOnCall stores children in reverse-formal order. Push that
			// stored order so the last formal is deepest on the stack.
			// Hidden lambda captures are extra formals after the user params.
			for( asUINT i = 0; i < storedParams; ++i )
			{
				int argDwords = 1;
				bool byRef = false;
				bool srcStoresPointer = false;
				int src = 0x7fffffff;
				asUINT actualArgumentIndex = asUINT(-1);
				const asCDataType* formalType = 0;
				asCDataType sealedSourceFormalType;
				const asCDataType* valueFormalType = 0;
				if( callee && paramCount > i )
				{
					const asUINT formal = paramCount - 1 - i;
					const asCDataType& paramType = callee->parameterTypes[formal];
					formalType = &paramType;
					// Builder's public shell represents a source `T` VALUE
					// parameter as `const T&inout`. Call semantics must still
					// come from the sealed source declaration: explicit `T&`
					// aliases the caller, whereas source `T` copies (and for a
					// non-POD value transfers callee ownership). Do not infer
					// this distinction from the normalized Runtime datatype.
					if( formal < sealedFormalParams.GetLength() )
					{
						sealedSourceFormalType = TypeOf(
							sealedFormalParams[formal]->type);
						if( IsByValueObject(sealedSourceFormalType) )
						{
							valueFormalType = &sealedSourceFormalType;
						}
					}
					if( valueFormalType == 0 && IsByValueObject(paramType) )
					{
						valueFormalType = formalType;
					}
					byRef = paramType.IsReference();
					argDwords = SizeDwords(paramType);
					if( captureCount > 0 && formal >= userParamCount )
					{
						src = FindSlot(captureDecls[calleeBind->captureBegin + (formal - userParamCount)]);
					}
					else if( captureCount > 0 )
					{
						const asUINT userRev = userParamCount - 1 - formal;
						src = userRev < argOffsets.GetLength() ? argOffsets[userRev] : 0x7fffffff;
						actualArgumentIndex = userRev;
						srcStoresPointer = userRev < argOffsetsStorePointer.GetLength()
							&& argOffsetsStorePointer[userRev];
					}
					else
					{
						src = argOffsets[i];
						actualArgumentIndex = i;
						srcStoresPointer = i < argOffsetsStorePointer.GetLength()
							&& argOffsetsStorePointer[i];
					}
				}
				else
				{
					src = argOffsets[i];
					actualArgumentIndex = i;
					srcStoresPointer = i < argOffsetsStorePointer.GetLength()
						&& argOffsetsStorePointer[i];
				}
				if( src == 0x7fffffff )
				{
					FailAt(__LINE__, asNOT_SUPPORTED);
					return 0;
				}
				if( formalType != 0
					&& formalType->GetTokenType() == ttQuestion )
				{
					if( actualArgumentIndex >= argPublicTypeIdTypes.GetLength()
						|| !argPublicTypeIdTypes[actualArgumentIndex].IsValid() )
					{
						FailAt(__LINE__, asINVALID_TYPE);
						return 0;
					}
					// Match the maintained VM ABI: TYPEID is pushed before the
					// wildcard value/reference. The placeholder must never be observed;
					// ResolveAndPatchTypeRelocations late-projects the Engine-local id.
					bc.InstrDWORD(asBC_TYPEID, 0);
					if( !MarkLastTypeRelocation(
						argPublicTypeIdTypes[actualArgumentIndex],
						asTYPE_RELOC_PUBLIC_TYPE_ID) )
					{
						return 0;
					}
					pop += 1;
				}
				if( valueFormalType != 0 )
				{
					if( callee == 0 )
					{
						FailAt(__LINE__, asNOT_SUPPORTED);
						return 0;
					}
					if( srcStoresPointer )
					{
						if( CanEmitSealedStorageCopy(*valueFormalType)
							|| (callee->funcType != asFUNC_SYSTEM
								&& callee->funcType != asFUNC_SCRIPT) )
						{
							FailAt(__LINE__, asNOT_SUPPORTED);
							return 0;
						}
						bc.InstrSHORT(asBC_PshVPtr, (short)src);
						transferredOwnedArguments.PushLast(src);
					}
					else if( callee->funcType == asFUNC_SYSTEM )
					{
						int pointerSlot = 0x7fffffff;
						if( !EmitOwnedSealedValueArgument(
							src, *valueFormalType, pointerSlot) )
						{
							return 0;
						}
						transferredOwnedArguments.PushLast(pointerSlot);
					}
					else if( callee->funcType == asFUNC_SCRIPT )
					{
						if( !EmitSealedValueArgument(src, *valueFormalType) )
						{
							return 0;
						}
					}
					else
					{
						// Imported/indirect targets do not seal whether the
						// eventual callee consumes ownership. Fail closed until
						// that route fact is present in the canonical call plan.
						FailAt(__LINE__, asNOT_SUPPORTED);
						return 0;
					}
					pop += AS_PTR_SIZE;
				}
				else if( byRef )
				{
					bc.InstrSHORT(srcStoresPointer ? asBC_PshVPtr : asBC_PSF,
						(short)src);
					pop += AS_PTR_SIZE;
				}
				else
				{
					if( srcStoresPointer )
					{
						// String factories and other pointer identities need a
						// reference formal. Never reinterpret pointer bits as a scalar.
						FailAt(__LINE__, asNOT_SUPPORTED);
						return 0;
					}
					PushValue(src, argDwords > 0 ? argDwords : 1);
					pop += argDwords > 0 ? argDwords : 1;
				}
			}
			if( returnsOnStack )
			{
				bc.InstrSHORT(asBC_PSF, (short)returnDest);
				pop += AS_PTR_SIZE;
			}
			if( recvOffset != 0x7fffffff )
			{
				// CallGeneric pops the object from the top of the stack.
				if( recvOffsetStoresPointer
					|| (callee && callee->objectType
						&& (callee->objectType->GetFlags() & asOBJ_VALUE) == 0) )
					bc.InstrSHORT(asBC_PshVPtr, (short)recvOffset);
				else
					bc.InstrSHORT(asBC_PSF, (short)recvOffset);
				pop += AS_PTR_SIZE;
			}

			if( callee )
			{
				if( callee->funcType == asFUNC_IMPORTED )
					bc.Call(asBC_CALLBND, callee->id, pop);
				else if( callee->funcType == asFUNC_SYSTEM )
					bc.Call(asBC_CALLSYS, callee->id, pop);
				else if( callee->funcType == asFUNC_INTERFACE
					|| expr->callDispatch == asAST_CALL_DISPATCH_VIRTUAL )
				{
					// Sema owns the source-level distinction between an ordinary
					// virtual-eligible member call and Base::Method(). Runtime owns
					// only the final vtable slot/function ABI. Canonical bodies are
					// emitted before artifact commit assigns vfTableIdx, so current
					// slot availability is not a valid dispatch predicate here.
					bc.Call(asBC_CALLINTF, callee->id, pop);
				}
				else
					bc.Call(asBC_CALL, callee->id, pop);
			}
			else
			{
				bc.CallPtr(asBC_CallPtr, funcVar, pop);
			}
			for( asUINT i = 0; i < transferredOwnedArguments.GetLength(); ++i )
			{
				// CALLSYS retires the argument through cleanArgs; a script callee
				// retires it with FREE in its epilogue. Clear the caller's untracked
				// alias after either successful return.
				bc.InstrSHORT(
					asBC_ClrVPtr, (short)transferredOwnedArguments[i]);
			}
			if( deferredOutPlans.GetLength() != 0 )
			{
				// Preserve the primary result before a setter call can overwrite the
				// VM return registers.  Primitive/reference-return aliasing requires a
				// stronger lifetime contract and deliberately remains unsupported in
				// this first primitive-out closure.
				if( dataType.IsReference() && !dataType.IsObjectHandle() )
				{
					FailAt(__LINE__, asNOT_SUPPORTED);
					return 0;
				}
				int capturedReturn = 0x7fffffff;
				if( dataType.GetTokenType() != ttVoid )
				{
					if( returnsOnStack )
					{
						bc.ObjInfo(returnDest, asOBJ_INIT);
						capturedReturn = returnDest;
					}
					else if( dataType.IsObjectHandle() || dataType.IsFuncdef() )
					{
						capturedReturn = AllocTyped(dataType);
						bc.InstrSHORT(asBC_STOREOBJ, (short)capturedReturn);
						bc.ObjInfo(capturedReturn, asOBJ_INIT);
					}
					else
					{
						capturedReturn = AllocTemporary(
							dataType, dwords > 0 ? dwords : 1);
						StoreReturn(capturedReturn, dwords > 0 ? dwords : 1);
					}
				}

				for( asUINT i = 0; i < deferredOutPlans.GetLength(); ++i )
				{
					const asCExpr* const plan = deferredOutPlans[i];
					asCScriptFunction* const setter =
						FindFunc(plan->resolvedDecl);
					if( setter == 0 || setter->objectType == 0
						|| setter->parameterTypes.GetLength() != 1 )
					{
						FailAt(__LINE__, asNOT_SUPPORTED);
						return 0;
					}
					const int setterDwords =
						SizeDwords(setter->parameterTypes[0]);
					PushValue(deferredOutValueOffsets[i],
						setterDwords > 0 ? setterDwords : 1);
					bc.InstrSHORT(asBC_PshVPtr,
						(short)deferredOutReceiverOffsets[i]);
					const int setterPop =
						(setterDwords > 0 ? setterDwords : 1) + AS_PTR_SIZE;
					if( setter->funcType == asFUNC_SYSTEM )
						bc.Call(asBC_CALLSYS, setter->id, setterPop);
					else
						bc.Call(asBC_CALL, setter->id, setterPop);
				}
				return dataType.GetTokenType() == ttVoid ? 0 : capturedReturn;
			}
			if( dataType.GetTokenType() == ttVoid )
				return 0;
			if( returnsOnStack )
			{
				bc.ObjInfo(returnDest, asOBJ_INIT);
				return returnDest;
			}
			if( dataType.IsObjectHandle() || dataType.IsFuncdef() )
			{
				// REF/funcdef calls return through objectRegister, not valueRegister.
				// Move that owning reference into a typed slot so the existing
				// reverse epilogue cleanup can release it exactly once. Storing the
				// pointer with CpyRtoV8 reads the unrelated valueRegister and both
				// loses the returned object and leaves objectRegister owning it.
				const int dest = AllocTyped(dataType);
				bc.InstrSHORT(asBC_STOREOBJ, (short)dest);
				bc.ObjInfo(dest, asOBJ_INIT);
				return dest;
			}
			const int dest = AllocTemporary(
				dataType, dwords > 0 ? dwords : 1);
			StoreReturn(dest, dwords > 0 ? dwords : 1);
			return dest;
		}

		void EmitLine(const asCSourceRange& range)
		{
			if( !range.begin.IsValid() || bc.GetEngine() == 0 )
				return;
			asSSourceLineColumn location;
			if( !context.GetSourceManager().GetLineColumn(range.begin, location) )
				return;
			const char* key = context.GetSourceManager().GetLogicalKey(range.begin.fileID);
			const int sectionIdx = bc.GetEngine()->GetScriptSectionNameIndex(key ? key : "");
			bc.Line(location.row, location.column, sectionIdx);
		}

		bool IsSyntheticDeclaredInitializer(const asCStmt* stmt) const
		{
			if( stmt == 0 || stmt->kind != asAST_STMT_EXPR
				|| !stmt->expr.IsValid() )
			{
				return false;
			}
			const asCExpr* const assignment = context.GetExpr(stmt->expr);
			if( assignment == 0 || assignment->kind != asAST_EXPR_ASSIGN
				|| !assignment->literal.Equals("=")
				|| assignment->children.GetLength() != 2 )
			{
				return false;
			}
			const asCExpr* const lhs = UnwrapAssignLhs(
				context.GetExpr(assignment->children[0]));
			return lhs != 0 && lhs->kind == asAST_EXPR_DECL_REF
				&& IsDeclaredInitializer(
					lhs->resolvedDecl, assignment->children[1]);
		}

		void EmitStmt(asASTStmtId id)
		{
			if( !ok )
				return;
			const asCStmt* stmt = context.GetStmt(id);
			if( stmt == 0 )
			{
				FailAt(__LINE__, asAST_VERIFY_DANGLING_ID);
				return;
			}

			// A local declaration with an initializer is represented explicitly as
			// DeclStmt + synthetic Assign ExprStmt. Both carry the authored
			// declaration range, but they are one source-level stepping event. A
			// second LINE between VarDecl and the assignment exposes a live but
			// not-yet-initialized variable to line callbacks. Keep the semantic
			// assignment in bytecode while suppressing only its duplicate line mark.
			if( !IsSyntheticDeclaredInitializer(stmt) )
			{
				EmitLine(stmt->range);
			}

			switch( stmt->kind )
			{
			case asAST_STMT_BLOCK:
				bc.Block(true);
				for( asUINT i = 0; i < stmt->children.GetLength(); ++i )
					EmitStmt(stmt->children[i]);
				bc.Block(false);
				break;
			case asAST_STMT_DECL:
				if( stmt->decl.IsValid() )
				{
					const asCDecl* var = context.GetDecl(stmt->decl);
					if( var == 0 )
					{
						FailAt(__LINE__, asAST_VERIFY_DANGLING_ID);
						return;
					}
					const asCDataType dataType = TypeOf(var->type);
					int slot = FindSlot(stmt->decl);
					if( slot == 0x7fffffff )
					{
						slot = (dataType.IsObject() || dataType.IsObjectHandle() || dataType.IsFuncdef())
							? AllocTyped(dataType)
							: AllocDwords(SizeDwords(dataType));
						BindSlot(var->id, slot);
						if( dataType.IsObject() && !dataType.IsObjectHandle()
							&& dataType.GetTypeInfo()
							&& (dataType.GetTypeInfo()->GetFlags() & asOBJ_VALUE)
							&& !HasSealedValueInitializationPlan(var->id, dataType) )
						{
							FailAt(__LINE__, asNO_FUNCTION);
							return;
						}
					}
					if( !PublishDebugVariable(var, dataType, slot) )
						return;
				}
				break;
			case asAST_STMT_EXPR:
				if( stmt->expr.IsValid() )
				{
					const asCExpr* const expression = context.GetExpr(stmt->expr);
					if( expression != 0
						&& lifetimeView.FindCleanupBinding(stmt->id) != 0 )
					{
						if( EmitLexicalCleanup(stmt->id) )
						{
							RetireLexicalCleanupObject(stmt->id);
						}
					}
					else
					{
						EmitExpr(stmt->expr);
					}
				}
				break;
			case asAST_STMT_RETURN:
				if( stmt->expr.IsValid() && returnSlot != 0x7fffffff )
				{
					const asCExpr* retExpr = context.GetExpr(stmt->expr);
					int value = 0;
					if( func->returnType.IsReference() && !func->returnType.IsObjectHandle()
						&& retExpr && retExpr->kind == asAST_EXPR_DECL_REF )
					{
						value = EmitDeclRef(retExpr, TypeOf(retExpr->type), SizeDwords(TypeOf(retExpr->type)), true);
					}
					else
					{
						value = EmitExpr(stmt->expr);
					}
					if( !ok )
						return;
					CopyValue(returnSlot, value, func->returnType);
					DetachReturnedHandle(value);
				}
				EmitSealedTransferCleanups(stmt);
				if( !ok )
					return;
				EmitTransferCleanups(asASTStmtId());
				if( !ok )
					return;
				bc.InstrINT(asBC_JMP, 0);
				break;
			case asAST_STMT_IF:
				EmitIf(stmt);
				break;
			case asAST_STMT_WHILE:
				EmitWhile(stmt);
				break;
			case asAST_STMT_DO_WHILE:
				EmitDoWhile(stmt);
				break;
			case asAST_STMT_FOR:
				EmitFor(stmt);
				break;
			case asAST_STMT_FOREACH:
				// Sema seals foreach as init / condition / body / increment.
				// Reuse the structured loop emitter; no protocol lookup or source
				// reconstruction is permitted in the backend.
				EmitFor(stmt);
				break;
			case asAST_STMT_SWITCH:
				EmitSwitch(stmt);
				break;
			case asAST_STMT_CASE:
				break;
			case asAST_STMT_FALLTHROUGH:
				// Switch case bodies are emitted contiguously. The sealed AST verifier
				// has already proved that an explicit target, if present, is the next
				// case. Lexical cleanup children still execute before that transfer.
				EmitSealedTransferCleanups(stmt);
				break;
			case asAST_STMT_BREAK:
			{
				const int label = FindBreakLabel(stmt->target);
				if( label < 0 )
				{
					FailAt(__LINE__, asNOT_SUPPORTED);
					return;
				}
				EmitSealedTransferCleanups(stmt);
				if( !ok )
					return;
				EmitTransferCleanups(stmt->target);
				if( !ok )
					return;
				bc.InstrINT(asBC_JMP, label);
				break;
			}
			case asAST_STMT_CONTINUE:
			{
				const int label = FindContinueLabel(stmt->target);
				if( label < 0 )
				{
					FailAt(__LINE__, asNOT_SUPPORTED);
					return;
				}
				EmitSealedTransferCleanups(stmt);
				if( !ok )
					return;
				EmitTransferCleanups(stmt->target);
				if( !ok )
					return;
				bc.InstrINT(asBC_JMP, label);
				break;
			}
			default:
				FailAt(__LINE__, asCONTEXT_NOT_FINISHED);
				break;
			}
		}

		void PushLoop(
			asASTStmtId stmt,
			int breakLabel,
			int continueLabel,
			asASTStmtId cleanup = asASTStmtId())
		{
			asSCodeGenLoop frame;
			frame.stmt = stmt;
			frame.breakLabel = breakLabel;
			frame.continueLabel = continueLabel;
			frame.cleanup = cleanup;
			loops.PushLast(frame);
		}

		bool EmitScopeExitCleanup(asASTStmtId cleanupStmt)
		{
			const asSASTLifetimeCleanupBinding* const binding =
				lifetimeView.FindCleanupBinding(cleanupStmt);
			const asCDecl* const targetDecl = binding != 0
				&& binding->subject.nodeClass == asAST_LIFETIME_NODE_DECL
				? context.GetDecl(asASTDeclId(binding->subject.value)) : 0;
			const asCDecl* const destructor = binding != 0
				&& binding->actionTarget.nodeClass == asAST_LIFETIME_NODE_DECL
				? context.GetDecl(asASTDeclId(binding->actionTarget.value)) : 0;
			if( binding == 0
				|| binding->actionKind != asAST_LIFETIME_ACTION_DESTROY_VALUE
				|| targetDecl == 0 || targetDecl->kind != asAST_DECL_VAR
				|| destructor == 0
				|| destructor->kind != asAST_DECL_DESTRUCTOR )
			{
				FailAt(__LINE__, asAST_VERIFY_INVALID_CHILD);
				return false;
			}

			const int slot = FindSlot(targetDecl->id);
			asSCodeGenObject* const object = FindTrackedObject(slot);
			asCScriptFunction* const destructorFunction =
				FindFunc(destructor->id);
			asCObjectType* const objectType = object
				? CastToObjectType(object->type.GetTypeInfo()) : 0;
			if( slot == 0x7fffffff || object == 0 || objectType == 0
				|| destructorFunction == 0
				|| destructorFunction->objectType != objectType )
			{
				FailAt(__LINE__, asNO_FUNCTION);
				return false;
			}
			if( builder )
			{
				builder->MarkDependency(destructorFunction, 0, 0);
			}
			bc.InstrSHORT(asBC_PSF, (short)slot);
			bc.Call(destructorFunction->funcType == asFUNC_SYSTEM
					? asBC_CALLSYS : asBC_CALL,
				destructorFunction->id, AS_PTR_SIZE);
			bc.ObjInfo(slot, asOBJ_UNINIT);
			return true;
		}

		bool EmitScopeReleaseCleanup(asASTStmtId cleanupStmt)
		{
			const asSASTLifetimeCleanupBinding* const binding =
				lifetimeView.FindCleanupBinding(cleanupStmt);
			const asCDecl* const targetDecl = binding != 0
				&& binding->subject.nodeClass == asAST_LIFETIME_NODE_DECL
				? context.GetDecl(asASTDeclId(binding->subject.value)) : 0;
			const asCType* const canonicalType = targetDecl
				? context.GetType(targetDecl->type.type) : 0;
			if( binding == 0
				|| binding->actionKind != asAST_LIFETIME_ACTION_RELEASE_REFERENCE
				|| targetDecl == 0
				|| targetDecl->kind != asAST_DECL_VAR
				|| targetDecl->type.IsReference() || canonicalType == 0
				|| (canonicalType->kind != asAST_TYPE_REFERENCE_OBJECT
					&& canonicalType->kind != asAST_TYPE_FUNCDEF) )
			{
				FailAt(__LINE__, asAST_VERIFY_INVALID_CHILD);
				return false;
			}

			const int slot = FindSlot(targetDecl->id);
			asSCodeGenObject* const object = FindTrackedObject(slot);
			if( slot == 0x7fffffff || object == 0
				|| !IsOwnedReferenceObject(object->type) )
			{
				FailAt(__LINE__, asINVALID_TYPE);
				return false;
			}
			const asCDataType targetType = TypeOf(targetDecl->type);
			if( !ok || targetType.GetTypeInfo() != object->type.GetTypeInfo()
				|| targetType.IsFuncdef() != object->type.IsFuncdef() )
			{
				if( ok )
				{
					FailAt(__LINE__, asINVALID_TYPE);
				}
				return false;
			}
			return EmitOwnedReferenceRelease(*object);
		}

		bool EmitLexicalCleanup(asASTStmtId cleanupStmt)
		{
			const asSASTLifetimeCleanupBinding* const binding =
				lifetimeView.FindCleanupBinding(cleanupStmt);
			if( binding != 0 )
			{
				if( binding->actionKind == asAST_LIFETIME_ACTION_DESTROY_VALUE )
				{
					return EmitScopeExitCleanup(cleanupStmt);
				}
				if( binding->actionKind == asAST_LIFETIME_ACTION_RELEASE_REFERENCE )
				{
					return EmitScopeReleaseCleanup(cleanupStmt);
				}
			}
			FailAt(__LINE__, asAST_VERIFY_INVALID_CHILD);
			return false;
		}

		void EmitSealedTransferCleanups(const asCStmt* transfer)
		{
			for( asUINT childIndex = 0;
				transfer != 0 && childIndex < transfer->children.GetLength();
				++childIndex )
			{
				if( !EmitLexicalCleanup(transfer->children[childIndex]) )
				{
					return;
				}
			}
		}

		void EmitTransferCleanups(asASTStmtId target)
		{
			for( asUINT loopIndex = loops.GetLength(); loopIndex > 0; --loopIndex )
			{
				const asSCodeGenLoop& frame = loops[loopIndex - 1];
				if( target.IsValid() && frame.stmt == target )
				{
					// The target loop's break label enters its own cleanup phase;
					// continue remains inside that lifetime. Only exited inner loops
					// need cleanup emitted at the transfer site.
					break;
				}
				if( frame.cleanup.IsValid() && !EmitScopeExitCleanup(frame.cleanup) )
				{
					return;
				}
			}
		}

		void RetireLexicalCleanupObject(asASTStmtId cleanupStmt)
		{
			const asSASTLifetimeCleanupBinding* const binding =
				lifetimeView.FindCleanupBinding(cleanupStmt);
			const int slot = binding != 0
				&& binding->subject.nodeClass == asAST_LIFETIME_NODE_DECL
				? FindSlot(asASTDeclId(binding->subject.value)) : 0x7fffffff;
			for( asUINT objectIndex = 0; objectIndex < objects.GetLength(); ++objectIndex )
			{
				if( objects[objectIndex].offset == slot )
				{
					objects[objectIndex].live = false;
					return;
				}
			}
		}

		void PopLoop()
		{
			if( loops.GetLength() )
				loops.PopLast();
		}

		int FindBreakLabel(asASTStmtId target) const
		{
			if( target.IsValid() )
			{
				for( asUINT i = 0; i < loops.GetLength(); ++i )
				{
					if( loops[i].stmt == target )
						return loops[i].breakLabel;
				}
			}
			return -1;
		}

		bool HasConstructAssignTo(asASTDeclId varId) const
		{
			if( !varId.IsValid() )
			{
				return false;
			}
			const asCDecl* var = context.GetDecl(varId);
			if( var == 0 )
			{
				return false;
			}
			for( asUINT i = 0; i < var->inits.GetLength(); ++i )
			{
				const asCExpr* rhs = context.GetExpr(var->inits[i]);
				while( rhs
					&& (rhs->kind == asAST_EXPR_MATERIALIZE_TEMPORARY || rhs->kind == asAST_EXPR_CLEANUP)
					&& rhs->children.GetLength() )
				{
					rhs = context.GetExpr(rhs->children[0]);
				}
				if( rhs && rhs->kind == asAST_EXPR_CONSTRUCT )
				{
					return true;
				}
			}
			return false;
		}

		bool HasSealedValueInitializationPlan(
			asASTDeclId varId, const asCDataType& dataType)
		{
			if( HasConstructAssignTo(varId) )
			{
				return true;
			}
			if( !CanEmitSealedStorageCopy(dataType) )
			{
				return false;
			}
			const asCDecl* const var = context.GetDecl(varId);
			if( var == 0 || var->inits.GetLength() == 0 )
			{
				return false;
			}
			for( asUINT initIndex = 0; initIndex < var->inits.GetLength(); ++initIndex )
			{
				const asCExpr* const init = context.GetExpr(var->inits[initIndex]);
				if( init == 0 )
				{
					continue;
				}
				const asCDataType initType = TypeOf(init->type);
				if( initType.IsValid()
					&& initType.GetTypeInfo() == dataType.GetTypeInfo() )
				{
					return true;
				}
			}
			return false;
		}

		bool IsDeclaredInitializer(
			asASTDeclId varId, asASTExprId expressionId) const
		{
			const asCDecl* const var = context.GetDecl(varId);
			if( var == 0 || !expressionId.IsValid() )
			{
				return false;
			}
			for( asUINT initIndex = 0; initIndex < var->inits.GetLength(); ++initIndex )
			{
				if( var->inits[initIndex] == expressionId )
				{
					return true;
				}
			}
			return false;
		}

		int FindContinueLabel(asASTStmtId target) const
		{
			if( target.IsValid() )
			{
				for( asUINT i = 0; i < loops.GetLength(); ++i )
				{
					if( loops[i].stmt == target )
						return loops[i].continueLabel;
				}
			}
			return -1;
		}

		void EmitIf(const asCStmt* stmt)
		{
			if( !stmt->expr.IsValid() || stmt->children.GetLength() == 0 )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return;
			}
			const int cond = EmitExpr(stmt->expr);
			if( !ok )
				return;
			const int elseLabel = nextLabel++;
			const int endLabel = nextLabel++;
			EmitBoolJump(cond, asBC_JLowZ, elseLabel);
			EmitStmt(stmt->children[0]);
			if( stmt->children.GetLength() > 1 )
			{
				bc.InstrINT(asBC_JMP, endLabel);
				bc.Label((short)elseLabel);
				EmitStmt(stmt->children[1]);
				bc.Label((short)endLabel);
			}
			else
			{
				bc.Label((short)elseLabel);
				(void)endLabel;
			}
		}

		void EmitWhile(const asCStmt* stmt)
		{
			if( !stmt->expr.IsValid() || stmt->children.GetLength() == 0 )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return;
			}
			const int start = nextLabel++;
			const int end = nextLabel++;
			PushLoop(stmt->id, end, start);
			bc.Label((short)start);
			const int cond = EmitExpr(stmt->expr);
			EmitBoolJump(cond, asBC_JLowZ, end);
			bc.Instr(asBC_SUSPEND);
			bc.InstrPTR(asBC_JitEntry, 0);
			EmitStmt(stmt->children[0]);
			bc.InstrINT(asBC_JMP, start);
			bc.Label((short)end);
			PopLoop();
		}

		void EmitDoWhile(const asCStmt* stmt)
		{
			if( !stmt->expr.IsValid() || stmt->children.GetLength() == 0 )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return;
			}
			const int start = nextLabel++;
			const int condLabel = nextLabel++;
			const int end = nextLabel++;
			PushLoop(stmt->id, end, condLabel);
			bc.Label((short)start);
			EmitStmt(stmt->children[0]);
			bc.Label((short)condLabel);
			const int cond = EmitExpr(stmt->expr);
			EmitBoolJump(cond, asBC_JLowNZ, start);
			bc.Label((short)end);
			PopLoop();
		}

		void EmitFor(const asCStmt* stmt)
		{
			asASTStmtId initializerId;
			asASTExprId conditionId;
			asASTStmtId bodyId;
			asASTStmtId incrementId;
			asASTStmtId cleanup;
			if( stmt->kind == asAST_STMT_FOREACH )
			{
				const asSASTForEachPhases* const phases =
					lifetimeView.FindForEachPhases(stmt->id);
				if( phases == 0 )
				{
					FailAt(__LINE__, asAST_VERIFY_INVALID_LIFETIME_PROTOCOL);
					return;
				}
				initializerId = phases->initializer;
				conditionId = phases->condition;
				bodyId = phases->body;
				incrementId = phases->increment;
				cleanup = phases->cleanup;
			}
			else
			{
				if( stmt->children.GetLength() < 2 )
				{
					FailAt(__LINE__, asNOT_SUPPORTED);
					return;
				}
				initializerId = stmt->children[0];
				conditionId = stmt->expr;
				bodyId = stmt->children[1];
				incrementId = stmt->children.GetLength() > 2
					? stmt->children[2] : asASTStmtId();
			}
			if( !bodyId.IsValid() )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return;
			}

			// The parser represents a declaration initializer as a synthetic Block
			// containing DeclStmt + initializer ExprStmt. In the grammar this child
			// cannot be an authored `{}` statement, so emitting that Block directly
			// would incorrectly end the declared variable's debug lifetime before the
			// condition, body, and increment. Model the actual for-init lexical scope
			// across the complete loop and flatten only this position's synthetic block.
			bc.Block(true);
			if( initializerId.IsValid() )
			{
				const asCStmt* initializer = context.GetStmt(initializerId);
				if( initializer == 0 )
				{
					FailAt(__LINE__, asAST_VERIFY_DANGLING_ID);
					return;
				}
				if( initializer->kind == asAST_STMT_BLOCK )
				{
					for( asUINT childIndex = 0;
						childIndex < initializer->children.GetLength(); ++childIndex )
					{
						EmitStmt(initializer->children[childIndex]);
					}
				}
				else
				{
					EmitStmt(initializerId);
				}
			}
			const int start = nextLabel++;
			const int incr = nextLabel++;
			const int end = nextLabel++;
			PushLoop(stmt->id, end, incr, cleanup);
			bc.Label((short)start);
			if( conditionId.IsValid() )
			{
				const int cond = EmitExpr(conditionId);
				EmitBoolJump(cond, asBC_JLowZ, end);
			}
			EmitStmt(bodyId);
			bc.Label((short)incr);
			if( incrementId.IsValid() )
				EmitStmt(incrementId);
			bc.InstrINT(asBC_JMP, start);
			bc.Label((short)end);
			if( cleanup.IsValid() )
			{
				if( !EmitScopeExitCleanup(cleanup) )
				{
					PopLoop();
					return;
				}
				// Every fallthrough/break route enters the cleanup label. Return and
				// outer transfers emitted their own copy above, so the common function
				// epilogue must not destroy this exact iterator a second time.
				RetireLexicalCleanupObject(cleanup);
			}
			bc.Block(false);
			PopLoop();
		}

		void EmitSwitch(const asCStmt* stmt)
		{
			if( !stmt->expr.IsValid() )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return;
			}
			const asCExpr* selectorExpr = context.GetExpr(stmt->expr);
			if( selectorExpr == 0 )
			{
				FailAt(__LINE__, asAST_VERIFY_DANGLING_ID);
				return;
			}
			const asCDataType selectorType = TypeOf(selectorExpr->type);
			if( !ok )
				return;
			if( !selectorType.IsIntegerType() && !selectorType.IsUnsignedType() && !selectorType.IsEnumType() )
			{
				FailAt(__LINE__, asNOT_SUPPORTED);
				return;
			}

			const int end = nextLabel++;
			PushLoop(stmt->id, end, /*continueLabel*/ -1);
			const int selector = EmitExpr(stmt->expr);
			if( !ok )
			{
				PopLoop();
				return;
			}

			asCArray<int> bodyLabels;
			bodyLabels.SetLength(stmt->children.GetLength());
			int defaultIndex = -1;
			for( asUINT i = 0; i < stmt->children.GetLength(); ++i )
			{
				bodyLabels[i] = nextLabel++;
				const asCStmt* cs = context.GetStmt(stmt->children[i]);
				if( cs && cs->kind == asAST_STMT_CASE && !cs->expr.IsValid() )
					defaultIndex = int(i);
			}

			for( asUINT i = 0; i < stmt->children.GetLength(); ++i )
			{
				const asCStmt* cs = context.GetStmt(stmt->children[i]);
				if( cs == 0 || cs->kind != asAST_STMT_CASE || !cs->expr.IsValid() )
					continue;
				const int caseVal = EmitExpr(cs->expr);
				if( !ok )
				{
					PopLoop();
					return;
				}
				const int eq = EmitCompare("==", selector, caseVal, selectorType);
				if( !ok )
				{
					PopLoop();
					return;
				}
				EmitBoolJump(eq, asBC_JLowNZ, bodyLabels[i]);
			}
			if( defaultIndex >= 0 )
				bc.InstrINT(asBC_JMP, bodyLabels[asUINT(defaultIndex)]);
			else
				bc.InstrINT(asBC_JMP, end);

			for( asUINT i = 0; i < stmt->children.GetLength(); ++i )
			{
				const asCStmt* cs = context.GetStmt(stmt->children[i]);
				if( cs == 0 || cs->kind != asAST_STMT_CASE )
					continue;
				bc.Label((short)bodyLabels[i]);
				for( asUINT c = 0; c < cs->children.GetLength(); ++c )
				{
					EmitStmt(cs->children[c]);
					if( !ok )
					{
						PopLoop();
						return;
					}
				}
			}
			bc.Label((short)end);
			PopLoop();
		}

		const asCASTContext& context;
		asCASTLifetimeView lifetimeView;
		bool lifetimeViewValid;
		asCBuilder* builder;
		asCRuntimeTypeBridge* bridge;
		asSBytecodeCodeGenArtifact& artifact;
		asUINT functionArtifactIndex;
		asCArray<asSCodeGenFuncBind>& funcs;
		asCArray<asSCodeGenGlobal>& globals;
		asCArray<asASTDeclId>& captureDecls;
		asCCanonicalStringConstantLease& stringConstants;
		int& error;
		int& failLine;
		asCString& failureDetail;
		asCByteCode bc;
		asCScriptFunction* func;
		const asCDecl* activeDecl;
		asCArray<asSCodeGenSlot> slots;
		asCArray<asASTDeclId> debugVariableDecls;
		asCArray<asSCodeGenOwnedParameter> ownedParameters;
		asCArray<asSTypeRelocation> pendingTypeRelocations;
		asSRuntimeTypeBindingEnvironment bindingEnvironment =
			asMakeCurrentRuntimeTypeBindingEnvironment();
		asCArray<int> temporaryOffsets;
		asCArray<asSCodeGenLoop> loops;
		asCArray<asSCodeGenObject> objects;
		asCArray<asSExprSlotMemo> materializedSlots;
		asCArray<asSExprSlotMemo> opaqueSlots;
		asCArray<asSExprSlotMemo> indexPtrSlots;
		asCArray<asSIndexPtrMemo> indexPtrByArgs;
		int nextLocal;
		int nextLabel;
		int thisOffset;
		int returnSlot;
		int returnDwords;
		bool ok;

		asCObjectProperty* PropertyFromFieldDecl(const asCDecl* target, asCObjectType* objType)
		{
			if( target == 0 || target->kind != asAST_DECL_VAR || objType == 0 || target->byteOffset < 0 )
			{
				return 0;
			}
			for( asUINT i = 0; i < objType->properties.GetLength(); ++i )
			{
				asCObjectProperty* prop = objType->properties[i];
				if( prop && prop->byteOffset == target->byteOffset )
				{
					if( builder )
					{
						builder->MarkPropertyDependency(objType, prop);
					}
					return prop;
				}
			}
			return 0;
		}

		asCObjectProperty* FindThisProperty(const asCDecl* target)
		{
			if( target == 0 || func == 0 || func->objectType == 0 )
			{
				return 0;
			}
			if( FindSlot(target->id) != 0x7fffffff )
			{
				return 0;
			}
			return PropertyFromFieldDecl(target, func->objectType);
		}
	};

	// Global initialization is a lifecycle operation, not merely an allocation
	// time store. asCModule::ResetGlobalVars clears every mutable property and
	// later executes its init function; storing the constant directly during
	// Generate() would therefore be observably lost before the module can run.
	//
	// This deliberately covers only Sema-proven scalar constants for now. It
	// emits the same property-owned initialization-function protocol as the
	// maintained builder, so ResetGlobalVars(), SaveByteCode(), restore, module
	// discard, and retry ownership retain their normal behaviour. Dynamic
	// initializers and non-scalar lifetime rules stay fail-closed until their
	// canonical expression/lifetime lowering is implemented.
	bool EmitCanonicalScalarGlobalInitializer(
		const asCASTContext& context,
		asCBuilder& builder,
		asCScriptEngine* engine,
		asCModule* module,
		asSBytecodeCodeGenArtifact& artifact,
		asCGlobalProperty* prop,
		const asCDecl* decl,
		const asCDataType& dataType,
		asQWORD constantValue,
		bool attachToProperty = true)
	{
		if( engine == 0 || module == 0 || prop == 0 || decl == 0
			|| (!dataType.IsIntegerType() && !dataType.IsUnsignedType()
				&& !dataType.IsFloat32Type() && !dataType.IsFloat64Type()) )
		{
			return false;
		}

		const int bytes = dataType.GetSizeInMemoryBytes();
		if( bytes != 1 && bytes != 2 && bytes != 4 && bytes != 8 )
		{
			return false;
		}

		asCScriptFunction* const initFunc =
			asNEW(asCScriptFunction)(engine, module, asFUNC_SCRIPT);
		if( initFunc == 0 || initFunc->scriptData == 0 )
		{
			if( initFunc )
			{
				initFunc->funcType = asFUNC_DUMMY;
				asDELETE(initFunc, asCScriptFunction);
			}
			return false;
		}

		initFunc->id = artifact.GetNextPendingScriptFunctionId(engine);
		initFunc->nameSpace = prop->nameSpace;
		initFunc->returnType = asCDataType::CreatePrimitive(ttVoid, false);

		if( decl->range.begin.IsValid() )
		{
			asSSourceLineColumn location = {};
			if( context.GetSourceManager().GetLineColumn(decl->range.begin, location) )
			{
				const char* const key = context.GetSourceManager().GetLogicalKey(
					decl->range.begin.fileID);
				initFunc->scriptData->scriptSectionIdx =
					engine->GetScriptSectionNameIndex(key ? key : "");
				initFunc->scriptData->declaredAt =
					(location.row & 0xFFFFF) | ((location.column & 0xFFF) << 20);
			}
		}

		asCByteCode byteCode(&builder);
		// Frame slot zero is reserved by the VM. Bytecode offsets address the
		// high dword of a multi-dword value: AllocDwords(2) in the canonical
		// function emitter reserves slots 1/2 and returns 2 for SetV8/WRTV8.
		// Keep this standalone initializer on the identical stack convention.
		const int valueDwords = bytes > 4 ? 2 : 1;
		const int valueSlot = valueDwords;
		byteCode.InstrPTR(asBC_JitEntry, 0);
		if( bytes == 1 )
			byteCode.InstrSHORT_B(asBC_SetV1, (short)valueSlot, (asBYTE)constantValue);
		else if( bytes == 2 )
			byteCode.InstrSHORT_W(asBC_SetV2, (short)valueSlot, (asWORD)constantValue);
		else if( bytes == 4 )
			byteCode.InstrSHORT_DW(asBC_SetV4, (short)valueSlot, (asDWORD)constantValue);
		else
			byteCode.InstrSHORT_QW(asBC_SetV8, (short)valueSlot, constantValue);
		byteCode.InstrPTR(asBC_LDG, prop->GetAddressOfValue());
		if( bytes == 1 )
			byteCode.InstrSHORT(asBC_WRTV1, (short)valueSlot);
		else if( bytes == 2 )
			byteCode.InstrSHORT(asBC_WRTV2, (short)valueSlot);
		else if( bytes == 4 )
			byteCode.InstrSHORT(asBC_WRTV4, (short)valueSlot);
		else
			byteCode.InstrSHORT(asBC_WRTV8, (short)valueSlot);
		byteCode.Ret(0);

		asCArray<int> temporaries;
		byteCode.Finalize(temporaries);
		byteCode.ExtractObjectVariableInfo(initFunc);
		initFunc->scriptData->variableSpace = valueDwords;
		initFunc->scriptData->byteCode.SetLength(byteCode.GetSize());
		if( byteCode.GetSize() == 0 )
		{
			initFunc->funcType = asFUNC_DUMMY;
			asDELETE(initFunc, asCScriptFunction);
			return false;
		}
		byteCode.Output(initFunc->scriptData->byteCode.AddressOf());
		initFunc->scriptData->stackNeeded =
			byteCode.largestStackUsed + initFunc->scriptData->variableSpace;
		initFunc->scriptData->lineNumbers = byteCode.lineNumbers;
		// ExtractObjectVariableInfo() owns bytecode-side cleanup metadata, but
		// it does not establish the runtime count of heap object locals. This
		// scalar-only initializer has none. Leaving the count uninitialized
		// makes PrepareScriptFunction() walk arbitrary stack offsets.
		initFunc->scriptData->objVariablesOnHeap = 0;

		if( attachToProperty )
			prop->SetInitFunc(initFunc);
		// Keep the constructor's internal reference in the artifact until
		// Commit()/Abandon(). The property owns a second reference. Releasing it
		// here would make a rollback depend on the property teardown ordering and
		// would force the initializer into engine->scriptFunctions before all
		// bodies have emitted.
		artifact.functions.PushLast(initFunc);
		artifact.functionDeclKinds.PushLast(asBYTECODE_CODEGEN_GLOBAL_INITIALIZER);
		return true;
	}

	void DiscardAllocatedGlobals(asCModule* module, asCArray<asSCodeGenGlobal>& globals)
	{
		// Global ownership moved to asSBytecodeCodeGenArtifact. Keep this small
		// compatibility helper while the local declaration-to-property binding
		// array remains the emitter's lookup table; calling it after Abandon()
		// must never attempt to release a property twice.
		UNUSED_VAR(module);
		globals.SetLength(0);
	}

	sBindInfo* CreateCanonicalPendingImport(
		asCScriptEngine* engine,
		asCModule* module,
		int id,
		const asCString& funcName,
		const asCDataType& returnType,
		const asCArray<asCDataType>& params,
		const asCArray<asETypeModifiers>& inOutFlags,
		const asCArray<asCString*>& defaultArgs,
		asSNameSpace* nameSpace,
		const asCString& sourceModule)
	{
		if( engine == 0 || module == 0 || id < 0 || nameSpace == 0 )
		{
			for( asUINT i = 0; i < defaultArgs.GetLength(); ++i )
				if( defaultArgs[i] )
					asDELETE(defaultArgs[i], asCString);
			return 0;
		}

		asCScriptFunction* func = asNEW(asCScriptFunction)(engine, module, asFUNC_IMPORTED);
		if( func == 0 )
		{
			for( asUINT i = 0; i < defaultArgs.GetLength(); ++i )
				if( defaultArgs[i] )
					asDELETE(defaultArgs[i], asCString);
			return 0;
		}

		func->name = funcName;
		func->id = id;
		func->returnType = returnType;
		func->nameSpace = nameSpace;
		func->parameterTypes = params;
		func->inOutFlags = inOutFlags;
		func->defaultArgs = defaultArgs;
		func->objectType = 0;
		func->traits = asSFunctionTraits();
		func->CalculateParameterOffsets();

		sBindInfo* info = asNEW(sBindInfo);
		if( info == 0 )
		{
			func->funcType = asFUNC_DUMMY;
			asDELETE(func, asCScriptFunction);
			return 0;
		}
		info->importedFunctionSignature = func;
		info->boundFunctionId = -1;
		info->importFromModule = sourceModule;
		return info;
	}

	asCGlobalProperty* CreateCanonicalPendingGlobal(
		asCScriptEngine* engine,
		asCModule* module,
		int id,
		const char* name,
		const asCDataType& type,
		asSNameSpace* nameSpace)
	{
		if( engine == 0 || module == 0 || id < 0 || nameSpace == 0 )
			return 0;

		asCGlobalProperty* prop = asNEW(asCGlobalProperty);
		if( prop == 0 )
			return 0;

		prop->id = (asUINT)id;
		prop->name = name ? name : "";
		prop->type = type;
		prop->nameSpace = nameSpace;
		prop->module = module;
		prop->AllocateMemory();
		return prop;
	}

	int RegisterCanonicalScriptTypes(
		const asCASTContext& context,
		asCScriptEngine* engine,
		asCModule* module,
		asCRuntimeTypeBridge& bridge,
		asSBytecodeCodeGenArtifact& artifact)
	{
		if( engine == 0 || module == 0 )
		{
			return asINVALID_CONFIGURATION;
		}

		// Script types must be usable by later field/signature lowering, but
		// they do not have to be visible through engine/module lookup tables.
		// Populate the complete candidate set first and expose it only through
		// the local type bridge for this Generate() call.
		bridge.SetTransientTypes(&artifact.types);
		for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
		{
			const asCDecl* child = context.GetDecl(asASTDeclId(i));
			if( child == 0 || child->kind != asAST_DECL_CLASS )
			{
				continue;
			}
			if( child->origin.Equals("canonical-native-type-view") )
			{
				// Host types, including instantiated native templates such as
				// array<int>, are symbol/ABI projections in the sealed AST. They are
				// resolved through the runtime type bridge and must never be
				// republished as detached script classes merely because a template
				// instance is absent from the ordinary registered-name index.
				continue;
			}
			asSNameSpace* const nameSpace = ResolveCanonicalDeclNamespace(context, engine, module, child);
			if( nameSpace == 0 )
			{
				return asOUT_OF_MEMORY;
			}
			asCTypeInfo* existing = 0;
			if( child->name.GetLength() != 0 )
			{
				existing = FindRegisteredTypeInNamespace(engine, child->name, nameSpace);
			}
			if( existing )
			{
				if( existing->module )
				{
					// A later same-name script declaration must never be silently
					// mapped onto an earlier script type. CodeGen has no complete
					// merge/redeclaration contract yet, so reject before it can omit
					// source or mutate an unrelated script module.
					return asINVALID_DECLARATION;
				}

				// Native registrations deliberately have no owning script module.
				// Sema may expose such a host type through a class-shaped decl so
				// CodeGen can consume its already-registered behaviours; that is a
				// bridge, not a script redeclaration.
				continue;
			}
			if( child->name.GetLength() != 0
				&& FindArtifactType(&artifact, child->name, nameSpace) )
			{
				// The engine lookup above cannot see a detached candidate. Reject a
				// same-build duplicate before either definition can be published.
				return asINVALID_DECLARATION;
			}
			asCObjectType* st = asNEW(asCObjectType)(engine);
			if( st == 0 )
			{
				return asOUT_OF_MEMORY;
			}
			const bool isValue = (child->traits & asAST_TRAIT_VALUE) != 0;
			if( isValue )
			{
				st->flags = asOBJ_VALUE | asOBJ_SCRIPT_OBJECT | asOBJ_NOINHERIT;
			}
			else
			{
				st->flags = asOBJ_REF | asOBJ_SCRIPT_OBJECT | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE;
			}
			if( child->traits & asAST_TRAIT_SHARED )
				st->flags |= asOBJ_SHARED;
			// Class layout is a sealed Sema fact. CodeGen must consume it exactly,
			// never recalculate it from the subset of fields it happens to lower.
			if( child->byteSize < 0 || child->byteAlignment == 0 )
			{
				st->module = 0;
				st->ReleaseInternal();
				return asINVALID_DECLARATION;
			}
			st->size = child->byteSize > 0 ? child->byteSize : 1;
			st->name = child->name;
			st->nameSpace = nameSpace;
			st->module = module;
			st->alignment = child->byteAlignment;
			asPreClassData* const classData = module->PreClassData.Find(child->name);
			if( classData )
			{
				// Sema froze this layout-affecting input on the class declaration.
				// Refuse to publish if the embedding map and sealed graph diverge;
				// CodeGen supplies only the non-source Runtime links/user data.
				if( child->byteOffset < 0
					|| static_cast<size_t>(child->byteOffset) != classData->PropertyOffset
					|| (classData->ShadowType
						&& classData->ShadowType->alignment > static_cast<int>(child->byteAlignment)) )
				{
					st->module = 0;
					st->ReleaseInternal();
					return asINVALID_DECLARATION;
				}
				st->basePropertyOffset = child->byteOffset;
				st->shadowType = classData->ShadowType;
				st->plainUserData = reinterpret_cast<asPWORD>(classData->InitialUserData);
			}
			st->beh = engine->scriptTypeBehaviours.beh;
			if( isValue )
			{
				st->beh.factory = 0;
				st->beh.factories.SetLength(0);
			}
			else if( st->beh.factory
				&& (asUINT)st->beh.factory < engine->scriptFunctions.GetLength()
				&& engine->scriptFunctions[st->beh.factory] )
			{
				engine->scriptFunctions[st->beh.factory]->AddRefInternal();
			}
			if( st->beh.copy
				&& (asUINT)st->beh.copy < engine->scriptFunctions.GetLength()
				&& engine->scriptFunctions[st->beh.copy] )
			{
				engine->scriptFunctions[st->beh.copy]->AddRefInternal();
			}
			if( st->beh.construct
				&& (asUINT)st->beh.construct < engine->scriptFunctions.GetLength()
				&& engine->scriptFunctions[st->beh.construct] )
			{
				engine->scriptFunctions[st->beh.construct]->AddRefInternal();
			}
			st->beh.destruct = 0;
			artifact.types.PushLast(st);
		}

		// Enum declarations are first-class runtime types. Their enumerators are
		// already constant-folded by Sema, so the detached artifact can publish
		// the exact nominal type and value table without consulting asCCompiler.
		for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
		{
			const asCDecl* child = context.GetDecl(asASTDeclId(i));
			if( child == 0 || child->kind != asAST_DECL_ENUM )
			{
				continue;
			}
			asSNameSpace* const nameSpace = ResolveCanonicalDeclNamespace(context, engine, module, child);
			if( nameSpace == 0 || child->name.GetLength() == 0 )
			{
				return asINVALID_DECLARATION;
			}
			// Match asCBuilder::CheckNameConflict(): another script module may
			// declare the same module-local enum name, while a host registration
			// or a duplicate in this candidate remains a hard conflict.
			if( FindRegisteredHostTypeInNamespace(engine, child->name, nameSpace) != 0
				|| FindArtifactType(&artifact, child->name, nameSpace) != 0 )
			{
				return asINVALID_DECLARATION;
			}

			asCEnumType* const enumType = asNEW(asCEnumType)(engine);
			if( enumType == 0 )
			{
				return asOUT_OF_MEMORY;
			}
			enumType->flags = asOBJ_ENUM;
			enumType->size = 1;
			enumType->alignment = 1;
			enumType->name = child->name;
			enumType->nameSpace = nameSpace;
			enumType->module = module;
			artifact.types.PushLast(enumType);

			for( asUINT valueIndex = 0; valueIndex < child->children.GetLength(); ++valueIndex )
			{
				const asCDecl* valueDecl = context.GetDecl(child->children[valueIndex]);
				if( valueDecl == 0 || valueDecl->kind != asAST_DECL_VAR )
				{
					continue;
				}
				if( !valueDecl->hasConstantValue || valueDecl->name.GetLength() == 0 )
				{
					return asINVALID_DECLARATION;
				}
				const asINT64 value = static_cast<asINT64>(valueDecl->constantValue);
				if( value < MIN_int8 || value > MAX_int8 )
				{
					return asINVALID_DECLARATION;
				}
				asSEnumValue* const runtimeValue = asNEW(asSEnumValue);
				if( runtimeValue == 0 )
				{
					return asOUT_OF_MEMORY;
				}
				runtimeValue->name = valueDecl->name;
				runtimeValue->value = static_cast<int>(value);
				enumType->enumValues.PushLast(runtimeValue);
			}
		}

		// Typedefs remain nominal metadata nodes in the module inventory while
		// their aliased data type drives storage and bytecode. The parser limits
		// this fork to primitive aliases, but CodeGen consumes the sealed semantic
		// type rather than reparsing source text.
		for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
		{
			const asCDecl* child = context.GetDecl(asASTDeclId(i));
			if( child == 0 || child->kind != asAST_DECL_TYPEDEF )
			{
				continue;
			}
			asSNameSpace* const nameSpace = ResolveCanonicalDeclNamespace(context, engine, module, child);
			if( nameSpace == 0 || child->name.GetLength() == 0
				|| FindArtifactType(&artifact, child->name, nameSpace) != 0 )
			{
				return asINVALID_DECLARATION;
			}
			const asCDataType aliasType = bridge.Resolve(context, child->type);
			if( !aliasType.IsValid() || aliasType.GetTokenType() == ttVoid
				|| !aliasType.IsPrimitive() || aliasType.IsReference() || aliasType.IsObjectHandle() )
			{
				return asINVALID_TYPE;
			}

			asCTypedefType* const typedefType = asNEW(asCTypedefType)(engine);
			if( typedefType == 0 )
			{
				return asOUT_OF_MEMORY;
			}
			typedefType->flags = asOBJ_TYPEDEF;
			typedefType->size = aliasType.GetSizeInMemoryBytes();
			typedefType->alignment = aliasType.GetAlignment();
			typedefType->name = child->name;
			typedefType->nameSpace = nameSpace;
			typedefType->aliasForType = aliasType;
			typedefType->module = module;
			artifact.types.PushLast(typedefType);
		}

		// Every candidate type is now resolvable by the bridge. Rebuild the exact
		// sealed class inheritance graph while it is still detached. The base edge
		// owns one internal reference, matching asCObjectType::DestroyInternal().
		for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
		{
			const asCDecl* child = context.GetDecl(asASTDeclId(i));
			if( child == 0 || child->kind != asAST_DECL_CLASS )
				continue;
			asSNameSpace* const nameSpace = ResolveCanonicalDeclNamespace(context, engine, module, child);
			asCObjectType* const st = nameSpace
				? FindArtifactObjectType(&artifact, child->name, nameSpace)
				: 0;
			if( st == 0 )
				continue;

			const asCDecl* classBase = 0;
			for( asUINT baseIndex = 0; baseIndex < child->bases.GetLength(); ++baseIndex )
			{
				const asCDecl* const candidate = context.GetDecl(child->bases[baseIndex]);
				if( candidate == 0 || candidate->kind != asAST_DECL_CLASS )
					continue;
				if( classBase != 0 )
					return asINVALID_DECLARATION;
				classBase = candidate;
			}
			if( classBase == 0 )
				continue;
			if( (child->traits & asAST_TRAIT_VALUE) != 0 )
				return asINVALID_DECLARATION;
			asSNameSpace* const baseNameSpace = ResolveCanonicalDeclNamespace(
				context, engine, module, classBase);
			asCObjectType* const baseType = baseNameSpace
				? FindCanonicalObjectType(engine, module, classBase->name, baseNameSpace, &artifact)
				: 0;
			if( baseType == 0 || (baseType->flags & asOBJ_VALUE) != 0
				|| baseType == st || baseType->size > st->size
				|| baseType->alignment > st->alignment )
			{
				return asINVALID_DECLARATION;
			}
			st->derivedFrom = baseType;
			baseType->AddRefInternal();
		}

		// Populate only locally declared fields at their exact Sema offsets. The
		// inherited property views are appended in a separate base-first pass below
		// and share their base-owned property records, matching the legacy ABI.
		for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
		{
			const asCDecl* child = context.GetDecl(asASTDeclId(i));
			if( child == 0 || child->kind != asAST_DECL_CLASS )
			{
				continue;
			}
			asSNameSpace* const nameSpace = ResolveCanonicalDeclNamespace(context, engine, module, child);
			if( nameSpace == 0 )
			{
				return asOUT_OF_MEMORY;
			}
			asCObjectType* st = FindArtifactObjectType(&artifact, child->name, nameSpace);
			if( st == 0 )
			{
				// This class-shaped declaration names a pre-existing native type.
				continue;
			}
			for( asUINT p = 0; p < child->children.GetLength(); ++p )
			{
				const asCDecl* member = context.GetDecl(child->children[p]);
				if( member == 0 || member->kind != asAST_DECL_VAR )
				{
					continue;
				}
				const asCDataType dataType = bridge.Resolve(context, member->type);
				if( dataType.IsValid() && member->byteOffset < 0 )
				{
					return asINVALID_DECLARATION;
				}
			}
			for( asUINT p = 0; p < child->children.GetLength(); ++p )
			{
				const asCDecl* member = context.GetDecl(child->children[p]);
				if( member == 0 || member->kind != asAST_DECL_VAR )
				{
					continue;
				}
				asCDataType dataType = bridge.Resolve(context, member->type);
				if( !dataType.IsValid() )
				{
					continue;
				}
				asCObjectProperty* prop = st->AddPropertyToClass(member->name, dataType, false, false);
				if( prop == 0 )
				{
					return asOUT_OF_MEMORY;
				}
				prop->byteOffset = member->byteOffset;
				const int end = member->byteOffset + dataType.GetSizeInMemoryBytes();
				if( member->byteOffset < 0 || end > child->byteSize )
				{
					return asINVALID_DECLARATION;
				}
			}
		}

		// Append inherited property views only after every local property exists.
		// Resolve bases recursively so source declaration order cannot affect the
		// runtime property graph.
		asCArray<asBYTE> propertyStates;
		propertyStates.SetLength(artifact.types.GetLength());
		for( asUINT root = 0; root < artifact.types.GetLength(); ++root )
		{
			asCObjectType* const rootType = CastToObjectType(artifact.types[root]);
			if( rootType == 0 || propertyStates[root] == 2 )
				continue;
			asCArray<asUINT> stack;
			stack.PushLast(root);
			while( stack.GetLength() )
			{
				const asUINT index = stack[stack.GetLength() - 1];
				asCObjectType* const type = CastToObjectType(artifact.types[index]);
				if( type == 0 )
				{
					propertyStates[index] = 2;
					stack.PopLast();
					continue;
				}
				if( propertyStates[index] == 0 )
					propertyStates[index] = 1;
				asUINT baseArtifactIndex = asUINT(-1);
				for( asUINT candidateIndex = 0; candidateIndex < artifact.types.GetLength(); ++candidateIndex )
				{
					if( artifact.types[candidateIndex] == type->derivedFrom )
					{
						baseArtifactIndex = candidateIndex;
						break;
					}
				}
				if( baseArtifactIndex != asUINT(-1) && propertyStates[baseArtifactIndex] != 2 )
				{
					if( propertyStates[baseArtifactIndex] == 1 )
						return asINVALID_DECLARATION;
					stack.PushLast(baseArtifactIndex);
					continue;
				}
				if( type->derivedFrom )
				{
					for( asUINT p = 0; p < type->derivedFrom->properties.GetLength(); ++p )
					{
						asCObjectProperty* const inherited = type->derivedFrom->properties[p];
						type->properties.PushLast(inherited);
						type->propertyTable.Add(inherited);
					}
				}
				propertyStates[index] = 2;
				stack.PopLast();
			}
		}
		return asSUCCESS;
	}

	bool FillFunctionSourceMetadata(
		const asCASTContext& context,
		asCScriptEngine* engine,
		asCScriptFunction* func,
		const asCDecl* decl,
		int& error)
	{
		if( engine == 0 || func == 0 || func->scriptData == 0 || decl == 0 )
		{
			error = asINVALID_CONFIGURATION;
			return false;
		}

		if( !decl->range.begin.IsValid() )
		{
			// Declaration-only host projections and range-less generated helpers do
			// not claim authored debug coordinates.  Every authored body must have a
			// SourceManager identity, however: publishing it with a null primary
			// section makes cache/StaticJIT capture impossible and hides the producer
			// error until much later.
			if( decl->body.IsValid()
				&& (decl->traits & asAST_TRAIT_GENERATED) == 0 )
			{
				error = asINVALID_DECLARATION;
				return false;
			}
			return true;
		}

		asSSourceLineColumn location = {};
		const char* const key = context.GetSourceManager().GetLogicalKey(
			decl->range.begin.fileID);
		if( key == 0 || key[0] == 0
			|| !context.GetSourceManager().GetLineColumn(
				decl->range.begin, location) )
		{
			error = asINVALID_DECLARATION;
			return false;
		}

		const int sectionIndex = engine->GetScriptSectionNameIndex(key);
		if( sectionIndex < 0
			|| (func->scriptData->scriptSectionIdx >= 0
				&& func->scriptData->scriptSectionIdx != sectionIndex) )
		{
			error = asINVALID_DECLARATION;
			return false;
		}
		func->scriptData->scriptSectionIdx = sectionIndex;
		func->scriptData->declaredAt =
			(location.row & 0xFFFFF) | ((location.column & 0xFFF) << 20);
		return true;
	}

	bool FillFunctionSignature(
		const asCASTContext& context,
		asCScriptEngine* engine,
		asCModule* module,
		asCRuntimeTypeBridge& bridge,
		const asSBytecodeCodeGenArtifact& artifact,
		asCScriptFunction* func,
		const asCDecl* decl,
		int& error)
	{
		if( decl->stableKey.GetLength() == 0 )
		{
			error = asINVALID_DECLARATION;
			return false;
		}
		// Runtime anonymous functions are internal module functions. The maintained
		// module/restore protocol recognizes them by a leading '$' and, for legacy
		// rank fallback only, a trailing numeric component. Canonical identity does
		// not use that rank: it remains the exact parent declaration plus source
		// offset carried by canonicalASTStableDeclKey. Keep the Runtime name stable
		// and collision-resistant without weakening that canonical identity.
		if( decl->traits & asAST_TRAIT_LAMBDA )
		{
			func->name = "$";
			const asCDecl* const parent = context.GetDecl(decl->parent);
			if( parent && parent->stableKey.GetLength() )
				func->name += parent->stableKey;
			else
				func->name += "lambda";
			asCString offsetSuffix;
			offsetSuffix.Format("$%u", decl->range.begin.offset);
			func->name += offsetSuffix;
		}
		// Canonical Decl names keep the class identifier for destructors; the
		// declaration kind carries the '~' semantic and stable-key construction
		// adds it exactly once. Runtime function identity follows the maintained
		// AngelScript ABI, where destructor functions are publicly named ~Type.
		else if( decl->kind == asAST_DECL_DESTRUCTOR )
		{
			func->name = "~";
			func->name += decl->name;
		}
		else
		{
			func->name = decl->name;
		}
		func->canonicalASTStableDeclKey = decl->stableKey;
		if( !FillFunctionSourceMetadata(
			context, engine, func, decl, error) )
		{
			return false;
		}
		func->returnType = bridge.Resolve(context, decl->type);
		func->SetPrivate((decl->traits & asAST_TRAIT_PRIVATE) != 0);
		func->SetProtected((decl->traits & asAST_TRAIT_PROTECTED) != 0);
		func->SetReadOnly((decl->traits & asAST_TRAIT_CONST_METHOD) != 0);
		func->SetOverride((decl->traits & asAST_TRAIT_OVERRIDE) != 0);
		func->SetFinal((decl->traits & asAST_TRAIT_FINAL) != 0);
		func->SetShared((decl->traits & asAST_TRAIT_SHARED) != 0);
		func->SetMixin((decl->traits & asAST_TRAIT_MIXIN) != 0);
		func->traits.SetTrait(asTRAIT_EXTERNAL, (decl->traits & asAST_TRAIT_EXTERNAL) != 0);
		func->traits.SetTrait(asTRAIT_GENERATED_FUNCTION, (decl->traits & asAST_TRAIT_GENERATED) != 0);
		func->traits.SetTrait(asTRAIT_EXTERNAL_IMPLICIT_THIS,
			(decl->traits & asAST_TRAIT_EXTERNAL_IMPLICIT_THIS) != 0);
		func->traits.SetTrait(asTRAIT_CONSTRUCTOR, decl->kind == asAST_DECL_CONSTRUCTOR);
		func->traits.SetTrait(asTRAIT_DESTRUCTOR, decl->kind == asAST_DECL_DESTRUCTOR);
		if( !func->returnType.IsValid() )
		{
			if( decl->kind == asAST_DECL_CONSTRUCTOR || decl->kind == asAST_DECL_DESTRUCTOR )
			{
				func->returnType = asCDataType::CreatePrimitive(ttVoid, false);
			}
			else if( decl->traits & asAST_TRAIT_LAMBDA )
			{
				func->returnType = asCDataType::CreatePrimitive(ttInt, false);
			}
			else
			{
				error = asINVALID_DECLARATION;
				return false;
			}
		}

		// Parser-authored script declarations must expose the same maintained
		// Runtime ABI as Builder. Lambdas and generated accessors have no
		// Builder-owned source shell and intentionally keep their sealed direct
		// ABI (notably accessor ownership-transfer parameters).
		const bool usesCanonicalDirectFunctionABI =
			(decl->traits & asAST_TRAIT_LAMBDA) != 0
			|| (decl->kind == asAST_DECL_METHOD
				&& (decl->traits & asAST_TRAIT_GENERATED) != 0
				&& decl->accessorKind != asAST_ACCESSOR_NONE);
		for( asUINT i = 0; i < decl->children.GetLength(); ++i )
		{
			const asCDecl* child = context.GetDecl(decl->children[i]);
			if( child == 0 || child->kind != asAST_DECL_PARAM )
				continue;
			asCDataType paramType = bridge.Resolve(context, child->type);
			asETypeModifiers passing = InOutFromQuals(child->type.quals);
			if( !paramType.IsValid() || paramType.GetTokenType() == ttVoid )
			{
				error = asINVALID_TYPE;
				return false;
			}
			if( !usesCanonicalDirectFunctionABI )
			{
				asCRuntimeTypeBridge::NormalizeScriptParameterABI(
					paramType, passing);
			}
			func->parameterTypes.PushLast(paramType);
			func->parameterNames.PushLast(child->name);
			func->inOutFlags.PushLast(passing);
			func->defaultArgs.PushLast(0);
		}
		if( engine
			&& (decl->kind == asAST_DECL_METHOD
				|| decl->kind == asAST_DECL_CONSTRUCTOR
				|| decl->kind == asAST_DECL_DESTRUCTOR) )
		{
			const asCDecl* parent = context.GetDecl(decl->parent);
			if( parent && parent->kind == asAST_DECL_CLASS && parent->name.GetLength() )
			{
				asSNameSpace* const nameSpace = ResolveCanonicalDeclNamespace(context, engine, module, parent);
				if( nameSpace == 0 )
				{
					error = asOUT_OF_MEMORY;
					return false;
				}
				asCObjectType* objType = FindCanonicalObjectType(
					engine, module, parent->name, nameSpace, &artifact);
				if( objType )
				{
					func->objectType = objType;
					objType->AddRefInternal();
				}
			}
		}
		if( decl->traits & asAST_TRAIT_LAMBDA )
		{
			func->artifactInvocationKind = asBUILD_ARTIFACT_INVOCATION_LAMBDA;
		}
		else if( decl->kind == asAST_DECL_METHOD
			&& (decl->name.Equals("__InitDefaults")
				|| decl->origin.Equals("canonical-init-defaults")) )
		{
			func->artifactInvocationKind =
				asBUILD_ARTIFACT_INVOCATION_INIT_DEFAULTS;
		}
		else if( decl->kind == asAST_DECL_METHOD )
		{
			func->artifactInvocationKind = asBUILD_ARTIFACT_INVOCATION_METHOD;
		}
		else if( decl->kind == asAST_DECL_CONSTRUCTOR )
		{
			func->artifactInvocationKind =
				(decl->traits & asAST_TRAIT_GENERATED) != 0
				? asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_CONSTRUCTOR
				: asBUILD_ARTIFACT_INVOCATION_CONSTRUCTOR;
		}
		else if( decl->kind == asAST_DECL_DESTRUCTOR )
		{
			func->artifactInvocationKind =
				(decl->traits & asAST_TRAIT_GENERATED) != 0
				? asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_DESTRUCTOR
				: asBUILD_ARTIFACT_INVOCATION_DESTRUCTOR;
		}
		else
		{
			func->artifactInvocationKind = asBUILD_ARTIFACT_INVOCATION_GLOBAL_FUNCTION;
		}
		func->artifactOwnerType = func->objectType;
		func->CalculateParameterOffsets();
		return true;
	}

	bool CreateCanonicalConstructorFactory(
		asCScriptEngine* engine,
		asCModule* module,
		asSBytecodeCodeGenArtifact& artifact,
		asCScriptFunction* constructor)
	{
		if( engine == 0 || module == 0 || constructor == 0
			|| constructor->objectType == 0
			|| (constructor->objectType->flags & asOBJ_VALUE) != 0 )
		{
			return true;
		}

		asCScriptFunction* const factory = asNEW(asCScriptFunction)(engine, module, asFUNC_SCRIPT);
		if( factory == 0 )
			return false;
		factory->id = artifact.GetNextPendingScriptFunctionId(engine);
		if( factory->id < 0 )
		{
			factory->DestroyHalfCreated();
			return false;
		}
		factory->name = constructor->name;
		factory->nameSpace = constructor->nameSpace;
		factory->returnType = asCDataType::CreateObjectHandle(constructor->objectType, false);
		factory->parameterTypes = constructor->parameterTypes;
		factory->parameterNames = constructor->parameterNames;
		factory->inOutFlags = constructor->inOutFlags;
		factory->traits = constructor->traits;
		factory->traits.SetTrait(asTRAIT_UNSAFE_DURING_CONSTRUCTION, true);
		factory->canonicalASTStableDeclKey = constructor->canonicalASTStableDeclKey;
		factory->artifactInvocationKind = asBUILD_ARTIFACT_INVOCATION_FACTORY;
		factory->artifactOwnerType = constructor->objectType;
		if( factory->scriptData && constructor->scriptData )
		{
			factory->scriptData->scriptSectionIdx = constructor->scriptData->scriptSectionIdx;
			factory->scriptData->declaredAt = constructor->scriptData->declaredAt;
			factory->scriptData->artifactCanonicalSource =
				constructor->scriptData->artifactCanonicalSource;
		}
		for( asUINT i = 0; i < constructor->defaultArgs.GetLength(); ++i )
		{
			asCString* duplicate = 0;
			if( constructor->defaultArgs[i] )
			{
				duplicate = asNEW(asCString)(*constructor->defaultArgs[i]);
				if( duplicate == 0 )
				{
					factory->DestroyHalfCreated();
					return false;
				}
			}
			factory->defaultArgs.PushLast(duplicate);
		}
		factory->CalculateParameterOffsets();

		artifact.functions.PushLast(factory);
		artifact.functionDeclKinds.PushLast(asBYTECODE_CODEGEN_CONSTRUCTOR_FACTORY);
		asSCanonicalConstructorFactoryPair pair;
		pair.constructor = constructor;
		pair.factory = factory;
		pair.objectType = constructor->objectType;
		artifact.constructorFactories.PushLast(pair);
		return true;
	}

	bool EmitCanonicalConstructorFactory(
		asCBuilder& builder,
		const asSCanonicalConstructorFactoryPair& pair)
	{
		asCScriptFunction* const factory = pair.factory;
		asCScriptFunction* const constructor = pair.constructor;
		asCObjectType* const objectType = pair.objectType;
		if( factory == 0 || constructor == 0 || objectType == 0
			|| factory->scriptData == 0 || constructor->id <= 0 )
		{
			return false;
		}

		// ALLOC embeds both the concrete allocation type and the constructor
		// function. Publish those exact authorities while the factory's Cache V2
		// capture transaction is active.
		builder.MarkDependency(objectType, 0, 0);
		builder.MarkDependency(constructor, 0, 0);

		asCByteCode byteCode(&builder);
		const int objectSlot = AS_PTR_SIZE;
		byteCode.InstrPTR(asBC_JitEntry, 0);
		byteCode.InstrSHORT(asBC_PSF, (short)objectSlot);

		int offset = factory->GetSpaceNeededForArguments();
		for( int arg = (int)factory->parameterTypes.GetLength() - 1; arg >= 0; --arg )
		{
			const asCDataType& parameterType = factory->parameterTypes[arg];
			if( !parameterType.IsPrimitive() || parameterType.IsReference() )
			{
				offset -= AS_PTR_SIZE;
				byteCode.InstrSHORT(asBC_PshVPtr, (short)-offset);
			}
			else if( parameterType.GetSizeOnStackDWords() == 2 )
			{
				offset -= 2;
				byteCode.InstrSHORT(asBC_PshV8, (short)-offset);
			}
			else
			{
				offset -= 1;
				byteCode.InstrSHORT(asBC_PshV4, (short)-offset);
			}
		}

		const int argumentDwords = factory->GetSpaceNeededForArguments();
		byteCode.Alloc(
			asBC_ALLOC,
			objectType,
			constructor->id,
			argumentDwords + AS_PTR_SIZE,
			false);
		byteCode.InstrSHORT(asBC_LOADOBJ, (short)objectSlot);
		byteCode.Ret(argumentDwords);

		asCArray<int> temporaries;
		byteCode.Finalize(temporaries);
		byteCode.ExtractObjectVariableInfo(factory);
		factory->scriptData->variableSpace = AS_PTR_SIZE;
		factory->scriptData->byteCode.SetLength(byteCode.GetSize());
		if( byteCode.GetSize() == 0 )
			return false;
		byteCode.Output(factory->scriptData->byteCode.AddressOf());
		factory->scriptData->stackNeeded =
			byteCode.largestStackUsed + factory->scriptData->variableSpace;
		factory->scriptData->lineNumbers = byteCode.lineNumbers;
		factory->scriptData->objVariableTypes.PushLast(objectType);
		factory->scriptData->objVariablePos.PushLast(objectSlot);
		factory->scriptData->objVariablesOnHeap = 1;
		factory->dontCleanUpOnException = true;
		return true;
	}

	void ReleaseCanonicalBehaviourFunction(asCObjectType* objectType, int functionId)
	{
		if( objectType == 0 || objectType->engine == 0 || functionId <= 0
			|| (asUINT)functionId >= objectType->engine->scriptFunctions.GetLength() )
		{
			return;
		}
		asCScriptFunction* const old = objectType->engine->scriptFunctions[functionId];
		if( old )
			old->ReleaseInternal();
	}

	void AttachCanonicalObjectFunction(asCScriptFunction* func, int declKind)
	{
		if( func == 0 || func->objectType == 0 )
			return;

		asCObjectType* const objType = func->objectType;
		switch( static_cast<asEASTDeclKind>(declKind) )
		{
		case asAST_DECL_CONSTRUCTOR:
			if( func->parameterTypes.GetLength() == 0 )
			{
				if( objType->beh.constructors.GetLength() )
				{
					ReleaseCanonicalBehaviourFunction(objType, objType->beh.constructors[0]);
					objType->beh.constructors[0] = func->id;
				}
				else
				{
					objType->beh.constructors.PushLast(func->id);
				}
				objType->beh.construct = func->id;
			}
			else
				objType->beh.constructors.PushLast(func->id);
			func->AddRefInternal();
			break;
		case asAST_DECL_DESTRUCTOR:
			ReleaseCanonicalBehaviourFunction(objType, objType->beh.destruct);
			objType->beh.destruct = func->id;
			func->AddRefInternal();
			break;
		case asAST_DECL_METHOD:
			objType->methods.PushLast(func->id);
			objType->methodTable.Add(func);
			func->AddRefInternal();
			break;
		default:
			break;
		}
	}

	void AttachCanonicalConstructorFactory(
		const asSCanonicalConstructorFactoryPair& pair)
	{
		asCObjectType* const objectType = pair.objectType;
		asCScriptFunction* const factory = pair.factory;
		asCScriptFunction* const constructor = pair.constructor;
		if( objectType == 0 || factory == 0 || constructor == 0 )
			return;
		if( factory->parameterTypes.GetLength() == 0 )
		{
			if( objectType->beh.factories.GetLength() )
			{
				ReleaseCanonicalBehaviourFunction(objectType, objectType->beh.factories[0]);
				objectType->beh.factories[0] = factory->id;
			}
			else
			{
				objectType->beh.factories.PushLast(factory->id);
			}
			objectType->beh.factory = factory->id;
		}
		else
		{
			objectType->beh.factories.PushLast(factory->id);
		}
		factory->AddRefInternal();
	}

	int FinalizeOneCanonicalObjectInheritance(
		asSBytecodeCodeGenArtifact& artifact,
		asCScriptEngine* engine,
		asUINT index,
		asCArray<asBYTE>& states)
	{
		if( index >= artifact.types.GetLength() || index >= states.GetLength() )
			return asINVALID_DECLARATION;
		if( states[index] == 2 )
			return asSUCCESS;
		if( states[index] == 1 )
			return asINVALID_DECLARATION;
		states[index] = 1;
		asCObjectType* const type = CastToObjectType(artifact.types[index]);
		if( type == 0 )
		{
			states[index] = 2;
			return asSUCCESS;
		}

		asUINT baseArtifactIndex = asUINT(-1);
		for( asUINT candidate = 0; candidate < artifact.types.GetLength(); ++candidate )
		{
			if( artifact.types[candidate] == type->derivedFrom )
			{
				baseArtifactIndex = candidate;
				break;
			}
		}
		if( baseArtifactIndex != asUINT(-1) )
		{
			const int baseResult = FinalizeOneCanonicalObjectInheritance(
				artifact, engine, baseArtifactIndex, states);
			if( baseResult < 0 )
				return baseResult;
		}

		if( type->derivedFrom )
		{
			type->virtualFunctionTable = type->derivedFrom->virtualFunctionTable;
			for( asUINT baseMethodIndex = 0;
				baseMethodIndex < type->derivedFrom->methods.GetLength();
				++baseMethodIndex )
			{
				const int baseMethodId = type->derivedFrom->methods[baseMethodIndex];
				asCScriptFunction* const baseMethod = engine && baseMethodId >= 0
					&& (asUINT)baseMethodId < engine->scriptFunctions.GetLength()
					? engine->scriptFunctions[baseMethodId]
					: 0;
				if( baseMethod == 0 || baseMethod->funcType == asFUNC_SYSTEM )
					continue;

				asCScriptFunction* overrideMethod = 0;
				for( asUINT methodIndex = 0; methodIndex < type->methods.GetLength(); ++methodIndex )
				{
					const int methodId = type->methods[methodIndex];
					asCScriptFunction* const candidate = engine && methodId >= 0
						&& (asUINT)methodId < engine->scriptFunctions.GetLength()
						? engine->scriptFunctions[methodId]
						: 0;
					if( candidate == 0 || candidate->objectType != type
						|| candidate->name != baseMethod->name
						|| !candidate->IsSignatureExceptNameAndReturnTypeEqual(baseMethod) )
					{
						continue;
					}
					if( candidate->returnType != baseMethod->returnType )
					{
						if( baseMethod->name == "opConv" || baseMethod->name == "opImplConv"
							|| baseMethod->name == "opCast" || baseMethod->name == "opImplCast" )
						{
							continue;
						}
						return asINVALID_DECLARATION;
					}
					overrideMethod = candidate;
					break;
				}

				if( overrideMethod )
				{
					if( baseMethod->vfTableIdx < 0
						|| (asUINT)baseMethod->vfTableIdx >= type->virtualFunctionTable.GetLength() )
					{
						return asINVALID_DECLARATION;
					}
					overrideMethod->vfTableIdx = baseMethod->vfTableIdx;
					type->virtualFunctionTable[baseMethod->vfTableIdx] = overrideMethod;
				}
				else
				{
					type->methods.PushLast(baseMethod->id);
					type->methodTable.Add(baseMethod);
					baseMethod->AddRefInternal();
				}
			}
		}

		if( (type->flags & asOBJ_VALUE) == 0 )
		{
			for( asUINT methodIndex = 0; methodIndex < type->methods.GetLength(); ++methodIndex )
			{
				const int methodId = type->methods[methodIndex];
				asCScriptFunction* const method = engine && methodId >= 0
					&& (asUINT)methodId < engine->scriptFunctions.GetLength()
					? engine->scriptFunctions[methodId]
					: 0;
				if( method == 0 || method->funcType == asFUNC_SYSTEM
					|| method->objectType != type || method->vfTableIdx != -1 )
				{
					continue;
				}
				method->vfTableIdx = type->virtualFunctionTable.GetLength();
				type->virtualFunctionTable.PushLast(method);
			}
		}
		for( asUINT slot = 0; slot < type->virtualFunctionTable.GetLength(); ++slot )
		{
			if( type->virtualFunctionTable[slot] )
				type->virtualFunctionTable[slot]->AddRefInternal();
		}
		states[index] = 2;
		return asSUCCESS;
	}

	int FinalizeCanonicalObjectInheritance(
		asSBytecodeCodeGenArtifact& artifact,
		asCScriptEngine* engine)
	{
		asCArray<asBYTE> states;
		states.SetLength(artifact.types.GetLength());
		for( asUINT index = 0; index < artifact.types.GetLength(); ++index )
		{
			const int result = FinalizeOneCanonicalObjectInheritance(
				artifact, engine, index, states);
			if( result < 0 )
				return result;
		}
		return asSUCCESS;
	}
}

int asSBytecodeCodeGenArtifact::GetNextPendingScriptFunctionId(const asCScriptEngine* engine) const
{
	if( engine == 0 )
		return -1;

	const asUINT pendingCount = functions.GetLength();
	const asUINT freeCount = engine->freeScriptFunctionIds.GetLength();
	if( pendingCount < freeCount )
	{
		// The engine's allocator consumes free ids as a LIFO stack. Mirror that
		// order without modifying it so the committed artifact receives exactly
		// the ids it would have received under immediate registration.
		return engine->freeScriptFunctionIds[freeCount - pendingCount - 1];
	}
	return (int)(engine->scriptFunctions.GetLength() + pendingCount - freeCount);
}

int asSBytecodeCodeGenArtifact::GetNextPendingImportedFunctionId(const asCScriptEngine* engine) const
{
	if( engine == 0 )
		return -1;

	const asUINT pendingCount = imports.GetLength();
	const asUINT freeCount = engine->freeImportedFunctionIdxs.GetLength();
	if( pendingCount < freeCount )
	{
		return FUNC_IMPORTED | engine->freeImportedFunctionIdxs[freeCount - pendingCount - 1];
	}
	return FUNC_IMPORTED | (int)(engine->importedFunctions.GetLength() + pendingCount - freeCount);
}

int asSBytecodeCodeGenArtifact::GetNextPendingGlobalPropertyId(const asCScriptEngine* engine) const
{
	if( engine == 0 )
		return -1;

	const asUINT pendingCount = globals.GetLength();
	const asUINT freeCount = engine->freeGlobalPropertyIds.GetLength();
	if( pendingCount < freeCount )
	{
		// Global-property allocation also consumes reclaimed ids from a LIFO
		// stack. Reserve the prospective id without changing the engine until
		// the whole canonical artifact is ready to publish.
		return engine->freeGlobalPropertyIds[freeCount - pendingCount - 1];
	}
	return (int)(engine->globalProperties.GetLength() + pendingCount - freeCount);
}

int asSBytecodeCodeGenArtifact::PreparePendingTypeIds(asCScriptEngine* engine)
{
	if( engine == 0 )
		return asINVALID_ARG;
	if( pendingTypeIdsPublished )
		return asINVALID_CONFIGURATION;
	if( pendingTypeIdSequenceStart < 0 )
	{
		pendingTypeIdSequenceStart = engine->typeIdSeqNbr;
		pendingTypeIdCount = 0;
	}
	else if( engine->typeIdSeqNbr != pendingTypeIdSequenceStart )
	{
		return asINVALID_CONFIGURATION;
	}

	// Detached artifact-owned types always need a public Runtime projection on
	// successful publication. Keep ownership separate from the projection plan
	// so prepared Stage-2 types can join the same atomic sequence later.
	for( asUINT i = 0; i < types.GetLength(); ++i )
	{
		asCTypeInfo* const type = types[i];
		if( type == 0 )
			return asINVALID_TYPE;
		if( !pendingTypeIdTypes.Exists(type) )
		{
			if( type->typeId != -1 )
			{
				// A detached candidate must not have reached the Engine type-id map
				// through an eager GetTypeIdFromDataType() call.
				return asINVALID_CONFIGURATION;
			}
			pendingTypeIdTypes.PushLast(type);
		}
	}

	while( pendingTypeIdCount < pendingTypeIdTypes.GetLength() )
	{
		asCTypeInfo* const type = pendingTypeIdTypes[pendingTypeIdCount];
		if( type == 0 || type->typeId != -1 )
			return asINVALID_CONFIGURATION;
		int typeId = pendingTypeIdSequenceStart + (int)pendingTypeIdCount;
		if( type->flags & asOBJ_SCRIPT_OBJECT )
			typeId |= asTYPEID_SCRIPTOBJECT;
		else if( type->flags & asOBJ_TEMPLATE )
			typeId |= asTYPEID_TEMPLATE;
		else if( !(type->flags & asOBJ_ENUM) )
			typeId |= asTYPEID_APPOBJECT;
		type->typeId = typeId;
		++pendingTypeIdCount;
	}
	return asSUCCESS;
}

int asSBytecodeCodeGenArtifact::CommitPendingTypeIds(
	asCScriptEngine* engine)
{
	if( engine == 0 || pendingTypeIdSequenceStart < 0
		|| pendingTypeIdCount != pendingTypeIdTypes.GetLength() )
	{
		return asINVALID_CONFIGURATION;
	}
	const int expectedSequenceEnd = pendingTypeIdSequenceStart
		+ static_cast<int>(pendingTypeIdCount);
	if( pendingTypeIdsPublished )
	{
		if( engine->typeIdSeqNbr != expectedSequenceEnd )
			return asINVALID_CONFIGURATION;
		for( asUINT i = 0; i < pendingTypeIdTypes.GetLength(); ++i )
		{
			asCTypeInfo* const type = pendingTypeIdTypes[i];
			asCTypeInfo** const mapped = type
				? engine->mapTypeIdToTypeInfo.Find(type->typeId) : 0;
			if( mapped == 0 || *mapped != type )
				return asINVALID_CONFIGURATION;
		}
		return asSUCCESS;
	}
	if( engine->typeIdSeqNbr != pendingTypeIdSequenceStart )
		return asINVALID_CONFIGURATION;
	for( asUINT i = 0; i < pendingTypeIdTypes.GetLength(); ++i )
	{
		asCTypeInfo* const type = pendingTypeIdTypes[i];
		const int sequence = type
			? (type->typeId & asTYPEID_MASK_SEQNBR) : -1;
		if( type == 0
			|| sequence != pendingTypeIdSequenceStart + static_cast<int>(i)
			|| engine->mapTypeIdToTypeInfo.Contains(type->typeId) )
		{
			return asINVALID_CONFIGURATION;
		}
	}
	for( asUINT i = 0; i < pendingTypeIdTypes.GetLength(); ++i )
	{
		engine->mapTypeIdToTypeInfo.Add(
			pendingTypeIdTypes[i]->typeId, pendingTypeIdTypes[i]);
	}
	engine->typeIdSeqNbr = expectedSequenceEnd;
	pendingTypeIdsPublished = pendingTypeIdCount != 0;
	return asSUCCESS;
}

asUINT asSBytecodeCodeGenArtifact::GetOrAddRelocationFunction(
	asCScriptFunction* function)
{
	const int existing = relocationFunctions.IndexOf(function);
	if( existing >= 0 )
		return static_cast<asUINT>(existing);
	relocationFunctions.PushLast(function);
	return relocationFunctions.GetLength() - 1;
}

int asSBytecodeCodeGenArtifact::ResolveAndPatchTypeRelocations(
	asCScriptEngine* engine,
	const asCASTContext& context,
	const asSRuntimeTypeBindingEnvironment& environment,
	asCString* detail
#if WITH_ANGELSCRIPT_UNITTESTS
	, asUINT testFailureAtRelocationUse,
	int testFailureCode,
	bool* outTestFailureInjected
#endif
	)
{
#if WITH_ANGELSCRIPT_UNITTESTS
	if( outTestFailureInjected )
		*outTestFailureInjected = false;
#endif
	if( engine == 0 )
	{
		if( detail ) *detail = "invalid relocation engine";
		return asRUNTIME_TYPE_BIND_INVALID;
	}
	const int bindingResult = runtimeTypeBindings.Build(
		engine, context, typeRelocations, environment, &types, detail,
		&relocationFunctions);
	if( bindingResult != asRUNTIME_TYPE_BIND_OK )
		return bindingResult;

#if WITH_ANGELSCRIPT_UNITTESTS
	if( testFailureAtRelocationUse < asTYPE_RELOC_USE_COUNT )
	{
		for( asUINT i = 0; i < typeRelocations.GetLength(); ++i )
		{
			if( static_cast<asUINT>(typeRelocations[i].use)
				!= testFailureAtRelocationUse )
			{
				continue;
			}
			if( outTestFailureInjected )
				*outTestFailureInjected = true;
			if( detail )
			{
				detail->Format(
					"injected-type-relocation use=%u index=%u error=%d",
					testFailureAtRelocationUse, i, testFailureCode);
			}
			return testFailureCode < 0 ? testFailureCode : asERROR;
		}
		if( detail )
		{
			detail->Format(
				"requested type-relocation injection was not emitted: use=%u count=%u",
				testFailureAtRelocationUse, typeRelocations.GetLength());
		}
		return asRUNTIME_TYPE_BIND_INVALID;
	}
#endif

	// Validate every bytecode coordinate and opcode before patching the first
	// detached function. This preserves validate-all/commit-none even if a
	// producer emits a malformed relocation record.
	for( asUINT i = 0; i < typeRelocations.GetLength(); ++i )
	{
		const asSTypeRelocation& relocation = typeRelocations[i];
		if( relocation.functionArtifactIndex >= relocationFunctions.GetLength() )
		{
			if( detail ) detail->Format("relocation[%u] has invalid function index %u", i, relocation.functionArtifactIndex);
			return asRUNTIME_TYPE_BIND_INVALID;
		}
		asCScriptFunction* const function =
			relocationFunctions[relocation.functionArtifactIndex];
		if( function == 0 || function->scriptData == 0
			|| relocation.bytecodeDwordOffset
				>= function->scriptData->byteCode.GetLength() )
		{
			if( detail ) detail->Format("relocation[%u] has invalid bytecode coordinate function=%u offset=%u", i, relocation.functionArtifactIndex, relocation.bytecodeDwordOffset);
			return asRUNTIME_TYPE_BIND_INVALID;
		}
		const asDWORD* const instruction =
			function->scriptData->byteCode.AddressOf()
			+ relocation.bytecodeDwordOffset;
		const asEBCInstr opcode = static_cast<asEBCInstr>(
			*reinterpret_cast<const asBYTE*>(instruction));
		if( relocation.use == asTYPE_RELOC_PROPERTY )
		{
			if( opcode != asBC_ADDSi && opcode != asBC_LoadThisR
				&& opcode != asBC_LoadRObjR && opcode != asBC_LoadVObjR )
			{
				if( detail ) detail->Format("relocation[%u] property opcode mismatch: opcode=%u", i, static_cast<unsigned int>(opcode));
				return asRUNTIME_TYPE_BIND_INVALID;
			}
		}
		else if( relocation.use == asTYPE_RELOC_METADATA_ONLY )
		{
			if( opcode != asBC_COPY )
			{
				if( detail ) detail->Format("relocation[%u] metadata opcode mismatch: opcode=%u", i, static_cast<unsigned int>(opcode));
				return asRUNTIME_TYPE_BIND_INVALID;
			}
		}
		else if( relocation.use == asTYPE_RELOC_PUBLIC_TYPE_ID
			|| relocation.use == asTYPE_RELOC_RUNTIME_TYPE_TARGET )
		{
			const bool isPublicIdOpcode = opcode == asBC_TYPEID
				|| opcode == asBC_Cast || opcode == asBC_SetListType;
			const bool isRuntimeTargetOpcode = opcode == asBC_ALLOC
				|| opcode == asBC_FREE || opcode == asBC_REFCPY;
			if( (relocation.use == asTYPE_RELOC_PUBLIC_TYPE_ID
					&& !isPublicIdOpcode)
				|| (relocation.use == asTYPE_RELOC_RUNTIME_TYPE_TARGET
					&& !isPublicIdOpcode && !isRuntimeTargetOpcode) )
			{
				if( detail ) detail->Format("relocation[%u] type-target opcode mismatch: use=%u opcode=%u", i, static_cast<unsigned int>(relocation.use), static_cast<unsigned int>(opcode));
				return asRUNTIME_TYPE_BIND_INVALID;
			}
		}
		else if( relocation.use == asTYPE_RELOC_FUNCTION )
		{
			if( opcode != asBC_CALL && opcode != asBC_CALLSYS
				&& opcode != asBC_Thiscall1 && opcode != asBC_CALLBND
				&& opcode != asBC_CALLINTF && opcode != asBC_FuncPtr
				&& opcode != asBC_ALLOC )
			{
				if( detail ) detail->Format("relocation[%u] function opcode mismatch: opcode=%u", i, static_cast<unsigned int>(opcode));
				return asRUNTIME_TYPE_BIND_INVALID;
			}
		}
		else if( relocation.use == asTYPE_RELOC_LIST_PATTERN_TARGET )
		{
			if( opcode != asBC_FREE )
			{
				if( detail ) detail->Format(
					"relocation[%u] list-pattern opcode mismatch: opcode=%u",
					i, static_cast<unsigned int>(opcode));
				return asRUNTIME_TYPE_BIND_INVALID;
			}
		}
	}

	// Stable target/ABI resolution is complete, and every relocation coordinate
	// is known-valid. Only now extend the candidate-local numeric projection plan
	// for operands that actually carry a public TypeId. Prepared-module TypeInfos
	// are non-owning entries here; canonical identity remains in expectedType.
	for( asUINT i = 0; i < typeRelocations.GetLength(); ++i )
	{
		const asSTypeRelocation& relocation = typeRelocations[i];
		const asSResolvedRuntimeTypeBinding* const binding =
			runtimeTypeBindings.GetBinding(i);
		asCScriptFunction* const function =
			relocationFunctions[relocation.functionArtifactIndex];
		const asDWORD* const instruction = function->scriptData->byteCode.AddressOf()
			+ relocation.bytecodeDwordOffset;
		const asEBCInstr opcode = static_cast<asEBCInstr>(
			*reinterpret_cast<const asBYTE*>(instruction));
		const bool needsPublicTypeId =
			relocation.use == asTYPE_RELOC_PROPERTY
			|| relocation.use == asTYPE_RELOC_METADATA_ONLY
			|| relocation.use == asTYPE_RELOC_PUBLIC_TYPE_ID
			|| (relocation.use == asTYPE_RELOC_RUNTIME_TYPE_TARGET
				&& (opcode == asBC_TYPEID || opcode == asBC_Cast
					|| opcode == asBC_SetListType));
		if( needsPublicTypeId && binding != 0 && binding->publicTypeId < 0
			&& binding->typeInfo != 0
			&& !pendingTypeIdTypes.Exists(binding->typeInfo) )
		{
			pendingTypeIdTypes.PushLast(binding->typeInfo);
		}
	}
	const int prepareResult = PreparePendingTypeIds(engine);
	if( prepareResult < 0 )
	{
		if( detail ) detail->Format(
			"failed to prepare pending type ids: result=%d", prepareResult);
		return prepareResult;
	}
	const int projectionResult =
		runtimeTypeBindings.RefreshPublicTypeIdProjections(engine);
	if( projectionResult != asRUNTIME_TYPE_BIND_OK )
	{
		if( detail ) detail->Format(
			"failed to refresh pending public type ids: result=%d",
			projectionResult);
		return projectionResult;
	}

	for( asUINT i = 0; i < typeRelocations.GetLength(); ++i )
	{
		const asSTypeRelocation& relocation = typeRelocations[i];
		const asSResolvedRuntimeTypeBinding* const binding =
			runtimeTypeBindings.GetBinding(i);
		if( binding == 0 )
		{
			if( detail ) detail->Format("relocation[%u] has no frozen binding", i);
			return asRUNTIME_TYPE_BIND_INVALID;
		}
		asDWORD* const instruction =
			relocationFunctions[relocation.functionArtifactIndex]
				->scriptData->byteCode.AddressOf()
			+ relocation.bytecodeDwordOffset;
		const asEBCInstr opcode = static_cast<asEBCInstr>(
			*reinterpret_cast<const asBYTE*>(instruction));
		const bool needsPublicTypeId =
			relocation.use == asTYPE_RELOC_PROPERTY
			|| relocation.use == asTYPE_RELOC_METADATA_ONLY
			|| relocation.use == asTYPE_RELOC_PUBLIC_TYPE_ID
			|| (relocation.use == asTYPE_RELOC_RUNTIME_TYPE_TARGET
				&& (opcode == asBC_TYPEID || opcode == asBC_Cast
					|| opcode == asBC_SetListType));
		if( needsPublicTypeId && binding->publicTypeId < 0 )
		{
			if( detail ) detail->Format("relocation[%u] requires unpublished public type id: %s", i, binding->key.stableTypeKey.AddressOf() ? binding->key.stableTypeKey.AddressOf() : "");
			return asRUNTIME_TYPE_BIND_INVALID;
		}
		if( relocation.use == asTYPE_RELOC_PROPERTY )
		{
			if( binding->property == 0 || binding->propertyOffset < 0
				|| binding->propertyOffset > 0x7fff )
				return asRUNTIME_TYPE_BIND_MEMBER_MISSING;
			if( opcode == asBC_LoadRObjR || opcode == asBC_LoadVObjR )
			{
				asBC_SWORDARG1(instruction) =
					static_cast<short>(binding->propertyOffset);
				*reinterpret_cast<int*>(instruction + 2) =
					binding->publicTypeId;
			}
			else
			{
				asBC_SWORDARG0(instruction) =
					static_cast<short>(binding->propertyOffset);
				asBC_INTARG(instruction) = binding->publicTypeId;
			}
		}
		else if( relocation.use == asTYPE_RELOC_FUNCTION )
		{
			if( binding->function == 0 )
				return asRUNTIME_TYPE_BIND_MEMBER_MISSING;
			if( opcode == asBC_CALLSYS || opcode == asBC_Thiscall1
				|| opcode == asBC_FuncPtr )
			{
				asBC_PTRARG(instruction) =
					reinterpret_cast<asPWORD>(binding->function);
			}
			else if( opcode == asBC_ALLOC )
			{
				if( binding->typeInfo == 0 )
					return asRUNTIME_TYPE_BIND_INVALID;
				asBC_PTRARG(instruction) =
					reinterpret_cast<asPWORD>(binding->typeInfo);
				asBC_INTARG(instruction + AS_PTR_SIZE) =
					binding->function->id;
			}
			else
			{
				asBC_INTARG(instruction) = binding->function->id;
			}
		}
		else if( relocation.use == asTYPE_RELOC_LIST_PATTERN_TARGET )
		{
			asCObjectType* const ownerType =
				CastToObjectType(binding->typeInfo);
			asCObjectType* functionOwner = binding->function
				? binding->function->objectType : 0;
			if( functionOwner == 0 && binding->function )
			{
				functionOwner = CastToObjectType(
					binding->function->returnType.GetTypeInfo());
			}
			if( ownerType == 0 || binding->function == 0
				|| functionOwner != ownerType
				|| ownerType->beh.listFactory != binding->function->id
				|| binding->function->listPattern == 0 )
			{
				if( detail ) detail->Format(
					"relocation[%u] invalid list-factory target: %s",
					i,
					relocation.stableMemberKey.AddressOf()
						? relocation.stableMemberKey.AddressOf() : "");
				return asRUNTIME_TYPE_BIND_MEMBER_MISSING;
			}
			bool created = false;
			asCObjectType* const listPatternType =
				engine->GetListPatternTypeForObjectType(
					ownerType, &created);
			if( listPatternType == 0
				|| (listPatternType->flags & asOBJ_LIST_PATTERN) == 0 )
			{
				return asRUNTIME_TYPE_BIND_INVALID;
			}
			if( created && !createdListPatternTypes.Exists(listPatternType) )
			{
				createdListPatternTypes.PushLast(listPatternType);
			}
			asBC_PTRARG(instruction) =
				reinterpret_cast<asPWORD>(listPatternType);
		}
		else if( relocation.use == asTYPE_RELOC_RUNTIME_TYPE_TARGET
			&& (opcode == asBC_ALLOC || opcode == asBC_FREE
				|| opcode == asBC_REFCPY) )
		{
			if( binding->typeInfo == 0 )
				return asRUNTIME_TYPE_BIND_INVALID;
			asBC_PTRARG(instruction) =
				reinterpret_cast<asPWORD>(binding->typeInfo);
		}
		else if( opcode == asBC_SetListType )
		{
			*reinterpret_cast<int*>(instruction + 2) =
				binding->publicTypeId;
		}
		else
		{
			asBC_INTARG(instruction) = binding->publicTypeId;
		}
	}
	return asRUNTIME_TYPE_BIND_OK;
}

void asSBytecodeCodeGenArtifact::Abandon(asCScriptEngine* engine)
{
	// List-pattern cleanup helpers are anonymous Engine cache entries created
	// while patching a validated candidate. Remove only helpers this artifact
	// materialized; shared pre-existing entries are not candidate state.
	if( engine )
	{
		for( asUINT i = createdListPatternTypes.GetLength(); i > 0; --i )
		{
			asCObjectType* const type = createdListPatternTypes[i - 1];
			const int index = engine->listPatternTypes.IndexOf(type);
			if( index >= 0 )
			{
				engine->listPatternTypes.RemoveIndex(static_cast<asUINT>(index));
				type->ReleaseInternal();
			}
		}
	}
	createdListPatternTypes.SetLength(0);

	// Property-owned initializer functions hold a second internal reference
	// through their global property. Release properties before candidate
	// functions so every initializer is back to its single artifact-owned
	// reference before DestroyHalfCreated() runs below.
	for( asUINT i = globals.GetLength(); i > 0; --i )
	{
		asCGlobalProperty* prop = globals[i - 1];
		if( prop == 0 )
			continue;

		asCModule* owner = prop->module;
		const asUINT propId = prop->id;
		// Commit() publishes namespaced and generated-static globals to the
		// automatic-import index. That index is non-owning, so remove the entry
		// before RemoveGlobalVar() can release the property during a partial
		// Commit rollback.
		if( engine )
			engine->allScriptGlobalVariables.Remove(prop);
		bool wasPublished = false;
		if( owner )
		{
			const int index = owner->scriptGlobalsList.IndexOf(prop);
			if( index >= 0 )
			{
				owner->RemoveGlobalVar((asUINT)index);
				wasPublished = true;
			}
		}
		if( !wasPublished && engine
			&& propId < engine->globalProperties.GetLength()
			&& engine->globalProperties[propId] == prop )
		{
			engine->RemoveGlobalProperty(prop);
			wasPublished = true;
		}
		if( !wasPublished )
		{
			// Candidate globals have a real value address for emitted LDG
			// instructions but have not entered the module or engine yet.
			// Their constructor reference belongs solely to the artifact.
			prop->DestroyInternal();
			prop->Release();
			continue;
		}

		// RemoveGlobalProperty holes the slot and records a free id. If this
		// property was the last engine slot, shrink it so a failed Generate
		// restores both the visible table and the allocator state exactly.
		if( engine
			&& engine->freeGlobalPropertyIds.GetLength()
			&& engine->freeGlobalPropertyIds[engine->freeGlobalPropertyIds.GetLength() - 1] == (int)propId
			&& propId + 1 == engine->globalProperties.GetLength() )
		{
			engine->freeGlobalPropertyIds.PopLast();
			engine->globalProperties.SetLength(propId);
		}
	}
	globals.SetLength(0);

	// Imported declarations publish a module bind record and an engine import
	// slot before function bodies are emitted. They are provisional just like
	// pending script functions: erase the module owner before releasing the
	// signature, whose destructor removes the matching engine import slot.
	for( asUINT i = imports.GetLength(); i > 0; --i )
	{
		sBindInfo* info = imports[i - 1];
		if( info == 0 )
			continue;

		asCScriptFunction* func = info->importedFunctionSignature;
		if( func && func->module )
			func->module->bindInformations.RemoveValue(info);
		if( func )
			func->ReleaseInternal();
		asDELETE(info, sBindInfo);
	}
	imports.SetLength(0);

	// A partial Commit after function publication has already materialized all
	// bytecode/signature references. Reverse those edges while every referenced
	// engine slot is still present. DestroyHalfCreated() deliberately assumes a
	// pristine construction reference and cannot perform this committed rollback.
	if( functionReferencesAdded )
	{
		for( asUINT i = functions.GetLength(); i > 0; --i )
		{
			asCScriptFunction* const func = functions[i - 1];
			const int kind = i - 1 < functionDeclKinds.GetLength()
				? functionDeclKinds[i - 1]
				: asBYTECODE_CODEGEN_GLOBAL_INITIALIZER;
			if( func && kind != asBYTECODE_CODEGEN_GLOBAL_INITIALIZER )
				func->ReleaseReferences();
		}
		functionReferencesAdded = false;
	}

	// Object behaviour arrays, method lists, and virtual tables each own their
	// own internal function references. Detach them before deleting candidate
	// functions; DestroyInternal() later sees empty lists and remains idempotent.
	for( asUINT i = types.GetLength(); i > 0; --i )
	{
		if( asCObjectType* const objectType = CastToObjectType(types[i - 1]) )
			objectType->ReleaseAllFunctions();
	}

	for( asUINT i = functions.GetLength(); i > 0; --i )
	{
		asCScriptFunction* func = functions[i - 1];
		if( func == 0 )
			continue;
		if( func->scriptData )
			func->scriptData->byteCode.SetLength(0);

		// A Commit-stage failure may occur after the function has entered every
		// module/engine publication index. Remove these non-owning lookup entries
		// before DestroyHalfCreated() tears down the candidate. The ordinary
		// pre-Commit path does not populate them, so the removals are deliberately
		// idempotent.
		asCModule* owner = func->module;
		if( engine && func->objectType == 0 )
			engine->allScriptGlobalFunctions.Remove(func);
		if( owner )
		{
			owner->scriptFunctions.RemoveValue(func);
			if( func->objectType == 0 )
			{
				owner->globalFunctions.Remove(func);
				owner->globalFunctionList.RemoveValue(func);
			}
		}

		// Current candidate functions are deliberately not registered until
		// Commit(). Keep this defensive check for a future partially-committed
		// error path; never remove a slot that does not contain this function.
		if( engine && func->id >= 0 && (asUINT)func->id < engine->scriptFunctions.GetLength()
			&& engine->scriptFunctions[func->id] == func )
		{
			engine->RemoveScriptFunction(func);
		}
		func->DestroyHalfCreated();
	}
	functions.SetLength(0);
	functionDeclKinds.SetLength(0);
	constructorFactories.SetLength(0);

	// Numeric TypeIds are a late, reversible projection. Remove any partially
	// published map entries and restore every planned TypeInfo (including
	// prepared Stage-2 non-owning types) before artifact-owned types are
	// destroyed below. Canonical stable identity is unaffected by this rollback.
	if( engine && pendingTypeIdSequenceStart >= 0 )
	{
		for( asUINT i = 0; i < pendingTypeIdTypes.GetLength(); ++i )
		{
			asCTypeInfo* const type = pendingTypeIdTypes[i];
			if( type == 0 )
				continue;
			const int expectedSequence = pendingTypeIdSequenceStart
				+ static_cast<int>(i);
			if( (type->typeId & asTYPEID_MASK_SEQNBR) != expectedSequence )
				continue;
			if( pendingTypeIdsPublished )
			{
				asCTypeInfo** const mapped =
					engine->mapTypeIdToTypeInfo.Find(type->typeId);
				if( mapped != 0 && *mapped == type )
					engine->mapTypeIdToTypeInfo.Remove(type->typeId);
			}
			type->typeId = -1;
		}
	}

	// Script object types are detached candidates. They may hold references from
	// candidate function signatures and fields, so release functions first, then
	// tear down type internals. Keep the owner/index removal defensive for a
	// future partial-Commit error path; ordinary candidate abandonment has not
	// inserted these types into any module or engine lookup table.
	for( asUINT i = types.GetLength(); i > 0; --i )
	{
		asCTypeInfo* type = types[i - 1];
		if( type == 0 )
			continue;

		asCModule* owner = type->module;
		if( owner )
		{
			if( asCObjectType* objectType = CastToObjectType(type) )
				owner->classTypes.RemoveValue(objectType);
			else if( asCEnumType* enumType = CastToEnumType(type) )
				owner->enumTypes.RemoveValue(enumType);
			else if( asCTypedefType* typedefType = CastToTypedefType(type) )
				owner->typeDefs.RemoveValue(typedefType);
			owner->allLocalTypes.Remove(type);
		}
		if( engine )
			engine->allScriptDeclaredTypes.Remove(type);

		type->DestroyInternal();
		type->module = 0;
		type->ReleaseInternal();
	}
	funcdefs.SetLength(0);
	types.SetLength(0);
	if( pendingTypeIdsPublished && engine
		&& pendingTypeIdSequenceStart >= 0
		&& engine->typeIdSeqNbr
			== pendingTypeIdSequenceStart + (int)pendingTypeIdCount )
	{
		engine->typeIdSeqNbr = pendingTypeIdSequenceStart;
	}
	typeRelocations.SetLength(0);
	relocationFunctions.SetLength(0);
	pendingTypeIdTypes.SetLength(0);
	pendingTypeIdSequenceStart = -1;
	pendingTypeIdCount = 0;
	pendingTypeIdsPublished = false;
}

int asSBytecodeCodeGenArtifact::CommitDetachedSingleFunction(
	asCModule* diagnosticOwner,
	asCScriptFunction* primaryFunction)
{
	if( diagnosticOwner == 0 || diagnosticOwner->engine == 0
		|| primaryFunction == 0 )
	{
		return asINVALID_ARG;
	}
	// A public CompileFunction source cannot introduce module-level types,
	// globals, imports, or funcdefs. Reject such an artifact before publishing
	// any Engine slot instead of accidentally broadening the API contract.
	if( types.GetLength() != 0 || globals.GetLength() != 0
		|| imports.GetLength() != 0 || funcdefs.GetLength() != 0
		|| constructorFactories.GetLength() != 0 )
	{
		return asINVALID_DECLARATION;
	}

	asCScriptEngine* const engine = diagnosticOwner->engine;
	bool foundPrimary = false;
	for( asUINT i = 0; i < functions.GetLength(); ++i )
	{
		asCScriptFunction* const function = functions[i];
		const asUINT freeCount = engine->freeScriptFunctionIds.GetLength();
		const int expectedId = i < freeCount
			? engine->freeScriptFunctionIds[freeCount - i - 1]
			: (int)(engine->scriptFunctions.GetLength() + i - freeCount);
		if( function == 0 || function->id != expectedId
			|| function->module != diagnosticOwner )
		{
			return asERROR;
		}
		foundPrimary |= function == primaryFunction;
	}
	if( !foundPrimary )
	{
		return asNO_FUNCTION;
	}
	const int typeIdCommitResult = CommitPendingTypeIds(engine);
	if( typeIdCommitResult < 0 )
		return typeIdCommitResult;

	// Make every id resolvable before AddReferences walks cross-function calls
	// and lambda FuncPtr operands. No module lookup is mutated in detached mode.
	for( asUINT i = 0; i < functions.GetLength(); ++i )
	{
		engine->AddScriptFunction(functions[i]);
	}
	for( asUINT i = 0; i < functions.GetLength(); ++i )
	{
		functions[i]->AddReferences();
	}
	functionReferencesAdded = true;

	// Detached functions must satisfy asCScriptFunction::Release(): once the
	// caller's external and temporary internal references reach zero, module==0
	// allows normal self-destruction and Engine-slot removal.
	for( asUINT i = 0; i < functions.GetLength(); ++i )
	{
		functions[i]->module = 0;
	}
	for( asUINT i = 0; i < functions.GetLength(); ++i )
	{
		if( functions[i] != primaryFunction )
		{
			// Nested functions are retained only by real function references after
			// commit; their artifact construction reference is not a hidden owner.
			functions[i]->ReleaseInternal();
		}
	}

	diagnosticOwner->SetLastBytecodePublisher(
		asBYTECODE_PUBLISHER_CANONICAL_CODEGEN);
	functionReferencesAdded = false;
	functions.SetLength(0);
	functionDeclKinds.SetLength(0);
	constructorFactories.SetLength(0);
	return asAST_VERIFY_OK;
}

int asSBytecodeCodeGenArtifact::Commit(
	asCModule* module,
	bool publishAggregateRuntimeBindings
#if WITH_ANGELSCRIPT_UNITTESTS
	, asUINT testFailureAfterCommitStage, int testFailureCode,
	bool* outTestFailureInjected
#endif
)
{
	if( module == 0 || module->engine == 0 )
		return asNO_MODULE;
	asCScriptEngine* const engine = module->engine;
	if( publishAggregateRuntimeBindings )
	{
		const int generationResult =
			module->EnsureCanonicalRuntimeTypeGeneration();
		if( generationResult < 0 )
			return generationResult;
	}

#if WITH_ANGELSCRIPT_UNITTESTS
	if( outTestFailureInjected )
		*outTestFailureInjected = false;
	auto InjectAfterCommitStage = [&](asUINT stage) -> int
	{
		if( testFailureAfterCommitStage != stage )
			return asAST_VERIFY_OK;
		if( outTestFailureInjected )
			*outTestFailureInjected = true;
		return testFailureCode < 0 ? testFailureCode : asERROR;
	};
#endif

	// Script types stay detached while signatures and bodies are lowered. Check
	// their namespace/name ownership before publishing any table so a future
	// caller that violates the build-lock contract still fails closed.
	for( asUINT i = 0; i < types.GetLength(); ++i )
	{
		asCTypeInfo* type = types[i];
		const bool isObjectType = CastToObjectType(type) != 0;
		const bool isEnumType = CastToEnumType(type) != 0;
		const bool isTypedefType = CastToTypedefType(type) != 0;
		if( type == 0 || type->module != module || type->nameSpace == 0
			|| (!isObjectType && !isEnumType && !isTypedefType) )
			return asERROR;
		// Script typedefs are module inventory entries in this fork and do not
		// enter allScriptDeclaredTypes. Classes and enums retain the established
		// engine-wide collision contract.
		if( !isTypedefType )
		{
			asCTypeInfo* const existing = isEnumType
				? FindRegisteredHostTypeInNamespace(engine, type->name, type->nameSpace)
				: FindRegisteredTypeInNamespace(engine, type->name, type->nameSpace);
			if( existing != 0 )
				return asERROR;
		}
	}
	if( pendingTypeIdSequenceStart < 0
		|| engine->typeIdSeqNbr != pendingTypeIdSequenceStart
		|| pendingTypeIdCount != pendingTypeIdTypes.GetLength() )
	{
		return asINVALID_CONFIGURATION;
	}
	for( asUINT i = 0; i < pendingTypeIdTypes.GetLength(); ++i )
	{
		asCTypeInfo* const type = pendingTypeIdTypes[i];
		const int sequence = type
			? (type->typeId & asTYPEID_MASK_SEQNBR) : -1;
		if( type == 0
			|| sequence != pendingTypeIdSequenceStart + (int)i
			|| engine->mapTypeIdToTypeInfo.Contains(type->typeId) )
		{
			return asINVALID_CONFIGURATION;
		}
	}

	// Validate all candidate global slots before publishing any owner table.
	// Function AddReferences() below resolves LDG addresses through
	// varAddressMap, so globals must be installed before script functions.
	for( asUINT i = 0; i < globals.GetLength(); ++i )
	{
		asCGlobalProperty* prop = globals[i];
		const asUINT freeCount = engine->freeGlobalPropertyIds.GetLength();
		const int expectedId = i < freeCount
			? engine->freeGlobalPropertyIds[freeCount - i - 1]
			: (int)(engine->globalProperties.GetLength() + i - freeCount);
		if( prop == 0 || prop->module != module || prop->id != (asUINT)expectedId )
			return asERROR;
	}

	// Validate the deterministic import-slot plan before mutating either owner.
	// The engine build lock makes a concurrent allocator change impossible, but
	// this keeps Commit() fail-closed if a future caller violates that contract.
	for( asUINT i = 0; i < imports.GetLength(); ++i )
	{
		sBindInfo* info = imports[i];
		asCScriptFunction* func = info ? info->importedFunctionSignature : 0;
		const asUINT freeCount = engine->freeImportedFunctionIdxs.GetLength();
		const int expectedId = i < freeCount
			? FUNC_IMPORTED | engine->freeImportedFunctionIdxs[freeCount - i - 1]
			: FUNC_IMPORTED | (int)(engine->importedFunctions.GetLength() + i - freeCount);
		if( func == 0 || func->id != expectedId )
			return asERROR;
	}

	// Publish the complete late TypeId projection before properties/functions.
	// The projection may include prepared Stage-2 TypeInfos not owned by this
	// artifact; only the `types` loop below transfers module type ownership.
	const int typeIdCommitResult = CommitPendingTypeIds(engine);
	if( typeIdCommitResult < 0 )
		return typeIdCommitResult;
	for( asUINT i = 0; i < types.GetLength(); ++i )
	{
		asCTypeInfo* type = types[i];
		if( asCObjectType* objectType = CastToObjectType(type) )
		{
			engine->allScriptDeclaredTypes.Add(type);
			module->classTypes.PushLast(objectType);
		}
		else if( asCEnumType* enumType = CastToEnumType(type) )
		{
			engine->allScriptDeclaredTypes.Add(type);
			module->enumTypes.PushLast(enumType);
		}
		else if( asCTypedefType* typedefType = CastToTypedefType(type) )
		{
			module->typeDefs.PushLast(typedefType);
		}
		module->allLocalTypes.Add(type);
	}
#if WITH_ANGELSCRIPT_UNITTESTS
	if( const int injected = InjectAfterCommitStage(asBYTECODE_CODEGEN_TEST_COMMIT_AFTER_TYPES); injected < 0 )
		return injected;
#endif

	// Candidate global properties own their storage already, but not any
	// externally visible lookup entry. This reproduces the publication half of
	// asCModule::AllocateGlobalProperty() atomically after all CodeGen work has
	// succeeded.
	for( asUINT i = 0; i < globals.GetLength(); ++i )
	{
		asCGlobalProperty* prop = globals[i];
		const int propId = (int)prop->id;
		if( engine->freeGlobalPropertyIds.GetLength()
			&& engine->freeGlobalPropertyIds[engine->freeGlobalPropertyIds.GetLength() - 1] == propId )
		{
			engine->freeGlobalPropertyIds.PopLast();
			engine->globalProperties[propId] = prop;
		}
		else if( propId == (int)engine->globalProperties.GetLength() )
		{
			engine->globalProperties.PushLast(prop);
		}
		else
		{
			asASSERT(false);
			return asERROR;
		}
		engine->varAddressMap.Add(prop->GetAddressOfValue(), prop);
		// Mirror asCBuilder::BuildAllocateGlobalVariables(): root-namespace
		// globals stay module-local, while namespaced globals and generated
		// static-type slots are available to the established automatic-import
		// lookup. This remains in Commit() so a failed candidate cannot leak
		// a cross-module symbol before its complete generation is published.
		if( prop->nameSpace != module->defaultNamespace || prop->name.StartsWith("__StaticType_") )
			engine->allScriptGlobalVariables.Add(prop);
		module->scriptGlobals.Add(prop);
		module->scriptGlobalsList.PushLast(prop);
		prop->AddRef();
	}
#if WITH_ANGELSCRIPT_UNITTESTS
	if( const int injected = InjectAfterCommitStage(asBYTECODE_CODEGEN_TEST_COMMIT_AFTER_GLOBALS); injected < 0 )
		return injected;
#endif
	for( asUINT i = 0; i < imports.GetLength(); ++i )
	{
		sBindInfo* info = imports[i];
		asCScriptFunction* func = info->importedFunctionSignature;
		const int importIndex = func->id & ~FUNC_IMPORTED;
		module->bindInformations.PushLast(info);
		if( engine->freeImportedFunctionIdxs.GetLength()
			&& engine->freeImportedFunctionIdxs[engine->freeImportedFunctionIdxs.GetLength() - 1] == importIndex )
		{
			engine->freeImportedFunctionIdxs.PopLast();
			engine->importedFunctions[importIndex] = info;
		}
		else if( importIndex == (int)engine->importedFunctions.GetLength() )
		{
			engine->importedFunctions.PushLast(info);
		}
		else
			asASSERT(false);
	}
#if WITH_ANGELSCRIPT_UNITTESTS
	if( const int injected = InjectAfterCommitStage(asBYTECODE_CODEGEN_TEST_COMMIT_AFTER_IMPORTS); injected < 0 )
		return injected;
#endif

	// This is the only normal publication point for candidate script functions.
	// Every id was calculated from the unchanged engine allocator above, so the
	// complete function graph becomes visible together only after all bodies
	// have emitted successfully.
	for( asUINT i = 0; i < functions.GetLength(); ++i )
	{
		asCScriptFunction* func = functions[i];
		if( func == 0 )
			continue;
		engine->AddScriptFunction(func);
	}
	for( asUINT i = 0; i < functions.GetLength(); ++i )
	{
		asCScriptFunction* func = functions[i];
		if( func == 0 )
			continue;
		const int declKind = i < functionDeclKinds.GetLength()
			? functionDeclKinds[i]
			: -1;
		if( declKind == asBYTECODE_CODEGEN_GLOBAL_INITIALIZER )
		{
			// The global property owns the installed initializer, but retain the
			// artifact construction reference until every later Commit stage has
			// succeeded. A rollback first releases the property and then safely
			// destroys this still-owned candidate function.
			continue;
		}
		func->AddReferences();
		module->scriptFunctions.PushLast(func);
		if( declKind == asBYTECODE_CODEGEN_CONSTRUCTOR_FACTORY )
		{
			continue;
		}
		AttachCanonicalObjectFunction(func, declKind);
		if( func->objectType == 0
			&& func->artifactInvocationKind != asBUILD_ARTIFACT_INVOCATION_LAMBDA )
		{
			module->globalFunctions.Add(func);
			module->globalFunctionList.PushLast(func);
			// The Builder publishes normal script globals here too. Automatic
			// imports consume this engine-level map, so omitting it would make a
			// successfully promoted Canonical producer invisible to later modules.
			engine->allScriptGlobalFunctions.Add(func);
		}
	}
	functionReferencesAdded = true;
	for( asUINT i = 0; i < constructorFactories.GetLength(); ++i )
		AttachCanonicalConstructorFactory(constructorFactories[i]);
	const int inheritanceResult = FinalizeCanonicalObjectInheritance(*this, engine);
	if( inheritanceResult < 0 )
		return inheritanceResult;
#if WITH_ANGELSCRIPT_UNITTESTS
	if( const int injected = InjectAfterCommitStage(asBYTECODE_CODEGEN_TEST_COMMIT_AFTER_FUNCTIONS); injected < 0 )
		return injected;
#endif
	if( publishAggregateRuntimeBindings )
		module->AdoptCanonicalRuntimeTypeBindings(runtimeTypeBindings);
	module->SetLastBytecodePublisher(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN);
	for( asUINT i = 0; i < functions.GetLength(); ++i )
	{
		if( i < functionDeclKinds.GetLength()
			&& functionDeclKinds[i] == asBYTECODE_CODEGEN_GLOBAL_INITIALIZER
			&& functions[i] )
		{
			functions[i]->ReleaseInternal();
		}
	}
	functionReferencesAdded = false;
	functions.SetLength(0);
	functionDeclKinds.SetLength(0);
	constructorFactories.SetLength(0);
	globals.SetLength(0);
	imports.SetLength(0);
	funcdefs.SetLength(0);
	types.SetLength(0);
	return asAST_VERIFY_OK;
}

int asCBytecodeCodeGen::Generate(const asCASTContext& context, asCModule* module)
{
	return GenerateInternal(context, module, false, true, 0);
}

int asCBytecodeCodeGen::GenerateFunction(
	const asCASTContext& context,
	asCModule* module,
	bool addToModule,
	asCScriptFunction** outFunction)
{
	if( outFunction == 0 )
	{
		return asINVALID_ARG;
	}
	*outFunction = 0;
	return GenerateInternal(
		context, module, true, addToModule, outFunction);
}

int asCBytecodeCodeGen::GeneratePreparedModule(
	const asCASTContext& context,
	asCBuilder& preparedBuilder)
{
	failed = false;
	error = asAST_VERIFY_OK;
	lastFailLine = 0;
	failureDetail = "";
	failureSection = "";
	failureRow = 0;
	failureColumn = 0;
#if WITH_ANGELSCRIPT_UNITTESTS
	testTypeRelocationFailureInjected = false;
#endif

	asSAstVerifyResult publication;
	const int publicationResult = asCASTVerifyPublication(context, publication);
	if( publicationResult != asAST_VERIFY_OK )
	{
		failed = true;
		error = publicationResult;
		asCASTFormatVerifyResult(publication, failureDetail);
		return error;
	}

	asCModule* const module = preparedBuilder.module;
	if( module == 0 || module->engine == 0
		|| preparedBuilder.engine != module->engine )
	{
		failed = true;
		error = asNO_MODULE;
		failureDetail = "prepared CodeGen requires the live Builder module and Engine";
		return error;
	}
	if( module->GetLastLegacyCompilerInvocationCount() != 0 )
	{
		failed = true;
		error = asINVALID_CONFIGURATION;
		failureDetail = "legacy asCCompiler already ran before prepared Canonical CodeGen";
		return error;
	}
	asCScriptEngine* const engine = module->engine;
	if( preparedBuilder.numWarnings > 0
		&& engine->ep.compilerWarnings == 2 )
	{
		failed = true;
		error = asERROR;
		failureDetail = "prepared Canonical build rejects warnings treated as errors before body publication";
		return error;
	}

	asCRuntimeTypeBridge bridge(engine);
	asCCanonicalStringConstantLease stringConstants(engine);
	asCArray<asSCodeGenFuncBind> binds;
	asCArray<asSCodeGenGlobal> globals;
	asCArray<asASTDeclId> captureDecls;
	asCArray<const asCDecl*> emitDecls;
	asCArray<asCScriptFunction*> emitFunctions;
	asCArray<asSCanonicalConstructorFactoryPair> preparedFactories;
	asCArray<const asCDecl*> preparedFactoryConstructorDecls;
	asCArray<const asCDecl*> pendingGeneratedObjectFunctionDecls;
	asCArray<const asCDecl*> pendingLambdaDecls;
	asCArray<asCScriptFunction*> preparedImportFunctions;

	// Manual explicit imports are declaration shells owned by Stage 2, just like
	// the authored function shells below. Automatic imports intentionally omit
	// that mutable slot and bind the same sealed ImportDecl directly to the exact
	// same-batch provider shell. Neither route allocates or mutates here; failure
	// leaves the complete Stage2 graph available to normal compile rollback.
	for( asUINT declarationIndex = 1;
		declarationIndex <= context.GetDeclCount(); ++declarationIndex )
	{
		const asCDecl* const declaration = context.GetDecl(
			asASTDeclId(declarationIndex));
		if( declaration == 0 || declaration->kind != asAST_DECL_IMPORT )
		{
			continue;
		}

		asUINT matches = 0;
		asCString importDiagnostic;
		asCScriptFunction* const function = engine->ep.automaticImports
			? FindPreparedAutomaticImportedFunction(
				engine, module, bridge, context, declaration, matches,
				importDiagnostic)
			: FindPreparedImportedFunction(
				engine, module, bridge, context, declaration, matches,
				importDiagnostic);
		if( function == 0
			|| preparedImportFunctions.IndexOf(function) >= 0 )
		{
			failed = true;
			error = asINVALID_DECLARATION;
			failureDetail.Format(
				"prepared import must bind exactly one %s shell "
				"(decl=%s key=%s from=%s matches=%u; %s)",
				engine->ep.automaticImports
					? "automatic provider" : "Stage2",
				declaration->name.AddressOf()
					? declaration->name.AddressOf() : "<missing>",
				declaration->stableKey.AddressOf()
					? declaration->stableKey.AddressOf() : "<missing>",
				declaration->origin.AddressOf()
					? declaration->origin.AddressOf() : "<missing>",
				matches,
				importDiagnostic.AddressOf()
					? importDiagnostic.AddressOf() : "runtime-shells=unavailable");
			return error;
		}

		asSCodeGenFuncBind bind;
		bind.decl = declaration->id;
		bind.func = function;
		bind.captureBegin = 0;
		bind.captureCount = 0;
		binds.PushLast(bind);
		preparedImportFunctions.PushLast(function);
	}
	if( !engine->ep.automaticImports
		&& preparedImportFunctions.GetLength()
		!= module->bindInformations.GetLength() )
	{
		failed = true;
		error = asINVALID_DECLARATION;
		failureDetail.Format(
			"prepared import shell inventory differs from the sealed graph "
			"(sealed=%u runtime=%u)",
			preparedImportFunctions.GetLength(),
			module->bindInformations.GetLength());
		return error;
	}

	// Bind authored Stage 2 shells by the producer-carried stable declaration
	// key. Signature/name searches are intentionally insufficient here: an
	// overload or Hot Reload generation must never receive another body's code.
	for( asUINT i = 0; i < preparedBuilder.functions.GetLength(); ++i )
	{
		sFunctionDescription* const description = preparedBuilder.functions[i];
		if( description == 0 || description->isExistingShared )
			continue;
		asCScriptFunction* const function =
			description->funcId >= 0
				&& asUINT(description->funcId) < engine->scriptFunctions.GetLength()
			? engine->scriptFunctions[description->funcId] : 0;
		if( function == 0 || function->module != module
			|| function->scriptData == 0 )
		{
			failed = true;
			error = asNO_FUNCTION;
			failureDetail.Format(
				"prepared Builder function shell is missing (ordinal=%u id=%d)",
				i, description->funcId);
			return error;
		}

		asASTDeclId declarationId;
		asCString bindDetail;
		const int bindResult = bridge.BindFunctionDeclaration(
			context, *function, declarationId, &bindDetail);
		const asCDecl* const declaration = context.GetDecl(declarationId);
		if( bindResult != asRUNTIME_DECL_BIND_OK || declaration == 0 )
		{
			failed = true;
			error = asINVALID_DECLARATION;
			failureDetail.Format(
				"prepared function '%s' cannot bind its sealed declaration: result=%d detail=%s",
				function->GetDeclaration(true, true, false),
				bindResult,
				bindDetail.AddressOf() ? bindDetail.AddressOf() : "");
			return error;
		}
		if( !FillFunctionSourceMetadata(
			context, engine, function, declaration, error) )
		{
			failed = true;
			failureDetail.Format(
				"prepared function '%s' has no exact sealed source metadata: "
				"key=%s error=%d",
				function->GetDeclaration(true, true, false),
				declaration->stableKey.AddressOf()
					? declaration->stableKey.AddressOf() : "<missing>",
				error);
			return error;
		}

		asSCodeGenFuncBind bind;
		bind.decl = declarationId;
		bind.func = function;
		bind.captureBegin = 0;
		bind.captureCount = 0;
		binds.PushLast(bind);
		if( declaration->body.IsValid()
			|| declaration->kind == asAST_DECL_CONSTRUCTOR
			|| declaration->kind == asAST_DECL_DESTRUCTOR )
		{
			emitDecls.PushLast(declaration);
			emitFunctions.PushLast(function);
		}
	}

	// Stage 2 has already registered constructor factories in the Engine and in
	// the owning object's behaviour table. Derive each synthetic factory from
	// exactly one sealed constructor by owner plus complete callable ABI. The
	// factory is not a second source declaration: its stable identity is derived
	// from that constructor, matching the detached module generator.
	for( asUINT i = 0; i < preparedBuilder.factories.GetLength(); ++i )
	{
		const sFactoryDescription& description = preparedBuilder.factories[i];
		asCScriptFunction* const factory = description.funcId >= 0
			&& asUINT(description.funcId) < engine->scriptFunctions.GetLength()
			? engine->scriptFunctions[description.funcId] : 0;
		if( factory == 0 || factory->module != module || factory->scriptData == 0
			|| factory->scriptData->byteCode.GetLength() != 0
			|| description.objType == 0
			|| (description.objType->flags & asOBJ_VALUE) != 0 )
		{
			failed = true;
			error = asINVALID_CONFIGURATION;
			failureDetail.Format(
				"prepared constructor factory shell is incomplete (ordinal=%u id=%d)",
				i, description.funcId);
			return error;
		}

		bool behaviourOwnsFactory =
			description.objType->beh.factory == description.funcId;
		for( asUINT behaviourIndex = 0;
			!behaviourOwnsFactory
				&& behaviourIndex < description.objType->beh.factories.GetLength();
			++behaviourIndex )
		{
			behaviourOwnsFactory =
				description.objType->beh.factories[behaviourIndex]
					== description.funcId;
		}
		if( !behaviourOwnsFactory )
		{
			failed = true;
			error = asINVALID_CONFIGURATION;
			failureDetail.Format(
				"prepared constructor factory is absent from its owner behaviour table "
				"(ordinal=%u id=%d owner=%s)",
				i,
				description.funcId,
				description.objType->name.AddressOf()
					? description.objType->name.AddressOf() : "<missing>");
			return error;
		}

		asCScriptFunction* constructor = 0;
		const asCDecl* constructorDecl = 0;
		asUINT matches = 0;
		for( asUINT bindIndex = 0; bindIndex < binds.GetLength(); ++bindIndex )
		{
			asCScriptFunction* const candidate = binds[bindIndex].func;
			const asCDecl* const candidateDecl =
				context.GetDecl(binds[bindIndex].decl);
			if( candidate == 0 || candidateDecl == 0
				|| candidateDecl->kind != asAST_DECL_CONSTRUCTOR
				|| candidate->objectType != description.objType
				|| candidate->parameterTypes != factory->parameterTypes
				|| candidate->inOutFlags != factory->inOutFlags )
			{
				continue;
			}
			constructor = candidate;
			constructorDecl = candidateDecl;
			++matches;
		}
		if( matches != 1 || constructor == 0 || constructorDecl == 0 )
		{
			failed = true;
			error = asINVALID_DECLARATION;
			failureDetail.Format(
				"prepared constructor factory must derive from exactly one sealed constructor "
				"(ordinal=%u id=%d owner=%s matches=%u)",
				i,
				description.funcId,
				description.objType->name.AddressOf()
					? description.objType->name.AddressOf() : "<missing>",
				matches);
			return error;
		}
		if( factory->canonicalASTStableDeclKey.GetLength() == 0
			|| factory->canonicalASTStableDeclKey != constructorDecl->stableKey )
		{
			failed = true;
			error = asINVALID_DECLARATION;
			failureDetail.Format(
				"prepared constructor factory lacks its producer-carried constructor identity "
				"(ordinal=%u id=%d key=%s constructorKey=%s)",
				i,
				description.funcId,
				factory->canonicalASTStableDeclKey.AddressOf()
					? factory->canonicalASTStableDeclKey.AddressOf() : "<missing>",
				constructorDecl->stableKey.AddressOf()
					? constructorDecl->stableKey.AddressOf() : "<missing>");
			return error;
		}

		asSCanonicalConstructorFactoryPair pair;
		pair.constructor = constructor;
		pair.factory = factory;
		pair.objectType = description.objType;
		preparedFactories.PushLast(pair);
		preparedFactoryConstructorDecls.PushLast(constructorDecl);
	}

	// Resolve declaration-only host functions exactly as the detached module
	// generator does. They participate in call lowering but are never owned or
	// modified by this transaction.
	for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
	{
		const asCDecl* const declaration = context.GetDecl(asASTDeclId(i));
		if( declaration == 0 || !CanonicalDeclIsFunctionLike(declaration->kind) )
			continue;
		bool alreadyBound = false;
		for( asUINT b = 0; b < binds.GetLength(); ++b )
		{
			if( binds[b].decl == declaration->id )
			{
				alreadyBound = true;
				break;
			}
		}
		if( alreadyBound )
			continue;

		asCScriptFunction* external =
			FindCurrentModuleScriptGlobalFunction(
				module, bridge, context, declaration);
		if( external == 0 )
			external = FindAutomaticImportedScriptGlobalFunction(
				engine, module, bridge, context, declaration);
		if( external == 0 )
			external = FindRegisteredGlobalFunction(
				engine, module, bridge, context, declaration);
		if( external == 0 )
			external = FindExactRegisteredMethod(
				engine, bridge, context, declaration);
		if( external == 0 )
			external = FindExactRegisteredListFactory(
				engine, bridge, context, declaration);
		if( external == 0 )
			external = FindExactRegisteredConstructor(
				engine, bridge, context, declaration);
		if( external )
		{
			asSCodeGenFuncBind bind;
			bind.decl = declaration->id;
			bind.func = external;
			bind.captureBegin = 0;
			bind.captureCount = 0;
			binds.PushLast(bind);
			continue;
		}
		if( (declaration->traits & asAST_TRAIT_LAMBDA) != 0 )
		{
			// Lambda declarations are created by Parser/Sema while compiling their
			// enclosing function and therefore have no independent Stage 2 Builder
			// description. Canonical CodeGen owns their exact Runtime shell in the
			// same unpublished artifact as generated accessors and global initializers.
			pendingLambdaDecls.PushLast(declaration);
			continue;
		}
		const bool isGeneratedAccessor = declaration->kind == asAST_DECL_METHOD
			&& (declaration->traits & asAST_TRAIT_GENERATED) != 0
			&& declaration->accessorKind != asAST_ACCESSOR_NONE;
		const bool isGeneratedDestructor =
			declaration->kind == asAST_DECL_DESTRUCTOR
			&& (declaration->traits & asAST_TRAIT_GENERATED) != 0;
		if( isGeneratedAccessor || isGeneratedDestructor )
		{
			// Canonical Sema owns implicit field accessors and generated destructors.
			// The staged Builder has no source function description from which it
			// could prepare either shell, so keep the declaration pending until global
			// initializers have reserved their candidate ids. The same detached
			// artifact then publishes the complete generated callable/lifetime closure
			// atomically after all bodies emit.
			pendingGeneratedObjectFunctionDecls.PushLast(declaration);
			continue;
		}
		if( declaration->body.IsValid() )
		{
			failed = true;
			error = asNOT_SUPPORTED;
			failureDetail.Format(
				"sealed function has no exact prepared Runtime shell: key=%s kind=%s",
				declaration->stableKey.AddressOf()
					? declaration->stableKey.AddressOf() : "<missing>",
				CanonicalDeclKindName(declaration->kind));
			return error;
		}
	}

	struct asSPreparedGlobalState
	{
		sGlobalVariableDescription* description;
		const asCDecl* declaration;
		asCGlobalProperty* property;
		asCScriptFunction* initializer;
		asQWORD constantValue;
		int byteWidth;
		bool publishPureConstant;
		bool publishDefaultInit;
	};
	asCArray<asSPreparedGlobalState> globalStates;
	asSBytecodeCodeGenArtifact globalArtifact;

	auto RollbackPreparedGlobals = [&]()
	{
		globalArtifact.Abandon(engine);
		globalStates.SetLength(0);
	};

	// Stage 2 already registered and allocated every authoritative global
	// property. Bind each one by the stable declaration key captured while the
	// Parser/Sema node map was alive; name-only matching is unsafe across
	// namespaces and Hot Reload generations.
	for( asUINT globalIndex = 0;
		globalIndex < preparedBuilder.globVariableList.GetLength();
		++globalIndex )
	{
		sGlobalVariableDescription* const description =
			preparedBuilder.globVariableList[globalIndex];
		if( description == 0
			|| description->canonicalASTStableDeclKey.GetLength() == 0 )
		{
			failed = true;
			error = asINVALID_CONFIGURATION;
			failureDetail.Format(
				"prepared global shell is incomplete or already compiled (ordinal=%u name=%s)",
				globalIndex,
				description && description->name.AddressOf()
					? description->name.AddressOf() : "<missing>");
			RollbackPreparedGlobals();
			return error;
		}

		const asCDecl* declaration = 0;
		asUINT declarationMatches = 0;
		for( asUINT declarationIndex = 1;
			declarationIndex <= context.GetDeclCount(); ++declarationIndex )
		{
			const asCDecl* const candidate =
				context.GetDecl(asASTDeclId(declarationIndex));
			if( candidate && candidate->kind == asAST_DECL_VAR
				&& candidate->stableKey.Equals(
					description->canonicalASTStableDeclKey) )
			{
				declaration = candidate;
				++declarationMatches;
			}
		}

		if( description->isEnumValue )
		{
			const asCDecl* const owner = declaration
				? context.GetDecl(declaration->parent) : 0;
			const asCDataType dataType = declaration
				? bridge.Resolve(context, declaration->type)
				: asCDataType();
			const asQWORD expectedConstant = declaration
				? declaration->constantValue : 0;
			asCEnumType* const enumType =
				CastToEnumType(description->datatype.GetTypeInfo());
			asUINT runtimeMatches = 0;
			for( asUINT valueIndex = 0;
				enumType && valueIndex < enumType->enumValues.GetLength();
				++valueIndex )
			{
				const asSEnumValue* const value =
					enumType->enumValues[valueIndex];
				if( value && value->name == description->name
					&& value->value
						== static_cast<int>(expectedConstant) )
				{
					++runtimeMatches;
				}
			}

			// Stage 2 materializes enum values while registering their owning
			// Runtime type. In a Canonical prepared build this is not a second
			// semantic compilation: RegisterGlobalVariables copied the exact
			// Sema constant into the shell. Validate that bridge here and exclude
			// the dummy enum property from the ordinary global-initializer
			// transaction; CodeGen neither recompiles nor recreates the value.
			if( declarationMatches != 1 || declaration == 0
				|| declaration->kind != asAST_DECL_VAR
				|| owner == 0 || owner->kind != asAST_DECL_ENUM
				|| !declaration->hasConstantValue
				|| declaration->name != description->name
				|| !description->isCompiled
				|| description->property == 0
				|| !description->property->isPureConstant
				|| description->constantValue != declaration->constantValue
				|| description->property->storage
					!= declaration->constantValue
				|| !dataType.IsValid()
				|| !dataType.IsEqualExceptConst(description->datatype)
				|| enumType == 0 || enumType->module != module
				|| runtimeMatches != 1 )
			{
				failed = true;
				error = asINVALID_DECLARATION;
				failureDetail.Format(
					"prepared enum value differs from its sealed constant "
					"(ordinal=%u name=%s matches=%u runtimeMatches=%u "
					"declVar=%u ownerEnum=%u hasConstant=%u nameMatch=%u "
					"compiled=%u property=%u pure=%u descriptionBits=%llu "
					"propertyBits=%llu canonicalBits=%llu typeValid=%u "
					"typeMatch=%u runtimeEnum=%u moduleMatch=%u)",
					globalIndex,
					description->name.AddressOf()
						? description->name.AddressOf() : "<missing>",
					declarationMatches,
					runtimeMatches,
					declaration && declaration->kind == asAST_DECL_VAR,
					owner && owner->kind == asAST_DECL_ENUM,
					declaration && declaration->hasConstantValue,
					declaration && declaration->name == description->name,
					description->isCompiled,
					description->property != 0,
					description->property
						&& description->property->isPureConstant,
					static_cast<unsigned long long>(
						description->constantValue),
					static_cast<unsigned long long>(
						description->property
							? description->property->storage : 0),
					static_cast<unsigned long long>(expectedConstant),
					dataType.IsValid(),
					dataType.IsValid()
						&& dataType.IsEqualExceptConst(
							description->datatype),
					enumType != 0,
					enumType && enumType->module == module);
				RollbackPreparedGlobals();
				return error;
			}
			continue;
		}

		if( description->property == 0
			|| description->property->module != module
			|| description->property->GetAddressOfValue() == 0
			|| description->isCompiled
			|| description->property->GetInitFunc() != 0 )
		{
			failed = true;
			error = asINVALID_CONFIGURATION;
			failureDetail.Format(
				"prepared global shell is incomplete or already compiled (ordinal=%u name=%s)",
				globalIndex,
				description->name.AddressOf()
					? description->name.AddressOf() : "<missing>");
			RollbackPreparedGlobals();
			return error;
		}

		if( declarationMatches != 1 || declaration == 0
			|| !IsCanonicalNamespaceOrTranslationUnitMember(context, declaration)
			|| IsCanonicalAutomaticImportScriptGlobal(declaration)
			|| IsCanonicalCurrentModuleScriptGlobal(declaration)
			|| declaration->name != description->name )
		{
			failed = true;
			error = asINVALID_DECLARATION;
			failureDetail.Format(
				"prepared global stable key must resolve exactly once (ordinal=%u matches=%u key=%s)",
				globalIndex,
				declarationMatches,
				description->canonicalASTStableDeclKey.AddressOf());
			RollbackPreparedGlobals();
			return error;
		}

		const asCDataType dataType = bridge.Resolve(context, declaration->type);
		asSNameSpace* const nameSpace = ResolveCanonicalDeclNamespace(
			context, engine, module, declaration);
		if( !dataType.IsValid() || !(dataType == description->datatype)
			|| !(dataType == description->property->type)
			|| nameSpace == 0 || nameSpace != description->nameSpace )
		{
			failed = true;
			error = asINVALID_TYPE;
			failureDetail.Format(
				"prepared global Runtime type/namespace differs from its sealed declaration (name=%s)",
				description->name.AddressOf());
			RollbackPreparedGlobals();
			return error;
		}

		asSCodeGenGlobal bind;
		bind.decl = declaration->id;
		bind.prop = description->property;
		globals.PushLast(bind);

		asSPreparedGlobalState state;
		state.description = description;
		state.declaration = declaration;
		state.property = description->property;
		state.initializer = 0;
		state.constantValue = declaration->constantValue;
		state.byteWidth = dataType.GetSizeInMemoryBytes();
		state.publishPureConstant = false;
		state.publishDefaultInit = false;

		if( declaration->defaultArg.GetLength() != 0 )
		{
			if( (declaration->traits & asAST_TRAIT_DEFAULT_INITIALIZED) != 0 )
			{
				failed = true;
				error = asINVALID_DECLARATION;
				failureDetail.Format(
					"prepared global cannot own both an initializer and the sealed default-initialized lifecycle (name=%s)",
					description->name.AddressOf());
				RollbackPreparedGlobals();
				return error;
			}
			if( !declaration->hasConstantValue
				|| (!dataType.IsIntegerType() && !dataType.IsUnsignedType()
					&& !dataType.IsFloat32Type() && !dataType.IsFloat64Type())
				|| (state.byteWidth != 1 && state.byteWidth != 2
					&& state.byteWidth != 4 && state.byteWidth != 8) )
			{
				failed = true;
				error = asNOT_SUPPORTED;
				failureDetail.Format(
					"prepared global initializer is not a Sema-proven scalar constant (name=%s)",
					description->name.AddressOf());
				RollbackPreparedGlobals();
				return error;
			}

			if( (declaration->type.quals & asAST_QUAL_CONST) != 0 )
			{
				state.publishPureConstant = true;
				// Function bodies emit before CommitPreparedGlobals. Artifact
				// capture uses property->isPureConstant at MarkDependency time,
				// so the Sema-proven const must be visible during that emit.
				description->property->isPureConstant = true;
				description->isPureConstant = true;
			}
			else
			{
				const asUINT functionCountBefore =
					globalArtifact.functions.GetLength();
				if( !EmitCanonicalScalarGlobalInitializer(
					context,
					preparedBuilder,
					engine,
					module,
					globalArtifact,
					description->property,
					declaration,
					dataType,
					declaration->constantValue,
					false)
					|| globalArtifact.functions.GetLength()
						!= functionCountBefore + 1 )
				{
					failed = true;
					error = asNOT_SUPPORTED;
					failureDetail.Format(
						"prepared scalar global initializer emission failed (name=%s)",
						description->name.AddressOf());
					RollbackPreparedGlobals();
					return error;
				}
				state.initializer = globalArtifact.functions[
					globalArtifact.functions.GetLength() - 1];
			}
		}
		else if( (declaration->traits
			& asAST_TRAIT_DEFAULT_INITIALIZED) != 0 )
		{
			// Sema has already selected the no-initializer value-object
			// lifecycle. The Runtime data type is checked only as a stale-host
			// consistency guard; it does not choose the policy here.
			if( !dataType.IsNonPrimitiveValueType() )
			{
				failed = true;
				error = asINVALID_TYPE;
				failureDetail.Format(
					"sealed default-initialized global does not resolve to a Runtime value object (name=%s)",
					description->name.AddressOf());
				RollbackPreparedGlobals();
				return error;
			}
			state.publishDefaultInit = true;
		}
		else if( dataType.IsNonPrimitiveValueType() )
		{
			failed = true;
			error = asINVALID_DECLARATION;
			failureDetail.Format(
				"prepared value-object global is missing its sealed initialization lifecycle (name=%s)",
				description->name.AddressOf());
			RollbackPreparedGlobals();
			return error;
		}
		globalStates.PushLast(state);
	}

	// Host globals interned by Sema (for example `__StaticType_<Name>`) are not
	// Stage 2 module properties. Bind each unique registered shell so VALUE
	// lvalue addresses can load the existing engine storage.
	for( asUINT declarationIndex = 1;
		declarationIndex <= context.GetDeclCount(); ++declarationIndex )
	{
		const asCDecl* const declaration = context.GetDecl(
			asASTDeclId(declarationIndex));
		if( !IsCanonicalNativeGlobalProperty(declaration) )
		{
			continue;
		}
		bool alreadyBound = false;
		for( asUINT boundIndex = 0; boundIndex < globals.GetLength(); ++boundIndex )
		{
			if( globals[boundIndex].decl == declaration->id )
			{
				alreadyBound = true;
				break;
			}
		}
		if( alreadyBound )
		{
			continue;
		}
		asCGlobalProperty* const property = FindRegisteredNativeGlobalProperty(
			engine, bridge, context, declaration);
		if( property == 0 )
		{
			failed = true;
			error = asNO_GLOBAL_VAR;
			failureDetail.Format(
				"prepared native global property has no unique registered shell (name=%s)",
				declaration->name.AddressOf()
					? declaration->name.AddressOf() : "<missing>");
			RollbackPreparedGlobals();
			return error;
		}
		asSCodeGenGlobal bind;
		bind.decl = declaration->id;
		bind.prop = property;
		globals.PushLast(bind);
	}

	// Lambdas are source declarations, but they are nested inside an authored
	// function rather than represented by preparedBuilder.functions. Build each
	// shell from the sealed declaration, then append the exact Sema capture list
	// as hidden Runtime formals. Keeping these candidates inside globalArtifact
	// makes allocation, body emission and publication atomic with the rest of the
	// prepared module.
	for( asUINT i = 0; i < pendingLambdaDecls.GetLength(); ++i )
	{
		const asCDecl* const declaration = pendingLambdaDecls[i];
		asCScriptFunction* const function =
			asNEW(asCScriptFunction)(engine, module, asFUNC_SCRIPT);
		if( function == 0 )
		{
			failed = true;
			error = asOUT_OF_MEMORY;
			failureDetail = "failed to allocate prepared lambda shell";
			RollbackPreparedGlobals();
			return error;
		}
		function->id = globalArtifact.GetNextPendingScriptFunctionId(engine);
		if( function->id < 0 )
		{
			failed = true;
			error = asOUT_OF_MEMORY;
			failureDetail = "failed to reserve prepared lambda FunctionId";
			function->DestroyHalfCreated();
			RollbackPreparedGlobals();
			return error;
		}
		function->nameSpace = ResolveCanonicalDeclNamespace(
			context, engine, module, declaration);
		if( function->nameSpace == 0
			|| !FillFunctionSignature(
				context,
				engine,
				module,
				bridge,
				globalArtifact,
				function,
				declaration,
				error) )
		{
			failed = true;
			if( error == asAST_VERIFY_OK )
				error = asOUT_OF_MEMORY;
			failureDetail.Format(
				"failed to prepare lambda signature: key=%s error=%d",
				declaration && declaration->stableKey.AddressOf()
					? declaration->stableKey.AddressOf() : "<missing>",
				error);
			function->DestroyHalfCreated();
			RollbackPreparedGlobals();
			return error;
		}

		asSCodeGenFuncBind bind;
		bind.decl = declaration->id;
		bind.func = function;
		bind.captureBegin = captureDecls.GetLength();
		CollectLambdaCaptures(context, declaration, captureDecls);
		bind.captureCount = captureDecls.GetLength() - bind.captureBegin;
		for( asUINT captureIndex = 0;
			captureIndex < bind.captureCount; ++captureIndex )
		{
			const asCDecl* const captured = context.GetDecl(
				captureDecls[bind.captureBegin + captureIndex]);
			const asCDataType captureType = captured
				? bridge.Resolve(context, captured->type) : asCDataType();
			if( captured == 0 || !captureType.IsValid()
				|| captureType.GetTokenType() == ttVoid )
			{
				failed = true;
				error = captured ? asINVALID_TYPE : asINVALID_DECLARATION;
				failureDetail.Format(
					"prepared lambda capture is invalid: key=%s capture=%u",
					declaration->stableKey.AddressOf()
						? declaration->stableKey.AddressOf() : "<missing>",
					captureIndex);
				function->DestroyHalfCreated();
				RollbackPreparedGlobals();
				return error;
			}
			function->parameterTypes.PushLast(captureType);
			function->parameterNames.PushLast(captured->name);
			function->inOutFlags.PushLast(asTM_NONE);
			function->defaultArgs.PushLast(0);
		}
		if( bind.captureCount != 0 )
		{
			function->CalculateParameterOffsets();
		}

		globalArtifact.functions.PushLast(function);
		globalArtifact.functionDeclKinds.PushLast(
			static_cast<int>(declaration->kind));
		binds.PushLast(bind);
		emitDecls.PushLast(declaration);
		emitFunctions.PushLast(function);
	}

	// Generated accessors and destructors are semantic declarations rather than
	// Parser-authored functions, so Stage 2 cannot place them in
	// preparedBuilder.functions. Build their Runtime signatures only from the
	// sealed declaration and keep them in the same candidate artifact as prepared
	// global initializers. This preserves deterministic function-id allocation and
	// lets every later failure abandon the generated functions without leaking
	// Engine/module/behaviour-table entries.
	for( asUINT i = 0; i < pendingGeneratedObjectFunctionDecls.GetLength(); ++i )
	{
		const asCDecl* const declaration =
			pendingGeneratedObjectFunctionDecls[i];
		asCScriptFunction* const function =
			asNEW(asCScriptFunction)(engine, module, asFUNC_SCRIPT);
		if( function == 0 )
		{
			failed = true;
			error = asOUT_OF_MEMORY;
			failureDetail = "failed to allocate prepared generated-function shell";
			RollbackPreparedGlobals();
			return error;
		}
		function->id = globalArtifact.GetNextPendingScriptFunctionId(engine);
		if( function->id < 0 )
		{
			failed = true;
			error = asOUT_OF_MEMORY;
			failureDetail = "failed to reserve prepared generated-function FunctionId";
			function->DestroyHalfCreated();
			RollbackPreparedGlobals();
			return error;
		}
		function->nameSpace = ResolveCanonicalDeclNamespace(
			context, engine, module, declaration);
		if( function->nameSpace == 0
			|| !FillFunctionSignature(
				context,
				engine,
				module,
				bridge,
				globalArtifact,
				function,
				declaration,
				error) )
		{
			failed = true;
			if( error == asAST_VERIFY_OK )
				error = asOUT_OF_MEMORY;
			failureDetail.Format(
				"failed to prepare generated-function signature: key=%s error=%d",
				declaration && declaration->stableKey.AddressOf()
					? declaration->stableKey.AddressOf() : "<missing>",
				error);
			function->DestroyHalfCreated();
			RollbackPreparedGlobals();
			return error;
		}

		globalArtifact.functions.PushLast(function);
		globalArtifact.functionDeclKinds.PushLast(
			static_cast<int>(declaration->kind));

		asSCodeGenFuncBind bind;
		bind.decl = declaration->id;
		bind.func = function;
		bind.captureBegin = 0;
		bind.captureCount = 0;
		binds.PushLast(bind);
		emitDecls.PushLast(declaration);
		emitFunctions.PushLast(function);
	}

	auto CommitPreparedGlobals = [&]()
	{
		for( asUINT i = 0; i < globalArtifact.functions.GetLength(); ++i )
		{
			asCScriptFunction* const function = globalArtifact.functions[i];
			engine->AddScriptFunction(function);
			const int declarationKind =
				i < globalArtifact.functionDeclKinds.GetLength()
					? globalArtifact.functionDeclKinds[i]
					: asBYTECODE_CODEGEN_GLOBAL_INITIALIZER;
			if( declarationKind == asBYTECODE_CODEGEN_GLOBAL_INITIALIZER )
			{
				function->AddReferences();
				continue;
			}
			module->scriptFunctions.PushLast(function);
			AttachCanonicalObjectFunction(function, declarationKind);
		}
		for( asUINT i = 0; i < globalStates.GetLength(); ++i )
		{
			asSPreparedGlobalState& state = globalStates[i];
			if( state.publishPureConstant )
			{
				void* const memory = state.property->GetAddressOfValue();
				if( state.byteWidth == 1 )
					*static_cast<asBYTE*>(memory) = (asBYTE)state.constantValue;
				else if( state.byteWidth == 2 )
					*static_cast<asWORD*>(memory) = (asWORD)state.constantValue;
				else if( state.byteWidth == 4 )
					*static_cast<asDWORD*>(memory) = (asDWORD)state.constantValue;
				else
					*static_cast<asQWORD*>(memory) = state.constantValue;
				state.property->isPureConstant = true;
			}
			else if( state.publishDefaultInit )
			{
				state.property->isDefaultInit = true;
			}
			else if( state.initializer )
			{
				state.property->SetInitFunc(state.initializer);
			}
			state.description->isCompiled = true;
		}
		for( asUINT i = 0; i < globalArtifact.functions.GetLength(); ++i )
		{
			if( i < globalArtifact.functionDeclKinds.GetLength()
				&& globalArtifact.functionDeclKinds[i]
					== asBYTECODE_CODEGEN_GLOBAL_INITIALIZER )
			{
				globalArtifact.functions[i]->ReleaseInternal();
			}
		}
		globalArtifact.functions.SetLength(0);
		globalArtifact.functionDeclKinds.SetLength(0);
		globalArtifact.constructorFactories.SetLength(0);
		globalStates.SetLength(0);
	};

	auto FinalizePreparedBuilder = [&]()
	{
		for( asUINT i = 0;
			i < preparedBuilder.globVariableList.GetLength(); ++i )
		{
			sGlobalVariableDescription* const description =
				preparedBuilder.globVariableList[i];
			if( description && description->isEnumValue
				&& description->property )
			{
				// Enum constants use a Builder-owned dummy property only while
				// Stage 2 resolves later declarations. Once the sealed value has
				// passed the CodeGen validation above, remove that non-runtime
				// shell just as the legacy global-compile finalizer does.
				engine->allScriptGlobalVariables.Remove(
					description->property);
				asDELETE(description->property, asCGlobalProperty);
				description->property = 0;
			}
		}
		for( asUINT i = 0; i < preparedBuilder.classDeclarations.GetLength(); ++i )
		{
			sClassDeclaration* const declaration =
				preparedBuilder.classDeclarations[i];
			if( declaration && declaration->typeInfo )
			{
				asCObjectType* const objectType =
					CastToObjectType(declaration->typeInfo);
				if( objectType )
					objectType->compilingDeclaration = 0;
			}
		}
		for( asUINT i = 0; i < preparedBuilder.parsers.GetLength(); ++i )
		{
			asDELETE(preparedBuilder.parsers[i], asCParser);
		}
		preparedBuilder.parsers.SetLength(0);
	};

	if( emitDecls.GetLength() == 0 && preparedFactories.GetLength() == 0 )
	{
		CommitPreparedGlobals();
		FinalizePreparedBuilder();
		module->SetLastBytecodePublisher(
			asBYTECODE_PUBLISHER_CANONICAL_CODEGEN);
		module->SetLastCanonicalCodeGenAstDigest(
			ComputeCanonicalAstDigest(context));
		return asAST_VERIFY_OK;
	}

	struct asSPreparedBodyState
	{
		asCScriptFunction* function;
		asCScriptFunction::ScriptFunctionData* originalData;
		bool originalDontCleanUpOnException;
	};
	asCArray<asSPreparedBodyState> states;

	auto RollbackBodies = [&]()
	{
		for( asUINT i = 0; i < states.GetLength(); ++i )
		{
			asSPreparedBodyState& state = states[i];
			if( state.function == 0 )
				continue;
			state.function->DeallocateScriptFunctionData();
			state.function->scriptData = state.originalData;
			state.function->dontCleanUpOnException =
				state.originalDontCleanUpOnException;
		}
		states.SetLength(0);
	};

	// Factories participate in the same body transaction, after the authored
	// declaration/function pairs that keep emitDecls and emitFunctions aligned.
	for( asUINT i = 0; i < preparedFactories.GetLength(); ++i )
		emitFunctions.PushLast(preparedFactories[i].factory);

	// Give every target a fresh unpublished body state first. The previous body
	// remains detached and untouched until the entire module has emitted.
	for( asUINT i = 0; i < emitFunctions.GetLength(); ++i )
	{
		asCScriptFunction* const function = emitFunctions[i];
		if( function == 0 || function->scriptData == 0
			|| function->scriptData->byteCode.GetLength() != 0 )
		{
			failed = true;
			error = asINVALID_CONFIGURATION;
			failureDetail.Format(
				"prepared function body is not an empty Stage 2 shell (ordinal=%u)", i);
			RollbackBodies();
			RollbackPreparedGlobals();
			return error;
		}
		asSPreparedBodyState state;
		state.function = function;
		state.originalData = function->scriptData;
		state.originalDontCleanUpOnException =
			function->dontCleanUpOnException;
		function->scriptData = 0;
		function->AllocateScriptFunctionData();
		if( function->scriptData == 0 )
		{
			function->scriptData = state.originalData;
			failed = true;
			error = asOUT_OF_MEMORY;
			failureDetail = "failed to allocate detached prepared function body";
			RollbackBodies();
			RollbackPreparedGlobals();
			return error;
		}
		states.PushLast(state);
	}

	for( asUINT i = 0; i < emitDecls.GetLength(); ++i )
	{
		const asCDecl* const declaration = emitDecls[i];
		asCScriptFunction* const function = emitFunctions[i];
		// The body transaction above deliberately replaced the Stage 2
		// ScriptFunctionData with a detached, default-constructed instance. Source
		// coordinates previously validated on the shell therefore do not survive
		// by pointer swap. Publish them again from the sealed AST onto the actual
		// generated body before line tables and function facts are committed.
		if( !FillFunctionSourceMetadata(
			context, engine, function, declaration, error) )
		{
			failed = true;
			lastFailLine = __LINE__;
			failureDetail.Format(
				"detached prepared body has no exact sealed source metadata "
				"(ordinal=%u key=%s error=%d)",
				i,
				declaration && declaration->stableKey.AddressOf()
					? declaration->stableKey.AddressOf() : "<missing>",
				error);
			RollbackBodies();
			RollbackPreparedGlobals();
			return error;
		}
		const asSBuildArtifactInvocation invocation =
			preparedBuilder.BeginCanonicalBuildArtifactCompile(
				context, declaration, function);
#if WITH_ANGELSCRIPT_UNITTESTS
		if( i == testFailureBeforeFunctionEmission )
		{
			failed = true;
			error = testFailureCode;
			lastFailLine = __LINE__;
			failureDetail.Format(
				"injected-prepared-function-emission ordinal=%u key=%s error=%d",
				i,
				declaration->stableKey.AddressOf()
					? declaration->stableKey.AddressOf() : "<unknown>",
				error);
			preparedBuilder.CompleteBuildArtifactCompile(
				invocation, function, error,
				asBUILD_ARTIFACT_RESTORE_MISS, true);
			RollbackBodies();
			RollbackPreparedGlobals();
			return error;
		}
#endif
		asCCanonicalFunctionEmitter emitter(
			context,
			&preparedBuilder,
			&bridge,
			globalArtifact,
			binds,
			globals,
			captureDecls,
			stringConstants,
			error,
			lastFailLine,
			failureDetail);
		const bool oldHardDependencyPolicy =
			preparedBuilder.bValueDependenciesAreHard;
		preparedBuilder.bValueDependenciesAreHard =
			declaration->kind == asAST_DECL_CONSTRUCTOR;
		const bool emitted = emitter.Emit(function, declaration);
		preparedBuilder.bValueDependenciesAreHard = oldHardDependencyPolicy;
		if( !emitted )
		{
			failed = true;
			if( error == asAST_VERIFY_OK )
				error = asNOT_SUPPORTED;
			if( lastFailLine == 0 )
				lastFailLine = __LINE__;
			if( failureDetail.GetLength() == 0 )
			{
				failureDetail.Format(
					"prepared emitter returned false without diagnostic ordinal=%u key=%s error=%d",
					i,
					declaration && declaration->stableKey.AddressOf()
						? declaration->stableKey.AddressOf() : "<unknown>",
					error);
			}
			if( declaration->range.begin.IsValid() )
			{
				asSSourceLineColumn location = {};
				if( context.GetSourceManager().GetLineColumn(
					declaration->range.begin, location) )
				{
					const char* const key =
						context.GetSourceManager().GetLogicalKey(
							declaration->range.begin.fileID);
					failureSection = key ? key : "";
					failureRow = location.row;
					failureColumn = location.column;
				}
			}
			preparedBuilder.CompleteBuildArtifactCompile(
				invocation, function, error,
				asBUILD_ARTIFACT_RESTORE_MISS, true);
			RollbackBodies();
			RollbackPreparedGlobals();
			return error;
		}
		preparedBuilder.CompleteBuildArtifactCompile(
			invocation, function, asSUCCESS,
			asBUILD_ARTIFACT_RESTORE_MISS, true);
	}

	for( asUINT i = 0; i < preparedFactories.GetLength(); ++i )
	{
		const asSCanonicalConstructorFactoryPair& pair = preparedFactories[i];
		const asCDecl* const constructorDecl =
			preparedFactoryConstructorDecls[i];
		// The body transaction replaces the Stage 2 factory shell's
		// ScriptFunctionData just as it does for authored/generated functions.
		// A factory has no second source declaration, so republish the exact
		// constructor coordinates onto the synthetic wrapper after that swap.
		// Otherwise the final function keeps its stable key and Bytecode but loses
		// the primary debug section required by Cache/StaticJIT capture.
		if( !FillFunctionSourceMetadata(
			context, engine, pair.factory, constructorDecl, error) )
		{
			failed = true;
			lastFailLine = __LINE__;
			failureDetail.Format(
				"detached prepared factory has no exact sealed source metadata "
				"(ordinal=%u key=%s error=%d)",
				i,
				constructorDecl && constructorDecl->stableKey.AddressOf()
					? constructorDecl->stableKey.AddressOf() : "<missing>",
				error);
			RollbackBodies();
			RollbackPreparedGlobals();
			return error;
		}
		const asSBuildArtifactInvocation invocation =
			preparedBuilder.BeginCanonicalBuildArtifactCompile(
				context, constructorDecl, pair.factory);
#if WITH_ANGELSCRIPT_UNITTESTS
		if( i == testFailureBeforePreparedFactoryEmission )
		{
			failed = true;
			error = testPreparedFactoryFailureCode;
			lastFailLine = __LINE__;
			failureDetail.Format(
				"injected-prepared-factory-emission ordinal=%u key=%s error=%d",
				i,
				constructorDecl && constructorDecl->stableKey.AddressOf()
					? constructorDecl->stableKey.AddressOf() : "<unknown>",
				error);
			preparedBuilder.CompleteBuildArtifactCompile(
				invocation, pair.factory, error,
				asBUILD_ARTIFACT_RESTORE_MISS, true);
			RollbackBodies();
			RollbackPreparedGlobals();
			return error;
		}
		if( emitDecls.GetLength() + i == testFailureBeforeFunctionEmission )
		{
			failed = true;
			error = testFailureCode;
			lastFailLine = __LINE__;
			failureDetail.Format(
				"injected-prepared-factory-emission ordinal=%u key=%s error=%d",
				i,
				constructorDecl && constructorDecl->stableKey.AddressOf()
					? constructorDecl->stableKey.AddressOf() : "<unknown>",
				error);
			preparedBuilder.CompleteBuildArtifactCompile(
				invocation, pair.factory, error,
				asBUILD_ARTIFACT_RESTORE_MISS, true);
			RollbackBodies();
			RollbackPreparedGlobals();
			return error;
		}
#endif
		if( !EmitCanonicalConstructorFactory(preparedBuilder, pair) )
		{
			failed = true;
			error = asNOT_SUPPORTED;
			lastFailLine = __LINE__;
			failureDetail.Format(
				"prepared constructor-factory emission failed key=%s",
				constructorDecl && constructorDecl->stableKey.AddressOf()
					? constructorDecl->stableKey.AddressOf() : "<unknown>");
			preparedBuilder.CompleteBuildArtifactCompile(
				invocation, pair.factory, error,
				asBUILD_ARTIFACT_RESTORE_MISS, true);
			RollbackBodies();
			RollbackPreparedGlobals();
			return error;
		}
		preparedBuilder.CompleteBuildArtifactCompile(
			invocation, pair.factory, asSUCCESS,
			asBUILD_ARTIFACT_RESTORE_MISS, true);
	}

	const asSRuntimeTypeBindingEnvironment bindingEnvironment =
		asMakeCurrentRuntimeTypeBindingEnvironment();
	const int relocationResult = globalArtifact.ResolveAndPatchTypeRelocations(
		engine, context, bindingEnvironment, &failureDetail
#if WITH_ANGELSCRIPT_UNITTESTS
		, testFailureAtTypeRelocationUse,
		testTypeRelocationFailureCode,
		&testTypeRelocationFailureInjected
#endif
		);
	if( relocationResult != asRUNTIME_TYPE_BIND_OK )
	{
		failed = true;
		error = relocationResult;
		lastFailLine = __LINE__;
		RollbackBodies();
		RollbackPreparedGlobals();
		return error;
	}

	// AddReferences() deliberately treats an LDG-family operand that is absent
	// from varAddressMap as a string constant. A prepared script global must
	// never enter that fallback: it means Stage 2 allocated a property shell but
	// did not publish the exact value address that Canonical CodeGen embedded.
	// Fail transactionally here, while every generated body and initializer is
	// still detached, instead of handing an arbitrary address to the host string
	// factory during commit.
	for( asUINT i = 0; i < globals.GetLength(); ++i )
	{
		asCGlobalProperty* const property = globals[i].prop;
		void* const address = property ? property->GetAddressOfValue() : 0;
		asCGlobalProperty** const mapped = address
			? engine->varAddressMap.Find(address) : 0;
		if( property == 0 || address == 0 || mapped == 0 || *mapped != property )
		{
			failed = true;
			error = asINVALID_CONFIGURATION;
			lastFailLine = __LINE__;
			failureDetail.Format(
				"prepared global address is not published before function reference commit "
				"(ordinal=%u name=%s property=%p address=%p mapped=%p)",
				i,
				property && property->name.AddressOf()
					? property->name.AddressOf() : "<missing>",
				property,
				address,
				mapped ? *mapped : 0);
			RollbackBodies();
			RollbackPreparedGlobals();
			return error;
		}
	}

	// No operation below can fail. Publish prepared global initializers and
	// install normal VM references first, then
	// destroy the displaced empty states. Function pointers, ids, signatures,
	// object method tables, and UE descriptors remain exactly where Stage 2 put
	// them.
	const int typeIdCommitResult =
		globalArtifact.CommitPendingTypeIds(engine);
	if( typeIdCommitResult < 0 )
	{
		failed = true;
		error = typeIdCommitResult;
		lastFailLine = __LINE__;
		failureDetail.Format(
			"prepared late TypeId projection commit failed: result=%d",
			typeIdCommitResult);
		RollbackBodies();
		RollbackPreparedGlobals();
		return error;
	}
	CommitPreparedGlobals();
	for( asUINT i = 0; i < states.GetLength(); ++i )
	{
		asSPreparedBodyState& state = states[i];
		state.function->AddReferences();
		asCScriptFunction::ScriptFunctionData* const generatedData =
			state.function->scriptData;
		state.function->scriptData = state.originalData;
		state.function->DeallocateScriptFunctionData();
		state.function->scriptData = generatedData;
	}
	states.SetLength(0);

	FinalizePreparedBuilder();
	module->SetLastBytecodePublisher(asBYTECODE_PUBLISHER_CANONICAL_CODEGEN);
	module->SetLastCanonicalCodeGenAstDigest(ComputeCanonicalAstDigest(context));
	return asAST_VERIFY_OK;
}

int asCBytecodeCodeGen::GenerateInternal(
	const asCASTContext& context,
	asCModule* module,
	bool singleFunction,
	bool addToModule,
	asCScriptFunction** outFunction)
{
	failed = false;
	error = asAST_VERIFY_OK;
	lastFailLine = 0;
	failureDetail = "";
	failureSection = "";
	failureRow = 0;
	failureColumn = 0;
#if WITH_ANGELSCRIPT_UNITTESTS
	testTypeRelocationFailureInjected = false;
#endif

	asSAstVerifyResult publication;
	const int publicationResult = asCASTVerifyPublication(context, publication);
	if( publicationResult != asAST_VERIFY_OK )
	{
		failed = true;
		error = publicationResult;
		asCASTFormatVerifyResult(publication, failureDetail);
		return error;
	}
	if( context.GetDeclCount() == 0 )
	{
		failed = true;
		error = asAST_VERIFY_DANGLING_ID;
		return error;
	}
	const asCDecl* singleFunctionDecl = 0;
	if( singleFunction )
	{
		singleFunctionDecl = FindSingleCompileFunctionDecl(context);
		if( singleFunctionDecl == 0 )
		{
			failed = true;
			error = asINVALID_DECLARATION;
			failureDetail = "CompileFunction requires exactly one direct source function declaration";
			return error;
		}
	}

	bool hasFunctionLike = false;
	for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
	{
		const asCDecl* decl = context.GetDecl(asASTDeclId(i));
		if( decl == 0 || !CanonicalDeclIsFunctionLike(decl->kind) )
		{
			continue;
		}
		if( decl->body.IsValid()
			|| decl->kind == asAST_DECL_FUNCTION
			|| decl->kind == asAST_DECL_CONSTRUCTOR
			|| (decl->kind == asAST_DECL_METHOD && (decl->traits & asAST_TRAIT_GENERATED) != 0)
			|| decl->kind == asAST_DECL_DESTRUCTOR
			|| decl->kind == asAST_DECL_MIXIN )
		{
			hasFunctionLike = true;
			break;
		}
	}
	if( module == 0 || module->engine == 0 )
	{
		if( !hasFunctionLike )
		{
			return asAST_VERIFY_OK;
		}
		failed = true;
		error = asNO_MODULE;
		return error;
	}

	asCScriptEngine* engine = module->engine;
	asCBuilder builder(engine, module);
	builder.silent = true;
	asCRuntimeTypeBridge bridge(engine);
	asCCanonicalStringConstantLease stringConstants(engine);
	asSBytecodeCodeGenArtifact artifact;
	for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
	{
		const asCDecl* decl = context.GetDecl(asASTDeclId(i));
		if( decl == 0 )
		{
			continue;
		}
		if( !CanonicalDeclIsSupportedForCodeGen(decl->kind) )
		{
			failed = true;
			error = asNOT_SUPPORTED;
			lastFailLine = __LINE__;
			failureDetail.Format("unsupported declaration kind %s (id=%u name=%s)",
				CanonicalDeclKindName(decl->kind),
				decl->id.value,
				decl->name.AddressOf() ? decl->name.AddressOf() : "");
			if( decl->kind == asAST_DECL_NAMESPACE )
			{
				failureDetail += ": namespace-aware runtime publication is not implemented";
			}
			artifact.Abandon(engine);
			return error;
		}
	}
	const int typeRegistrationResult = RegisterCanonicalScriptTypes(context, engine, module, bridge, artifact);
	if( typeRegistrationResult < 0 )
	{
		failed = true;
		error = typeRegistrationResult;
		artifact.Abandon(engine);
		return error;
	}
	const int pendingTypeIdResult = artifact.PreparePendingTypeIds(engine);
	if( pendingTypeIdResult < 0 )
	{
		failed = true;
		error = pendingTypeIdResult;
		failureDetail = "failed to prepare detached canonical type-id projections";
		artifact.Abandon(engine);
		return error;
	}

	asCArray<const asCDecl*> functionDecls;
	for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
	{
		const asCDecl* decl = context.GetDecl(asASTDeclId(i));
		if( decl == 0 || !CanonicalDeclIsFunctionLike(decl->kind) )
		{
			continue;
		}
		if( decl->body.IsValid()
			|| decl->kind == asAST_DECL_FUNCTION
			|| decl->kind == asAST_DECL_CONSTRUCTOR
			|| decl->kind == asAST_DECL_METHOD
			|| decl->kind == asAST_DECL_DESTRUCTOR
			|| decl->kind == asAST_DECL_MIXIN )
		{
			functionDecls.PushLast(decl);
		}
	}

	asCArray<asSCodeGenGlobal> globals;
	for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
	{
		const asCDecl* child = context.GetDecl(asASTDeclId(i));
		if( child == 0 || child->kind != asAST_DECL_VAR
			|| IsCanonicalAutomaticImportScriptGlobal(child)
			|| IsCanonicalCurrentModuleScriptGlobal(child)
			|| IsCanonicalNativeGlobalProperty(child)
			|| !IsCanonicalNamespaceOrTranslationUnitMember(context, child) )
		{
			continue;
		}
		// A declaration-only global is valid AngelScript: its candidate property
		// supplies the language default value (zero/null). Do not let a module
		// containing only such globals appear to compile successfully while
		// silently omitting every declaration from publication.
		asCDataType dataType = bridge.Resolve(context, child->type);
		asSNameSpace* const nameSpace = ResolveCanonicalDeclNamespace(context, engine, module, child);
		if( !dataType.IsValid() || nameSpace == 0 )
		{
			failed = true;
			error = dataType.IsValid() ? asOUT_OF_MEMORY : asNOT_SUPPORTED;
			artifact.Abandon(engine);
			DiscardAllocatedGlobals(module, globals);
			return error;
		}
		const int globalPropertyId = artifact.GetNextPendingGlobalPropertyId(engine);
		asCGlobalProperty* prop = CreateCanonicalPendingGlobal(
			engine,
			module,
			globalPropertyId,
			child->name.AddressOf(),
			dataType,
			nameSpace);
		if( prop == 0 )
		{
			failed = true;
			error = asOUT_OF_MEMORY;
			artifact.Abandon(engine);
			DiscardAllocatedGlobals(module, globals);
			return error;
		}
		artifact.globals.PushLast(prop);
		if( child->defaultArg.GetLength() )
		{
			// The Sema-owned constant value is the source of truth. Re-parsing
			// defaultArg here would make CodeGen depend on presentation text and
			// lose the type-checked/evaluated AST result (e.g. 0x29 or 40 + 1).
			if( (!dataType.IsIntegerType() && !dataType.IsUnsignedType()
					&& !dataType.IsFloat32Type() && !dataType.IsFloat64Type())
				|| !child->hasConstantValue )
			{
				failed = true;
				error = asNOT_SUPPORTED;
				artifact.Abandon(engine);
				DiscardAllocatedGlobals(module, globals);
				return error;
			}
			const int bytes = dataType.GetSizeInMemoryBytes();
			if( bytes != 1 && bytes != 2 && bytes != 4 && bytes != 8 )
			{
				failed = true;
				error = asNOT_SUPPORTED;
				artifact.Abandon(engine);
				DiscardAllocatedGlobals(module, globals);
				return error;
			}
			if( (child->type.quals & asAST_QUAL_CONST) != 0 )
			{
				void* const memory = prop->GetAddressOfValue();
				if( memory == 0 )
				{
					failed = true;
					error = asNOT_SUPPORTED;
					artifact.Abandon(engine);
					DiscardAllocatedGlobals(module, globals);
					return error;
				}
				if( bytes == 1 )
				{
					*static_cast<asBYTE*>(memory) = (asBYTE)child->constantValue;
				}
				else if( bytes == 2 )
				{
					*static_cast<asWORD*>(memory) = (asWORD)child->constantValue;
				}
				else if( bytes == 4 )
				{
					*static_cast<asDWORD*>(memory) = (asDWORD)child->constantValue;
				}
				else
				{
					*static_cast<asQWORD*>(memory) = child->constantValue;
				}
				prop->isPureConstant = true;
			}
			else if( !EmitCanonicalScalarGlobalInitializer(
				context, builder, engine, module, artifact, prop, child, dataType,
				child->constantValue) )
			{
				failed = true;
				error = asNOT_SUPPORTED;
				artifact.Abandon(engine);
				DiscardAllocatedGlobals(module, globals);
				return error;
			}
		}
		asSCodeGenGlobal bind;
		bind.decl = child->id;
		bind.prop = prop;
		globals.PushLast(bind);
	}

	// External automatic-import and current-module globals are declaration-only
	// AST projections. They are emitter lookup entries, not candidate-owned
	// properties: artifact commit and rollback must never publish or release them.
	for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
	{
		const asCDecl* decl = context.GetDecl(asASTDeclId(i));
		if( !IsCanonicalAutomaticImportScriptGlobal(decl)
			&& !IsCanonicalCurrentModuleScriptGlobal(decl)
			&& !IsCanonicalNativeGlobalProperty(decl) )
		{
			continue;
		}
		asCGlobalProperty* property = FindCurrentModuleScriptGlobal(
			module, bridge, context, decl);
		if( property == 0 )
		{
			property = FindAutomaticImportedScriptGlobal(
				engine, module, bridge, context, decl);
		}
		if( property == 0 )
		{
			property = FindRegisteredNativeGlobalProperty(
				engine, bridge, context, decl);
		}
		if( property == 0 )
		{
			failed = true;
			error = asNO_GLOBAL_VAR;
			artifact.Abandon(engine);
			DiscardAllocatedGlobals(module, globals);
			return error;
		}
		asSCodeGenGlobal bind;
		bind.decl = decl->id;
		bind.prop = property;
		globals.PushLast(bind);
	}

	asCArray<asSCodeGenFuncBind> binds;
	asCArray<asASTDeclId> captureDecls;

	for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
	{
		const asCDecl* decl = context.GetDecl(asASTDeclId(i));
		if( decl == 0 || decl->kind != asAST_DECL_IMPORT )
			continue;
		if( decl->origin.GetLength() == 0 )
		{
			failed = true;
			error = asNO_MODULE;
			artifact.Abandon(engine);
			DiscardAllocatedGlobals(module, globals);
			return error;
		}

		const int importId = artifact.GetNextPendingImportedFunctionId(engine);
		if( importId < 0 )
		{
			failed = true;
			error = asOUT_OF_MEMORY;
			artifact.Abandon(engine);
			DiscardAllocatedGlobals(module, globals);
			return error;
		}
		asCDataType returnType = bridge.Resolve(context, decl->type);
		asSNameSpace* const nameSpace = ResolveCanonicalDeclNamespace(context, engine, module, decl);
		if( !returnType.IsValid() || nameSpace == 0 )
		{
			failed = true;
			error = returnType.IsValid() ? asOUT_OF_MEMORY : asINVALID_TYPE;
			artifact.Abandon(engine);
			DiscardAllocatedGlobals(module, globals);
			return error;
		}

		asCArray<asCDataType> parameterTypes;
		asCArray<asETypeModifiers> inOutFlags;
		asCArray<asCString *> defaultArgs;
		for( asUINT p = 0; p < decl->children.GetLength(); ++p )
		{
			const asCDecl* child = context.GetDecl(decl->children[p]);
			if( child == 0 || child->kind != asAST_DECL_PARAM )
				continue;
			asCDataType paramType = bridge.Resolve(context, child->type);
			if( !paramType.IsValid() || paramType.GetTokenType() == ttVoid )
			{
				failed = true;
				error = asINVALID_TYPE;
				artifact.Abandon(engine);
				DiscardAllocatedGlobals(module, globals);
				return error;
			}
			parameterTypes.PushLast(paramType);
			inOutFlags.PushLast(InOutFromQuals(child->type.quals));
			defaultArgs.PushLast(0);
		}

		sBindInfo* importInfo = CreateCanonicalPendingImport(
			engine,
			module,
			importId,
			decl->name,
			returnType,
			parameterTypes,
			inOutFlags,
			defaultArgs,
			nameSpace,
			decl->origin);
		if( importInfo == 0 )
		{
			failed = true;
			error = asOUT_OF_MEMORY;
			artifact.Abandon(engine);
			DiscardAllocatedGlobals(module, globals);
			return error;
		}

		if( importInfo->importedFunctionSignature == 0 )
		{
			failed = true;
			error = asNO_FUNCTION;
			artifact.Abandon(engine);
			DiscardAllocatedGlobals(module, globals);
			return error;
		}
		artifact.imports.PushLast(importInfo);

		asSCodeGenFuncBind bind;
		bind.decl = decl->id;
		bind.func = importInfo->importedFunctionSignature;
		bind.captureBegin = 0;
		bind.captureCount = 0;
		binds.PushLast(bind);
	}

	asCArray<const asCDecl*> emitDecls;
	asCArray<asCScriptFunction*> emitFunctions;
	for( asUINT i = 0; i < functionDecls.GetLength(); ++i )
	{
		const asCDecl* decl = functionDecls[i];
		if( asCScriptFunction* currentModuleFunction = FindCurrentModuleScriptGlobalFunction(
			module, bridge, context, decl) )
		{
			asSCodeGenFuncBind bind;
			bind.decl = decl->id;
			bind.func = currentModuleFunction;
			bind.captureBegin = 0;
			bind.captureCount = 0;
			binds.PushLast(bind);
			continue;
		}
		if( asCScriptFunction* importedScript = FindAutomaticImportedScriptGlobalFunction(
			engine, module, bridge, context, decl) )
		{
			asSCodeGenFuncBind bind;
			bind.decl = decl->id;
			bind.func = importedScript;
			bind.captureBegin = 0;
			bind.captureCount = 0;
			binds.PushLast(bind);
			continue;
		}
		if( asCScriptFunction* host = FindRegisteredGlobalFunction(engine, module, bridge, context, decl) )
		{
			asSCodeGenFuncBind bind;
			bind.decl = decl->id;
			bind.func = host;
			bind.captureBegin = 0;
			bind.captureCount = 0;
			binds.PushLast(bind);
			continue;
		}
		if( asCScriptFunction* hostMethod = FindExactRegisteredMethod(engine, bridge, context, decl) )
		{
			asSCodeGenFuncBind bind;
			bind.decl = decl->id;
			bind.func = hostMethod;
			bind.captureBegin = 0;
			bind.captureCount = 0;
			binds.PushLast(bind);
			continue;
		}
		if( asCScriptFunction* hostListFactory = FindExactRegisteredListFactory(engine, bridge, context, decl) )
		{
			asSCodeGenFuncBind bind;
			bind.decl = decl->id;
			bind.func = hostListFactory;
			bind.captureBegin = 0;
			bind.captureCount = 0;
			binds.PushLast(bind);
			continue;
		}
		if( asCScriptFunction* hostCtor = FindExactRegisteredConstructor(engine, bridge, context, decl) )
		{
			asSCodeGenFuncBind bind;
			bind.decl = decl->id;
			bind.func = hostCtor;
			bind.captureBegin = 0;
			bind.captureCount = 0;
			binds.PushLast(bind);
			continue;
		}
		if( ((decl->kind == asAST_DECL_METHOD) || (decl->kind == asAST_DECL_CONSTRUCTOR))
			&& !decl->body.IsValid()
			&& (decl->traits & asAST_TRAIT_GENERATED) == 0 )
		{
			continue;
		}
		asCScriptFunction* func = asNEW(asCScriptFunction)(engine, module, asFUNC_SCRIPT);
		if( func == 0 )
		{
			failed = true;
			error = asOUT_OF_MEMORY;
			artifact.Abandon(engine);
			DiscardAllocatedGlobals(module, globals);
			return error;
		}
		func->id = artifact.GetNextPendingScriptFunctionId(engine);
		if( func->id < 0 )
		{
			failed = true;
			error = asOUT_OF_MEMORY;
			func->DestroyHalfCreated();
			artifact.Abandon(engine);
			DiscardAllocatedGlobals(module, globals);
			return error;
		}
		func->nameSpace = ResolveCanonicalDeclNamespace(context, engine, module, decl);
		if( func->nameSpace == 0 )
		{
			failed = true;
			error = asOUT_OF_MEMORY;
			func->DestroyHalfCreated();
			artifact.Abandon(engine);
			DiscardAllocatedGlobals(module, globals);
			return error;
		}
		if( !FillFunctionSignature(context, engine, module, bridge, artifact, func, decl, error) )
		{
			failed = true;
			lastFailLine = __LINE__;
			if( failureDetail.GetLength() == 0 )
			{
				failureDetail.Format(
					"failed to materialize detached function metadata/signature: key=%s returnType=%u:%u error=%d",
					decl && decl->stableKey.AddressOf()
						? decl->stableKey.AddressOf() : "<missing>",
					decl ? decl->type.type.value : 0,
					decl ? decl->type.type.snapshotOwner : 0,
					error);
			}
			if( func->scriptData )
				func->scriptData->byteCode.SetLength(0);
			func->DestroyHalfCreated();
			artifact.Abandon(engine);
			DiscardAllocatedGlobals(module, globals);
			return error;
		}
		artifact.functions.PushLast(func);
		artifact.functionDeclKinds.PushLast(static_cast<int>(decl->kind));
		if( decl->kind == asAST_DECL_CONSTRUCTOR
			&& !CreateCanonicalConstructorFactory(engine, module, artifact, func) )
		{
			failed = true;
			error = asOUT_OF_MEMORY;
			artifact.Abandon(engine);
			DiscardAllocatedGlobals(module, globals);
			return error;
		}
		asSCodeGenFuncBind bind;
		bind.decl = decl->id;
		bind.func = func;
		bind.captureBegin = captureDecls.GetLength();
		bind.captureCount = 0;
		if( decl->traits & asAST_TRAIT_LAMBDA )
		{
			CollectLambdaCaptures(context, decl, captureDecls);
			bind.captureCount = captureDecls.GetLength() - bind.captureBegin;
			for( asUINT c = 0; c < bind.captureCount; ++c )
			{
				const asCDecl* captured = context.GetDecl(captureDecls[bind.captureBegin + c]);
				if( captured == 0 )
				{
					failed = true;
					error = asINVALID_DECLARATION;
					artifact.Abandon(engine);
					DiscardAllocatedGlobals(module, globals);
					return error;
				}
				asCDataType capType = bridge.Resolve(context, captured->type);
				if( !capType.IsValid() || capType.GetTokenType() == ttVoid )
				{
					failed = true;
					error = asINVALID_TYPE;
					artifact.Abandon(engine);
					DiscardAllocatedGlobals(module, globals);
					return error;
				}
				func->parameterTypes.PushLast(capType);
				func->parameterNames.PushLast(captured->name);
				func->inOutFlags.PushLast(asTM_NONE);
				func->defaultArgs.PushLast(0);
			}
		}
		if( bind.captureCount )
		{
			// Capture parameters are appended after FillFunctionSignature has
			// established the normal ABI. Recalculate the maintained function
			// record rather than inventing a CodeGen-only object layout.
			func->CalculateParameterOffsets();
		}
		binds.PushLast(bind);
		emitDecls.PushLast(decl);
		emitFunctions.PushLast(func);
	}

	asCScriptFunction* singleFunctionResult = 0;
	if( singleFunction )
	{
		for( asUINT i = 0; i < emitDecls.GetLength(); ++i )
		{
			if( emitDecls[i] == singleFunctionDecl )
			{
				singleFunctionResult = emitFunctions[i];
				break;
			}
		}
		if( singleFunctionResult == 0 )
		{
			failed = true;
			error = asNO_FUNCTION;
			failureDetail = "CompileFunction primary declaration did not produce an emitted function";
			artifact.Abandon(engine);
			DiscardAllocatedGlobals(module, globals);
			return error;
		}
		// Preserve the public CompileFunction build-artifact contract. In
		// particular, this coordinate is intentionally not cacheable; treating it
		// as an ordinary module global would let an explicitly enabled Cache V2
		// restore a function whose source does not belong to a retained module
		// generation. Nested lambdas keep their own LAMBDA invocation kind.
		singleFunctionResult->artifactInvocationKind =
			asBUILD_ARTIFACT_INVOCATION_PUBLIC_SINGLE_FUNCTION;
		if( addToModule
			&& builder.CheckNameConflict(
				singleFunctionResult->name.AddressOf(), 0, 0,
				singleFunctionResult->nameSpace) < 0 )
		{
			// Match the maintained CompileFunction name contract: functions may
			// overload one another, but a type/global/property collision rejects the
			// candidate before any Engine or module table is mutated.
			failed = true;
			error = asERROR;
			failureDetail.Format(
				"CompileFunction name conflicts with an existing declaration: %s",
				singleFunctionDecl->name.AddressOf()
					? singleFunctionDecl->name.AddressOf() : "<anonymous>");
			artifact.Abandon(engine);
			DiscardAllocatedGlobals(module, globals);
			return error;
		}
	}

	for( asUINT i = 0; i < emitDecls.GetLength(); ++i )
	{
		const asSBuildArtifactInvocation invocation =
			builder.BeginCanonicalBuildArtifactCompile(
				context, emitDecls[i], emitFunctions[i]);
		const asEBuildArtifactRestoreResult restoreResult =
			builder.TryRestoreBuildArtifact(invocation, emitFunctions[i]);
		const bool emitterInvoked =
			restoreResult != asBUILD_ARTIFACT_RESTORE_RESTORED;
#if WITH_ANGELSCRIPT_UNITTESTS
		if( emitterInvoked && i == testFailureBeforeFunctionEmission )
		{
			failed = true;
			error = testFailureCode;
			lastFailLine = __LINE__;
			const asCDecl* const failedDecl = emitDecls[i];
			failureDetail.Format(
				"injected-function-emission ordinal=%u function=%s error=%d",
				i,
				failedDecl && failedDecl->stableKey.AddressOf()
					? failedDecl->stableKey.AddressOf()
					: "<unknown>",
				error);
			builder.CompleteBuildArtifactCompile(
				invocation, emitFunctions[i], error, restoreResult, true);
			artifact.Abandon(engine);
			DiscardAllocatedGlobals(module, globals);
			return error;
		}
#endif
		if( emitterInvoked )
		{
			asCCanonicalFunctionEmitter emitter(
				context, &builder, &bridge, artifact,
				binds, globals, captureDecls,
				stringConstants, error, lastFailLine, failureDetail);
			// Constructor bytecode observes the implementation of every selected
			// base/member constructor, not only its callable ABI. Preserve the
			// maintained Builder's hard-dependency policy for this lowering window.
			const bool oldHardDependencyPolicy =
				builder.bValueDependenciesAreHard;
			builder.bValueDependenciesAreHard =
				emitDecls[i]->kind == asAST_DECL_CONSTRUCTOR;
			const bool emitted = emitter.Emit(emitFunctions[i], emitDecls[i]);
			builder.bValueDependenciesAreHard = oldHardDependencyPolicy;
			if( !emitted )
			{
				failed = true;
				if( error == asAST_VERIFY_OK )
					error = asNOT_SUPPORTED;
				const asCDecl* const failedDecl = emitDecls[i];
				if( lastFailLine == 0 )
					lastFailLine = __LINE__;
				if( failureDetail.GetLength() == 0 )
				{
					failureDetail.Format(
						"canonical emitter returned false without diagnostic ordinal=%u key=%s error=%d",
						i,
						failedDecl && failedDecl->stableKey.AddressOf()
							? failedDecl->stableKey.AddressOf() : "<unknown>",
						error);
				}
				if( failedDecl && failedDecl->range.begin.IsValid() )
				{
					asSSourceLineColumn location = {};
					if( context.GetSourceManager().GetLineColumn(
						failedDecl->range.begin, location) )
					{
						const char* const key = context.GetSourceManager().GetLogicalKey(
							failedDecl->range.begin.fileID);
						failureSection = key ? key : "";
						failureRow = location.row;
						failureColumn = location.column;
					}
				}
				builder.CompleteBuildArtifactCompile(
					invocation, emitFunctions[i], error, restoreResult, true);
				artifact.Abandon(engine);
				DiscardAllocatedGlobals(module, globals);
				return error;
			}
		}
		builder.CompleteBuildArtifactCompile(
			invocation, emitFunctions[i], asSUCCESS, restoreResult, emitterInvoked);
	}
	for( asUINT i = 0; i < artifact.constructorFactories.GetLength(); ++i )
	{
		const asSCanonicalConstructorFactoryPair& pair =
			artifact.constructorFactories[i];
		const asCDecl* constructorDecl = 0;
		for( asUINT bindIndex = 0; bindIndex < emitFunctions.GetLength(); ++bindIndex )
		{
			if( emitFunctions[bindIndex] == pair.constructor )
			{
				constructorDecl = emitDecls[bindIndex];
				break;
			}
		}
		const asSBuildArtifactInvocation invocation =
			builder.BeginCanonicalBuildArtifactCompile(
				context, constructorDecl, pair.factory);
		const asEBuildArtifactRestoreResult restoreResult =
			builder.TryRestoreBuildArtifact(invocation, pair.factory);
		const bool emitterInvoked =
			restoreResult != asBUILD_ARTIFACT_RESTORE_RESTORED;
		if( emitterInvoked && !EmitCanonicalConstructorFactory(builder, pair) )
		{
			failed = true;
			error = asNOT_SUPPORTED;
			lastFailLine = __LINE__;
			asCScriptFunction* const constructor =
				pair.constructor;
			failureDetail.Format(
				"constructor-factory-emission function=%s error=%d",
				constructor && constructor->canonicalASTStableDeclKey.AddressOf()
					? constructor->canonicalASTStableDeclKey.AddressOf()
					: "<unknown>",
				error);
			builder.CompleteBuildArtifactCompile(
				invocation, pair.factory, error, restoreResult, true);
			artifact.Abandon(engine);
			DiscardAllocatedGlobals(module, globals);
			return error;
		}
		builder.CompleteBuildArtifactCompile(
			invocation, pair.factory, asSUCCESS, restoreResult, emitterInvoked);
	}

	if( functionDecls.GetLength() == 0
		&& globals.GetLength() == 0
		&& binds.GetLength() == 0
		&& artifact.types.GetLength() == 0 )
	{
		return asAST_VERIFY_OK;
	}
	const asSRuntimeTypeBindingEnvironment bindingEnvironment =
		asMakeCurrentRuntimeTypeBindingEnvironment();
	const int relocationResult = artifact.ResolveAndPatchTypeRelocations(
		engine, context, bindingEnvironment, &failureDetail
#if WITH_ANGELSCRIPT_UNITTESTS
		, testFailureAtTypeRelocationUse,
		testTypeRelocationFailureCode,
		&testTypeRelocationFailureInjected
#endif
		);
	if( relocationResult != asRUNTIME_TYPE_BIND_OK )
	{
		failed = true;
		error = relocationResult;
		lastFailLine = __LINE__;
		artifact.Abandon(engine);
		DiscardAllocatedGlobals(module, globals);
		return error;
	}
#if WITH_ANGELSCRIPT_UNITTESTS
	bool testCommitFailureInjected = false;
#endif
	int commit = asAST_VERIFY_OK;
	if( singleFunction && !addToModule )
	{
		commit = artifact.CommitDetachedSingleFunction(
			module, singleFunctionResult);
	}
	else
	{
		commit = artifact.Commit(module, !singleFunction
#if WITH_ANGELSCRIPT_UNITTESTS
			, testFailureAfterCommitStage, testCommitFailureCode,
			&testCommitFailureInjected
#endif
		);
	}
	if( commit < 0 )
	{
		failed = true;
		error = commit;
#if WITH_ANGELSCRIPT_UNITTESTS
		if( testCommitFailureInjected )
		{
			lastFailLine = __LINE__;
			failureDetail.Format(
				"injected-commit-publication stage=%u error=%d",
				testFailureAfterCommitStage,
				commit);
		}
#endif
		artifact.Abandon(engine);
		DiscardAllocatedGlobals(module, globals);
		return error;
	}
	// Record only a successfully committed CodeGen artifact. A failed Generate
	// must not leave a digest that falsely claims the retained snapshot was the
	// source of installed Bytecode.
	module->SetLastCanonicalCodeGenAstDigest(ComputeCanonicalAstDigest(context));
	if( singleFunction )
	{
		// Attached Commit transferred the construction reference into module
		// ownership, so add one temporary reference for asCModule::CompileFunction
		// to release after it has established the caller's external reference.
		// Detached Commit deliberately leaves the primary construction reference
		// as that same temporary owner.
		if( addToModule )
			singleFunctionResult->AddRefInternal();
		*outFunction = singleFunctionResult;
	}
	return asAST_VERIFY_OK;
}

void asCBytecodeCodeGenDumpFunction(const asCScriptFunction* func, asCString& out)
{
	if( func == 0 )
	{
		out += "<null function>\n";
		return;
	}
	asCString line;
	line.Format("func=%s id=%d type=%d\n",
		func->GetName() ? func->GetName() : "",
		func->GetId(),
		static_cast<int>(func->funcType));
	out += line;
	if( func->scriptData == 0 )
	{
		out += "scriptData=<null>\n";
		return;
	}
	const int declaredAt = func->scriptData->declaredAt;
	line.Format(
		"debug section=%s sectionIdx=%d declaredRow=%d declaredColumn=%d lineEntries=%u sectionTransitions=%u\n",
		func->GetScriptSectionName() ? func->GetScriptSectionName() : "",
		func->scriptData->scriptSectionIdx,
		declaredAt & 0xFFFFF,
		(declaredAt >> 20) & 0xFFF,
		func->scriptData->lineNumbers.GetLength() / 2,
		func->scriptData->sectionIdxs.GetLength() / 2);
	out += line;
	for( asUINT n = 0; n + 1 < func->scriptData->lineNumbers.GetLength(); n += 2 )
	{
		const int encoded = func->scriptData->lineNumbers[n + 1];
		line.Format(
			"  line pc=%d row=%d column=%d encoded=%d\n",
			func->scriptData->lineNumbers[n],
			encoded & 0xFFFFF,
			(encoded >> 20) & 0xFFF,
			encoded);
		out += line;
	}
	for( asUINT n = 0; n + 1 < func->scriptData->sectionIdxs.GetLength(); n += 2 )
	{
		line.Format(
			"  section pc=%d sectionIdx=%d\n",
			func->scriptData->sectionIdxs[n],
			func->scriptData->sectionIdxs[n + 1]);
		out += line;
	}
	line.Format(
		"frame variableSpace=%u stackNeeded=%d debugVariables=%u temporaryVariables=%u pointerVariables=%u\n",
		func->scriptData->variableSpace,
		func->scriptData->stackNeeded,
		func->scriptData->variables.GetLength(),
		func->scriptData->temporaryVariables.GetLength(),
		func->scriptData->pointerVariablePos.GetLength());
	out += line;
	for( asUINT n = 0; n < func->scriptData->variables.GetLength(); ++n )
	{
		const asSScriptVariable* const variable = func->scriptData->variables[n];
		const asCString typeName = variable
			? variable->type.Format(func->nameSpace, true)
			: asCString("<missing>");
		line.Format(
			"  var index=%u name=%s type=%s stack=%d onHeap=%d declaredPc=%u\n",
			n,
			variable && variable->name.AddressOf()
				? variable->name.AddressOf() : "<missing>",
			typeName.AddressOf() ? typeName.AddressOf() : "<invalid>",
			variable ? variable->stackOffset : 0,
			variable && variable->onHeap ? 1 : 0,
			variable ? variable->declaredAtProgramPos : 0);
		out += line;
	}
	for( asUINT n = 0; n < func->scriptData->temporaryVariables.GetLength(); ++n )
	{
		const asCScriptFunction::ScriptFunctionData::ScriptTempVariable& temporary =
			func->scriptData->temporaryVariables[n];
		line.Format("  temp index=%u stack=%d token=%d\n",
			n, temporary.Offset, static_cast<int>(temporary.Token));
		out += line;
	}
	for( asUINT n = 0; n < func->scriptData->pointerVariablePos.GetLength(); ++n )
	{
		line.Format("  pointer index=%u stack=%d\n",
			n, func->scriptData->pointerVariablePos[n]);
		out += line;
	}
	line.Format("heap=%u types=%u bytecode=%u\n",
		func->scriptData->objVariablesOnHeap,
		func->scriptData->objVariableTypes.GetLength(),
		func->scriptData->byteCode.GetLength());
	out += line;
	for( asUINT n = 0; n < func->scriptData->objVariableTypes.GetLength(); ++n )
	{
		asCTypeInfo* info = func->scriptData->objVariableTypes[n];
		const int pos = n < func->scriptData->objVariablePos.GetLength()
			? func->scriptData->objVariablePos[n]
			: 0;
		line.Format("  objvar pos=%d flags=0x%x name=%s\n",
			pos,
			info ? info->GetFlags() : 0,
			info && info->GetName() ? info->GetName() : "");
		out += line;
	}
	const asDWORD* bytes = func->scriptData->byteCode.AddressOf();
	const asUINT length = func->scriptData->byteCode.GetLength();
	asUINT index = 0;
	while( index < length )
	{
		const asEBCInstr instruction = static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(bytes + index));
		const char* name = asBCInfo[instruction].name;
		line.Format("  bc %s", name ? name : "?");
		out += line;
		if( instruction == asBC_FREE || instruction == asBC_REFCPY || instruction == asBC_FuncPtr )
		{
			asCTypeInfo* operand = reinterpret_cast<asCTypeInfo*>(asBC_PTRARG(bytes + index));
			if( instruction == asBC_FuncPtr )
			{
				asCScriptFunction* callee = reinterpret_cast<asCScriptFunction*>(asBC_PTRARG(bytes + index));
				line.Format(" callee=%s", callee && callee->GetName() ? callee->GetName() : "");
				out += line;
			}
			else
			{
				line.Format(" type=%s", operand && operand->GetName() ? operand->GetName() : "");
				out += line;
			}
		}
		out += "\n";
		const asUINT size = static_cast<asUINT>(asBCTypeSize[asBCInfo[instruction].type]);
		index += size > 0 ? size : 1;
	}
}

void asCBytecodeCodeGenDumpModule(const asCModule* module, asCString& out)
{
	if( module == 0 )
	{
		out += "<null module>\n";
		return;
	}
	asCString line;
	asQWORD canonicalAstDigest = 0;
	if( module->GetLastCanonicalCodeGenAstDigest(canonicalAstDigest) )
	{
		line.Format("module=%s functions=%u publisher=%d legacyCompilerInvocations=%u canonicalAstDigest=%016llx\n",
			module->GetName() ? module->GetName() : "",
			module->scriptFunctions.GetLength(),
			static_cast<int>(module->GetLastBytecodePublisher()),
			module->GetLastLegacyCompilerInvocationCount(),
			static_cast<unsigned long long>(canonicalAstDigest));
	}
	else
	{
		line.Format("module=%s functions=%u publisher=%d legacyCompilerInvocations=%u canonicalAstDigest=<none>\n",
			module->GetName() ? module->GetName() : "",
			module->scriptFunctions.GetLength(),
			static_cast<int>(module->GetLastBytecodePublisher()),
			module->GetLastLegacyCompilerInvocationCount());
	}
	out += line;
	for( asUINT n = 0; n < module->scriptFunctions.GetLength(); ++n )
		asCBytecodeCodeGenDumpFunction(module->scriptFunctions[n], out);
}

END_AS_NAMESPACE

#endif // AS_NO_COMPILER
