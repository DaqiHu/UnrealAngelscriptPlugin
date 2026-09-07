/* Maintained Unreal AngelScript fork — SDK object header before payload. */
#pragma once

#include "as_config.h"
#include "as_atomic.h"

BEGIN_AS_NAMESPACE

class asCObjectType;
class asCScriptEngine;
class asCLockableSharedBool;

struct asSVmObjectHeader
{
	static constexpr asUINT MagicValue = 0x564D4F42u;
	asUINT Magic = 0;
	asUINT PayloadSize = 0;
	asCAtomic RefCount;
	bool GcFlag = false;
	bool InGc = false;
	asCLockableSharedBool* WeakFlag = nullptr;
	asCObjectType* Type = nullptr;
	asCScriptEngine* Engine = nullptr;
};

ANGELSCRIPTRUNTIME_API void* asVmAllocateObject(asCObjectType* Type);
ANGELSCRIPTRUNTIME_API void asVmFreeObject(void* Payload);
ANGELSCRIPTRUNTIME_API asSVmObjectHeader* asVmHeaderFromPayload(void* Payload);
ANGELSCRIPTRUNTIME_API asCObjectType* asVmTypeFromPayload(void* Payload);
ANGELSCRIPTRUNTIME_API void asVmInvokeDestruct(void* Payload, asCObjectType* Type);
ANGELSCRIPTRUNTIME_API int asVmAddRef(void* Payload);
ANGELSCRIPTRUNTIME_API int asVmRelease(void* Payload);
ANGELSCRIPTRUNTIME_API int asVmGetRefCount(void* Payload);
ANGELSCRIPTRUNTIME_API void asVmSetGcFlag(void* Payload, bool Flag);
ANGELSCRIPTRUNTIME_API bool asVmGetGcFlag(void* Payload);
ANGELSCRIPTRUNTIME_API void asVmEnumReferences(void* Payload, asCScriptEngine* Engine);
ANGELSCRIPTRUNTIME_API void asVmReleaseAllReferences(void* Payload, asCScriptEngine* Engine);
ANGELSCRIPTRUNTIME_API void asVmAbortPartialConstruction(void* Payload, asUINT ConstructedMemberCount);
ANGELSCRIPTRUNTIME_API asCLockableSharedBool* asVmGetWeakFlag(void* Payload);

END_AS_NAMESPACE
