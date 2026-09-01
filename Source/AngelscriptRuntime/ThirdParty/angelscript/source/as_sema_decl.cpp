#include "as_sema_decl.h"
#include "as_config.h"
#include "as_datatype.h"
#include "as_module.h"
#include "as_objecttype.h"
#include "as_runtime_type_bridge.h"
#include "as_sema_lifetime.h"
#include "as_scriptcode.h"
#include "as_scriptengine.h"
#include "as_tokendef.h"
#include "as_tokenizer.h"
#include "as_typeinfo.h"
#include <climits>
#include <cstdlib>

BEGIN_AS_NAMESPACE

static const char* const CANONICAL_NATIVE_TYPE_VIEW_ORIGIN =
	"canonical-native-type-view";

static eTokenType CanonicalPrimitiveToken(eTokenType token)
{
	switch( token )
	{
	case ttVoid:
	case ttBool:
	case ttInt8:
	case ttInt16:
	case ttInt:
	case ttInt64:
	case ttUInt8:
	case ttUInt16:
	case ttUInt:
	case ttUInt64:
	case ttFloat:
	case ttDouble:
		return token;
	case ttFloat32:
		return ttFloat;
	case ttFloat64:
		return ttDouble;
	default:
		return ttUnrecognizedToken;
	}
}

static asDWORD DeclaredEffectiveQualifiers(
	const asCDecl* declaration,
	asDWORD authoredQualifiers)
{
	// A Canonical declaration may own an intrinsic language qualifier that is
	// absent from source punctuation. Script class/interface record headers use
	// this for implicit-handle semantics. Merge only that intrinsic handle bit;
	// reference direction and auto-handle remain properties of the use site.
	if( declaration != 0
		&& declaration->type.IsValid()
		&& declaration->type.IsHandle() )
	{
		authoredQualifiers |= asAST_QUAL_HANDLE;
	}
	return authoredQualifiers;
}

static asCString CanonicalStructuredTypeKey(const asCString& spelling)
{
	if( spelling.GetLength() >= 2
		&& spelling[0] == ':' && spelling[1] == ':' )
	{
		return asCString(spelling.AddressOf() + 2);
	}
	return spelling;
}

static asCQualType InternResolvedLexicalType(
	asCASTContext& context,
	const asCDecl* declaration,
	asDWORD authoredQualifiers)
{
	if( declaration == 0 || declaration->stableKey.GetLength() == 0 )
	{
		return asCQualType();
	}
	switch( declaration->kind )
	{
	case asAST_DECL_ENUM:
		return context.InternNamedType(
			asAST_TYPE_ENUM,
			declaration->stableKey.AddressOf(),
			authoredQualifiers);
	case asAST_DECL_CLASS:
	case asAST_DECL_INTERFACE:
		return context.InternNamedType(
			declaration->kind == asAST_DECL_INTERFACE
				|| (declaration->traits & asAST_TRAIT_VALUE) == 0
				? asAST_TYPE_REFERENCE_OBJECT
				: asAST_TYPE_VALUE_OBJECT,
			declaration->stableKey.AddressOf(),
			DeclaredEffectiveQualifiers(
				declaration, authoredQualifiers));
	case asAST_DECL_FUNCDEF:
		return context.InternNamedType(
			asAST_TYPE_FUNCDEF,
			declaration->stableKey.AddressOf(),
			authoredQualifiers);
	default:
		return asCQualType();
	}
}

static bool StableTypeKeyHasUnqualifiedName(
	const asCString& stableKey,
	const char* name)
{
	if( name == 0 || name[0] == 0 )
	{
		return false;
	}
	const int separator = stableKey.FindLast("::");
	return separator >= 0
		? stableKey.SubString(separator + 2).Equals(name)
		: stableKey.Equals(name);
}

bool asCSema::IsKnownTypeNameForParsing(const char* name) const
{
	if( name == 0 || name[0] == 0 )
	{
		return false;
	}

	// Current-source declarations are already Canonical Sema facts. Match the
	// unqualified token exactly, mirroring Parser's historical
	// "regardless of namespace" disambiguation without reading a native tree.
	for( asUINT declarationIndex = 1;
		declarationIndex <= context.GetDeclCount(); ++declarationIndex )
	{
		const asCDecl* const declaration =
			context.GetDecl(asASTDeclId(declarationIndex));
		if( declaration == 0 || !declaration->name.Equals(name) )
		{
			continue;
		}
		switch( declaration->kind )
		{
		case asAST_DECL_CLASS:
		case asAST_DECL_INTERFACE:
		case asAST_DECL_ENUM:
		case asAST_DECL_FUNCDEF:
		case asAST_DECL_TYPEDEF:
			return true;
		default:
			break;
		}
	}

	// Registered and explicitly imported Runtime types were copied into this
	// generation before parsing. Keep the query pointer-free and deterministic;
	// ambiguity still means that a type-shaped token exists, while Sema reports
	// the actual ambiguous lookup if the expression consumes it as a type.
	for( asUINT typeIndex = 0;
		typeIndex < canonicalRegisteredTypeDeclarations.GetLength(); ++typeIndex )
	{
		if( StableTypeKeyHasUnqualifiedName(
			canonicalRegisteredTypeDeclarations[typeIndex].stableKey, name) )
		{
			return true;
		}
	}
	for( asUINT templateIndex = 0;
		templateIndex < canonicalTemplateDeclarations.GetLength(); ++templateIndex )
	{
		if( StableTypeKeyHasUnqualifiedName(
			canonicalTemplateDeclarations[templateIndex].stableKey, name) )
		{
			return true;
		}
	}
	return false;
}

const asSCanonicalRegisteredTypeDeclarationFact*
asCSema::FindExactCanonicalRegisteredTypeDeclaration(
	const asCString& stableKey) const
{
	// The Engine copy contract orders facts by stable key. Binary lookup keeps
	// type resolution independent of the size of the UE/application registry.
	asUINT first = 0;
	asUINT count = canonicalRegisteredTypeDeclarations.GetLength();
	while( count != 0 )
	{
		const asUINT step = count / 2;
		const asUINT index = first + step;
		const asSCanonicalRegisteredTypeDeclarationFact& declaration =
			canonicalRegisteredTypeDeclarations[index];
		const int comparison = declaration.stableKey.Compare(stableKey);
		if( comparison == 0 )
		{
			return &declaration;
		}
		if( comparison < 0 )
		{
			first = index + 1;
			count -= step + 1;
		}
		else
		{
			count = step;
		}
	}
	return 0;
}

const asSCanonicalRegisteredTypeDeclarationFact*
asCSema::ResolveCanonicalRegisteredTypeDeclaration(
	const asCString& authoredName,
	bool absoluteScope,
	asASTDeclId lookupScope,
	asCString& resolvedStableKey,
	bool& ambiguous) const
{
	resolvedStableKey = "";
	ambiguous = false;
	asCString canonicalName = authoredName;
	if( canonicalName.GetLength() >= 2
		&& canonicalName[0] == ':' && canonicalName[1] == ':' )
	{
		canonicalName = canonicalName.SubString(2);
		absoluteScope = true;
	}
	if( canonicalName.GetLength() == 0 )
	{
		return 0;
	}

	auto findExact = [this, &resolvedStableKey, &ambiguous](
		const asCString& stableKey)
		-> const asSCanonicalRegisteredTypeDeclarationFact*
	{
		const asSCanonicalRegisteredTypeDeclarationFact* const fact =
			FindExactCanonicalRegisteredTypeDeclaration(stableKey);
		if( fact == 0 )
		{
			return 0;
		}
		if( fact->ambiguous )
		{
			ambiguous = true;
			return 0;
		}
		resolvedStableKey = fact->stableKey;
		return fact;
	};

	if( !absoluteScope )
	{
		asASTDeclId current = lookupScope;
		while( current.IsValid() )
		{
			const asCDecl* const declaration = context.GetDecl(current);
			if( declaration == 0 )
			{
				break;
			}
			if( declaration->kind == asAST_DECL_NAMESPACE
				&& declaration->stableKey.GetLength() != 0 )
			{
				asCString candidate = declaration->stableKey;
				candidate += "::";
				candidate += canonicalName;
				const asSCanonicalRegisteredTypeDeclarationFact* const fact =
					findExact(candidate);
				if( fact != 0 || ambiguous )
				{
					return fact;
				}
			}
			current = declaration->parent;
		}
	}
	return findExact(canonicalName);
}

asCQualType asCSema::ResolveQualifiedNominalType(
	const asCString& canonicalSpelling,
	bool absoluteScope,
	asDWORD authoredQualifiers,
	bool& ambiguous)
{
	ambiguous = false;
	if( canonicalSpelling.GetLength() == 0 )
	{
		return asCQualType();
	}

	auto resolveExact = [this, authoredQualifiers, &ambiguous](
		const asCString& stableKey) -> asCQualType
	{
		asCQualType existingNativeView;
		for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
		{
			const asCDecl* const declaration =
				context.GetDecl(asASTDeclId(i));
			if( declaration != 0
				&& declaration->stableKey.Equals(stableKey) )
			{
				const asCQualType lexical = InternResolvedLexicalType(
					context, declaration, authoredQualifiers);
				if( lexical.IsValid() )
				{
					// A range-less native symbol view may have been interned
					// earlier for a property, method, or behaviour. Do not let
					// that partial shell bypass the registered declaration graph;
					// project/repair its stable base edges below first.
					if( declaration->origin.Equals(
						"canonical-native-type-view") )
					{
						existingNativeView = lexical;
						continue;
					}
					return lexical;
				}
			}
		}

		const asSCanonicalRegisteredTypeDeclarationFact* const fact =
			FindExactCanonicalRegisteredTypeDeclaration(stableKey);
		if( fact == 0 )
		{
			return existingNativeView;
		}
		if( fact->ambiguous )
		{
			ambiguous = true;
			return asCQualType();
		}
		asDWORD effectiveQualifiers = authoredQualifiers;
		if( (fact->flags & asOBJ_IMPLICIT_HANDLE) != 0 )
		{
			effectiveQualifiers |= asAST_QUAL_HANDLE;
		}
		if( (fact->kind == asAST_TYPE_REFERENCE_OBJECT
				|| fact->kind == asAST_TYPE_VALUE_OBJECT)
			&& context.GetTranslationUnit().IsValid() )
		{
			bool projectionAmbiguous = false;
			const asASTDeclId projected =
				ProjectCanonicalRegisteredBaseType(
					fact->stableKey,
					true,
					context.GetTranslationUnit(),
					projectionAmbiguous);
			if( projectionAmbiguous )
			{
				ambiguous = true;
				return asCQualType();
			}
			if( projected.IsValid() )
			{
				const asCQualType lexical = InternResolvedLexicalType(
					context,
					context.GetDecl(projected),
					effectiveQualifiers);
				if( lexical.IsValid() )
				{
					return lexical;
				}
			}
		}
		if( fact->kind == asAST_TYPE_FUNCDEF
			&& context.GetTranslationUnit().IsValid() )
		{
			const asASTDeclId projected =
				ProjectCanonicalRegisteredFuncdef(fact->stableKey);
			if( projected.IsValid() )
			{
				const asCQualType lexical = InternResolvedLexicalType(
					context,
					context.GetDecl(projected),
					effectiveQualifiers);
				if( lexical.IsValid() )
				{
					return lexical;
				}
			}
		}
		return context.InternNamedType(
			fact->kind,
			fact->stableKey.AddressOf(),
			effectiveQualifiers);
	};

	// AngelScript resolves a relative qualified scope from the current
	// namespace and then each enclosing namespace. Function/record contexts do
	// not contribute spelling of their own here; climb until a namespace is
	// reached, then try its stable owner key. A leading `::` skips this search.
	if( !absoluteScope )
	{
		asASTDeclId current = CurrentDeclContext();
		while( current.IsValid() )
		{
			const asCDecl* const declaration = context.GetDecl(current);
			if( declaration == 0 )
			{
				break;
			}
			if( declaration->kind == asAST_DECL_NAMESPACE
				&& declaration->stableKey.GetLength() != 0 )
			{
				asCString candidate = declaration->stableKey;
				candidate += "::";
				candidate += canonicalSpelling;
				const asCQualType resolved = resolveExact(candidate);
				if( resolved.IsValid() || ambiguous )
				{
					return resolved;
				}
			}
			current = declaration->parent;
		}
	}

	// The translation-unit root has no language namespace component. It is the
	// only candidate for an absolute name and the final candidate for a relative
	// name after enclosing-namespace search is exhausted.
	return resolveExact(canonicalSpelling);
}

const asSCanonicalTemplateDeclarationFact*
asCSema::FindExactCanonicalTemplateDeclaration(
	const asCString& stableKey) const
{
	for( asUINT index = 0;
		index < canonicalTemplateDeclarations.GetLength(); ++index )
	{
		const asSCanonicalTemplateDeclarationFact& declaration =
			canonicalTemplateDeclarations[index];
		if( declaration.stableKey.Equals(stableKey) )
		{
			return &declaration;
		}
	}
	return 0;
}

const asSCanonicalTemplateDeclarationFact*
asCSema::ResolveCanonicalTemplateDeclaration(
	const asCString& authoredBase,
	bool absoluteScope,
	asCString& resolvedStableKey) const
{
	resolvedStableKey.SetLength(0);
	if( authoredBase.GetLength() == 0 )
	{
		return 0;
	}

	asCString canonicalBase = authoredBase;
	if( canonicalBase.GetLength() >= 2
		&& canonicalBase[0] == ':' && canonicalBase[1] == ':' )
	{
		canonicalBase = asCString(canonicalBase.AddressOf() + 2);
		absoluteScope = true;
	}

	if( !absoluteScope )
	{
		asASTDeclId current = CurrentDeclContext();
		while( current.IsValid() )
		{
			const asCDecl* const declaration = context.GetDecl(current);
			if( declaration == 0 )
			{
				break;
			}
			if( declaration->kind == asAST_DECL_NAMESPACE
				&& declaration->stableKey.GetLength() != 0 )
			{
				asCString candidate = declaration->stableKey;
				candidate += "::";
				candidate += canonicalBase;
				const asSCanonicalTemplateDeclarationFact* const exact =
					FindExactCanonicalTemplateDeclaration(candidate);
				if( exact != 0 )
				{
					resolvedStableKey = exact->stableKey;
					return exact;
				}
			}
			current = declaration->parent;
		}
	}

	const asSCanonicalTemplateDeclarationFact* const exact =
		FindExactCanonicalTemplateDeclaration(canonicalBase);
	if( exact != 0 )
	{
		resolvedStableKey = exact->stableKey;
	}
	return exact;
}

asCQualType asCSema::InternCanonicalTemplateInstanceType(
	const asSCanonicalTemplateDeclarationFact* declaration,
	const asCString& instanceKey,
	asDWORD quals,
	const asCArray<asCQualType>* arguments)
{
	if( instanceKey.GetLength() == 0 )
	{
		return asCQualType();
	}
	const asCQualType interned = context.InternNamedType(
		asAST_TYPE_TEMPLATE,
		instanceKey.AddressOf(),
		quals);
	if( interned.IsValid()
		&& declaration != 0
		&& declaration->requiresRuntimeValidation )
	{
		context.SetTypeRequiresRuntimeValidation(interned.type, true);
	}
	if( interned.IsValid() && arguments != 0 )
	{
		if( context.SetTypeTemplateArguments(interned.type, *arguments)
			!= asAST_VERIFY_OK )
		{
			return asCQualType();
		}
	}
	return interned;
}

bool asCSema::IsCanonicalTemplateSubtypeAllowed(
	const asSCanonicalTemplateDeclarationFact& declaration,
	const asCQualType& subtype) const
{
	const asCType* const canonical = context.GetType(subtype.type);
	if( canonical == 0 )
	{
		return false;
	}

	bool isValueSubtype = canonical->kind == asAST_TYPE_VOID
		|| canonical->kind == asAST_TYPE_PRIMITIVE
		|| canonical->kind == asAST_TYPE_VALUE_OBJECT;
	bool isReferenceSubtype = canonical->kind == asAST_TYPE_REFERENCE_OBJECT;
	if( canonical->kind == asAST_TYPE_TEMPLATE )
	{
		asCString base;
		for( asUINT index = 0; index < canonical->stableKey.GetLength(); ++index )
		{
			if( canonical->stableKey[index] == '<' )
			{
				base.Assign(canonical->stableKey.AddressOf(), index);
				break;
			}
		}
		const asSCanonicalTemplateDeclarationFact* const nested =
			FindExactCanonicalTemplateDeclaration(base);
		if( nested != 0 )
		{
			isValueSubtype = (nested->flags & asOBJ_VALUE) != 0;
			isReferenceSubtype = (nested->flags & asOBJ_REF) != 0;
		}
	}

	if( !declaration.acceptValueSubtype && isValueSubtype )
	{
		return false;
	}
	if( !declaration.acceptReferenceSubtype && isReferenceSubtype )
	{
		return false;
	}
	return true;
}

bool asCSema::CanonicalTemplateHasChildFuncdef(
	const asSCanonicalTemplateDeclarationFact& declaration,
	const asCString& name) const
{
	for( asUINT index = 0;
		index < declaration.childFuncdefs.GetLength(); ++index )
	{
		if( declaration.childFuncdefs[index].Equals(name) )
		{
			return true;
		}
	}
	return false;
}

bool asCSema::IsCanonicalNonPrimitiveValueType(
	const asCQualType& type) const
{
	const asCType* const canonical = context.GetType(type.type);
	if( canonical == 0 )
	{
		return false;
	}
	if( canonical->kind == asAST_TYPE_VALUE_OBJECT )
	{
		return true;
	}
	if( canonical->kind != asAST_TYPE_TEMPLATE )
	{
		return false;
	}

	// Template instances intentionally retain one Canonical kind regardless of
	// the host template's storage category. Recover only that declaration-level
	// VALUE fact from the pointer-free Sema snapshot; never instantiate the
	// Runtime application type merely to choose a declaration lifecycle trait.
	asCString templateBase;
	for( asUINT index = 0; index < canonical->stableKey.GetLength(); ++index )
	{
		if( canonical->stableKey[index] == '<' )
		{
			templateBase.Assign(canonical->stableKey.AddressOf(), index);
			break;
		}
	}
	const asSCanonicalTemplateDeclarationFact* const declaration =
		templateBase.GetLength() != 0
			? FindExactCanonicalTemplateDeclaration(templateBase) : 0;
	return declaration != 0 && (declaration->flags & asOBJ_VALUE) != 0;
}

bool asCSema::InferCanonicalAutoType(
	const asCQualType& placeholder,
	asASTExprId initializer,
	bool allowReference,
	bool autoConstRef,
	asCQualType& inferred) const
{
	const asCType* const placeholderType = context.GetType(placeholder.type);
	const bool isAuto = placeholderType != 0
		&& placeholderType->stableKey.Equals("auto");
	if( !isAuto )
	{
		inferred = placeholder;
		// The preprocessor's range-for lowering applies this internal marker to
		// explicit element types too. It is construction intent only: consume it
		// into the final exact QualType and never persist a marker in the AST.
		if( autoConstRef && !inferred.IsReference() )
		{
			if( IsCanonicalNonPrimitiveValueType(inferred) )
			{
				inferred.quals |= asAST_QUAL_REFERENCE;
			}
			inferred.quals |= asAST_QUAL_CONST;
		}
		return asASTQualifiersAreValid(inferred.quals);
	}

	const asCExpr* const expression = context.GetExpr(initializer);
	const asCType* const initializerType = expression != 0
		? context.GetType(expression->type.type) : 0;
	const asCDecl* const resolved = expression != 0
		? context.GetDecl(expression->resolvedDecl) : 0;
	if( expression == 0 || initializerType == 0
		|| expression->kind == asAST_EXPR_NULL_LITERAL
		|| expression->kind == asAST_EXPR_ERROR
		|| initializerType->kind == asAST_TYPE_ERROR
		|| initializerType->kind == asAST_TYPE_WILDCARD
		|| initializerType->stableKey.Equals("auto")
		|| (resolved != 0 && (resolved->traits & asAST_TRAIT_LAMBDA) != 0) )
	{
		return false;
	}

	// Match legacy CompileAutoType: nominal identity comes from the already
	// typed initializer; reference intent is authored by local `auto&` and is
	// never inherited merely from expression value category.
	inferred = expression->type;
	inferred.quals &= ~(asAST_QUAL_REFERENCE
		| asAST_QUAL_IN | asAST_QUAL_OUT | asAST_QUAL_INOUT
		| asAST_QUAL_AUTO_HANDLE);
	if( allowReference && placeholder.IsReference() )
	{
		inferred.quals |= asAST_QUAL_REFERENCE;
	}

	if( placeholder.IsHandle() && !inferred.IsHandle() )
	{
		bool supportsHandle = initializerType->kind == asAST_TYPE_REFERENCE_OBJECT
			|| initializerType->kind == asAST_TYPE_FUNCDEF;
		if( !supportsHandle && initializerType->kind == asAST_TYPE_TEMPLATE )
		{
			asCString templateBase;
			for( asUINT index = 0;
				index < initializerType->stableKey.GetLength(); ++index )
			{
				if( initializerType->stableKey[index] == '<' )
				{
					templateBase.Assign(
						initializerType->stableKey.AddressOf(), index);
					break;
				}
			}
			const asSCanonicalTemplateDeclarationFact* const declaration =
				templateBase.GetLength() != 0
					? FindExactCanonicalTemplateDeclaration(templateBase) : 0;
			supportsHandle = declaration != 0
				&& (declaration->flags
					& (asOBJ_REF | asOBJ_ASHANDLE | asOBJ_IMPLICIT_HANDLE)) != 0;
		}
		if( !supportsHandle )
		{
			const asSCanonicalRegisteredTypeDeclarationFact* const declaration =
				FindExactCanonicalRegisteredTypeDeclaration(
					initializerType->stableKey);
			supportsHandle = declaration != 0
				&& (declaration->flags
					& (asOBJ_REF | asOBJ_ASHANDLE | asOBJ_IMPLICIT_HANDLE)) != 0;
		}
		if( !supportsHandle )
		{
			return false;
		}
		inferred.quals |= asAST_QUAL_HANDLE;
	}

	// On handles CONST denotes handle-to-const; on values it denotes readonly.
	// Non-const auto values discard initializer const unless `auto&` retained
	// the source reference, exactly as legacy CompileAutoType does.
	if( placeholder.IsConst() )
	{
		inferred.quals |= asAST_QUAL_CONST;
	}
	else if( !(allowReference && placeholder.IsReference()) )
	{
		inferred.quals &= ~asAST_QUAL_CONST;
	}

	if( autoConstRef && !inferred.IsReference() )
	{
		if( IsCanonicalNonPrimitiveValueType(inferred) )
		{
			inferred.quals |= asAST_QUAL_REFERENCE;
		}
		inferred.quals |= asAST_QUAL_CONST;
	}
	return asASTQualifiersAreValid(inferred.quals);
}

