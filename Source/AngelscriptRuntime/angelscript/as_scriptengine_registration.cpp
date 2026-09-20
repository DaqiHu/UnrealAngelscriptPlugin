/* Maintained Unreal AngelScript fork — transactional semantic registration. */
#include "as_scriptengine.h"
#include "as_execution_snapshot.h"
#include "as_module.h"
#include "as_namespace.h"
#include "as_property.h"
#include "as_scriptfunction.h"
#include "as_type_id_registry.h"
#include "as_typeinfo.h"
#include "frontend/AST/as_type_context.h"
#include "Containers/Set.h"
#include <algorithm>
#include <functional>
#include <vector>

BEGIN_AS_NAMESPACE

namespace
{
	FString MetadataName(const char* Namespace, const char* Name)
	{
		return FString(UTF8_TO_TCHAR(Namespace)) + TEXT("::") + UTF8_TO_TCHAR(Name);
	}
	FString MetadataTypeName(const asCTypeInfo& Type)
	{
		TArray<FString> Owners;
		for (const asCTypeInfo* Owner = Type.GetDeclarationOwner(); Owner; Owner = Owner->GetDeclarationOwner())
			Owners.Insert(UTF8_TO_TCHAR(Owner->GetName()), 0);
		FString Scope = UTF8_TO_TCHAR(Type.GetNamespace());
		if (!Owners.IsEmpty())
		{
			if (!Scope.IsEmpty()) Scope += TEXT("::");
			Scope += FString::Join(Owners, TEXT("::"));
		}
		return Scope + TEXT("::") + UTF8_TO_TCHAR(Type.GetName());
	}
}

asERegistrationResult asCScriptEngine::RegisterExternalDefinitions(TUniquePtr<asCDefinitions> Set)
{
	const TMap<const asCTypeInfo*, int> TypeIds;
	const TMap<const asCScriptFunction*, int> FunctionIds;
	return RegisterExternalDefinitions(MoveTemp(Set), TypeIds, FunctionIds);
}

