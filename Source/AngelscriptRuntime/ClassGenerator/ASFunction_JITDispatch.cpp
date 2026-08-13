#include "ClassGenerator/ASFunction_CallHelpers.h"

void UASFunction_JIT::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
	AngelscriptCallFromBPVM<true, true>(this, Object, Stack, RESULT_PARAM);
}

void UASFunction_JIT::RuntimeCallEvent(UObject* Object, void* Parms)
{
	AngelscriptCallFromParms<true, true>(this, Object, Parms);
}

void UASFunction_NotThreadSafe_JIT::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL)
{
	AngelscriptCallFromBPVM<false, true>(this, Object, Stack, RESULT_PARAM);
}

void UASFunction_NotThreadSafe_JIT::RuntimeCallEvent(UObject* Object, void* Parms)
{
	AngelscriptCallFromParms<false, true>(this, Object, Parms);
}

// Reloadable wrappers may keep their UFunction subclass, but never a
// content-specific entry pointer. Every call reads one current immutable AS
// Binding and falls back to the current VM when that Binding has been retired.
#define IMPLEMENT_CURRENT_BINDING_JIT_DISPATCH(FunctionClass) \
	void FunctionClass::RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) \
	{ \
		AngelscriptCallFromBPVM<false, true>(this, Object, Stack, RESULT_PARAM); \
	} \
	void FunctionClass::RuntimeCallEvent(UObject* Object, void* Parms) \
	{ \
		AngelscriptCallFromParms<false, true>(this, Object, Parms); \
	}

IMPLEMENT_CURRENT_BINDING_JIT_DISPATCH(UASFunction_NoParams_JIT)
IMPLEMENT_CURRENT_BINDING_JIT_DISPATCH(UASFunction_DWordArg_JIT)
IMPLEMENT_CURRENT_BINDING_JIT_DISPATCH(UASFunction_FloatArg_JIT)
IMPLEMENT_CURRENT_BINDING_JIT_DISPATCH(UASFunction_DoubleArg_JIT)
IMPLEMENT_CURRENT_BINDING_JIT_DISPATCH(UASFunction_FloatExtendedToDoubleArg_JIT)
IMPLEMENT_CURRENT_BINDING_JIT_DISPATCH(UASFunction_QWordArg_JIT)
IMPLEMENT_CURRENT_BINDING_JIT_DISPATCH(UASFunction_ByteArg_JIT)
IMPLEMENT_CURRENT_BINDING_JIT_DISPATCH(UASFunction_ReferenceArg_JIT)
IMPLEMENT_CURRENT_BINDING_JIT_DISPATCH(UASFunction_ObjectReturn_JIT)
IMPLEMENT_CURRENT_BINDING_JIT_DISPATCH(UASFunction_DWordReturn_JIT)
IMPLEMENT_CURRENT_BINDING_JIT_DISPATCH(UASFunction_FloatExtendedToDoubleReturn_JIT)
IMPLEMENT_CURRENT_BINDING_JIT_DISPATCH(UASFunction_FloatReturn_JIT)
IMPLEMENT_CURRENT_BINDING_JIT_DISPATCH(UASFunction_DoubleReturn_JIT)
IMPLEMENT_CURRENT_BINDING_JIT_DISPATCH(UASFunction_ByteReturn_JIT)

#undef IMPLEMENT_CURRENT_BINDING_JIT_DISPATCH
