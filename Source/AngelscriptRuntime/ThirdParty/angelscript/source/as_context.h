/*
   AngelCode Scripting Library
   Copyright (c) 2003-2018 Andreas Jonsson

   This software is provided 'as-is', without any express or implied
   warranty. In no event will the authors be held liable for any
   damages arising from the use of this software.

   Permission is granted to anyone to use this software for any
   purpose, including commercial applications, and to alter it and
   redistribute it freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you
      must not claim that you wrote the original software. If you use
      this software in a product, an acknowledgment in the product
      documentation would be appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and
      must not be misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
      distribution.

   The original version of this library can be located at:
   http://www.angelcode.com/angelscript/

   Andreas Jonsson
   andreas@angelcode.com
*/


//
// as_context.h
//
// This class handles the execution of the byte code
//


#ifndef AS_CONTEXT_H
#define AS_CONTEXT_H

#include "as_config.h"
#include "as_atomic.h"
#include "as_array.h"
#include "as_string.h"
#include "as_thread.h"
#include "as_objecttype.h"
#include "as_callfunc.h"
#include "as_scriptfunction.h"
#include "AngelscriptEngine.h"

BEGIN_AS_NAMESPACE

class asCScriptFunction;
class asCScriptEngine;
class asCExecutableSnapshot;
class asCExecutableFunction;
struct FScriptExecution;
struct asSStableKey;
ANGELSCRIPTRUNTIME_API void asVmInvokeDestruct(void*, asCObjectType*);

// Call-scoped exception payload shared by direct JIT calls. Context-mediated
// calls copy their first public exception into the caller's record exactly
// once. The bool on FScriptExecution remains the generated-code fast path;
// this record retains the diagnostic identity needed when control returns to
// the public asIScriptContext.
struct asSJITFailureRecord
{
	bool bHasFailure = false;
	bool bReported = false;
	bool bAdopted = false;
	int FunctionId = -1;
	int Line = 0;
	int Column = 0;
	asCString Message;
	asCString StableFunctionIdentity;
	asCString SectionName;
	asCString RouteOrigin;
	asCString BackendOrigin;
	// Owned, pointer-free diagnostic lines captured at the first JIT failure.
	// A nested temporary context may unwind its live debug-frame chain before
	// the public caller publishes the shared exception, so delayed reporting
	// must not retain FScopeJITDebugCallstack or UObject pointers.
	asCString CapturedStackModule;
	asCArray<asCString> CapturedStackFrames;
};


using asLineCallback = void(*)(asCContext*);
using asStackPopCallback = void(*)(asCContext*, void* oldStackFrameStart, void* oldStackFrameEnd);