asCQualType asCSema::ActOnQualType(const char* key, asDWORD quals)
{
	asCString text;
	if( key && key[0] )
	{
		text = key;
	}
	if( text.Equals("float") )
	{
		return context.InternPrimitive(engine && engine->ep.floatIsFloat64 ? ttFloat64 : ttFloat32, quals);
	}
	if( text.Equals("double") )
	{
		return context.InternPrimitive(ttFloat64, quals);
	}
	if( text.Equals("float32") ) return context.InternPrimitive(ttFloat32, quals);
	if( text.Equals("float64") ) return context.InternPrimitive(ttFloat64, quals);

	bool isTemplate = false;
	for( asUINT i = 0; i < text.GetLength(); ++i )
	{
		if( text[i] == '<' )
		{
			isTemplate = true;
			break;
		}
	}
	if( isTemplate )
	{
		asCString authoredBase;
		asCString templateSuffix;
		for( asUINT index = 0; index < text.GetLength(); ++index )
		{
			if( text[index] == '<' )
			{
				authoredBase.Assign(text.AddressOf(), index);
				templateSuffix = text.AddressOf() + index;
				break;
			}
		}
		asCString resolvedBase;
		const asSCanonicalTemplateDeclarationFact* const declaration =
			ResolveCanonicalTemplateDeclaration(
				authoredBase, false, resolvedBase);
		asCString stableKey = declaration != 0 ? resolvedBase : text;
		if( declaration != 0 )
		{
			stableKey += templateSuffix;
			if( (declaration->flags & asOBJ_IMPLICIT_HANDLE) != 0 )
			{
				quals |= asAST_QUAL_HANDLE;
			}
		}
		return InternCanonicalTemplateInstanceType(
			declaration,
			stableKey,
			quals);
	}

	if( text.Equals("void") ) return context.InternPrimitive(ttVoid, quals);
	if( text.Equals("bool") ) return context.InternPrimitive(ttBool, quals);
	if( text.Equals("int") ) return context.InternPrimitive(ttInt, quals);
	if( text.Equals("int8") ) return context.InternPrimitive(ttInt8, quals);
	if( text.Equals("int16") ) return context.InternPrimitive(ttInt16, quals);
	if( text.Equals("int64") ) return context.InternPrimitive(ttInt64, quals);
	if( text.Equals("uint") ) return context.InternPrimitive(ttUInt, quals);
	if( text.Equals("uint8") ) return context.InternPrimitive(ttUInt8, quals);
	if( text.Equals("uint16") ) return context.InternPrimitive(ttUInt16, quals);
	if( text.Equals("uint64") ) return context.InternPrimitive(ttUInt64, quals);

	if( text.GetLength() == 0 )
	{
		return context.InternPrimitive(ttInt, quals);
	}

	// Script declarations are the lexical type authority while the module is
	// being compiled.  They are not visible through the engine type registry
	// yet, so consulting only GetTypeInfoByName would misclassify a script enum
	// as a value object until publication.  Preserve the already-created enum
	// Decl as an enum QualType for locals, parameters, conversions and later
	// backend consumers.
	asCArray<asASTDeclId> lexicalTypes;
	LookupCandidates(text.AddressOf(), lexicalTypes);
	for( asUINT i = 0; i < lexicalTypes.GetLength(); ++i )
	{
		const asCDecl* decl = context.GetDecl(lexicalTypes[i]);
		if( decl != 0 && decl->kind == asAST_DECL_ENUM )
		{
			const char* stableKey = decl->stableKey.GetLength() != 0
				? decl->stableKey.AddressOf()
				: decl->name.AddressOf();
			return context.InternNamedType(asAST_TYPE_ENUM, stableKey, quals);
		}
		if( decl != 0 && (decl->kind == asAST_DECL_CLASS
			|| decl->kind == asAST_DECL_INTERFACE) )
		{
			// Native member interning may have created only a range-less symbol
			// shell. Route that shell through ResolveQualifiedNominalType below so
			// the copied registered ancestry is projected before the type escapes
			// this action. Authored script declarations remain lexical authority.
			if( decl->origin.Equals("canonical-native-type-view") )
			{
				continue;
			}
			const char* stableKey = decl->stableKey.GetLength() != 0
				? decl->stableKey.AddressOf()
				: decl->name.AddressOf();
			const asEASTTypeKind kind = decl->kind == asAST_DECL_INTERFACE
				|| (decl->traits & asAST_TRAIT_VALUE) == 0
				? asAST_TYPE_REFERENCE_OBJECT
				: asAST_TYPE_VALUE_OBJECT;
			return context.InternNamedType(
				kind,
				stableKey,
				DeclaredEffectiveQualifiers(decl, quals));
		}
	}
	for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
	{
		const asCDecl* decl = context.GetDecl(asASTDeclId(i));
		if( decl != 0 && decl->kind == asAST_DECL_ENUM
			&& decl->stableKey.Equals(text) )
		{
			return context.InternNamedType(
				asAST_TYPE_ENUM, decl->stableKey.AddressOf(), quals);
		}
		if( decl != 0 && (decl->kind == asAST_DECL_CLASS
			|| decl->kind == asAST_DECL_INTERFACE)
			&& decl->stableKey.Equals(text) )
		{
			if( decl->origin.Equals("canonical-native-type-view") )
			{
				continue;
			}
			const asEASTTypeKind kind = decl->kind == asAST_DECL_INTERFACE
				|| (decl->traits & asAST_TRAIT_VALUE) == 0
				? asAST_TYPE_REFERENCE_OBJECT
				: asAST_TYPE_VALUE_OBJECT;
			return context.InternNamedType(
				kind,
				decl->stableKey.AddressOf(),
				DeclaredEffectiveQualifiers(decl, quals));
		}
	}

	const bool absoluteScope = text.GetLength() >= 2
		&& text[0] == ':' && text[1] == ':';
	const asCString canonicalText =
		CanonicalStructuredTypeKey(text);
	bool ambiguous = false;
	const asCQualType registered = ResolveQualifiedNominalType(
		canonicalText, absoluteScope, quals, ambiguous);
	if( ambiguous )
	{
		AddDiagnostic("qualtype-registered-nominal-ambiguous");
		return asCQualType();
	}
	if( registered.IsValid() )
	{
		return registered;
	}

	asEASTTypeKind kind = asAST_TYPE_VALUE_OBJECT;
	if( quals & asAST_QUAL_HANDLE )
	{
		kind = asAST_TYPE_REFERENCE_OBJECT;
	}
	return context.InternNamedType(kind, canonicalText.AddressOf(), quals);
}

asCSourceRange asCSema::QualTypeSyntaxNodeRange(
	const asSQualTypeSyntaxNode& node) const
{
	if( parsedFile == 0 || node.endOffset < node.beginOffset )
	{
		return asCSourceRange();
	}
	return context.GetSourceManager().MakeRange(
		parsedFile, node.beginOffset, node.endOffset);
}

asCQualType asCSema::ActOnQualTypeSyntaxNode(
	const asSQualTypeSyntaxAction& action,
	asUINT nodeIndex,
	asUINT depth)
{
	if( nodeIndex >= action.nodes.GetLength() || depth > 64 )
	{
		AddDiagnostic("qualtype-action-structure-invalid");
		return asCQualType();
	}
	const asSQualTypeSyntaxNode& node = action.nodes[nodeIndex];
	const asCSourceRange range = QualTypeSyntaxNodeRange(node);
	if( node.spelling.GetLength() == 0
		|| node.endOffset < node.beginOffset
		|| node.subtreeSize == 0
		|| nodeIndex + node.subtreeSize > action.nodes.GetLength()
		|| !asASTQualifiersAreValid(node.qualifiers) )
	{
		AddDiagnostic("qualtype-action-invalid", range);
		return asCQualType();
	}

	const eTokenType primitive = CanonicalPrimitiveToken(node.primitiveToken);
	const asUINT subtreeEnd = nodeIndex + node.subtreeSize;
	asUINT childIndex = nodeIndex + 1;

	// Append the direct template arguments owned by one syntax node. Scope
	// subtrees are consumed by the caller before this lambda is entered. Each
	// subtype is resolved through its own structured action, so configured
	// primitive widths and nested qualified/template identities remain exact.
	auto appendTemplateArguments = [this, &action, depth](
		const asSQualTypeSyntaxNode& owner,
		asUINT ownerEnd,
		asUINT& nextChild,
		asCString& spelling,
		const asCSourceRange& ownerRange,
		asCArray<asCQualType>* argumentTypes) -> bool
	{
		if( owner.templateArgumentCount == 0 )
		{
			if( owner.firstTemplateArgument != 0 )
			{
				AddDiagnostic("qualtype-action-structure-invalid", ownerRange);
				return false;
			}
			return true;
		}
		if( owner.firstTemplateArgument != nextChild )
		{
			AddDiagnostic("qualtype-action-structure-invalid", ownerRange);
			return false;
		}

		spelling += "<";
		for( asUINT argumentIndex = 0;
			argumentIndex < owner.templateArgumentCount; ++argumentIndex )
		{
			if( nextChild >= ownerEnd
				|| nextChild >= action.nodes.GetLength() )
			{
				AddDiagnostic("qualtype-action-structure-invalid", ownerRange);
				return false;
			}
			const asSQualTypeSyntaxNode& childNode = action.nodes[nextChild];
			const asCSourceRange childRange = QualTypeSyntaxNodeRange(childNode);
			if( childNode.qualifiers & asAST_QUAL_CONST )
			{
				AddDiagnostic("qualtype-template-subtype-readonly", childRange);
				return false;
			}
			const asDWORD allowedTemplateQualifiers = asAST_QUAL_HANDLE;
			if( childNode.qualifiers & ~allowedTemplateQualifiers )
			{
				AddDiagnostic(
					"qualtype-template-subtype-modifier-invalid", childRange);
				return false;
			}

			const asCQualType childType = ActOnQualTypeSyntaxNode(
				action, nextChild, depth + 1);
			const asCType* const childCanonical =
				context.GetType(childType.type);
			if( !childType.IsValid() || childCanonical == 0
				|| childCanonical->stableKey.GetLength() == 0 )
			{
				return false;
			}
			if( argumentTypes != 0 )
			{
				argumentTypes->PushLast(childType);
			}
			if( argumentIndex != 0 )
			{
				spelling += ",";
			}
			spelling += childCanonical->stableKey;
			// Stable template identity preserves the authored subtype spelling.
			// A registered asOBJ_IMPLICIT_HANDLE subtype carries HANDLE in its
			// effective QualType even when the source omitted '@'; serializing that
			// derived bit produces a key (for example TSubclassOf<UObject@>) that
			// cannot bind the Runtime template instance TSubclassOf<UObject>.
			// Explicit source '@' remains part of the nominal template key.
			if( childNode.qualifiers & asAST_QUAL_HANDLE )
			{
				spelling += "@";
			}
			if( childNode.subtreeSize == 0
				|| nextChild + childNode.subtreeSize > ownerEnd )
			{
				AddDiagnostic("qualtype-action-structure-invalid", childRange);
				return false;
			}
			nextChild += childNode.subtreeSize;
		}
		spelling += ">";
		return true;
	};

	if( node.scopeSegmentCount == 0 )
	{
		if( node.firstScopeSegment != 0 )
		{
			AddDiagnostic("qualtype-action-structure-invalid", range);
			return asCQualType();
		}
	}
	else if( node.firstScopeSegment != childIndex
		|| primitive != ttUnrecognizedToken )
	{
		AddDiagnostic("qualtype-action-structure-invalid", range);
		return asCQualType();
	}

	asCString qualifiedPrefix;
	if( node.absoluteScope )
	{
		qualifiedPrefix = "::";
	}
	const asSCanonicalTemplateDeclarationFact* templateParent = 0;
	asCString templateParentInstanceKey;
	for( asUINT scopeIndex = 0;
		scopeIndex < node.scopeSegmentCount; ++scopeIndex )
	{
		if( childIndex >= subtreeEnd || childIndex >= action.nodes.GetLength() )
		{
			AddDiagnostic("qualtype-action-structure-invalid", range);
			return asCQualType();
		}
		const asUINT segmentIndex = childIndex;
		const asSQualTypeSyntaxNode& segment = action.nodes[segmentIndex];
		const asCSourceRange segmentRange = QualTypeSyntaxNodeRange(segment);
		if( segment.spelling.GetLength() == 0
			|| segment.endOffset < segment.beginOffset
			|| segment.subtreeSize == 0
			|| segmentIndex + segment.subtreeSize > subtreeEnd
			|| segment.absoluteScope
			|| segment.firstScopeSegment != 0
			|| segment.scopeSegmentCount != 0
			|| segment.qualifiers != 0
			|| CanonicalPrimitiveToken(segment.primitiveToken)
				!= ttUnrecognizedToken )
		{
			AddDiagnostic("qualtype-action-structure-invalid", segmentRange);
			return asCQualType();
		}
		if( segment.templateArgumentCount != 0
			&& scopeIndex + 1 != node.scopeSegmentCount )
		{
			AddDiagnostic("qualtype-action-structure-invalid", segmentRange);
			return asCQualType();
		}
		if( (segment.templateArgumentCount == 0
				&& segment.firstTemplateArgument != 0)
			|| (segment.templateArgumentCount != 0
				&& segment.firstTemplateArgument != segmentIndex + 1) )
		{
			AddDiagnostic("qualtype-action-structure-invalid", segmentRange);
			return asCQualType();
		}

		asCString segmentSpelling = segment.spelling;
		const asSCanonicalTemplateDeclarationFact* segmentTemplate = 0;
		if( segment.templateArgumentCount != 0 )
		{
			asCString resolvedBase;
			const asCString authoredBase =
				qualifiedPrefix + segment.spelling;
			segmentTemplate = ResolveCanonicalTemplateDeclaration(
				authoredBase, node.absoluteScope, resolvedBase);
			if( segmentTemplate == 0
				|| segmentTemplate->subtypeCount
					!= segment.templateArgumentCount )
			{
				AddDiagnostic("qualtype-template-instance-invalid", segmentRange);
				return asCQualType();
			}
			segmentSpelling = resolvedBase;
		}
		asUINT segmentChild = segmentIndex + 1;
		const asUINT segmentEnd = segmentIndex + segment.subtreeSize;
		asCArray<asCQualType> segmentArgumentTypes;
		if( !appendTemplateArguments(
			segment,
			segmentEnd,
			segmentChild,
			segmentSpelling,
			segmentRange,
			&segmentArgumentTypes) )
		{
			return asCQualType();
		}
		if( segmentChild != segmentEnd )
		{
			AddDiagnostic("qualtype-action-structure-invalid", segmentRange);
			return asCQualType();
		}

		if( segmentTemplate != 0 )
		{
			for( asUINT argumentIndex = 0;
				argumentIndex < segmentArgumentTypes.GetLength(); ++argumentIndex )
			{
				if( !IsCanonicalTemplateSubtypeAllowed(
					*segmentTemplate, segmentArgumentTypes[argumentIndex]) )
				{
					AddDiagnostic(
						"qualtype-template-instance-invalid", segmentRange);
					return asCQualType();
				}
			}
			templateParent = segmentTemplate;
			templateParentInstanceKey =
				CanonicalStructuredTypeKey(segmentSpelling);
			qualifiedPrefix = templateParentInstanceKey;
		}
		else
		{
			qualifiedPrefix += segmentSpelling;
		}

		qualifiedPrefix += "::";
		childIndex = segmentEnd;
	}

	if( primitive != ttUnrecognizedToken )
	{
		if( node.absoluteScope || node.scopeSegmentCount != 0
			|| node.templateArgumentCount != 0
			|| node.firstTemplateArgument != 0
			|| childIndex != subtreeEnd )
		{
			AddDiagnostic("qualtype-action-structure-invalid", range);
			return asCQualType();
		}
		return context.InternPrimitive(primitive, node.qualifiers);
	}

	const asSCanonicalTemplateDeclarationFact* finalTemplate = 0;
	asCString completeSpelling = qualifiedPrefix + node.spelling;
	if( (node.templateArgumentCount == 0
			&& node.firstTemplateArgument != 0)
		|| (node.templateArgumentCount != 0
			&& node.firstTemplateArgument != childIndex) )
	{
		AddDiagnostic("qualtype-action-structure-invalid", range);
		return asCQualType();
	}
	if( node.templateArgumentCount != 0 )
	{
		asCString resolvedBase;
		finalTemplate = ResolveCanonicalTemplateDeclaration(
			completeSpelling, node.absoluteScope, resolvedBase);
		if( finalTemplate == 0
			|| finalTemplate->subtypeCount != node.templateArgumentCount )
		{
			AddDiagnostic("qualtype-template-instance-invalid", range);
			return asCQualType();
		}
		completeSpelling = resolvedBase;
	}
	asCArray<asCQualType> finalArgumentTypes;
	if( !appendTemplateArguments(
		node,
		subtreeEnd,
		childIndex,
		completeSpelling,
		range,
		&finalArgumentTypes) )
	{
		return asCQualType();
	}
	if( childIndex != subtreeEnd )
	{
		AddDiagnostic("qualtype-action-structure-invalid", range);
		return asCQualType();
	}

	if( node.templateArgumentCount == 0 && templateParent == 0 )
	{
		asCString resolvedBase;
		if( ResolveCanonicalTemplateDeclaration(
			completeSpelling, node.absoluteScope, resolvedBase) != 0 )
		{
			AddDiagnostic("qualtype-template-arity-invalid", range);
			return asCQualType();
		}
	}

	const asCString canonicalSpelling =
		CanonicalStructuredTypeKey(completeSpelling);
	if( finalTemplate != 0 )
	{
		for( asUINT argumentIndex = 0;
			argumentIndex < finalArgumentTypes.GetLength(); ++argumentIndex )
		{
			if( !IsCanonicalTemplateSubtypeAllowed(
				*finalTemplate, finalArgumentTypes[argumentIndex]) )
			{
				AddDiagnostic("qualtype-template-instance-invalid", range);
				return asCQualType();
			}
		}
		asDWORD effectiveQualifiers = node.qualifiers;
		if( (finalTemplate->flags & asOBJ_IMPLICIT_HANDLE) != 0 )
		{
			effectiveQualifiers |= asAST_QUAL_HANDLE;
		}
		return InternCanonicalTemplateInstanceType(
			finalTemplate,
			canonicalSpelling,
			effectiveQualifiers,
			&finalArgumentTypes);
	}

	// A template-bearing innermost scope denotes a child of the exact stable
	// parent instance. The copied host declaration fact proves membership; a
	// live Runtime parent/child object is created only at generation install.
	if( templateParent != 0 )
	{
		if( !CanonicalTemplateHasChildFuncdef(*templateParent, node.spelling) )
		{
			AddDiagnostic("qualtype-parent-child-invalid", range);
			return asCQualType();
		}
		return context.InternNamedType(
			asAST_TYPE_FUNCDEF,
			canonicalSpelling.AddressOf(),
			node.qualifiers);
	}

	// Qualified nominal types must resolve in the Canonical declaration
	// environment before script declarations are published to the Runtime
	// registry. The same search accepts an exact translation-unit snapshot fact,
	// but never observes or publishes a live Runtime type pointer.
	// This covers namespace-qualified records/enums and non-template child
	// funcdefs without depending on Builder's GetNameSpaceFromNode walk.
	if( node.scopeSegmentCount != 0 )
	{
		bool ambiguous = false;
		const asCQualType qualified = ResolveQualifiedNominalType(
			canonicalSpelling,
			node.absoluteScope,
			node.qualifiers,
			ambiguous);
		if( ambiguous )
		{
			AddDiagnostic(
				"qualtype-registered-nominal-ambiguous", range);
			return asCQualType();
		}
		if( qualified.IsValid() )
		{
			return qualified;
		}
	}
	return ActOnQualType(canonicalSpelling.AddressOf(), node.qualifiers);
}

asCQualType asCSema::ActOnQualTypeAction(
	const asSQualTypeSyntaxAction& action,
	asCScriptCode* script)
{
	if( action.nodes.GetLength() == 0 )
	{
		AddDiagnostic("qualtype-action-invalid");
		return asCQualType();
	}
	if( script != 0 && !PrepareParsedSource(script) )
	{
		AddDiagnostic("qualtype-action-source-invalid");
		return asCQualType();
	}
	if( action.nodes[0].subtreeSize != action.nodes.GetLength() )
	{
		AddDiagnostic(
			"qualtype-action-structure-invalid",
			QualTypeSyntaxNodeRange(action.nodes[0]));
		return asCQualType();
	}
	return ActOnQualTypeSyntaxNode(action, 0, 0);
}

asASTDeclId asCSema::ActOnParameterDeclAction(
	const asSParameterDeclAction& action,
	asCScriptCode* script)
{
	lastActedDecl = asASTDeclId();
	if( !action.callable.IsValid()
		|| (!action.typeOmitted && !action.type.IsValid())
		|| (action.typeOmitted && action.type.IsValid())
		|| action.endOffset <= action.beginOffset
		|| !PrepareParsedSource(script) )
	{
		AddDiagnostic("parameter-decl-action-invalid");
		return asASTDeclId();
	}

	const asCDecl* const owner = context.GetDecl(action.callable);
	if( owner == 0
		|| (owner->kind != asAST_DECL_FUNCTION
			&& owner->kind != asAST_DECL_METHOD
			&& owner->kind != asAST_DECL_CONSTRUCTOR
			&& owner->kind != asAST_DECL_DESTRUCTOR
			&& owner->kind != asAST_DECL_MIXIN
			&& owner->kind != asAST_DECL_IMPORT
			&& owner->kind != asAST_DECL_FUNCDEF) )
	{
		AddDiagnostic("parameter-decl-owner-invalid");
		return asASTDeclId();
	}
	const bool lambdaOwner = owner != 0
		&& owner->kind == asAST_DECL_FUNCTION
		&& (owner->traits & asAST_TRAIT_LAMBDA) != 0;
	if( action.typeOmitted && !lambdaOwner )
	{
		AddDiagnostic("parameter-decl-omitted-type-owner-invalid");
		return asASTDeclId();
	}
	if( !action.typeOmitted && context.GetType(action.type.type) == 0 )
	{
		AddDiagnostic("parameter-decl-type-invalid");
		return asASTDeclId();
	}

	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("parameter-decl-source-range-invalid");
		return asASTDeclId();
	}
	const asASTDeclId parameter = ActOnStartParamDecl(
		action.callable,
		action.name.GetLength() ? action.name.AddressOf() : "",
		action.type,
		range);
	if( !parameter.IsValid() )
	{
		AddDiagnostic("parameter-decl-action-invalid", range);
		return asASTDeclId();
	}
	if( action.typeOmitted )
	{
		bool recorded = false;
		for( asUINT factIndex = 0;
			factIndex < lambdaSyntaxFacts.GetLength(); ++factIndex )
		{
			asSLambdaSyntaxFact& fact = lambdaSyntaxFacts[factIndex];
			if( fact.lambda != action.callable )
			{
				continue;
			}
			recorded = true;
			bool present = false;
			for( asUINT omittedIndex = 0;
				omittedIndex < fact.omittedParameters.GetLength();
				++omittedIndex )
			{
				if( fact.omittedParameters[omittedIndex] == parameter )
				{
					present = true;
					break;
				}
			}
			if( !present )
			{
				fact.omittedParameters.PushLast(parameter);
			}
			break;
		}
		if( !recorded )
		{
			AddDiagnostic("lambda-omitted-formal-state-invalid", range);
			return asASTDeclId();
		}
	}
	else if( const asCType* const named = context.GetType(action.type.type) )
	{
		if( named->kind != asAST_TYPE_PRIMITIVE
			&& named->kind != asAST_TYPE_VOID
			&& named->stableKey.GetLength() != 0 )
		{
			RecordDependency(action.callable, named->stableKey.AddressOf());
		}
	}
	return parameter;
}

asASTDeclId asCSema::ActOnLambdaHeaderAction(
	const asSLambdaHeaderAction& action,
	asCScriptCode* script)
{
	lastActedDecl = asASTDeclId();
	if( !action.owner.IsValid()
		|| !action.recoveryReturnType.IsValid()
		|| action.explicitlyTypedParameterCount > action.parameterCount
		|| action.endOffset <= action.beginOffset
		|| context.GetDecl(action.owner) == 0
		|| context.GetType(action.recoveryReturnType.type) == 0
		|| !PrepareParsedSource(script) )
	{
		AddDiagnostic("lambda-header-action-invalid");
		return asASTDeclId();
	}

	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("lambda-header-source-range-invalid");
		return asASTDeclId();
	}

	const asASTDeclId lambda = ActOnStartFunctionDecl(
		action.owner, "<lambda>", action.recoveryReturnType, range);
	if( !lambda.IsValid() )
	{
		AddDiagnostic("lambda-header-action-invalid", range);
		return asASTDeclId();
	}
	context.AddDeclTrait(lambda, asAST_TRAIT_LAMBDA);
	context.SetDeclName(lambda, "<lambda>");
	bool hasSyntaxFact = false;
	for( asUINT i = 0; i < lambdaSyntaxFacts.GetLength(); ++i )
	{
		if( lambdaSyntaxFacts[i].lambda != lambda )
		{
			continue;
		}
		hasSyntaxFact = true;
		if( lambdaSyntaxFacts[i].parameterCount != action.parameterCount
			|| lambdaSyntaxFacts[i].explicitlyTypedParameterCount
				!= action.explicitlyTypedParameterCount )
		{
			AddDiagnostic("lambda-header-action-conflict", range);
		}
		break;
	}
	if( !hasSyntaxFact )
	{
		asSLambdaSyntaxFact fact;
		fact.lambda = lambda;
		fact.parameterCount = action.parameterCount;
		fact.explicitlyTypedParameterCount =
			action.explicitlyTypedParameterCount;
		lambdaSyntaxFacts.PushLast(fact);
	}
	FinishDecl(lambda);
	NoteActedDecl(lambda);
	return lambda;
}

