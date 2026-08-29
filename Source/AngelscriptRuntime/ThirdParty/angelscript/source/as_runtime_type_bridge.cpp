#include "as_runtime_type_bridge.h"
#include "as_runtime_type_binding.h"
#include "as_module.h"
#include "as_namespace.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"

BEGIN_AS_NAMESPACE

asCRuntimeTypeBridge::asCRuntimeTypeBridge(asCScriptEngine* inEngine)
	: engine(inEngine)
	, transientTypes(0)
{
}

void asCRuntimeTypeBridge::SetTransientTypes(const asCArray<asCTypeInfo*>* types)
{
	transientTypes = types;
}

asCDataType asCRuntimeTypeBridge::Resolve(const asCASTContext& context, const asCQualType& qualType) const
{
	asCDataType dataType;
	asCTypeInfo* typeInfo = 0;
	return asCRuntimeTypeBindingTable::ResolveCanonicalType(
		engine, context, qualType, dataType, typeInfo, transientTypes, 0) == asRUNTIME_TYPE_BIND_OK
		? dataType
		: asCDataType();
}

bool asCRuntimeTypeBridge::ValidateContextTypes(const asCASTContext& context, asCString* unresolvedStableKey) const
{
	if( unresolvedStableKey )
	{
		*unresolvedStableKey = "";
	}
	for( asUINT i = 1; i <= context.GetTypeCount(); ++i )
	{
		const asCType* type = context.GetType(asASTTypeRef(i));
		asCDataType dataType;
		asCTypeInfo* typeInfo = 0;
		if( type == 0 || asCRuntimeTypeBindingTable::ResolveCanonicalType(
			engine,
			context,
			asCQualType(asASTTypeRef(i), 0),
			dataType,
			typeInfo,
			transientTypes,
			0) != asRUNTIME_TYPE_BIND_OK )
		{
			if( unresolvedStableKey && type )
			{
				*unresolvedStableKey = type->stableKey;
			}
			return false;
		}
	}
	return true;
}

static bool IsRuntimeBoundFunctionLike(asEASTDeclKind kind)
{
	return kind == asAST_DECL_FUNCTION
		|| kind == asAST_DECL_METHOD
		|| kind == asAST_DECL_CONSTRUCTOR
		|| kind == asAST_DECL_DESTRUCTOR
		|| kind == asAST_DECL_MIXIN;
}

static bool IsCanonicalClassOwner(const asCDecl* decl)
{
	return decl != 0
		&& (decl->kind == asAST_DECL_CLASS || decl->kind == asAST_DECL_INTERFACE);
}

static asETypeModifiers CanonicalParameterPassing(asDWORD quals)
{
	if( (quals & asAST_QUAL_PARAM_DIR_MASK) == asAST_QUAL_INOUT )
	{
		return asTM_INOUTREF;
	}
	if( quals & asAST_QUAL_OUT )
	{
		return asTM_OUTREF;
	}
	if( quals & asAST_QUAL_IN )
	{
		return asTM_INREF;
	}
	// AngelScript's parameter grammar permits a bare '&'.  Builder freezes that
	// source spelling as the default inout reference in the Runtime shell.  Keep
	// the absence of an explicit direction in the source QualType/stable key,
	// but compare against the exact effective passing convention here.
	if( quals & asAST_QUAL_REFERENCE )
	{
		return asTM_INOUTREF;
	}
	return asTM_NONE;
}

