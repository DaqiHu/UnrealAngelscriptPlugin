#include "as_vm_object.h"
#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"
#include "as_scriptobject.h"
#include "as_context.h"
#include "as_property.h"
#include "HAL/CriticalSection.h"
#include "HAL/UnrealMemory.h"

BEGIN_AS_NAMESPACE

namespace
{
	FCriticalSection GVmPayloadMutex;
	TMap<void*, asSVmObjectHeader*> GVmPayloadHeaders;
	thread_local const asCScriptEngine* GVmCleanupEngine = nullptr;

	void* AlignPointer(void* Pointer, asUINT Alignment)
	{
		const asPWORD Value = reinterpret_cast<asPWORD>(Pointer);
		const asPWORD Mask = static_cast<asPWORD>(Alignment) - 1;
		const asPWORD Aligned = (Value + Mask) & ~Mask;
		return reinterpret_cast<void*>(Aligned);
	}

	void RegisterPayload(void* Payload, asSVmObjectHeader* Header)
	{
		FScopeLock Lock(&GVmPayloadMutex);
		GVmPayloadHeaders.Add(Payload, Header);
	}

	void UnregisterPayload(void* Payload)
	{
		FScopeLock Lock(&GVmPayloadMutex);
		GVmPayloadHeaders.Remove(Payload);
	}
}

bool asVmHasCleanupAuthority(const asCScriptEngine* Engine)
{
	return Engine && Engine == GVmCleanupEngine;
}

void* asVmAllocateObject(asCObjectType* Type)
{
	if (!Type) return nullptr;
	if (Type->GetMetadataImage() && Type->GetMetadataImage()->GetState() == asEMetadataImageState::Retired) return nullptr;
	asCScriptEngine* Engine = static_cast<asCScriptEngine*>(Type->GetEngine());
	if (Engine && !Engine->AcquireVmObjectLease()) return nullptr;
	Type->AddRef();
	const asUINT PayloadSize = Type->size > 0 ? static_cast<asUINT>(Type->size) : 4u;
	asUINT Align = static_cast<asUINT>(FMath::Max(8, Type->alignment));
	if (Align < static_cast<asUINT>(sizeof(void*))) Align = static_cast<asUINT>(sizeof(void*));
	const asUINT Prefix = sizeof(asSVmObjectHeader) + sizeof(void*);
	void* Base = FMemory::Malloc(Prefix + PayloadSize + Align, Align);
	if (!Base) { Type->Release(); if (Engine) Engine->ReleaseVmObjectLease(); return nullptr; }
	asSVmObjectHeader* Header = static_cast<asSVmObjectHeader*>(Base);
	Header->Magic = asSVmObjectHeader::MagicValue;
	Header->PayloadSize = PayloadSize;
	Header->RefCount.set(1);
	Header->GcFlag = false;
	Header->InGc = false;
	Header->WeakFlag = nullptr;
	Header->Type = Type;
	Header->Engine = Engine;
	char* Payload = static_cast<char*>(AlignPointer(static_cast<char*>(Base) + Prefix, Align));
	void** BackPointer = reinterpret_cast<void**>(Payload - sizeof(void*));
	*BackPointer = Base;
	FMemory::Memzero(Payload, PayloadSize);
	RegisterPayload(Payload, Header);
	if ((Type->flags & asOBJ_GC) && Type->GetEngine())
	{
		static_cast<asCScriptEngine*>(Type->GetEngine())->NotifyGarbageCollectorOfNewObject(Payload, Type);
		if (asSVmObjectHeader* Published = asVmHeaderFromPayload(Payload))
		{
			Published->InGc = true;
		}
	}
	return Payload;
}

void asVmFreeObject(void* Payload)
{
	asSVmObjectHeader* Header = asVmHeaderFromPayload(Payload);
	if (!Header) return;
	asCObjectType* Type = Header->Type;
	asCScriptEngine* Engine = Header->Engine;
	UnregisterPayload(Payload);
	FMemory::Free(Header);
	Type->Release();
	if (Engine) Engine->ReleaseVmObjectLease();
}