asASTExprId asCSema::ActOnLambdaExpressionAction(
	const asSLambdaExpressionAction& action,
	asCScriptCode* script)
{
	const asCDecl* const declaration = context.GetDecl(action.lambdaDecl);
	if( !action.lambdaDecl.IsValid()
		|| action.endOffset <= action.beginOffset
		|| declaration == 0
		|| declaration->kind != asAST_DECL_FUNCTION
		|| (declaration->traits & asAST_TRAIT_LAMBDA) == 0
		|| !PrepareParsedSource(script) )
	{
		AddDiagnostic("lambda-expression-action-invalid");
		return asASTExprId();
	}

	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("lambda-expression-source-range-invalid");
		return asASTExprId();
	}
	return ActOnDeclRef(action.lambdaDecl, declaration->type, range);
}

bool asCSema::ActOnParameterDefaultAction(
	const asSParameterDefaultAction& action,
	asCScriptCode* script)
{
	if( !action.parameter.IsValid()
		|| !action.expression.IsValid()
		|| action.authoredText.GetLength() == 0
		|| action.endOffset <= action.beginOffset
		|| action.recovered
		|| !PrepareParsedSource(script) )
	{
		AddDiagnostic(action.recovered
			? "parameter-default-action-recovered"
			: "parameter-default-action-invalid");
		return false;
	}
	const asCDecl* const parameter = context.GetDecl(action.parameter);
	if( parameter == 0 || parameter->kind != asAST_DECL_PARAM )
	{
		AddDiagnostic("parameter-default-owner-invalid");
		return false;
	}
	if( context.GetExpr(action.expression) == 0 )
	{
		AddDiagnostic("parameter-default-expression-invalid");
		return false;
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid()
		|| script == 0 || script->code == 0
		|| action.endOffset > script->codeLength )
	{
		AddDiagnostic("parameter-default-source-range-invalid", range);
		return false;
	}
	asCString sourceText;
	sourceText.Assign(
		script->code + action.beginOffset,
		action.endOffset - action.beginOffset);
	if( !sourceText.Equals(action.authoredText) )
	{
		AddDiagnostic("parameter-default-source-text-mismatch", range);
		return false;
	}
	if( parameter->inits.GetLength() != 0 )
	{
		if( parameter->inits.GetLength() == 1
			&& parameter->inits[0] == action.expression
			&& parameter->defaultArg.Equals(action.authoredText) )
		{
			return true;
		}
		AddDiagnostic("parameter-default-already-bound", range);
		return false;
	}
	context.SetDefaultArg(action.parameter, action.authoredText.AddressOf());
	context.AddDeclInit(action.parameter, action.expression);
	return true;
}

asASTDeclId asCSema::ActOnEnumeratorName(
	asASTDeclId enumDecl,
	const asSEnumDeclActionName& action,
	asCScriptCode* script)
{
	if( !enumDecl.IsValid()
		|| action.name.GetLength() == 0
		|| action.endOffset <= action.beginOffset
		|| !PrepareParsedSource(script) )
	{
		AddDiagnostic("enumerator-action-invalid");
		return asASTDeclId();
	}
	const asCDecl* owner = context.GetDecl(enumDecl);
	if( owner == 0 || owner->kind != asAST_DECL_ENUM )
	{
		AddDiagnostic("enumerator-owner-invalid");
		return asASTDeclId();
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("enumerator-source-range-invalid");
		return asASTDeclId();
	}
	return ActOnStartEnumeratorDecl(enumDecl, action.name.AddressOf(), range);
}

bool asCSema::ActOnEnumeratorInitializerAction(
	const asSEnumeratorInitializerAction& action,
	asCScriptCode* script)
{
	if( !action.enumerator.IsValid()
		|| !action.expression.IsValid()
		|| action.endOffset <= action.beginOffset
		|| !PrepareParsedSource(script) )
	{
		AddDiagnostic("enumerator-initializer-action-invalid");
		return false;
	}
	const asCDecl* const decl = context.GetDecl(action.enumerator);
	const asCDecl* const owner = decl != 0
		? context.GetDecl(decl->parent) : 0;
	if( decl == 0 || decl->kind != asAST_DECL_VAR
		|| owner == 0 || owner->kind != asAST_DECL_ENUM
		|| context.GetExpr(action.expression) == 0 )
	{
		AddDiagnostic("enumerator-initializer-owner-invalid");
		return false;
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("enumerator-initializer-source-range-invalid");
		return false;
	}
	if( decl->inits.GetLength() != 0 )
	{
		if( decl->inits.GetLength() == 1
			&& decl->inits[0] == action.expression )
		{
			return true;
		}
		AddDiagnostic("enumerator-initializer-already-bound", range);
		return false;
	}

	ActOnFinishEnumeratorDecl(action.enumerator, action.expression);
	const asCDecl* const finished = context.GetDecl(action.enumerator);
	return finished != 0
		&& finished->inits.GetLength() == 1
		&& finished->inits[0] == action.expression;
}

static bool ConstructorAlreadyInitializesMember(
	const asCASTContext& context,
	const asCDecl* constructor,
	asASTDeclId member)
{
	if( constructor == 0 || !member.IsValid() )
	{
		return false;
	}
	for( asUINT initIndex = 0; initIndex < constructor->inits.GetLength(); ++initIndex )
	{
		const asCExpr* const init = context.GetExpr(constructor->inits[initIndex]);
		if( init == 0 || init->kind != asAST_EXPR_ASSIGN
			|| init->children.GetLength() < 1 )
		{
			continue;
		}
		const asCExpr* const lhs = context.GetExpr(init->children[0]);
		if( lhs && lhs->resolvedDecl == member )
		{
			return true;
		}
	}
	return false;
}

static asASTExprId CreateDefaultMemberInit(asCSema* sema, const asCDecl* member)
{
	if( sema == 0 || member == 0 || !member->type.IsValid() )
	{
		return asASTExprId();
	}
	const asCType* const type = sema->GetContext().GetType(member->type.type);
	if( type == 0 )
	{
		return asASTExprId();
	}
	// Reference objects occupy pointer slots even when the source spelling uses
	// this fork's implicit-handle form (for example a registered asOBJ_REF field
	// written as `CObj Value`).  They have no inline value constructor to bind.
	// Seal their generated default as null instead of manufacturing a Construct
	// expression with an invalid resolvedDecl.
	if( member->type.IsHandle()
		|| type->kind == asAST_TYPE_REFERENCE_OBJECT
		|| type->kind == asAST_TYPE_FUNCDEF )
	{
		return sema->ActOnNullLiteral(member->range);
	}
	if( type->kind == asAST_TYPE_VALUE_OBJECT
		|| type->kind == asAST_TYPE_TEMPLATE )
	{
		asCArray<asASTExprId> arguments;
		return sema->ActOnConstruct(member->type, arguments, member->range);
	}
	if( type->kind == asAST_TYPE_PRIMITIVE && type->primitiveToken == ttBool )
	{
		return sema->ActOnBoolLiteral(false, member->range);
	}
	if( type->kind == asAST_TYPE_PRIMITIVE
		&& (type->primitiveToken == ttFloat
			|| type->primitiveToken == ttFloat32) )
	{
		return sema->ActOnFloatLiteral("0", member->range);
	}
	if( type->kind == asAST_TYPE_PRIMITIVE
		&& (type->primitiveToken == ttDouble
			|| type->primitiveToken == ttFloat64) )
	{
		return sema->ActOnDoubleLiteral("0", member->range);
	}
	if( type->kind == asAST_TYPE_PRIMITIVE || type->kind == asAST_TYPE_ENUM )
	{
		return sema->ActOnIntegerLiteral(0, member->range);
	}
	return asASTExprId();
}

static const asCExpr* UnwrapConstructorMemberInit(
	const asCASTContext& context,
	const asCExpr* expression)
{
	while( expression
		&& (expression->kind == asAST_EXPR_MATERIALIZE_TEMPORARY
			|| expression->kind == asAST_EXPR_CLEANUP)
		&& expression->children.GetLength() == 1 )
	{
		expression = context.GetExpr(expression->children[0]);
	}
	return expression;
}

static void AttachConstructorMemberInits(asCSema* sema, asASTDeclId cls)
{
	const asCDecl* classDecl = sema->GetContext().GetDecl(cls);
	if( classDecl == 0 )
	{
		return;
	}
	for( asUINT c = 0; c < classDecl->children.GetLength(); ++c )
	{
		const asCDecl* ctorDecl = sema->GetContext().GetDecl(classDecl->children[c]);
		if( ctorDecl == 0 || ctorDecl->kind != asAST_DECL_CONSTRUCTOR )
		{
			continue;
		}
		for( asUINT i = 0; i < classDecl->children.GetLength(); ++i )
		{
			const asCDecl* member = sema->GetContext().GetDecl(classDecl->children[i]);
			if( member == 0 || member->kind != asAST_DECL_VAR
				|| ConstructorAlreadyInitializesMember(
					sema->GetContext(), ctorDecl, member->id) )
			{
				continue;
			}
			asASTExprId rhs = member->inits.GetLength() != 0
				? member->inits[0] : CreateDefaultMemberInit(sema, member);
			if( !rhs.IsValid() )
			{
				continue;
			}

			// An authored initializer for a non-trivial VALUE member initializes
			// previously-unconstructed storage. Seal an exact constructor plan here;
			// an ordinary assignment would invoke opAssign (or a scalar write) before
			// the member's lifetime begins. Already-authored Construct plans, including
			// generated zero-initializers, remain unchanged.
			const asCExpr* const unwrapped = UnwrapConstructorMemberInit(
				sema->GetContext(), sema->GetContext().GetExpr(rhs));
			if( sema->RequiresExactValueObjectPlan(member->type)
				&& (unwrapped == 0
					|| unwrapped->kind != asAST_EXPR_CONSTRUCT
					|| unwrapped->type != member->type) )
			{
				asCArray<asASTExprId> arguments;
				arguments.PushLast(rhs);
				rhs = sema->ActOnConstruct(
					member->type, arguments, member->range);
			}
			const asASTExprId ref = sema->ActOnDeclRef(
				member->id, member->type, member->range);
			const asASTExprId assign = sema->ActOnAssign(
				ref, rhs, member->type, member->range);
			sema->GetContext().AddDeclInit(ctorDecl->id, assign);
		}
	}
}

static bool IsExactImplicitCopyParameter(
	const asCASTContext& context,
	const asCDecl* callable,
	const asCQualType& recordType)
{
	if( callable == 0 || !recordType.IsValid()
		|| context.GetFormalDeclCount(callable->id) != 1 )
	{
		return false;
	}
	const asCDecl* const parameter =
		context.GetFormalDecl(callable->id, 0);
	if( parameter == 0 || parameter->type.type != recordType.type
		|| !parameter->type.IsConst() || !parameter->type.IsReference() )
	{
		return false;
	}
	const asDWORD direction =
		parameter->type.quals & asAST_QUAL_PARAM_DIR_MASK;
	return direction == 0 || (direction & asAST_QUAL_IN) != 0;
}

static void EnsureGeneratedLifecycle(asCSema* sema, asASTDeclId cls)
{
	const asCDecl* decl = sema->GetContext().GetDecl(cls);
	if( decl == 0 || (decl->kind != asAST_DECL_CLASS && decl->kind != asAST_DECL_INTERFACE) )
	{
		return;
	}

	bool hasCtor = false;
	bool hasZeroArgumentCtor = false;
	bool hasDtor = false;
	bool hasCopyConstructor = false;
	bool hasAssignmentOperator = false;
	asASTDeclId generatedCtor;
	for( asUINT i = 0; i < decl->children.GetLength(); ++i )
	{
		const asCDecl* child = sema->GetContext().GetDecl(decl->children[i]);
		if( child == 0 )
		{
			continue;
		}
		if( child->kind == asAST_DECL_CONSTRUCTOR )
		{
			hasCtor = true;
			asUINT parameterCount = 0;
			for( asUINT parameterIndex = 0;
				parameterIndex < child->children.GetLength(); ++parameterIndex )
			{
				const asCDecl* const parameter = sema->GetContext().GetDecl(
					child->children[parameterIndex]);
				if( parameter && parameter->kind == asAST_DECL_PARAM )
				{
					++parameterCount;
				}
			}
			if( parameterCount == 0 )
			{
				hasZeroArgumentCtor = true;
			}
			if( IsExactImplicitCopyParameter(
				sema->GetContext(), child, decl->type) )
			{
				hasCopyConstructor = true;
			}
			if( (child->traits & asAST_TRAIT_GENERATED) != 0 )
			{
				generatedCtor = child->id;
			}
		}
		if( child->kind == asAST_DECL_METHOD
			&& child->name.Equals("opAssign")
			&& IsExactImplicitCopyParameter(
				sema->GetContext(), child, decl->type) )
		{
			hasAssignmentOperator = true;
		}
		if( child->kind == asAST_DECL_DESTRUCTOR )
		{
			hasDtor = true;
		}
	}

	// The maintained UE engine profile enables ALWAYS_IMPL_DEFAULT_CONSTRUCT.
	// Builder therefore retains an implicit zero-argument Runtime shell even
	// when the class authors only non-zero-argument overloads. Sema must expose
	// the same policy as an explicit generated declaration; otherwise the
	// prepared backend has no stable semantic identity for that live shell.
	const asCScriptEngine* const engine = sema->GetEngine();
	const bool requiresGeneratedDefault = !hasCtor
		|| (engine != 0 && engine->ep.alwaysImplDefaultConstruct != 0);
	if( requiresGeneratedDefault && !hasZeroArgumentCtor )
	{
		generatedCtor = sema->ActOnConstructorDecl(cls, decl->name.AddressOf(), decl->range);
		sema->GetContext().AddDeclTrait(generatedCtor, asAST_TRAIT_GENERATED);
		sema->GetContext().SetOrigin(
			generatedCtor, "canonical-generated-default-constructor");
	}

	// AngelScript's implicit script-object copy construction is semantically
	// "default construct, then default opAssign". Preserve that contract as
	// stable script-owned declarations instead of asking CodeGen to rediscover a
	// Runtime behaviour id. Explicit authored special members always win.
	if( (decl->traits & asAST_TRAIT_VALUE) != 0 )
	{
		const asCString recordName = decl->name;
		const asCSourceRange recordRange = decl->range;
		const asCQualType recordType = decl->type;
		const asCQualType constRecordReference(
			recordType.type,
			(recordType.quals & ~asAST_QUAL_PARAM_DIR_MASK)
				| asAST_QUAL_CONST | asAST_QUAL_REFERENCE);
		const asCQualType recordReference(
			recordType.type,
			(recordType.quals & ~asAST_QUAL_PARAM_DIR_MASK)
				| asAST_QUAL_REFERENCE);

		if( !hasAssignmentOperator )
		{
			const asASTDeclId assignment = sema->ActOnMethodDecl(
				cls, "opAssign", recordReference, recordRange);
			sema->GetContext().AddDeclTrait(
				assignment, asAST_TRAIT_GENERATED);
			sema->GetContext().SetOrigin(
				assignment,
				"canonical-generated-implicit-assignment-operator");
			sema->ActOnParamDecl(
				assignment, "Other", constRecordReference, recordRange);
		}
		if( !hasCopyConstructor )
		{
			const asASTDeclId copyConstructor =
				sema->ActOnConstructorDecl(
					cls, recordName.AddressOf(), recordRange);
			sema->GetContext().AddDeclTrait(
				copyConstructor, asAST_TRAIT_GENERATED);
			sema->GetContext().SetOrigin(
				copyConstructor,
				"canonical-generated-implicit-copy-constructor");
			sema->ActOnParamDecl(
				copyConstructor, "Other",
				constRecordReference, recordRange);
		}
	}
	AttachConstructorMemberInits(sema, cls);
	if( !hasDtor )
	{
		const asASTDeclId dtor = sema->ActOnDestructorDecl(cls, decl->name.AddressOf(), decl->range);
		sema->GetContext().AddDeclTrait(dtor, asAST_TRAIT_GENERATED);
	}
}

static bool FinalizeGeneratedImplicitAssignmentBody(
	asCSema* sema,
	asASTDeclId classId,
	asASTDeclId assignmentId,
	asASTDeclId parameterId,
	const asCArray<asASTDeclId>& fields)
{
	asCASTContext& context = sema->GetContext();
	const asCDecl* const existing = context.GetDecl(assignmentId);
	const asCDecl* const classDecl = context.GetDecl(classId);
	if( existing == 0 || classDecl == 0 || existing->body.IsValid()
		|| !parameterId.IsValid() || !classDecl->type.IsValid() )
	{
		return existing != 0 && existing->body.IsValid();
	}

	const asCQualType classType = classDecl->type;
	const asCSourceRange classRange = classDecl->range;
	asCArray<asASTStmtId> statements;
	bool canGenerate = true;
	for( asUINT fieldIndex = 0; fieldIndex < fields.GetLength(); ++fieldIndex )
	{
		const asCDecl* const field = context.GetDecl(fields[fieldIndex]);
		if( field == 0 || field->kind != asAST_DECL_VAR
			|| !field->type.IsValid() )
		{
			continue;
		}

		// Native-method projection can grow the declaration table. Copy every
		// field fact before resolving its assignment so no retained Decl pointer
		// crosses that mutation boundary.
		const asASTDeclId fieldId = field->id;
		const asCString fieldName = field->name;
		const asCQualType fieldType = field->type;
		const asCSourceRange fieldRange = field->range;
		const asASTExprId targetBase = sema->ActOnThisExpr(
			classType, fieldRange);
		const asCDecl* const parameter = context.GetDecl(parameterId);
		const asASTExprId sourceBase = parameter != 0
			? sema->ActOnDeclRef(
				parameterId, parameter->type, fieldRange)
			: asASTExprId();
		const asASTExprId target = sema->ActOnMemberRef(
			targetBase, fieldName.AddressOf(), fieldType, fieldRange);
		const asASTExprId source = sema->ActOnMemberRef(
			sourceBase, fieldName.AddressOf(), fieldType, fieldRange);
		context.SetResolvedDecl(target, fieldId);
		context.SetResolvedDecl(source, fieldId);
		const asASTExprId assignment = sema->ActOnGeneratedAssign(
			target, source, fieldType, fieldRange);
		const asCExpr* const assignmentExpr = context.GetExpr(assignment);
		if( sema->RequiresExactValueObjectPlan(fieldType)
			&& (assignmentExpr == 0
				|| !assignmentExpr->resolvedDecl.IsValid()) )
		{
			// An implicit script-value assignment operator is conditionally
			// available. A member that has no exact opAssign makes this generated
			// special member unavailable, but must not reject a module that never
			// uses the owner assignment (for example an unrelated generated getter
			// that only needs the member's copy constructor). Leave the generated
			// declaration bodyless so overload use cannot be published by CodeGen;
			// never replace the missing operation with a raw storage copy.
			canGenerate = false;
		}
		statements.PushLast(sema->ActOnExprStmt(
			assignmentId, assignment, fieldRange));
	}
	if( !canGenerate )
	{
		return false;
	}

	const asASTExprId returnValue = sema->ActOnThisExpr(
		classType, classRange);
	statements.PushLast(sema->ActOnReturnStmt(
		assignmentId, returnValue, classRange));
	context.SetBody(
		assignmentId,
		sema->ActOnBlock(assignmentId, statements, classRange));
	return true;
}

static void FinalizeGeneratedImplicitCopyConstructorBody(
	asCSema* sema,
	asASTDeclId classId,
	asASTDeclId copyConstructorId,
	asASTDeclId copyParameterId,
	asASTDeclId assignmentId)
{
	asCASTContext& context = sema->GetContext();
	const asCDecl* const copyConstructor =
		context.GetDecl(copyConstructorId);
	const asCDecl* const classDecl = context.GetDecl(classId);
	const asCDecl* const parameter = context.GetDecl(copyParameterId);
	if( copyConstructor == 0 || classDecl == 0 || parameter == 0
		|| copyConstructor->body.IsValid()
		|| !classDecl->type.IsValid() )
	{
		return;
	}

	const asCQualType classType = classDecl->type;
	const asCSourceRange range = classDecl->range;
	const asASTExprId target = sema->ActOnThisExpr(classType, range);
	const asASTExprId source = sema->ActOnDeclRef(
		copyParameterId, parameter->type, range);
	const asASTExprId assignment = sema->ActOnGeneratedAssign(
		target, source, classType, range);
	const asCExpr* const assignmentExpr = context.GetExpr(assignment);
	if( assignmentExpr == 0 || assignmentExpr->kind != asAST_EXPR_ASSIGN
		|| assignmentExpr->resolvedDecl != assignmentId )
	{
		sema->AddDiagnostic(
			"implicit-copy-assignment-operator-unavailable", range);
	}
	asCArray<asASTStmtId> statements;
	statements.PushLast(sema->ActOnExprStmt(
		copyConstructorId, assignment, range));
	context.SetBody(
		copyConstructorId,
		sema->ActOnBlock(copyConstructorId, statements, range));
}

static void FinalizeGeneratedImplicitCopyLifecycle(asCSema* sema)
{
	asCASTContext& context = sema->GetContext();
	const asUINT declarationCount = context.GetDeclCount();
	for( asUINT declarationIndex = 1;
		declarationIndex <= declarationCount; ++declarationIndex )
	{
		const asCDecl* const candidate = context.GetDecl(
			asASTDeclId(declarationIndex));
		if( candidate == 0 || candidate->kind != asAST_DECL_CLASS
			|| (candidate->traits & asAST_TRAIT_VALUE) == 0
			|| candidate->origin.Equals("canonical-native-type-view") )
		{
			continue;
		}

		const asASTDeclId classId = candidate->id;
		const asCArray<asASTDeclId> children = candidate->children;
		asCArray<asASTDeclId> fields;
		asASTDeclId generatedAssignment;
		asASTDeclId generatedAssignmentParameter;
		asASTDeclId effectiveAssignment;
		asASTDeclId generatedCopyConstructor;
		asASTDeclId generatedCopyParameter;
		for( asUINT childIndex = 0;
			childIndex < children.GetLength(); ++childIndex )
		{
			const asCDecl* const child = context.GetDecl(children[childIndex]);
			if( child == 0 )
			{
				continue;
			}
			if( child->kind == asAST_DECL_VAR )
			{
				fields.PushLast(child->id);
				continue;
			}
			if( child->kind == asAST_DECL_METHOD
				&& child->name.Equals("opAssign")
				&& IsExactImplicitCopyParameter(
					context, child, candidate->type) )
			{
				// Explicit authored semantics win. There is at most one exact
				// signature, but retain the first in source/declaration order.
				if( !effectiveAssignment.IsValid()
					|| (context.GetDecl(effectiveAssignment)->traits
						& asAST_TRAIT_GENERATED) != 0 )
				{
					effectiveAssignment = child->id;
				}
				if( child->origin.Equals(
					"canonical-generated-implicit-assignment-operator") )
				{
					generatedAssignment = child->id;
					const asCDecl* const parameter =
						context.GetFormalDecl(child->id, 0);
					generatedAssignmentParameter = parameter != 0
						? parameter->id : asASTDeclId();
				}
			}
			if( child->kind == asAST_DECL_CONSTRUCTOR
				&& child->origin.Equals(
					"canonical-generated-implicit-copy-constructor") )
			{
				generatedCopyConstructor = child->id;
				const asCDecl* const parameter =
					context.GetFormalDecl(child->id, 0);
				generatedCopyParameter = parameter != 0
					? parameter->id : asASTDeclId();
			}
		}

		bool generatedAssignmentAvailable = false;
		if( generatedAssignment.IsValid() )
		{
			generatedAssignmentAvailable = FinalizeGeneratedImplicitAssignmentBody(
				sema, classId, generatedAssignment,
				generatedAssignmentParameter, fields);
		}
		if( generatedCopyConstructor.IsValid()
			&& effectiveAssignment.IsValid()
			&& (effectiveAssignment != generatedAssignment
				|| generatedAssignmentAvailable) )
		{
			FinalizeGeneratedImplicitCopyConstructorBody(
				sema, classId, generatedCopyConstructor,
				generatedCopyParameter, effectiveAssignment);
		}
	}
}

static bool ClassHasNamedMethod(asCSema* sema, const asCDecl* cls, const char* name)
{
	if( cls == 0 || name == 0 )
	{
		return false;
	}
	for( asUINT i = 0; i < cls->children.GetLength(); ++i )
	{
		const asCDecl* child = sema->GetContext().GetDecl(cls->children[i]);
		if( child && child->name.Equals(name)
			&& (child->kind == asAST_DECL_METHOD || child->kind == asAST_DECL_FUNCTION) )
		{
			return true;
		}
	}
	return false;
}