asERegistrationResult asCScriptEngine::InjectDefinitions(const asCDefinitions& Graph)
{
	std::lock_guard<std::mutex> EngineLock(registrationMutex);
	if (shuttingDown || vmRetirementRequested || registrationRetired) return asERegistrationResult::Retired;
	auto IsAdmitted = [&](const asCDefinitions* Item) -> bool
	{
		for (const asCDefinitionGraphLease& Lease : admittedHostGraphs)
		{
			if (Lease.Get() == Item) return true;
		}
		return false;
	};
	if (IsAdmitted(&Graph)) return asERegistrationResult::AlreadyRegistered;
	if (Graph.GetOptions().Kind != asETypeInfoKind::HostProcess) return asERegistrationResult::InvalidArgument;
	if (Graph.GetState() != asEDefinitionState::Frozen) return asERegistrationResult::InvalidState;
	TArray<asCDefinitions*> Closure;
	TSet<asCDefinitions*> Visited;
	std::function<asERegistrationResult(asCDefinitions*)> Visit = [&](asCDefinitions* Item) -> asERegistrationResult
	{
		if (!Item || Visited.Contains(Item)) return asERegistrationResult::Succeeded;
		Visited.Add(Item);
		if (Item->GetOptions().Kind != asETypeInfoKind::HostProcess) return asERegistrationResult::InvalidArgument;
		if (Item->GetState() != asEDefinitionState::Frozen) return asERegistrationResult::InvalidState;
		for (asCDefinitions* Dependency : Item->GetDependencies())
		{
			const asERegistrationResult Nested = Visit(Dependency);
			if (Nested != asERegistrationResult::Succeeded) return Nested;
		}
		Closure.Add(Item);
		return asERegistrationResult::Succeeded;
	};
	if (const asERegistrationResult Walk = Visit(const_cast<asCDefinitions*>(&Graph)); Walk != asERegistrationResult::Succeeded)
	{
		return Walk;
	}
	TArray<asCDefinitionGraphLease> NewLeases;
	for (asCDefinitions* Item : Closure)
	{
		if (IsAdmitted(Item)) continue;
		asCDefinitionGraphLease Lease = Item->RetainGraph();
		if (!Lease) return asERegistrationResult::InvalidState;
		NewLeases.Add(MoveTemp(Lease));
	}
	std::vector<asCDefinitions*> LockOrder;
	for (asCDefinitions* Item : Closure) LockOrder.push_back(Item);
	std::sort(LockOrder.begin(), LockOrder.end(), std::less<asCDefinitions*>{});
	std::vector<std::unique_lock<std::mutex>> SetLocks;
	for (asCDefinitions* Item : LockOrder) SetLocks.emplace_back(Item->MutationMutex);
	for (asCDefinitions* Item : Closure)
	{
		if (Item->Options.PointerByteSize != sizeof(void*) || Item->Options.TargetProfile != "native" ||
			Item->Options.FloatIsFloat64 != ep.floatIsFloat64 || !Item->ValidateFrozenLayout())
		{
			return asERegistrationResult::InvalidLayout;
		}
		if (!Item->ValidateAccessSnapshot()) return asERegistrationResult::InvalidState;
	}
	auto TypesByKey = typesByKey;
	auto TypesByName = typesByName;
	auto TypesById = typesById;
	auto FunctionsByKey = functionsByKey;
	auto FunctionsById = functionsById;
	auto GlobalsByKey = globalsByKey;
	auto GlobalsByName = globalsByName;
	int NextType = nextTypeId;
	int NextFunction = nextFunctionId;
	for (asCDefinitions* Item : Closure)
	{
		if (IsAdmitted(Item)) continue;
		for (asCTypeInfo* Type : Item->Types)
		{
			if (TypesByKey.Contains(Type->GetStableKey()) || FunctionsByKey.Contains(Type->GetStableKey()) || GlobalsByKey.Contains(Type->GetStableKey()))
				return asERegistrationResult::IdentityConflict;
			const FString Name = MetadataTypeName(*Type);
			const auto* Object = CastToObjectType(Type);
			const bool IsSpecialization = Object && Object->GetTemplateDefinition();
			if (!IsSpecialization && (TypesByName.Contains(Name) || GlobalsByName.Contains(Name)))
				return asERegistrationResult::NameConflict;
			const int* Id = Item->BoundTypeIds.Find(Type);
			if (!Id || asCTypeIdAllocator::IsFixedPrimitiveTypeId(*Id) || TypesById.Contains(*Id))
				return asERegistrationResult::InvalidIdentity;
			const int Sequence = asCTypeIdAllocator::TypeSequenceFromTypeId(*Id) + 1;
			if (Sequence > NextType) NextType = Sequence;
			TypesByKey.Add(Type->GetStableKey(), Type);
			TypesById.Add(*Id, Type);
			if (!IsSpecialization) TypesByName.Add(Name, Type);
		}
		for (asCScriptFunction* Function : Item->Functions)
		{
			if (!Function->IsMetadataCallableSignature() &&
				(FunctionsByKey.Contains(Function->GetStableKey()) || TypesByKey.Contains(Function->GetStableKey()) || GlobalsByKey.Contains(Function->GetStableKey())))
			{
				return asERegistrationResult::IdentityConflict;
			}
			const int* Id = Item->BoundFunctionIds.Find(Function);
			if (!Id || *Id <= 0 || FunctionsById.Contains(*Id)) return asERegistrationResult::InvalidIdentity;
			if (*Id + 1 > NextFunction && *Id != MAX_int32) NextFunction = *Id + 1;
			if (!Function->IsMetadataCallableSignature()) FunctionsByKey.Add(Function->GetStableKey(), Function);
			FunctionsById.Add(*Id, Function);
		}
		for (asCGlobalProperty* Global : Item->Globals)
		{
			const auto& GlobalKey = Global->GetStableKey();
			if (GlobalsByKey.Contains(GlobalKey) || TypesByKey.Contains(GlobalKey) || FunctionsByKey.Contains(GlobalKey))
				return asERegistrationResult::IdentityConflict;
			const FString Name = MetadataName(Global->nameSpace->name.AddressOf(), Global->name.AddressOf());
			if (GlobalsByName.Contains(Name) || TypesByName.Contains(Name)) return asERegistrationResult::NameConflict;
			GlobalsByKey.Add(GlobalKey, Global);
			GlobalsByName.Add(Name, Global);
		}
	}
	typesByKey = MoveTemp(TypesByKey);
	typesByName = MoveTemp(TypesByName);
	typesById = MoveTemp(TypesById);
	functionsByKey = MoveTemp(FunctionsByKey);
	functionsById = MoveTemp(FunctionsById);
	globalsByKey = MoveTemp(GlobalsByKey);
	globalsByName = MoveTemp(GlobalsByName);
	nextTypeId = NextType;
	nextFunctionId = NextFunction;
	for (asCDefinitionGraphLease& Lease : NewLeases)
	{
		admittedHostGraphs.Add(MoveTemp(Lease));
	}
	return asERegistrationResult::Succeeded;
}