// Script source identity deliberately keeps a value-type parameter as T.  The
// Runtime shell ABI has always represented that same parameter as
// `const T&inout` so the VM can pass the value-object storage without copying
// it into an assignable parameter slot.  Keep this deterministic ABI lowering
// in one bridge helper: stable declaration keys remain source-level, while
// structural Runtime binding compares the exact normalized shell signature.
void asCRuntimeTypeBridge::NormalizeScriptParameterABI(
	asCDataType& type,
	asETypeModifiers& passing)
{
	if( type.IsReference() )
	{
		return;
	}

	asCTypeInfo* const typeInfo = type.GetTypeInfo();
	if( typeInfo != 0 && (typeInfo->GetFlags() & asOBJ_VALUE) != 0 )
	{
		type.MakeReference(true);
		type.MakeReadOnly(true);
		passing = asTM_INOUTREF;
		return;
	}

	// Builder also freezes primitive value parameters as const.  Const is not
	// source overload identity for a primitive value, but it is part of the live
	// shell datatype and therefore belongs in this ABI normalization.
	if( type.IsPrimitive() && !type.IsReadOnly() )
	{
		type.MakeReadOnly(true);
	}
}

asCQualType asCRuntimeTypeBridge::FromScriptParameterABI(
	asCASTContext& context,
	const asCDataType& dataType,
	asETypeModifiers passing) const
{
	asCDataType sourceType = dataType;
	const asCTypeInfo* const typeInfo = sourceType.GetTypeInfo();
	const bool normalizedValueObject = typeInfo != 0
		&& (typeInfo->GetFlags() & asOBJ_VALUE) != 0
		&& sourceType.IsReference()
		&& sourceType.IsReadOnly()
		&& passing == asTM_INOUTREF;
	if( normalizedValueObject )
	{
		// This is the exact inverse of NormalizeScriptParameterABI and matches
		// asCScriptFunction::GetDeclarationStr(forDisplay=true). Runtime shells
		// do not preserve a distinction from an explicitly authored const
		// value-object &inout; a Canonical FuncDefDecl remains authoritative when
		// that source distinction is available.
		sourceType.MakeReference(false);
		sourceType.MakeReadOnly(false);
		passing = asTM_NONE;
	}
	else if( sourceType.IsPrimitive() && !sourceType.IsReference()
		&& sourceType.IsReadOnly() && passing == asTM_NONE )
	{
		// Primitive by-value constness belongs only to the live script shell ABI.
		sourceType.MakeReadOnly(false);
	}

	asCQualType result = FromDataType(context, sourceType);
	if( !result.IsValid() )
	{
		return result;
	}
	if( passing != asTM_NONE )
	{
		result.quals |= asAST_QUAL_REFERENCE;
		if( passing == asTM_INREF )
		{
			result.quals |= asAST_QUAL_IN;
		}
		else if( passing == asTM_OUTREF )
		{
			result.quals |= asAST_QUAL_OUT;
		}
		else if( passing == asTM_INOUTREF )
		{
			result.quals |= asAST_QUAL_INOUT;
		}
		else
		{
			return asCQualType();
		}
	}
	return asASTQualifiersAreValid(result.quals)
		? result : asCQualType();
}

static asCString CanonicalNamespaceKey(
	const asCASTContext& context,
	const asCDecl& decl)
{
	asCString result;
	const asCDecl* cursor = context.GetDecl(decl.parent);
	while( cursor != 0 && cursor->kind != asAST_DECL_TRANSLATION_UNIT )
	{
		if( cursor->kind == asAST_DECL_NAMESPACE && cursor->name.GetLength() )
		{
			asCString prefix = cursor->name;
			if( result.GetLength() )
			{
				prefix += "::";
				prefix += result;
			}
			result = prefix;
		}
		cursor = context.GetDecl(cursor->parent);
	}
	return result;
}

