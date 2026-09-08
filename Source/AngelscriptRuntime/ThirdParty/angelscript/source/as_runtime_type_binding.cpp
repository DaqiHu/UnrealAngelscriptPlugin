#include "as_runtime_type_binding.h"

#include <cstring>

#include "Containers/StringConv.h"

#include "as_module.h"
#include "as_namespace.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"

BEGIN_AS_NAMESPACE

namespace
{
	void ClearDetail(asCString* detail)
	{
		if( detail )
		{
			*detail = "";
		}
	}

	void SetDetail(asCString* detail, const char* category, const char* key)
	{
		if( detail )
		{
			detail->Format("%s: %s", category ? category : "runtime-type-binding", key ? key : "");
		}
	}

	void SetDataTypeDetail(
		asCString* detail,
		const char* category,
		const asCDataType& dataType)
	{
		if( detail == 0 )
			return;
		asCTypeInfo* const typeInfo = dataType.GetTypeInfo();
		const char* const name = typeInfo && typeInfo->name.AddressOf()
			? typeInfo->name.AddressOf() : "<anonymous>";
		detail->Format(
			"%s: name=%s token=%d flags=%llu handle=%d ref=%d const=%d",
			category ? category : "runtime-type-binding",
			name,
			static_cast<int>(dataType.GetTokenType()),
			static_cast<unsigned long long>(typeInfo ? typeInfo->GetFlags() : 0),
			dataType.IsObjectHandle() ? 1 : 0,
			dataType.IsReference() ? 1 : 0,
			dataType.IsReadOnly() ? 1 : 0);
	}

	void SetABIMismatchDetail(
		asCString* detail,
		const asSTypeABIKey& expected,
		const asSTypeABIKey& actual)
	{
		if( detail == 0 )
			return;
		const char* field = "layout-hash";
		if( !expected.stableTypeKey.Equals(actual.stableTypeKey) ) field = "stable-type-key";
		else if( !expected.targetProfileKey.Equals(actual.targetProfileKey) ) field = "target-profile";
		else if( !expected.nativeEnvironmentKey.Equals(actual.nativeEnvironmentKey) ) field = "native-environment";
		else if( expected.typeKind != actual.typeKind ) field = "type-kind";
		else if( expected.qualifiers != actual.qualifiers ) field = "qualifiers";
		else if( expected.objectFlags != actual.objectFlags ) field = "object-flags";
		else if( expected.byteSize != actual.byteSize ) field = "byte-size";
		else if( expected.byteAlignment != actual.byteAlignment ) field = "byte-alignment";
		else if( !expected.layoutSignature.Equals(actual.layoutSignature) ) field = "layout-signature";
		else if( !expected.behaviourSignature.Equals(actual.behaviourSignature) ) field = "behaviour-signature";
		detail->Format(
			"runtime type ABI mismatch (%s): %s",
			field,
			expected.stableTypeKey.AddressOf() ? expected.stableTypeKey.AddressOf() : "");
	}

	asCString RuntimeNominalDataTypeKey(
		const asCDataType& dataType,
		asSNameSpace* currentNamespace,
		bool includeNamespace)
	{
		asCTypeInfo* const typeInfo = dataType.GetTypeInfo();
		if( typeInfo == 0 || (typeInfo->GetFlags() & asOBJ_TEMPLATE) == 0 )
		{
			return dataType.Format(currentNamespace, includeNamespace);
		}

		// asCDataType::Format intentionally prints the Engine's default array
		// template with the shorthand `T[]`. Canonical Sema keeps the nominal
		// source spelling (`TArray<T>` in the UE host), so reconstruct the
		// registered template declaration here. This value is used only to select
		// the current Engine candidate; no TypeInfo pointer or numeric typeId is
		// copied into durable AST/artifact identity.
		asCString key;
		if( dataType.IsReadOnly() )
		{
			key = "const ";
		}
		if( includeNamespace
			&& typeInfo->nameSpace != 0
			&& typeInfo->nameSpace->name.GetLength() != 0 )
		{
			key += typeInfo->nameSpace->name;
			key += "::";
		}
		key += typeInfo->name;

		asCObjectType* const objectType = CastToObjectType(typeInfo);
		if( objectType != 0 && objectType->templateSubTypes.GetLength() != 0 )
		{
			key += "<";
			for( asUINT i = 0; i < objectType->templateSubTypes.GetLength(); ++i )
			{
				if( i != 0 )
				{
					key += ",";
				}
				key += RuntimeNominalDataTypeKey(
					objectType->templateSubTypes[i], currentNamespace, includeNamespace);
			}
			key += ">";
		}
		if( dataType.IsObjectHandle() && dataType.IsHandleToConst() )
		{
			key += " const";
		}
		if( dataType.IsReference() )
		{
			key += "&";
		}
		return key;
	}

	asCString StripImplicitHandleMarks(const asCString& key)
	{
		asCString stripped;
		stripped.Assign(key.AddressOf(), key.GetLength());
		asUINT write = 0;
		for( asUINT read = 0; read < stripped.GetLength(); ++read )
		{
			const char current = stripped[read];
			if( current == '@' )
			{
				const char next = (read + 1 < stripped.GetLength())
					? stripped[read + 1] : '\0';
				if( next == '\0' || next == '>' || next == ',' || next == '&' )
				{
					continue;
				}
			}
			stripped[write] = current;
			++write;
		}
		stripped.SetLength(write);
		return stripped;
	}