asERegistrationResult asCScriptEngine::RegisterExternalDefinitions(
	TUniquePtr<asCDefinitions> Set,
	const TMap<const asCTypeInfo*, int>& PublishedTypeIds,
	const TMap<const asCScriptFunction*, int>& PublishedFunctionIds)
{
	if (!Set) return asERegistrationResult::InvalidArgument;
	asCDefinitions* const Root = Set.Get();
	std::lock_guard<std::mutex> EngineLock(registrationMutex);
	if (shuttingDown || vmRetirementRequested || registrationRetired) return asERegistrationResult::Retired;
	if (!Root->IsFrozen()) return asERegistrationResult::InvalidState;
	TArray<asCDefinitions*> Closure;
	TSet<asCDefinitions*> Visited;
	std::function<void(asCDefinitions*)> Visit = [&](asCDefinitions* Item)
	{
		if (!Item || Visited.Contains(Item)) return;
		Visited.Add(Item);
		for (asCDefinitions* Dependency : Item->GetDependencies()) Visit(Dependency);
		Closure.Add(Item);
	};
	Visit(Root);
	auto IsAdmitted = [&](const asCDefinitions* Ptr) -> bool
	{
		for (const asCDefinitionGraphLease& Lease : admittedHostGraphs)
		{
			if (Lease.Get() == Ptr) return true;
		}
		return false;
	};
	auto IsAdopted = [&](asCDefinitions* Ptr) -> bool
	{
		if (Ptr == Root) return true;
		if (IsAdmitted(Ptr)) return true;
		for (const TUniquePtr<asCDefinitions>& Owned : definitionSets)
			if (Owned.Get() == Ptr) return true;
		return false;
	};
	for (asCDefinitions* Item : Closure)
	{
		if (!IsAdopted(Item)) return asERegistrationResult::InvalidState;
	}
	// Every Engine acquires images in this order, so overlapping dependency DAGs
	// cannot form a lock cycle across two competing Engine registration calls.
	std::vector<asCDefinitions*> LockOrder;
	for (asCDefinitions* Item : Closure) LockOrder.push_back(Item);
	std::sort(LockOrder.begin(), LockOrder.end(), std::less<asCDefinitions*>{});
	std::vector<std::unique_lock<std::mutex>> SetLocks;
	for (auto* Item : LockOrder) SetLocks.emplace_back(Item->MutationMutex);
	auto EngineFromSet = [](asCDefinitions* Item) -> asCScriptEngine*
	{
		for (asCTypeInfo* Type : Item->Types)
		{
			if (Type && Type->engine) return Type->engine;
		}
		for (asCScriptFunction* Function : Item->Functions)
		{
			if (Function && Function->engine) return Function->engine;
		}
		return nullptr;
	};
	if (Root->GetState() == asEDefinitionState::Retired) return asERegistrationResult::Retired;
	if (asCScriptEngine* Existing = EngineFromSet(Root))
	{
		return Existing == this ? asERegistrationResult::AlreadyRegistered : asERegistrationResult::ForeignEngine;
	}
	for (asCDefinitions* Item : Closure)
	{
		if (IsAdmitted(Item)) continue;
		if (Item->GetState() == asEDefinitionState::Retired) return asERegistrationResult::Retired;
		if (asCScriptEngine* Existing = EngineFromSet(Item))
		{
			if (Existing != this) return asERegistrationResult::ForeignEngine;
		}
		if (Item->GetState() != asEDefinitionState::Frozen && Item->GetState() != asEDefinitionState::Attached)
			return asERegistrationResult::InvalidState;
		if (Item->Options.PointerByteSize != sizeof(void*) || Item->Options.TargetProfile != "native" ||
			Item->Options.FloatIsFloat64 != ep.floatIsFloat64 || !Item->ValidateFrozenLayout())
			return asERegistrationResult::InvalidLayout;
		if (!Item->ValidateFrozenIdentity()) return asERegistrationResult::InvalidIdentity;
		if (!Item->ValidateAccessSnapshot()) return asERegistrationResult::InvalidState;
	}
	// Prepare a complete replacement index, including IDs, without touching any
	// visible Engine or Set state. Failure leaves the prior snapshot intact.
	auto TypesByKey = typesByKey;
	auto TypesByName = typesByName;
	auto TypesById = typesById;
	auto FunctionsByKey = functionsByKey;
	auto FunctionsById = functionsById;
	auto GlobalsByKey = globalsByKey;
	auto GlobalsByName = globalsByName;
	int NextType = nextTypeId;
	int NextFunction = nextFunctionId;
	asUINT UnpublishedTypes = 0;
	asUINT UnpublishedFunctions = 0;
	for (asCDefinitions* Item : Closure)
	{
		if (IsAdmitted(Item) || EngineFromSet(Item) == this) continue;
		for (asCTypeInfo* Type : Item->Types)
		{
			if (!PublishedTypeIds.Find(Type)) ++UnpublishedTypes;
		}
		for (asCScriptFunction* Function : Item->Functions)
		{
			if (!PublishedFunctionIds.Find(Function)) ++UnpublishedFunctions;
		}
	}
	asSTypeIdReservation Unpublished;
	if (UnpublishedTypes > 0 || UnpublishedFunctions > 0)
	{
		asSTypeIdReservationRequest Request;
		Request.TypeSequenceCount = UnpublishedTypes;
		Request.FunctionIdCount = UnpublishedFunctions;
		if (asCTypeIdRegistry::Get().Reserve(Request, Unpublished) != asTYPE_ID_RESERVE_SUCCEEDED)
		{
			return asERegistrationResult::InvalidState;
		}
	}
	int NextReservedType = Unpublished.FirstTypeSequence;
	int NextReservedFunction = Unpublished.FirstFunctionId;
	TMap<const asCTypeInfo*, int> ClosureTypeIds;
	TMap<const asCScriptFunction*, int> ClosureFunctionIds;
	for (asCDefinitions* Item : Closure)
	{
		if (IsAdmitted(Item) || EngineFromSet(Item) == this)
		{
			ClosureTypeIds.Append(Item->BoundTypeIds);
			ClosureFunctionIds.Append(Item->BoundFunctionIds);
			continue;
		}
		for (asCTypeInfo* Type : Item->Types)
		{
			if (TypesByKey.Contains(Type->GetStableKey()) || FunctionsByKey.Contains(Type->GetStableKey()) || GlobalsByKey.Contains(Type->GetStableKey()))
				return asERegistrationResult::IdentityConflict;
			const FString Name = MetadataTypeName(*Type);
			const auto* Object = CastToObjectType(Type);
			const bool IsSpecialization = Object && Object->GetTemplateDefinition();
			if (!IsSpecialization && (TypesByName.Contains(Name) || GlobalsByName.Contains(Name))) return asERegistrationResult::NameConflict;
			int Id = 0;
			if (const int* Published = PublishedTypeIds.Find(Type))
			{
				Id = *Published;
				if (asCTypeIdAllocator::IsFixedPrimitiveTypeId(Id) || TypesById.Contains(Id))
					return asERegistrationResult::InvalidIdentity;
				const int Sequence = asCTypeIdAllocator::TypeSequenceFromTypeId(Id) + 1;
				if (Sequence > NextType) NextType = Sequence;
			}
			else
			{
				Id = asCTypeIdAllocator::EncodeObjectTypeId(NextReservedType++, Type->flags);
				const int Sequence = asCTypeIdAllocator::TypeSequenceFromTypeId(Id) + 1;
				if (Sequence > NextType) NextType = Sequence;
			}
			TypesByKey.Add(Type->GetStableKey(), Type); TypesById.Add(Id, Type);
			// Specializations share the authored template name. Their complete
			// identity, not an invented display string, selects the actual object.
			if (!IsSpecialization) TypesByName.Add(Name, Type);
			ClosureTypeIds.Add(Type, Id);
		}
		for (asCScriptFunction* Function : Item->Functions)
		{
			if (!Function->IsMetadataCallableSignature() &&
				(FunctionsByKey.Contains(Function->GetStableKey()) || TypesByKey.Contains(Function->GetStableKey()) || GlobalsByKey.Contains(Function->GetStableKey())))
				return asERegistrationResult::IdentityConflict;
			int Id = 0;
			if (const int* Published = PublishedFunctionIds.Find(Function))
			{
				Id = *Published;
				if (Id <= 0 || FunctionsById.Contains(Id)) return asERegistrationResult::InvalidIdentity;
				if (Id + 1 > NextFunction && Id != MAX_int32) NextFunction = Id + 1;
			}
			else
			{
				Id = NextReservedFunction++;
				if (Id + 1 > NextFunction && Id != MAX_int32) NextFunction = Id + 1;
			}
			if (!Function->IsMetadataCallableSignature()) FunctionsByKey.Add(Function->GetStableKey(), Function);
			FunctionsById.Add(Id, Function);
			ClosureFunctionIds.Add(Function, Id);
		}
		for (asCGlobalProperty* Global : Item->Globals)
		{
			const auto& Key = Global->GetStableKey();
			if (GlobalsByKey.Contains(Key) || TypesByKey.Contains(Key) || FunctionsByKey.Contains(Key))
				return asERegistrationResult::IdentityConflict;
			const FString Name = MetadataName(Global->nameSpace->name.AddressOf(), Global->name.AddressOf());
			if (GlobalsByName.Contains(Name) || TypesByName.Contains(Name)) return asERegistrationResult::NameConflict;
			GlobalsByKey.Add(Key, Global); GlobalsByName.Add(Name, Global);
		}
	}
	for (asCDefinitions* Item : Closure)
	{
		if (IsAdmitted(Item) || EngineFromSet(Item) == this) continue;
		Item->State.store(asEDefinitionState::Attaching, std::memory_order_release);
		for (const auto& Pair : ClosureTypeIds)
			if (Item->CanReference(Pair.Key)) Item->BoundTypeIds.Add(Pair.Key, Pair.Value);
		for (const auto& Pair : ClosureFunctionIds)
			if (Item->Owns(Pair.Key)) Item->BoundFunctionIds.Add(Pair.Key, Pair.Value);
		for (asCTypeInfo* Type : Item->Types)
		{
			if (Type) Type->engine = this;
		}
		for (asCScriptFunction* Function : Item->Functions)
		{
			if (Function) Function->engine = this;
		}
	}
	definitionSets.Add(MoveTemp(Set));
	typesByKey = MoveTemp(TypesByKey); typesByName = MoveTemp(TypesByName); typesById = MoveTemp(TypesById);
	functionsByKey = MoveTemp(FunctionsByKey); functionsById = MoveTemp(FunctionsById);
	globalsByKey = MoveTemp(GlobalsByKey); globalsByName = MoveTemp(GlobalsByName);
	nextTypeId = NextType; nextFunctionId = NextFunction;
	for (asCDefinitions* Item : Closure)
	{
		if (IsAdmitted(Item)) continue;
		Item->State.store(asEDefinitionState::Attached, std::memory_order_release);
	}
	// Engine queries take EngineLock; image binding queries take their image
	// lock. Neither can see IDs or visibility from a partially committed graph.
	return asERegistrationResult::Succeeded;
}

