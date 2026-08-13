#include "Cache/AngelscriptCacheEnvironment.h"

#include "Core/Artifacts/AngelscriptArtifactIdentity.h"
#include "Core/AngelscriptType.h"
#include "UObject/UObjectIterator.h"

#include "as_callfunc.h"
#include "as_datatype.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"

namespace AngelscriptCacheEnvironment_Private
{
	enum class EEnvironmentTypeShape : uint8
	{
		Invalid = 0,
		Object = 1,
		Enum = 2,
		Typedef = 3,
		Funcdef = 4,
	};

	static EEnvironmentTypeShape Classify(const asCTypeInfo& Type)
	{
		if (CastToObjectType(const_cast<asCTypeInfo*>(&Type)) != nullptr)
		{
			return EEnvironmentTypeShape::Object;
		}
		if (CastToEnumType(const_cast<asCTypeInfo*>(&Type)) != nullptr)
		{
			return EEnvironmentTypeShape::Enum;
		}
		if (CastToTypedefType(const_cast<asCTypeInfo*>(&Type)) != nullptr)
		{
			return EEnvironmentTypeShape::Typedef;
		}
		if (CastToFuncdefType(const_cast<asCTypeInfo*>(&Type)) != nullptr)
		{
			return EEnvironmentTypeShape::Funcdef;
		}
		return EEnvironmentTypeShape::Invalid;
	}

	static bool TryBuildCanonicalType(
		const asCTypeInfo& Type,
		FString& OutCanonicalType)
	{
		OutCanonicalType.Reset();
		if (Type.module != nullptr || Type.engine == nullptr
			|| Type.GetName() == nullptr || Type.GetName()[0] == '\0')
		{
			return false;
		}

		const asCDataType DataType = asCDataType::CreateType(
			const_cast<asCTypeInfo*>(&Type), false);
		const asCString Formatted = DataType.Format(nullptr, true, false);
		OutCanonicalType = UTF8_TO_TCHAR(Formatted.AddressOf());
		return !OutCanonicalType.IsEmpty();
	}