//[UE++]: Rename export macro to match the runtime module rename.
class ANGELSCRIPTRUNTIME_API asCContext final : public asIScriptContext
//[UE--]
{
public:
	// Memory management
	int  AddRef() const;
	int  Release() const;

	void MovedToNewThread();

	// Miscellaneous
	asIScriptEngine *GetEngine() const;

	// Execution
private:
	friend ANGELSCRIPTRUNTIME_API void asVmInvokeDestruct(void*, asCObjectType*);
	int PrepareForVmCleanup(asCScriptFunction* Function);
	bool m_vmCleanup = false;
public:
	int             Prepare(asIScriptFunction *func);
	int             Prepare(asCExecutableSnapshot* Snapshot, const asSStableKey& Key);
	int             Unprepare();
	int             Execute();
	int             Abort();
	int             Suspend();
	asEContextState GetState() const;
	int             PushState();
	int             PopState();
	bool            IsNested(asUINT *nestCount = 0) const;

	// Object pointer for calling class methods
	int SetObject(void *obj);

	// Arguments
	int   SetArgByte(asUINT arg, asBYTE value);
	int   SetArgWord(asUINT arg, asWORD value);
	int   SetArgDWord(asUINT arg, asDWORD value);
	int   SetArgQWord(asUINT arg, asQWORD value);
	int   SetArgFloat(asUINT arg, float value);
	int   SetArgDouble(asUINT arg, double value);
	int   SetArgAddress(asUINT arg, void *addr);
	int   SetArgObject(asUINT arg, void *obj);
	int   SetArgVarType(asUINT arg, void *ptr, int typeId);
	void *GetAddressOfArg(asUINT arg);

	// Return value
	asBYTE  GetReturnByte();
	asWORD  GetReturnWord();
	asDWORD GetReturnDWord();
	asQWORD GetReturnQWord();
	float   GetReturnFloat();
	double  GetReturnDouble();
	void   *GetReturnAddress();
	void   *GetReturnObject();
	void   *GetAddressOfReturnValue();
	void   SetReturnIntoValue(void* ReturnValue);

	// Exception handling
	int                SetException(const char *descr);
	int                GetExceptionLineNumber(int *column, const char **sectionName);
	asIScriptFunction *GetExceptionFunction();
	const char *       GetExceptionString();
	bool               WillExceptionBeCaught();
	int                SetExceptionCallback(asSFuncPtr callback, void *obj, int callConv);
	void               ClearExceptionCallback();

	// Debugging
	int                SetLineCallback(asLineCallback Callback);
	int                SetLoopDetectionCallback(asLineCallback Callback);
	int                SetStackPopCallback(asStackPopCallback Callback);
	int                SetInstructionCallback(asVMInstructionCallback Callback, void* UserData = 0);
	void               ClearLineCallback();
	void               ClearStackPopCallback();
	void               ClearInstructionCallback();
	asUINT             GetCallstackSize() const;
	asIScriptFunction *GetFunction(asUINT stackLevel);
	asUINT             GetBlueprintCallstackFrame(asUINT stackLevel);
	int                GetLineNumber(asUINT stackLevel, int *column, const char **sectionName);
	int                GetVarCount(asUINT stackLevel);
	const char        *GetVarName(asUINT varIndex, asUINT stackLevel);
	const char        *GetVarDeclaration(asUINT varIndex, asUINT stackLevel, bool includeNamespace);
	int                GetVarTypeId(asUINT varIndex, asUINT stackLevel);
	void              *GetAddressOfVar(asUINT varIndex, asUINT stackLevel);
	bool               IsVarInScope(asUINT varIndex, asUINT stackLevel);
	int                GetThisTypeId(asUINT stackLevel);
    void              *GetThisPointer(asUINT stackLevel);
    void              *GetStackFrame(asUINT stackLevel);
	int                GetStackFrameSize(asUINT stackLevel) const;

	// User data
	void *SetUserData(void *data, asPWORD type);
	void *GetUserData(asPWORD type) const;
	// [UE++]: Borrowed from the exact acquired native binding for this invocation.
	void *GetNativeCallAuxiliary() const { return m_nativeCallAuxiliary; }

private:
	void *m_nativeCallAuxiliary = nullptr;
public:

	void* DebugFramePtr = nullptr;
public:
	// Internal public functions
	asCContext(asCScriptEngine *engine, bool holdRef);
	virtual ~asCContext();

//protected:
	friend class asCScriptEngine;
	friend struct FScriptExecution;
	friend int CallSystemFunction(int id, asCContext *context);
	friend int CallSystemFunction(asCScriptFunction *function, asCContext *context);

	void CallExceptionCallback();

	int  CallGeneric(asCScriptFunction *func, const std::shared_ptr<asSSystemFunctionInterface>& Binding = {});
	int  CallFunctionCaller(asCScriptFunction *func, const std::shared_ptr<asSSystemFunctionInterface>& Binding = {});
#ifndef AS_NO_EXCEPTIONS
	void HandleAppException();
#endif
	void DetachEngine();

	void ExecuteNext();
	void NotifyInstructionCallback(asEVMInstructionPhase Phase, asBYTE Instruction, const char* InstructionName, asDWORD* ProgramPointer, asDWORD* StackPointer, asDWORD* StackFramePointer);
	void CleanStack(bool catchException = false);
	bool CleanStackFrame(bool catchException = false);
	void CleanArgsOnStack();
	void CleanReturnObject();
	void ReleaseRegisteredVmPayloadsFromFrame();
	void DetermineLiveObjects(asCArray<int> &liveObjects, asUINT stackLevel);

	void PushCallState();
	void PopCallState();
	void CallScriptFunction(asCScriptFunction *func);
	bool CallInterfaceMethod(asCScriptFunction *func);
	void PrepareScriptFunction();
	void BindExecutableFor(asCScriptFunction* Function);
	void ReleaseBoundSnapshot();
	asCScriptFunction::ScriptFunctionData* ActiveScriptData() const;

	bool ReserveStackSpace(asUINT size);

	void SetInternalException(const char *descr, bool allowCatch = true);
	bool FindExceptionTryCatch();

	static bool CanEverRunLineCallback;
	static bool ShouldAlwaysRunLineCallback;

	// Must be protected for multiple accesses
	mutable asCAtomic m_refCount;

	bool             m_holdEngineRef;
	asCScriptEngine *m_engine;

	asEContextState m_status;
	bool            m_doSuspend;
	bool            m_doAbort;
	bool            m_externalSuspendRequest;
	bool m_executeVirtualCall;

	asCScriptFunction *m_currentFunction;
	asCExecutableFunction *m_currentExecutable;
	asCExecutableFunction *m_initialExecutable;
	asCExecutableSnapshot *m_boundSnapshot;
	asCScriptFunction *m_callingSystemFunction;

	// The call stack holds program pointer, stack pointer, etc for caller functions
	asCArray<size_t>    m_callStack;

	// Dynamically growing local stack
	asCArray<asDWORD *> m_stackBlocks;
	asUINT              m_stackBlockSize;
	asUINT              m_stackIndex;
	asDWORD            *m_originalStackPointer;
	asUINT              m_originalStackIndex;
	asUINT              m_blueprintStackFrameIndex;

	// Exception handling
	bool      m_isStackMemoryNotAllocated;
	bool      m_needToCleanupArgs;
	bool      m_inExceptionHandler;
	bool      m_returnIsExternal;
	asCString m_exceptionString;
	int       m_exceptionFunction;
	int       m_exceptionSectionIdx;
	int       m_exceptionLine;
	int       m_exceptionColumn;
	bool      m_exceptionWillBeCaught;
	// Preserves Runtime-owned diagnostic provenance across a temporary
	// context boundary. Standard VM exceptions leave this record empty.
	asSJITFailureRecord m_jitFailureRecord;

	// The last prepared function, and some cached values related to it
	asCScriptFunction *m_initialFunction;
	int                m_returnValueSize;
	int                m_argumentsSize;
	asSVMCallFrame m_initialCallFrame;

	// callbacks
	asLineCallback m_lineCallback = nullptr;
	asLineCallback m_loopDetectionCallback = nullptr;
	asStackPopCallback m_stackPopCallback = nullptr;
	asVMInstructionCallback m_instructionCallback = nullptr;
	void* m_instructionCallbackUserData = nullptr;
	double m_loopDetectionTimer = -1.0;
	int m_loopDetectionCounter = 0;
	int m_loopDetectionExclusionCounter = 0;

	bool                       m_exceptionCallback;
	asSSystemFunctionInterface m_exceptionCallbackFunc;
	void *                     m_exceptionCallbackObj;

	asCArray<asPWORD> m_userData;

	// Registers available to JIT compiler functions
	asSVMRegisters m_regs;

#if AS_REFERENCE_DEBUGGING
	TArray<void**> TrackedReferences;

	void InvalidateReferencesToMemoryBlock(void* Address, SIZE_T Size);
#endif
};