void asCScriptEngine::UnpublishDefinitionSets(TConstArrayView<asCDefinitions*> Sets)
{
	TArray<asSStableKey> ExecutableKeys;
	{
		std::lock_guard<std::mutex> EngineLock(registrationMutex);
		TSet<asCDefinitions*> Owned;
		for (const TUniquePtr<asCDefinitions>& Item : definitionSets)
		{
			Owned.Add(Item.Get());
		}
		for (asCDefinitions* Set : Sets)
		{
			if (!Set || !Owned.Contains(Set))
			{
				continue;
			}
			for (asCTypeInfo* Type : Set->Types)
			{
				if (!Type)
				{
					continue;
				}
				typesByKey.Remove(Type->GetStableKey());
				typesByName.Remove(MetadataTypeName(*Type));
				if (const int* Id = Set->BoundTypeIds.Find(Type))
				{
					typesById.Remove(*Id);
				}
			}
			for (asCScriptFunction* Function : Set->Functions)
			{
				if (!Function)
				{
					continue;
				}
				if (!Function->IsMetadataCallableSignature())
				{
					functionsByKey.Remove(Function->GetStableKey());
				}
				if (const int* Id = Set->BoundFunctionIds.Find(Function))
				{
					functionsById.Remove(*Id);
				}
				ExecutableKeys.Add(Function->GetStableKey());
			}
			for (asCGlobalProperty* Global : Set->Globals)
			{
				if (!Global)
				{
					continue;
				}
				globalsByKey.Remove(Global->GetStableKey());
				if (Global->nameSpace)
				{
					globalsByName.Remove(MetadataName(Global->nameSpace->name.AddressOf(), Global->name.AddressOf()));
				}
			}
		}
	}
	for (const asSStableKey& Key : ExecutableKeys)
	{
		if (asCExecutableFunction* Existing = FindPublishedExecutable(Key))
		{
			UnpublishExecutable(Key, Existing);
		}
	}
}