	static bool TryBuildTypeStableKey(
		const asCTypeInfo& Type,
		FAngelscriptHash256& OutStableKey,
		FString* OutCanonicalType = nullptr,
		EEnvironmentTypeShape* OutShape = nullptr)
	{
		OutStableKey = {};
		FString CanonicalType;
		if (!TryBuildCanonicalType(Type, CanonicalType))
		{
			return false;
		}
		const EEnvironmentTypeShape Shape = Classify(Type);
		if (Shape == EEnvironmentTypeShape::Invalid)
		{
			return false;
		}

		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("cache-v2-environment-type-key-v1"));
		Writer.WriteUInt8(static_cast<uint8>(Shape));
		Writer.WriteString(CanonicalType);
		OutStableKey = Writer.FinalizeHash();
		if (OutCanonicalType != nullptr)
		{
			*OutCanonicalType = MoveTemp(CanonicalType);
		}
		if (OutShape != nullptr)
		{
			*OutShape = Shape;
		}
		return !OutStableKey.IsZero();
	}

	static FString FormatTypeId(
		const asCScriptEngine& Engine,
		const int TypeId)
	{
		const asCDataType DataType = Engine.GetDataTypeFromTypeId(TypeId);
		const asCString Formatted = DataType.Format(nullptr, true, false);
		return UTF8_TO_TCHAR(Formatted.AddressOf());
	}

	static bool TryBuildTypeAbi(
		const asCTypeInfo& Type,
		const FAngelscriptHash256& StableKey,
		const FStringView CanonicalType,
		const EEnvironmentTypeShape Shape,
		FAngelscriptHash256& OutAbi)
	{
		OutAbi = {};
		if (Type.engine == nullptr || StableKey.IsZero()
			|| CanonicalType.IsEmpty())
		{
			return false;
		}

		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("cache-v2-environment-type-abi-v1"));
		Writer.WriteHash(StableKey);
		Writer.WriteString(CanonicalType);
		Writer.WriteUInt8(static_cast<uint8>(Shape));
		Writer.WriteUInt64(static_cast<uint64>(Type.flags));
		Writer.WriteUInt32(static_cast<uint32>(FMath::Max(Type.size, 0)));
		Writer.WriteUInt32(static_cast<uint32>(FMath::Max(
			asCDataType::CreateType(const_cast<asCTypeInfo*>(&Type), false).
				GetAlignment(), 0)));
		Writer.WriteUInt32(static_cast<uint32>(FMath::Max(
			Type.basePropertyOffset, 0)));

		if (const asCObjectType* ObjectType = CastToObjectType(
			const_cast<asCTypeInfo*>(&Type)))
		{
			Writer.WriteUInt32(ObjectType->GetSubTypeCount());
			for (asUINT Index = 0; Index < ObjectType->GetSubTypeCount(); ++Index)
			{
				Writer.WriteString(FormatTypeId(
					*Type.engine, ObjectType->GetSubTypeId(Index)));
			}

			Writer.WriteUInt32(ObjectType->GetPropertyCount());
			for (asUINT Index = 0; Index < ObjectType->GetPropertyCount(); ++Index)
			{
				const char* Name = nullptr;
				int PropertyTypeId = 0;
				bool bPrivate = false;
				bool bProtected = false;
				int Offset = 0;
				bool bReference = false;
				asDWORD AccessMask = 0;
				int CompositeOffset = 0;
				bool bCompositeIndirect = false;
				bool bConst = false;
				if (ObjectType->GetProperty(Index, &Name, &PropertyTypeId,
					&bPrivate, &bProtected, &Offset, &bReference,
					&AccessMask, &CompositeOffset, &bCompositeIndirect,
					&bConst) < 0)
				{
					return false;
				}
				Writer.WriteString(Name != nullptr
					? UTF8_TO_TCHAR(Name) : FString());
				Writer.WriteString(FormatTypeId(*Type.engine, PropertyTypeId));
				Writer.WriteUInt32(static_cast<uint32>(Offset));
				Writer.WriteUInt32(static_cast<uint32>(CompositeOffset));
				Writer.WriteBool(bPrivate);
				Writer.WriteBool(bProtected);
				Writer.WriteBool(bReference);
				Writer.WriteBool(bCompositeIndirect);
				Writer.WriteBool(bConst);
				Writer.WriteUInt32(AccessMask);
			}

			TArray<FString> Behaviours;
			for (asUINT Index = 0; Index < ObjectType->GetBehaviourCount(); ++Index)
			{
				asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
				const asIScriptFunction* Function =
					ObjectType->GetBehaviourByIndex(Index, &Behaviour);
				if (Function == nullptr)
				{
					return false;
				}
				Behaviours.Add(FString::Printf(TEXT("%u:%s"),
					static_cast<uint32>(Behaviour),
					UTF8_TO_TCHAR(Function->GetDeclaration(
						true, false, false))));
			}
			Behaviours.Sort([](const FString& Left, const FString& Right)
			{
				return FAngelscriptArtifactCanonicalWriter::
					CompareCanonicalUtf8Strings(Left, Right) < 0;
			});
			Writer.WriteUInt32(static_cast<uint32>(Behaviours.Num()));
			for (const FString& Behaviour : Behaviours)
			{
				Writer.WriteString(Behaviour);
			}
		}
		else if (const asCEnumType* EnumType = CastToEnumType(
			const_cast<asCTypeInfo*>(&Type)))
		{
			Writer.WriteUInt32(EnumType->GetEnumValueCount());
			for (asUINT Index = 0; Index < EnumType->GetEnumValueCount(); ++Index)
			{
				int Value = 0;
				const char* Name = EnumType->GetEnumValueByIndex(Index, &Value);
				Writer.WriteString(Name != nullptr
					? UTF8_TO_TCHAR(Name) : FString());
				Writer.WriteUInt32(static_cast<uint32>(Value));
			}
		}
		else if (const asCTypedefType* TypedefType = CastToTypedefType(
			const_cast<asCTypeInfo*>(&Type)))
		{
			Writer.WriteString(FormatTypeId(
				*Type.engine, TypedefType->GetTypedefTypeId()));
		}
		else if (const asCFuncdefType* FuncdefType = CastToFuncdefType(
			const_cast<asCTypeInfo*>(&Type)))
		{
			const asIScriptFunction* Signature =
				FuncdefType->GetFuncdefSignature();
			if (Signature == nullptr)
			{
				return false;
			}
			Writer.WriteString(UTF8_TO_TCHAR(Signature->GetDeclaration(
				true, false, false)));
		}
		else
		{
			return false;
		}

		OutAbi = Writer.FinalizeHash();
		return !OutAbi.IsZero();
	}

	static bool TryBuildFunctionStableKey(
		const asCScriptFunction& Function,
		FAngelscriptHash256& OutStableKey,
		FString* OutCanonicalDeclaration = nullptr,
		TOptional<FAngelscriptArtifactReference>* OutOwner = nullptr)
	{
		OutStableKey = {};
		if (OutCanonicalDeclaration != nullptr)
		{
			OutCanonicalDeclaration->Reset();
		}
		if (OutOwner != nullptr)
		{
			OutOwner->Reset();
		}
		if (Function.module != nullptr || Function.engine == nullptr
			|| Function.GetFuncType() != asFUNC_SYSTEM
			|| Function.GetName() == nullptr || Function.GetName()[0] == '\0')
		{
			return false;
		}

		const FString CanonicalDeclaration = UTF8_TO_TCHAR(
			Function.GetDeclaration(false, false, false));
		if (CanonicalDeclaration.IsEmpty())
		{
			return false;
		}
		TOptional<FAngelscriptArtifactReference> Owner;
		if (Function.objectType != nullptr)
		{
			FAngelscriptArtifactReference OwnerReference;
			if (!FAngelscriptCacheEnvironmentIdentity::TryBuildTypeReference(
				*Function.objectType, OwnerReference))
			{
				return false;
			}
			Owner = OwnerReference;
		}

		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("cache-v2-environment-function-key-v1"));
		Writer.WriteBool(Owner.IsSet());
		if (Owner.IsSet())
		{
			Writer.WriteHash(Owner->StableKey);
		}
		else
		{
			Writer.WriteString(UTF8_TO_TCHAR(Function.GetNamespace()));
		}
		Writer.WriteString(CanonicalDeclaration);
		OutStableKey = Writer.FinalizeHash();
		if (OutCanonicalDeclaration != nullptr)
		{
			*OutCanonicalDeclaration = CanonicalDeclaration;
		}
		if (OutOwner != nullptr)
		{
			*OutOwner = Owner;
		}
		return !OutStableKey.IsZero();
	}

	static bool TryBuildFunctionAbi(
		const asCScriptFunction& Function,
		const FAngelscriptHash256& StableKey,
		const FStringView CanonicalDeclaration,
		const TOptional<FAngelscriptArtifactReference>& Owner,
		FAngelscriptHash256& OutAbi)
	{
		OutAbi = {};
		if (StableKey.IsZero() || CanonicalDeclaration.IsEmpty()
			|| Function.sysFuncIntf == nullptr)
		{
			return false;
		}

		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("cache-v2-environment-function-abi-v1"));
		Writer.WriteHash(StableKey);
		Writer.WriteString(CanonicalDeclaration);
		Writer.WriteUInt32(Function.traits.traits);
		Writer.WriteUInt32(Function.accessMask);
		Writer.WriteUInt32(Function.exposedType);
		Writer.WriteBool(Function.dontCleanUpOnException);
		Writer.WriteBool(Owner.IsSet());
		if (Owner.IsSet())
		{
			Writer.WriteHash(Owner->ExpectedAbi);
		}

		auto WriteDataType = [&Writer](const asCDataType& Type)
		{
			const asCString Formatted = Type.Format(nullptr, true, false);
			Writer.WriteString(UTF8_TO_TCHAR(Formatted.AddressOf()));
		};
		WriteDataType(Function.returnType);
		Writer.WriteUInt32(Function.parameterTypes.GetLength());
		for (asUINT Index = 0; Index < Function.parameterTypes.GetLength(); ++Index)
		{
			WriteDataType(Function.parameterTypes[Index]);
			Writer.WriteUInt32(Index < Function.inOutFlags.GetLength()
				? static_cast<uint32>(Function.inOutFlags[Index]) : 0u);
		}

		const asSSystemFunctionInterface& Interface = *Function.sysFuncIntf;
		Writer.WriteUInt32(static_cast<uint32>(Interface.callConv));
		Writer.WriteUInt32(static_cast<uint32>(Interface.baseOffset));
		Writer.WriteUInt32(static_cast<uint32>(Interface.scriptReturnSize));
		Writer.WriteUInt32(static_cast<uint32>(Interface.paramSize));
		Writer.WriteUInt8(static_cast<uint8>(Interface.passFirstParamMetaData));
		Writer.WriteBool(Interface.func != nullptr);
		Writer.WriteBool(Interface.method != nullptr);
		if (Interface.caller.type < 0 || Interface.caller.type > 2)
		{
			return false;
		}
		Writer.WriteUInt8(static_cast<uint8>(Interface.caller.type));
		Writer.WriteUInt32(Interface.cleanArgs.GetLength());
		for (asUINT Index = 0; Index < Interface.cleanArgs.GetLength(); ++Index)
		{
			const asSSystemFunctionInterface::SClean& Clean =
				Interface.cleanArgs[Index];
			FAngelscriptArtifactReference CleanType;
			if (Clean.ot == nullptr
				|| !FAngelscriptCacheEnvironmentIdentity::TryBuildTypeReference(
					*Clean.ot, CleanType))
			{
				return false;
			}
			Writer.WriteHash(CleanType.StableKey);
			Writer.WriteHash(CleanType.ExpectedAbi);
			Writer.WriteUInt32(static_cast<uint32>(Clean.op));
			Writer.WriteUInt32(static_cast<uint32>(Clean.off));
		}

		OutAbi = Writer.FinalizeHash();
		return !OutAbi.IsZero();
	}

	static bool TryBuildPropertyStableKey(
		const asCTypeInfo& OwnerType,
		const asCObjectProperty& Property,
		FAngelscriptHash256& OutStableKey,
		FString* OutCanonicalType = nullptr,
		TOptional<FAngelscriptArtifactReference>* OutOwner = nullptr)
	{
		OutStableKey = {};
		if (OutCanonicalType != nullptr)
		{
			OutCanonicalType->Reset();
		}
		if (OutOwner != nullptr)
		{
			OutOwner->Reset();
		}

		const asCObjectType* ObjectType = CastToObjectType(
			const_cast<asCTypeInfo*>(&OwnerType));
		if (ObjectType == nullptr || OwnerType.module != nullptr
			|| OwnerType.engine == nullptr || Property.name.GetLength() == 0)
		{
			return false;
		}

		uint32 MatchingPropertyCount = 0;
		for (asUINT Index = 0; Index < ObjectType->properties.GetLength(); ++Index)
		{
			MatchingPropertyCount += ObjectType->properties[Index] == &Property
				? 1u : 0u;
		}
		if (MatchingPropertyCount != 1)
		{
			return false;
		}

		FAngelscriptArtifactReference Owner;
		if (!FAngelscriptCacheEnvironmentIdentity::TryBuildTypeReference(
			OwnerType, Owner))
		{
			return false;
		}
		const asCString FormattedType = Property.type.Format(
			OwnerType.nameSpace, false, false);
		const FString CanonicalType = UTF8_TO_TCHAR(
			FormattedType.AddressOf());
		if (CanonicalType.IsEmpty())
		{
			return false;
		}

		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("cache-v2-environment-property-key-v1"));
		Writer.WriteHash(Owner.StableKey);
		Writer.WriteString(UTF8_TO_TCHAR(Property.name.AddressOf()));
		Writer.WriteString(CanonicalType);
		OutStableKey = Writer.FinalizeHash();
		if (OutCanonicalType != nullptr)
		{
			*OutCanonicalType = CanonicalType;
		}
		if (OutOwner != nullptr)
		{
			*OutOwner = Owner;
		}
		return !OutStableKey.IsZero();
	}

	static bool TryBuildPropertyAbi(
		const asCObjectProperty& Property,
		const FAngelscriptHash256& StableKey,
		const FStringView CanonicalType,
		const TOptional<FAngelscriptArtifactReference>& Owner,
		FAngelscriptHash256& OutAbi)
	{
		OutAbi = {};
		if (StableKey.IsZero() || CanonicalType.IsEmpty() || !Owner.IsSet()
			|| Property.byteOffset < 0 || Property.compositeOffset < 0)
		{
			return false;
		}

		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("cache-v2-environment-property-abi-v1"));
		Writer.WriteHash(StableKey);
		Writer.WriteHash(Owner->ExpectedAbi);
		Writer.WriteString(CanonicalType);
		Writer.WriteUInt32(static_cast<uint32>(Property.byteOffset));
		Writer.WriteUInt32(static_cast<uint32>(Property.compositeOffset));
		Writer.WriteUInt32(Property.accessMask);
		Writer.WriteUInt32(Property.exposedType);
		Writer.WriteBool(Property.isCompositeIndirect);
		Writer.WriteBool(Property.isPrivate);
		Writer.WriteBool(Property.isProtected);
		Writer.WriteBool(Property.isInherited);
		Writer.WriteBool(Property.isAppBindProperty);
		OutAbi = Writer.FinalizeHash();
		return !OutAbi.IsZero();
	}
}

