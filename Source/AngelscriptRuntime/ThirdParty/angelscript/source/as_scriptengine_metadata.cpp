/* Maintained Unreal AngelScript fork — transactional semantic registration. */
#include "as_scriptengine.h"
#include "as_namespace.h"
#include "as_type_id_registry.h"
#include "frontend/as_type_context.h"
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
		for (const asCTypeInfo* Owner = Type.GetMetadataDeclarationOwner(); Owner; Owner = Owner->GetMetadataDeclarationOwner())
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

asEMetadataRegistrationResult asCScriptEngine::RegisterMetadataImage(TUniquePtr<asCMetadataImage> Image)
{
	const TMap<const asCTypeInfo*, int> TypeIds;
	const TMap<const asCScriptFunction*, int> FunctionIds;
	return RegisterMetadataImage(MoveTemp(Image), TypeIds, FunctionIds);
}

asEMetadataRegistrationResult asCScriptEngine::RegisterMetadataImage(
	TUniquePtr<asCMetadataImage> Image,
	const TMap<const asCTypeInfo*, int>& PublishedTypeIds,
	const TMap<const asCScriptFunction*, int>& PublishedFunctionIds)
{
	if (!Image) return asEMetadataRegistrationResult::InvalidArgument;
	asCMetadataImage* const Root = Image.Get();
	std::lock_guard<std::mutex> EngineLock(metadataRegistrationMutex);
	if (shuttingDown || vmRetirementRequested || metadataRegistrationRetired) return asEMetadataRegistrationResult::Retired;
	if (!Root->IsFrozen()) return asEMetadataRegistrationResult::InvalidState;
	TArray<asCMetadataImage*> Closure;
	TSet<asCMetadataImage*> Visited;
	std::function<void(asCMetadataImage*)> Visit = [&](asCMetadataImage* Item)
	{
		if (!Item || Visited.Contains(Item)) return;
		Visited.Add(Item);
		for (asCMetadataImage* Dependency : Item->GetDependencies()) Visit(Dependency);
		Closure.Add(Item);
	};
	Visit(Root);
	auto IsAdopted = [&](asCMetadataImage* Ptr) -> bool
	{
		if (Ptr == Root) return true;
		for (const TUniquePtr<asCMetadataImage>& Owned : metadataImages)
			if (Owned.Get() == Ptr) return true;
		return false;
	};
	for (asCMetadataImage* Item : Closure)
	{
		if (!IsAdopted(Item)) return asEMetadataRegistrationResult::InvalidState;
	}
	// Every Engine acquires images in this order, so overlapping dependency DAGs
	// cannot form a lock cycle across two competing Engine registration calls.
	std::vector<asCMetadataImage*> LockOrder;
	for (asCMetadataImage* Item : Closure) LockOrder.push_back(Item);
	std::sort(LockOrder.begin(), LockOrder.end(), std::less<asCMetadataImage*>{});
	std::vector<std::unique_lock<std::mutex>> ImageLocks;
	for (auto* Item : LockOrder) ImageLocks.emplace_back(Item->MutationMutex);
	auto EngineFromImage = [](asCMetadataImage* Item) -> asCScriptEngine*
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
	if (Root->GetState() == asEMetadataImageState::Retired) return asEMetadataRegistrationResult::Retired;
	if (asCScriptEngine* Existing = EngineFromImage(Root))
	{
		return Existing == this ? asEMetadataRegistrationResult::AlreadyRegistered : asEMetadataRegistrationResult::ForeignEngine;
	}
	for (asCMetadataImage* Item : Closure)
	{
		if (Item->GetState() == asEMetadataImageState::Retired) return asEMetadataRegistrationResult::Retired;
		if (asCScriptEngine* Existing = EngineFromImage(Item))
		{
			if (Existing != this) return asEMetadataRegistrationResult::ForeignEngine;
		}
		if (Item->GetState() != asEMetadataImageState::Frozen && Item->GetState() != asEMetadataImageState::Attached)
			return asEMetadataRegistrationResult::InvalidState;
		if (Item->Options.PointerByteSize != sizeof(void*) || Item->Options.TargetProfile != "native" ||
			Item->Options.FloatIsFloat64 != ep.floatIsFloat64 || !Item->ValidateFrozenLayout())
			return asEMetadataRegistrationResult::InvalidLayout;
		if (!Item->ValidateFrozenIdentity()) return asEMetadataRegistrationResult::InvalidIdentity;
		if (!Item->ValidateAccessSnapshot()) return asEMetadataRegistrationResult::InvalidState;
	}
	// Prepare a complete replacement index, including IDs, without touching any
	// visible Engine or Image state. Failure leaves the prior snapshot intact.
	auto TypesByKey = metadataTypesByKey;
	auto TypesByName = metadataTypesByName;
	auto TypesById = metadataTypesById;
	auto FunctionsByKey = metadataFunctionsByKey;
	auto FunctionsById = metadataFunctionsById;
	auto GlobalsByKey = metadataGlobalsByKey;
	auto GlobalsByName = metadataGlobalsByName;
	int NextType = nextMetadataTypeId;
	int NextFunction = nextMetadataFunctionId;
	asUINT UnpublishedTypes = 0;
	asUINT UnpublishedFunctions = 0;
	for (asCMetadataImage* Item : Closure)
	{
		if (EngineFromImage(Item) == this) continue;
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
			return asEMetadataRegistrationResult::InvalidState;
		}
	}
	int NextReservedType = Unpublished.FirstTypeSequence;
	int NextReservedFunction = Unpublished.FirstFunctionId;
	TMap<const asCTypeInfo*, int> ClosureTypeIds;
	TMap<const asCScriptFunction*, int> ClosureFunctionIds;
	for (asCMetadataImage* Item : Closure)
	{
		if (EngineFromImage(Item) == this)
		{
			ClosureTypeIds.Append(Item->BoundTypeIds);
			ClosureFunctionIds.Append(Item->BoundFunctionIds);
			continue;
		}
		for (asCTypeInfo* Type : Item->Types)
		{
			if (TypesByKey.Contains(Type->GetStableKey()) || FunctionsByKey.Contains(Type->GetStableKey()) || GlobalsByKey.Contains(Type->GetStableKey()))
				return asEMetadataRegistrationResult::IdentityConflict;
			const FString Name = MetadataTypeName(*Type);
			const auto* Object = CastToObjectType(Type);
			const bool IsSpecialization = Object && Object->GetMetadataTemplateDefinition();
			if (!IsSpecialization && (TypesByName.Contains(Name) || GlobalsByName.Contains(Name))) return asEMetadataRegistrationResult::NameConflict;
			int Id = 0;
			if (const int* Published = PublishedTypeIds.Find(Type))
			{
				Id = *Published;
				if (asCTypeIdAllocator::IsFixedPrimitiveTypeId(Id) || TypesById.Contains(Id))
					return asEMetadataRegistrationResult::InvalidIdentity;
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
				return asEMetadataRegistrationResult::IdentityConflict;
			int Id = 0;
			if (const int* Published = PublishedFunctionIds.Find(Function))
			{
				Id = *Published;
				if (Id <= 0 || FunctionsById.Contains(Id)) return asEMetadataRegistrationResult::InvalidIdentity;
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
				return asEMetadataRegistrationResult::IdentityConflict;
			const FString Name = MetadataName(Global->nameSpace->name.AddressOf(), Global->name.AddressOf());
			if (GlobalsByName.Contains(Name) || TypesByName.Contains(Name)) return asEMetadataRegistrationResult::NameConflict;
			GlobalsByKey.Add(Key, Global); GlobalsByName.Add(Name, Global);
		}
	}
	for (asCMetadataImage* Item : Closure)
	{
		if (EngineFromImage(Item) == this) continue;
		Item->State.store(asEMetadataImageState::Attaching, std::memory_order_release);
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
	metadataImages.Add(MoveTemp(Image));
	metadataTypesByKey = MoveTemp(TypesByKey); metadataTypesByName = MoveTemp(TypesByName); metadataTypesById = MoveTemp(TypesById);
	metadataFunctionsByKey = MoveTemp(FunctionsByKey); metadataFunctionsById = MoveTemp(FunctionsById);
	metadataGlobalsByKey = MoveTemp(GlobalsByKey); metadataGlobalsByName = MoveTemp(GlobalsByName);
	nextMetadataTypeId = NextType; nextMetadataFunctionId = NextFunction;
	for (asCMetadataImage* Item : Closure) Item->State.store(asEMetadataImageState::Attached, std::memory_order_release);
	// Engine queries take EngineLock; image binding queries take their image
	// lock. Neither can see IDs or visibility from a partially committed graph.
	return asEMetadataRegistrationResult::Succeeded;
}

void asCScriptEngine::RetireMetadataImages()
{
	TArray<TUniquePtr<asCMetadataImage>> ReleasedImages;
	{
		std::lock_guard<std::mutex> EngineLock(metadataRegistrationMutex);
		if (metadataRegistrationRetired) return;
		std::unique_lock<std::mutex> PublishLock(executablePublicationMutex);
		if (!vmRetirementRequested.exchange(true)) ++executablePublicationGeneration;
		if (vmObjectLeases || vmOperations || vmDrainActive) return;
		std::vector<asCMetadataImage*> LockOrder;
		for (const TUniquePtr<asCMetadataImage>& Item : metadataImages) LockOrder.push_back(Item.Get());
		std::sort(LockOrder.begin(), LockOrder.end(), std::less<asCMetadataImage*>{});
		std::vector<std::unique_lock<std::mutex>> ImageLocks;
		for (auto* Item : LockOrder) ImageLocks.emplace_back(Item->MutationMutex);
		metadataTypesByKey.Empty(); metadataTypesByName.Empty(); metadataTypesById.Empty();
		metadataFunctionsByKey.Empty(); metadataFunctionsById.Empty();
		metadataGlobalsByKey.Empty(); metadataGlobalsByName.Empty();
		metadataGlobalBindings.Empty();
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
			metadataRegistrationRetired = true;
		}
		PublishLock.unlock();
		for (asCExecutableSnapshot* Snapshot : SnapshotLeases)
		{
			if (Snapshot) Snapshot->Release();
		}
		for (const auto& Item : metadataImages)
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
			Item->State.store(asEMetadataImageState::Retired, std::memory_order_release);
		}
		metadataRegistrationRetired = true;
		ReleasedImages = MoveTemp(metadataImages);
	}
	// Final image destruction can execute container destructors. Release outside
	// both lock domains, after all Engine pointers and visible bindings are gone.
	ReleasedImages.Empty();
}