static asCString StripCanonicalHandleMarksLocal(const asCString& key)
{
	asCString stripped;
	stripped.Assign(key.AddressOf(), key.GetLength());
	asUINT write = 0;
	for( asUINT read = 0; read < stripped.GetLength(); ++read )
	{
		if( stripped[read] == '@' )
		{
			continue;
		}
		stripped[write++] = stripped[read];
	}
	stripped.SetLength(write);
	return stripped;
}

static const asCExpr* UnwrapGeneratedAccessorPlan(
	const asCASTContext& context, const asCExpr* expr)
{
	while( expr != 0
		&& (expr->kind == asAST_EXPR_CLEANUP
			|| expr->kind == asAST_EXPR_MATERIALIZE_TEMPORARY)
		&& expr->children.GetLength() == 1 )
	{
		expr = context.GetExpr(expr->children[0]);
	}
	return expr;
}

static const asCDecl* FindGeneratedAccessorValueClass(
	const asCASTContext& context,
	const asCQualType& valueType)
{
	const asCType* const canonicalType = context.GetType(valueType.type);
	if( canonicalType == 0
		|| (canonicalType->kind != asAST_TYPE_VALUE_OBJECT
			&& canonicalType->kind != asAST_TYPE_TEMPLATE)
		|| canonicalType->stableKey.GetLength() == 0 )
	{
		return 0;
	}
	for( asUINT declIndex = 1;
		declIndex <= context.GetDeclCount(); ++declIndex )
	{
		const asCDecl* const classDecl =
			context.GetDecl(asASTDeclId(declIndex));
		if( classDecl != 0 && classDecl->kind == asAST_DECL_CLASS
			&& (classDecl->stableKey.Equals(canonicalType->stableKey)
				|| (classDecl->stableKey.GetLength() != 0
					&& canonicalType->stableKey.GetLength() != 0
					&& StripCanonicalHandleMarksLocal(classDecl->stableKey).Equals(
						StripCanonicalHandleMarksLocal(canonicalType->stableKey)))) )
		{
			return classDecl;
		}
	}
	return 0;
}

static void AttachGeneratedGetterCopyPlan(
	asCSema* sema, asASTDeclId getter, const asCDecl* field)
{
	if( sema == 0 || field == 0 || !getter.IsValid()
		|| sema->GetEngine() == 0 )
	{
		return;
	}

	// A generated getter for a non-trivial VALUE object is not a storage read.
	// It constructs a detached return value from the live backing field. Record
	// that exact copy-construction route in the sealed declaration so CodeGen
	// never guesses a constructor from Runtime type metadata or widens a scalar
	// read into an unsafe memcpy.
	sema->InternNativeCallablesForType(field->type);
	const asCDecl* const valueClass = FindGeneratedAccessorValueClass(
		sema->GetContext(), field->type);
	asASTDeclId selectedCopy;
	if( valueClass != 0 )
	{
		for( asUINT childIndex = 0;
			childIndex < valueClass->children.GetLength(); ++childIndex )
		{
			const asCDecl* const child = sema->GetContext().GetDecl(
				valueClass->children[childIndex]);
			if( child == 0 || child->kind != asAST_DECL_CONSTRUCTOR
				|| sema->GetContext().GetFormalDeclCount(child->id) != 1 )
			{
				continue;
			}
			const asCDecl* const parameter =
				sema->GetContext().GetFormalDecl(child->id, 0);
			const asCType* const paramType = parameter != 0
				? sema->GetContext().GetType(parameter->type.type) : 0;
			const asCType* const fieldNamed =
				sema->GetContext().GetType(field->type.type);
			if( parameter == 0 || paramType == 0 || fieldNamed == 0
				|| !parameter->type.IsConst()
				|| !parameter->type.IsReference() )
			{
				continue;
			}
			if( parameter->type.type == field->type.type
				|| paramType->stableKey.Equals(fieldNamed->stableKey)
				|| (paramType->stableKey.GetLength() != 0
					&& fieldNamed->stableKey.GetLength() != 0
					&& StripCanonicalHandleMarksLocal(paramType->stableKey).Equals(
						StripCanonicalHandleMarksLocal(fieldNamed->stableKey))) )
			{
				selectedCopy = child->id;
				break;
			}
		}
	}
	if( !selectedCopy.IsValid() )
	{
		return;
	}

	const asASTExprId source = sema->ActOnDeclRef(
		field->id, field->type, field->range);
	asCArray<asASTExprId> arguments;
	arguments.PushLast(source);
	const asASTExprId plan = sema->ActOnConstruct(
		field->type, arguments, field->range, selectedCopy);
	const asCExpr* const construct = UnwrapGeneratedAccessorPlan(
		sema->GetContext(), sema->GetContext().GetExpr(plan));
	const asCDecl* const constructor = construct != 0
		? sema->GetContext().GetDecl(construct->resolvedDecl) : 0;
	const asCDecl* const copyParameter = constructor != 0
		? sema->GetContext().GetFormalDecl(constructor->id, 0) : 0;
	const asCType* const copyType = copyParameter != 0
		? sema->GetContext().GetType(copyParameter->type.type) : 0;
	const asCType* const fieldNamed = sema->GetContext().GetType(field->type.type);
	const bool sameValueType = copyType != 0 && fieldNamed != 0
		&& (copyParameter->type.type == field->type.type
			|| copyType->stableKey.Equals(fieldNamed->stableKey)
			|| (copyType->stableKey.GetLength() != 0
				&& fieldNamed->stableKey.GetLength() != 0
				&& StripCanonicalHandleMarksLocal(copyType->stableKey).Equals(
					StripCanonicalHandleMarksLocal(fieldNamed->stableKey))));
	if( construct == 0 || construct->kind != asAST_EXPR_CONSTRUCT
		|| constructor == 0
		|| constructor->kind != asAST_DECL_CONSTRUCTOR
		|| valueClass == 0 || constructor->parent != valueClass->id
		|| sema->GetContext().GetFormalDeclCount(constructor->id) != 1
		|| copyParameter == 0
		|| !sameValueType
		|| !copyParameter->type.IsConst()
		|| !copyParameter->type.IsReference()
		|| construct->children.GetLength() != 1
		|| construct->children[0] != source )
	{
		// Host value types such as TMap register opAssign but no copy
		// constructor. A generated by-value GetX cannot be sealed; leave the
		// accessor declaration-only so field MEMBER_REF remains the authored path.
		return;
	}
	sema->GetContext().AddDeclInit(getter, plan);
}

static void AttachGeneratedSetterAssignmentPlan(
	asCSema* sema,
	asASTDeclId setter,
	asASTDeclId parameter,
	const asCDecl* field)
{
	if( sema == 0 || field == 0 || !setter.IsValid()
		|| !parameter.IsValid() || sema->GetEngine() == 0 )
	{
		return;
	}

	// Primitive/POD setters remain exact storage writes. A non-trivial VALUE setter
	// is an assignment operation: its backing object is already live and must be
	// mutated through the overload selected by Sema, never reconstructed or
	// widened into a raw storage copy by a backend.
	const asASTExprId lhs = sema->ActOnDeclRef(
		field->id, field->type, field->range);
	const asASTExprId rhs = sema->ActOnDeclRef(
		parameter, field->type, field->range);
	const asASTExprId plan = sema->ActOnGeneratedAssign(
		lhs, rhs, field->type, field->range);
	const asCExpr* const assign = sema->GetContext().GetExpr(plan);
	const asCDecl* const valueClass = FindGeneratedAccessorValueClass(
		sema->GetContext(), field->type);
	const asCDecl* const assignOperator = assign != 0
		? sema->GetContext().GetDecl(assign->resolvedDecl) : 0;
	const asCDecl* const assignParameter = assignOperator != 0
		? sema->GetContext().GetFormalDecl(assignOperator->id, 0) : 0;
	const asDWORD assignDirection = assignParameter != 0
		? (assignParameter->type.quals & asAST_QUAL_PARAM_DIR_MASK) : 0;
	if( assign == 0 || assign->kind != asAST_EXPR_ASSIGN
		|| assign->type != field->type
		|| assign->children.GetLength() != 2
		|| assign->children[0] != lhs || assign->children[1] != rhs
		|| assignOperator == 0
		|| assignOperator->kind != asAST_DECL_METHOD
		|| !assignOperator->name.Equals("opAssign")
		|| valueClass == 0 || assignOperator->parent != valueClass->id
		|| sema->GetContext().GetFormalDeclCount(assignOperator->id) != 1
		|| assignParameter == 0
		|| assignParameter->type.type != field->type.type
		|| !assignParameter->type.IsConst()
		|| !assignParameter->type.IsReference()
		|| (assignDirection != 0
			&& (assignDirection & asAST_QUAL_IN) == 0) )
	{
		sema->AddDiagnostic(
			"generated-accessor-assignment-operator-unavailable", field->range);
		return;
	}
	sema->GetContext().AddDeclInit(setter, plan);
}

void asCSema::ReconcileGeneratedAccessorPlans()
{
	// Accessor declarations are synthesized as soon as their owning field is
	// parsed, but a later source section can still supply the field type's final
	// storage trait, copy constructor and opAssign. Author plans once at the
	// final layout boundary instead of baking parser callback order into them.
	const asUINT declarationCount = context.GetDeclCount();
	for( asUINT declarationIndex = 1;
		declarationIndex <= declarationCount; ++declarationIndex )
	{
		const asCDecl* const accessor = context.GetDecl(
			asASTDeclId(declarationIndex));
		if( accessor == 0 || accessor->kind != asAST_DECL_METHOD
			|| (accessor->traits & asAST_TRAIT_GENERATED) == 0
			|| accessor->accessorKind == asAST_ACCESSOR_NONE
			|| !accessor->accessorField.IsValid()
			|| accessor->inits.GetLength() != 0 )
		{
			continue;
		}

		const asCDecl* const field = context.GetDecl(accessor->accessorField);
		if( field == 0 || field->kind != asAST_DECL_VAR
			|| !RequiresExactValueObjectPlan(field->type) )
		{
			continue;
		}

		if( accessor->accessorKind == asAST_ACCESSOR_GET )
		{
			AttachGeneratedGetterCopyPlan(this, accessor->id, field);
			continue;
		}
		if( accessor->accessorKind != asAST_ACCESSOR_SET
			|| context.GetFormalDeclCount(accessor->id) != 1 )
		{
			AddDiagnostic(
				"generated-accessor-parameter-unavailable", accessor->range);
			continue;
		}
		const asCDecl* const parameter =
			context.GetFormalDecl(accessor->id, 0);
		if( parameter == 0 || parameter->type != field->type )
		{
			AddDiagnostic(
				"generated-accessor-parameter-unavailable", accessor->range);
			continue;
		}
		AttachGeneratedSetterAssignmentPlan(
			this, accessor->id, parameter->id, field);
	}
}

static void EnsureGeneratedAccessors(asCSema* sema, asASTDeclId cls)
{
	const asCDecl* decl = sema->GetContext().GetDecl(cls);
	if( decl == 0 || (decl->kind != asAST_DECL_CLASS && decl->kind != asAST_DECL_INTERFACE) )
	{
		return;
	}

	asCArray<asASTDeclId> vars;
	for( asUINT i = 0; i < decl->children.GetLength(); ++i )
	{
		const asCDecl* child = sema->GetContext().GetDecl(decl->children[i]);
		if( child && child->kind == asAST_DECL_VAR )
		{
			vars.PushLast(child->id);
		}
	}

	for( asUINT v = 0; v < vars.GetLength(); ++v )
	{
		const asCDecl* field = sema->GetContext().GetDecl(vars[v]);
		if( field == 0 || field->name.GetLength() == 0 )
		{
			continue;
		}
		asCString getName = AS_GET_PREFIX;
		getName += field->name;
		asCString setName = AS_SET_PREFIX;
		setName += field->name;
		if( !ClassHasNamedMethod(sema, decl, getName.AddressOf()) )
		{
			const asASTDeclId getter = sema->ActOnMethodDecl(cls, getName.AddressOf(), field->type, field->range);
			sema->GetContext().AddDeclTrait(getter, asAST_TRAIT_GENERATED);
			sema->GetContext().SetGeneratedAccessor(getter, asAST_ACCESSOR_GET, field->id);
			if( field->accessSpecifier.IsValid() )
			{
				sema->GetContext().SetDeclAccessSpecifier(
					getter, field->accessSpecifier);
			}
		}
		if( !ClassHasNamedMethod(sema, decl, setName.AddressOf()) )
		{
			const asCQualType voidType = sema->GetContext().InternPrimitive(ttVoid, 0);
			const asASTDeclId setter = sema->ActOnMethodDecl(cls, setName.AddressOf(), voidType, field->range);
			sema->GetContext().AddDeclTrait(setter, asAST_TRAIT_GENERATED);
			const asASTDeclId parameter =
				sema->ActOnParamDecl(setter, "value", field->type, field->range);
			sema->GetContext().SetGeneratedAccessor(setter, asAST_ACCESSOR_SET, field->id);
			if( field->accessSpecifier.IsValid() )
			{
				sema->GetContext().SetDeclAccessSpecifier(
					setter, field->accessSpecifier);
			}
		}
	}
}

asASTDeclId asCSema::ProjectCanonicalRegisteredBaseType(
	const asCString& authoredName,
	bool absoluteScope,
	asASTDeclId lookupScope,
	bool& ambiguous)
{
	ambiguous = false;
	if( authoredName.GetLength() == 0 )
	{
		return asASTDeclId();
	}

	asCString stableKey;
	const asSCanonicalRegisteredTypeDeclarationFact* const fact =
		ResolveCanonicalRegisteredTypeDeclaration(
			authoredName,
			absoluteScope,
			lookupScope,
			stableKey,
			ambiguous);
	if( fact == 0 || ambiguous
		|| (fact->kind != asAST_TYPE_REFERENCE_OBJECT
			&& fact->kind != asAST_TYPE_VALUE_OBJECT) )
	{
		return asASTDeclId();
	}

	// Build the reachable registered graph in two phases. Creating/finding all
	// declaration shells before adding edges makes projection idempotent, repairs
	// shells previously interned for native members, and terminates even if a
	// malformed host registry contains an ancestry cycle.
	asCArray<asCString> projectedKeys;
	asCArray<asASTDeclId> projectedDeclarations;
	projectedKeys.PushLast(stableKey);
	for( asUINT projectedIndex = 0;
		projectedIndex < projectedKeys.GetLength(); ++projectedIndex )
	{
		const asCString projectedKey = projectedKeys[projectedIndex];
		const asSCanonicalRegisteredTypeDeclarationFact* const projectedFact =
			FindExactCanonicalRegisteredTypeDeclaration(projectedKey);
		if( projectedFact == 0
			|| projectedFact->ambiguous
			|| (projectedFact->kind != asAST_TYPE_REFERENCE_OBJECT
				&& projectedFact->kind != asAST_TYPE_VALUE_OBJECT) )
		{
			ambiguous = projectedFact != 0 && projectedFact->ambiguous;
			AddDiagnostic(ambiguous
				? "registered-native-base-ambiguous"
				: "registered-native-base-unresolved");
			return asASTDeclId();
		}

		asASTDeclId projected;
		for( asUINT declarationIndex = 1;
			declarationIndex <= context.GetDeclCount(); ++declarationIndex )
		{
			const asCDecl* const existing =
				context.GetDecl(asASTDeclId(declarationIndex));
			if( existing != 0
				&& (existing->kind == asAST_DECL_CLASS
					|| existing->kind == asAST_DECL_INTERFACE)
				&& existing->origin.Equals(CANONICAL_NATIVE_TYPE_VIEW_ORIGIN)
				&& existing->stableKey.Equals(projectedKey) )
			{
				projected = existing->id;
				break;
			}
		}
		if( !projected.IsValid() )
		{
			projected = projectedFact->isInterface
				? ActOnInterfaceDecl(
					context.GetTranslationUnit(),
					projectedKey.AddressOf(),
					asCSourceRange())
				: ActOnClassDecl(
					context.GetTranslationUnit(),
					projectedKey.AddressOf(),
					asCSourceRange());
			if( !projected.IsValid() )
			{
				return projected;
			}
		}

		context.SetOrigin(projected, CANONICAL_NATIVE_TYPE_VIEW_ORIGIN);
		asDWORD projectedQuals = 0;
		if( (projectedFact->flags & asOBJ_IMPLICIT_HANDLE) != 0 )
		{
			projectedQuals |= asAST_QUAL_HANDLE;
		}
		context.SetDeclType(projected, context.InternNamedType(
			projectedFact->kind, projectedKey.AddressOf(), projectedQuals));
		if( (projectedFact->flags & asOBJ_VALUE) != 0 )
		{
			context.AddDeclTrait(projected, asAST_TRAIT_VALUE);
		}
		projectedDeclarations.PushLast(projected);

		for( asUINT baseIndex = 0;
			baseIndex < projectedFact->baseStableKeys.GetLength(); ++baseIndex )
		{
			const asCString& baseKey =
				projectedFact->baseStableKeys[baseIndex];
			bool alreadyQueued = false;
			for( asUINT keyIndex = 0;
				keyIndex < projectedKeys.GetLength(); ++keyIndex )
			{
				if( projectedKeys[keyIndex].Equals(baseKey) )
				{
					alreadyQueued = true;
					break;
				}
			}
			if( !alreadyQueued )
			{
				projectedKeys.PushLast(baseKey);
			}
		}
	}

	for( asUINT projectedIndex = 0;
		projectedIndex < projectedKeys.GetLength(); ++projectedIndex )
	{
		const asSCanonicalRegisteredTypeDeclarationFact* const projectedFact =
			FindExactCanonicalRegisteredTypeDeclaration(
				projectedKeys[projectedIndex]);
		for( asUINT baseIndex = 0;
			projectedFact != 0
				&& baseIndex < projectedFact->baseStableKeys.GetLength();
			++baseIndex )
		{
			asASTDeclId base;
			for( asUINT keyIndex = 0;
				keyIndex < projectedKeys.GetLength(); ++keyIndex )
			{
				if( projectedKeys[keyIndex].Equals(
					projectedFact->baseStableKeys[baseIndex]) )
				{
					base = projectedDeclarations[keyIndex];
					break;
				}
			}
			if( !base.IsValid() )
			{
				AddDiagnostic("registered-native-base-unresolved");
				return asASTDeclId();
			}

			const asCDecl* const declaration =
				context.GetDecl(projectedDeclarations[projectedIndex]);
			bool alreadyRecorded = false;
			for( asUINT recordedIndex = 0;
				declaration != 0
					&& recordedIndex < declaration->bases.GetLength();
				++recordedIndex )
			{
				if( declaration->bases[recordedIndex] == base )
				{
					alreadyRecorded = true;
					break;
				}
			}
			if( !alreadyRecorded )
			{
				context.AddBase(projectedDeclarations[projectedIndex], base);
			}
		}
	}
	return projectedDeclarations.GetLength() != 0
		? projectedDeclarations[0]
		: asASTDeclId();
}

asCQualType asCSema::InternCanonicalRegisteredTypeRef(
	const asSCanonicalRegisteredTypeRefFact& fact)
{
	if( fact.kind == asAST_TYPE_VOID || fact.kind == asAST_TYPE_PRIMITIVE )
	{
		return context.InternPrimitive(fact.primitiveToken, fact.quals);
	}
	if( fact.kind == asAST_TYPE_INVALID || fact.stableKey.GetLength() == 0 )
	{
		return asCQualType();
	}
	return context.InternNamedType(
		fact.kind, fact.stableKey.AddressOf(), fact.quals);
}

asASTDeclId asCSema::ProjectCanonicalRegisteredFuncdef(const asCString& stableKey)
{
	if( stableKey.GetLength() == 0
		|| !context.GetTranslationUnit().IsValid() )
	{
		return asASTDeclId();
	}
	const asSCanonicalRegisteredTypeDeclarationFact* const fact =
		FindExactCanonicalRegisteredTypeDeclaration(stableKey);
	if( fact == 0 || fact->ambiguous
		|| fact->kind != asAST_TYPE_FUNCDEF
		|| (fact->funcdefReturn.kind != asAST_TYPE_VOID
			&& fact->funcdefReturn.kind != asAST_TYPE_PRIMITIVE
			&& fact->funcdefReturn.stableKey.GetLength() == 0) )
	{
		return asASTDeclId();
	}

	asASTDeclId projected;
	for( asUINT declarationIndex = 1;
		declarationIndex <= context.GetDeclCount(); ++declarationIndex )
	{
		const asCDecl* const existing =
			context.GetDecl(asASTDeclId(declarationIndex));
		if( existing != 0
			&& existing->kind == asAST_DECL_FUNCDEF
			&& existing->origin.Equals(CANONICAL_NATIVE_TYPE_VIEW_ORIGIN)
			&& existing->stableKey.Equals(stableKey) )
		{
			projected = existing->id;
			break;
		}
	}
	if( !projected.IsValid() )
	{
		projected = ActOnStartFuncDefDecl(
			context.GetTranslationUnit(),
			stableKey.AddressOf(),
			asCSourceRange());
		if( !projected.IsValid() )
		{
			return projected;
		}
		context.SetOrigin(projected, CANONICAL_NATIVE_TYPE_VIEW_ORIGIN);
	}

	const asCQualType returnType = InternCanonicalRegisteredTypeRef(
		fact->funcdefReturn);
	if( !returnType.IsValid() )
	{
		return asASTDeclId();
	}
	context.SetDeclType(projected, returnType);

	if( context.GetFormalDeclCount(projected) == 0 )
	{
		for( asUINT formalIndex = 0;
			formalIndex < fact->funcdefFormals.GetLength(); ++formalIndex )
		{
			const asCQualType formalType = InternCanonicalRegisteredTypeRef(
				fact->funcdefFormals[formalIndex]);
			if( !formalType.IsValid() )
			{
				return asASTDeclId();
			}
			if( !ActOnParamDecl(
				projected, "", formalType, asCSourceRange()).IsValid() )
			{
				return asASTDeclId();
			}
		}
	}
	else if( context.GetFormalDeclCount(projected)
		!= fact->funcdefFormals.GetLength() )
	{
		return asASTDeclId();
	}
	context.SetOrigin(projected, CANONICAL_NATIVE_TYPE_VIEW_ORIGIN);
	return projected;
}

static bool IsRecordDeclaration(const asCDecl* declaration)
{
	return declaration != 0
		&& (declaration->kind == asAST_DECL_CLASS
			|| declaration->kind == asAST_DECL_INTERFACE);
}

static bool IsCanonicalMethodDeclaration(const asCDecl* declaration)
{
	return declaration != 0 && declaration->kind == asAST_DECL_METHOD;
}

static bool CanonicalMethodSignaturesEqual(
	const asCASTContext& context,
	const asCDecl* lhs,
	const asCDecl* rhs)
{
	if( !IsCanonicalMethodDeclaration(lhs)
		|| !IsCanonicalMethodDeclaration(rhs)
		|| !lhs->name.Equals(rhs->name)
		|| lhs->type != rhs->type
		|| ((lhs->traits & asAST_TRAIT_CONST_METHOD) != 0)
			!= ((rhs->traits & asAST_TRAIT_CONST_METHOD) != 0) )
	{
		return false;
	}

	const asUINT lhsParameterCount = context.GetFormalDeclCount(lhs->id);
	const asUINT rhsParameterCount = context.GetFormalDeclCount(rhs->id);
	if( lhsParameterCount != rhsParameterCount )
	{
		return false;
	}
	for( asUINT formalIndex = 0;
		formalIndex < lhsParameterCount; ++formalIndex )
	{
		const asCDecl* const lhsParameter =
			context.GetFormalDecl(lhs->id, formalIndex);
		const asCDecl* const rhsParameter =
			context.GetFormalDecl(rhs->id, formalIndex);
		if( lhsParameter == 0 || rhsParameter == 0
			|| lhsParameter->type != rhsParameter->type )
		{
			return false;
		}
	}
	return true;
}