void FAngelscriptCacheEnvironmentIdentity::CollectCurrentRegisteredFunctions(
	const asCScriptEngine& Engine,
	TArray<asCScriptFunction*>& OutFunctions)
{
	TArray<asCObjectType*> CurrentObjectTypes;
	Engine.allRegisteredTypes.IterateAll(
		[&](asCTypeInfo* Type)
		{
			if (Type == nullptr || Type->engine != &Engine
				|| Type->module != nullptr)
			{
				return;
			}
			if (asCObjectType* const ObjectType = CastToObjectType(Type))
			{
				CurrentObjectTypes.Add(ObjectType);
			}
		});
	// These two engine-owned pseudo types are deliberately absent from
	// allRegisteredTypes, but their behaviours are part of the live compiler
	// environment for the entire engine lifetime.
	CurrentObjectTypes.AddUnique(
		const_cast<asCObjectType*>(&Engine.scriptTypeBehaviours));
	CurrentObjectTypes.AddUnique(
		const_cast<asCObjectType*>(&Engine.functionBehaviours));

	TSet<asCScriptFunction*> CurrentFunctions;
	auto AddCurrentFunction = [&Engine, &CurrentFunctions](
		asIScriptFunction* Candidate)
	{
		asCScriptFunction* const Function =
			static_cast<asCScriptFunction*>(Candidate);
		if (Function == nullptr || Function->engine != &Engine
			|| Function->module != nullptr
			|| Function->GetFuncType() != asFUNC_SYSTEM)
		{
			return;
		}
		CurrentFunctions.Add(Function);
	};

	for (asUINT Index = 0;
		Index < Engine.registeredGlobalFuncs.GetLength(); ++Index)
	{
		AddCurrentFunction(Engine.registeredGlobalFuncs[Index]);
	}

	for (asCObjectType* const ObjectType : CurrentObjectTypes)
	{
		for (asUINT Index = 0;
			Index < ObjectType->GetFactoryCount(); ++Index)
		{
			AddCurrentFunction(ObjectType->GetFactoryByIndex(Index));
		}
		for (asUINT Index = 0;
			Index < ObjectType->GetMethodCount(); ++Index)
		{
			AddCurrentFunction(ObjectType->GetMethodByIndex(Index));
		}
		for (asUINT Index = 0;
			Index < ObjectType->GetBehaviourCount(); ++Index)
		{
			AddCurrentFunction(ObjectType->GetBehaviourByIndex(Index, nullptr));
		}
	}

	OutFunctions = CurrentFunctions.Array();
	OutFunctions.Sort(
		[](const asCScriptFunction& Left, const asCScriptFunction& Right)
		{
			return Left.GetId() < Right.GetId();
		});
}