	bool RuntimeTypeMatchesStableKey(const asCTypeInfo* candidate, const char* stableKey)
	{
		if( candidate == 0 || stableKey == 0 || stableKey[0] == 0 )
		{
			return false;
		}
		if( candidate->name.Equals(stableKey) )
		{
			return true;
		}
		if( candidate->nameSpace != 0 && candidate->nameSpace->name.GetLength() != 0 )
		{
			asCString qualified = candidate->nameSpace->name;
			qualified += "::";
			qualified += candidate->name;
			if( qualified.Equals(stableKey) )
			{
				return true;
			}
		}
		if( candidate->engine != 0 )
		{
			asCDataType formatted = asCDataType::CreateType(const_cast<asCTypeInfo*>(candidate), false);
			formatted.MakeHandle(false);
			formatted.MakeReference(false);
			formatted.MakeReadOnly(false);
			asSNameSpace* const ns = candidate->engine->nameSpaces.GetLength()
				? candidate->engine->nameSpaces[0]
				: candidate->nameSpace;
			const asCString nominalWithNs = RuntimeNominalDataTypeKey(formatted, ns, true);
			const asCString nominalBare = RuntimeNominalDataTypeKey(formatted, ns, false);
			if( formatted.Format(ns, true).Equals(stableKey)
				|| nominalWithNs.Equals(stableKey)
				|| nominalBare.Equals(stableKey) )
			{
				return true;
			}
			const asCString strippedKey = StripImplicitHandleMarks(asCString(stableKey));
			if( StripImplicitHandleMarks(nominalWithNs).Equals(strippedKey)
				|| StripImplicitHandleMarks(nominalBare).Equals(strippedKey)
				|| StripImplicitHandleMarks(formatted.Format(ns, true)).Equals(strippedKey) )
			{
				return true;
			}
		}
		return false;
	}

	void AddUniqueCandidate(asCArray<asCTypeInfo*>& candidates, asCTypeInfo* candidate, const char* stableKey)
	{
		if( RuntimeTypeMatchesStableKey(candidate, stableKey) && !candidates.Exists(candidate) )
		{
			candidates.PushLast(candidate);
		}
	}

	void FindRuntimeTypeCandidates(
		asCScriptEngine* engine,
		const char* stableKey,
		const asCArray<asCTypeInfo*>* transientTypes,
		asCArray<asCTypeInfo*>& outCandidates)
	{
		if( engine == 0 || stableKey == 0 || stableKey[0] == 0 )
		{
			return;
		}

		// Detached candidate types shadow published generations, but the
		// candidate set itself still has to be unambiguous.
		if( transientTypes )
		{
			for( asUINT i = 0; i < transientTypes->GetLength(); ++i )
			{
				AddUniqueCandidate(outCandidates, (*transientTypes)[i], stableKey);
			}
			if( outCandidates.GetLength() != 0 )
			{
				return;
			}
		}

		// Enumerate already-materialized Runtime types before invoking declaration
		// parsing. GetTypeInfoByDecl may instantiate a template and allocate an
		// Engine-local type id, which is forbidden while a candidate transaction is
		// still in validate-all/commit-none.
		for( asUINT i = 0; i < engine->GetObjectTypeCount(); ++i )
		{
			AddUniqueCandidate(outCandidates, static_cast<asCTypeInfo*>(engine->GetObjectTypeByIndex(i)), stableKey);
		}
		for( asUINT i = 0; i < engine->callableTypes.GetLength(); ++i )
		{
			AddUniqueCandidate(outCandidates, engine->callableTypes[i], stableKey);
		}
		for( asUINT m = 0; m < engine->scriptModules.GetLength(); ++m )
		{
			asCModule* const module = engine->scriptModules[m];
			if( module == 0 )
			{
				continue;
			}
			for( asUINT t = 0; t < module->classTypes.GetLength(); ++t )
			{
				AddUniqueCandidate(outCandidates, module->classTypes[t], stableKey);
			}
			for( asUINT t = 0; t < module->enumTypes.GetLength(); ++t )
			{
				AddUniqueCandidate(outCandidates, module->enumTypes[t], stableKey);
			}
			for( asUINT t = 0; t < module->typeDefs.GetLength(); ++t )
			{
				AddUniqueCandidate(outCandidates, module->typeDefs[t], stableKey);
			}
			for( int t = 0; t < module->templateInstances.Num(); ++t )
			{
				AddUniqueCandidate(outCandidates, module->templateInstances[t], stableKey);
			}
		}
		for( const auto& bucket : engine->templateInstanceBuckets )
		{
			for( asCObjectType* const instance : bucket.Value )
			{
				AddUniqueCandidate(outCandidates, instance, stableKey);
			}
		}
		if( outCandidates.GetLength() != 0 )
		{
			return;
		}

		AddUniqueCandidate(outCandidates,
			static_cast<asCTypeInfo*>(engine->GetTypeInfoByName(stableKey)),
			stableKey);
		if( outCandidates.GetLength() == 0 )
		{
			asCTypeInfo* const byDecl =
				static_cast<asCTypeInfo*>(engine->GetTypeInfoByDecl(stableKey));
			if( byDecl != 0 && !outCandidates.Exists(byDecl) )
			{
				// GetTypeInfoByDecl parsed the exact Canonical key. Template
				// instances keep the base name (`TSubclassOf`) while the sealed
				// key includes subtypes; do not reject that engine-owned match.
				outCandidates.PushLast(byDecl);
			}
		}
		if( outCandidates.GetLength() == 0 )
		{
			const char* const open = strchr(stableKey, '<');
			const asUINT keyLength = asCString(stableKey).GetLength();
			if( open != 0 && keyLength > 0 && stableKey[keyLength - 1] == '>' )
			{
				asCString baseName;
				baseName.Assign(stableKey, asUINT(open - stableKey));
				asCString innerName;
				innerName.Assign(
					open + 1,
					asUINT((stableKey + keyLength - 1) - (open + 1)));
				if( innerName.GetLength() != 0 && innerName[innerName.GetLength() - 1] == '@' )
				{
					innerName.SetLength(innerName.GetLength() - 1);
				}
				asCObjectType* const templateType = CastToObjectType(
					static_cast<asCTypeInfo*>(engine->GetTypeInfoByName(baseName.AddressOf())));
				asCTypeInfo* innerType =
					static_cast<asCTypeInfo*>(engine->GetTypeInfoByName(innerName.AddressOf()));
				if( innerType == 0 )
				{
					for( asUINT m = 0;
						innerType == 0 && m < engine->scriptModules.GetLength();
						++m )
					{
						asCModule* const module = engine->scriptModules[m];
						if( module == 0 )
						{
							continue;
						}
						for( asUINT t = 0; t < module->classTypes.GetLength(); ++t )
						{
							if( module->classTypes[t] != 0
								&& module->classTypes[t]->name.Equals(
									innerName.AddressOf()) )
							{
								innerType = module->classTypes[t];
								break;
							}
						}
					}
				}
				if( templateType != 0 && innerType != 0
					&& (templateType->GetFlags() & asOBJ_TEMPLATE) != 0 )
				{
					asCArray<asCDataType> subTypes;
					asCDataType subType = asCDataType::CreateType(innerType, false);
					if( (innerType->GetFlags() & asOBJ_REF) != 0
						&& (innerType->GetFlags() & asOBJ_IMPLICIT_HANDLE) == 0 )
					{
						subType.MakeHandle(true);
					}
					subTypes.PushLast(subType);
					asCObjectType* instance = engine->GetTemplateInstanceType(
						templateType, subTypes, innerType->module);
					if( instance == 0 && !subType.IsObjectHandle()
						&& (innerType->GetFlags() & asOBJ_REF) != 0 )
					{
						subType.MakeHandle(true);
						subTypes[0] = subType;
						instance = engine->GetTemplateInstanceType(
							templateType, subTypes, innerType->module);
					}
					if( instance != 0 && !outCandidates.Exists(instance) )
					{
						outCandidates.PushLast(instance);
					}
				}
			}
		}
	}