void asCScriptEngine::RetireDefinitionSets(TConstArrayView<asCDefinitions*> Sets)
{
	UnpublishDefinitionSets(Sets);
	TArray<TUniquePtr<asCDefinitions>> ReleasedSets;
	{
		std::lock_guard<std::mutex> EngineLock(registrationMutex);
		TSet<asCDefinitions*> Retire;
		for (asCDefinitions* Set : Sets)
		{
			if (Set)
			{
				Retire.Add(Set);
			}
		}
		for (asUINT ModuleIndex = 0; ModuleIndex < scriptModules.GetLength(); ++ModuleIndex)
		{
			if (asCModule* Module = scriptModules[ModuleIndex])
			{
				bool bOwnsRetired = false;
				auto MarksRetired = [&](asITypeInfo* Info) -> bool
				{
					asCTypeInfo* Type = static_cast<asCTypeInfo*>(Info);
					return Type && Retire.Contains(Type->GetDefinitions());
				};
				for (asUINT TypeIndex = 0; !bOwnsRetired && TypeIndex < Module->GetObjectTypeCount(); ++TypeIndex)
				{
					bOwnsRetired = MarksRetired(Module->GetObjectTypeByIndex(TypeIndex));
				}
				for (asUINT EnumIndex = 0; !bOwnsRetired && EnumIndex < Module->GetEnumCount(); ++EnumIndex)
				{
					bOwnsRetired = MarksRetired(Module->GetEnumByIndex(EnumIndex));
				}
				if (bOwnsRetired)
				{
					Module->DetachDefinitionOwnedTypes();
				}
			}
		}
		for (int32 Index = definitionSets.Num() - 1; Index >= 0; --Index)
		{
			asCDefinitions* Item = definitionSets[Index].Get();
			if (!Retire.Contains(Item))
			{
				continue;
			}
			Item->BoundEngine = nullptr;
			for (asCTypeInfo* Type : Item->Types)
			{
				if (Type)
				{
					Type->engine = nullptr;
				}
			}
			for (asCScriptFunction* Function : Item->Functions)
			{
				if (Function)
				{
					Function->engine = nullptr;
				}
			}
			Item->BoundTypeIds.Empty();
			Item->BoundFunctionIds.Empty();
			Item->State.store(asEDefinitionState::Retired, std::memory_order_release);
			ReleasedSets.Add(MoveTemp(definitionSets[Index]));
			definitionSets.RemoveAt(Index);
		}
	}
	ReleasedSets.Empty();
}