bool FAngelscriptCacheEnvironmentIdentity::TryBuildTypeReference(
	const asCTypeInfo& Type,
	FAngelscriptArtifactReference& OutReference)
{
	using namespace AngelscriptCacheEnvironment_Private;
	OutReference = {};
	FString CanonicalType;
	EEnvironmentTypeShape Shape = EEnvironmentTypeShape::Invalid;
	if (!TryBuildTypeStableKey(
		Type, OutReference.StableKey, &CanonicalType, &Shape)
		|| !TryBuildTypeAbi(Type, OutReference.StableKey,
			CanonicalType, Shape, OutReference.ExpectedAbi))
	{
		OutReference = {};
		return false;
	}
	OutReference.Kind = EAngelscriptArtifactReferenceKind::EnvironmentSymbol;
	return true;
}

bool FAngelscriptCacheEnvironmentIdentity::TryBuildFunctionReference(
	const asCScriptFunction& Function,
	FAngelscriptArtifactReference& OutReference)
{
	using namespace AngelscriptCacheEnvironment_Private;
	OutReference = {};
	FString CanonicalDeclaration;
	TOptional<FAngelscriptArtifactReference> Owner;
	if (!TryBuildFunctionStableKey(
		Function, OutReference.StableKey, &CanonicalDeclaration, &Owner)
		|| !TryBuildFunctionAbi(
			Function,
			OutReference.StableKey,
			CanonicalDeclaration,
			Owner,
			OutReference.ExpectedAbi))
	{
		OutReference = {};
		return false;
	}
	OutReference.Kind = EAngelscriptArtifactReferenceKind::EnvironmentSymbol;
	return true;
}