	int ExistingPublicTypeIdProjection(
		asCScriptEngine* engine,
		const asCDataType& dataType)
	{
		asCTypeInfo* const typeInfo = dataType.GetTypeInfo();
		if( typeInfo == 0 )
		{
			// Primitive ids are fixed constants and this call cannot mutate Engine
			// type-id state.
			return engine ? engine->GetTypeIdFromDataType(dataType) : -1;
		}
		int typeId = typeInfo->typeId;
		if( typeId < 0 )
			return -1;
		if( (typeInfo->GetFlags() & asOBJ_ASHANDLE) == 0 )
		{
			if( dataType.IsObjectHandle() )
				typeId |= asTYPEID_OBJHANDLE;
			if( dataType.IsHandleToConst() )
				typeId |= asTYPEID_HANDLETOCONST;
		}
		return typeId;
	}

	asEASTTypeKind RuntimeTypeKind(const asCTypeInfo* info)
	{
		if( info == 0 )
		{
			return asAST_TYPE_INVALID;
		}
		const asQWORD flags = info->GetFlags();
		if( (flags & asOBJ_CALLABLE_TYPE) != 0 )
		{
			return asAST_TYPE_CALLABLE_TYPE;
		}
		if( (flags & asOBJ_ENUM) != 0 )
		{
			return asAST_TYPE_ENUM;
		}
		if( (flags & asOBJ_TEMPLATE) != 0 )
		{
			return asAST_TYPE_TEMPLATE;
		}
		// Typedefs retain their nominal spelling in the AST but lower to the
		// underlying VM datatype at installation.
		if( (flags & asOBJ_TYPEDEF) != 0 || (flags & asOBJ_VALUE) != 0 )
		{
			return asAST_TYPE_VALUE_OBJECT;
		}
		if( (flags & asOBJ_REF) != 0 )
		{
			return asAST_TYPE_REFERENCE_OBJECT;
		}
		return asAST_TYPE_INVALID;
	}

#if 0 // Preserved dormant Canonical AST binding; not a supported configuration path.
	bool FindCanonicalTypeForDataType(
		const asCASTContext& context,
		const asCDataType& dataType,
		asCQualType& outType)
	{
		outType = asCQualType();
		asEASTTypeKind kind = asAST_TYPE_INVALID;
		asCString stableKey;
		asCTypeInfo* const typeInfo = dataType.GetTypeInfo();
		if( typeInfo )
		{
			kind = RuntimeTypeKind(typeInfo);
			if( typeInfo->nameSpace && typeInfo->nameSpace->name.GetLength() )
			{
				stableKey = typeInfo->nameSpace->name;
				stableKey += "::";
			}
			stableKey += typeInfo->name;
		}
		else if( dataType.GetTokenType() == ttVoid )
		{
			kind = asAST_TYPE_VOID;
			stableKey = "void";
		}
		else if( dataType.GetTokenType() == ttQuestion )
		{
			kind = asAST_TYPE_WILDCARD;
			stableKey = "?";
		}
		else
		{
			kind = asAST_TYPE_PRIMITIVE;
		}

		asDWORD qualifiers = 0;
		if( dataType.IsReadOnly() || dataType.IsHandleToConst() )
			qualifiers |= asAST_QUAL_CONST;
		if( dataType.IsObjectHandle() )
			qualifiers |= asAST_QUAL_HANDLE;
		if( dataType.IsReference() )
			qualifiers |= asAST_QUAL_REFERENCE;

		for( asUINT i = 1; i <= context.GetTypeCount(); ++i )
		{
			const asCType* const candidate = context.GetType(asASTTypeRef(i));
			if( candidate == 0 )
				continue;
			const bool kindMatches = candidate->kind == kind
				|| ((kind == asAST_TYPE_VALUE_OBJECT
						|| kind == asAST_TYPE_TEMPLATE)
					&& (candidate->kind == asAST_TYPE_VALUE_OBJECT
						|| candidate->kind == asAST_TYPE_TEMPLATE))
				|| ((kind == asAST_TYPE_VALUE_OBJECT
						|| kind == asAST_TYPE_REFERENCE_OBJECT)
					&& (candidate->kind == asAST_TYPE_VALUE_OBJECT
						|| candidate->kind == asAST_TYPE_REFERENCE_OBJECT));
			if( !kindMatches )
				continue;
			if( kind == asAST_TYPE_PRIMITIVE )
			{
				if( candidate->primitiveToken != dataType.GetTokenType() )
					continue;
			}
			else if( typeInfo != 0
				? !RuntimeTypeMatchesStableKey(typeInfo, candidate->stableKey.AddressOf())
				: !candidate->stableKey.Equals(stableKey) )
			{
				continue;
			}
			outType = asCQualType(candidate->id, qualifiers);
			return true;
		}
		return false;
	}