static bool ContainsDeclId(
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

static void CollectClassChain(
	const asCASTContext& context,
	const asCDecl* record,
	asCArray<asASTDeclId>& classes)
{
	const asCDecl* current = record;
	while( current != 0 )
	{
		asASTDeclId nextClass;
		for( asUINT baseIndex = 0;
			baseIndex < current->bases.GetLength(); ++baseIndex )
		{
			const asCDecl* const base = context.GetDecl(current->bases[baseIndex]);
			if( base != 0 && base->kind == asAST_DECL_CLASS )
			{
				nextClass = base->id;
				break;
			}
		}
		if( !nextClass.IsValid() || ContainsDeclId(classes, nextClass) )
		{
			return;
		}
		classes.PushLast(nextClass);
		current = context.GetDecl(nextClass);
	}
}

static void CollectInterfaceClosure(
	const asCASTContext& context,
	const asCDecl* record,
	const asCArray<asASTDeclId>& classChain,
	asCArray<asASTDeclId>& interfaces)
{
	asCArray<asASTDeclId> pending;
	if( record != 0 )
	{
		for( asUINT baseIndex = 0;
			baseIndex < record->bases.GetLength(); ++baseIndex )
		{
			pending.PushLast(record->bases[baseIndex]);
		}
	}
	for( asUINT classIndex = 0;
		classIndex < classChain.GetLength(); ++classIndex )
	{
		const asCDecl* const cls = context.GetDecl(classChain[classIndex]);
		for( asUINT baseIndex = 0;
			cls != 0 && baseIndex < cls->bases.GetLength(); ++baseIndex )
		{
			pending.PushLast(cls->bases[baseIndex]);
		}
	}

	for( asUINT pendingIndex = 0;
		pendingIndex < pending.GetLength(); ++pendingIndex )
	{
		const asCDecl* const base = context.GetDecl(pending[pendingIndex]);
		if( base == 0 || base->kind != asAST_DECL_INTERFACE
			|| ContainsDeclId(interfaces, base->id) )
		{
			continue;
		}
		interfaces.PushLast(base->id);
		for( asUINT baseIndex = 0;
			baseIndex < base->bases.GetLength(); ++baseIndex )
		{
			pending.PushLast(base->bases[baseIndex]);
		}
	}
}

static asUINT FindExactMethodsInRecord(
	const asCASTContext& context,
	const asCDecl* owner,
	const asCDecl* signature,
	asASTDeclId& match)
{
	match = asASTDeclId();
	asUINT count = 0;
	for( asUINT childIndex = 0;
		owner != 0 && childIndex < owner->children.GetLength(); ++childIndex )
	{
		const asCDecl* const child = context.GetDecl(owner->children[childIndex]);
		if( CanonicalMethodSignaturesEqual(context, child, signature) )
		{
			match = child->id;
			++count;
		}
	}
	return count;
}

static void AddMethodRelationDiagnostic(
	asCSema* sema,
	const char* prefix,
	const asCDecl* declaration,
	const asCSourceRange& range)
{
	asCString diagnostic = prefix;
	if( declaration != 0 && declaration->stableKey.GetLength() != 0 )
	{
		diagnostic += ":";
		diagnostic += declaration->stableKey;
	}
	sema->AddDiagnostic(diagnostic.AddressOf(), range);
}

static bool AddCanonicalMethodRelation(
	asCSema* sema,
	asASTDeclId owner,
	asEASTMethodRelationKind kind,
	asASTDeclId implementation,
	asASTDeclId requirement)
{
	const int status = sema->GetContext().AddMethodRelation(
		owner,
		asSASTMethodRelation(kind, implementation, requirement));
	if( status == asAST_VERIFY_OK )
	{
		return true;
	}
	sema->AddDiagnostic("method-relation-publication-invalid");
	return false;
}

static void FinalizeRecordMethodRelations(
	asCSema* sema,
	asASTDeclId recordId)
{
	asCASTContext& context = sema->GetContext();
	const asCDecl* const record = context.GetDecl(recordId);
	if( record == 0 || record->kind != asAST_DECL_CLASS )
	{
		return;
	}

	asCArray<asASTDeclId> classChain;
	CollectClassChain(context, record, classChain);
	asCArray<asASTDeclId> interfaces;
	CollectInterfaceClosure(context, record, classChain, interfaces);

	asCArray<asASTDeclId> ownMethods;
	for( asUINT childIndex = 0;
		childIndex < record->children.GetLength(); ++childIndex )
	{
		const asCDecl* const child = context.GetDecl(record->children[childIndex]);
		if( IsCanonicalMethodDeclaration(child) )
		{
			ownMethods.PushLast(child->id);
		}
	}

	asCArray<asASTDeclId> relatedOwnMethods;
	for( asUINT methodIndex = 0;
		methodIndex < ownMethods.GetLength(); ++methodIndex )
	{
		const asCDecl* const method = context.GetDecl(ownMethods[methodIndex]);
		for( asUINT classIndex = 0;
			classIndex < classChain.GetLength(); ++classIndex )
		{
			asASTDeclId target;
			const asUINT count = FindExactMethodsInRecord(
				context, context.GetDecl(classChain[classIndex]), method, target);
			if( count > 1 )
			{
				AddMethodRelationDiagnostic(
					sema, "method-override-target-ambiguous", method, method->range);
				break;
			}
			if( count == 1 )
			{
				if( AddCanonicalMethodRelation(
					sema,
					recordId,
					asAST_METHOD_RELATION_BASE_OVERRIDE,
					method->id,
					target) )
				{
					relatedOwnMethods.PushLast(method->id);
				}
				break;
			}
		}
	}

	for( asUINT interfaceIndex = 0;
		interfaceIndex < interfaces.GetLength(); ++interfaceIndex )
	{
		const asCDecl* const interfaceDecl =
			context.GetDecl(interfaces[interfaceIndex]);
		for( asUINT childIndex = 0;
			interfaceDecl != 0
				&& childIndex < interfaceDecl->children.GetLength(); ++childIndex )
		{
			const asCDecl* const requirement =
				context.GetDecl(interfaceDecl->children[childIndex]);
			if( !IsCanonicalMethodDeclaration(requirement) )
			{
				continue;
			}

			asASTDeclId implementation;
			bool ambiguous = false;
			asASTDeclId candidate;
			asUINT count = FindExactMethodsInRecord(
				context, record, requirement, candidate);
			if( count > 1 )
			{
				ambiguous = true;
			}
			else if( count == 1 )
			{
				implementation = candidate;
			}
			else
			{
				for( asUINT classIndex = 0;
					classIndex < classChain.GetLength(); ++classIndex )
				{
					count = FindExactMethodsInRecord(
						context,
						context.GetDecl(classChain[classIndex]),
						requirement,
						candidate);
					if( count > 1 )
					{
						ambiguous = true;
						break;
					}
					if( count == 1 )
					{
						implementation = candidate;
						break;
					}
				}
			}

			if( ambiguous )
			{
				AddMethodRelationDiagnostic(
					sema,
					"interface-method-implementation-ambiguous",
					requirement,
					requirement->range);
				continue;
			}
			if( !implementation.IsValid() )
			{
				AddMethodRelationDiagnostic(
					sema,
					"interface-method-implementation-missing",
					requirement,
					record->range);
				continue;
			}
			if( AddCanonicalMethodRelation(
				sema,
				recordId,
				asAST_METHOD_RELATION_INTERFACE_IMPLEMENTATION,
				implementation,
				requirement->id)
				&& ContainsDeclId(ownMethods, implementation)
				&& !ContainsDeclId(relatedOwnMethods, implementation) )
			{
				relatedOwnMethods.PushLast(implementation);
			}
		}
	}

	for( asUINT methodIndex = 0;
		methodIndex < ownMethods.GetLength(); ++methodIndex )
	{
		const asCDecl* const method = context.GetDecl(ownMethods[methodIndex]);
		if( method != 0
			&& (method->traits & asAST_TRAIT_OVERRIDE) != 0
			&& !ContainsDeclId(relatedOwnMethods, method->id) )
		{
			AddMethodRelationDiagnostic(
				sema,
				"method-override-target-missing",
				method,
				method->range);
		}
	}
}

static bool IsGlobalOrFieldVariableParent(const asCDecl* declaration)
{
	return declaration != 0
		&& (declaration->kind == asAST_DECL_TRANSLATION_UNIT
			|| declaration->kind == asAST_DECL_NAMESPACE
			|| declaration->kind == asAST_DECL_CLASS);
}

static asASTDeclId FindExactRecordStableKey(
	asCASTContext& context,
	const asCString& stableKey,
	bool& ambiguous)
{
	ambiguous = false;
	asASTDeclId match;
	for( asUINT declarationIndex = 1;
		declarationIndex <= context.GetDeclCount(); ++declarationIndex )
	{
		const asCDecl* const declaration =
			context.GetDecl(asASTDeclId(declarationIndex));
		if( !IsRecordDeclaration(declaration)
			|| !declaration->stableKey.Equals(stableKey) )
		{
			continue;
		}
		if( match.IsValid() && match != declaration->id )
		{
			ambiguous = true;
			return asASTDeclId();
		}
		match = declaration->id;
	}
	return match;
}

static asASTDeclId ResolveRecordBaseAction(
	asCSema* sema,
	asASTDeclId derived,
	const asCString& authoredName,
	bool& ambiguous)
{
	ambiguous = false;
	if( sema == 0 || authoredName.GetLength() == 0 )
	{
		return asASTDeclId();
	}
	asCASTContext& context = sema->GetContext();
	const asCDecl* const derivedDecl = context.GetDecl(derived);
	if( !IsRecordDeclaration(derivedDecl) )
	{
		return asASTDeclId();
	}

	asCString name = authoredName;
	const bool absolute = name.GetLength() >= 2
		&& name.SubString(0, 2) == "::";
	if( absolute )
	{
		name = name.SubString(2);
	}
	const bool qualified = name.FindLast("::") >= 0;
	if( qualified )
	{
		asASTDeclId scope = absolute
			? context.GetTranslationUnit() : derivedDecl->parent;
		while( scope.IsValid() )
		{
			const asCDecl* const scopeDecl = context.GetDecl(scope);
			asCString candidateKey;
			if( scopeDecl != 0
				&& scopeDecl->kind != asAST_DECL_TRANSLATION_UNIT
				&& scopeDecl->stableKey.GetLength() != 0 )
			{
				candidateKey = scopeDecl->stableKey;
				candidateKey += "::";
			}
			candidateKey += name;
			const asASTDeclId exact =
				FindExactRecordStableKey(context, candidateKey, ambiguous);
			if( exact.IsValid() || ambiguous )
			{
				return exact;
			}
			if( absolute || scopeDecl == 0 )
			{
				break;
			}
			scope = scopeDecl->parent;
		}
	}
	else
	{
		asCArray<asASTDeclId> hits;
		sema->LookupCandidatesFrom(derivedDecl->parent, name.AddressOf(), hits);
		asASTDeclId match;
		for( asUINT hitIndex = 0; hitIndex < hits.GetLength(); ++hitIndex )
		{
			const asCDecl* const hit = context.GetDecl(hits[hitIndex]);
			if( !IsRecordDeclaration(hit) )
			{
				continue;
			}
			if( match.IsValid() && match != hit->id )
			{
				ambiguous = true;
				return asASTDeclId();
			}
			match = hit->id;
		}
		if( match.IsValid() )
		{
			return match;
		}
	}
	return sema->ProjectCanonicalRegisteredBaseType(
		name, absolute, derivedDecl->parent, ambiguous);
}

void asCSema::ActOnRecordBaseSpecifiersAction(
	asASTDeclId derived,
	const asCArray<asSRecordBaseSpecifierAction>& actions,
	asCScriptCode* script)
{
	lastActedDecl = asASTDeclId();
	if( !PrepareParsedSource(script) )
	{
		return;
	}
	const asCDecl* const initialDeclaration = context.GetDecl(derived);
	if( !IsRecordDeclaration(initialDeclaration) )
	{
		AddDiagnostic("record-base-owner-invalid");
		return;
	}

	const asCSourceManager& manager = context.GetSourceManager();
	for( asUINT actionIndex = 0; actionIndex < actions.GetLength(); ++actionIndex )
	{
		const asSRecordBaseSpecifierAction& action = actions[actionIndex];
		if( action.qualifiedName.GetLength() == 0
			|| action.endOffset < action.beginOffset )
		{
			AddDiagnostic("record-base-action-invalid");
			return;
		}
		const asCSourceRange range = manager.MakeRange(
			parsedFile, action.beginOffset, action.endOffset);
		if( !range.IsValid() )
		{
			AddDiagnostic("record-base-source-range-invalid");
			return;
		}
	}

	for( asUINT actionIndex = 0; actionIndex < actions.GetLength(); ++actionIndex )
	{
		const asSRecordBaseSpecifierAction& action = actions[actionIndex];
		bool ambiguous = false;
		const asASTDeclId resolvedBase = ResolveRecordBaseAction(
			this, derived, action.qualifiedName, ambiguous);
		const asCSourceRange range = manager.MakeRange(
			parsedFile, action.beginOffset, action.endOffset);
		if( ambiguous )
		{
			AddDiagnostic("record-base-ambiguous", range);
			continue;
		}
		const asCDecl* const baseDecl = context.GetDecl(resolvedBase);
		RecordDependency(derived,
			baseDecl != 0 && baseDecl->stableKey.GetLength() != 0
				? baseDecl->stableKey.AddressOf()
				: action.qualifiedName.AddressOf());
		if( !resolvedBase.IsValid() )
		{
			continue;
		}
		const asCDecl* const currentDerived = context.GetDecl(derived);
		bool alreadyRecorded = false;
		for( asUINT baseIndex = 0;
			currentDerived != 0
				&& baseIndex < currentDerived->bases.GetLength();
			++baseIndex )
		{
			if( currentDerived->bases[baseIndex] == resolvedBase )
			{
				alreadyRecorded = true;
				break;
			}
		}
		if( !alreadyRecorded )
		{
			context.AddBase(derived, resolvedBase);
		}
	}

	// The UE preprocessor deliberately erases a native code superclass from the
	// Parser text while retaining it in the module's PreClass metadata. The
	// native shadow is an orthogonal embedding/layout relation: it may coexist
	// with one authored script base. Consume the pointer-free fact captured at
	// that registration boundary after the complete authored base list, and
	// publish only stable AST identity.
	const asCDecl* declaration = context.GetDecl(derived);
	if( declaration != 0 && buildModule != 0 )
	{
		const asSCanonicalPreClassFact* const classFact =
			buildModule->FindCanonicalPreClassFact(declaration->name);
		if( classFact != 0 && classFact->hasShadowType )
		{
			const asASTDeclId declarationParent = declaration->parent;
			const asCSourceRange declarationRange = declaration->range;
			bool ambiguous = false;
			const asASTDeclId nativeBase =
				ProjectCanonicalRegisteredBaseType(
					classFact->shadowTypeStableKey,
					true,
					declarationParent,
					ambiguous);
			if( nativeBase.IsValid() )
			{
				const asCDecl* const projected = context.GetDecl(nativeBase);
				RecordDependency(derived,
					projected != 0 && projected->stableKey.GetLength() != 0
						? projected->stableKey.AddressOf()
						: classFact->shadowTypeStableKey.AddressOf());
				const asCDecl* const currentDerived = context.GetDecl(derived);
				bool alreadyRecorded = false;
				for( asUINT baseIndex = 0;
					currentDerived != 0
						&& baseIndex < currentDerived->bases.GetLength();
					++baseIndex )
				{
					if( currentDerived->bases[baseIndex] == nativeBase )
					{
						alreadyRecorded = true;
						break;
					}
				}
				if( !alreadyRecorded )
				{
					context.AddBase(derived, nativeBase);
				}
			}
			else
			{
				AddDiagnostic(
					ambiguous
						? "preclass-native-base-ambiguous"
						: "preclass-native-base-unresolved",
					declarationRange);
			}
		}
	}
	NoteActedDecl(derived);
}

static asASTDeclId FindExistingNamedDecl(asCSema* sema, asASTDeclId parent, const char* name, asEASTDeclKind kind)
{
	if( name == 0 || *name == 0 )
	{
		return asASTDeclId();
	}
	const asCDecl* parentDecl = sema->GetContext().GetDecl(parent);
	if( parentDecl == 0 )
	{
		return asASTDeclId();
	}
	for( asUINT i = 0; i < parentDecl->children.GetLength(); ++i )
	{
		const asCDecl* child = sema->GetContext().GetDecl(parentDecl->children[i]);
		if( child && child->kind == kind && child->name.Equals(name) )
		{
			return child->id;
		}
	}
	return asASTDeclId();
}

static bool IsFunctionLikeDeclKind(asEASTDeclKind kind)
{
	return kind == asAST_DECL_FUNCTION
		|| kind == asAST_DECL_METHOD
		|| kind == asAST_DECL_CONSTRUCTOR
		|| kind == asAST_DECL_DESTRUCTOR
		|| kind == asAST_DECL_MIXIN;
}

static bool IsEnclosingFunctionLocal(asCSema* sema, const asCDecl* target, asASTDeclId lambda)
{
	if( sema == 0 || target == 0 || !lambda.IsValid() )
	{
		return false;
	}
	asASTDeclId current = target->parent;
	while( current.IsValid() )
	{
		if( current == lambda )
		{
			return false;
		}
		const asCDecl* owner = sema->GetContext().GetDecl(current);
		if( owner == 0 )
		{
			break;
		}
		if( IsFunctionLikeDeclKind(owner->kind) )
		{
			return true;
		}
		current = owner->parent;
	}
	return false;
}

static void RecordCapturesFromExpr(asCSema* sema, asASTExprId id, asASTDeclId lambda);
static void RecordCapturesFromStmt(asCSema* sema, asASTStmtId id, asASTDeclId lambda);

static void RecordCapturesFromExpr(asCSema* sema, asASTExprId id, asASTDeclId lambda)
{
	const asCExpr* expr = sema->GetContext().GetExpr(id);
	if( expr == 0 )
	{
		return;
	}
	if( expr->kind == asAST_EXPR_DECL_REF )
	{
		const asCDecl* target = sema->GetContext().GetDecl(expr->resolvedDecl);
		if( target
			&& (target->kind == asAST_DECL_VAR || target->kind == asAST_DECL_PARAM)
			&& IsEnclosingFunctionLocal(sema, target, lambda) )
		{
			sema->ActOnLambdaCapture(lambda, target->id);
		}
	}
	for( asUINT i = 0; i < expr->children.GetLength(); ++i )
	{
		RecordCapturesFromExpr(sema, expr->children[i], lambda);
	}
	if( expr->kind == asAST_EXPR_CALL && expr->receiver.IsValid() )
	{
		RecordCapturesFromExpr(sema, expr->receiver, lambda);
	}
}

static void RecordCapturesFromStmt(asCSema* sema, asASTStmtId id, asASTDeclId lambda)
{
	const asCStmt* stmt = sema->GetContext().GetStmt(id);
	if( stmt == 0 )
	{
		return;
	}
	if( stmt->expr.IsValid() )
	{
		RecordCapturesFromExpr(sema, stmt->expr, lambda);
	}
	for( asUINT i = 0; i < stmt->children.GetLength(); ++i )
	{
		RecordCapturesFromStmt(sema, stmt->children[i], lambda);
	}
}

static void RecordLambdaCaptures(asCSema* sema, asASTDeclId fn)
{
	const asCDecl* lambda = sema->GetContext().GetDecl(fn);
	if( lambda == 0 || (lambda->traits & asAST_TRAIT_LAMBDA) == 0 || !lambda->body.IsValid() )
	{
		return;
	}
	RecordCapturesFromStmt(sema, lambda->body, fn);
}

void asCSema::ValidateCapturedLambdaUses()
{
	// AngelScript funcdef values are function handles, not closure objects. A
	// non-capturing anonymous function can safely escape into one; a capturing
	// lambda can currently execute only through its direct Call, where Canonical
	// CodeGen passes the sealed captures as hidden arguments. Validate this as a
	// whole-graph Sema fact so assignment/conversion/return/call-argument wrappers
	// cannot hide an unsupported escape from the backend.
	for( asUINT declIndex = 1; declIndex <= context.GetDeclCount(); ++declIndex )
	{
		const asCDecl* const lambda = context.GetDecl(asASTDeclId(declIndex));
		if( lambda == 0
			|| (lambda->traits & asAST_TRAIT_LAMBDA) == 0
			|| lambda->captures.GetLength() == 0 )
		{
			continue;
		}

		bool hasDirectCall = false;
		for( asUINT exprIndex = 1; exprIndex <= context.GetExprCount(); ++exprIndex )
		{
			const asCExpr* const expr = context.GetExpr(asASTExprId(exprIndex));
			if( expr != 0
				&& expr->kind == asAST_EXPR_CALL
				&& expr->resolvedDecl == lambda->id )
			{
				hasDirectCall = true;
				break;
			}
		}

		if( !hasDirectCall )
		{
			AddDiagnostic("capturing-lambda-cannot-escape", lambda->range);
		}
	}
}

static bool CanonicalFormalHasDefault(const asCDecl* parameter)
{
	return parameter != 0
		&& (parameter->defaultArg.GetLength() != 0
			|| parameter->inits.GetLength() != 0);
}

static bool CanonicalDefaultArgOverloadConflict(
	const asCASTContext& context,
	const asCDecl* left,
	const asCDecl* right)
{
	if( left == 0 || right == 0 || left == right
		|| !left->name.Equals(right->name)
		|| left->parent != right->parent )
	{
		return false;
	}
	const asDWORD constMask = asAST_TRAIT_CONST_METHOD;
	if( (left->traits & constMask) != (right->traits & constMask) )
	{
		return false;
	}

	const asUINT leftCount = context.GetFormalDeclCount(left->id);
	const asUINT rightCount = context.GetFormalDeclCount(right->id);
	asUINT prefix = 0;
	const asUINT shared = leftCount < rightCount ? leftCount : rightCount;
	for( ; prefix < shared; ++prefix )
	{
		const asCDecl* const leftFormal =
			context.GetFormalDecl(left->id, prefix);
		const asCDecl* const rightFormal =
			context.GetFormalDecl(right->id, prefix);
		if( CanonicalFormalHasDefault(leftFormal)
			|| CanonicalFormalHasDefault(rightFormal) )
		{
			break;
		}
		if( leftFormal == 0 || rightFormal == 0
			|| leftFormal->type != rightFormal->type )
		{
			return false;
		}
	}

	if( prefix >= leftCount && prefix < rightCount )
	{
		return CanonicalFormalHasDefault(
			context.GetFormalDecl(right->id, prefix));
	}
	if( prefix >= rightCount && prefix < leftCount )
	{
		return CanonicalFormalHasDefault(
			context.GetFormalDecl(left->id, prefix));
	}
	return false;
}

void asCSema::ValidateDefaultArgumentOverloads()
{
	const asUINT declarationCount = context.GetDeclCount();
	for( asUINT leftIndex = 1; leftIndex <= declarationCount; ++leftIndex )
	{
		const asCDecl* const left = context.GetDecl(asASTDeclId(leftIndex));
		if( left == 0
			|| (left->kind != asAST_DECL_FUNCTION
				&& left->kind != asAST_DECL_METHOD
				&& left->kind != asAST_DECL_MIXIN)
			|| (left->traits & asAST_TRAIT_GENERATED) != 0
			|| (left->traits & asAST_TRAIT_LAMBDA) != 0 )
		{
			continue;
		}
		for( asUINT rightIndex = leftIndex + 1;
			rightIndex <= declarationCount; ++rightIndex )
		{
			const asCDecl* const right =
				context.GetDecl(asASTDeclId(rightIndex));
			if( right == 0
				|| right->kind != left->kind
				|| (right->traits & asAST_TRAIT_GENERATED) != 0
				|| (right->traits & asAST_TRAIT_LAMBDA) != 0 )
			{
				continue;
			}
			if( CanonicalDefaultArgOverloadConflict(context, left, right) )
			{
				AddDiagnostic("default-argument-overload-conflict", right->range);
			}
		}
	}
}

void asCSema::AddEditorOnlyCharacterRange(asUINT begin, asUINT end)
{
	if( end < begin )
	{
		return;
	}
	editorOnlyBegin.PushLast(begin);
	editorOnlyEnd.PushLast(end);
}

void asCSema::ApplyEditorOnlyTraitFromSource(asASTDeclId declaration)
{
	const asCDecl* const decl = context.GetDecl(declaration);
	if( decl == 0 || !decl->range.begin.IsValid() )
	{
		return;
	}
	const asUINT offset = decl->range.begin.offset;
	for( asUINT index = 0; index < editorOnlyBegin.GetLength()
		&& index < editorOnlyEnd.GetLength(); ++index )
	{
		if( offset >= editorOnlyBegin[index]
			&& offset <= editorOnlyEnd[index] )
		{
			context.AddDeclTrait(declaration, asAST_TRAIT_EDITOR_ONLY);
			return;
		}
	}
}

void asCSema::ValidateEditorOnlyOverrides()
{
	const asUINT declarationCount = context.GetDeclCount();
	for( asUINT index = 1; index <= declarationCount; ++index )
	{
		const asCDecl* const record = context.GetDecl(asASTDeclId(index));
		if( record == 0 || record->kind != asAST_DECL_CLASS )
		{
			continue;
		}
		for( asUINT relationIndex = 0;
			relationIndex < record->methodRelations.GetLength();
			++relationIndex )
		{
			const asSASTMethodRelation& relation =
				record->methodRelations[relationIndex];
			if( relation.kind != asAST_METHOD_RELATION_BASE_OVERRIDE )
			{
				continue;
			}
			const asCDecl* const implementation =
				context.GetDecl(relation.implementation);
			const asCDecl* const requirement =
				context.GetDecl(relation.requirement);
			if( implementation == 0 || requirement == 0 )
			{
				continue;
			}
			if( (requirement->traits & asAST_TRAIT_EDITOR_ONLY) != 0
				&& (implementation->traits & asAST_TRAIT_EDITOR_ONLY) == 0 )
			{
				AddDiagnostic(
					"editor-only-override-mismatch", implementation->range);
			}
		}
	}
}

bool asCSema::ActOnFunctionBodyAction(
	const asSFunctionBodyAction& action,
	asCScriptCode* script)
{
	const asCDecl* const declaration = context.GetDecl(action.function);
	if( !action.function.IsValid() || !action.body.IsValid()
		|| action.endOffset <= action.beginOffset || declaration == 0
		|| (declaration->kind != asAST_DECL_FUNCTION
			&& declaration->kind != asAST_DECL_METHOD
			&& declaration->kind != asAST_DECL_CONSTRUCTOR
			&& declaration->kind != asAST_DECL_DESTRUCTOR
			&& declaration->kind != asAST_DECL_MIXIN)
		|| !PrepareParsedSource(script) )
	{
		AddDiagnostic("function-body-action-invalid");
		return false;
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	const asCStmt* const body = context.GetStmt(action.body);
	if( !range.IsValid() || body == 0 || body->kind != asAST_STMT_BLOCK
		|| body->owner != action.function || body->range != range )
	{
		AddDiagnostic("function-body-statement-invalid", range);
		return false;
	}
	if( declaration->body.IsValid() )
	{
		if( declaration->body == action.body )
		{
			return true;
		}
		AddDiagnostic("function-body-already-bound", range);
		return false;
	}
	context.SetBody(action.function, action.body);
	context.SetStmtSafePointRole(
		action.body, asAST_SAFEPOINT_FUNCTION_ENTRY);
	RecordLambdaCaptures(this, action.function);
	NoteActedDecl(action.function);
	return true;
}

asASTDeclId asCSema::ActOnNamespaceDecl(asASTDeclId parent, const char* name, const asCSourceRange& range)
{
	const asASTDeclId id = context.CreateDecl(asAST_DECL_NAMESPACE, parent, range, name);
	FinishDecl(id);
	InsertSymbol(id);
	return id;
}

asASTDeclId asCSema::ActOnNamespacePath(
	const asCArray<asSNamespaceDeclActionSegment>& segments,
	asCScriptCode* script)
{
	lastActedDecl = asASTDeclId();
	if( segments.GetLength() == 0 )
	{
		AddDiagnostic("namespace-path-empty");
		return asASTDeclId();
	}
	if( !PrepareParsedSource(script) )
	{
		return asASTDeclId();
	}

	const char* const sectionName = script && script->name.GetLength()
		? script->name.AddressOf() : "<memory>";
	asASTDeclId parent = CurrentDeclContext();
	if( !parent.IsValid() )
	{
		parent = context.GetTranslationUnit();
		if( !parent.IsValid() )
		{
			parent = ActOnTranslationUnit(sectionName);
		}
	}

	const asCSourceManager& manager = context.GetSourceManager();
	for( asUINT i = 0; i < segments.GetLength(); ++i )
	{
		const asSNamespaceDeclActionSegment& segment = segments[i];
		if( segment.name.GetLength() == 0 || segment.endOffset < segment.beginOffset )
		{
			AddDiagnostic("namespace-segment-invalid");
			lastActedDecl = asASTDeclId();
			return asASTDeclId();
		}
		const asCSourceRange range = manager.MakeRange(
			parsedFile, segment.beginOffset, segment.endOffset);
		if( !range.IsValid() )
		{
			AddDiagnostic("namespace-source-range-invalid");
			lastActedDecl = asASTDeclId();
			return asASTDeclId();
		}
		parent = ActOnStartNamespaceDecl(parent, segment.name.AddressOf(), range);
		if( !parent.IsValid() )
		{
			lastActedDecl = asASTDeclId();
			return asASTDeclId();
		}
	}
	return parent;
}

asASTDeclId asCSema::ActOnClassDecl(asASTDeclId parent, const char* name, const asCSourceRange& range)
{
	const asASTDeclId id = context.CreateDecl(asAST_DECL_CLASS, parent, range, name);
	FinishDecl(id);
	InsertSymbol(id);
	return id;
}

asASTDeclId asCSema::ActOnEnumDecl(asASTDeclId parent, const char* name, const asCSourceRange& range)
{
	const asASTDeclId id = context.CreateDecl(asAST_DECL_ENUM, parent, range, name);
	FinishDecl(id);
	if( const asCDecl* const decl = context.GetDecl(id) )
	{
		const char* const typeKey = decl->stableKey.GetLength() != 0
			? decl->stableKey.AddressOf()
			: decl->name.AddressOf();
		context.SetDeclType(id,
			context.InternNamedType(asAST_TYPE_ENUM, typeKey, 0));
	}
	InsertSymbol(id);
	return id;
}

asASTDeclId asCSema::ActOnEnumName(
	const asSEnumDeclActionName& action,
	asCScriptCode* script)
{
	lastActedDecl = asASTDeclId();
	if( action.name.GetLength() == 0 || action.endOffset <= action.beginOffset )
	{
		AddDiagnostic("enum-action-invalid");
		return asASTDeclId();
	}
	if( !PrepareParsedSource(script) )
	{
		return asASTDeclId();
	}

	const char* const sectionName = script && script->name.GetLength()
		? script->name.AddressOf() : "<memory>";
	asASTDeclId parent = CurrentDeclContext();
	if( !parent.IsValid() )
	{
		parent = context.GetTranslationUnit();
		if( !parent.IsValid() )
		{
			parent = ActOnTranslationUnit(sectionName);
		}
	}

	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("enum-source-range-invalid");
		return asASTDeclId();
	}
	return ActOnStartEnumDecl(parent, action.name.AddressOf(), range);
}

asASTDeclId asCSema::ActOnStartNamespaceDecl(asASTDeclId parent, const char* name, const asCSourceRange& range)
{
	asASTDeclId id = FindExistingNamedDecl(this, parent, name, asAST_DECL_NAMESPACE);
	if( !id.IsValid() )
	{
		id = ActOnNamespaceDecl(parent, name, range);
	}
	NoteActedDecl(id);
	return id;
}

asASTDeclId asCSema::ActOnStartClassDecl(asASTDeclId parent, const char* name, const asCSourceRange& range)
{
	asASTDeclId id = FindExistingNamedDecl(this, parent, name, asAST_DECL_CLASS);
	if( !id.IsValid() )
	{
		id = ActOnClassDecl(parent, name, range);
	}
	else if( const asCDecl* const existing = context.GetDecl(id) )
	{
		// Runtime preparation can create a native-view shell before the parser
		// reaches a later current-module class declaration.  The first valid
		// source spelling promotes that same stable Decl identity to authored
		// script ownership.  Leaving the native origin behind would let lazy
		// Runtime callable projection append ABI-normalized constructors beside
		// the authored declarations during finalization.
		if( range.IsValid()
			&& existing->origin.Equals(CANONICAL_NATIVE_TYPE_VIEW_ORIGIN) )
		{
			context.SetOrigin(id, "");
		}
		// Type/layout preparation may publish the script class identity before
		// the parser reaches its authored declaration. Preserve that identity,
		// but let the first real spelling become the source authority. Generated
		// lifecycle declarations inherit this range, so leaving the early shell
		// range-less would make their Runtime debug section (and consequently
		// StaticJIT/Cache function-fact capture) permanently unmappable.
		if( !existing->range.IsValid() && range.IsValid() )
		{
			context.SetDeclRange(id, range);
			for( asUINT childIndex = 0;
				childIndex < existing->children.GetLength(); ++childIndex )
			{
				const asCDecl* const child =
					context.GetDecl(existing->children[childIndex]);
				if( child != 0
					&& (child->traits & asAST_TRAIT_GENERATED) != 0
					&& (child->kind == asAST_DECL_CONSTRUCTOR
						|| child->kind == asAST_DECL_DESTRUCTOR)
					&& !child->range.IsValid() )
				{
					context.SetDeclRange(child->id, range);
				}
			}
		}
	}
	NoteActedDecl(id);
	return id;
}

asASTDeclId asCSema::ActOnStartEnumDecl(asASTDeclId parent, const char* name, const asCSourceRange& range)
{
	asASTDeclId id = FindExistingNamedDecl(this, parent, name, asAST_DECL_ENUM);
	if( !id.IsValid() )
	{
		id = ActOnEnumDecl(parent, name, range);
	}
	else if( const asCDecl* const existing = context.GetDecl(id) )
	{
		// Stage 2 can create the Runtime enum shell before the parser reaches the
		// authored declaration. Replaying that shell must complete the same
		// canonical nominal type invariant as ActOnEnumDecl(); otherwise every
		// enumerator correctly inherits an invalid owner QualType and prepared
		// publication is forced to rediscover the enum through Runtime state.
		if( !existing->type.IsValid() )
		{
			const char* const typeKey = existing->stableKey.GetLength() != 0
				? existing->stableKey.AddressOf()
				: existing->name.AddressOf();
			context.SetDeclType(id,
				context.InternNamedType(asAST_TYPE_ENUM, typeKey, 0));
		}
	}
	NoteActedDecl(id);
	return id;
}

asASTDeclId asCSema::ActOnStartInterfaceDecl(asASTDeclId parent, const char* name, const asCSourceRange& range)
{
	asASTDeclId id = FindExistingNamedDecl(this, parent, name, asAST_DECL_INTERFACE);
	if( !id.IsValid() )
	{
		id = ActOnInterfaceDecl(parent, name, range);
	}
	NoteActedDecl(id);
	return id;
}

asASTDeclId asCSema::ActOnRecordHeaderAction(
	const asSRecordHeaderAction& action,
	asCScriptCode* script)
{
	lastActedDecl = asASTDeclId();
	if( action.name.GetLength() == 0
		|| action.endOffset < action.beginOffset )
	{
		AddDiagnostic("record-header-action-invalid");
		return asASTDeclId();
	}
	if( action.kind != asRECORD_ACTION_CLASS
		&& action.kind != asRECORD_ACTION_STRUCT
		&& action.kind != asRECORD_ACTION_INTERFACE )
	{
		AddDiagnostic("record-header-kind-invalid");
		return asASTDeclId();
	}
	if( !PrepareParsedSource(script) )
	{
		return asASTDeclId();
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("record-header-source-range-invalid");
		return asASTDeclId();
	}

	asASTDeclId parent = CurrentDeclContext();
	if( !parent.IsValid() )
	{
		parent = context.GetTranslationUnit();
		if( !parent.IsValid() )
		{
			parent = ActOnTranslationUnit(
				script && script->name.GetLength()
					? script->name.AddressOf() : "<memory>");
		}
	}

	const bool isInterface = action.kind == asRECORD_ACTION_INTERFACE;
	const bool isValue = action.kind == asRECORD_ACTION_STRUCT;
	const asASTDeclId record = isInterface
		? ActOnStartInterfaceDecl(parent, action.name.AddressOf(), range)
		: ActOnStartClassDecl(parent, action.name.AddressOf(), range);
	if( !record.IsValid() )
	{
		return record;
	}
	if( isValue )
	{
		context.AddDeclTrait(record, asAST_TRAIT_VALUE);
	}
	const asCDecl* const declaration = context.GetDecl(record);
	const char* const stableKey = declaration != 0
		&& declaration->stableKey.GetLength() != 0
		? declaration->stableKey.AddressOf()
		: action.name.AddressOf();
	context.SetDeclType(record, context.InternNamedType(
		isValue ? asAST_TYPE_VALUE_OBJECT : asAST_TYPE_REFERENCE_OBJECT,
		stableKey,
		isValue ? 0 : asAST_QUAL_HANDLE));
	NoteActedDecl(record);
	return record;
}

void asCSema::ActOnRecordFinishAction(asASTDeclId recordDecl)
{
	const asCDecl* const declaration = context.GetDecl(recordDecl);
	if( !IsRecordDeclaration(declaration) )
	{
		AddDiagnostic("record-finish-owner-invalid");
		return;
	}
	FinalizeRecordMethodRelations(this, recordDecl);
	if( declaration->kind == asAST_DECL_CLASS )
	{
		EnsureGeneratedLifecycle(this, recordDecl);
		EnsureGeneratedAccessors(this, recordDecl);
	}
	NoteActedDecl(recordDecl);
}

asASTDeclId asCSema::ActOnClassDefaultStartAction(
	asASTDeclId ownerClass,
	asCScriptCode* script,
	asUINT beginOffset,
	asUINT endOffset)
{
	lastActedDecl = asASTDeclId();
	if( endOffset <= beginOffset || !PrepareParsedSource(script) )
	{
		AddDiagnostic("class-default-start-action-invalid");
		return asASTDeclId();
	}
	const asCDecl* const owner = context.GetDecl(ownerClass);
	if( owner == 0 || owner->kind != asAST_DECL_CLASS )
	{
		AddDiagnostic("class-default-owner-invalid");
		return asASTDeclId();
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, beginOffset, endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("class-default-source-range-invalid");
		return asASTDeclId();
	}

	asASTDeclId initDefaults;
	for( asUINT childIndex = 0;
		childIndex < owner->children.GetLength(); ++childIndex )
	{
		const asCDecl* const child = context.GetDecl(
			owner->children[childIndex]);
		if( child == 0 || child->kind != asAST_DECL_METHOD
			|| !child->name.Equals("__InitDefaults")
			|| !child->origin.Equals("canonical-init-defaults") )
		{
			continue;
		}
		if( initDefaults.IsValid() )
		{
			AddDiagnostic("class-default-generated-method-ambiguous", range);
			return asASTDeclId();
		}
		initDefaults = child->id;
	}
	if( !initDefaults.IsValid() )
	{
		initDefaults = ActOnMethodDecl(
			ownerClass,
			"__InitDefaults",
			context.InternPrimitive(ttVoid, 0),
			range);
		if( !initDefaults.IsValid() )
		{
			AddDiagnostic("class-default-generated-method-invalid", range);
			return asASTDeclId();
		}
		context.AddDeclTrait(initDefaults, asAST_TRAIT_GENERATED);
		context.SetOrigin(initDefaults, "canonical-init-defaults");
	}
	else if( const asCDecl* const current = context.GetDecl(initDefaults) )
	{
		asCSourceRange combined = current->range;
		if( combined.begin.fileID == range.begin.fileID
			&& range.end.offset > combined.end.offset )
		{
			combined.end = range.end;
			context.SetDeclRange(initDefaults, combined);
		}
	}
	NoteActedDecl(initDefaults);
	return initDefaults;
}

asASTStmtId asCSema::ActOnClassDefaultStatementAction(
	asASTDeclId initDefaults,
	asCScriptCode* script,
	asUINT beginOffset,
	asUINT endOffset)
{
	if( endOffset <= beginOffset || !PrepareParsedSource(script) )
	{
		AddDiagnostic("class-default-statement-action-invalid");
		return asASTStmtId();
	}
	const asCDecl* method = context.GetDecl(initDefaults);
	const asCDecl* const owner = method != 0
		? context.GetDecl(method->parent) : 0;
	if( method == 0 || method->kind != asAST_DECL_METHOD
		|| !method->name.Equals("__InitDefaults")
		|| !method->origin.Equals("canonical-init-defaults")
		|| owner == 0 || owner->kind != asAST_DECL_CLASS )
	{
		AddDiagnostic("class-default-generated-method-invalid");
		return asASTStmtId();
	}
	const asCSourceRange statementRange = context.GetSourceManager().MakeRange(
		parsedFile, beginOffset, endOffset);
	if( !statementRange.IsValid() )
	{
		AddDiagnostic("class-default-statement-source-range-invalid");
		return asASTStmtId();
	}

	asASTStmtId statement;
	for( asUINT statementIndex = 1;
		statementIndex <= context.GetStmtCount(); ++statementIndex )
	{
		const asCStmt* const candidate = context.GetStmt(
			asASTStmtId(statementIndex));
		if( candidate == 0 || candidate->owner != initDefaults
			|| !(candidate->range == statementRange) )
		{
			continue;
		}
		if( statement.IsValid() )
		{
			AddDiagnostic(
				"class-default-statement-action-ambiguous", statementRange);
			return asASTStmtId();
		}
		statement = candidate->id;
	}
	if( !statement.IsValid() )
	{
		AddDiagnostic("class-default-statement-action-missing", statementRange);
		return asASTStmtId();
	}

	asCSourceRange combined = method->range;
	if( combined.begin.fileID == statementRange.begin.fileID
		&& statementRange.end.offset > combined.end.offset )
	{
		combined.end = statementRange.end;
		context.SetDeclRange(initDefaults, combined);
	}
	method = context.GetDecl(initDefaults);
	if( method == 0 )
	{
		AddDiagnostic("class-default-generated-method-invalid", statementRange);
		return asASTStmtId();
	}
	if( !method->body.IsValid() )
	{
		asCArray<asASTStmtId> statements;
		statements.PushLast(statement);
		const asASTStmtId body = ActOnBlock(
			initDefaults, statements, combined);
		if( !body.IsValid() || context.SetBody(initDefaults, body) != asAST_VERIFY_OK )
		{
			AddDiagnostic("class-default-body-invalid", combined);
			return asASTStmtId();
		}
	}
	else
	{
		const asCStmt* const body = context.GetStmt(method->body);
		if( body == 0 || body->kind != asAST_STMT_BLOCK
			|| body->owner != initDefaults )
		{
			AddDiagnostic("class-default-body-invalid", statementRange);
			return asASTStmtId();
		}
		bool alreadyAttached = false;
		for( asUINT childIndex = 0;
			childIndex < body->children.GetLength(); ++childIndex )
		{
			if( body->children[childIndex] == statement )
			{
				alreadyAttached = true;
				break;
			}
		}
		if( !alreadyAttached
			&& context.AddStmtChild(method->body, statement) != asAST_VERIFY_OK )
		{
			AddDiagnostic("class-default-body-invalid", statementRange);
			return asASTStmtId();
		}
	}
	FinishDecl(initDefaults);
	NoteActedDecl(initDefaults);
	return statement;
}

asASTDeclId asCSema::ActOnStartTypedefDecl(asASTDeclId parent, const char* name, const asCSourceRange& range)
{
	asASTDeclId id = FindExistingNamedDecl(this, parent, name, asAST_DECL_TYPEDEF);
	if( !id.IsValid() )
	{
		id = ActOnTypedefDecl(parent, name, range);
	}
	NoteActedDecl(id);
	return id;
}

asASTDeclId asCSema::ActOnTypedefAction(
	const asSTypedefDeclAction& action,
	asCScriptCode* script)
{
	lastActedDecl = asASTDeclId();
	const eTokenType primitive = CanonicalPrimitiveToken(action.primitiveToken);
	if( action.name.GetLength() == 0
		|| action.endOffset <= action.beginOffset
		|| primitive == ttUnrecognizedToken
		|| primitive == ttVoid )
	{
		AddDiagnostic("typedef-action-invalid");
		return asASTDeclId();
	}
	if( !PrepareParsedSource(script) )
	{
		return asASTDeclId();
	}

	const char* const sectionName = script && script->name.GetLength()
		? script->name.AddressOf() : "<memory>";
	asASTDeclId parent = CurrentDeclContext();
	if( !parent.IsValid() )
	{
		parent = context.GetTranslationUnit();
		if( !parent.IsValid() )
		{
			parent = ActOnTranslationUnit(sectionName);
		}
	}

	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("typedef-source-range-invalid");
		return asASTDeclId();
	}

	const asASTDeclId alias = ActOnStartTypedefDecl(
		parent, action.name.AddressOf(), range);
	if( alias.IsValid() )
	{
		context.SetDeclType(alias, context.InternPrimitive(primitive, 0));
	}
	return alias;
}

