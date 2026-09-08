/* Maintained Unreal AngelScript fork — transactional semantic registration. */
#include "as_scriptengine.h"
#include "as_namespace.h"
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

asEMetadataRegistrationResult asCScriptEngine::RegisterMetadataImage(const std::shared_ptr<asCMetadataImage>& Image)
{
	if (!Image) return asEMetadataRegistrationResult::InvalidArgument;
	std::lock_guard<std::mutex> EngineLock(metadataRegistrationMutex);
	if (shuttingDown || vmRetirementRequested || metadataRegistrationRetired) return asEMetadataRegistrationResult::Retired;
	if (!Image->IsFrozen()) return asEMetadataRegistrationResult::InvalidState;
	TArray<std::shared_ptr<asCMetadataImage>> Closure;
	TSet<asCMetadataImage*> Visited;
	std::function<void(const std::shared_ptr<asCMetadataImage>&)> Visit = [&](const auto& Item)
	{
		if (Visited.Contains(Item.get())) return;
		Visited.Add(Item.get());
		for (const auto& Dependency : Item->Dependencies) Visit(Dependency);
		Closure.Add(Item);
	};
	Visit(Image);
	// Every Engine acquires images in this order, so overlapping dependency DAGs
	// cannot form a lock cycle across two competing Engine registration calls.
	std::vector<asCMetadataImage*> LockOrder;
	for (const auto& Item : Closure) LockOrder.push_back(Item.get());
	std::sort(LockOrder.begin(), LockOrder.end(), std::less<asCMetadataImage*>{});
	std::vector<std::unique_lock<std::mutex>> ImageLocks;
	for (auto* Item : LockOrder) ImageLocks.emplace_back(Item->MutationMutex);
	if (Image->GetState() == asEMetadataImageState::Retired) return asEMetadataRegistrationResult::Retired;
	if (Image->BoundEngine) return Image->BoundEngine == this ? asEMetadataRegistrationResult::AlreadyRegistered : asEMetadataRegistrationResult::ForeignEngine;
	for (const auto& Item : Closure)
	{
		if (Item->GetState() == asEMetadataImageState::Retired) return asEMetadataRegistrationResult::Retired;
		if (Item->BoundEngine && Item->BoundEngine != this) return asEMetadataRegistrationResult::ForeignEngine;
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
	TMap<const asCTypeInfo*, int> ClosureTypeIds;
	TMap<const asCScriptFunction*, int> ClosureFunctionIds;
	for (const auto& Item : Closure)
	{
		if (Item->BoundEngine == this)
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
			if (NextType > asTYPEID_MASK_SEQNBR) return asEMetadataRegistrationResult::InvalidState;
			int Id = NextType++;
			if (Type->flags & asOBJ_SCRIPT_OBJECT) Id |= asTYPEID_SCRIPTOBJECT;
			else if (Type->flags & asOBJ_TEMPLATE) Id |= asTYPEID_TEMPLATE;
			else if (!(Type->flags & asOBJ_ENUM)) Id |= asTYPEID_APPOBJECT;
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
			if (NextFunction == MAX_int32) return asEMetadataRegistrationResult::InvalidState;
			const int Id = NextFunction++;
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
	for (const auto& Item : Closure)
	{
		if (Item->BoundEngine == this) continue;
		Item->State.store(asEMetadataImageState::Attaching, std::memory_order_release);
		// Retain bindings only for this image's actual dependency DAG, never
		// unrelated siblings that happened to participate in the transaction.
		for (const auto& Pair : ClosureTypeIds)
			if (Item->CanReference(Pair.Key)) Item->BoundTypeIds.Add(Pair.Key, Pair.Value);
		for (const auto& Pair : ClosureFunctionIds)
			if (Item->Owns(Pair.Key)) Item->BoundFunctionIds.Add(Pair.Key, Pair.Value);
		Item->BoundEngine = this;
		metadataImages.Add(Item);
	}
	metadataTypesByKey = MoveTemp(TypesByKey); metadataTypesByName = MoveTemp(TypesByName); metadataTypesById = MoveTemp(TypesById);
	metadataFunctionsByKey = MoveTemp(FunctionsByKey); metadataFunctionsById = MoveTemp(FunctionsById);
	metadataGlobalsByKey = MoveTemp(GlobalsByKey); metadataGlobalsByName = MoveTemp(GlobalsByName);
	nextMetadataTypeId = NextType; nextMetadataFunctionId = NextFunction;
	for (const auto& Item : Closure) Item->State.store(asEMetadataImageState::Attached, std::memory_order_release);
	// Engine queries take EngineLock; image binding queries take their image
	// lock. Neither can see IDs or visibility from a partially committed graph.
	return asEMetadataRegistrationResult::Succeeded;
}

void asCScriptEngine::RetireMetadataImages()
{
	TArray<std::shared_ptr<asCMetadataImage>> ReleasedImages;
	{
		std::lock_guard<std::mutex> EngineLock(metadataRegistrationMutex);
		if (metadataRegistrationRetired) return;
		std::unique_lock<std::mutex> PublishLock(executablePublicationMutex);
		if (!vmRetirementRequested.exchange(true)) ++executablePublicationGeneration;
		if (vmObjectLeases || vmOperations || vmDrainActive) return;
		std::vector<asCMetadataImage*> LockOrder;
		for (const auto& Item : metadataImages) LockOrder.push_back(Item.get());
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

TArray<std::shared_ptr<const asCMetadataImage>> asCScriptEngine::GetRegisteredMetadataImages() const
{
	std::lock_guard<std::mutex> Lock(metadataRegistrationMutex);
	TArray<std::shared_ptr<const asCMetadataImage>> Result;
	Result.Reserve(metadataImages.Num());
	for (const auto& Image : metadataImages) Result.Add(Image);
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
	const auto Registration = RegisterMetadataImage(Image);
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