	asCDataType MakeResolvedDataType(const asCType& type, asDWORD qualifiers, asCTypeInfo* info)
	{
		asCDataType dataType;
		if( type.kind == asAST_TYPE_VOID )
		{
			dataType = (qualifiers & asAST_QUAL_HANDLE) != 0
				? asCDataType::CreateNullHandle()
				: asCDataType::CreatePrimitive(ttVoid, (qualifiers & asAST_QUAL_CONST) != 0);
		}
		else if( type.kind == asAST_TYPE_PRIMITIVE )
		{
			dataType = asCDataType::CreatePrimitive(type.primitiveToken, (qualifiers & asAST_QUAL_CONST) != 0);
		}
		else if( type.kind == asAST_TYPE_WILDCARD )
		{
			dataType = asCDataType::CreatePrimitive(ttQuestion, (qualifiers & asAST_QUAL_CONST) != 0);
		}
		else if( info != 0 && (info->GetFlags() & asOBJ_TYPEDEF) != 0 )
		{
			dataType = CastToTypedefType(info)->aliasForType;
			dataType.MakeReadOnly((qualifiers & asAST_QUAL_CONST) != 0);
		}
		else if( info != 0 )
		{
			dataType = asCDataType::CreateType(info, (qualifiers & asAST_QUAL_CONST) != 0);
			if( (info->GetFlags() & asOBJ_REF) != 0 || (info->GetFlags() & asOBJ_CALLABLE_TYPE) != 0 )
			{
				dataType.MakeHandle(true);
			}
		}

		if( (qualifiers & asAST_QUAL_HANDLE) != 0 )
		{
			dataType.MakeHandle(true);
		}
		if( (qualifiers & asAST_QUAL_REFERENCE) != 0 )
		{
			dataType.MakeReference(true);
		}
		return dataType;
	}

	void AppendFunctionSignature(asCString& out, const asIScriptFunction* function)
	{
		if( function == 0 )
		{
			out += "<missing>";
			return;
		}
		const char* const declaration = function->GetDeclaration(true, true, false);
		out += declaration ? declaration : "<unnamed>";
	}

	void BuildABISignatures(
		asCTypeInfo* typeInfo,
		const asCDataType& dataType,
		asCString& outLayout,
		asCString& outBehaviour)
	{
		outLayout.Format("size=%d;align=%d", dataType.GetSizeOfVariableBytes(), dataType.GetAlignment());
		outBehaviour = "";
		if( typeInfo == 0 )
		{
			return;
		}

		const asQWORD flags = typeInfo->GetFlags();
		asCString flagText;
		flagText.Format(";flags=%llu", static_cast<unsigned long long>(flags));
		outLayout += flagText;

		if( (flags & asOBJ_ENUM) != 0 )
		{
			asCEnumType* const enumType = CastToEnumType(typeInfo);
			for( asUINT i = 0; enumType && i < enumType->GetEnumValueCount(); ++i )
			{
				int value = 0;
				const char* const name = enumType->GetEnumValueByIndex(i, &value);
				asCString entry;
				entry.Format(";enum=%s:%d", name ? name : "", value);
				outLayout += entry;
			}
		}

		asCObjectType* const objectType = CastToObjectType(typeInfo);
		if( objectType )
		{
			for( asUINT i = 0; i < objectType->properties.GetLength(); ++i )
			{
				const asCObjectProperty* const property = objectType->properties[i];
				if( property == 0 )
				{
					outLayout += ";property=<missing>";
					continue;
				}
				asCString entry;
				entry.Format(";property=%s@%d+%d:%d:%d:type=",
					property->name.AddressOf(),
					property->byteOffset,
					property->compositeOffset,
					property->isCompositeIndirect ? 1 : 0,
					property->isInherited ? 1 : 0);
				outLayout += entry;
				outLayout += property->type.Format(typeInfo->nameSpace, true);
			}

			for( asUINT i = 0; i < objectType->GetMethodCount(); ++i )
			{
				outBehaviour += ";method=";
				AppendFunctionSignature(outBehaviour, objectType->GetMethodByIndex(i, true));
			}
			for( asUINT i = 0; i < objectType->GetBehaviourCount(); ++i )
			{
				asEBehaviours behaviour = asBEHAVE_CONSTRUCT;
				asIScriptFunction* const function = objectType->GetBehaviourByIndex(i, &behaviour);
				asCString entry;
				entry.Format(";behaviour=%d:", static_cast<int>(behaviour));
				outBehaviour += entry;
				AppendFunctionSignature(outBehaviour, function);
			}
			for( asUINT i = 0; i < objectType->templateSubTypes.GetLength(); ++i )
			{
				outLayout += ";subtype=";
				outLayout += objectType->templateSubTypes[i].Format(typeInfo->nameSpace, true);
			}
		}
		else if( (flags & asOBJ_CALLABLE_TYPE) != 0 )
		{
			outBehaviour += ";funcdef=";
			AppendFunctionSignature(outBehaviour, CastToCallableType(typeInfo)->callableSignature);
		}
		else if( (flags & asOBJ_TYPEDEF) != 0 )
		{
			outLayout += ";alias=";
			outLayout += CastToTypedefType(typeInfo)->aliasForType.Format(typeInfo->nameSpace, true);
		}
	}