asASTDeclId asCSema::ActOnStartImportDecl(asASTDeclId parent, const char* name, const asCSourceRange& range)
{
	asASTDeclId id = FindExistingNamedDecl(this, parent, name, asAST_DECL_IMPORT);
	if( !id.IsValid() )
	{
		id = ActOnImportDecl(parent, name, range);
	}
	NoteActedDecl(id);
	return id;
}

asASTDeclId asCSema::ActOnStartFuncDefDecl(
	asASTDeclId parent, const char* name, const asCSourceRange& range)
{
	asASTDeclId id = FindExistingNamedDecl(
		this, parent, name, asAST_DECL_FUNCDEF);
	if( !id.IsValid() )
	{
		id = ActOnFuncDefDecl(parent, name, range);
	}
	NoteActedDecl(id);
	return id;
}

asASTDeclId asCSema::ActOnFuncDefSignatureAction(
	const asSFuncDefSignatureAction& action,
	asCScriptCode* script)
{
	lastActedDecl = asASTDeclId();
	if( action.name.GetLength() == 0
		|| !action.returnType.IsValid()
		|| action.endOffset <= action.beginOffset
		|| !PrepareParsedSource(script) )
	{
		AddDiagnostic("funcdef-signature-action-invalid");
		return asASTDeclId();
	}

	const char* const sectionName = script && script->name.GetLength()
		? script->name.AddressOf() : "<memory>";
	asASTDeclId parent = CurrentDeclContext();
	if( !parent.IsValid() )
	{
		parent = context.GetTranslationUnit();
		if( !parent.IsValid() )
		{
			parent = ActOnTranslationUnit(sectionName);
		}
	}

	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("funcdef-signature-source-range-invalid");
		return asASTDeclId();
	}

	const asASTDeclId funcDef = ActOnStartFuncDefDecl(
		parent, action.name.AddressOf(), range);
	if( !funcDef.IsValid() )
	{
		AddDiagnostic("funcdef-signature-action-invalid", range);
		return asASTDeclId();
	}
	context.SetDeclType(funcDef, action.returnType);
	if( const asCType* const named = context.GetType(action.returnType.type) )
	{
		if( named->kind != asAST_TYPE_PRIMITIVE
			&& named->kind != asAST_TYPE_VOID
			&& named->stableKey.GetLength() != 0 )
		{
			RecordDependency(funcDef, named->stableKey.AddressOf());
		}
	}
	FinishDecl(funcDef);
	NoteActedDecl(funcDef);
	return funcDef;
}

