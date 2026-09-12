#pragma once

#include "as_module_definition_set.h"
#include "as_engine_compile_registration.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"
#include "as_property.h"
#include "frontend/as_type_context.h"

#include "Templates/SharedPointer.h"

#include <memory>

struct FDetachedDefinitionFixture
{
	TSharedRef<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe> Context =
		MakeShared<AS_NAMESPACE_QUALIFIER asCTypeContext, ESPMode::ThreadSafe>();
	TUniquePtr<asCModuleDefinitionSet> Set = asCModuleDefinitionSet::Create();
	FString Scope = TEXT("vm-detached");

	bool BindContext()
	{
		return Set && Set->SetIdentityContext(Context) == asEMetadataResult::Succeeded;
	}

	asCObjectType* DefineType(const TCHAR* Name, const TCHAR* Namespace, asQWORD Flags)
	{
		if (!BindContext()) return nullptr;
		AS_NAMESPACE_QUALIFIER asSNominalTypeDescriptor Descriptor;
		Descriptor.StableScope = Scope;
		Descriptor.Name = Name;
		Descriptor.Kind = (Flags & asOBJ_VALUE)
			? AS_NAMESPACE_QUALIFIER asETypeDeclKind::Struct
			: AS_NAMESPACE_QUALIFIER asETypeDeclKind::Class;
		if (Namespace && *Namespace)
		{
			FString(Namespace).ParseIntoArray(Descriptor.NamespacePath, TEXT("::"), true);
		}
		AS_NAMESPACE_QUALIFIER asSStableKey Key;
		asCObjectType* Type = nullptr;
		if (Context->GetIdentityRegistry().InternNominalType(Descriptor, Key) != AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
		{
			return nullptr;
		}
		const auto Utf8Name = StringCast<ANSICHAR>(Name);
		const auto Utf8Namespace = StringCast<ANSICHAR>(Namespace ? Namespace : TEXT(""));
		if (Set->CreateObjectType(Key, Utf8Name.Get(), Utf8Namespace.Get(), Flags, Type) != asEMetadataResult::Succeeded)
		{
			return nullptr;
		}
		return Type;
	}

	asCScriptFunction* DefineFunction(
		const TCHAR* Name,
		const TCHAR* Namespace,
		asEFuncType Kind,
		const asCDataType& ReturnType,
		TConstArrayView<asSMetadataParameter> Parameters,
		asCObjectType* Owner = nullptr,
		AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind IdentityKind = AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Global)
	{
		if (!BindContext()) return nullptr;
		AS_NAMESPACE_QUALIFIER asSFunctionIdentityDescriptor Descriptor;
		Descriptor.StableScope = Scope;
		Descriptor.Name = Name;
		Descriptor.Kind = Owner
			? (IdentityKind == AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Global
				? AS_NAMESPACE_QUALIFIER asEFunctionIdentityKind::Method
				: IdentityKind)
			: IdentityKind;
		if (Owner) Descriptor.Owner = Owner->GetStableKey();
		if (Namespace && *Namespace)
		{
			FString(Namespace).ParseIntoArray(Descriptor.NamespacePath, TEXT("::"), true);
		}
		if (!InternDataType(ReturnType, Descriptor.ReturnType)) return nullptr;
		for (const asSMetadataParameter& Parameter : Parameters)
		{
			AS_NAMESPACE_QUALIFIER asSStableParameter Stable;
			asCDataType ParameterType = Parameter.Type;
			if (Parameter.Flags != asTM_NONE && !ParameterType.IsReference())
			{
				ParameterType.MakeReference(true);
			}
			if (!InternDataType(ParameterType, Stable.Type)) return nullptr;
			if (Parameter.Flags == asTM_INREF) Stable.PassingMode = AS_NAMESPACE_QUALIFIER asEParameterPassingMode::In;
			else if (Parameter.Flags == asTM_OUTREF) Stable.PassingMode = AS_NAMESPACE_QUALIFIER asEParameterPassingMode::Out;
			else if (Parameter.Flags == asTM_INOUTREF) Stable.PassingMode = AS_NAMESPACE_QUALIFIER asEParameterPassingMode::InOut;
			Descriptor.Parameters.Add(Stable);
		}
		AS_NAMESPACE_QUALIFIER asSStableKey Key;
		if (Context->GetIdentityRegistry().InternFunction(Descriptor, Key) != AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded)
		{
			return nullptr;
		}
		asCScriptFunction* Function = nullptr;
		const auto Utf8Name = StringCast<ANSICHAR>(Name);
		const auto Utf8Namespace = StringCast<ANSICHAR>(Namespace ? Namespace : TEXT(""));
		if (Set->CreateFunction(Key, Utf8Name.Get(), Utf8Namespace.Get(), Kind, Function) != asEMetadataResult::Succeeded)
		{
			return nullptr;
		}
		if (Set->DefineFunction(*Function, ReturnType, Parameters, Owner) != asEMetadataResult::Succeeded)
		{
			return nullptr;
		}
		if (Set->SetFunctionDeclarationFacts(*Function, Descriptor.Kind, Owner) != asEMetadataResult::Succeeded)
		{
			return nullptr;
		}
		return Function;
	}

	asCObjectProperty* DefineProperty(asCObjectType& Owner, const char* Name, const asCDataType& Type)
	{
		asCObjectProperty* Property = nullptr;
		if (Set->AddProperty(Owner, Name, Type, Property) != asEMetadataResult::Succeeded)
		{
			return nullptr;
		}
		return Property;
	}

	bool FinalizeAndFreeze()
	{
		if (!Set) return false;
		if (!(Set->FinalizeLayouts() == asEMetadataResult::Succeeded
			&& Set->Freeze() == asEMetadataResult::Succeeded
			&& Set->ValidateFrozenDefinitions()))
		{
			return false;
		}
		return Set.IsValid() && Set->IsImmutable();
	}

	TUniquePtr<asCModuleDefinitionSet> TakeSet()
	{
		return MoveTemp(Set);
	}

	void Reset()
	{
		Set.Reset();
	}

private:
	bool InternDataType(const asCDataType& Type, AS_NAMESPACE_QUALIFIER asSStableKey& OutKey)
	{
		AS_NAMESPACE_QUALIFIER asSStableTypeUseDescriptor Descriptor;
		if (Type.IsReadOnly() || Type.IsHandleToConst())
		{
			Descriptor.Qualifiers = Descriptor.Qualifiers | AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Const;
		}
		if (Type.IsReference())
		{
			Descriptor.Qualifiers = Descriptor.Qualifiers | AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Reference;
		}
		if (Type.IsObjectHandle())
		{
			Descriptor.Qualifiers = Descriptor.Qualifiers | AS_NAMESPACE_QUALIFIER asETypeUseQualifiers::Handle;
		}
		if (Type.GetTypeInfo())
		{
			Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Nominal;
			Descriptor.Declaration = Type.GetTypeInfo()->GetStableKey();
		}
		else
		{
			Descriptor.Kind = AS_NAMESPACE_QUALIFIER asETypeUseKind::Primitive;
			switch (Type.GetTokenType())
			{
			case ttInt8: Descriptor.Primitive = AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int8; break;
			case ttInt16: Descriptor.Primitive = AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int16; break;
			case ttInt: Descriptor.Primitive = AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int32; break;
			case ttInt64: Descriptor.Primitive = AS_NAMESPACE_QUALIFIER asEPrimitiveType::Int64; break;
			case ttUInt8: Descriptor.Primitive = AS_NAMESPACE_QUALIFIER asEPrimitiveType::UInt8; break;
			case ttUInt16: Descriptor.Primitive = AS_NAMESPACE_QUALIFIER asEPrimitiveType::UInt16; break;
			case ttUInt: Descriptor.Primitive = AS_NAMESPACE_QUALIFIER asEPrimitiveType::UInt32; break;
			case ttFloat32: Descriptor.Primitive = AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float32; break;
			case ttFloat64: Descriptor.Primitive = AS_NAMESPACE_QUALIFIER asEPrimitiveType::Float64; break;
			case ttBool: Descriptor.Primitive = AS_NAMESPACE_QUALIFIER asEPrimitiveType::Bool; break;
			case ttVoid: Descriptor.Primitive = AS_NAMESPACE_QUALIFIER asEPrimitiveType::Void; break;
			default: return false;
			}
		}
		return Context->GetIdentityRegistry().InternTypeUse(Descriptor, OutKey) == AS_NAMESPACE_QUALIFIER asETypeIdentityStatus::Succeeded;
	}
};

inline asEMetadataRegistrationResult RegisterCompiledDefinitions(
	asCScriptEngine& Engine,
	TUniquePtr<asCModuleDefinitionSet> Set)
{
	asCEngineCompileRegistration Registration(Engine);
	TArray<TUniquePtr<asCModuleDefinitionSet>> Units;
	if (Set) Units.Add(MoveTemp(Set));
	return Registration.Register(MoveTemp(Units));
}