	asQWORD HashBytes(asQWORD hash, const char* text)
	{
		if( text == 0 )
		{
			return hash;
		}
		for( const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text); *cursor; ++cursor )
		{
			hash ^= static_cast<asQWORD>(*cursor);
			hash *= 1099511628211ull;
		}
		return hash;
	}

	asQWORD HashABIKey(const asSTypeABIKey& key)
	{
		asQWORD hash = 1469598103934665603ull;
		hash = HashBytes(hash, key.stableTypeKey.AddressOf());
		hash = HashBytes(hash, key.targetProfileKey.AddressOf());
		hash = HashBytes(hash, key.nativeEnvironmentKey.AddressOf());
		hash = HashBytes(hash, key.layoutSignature.AddressOf());
		hash = HashBytes(hash, key.behaviourSignature.AddressOf());
		asCString scalar;
		scalar.Format(";%u;%u;%llu;%d;%d",
			static_cast<unsigned int>(key.typeKind),
			static_cast<unsigned int>(key.qualifiers),
			static_cast<unsigned long long>(key.objectFlags),
			key.byteSize,
			key.byteAlignment);
		return HashBytes(hash, scalar.AddressOf());
	}

	const asCType* FindCanonicalType(
		const asCASTContext& context,
		const asSTypeABIKey& expected,
		bool& foundWrongKind)
	{
		foundWrongKind = false;
		for( asUINT i = 1; i <= context.GetTypeCount(); ++i )
		{
			const asCType* const type = context.GetType(asASTTypeRef(i));
			if( type == 0 || !type->stableKey.Equals(expected.stableTypeKey) )
			{
				continue;
			}
			if( type->kind == expected.typeKind )
			{
				return type;
			}
			foundWrongKind = true;
		}
		return 0;
	}

	bool MemberKeyMatches(const char* ownerKey, const char* memberName, const char* expected)
	{
		if( memberName == 0 || expected == 0 )
		{
			return false;
		}
		if( ownerKey == 0 || ownerKey[0] == 0 )
		{
			return strcmp(memberName, expected) == 0;
		}
		asCString qualified = ownerKey;
		qualified += "::";
		qualified += memberName;
		return qualified.Equals(expected);
	}

	asCObjectProperty* ResolveProperty(
		asCTypeInfo* typeInfo,
		const char* stableOwnerKey,
		const char* stableMemberKey)
	{
		asCObjectType* const objectType = CastToObjectType(typeInfo);
		asCObjectProperty* result = 0;
		if( objectType == 0 )
		{
			return 0;
		}
		for( asUINT i = 0; i < objectType->properties.GetLength(); ++i )
		{
			asCObjectProperty* const candidate = objectType->properties[i];
			if( candidate && MemberKeyMatches(
				stableOwnerKey, candidate->name.AddressOf(), stableMemberKey) )
			{
				if( result != 0 && result != candidate )
				{
					return 0;
				}
				result = candidate;
			}
		}
		return result;
	}

	asCScriptFunction* ResolveFunction(
		asCScriptEngine* engine,
		asCTypeInfo* typeInfo,
		const char* stableOwnerKey,
		const char* stableMemberKey,
		const asCArray<asCScriptFunction*>* transientFunctions)
	{
		asCScriptFunction* result = 0;
		// Detached candidate functions are the prospective generation and shadow
		// currently published functions. Match them only by the complete canonical
		// declaration (or complete Runtime declaration for host shells); a numeric
		// FunctionId is merely the later candidate-local projection.
		if( transientFunctions )
		{
			for( asUINT i = 0; i < transientFunctions->GetLength(); ++i )
			{
				asCScriptFunction* const candidate = (*transientFunctions)[i];
				if( candidate == 0 )
					continue;
				const bool matchesCanonical =
					candidate->canonicalASTStableDeclKey.GetLength() != 0
					&& candidate->canonicalASTStableDeclKey.Equals(stableMemberKey);
				const char* const declaration =
					candidate->GetDeclaration(true, true, false);
				const bool matchesRuntime = declaration != 0
					&& strcmp(declaration, stableMemberKey) == 0;
				if( !matchesCanonical && !matchesRuntime )
					continue;
				if( result != 0 && result != candidate )
					return 0;
				result = candidate;
			}
			if( result != 0 )
				return result;
		}

		asCObjectType* const objectType = CastToObjectType(typeInfo);
		if( objectType )
		{
			for( asUINT i = 0; i < objectType->GetMethodCount(); ++i )
			{
				asCScriptFunction* const candidate = static_cast<asCScriptFunction*>(objectType->GetMethodByIndex(i, true));
				if( candidate && (MemberKeyMatches(stableOwnerKey, candidate->GetName(), stableMemberKey)
					|| (candidate->canonicalASTStableDeclKey.GetLength() != 0
						&& candidate->canonicalASTStableDeclKey.Equals(stableMemberKey))
					|| (candidate->GetDeclaration(true, true, false)
						&& strcmp(candidate->GetDeclaration(true, true, false), stableMemberKey) == 0)) )
				{
					if( result != 0 && result != candidate )
					{
						return 0;
					}
					result = candidate;
				}
			}
			if( result != 0 )
				return result;
		}

		if( engine )
		{
			for( asUINT i = 0; i < engine->GetGlobalFunctionCount(); ++i )
			{
				asCScriptFunction* const candidate = static_cast<asCScriptFunction*>(engine->GetGlobalFunctionByIndex(i));
				const char* const declaration = candidate
					? candidate->GetDeclaration(true, true, false) : 0;
				if( candidate
					&& ((candidate->canonicalASTStableDeclKey.GetLength() != 0
						&& candidate->canonicalASTStableDeclKey.Equals(stableMemberKey))
						|| (declaration != 0
							&& strcmp(declaration, stableMemberKey) == 0)) )
				{
					if( result != 0 && result != candidate )
					{
						return 0;
					}
					result = candidate;
				}
			}
		}
		return result;
	}

	int ResolveSignatureRuntimeCoordinate(
		const asSTypeRelocation& relocation,
		const asCArray<asCScriptFunction*>* transientFunctions,
		asCScriptFunction*& outOwner,
		asCTypeInfo*& outType,
		asCString* detail)
	{
		outOwner = 0;
		outType = 0;
		if( relocation.use != asTYPE_RELOC_FUNCTION_SIGNATURE
			|| relocation.ownerFunctionStableKey.GetLength() == 0
			|| relocation.signatureRole == asRUNTIME_TYPE_SIGNATURE_NONE
			|| transientFunctions == 0
			|| relocation.functionArtifactIndex >= transientFunctions->GetLength() )
		{
			SetDetail(detail, "invalid function signature binding coordinate",
				relocation.ownerFunctionStableKey.AddressOf());
			return asRUNTIME_TYPE_BIND_INVALID;
		}

		outOwner = (*transientFunctions)[relocation.functionArtifactIndex];
		if( outOwner == 0
			|| !outOwner->canonicalASTStableDeclKey.Equals(
				relocation.ownerFunctionStableKey) )
		{
			SetDetail(detail, "function signature owner identity mismatch",
				relocation.ownerFunctionStableKey.AddressOf());
			return asRUNTIME_TYPE_BIND_MEMBER_MISSING;
		}

		if( relocation.signatureRole == asRUNTIME_TYPE_SIGNATURE_RETURN )
		{
			if( relocation.formalIndex != asUINT(-1) )
				return asRUNTIME_TYPE_BIND_INVALID;
			outType = outOwner->returnType.GetTypeInfo();
		}
		else if( relocation.signatureRole
			== asRUNTIME_TYPE_SIGNATURE_PARAMETER )
		{
			if( relocation.formalIndex >= outOwner->parameterTypes.GetLength() )
			{
				SetDetail(detail, "function signature formal index out of range",
					relocation.ownerFunctionStableKey.AddressOf());
				return asRUNTIME_TYPE_BIND_INVALID;
			}
			outType = outOwner->parameterTypes[relocation.formalIndex].GetTypeInfo();
		}
		else if( relocation.signatureRole == asRUNTIME_TYPE_SIGNATURE_RECEIVER )
		{
			if( relocation.formalIndex != asUINT(-1)
				|| outOwner->objectType == 0 )
				return asRUNTIME_TYPE_BIND_INVALID;
			outType = outOwner->objectType;
		}
		else
		{
			return asRUNTIME_TYPE_BIND_INVALID;
		}
		if( outType == 0 )
		{
			SetDetail(detail, "function signature runtime type missing",
				relocation.ownerFunctionStableKey.AddressOf());
			return asRUNTIME_TYPE_BIND_ABI_MISMATCH;
		}
		return asRUNTIME_TYPE_BIND_OK;
	}
