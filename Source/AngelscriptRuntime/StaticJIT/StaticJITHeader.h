#pragma once

#include "Artifacts/AngelscriptArtifactIdentity.h"
#include "CoreMinimal.h"

#include <cmath>
#include <type_traits>

#include "Kismet/KismetMathLibrary.h"
#include "AngelscriptEngine.h"
#include "StaticJIT/StaticJITHelperFunctions.h"
#include "StaticJIT/StaticJITConfig.h"

#include "StartAngelscriptHeaders.h"
#include "AngelscriptInclude.h"
//#include "as_scriptfunction.h"
//#include "as_callfunc.h"
//#include "as_typeinfo.h"
//#include "as_context.h"
//#include "as_scriptengine.h"
//#include "as_scriptobject.h"
#include "source/as_scriptfunction.h"
#include "source/as_callfunc.h"
#include "source/as_typeinfo.h"
#include "source/as_context.h"
#include "source/as_property.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptobject.h"
#include "EndAngelscriptHeaders.h"

#if defined(_MSC_VER)
#define SCRIPT_ASSUME_NOT_NULL(V) __assume(V != 0);
#define SCRIPT_ASSUME(V) __assume(V);
#elif defined(__GNUC__) || defined(__clang__)
#define SCRIPT_ASSUME_NOT_NULL(V) __builtin_assume(V != 0);
#define SCRIPT_ASSUME(V) __builtin_assume(V);
#else
#define SCRIPT_ASSUME_NOT_NULL(V) 
#define SCRIPT_ASSUME(V) 
#endif

#include "Binds/Bind_TArray.h"

#ifndef AS_FORCE_LINK
#if defined(__GNUC__) || defined(__clang__)
#define AS_FORCE_LINK [[gnu::used, gnu::retain]]
#else
#define AS_FORCE_LINK
#endif
#endif

#define AS_JIT_DEBUG_CALLSTACKS (!UE_BUILD_SHIPPING)

#ifdef _MSC_VER
// Disable some warnings we dgaf about in static jit
#pragma warning(disable : 4101)
#pragma warning(disable : 4102)
#pragma warning(disable : 4191)
#pragma warning(disable : 4996)
#pragma warning(disable : 4883)
#pragma warning(disable : 4702)
#endif

#ifdef __clang__
#pragma clang diagnostic ignored "-Wself-assign"
#endif

enum class EJITException : uint8
{
	NullPointer,
	Div0,
	Overflow,
	UnboundFunction,
	OutOfBounds,
};

/**
 * Holds the current Engine-local callee for one reloadable generated call.
 * The stable route is resolved before virtual dispatch and the resulting
 * function is retained until the nested call has finished.
 */
class ANGELSCRIPTRUNTIME_API FStaticJITCurrentFunctionCall
{
public:
	FStaticJITCurrentFunctionCall(
		FScriptExecution& Execution,
		const struct FAngelscriptStableFunctionKey& FunctionKey,
		asCScriptFunction* ReferencedFunction,
		void* CallObject,
		bool bResolveVirtualFunction);
	~FStaticJITCurrentFunctionCall();

	FStaticJITCurrentFunctionCall(
		const FStaticJITCurrentFunctionCall&) = delete;
	FStaticJITCurrentFunctionCall& operator=(
		const FStaticJITCurrentFunctionCall&) = delete;

	asCScriptFunction* Get() const
	{
		return Function;
	}

private:
	asCScriptFunction* Function = nullptr;
};

struct FAngelscriptJITResolvedSystemFunctionPointer
{
	asFUNCTION_t Function = nullptr;
	asMETHOD_t Method = nullptr;

	FORCEINLINE asFUNCTION_t GetFunction() const { return Function; }
	FORCEINLINE asMETHOD_t GetMethod() const { return Method; }
};

/**
 * Keeps a pooled nested context from consuming the public exception callback.
 *
 * StaticJIT call bridges execute a temporary context and then adopt its first
 * failure into the owning FScriptExecution.  The temporary context must still
 * retain its exception payload, but the public caller owns the one-shot
 * callback.  The exact pooled callback state is restored before the context is
 * returned, including when Execute exits through an exception.
 */
class ANGELSCRIPTRUNTIME_API FScopeStaticJITNestedExceptionAdoption
{
public:
	explicit FScopeStaticJITNestedExceptionAdoption(
		asIScriptContext& InContext);
	~FScopeStaticJITNestedExceptionAdoption();