asASTDeclId asCSema::ActOnFunctionSignatureAction(
	const asSFunctionSignatureAction& action,
	asCScriptCode* script)
{
	if( action.name.GetLength() == 0
		|| action.endOffset <= action.beginOffset
		|| action.kind < asFUNCTION_ACTION_FUNCTION
		|| action.kind > asFUNCTION_ACTION_DESTRUCTOR
		|| !PrepareParsedSource(script) )
	{
		AddDiagnostic("function-signature-action-invalid");
		return asASTDeclId();
	}

	const char* const sectionName = script && script->name.GetLength()
		? script->name.AddressOf() : "<memory>";
	asASTDeclId parent = CurrentDeclContext();
	if( !parent.IsValid() )
	{
		parent = context.GetTranslationUnit();
		if( !parent.IsValid() )
		{
			parent = ActOnTranslationUnit(sectionName);
		}
	}
	const asCDecl* const owner = context.GetDecl(parent);
	const bool ownerIsType = owner
		&& (owner->kind == asAST_DECL_CLASS
			|| owner->kind == asAST_DECL_INTERFACE);
	const bool isConstructor = action.kind == asFUNCTION_ACTION_METHOD
		&& ownerIsType && action.name.Equals(owner->name);
	if( !action.returnType.IsValid()
		&& !isConstructor
		&& action.kind != asFUNCTION_ACTION_DESTRUCTOR )
	{
		AddDiagnostic("function-signature-return-type-invalid");
		return asASTDeclId();
	}

	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("function-signature-source-range-invalid");
		return asASTDeclId();
	}

	asASTDeclId functionDecl;
	if( action.kind == asFUNCTION_ACTION_MIXIN )
	{
		functionDecl = ActOnStartMixinDecl(
			parent, action.name.AddressOf(), range);
		if( functionDecl.IsValid() )
		{
			context.SetDeclType(functionDecl, action.returnType);
			context.AddDeclTrait(functionDecl, asAST_TRAIT_MIXIN);
		}
	}
	else if( action.kind == asFUNCTION_ACTION_DESTRUCTOR )
	{
		functionDecl = ActOnStartDestructorDecl(
			parent, action.name.AddressOf(), range);
	}
	else if( isConstructor )
	{
		functionDecl = ActOnStartConstructorDecl(
			parent, action.name.AddressOf(), range);
	}
	else if( action.kind == asFUNCTION_ACTION_METHOD )
	{
		functionDecl = ActOnStartMethodDecl(
			parent, action.name.AddressOf(), action.returnType, range);
	}
	else
	{
		functionDecl = ActOnStartFunctionDecl(
			parent, action.name.AddressOf(), action.returnType, range);
	}

	if( functionDecl.IsValid() && action.returnType.IsValid() )
	{
		if( const asCType* const named = context.GetType(action.returnType.type) )
		{
			if( named->kind != asAST_TYPE_PRIMITIVE
				&& named->kind != asAST_TYPE_VOID
				&& named->stableKey.GetLength() != 0 )
			{
				RecordDependency(functionDecl, named->stableKey.AddressOf());
			}
		}
	}
	if( functionDecl.IsValid() && action.accessSpecifierName.GetLength() != 0 )
	{
		ActOnDeclAccessSpecifierAction(
			functionDecl, action.accessSpecifierName.AddressOf());
	}
	NoteActedDecl(functionDecl);
	return functionDecl;
}

void asCSema::ActOnFunctionTraitsAction(
	asASTDeclId functionDecl, asDWORD traits)
{
	const asDWORD allowedTraits = asAST_TRAIT_PRIVATE
		| asAST_TRAIT_PROTECTED
		| asAST_TRAIT_CONST_METHOD
		| asAST_TRAIT_OVERRIDE
		| asAST_TRAIT_FINAL
		| asAST_TRAIT_MIXIN
		| asAST_TRAIT_GENERATED
		| asAST_TRAIT_EXTERNAL_IMPLICIT_THIS;
	const asCDecl* const declaration = context.GetDecl(functionDecl);
	if( declaration == 0
		|| (declaration->kind != asAST_DECL_FUNCTION
			&& declaration->kind != asAST_DECL_METHOD
			&& declaration->kind != asAST_DECL_CONSTRUCTOR
			&& declaration->kind != asAST_DECL_DESTRUCTOR
			&& declaration->kind != asAST_DECL_MIXIN)
		|| (traits & ~allowedTraits) != 0 )
	{
		AddDiagnostic("function-traits-action-invalid");
		return;
	}
	context.AddDeclTrait(functionDecl, traits);
	ApplyEditorOnlyTraitFromSource(functionDecl);
	FinishDecl(functionDecl);
	NoteActedDecl(functionDecl);
}

asASTDeclId asCSema::ActOnImportSignatureAction(
	const asSImportSignatureAction& action,
	asCScriptCode* script)
{
	if( action.name.GetLength() == 0
		|| !action.returnType.IsValid()
		|| action.endOffset <= action.beginOffset
		|| !PrepareParsedSource(script) )
	{
		AddDiagnostic("import-signature-action-invalid");
		return asASTDeclId();
	}

	const char* const sectionName = script && script->name.GetLength()
		? script->name.AddressOf() : "<memory>";
	asASTDeclId parent = CurrentDeclContext();
	if( !parent.IsValid() )
	{
		parent = context.GetTranslationUnit();
		if( !parent.IsValid() )
		{
			parent = ActOnTranslationUnit(sectionName);
		}
	}

	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("import-signature-source-range-invalid");
		return asASTDeclId();
	}

	const asASTDeclId importDecl = ActOnStartImportDecl(
		parent, action.name.AddressOf(), range);
	if( importDecl.IsValid() )
	{
		context.SetDeclType(importDecl, action.returnType);
		FinishDecl(importDecl);
		NoteActedDecl(importDecl);
	}
	return importDecl;
}

void asCSema::ActOnImportOriginAction(
	asASTDeclId importDecl,
	const asSImportOriginAction& action,
	asCScriptCode* script)
{
	if( !importDecl.IsValid()
		|| action.origin.GetLength() == 0
		|| action.endOffset <= action.beginOffset
		|| !PrepareParsedSource(script) )
	{
		AddDiagnostic("import-origin-action-invalid");
		return;
	}
	const asCDecl* const declaration = context.GetDecl(importDecl);
	if( declaration == 0 || declaration->kind != asAST_DECL_IMPORT )
	{
		AddDiagnostic("import-origin-owner-invalid");
		return;
	}
	const asCSourceRange originRange = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !originRange.IsValid() )
	{
		AddDiagnostic("import-origin-source-range-invalid");
		return;
	}
	context.SetOrigin(importDecl, action.origin.AddressOf());
	RecordDependency(importDecl, action.origin.AddressOf());
	FinishDecl(importDecl);
	NoteActedDecl(importDecl);
}

asASTDeclId asCSema::ActOnAccessSpecifierAction(
	const asSAccessSpecifierAction& action,
	asCScriptCode* script)
{
	lastActedDecl = asASTDeclId();
	const asDWORD baseMask = asAST_TRAIT_PRIVATE | asAST_TRAIT_PROTECTED;
	const asDWORD baseTraits = action.traits & baseMask;
	if( action.name.GetLength() == 0
		|| action.endOffset <= action.beginOffset
		|| (action.traits & ~baseMask) != 0
		|| (baseTraits != asAST_TRAIT_PRIVATE
			&& baseTraits != asAST_TRAIT_PROTECTED)
		|| !PrepareParsedSource(script) )
	{
		AddDiagnostic("access-specifier-action-invalid");
		return asASTDeclId();
	}

	asASTDeclId parent = CurrentDeclContext();
	const asCDecl* const owner = context.GetDecl(parent);
	if( owner == 0 || (owner->kind != asAST_DECL_CLASS
		&& owner->kind != asAST_DECL_INTERFACE
		&& owner->kind != asAST_DECL_MIXIN) )
	{
		AddDiagnostic("access-specifier-owner-invalid");
		return asASTDeclId();
	}

	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("access-specifier-source-range-invalid");
		return asASTDeclId();
	}
	if( FindExistingNamedDecl(
		this, parent, action.name.AddressOf(),
		asAST_DECL_ACCESS_SPECIFIER).IsValid() )
	{
		AddDiagnostic("access-specifier-already-declared", range);
		return asASTDeclId();
	}

	const asDWORD permissionMask = asAST_TRAIT_ACCESS_WILDCARD
		| asAST_TRAIT_ACCESS_READONLY
		| asAST_TRAIT_ACCESS_EDITDEFAULTS
		| asAST_TRAIT_ACCESS_INHERITED;
	for( asUINT i = 0; i < action.permissions.GetLength(); ++i )
	{
		const asSAccessPermissionAction& permission = action.permissions[i];
		const bool wildcard = permission.name.Equals("*");
		const asCSourceRange permissionRange =
			context.GetSourceManager().MakeRange(
				parsedFile, permission.beginOffset, permission.endOffset);
		if( permission.name.GetLength() == 0
			|| permission.endOffset <= permission.beginOffset
			|| !permissionRange.IsValid()
			|| (permission.traits & ~permissionMask) != 0
			|| (((permission.traits & asAST_TRAIT_ACCESS_WILDCARD) != 0)
				!= wildcard)
			|| (wildcard
				&& (permission.traits & asAST_TRAIT_ACCESS_INHERITED) != 0) )
		{
			AddDiagnostic("access-permission-action-invalid", permissionRange);
			return asASTDeclId();
		}
	}

	const asASTDeclId specifier = context.CreateDecl(
		asAST_DECL_ACCESS_SPECIFIER, parent, range, action.name.AddressOf());
	if( !specifier.IsValid()
		|| context.SetDeclTraits(specifier, action.traits) != asAST_VERIFY_OK )
	{
		AddDiagnostic("access-specifier-action-invalid", range);
		return asASTDeclId();
	}
	FinishDecl(specifier);
	for( asUINT i = 0; i < action.permissions.GetLength(); ++i )
	{
		const asSAccessPermissionAction& permission = action.permissions[i];
		const asCSourceRange permissionRange =
			context.GetSourceManager().MakeRange(
				parsedFile, permission.beginOffset, permission.endOffset);
		const asASTDeclId permissionDecl = context.CreateDecl(
			asAST_DECL_ACCESS_PERMISSION, specifier, permissionRange,
			permission.name.AddressOf());
		if( !permissionDecl.IsValid()
			|| context.SetDeclTraits(
				permissionDecl, permission.traits) != asAST_VERIFY_OK )
		{
			AddDiagnostic("access-permission-action-invalid", permissionRange);
			return asASTDeclId();
		}
		FinishDecl(permissionDecl);
	}
	NoteActedDecl(specifier);
	return specifier;
}

asASTDeclId asCSema::ActOnDeclAccessSpecifierAction(
	asASTDeclId member,
	const char* accessSpecifierName)
{
	const asCDecl* const memberDecl = context.GetDecl(member);
	if( memberDecl == 0 || accessSpecifierName == 0
		|| accessSpecifierName[0] == 0 )
	{
		AddDiagnostic("decl-access-specifier-action-invalid");
		return asASTDeclId();
	}
	const bool memberKind = memberDecl->kind == asAST_DECL_VAR
		|| memberDecl->kind == asAST_DECL_PROPERTY
		|| memberDecl->kind == asAST_DECL_METHOD
		|| memberDecl->kind == asAST_DECL_CONSTRUCTOR
		|| memberDecl->kind == asAST_DECL_DESTRUCTOR;
	const asCDecl* const owner = context.GetDecl(memberDecl->parent);
	if( !memberKind || owner == 0 || (owner->kind != asAST_DECL_CLASS
		&& owner->kind != asAST_DECL_INTERFACE
		&& owner->kind != asAST_DECL_MIXIN) )
	{
		AddDiagnostic("decl-access-specifier-owner-invalid", memberDecl->range);
		return asASTDeclId();
	}

	asASTDeclId resolved;
	asUINT matches = 0;
	for( asUINT i = 0; i < owner->children.GetLength(); ++i )
	{
		const asCDecl* const child = context.GetDecl(owner->children[i]);
		if( child != 0 && child->kind == asAST_DECL_ACCESS_SPECIFIER
			&& child->name.Equals(accessSpecifierName) )
		{
			resolved = child->id;
			++matches;
		}
	}
	if( matches != 1 )
	{
		AddDiagnostic(matches == 0
			? "decl-access-specifier-not-found"
			: "decl-access-specifier-ambiguous", memberDecl->range);
		return asASTDeclId();
	}
	if( context.SetDeclAccessSpecifier(member, resolved) != asAST_VERIFY_OK )
	{
		AddDiagnostic("decl-access-specifier-action-invalid", memberDecl->range);
		return asASTDeclId();
	}
	return resolved;
}

asASTDeclId asCSema::ActOnImportDecl(asASTDeclId parent, const char* name, const asCSourceRange& range)
{
	const asASTDeclId id = context.CreateDecl(asAST_DECL_IMPORT, parent, range, name);
	FinishDecl(id);
	InsertSymbol(id);
	return id;
}

asASTDeclId asCSema::ActOnVarDecl(asASTDeclId parent, const char* name, const asCQualType& type, const asCSourceRange& range)
{
	const asASTDeclId id = context.CreateDecl(asAST_DECL_VAR, parent, range, name);
	context.SetDeclType(id, type);
	FinishDecl(id);
	InsertSymbol(id);
	return id;
}

asASTDeclId asCSema::ActOnStartVarDecl(asASTDeclId parent, const char* name, const asCQualType& type, const asCSourceRange& range)
{
	asASTDeclId id = FindExistingNamedDecl(this, parent, name, asAST_DECL_VAR);
	if( !id.IsValid() )
	{
		id = ActOnVarDecl(parent, name, type, range);
	}
	NoteActedDecl(id);
	return id;
}

asASTDeclId asCSema::ActOnVariableHeaderAction(
	const asSVariableHeaderAction& action,
	asCScriptCode* script)
{
	lastActedDecl = asASTDeclId();
	const asDWORD allowedTraits = asAST_TRAIT_PRIVATE
		| asAST_TRAIT_PROTECTED;
	if( action.name.GetLength() == 0
		|| !action.type.IsValid()
		|| action.endOffset <= action.beginOffset
		|| (action.traits & ~allowedTraits) != 0
		|| !PrepareParsedSource(script) )
	{
		AddDiagnostic("variable-header-action-invalid");
		return asASTDeclId();
	}

	asASTDeclId parent = CurrentDeclContext();
	if( !parent.IsValid() )
	{
		parent = context.GetTranslationUnit();
		if( !parent.IsValid() )
		{
			parent = ActOnTranslationUnit(
				script && script->name.GetLength()
					? script->name.AddressOf() : "<memory>");
		}
	}
	const asCDecl* const parentDecl = context.GetDecl(parent);
	const bool globalOrField = IsGlobalOrFieldVariableParent(parentDecl);
	const bool local = parentDecl != 0
		&& IsFunctionLikeDeclKind(parentDecl->kind);
	if( !globalOrField && !local )
	{
		AddDiagnostic("variable-header-owner-invalid");
		return asASTDeclId();
	}
	if( action.traits != 0
		&& (parentDecl == 0 || parentDecl->kind != asAST_DECL_CLASS) )
	{
		AddDiagnostic("variable-header-traits-invalid");
		return asASTDeclId();
	}

	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("variable-header-source-range-invalid");
		return asASTDeclId();
	}

	asASTDeclId variable;
	if( local )
	{
		// Local declarations share a function DeclContext even when lexical
		// blocks shadow the same name. Never use name-only interning here: exact
		// source identity distinguishes those declarations before scope replay.
		if( parentDecl != 0 )
		{
			for( asUINT childIndex = 0;
				childIndex < parentDecl->children.GetLength(); ++childIndex )
			{
				const asCDecl* const child = context.GetDecl(
					parentDecl->children[childIndex]);
				if( child != 0 && child->kind == asAST_DECL_VAR
					&& child->name.Equals(action.name)
					&& child->range == range )
				{
					variable = child->id;
					break;
				}
			}
		}
		if( !variable.IsValid() )
		{
			variable = ActOnVarDecl(
				parent, action.name.AddressOf(), action.type, range);
		}
	}
	else
	{
		variable = ActOnStartVarDecl(
			parent, action.name.AddressOf(), action.type, range);
	}
	if( !variable.IsValid() )
	{
		return variable;
	}
	context.AddDeclTrait(variable, action.traits);
	if( action.accessSpecifierName.GetLength() != 0 )
	{
		ActOnDeclAccessSpecifierAction(
			variable, action.accessSpecifierName.AddressOf());
	}
	if( const asCType* const named = context.GetType(action.type.type) )
	{
		if( named->kind != asAST_TYPE_PRIMITIVE
			&& named->kind != asAST_TYPE_VOID
			&& !named->stableKey.Equals("auto")
			&& named->stableKey.GetLength() != 0 )
		{
			RecordDependency(variable, named->stableKey.AddressOf());
		}
	}
	NoteActedDecl(variable);
	return variable;
}

bool asCSema::ActOnVariableInitializerAction(
	const asSVariableInitializerAction& action,
	asCScriptCode* script)
{
	if( !action.variable.IsValid()
		|| action.kind <= asVARIABLE_INITIALIZER_INVALID
		|| action.kind > asVARIABLE_INITIALIZER_DIRECT_CONSTRUCT
		|| action.endOffset <= action.beginOffset
		|| !PrepareParsedSource(script) )
	{
		AddDiagnostic("variable-initializer-action-invalid");
		return false;
	}
	const asCDecl* const variable = context.GetDecl(action.variable);
	const asCDecl* const parent = variable != 0
		? context.GetDecl(variable->parent) : 0;
	if( variable == 0 || variable->kind != asAST_DECL_VAR
		|| !IsGlobalOrFieldVariableParent(parent) )
	{
		AddDiagnostic("variable-initializer-owner-invalid");
		return false;
	}
	const asCSourceRange range = context.GetSourceManager().MakeRange(
		parsedFile, action.beginOffset, action.endOffset);
	if( !range.IsValid() )
	{
		AddDiagnostic("variable-initializer-source-range-invalid");
		return false;
	}
	auto commitVariableType = [this, &action, &range](
		const asCQualType& type) -> bool
	{
		const asCDecl* const current = context.GetDecl(action.variable);
		if( current == 0 )
		{
			return false;
		}
		if( current->type != type
			&& context.SetDeclType(action.variable, type) != asAST_VERIFY_OK )
		{
			AddDiagnostic("variable-auto-type-commit-invalid", range);
			return false;
		}
		const asCType* const named = context.GetType(type.type);
		if( named != 0 && named->kind != asAST_TYPE_PRIMITIVE
			&& named->kind != asAST_TYPE_VOID
			&& !named->stableKey.Equals("auto")
			&& named->stableKey.GetLength() != 0 )
		{
			RecordDependency(action.variable, named->stableKey.AddressOf());
		}
		return true;
	};

	if( action.kind == asVARIABLE_INITIALIZER_NONE )
	{
		if( action.expression.IsValid() || action.arguments.GetLength() != 0
			|| action.argumentNames.GetLength() != 0
			|| variable->inits.GetLength() != 0 )
		{
			AddDiagnostic("variable-initializer-action-invalid", range);
			return false;
		}
		asCQualType committedType;
		if( !InferCanonicalAutoType(
			variable->type, asASTExprId(), false,
			action.autoConstRef, committedType) )
		{
			AddDiagnostic("variable-auto-inference-invalid", range);
			return false;
		}
		if( !commitVariableType(committedType) )
		{
			return false;
		}
		if( parent->kind == asAST_DECL_TRANSLATION_UNIT
			|| parent->kind == asAST_DECL_NAMESPACE )
		{
			if( IsCanonicalNonPrimitiveValueType(committedType) )
			{
				context.AddDeclTrait(
					action.variable, asAST_TRAIT_DEFAULT_INITIALIZED);
			}
		}
		NoteActedDecl(action.variable);
		return true;
	}

	asASTExprId initExpr;
	if( action.kind == asVARIABLE_INITIALIZER_EXPRESSION )
	{
		if( !action.expression.IsValid()
			|| action.arguments.GetLength() != 0
			|| action.argumentNames.GetLength() != 0
			|| context.GetExpr(action.expression) == 0 )
		{
			AddDiagnostic("variable-initializer-expression-invalid", range);
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
			AddDiagnostic("variable-initializer-construct-invalid", range);
			return false;
		}
		for( asUINT i = 0; i < action.arguments.GetLength(); ++i )
		{
			if( !action.arguments[i].IsValid()
				|| context.GetExpr(action.arguments[i]) == 0 )
			{
				AddDiagnostic("variable-initializer-construct-invalid", range);
				return false;
			}
		}
	}
	asCQualType committedType;
	if( !InferCanonicalAutoType(
		variable->type, initExpr, false,
		action.autoConstRef, committedType) )
	{
		AddDiagnostic("variable-auto-inference-invalid", range);
		return false;
	}
	if( !commitVariableType(committedType) )
	{
		return false;
	}

	if( variable->inits.GetLength() != 0 )
	{
		if( variable->inits.GetLength() == 1 )
		{
			const asCExpr* const existing =
				context.GetExpr(variable->inits[0]);
			if( action.kind == asVARIABLE_INITIALIZER_EXPRESSION
				&& variable->inits[0] == initExpr )
			{
				return true;
			}
			if( action.kind == asVARIABLE_INITIALIZER_DIRECT_CONSTRUCT
				&& existing != 0
				&& existing->kind == asAST_EXPR_CONSTRUCT
				&& existing->type == variable->type
				&& existing->children.GetLength() == action.arguments.GetLength() )
			{
				bool same = true;
				for( asUINT i = 0; i < action.arguments.GetLength(); ++i )
				{
					if( existing->children[i] != action.arguments[i] )
					{
						same = false;
						break;
					}
				}
				if( same )
				{
					return true;
				}
			}
		}
		AddDiagnostic("variable-initializer-already-bound", range);
		return false;
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

	if( parent->kind == asAST_DECL_TRANSLATION_UNIT
		|| parent->kind == asAST_DECL_NAMESPACE )
	{
		// ActOnGlobalVarInit owns constant evaluation and writes the normalized
		// canonical default text. Do not replace `40 + 1` with the first source
		// literal (`40`) after it has sealed the evaluated value (`41`).
		if( !ActOnGlobalVarInit(action.variable, initExpr) )
		{
			NoteActedDecl(action.variable);
			return false;
		}
		context.AddDeclInit(action.variable, initExpr);
	}
	else if( initExpr.IsValid() )
	{
		context.AddDeclInit(action.variable, initExpr);
	}
	NoteActedDecl(action.variable);
	return initExpr.IsValid();
}

asASTDeclId asCSema::ActOnListPatternDecl(asASTDeclId parent, const char* structured, const asCSourceRange& range)
{
	const char* origin = (structured && *structured) ? structured : "list-pattern";
	if( const asCDecl* parentDecl = context.GetDecl(parent) )
	{
		for( asUINT i = 0; i < parentDecl->children.GetLength(); ++i )
		{
			const asCDecl* child = context.GetDecl(parentDecl->children[i]);
			if( child && child->kind == asAST_DECL_VAR
				&& child->name.Equals("list-pattern")
				&& child->origin.Equals(origin) )
			{
				NoteActedDecl(child->id);
				return child->id;
			}
		}
	}

	const asCQualType intType = context.InternPrimitive(ttInt, 0);
	const asASTDeclId id = ActOnVarDecl(parent, "list-pattern", intType, range);
	context.SetOrigin(id, origin);
	context.AddDeclTrait(id, asAST_TRAIT_GENERATED);
	RecordDependency(parent, origin);
	NoteActedDecl(id);
	return id;
}

static asCQualType ClassObjectQualType(asCASTContext& context, const asCDecl* classDecl)
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
	const char* const name = classDecl && classDecl->name.GetLength()
		? classDecl->name.AddressOf()
		: "";
	return context.InternNamedType(kind, name, quals);
}

static const asCDecl* FindDefaultConstructor(const asCASTContext& context, const asCDecl* classDecl)
{
	if( classDecl == 0 )
	{
		return 0;
	}
	const asCDecl* fallback = 0;
	for( asUINT childIndex = 0; childIndex < classDecl->children.GetLength(); ++childIndex )
	{
		const asCDecl* const child = context.GetDecl(classDecl->children[childIndex]);
		if( child == 0 || child->kind != asAST_DECL_CONSTRUCTOR )
		{
			continue;
		}
		asUINT parameterCount = 0;
		for( asUINT nestedIndex = 0; nestedIndex < child->children.GetLength(); ++nestedIndex )
		{
			const asCDecl* const nested = context.GetDecl(child->children[nestedIndex]);
			if( nested && nested->kind == asAST_DECL_PARAM )
			{
				++parameterCount;
			}
		}
		if( parameterCount != 0 )
		{
			continue;
		}
		if( (child->traits & asAST_TRAIT_GENERATED) != 0 )
		{
			return child;
		}
		fallback = child;
	}
	return fallback;
}

static const asCDecl* FindLayoutBaseClass(const asCASTContext& context, const asCDecl* classDecl)
{
	if( classDecl == 0 )
	{
		return 0;
	}
	for( asUINT baseIndex = 0; baseIndex < classDecl->bases.GetLength(); ++baseIndex )
	{
		const asCDecl* const baseDecl = context.GetDecl(classDecl->bases[baseIndex]);
		if( baseDecl && baseDecl->kind == asAST_DECL_CLASS
			&& !baseDecl->origin.Equals("canonical-native-type-view") )
		{
			return baseDecl;
		}
	}
	return 0;
}

static const asCDecl* FindLayoutClassForType(
	const asCASTContext& context, const asCType* canonicalType)
{
	if( canonicalType == 0 || canonicalType->kind != asAST_TYPE_VALUE_OBJECT )
	{
		return 0;
	}
	for( asUINT candidateIndex = 1;
		candidateIndex <= context.GetDeclCount(); ++candidateIndex )
	{
		const asCDecl* const candidate =
			context.GetDecl(asASTDeclId(candidateIndex));
		if( candidate && candidate->kind == asAST_DECL_CLASS
			&& (candidate->stableKey.Equals(canonicalType->stableKey)
				|| candidate->name.Equals(canonicalType->stableKey)) )
		{
			return candidate;
		}
	}
	return 0;
}