#endif
}

asSRuntimeTypeBindingEnvironment asMakeCurrentRuntimeTypeBindingEnvironment()
{
	asSRuntimeTypeBindingEnvironment environment;
	environment.targetProfileKey = "vm-bytecode.current";
	const char* const endian =
#ifdef AS_BIG_ENDIAN
		"big";
#else
		"little";
#endif
	environment.nativeEnvironmentKey.Format(
		"as=%u;ptr-dwords=%u;dword-bytes=%u;endian=%s",
		static_cast<unsigned int>(ANGELSCRIPT_VERSION),
		static_cast<unsigned int>(AS_PTR_SIZE),
		static_cast<unsigned int>(sizeof(asDWORD)),
		endian);
	return environment;
}

int asValidateRuntimeTypeIdOwner(
	const asSRuntimeTypeId& runtimeTypeId,
	const asSRuntimeTypeGeneration& expectedGeneration)
{
	if( !runtimeTypeId.IsValid() || !expectedGeneration.IsValid() )
		return asRUNTIME_TYPE_BIND_INVALID;
	return runtimeTypeId.Generation == expectedGeneration
		? asRUNTIME_TYPE_BIND_OK
		: asRUNTIME_TYPE_BIND_INVALID_OWNER;
}

int asRejectRuntimeTypeIdForDurableEncoding(const asSRuntimeTypeId& runtimeTypeId)
{
	return runtimeTypeId.IsValid()
		? asRUNTIME_TYPE_BIND_INVALID_DURABLE_ID
		: asRUNTIME_TYPE_BIND_INVALID;
}

int asProjectCanonicalTypeUse(
	const asCStableIdentityRegistry& registry,
	const asSStableKey& canonicalTypeUse,
	asSTypeABIKey& outKey)
{
	asEStableIdentityRole role = asEStableIdentityRole::Invalid;
	if( registry.ResolveIdentityRole(canonicalTypeUse, role)
		!= asETypeIdentityStatus::Succeeded || role != asEStableIdentityRole::TypeUse )
	{
		return asRUNTIME_TYPE_BIND_INVALID;
	}
	outKey.canonicalTypeUse = canonicalTypeUse;
	return asRUNTIME_TYPE_BIND_OK;
}

bool asSTypeABIKey::Equals(const asSTypeABIKey& other) const
{
	return canonicalTypeUse == other.canonicalTypeUse
		&& stableTypeKey.Equals(other.stableTypeKey)
		&& targetProfileKey.Equals(other.targetProfileKey)
		&& nativeEnvironmentKey.Equals(other.nativeEnvironmentKey)
		&& typeKind == other.typeKind
		&& qualifiers == other.qualifiers
		&& objectFlags == other.objectFlags
		&& byteSize == other.byteSize
		&& byteAlignment == other.byteAlignment
		&& layoutSignature.Equals(other.layoutSignature)
		&& behaviourSignature.Equals(other.behaviourSignature);
}

asCRuntimeTypeBindingTable::asCRuntimeTypeBindingTable()
	: frozen(false)
{
}

#if 0 // Preserved dormant Canonical AST binding; not a supported configuration path.
int asCRuntimeTypeBindingTable::ResolveCanonicalType(
	asCScriptEngine* engine,
	const asCASTContext& context,
	const asCQualType& canonicalType,
	asCDataType& outDataType,
	asCTypeInfo*& outTypeInfo,
	const asCArray<asCTypeInfo*>* transientTypes,
	asCString* detail)
{
	ClearDetail(detail);
	outDataType = asCDataType();
	outTypeInfo = 0;
	const asCType* const type = context.GetType(canonicalType.type);
	if( type == 0 || !canonicalType.IsValid() )
	{
		SetDetail(detail, "invalid canonical type", "");
		return asRUNTIME_TYPE_BIND_INVALID;
	}

	if( type->kind == asAST_TYPE_VOID || type->kind == asAST_TYPE_PRIMITIVE || type->kind == asAST_TYPE_WILDCARD )
	{
		outDataType = MakeResolvedDataType(*type, canonicalType.quals, 0);
		return outDataType.IsValid() ? asRUNTIME_TYPE_BIND_OK : asRUNTIME_TYPE_BIND_INVALID;
	}
	if( engine == 0 || type->stableKey.GetLength() == 0 )
	{
		SetDetail(detail, "missing runtime type", type->stableKey.AddressOf());
		return asRUNTIME_TYPE_BIND_MISSING;
	}

	asCArray<asCTypeInfo*> candidates;
	FindRuntimeTypeCandidates(engine, type->stableKey.AddressOf(), transientTypes, candidates);
	if( candidates.GetLength() == 0 )
	{
		SetDetail(detail, "missing runtime type", type->stableKey.AddressOf());
		return asRUNTIME_TYPE_BIND_MISSING;
	}
	if( candidates.GetLength() != 1 )
	{
		SetDetail(detail, "ambiguous runtime type", type->stableKey.AddressOf());
		return asRUNTIME_TYPE_BIND_AMBIGUOUS;
	}
	if( RuntimeTypeKind(candidates[0]) != type->kind )
	{
		const asEASTTypeKind runtimeKind = RuntimeTypeKind(candidates[0]);
		const bool templateValueAlias =
			(type->kind == asAST_TYPE_VALUE_OBJECT
				&& runtimeKind == asAST_TYPE_TEMPLATE)
			|| (type->kind == asAST_TYPE_TEMPLATE
				&& runtimeKind == asAST_TYPE_VALUE_OBJECT)
			|| (type->kind == asAST_TYPE_VALUE_OBJECT
				&& runtimeKind == asAST_TYPE_REFERENCE_OBJECT)
			|| (type->kind == asAST_TYPE_REFERENCE_OBJECT
				&& runtimeKind == asAST_TYPE_VALUE_OBJECT);
		if( !templateValueAlias )
		{
			SetDetail(detail, "wrong runtime type kind", type->stableKey.AddressOf());
			return asRUNTIME_TYPE_BIND_WRONG_KIND;
		}
	}

	outTypeInfo = candidates[0];
	outDataType = MakeResolvedDataType(*type, canonicalType.quals, outTypeInfo);
	if( !outDataType.IsValid() )
	{
		outTypeInfo = 0;
		SetDetail(detail, "invalid runtime datatype", type->stableKey.AddressOf());
		return asRUNTIME_TYPE_BIND_INVALID;
	}
	return asRUNTIME_TYPE_BIND_OK;
}