	FScopeStaticJITNestedExceptionAdoption(
		const FScopeStaticJITNestedExceptionAdoption&) = delete;
	FScopeStaticJITNestedExceptionAdoption& operator=(
		const FScopeStaticJITNestedExceptionAdoption&) = delete;

private:
	asCContext& Context;
	bool bPreviousExceptionCallback = false;
	asSSystemFunctionInterface PreviousExceptionCallbackFunction;
	void* PreviousExceptionCallbackObject = nullptr;
};

/** Stable-slot access used only by relocatable Provider-generated code. */
struct ANGELSCRIPTRUNTIME_API FAngelscriptJITGeneratedReferenceAccess
{
	// Returns the current Engine's function object for either a script-function
	// slot or an application-registered EnvironmentSymbol function slot.
	static asCScriptFunction* GetScriptFunction(
		const FScriptExecution& Execution,
		uint32 SlotIndex);
	static FAngelscriptJITResolvedSystemFunctionPointer
	GetSystemFunctionPointer(
		const FScriptExecution& Execution,
		uint32 SlotIndex);
	static asCTypeInfo* GetTypeInfo(
		const FScriptExecution& Execution,
		uint32 SlotIndex);
	static asCObjectType* GetObjectType(
		const FScriptExecution& Execution,
		uint32 SlotIndex);
	static void* GetGlobalStorage(
		const FScriptExecution& Execution,
		uint32 SlotIndex);
	static uint32 GetPropertyOffset(
		const FScriptExecution& Execution,
		uint32 SlotIndex);
};

struct ANGELSCRIPTRUNTIME_API FStaticJITFunction
{
	FORCENOINLINE static void ScriptFinishConstruct(FScriptExecution& Execution, asIScriptObject* Object, asITypeInfo* TypeInfo);
	FORCENOINLINE static void ScriptCallNative(FScriptExecution& Execution, asCScriptFunction* Function, asBYTE* l_sp, asQWORD* valueRegister, void** objectRegister);
	FORCENOINLINE static void AdoptContextException(FScriptExecution& Execution, asIScriptContext& Context);
	FORCENOINLINE static void SetExternalException(FScriptExecution& Execution, const ANSICHAR* Message);
	FORCENOINLINE static void SetException(FScriptExecution& Execution, EJITException Exception);
	FORCENOINLINE static void SetNullPointerException(FScriptExecution& Execution);
	FORCENOINLINE static void SetDivByZeroException(FScriptExecution& Execution);
	FORCENOINLINE static void SetOverflowException(FScriptExecution& Execution);
	FORCENOINLINE static void SetPowerOverflowException(FScriptExecution& Execution);
	FORCENOINLINE static void SetUnboundFunctionException(FScriptExecution& Execution);
	FORCENOINLINE static void SetOutOfBoundsException(FScriptExecution& Execution);
	FORCENOINLINE static void SetSwitchValueInvalidException(FScriptExecution& Execution);
	FORCENOINLINE static void SetUnknownException(FScriptExecution& Execution);
};

struct FUnknownValueType
{
	union
	{
		float FloatValue;
		asDWORD DWordValue;
		asBYTE ByteValue;
		void* PtrValue;
	};
};

struct FScopeInformSystemFunction
{
	asIScriptFunction* PrevActiveFunction;
	asCThreadLocalData* TLD;

	FORCEINLINE FScopeInformSystemFunction(FScriptExecution& Execution, asIScriptFunction* ScriptFunc)
	{
		TLD = Execution.tld;
		PrevActiveFunction = TLD->activeFunction;
		TLD->activeFunction = ScriptFunc;
	}

	FORCEINLINE ~FScopeInformSystemFunction()
	{
		TLD->activeFunction = PrevActiveFunction;
	}
};

#if AS_JIT_DEBUG_CALLSTACKS
struct FScopeJITDebugCallstack
{
	const char* Filename;
	const char* FunctionName;
	void* ThisObject;
	int32 LineNumber;
	int32 ColumnNumber;
	FScopeJITDebugCallstack* PrevFrame;
	FScriptExecution& Execution;
	bool bLinked = false;

	FORCEINLINE FScopeJITDebugCallstack(FScriptExecution& InExecution, const char* InFilename, const char* InFunctionName, int32 InLineNumber, void* InThisObject, bool bLinkFrame = true)
		: Execution(InExecution)
	{
		Filename = InFilename;
		FunctionName = InFunctionName;
		LineNumber = InLineNumber;
		ColumnNumber = 0;
		PrevFrame = (FScopeJITDebugCallstack*)Execution.debugCallStack;
		ThisObject = InThisObject;
		bLinked = bLinkFrame;
		if (bLinked)
		{
			Execution.debugCallStack = this;
		}
	}