asCTypeInfo* asCScriptEngine::FindMetadataType(const char* Namespace, const char* Name) const
{
	if (!Namespace || !Name) return nullptr;
	std::lock_guard<std::mutex> Lock(metadataRegistrationMutex);
	return metadataTypesByName.FindRef(MetadataName(Namespace, Name));
}

asCTypeInfo* asCScriptEngine::FindMetadataType(const asSStableKey& Key) const
{
	std::lock_guard<std::mutex> Lock(metadataRegistrationMutex);
	return metadataTypesByKey.FindRef(Key);
}

asCScriptFunction* asCScriptEngine::FindMetadataFunction(const asSStableKey& Key) const
{
	std::lock_guard<std::mutex> Lock(metadataRegistrationMutex);
	return metadataFunctionsByKey.FindRef(Key);
}

asCGlobalProperty* asCScriptEngine::FindMetadataGlobalProperty(const char* Namespace, const char* Name) const
{
	if (!Namespace || !Name) return nullptr;
	std::lock_guard<std::mutex> Lock(metadataRegistrationMutex);
	return metadataGlobalsByName.FindRef(MetadataName(Namespace, Name));
}

asCGlobalProperty* asCScriptEngine::FindMetadataGlobalProperty(const asSStableKey& Key) const
{
	std::lock_guard<std::mutex> Lock(metadataRegistrationMutex);
	return metadataGlobalsByKey.FindRef(Key);
}