void asCScriptEngine::RetireExternalDefinitions()
{
	TArray<TUniquePtr<asCDefinitions>> ReleasedSets;
	TArray<asCDefinitionGraphLease> ReleasedHosts;
	{
		std::lock_guard<std::mutex> EngineLock(registrationMutex);
		if (registrationRetired) return;
		std::unique_lock<std::mutex> PublishLock(executablePublicationMutex);
		if (!vmRetirementRequested.exchange(true)) ++executablePublicationGeneration;
		if (vmObjectLeases || vmOperations || vmDrainActive) return;
		std::vector<asCDefinitions*> LockOrder;
		for (const TUniquePtr<asCDefinitions>& Item : definitionSets) LockOrder.push_back(Item.Get());
		std::sort(LockOrder.begin(), LockOrder.end(), std::less<asCDefinitions*>{});
		std::vector<std::unique_lock<std::mutex>> SetLocks;
		for (auto* Item : LockOrder) SetLocks.emplace_back(Item->MutationMutex);
		typesByKey.Empty(); typesByName.Empty(); typesById.Empty();
		functionsByKey.Empty(); functionsById.Empty();
		globalsByKey.Empty(); globalsByName.Empty();
		globalBindings.Empty();
		defaultArrayObjectType = nullptr;
		stringFactory = nullptr;
		stringType = asCDataType();
		TArray<asCExecutableSnapshot*> SnapshotLeases;
		{

			++executablePublicationGeneration;
			publishedExecutables.Empty();
			SnapshotLeases = MoveTemp(publishedSnapshotLeases);
			// Caller-held snapshots can outlive the Engine. Their immutable code
			// remains readable, but destruction must not call a dead publisher.
			for (asCExecutableSnapshot* Snapshot : SnapshotLeases)
				if (Snapshot) Snapshot->Engine.store(nullptr);
			// Current native generations remain Engine-owned cleanup leases until final
			// drain/destruction; retired admissions cannot install or execute new work.
			registrationRetired = true;
		}
		PublishLock.unlock();
		for (asCExecutableSnapshot* Snapshot : SnapshotLeases)
		{
			if (Snapshot) Snapshot->Release();
		}
		for (const auto& Item : definitionSets)
		{
			Item->BoundEngine = nullptr;
			for (asCTypeInfo* Type : Item->Types)
			{
				if (Type) Type->engine = nullptr;
			}
			for (asCScriptFunction* Function : Item->Functions)
			{
				if (Function) Function->engine = nullptr;
			}
			Item->BoundTypeIds.Empty(); Item->BoundFunctionIds.Empty();
			Item->State.store(asEDefinitionState::Retired, std::memory_order_release);
		}
		for (asUINT ModuleIndex = 0; ModuleIndex < scriptModules.GetLength(); ++ModuleIndex)
		{
			if (asCModule* Module = scriptModules[ModuleIndex])
			{
				Module->DetachDefinitionOwnedTypes();
			}
		}
		registrationRetired = true;
		ReleasedSets = MoveTemp(definitionSets);
		ReleasedHosts = MoveTemp(admittedHostGraphs);
	}
	// Final image destruction can execute container destructors. Release outside
	// both lock domains, after all Engine pointers and visible bindings are gone.
	ReleasedSets.Empty();
	ReleasedHosts.Empty();
}

asCTypeInfo* asCScriptEngine::FindType(const char* Namespace, const char* Name) const
{
	if (!Namespace || !Name) return nullptr;
	std::lock_guard<std::mutex> Lock(registrationMutex);
	return typesByName.FindRef(MetadataName(Namespace, Name));
}

asCObjectType* asCScriptEngine::FindTemplateInstance(const asCObjectType* Template, TConstArrayView<asCDataType> Arguments) const
{
	if (!Template) return nullptr;
	std::lock_guard<std::mutex> Lock(registrationMutex);
	for (const asCDefinitionGraphLease& Lease : admittedHostGraphs)
	{
		if (asCDefinitions* Graph = Lease.Get())
		{
			if (asCObjectType* Found = Graph->FindTemplateInstance(Template, Arguments)) return Found;
		}
	}
	if (liveDefinitions)
	{
		if (asCObjectType* Found = liveDefinitions->FindTemplateInstance(Template, Arguments)) return Found;
	}
	return nullptr;
}