static bool IsSameCanonicalValueType(
	const asCASTContext& context,
	const asCQualType& candidateType,
	const asCDecl* classDecl)
{
	const asCType* const candidate = context.GetType(candidateType.type);
	return candidate != 0
		&& candidate->kind == asAST_TYPE_VALUE_OBJECT
		&& classDecl != 0
		&& (candidate->stableKey.Equals(classDecl->stableKey)
			|| candidate->stableKey.Equals(classDecl->name));
}

static bool ClassHasAuthoredNonTrivialCopySemantics(
	const asCASTContext& context, const asCDecl* classDecl)
{
	if( classDecl == 0 )
	{
		return true;
	}
	for( asUINT childIndex = 0;
		childIndex < classDecl->children.GetLength(); ++childIndex )
	{
		const asCDecl* const child = context.GetDecl(classDecl->children[childIndex]);
		if( child == 0 || (child->traits & asAST_TRAIT_GENERATED) != 0 )
		{
			continue;
		}
		if( child->kind == asAST_DECL_DESTRUCTOR
			|| (child->kind == asAST_DECL_METHOD && child->name.Equals("opAssign")) )
		{
			return true;
		}
		if( child->kind != asAST_DECL_CONSTRUCTOR )
		{
			continue;
		}
		const asCDecl* singleParameter = 0;
		asUINT parameterCount = 0;
		for( asUINT parameterIndex = 0;
			parameterIndex < child->children.GetLength(); ++parameterIndex )
		{
			const asCDecl* const parameter =
				context.GetDecl(child->children[parameterIndex]);
			if( parameter && parameter->kind == asAST_DECL_PARAM )
			{
				++parameterCount;
				singleParameter = parameter;
			}
		}
		if( parameterCount == 1 && singleParameter != 0
			&& IsSameCanonicalValueType(context, singleParameter->type, classDecl) )
		{
			return true;
		}
	}
	return false;
}

static bool IsCanonicalNativeTypeView(const asCDecl* classDecl)
{
	return classDecl != 0
		&& classDecl->origin.Equals("canonical-native-type-view");
}

static bool LayoutOneScriptClass(
	asCSema* sema,
	asCRuntimeTypeBridge& bridge,
	asCModule* module,
	asASTDeclId classId,
	asCArray<asBYTE>& states)
{
	asCASTContext& context = sema->GetContext();
	const asCDecl* const classDecl = context.GetDecl(classId);
	if( classDecl == 0 || classDecl->kind != asAST_DECL_CLASS )
	{
		return true;
	}
	if( classId.value >= states.GetLength() )
	{
		sema->AddDiagnostic("class-layout-id-out-of-range", classDecl->range);
		return false;
	}
	if( IsCanonicalNativeTypeView(classDecl) )
	{
		// Native type views already contain host-registered member offsets. Their
		// object extent and alignment are owned by the embedding ABI, not by the
		// script class-layout algorithm. Still seal the current Engine size so
		// Bytecode COPY/read of POD or destructor-free values is authorized from
		// producer facts rather than remaining at the unlaid-out -1/0 sentinel.
		asCScriptEngine* const engine = sema->GetEngine();
		if( engine != 0 && (classDecl->traits & asAST_TRAIT_VALUE) != 0 )
		{
			const asCDataType runtimeType = bridge.Resolve(
				context, classDecl->type.IsValid()
					? classDecl->type
					: context.InternNamedType(
						asAST_TYPE_VALUE_OBJECT,
						classDecl->stableKey.GetLength()
							? classDecl->stableKey.AddressOf()
							: classDecl->name.AddressOf(),
						0));
			asCTypeInfo* const typeInfo = runtimeType.GetTypeInfo();
			if( typeInfo != 0 && (typeInfo->GetFlags() & asOBJ_VALUE) != 0
				&& typeInfo->size > 0 )
			{
				asUINT alignment = runtimeType.GetAlignment();
				if( alignment == 0 )
				{
					alignment = 1;
				}
				context.SetDeclLayout(classDecl->id, typeInfo->size, alignment);
				if( (typeInfo->GetFlags() & asOBJ_POD) != 0
					|| (typeInfo->GetFlags() & asOBJ_APP_CLASS_DESTRUCTOR) == 0 )
				{
					context.AddDeclTrait(
						classDecl->id, asAST_TRAIT_TRIVIAL_STORAGE_COPY);
				}
			}
		}
		states[classId.value] = 2;
		return true;
	}
	if( states[classId.value] == 2 )
	{
		return true;
	}
	if( states[classId.value] == 1 )
	{
		sema->AddDiagnostic("class-layout-inheritance-cycle", classDecl->range);
		return false;
	}
	states[classId.value] = 1;

	int size = 0;
	asUINT alignment = 1;
	bool trivialStorageCopy = (classDecl->traits & asAST_TRAIT_VALUE) != 0
		&& !ClassHasAuthoredNonTrivialCopySemantics(context, classDecl);
	const asCDecl* const baseClass = FindLayoutBaseClass(context, classDecl);
	if( baseClass )
	{
		if( !LayoutOneScriptClass(sema, bridge, module, baseClass->id, states) )
		{
			return false;
		}
		if( baseClass->byteSize < 0 || baseClass->byteAlignment == 0 )
		{
			sema->AddDiagnostic("class-layout-base-unsealed", classDecl->range);
			return false;
		}
		size = baseClass->byteSize;
		alignment = baseClass->byteAlignment;
		trivialStorageCopy = trivialStorageCopy
			&& (baseClass->traits & asAST_TRAIT_TRIVIAL_STORAGE_COPY) != 0;
	}

	// PreClassData describes embedding-owned storage that precedes the authored
	// fields. It changes source-visible property offsets and the object extent,
	// so it is a Sema layout input rather than CodeGen-only metadata. Freeze the
	// exact offset on the class declaration (class byteOffset denotes this
	// pre-class prefix) and let the ordinary field algorithm start after it.
	const asSCanonicalPreClassFact* const classFact = module
		? module->FindCanonicalPreClassFact(classDecl->name) : 0;
	if( classFact )
	{
		trivialStorageCopy = false;
		if( classFact->propertyOffset > static_cast<size_t>(INT_MAX) )
		{
			sema->AddDiagnostic("class-layout-preclass-offset-overflow", classDecl->range);
			return false;
		}
		const int propertyOffset = static_cast<int>(classFact->propertyOffset);
		context.SetDeclByteOffset(classDecl->id, propertyOffset);
		if( baseClass == 0 )
		{
			size = propertyOffset;
		}
		if( classFact->hasShadowType
			&& classFact->shadowTypeAlignment > static_cast<int>(alignment) )
		{
			alignment = static_cast<asUINT>(classFact->shadowTypeAlignment);
		}
	}

	for( asUINT memberIndex = 0; memberIndex < classDecl->children.GetLength(); ++memberIndex )
	{
		const asCDecl* const member = context.GetDecl(classDecl->children[memberIndex]);
		if( member == 0 || member->kind != asAST_DECL_VAR )
		{
			continue;
		}
		const asCDataType dataType = bridge.Resolve(context, member->type);
		bool fieldHasTrivialStorageCopy = false;
		int bytes = 0;
		asUINT fieldAlignment = 1;
		if( dataType.IsValid() )
		{
			fieldHasTrivialStorageCopy = dataType.IsPrimitive() || dataType.IsEnumType();
			if( dataType.IsObject() && !dataType.IsObjectHandle()
				&& !dataType.IsReference() && dataType.GetTypeInfo() )
			{
				fieldHasTrivialStorageCopy =
					(dataType.GetTypeInfo()->GetFlags() & asOBJ_POD) != 0;
			}
			// Mirror the maintained Builder ABI: reference objects occupy one
			// pointer slot, while value objects use their inline storage extent.
			bytes = dataType.IsObject() && dataType.GetTypeInfo()
				&& (dataType.GetTypeInfo()->flags & asOBJ_VALUE) == 0
				? dataType.GetSizeOnStackDWords() * 4
				: dataType.GetSizeInMemoryBytes();
			fieldAlignment = dataType.GetAlignment()
				? dataType.GetAlignment() : 1;
		}
		else
		{
			// Script types are intentionally absent from the Runtime registry until
			// the sealed AST has passed verification and CodeGen commits. Resolve
			// their layout from the Canonical Type/Decl graph instead of creating a
			// registration-order dependency between Sema and the backend.
			const asCType* const canonicalType = context.GetType(member->type.type);
			if( canonicalType && (member->type.IsHandle()
				|| canonicalType->kind == asAST_TYPE_REFERENCE_OBJECT
				|| canonicalType->kind == asAST_TYPE_FUNCDEF) )
			{
				bytes = AS_PTR_SIZE * 4;
				fieldAlignment = AS_PTR_SIZE * 4;
			}
			else if( canonicalType && canonicalType->kind == asAST_TYPE_ENUM )
			{
				// This maintained fork uses a signed-byte Runtime carrier for script
				// enums. The semantic enum identity remains distinct in the AST.
				bytes = 1;
				fieldAlignment = 1;
			}
			else if( canonicalType && canonicalType->kind == asAST_TYPE_VALUE_OBJECT )
			{
				const asCDecl* const valueClass =
					FindLayoutClassForType(context, canonicalType);
				if( valueClass && LayoutOneScriptClass(
					sema, bridge, module, valueClass->id, states)
					&& valueClass->byteSize >= 0
					&& valueClass->byteAlignment > 0 )
				{
					bytes = valueClass->byteSize;
					fieldAlignment = valueClass->byteAlignment;
					fieldHasTrivialStorageCopy =
						(valueClass->traits & asAST_TRAIT_TRIVIAL_STORAGE_COPY) != 0;
				}
			}
		}
		if( bytes <= 0 )
		{
			sema->AddDiagnostic("class-layout-field-type-unresolved", member->range);
			return false;
		}
		if( fieldAlignment > alignment )
		{
			alignment = fieldAlignment;
		}
		const asUINT misalignment = static_cast<asUINT>(size) & (fieldAlignment - 1);
		if( misalignment != 0 )
		{
			size += static_cast<int>(fieldAlignment - misalignment);
		}
		context.SetDeclByteOffset(member->id, size);
		size += bytes;
		trivialStorageCopy = trivialStorageCopy && fieldHasTrivialStorageCopy;
	}
	if( alignment > 1 && size > 0 )
	{
		size = (size + static_cast<int>(alignment) - 1)
			& ~(static_cast<int>(alignment) - 1);
	}
	if( size <= 0 )
	{
		// A script class with no authored fields still needs a sealed extent.
		// Native REF bases are not layout owners; extra-module compiles also
		// have no PreClassData. Keep a pointer-sized object so CodeGen can
		// publish the type instead of leaving the -1/0 unlaid-out sentinel.
		size = AS_PTR_SIZE * 4;
		if( alignment < static_cast<asUINT>(AS_PTR_SIZE * 4) )
		{
			alignment = static_cast<asUINT>(AS_PTR_SIZE * 4);
		}
	}
	context.SetDeclLayout(classDecl->id, size, alignment);
	if( trivialStorageCopy )
	{
		context.AddDeclTrait(classDecl->id, asAST_TRAIT_TRIVIAL_STORAGE_COPY);
	}
	states[classId.value] = 2;
	return true;
}

static void AttachBaseConstructorPlans(asCSema* sema)
{
	asCASTContext& context = sema->GetContext();
	for( asUINT declIndex = 1; declIndex <= context.GetDeclCount(); ++declIndex )
	{
		const asCDecl* const classDecl = context.GetDecl(asASTDeclId(declIndex));
		if( classDecl == 0 || classDecl->kind != asAST_DECL_CLASS
			|| IsCanonicalNativeTypeView(classDecl) )
		{
			continue;
		}
		const asCDecl* const baseClass = FindLayoutBaseClass(context, classDecl);
		if( baseClass == 0 )
		{
			continue;
		}
		const asCDecl* const baseConstructor = FindDefaultConstructor(context, baseClass);
		if( baseConstructor == 0 )
		{
			sema->AddDiagnostic("base-default-constructor-unavailable", classDecl->range);
			continue;
		}
		for( asUINT childIndex = 0; childIndex < classDecl->children.GetLength(); ++childIndex )
		{
			const asCDecl* const constructor = context.GetDecl(classDecl->children[childIndex]);
			if( constructor == 0 || constructor->kind != asAST_DECL_CONSTRUCTOR )
			{
				continue;
			}
			bool alreadyAttached = false;
			for( asUINT initIndex = 0; initIndex < constructor->inits.GetLength(); ++initIndex )
			{
				const asCExpr* const init = context.GetExpr(constructor->inits[initIndex]);
				if( init && init->kind == asAST_EXPR_CALL
					&& init->resolvedDecl == baseConstructor->id )
				{
					alreadyAttached = true;
					break;
				}
			}
			if( alreadyAttached )
			{
				continue;
			}
			const asASTExprId receiver = sema->ActOnThisExpr(
				ClassObjectQualType(context, classDecl), constructor->range);
			asCArray<asASTExprId> arguments;
			arguments.PushLast(receiver);
			const asASTExprId call = sema->ActOnCall(
				baseConstructor->id, arguments,
				context.InternPrimitive(ttVoid, 0), constructor->range);
			context.SetExprReceiver(call, receiver);
			context.PrependDeclInit(constructor->id, call);
		}
	}
}

static const asCDecl* FindClassDestructor(
	const asCASTContext& context,
	const asCDecl* classDecl)
{
	for( asUINT childIndex = 0;
		classDecl != 0 && childIndex < classDecl->children.GetLength();
		++childIndex )
	{
		const asCDecl* const child =
			context.GetDecl(classDecl->children[childIndex]);
		if( child != 0 && child->kind == asAST_DECL_DESTRUCTOR )
		{
			return child;
		}
	}
	return 0;
}

static const asCDecl* FindValueTypeDestructor(
	const asCASTContext& context,
	const asCDecl* field)
{
	const asCType* const type = field != 0 && field->type.IsValid()
		? context.GetType(field->type.type) : 0;
	if( field == 0 || field->kind != asAST_DECL_VAR
		|| type == 0 || type->kind != asAST_TYPE_VALUE_OBJECT
		|| field->type.IsReference() || field->type.IsHandle() )
	{
		return 0;
	}
	for( asUINT declIndex = 1;
		declIndex <= context.GetDeclCount(); ++declIndex )
	{
		const asCDecl* const destructor =
			context.GetDecl(asASTDeclId(declIndex));
		const asCDecl* const owner = destructor != 0
			&& destructor->kind == asAST_DECL_DESTRUCTOR
			? context.GetDecl(destructor->parent) : 0;
		if( owner != 0 && owner->kind == asAST_DECL_CLASS
			&& owner->stableKey.Equals(type->stableKey) )
		{
			return destructor;
		}
	}
	return 0;
}

static bool HasConstructorLifetimePlan(
	const asCASTContext& context,
	asASTDeclId constructor)
{
	for( asUINT index = 0;
		index < context.GetLifetimeRecordCount(); ++index )
	{
		const asSASTLifetimeRecord* const record =
			context.GetLifetimeRecord(index);
		if( record != 0
			&& record->subjectKind
				== asAST_LIFETIME_SUBJECT_CONSTRUCTION_SUBOBJECT
			&& record->semanticRegion
				== asSASTLifetimeNodeId::FromDecl(constructor) )
		{
			return true;
		}
	}
	return false;
}

static bool AuthorConstructorLifetimeRecord(
	asCSema* sema,
	asASTDeclId constructor,
	asSASTLifetimeRecord& record,
	asUINT step)
{
	asCASTContext& context = sema->GetContext();
	record.subjectKind =
		asAST_LIFETIME_SUBJECT_CONSTRUCTION_SUBOBJECT;
	record.semanticRegion = asSASTLifetimeNodeId::FromDecl(constructor);
	record.constructionStep = step;
	record.order = context.GetLifetimeRecordCount();
	if( asSemaAuthorLifetimeRecord(context, record) != asAST_VERIFY_OK )
	{
		sema->AddDiagnostic("constructor-lifetime-record-invalid");
		return false;
	}
	return true;
}

static bool AuthorOneConstructorLifetimePlan(
	asCSema* sema,
	const asCDecl* classDecl,
	const asCDecl* constructor)
{
	if( sema == 0 || classDecl == 0 || constructor == 0
		|| constructor->kind != asAST_DECL_CONSTRUCTOR )
	{
		return false;
	}
	asCASTContext& context = sema->GetContext();
	if( HasConstructorLifetimePlan(context, constructor->id) ) return true;
	if( (constructor->traits
		& (asAST_TRAIT_EXTERNAL | asAST_TRAIT_LIST_FACTORY)) != 0 )
	{
		return true;
	}

	const asCDecl* const completeDestructor =
		FindClassDestructor(context, classDecl);
	if( completeDestructor == 0 )
	{
		sema->AddDiagnostic(
			"constructor-complete-destructor-unavailable",
			constructor->range);
		return false;
	}

	asUINT step = 0;
	for( asUINT initIndex = 0;
		initIndex < constructor->inits.GetLength(); ++initIndex )
	{
		const asCExpr* const init =
			context.GetExpr(constructor->inits[initIndex]);
		if( init == 0 ) continue;

		if( init->kind == asAST_EXPR_CALL )
		{
			const asCDecl* const targetConstructor =
				context.GetDecl(init->resolvedDecl);
			const asCDecl* const targetOwner = targetConstructor != 0
				&& targetConstructor->kind == asAST_DECL_CONSTRUCTOR
				? context.GetDecl(targetConstructor->parent) : 0;
			if( targetOwner == 0 || targetOwner->kind != asAST_DECL_CLASS )
			{
				continue;
			}

			if( targetOwner->id == classDecl->id )
			{
				asSASTLifetimeRecord record;
				record.subject = asSASTLifetimeNodeId::FromDecl(
					classDecl->id);
				record.actionKind =
					asAST_LIFETIME_ACTION_DESTROY_VALUE;
				record.actionTarget = asSASTLifetimeNodeId::FromDecl(
					completeDestructor->id);
				record.activationPoint = asSASTLifetimeNodeId::FromExpr(
					init->id);
				record.phase =
					asAST_LIFETIME_PHASE_CONSTRUCTOR_DELEGATING;
				record.supportedExitMask =
					asAST_LIFETIME_EXIT_EXCEPTION;
				record.completeObjectCommit = true;
				return step == 0
					&& constructor->inits.GetLength() == 1
					&& AuthorConstructorLifetimeRecord(
						sema, constructor->id, record, step);
			}

			bool directBase = false;
			for( asUINT baseIndex = 0;
				baseIndex < classDecl->bases.GetLength(); ++baseIndex )
			{
				if( classDecl->bases[baseIndex] == targetOwner->id )
				{
					directBase = true;
					break;
				}
			}
			const asCDecl* const destructor = directBase
				? FindClassDestructor(context, targetOwner) : 0;
			if( destructor == 0 ) continue;

			asSASTLifetimeRecord record;
			record.subject = asSASTLifetimeNodeId::FromDecl(targetOwner->id);
			record.actionKind = asAST_LIFETIME_ACTION_DESTROY_VALUE;
			record.actionTarget = asSASTLifetimeNodeId::FromDecl(
				destructor->id);
			record.activationPoint = asSASTLifetimeNodeId::FromExpr(init->id);
			record.phase = asAST_LIFETIME_PHASE_CONSTRUCTOR_BASE;
			record.supportedExitMask = asAST_LIFETIME_EXIT_EXCEPTION;
			if( !AuthorConstructorLifetimeRecord(
				sema, constructor->id, record, step++) )
			{
				return false;
			}
			continue;
		}

		const asCExpr* const target = init->kind == asAST_EXPR_ASSIGN
			&& init->children.GetLength() == 2
			? context.GetExpr(init->children[0]) : 0;
		const asCDecl* const field = target != 0
			&& target->kind == asAST_EXPR_DECL_REF
			? context.GetDecl(target->resolvedDecl) : 0;
		if( field == 0 || field->kind != asAST_DECL_VAR
			|| field->parent != classDecl->id )
		{
			continue;
		}
		const asCType* const fieldType = field->type.IsValid()
			? context.GetType(field->type.type) : 0;
		const asCDecl* const destructor =
			FindValueTypeDestructor(context, field);
		const bool ownedReference = fieldType != 0
			&& !field->type.IsReference()
			&& (fieldType->kind == asAST_TYPE_REFERENCE_OBJECT
				|| fieldType->kind == asAST_TYPE_FUNCDEF);
		if( destructor == 0 && !ownedReference ) continue;

		asSASTLifetimeRecord record;
		record.subject = asSASTLifetimeNodeId::FromDecl(field->id);
		record.actionKind = destructor != 0
			? asAST_LIFETIME_ACTION_DESTROY_VALUE
			: asAST_LIFETIME_ACTION_RELEASE_REFERENCE;
		record.actionTarget = asSASTLifetimeNodeId::FromDecl(
			destructor != 0 ? destructor->id : field->id);
		record.activationPoint = asSASTLifetimeNodeId::FromExpr(init->id);
		record.phase = asAST_LIFETIME_PHASE_CONSTRUCTOR_MEMBER;
		record.supportedExitMask = asAST_LIFETIME_EXIT_EXCEPTION;
		if( !AuthorConstructorLifetimeRecord(
			sema, constructor->id, record, step++) )
		{
			return false;
		}
	}

	asASTStmtId body = constructor->body;
	if( !body.IsValid() )
	{
		body = context.CreateStmt(
			asAST_STMT_BLOCK, constructor->id, constructor->range);
		if( !body.IsValid()
			|| context.SetBody(constructor->id, body) != asAST_VERIFY_OK )
		{
			sema->AddDiagnostic(
				"constructor-complete-boundary-invalid",
				constructor->range);
			return false;
		}
	}
	asSASTLifetimeRecord complete;
	complete.subject = asSASTLifetimeNodeId::FromDecl(classDecl->id);
	complete.actionKind = asAST_LIFETIME_ACTION_DESTROY_VALUE;
	complete.actionTarget = asSASTLifetimeNodeId::FromDecl(
		completeDestructor->id);
	complete.activationPoint = asSASTLifetimeNodeId::FromStmt(body);
	complete.phase = asAST_LIFETIME_PHASE_COMPLETE_OBJECT;
	complete.supportedExitMask = asAST_LIFETIME_EXIT_NORMAL;
	complete.completeObjectCommit = true;
	return AuthorConstructorLifetimeRecord(
		sema, constructor->id, complete, step);
}

static void AuthorConstructorLifetimePlans(asCSema* sema)
{
	asCASTContext& context = sema->GetContext();
	for( asUINT declIndex = 1;
		declIndex <= context.GetDeclCount(); ++declIndex )
	{
		const asCDecl* const classDecl =
			context.GetDecl(asASTDeclId(declIndex));
		if( classDecl == 0 || classDecl->kind != asAST_DECL_CLASS
			|| IsCanonicalNativeTypeView(classDecl) )
		{
			continue;
		}
		for( asUINT childIndex = 0;
			childIndex < classDecl->children.GetLength(); ++childIndex )
		{
			const asCDecl* const constructor =
				context.GetDecl(classDecl->children[childIndex]);
			if( constructor != 0
				&& constructor->kind == asAST_DECL_CONSTRUCTOR )
			{
				AuthorOneConstructorLifetimePlan(
					sema, classDecl, constructor);
			}
		}
	}
}

void asCSema::LayoutScriptClassFields(asCModule* module)
{
	if( engine == 0 )
	{
		return;
	}
	asCRuntimeTypeBridge bridge(engine);
	asCArray<asBYTE> states;
	states.SetLength(context.GetDeclCount() + 1);
	for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
	{
		const asCDecl* cls = context.GetDecl(asASTDeclId(i));
		if( cls == 0 || cls->kind != asAST_DECL_CLASS )
		{
			continue;
		}
		LayoutOneScriptClass(this, bridge, module, cls->id, states);
	}
	FinalizeGeneratedImplicitCopyLifecycle(this);
	AttachBaseConstructorPlans(this);
	AuthorConstructorLifetimePlans(this);
	ReconcileGeneratedAccessorPlans();
	// Generated wrappers can be declared after an already-resolved consumer.
	// Their final trivial-copy trait and copy constructor are authoritative only
	// after this layout pass, so close those earlier call edges before Seal.
	ReconcileDeferredByValueCopyPlans();
}

END_AS_NAMESPACE