int asCRuntimeTypeBindingTable::CaptureExpectedABI(
	asCScriptEngine* engine,
	const asCASTContext& context,
	const asCQualType& canonicalType,
	const asSRuntimeTypeBindingEnvironment& environment,
	asSTypeABIKey& outKey,
	const asCArray<asCTypeInfo*>* transientTypes,
	asCString* detail)
{
	outKey = asSTypeABIKey();
	asCDataType dataType;
	asCTypeInfo* typeInfo = 0;
	const int result = ResolveCanonicalType(
		engine, context, canonicalType, dataType, typeInfo, transientTypes, detail);
	if( result != asRUNTIME_TYPE_BIND_OK )
	{
		return result;
	}

	const asCType* const type = context.GetType(canonicalType.type);
	outKey.stableTypeKey = type->stableKey;
	outKey.targetProfileKey = environment.targetProfileKey;
	outKey.nativeEnvironmentKey = environment.nativeEnvironmentKey;
	outKey.typeKind = type->kind;
	outKey.qualifiers = canonicalType.quals;
	outKey.objectFlags = typeInfo ? typeInfo->GetFlags() : 0;
	outKey.byteSize = dataType.GetSizeOfVariableBytes();
	outKey.byteAlignment = dataType.GetAlignment();
	BuildABISignatures(typeInfo, dataType, outKey.layoutSignature, outKey.behaviourSignature);
	outKey.layoutHash = HashABIKey(outKey);
	return asRUNTIME_TYPE_BIND_OK;
}

int asCRuntimeTypeBindingTable::CaptureExpectedABI(
	asCScriptEngine* engine,
	const asCASTContext& context,
	const asCDataType& dataType,
	const asSRuntimeTypeBindingEnvironment& environment,
	asSTypeABIKey& outKey,
	const asCArray<asCTypeInfo*>* transientTypes,
	asCString* detail)
{
	asCQualType canonicalType;
	if( !dataType.IsValid()
		|| !FindCanonicalTypeForDataType(context, dataType, canonicalType) )
	{
		outKey = asSTypeABIKey();
		SetDataTypeDetail(detail, "missing canonical type for runtime datatype", dataType);
		if( detail != 0 )
		{
			asCTypeInfo* const typeInfo = dataType.GetTypeInfo();
			asCString expected;
			expected.Format(
				" expected-kind=%d canonical-types={",
				static_cast<int>(RuntimeTypeKind(typeInfo)));
			*detail += expected;
			const asUINT diagnosticLimit = 16;
			asUINT described = 0;
			for( asUINT i = 1; i <= context.GetTypeCount() && described < diagnosticLimit; ++i )
			{
				const asCType* const candidate = context.GetType(asASTTypeRef(i));
				if( candidate == 0 )
					continue;
				asCString description;
				description.Format(
					"%s[id=%u kind=%d key=%s]",
					described == 0 ? "" : ",",
					candidate->id.value,
					static_cast<int>(candidate->kind),
					candidate->stableKey.AddressOf() ? candidate->stableKey.AddressOf() : "");
				*detail += description;
				++described;
			}
			if( context.GetTypeCount() > described )
			{
				asCString remainder;
				remainder.Format(",... total=%u", context.GetTypeCount());
				*detail += remainder;
			}
			*detail += "}";
		}
		return asRUNTIME_TYPE_BIND_MISSING;
	}
	return CaptureExpectedABI(
		engine, context, canonicalType, environment, outKey,
		transientTypes, detail);
}

