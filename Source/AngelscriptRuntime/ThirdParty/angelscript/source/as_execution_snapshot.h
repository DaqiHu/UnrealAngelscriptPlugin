/* Maintained Unreal AngelScript fork — Engine-owned executable snapshot. */
#pragma once

#include "as_bytecode_image.h"
#include "as_atomic.h"
#include "as_memory.h"
#include "as_scriptfunction.h"
#include "as_callfunc.h"

#include "Containers/Map.h"
#include "Templates/UniquePtr.h"

BEGIN_AS_NAMESPACE

class asCScriptEngine;
class asCTypeInfo;
class asCExecutableSnapshot;

enum class asEByteCodeLinkStatus : asBYTE
{
	Succeeded,
	InvalidArgument,
	Unverified,
	MissingDeclaration,
	SchemaMismatch,
	BodyConflict,
	ResourceFailure,
	MissingBinding,
	Retired
};

class ANGELSCRIPTRUNTIME_API asCExecutableFunction
{
public:
	asCScriptFunction* Declaration = nullptr;
	asCScriptFunction::ScriptFunctionData* scriptData = nullptr;
	asSSystemFunctionInterface* sysFuncIntf = nullptr;
	asSStableKey Key;
	asCExecutableSnapshot* Owner = nullptr;

	asCExecutableFunction();
	~asCExecutableFunction();
	void AllocateScriptData();
};

class ANGELSCRIPTRUNTIME_API asCExecutableSnapshot
{
public:
	asCScriptEngine* Engine = nullptr;
	TUniquePtr<asCByteCodeImage> Image;
	TMap<asSStableKey, asCScriptFunction*> Functions;
	TMap<asSStableKey, asCTypeInfo*> Types;
	TMap<asSStableKey, TUniquePtr<asCExecutableFunction>> Executables;
	void* NativeBindings[1] = {};

	asCExecutableSnapshot() { RefCount.set(0); }
	~asCExecutableSnapshot();
	int AddRef() const { return RefCount.atomicInc(); }
	int Release() const
	{
		const int Remaining = RefCount.atomicDec();
		if (Remaining == 0) asDELETE(const_cast<asCExecutableSnapshot*>(this), asCExecutableSnapshot);
		return Remaining;
	}

	asCScriptFunction* FindFunction(const asSStableKey& Key) const
	{
		if (const auto* Found = Functions.Find(Key)) return *Found;
		return nullptr;
	}

	asCExecutableFunction* FindExecutable(const asSStableKey& Key) const
	{
		if (const TUniquePtr<asCExecutableFunction>* Found = Executables.Find(Key)) return Found->Get();
		return nullptr;
	}

private:
	mutable asCAtomic RefCount;
};

END_AS_NAMESPACE
