#include "as_definitions.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_scriptfunction.h"
#include "as_memory.h"
#include "Containers/Set.h"
#include <cstring>

BEGIN_AS_NAMESPACE
namespace asMetadataAccess
{
	bool ValidName(const asCString& Name)
	{
		return Name.GetLength() && Name.GetLength() <= 4096 && std::memchr(Name.AddressOf(), 0, Name.GetLength()) == nullptr;
	}
	bool ValidPolicy(const asSAccessSpecifier& Policy)
	{
		if (!ValidName(Policy.name) || (Policy.bIsPrivate && Policy.bIsProtected) || Policy.permissions.GetLength() > 4096) return false;
		for (asUINT Index = 0; Index < Policy.permissions.GetLength(); ++Index)
			if (!ValidName(Policy.permissions[Index].accessName)) return false;
		return true;
	}
	TArray<uint8> Encode(const asSAccessSpecifier& Policy)
	{
		TArray<uint8> Bytes;
		auto UInt = [&](uint32 Value) { for (int Shift = 0; Shift < 32; Shift += 8) Bytes.Add(uint8(Value >> Shift)); };
		auto Name = [&](const asCString& Value) { UInt(Value.GetLength()); Bytes.Append(reinterpret_cast<const uint8*>(Value.AddressOf()), Value.GetLength()); };
		Name(Policy.name);
		Bytes.Add(uint8(Policy.bIsPrivate) | (uint8(Policy.bIsProtected) << 1) | (uint8(Policy.bAnyReadOnly) << 2) | (uint8(Policy.bAnyEditDefaults) << 3));
		UInt(Policy.permissions.GetLength());
		for (asUINT Index = 0; Index < Policy.permissions.GetLength(); ++Index)
		{
			const auto& Permission = Policy.permissions[Index]; Name(Permission.accessName);
			Bytes.Add(uint8(Permission.bInherited) | (uint8(Permission.bReadOnly) << 1) | (uint8(Permission.bEditDefaults) << 2));
		}
		return Bytes;
	}
}

bool asCDefinitions::CanUseAccessSpecifier(const asCObjectType& Owner, const asSAccessSpecifier* Policy) const
{
	if (!Policy) return true;
	TSet<const asCObjectType*> Visited;
	for (const asCObjectType* Type = &Owner; Type && Visited.Num() < 256 && !Visited.Contains(Type); Type = Type->derivedFrom)
	{
		if (!CanReference(Type)) return false;
		Visited.Add(Type);
		for (asUINT Index = 0; Index < Type->accessSpecifiers.GetLength(); ++Index)
			if (Type->accessSpecifiers[Index] == Policy) return true;
	}
	return false;
}

asEDefinitionResult asCDefinitions::DefineAccessSpecifier(asCObjectType& Owner, const asSAccessSpecifier& Definition, asSAccessSpecifier*& Out)
{
	Out = nullptr;
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Owner)) return asEDefinitionResult::ForeignOwner;
	if (!asMetadataAccess::ValidPolicy(Definition)) return asEDefinitionResult::InvalidArgument;
	if (Owner.GetAccessSpecifier(Definition.name.AddressOf())) return asEDefinitionResult::DuplicateName;
	Out = asNEW(asSAccessSpecifier);
	*Out = Definition;
	Owner.accessSpecifiers.PushLast(Out);
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::SetPropertyAccess(asCObjectType& Owner, asCObjectProperty& Property, asSAccessSpecifier& Policy)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Owner) || !CanUseAccessSpecifier(Owner, &Policy)) return asEDefinitionResult::ForeignOwner;
	bool Found = false;
	for (asUINT Index = 0; Index < Owner.localProperties.GetLength(); ++Index) Found |= Owner.localProperties[Index] == &Property;
	if (!Found) return asEDefinitionResult::ForeignOwner;
	Property.accessSpecifier = &Policy;
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

asEDefinitionResult asCDefinitions::SetFunctionAccess(asCScriptFunction& Function, asSAccessSpecifier& Policy)
{
	std::lock_guard<std::mutex> Lock(MutationMutex);
	if (GetState() != asEDefinitionState::Building) return asEDefinitionResult::InvalidState;
	if (!Owns(&Function) || !Function.objectType || !CanUseAccessSpecifier(*Function.objectType, &Policy)) return asEDefinitionResult::ForeignOwner;
	Function.accessSpecifier = &Policy;
	InvalidateLayouts();
	return asEDefinitionResult::Succeeded;
}