int asCRuntimeTypeBindingTable::Build(
	asCScriptEngine* engine,
	const asCASTContext& context,
	const asCArray<asSTypeRelocation>& relocations,
	const asSRuntimeTypeBindingEnvironment& environment,
	const asCArray<asCTypeInfo*>* transientTypes,
	asCString* detail,
	const asCArray<asCScriptFunction*>* transientFunctions)
{
	ClearDetail(detail);
	if( frozen )
	{
		SetDetail(detail, "runtime binding table already frozen", "");
		return asRUNTIME_TYPE_BIND_ALREADY_FROZEN;
	}
	if( engine == 0 )
	{
		SetDetail(detail, "invalid runtime binding engine", "");
		return asRUNTIME_TYPE_BIND_INVALID;
	}

	asCArray<asSResolvedRuntimeTypeBinding> pending;
	for( asUINT i = 0; i < relocations.GetLength(); ++i )
	{
		const asSTypeRelocation& relocation = relocations[i];
		const asSTypeABIKey& expected = relocation.expectedType;
		if( expected.stableTypeKey.GetLength() == 0 || expected.typeKind == asAST_TYPE_INVALID )
		{
			SetDetail(detail, "invalid expected type ABI", expected.stableTypeKey.AddressOf());
			return asRUNTIME_TYPE_BIND_INVALID;
		}
		if( !expected.targetProfileKey.Equals(environment.targetProfileKey) )
		{
			SetDetail(detail, "wrong target profile", expected.stableTypeKey.AddressOf());
			return asRUNTIME_TYPE_BIND_WRONG_PROFILE;
		}
		if( !expected.nativeEnvironmentKey.Equals(environment.nativeEnvironmentKey) )
		{
			SetDetail(detail, "wrong native environment", expected.stableTypeKey.AddressOf());
			return asRUNTIME_TYPE_BIND_WRONG_NATIVE_ENVIRONMENT;
		}

		asCArray<asCTypeInfo*> exactSignatureTypes;
		asCScriptFunction* signatureOwner = 0;
		asCTypeInfo* signatureType = 0;
		const asCArray<asCTypeInfo*>* resolutionTypes = transientTypes;
		if( relocation.use == asTYPE_RELOC_FUNCTION_SIGNATURE )
		{
			const int coordinateResult = ResolveSignatureRuntimeCoordinate(
				relocation, transientFunctions, signatureOwner, signatureType,
				detail);
			if( coordinateResult != asRUNTIME_TYPE_BIND_OK )
				return coordinateResult;
			exactSignatureTypes.PushLast(signatureType);
			resolutionTypes = &exactSignatureTypes;
		}

		bool foundWrongKind = false;
		const asCType* const type = FindCanonicalType(context, expected, foundWrongKind);
		if( type == 0 )
		{
			SetDetail(detail, foundWrongKind ? "wrong canonical type kind" : "missing canonical type",
				expected.stableTypeKey.AddressOf());
			return foundWrongKind ? asRUNTIME_TYPE_BIND_WRONG_KIND : asRUNTIME_TYPE_BIND_MISSING;
		}

		asCQualType canonicalType(type->id, expected.qualifiers);
		asCDataType dataType;
		asCTypeInfo* typeInfo = 0;
		const int resolveResult = ResolveCanonicalType(
			engine, context, canonicalType, dataType, typeInfo, resolutionTypes, detail);
		if( resolveResult != asRUNTIME_TYPE_BIND_OK )
		{
			return resolveResult;
		}

		asSTypeABIKey actual;
		const int captureResult = CaptureExpectedABI(
			engine, context, canonicalType, environment, actual, resolutionTypes, detail);
		if( captureResult != asRUNTIME_TYPE_BIND_OK )
		{
			return captureResult;
		}
		if( !expected.Equals(actual) )
		{
			SetABIMismatchDetail(detail, expected, actual);
			return asRUNTIME_TYPE_BIND_ABI_MISMATCH;
		}

		asSResolvedRuntimeTypeBinding binding;
		binding.expectedKey = expected;
		binding.key = actual;
		binding.ownerFunctionStableKey = relocation.ownerFunctionStableKey;
		binding.relocationUse = relocation.use;
		binding.signatureRole = relocation.signatureRole;
		binding.formalIndex = relocation.formalIndex;
		binding.dataType = dataType;
		binding.typeInfo = typeInfo;
		binding.publicTypeId = ExistingPublicTypeIdProjection(engine, dataType);

		if( relocation.use == asTYPE_RELOC_FUNCTION_SIGNATURE )
		{
			binding.ownerFunction = signatureOwner;
			if( signatureType != binding.typeInfo )
			{
				SetDetail(detail, "function signature runtime type mismatch",
					relocation.ownerFunctionStableKey.AddressOf());
				return asRUNTIME_TYPE_BIND_ABI_MISMATCH;
			}
		}

		if( relocation.use == asTYPE_RELOC_PROPERTY )
		{
			binding.property = ResolveProperty(
				typeInfo,
				expected.stableTypeKey.AddressOf(),
				relocation.stableMemberKey.AddressOf());
			if( binding.property == 0 )
			{
				SetDetail(detail, "missing runtime property", relocation.stableMemberKey.AddressOf());
				return asRUNTIME_TYPE_BIND_MEMBER_MISSING;
			}
			binding.propertyOffset = binding.property->byteOffset;
		}
		else if( relocation.use == asTYPE_RELOC_FUNCTION
			|| relocation.use == asTYPE_RELOC_LIST_PATTERN_TARGET )
		{
			if( relocation.targetFunctionArtifactIndex != asUINT(-1) )
			{
				if( transientFunctions == 0
					|| relocation.targetFunctionArtifactIndex
						>= transientFunctions->GetLength() )
				{
					SetDetail(detail, "invalid target function ordinal",
						relocation.stableMemberKey.AddressOf());
					return asRUNTIME_TYPE_BIND_INVALID;
				}
				asCScriptFunction* const candidate =
					(*transientFunctions)[relocation.targetFunctionArtifactIndex];
				const char* const runtimeDeclaration = candidate
					? candidate->GetDeclaration(true, true, false) : 0;
				const bool matchesCanonical = candidate
					&& candidate->canonicalASTStableDeclKey.GetLength() != 0
					&& candidate->canonicalASTStableDeclKey.Equals(
						relocation.stableMemberKey);
				const bool matchesRuntime = runtimeDeclaration != 0
					&& strcmp(runtimeDeclaration,
						relocation.stableMemberKey.AddressOf()) == 0;
				if( !matchesCanonical && !matchesRuntime )
				{
					SetDetail(detail, "target function identity mismatch",
						relocation.stableMemberKey.AddressOf());
					return asRUNTIME_TYPE_BIND_MEMBER_MISSING;
				}
				binding.function = candidate;
			}
			else
			{
				binding.function = ResolveFunction(
					engine,
					typeInfo,
					expected.stableTypeKey.AddressOf(),
					relocation.stableMemberKey.AddressOf(),
					transientFunctions);
			}
			if( binding.function == 0 )
			{
				SetDetail(detail, "missing runtime function", relocation.stableMemberKey.AddressOf());
				return asRUNTIME_TYPE_BIND_MEMBER_MISSING;
			}
		}

		pending.PushLast(binding);
	}

	bindings.SwapWith(pending);
	frozen = true;
	return asRUNTIME_TYPE_BIND_OK;
}
#endif

int asCRuntimeTypeBindingTable::RefreshPublicTypeIdProjections(
	asCScriptEngine* engine)
{
	if( !frozen || engine == 0 )
		return asRUNTIME_TYPE_BIND_INVALID;
	for( asUINT i = 0; i < bindings.GetLength(); ++i )
	{
		bindings[i].publicTypeId = ExistingPublicTypeIdProjection(
			engine, bindings[i].dataType);
	}
	return asRUNTIME_TYPE_BIND_OK;
}

const asSResolvedRuntimeTypeBinding* asCRuntimeTypeBindingTable::GetBinding(asUINT index) const
{
	return frozen && index < bindings.GetLength() ? &bindings[index] : 0;
}

void asCRuntimeTypeBindingTable::SwapWith(
	asCRuntimeTypeBindingTable& other)
{
	bindings.SwapWith(other.bindings);
	const bool wasFrozen = frozen;
	frozen = other.frozen;
	other.frozen = wasFrozen;
}

END_AS_NAMESPACE