asERegistrationResult asCScriptEngine::SpecializeTemplate(asCObjectType& Template, TConstArrayView<asCDataType> Arguments, asCObjectType*& Out)
{
	std::lock_guard<std::mutex> Lock(registrationMutex);
	Out = nullptr;
	bool bPrivate = false;
	for (const asCDataType& Argument : Arguments)
	{
		if (asCTypeInfo* Type = Argument.GetTypeInfo())
		{
			if (Type->GetTypeInfoKind() != asETypeInfoKind::HostProcess) bPrivate = true;
		}
	}
	if (!bPrivate)
	{
		for (const asCDefinitionGraphLease& Lease : admittedHostGraphs)
		{
			if (asCDefinitions* Graph = Lease.Get())
			{
				if (asCObjectType* Found = Graph->FindTemplateInstance(&Template, Arguments))
				{
					Out = Found;
					return asERegistrationResult::Succeeded;
				}
			}
		}
		return asERegistrationResult::InvalidArgument;
	}
	for (const asCDataType& Argument : Arguments)
	{
		if (asCTypeInfo* Type = Argument.GetTypeInfo())
		{
			if (Type->GetTypeInfoKind() == asETypeInfoKind::HostProcess) continue;
			if (Type->GetEngine() != this) return asERegistrationResult::ForeignEngine;
		}
	}
	asCDefinitions* Live = EnsureLiveDefinitions();
	if (!Live) return asERegistrationResult::InvalidState;
	if (asCDefinitions* Host = Template.GetDefinitions())
	{
		if (Live->AddDependency(Host) != asEDefinitionResult::Succeeded) return asERegistrationResult::InvalidArgument;
	}
	if (asCObjectType* Existing = Live->FindTemplateInstance(&Template, Arguments))
	{
		Out = Existing;
		return asERegistrationResult::AlreadyRegistered;
	}
	FString ArgumentIdentity;
	for (const asCDataType& Argument : Arguments)
	{
		if (asCTypeInfo* Type = Argument.GetTypeInfo())
		{
			ArgumentIdentity += TEXT("|");
			ArgumentIdentity += UTF8_TO_TCHAR(Type->GetNamespace());
			ArgumentIdentity += TEXT("::");
			ArgumentIdentity += UTF8_TO_TCHAR(Type->GetName());
		}
		else
		{
			ArgumentIdentity += FString::Printf(TEXT("|tok%d"), static_cast<int32>(Argument.GetTokenType()));
		}
	}
	const FString InstanceName = FString(UTF8_TO_TCHAR(Template.GetName())) + ArgumentIdentity;
	const asSStableKey Key = MakeLiveStableKey("tmpl", Template.GetNamespace(), TCHAR_TO_UTF8(*InstanceName));
	if (Live->CreateHostTemplateInstance(Template, Key, Arguments, Out) != asEDefinitionResult::Succeeded || !Out)
	{
		Out = nullptr;
		return asERegistrationResult::InvalidArgument;
	}
	if (PublishLiveType(*Out) < 0)
	{
		Out = nullptr;
		return asERegistrationResult::InvalidIdentity;
	}
	return asERegistrationResult::Succeeded;
}

asCTypeInfo* asCScriptEngine::FindType(const asSStableKey& Key) const
{
	std::lock_guard<std::mutex> Lock(registrationMutex);
	return typesByKey.FindRef(Key);
}

asCScriptFunction* asCScriptEngine::FindFunction(const asSStableKey& Key) const
{
	std::lock_guard<std::mutex> Lock(registrationMutex);
	return functionsByKey.FindRef(Key);
}

asCScriptFunction* asCScriptEngine::FindRegisteredFunction(const asSStableKey& Key) const
{
	std::lock_guard<std::mutex> Lock(registrationMutex);
	if (asCScriptFunction* Function = functionsByKey.FindRef(Key))
	{
		return Function;
	}
	for (const TPair<int, asCScriptFunction*>& Pair : functionsById)
	{
		if (Pair.Value && Pair.Value->GetStableKey() == Key)
		{
			return Pair.Value;
		}
	}
	return nullptr;
}

asCGlobalProperty* asCScriptEngine::FindGlobalProperty(const char* Namespace, const char* Name) const
{
	if (!Namespace || !Name) return nullptr;
	std::lock_guard<std::mutex> Lock(registrationMutex);
	return globalsByName.FindRef(MetadataName(Namespace, Name));
}

asCGlobalProperty* asCScriptEngine::FindGlobalProperty(const asSStableKey& Key) const
{
	std::lock_guard<std::mutex> Lock(registrationMutex);
	return globalsByKey.FindRef(Key);
}

TPair<asUINT, asUINT> asCScriptEngine::GetRegistrationCounts() const
{
	std::lock_guard<std::mutex> Lock(registrationMutex);
	return {static_cast<asUINT>(typesByKey.Num()), static_cast<asUINT>(functionsById.Num())};
}

TArray<const asCDefinitions*> asCScriptEngine::GetRegisteredDefinitions() const
{
	std::lock_guard<std::mutex> Lock(registrationMutex);
	TArray<const asCDefinitions*> Result;
	Result.Reserve(definitionSets.Num() + admittedHostGraphs.Num());
	for (const TUniquePtr<asCDefinitions>& Set : definitionSets) Result.Add(Set.Get());
	for (const asCDefinitionGraphLease& Lease : admittedHostGraphs) Result.Add(Lease.Get());
	return Result;
}