bool FAngelscriptCacheEnvironmentIdentity::TryBuildPropertyReference(
	const asCTypeInfo& OwnerType,
	const asCObjectProperty& Property,
	FAngelscriptArtifactReference& OutReference)
{
	using namespace AngelscriptCacheEnvironment_Private;
	OutReference = {};
	FString CanonicalType;
	TOptional<FAngelscriptArtifactReference> Owner;
	if (!TryBuildPropertyStableKey(
		OwnerType, Property, OutReference.StableKey,
		&CanonicalType, &Owner)
		|| !TryBuildPropertyAbi(
			Property, OutReference.StableKey,
			CanonicalType, Owner, OutReference.ExpectedAbi))
	{
		OutReference = {};
		return false;
	}
	OutReference.Kind = EAngelscriptArtifactReferenceKind::EnvironmentSymbol;
	return true;
}

bool FAngelscriptCacheEnvironmentIdentity::TryBuildCodeRootReference(
	const UClass& CodeRoot,
	FAngelscriptArtifactReference& OutReference)
{
	OutReference = {};
	const FString Path = CodeRoot.GetPathName();
	if (Path.IsEmpty())
	{
		return false;
	}
	FAngelscriptArtifactCanonicalWriter KeyWriter(
		TEXT("cache-v2-environment-code-root-key-v1"));
	KeyWriter.WriteString(Path);
	OutReference.StableKey = KeyWriter.FinalizeHash();

	FAngelscriptArtifactCanonicalWriter AbiWriter(
		TEXT("cache-v2-environment-code-root-declaration-abi-v1"));
	AbiWriter.WriteUInt32(2);
	AbiWriter.WriteString(Path);
	AbiWriter.WriteString(CodeRoot.GetSuperClass() != nullptr
		? CodeRoot.GetSuperClass()->GetPathName() : FString());
	OutReference.ExpectedAbi = AbiWriter.FinalizeHash();
	OutReference.Kind = EAngelscriptArtifactReferenceKind::EnvironmentSymbol;
	return !OutReference.StableKey.IsZero()
		&& !OutReference.ExpectedAbi.IsZero();
}

FAngelscriptCacheEngineEnvironmentResolver::
	FAngelscriptCacheEngineEnvironmentResolver(const asCScriptEngine& InEngine)
	: Engine(&InEngine)
{
}