asSVmObjectHeader* asVmHeaderFromPayload(void* Payload)
{
	if (!Payload) return nullptr;
	FScopeLock Lock(&GVmPayloadMutex);
	if (asSVmObjectHeader* const* Found = GVmPayloadHeaders.Find(Payload)) return *Found;
	return nullptr;
}

asCObjectType* asVmTypeFromPayload(void* Payload)
{
	asSVmObjectHeader* Header = asVmHeaderFromPayload(Payload);
	return Header ? Header->Type : nullptr;
}

void asVmInvokeDestruct(void* Payload, asCObjectType* Type)
{
	if (!Payload || !Type) return;
	asCScriptEngine* Engine = static_cast<asCScriptEngine*>(Type->GetEngine());
	if (!Engine)
	{
		if (asSVmObjectHeader* Header = asVmHeaderFromPayload(Payload))
		{
			Engine = Header->Engine;
		}
	}
	if (!Engine || !Engine->BeginVmOperation(true)) return;
	struct FEndCleanup
	{
		asCScriptEngine* Engine;
		const asCScriptEngine* Previous;
		~FEndCleanup()
		{
			GVmCleanupEngine = Previous;
			Engine->EndVmOperation();
		}
	} Cleanup{Engine, GVmCleanupEngine};
	GVmCleanupEngine = Engine;
	for (asUINT Index = 0; Index < Type->metadataBehaviours.GetLength(); ++Index)
	{
		const asCObjectType::asSMetadataBehaviour& Behaviour = Type->metadataBehaviours[Index];
		if (Behaviour.kind != asBEHAVE_DESTRUCT || !Behaviour.function) continue;
		asCScriptFunction* Function = Behaviour.function;
		if (Function->funcType == asFUNC_SYSTEM)
		{
			if (auto Interface = Engine->AcquireSystemInterface(Function))
			{
				Engine->CallObjectMethod(Payload, Interface.get(), Function);
			}
			continue;
		}
		if (Function->funcType != asFUNC_SCRIPT) continue;
		asIScriptContext* Context = Engine->CreateContext();
		if (!Context) continue;
		if (static_cast<asCContext*>(Context)->PrepareForVmCleanup(Function) >= 0)
		{
			Context->SetObject(Payload);
			Context->Execute();
		}
		Context->Release();
	}
}

int asVmAddRef(void* Payload)
{
	asSVmObjectHeader* Header = asVmHeaderFromPayload(Payload);
	if (!Header) return 0;
	Header->GcFlag = false;
	return static_cast<int>(Header->RefCount.atomicInc());
}

int asVmRelease(void* Payload)
{
	asSVmObjectHeader* Header = asVmHeaderFromPayload(Payload);
	if (!Header || Header->RefCount.get() == 0) return 0;
	Header->GcFlag = false;
	const asDWORD Remaining = Header->RefCount.atomicDec();
	if (Remaining > 0)
	{
		// A shutdown collector may own the last reference. Do not touch the
		// payload after draining, since it may have been destroyed by the GC.
		asCScriptEngine* Engine = Header->Engine;
		if (Engine && Header->InGc) Engine->DrainVmRetirement();
		return static_cast<int>(Remaining);
	}
	asCObjectType* Type = Header->Type;
	if (Header->WeakFlag)
	{
		Header->WeakFlag->Set(true);
		Header->WeakFlag->Release();
		Header->WeakFlag = nullptr;
	}
	asVmInvokeDestruct(Payload, Type);
	asVmFreeObject(Payload);
	return 0;
}

int asVmGetRefCount(void* Payload)
{
	asSVmObjectHeader* Header = asVmHeaderFromPayload(Payload);
	return Header ? static_cast<int>(Header->RefCount.get()) : 0;
}