	FORCEINLINE ~FScopeJITDebugCallstack()
	{
		if (bLinked)
		{
			Execution.debugCallStack = PrevFrame;
		}
	}
};
#endif

/**
 * One maintained execution frame for a TypedASTJIT root or direct helper.
 *
 * The scope shares FScriptExecution's call-scoped native depth budget across
 * direct helpers and nested JIT executions, publishes the current script
 * function through the existing TLD contract, and (when available) links the
 * existing non-Shipping debug frame. The destructor restores every piece of
 * outer state on all C++ exits.
 */
struct ANGELSCRIPTRUNTIME_API FScopeTypedASTJITExecutionFrame
{
	FScopeTypedASTJITExecutionFrame(
		FScriptExecution& InExecution,
		asIScriptFunction* InFunction,
		const ANSICHAR* InFilename,
		const ANSICHAR* InFunctionName,
		int32 InLineNumber,
		void* InThisObject,
		bool bPublishSourcePosition = true);
	~FScopeTypedASTJITExecutionFrame();

	FScopeTypedASTJITExecutionFrame(
		const FScopeTypedASTJITExecutionFrame&) = delete;
	FScopeTypedASTJITExecutionFrame& operator=(
		const FScopeTypedASTJITExecutionFrame&) = delete;

	bool IsActive() const
	{
		return bActive;
	}

	/** Position-only metadata update; never invokes line/coverage callbacks. */
	void SetLineNumber(const int32 InLineNumber)
	{
		SetSourcePosition(InLineNumber, 0);
	}

	/** Position-only metadata update; never invokes line/coverage callbacks. */
	void SetSourcePosition(
		const int32 InLineNumber,
		const int32 InColumnNumber)
	{
#if AS_JIT_DEBUG_CALLSTACKS
		DebugFrame.LineNumber = InLineNumber;
		DebugFrame.ColumnNumber = InColumnNumber;
#else
		static_cast<void>(InLineNumber);
		static_cast<void>(InColumnNumber);
#endif
	}

private:
	FScriptExecution& Execution;
	asIScriptFunction* Function = nullptr;
	asIScriptFunction* PreviousActiveFunction = nullptr;
	bool bActive = false;
#if AS_JIT_DEBUG_CALLSTACKS
	FScopeJITDebugCallstack DebugFrame;
#endif
};

template<typename T>
struct TAutoDereference
{
	T* Ptr;

	TAutoDereference(T* InPtr)
		: Ptr(InPtr)
	{}

	TAutoDereference(void* InPtr)
		: Ptr((T*)InPtr)
	{}

	TAutoDereference(const T& InPtr)
		: Ptr((T*)&InPtr)
	{}

	TAutoDereference(T& InPtr)
		: Ptr(&InPtr)
	{}

	FORCEINLINE operator T&()
	{
		return *Ptr;
	}

	FORCEINLINE operator T*()
	{
		return Ptr;
	}

	FORCEINLINE operator void*()
	{
		return Ptr;
	}
};

template<typename OutT, typename InT>
FORCEINLINE OutT value_as(InT InValue)
{
	if constexpr (std::is_same_v<OutT, InT>)
	{
		return InValue;
	}
	else if constexpr (sizeof(InT) < sizeof(OutT))
	{
		OutT ExtendedValue = {};
		memcpy(&ExtendedValue, &InValue, sizeof(InT));
		return ExtendedValue;
	}
	else
	{
		OutT OutValue;
		memcpy(&OutValue, &InValue, sizeof(OutT));
		return OutValue;
	}
}

template<typename OutT>
FORCEINLINE OutT value_read(void* InValue)
{
	OutT OutValue;
	memcpy(&OutValue, InValue, sizeof(OutT));
	return OutValue;
}

template<typename OutT, typename InT>
FORCEINLINE void value_assign_safe(void* OutValue, InT InValue)
{
	if constexpr (std::is_same_v<OutT, InT>)
	{
		*(OutT*)OutValue = InValue;
	}
	else if constexpr (sizeof(InT) < sizeof(OutT))
	{
		OutT ExtendedValue = {};
		memcpy(&ExtendedValue, &InValue, sizeof(InT));
		memcpy(OutValue, &ExtendedValue, sizeof(OutT));
	}
	else
	{
		memcpy(OutValue, &InValue, sizeof(OutT));
	}
}