TOptional<FAngelscriptCacheCurrentSymbol>
FAngelscriptCacheEngineEnvironmentResolver::Resolve(
	const EAngelscriptArtifactReferenceKind ReferenceKind,
	const FAngelscriptHash256& StableKey) const
{
	using namespace AngelscriptCacheEnvironment_Private;
	if (ReferenceKind != EAngelscriptArtifactReferenceKind::EnvironmentSymbol
		|| StableKey.IsZero() || Engine == nullptr)
	{
		return {};
	}

	TOptional<FAngelscriptCacheCurrentSymbol> Match;
	bool bAmbiguous = false;
	auto ConsiderReference = [&Match, &bAmbiguous, &StableKey](
		const FAngelscriptArtifactReference& Reference)
	{
		if (bAmbiguous || !(Reference.StableKey == StableKey))
		{
			return;
		}
		if (Match.IsSet())
		{
			bAmbiguous = true;
			Match.Reset();
			return;
		}
		FAngelscriptCacheCurrentSymbol Current;
		Current.CurrentAbi = Reference.ExpectedAbi;
		Current.CurrentContentOrValue = Reference.ExpectedAbi;
		Match = MoveTemp(Current);
	};
	for (TObjectIterator<UClass> It; !bAmbiguous && It; ++It)
	{
		FAngelscriptArtifactReference Reference;
		if (FAngelscriptCacheEnvironmentIdentity::TryBuildCodeRootReference(
			**It, Reference))
		{
			ConsiderReference(Reference);
		}
	}
	Engine->allRegisteredTypes.IterateAll(
		[&](asCTypeInfo* Type)
		{
			if (bAmbiguous || Type == nullptr || Type->module != nullptr)
			{
				return;
			}
			FAngelscriptArtifactReference Reference;
			if (!FAngelscriptCacheEnvironmentIdentity::TryBuildTypeReference(
				*Type, Reference))
			{
				return;
			}
			ConsiderReference(Reference);

			const asCObjectType* ObjectType = CastToObjectType(Type);
			if (ObjectType == nullptr)
			{
				return;
			}
			for (asUINT PropertyIndex = 0;
				!bAmbiguous
					&& PropertyIndex < ObjectType->properties.GetLength();
				++PropertyIndex)
			{
				const asCObjectProperty* Property =
					ObjectType->properties[PropertyIndex];
				FAngelscriptArtifactReference PropertyReference;
				if (Property != nullptr
					&& FAngelscriptCacheEnvironmentIdentity::
						TryBuildPropertyReference(
							*Type, *Property, PropertyReference))
				{
					ConsiderReference(PropertyReference);
				}
			}
		});
	TArray<asCScriptFunction*> CurrentFunctions;
	FAngelscriptCacheEnvironmentIdentity::CollectCurrentRegisteredFunctions(
		*Engine, CurrentFunctions);
	for (asCScriptFunction* Function : CurrentFunctions)
	{
		FAngelscriptArtifactReference Reference;
		if (FAngelscriptCacheEnvironmentIdentity::TryBuildFunctionReference(
			*Function, Reference))
		{
			ConsiderReference(Reference);
		}
	}
	return bAmbiguous ? TOptional<FAngelscriptCacheCurrentSymbol>() : Match;
}

asCTypeInfo* FAngelscriptCacheEngineEnvironmentResolver::ResolveTypeReference(
	const FAngelscriptArtifactReference& Expected) const
{
	if (Engine == nullptr
		|| Expected.Kind != EAngelscriptArtifactReferenceKind::EnvironmentSymbol
		|| Expected.StableKey.IsZero() || Expected.ExpectedAbi.IsZero())
	{
		return nullptr;
	}

	asCTypeInfo* Match = nullptr;
	bool bAmbiguous = false;
	Engine->allRegisteredTypes.IterateAll(
		[&](asCTypeInfo* Type)
		{
			if (bAmbiguous || Type == nullptr || Type->module != nullptr)
			{
				return;
			}
			FAngelscriptArtifactReference Current;
			if (!FAngelscriptCacheEnvironmentIdentity::TryBuildTypeReference(
					*Type, Current)
				|| Current.StableKey != Expected.StableKey
				|| Current.ExpectedAbi != Expected.ExpectedAbi)
			{
				return;
			}
			if (Match != nullptr)
			{
				Match = nullptr;
				bAmbiguous = true;
				return;
			}
			Match = Type;
		});
	return bAmbiguous ? nullptr : Match;
}