void asCScriptEngine::InitializeDefinitionBuiltins()
{
auto Context = MakeShared<asCTypeContext, ESPMode::ThreadSafe>();
	auto& Registry = Context->GetIdentityRegistry();
	TUniquePtr<asCDefinitions> Set = asCDefinitions::Create();
	asEDefinitionResult Result = Set->SetIdentityContext(Context);
	asASSERT(Result == asEDefinitionResult::Succeeded);
	asSNominalTypeDescriptor TypeDescriptor;
	TypeDescriptor.StableScope = TEXT("angelscript.builtin");
	TypeDescriptor.Kind = asETypeDeclKind::Class;
	asCObjectType* Object = nullptr;
	asCObjectType* Function = nullptr;
	asSStableKey ObjectKey, FunctionKey;
	TypeDescriptor.Name = TEXT("$obj");
	auto IdentityResult = Registry.InternNominalType(TypeDescriptor, ObjectKey);
	asASSERT(IdentityResult == asETypeIdentityStatus::Succeeded);
	Result = Set->CreateObjectType(ObjectKey, "$obj", "", asOBJ_REF | asOBJ_SCRIPT_OBJECT | asOBJ_NOCOUNT, Object);
	asASSERT(Result == asEDefinitionResult::Succeeded);
	TypeDescriptor.Name = TEXT("$func");
	IdentityResult = Registry.InternNominalType(TypeDescriptor, FunctionKey);
	asASSERT(IdentityResult == asETypeIdentityStatus::Succeeded);
	Result = Set->CreateObjectType(FunctionKey, "$func", "", asOBJ_REF | asOBJ_SCRIPT_OBJECT | asOBJ_NOCOUNT, Function);
	asASSERT(Result == asEDefinitionResult::Succeeded);
	asSStableKey VoidKey;
	IdentityResult = Context->InternPrimitiveTypeUse(asEPrimitiveType::Void, VoidKey);
	asASSERT(IdentityResult == asETypeIdentityStatus::Succeeded);
	// Typed declaration only. Runtime construction, GC and delegate invocation
	// remain dormant; there are no parser strings or registered native callables.
	asSFunctionIdentityDescriptor Method;
	Method.StableScope = TypeDescriptor.StableScope;
	Method.Owner = ObjectKey;
	Method.Kind = asEFunctionIdentityKind::Constructor;
	Method.Name = TEXT("$obj");
	Method.ReturnType = VoidKey;
	asSStableKey MethodKey;
	IdentityResult = Registry.InternFunction(Method, MethodKey);
	asASSERT(IdentityResult == asETypeIdentityStatus::Succeeded);
	asCScriptFunction* Declaration = nullptr;
	Result = Set->CreateFunction(MethodKey, "$obj", "", asFUNC_SYSTEM, Declaration);
	asASSERT(Result == asEDefinitionResult::Succeeded);
	Result = Set->DefineFunction(*Declaration, Set->Primitive(ttVoid), {}, Object);
	asASSERT(Result == asEDefinitionResult::Succeeded);
	Result = Set->SetFunctionDeclarationFacts(*Declaration, asEFunctionIdentityKind::Constructor, Object);
	asASSERT(Result == asEDefinitionResult::Succeeded);
	Result = Set->AddBehaviour(*Object, asBEHAVE_CONSTRUCT, *Declaration);
	asASSERT(Result == asEDefinitionResult::Succeeded);
	Result = Set->FinalizeLayouts(); asASSERT(Result == asEDefinitionResult::Succeeded);
	Result = Set->Freeze(); asASSERT(Result == asEDefinitionResult::Succeeded);
	const auto Registration = RegisterExternalDefinitions(MoveTemp(Set));
	asASSERT(Registration == asERegistrationResult::Succeeded);
	if (Registration != asERegistrationResult::Succeeded) configFailed = true;
	UNUSED_VAR(Result); UNUSED_VAR(IdentityResult);
}

asUINT asCScriptEngine::CountTypes(asQWORD Include, asQWORD Exclude) const
{
	std::lock_guard<std::mutex> Lock(registrationMutex);
	asUINT Count = 0;
	auto CountSet = [&](const asCDefinitions* Set)
	{
		if (!Set) return;
		for (asCTypeInfo* Type : Set->GetTypes())
			if (Type && (Type->flags & Include) && !(Type->flags & Exclude)) ++Count;
	};
	for (const auto& Set : definitionSets) CountSet(Set.Get());
	CountSet(liveDefinitions.Get());
	return Count;
}

asCTypeInfo* asCScriptEngine::TypeByIndex(asUINT Index, asQWORD Include, asQWORD Exclude) const
{
	std::lock_guard<std::mutex> Lock(registrationMutex);
	auto Visit = [&](const asCDefinitions* Set) -> asCTypeInfo*
	{
		if (!Set) return nullptr;
		for (asCTypeInfo* Type : Set->GetTypes())
		{
			if (!Type || !(Type->flags & Include) || (Type->flags & Exclude)) continue;
			if (Index == 0) return Type;
			--Index;
		}
		return nullptr;
	};
	if (asCTypeInfo* Type = Visit(liveDefinitions.Get())) return Type;
	for (const auto& Set : definitionSets)
		if (asCTypeInfo* Type = Visit(Set.Get())) return Type;
	return nullptr;
}

END_AS_NAMESPACE