TPair<asUINT, asUINT> asCScriptEngine::GetMetadataRegistrationCounts() const
{
	std::lock_guard<std::mutex> Lock(metadataRegistrationMutex);
	return {static_cast<asUINT>(metadataTypesByKey.Num()), static_cast<asUINT>(metadataFunctionsById.Num())};
}

TArray<const asCMetadataImage*> asCScriptEngine::GetRegisteredMetadataImages() const
{
	std::lock_guard<std::mutex> Lock(metadataRegistrationMutex);
	TArray<const asCMetadataImage*> Result;
	Result.Reserve(metadataImages.Num());
	for (const TUniquePtr<asCMetadataImage>& Image : metadataImages) Result.Add(Image.Get());
	return Result;
}

void asCScriptEngine::InitializeMetadataBuiltins()
{
auto Context = MakeShared<asCTypeContext, ESPMode::ThreadSafe>();
	auto& Registry = Context->GetIdentityRegistry();
	auto Image = asCMetadataImage::Create();
	asEMetadataResult Result = Image->SetIdentityContext(Context);
	asASSERT(Result == asEMetadataResult::Succeeded);
	asSNominalTypeDescriptor TypeDescriptor;
	TypeDescriptor.StableScope = TEXT("angelscript.builtin");
	TypeDescriptor.Kind = asETypeDeclKind::Class;
	asCObjectType* Object = nullptr;
	asCObjectType* Function = nullptr;
	asSStableKey ObjectKey, FunctionKey;
	TypeDescriptor.Name = TEXT("$obj");
	auto IdentityResult = Registry.InternNominalType(TypeDescriptor, ObjectKey);
	asASSERT(IdentityResult == asETypeIdentityStatus::Succeeded);
	Result = Image->CreateObjectType(ObjectKey, "$obj", "", asOBJ_REF | asOBJ_SCRIPT_OBJECT | asOBJ_NOCOUNT, Object);
	asASSERT(Result == asEMetadataResult::Succeeded);
	TypeDescriptor.Name = TEXT("$func");
	IdentityResult = Registry.InternNominalType(TypeDescriptor, FunctionKey);
	asASSERT(IdentityResult == asETypeIdentityStatus::Succeeded);
	Result = Image->CreateObjectType(FunctionKey, "$func", "", asOBJ_REF | asOBJ_SCRIPT_OBJECT | asOBJ_NOCOUNT, Function);
	asASSERT(Result == asEMetadataResult::Succeeded);
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
	Result = Image->CreateFunction(MethodKey, "$obj", "", asFUNC_SYSTEM, Declaration);
	asASSERT(Result == asEMetadataResult::Succeeded);
	Result = Image->DefineFunction(*Declaration, Image->Primitive(ttVoid), {}, Object);
	asASSERT(Result == asEMetadataResult::Succeeded);
	Result = Image->SetFunctionDeclarationFacts(*Declaration, asEFunctionIdentityKind::Constructor, Object);
	asASSERT(Result == asEMetadataResult::Succeeded);
	Result = Image->AddBehaviour(*Object, asBEHAVE_CONSTRUCT, *Declaration);
	asASSERT(Result == asEMetadataResult::Succeeded);
	Result = Image->FinalizeLayouts(); asASSERT(Result == asEMetadataResult::Succeeded);
	Result = Image->Freeze(); asASSERT(Result == asEMetadataResult::Succeeded);
	const auto Registration = RegisterMetadataImage(MoveTemp(Image));
	asASSERT(Registration == asEMetadataRegistrationResult::Succeeded);
	if (Registration != asEMetadataRegistrationResult::Succeeded) configFailed = true;
	UNUSED_VAR(Result); UNUSED_VAR(IdentityResult);
}

asUINT asCScriptEngine::CountMetadataTypes(asQWORD Include, asQWORD Exclude) const
{
	std::lock_guard<std::mutex> Lock(metadataRegistrationMutex);
	asUINT Count = 0;
	for (const auto& Image : metadataImages)
		for (asCTypeInfo* Type : Image->Types)
			if ((Type->flags & Include) && !(Type->flags & Exclude)) ++Count;
	return Count;
}

asCTypeInfo* asCScriptEngine::MetadataTypeByIndex(asUINT Index, asQWORD Include, asQWORD Exclude) const
{
	std::lock_guard<std::mutex> Lock(metadataRegistrationMutex);
	for (const auto& Image : metadataImages)
		for (asCTypeInfo* Type : Image->Types)
			if ((Type->flags & Include) && !(Type->flags & Exclude))
			{
				if (Index == 0) return Type;
				--Index;
			}
	return nullptr;
}

END_AS_NAMESPACE