asCScriptFunction*
FAngelscriptCacheEngineEnvironmentResolver::ResolveFunctionReference(
	const FAngelscriptArtifactReference& Expected) const
{
	if (Engine == nullptr
		|| Expected.Kind != EAngelscriptArtifactReferenceKind::EnvironmentSymbol
		|| Expected.StableKey.IsZero() || Expected.ExpectedAbi.IsZero())
	{
		return nullptr;
	}

	asCScriptFunction* Match = nullptr;
	TArray<asCScriptFunction*> CurrentFunctions;
	FAngelscriptCacheEnvironmentIdentity::CollectCurrentRegisteredFunctions(
		*Engine, CurrentFunctions);
	for (asCScriptFunction* Function : CurrentFunctions)
	{
		FAngelscriptArtifactReference Current;
		if (!FAngelscriptCacheEnvironmentIdentity::TryBuildFunctionReference(
				*Function, Current)
			|| Current.StableKey != Expected.StableKey
			|| Current.ExpectedAbi != Expected.ExpectedAbi)
		{
			continue;
		}
		if (Match != nullptr)
		{
			return nullptr;
		}
		Match = Function;
	}
	return Match;
}

asCObjectProperty*
FAngelscriptCacheEngineEnvironmentResolver::ResolvePropertyReference(
	const FAngelscriptArtifactReference& Expected,
	asCTypeInfo** OutOwnerType) const
{
	if (OutOwnerType != nullptr)
	{
		*OutOwnerType = nullptr;
	}
	if (Engine == nullptr
		|| Expected.Kind != EAngelscriptArtifactReferenceKind::EnvironmentSymbol
		|| Expected.StableKey.IsZero() || Expected.ExpectedAbi.IsZero())
	{
		return nullptr;
	}

	asCObjectProperty* Match = nullptr;
	asCTypeInfo* MatchOwner = nullptr;
	bool bAmbiguous = false;
	Engine->allRegisteredTypes.IterateAll(
		[&](asCTypeInfo* Type)
		{
			if (bAmbiguous || Type == nullptr || Type->module != nullptr)
			{
				return;
			}
			asCObjectType* const ObjectType = CastToObjectType(Type);
			if (ObjectType == nullptr)
			{
				return;
			}
			for (asUINT Index = 0;
				!bAmbiguous && Index < ObjectType->properties.GetLength();
				++Index)
			{
				asCObjectProperty* const Property = ObjectType->properties[Index];
				FAngelscriptArtifactReference Current;
				if (Property == nullptr
					|| !FAngelscriptCacheEnvironmentIdentity::
						TryBuildPropertyReference(*Type, *Property, Current)
					|| Current.StableKey != Expected.StableKey
					|| Current.ExpectedAbi != Expected.ExpectedAbi)
				{
					continue;
				}
				if (Match != nullptr)
				{
					Match = nullptr;
					MatchOwner = nullptr;
					bAmbiguous = true;
					return;
				}
				Match = Property;
				MatchOwner = Type;
			}
		});
	if (!bAmbiguous && Match != nullptr && OutOwnerType != nullptr)
	{
		*OutOwnerType = MatchOwner;
	}
	return bAmbiguous ? nullptr : Match;
}

UClass* FAngelscriptCacheEngineEnvironmentResolver::ResolveCodeRootReference(
	const FAngelscriptArtifactReference& Expected) const
{
	if (Engine == nullptr
		|| Expected.Kind != EAngelscriptArtifactReferenceKind::EnvironmentSymbol
		|| Expected.StableKey.IsZero() || Expected.ExpectedAbi.IsZero())
	{
		return nullptr;
	}

	UClass* Match = nullptr;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		FAngelscriptArtifactReference Current;
		if (!FAngelscriptCacheEnvironmentIdentity::TryBuildCodeRootReference(
				**It, Current)
			|| Current.StableKey != Expected.StableKey
			|| Current.ExpectedAbi != Expected.ExpectedAbi)
		{
			continue;
		}
		if (Match != nullptr)
		{
			return nullptr;
		}
		Match = *It;
	}
	return Match;
}

FAngelscriptCacheEngineLayoutResolver::FAngelscriptCacheEngineLayoutResolver(
	const asCScriptEngine& InEngine)
	: Engine(&InEngine)
{
}