void asVmSetGcFlag(void* Payload, bool Flag)
{
	if (asSVmObjectHeader* Header = asVmHeaderFromPayload(Payload)) Header->GcFlag = Flag;
}

bool asVmGetGcFlag(void* Payload)
{
	asSVmObjectHeader* Header = asVmHeaderFromPayload(Payload);
	return Header && Header->GcFlag;
}

namespace
{
	template <typename Callback>
	void ForEachVmHandle(void* Payload, Callback&& Visit)
	{
		asCObjectType* Type = asVmTypeFromPayload(Payload);
		if (!Type) return;
		const asCArray<asCObjectProperty*>& Properties =
			Type->localProperties.GetLength() ? Type->localProperties : Type->properties;
		for (asUINT Index = 0; Index < Properties.GetLength(); ++Index)
		{
			asCObjectProperty* Property = Properties[Index];
			if (!Property) continue;
			if (!Property->type.IsObjectHandle() && !Property->type.IsCallableType()) continue;
			void** Slot = reinterpret_cast<void**>(static_cast<char*>(Payload) + Property->byteOffset);
			if (*Slot) Visit(*Slot, Property);
		}
	}
}

void asVmEnumReferences(void* Payload, asCScriptEngine* Engine)
{
	if (!Engine) return;
	ForEachVmHandle(Payload, [Engine](void* Reference, asCObjectProperty*)
	{
		Engine->GCEnumCallback(Reference);
	});
}

void asVmReleaseAllReferences(void* Payload, asCScriptEngine* Engine)
{
	if (!Engine) return;
	ForEachVmHandle(Payload, [Engine, Payload](void* Reference, asCObjectProperty* Property)
	{
		void** Slot = reinterpret_cast<void**>(static_cast<char*>(Payload) + Property->byteOffset);
		*Slot = nullptr;
		if (asCObjectType* Held = asVmTypeFromPayload(Reference))
		{
			Engine->ReleaseScriptObject(Reference, Held);
			return;
		}
		if (Property && Property->type.IsCallableType() && Reference)
		{
			static_cast<asCScriptFunction*>(Reference)->Release();
		}
	});
}

void asVmAbortPartialConstruction(void* Payload, asUINT ConstructedMemberCount)
{
	asSVmObjectHeader* Header = asVmHeaderFromPayload(Payload);
	if (!Header || !Header->Type) return;
	asCObjectType* Type = Header->Type;
	const asCArray<asCObjectProperty*>& Properties =
		Type->localProperties.GetLength() ? Type->localProperties : Type->properties;
	TArray<asCObjectProperty*> ValueMembers;
	for (asUINT Index = 0; Index < Properties.GetLength(); ++Index)
	{
		asCObjectProperty* Property = Properties[Index];
		if (!Property || !Property->type.IsObject() || Property->type.IsObjectHandle()) continue;
		ValueMembers.Add(Property);
	}
	const asUINT Limit = FMath::Min(ConstructedMemberCount, static_cast<asUINT>(ValueMembers.Num()));
	for (asUINT Index = Limit; Index > 0; --Index)
	{
		asCObjectProperty* Property = ValueMembers[Index - 1];
		asCObjectType* MemberType = static_cast<asCObjectType*>(Property->type.GetTypeInfo());
		if (!MemberType) continue;
		asVmInvokeDestruct(static_cast<char*>(Payload) + Property->byteOffset, MemberType);
	}
	asVmFreeObject(Payload);
}

asCLockableSharedBool* asVmGetWeakFlag(void* Payload)
{
	asSVmObjectHeader* Header = asVmHeaderFromPayload(Payload);
	if (!Header) return nullptr;
	if (!Header->WeakFlag) Header->WeakFlag = asNEW(asCLockableSharedBool);
	if (Header->WeakFlag) Header->WeakFlag->AddRef();
	return Header->WeakFlag;
}

END_AS_NAMESPACE