// This function is FORCENOINLINE to work around an MSVC
// compiler bug where the optimizer would confuse signed and unsigned integers,
// so a integer -1 could get converted to a double of value UINT_MAX.
template<typename OutT, typename InT>
FORCENOINLINE OutT ConvertPrimitiveValue(InT InValue)
{
	return (OutT)(InValue);
}

// Proxy struct for script structs that are fully POD, allows better code generation
template<int Size, int Alignment> 
struct alignas(Alignment) TScriptPODStruct
{
	uint8 Data[Size];
};

template<int Alignment>
struct alignas(Alignment) TScriptPODEmptyStruct
{
};

constexpr SIZE_T AlignmentMax(SIZE_T A, SIZE_T B)
{
	return A >= B ? A : B;
}

#define SCRIPT_CALLSTACK_FRAME_SIZE 10

#define SCRIPT_ENGINE (FAngelscriptEngine::Get().Engine)
#define SCRIPT_TLD (Execution.tld)

#if AS_JIT_DEBUG_CALLSTACKS
#define SCRIPT_DEBUG_CALLSTACK_FRAME(FunctionName, LineNumber) FScopeJITDebugCallstack DebugCallstack(Execution, SCRIPT_DEBUG_FILENAME, FunctionName, LineNumber, nullptr);
#define SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT(FunctionName, LineNumber) FScopeJITDebugCallstack DebugCallstack(Execution, SCRIPT_DEBUG_FILENAME, FunctionName, LineNumber, l_This);
#define SCRIPT_DEBUG_CALLSTACK_LINE(InLineNumber) DebugCallstack.LineNumber = InLineNumber;
#define SCRIPT_DEBUG_CALLSTACK_POSITION(InLineNumber, InColumnNumber) \
	do { \
		DebugCallstack.LineNumber = InLineNumber; \
		DebugCallstack.ColumnNumber = InColumnNumber; \
	} while (false)
#else
#define SCRIPT_DEBUG_CALLSTACK_FRAME(FunctionName, LineNumber) 
#define SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT(FunctionName, LineNumber) 
#define SCRIPT_DEBUG_CALLSTACK_LINE(LineNumber) 
#define SCRIPT_DEBUG_CALLSTACK_POSITION(LineNumber, ColumnNumber)
#endif

#define SCRIPT_NULL_POINTER_EXCEPTION() \
	FStaticJITFunction::SetNullPointerException(Execution);

#define SCRIPT_CALL_NATIVE(Function, StackPosition) \
	FStaticJITFunction::ScriptCallNative(Execution, Function, &l_stack[StackPosition], &l_valueRegister, &l_objectRegister);

#define SCRIPT_FINISH_CONSTRUCT(Object, TypeInfo) \
	FStaticJITFunction::ScriptFinishConstruct(Execution, Object, TypeInfo);

#define SCRIPT_DIV0_EXCEPTION() \
	FStaticJITFunction::SetDivByZeroException(Execution);

#define SCRIPT_OVERFLOW_EXCEPTION() \
	FStaticJITFunction::SetOverflowException(Execution);

#define SCRIPT_POWER_OVERFLOW_EXCEPTION() \
	FStaticJITFunction::SetPowerOverflowException(Execution);

#define SCRIPT_UNBOUND_EXCEPTION() \
	FStaticJITFunction::SetUnboundFunctionException(Execution);

#define SCRIPT_OUT_OF_BOUNDS_EXCEPTION() \
	FStaticJITFunction::SetOutOFBoundsException(Execution);

#define SCRIPT_SWITCH_VALUE_INVALID_EXCEPTION() \
	FStaticJITFunction::SetSwitchValueInvalidException(Execution);

#define SCRIPT_UNKNOWN_EXCEPTION() \
	FStaticJITFunction::SetUnknownException(Execution);

#define SCRIPT_ASSUME_NO_EXCEPTION() SCRIPT_ASSUME(Execution.bExceptionThrown == false)

// Isolated child/VM cleanup bridge: never enter a generated destructor
// prolog with an already-failed FScriptExecution. Cleanup exceptions are
// discarded and must not replace the outer primary failure record.
ANGELSCRIPTRUNTIME_API void AngelscriptDestroyScriptObjectIsolated(
	FScriptExecution& OuterExecution,
	asIScriptEngine* Engine,
	asITypeInfo* ObjectType,
	void* Object);