struct FScriptExecution
{
	// Match the maintained VM nested-call sanity limit: m_callStack uses ten
	// words per frame and rejects once its length grows beyond 10000. Native
	// TypedASTJIT helpers do not grow that VM array, so they share this explicit
	// call-scoped frame counter instead.
	static constexpr asUINT DefaultNativeJITFrameLimit = 1000;

	class asCThreadLocalData* tld;
	FScriptExecution* prevExecution;
	class asCContext* prevContext;
	// Runtime-owned, call-scoped StaticJIT context. Generated code may inspect
	// these values but must never retain them beyond the current entry call.
	asIScriptEngine* jitEngine = nullptr;
	asIScriptFunction* jitFunction = nullptr;
	const asSJITFunctionBinding* jitBinding = nullptr;
	void* jitBindingUserData = nullptr;
	const void* jitReferenceTable = nullptr;
#if !UE_BUILD_SHIPPING
	void* debugCallStack = nullptr;
#endif

	bool bExceptionThrown;
	asSJITFailureRecord OwnedFailureRecord;
	asSJITFailureRecord* FailureRecord = nullptr;
	asUINT OwnedNativeJITFrameDepth = 0;
	asUINT OwnedNativeJITFrameLimit = DefaultNativeJITFrameLimit;
	asUINT* NativeJITFrameDepth = &OwnedNativeJITFrameDepth;
	asUINT* NativeJITFrameLimit = &OwnedNativeJITFrameLimit;
	FScriptActivation Activation;