bool asCDefinitions::CollectAccessSnapshot(TMap<const asSAccessSpecifier*, asSAccessPolicySnapshot>& Policies,
	TMap<const asCObjectProperty*, asSMemberAccessSnapshot>& Properties,
	TMap<const asCScriptFunction*, asSMemberAccessSnapshot>& Methods) const
{
	for (const asCTypeInfo* Type : Types)
	{
		const asCObjectType* Object = CastToObjectType(const_cast<asCTypeInfo*>(Type));
		if (!Object) continue;
		TSet<FString> Names;
		for (asUINT Index = 0; Index < Object->accessSpecifiers.GetLength(); ++Index)
		{
			const asSAccessSpecifier* Policy = Object->accessSpecifiers[Index];
			if (!Policy || Policies.Contains(Policy) || !asMetadataAccess::ValidPolicy(*Policy)) return false;
			const FString Name = UTF8_TO_TCHAR(Policy->name.AddressOf());
			if (Names.Contains(Name)) return false;
			Names.Add(Name); Policies.Add(Policy, {Object, asMetadataAccess::Encode(*Policy)});
		}
		for (asUINT Index = 0; Index < Object->localProperties.GetLength(); ++Index)
		{
			const asCObjectProperty* Property = Object->localProperties[Index];
			if (!Property || Properties.Contains(Property) || (Property->isPrivate && Property->isProtected)
				|| !CanUseAccessSpecifier(*Object, Property->accessSpecifier)) return false;
			Properties.Add(Property, {Property->accessSpecifier, uint8(uint8(Property->isPrivate) | (uint8(Property->isProtected) << 1))});
		}
	}
	for (const asCScriptFunction* Function : Functions)
	{
		if (!Function || (Function->IsPrivate() && Function->IsProtected()) || (Function->accessSpecifier &&
			(!Function->objectType || !CanUseAccessSpecifier(*Function->objectType, Function->accessSpecifier)))) return false;
		Methods.Add(Function, {Function->accessSpecifier, uint8(uint8(Function->IsPrivate()) | (uint8(Function->IsProtected()) << 1))});
	}
	return true;
}

bool asCDefinitions::CaptureAccessSnapshot()
{
	TMap<const asSAccessSpecifier*, asSAccessPolicySnapshot> Policies;
	TMap<const asCObjectProperty*, asSMemberAccessSnapshot> Properties;
	TMap<const asCScriptFunction*, asSMemberAccessSnapshot> Methods;
	if (!CollectAccessSnapshot(Policies, Properties, Methods)) return false;
	FrozenAccessPolicies = MoveTemp(Policies); FrozenPropertyAccess = MoveTemp(Properties); FrozenFunctionAccess = MoveTemp(Methods);
	return true;
}

bool asCDefinitions::ValidateAccessSnapshot() const
{
	TMap<const asSAccessSpecifier*, asSAccessPolicySnapshot> Policies;
	TMap<const asCObjectProperty*, asSMemberAccessSnapshot> Properties;
	TMap<const asCScriptFunction*, asSMemberAccessSnapshot> Methods;
	if (!CollectAccessSnapshot(Policies, Properties, Methods) || Policies.Num() != FrozenAccessPolicies.Num()
		|| Properties.Num() != FrozenPropertyAccess.Num() || Methods.Num() != FrozenFunctionAccess.Num()) return false;
	for (const auto& Pair : Policies)
	{
		const auto* Frozen = FrozenAccessPolicies.Find(Pair.Key);
		if (!Frozen || Frozen->Owner != Pair.Value.Owner || Frozen->Encoding != Pair.Value.Encoding) return false;
	}
	for (const auto& Pair : Properties)
	{
		const auto* Frozen = FrozenPropertyAccess.Find(Pair.Key);
		if (!Frozen || !(*Frozen == Pair.Value)) return false;
	}
	for (const auto& Pair : Methods)
	{
		const auto* Frozen = FrozenFunctionAccess.Find(Pair.Key);
		if (!Frozen || !(*Frozen == Pair.Value)) return false;
	}
	return true;
}
END_AS_NAMESPACE