static asCString CanonicalStableKey(
	const asCASTContext& context,
	const asCDecl& decl)
{
	const asCDecl* parent = context.GetDecl(decl.parent);
	asCString key;
	const bool qualifyWithParent = parent != 0
		&& parent->stableKey.GetLength() != 0
		&& parent->kind != asAST_DECL_TRANSLATION_UNIT
		&& decl.kind != asAST_DECL_PARAM;
	asCString namePart = decl.name;
	if( decl.kind == asAST_DECL_DESTRUCTOR )
	{
		namePart = "~";
		namePart += decl.name;
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
	if( IsRuntimeBoundFunctionLike(decl.kind) )
	{
		key += "(";
		bool first = true;
		for( asUINT i = 0; i < decl.children.GetLength(); ++i )
		{
			const asCDecl* child = context.GetDecl(decl.children[i]);
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
	if( decl.traits & asAST_TRAIT_CONST_METHOD )
	{
		key += " const";
	}
	if( decl.traits & asAST_TRAIT_LAMBDA )
	{
		asCString location;
		location.Format("@%u", decl.range.begin.offset);
		key += location;
	}
	return key;
}

static bool FunctionBindsCanonicalDecl(
	const asCRuntimeTypeBridge& bridge,
	const asCASTContext& context,
	const asCModule& module,
	const asCDecl& decl,
	const asCScriptFunction& function,
	asCString* detail)
{
	const bool isLambda = (decl.traits & asAST_TRAIT_LAMBDA) != 0;
	const bool isFactory = function.artifactInvocationKind
		== asBUILD_ARTIFACT_INVOCATION_FACTORY;
	const bool usesCanonicalDirectFunctionABI = isLambda
		|| (decl.kind == asAST_DECL_METHOD
			&& (decl.traits & asAST_TRAIT_GENERATED) != 0
			&& decl.accessorKind != asAST_ACCESSOR_NONE);
	if( isFactory && decl.kind != asAST_DECL_CONSTRUCTOR )
	{
		if( detail )
		{
			detail->Format(
				"factory declaration mismatch: canonical kind=%d is not a constructor",
				(int)decl.kind);
		}
		return false;
	}
	asCString destructorName;
	if( decl.kind == asAST_DECL_DESTRUCTOR )
	{
		destructorName = "~";
		destructorName += decl.name;
	}
	const bool nameMatches = isLambda
		? (function.artifactInvocationKind == asBUILD_ARTIFACT_INVOCATION_LAMBDA
			|| function.name.Equals(decl.name)
			|| (function.name.GetLength() && function.name[0] == '$'))
		: (decl.kind == asAST_DECL_DESTRUCTOR
			? (function.name.Equals(decl.name) || function.name.Equals(destructorName))
			: function.name.Equals(decl.name));
	if( function.module != &module || function.funcType != asFUNC_SCRIPT
		|| !nameMatches )
	{
		if( detail )
		{
			detail->Format(
				"function identity mismatch: expected name='%s' module=%p kind=script; actual name='%s' module=%p kind=%d",
				decl.name.AddressOf(), &module, function.name.AddressOf(),
				function.module, (int)function.funcType);
		}
		return false;
	}
	const asCDecl* parent = context.GetDecl(decl.parent);
	const bool expectsObjectOwner = IsCanonicalClassOwner(parent);
	const asCTypeInfo* const runtimeOwner = isFactory
		? function.artifactOwnerType : function.objectType;
	if( expectsObjectOwner != (runtimeOwner != 0)
		|| (isFactory && function.objectType != 0) )
	{
		if( detail )
		{
			detail->Format(
				"owner mismatch: canonical expectsObjectOwner=%d but Runtime owner='%s' wrapperObjectOwner='%s'",
				expectsObjectOwner ? 1 : 0,
				runtimeOwner ? runtimeOwner->name.AddressOf() : "<global>",
				function.objectType ? function.objectType->name.AddressOf() : "<global>");
		}
		return false;
	}
	if( expectsObjectOwner
		&& (!runtimeOwner->name.Equals(parent->name)
			|| runtimeOwner->nameSpace == 0) )
	{
		if( detail )
		{
			detail->Format(
				"owner mismatch: canonical owner='%s' but Runtime owner='%s'",
				parent ? parent->name.AddressOf() : "<missing>",
				runtimeOwner ? runtimeOwner->name.AddressOf() : "<global>");
		}
		return false;
	}
	const asCString expectedNamespace = CanonicalNamespaceKey(context, decl);
	const asSNameSpace* const actualNamespace = runtimeOwner
		? runtimeOwner->nameSpace : function.nameSpace;
	if( (actualNamespace == 0 && expectedNamespace.GetLength() != 0)
		|| (actualNamespace != 0 && !actualNamespace->name.Equals(expectedNamespace)) )
	{
		if( detail )
		{
			detail->Format(
				"namespace mismatch: canonical='%s' Runtime='%s'",
				expectedNamespace.AddressOf(),
				actualNamespace ? actualNamespace->name.AddressOf() : "");
		}
		return false;
	}
	if( isFactory )
	{
		// A factory is a generated Runtime wrapper for one source constructor,
		// not a second canonical declaration. Its callable ABI is deliberately
		// global and returns the constructed reference-class handle, while the
		// canonical constructor is an object-owned void function. The producer
		// must therefore carry both the constructor stable key and exact transient
		// owner identity; accepting the key alone would permit a cross-type factory
		// to bind the wrong constructor.
		if( runtimeOwner == 0
			|| !function.returnType.IsObjectHandle()
			|| function.returnType.IsReference()
			|| function.returnType.GetTypeInfo() != runtimeOwner )
		{
			if( detail )
			{
				const asCString actualText = function.returnType.Format(
					function.nameSpace, true, true);
				detail->Format(
					"factory return mismatch: expected handle owner='%s' actual='%s' actualType=%p ownerType=%p",
					runtimeOwner ? runtimeOwner->name.AddressOf() : "<missing>",
					actualText.AddressOf(), function.returnType.GetTypeInfo(),
					runtimeOwner);
			}
			return false;
		}
	}
	else
	{
		const asCDataType expectedReturn = bridge.Resolve(context, decl.type);
		if( !expectedReturn.IsValid() || function.returnType != expectedReturn )
		{
			if( detail )
			{
				const asCString expectedText = expectedReturn.IsValid()
					? expectedReturn.Format(function.nameSpace, true, true)
					: asCString("<invalid>");
				const asCString actualText = function.returnType.Format(
					function.nameSpace, true, true);
				detail->Format(
					"return type mismatch: canonical='%s' Runtime='%s'",
					expectedText.AddressOf(), actualText.AddressOf());
			}
			return false;
		}
	}
	if( function.IsReadOnly()
		!= ((decl.traits & asAST_TRAIT_CONST_METHOD) != 0) )
	{
		if( detail )
		{
			detail->Format(
				"const-method mismatch: canonical=%d Runtime=%d",
				(decl.traits & asAST_TRAIT_CONST_METHOD) != 0 ? 1 : 0,
				function.IsReadOnly() ? 1 : 0);
		}
		return false;
	}
	asUINT parameterIndex = 0;
	for( asUINT i = 0; i < decl.children.GetLength(); ++i )
	{
		const asCDecl* param = context.GetDecl(decl.children[i]);
		if( param == 0 || param->kind != asAST_DECL_PARAM )
		{
			continue;
		}
		asCDataType expectedParam = bridge.Resolve(context, param->type);
		asETypeModifiers expectedPassing =
			CanonicalParameterPassing(param->type.quals);
		if( expectedParam.IsValid() && !usesCanonicalDirectFunctionABI )
		{
			// Parser-authored script functions pass through Builder, whose shell
			// ABI normalizes primitive values to const and value objects to const
			// inout references. Generated accessors have no Parser/Builder shell:
			// Canonical CodeGen creates their Runtime signature directly from the
			// sealed declaration. Preserve that exact ABI so a non-POD setter keeps
			// its explicit by-value ownership-transfer contract.
			asCRuntimeTypeBridge::NormalizeScriptParameterABI(
				expectedParam, expectedPassing);
		}
		const asETypeModifiers actualPassing =
			parameterIndex < function.inOutFlags.GetLength()
			? function.inOutFlags[parameterIndex] : asTM_NONE;
		const bool parameterTypeMatches =
			parameterIndex < function.parameterTypes.GetLength()
			&& function.parameterTypes[parameterIndex] == expectedParam;
		if( !expectedParam.IsValid()
			|| !parameterTypeMatches )
		{
			if( detail )
			{
				const asCString expectedText = expectedParam.IsValid()
					? expectedParam.Format(function.nameSpace, true, true)
					: asCString("<invalid>");
				const asCString actualText =
					parameterIndex < function.parameterTypes.GetLength()
					? function.parameterTypes[parameterIndex].Format(
						function.nameSpace, true, true)
					: asCString("<missing>");
				detail->Format(
					"parameter[%u] type mismatch: canonical-source-quals=0x%X expected-Runtime='%s' actual-Runtime='%s' expected-passing=%d actual-passing=%d",
					parameterIndex, param->type.quals,
					expectedText.AddressOf(), actualText.AddressOf(),
					(int)expectedPassing, (int)actualPassing);
			}
			return false;
		}
		if( actualPassing != expectedPassing )
		{
			if( detail )
			{
				const asCString typeText = expectedParam.Format(
					function.nameSpace, true, true);
				detail->Format(
					"parameter[%u] passing mismatch: Runtime-type='%s' canonical-source-quals=0x%X expected-passing=%d actual-passing=%d",
					parameterIndex, typeText.AddressOf(), param->type.quals,
					(int)expectedPassing, (int)actualPassing);
			}
			return false;
		}
		++parameterIndex;
	}
	// Canonical lambda captures are hidden VM formals appended after the user
	// parameters. They are part of structural binding even though they are not
	// part of the source-level stable signature.
	if( isLambda )
	{
		for( asUINT i = 0; i < decl.captures.GetLength(); ++i )
		{
			const asCDecl* captured = context.GetDecl(decl.captures[i]);
			if( captured == 0 )
			{
				if( detail )
				{
					detail->Format(
						"lambda capture[%u] references a missing declaration",
						i);
				}
				return false;
			}
			const asCDataType expectedCapture = bridge.Resolve(context, captured->type);
			if( !expectedCapture.IsValid()
				|| parameterIndex >= function.parameterTypes.GetLength()
				|| function.parameterTypes[parameterIndex] != expectedCapture )
			{
				if( detail )
				{
					detail->Format(
						"lambda capture[%u] type mismatch at Runtime parameter[%u]",
						i, parameterIndex);
				}
				return false;
			}
			const asETypeModifiers actualPassing =
				parameterIndex < function.inOutFlags.GetLength()
				? function.inOutFlags[parameterIndex] : asTM_NONE;
			if( actualPassing != asTM_NONE )
			{
				if( detail )
				{
					detail->Format(
						"lambda capture[%u] passing mismatch at Runtime parameter[%u]: actual=%d",
						i, parameterIndex, (int)actualPassing);
				}
				return false;
			}
			++parameterIndex;
		}
	}
	if( parameterIndex != function.parameterTypes.GetLength() )
	{
		if( detail )
		{
			detail->Format(
				"parameter count mismatch: canonical-effective=%u Runtime=%u",
				parameterIndex, function.parameterTypes.GetLength());
		}
		return false;
	}
	return true;
}

int asCRuntimeTypeBridge::BindFunctionDeclaration(
	const asCASTContext& context,
	const asCScriptFunction& function,
	asASTDeclId& outDeclaration,
	asCString* detail) const
{
	outDeclaration = asASTDeclId();
	if( detail )
	{
		*detail = "";
	}
	if( !context.IsPublishable() || function.module == 0 || function.funcType != asFUNC_SCRIPT )
	{
		if( detail )
		{
			*detail = "canonical declaration bind requires a sealed AST and a live script function";
		}
		return asRUNTIME_DECL_BIND_INVALID;
	}
	if( function.canonicalASTStableDeclKey.GetLength() == 0 )
	{
		if( detail )
		{
			*detail = "live function has no producer-carried canonical declaration key";
		}
		return asRUNTIME_DECL_BIND_MISSING_KEY;
	}

	const asCDecl* match = 0;
	asUINT matches = 0;
	for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
	{
		const asCDecl* decl = context.GetDecl(asASTDeclId(i));
		if( decl != 0 && IsRuntimeBoundFunctionLike(decl->kind)
			&& decl->stableKey.Equals(function.canonicalASTStableDeclKey) )
		{
			match = decl;
			++matches;
		}
	}
	if( matches == 0 )
	{
		if( detail )
		{
			detail->Format("canonical declaration key '%s' was not found",
				function.canonicalASTStableDeclKey.AddressOf());
		}
		return asRUNTIME_DECL_BIND_NOT_FOUND;
	}
	if( matches != 1 )
	{
		if( detail )
		{
			detail->Format("canonical declaration key '%s' matched %u declarations",
				function.canonicalASTStableDeclKey.AddressOf(), matches);
		}
		return asRUNTIME_DECL_BIND_AMBIGUOUS;
	}
	if( match == 0 || !FunctionBindsCanonicalDecl(
		*this, context, *function.module, *match, function, detail) )
	{
		if( detail && detail->GetLength() == 0 )
		{
			detail->Format("canonical declaration key '%s' does not match the live function signature",
				function.canonicalASTStableDeclKey.AddressOf());
		}
		return asRUNTIME_DECL_BIND_SIGNATURE_MISMATCH;
	}

	outDeclaration = match->id;
	return asRUNTIME_DECL_BIND_OK;
}

static bool ValidateContextDeclarationBindings(
	const asCRuntimeTypeBridge& bridge,
	const asCASTContext& context,
	const asCModule& module,
	asCString* unresolvedStableKey,
	asCArray<asCScriptFunction*>* boundFunctions,
	asCArray<asCString>* boundStableKeys)
{
	if( boundFunctions )
	{
		boundFunctions->SetLength(0);
	}
	if( boundStableKeys )
	{
		boundStableKeys->SetLength(0);
	}
	if( unresolvedStableKey )
	{
		*unresolvedStableKey = "";
	}
	for( asUINT i = 1; i <= context.GetDeclCount(); ++i )
	{
		const asCDecl* decl = context.GetDecl(asASTDeclId(i));
		if( decl == 0 )
		{
			return false;
		}

		const asCString expectedStableKey = CanonicalStableKey(context, *decl);
		if( expectedStableKey.GetLength() == 0
			|| !decl->stableKey.Equals(expectedStableKey) )
		{
			if( unresolvedStableKey )
			{
				*unresolvedStableKey = decl->stableKey;
			}
			return false;
		}

		// Every restored declaration must first agree with the snapshot-local
		// canonical identity derived from its sealed graph. This includes
		// declarations such as enums and namespaces that deliberately do not
		// have a local asCScriptFunction skeleton to bind below.
		if( !IsRuntimeBoundFunctionLike(decl->kind) || !decl->body.IsValid() )
		{
			continue;
		}

		asUINT matches = 0;
		for( asUINT f = 0; f < module.scriptFunctions.GetLength(); ++f )
		{
			asCScriptFunction* function = module.scriptFunctions[f];
			if( function && FunctionBindsCanonicalDecl(
				bridge, context, module, *decl, *function, 0) )
			{
				if( matches == 0 && boundFunctions )
				{
					boundFunctions->PushLast(function);
				}
				++matches;
			}
		}
		if( matches != 1 )
		{
			if( unresolvedStableKey )
			{
				*unresolvedStableKey = decl->stableKey;
			}
			return false;
		}
		if( boundFunctions && boundStableKeys )
		{
			asCScriptFunction* const function =
				(*boundFunctions)[boundFunctions->GetLength() - 1];
			for( asUINT previous = 0;
				previous + 1 < boundFunctions->GetLength(); ++previous )
			{
				if( (*boundFunctions)[previous] == function )
				{
					if( unresolvedStableKey )
					{
						*unresolvedStableKey = decl->stableKey;
					}
					return false;
				}
			}
			boundStableKeys->PushLast(decl->stableKey);
		}
	}
	return true;
}

bool asCRuntimeTypeBridge::ValidateContextDeclarations(
	const asCASTContext& context,
	const asCModule& module,
	asCString* unresolvedStableKey) const
{
	return ValidateContextDeclarationBindings(
		*this, context, module, unresolvedStableKey, 0, 0);
}

bool asCRuntimeTypeBridge::ValidateAndBindContextDeclarations(
	const asCASTContext& context,
	asCModule& module,
	asCString* unresolvedStableKey) const
{
	asCArray<asCScriptFunction*> functions;
	asCArray<asCString> stableKeys;
	if( !ValidateContextDeclarationBindings(
		*this, context, module, unresolvedStableKey, &functions, &stableKeys) )
	{
		return false;
	}
	if( functions.GetLength() != stableKeys.GetLength() )
	{
		return false;
	}
	for( asUINT i = 0; i < functions.GetLength(); ++i )
	{
		functions[i]->canonicalASTStableDeclKey = stableKeys[i];
	}
	return true;
}

asCQualType asCRuntimeTypeBridge::FromDataType(asCASTContext& context, const asCDataType& dataType) const
{
	if( dataType.IsNullHandle() )
	{
		return context.InternPrimitive(ttVoid, asAST_QUAL_HANDLE);
	}

	asDWORD quals = 0;
	if( dataType.IsObjectConst() || dataType.IsReadOnly() )
	{
		quals |= asAST_QUAL_CONST;
	}
	if( dataType.IsObjectHandle() )
	{
		quals |= asAST_QUAL_HANDLE;
	}
	if( dataType.IsReference() )
	{
		quals |= asAST_QUAL_REFERENCE;
	}
	if( dataType.GetTokenType() == ttQuestion )
	{
		return context.InternNamedType(asAST_TYPE_WILDCARD, "?", quals);
	}

	asCTypeInfo* info = dataType.GetTypeInfo();
	if( info )
	{
		const asDWORD flags = info->GetFlags();
		asEASTTypeKind kind = asAST_TYPE_VALUE_OBJECT;
		if( flags & asOBJ_ENUM )
		{
			kind = asAST_TYPE_ENUM;
		}
		else if( flags & asOBJ_FUNCDEF )
		{
			kind = asAST_TYPE_FUNCDEF;
		}
		else if( flags & asOBJ_TEMPLATE )
		{
			kind = asAST_TYPE_TEMPLATE;
		}
		else if( flags & asOBJ_REF )
		{
			kind = asAST_TYPE_REFERENCE_OBJECT;
		}

		asCDataType keyDt = dataType;
		// Stable canonical Type identity never includes top-level, handle, or
		// pointee constness. asCDataType stores handle-to-const in isReadOnly while
		// MakeReadOnly() addresses const-handle state once the value is a handle,
		// so clear both representations before formatting the key.
		keyDt.MakeConstValueOrConstObject(false);
		if( keyDt.IsObjectHandle() )
		{
			keyDt.MakeHandleToConst(false);
		}
		keyDt.MakeReadOnly(false);
		keyDt.MakeHandle(false);
		keyDt.MakeReference(false);
		asCString key;
		if( engine && engine->nameSpaces.GetLength() > 0 )
		{
			key = keyDt.Format(engine->nameSpaces[0], true);
		}
		else if( info->GetName() )
		{
			key = info->GetName();
		}
		if( key.GetLength() == 0 )
		{
			return asCQualType();
		}
		return context.InternNamedType(kind, key.AddressOf(), quals);
	}

	if( dataType.GetTokenType() == ttVoid || dataType.IsPrimitive() )
	{
		return context.InternPrimitive(dataType.GetTokenType(), quals);
	}
	return asCQualType();
}

END_AS_NAMESPACE