TOptional<FAngelscriptCacheResolvedDataTypeLayout>
FAngelscriptCacheEngineLayoutResolver::ResolveDataTypeLayout(
	const FAngelscriptCachedDataType& DataType,
	const IAngelscriptCacheProspectiveTypeLayoutView&) const
{
	if (Engine == nullptr
		|| DataType.Kind != EAngelscriptCachedDataTypeKind::EnvironmentType
		|| !DataType.TypeReference.IsSet())
	{
		return {};
	}
	const FAngelscriptArtifactReference& Expected =
		DataType.TypeReference.GetValue();
	if (Expected.Kind != EAngelscriptArtifactReferenceKind::EnvironmentSymbol
		|| Expected.StableKey.IsZero() || Expected.ExpectedAbi.IsZero())
	{
		return {};
	}

	TOptional<FAngelscriptCacheResolvedDataTypeLayout> Match;
	bool bAmbiguous = false;
	Engine->allRegisteredTypes.IterateAll(
		[&](asCTypeInfo* Type)
		{
			if (bAmbiguous || Type == nullptr || Type->module != nullptr)
			{
				return;
			}
			FAngelscriptArtifactReference Current;
			if (!FAngelscriptCacheEnvironmentIdentity::TryBuildTypeReference(
					*Type, Current)
				|| !(Current.StableKey == Expected.StableKey)
				|| !(Current.ExpectedAbi == Expected.ExpectedAbi))
			{
				return;
			}
			const asCDataType VmType = asCDataType::CreateType(Type, false);
			const int32 Size = VmType.GetSizeInMemoryBytes();
			const int32 Alignment = VmType.GetAlignment();
			if (Size <= 0 || Alignment <= 0 || !FMath::IsPowerOfTwo(Alignment))
			{
				bAmbiguous = true;
				Match.Reset();
				return;
			}

			FAngelscriptCacheResolvedDataTypeLayout Candidate;
			const uint32 ObjectHandleFlag = static_cast<uint32>(
				EAngelscriptCachedTypeQualifierFlags::ObjectHandle);
			if ((DataType.QualifierFlags & ObjectHandleFlag) != 0)
			{
				const FAngelscriptCacheV1StorageLayout Handle =
					FAngelscriptCacheTypeSchemaArchive::
						GetV1BuildLayoutConstants().
						GetObjectHandleStorageLayout();
				Candidate.StorageKind =
					EAngelscriptCachedPropertyStorageKind::ObjectHandle;
				Candidate.SemanticStorageSize = Handle.SemanticStorageSize;
				Candidate.SemanticStorageAlignment =
					Handle.SemanticStorageAlignment;
			}
			else
			{
				Candidate.StorageKind =
					EAngelscriptCachedPropertyStorageKind::InlineValue;
				Candidate.SemanticStorageSize = static_cast<uint32>(Size);
				Candidate.SemanticStorageAlignment =
					static_cast<uint32>(Alignment);
			}
			if (Match.IsSet()
				&& (Match->StorageKind != Candidate.StorageKind
					|| Match->SemanticStorageSize
						!= Candidate.SemanticStorageSize
					|| Match->SemanticStorageAlignment
						!= Candidate.SemanticStorageAlignment))
			{
				bAmbiguous = true;
				Match.Reset();
				return;
			}
			Match = Candidate;
		});
	return bAmbiguous
		? TOptional<FAngelscriptCacheResolvedDataTypeLayout>() : Match;
}

TOptional<FAngelscriptCacheResolvedTypeLayoutInput>
FAngelscriptCacheEngineLayoutResolver::ResolveTypeLayoutInput(
	const EAngelscriptCachedTypeLayoutInputKind InputKind,
	const EAngelscriptArtifactReferenceKind ReferenceKind,
	const FAngelscriptHash256& StableKey) const
{
	if (Engine == nullptr
		|| InputKind != EAngelscriptCachedTypeLayoutInputKind::CodeRoot
		|| ReferenceKind != EAngelscriptArtifactReferenceKind::EnvironmentSymbol
		|| StableKey.IsZero())
	{
		return {};
	}

	TOptional<FAngelscriptCacheResolvedTypeLayoutInput> Match;
	bool bAmbiguous = false;
	for (TObjectIterator<UClass> It; !bAmbiguous && It; ++It)
	{
		FAngelscriptArtifactReference Reference;
		if (!FAngelscriptCacheEnvironmentIdentity::TryBuildCodeRootReference(
				**It, Reference)
			|| !(Reference.StableKey == StableKey))
		{
			continue;
		}
		const FString BoundName = FAngelscriptType::GetBoundClassName(*It);
		asCTypeInfo* ShadowType =
			Engine->allRegisteredTypesByName.FindFirst_CaseInsensitive(
				TCHAR_TO_ANSI(*BoundName));
		if (ShadowType == nullptr || ShadowType->alignment <= 0
			|| !FMath::IsPowerOfTwo(ShadowType->alignment)
			|| It->GetPropertiesSize() < 0)
		{
			bAmbiguous = true;
			Match.Reset();
			break;
		}
		FAngelscriptCacheResolvedTypeLayoutInput Candidate;
		Candidate.BoundaryContribution =
			static_cast<uint32>(It->GetPropertiesSize());
		Candidate.AlignmentContribution =
			static_cast<uint32>(ShadowType->alignment);
		if (Match.IsSet()
			&& (Match->BoundaryContribution
					!= Candidate.BoundaryContribution
				|| Match->AlignmentContribution
					!= Candidate.AlignmentContribution))
		{
			bAmbiguous = true;
			Match.Reset();
			break;
		}
		Match = Candidate;
	}
	return bAmbiguous
		? TOptional<FAngelscriptCacheResolvedTypeLayoutInput>() : Match;
}
