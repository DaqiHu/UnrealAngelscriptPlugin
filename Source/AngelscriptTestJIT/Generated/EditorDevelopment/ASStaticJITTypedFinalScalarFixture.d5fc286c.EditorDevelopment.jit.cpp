// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Module
 * VirtualSourcePath   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as
 * CanonicalModuleName : ASStaticJITTypedFinalScalarFixture
 * TargetProfile       : EditorDevelopment
 * StableModuleKey     : d5fc286cb1b96446cf9ab866b8485d617ca75fc6ed7bfa9cf90f88338e6e615a
 * ProviderId          : 7dc70fd55a76cc55d726e32d5c2434713bd93755a32e4dd524a4b7c39c8b5d44
 * ArtifactProfile     : b58efc4c9f40879f0b98bcb54704d1b342ae56948a00e9ddbaa709787226e0c8
 * NativeEnvironment   : 76d620ec7d0db38aa2028996e595f75523f201e7e560c05d8c9000d60d04cd73
 * FunctionCount       : 7
 */

#if WITH_EDITOR && UE_BUILD_DEVELOPMENT
#include "StaticJIT/StaticJITConfig.h"
#ifndef AS_SKIP_JITTED_CODE
#include "CoreGlobals.h"
#include "CoreMinimal.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallables.h"
#include "StaticJIT/StaticJITHeader.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallBridge.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITScalarOps.h"
constexpr SIZE_T POFFSET_UStaticJITAotFunctionCarrier_StoredValue = Align(sizeof(UObject) + 0, 4);
constexpr SIZE_T PALIGN_UStaticJITAotFunctionCarrier_StoredValue = AlignmentMax(alignof(UObject), 4);
constexpr SIZE_T TALIGN_UStaticJITAotFunctionCarrier = PALIGN_UStaticJITAotFunctionCarrier_StoredValue;
constexpr SIZE_T TSIZE_UStaticJITAotFunctionCarrier = Align(POFFSET_UStaticJITAotFunctionCarrier_StoredValue + 4, TALIGN_UStaticJITAotFunctionCarrier);
/*
 * AngelScript Static JIT Function
 * Declaration       : float SemanticPowerBoundary(const float, const float)
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:73:2
 * StableFunctionKey  : 23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67
 * ExecutionHash      : 817a41df8f6f47eff93c571e9dcb61b107ba9c4cdf34531109218faacc9080e3
 * DebugHash          : b09e16ad5f9a3216822a51e060ae2a349bff1f29c5fca30db7ced65dd95ffeb8
 * EntryAbiHash       : 5bdf2107db2403a739f0c1ec988a215ef8d4496714c3a85248de4057037ca025
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static double ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_817a41df8f6f47eff93c571e9dcb61b107ba9c4cdf34531109218faacc9080e3_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const double as_sem_s1,
	const double as_sem_s2);

static double ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_817a41df8f6f47eff93c571e9dcb61b107ba9c4cdf34531109218faacc9080e3_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const double as_sem_s1,
	const double as_sem_s2)
{
	TypedFrame.SetSourcePosition(77, 3);
	return ([&]() -> double
	{
		const double as_sem_e2_left = as_sem_s1;
		const double as_sem_e2_right = as_sem_s2;
		return AngelscriptTypedASTJIT::CheckedPower<double>(Execution, as_sem_e2_left, as_sem_e2_right);
	}());
}

// AS Function : float SemanticPowerBoundary(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:73:2
// JIT Entry   : Raw
double ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_817a41df8f6f47eff93c571e9dcb61b107ba9c4cdf34531109218faacc9080e3_Raw(FScriptExecution& Execution, void* Object, double p_0, double p_1)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as",
		"float SemanticPowerBoundary(const float, const float)",
		73,
		Object,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_817a41df8f6f47eff93c571e9dcb61b107ba9c4cdf34531109218faacc9080e3_TypedBody(Execution, TypedFrame, static_cast<double>(p_0), static_cast<double>(p_1));
}

// AS Function : float SemanticPowerBoundary(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:73:2
// JIT Entry   : VM
void ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_817a41df8f6f47eff93c571e9dcb61b107ba9c4cdf34531109218faacc9080e3_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(double*)l_outValue = ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_817a41df8f6f47eff93c571e9dcb61b107ba9c4cdf34531109218faacc9080e3_Raw(Execution,
		*(void**)l_fp,
		*(double*)(l_fp + 2),
		*(double*)(l_fp + 4));
}

// AS Function : float SemanticPowerBoundary(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:73:2
// JIT Entry   : Parms
void ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_817a41df8f6f47eff93c571e9dcb61b107ba9c4cdf34531109218faacc9080e3_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(double));
	const SIZE_T ParmOffset_1 = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(double));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(double*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_817a41df8f6f47eff93c571e9dcb61b107ba9c4cdf34531109218faacc9080e3_Raw(Execution,
		Object,
		static_cast<double>(*(double*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<double>(*(double*)(((SIZE_T)Parms) + ParmOffset_1)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UTypedASTFinalScalarReceiver UTypedASTFinalScalarReceiver()
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:0
 * StableFunctionKey  : b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f
 * ExecutionHash      : 8d59a3666cb1e9468ba047e44d12e822bc32d189c66e2841f3e0d28c530e714e
 * DebugHash          : 61e622bd99778e23d4a7ed2ae92ae79a5925d441a62be32d381cd879e9f774fe
 * EntryAbiHash       : 5d9f493ad48c6524ca33a73c311249bdf7a4315a86649957cedb5fedfe5acda5
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f_8d59a3666cb1e9468ba047e44d12e822bc32d189c66e2841f3e0d28c530e714e = "ASStaticJITTypedFinalScalarFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f_8d59a3666cb1e9468ba047e44d12e822bc32d189c66e2841f3e0d28c530e714e
#endif

// AS Function : UTypedASTFinalScalarReceiver UTypedASTFinalScalarReceiver()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:0
// JIT Entry   : Raw
UObject* ASJIT_b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f_8d59a3666cb1e9468ba047e44d12e822bc32d189c66e2841f3e0d28c530e714e(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("UTypedASTFinalScalarReceiver UTypedASTFinalScalarReceiver()", 0);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[16];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
UObject* v_TEMP_ptr_2 = nullptr;
// JitEntry *
// PSF v2
// ALLOC
{
  asCObjectType* objType = (asCObjectType*)FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo(Execution, 0u);
  asDWORD* mem = (asDWORD*)SCRIPT_ENGINE->AllocScriptObject(objType);
  ScriptObject_Construct(objType, (asCScriptObject*)mem);
  asPWORD* a = (asPWORD*)((&v_TEMP_ptr_2));
  if(a != nullptr) *a = (asPWORD)mem;
value_assign_safe<asQWORD>(&l_stack[0], mem);
// UTypedASTFinalScalarReceiver::UTypedASTFinalScalarReceiver()
SCRIPT_DEBUG_CALLSTACK_POSITION(0, 0);
{
  asCScriptFunction* CallFunction = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 1u);
  FStaticJITCurrentFunctionCall CurrentFunctionCall(Execution, FAngelscriptStableFunctionKey{{FAngelscriptHash256{{FBlake3Hash(FWideStringView(TEXT("d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609")))}}}}, CallFunction, (void*)(((asQWORD&)l_stack[0])), true);
  CallFunction = CurrentFunctionCall.Get();
  if (CallFunction == nullptr) [[unlikely]]
  {
     SCRIPT_UNBOUND_EXCEPTION();
return {};
  }
  FAngelscriptContext CallContext(CallFunction->GetEngine());
  CallContext->Prepare(CallFunction);
  CallContext->SetObject((void*)((asQWORD&)l_stack[0]));
  {
     FScopeStaticJITNestedExceptionAdoption ExceptionAdoption(*CallContext);
     CallContext->Execute();
  }
  if (Execution.bExceptionThrown || CallContext->m_status != asEXECUTION_FINISHED)
  {
     FStaticJITFunction::AdoptContextException(Execution, *CallContext);
     Execution.bExceptionThrown = true;
return {};
  }
}
}
// JitEntry *
// LOADOBJ v2
l_objectRegister = (void*)v_TEMP_ptr_2;
v_TEMP_ptr_2 = nullptr;
// RET 0
  return (UObject*)l_objectRegister;
}
// AS Function : UTypedASTFinalScalarReceiver UTypedASTFinalScalarReceiver()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:0
// JIT Entry   : VM
void ASJIT_b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f_8d59a3666cb1e9468ba047e44d12e822bc32d189c66e2841f3e0d28c530e714e_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(UObject**)l_outValue = ASJIT_b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f_8d59a3666cb1e9468ba047e44d12e822bc32d189c66e2841f3e0d28c530e714e(Execution);
}
// AS Function : UTypedASTFinalScalarReceiver UTypedASTFinalScalarReceiver()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:0
// JIT Entry   : Parms
void ASJIT_b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f_8d59a3666cb1e9468ba047e44d12e822bc32d189c66e2841f3e0d28c530e714e_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(UObject**)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f_8d59a3666cb1e9468ba047e44d12e822bc32d189c66e2841f3e0d28c530e714e(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int DifferentialScalarValue(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:14:2
 * StableFunctionKey  : bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b
 * ExecutionHash      : 24397a70f048c17cf8cc8051d54e6394dc379c349573bc515f4253b4d6d503ae
 * DebugHash          : 7e95ff287f42acc18ce60419452b035698306752b09373a328b1d598db200f98
 * EntryAbiHash       : dabb76ccb243078a57121d4c3a40ed732a70a68cca7584d9e9320a1fd5a679c4
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_24397a70f048c17cf8cc8051d54e6394dc379c349573bc515f4253b4d6d503ae_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s1,
	const int32 as_sem_s2);

static int32 ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_24397a70f048c17cf8cc8051d54e6394dc379c349573bc515f4253b4d6d503ae_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s1,
	const int32 as_sem_s2)
{
	TypedFrame.SetSourcePosition(16, 3);
	return ([&]() -> int32
	{
		const int32 as_sem_e6_left = ([&]() -> int32
	{
		const int32 as_sem_e4_left = ([&]() -> int32
	{
		const int32 as_sem_e2_left = as_sem_s1;
		const int32 as_sem_e2_right = int32(3);
		return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_sem_e2_left, as_sem_e2_right);
	}());
		const int32 as_sem_e4_right = as_sem_s2;
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e4_left, as_sem_e4_right);
	}());
		const int32 as_sem_e6_right = int32(7);
		return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_sem_e6_left, as_sem_e6_right);
	}());
}

// AS Function : int DifferentialScalarValue(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:14:2
// JIT Entry   : Raw
int32 ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_24397a70f048c17cf8cc8051d54e6394dc379c349573bc515f4253b4d6d503ae_Raw(FScriptExecution& Execution, void* Object, asDWORD p_0, asDWORD p_1)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as",
		"int DifferentialScalarValue(const int, const int)",
		14,
		Object,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_24397a70f048c17cf8cc8051d54e6394dc379c349573bc515f4253b4d6d503ae_TypedBody(TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1));
}

// AS Function : int DifferentialScalarValue(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:14:2
// JIT Entry   : VM
void ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_24397a70f048c17cf8cc8051d54e6394dc379c349573bc515f4253b4d6d503ae_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_24397a70f048c17cf8cc8051d54e6394dc379c349573bc515f4253b4d6d503ae_Raw(Execution,
		*(void**)l_fp,
		*(asDWORD*)(l_fp + 2),
		*(asDWORD*)(l_fp + 3));
}

// AS Function : int DifferentialScalarValue(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:14:2
// JIT Entry   : Parms
void ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_24397a70f048c17cf8cc8051d54e6394dc379c349573bc515f4253b4d6d503ae_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_24397a70f048c17cf8cc8051d54e6394dc379c349573bc515f4253b4d6d503ae_Raw(Execution,
		Object,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticImportedBinding(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:20:2
 * StableFunctionKey  : cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746
 * ExecutionHash      : 6dceb6b02db4f3cc8af890b8fb3b98bc8045876aa95de5cf744cbbc064c6d0db
 * DebugHash          : 4e812d41acefcdde1a5d04df3f8a0ddbbcb2dee67d6a67d001583b4ee7660916
 * EntryAbiHash       : a4b711dd5134aeaca25a93796be749414190495c109eeba3fb1015c75b1a90b1
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_HelperBody_ImportedValueForAOT_69a0ada34d03f3f5_cc438c3e7ddfcc30(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0);
static int32 ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_6dceb6b02db4f3cc8af890b8fb3b98bc8045876aa95de5cf744cbbc064c6d0db_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s1);
static int32 ASJIT_Helper_ImportedValueForAOT_69a0ada34d03f3f5_cc438c3e7ddfcc30(
	FScriptExecution& Execution,
	const int32 as_helper_p0);

static int32 ASJIT_HelperBody_ImportedValueForAOT_69a0ada34d03f3f5_cc438c3e7ddfcc30(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0)
{
	TypedFrame.SetSourcePosition(3, 2);
	return ([&]() -> int32
	{
		const int32 as_sem_e2_left = as_sem_s0;
		const int32 as_sem_e2_right = int32(77);
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e2_left, as_sem_e2_right);
	}());
}

static int32 ASJIT_Helper_ImportedValueForAOT_69a0ada34d03f3f5_cc438c3e7ddfcc30(
	FScriptExecution& Execution,
	const int32 as_helper_p0)
{
	asIScriptFunction* const TypedFunction =
		FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(
			Execution, 1u);
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		TypedFunction,
		"/Angelscript/Game/ASStaticJITAotImportProvider.as",
		"int ImportedValueForAOT(const int)",
		1,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_HelperBody_ImportedValueForAOT_69a0ada34d03f3f5_cc438c3e7ddfcc30(
		TypedFrame, as_helper_p0);
}

static int32 ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_6dceb6b02db4f3cc8af890b8fb3b98bc8045876aa95de5cf744cbbc064c6d0db_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s1)
{
	TypedFrame.SetSourcePosition(22, 3);
	return ([&]() -> int32
	{
		TypedFrame.SetSourcePosition(22, 10);
		const int32 as_sem_e1_arg0 = as_sem_s1;
		const int32 as_sem_e1_result = ASJIT_Helper_ImportedValueForAOT_69a0ada34d03f3f5_cc438c3e7ddfcc30(Execution, as_sem_e1_arg0);
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return as_sem_e1_result;
	}());
}

// AS Function : int SemanticImportedBinding(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:20:2
// JIT Entry   : Raw
int32 ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_6dceb6b02db4f3cc8af890b8fb3b98bc8045876aa95de5cf744cbbc064c6d0db_Raw(FScriptExecution& Execution, void* Object, asDWORD p_0)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as",
		"int SemanticImportedBinding(const int)",
		20,
		Object,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_6dceb6b02db4f3cc8af890b8fb3b98bc8045876aa95de5cf744cbbc064c6d0db_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0));
}

// AS Function : int SemanticImportedBinding(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:20:2
// JIT Entry   : VM
void ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_6dceb6b02db4f3cc8af890b8fb3b98bc8045876aa95de5cf744cbbc064c6d0db_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_6dceb6b02db4f3cc8af890b8fb3b98bc8045876aa95de5cf744cbbc064c6d0db_Raw(Execution,
		*(void**)l_fp,
		*(asDWORD*)(l_fp + 2));
}

// AS Function : int SemanticImportedBinding(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:20:2
// JIT Entry   : Parms
void ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_6dceb6b02db4f3cc8af890b8fb3b98bc8045876aa95de5cf744cbbc064c6d0db_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_6dceb6b02db4f3cc8af890b8fb3b98bc8045876aa95de5cf744cbbc064c6d0db_Raw(Execution,
		Object,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UTypedASTFinalScalarReceiver()
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:0
 * StableFunctionKey  : d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609
 * ExecutionHash      : a8d405185ad471e29551d89d8cdd792471125c8887e1ba52e64543ab2066a855
 * DebugHash          : 61e622bd99778e23d4a7ed2ae92ae79a5925d441a62be32d381cd879e9f774fe
 * EntryAbiHash       : e7fe3e49a0111396d17b5901011b806a76bd6f4b076fedbb28cc2e4f38d7d5b5
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609_a8d405185ad471e29551d89d8cdd792471125c8887e1ba52e64543ab2066a855 = "ASStaticJITTypedFinalScalarFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609_a8d405185ad471e29551d89d8cdd792471125c8887e1ba52e64543ab2066a855
#endif

// AS Function : UTypedASTFinalScalarReceiver()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:0
// JIT Entry   : Raw
void ASJIT_d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609_a8d405185ad471e29551d89d8cdd792471125c8887e1ba52e64543ab2066a855(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("UTypedASTFinalScalarReceiver::UTypedASTFinalScalarReceiver()", 0);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[8];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
// JitEntry *
// PshVPtr v0
// FinConstruct *
{
  asIScriptObject* Object = (asIScriptObject*)(l_This);
  asITypeInfo* TypeInfo = (asITypeInfo*)FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo(Execution, 0u);
  SCRIPT_FINISH_CONSTRUCT(Object, TypeInfo);
}
// RET 2
  return;
}
// AS Function : UTypedASTFinalScalarReceiver()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:0
// JIT Entry   : VM
void ASJIT_d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609_a8d405185ad471e29551d89d8cdd792471125c8887e1ba52e64543ab2066a855_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609_a8d405185ad471e29551d89d8cdd792471125c8887e1ba52e64543ab2066a855(Execution,
		*(UObject**)l_fp);
}
// AS Function : UTypedASTFinalScalarReceiver()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:0
// JIT Entry   : Parms
void ASJIT_d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609_a8d405185ad471e29551d89d8cdd792471125c8887e1ba52e64543ab2066a855_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609_a8d405185ad471e29551d89d8cdd792471125c8887e1ba52e64543ab2066a855(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedFinalRawValue()
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:8:2
 * StableFunctionKey  : d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0
 * ExecutionHash      : d71e9179f1fc35d0d447f8ed3b9a98c01f3b4fa9e93e54bbc29019ff8af780d2
 * DebugHash          : aa4b5c7a3a4434a646e73c23ef537d880a045338ad3e3dce76f776a94161b3bc
 * EntryAbiHash       : 855bd98169313400b8f1d6686e5875bbad13825c3cc12fdde4e9ad4afcdbc31f
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_d71e9179f1fc35d0d447f8ed3b9a98c01f3b4fa9e93e54bbc29019ff8af780d2_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame);

static int32 ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_d71e9179f1fc35d0d447f8ed3b9a98c01f3b4fa9e93e54bbc29019ff8af780d2_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame)
{
	TypedFrame.SetSourcePosition(10, 3);
	return int32(84);
}

// AS Function : int TypedFinalRawValue()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:8:2
// JIT Entry   : Raw
int32 ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_d71e9179f1fc35d0d447f8ed3b9a98c01f3b4fa9e93e54bbc29019ff8af780d2_Raw(FScriptExecution& Execution, void* Object)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as",
		"int TypedFinalRawValue()",
		8,
		Object,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_d71e9179f1fc35d0d447f8ed3b9a98c01f3b4fa9e93e54bbc29019ff8af780d2_TypedBody(TypedFrame);
}

// AS Function : int TypedFinalRawValue()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:8:2
// JIT Entry   : VM
void ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_d71e9179f1fc35d0d447f8ed3b9a98c01f3b4fa9e93e54bbc29019ff8af780d2_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_d71e9179f1fc35d0d447f8ed3b9a98c01f3b4fa9e93e54bbc29019ff8af780d2_Raw(Execution,
		*(void**)l_fp);
}

// AS Function : int TypedFinalRawValue()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:8:2
// JIT Entry   : Parms
void ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_d71e9179f1fc35d0d447f8ed3b9a98c01f3b4fa9e93e54bbc29019ff8af780d2_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_d71e9179f1fc35d0d447f8ed3b9a98c01f3b4fa9e93e54bbc29019ff8af780d2_Raw(Execution,
		Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticNumericBoundary(const int, const int, const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:2
 * StableFunctionKey  : de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e
 * ExecutionHash      : 58539743e28e50faa5d5ee3e50261836e051b1dd0a7d68b805bab6072fd2d80a
 * DebugHash          : 33ab9e7722bfe134d36a9e683a832e75cb1a6a5322a13ab18e7bd23d86aea50f
 * EntryAbiHash       : ce69b03c30f4aba54525c218c4551a91f6dfe1f289fefa74e6e6ea01b1bab739
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_58539743e28e50faa5d5ee3e50261836e051b1dd0a7d68b805bab6072fd2d80a_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s1,
	const int32 as_sem_s2,
	const int32 as_sem_s3,
	const int32 as_sem_s4);

static int32 ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_58539743e28e50faa5d5ee3e50261836e051b1dd0a7d68b805bab6072fd2d80a_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s1,
	const int32 as_sem_s2,
	const int32 as_sem_s3,
	const int32 as_sem_s4)
{
	TypedFrame.SetSourcePosition(32, 3);
	const int32 as_sem_switch_selector_43 = static_cast<int32>(as_sem_s4);
	if (UNLIKELY(Execution.bExceptionThrown))
	{
		return int32{};
	}
	switch (as_sem_switch_selector_43)
	{
		case int32(0):
			TypedFrame.SetSourcePosition(34, 3);
			{
				TypedFrame.SetSourcePosition(35, 4);
				return ([&]() -> int32
				{
					const int32 as_sem_e14_left = as_sem_s1;
					const int32 as_sem_e14_right = as_sem_s2;
					return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e14_left, as_sem_e14_right);
				}());
			}
			break;
		case int32(1):
			TypedFrame.SetSourcePosition(36, 3);
			{
				TypedFrame.SetSourcePosition(37, 4);
				return ([&]() -> int32
				{
					const int32 as_sem_e17_left = as_sem_s1;
					const int32 as_sem_e17_right = as_sem_s2;
					return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_sem_e17_left, as_sem_e17_right);
				}());
			}
			break;
		case int32(2):
			TypedFrame.SetSourcePosition(38, 3);
			{
				TypedFrame.SetSourcePosition(39, 4);
				return ([&]() -> int32
				{
					const int32 as_sem_e20_left = as_sem_s1;
					const int32 as_sem_e20_right = as_sem_s2;
					return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_sem_e20_left, as_sem_e20_right);
				}());
			}
			break;
		case int32(3):
			TypedFrame.SetSourcePosition(40, 3);
			{
				TypedFrame.SetSourcePosition(41, 4);
				return ([&]() -> int32
				{
					const int32 as_sem_e23_left = as_sem_s1;
					const int32 as_sem_e23_right = as_sem_s2;
					return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_sem_e23_left, as_sem_e23_right);
				}());
			}
			break;
		case int32(4):
			TypedFrame.SetSourcePosition(42, 3);
			{
				TypedFrame.SetSourcePosition(43, 4);
				return ([&]() -> int32
				{
					const int32 as_sem_e26_left = as_sem_s1;
					const int32 as_sem_e26_right = as_sem_s2;
					return AngelscriptTypedASTJIT::CheckedRemainder<int32>(Execution, as_sem_e26_left, as_sem_e26_right);
				}());
			}
			break;
		case int32(5):
			TypedFrame.SetSourcePosition(44, 3);
			{
				TypedFrame.SetSourcePosition(45, 4);
				return ([&]() -> int32
				{
					const int32 as_sem_e30_left = as_sem_s1;
					const uint32 as_sem_e30_right = AngelscriptTypedASTJIT::ConvertInteger<uint32>(as_sem_s3);
					return AngelscriptTypedASTJIT::ShiftLeft<int32>(as_sem_e30_left, as_sem_e30_right);
				}());
			}
			break;
		case int32(6):
			TypedFrame.SetSourcePosition(46, 3);
			{
				TypedFrame.SetSourcePosition(47, 4);
				return ([&]() -> int32
				{
					const int32 as_sem_e34_left = as_sem_s1;
					const uint32 as_sem_e34_right = AngelscriptTypedASTJIT::ConvertInteger<uint32>(as_sem_s3);
					return AngelscriptTypedASTJIT::LogicalShiftRight<int32>(as_sem_e34_left, as_sem_e34_right);
				}());
			}
			break;
		case int32(7):
			TypedFrame.SetSourcePosition(48, 3);
			{
				TypedFrame.SetSourcePosition(49, 4);
				return ([&]() -> int32
				{
					const int32 as_sem_e38_left = as_sem_s1;
					const uint32 as_sem_e38_right = AngelscriptTypedASTJIT::ConvertInteger<uint32>(as_sem_s3);
					return AngelscriptTypedASTJIT::ArithmeticShiftRight<int32>(as_sem_e38_left, as_sem_e38_right);
				}());
			}
			break;
		case int32(8):
			TypedFrame.SetSourcePosition(50, 3);
			{
				TypedFrame.SetSourcePosition(52, 10);
				const int64 as_sem_s5 = static_cast<int64>(as_sem_s1);
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				TypedFrame.SetSourcePosition(53, 4);
				const bool as_sem_if_condition_27 = ([&]() -> bool
				{
					const int64 as_sem_e44_left = as_sem_s5;
					const int64 as_sem_e44_right = static_cast<int64>(as_sem_s1);
					return (as_sem_e44_left == as_sem_e44_right);
				}());
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				if (as_sem_if_condition_27)
				{
					TypedFrame.SetSourcePosition(55, 5);
					return as_sem_s1;
				}
				TypedFrame.SetSourcePosition(57, 4);
				return int32(0);
			}
			break;
		case int32(9):
			TypedFrame.SetSourcePosition(59, 3);
			{
				TypedFrame.SetSourcePosition(60, 4);
				const bool as_sem_if_condition_33 = ([&]() -> bool
				{
					const int32 as_sem_e49_left = as_sem_s1;
					const int32 as_sem_e49_right = int32(0);
					return (as_sem_e49_left != as_sem_e49_right);
				}());
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				if (as_sem_if_condition_33)
				{
					TypedFrame.SetSourcePosition(62, 5);
					return int32(1);
				}
				TypedFrame.SetSourcePosition(64, 4);
				return int32(0);
			}
			break;
		case int32(10):
			TypedFrame.SetSourcePosition(65, 3);
			{
				TypedFrame.SetSourcePosition(66, 4);
				return ([&]() -> int32
				{
					const int32 as_sem_e54_left = AngelscriptTypedASTJIT::FromCanonicalBits<int32>(UINT64_C(7001));
					const int32 as_sem_e54_right = as_sem_s1;
					return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e54_left, as_sem_e54_right);
				}());
			}
			break;
		default:
			TypedFrame.SetSourcePosition(67, 3);
			{
				TypedFrame.SetSourcePosition(68, 4);
				return AngelscriptTypedASTJIT::WrapNegate<int32>(int32(1));
			}
			break;
	}
}

// AS Function : int SemanticNumericBoundary(const int, const int, const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:2
// JIT Entry   : Raw
int32 ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_58539743e28e50faa5d5ee3e50261836e051b1dd0a7d68b805bab6072fd2d80a_Raw(FScriptExecution& Execution, void* Object, asDWORD p_0, asDWORD p_1, asDWORD p_2, asDWORD p_3)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as",
		"int SemanticNumericBoundary(const int, const int, const int, const int)",
		26,
		Object,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_58539743e28e50faa5d5ee3e50261836e051b1dd0a7d68b805bab6072fd2d80a_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1), static_cast<int32>(p_2), static_cast<int32>(p_3));
}

// AS Function : int SemanticNumericBoundary(const int, const int, const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:2
// JIT Entry   : VM
void ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_58539743e28e50faa5d5ee3e50261836e051b1dd0a7d68b805bab6072fd2d80a_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_58539743e28e50faa5d5ee3e50261836e051b1dd0a7d68b805bab6072fd2d80a_Raw(Execution,
		*(void**)l_fp,
		*(asDWORD*)(l_fp + 2),
		*(asDWORD*)(l_fp + 3),
		*(asDWORD*)(l_fp + 4),
		*(asDWORD*)(l_fp + 5));
}

// AS Function : int SemanticNumericBoundary(const int, const int, const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:2
// JIT Entry   : Parms
void ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_58539743e28e50faa5d5ee3e50261836e051b1dd0a7d68b805bab6072fd2d80a_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_2 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_3 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_58539743e28e50faa5d5ee3e50261836e051b1dd0a7d68b805bab6072fd2d80a_Raw(Execution,
		Object,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_2)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_3)));
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