	bool TryRecordException(
		const char* Message,
		asIScriptFunction* Function,
		const char* StableFunctionIdentity,
		const char* SectionName,
		int Line,
		int Column,
		const char* RouteOrigin,
		const char* BackendOrigin,
		bool bReported,
		bool bAdopted = false);
	bool AdoptException(asIScriptContext& Context);
	bool PublishException(asCContext& Context);

	bool TryEnterNativeJITFrame()
	{
		if (NativeJITFrameDepth == nullptr || NativeJITFrameLimit == nullptr
			|| *NativeJITFrameDepth >= *NativeJITFrameLimit)
			return false;
		++*NativeJITFrameDepth;
		return true;
	}

	void LeaveNativeJITFrame()
	{
		asASSERT(NativeJITFrameDepth != nullptr
			&& *NativeJITFrameDepth > 0);
		--*NativeJITFrameDepth;
	}

	asUINT GetNativeJITFrameDepth() const
	{
		return NativeJITFrameDepth != nullptr
			? *NativeJITFrameDepth : 0;
	}

	bool HasContextExceptionPublisher() const
	{
		for (const FScriptExecution* Execution = this;
			Execution != nullptr;
			Execution = Execution->prevExecution)
		{
			if (Execution->prevContext != nullptr)
				return true;
		}
		return false;
	}

	FScriptExecution(asCContext* ctx)
		: tld(ctx->m_regs.tld)
		, bExceptionThrown(false)
	{
		prevExecution = tld->activeExecution;
		prevContext = tld->activeContext;
		FailureRecord = prevExecution != nullptr
			&& prevExecution->FailureRecord != nullptr
			? prevExecution->FailureRecord : &OwnedFailureRecord;
		if (prevExecution != nullptr)
		{
			NativeJITFrameDepth = prevExecution->NativeJITFrameDepth;
			NativeJITFrameLimit = prevExecution->NativeJITFrameLimit;
		}
		Activation.Execution = this;
		Activation.Previous = tld->activeActivation;
		tld->activeActivation = &Activation;
		tld->activeExecution = this;
		tld->activeContext = nullptr;
	}

	FScriptExecution(class asCThreadLocalData* tld)
		: tld(tld)
		, bExceptionThrown(false)
	{
		prevExecution = tld->activeExecution;
		prevContext = tld->activeContext;
		FailureRecord = prevExecution != nullptr
			&& prevExecution->FailureRecord != nullptr
			? prevExecution->FailureRecord : &OwnedFailureRecord;
		if (prevExecution != nullptr)
		{
			NativeJITFrameDepth = prevExecution->NativeJITFrameDepth;
			NativeJITFrameLimit = prevExecution->NativeJITFrameLimit;
		}
		Activation.Execution = this;
		Activation.Previous = tld->activeActivation;
		tld->activeActivation = &Activation;
		tld->activeExecution = this;
		tld->activeContext = nullptr;
	}

	~FScriptExecution()
	{
		asASSERT(tld->activeActivation == &Activation);
		if (bExceptionThrown && prevExecution != nullptr)
		{
			// Nested native/JIT entry points share the first-failure record, but
			// generated callers branch on their own fast-path boolean. Propagate
			// the stop signal while leaving the shared diagnostic payload intact.
			prevExecution->bExceptionThrown = true;
		}
		tld->activeActivation = Activation.Previous;
		tld->activeExecution = prevExecution;
		tld->activeContext = prevContext;
	}
};

// TODO: Move these to as_utils.h
int     as_powi(int base, int exponent, bool& isOverflow);
asDWORD as_powu(asDWORD base, asDWORD exponent, bool& isOverflow);
asINT64 as_powi64(asINT64 base, asINT64 exponent, bool& isOverflow);
asQWORD as_powu64(asQWORD base, asQWORD exponent, bool& isOverflow);

// Optional template version of powi if overflow detection is not used.
#if 0
template <class T>
T as_powi(T base, T exponent)
{
	// Test for sign bit (huge number is OK)
	if( exponent & (T(1)<<(sizeof(T)*8-1)) )
		return 0;
	else
	{
		int result = 1;
		while( exponent )
		{
			if( exponent & 1 )
				result *= base;
			exponent >>= 1;
			base *= base;
		}
		return result;
	}
}
#endif

END_AS_NAMESPACE

#endif
