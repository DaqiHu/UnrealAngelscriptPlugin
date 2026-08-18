// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Module
 * VirtualSourcePath   : /Angelscript/Game/ASStaticJITAotFixture.as
 * CanonicalModuleName : ASStaticJITAotFixture
 * TargetProfile       : EditorDevelopment
 * StableModuleKey     : c8f2291154afd48cf9d8c267f8bfcf5bff307741bbaa61ea978863ed08f780ed
 * ProviderId          : 7dc70fd55a76cc55d726e32d5c2434713bd93755a32e4dd524a4b7c39c8b5d44
 * ArtifactProfile     : b58efc4c9f40879f0b98bcb54704d1b342ae56948a00e9ddbaa709787226e0c8
 * NativeEnvironment   : 76d620ec7d0db38aa2028996e595f75523f201e7e560c05d8c9000d60d04cd73
 * FunctionCount       : 54
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
 * Declaration       : uint64 DoubleToUint64ForAOT(const float)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:93:1
 * StableFunctionKey  : 04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29
 * ExecutionHash      : 731ebcfc6e1ae2e5f16413e8e02b51ce0c34139e6530879c3d632720567416d6
 * DebugHash          : 63ef133fed1ae03934d94125ef46263148488799545c7011a1864f4901c3130c
 * EntryAbiHash       : 5e708ac1a7afdc6a24646cbb3780e38d2f6639308f316ff6b2ba697920d65899
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_731ebcfc6e1ae2e5f16413e8e02b51ce0c34139e6530879c3d632720567416d6 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_731ebcfc6e1ae2e5f16413e8e02b51ce0c34139e6530879c3d632720567416d6
#endif

// AS Function : uint64 DoubleToUint64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:93:1
// JIT Entry   : Raw
uint64 ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_731ebcfc6e1ae2e5f16413e8e02b51ce0c34139e6530879c3d632720567416d6(FScriptExecution& Execution, double p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("uint64 DoubleToUint64ForAOT(float)", 95);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asQWORD v_TEMP_qword_2 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// dTOu64 v2, v0
v_TEMP_qword_2 = uint64(p_Value);
// CpyVtoR8 v2
l_valueRegister = v_TEMP_qword_2;
// RET 2
  return (uint64)l_valueRegister;
}
// AS Function : uint64 DoubleToUint64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:93:1
// JIT Entry   : VM
void ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_731ebcfc6e1ae2e5f16413e8e02b51ce0c34139e6530879c3d632720567416d6_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(uint64*)l_outValue = ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_731ebcfc6e1ae2e5f16413e8e02b51ce0c34139e6530879c3d632720567416d6(Execution,
		*(double*)(l_fp + 0));
}
// AS Function : uint64 DoubleToUint64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:93:1
// JIT Entry   : Parms
void ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_731ebcfc6e1ae2e5f16413e8e02b51ce0c34139e6530879c3d632720567416d6_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(uint64));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(uint64*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_731ebcfc6e1ae2e5f16413e8e02b51ce0c34139e6530879c3d632720567416d6(Execution,
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UStaticJITAotVirtualChild UStaticJITAotVirtualChild()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:309:0
 * StableFunctionKey  : 0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe
 * ExecutionHash      : 5ff14395e534561f56d7569924ccefed5ca7db316e28b96e960c783d4963dbbe
 * DebugHash          : e99c1b7c1598b7ed5632a3ec913c738a97522a396a8ed5610511320c8b6818b1
 * EntryAbiHash       : 1cdfabcf3e8a6c5b336a09064baa4231093ea1b1252679df79c5a50f263c311d
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe_5ff14395e534561f56d7569924ccefed5ca7db316e28b96e960c783d4963dbbe = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe_5ff14395e534561f56d7569924ccefed5ca7db316e28b96e960c783d4963dbbe
#endif

// AS Function : UStaticJITAotVirtualChild UStaticJITAotVirtualChild()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:309:0
// JIT Entry   : Raw
UObject* ASJIT_0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe_5ff14395e534561f56d7569924ccefed5ca7db316e28b96e960c783d4963dbbe(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("UStaticJITAotVirtualChild UStaticJITAotVirtualChild()", 0);
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
// UStaticJITAotVirtualChild::UStaticJITAotVirtualChild()
SCRIPT_DEBUG_CALLSTACK_POSITION(0, 0);
{
  asCScriptFunction* CallFunction = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 1u);
  FStaticJITCurrentFunctionCall CurrentFunctionCall(Execution, FAngelscriptStableFunctionKey{{FAngelscriptHash256{{FBlake3Hash(FWideStringView(TEXT("521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7")))}}}}, CallFunction, (void*)(((asQWORD&)l_stack[0])), true);
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
// AS Function : UStaticJITAotVirtualChild UStaticJITAotVirtualChild()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:309:0
// JIT Entry   : VM
void ASJIT_0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe_5ff14395e534561f56d7569924ccefed5ca7db316e28b96e960c783d4963dbbe_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(UObject**)l_outValue = ASJIT_0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe_5ff14395e534561f56d7569924ccefed5ca7db316e28b96e960c783d4963dbbe(Execution);
}
// AS Function : UStaticJITAotVirtualChild UStaticJITAotVirtualChild()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:309:0
// JIT Entry   : Parms
void ASJIT_0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe_5ff14395e534561f56d7569924ccefed5ca7db316e28b96e960c783d4963dbbe_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(UObject**)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe_5ff14395e534561f56d7569924ccefed5ca7db316e28b96e960c783d4963dbbe(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : float32 SemanticPowerFloat(const float32, const float32)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:28:1
 * StableFunctionKey  : 0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a
 * ExecutionHash      : f42c1d8d7a8a4ef05d516a4519365e6f4393b1ec90eac28fc9ec71a275298a35
 * DebugHash          : 0cb215108d15f1e335ade65578b983f7b1fe7a89a90ee16d505875ad8bd5997b
 * EntryAbiHash       : baec52322166f9d9a578e658e1d6db3e7012f171953f1f761c1b35fa1d15862f
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a_f42c1d8d7a8a4ef05d516a4519365e6f4393b1ec90eac28fc9ec71a275298a35 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a_f42c1d8d7a8a4ef05d516a4519365e6f4393b1ec90eac28fc9ec71a275298a35
#endif

// AS Function : float32 SemanticPowerFloat(const float32, const float32)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:28:1
// JIT Entry   : Raw
float ASJIT_0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a_f42c1d8d7a8a4ef05d516a4519365e6f4393b1ec90eac28fc9ec71a275298a35(FScriptExecution& Execution, float p_Base, float p_Exponent)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("float32 SemanticPowerFloat(float32, float32)", 30);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
float v_TEMP_float_1 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// POWf v1, v0, v-1
v_TEMP_float_1 = FMath::Pow(p_Base, p_Exponent);
if (v_TEMP_float_1 == static_cast<float>(HUGE_VAL)) [[unlikely]]
{
SCRIPT_DEBUG_CALLSTACK_POSITION(30, 2);
  SCRIPT_POWER_OVERFLOW_EXCEPTION();
return {};
}
// CpyVtoR4 v1
l_floatRegister = v_TEMP_float_1;
// RET 2
  return l_floatRegister;
}
// AS Function : float32 SemanticPowerFloat(const float32, const float32)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:28:1
// JIT Entry   : VM
void ASJIT_0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a_f42c1d8d7a8a4ef05d516a4519365e6f4393b1ec90eac28fc9ec71a275298a35_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(float*)l_outValue = ASJIT_0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a_f42c1d8d7a8a4ef05d516a4519365e6f4393b1ec90eac28fc9ec71a275298a35(Execution,
		*(float*)(l_fp + 0),
		*(float*)(l_fp + 1));
}
// AS Function : float32 SemanticPowerFloat(const float32, const float32)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:28:1
// JIT Entry   : Parms
void ASJIT_0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a_f42c1d8d7a8a4ef05d516a4519365e6f4393b1ec90eac28fc9ec71a275298a35_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Base = ParmsOffset;
	ParmsOffset += sizeof(float);
	ParmsOffset = Align(ParmsOffset, alignof(float));
	const SIZE_T ParmOffset_1_Exponent = ParmsOffset;
	ParmsOffset += sizeof(float);
	ParmsOffset = Align(ParmsOffset, alignof(float));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(float*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a_f42c1d8d7a8a4ef05d516a4519365e6f4393b1ec90eac28fc9ec71a275298a35(Execution,
		(float)*(float*)(((SIZE_T)Parms) + ParmOffset_0_Base),
		(float)*(float*)(((SIZE_T)Parms) + ParmOffset_1_Exponent));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int RecursionEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:227:2
 * StableFunctionKey  : 13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c
 * ExecutionHash      : 48d363b5454e3ef4da6a26fbaf9dc9113da26d724fc20ec8a3d016b950b2d9d6
 * DebugHash          : 19adf894fd7441d9e6c9881e90d8280e86173e58aa12b4455b3ddb8e67123860
 * EntryAbiHash       : 9807be95f7b037b5a524626bf011302bd0844098a4a72bbe407d8cd5e21fa5ae
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_48d363b5454e3ef4da6a26fbaf9dc9113da26d724fc20ec8a3d016b950b2d9d6 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_48d363b5454e3ef4da6a26fbaf9dc9113da26d724fc20ec8a3d016b950b2d9d6
#endif

// AS Function : int RecursionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:227:2
// JIT Entry   : Raw
int32 ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_48d363b5454e3ef4da6a26fbaf9dc9113da26d724fc20ec8a3d016b950b2d9d6(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::RecursionEntryForAOT()", 229);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[12];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
asDWORD v_TEMP_dword_2 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// PshC4 4
// PshVPtr v0
// CALLINTF
// int UStaticJITAotFunctionCarrier::RecurseForAOT(const int Value)
SCRIPT_DEBUG_CALLSTACK_POSITION(229, 3);
{
  asCScriptFunction* CallFunction = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 1u);
  FStaticJITCurrentFunctionCall CurrentFunctionCall(Execution, FAngelscriptStableFunctionKey{{FAngelscriptHash256{{FBlake3Hash(FWideStringView(TEXT("947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7")))}}}}, CallFunction, (void*)((l_This)), true);
  CallFunction = CurrentFunctionCall.Get();
  if (CallFunction == nullptr) [[unlikely]]
  {
     SCRIPT_UNBOUND_EXCEPTION();
return {};
  }
  FAngelscriptContext CallContext(CallFunction->GetEngine());
  CallContext->Prepare(CallFunction);
  CallContext->SetObject((void*)(l_This));
  CallContext->SetArgDWord(0, value_as<asDWORD>(((asDWORD)0x4u)));
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
  l_valueRegister = CallContext->m_regs.valueRegister;
}
// JitEntry *
// CpyRtoV4 v2
memcpy((&v_TEMP_dword_2), &l_valueRegister, 4);
// CpyVtoR4 v2
l_dwordRegister = v_TEMP_dword_2;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int RecursionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:227:2
// JIT Entry   : VM
void ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_48d363b5454e3ef4da6a26fbaf9dc9113da26d724fc20ec8a3d016b950b2d9d6_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_48d363b5454e3ef4da6a26fbaf9dc9113da26d724fc20ec8a3d016b950b2d9d6(Execution,
		*(UObject**)l_fp);
}
// AS Function : int RecursionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:227:2
// JIT Entry   : Parms
void ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_48d363b5454e3ef4da6a26fbaf9dc9113da26d724fc20ec8a3d016b950b2d9d6_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_48d363b5454e3ef4da6a26fbaf9dc9113da26d724fc20ec8a3d016b950b2d9d6(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int GetVirtualValue_Implementation()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:294:3
 * StableFunctionKey  : 191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5
 * ExecutionHash      : 4bd3c1036a3f94095c4d862d4ad7636a344a6cb39d21e9879d8235c00b1f8772
 * DebugHash          : a7c54f633942df98e0b7d62174bbbd402058e5833eb794051535e4bd2177567d
 * EntryAbiHash       : 507cc902edc1d9bd3e2f2572fe86bb7d02b96a550445798d3f067222eb350188
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_4bd3c1036a3f94095c4d862d4ad7636a344a6cb39d21e9879d8235c00b1f8772 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_4bd3c1036a3f94095c4d862d4ad7636a344a6cb39d21e9879d8235c00b1f8772
#endif

// AS Function : int GetVirtualValue_Implementation()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:294:3
// JIT Entry   : Raw
int32 ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_4bd3c1036a3f94095c4d862d4ad7636a344a6cb39d21e9879d8235c00b1f8772(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotVirtualParent::GetVirtualValue_Implementation()", 296);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v1, 21
v_TEMP_dword_1 = 0x15u;
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int GetVirtualValue_Implementation()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:294:3
// JIT Entry   : VM
void ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_4bd3c1036a3f94095c4d862d4ad7636a344a6cb39d21e9879d8235c00b1f8772_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_4bd3c1036a3f94095c4d862d4ad7636a344a6cb39d21e9879d8235c00b1f8772(Execution,
		*(UObject**)l_fp);
}
// AS Function : int GetVirtualValue_Implementation()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:294:3
// JIT Entry   : Parms
void ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_4bd3c1036a3f94095c4d862d4ad7636a344a6cb39d21e9879d8235c00b1f8772_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_4bd3c1036a3f94095c4d862d4ad7636a344a6cb39d21e9879d8235c00b1f8772(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ObjectLastNativeForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:180:2
 * StableFunctionKey  : 1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f
 * ExecutionHash      : f23fb7fb95649c31568b374d896d6d0cfbd204e8d2b30b4fa4e7b9e779061880
 * DebugHash          : 7ec0516948804868f689e6bcd58bcc1160a55a913ca43863c25a15a8e8f92722
 * EntryAbiHash       : 37241b3ba70d4b0f0af53358d5dcd4fda67f64c6db1715fe217a46674e3ba526
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_f23fb7fb95649c31568b374d896d6d0cfbd204e8d2b30b4fa4e7b9e779061880 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_f23fb7fb95649c31568b374d896d6d0cfbd204e8d2b30b4fa4e7b9e779061880
#endif

// AS Function : int ObjectLastNativeForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:180:2
// JIT Entry   : Raw
int32 ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_f23fb7fb95649c31568b374d896d6d0cfbd204e8d2b30b4fa4e7b9e779061880(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::ObjectLastNativeForAOT()", 182);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[16];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
alignas(8) asBYTE MEM_v_Value[4];
FUnknownValueType& v_Value = (FUnknownValueType&)MEM_v_Value[0];
asDWORD v_TEMP_dword_3 = {};
asDWORD v_TEMP_dword_4 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// PshC4 97
// PshC4 39
// PSF v2
// CALLSYS *
// FAotObjectLastProbe::FAotObjectLastProbe(int Left, int Right)
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 1u).GetFunction();
  auto CastedFuncPtr = (void(*)(int32,int32,void*))RawFuncPtr;
  void* Object = (void*)((&v_Value));
SCRIPT_DEBUG_CALLSTACK_POSITION(182, 3);
CastedFuncPtr(value_as<int32>(((asDWORD)0x27u)),value_as<int32>(((asDWORD)0x61u)),Object);
if (Execution.bExceptionThrown) [[unlikely]]
{
return {};
}
}
// JitEntry *
// SUSPEND
// JitEntry *
// LoadVObjR
l_valueRegister = (asQWORD)((&v_Value)) + (short)0;
// RDR4 v3
v_TEMP_dword_3 = value_read<asDWORD>((void*)l_valueRegister);
// CpyVtoR4 v3
l_dwordRegister = v_TEMP_dword_3;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int ObjectLastNativeForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:180:2
// JIT Entry   : VM
void ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_f23fb7fb95649c31568b374d896d6d0cfbd204e8d2b30b4fa4e7b9e779061880_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_f23fb7fb95649c31568b374d896d6d0cfbd204e8d2b30b4fa4e7b9e779061880(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ObjectLastNativeForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:180:2
// JIT Entry   : Parms
void ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_f23fb7fb95649c31568b374d896d6d0cfbd204e8d2b30b4fa4e7b9e779061880_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_f23fb7fb95649c31568b374d896d6d0cfbd204e8d2b30b4fa4e7b9e779061880(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int AddForAOT(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:159:2
 * StableFunctionKey  : 2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5
 * ExecutionHash      : 87e8e30475f9cbded1fa2410dfa61a9fa9ca8726cd71c4173e2bef0ab315039e
 * DebugHash          : 75500131f3999402087e2169ec0b1a66581c92b2efdbf19d51a1487df27ee773
 * EntryAbiHash       : 73a436254e2d895a203c53bb56b8bd06ea4085e6086c3b045fabf536dcbc0131
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_87e8e30475f9cbded1fa2410dfa61a9fa9ca8726cd71c4173e2bef0ab315039e = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_87e8e30475f9cbded1fa2410dfa61a9fa9ca8726cd71c4173e2bef0ab315039e
#endif

// AS Function : int AddForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:159:2
// JIT Entry   : Raw
int32 ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_87e8e30475f9cbded1fa2410dfa61a9fa9ca8726cd71c4173e2bef0ab315039e(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::AddForAOT(int)", 161);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// ADDIi v1, v-2, 7
((int32&)v_TEMP_dword_1) = ((int32&)p_Value) + value_as<int>((asDWORD)0x7u);
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 3
  return (int32)l_dwordRegister;
}
// AS Function : int AddForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:159:2
// JIT Entry   : VM
void ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_87e8e30475f9cbded1fa2410dfa61a9fa9ca8726cd71c4173e2bef0ab315039e_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_87e8e30475f9cbded1fa2410dfa61a9fa9ca8726cd71c4173e2bef0ab315039e(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : int AddForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:159:2
// JIT Entry   : Parms
void ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_87e8e30475f9cbded1fa2410dfa61a9fa9ca8726cd71c4173e2bef0ab315039e_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_87e8e30475f9cbded1fa2410dfa61a9fa9ca8726cd71c4173e2bef0ab315039e(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ReferenceEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:121:1
 * StableFunctionKey  : 2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9
 * ExecutionHash      : e73ef907c33b0fc6c37c610a5e074617f248d291b078002f51991fe36eec5674
 * DebugHash          : 74aeb2893c5c297beac24d9a38bd15d0ee4fe1ab2fdabdb34dd90b1f21a834b2
 * EntryAbiHash       : eb88e947baa4860face1585fa0d6b66fc0a33ee288c7ad728b2271e86a77d1e6
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_e73ef907c33b0fc6c37c610a5e074617f248d291b078002f51991fe36eec5674 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_e73ef907c33b0fc6c37c610a5e074617f248d291b078002f51991fe36eec5674
#endif

// AS Function : int ReferenceEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:121:1
// JIT Entry   : Raw
int32 ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_e73ef907c33b0fc6c37c610a5e074617f248d291b078002f51991fe36eec5674(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int ReferenceEntryForAOT()", 123);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[8];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_Value = {};
asDWORD v_Returned = {};
asDWORD v_TEMP_dword_3 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v1, 7
v_Value = 0x7u;
// SUSPEND
// JitEntry *
// PSF v1
// CALL
// int BumpReferenceForAOT(int&inout Value)
SCRIPT_DEBUG_CALLSTACK_POSITION(124, 2);
{
  asCScriptFunction* CallFunction = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 0u);
  FStaticJITCurrentFunctionCall CurrentFunctionCall(Execution, FAngelscriptStableFunctionKey{{FAngelscriptHash256{{FBlake3Hash(FWideStringView(TEXT("9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05")))}}}}, CallFunction, nullptr, false);
  CallFunction = CurrentFunctionCall.Get();
  if (CallFunction == nullptr) [[unlikely]]
  {
     SCRIPT_UNBOUND_EXCEPTION();
return {};
  }
  FAngelscriptContext CallContext(CallFunction->GetEngine());
  CallContext->Prepare(CallFunction);
  CallContext->SetArgAddress(0, (void*)((&v_Value)));
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
  l_valueRegister = CallContext->m_regs.valueRegister;
}
// JitEntry *
// CpyRtoV4 v2
memcpy((&v_Returned), &l_valueRegister, 4);
// SUSPEND
// JitEntry *
// MULIi v3, v1, 100
((int32&)v_TEMP_dword_3) = ((int32&)v_Value) * value_as<int>((asDWORD)0x64u);
// ADDi v3, v3, v2
((int32&)v_TEMP_dword_3) = ((int32&)v_TEMP_dword_3) + ((int32&)v_Returned);
// CpyVtoR4 v3
l_dwordRegister = v_TEMP_dword_3;
// RET 0
  return (int32)l_dwordRegister;
}
// AS Function : int ReferenceEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:121:1
// JIT Entry   : VM
void ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_e73ef907c33b0fc6c37c610a5e074617f248d291b078002f51991fe36eec5674_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_e73ef907c33b0fc6c37c610a5e074617f248d291b078002f51991fe36eec5674(Execution);
}
// AS Function : int ReferenceEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:121:1
// JIT Entry   : Parms
void ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_e73ef907c33b0fc6c37c610a5e074617f248d291b078002f51991fe36eec5674_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_e73ef907c33b0fc6c37c610a5e074617f248d291b078002f51991fe36eec5674(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int AddTwo(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:272:2
 * StableFunctionKey  : 30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c
 * ExecutionHash      : 974769158959206c4fbb982fd641349ba8f108266ac90ad1787b06d55e150876
 * DebugHash          : 83a08fc0d01041f52a5391f522f4671a45a7eb9bedd7cf0e3dbc79386cf32d43
 * EntryAbiHash       : c5fdf7c9de85d7a785dfc3e36e262ff979f7b922c419614f25625ceaf01dc6bd
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_974769158959206c4fbb982fd641349ba8f108266ac90ad1787b06d55e150876 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_974769158959206c4fbb982fd641349ba8f108266ac90ad1787b06d55e150876
#endif

// AS Function : int AddTwo(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:272:2
// JIT Entry   : Raw
int32 ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_974769158959206c4fbb982fd641349ba8f108266ac90ad1787b06d55e150876(FScriptExecution& Execution, UObject* l_This, asDWORD p_A, asDWORD p_B)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::AddTwo(int, int)", 274);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// ADDi v1, v-2, v-3
((int32&)v_TEMP_dword_1) = ((int32&)p_A) + ((int32&)p_B);
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 4
  return (int32)l_dwordRegister;
}
// AS Function : int AddTwo(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:272:2
// JIT Entry   : VM
void ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_974769158959206c4fbb982fd641349ba8f108266ac90ad1787b06d55e150876_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_974769158959206c4fbb982fd641349ba8f108266ac90ad1787b06d55e150876(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2),
		*(asDWORD*)(l_fp + 3));
}
// AS Function : int AddTwo(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:272:2
// JIT Entry   : Parms
void ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_974769158959206c4fbb982fd641349ba8f108266ac90ad1787b06d55e150876_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_A = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1_B = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_974769158959206c4fbb982fd641349ba8f108266ac90ad1787b06d55e150876(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_A),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_1_B));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void FailForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:104:1
 * StableFunctionKey  : 34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f
 * ExecutionHash      : fabd5fa6aad88b7821a069f290857dc6bce85d88e48d44a9e9657b2eee35c1a0
 * DebugHash          : c76b6973808ada74cc1d61368d9f1b05dea2e103fabe51ddc492d3a8fea8749b
 * EntryAbiHash       : 5c88cfe1c954db11e6955527a1273af1e6e20436cb7527444616a92e521bcb4d
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_fabd5fa6aad88b7821a069f290857dc6bce85d88e48d44a9e9657b2eee35c1a0 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_fabd5fa6aad88b7821a069f290857dc6bce85d88e48d44a9e9657b2eee35c1a0
#endif

// AS Function : void FailForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:104:1
// JIT Entry   : Raw
void ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_fabd5fa6aad88b7821a069f290857dc6bce85d88e48d44a9e9657b2eee35c1a0(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("void FailForAOT()", 106);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[8];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// CALLSYS *
// void throw(const FString&inout Message)
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 0u).GetFunction();
  auto CastedFuncPtr = (void(*)(void*))RawFuncPtr;
SCRIPT_DEBUG_CALLSTACK_POSITION(106, 2);
CastedFuncPtr((void**)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 2u)));
if (Execution.bExceptionThrown) [[unlikely]]
{
return;
}
}
// JitEntry *
// SUSPEND
// JitEntry *
// RET 0
  return;
}
// AS Function : void FailForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:104:1
// JIT Entry   : VM
void ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_fabd5fa6aad88b7821a069f290857dc6bce85d88e48d44a9e9657b2eee35c1a0_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_fabd5fa6aad88b7821a069f290857dc6bce85d88e48d44a9e9657b2eee35c1a0(Execution);
}
// AS Function : void FailForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:104:1
// JIT Entry   : Parms
void ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_fabd5fa6aad88b7821a069f290857dc6bce85d88e48d44a9e9657b2eee35c1a0_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_fabd5fa6aad88b7821a069f290857dc6bce85d88e48d44a9e9657b2eee35c1a0(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int RecursionEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:144:1
 * StableFunctionKey  : 3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5
 * ExecutionHash      : e25691bf1c6694ad7ed2809095608dd750fb4f658eb9106dc9346f07810da1e1
 * DebugHash          : 831046fe2f23cf083aaeb17ca76a200e3af3ab8e19a0b309383c62db4a308dc2
 * EntryAbiHash       : 9807be95f7b037b5a524626bf011302bd0844098a4a72bbe407d8cd5e21fa5ae
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_e25691bf1c6694ad7ed2809095608dd750fb4f658eb9106dc9346f07810da1e1 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_e25691bf1c6694ad7ed2809095608dd750fb4f658eb9106dc9346f07810da1e1
#endif

// AS Function : int RecursionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:144:1
// JIT Entry   : Raw
int32 ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_e25691bf1c6694ad7ed2809095608dd750fb4f658eb9106dc9346f07810da1e1(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int RecursionEntryForAOT()", 146);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[4];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
asDWORD v_TEMP_dword_2 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// PshC4 4
// CALL
// int RecurseForAOT(const int Value)
SCRIPT_DEBUG_CALLSTACK_POSITION(146, 2);
{
  asCScriptFunction* CallFunction = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 0u);
  FStaticJITCurrentFunctionCall CurrentFunctionCall(Execution, FAngelscriptStableFunctionKey{{FAngelscriptHash256{{FBlake3Hash(FWideStringView(TEXT("f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556")))}}}}, CallFunction, nullptr, false);
  CallFunction = CurrentFunctionCall.Get();
  if (CallFunction == nullptr) [[unlikely]]
  {
     SCRIPT_UNBOUND_EXCEPTION();
return {};
  }
  FAngelscriptContext CallContext(CallFunction->GetEngine());
  CallContext->Prepare(CallFunction);
  CallContext->SetArgDWord(0, value_as<asDWORD>(((asDWORD)0x4u)));
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
  l_valueRegister = CallContext->m_regs.valueRegister;
}
// JitEntry *
// CpyRtoV4 v2
memcpy((&v_TEMP_dword_2), &l_valueRegister, 4);
// CpyVtoR4 v2
l_dwordRegister = v_TEMP_dword_2;
// RET 0
  return (int32)l_dwordRegister;
}
// AS Function : int RecursionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:144:1
// JIT Entry   : VM
void ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_e25691bf1c6694ad7ed2809095608dd750fb4f658eb9106dc9346f07810da1e1_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_e25691bf1c6694ad7ed2809095608dd750fb4f658eb9106dc9346f07810da1e1(Execution);
}
// AS Function : int RecursionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:144:1
// JIT Entry   : Parms
void ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_e25691bf1c6694ad7ed2809095608dd750fb4f658eb9106dc9346f07810da1e1_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_e25691bf1c6694ad7ed2809095608dd750fb4f658eb9106dc9346f07810da1e1(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ObjectLifetimeEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:211:2
 * StableFunctionKey  : 40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135
 * ExecutionHash      : 0b9e1f34b7f61af8d6807ddc1048c145b5001e3240c90d3c036b90465f9a167f
 * DebugHash          : 520cb5ce57bb03da73995cf65975a2380fafa275dff0bbb946e4c2828111e624
 * EntryAbiHash       : c8041da3cad4a3fdc7be0bd5795a7456826fc964a0eeef2d2d35507862c860f0
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_0b9e1f34b7f61af8d6807ddc1048c145b5001e3240c90d3c036b90465f9a167f = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_0b9e1f34b7f61af8d6807ddc1048c145b5001e3240c90d3c036b90465f9a167f
#endif

// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:211:2
// JIT Entry   : Raw
int32 ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_0b9e1f34b7f61af8d6807ddc1048c145b5001e3240c90d3c036b90465f9a167f(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::ObjectLifetimeEntryForAOT()", 213);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[24];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
alignas(8) asBYTE MEM_v_Original[sizeof(FString)];
FString& v_Original = (FString&)MEM_v_Original[0];
alignas(8) asBYTE MEM_v_Decorated[sizeof(FString)];
FString& v_Decorated = (FString&)MEM_v_Decorated[0];
alignas(8) asBYTE MEM_v_TEMP_8[sizeof(FString)];
FString& v_TEMP_8 = (FString&)MEM_v_TEMP_8[0];
asBYTE v_TEMP_byte_13 = {};
asDWORD v_TEMP_dword_14 = {};
asDWORD v_TEMP_dword_15 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PSF v4
// CALLSYS *
// FString::FString(const FString&inout Other)
{
  FString* Object = (FString*)((&v_Original));
new (Object) FString((*(FString*)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 7u))));
}
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PSF v12
// PSF v4
// CALLSYS *
// FString FString::opAdd(const FString&inout Other) const
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 2u).GetFunction();
  auto CastedFuncPtr = (FString(*)(void*,void*))RawFuncPtr;
  void* Object = (void*)((&v_Original));
SCRIPT_DEBUG_CALLSTACK_POSITION(214, 3);
new((void*)((&v_Decorated))) FString(CastedFuncPtr(Object,(void**)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 8u))));
if (Execution.bExceptionThrown) [[unlikely]]
{
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_EXCEPTION_0;
}
}
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PSF v4
// CALLSYS *
// bool FString::opEquals(const FString&inout Other) const
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 1u).GetFunction();
  auto CastedFuncPtr = (bool(*)(void*,void*))RawFuncPtr;
  void* Object = (void*)((&v_Original));
SCRIPT_DEBUG_CALLSTACK_POSITION(215, 3);
bool FunctionReturnValue = CastedFuncPtr(Object,(void**)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 7u)));
l_byteRegister = (asBYTE&)FunctionReturnValue;
if (Execution.bExceptionThrown) [[unlikely]]
{
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_EXCEPTION_1;
}
}
// JitEntry *
// JLowZ 4
if(l_byteRegister == 0) {
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_52;
}
// SetV4 v14, 100
v_TEMP_dword_14 = 0x64u;
// JMP 2
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_54;
LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_52:
// SetV4 v14, 0
v_TEMP_dword_14 = 0x0u;
LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_54:
// PSF v12
// CALLSYS *
// int FString::Len() const
{
  FString* Object = (FString*)((&v_Decorated));
int32 FunctionReturnValue = Object->Len();
l_dwordRegister = (asDWORD&)FunctionReturnValue;
}
// JitEntry *
// CpyRtoV4 v15
v_TEMP_dword_15 = l_dwordRegister;
// ADDi v14, v14, v15
((int32&)v_TEMP_dword_14) = ((int32&)v_TEMP_dword_14) + ((int32&)v_TEMP_dword_15);
// PSF v12
// CALLSYS *
// FString::~FString()
{
  FString* Object = (FString*)((&v_Decorated));
((FString*)Object)->~FString();
}
// JitEntry *
// PSF v4
// CALLSYS *
// FString::~FString()
{
  FString* Object = (FString*)((&v_Original));
((FString*)Object)->~FString();
}
// JitEntry *
// CpyVtoR4 v14
l_dwordRegister = v_TEMP_dword_14;
// RET 2
  return (int32)l_dwordRegister;
SCRIPT_ASSUME(false);

LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_EXCEPTION_0:
// Exception cleanup for variables 4
((FString*)((FString*)(&v_Original)))->~FString();
return {};
LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_EXCEPTION_1:
// Exception cleanup for variables 4, 12
((FString*)((FString*)(&v_Decorated)))->~FString();
((FString*)((FString*)(&v_Original)))->~FString();
return {};
}
// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:211:2
// JIT Entry   : VM
void ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_0b9e1f34b7f61af8d6807ddc1048c145b5001e3240c90d3c036b90465f9a167f_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_0b9e1f34b7f61af8d6807ddc1048c145b5001e3240c90d3c036b90465f9a167f(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:211:2
// JIT Entry   : Parms
void ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_0b9e1f34b7f61af8d6807ddc1048c145b5001e3240c90d3c036b90465f9a167f_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_0b9e1f34b7f61af8d6807ddc1048c145b5001e3240c90d3c036b90465f9a167f(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int64 DoubleToInt64ForAOT(const float)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:170:2
 * StableFunctionKey  : 486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52
 * ExecutionHash      : f1d8185b41d4aff3510fd1255a5b7478b0a0845e9a45b7e1c21dd42d0f341f8f
 * DebugHash          : 8ef2103331b138e2c90a58e738fe4ffd5ada6bec8cda9446a8aec182df7fe172
 * EntryAbiHash       : a1d1604b170c8afa6d8aebaea217a68d24d4f67ff1cce99952cd0a22877c25f4
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_f1d8185b41d4aff3510fd1255a5b7478b0a0845e9a45b7e1c21dd42d0f341f8f = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_f1d8185b41d4aff3510fd1255a5b7478b0a0845e9a45b7e1c21dd42d0f341f8f
#endif

// AS Function : int64 DoubleToInt64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:170:2
// JIT Entry   : Raw
int64 ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_f1d8185b41d4aff3510fd1255a5b7478b0a0845e9a45b7e1c21dd42d0f341f8f(FScriptExecution& Execution, UObject* l_This, double p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int64 UStaticJITAotFunctionCarrier::DoubleToInt64ForAOT(float)", 172);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asQWORD v_TEMP_qword_2 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// dTOi64 v2, v-2
((int64&)v_TEMP_qword_2) = int64(p_Value);
// CpyVtoR8 v2
l_valueRegister = v_TEMP_qword_2;
// RET 4
  return (int64)l_valueRegister;
}
// AS Function : int64 DoubleToInt64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:170:2
// JIT Entry   : VM
void ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_f1d8185b41d4aff3510fd1255a5b7478b0a0845e9a45b7e1c21dd42d0f341f8f_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int64*)l_outValue = ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_f1d8185b41d4aff3510fd1255a5b7478b0a0845e9a45b7e1c21dd42d0f341f8f(Execution,
		*(UObject**)l_fp,
		*(double*)(l_fp + 2));
}
// AS Function : int64 DoubleToInt64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:170:2
// JIT Entry   : Parms
void ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_f1d8185b41d4aff3510fd1255a5b7478b0a0845e9a45b7e1c21dd42d0f341f8f_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(int64));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int64*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_f1d8185b41d4aff3510fd1255a5b7478b0a0845e9a45b7e1c21dd42d0f341f8f(Execution,
		(UObject*)Object,
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int AddForAOT(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:3:1
 * StableFunctionKey  : 4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30
 * ExecutionHash      : 8cd694ba8a03a3e3e79bbc4bc33a2f8874cb43658794fff89ed0fc55799f1342
 * DebugHash          : a51151f33be0f065b6b70f9816f5a3fe62f5ce21c7c8c1637abec3c4139e781b
 * EntryAbiHash       : 73a436254e2d895a203c53bb56b8bd06ea4085e6086c3b045fabf536dcbc0131
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30_8cd694ba8a03a3e3e79bbc4bc33a2f8874cb43658794fff89ed0fc55799f1342 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30_8cd694ba8a03a3e3e79bbc4bc33a2f8874cb43658794fff89ed0fc55799f1342
#endif

// AS Function : int AddForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:3:1
// JIT Entry   : Raw
int32 ASJIT_4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30_8cd694ba8a03a3e3e79bbc4bc33a2f8874cb43658794fff89ed0fc55799f1342(FScriptExecution& Execution, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int AddForAOT(int)", 5);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// ADDIi v1, v0, 7
((int32&)v_TEMP_dword_1) = ((int32&)p_Value) + value_as<int>((asDWORD)0x7u);
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 1
  return (int32)l_dwordRegister;
}
// AS Function : int AddForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:3:1
// JIT Entry   : VM
void ASJIT_4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30_8cd694ba8a03a3e3e79bbc4bc33a2f8874cb43658794fff89ed0fc55799f1342_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30_8cd694ba8a03a3e3e79bbc4bc33a2f8874cb43658794fff89ed0fc55799f1342(Execution,
		*(asDWORD*)(l_fp + 0));
}
// AS Function : int AddForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:3:1
// JIT Entry   : Parms
void ASJIT_4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30_8cd694ba8a03a3e3e79bbc4bc33a2f8874cb43658794fff89ed0fc55799f1342_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30_8cd694ba8a03a3e3e79bbc4bc33a2f8874cb43658794fff89ed0fc55799f1342(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticMutationParity(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:18:1
 * StableFunctionKey  : 4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4
 * ExecutionHash      : 3c12d161e3d40eea5b8c990ac8c7eaa0388a55436b51618b8594df7108707d90
 * DebugHash          : eccba4f1e1e23806e14afc14cd7c20ec4659f637f48fa7ba5a5b8b65bac45193
 * EntryAbiHash       : 45c016e37ac90d9198599858f3806eb4651e70a0969f759160a4683df7eee13c
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4_3c12d161e3d40eea5b8c990ac8c7eaa0388a55436b51618b8594df7108707d90 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4_3c12d161e3d40eea5b8c990ac8c7eaa0388a55436b51618b8594df7108707d90
#endif

// AS Function : int SemanticMutationParity(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:18:1
// JIT Entry   : Raw
int32 ASJIT_4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4_3c12d161e3d40eea5b8c990ac8c7eaa0388a55436b51618b8594df7108707d90(FScriptExecution& Execution, asDWORD p_Input, asDWORD p_Source)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int SemanticMutationParity(int, int)", 20);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_Value = {};
asDWORD v_Prefix = {};
asDWORD v_Postfix = {};
asDWORD v_TEMP_dword_4 = {};
asDWORD v_TEMP_dword_5 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// CpyVtoV4 v1, v0
v_Value = p_Input;
// SUSPEND
// JitEntry *
// CpyVtoV4 v1, v-1
v_Value = p_Source;
// SUSPEND
// JitEntry *
// ADDIi v1, v1, 3
((int32&)v_Value) = ((int32&)v_Value) + value_as<int>((asDWORD)0x3u);
// SUSPEND
// JitEntry *
// LDV v1
l_valueRegister = (asQWORD)(&v_Value);
// INCi
(**(int32**)&l_valueRegister)++;
// RDR4 v2
v_Postfix = value_read<asDWORD>((void*)l_valueRegister);
// CpyVtoV4 v3, v2
v_Prefix = v_Postfix;
// SUSPEND
// JitEntry *
// LDV v1
l_valueRegister = (asQWORD)(&v_Value);
// RDR4 v2
v_Postfix = value_read<asDWORD>((void*)l_valueRegister);
// INCi
(**(int32**)&l_valueRegister)++;
// SUSPEND
// JitEntry *
// MULIi v4, v1, 1000
((int32&)v_TEMP_dword_4) = ((int32&)v_Value) * value_as<int>((asDWORD)0x3e8u);
// MULIi v5, v3, 100
((int32&)v_TEMP_dword_5) = ((int32&)v_Prefix) * value_as<int>((asDWORD)0x64u);
// ADDi v4, v4, v5
((int32&)v_TEMP_dword_4) = ((int32&)v_TEMP_dword_4) + ((int32&)v_TEMP_dword_5);
// ADDi v5, v4, v2
((int32&)v_TEMP_dword_5) = ((int32&)v_TEMP_dword_4) + ((int32&)v_Postfix);
// CpyVtoR4 v5
l_dwordRegister = v_TEMP_dword_5;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int SemanticMutationParity(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:18:1
// JIT Entry   : VM
void ASJIT_4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4_3c12d161e3d40eea5b8c990ac8c7eaa0388a55436b51618b8594df7108707d90_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4_3c12d161e3d40eea5b8c990ac8c7eaa0388a55436b51618b8594df7108707d90(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}
// AS Function : int SemanticMutationParity(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:18:1
// JIT Entry   : Parms
void ASJIT_4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4_3c12d161e3d40eea5b8c990ac8c7eaa0388a55436b51618b8594df7108707d90_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Input = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1_Source = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4_3c12d161e3d40eea5b8c990ac8c7eaa0388a55436b51618b8594df7108707d90(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Input),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_1_Source));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int BumpReferenceForAOT(int&inout)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:197:2
 * StableFunctionKey  : 5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65
 * ExecutionHash      : 0e62174e7a6ac532611bb5c523304c0baefade0a901b22ab785d62af07152acc
 * DebugHash          : 18c42e648e7ed22ea5d970260276f206d27554fb09a9afeec042fdacae09eeaf
 * EntryAbiHash       : b80efaaa778ead65430c5702610f23a7b6dccd414f3c044078aa45e30f15b055
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_0e62174e7a6ac532611bb5c523304c0baefade0a901b22ab785d62af07152acc = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_0e62174e7a6ac532611bb5c523304c0baefade0a901b22ab785d62af07152acc
#endif

// AS Function : int BumpReferenceForAOT(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:197:2
// JIT Entry   : Raw
int32 ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_0e62174e7a6ac532611bb5c523304c0baefade0a901b22ab785d62af07152acc(FScriptExecution& Execution, UObject* l_This, int32* p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::BumpReferenceForAOT(int&)", 199);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[8];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
asDWORD v_TEMP_dword_2 = {};
// TrackRef v-2
// JitEntry *
// SUSPEND
// JitEntry *
// ValidateRef v-2
// PshVPtr v-2
// PopRPtr
l_valueRegister = (asQWORD)(p_Value);
// RDR4 v1
v_TEMP_dword_1 = value_read<asDWORD>((void*)l_valueRegister);
// ADDIi v1, v1, 5
((int32&)v_TEMP_dword_1) = ((int32&)v_TEMP_dword_1) + value_as<int>((asDWORD)0x5u);
// WRTV4 v1
memcpy((void*)l_valueRegister, (void*)(&v_TEMP_dword_1), 4);
// SUSPEND
// JitEntry *
// ValidateRef v-2
// PshVPtr v-2
// PopRPtr
l_valueRegister = (asQWORD)(p_Value);
// RDR4 v2
v_TEMP_dword_2 = value_read<asDWORD>((void*)l_valueRegister);
// MULIi v2, v2, 2
((int32&)v_TEMP_dword_2) = ((int32&)v_TEMP_dword_2) * value_as<int>((asDWORD)0x2u);
// CpyVtoR4 v2
l_dwordRegister = v_TEMP_dword_2;
// UntrackRef v-2
// RET 4
  return (int32)l_dwordRegister;
}
// AS Function : int BumpReferenceForAOT(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:197:2
// JIT Entry   : VM
void ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_0e62174e7a6ac532611bb5c523304c0baefade0a901b22ab785d62af07152acc_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_0e62174e7a6ac532611bb5c523304c0baefade0a901b22ab785d62af07152acc(Execution,
		*(UObject**)l_fp,
		*(int32**)(l_fp + 2));
}
// AS Function : int BumpReferenceForAOT(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:197:2
// JIT Entry   : Parms
void ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_0e62174e7a6ac532611bb5c523304c0baefade0a901b22ab785d62af07152acc_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_0e62174e7a6ac532611bb5c523304c0baefade0a901b22ab785d62af07152acc(Execution,
		(UObject*)Object,
		(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UStaticJITAotVirtualChild()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:309:0
 * StableFunctionKey  : 521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7
 * ExecutionHash      : 08e5d45dc20b81f5eed1bfd2dffc742af8fbde6e6be8fa295cc4db70f9dabd4e
 * DebugHash          : e99c1b7c1598b7ed5632a3ec913c738a97522a396a8ed5610511320c8b6818b1
 * EntryAbiHash       : 847e0fd88661ad9588f2e5b3a2e560f536b2c027896b3664ab7a3f99a32fee92
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_08e5d45dc20b81f5eed1bfd2dffc742af8fbde6e6be8fa295cc4db70f9dabd4e = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_08e5d45dc20b81f5eed1bfd2dffc742af8fbde6e6be8fa295cc4db70f9dabd4e
#endif

// AS Function : UStaticJITAotVirtualChild()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:309:0
// JIT Entry   : Raw
void ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_08e5d45dc20b81f5eed1bfd2dffc742af8fbde6e6be8fa295cc4db70f9dabd4e(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("UStaticJITAotVirtualChild::UStaticJITAotVirtualChild()", 0);
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
// CALL
// UStaticJITAotVirtualParent::UStaticJITAotVirtualParent()
SCRIPT_DEBUG_CALLSTACK_POSITION(0, 0);
{
  asCScriptFunction* CallFunction = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 1u);
  FStaticJITCurrentFunctionCall CurrentFunctionCall(Execution, FAngelscriptStableFunctionKey{{FAngelscriptHash256{{FBlake3Hash(FWideStringView(TEXT("f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef")))}}}}, CallFunction, (void*)((l_This)), false);
  CallFunction = CurrentFunctionCall.Get();
  if (CallFunction == nullptr) [[unlikely]]
  {
     SCRIPT_UNBOUND_EXCEPTION();
return;
  }
  FAngelscriptContext CallContext(CallFunction->GetEngine());
  CallContext->Prepare(CallFunction);
  CallContext->SetObject((void*)(l_This));
  {
     FScopeStaticJITNestedExceptionAdoption ExceptionAdoption(*CallContext);
     CallContext->Execute();
  }
  if (Execution.bExceptionThrown || CallContext->m_status != asEXECUTION_FINISHED)
  {
     FStaticJITFunction::AdoptContextException(Execution, *CallContext);
     Execution.bExceptionThrown = true;
return;
  }
}
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
// AS Function : UStaticJITAotVirtualChild()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:309:0
// JIT Entry   : VM
void ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_08e5d45dc20b81f5eed1bfd2dffc742af8fbde6e6be8fa295cc4db70f9dabd4e_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_08e5d45dc20b81f5eed1bfd2dffc742af8fbde6e6be8fa295cc4db70f9dabd4e(Execution,
		*(UObject**)l_fp);
}
// AS Function : UStaticJITAotVirtualChild()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:309:0
// JIT Entry   : Parms
void ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_08e5d45dc20b81f5eed1bfd2dffc742af8fbde6e6be8fa295cc4db70f9dabd4e_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_08e5d45dc20b81f5eed1bfd2dffc742af8fbde6e6be8fa295cc4db70f9dabd4e(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int Entry()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:83:1
 * StableFunctionKey  : 5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa
 * ExecutionHash      : 59c8c94c7be658662a71a29a388ac68fe8dfd9c1b43aa31eb7f3877d1758cbf9
 * DebugHash          : 3e21d3af5c5cfb08483851c8bb1b53a547ff080490619146be4cf2acb3362b00
 * EntryAbiHash       : db0c8f3058412e5bdc683b188ff132b140d00caf79e3e0658bda0254082fdee0
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_59c8c94c7be658662a71a29a388ac68fe8dfd9c1b43aa31eb7f3877d1758cbf9 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_59c8c94c7be658662a71a29a388ac68fe8dfd9c1b43aa31eb7f3877d1758cbf9
#endif

// AS Function : int Entry()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:83:1
// JIT Entry   : Raw
int32 ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_59c8c94c7be658662a71a29a388ac68fe8dfd9c1b43aa31eb7f3877d1758cbf9(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int Entry()", 85);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[4];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
asDWORD v_TEMP_dword_2 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// PshC4 35
// CALL
// int AddForAOT(const int Value)
SCRIPT_DEBUG_CALLSTACK_POSITION(85, 2);
{
  asCScriptFunction* CallFunction = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 0u);
  FStaticJITCurrentFunctionCall CurrentFunctionCall(Execution, FAngelscriptStableFunctionKey{{FAngelscriptHash256{{FBlake3Hash(FWideStringView(TEXT("4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30")))}}}}, CallFunction, nullptr, false);
  CallFunction = CurrentFunctionCall.Get();
  if (CallFunction == nullptr) [[unlikely]]
  {
     SCRIPT_UNBOUND_EXCEPTION();
return {};
  }
  FAngelscriptContext CallContext(CallFunction->GetEngine());
  CallContext->Prepare(CallFunction);
  CallContext->SetArgDWord(0, value_as<asDWORD>(((asDWORD)0x23u)));
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
  l_valueRegister = CallContext->m_regs.valueRegister;
}
// JitEntry *
// CpyRtoV4 v2
memcpy((&v_TEMP_dword_2), &l_valueRegister, 4);
// CpyVtoR4 v2
l_dwordRegister = v_TEMP_dword_2;
// RET 0
  return (int32)l_dwordRegister;
}
// AS Function : int Entry()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:83:1
// JIT Entry   : VM
void ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_59c8c94c7be658662a71a29a388ac68fe8dfd9c1b43aa31eb7f3877d1758cbf9_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_59c8c94c7be658662a71a29a388ac68fe8dfd9c1b43aa31eb7f3877d1758cbf9(Execution);
}
// AS Function : int Entry()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:83:1
// JIT Entry   : Parms
void ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_59c8c94c7be658662a71a29a388ac68fe8dfd9c1b43aa31eb7f3877d1758cbf9_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_59c8c94c7be658662a71a29a388ac68fe8dfd9c1b43aa31eb7f3877d1758cbf9(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void FailForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:186:2
 * StableFunctionKey  : 58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9
 * ExecutionHash      : 68246c952cd8e1da3ce4bf073623a504479ab8131a58e607cd2e8186928d1f00
 * DebugHash          : 28f98b27045bf896f77c156b2de79065648c8e4444ec87e711d84113777ce3d5
 * EntryAbiHash       : 5c88cfe1c954db11e6955527a1273af1e6e20436cb7527444616a92e521bcb4d
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_68246c952cd8e1da3ce4bf073623a504479ab8131a58e607cd2e8186928d1f00 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_68246c952cd8e1da3ce4bf073623a504479ab8131a58e607cd2e8186928d1f00
#endif

// AS Function : void FailForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:186:2
// JIT Entry   : Raw
void ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_68246c952cd8e1da3ce4bf073623a504479ab8131a58e607cd2e8186928d1f00(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("void UStaticJITAotFunctionCarrier::FailForAOT()", 188);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[8];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// CALLSYS *
// void throw(const FString&inout Message)
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 1u).GetFunction();
  auto CastedFuncPtr = (void(*)(void*))RawFuncPtr;
SCRIPT_DEBUG_CALLSTACK_POSITION(188, 3);
CastedFuncPtr((void**)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 3u)));
if (Execution.bExceptionThrown) [[unlikely]]
{
return;
}
}
// JitEntry *
// SUSPEND
// JitEntry *
// RET 2
  return;
}
// AS Function : void FailForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:186:2
// JIT Entry   : VM
void ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_68246c952cd8e1da3ce4bf073623a504479ab8131a58e607cd2e8186928d1f00_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_68246c952cd8e1da3ce4bf073623a504479ab8131a58e607cd2e8186928d1f00(Execution,
		*(UObject**)l_fp);
}
// AS Function : void FailForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:186:2
// JIT Entry   : Parms
void ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_68246c952cd8e1da3ce4bf073623a504479ab8131a58e607cd2e8186928d1f00_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_68246c952cd8e1da3ce4bf073623a504479ab8131a58e607cd2e8186928d1f00(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int GetVirtualValue()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:293:2
 * StableFunctionKey  : 598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c
 * ExecutionHash      : 161433a112646699b48a9161175b1156d5479b763d25cf71e7795de8c8731f2e
 * DebugHash          : 3d40835b6df7855101e5a66b1f0405e8c461b81a9de292b98d76bf8749362c78
 * EntryAbiHash       : 3c7da2556903a2aede2bf7c4a70e333223d38b106a790953f977db702ff61900
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_161433a112646699b48a9161175b1156d5479b763d25cf71e7795de8c8731f2e = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_161433a112646699b48a9161175b1156d5479b763d25cf71e7795de8c8731f2e
#endif

// AS Function : int GetVirtualValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:293:2
// JIT Entry   : Raw
int32 ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_161433a112646699b48a9161175b1156d5479b763d25cf71e7795de8c8731f2e(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotVirtualParent::GetVirtualValue()", 293);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[20];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v___ReturnValue = {};
UObject* v_TEMP_ptr_8 = nullptr;
alignas(4) asBYTE MEM_v_TEMP_5[sizeof(FName)];
FName& v_TEMP_5 = (FName&)MEM_v_TEMP_5[0];
asDWORD v_TEMP_dword_2 = {};
alignas(16) asBYTE l_ParmStruct[4];
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v1, 0
v___ReturnValue = 0x0u;
// SUSPEND
// JitEntry *
// PSF v1
// CALLSYS *
// void __Evt_PushArgumentRef__int(const int&inout Value)
{
(asDWORD&)l_ParmStruct[0] = *(asDWORD*)((&v___ReturnValue));
}
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PshC4 1
// PSF v5
// CALLSYS *
// FName __STATIC_NAME(int Id, const FString&inout CanonicalName)
{
new(((void*)((&v_TEMP_5)))) FName(FAngelscriptEngine::ResolveStaticName(value_as<int32>(((asDWORD)0x1u)), (*(FString*)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 7u)))));
}
// JitEntry *
// PSF v5
// PshVPtr v0
// RefCpyV v8, *
{
  asPWORD* d = (asPWORD*)(&v_TEMP_ptr_8);
  asPWORD s = (asPWORD)(l_This);
  asCObjectType* objType = (asCObjectType*)FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo(Execution, 3u);
  if (*d != s)
  {
    if ((objType->flags & (asOBJ_NOCOUNT | asOBJ_VALUE)) == 0)
    {
      if (*d != 0 && objType->beh.release != 0)
        SCRIPT_ENGINE->CallObjectMethod((void*)*d, objType->beh.release);
      if (s != 0 && objType->beh.addref != 0)
        SCRIPT_ENGINE->CallObjectMethod((void*)s, objType->beh.addref);
    }
  }
  if (*d != s)
    *d = s;
}
// PopPtr
// VAR v8
// GETOBJ
  asPWORD v = (asPWORD)(&v_TEMP_ptr_8);
// Materialize stack 1
value_assign_safe<asQWORD>(&l_stack[4], *(asPWORD*)v);
  *(asPWORD*)v = 0;
// CALLSYS *
// void __Evt_Execute(const UObject Object, const FName&inout Name)
{
UObject* EventObject = ((UObject*)((asQWORD&)l_stack[4]));
UFunction* EventFunction = EventObject->FindFunctionChecked(*(FName*)(((&v_TEMP_5))));
EventObject->ProcessEvent(EventFunction, &l_ParmStruct[0]);
*(asDWORD*)(((&v___ReturnValue))) = (asDWORD&)l_ParmStruct[0];
}
// JitEntry *
// SUSPEND
// JitEntry *
// CpyVtoR4 v1
l_dwordRegister = v___ReturnValue;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int GetVirtualValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:293:2
// JIT Entry   : VM
void ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_161433a112646699b48a9161175b1156d5479b763d25cf71e7795de8c8731f2e_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_161433a112646699b48a9161175b1156d5479b763d25cf71e7795de8c8731f2e(Execution,
		*(UObject**)l_fp);
}
// AS Function : int GetVirtualValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:293:2
// JIT Entry   : Parms
void ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_161433a112646699b48a9161175b1156d5479b763d25cf71e7795de8c8731f2e_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_161433a112646699b48a9161175b1156d5479b763d25cf71e7795de8c8731f2e(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UStaticJITAotFunctionCarrier ReturnSelfObject()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:266:2
 * StableFunctionKey  : 5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f
 * ExecutionHash      : 3018370c1d5e1cc9de3fd683fadb75e7761f78e06dd8ce7987c9330a1bce062b
 * DebugHash          : 3fdf6c8a27aa66040729e360f04f3ace30d4eb86ede5d23502ac973db7c645d0
 * EntryAbiHash       : 30ea0052fcb05cde2ff76d0f9f168b4c15908ca08694ae1622949ba51c05132e
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_3018370c1d5e1cc9de3fd683fadb75e7761f78e06dd8ce7987c9330a1bce062b = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_3018370c1d5e1cc9de3fd683fadb75e7761f78e06dd8ce7987c9330a1bce062b
#endif

// AS Function : UStaticJITAotFunctionCarrier ReturnSelfObject()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:266:2
// JIT Entry   : Raw
UObject* ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_3018370c1d5e1cc9de3fd683fadb75e7761f78e06dd8ce7987c9330a1bce062b(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("UStaticJITAotFunctionCarrier UStaticJITAotFunctionCarrier::ReturnSelfObject()", 268);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[8];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
UObject* v_TEMP_ptr_2 = nullptr;
// JitEntry *
// SUSPEND
// JitEntry *
// PshVPtr v0
// RefCpyV v2, *
{
  asPWORD* d = (asPWORD*)(&v_TEMP_ptr_2);
  asPWORD s = (asPWORD)(l_This);
  asCObjectType* objType = (asCObjectType*)FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo(Execution, 0u);
  if (*d != s)
  {
    if ((objType->flags & (asOBJ_NOCOUNT | asOBJ_VALUE)) == 0)
    {
      if (*d != 0 && objType->beh.release != 0)
        SCRIPT_ENGINE->CallObjectMethod((void*)*d, objType->beh.release);
      if (s != 0 && objType->beh.addref != 0)
        SCRIPT_ENGINE->CallObjectMethod((void*)s, objType->beh.addref);
    }
  }
  if (*d != s)
    *d = s;
}
// PopPtr
// LOADOBJ v2
l_objectRegister = (void*)v_TEMP_ptr_2;
v_TEMP_ptr_2 = nullptr;
// RET 2
  return (UObject*)l_objectRegister;
}
// AS Function : UStaticJITAotFunctionCarrier ReturnSelfObject()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:266:2
// JIT Entry   : VM
void ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_3018370c1d5e1cc9de3fd683fadb75e7761f78e06dd8ce7987c9330a1bce062b_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(UObject**)l_outValue = ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_3018370c1d5e1cc9de3fd683fadb75e7761f78e06dd8ce7987c9330a1bce062b(Execution,
		*(UObject**)l_fp);
}
// AS Function : UStaticJITAotFunctionCarrier ReturnSelfObject()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:266:2
// JIT Entry   : Parms
void ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_3018370c1d5e1cc9de3fd683fadb75e7761f78e06dd8ce7987c9330a1bce062b_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(UObject**)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_3018370c1d5e1cc9de3fd683fadb75e7761f78e06dd8ce7987c9330a1bce062b(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ObjectLifetimeEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:128:1
 * StableFunctionKey  : 5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d
 * ExecutionHash      : 558ba86db59491db742bc487a8c7901ae4d87cef1db86f96ea7e5c0761de4eef
 * DebugHash          : 06201126460fc03761ea3f11227edf85a6f198dc567103236424d0f1d41266cd
 * EntryAbiHash       : c8041da3cad4a3fdc7be0bd5795a7456826fc964a0eeef2d2d35507862c860f0
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_558ba86db59491db742bc487a8c7901ae4d87cef1db86f96ea7e5c0761de4eef = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_558ba86db59491db742bc487a8c7901ae4d87cef1db86f96ea7e5c0761de4eef
#endif

// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:128:1
// JIT Entry   : Raw
int32 ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_558ba86db59491db742bc487a8c7901ae4d87cef1db86f96ea7e5c0761de4eef(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int ObjectLifetimeEntryForAOT()", 130);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[24];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
alignas(8) asBYTE MEM_v_Original[sizeof(FString)];
FString& v_Original = (FString&)MEM_v_Original[0];
alignas(8) asBYTE MEM_v_Decorated[sizeof(FString)];
FString& v_Decorated = (FString&)MEM_v_Decorated[0];
alignas(8) asBYTE MEM_v_TEMP_8[sizeof(FString)];
FString& v_TEMP_8 = (FString&)MEM_v_TEMP_8[0];
asBYTE v_TEMP_byte_13 = {};
asDWORD v_TEMP_dword_14 = {};
asDWORD v_TEMP_dword_15 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PSF v4
// CALLSYS *
// FString::FString(const FString&inout Other)
{
  FString* Object = (FString*)((&v_Original));
new (Object) FString((*(FString*)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 6u))));
}
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PSF v12
// PSF v4
// CALLSYS *
// FString FString::opAdd(const FString&inout Other) const
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 1u).GetFunction();
  auto CastedFuncPtr = (FString(*)(void*,void*))RawFuncPtr;
  void* Object = (void*)((&v_Original));
SCRIPT_DEBUG_CALLSTACK_POSITION(131, 2);
new((void*)((&v_Decorated))) FString(CastedFuncPtr(Object,(void**)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 7u))));
if (Execution.bExceptionThrown) [[unlikely]]
{
goto LABEL_ObjectLifetimeEntryForAOT_EXCEPTION_0;
}
}
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PSF v4
// CALLSYS *
// bool FString::opEquals(const FString&inout Other) const
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 0u).GetFunction();
  auto CastedFuncPtr = (bool(*)(void*,void*))RawFuncPtr;
  void* Object = (void*)((&v_Original));
SCRIPT_DEBUG_CALLSTACK_POSITION(132, 2);
bool FunctionReturnValue = CastedFuncPtr(Object,(void**)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 6u)));
l_byteRegister = (asBYTE&)FunctionReturnValue;
if (Execution.bExceptionThrown) [[unlikely]]
{
goto LABEL_ObjectLifetimeEntryForAOT_EXCEPTION_1;
}
}
// JitEntry *
// JLowZ 4
if(l_byteRegister == 0) {
goto LABEL_ObjectLifetimeEntryForAOT_52;
}
// SetV4 v14, 100
v_TEMP_dword_14 = 0x64u;
// JMP 2
goto LABEL_ObjectLifetimeEntryForAOT_54;
LABEL_ObjectLifetimeEntryForAOT_52:
// SetV4 v14, 0
v_TEMP_dword_14 = 0x0u;
LABEL_ObjectLifetimeEntryForAOT_54:
// PSF v12
// CALLSYS *
// int FString::Len() const
{
  FString* Object = (FString*)((&v_Decorated));
int32 FunctionReturnValue = Object->Len();
l_dwordRegister = (asDWORD&)FunctionReturnValue;
}
// JitEntry *
// CpyRtoV4 v15
v_TEMP_dword_15 = l_dwordRegister;
// ADDi v14, v14, v15
((int32&)v_TEMP_dword_14) = ((int32&)v_TEMP_dword_14) + ((int32&)v_TEMP_dword_15);
// PSF v12
// CALLSYS *
// FString::~FString()
{
  FString* Object = (FString*)((&v_Decorated));
((FString*)Object)->~FString();
}
// JitEntry *
// PSF v4
// CALLSYS *
// FString::~FString()
{
  FString* Object = (FString*)((&v_Original));
((FString*)Object)->~FString();
}
// JitEntry *
// CpyVtoR4 v14
l_dwordRegister = v_TEMP_dword_14;
// RET 0
  return (int32)l_dwordRegister;
SCRIPT_ASSUME(false);

LABEL_ObjectLifetimeEntryForAOT_EXCEPTION_0:
// Exception cleanup for variables 4
((FString*)((FString*)(&v_Original)))->~FString();
return {};
LABEL_ObjectLifetimeEntryForAOT_EXCEPTION_1:
// Exception cleanup for variables 4, 12
((FString*)((FString*)(&v_Decorated)))->~FString();
((FString*)((FString*)(&v_Original)))->~FString();
return {};
}
// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:128:1
// JIT Entry   : VM
void ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_558ba86db59491db742bc487a8c7901ae4d87cef1db86f96ea7e5c0761de4eef_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_558ba86db59491db742bc487a8c7901ae4d87cef1db86f96ea7e5c0761de4eef(Execution);
}
// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:128:1
// JIT Entry   : Parms
void ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_558ba86db59491db742bc487a8c7901ae4d87cef1db86f96ea7e5c0761de4eef_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_558ba86db59491db742bc487a8c7901ae4d87cef1db86f96ea7e5c0761de4eef(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int BumpReference(int&inout)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:258:2
 * StableFunctionKey  : 635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1
 * ExecutionHash      : b9b9cf20c0c6ef3cd5642c43083fc7bf8266b6b8b2df8437b2603377d837b03e
 * DebugHash          : 7a2ab2848a816f22a3935278fe271a014b51b7ce2fcb4f06da7b416c842bbc6e
 * EntryAbiHash       : 59029a9011b50bdad9e86b6ca1d8d31868558cdf84799ffea7ef7660667c250a
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_b9b9cf20c0c6ef3cd5642c43083fc7bf8266b6b8b2df8437b2603377d837b03e = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_b9b9cf20c0c6ef3cd5642c43083fc7bf8266b6b8b2df8437b2603377d837b03e
#endif

// AS Function : int BumpReference(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:258:2
// JIT Entry   : Raw
int32 ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_b9b9cf20c0c6ef3cd5642c43083fc7bf8266b6b8b2df8437b2603377d837b03e(FScriptExecution& Execution, UObject* l_This, int32* p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::BumpReference(int&)", 260);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[8];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
asDWORD v_TEMP_dword_2 = {};
// TrackRef v-2
// JitEntry *
// SUSPEND
// JitEntry *
// ValidateRef v-2
// PshVPtr v-2
// PopRPtr
l_valueRegister = (asQWORD)(p_Value);
// RDR4 v1
v_TEMP_dword_1 = value_read<asDWORD>((void*)l_valueRegister);
// ADDIi v1, v1, 5
((int32&)v_TEMP_dword_1) = ((int32&)v_TEMP_dword_1) + value_as<int>((asDWORD)0x5u);
// WRTV4 v1
memcpy((void*)l_valueRegister, (void*)(&v_TEMP_dword_1), 4);
// SUSPEND
// JitEntry *
// ValidateRef v-2
// PshVPtr v-2
// PopRPtr
l_valueRegister = (asQWORD)(p_Value);
// RDR4 v2
v_TEMP_dword_2 = value_read<asDWORD>((void*)l_valueRegister);
// LoadThisR +48
l_valueRegister = ((asQWORD)l_This) + POFFSET_UStaticJITAotFunctionCarrier_StoredValue;
// WRTV4 v2
memcpy((void*)l_valueRegister, (void*)(&v_TEMP_dword_2), 4);
// SUSPEND
// JitEntry *
// ValidateRef v-2
// PshVPtr v-2
// PopRPtr
l_valueRegister = (asQWORD)(p_Value);
// RDR4 v1
v_TEMP_dword_1 = value_read<asDWORD>((void*)l_valueRegister);
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// UntrackRef v-2
// RET 4
  return (int32)l_dwordRegister;
}
// AS Function : int BumpReference(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:258:2
// JIT Entry   : VM
void ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_b9b9cf20c0c6ef3cd5642c43083fc7bf8266b6b8b2df8437b2603377d837b03e_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_b9b9cf20c0c6ef3cd5642c43083fc7bf8266b6b8b2df8437b2603377d837b03e(Execution,
		*(UObject**)l_fp,
		*(int32**)(l_fp + 2));
}
// AS Function : int BumpReference(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:258:2
// JIT Entry   : Parms
void ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_b9b9cf20c0c6ef3cd5642c43083fc7bf8266b6b8b2df8437b2603377d837b03e_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_b9b9cf20c0c6ef3cd5642c43083fc7bf8266b6b8b2df8437b2603377d837b03e(Execution,
		(UObject*)Object,
		(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UStaticJITAotFunctionCarrier UStaticJITAotFunctionCarrier()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:155:0
 * StableFunctionKey  : 6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060
 * ExecutionHash      : fb05ff1cd40b005becc20bdca090866a9a681873a5ef622458c77a1146814bec
 * DebugHash          : e99c1b7c1598b7ed5632a3ec913c738a97522a396a8ed5610511320c8b6818b1
 * EntryAbiHash       : b064ff3e8589a401db170315cb55c048d06915739f55618b8165aeca3b2dc0f1
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060_fb05ff1cd40b005becc20bdca090866a9a681873a5ef622458c77a1146814bec = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060_fb05ff1cd40b005becc20bdca090866a9a681873a5ef622458c77a1146814bec
#endif

// AS Function : UStaticJITAotFunctionCarrier UStaticJITAotFunctionCarrier()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:155:0
// JIT Entry   : Raw
UObject* ASJIT_6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060_fb05ff1cd40b005becc20bdca090866a9a681873a5ef622458c77a1146814bec(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("UStaticJITAotFunctionCarrier UStaticJITAotFunctionCarrier()", 0);
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
// UStaticJITAotFunctionCarrier::UStaticJITAotFunctionCarrier()
SCRIPT_DEBUG_CALLSTACK_POSITION(0, 0);
{
  asCScriptFunction* CallFunction = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 1u);
  FStaticJITCurrentFunctionCall CurrentFunctionCall(Execution, FAngelscriptStableFunctionKey{{FAngelscriptHash256{{FBlake3Hash(FWideStringView(TEXT("c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5")))}}}}, CallFunction, (void*)(((asQWORD&)l_stack[0])), true);
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
// AS Function : UStaticJITAotFunctionCarrier UStaticJITAotFunctionCarrier()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:155:0
// JIT Entry   : VM
void ASJIT_6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060_fb05ff1cd40b005becc20bdca090866a9a681873a5ef622458c77a1146814bec_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(UObject**)l_outValue = ASJIT_6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060_fb05ff1cd40b005becc20bdca090866a9a681873a5ef622458c77a1146814bec(Execution);
}
// AS Function : UStaticJITAotFunctionCarrier UStaticJITAotFunctionCarrier()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:155:0
// JIT Entry   : Parms
void ASJIT_6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060_fb05ff1cd40b005becc20bdca090866a9a681873a5ef622458c77a1146814bec_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(UObject**)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060_fb05ff1cd40b005becc20bdca090866a9a681873a5ef622458c77a1146814bec(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTExportedSymbolShowcase(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:44:1
 * StableFunctionKey  : 73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795
 * ExecutionHash      : e1f3ddcce67ec6a308007c17a95e2baf679cfc66c7a8c3627d3d235d989b3ae2
 * DebugHash          : 8d22bdc57beacd09150892181f521b7d68e0f22c64dd1915e8a57723de0f8063
 * EntryAbiHash       : 9309f3438eca43da589c8d5ac23b06a58e143747a47d2f54ff811daf7b2e9706
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_e1f3ddcce67ec6a308007c17a95e2baf679cfc66c7a8c3627d3d235d989b3ae2_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0,
	const int32 as_sem_s1);

static const FAngelscriptTypedASTJITBoundCallSite ASJIT_Call_DaysInMonth_a23d24f5_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795 = {
	TEXT("int DaysInMonth(int, int)"),
	TEXT("FDateTime::DaysInMonth"),
	FAngelscriptStableFunctionKey{FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("a23d24f5578bcda99902f5751c10969f2ce363c52ba277760f88484adeb965b3"))) }},
	FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("0b2738fee86236a40456adc468988e2e71464459685e587540372da2c4eaca13"))) },
	0
};

/*
 * AngelScript Native Call Site 0
 * AS Declaration      : int DaysInMonth(int, int)
 * Emitted C++ Callee  : AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32, int32>
 * Route               : CurrentNativeBinding
 * Effective Target    : FDateTime::DaysInMonth
 * Runtime DLL Core    : AngelscriptTypedASTJIT::ResolveBoundNative
 * Metadata Row        : ASJIT_Call_DaysInMonth_a23d24f5_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795
 * Reference Slot      : 0
 */
static int32 ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_e1f3ddcce67ec6a308007c17a95e2baf679cfc66c7a8c3627d3d235d989b3ae2_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0,
	const int32 as_sem_s1)
{
	TypedFrame.SetSourcePosition(46, 2);
	return ([&]() -> int32
	{
		TypedFrame.SetSourcePosition(46, 9);
		const int32 as_sem_e2_arg1 = as_sem_s1;
		const int32 as_sem_e2_arg0 = as_sem_s0;
		const int32 as_sem_e2_result = AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32, int32>(
			Execution, ASJIT_Call_DaysInMonth_a23d24f5_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795, as_sem_e2_arg0, as_sem_e2_arg1);
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return as_sem_e2_result;
	}());
}

// AS Function : int TypedASTExportedSymbolShowcase(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:44:1
// JIT Entry   : Raw
int32 ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_e1f3ddcce67ec6a308007c17a95e2baf679cfc66c7a8c3627d3d235d989b3ae2_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITAotFixture.as",
		"int TypedASTExportedSymbolShowcase(const int, const int)",
		44,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_e1f3ddcce67ec6a308007c17a95e2baf679cfc66c7a8c3627d3d235d989b3ae2_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1));
}

// AS Function : int TypedASTExportedSymbolShowcase(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:44:1
// JIT Entry   : VM
void ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_e1f3ddcce67ec6a308007c17a95e2baf679cfc66c7a8c3627d3d235d989b3ae2_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_e1f3ddcce67ec6a308007c17a95e2baf679cfc66c7a8c3627d3d235d989b3ae2_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}

// AS Function : int TypedASTExportedSymbolShowcase(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:44:1
// JIT Entry   : Parms
void ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_e1f3ddcce67ec6a308007c17a95e2baf679cfc66c7a8c3627d3d235d989b3ae2_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	static_cast<void>(Object);
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(void*));
	const SIZE_T GeneratedWorldContextParmOffset = ParmsOffset;
	ParmsOffset += sizeof(void*);
	static_cast<void>(GeneratedWorldContextParmOffset);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_e1f3ddcce67ec6a308007c17a95e2baf679cfc66c7a8c3627d3d235d989b3ae2_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : float SemanticPowerDoubleInt(const float, const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:38:1
 * StableFunctionKey  : 75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f
 * ExecutionHash      : 90a1fc318f267fc8f5e2b3073e52e781ebd72ad0117f706062b82d517a9d0848
 * DebugHash          : 7803600e49438af8e8cd92e8edda6b696fe2fb6a6906b8f1b850ca39def4b5cf
 * EntryAbiHash       : db0465d360843dd633375f902a889661f9e22535246942bafbda919d3c3cbcf1
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f_90a1fc318f267fc8f5e2b3073e52e781ebd72ad0117f706062b82d517a9d0848 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f_90a1fc318f267fc8f5e2b3073e52e781ebd72ad0117f706062b82d517a9d0848
#endif

// AS Function : float SemanticPowerDoubleInt(const float, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:38:1
// JIT Entry   : Raw
double ASJIT_75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f_90a1fc318f267fc8f5e2b3073e52e781ebd72ad0117f706062b82d517a9d0848(FScriptExecution& Execution, double p_Base, asDWORD p_Exponent)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("float SemanticPowerDoubleInt(float, int)", 40);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
double v_TEMP_double_2 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// POWdi v2, v0, v-2
v_TEMP_double_2 = FMath::Pow(p_Base, static_cast<double>(((int32&)p_Exponent)));
if (v_TEMP_double_2 == HUGE_VAL) [[unlikely]]
{
SCRIPT_DEBUG_CALLSTACK_POSITION(40, 2);
  SCRIPT_POWER_OVERFLOW_EXCEPTION();
return {};
}
// CpyVtoR8 v2
l_doubleRegister = v_TEMP_double_2;
// RET 3
  return l_doubleRegister;
}
// AS Function : float SemanticPowerDoubleInt(const float, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:38:1
// JIT Entry   : VM
void ASJIT_75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f_90a1fc318f267fc8f5e2b3073e52e781ebd72ad0117f706062b82d517a9d0848_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(double*)l_outValue = ASJIT_75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f_90a1fc318f267fc8f5e2b3073e52e781ebd72ad0117f706062b82d517a9d0848(Execution,
		*(double*)(l_fp + 0),
		*(asDWORD*)(l_fp + 2));
}
// AS Function : float SemanticPowerDoubleInt(const float, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:38:1
// JIT Entry   : Parms
void ASJIT_75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f_90a1fc318f267fc8f5e2b3073e52e781ebd72ad0117f706062b82d517a9d0848_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Base = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1_Exponent = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(double));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(double*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f_90a1fc318f267fc8f5e2b3073e52e781ebd72ad0117f706062b82d517a9d0848(Execution,
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_0_Base),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_1_Exponent));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : float SemanticPowerDouble(const float, const float)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:33:1
 * StableFunctionKey  : 794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43
 * ExecutionHash      : 48cea96ba0a986d0ad01106cf0cf4dfb83b96cbe754b2e295e2df31f57afb992
 * DebugHash          : 5e66cad00ad2353fbe348978d9a5f8903edc1560e791cfa08c2cbb9c7ed5bd8e
 * EntryAbiHash       : 802462d137df2242b59cb2b901557e6d9f177fed1caae7e133b299f6f631b506
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43_48cea96ba0a986d0ad01106cf0cf4dfb83b96cbe754b2e295e2df31f57afb992 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43_48cea96ba0a986d0ad01106cf0cf4dfb83b96cbe754b2e295e2df31f57afb992
#endif

// AS Function : float SemanticPowerDouble(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:33:1
// JIT Entry   : Raw
double ASJIT_794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43_48cea96ba0a986d0ad01106cf0cf4dfb83b96cbe754b2e295e2df31f57afb992(FScriptExecution& Execution, double p_Base, double p_Exponent)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("float SemanticPowerDouble(float, float)", 35);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
double v_TEMP_double_2 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// POWd v2, v0, v-2
v_TEMP_double_2 = FMath::Pow(p_Base, p_Exponent);
if (v_TEMP_double_2 == HUGE_VAL) [[unlikely]]
{
SCRIPT_DEBUG_CALLSTACK_POSITION(35, 2);
  SCRIPT_POWER_OVERFLOW_EXCEPTION();
return {};
}
// CpyVtoR8 v2
l_doubleRegister = v_TEMP_double_2;
// RET 4
  return l_doubleRegister;
}
// AS Function : float SemanticPowerDouble(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:33:1
// JIT Entry   : VM
void ASJIT_794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43_48cea96ba0a986d0ad01106cf0cf4dfb83b96cbe754b2e295e2df31f57afb992_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(double*)l_outValue = ASJIT_794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43_48cea96ba0a986d0ad01106cf0cf4dfb83b96cbe754b2e295e2df31f57afb992(Execution,
		*(double*)(l_fp + 0),
		*(double*)(l_fp + 2));
}
// AS Function : float SemanticPowerDouble(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:33:1
// JIT Entry   : Parms
void ASJIT_794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43_48cea96ba0a986d0ad01106cf0cf4dfb83b96cbe754b2e295e2df31f57afb992_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Base = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(double));
	const SIZE_T ParmOffset_1_Exponent = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(double));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(double*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43_48cea96ba0a986d0ad01106cf0cf4dfb83b96cbe754b2e295e2df31f57afb992(Execution,
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_0_Base),
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_1_Exponent));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int StaticWorldContextCheck(UObject, const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:324:1
 * StableFunctionKey  : 89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652
 * ExecutionHash      : 8d91fd16350ec4e44575077d45598ce8ae04e9b9a5c5158835a0dae927ccd053
 * DebugHash          : b8c46940f17d3bc305943ac076fd24a05b7c3d347ce684fae051e01a0a93f649
 * EntryAbiHash       : 43bf555b1cdd803979e0fcaa94ab1a10d69fba57c0e1e44934d93cb64c5eb6d1
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_8d91fd16350ec4e44575077d45598ce8ae04e9b9a5c5158835a0dae927ccd053 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_8d91fd16350ec4e44575077d45598ce8ae04e9b9a5c5158835a0dae927ccd053
#endif

// AS Function : int StaticWorldContextCheck(UObject, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:324:1
// JIT Entry   : Raw
int32 ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_8d91fd16350ec4e44575077d45598ce8ae04e9b9a5c5158835a0dae927ccd053(FScriptExecution& Execution, UObject* p_WorldContextObject, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int StaticWorldContextCheck(UObject, int)", 326);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[8];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
UObject* v_TEMP_ptr_2 = nullptr;
UObject* v_TEMP_ptr_4 = nullptr;
asBYTE v_TEMP_byte_5 = {};
asDWORD v_TEMP_dword_6 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// CALLSYS *
// UObject __WorldContext()
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 0u).GetFunction();
  auto CastedFuncPtr = (UObject*(*)())RawFuncPtr;
SCRIPT_DEBUG_CALLSTACK_POSITION(326, 2);
UObject* FunctionReturnValue = CastedFuncPtr();
l_objectRegister = (void*)FunctionReturnValue;
if (Execution.bExceptionThrown) [[unlikely]]
{
return {};
}
}
// JitEntry *
// STOREOBJ v2
v_TEMP_ptr_2 = (UObject*)l_objectRegister;
l_objectRegister = nullptr;
// PshVPtr v0
// RefCpyV v4, *
{
  asPWORD* d = (asPWORD*)(&v_TEMP_ptr_4);
  asPWORD s = (asPWORD)(p_WorldContextObject);
  asCObjectType* objType = (asCObjectType*)FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo(Execution, 1u);
  if (*d != s)
  {
    if ((objType->flags & (asOBJ_NOCOUNT | asOBJ_VALUE)) == 0)
    {
      if (*d != 0 && objType->beh.release != 0)
        SCRIPT_ENGINE->CallObjectMethod((void*)*d, objType->beh.release);
      if (s != 0 && objType->beh.addref != 0)
        SCRIPT_ENGINE->CallObjectMethod((void*)s, objType->beh.addref);
    }
  }
  if (*d != s)
    *d = s;
}
// PopPtr
// CmpPtr v2, v4
l_byteRegister = ((void*)v_TEMP_ptr_2 == (void*)v_TEMP_ptr_4) ? 0 : 1;
// TNZ
if(l_byteRegister != 0)
   l_byteRegister = 1;
else
   l_byteRegister = 0;
// CpyRtoV4 v5
v_TEMP_byte_5 = l_byteRegister;
// FREE v2, *
{
  void* obj = (void*)v_TEMP_ptr_2;
  if (obj != nullptr)
  {
    asCObjectType* objType = (asCObjectType*)FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo(Execution, 1u);
    if ((objType->flags & asOBJ_NOCOUNT) == 0 && objType->beh.release != 0)
      SCRIPT_ENGINE->CallObjectMethod(obj, objType->beh.release);
  }
}
v_TEMP_ptr_2 = nullptr;
// FREE v4, *
{
  void* obj = (void*)v_TEMP_ptr_4;
  if (obj != nullptr)
  {
    asCObjectType* objType = (asCObjectType*)FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo(Execution, 1u);
    if ((objType->flags & asOBJ_NOCOUNT) == 0 && objType->beh.release != 0)
      SCRIPT_ENGINE->CallObjectMethod(obj, objType->beh.release);
  }
}
v_TEMP_ptr_4 = nullptr;
// CpyVtoR1 v5
l_byteRegister = v_TEMP_byte_5;
// JLowZ 13
if(l_byteRegister == 0) {
goto LABEL_StaticWorldContextCheck_45;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v6, -1
v_TEMP_dword_6 = 0xffffffffu;
// CpyVtoR4 v6
l_dwordRegister = v_TEMP_dword_6;
// JMP 8
goto LABEL_StaticWorldContextCheck_53;
LABEL_StaticWorldContextCheck_45:
// SUSPEND
// JitEntry *
// ADDIi v6, v-2, 2
((int32&)v_TEMP_dword_6) = ((int32&)p_Value) + value_as<int>((asDWORD)0x2u);
// CpyVtoR4 v6
l_dwordRegister = v_TEMP_dword_6;
LABEL_StaticWorldContextCheck_53:
// FREE v0, *
{
  void* obj = (void*)p_WorldContextObject;
  if (obj != nullptr)
  {
    asCObjectType* objType = (asCObjectType*)FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo(Execution, 1u);
    if ((objType->flags & asOBJ_NOCOUNT) == 0 && objType->beh.release != 0)
      SCRIPT_ENGINE->CallObjectMethod(obj, objType->beh.release);
  }
}
p_WorldContextObject = nullptr;
// RET 3
  return (int32)l_dwordRegister;
}
// AS Function : int StaticWorldContextCheck(UObject, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:324:1
// JIT Entry   : VM
void ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_8d91fd16350ec4e44575077d45598ce8ae04e9b9a5c5158835a0dae927ccd053_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_8d91fd16350ec4e44575077d45598ce8ae04e9b9a5c5158835a0dae927ccd053(Execution,
		*(UObject**)(l_fp + 0),
		*(asDWORD*)(l_fp + 2));
}
// AS Function : int StaticWorldContextCheck(UObject, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:324:1
// JIT Entry   : Parms
void ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_8d91fd16350ec4e44575077d45598ce8ae04e9b9a5c5158835a0dae927ccd053_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_WorldContextObject = ParmsOffset;
	ParmsOffset += sizeof(void*);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_8d91fd16350ec4e44575077d45598ce8ae04e9b9a5c5158835a0dae927ccd053(Execution,
		*(UObject**)(((SIZE_T)Parms) + ParmOffset_0_WorldContextObject),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_1_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void StorePrimitiveArg(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:239:2
 * StableFunctionKey  : 8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf
 * ExecutionHash      : 8ebf159adfc3c0b241459d0fa1d50a697cd4b805685a94068e4eb11124e56f03
 * DebugHash          : 23ee88938aabf14e26e28997d16e6dcd17a36f7c5fa5f97982fc2124a2a0a592
 * EntryAbiHash       : 59c253776ca706e4b802002aa63a0562f899465e5e8dcb6976ad0e14745f555a
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_8ebf159adfc3c0b241459d0fa1d50a697cd4b805685a94068e4eb11124e56f03 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_8ebf159adfc3c0b241459d0fa1d50a697cd4b805685a94068e4eb11124e56f03
#endif

// AS Function : void StorePrimitiveArg(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:239:2
// JIT Entry   : Raw
void ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_8ebf159adfc3c0b241459d0fa1d50a697cd4b805685a94068e4eb11124e56f03(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("void UStaticJITAotFunctionCarrier::StorePrimitiveArg(int)", 241);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// ADDIi v1, v-2, 3
((int32&)v_TEMP_dword_1) = ((int32&)p_Value) + value_as<int>((asDWORD)0x3u);
// LoadThisR +48
l_valueRegister = ((asQWORD)l_This) + POFFSET_UStaticJITAotFunctionCarrier_StoredValue;
// WRTV4 v1
memcpy((void*)l_valueRegister, (void*)(&v_TEMP_dword_1), 4);
// SUSPEND
// JitEntry *
// RET 3
  return;
}
// AS Function : void StorePrimitiveArg(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:239:2
// JIT Entry   : VM
void ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_8ebf159adfc3c0b241459d0fa1d50a697cd4b805685a94068e4eb11124e56f03_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_8ebf159adfc3c0b241459d0fa1d50a697cd4b805685a94068e4eb11124e56f03(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : void StorePrimitiveArg(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:239:2
// JIT Entry   : Parms
void ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_8ebf159adfc3c0b241459d0fa1d50a697cd4b805685a94068e4eb11124e56f03_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_8ebf159adfc3c0b241459d0fa1d50a697cd4b805685a94068e4eb11124e56f03(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ExceptionEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:191:2
 * StableFunctionKey  : 91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277
 * ExecutionHash      : 37f8b3d211d13533245d7e8e2c19aa9094d05556c2240e1289b057acd844c5d7
 * DebugHash          : 523e1087e636c4e8b3fd20f68eabafb8d532c41ee0e45afa01ba4ff14b25fdb9
 * EntryAbiHash       : 8e204c65d868a3511f949a72a8040d1e58f2c414351b97e6ddef4ad8bc9d5b08
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_37f8b3d211d13533245d7e8e2c19aa9094d05556c2240e1289b057acd844c5d7 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_37f8b3d211d13533245d7e8e2c19aa9094d05556c2240e1289b057acd844c5d7
#endif

// AS Function : int ExceptionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:191:2
// JIT Entry   : Raw
int32 ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_37f8b3d211d13533245d7e8e2c19aa9094d05556c2240e1289b057acd844c5d7(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::ExceptionEntryForAOT()", 193);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[8];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// PshVPtr v0
// CALLINTF
// void UStaticJITAotFunctionCarrier::FailForAOT()
SCRIPT_DEBUG_CALLSTACK_POSITION(193, 3);
{
  asCScriptFunction* CallFunction = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 1u);
  FStaticJITCurrentFunctionCall CurrentFunctionCall(Execution, FAngelscriptStableFunctionKey{{FAngelscriptHash256{{FBlake3Hash(FWideStringView(TEXT("58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9")))}}}}, CallFunction, (void*)((l_This)), true);
  CallFunction = CurrentFunctionCall.Get();
  if (CallFunction == nullptr) [[unlikely]]
  {
     SCRIPT_UNBOUND_EXCEPTION();
return {};
  }
  FAngelscriptContext CallContext(CallFunction->GetEngine());
  CallContext->Prepare(CallFunction);
  CallContext->SetObject((void*)(l_This));
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
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v1, 99
v_TEMP_dword_1 = 0x63u;
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int ExceptionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:191:2
// JIT Entry   : VM
void ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_37f8b3d211d13533245d7e8e2c19aa9094d05556c2240e1289b057acd844c5d7_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_37f8b3d211d13533245d7e8e2c19aa9094d05556c2240e1289b057acd844c5d7(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ExceptionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:191:2
// JIT Entry   : Parms
void ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_37f8b3d211d13533245d7e8e2c19aa9094d05556c2240e1289b057acd844c5d7_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_37f8b3d211d13533245d7e8e2c19aa9094d05556c2240e1289b057acd844c5d7(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int RecurseForAOT(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:218:2
 * StableFunctionKey  : 947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7
 * ExecutionHash      : e2a0dedd7d7ebfe498830d40c0881570ab0cefd2eb6f2de8c3ffcdbf87c37157
 * DebugHash          : 375d74be85cb27dd373f5a6972393f822c4d0814e4ebb5bdb7d6995f26cdacea
 * EntryAbiHash       : 805d2b5d622dfc1d79e7a6e1f19c0e4068732e3afa951e3f3b429e924ed5af85
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_e2a0dedd7d7ebfe498830d40c0881570ab0cefd2eb6f2de8c3ffcdbf87c37157 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_e2a0dedd7d7ebfe498830d40c0881570ab0cefd2eb6f2de8c3ffcdbf87c37157
#endif

// AS Function : int RecurseForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:218:2
// JIT Entry   : Raw
int32 ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_e2a0dedd7d7ebfe498830d40c0881570ab0cefd2eb6f2de8c3ffcdbf87c37157(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::RecurseForAOT(int)", 220);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[12];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
asBYTE v_TEMP_byte_2 = {};
asDWORD v_TEMP_dword_3 = {};
asDWORD v_TEMP_dword_4 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// CMPIi v-2, 0
  if (((int32&)p_Value) == value_as<int>((asDWORD)0x0u))     l_byteRegister = 0;
  else if (((int32&)p_Value) < value_as<int>((asDWORD)0x0u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JNZ 13
if(l_byteRegister != 0) {
goto LABEL_UStaticJITAotFunctionCarrier__RecurseForAOT_24;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v1, 0
v_TEMP_dword_1 = 0x0u;
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// JMP 21
goto LABEL_UStaticJITAotFunctionCarrier__RecurseForAOT_45;
LABEL_UStaticJITAotFunctionCarrier__RecurseForAOT_24:
// SUSPEND
// JitEntry *
// SUBIi v1, v-2, 1
((int32&)v_TEMP_dword_1) = ((int32&)p_Value) - value_as<int>((asDWORD)0x1u);
// PshV4 v1
// PshVPtr v0
// CALLINTF
// int UStaticJITAotFunctionCarrier::RecurseForAOT(const int Value)
SCRIPT_DEBUG_CALLSTACK_POSITION(224, 3);
{
  asCScriptFunction* CallFunction = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 1u);
  FStaticJITCurrentFunctionCall CurrentFunctionCall(Execution, FAngelscriptStableFunctionKey{{FAngelscriptHash256{{FBlake3Hash(FWideStringView(TEXT("947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7")))}}}}, CallFunction, (void*)((l_This)), true);
  CallFunction = CurrentFunctionCall.Get();
  if (CallFunction == nullptr) [[unlikely]]
  {
     SCRIPT_UNBOUND_EXCEPTION();
return {};
  }
  FAngelscriptContext CallContext(CallFunction->GetEngine());
  CallContext->Prepare(CallFunction);
  CallContext->SetObject((void*)(l_This));
  CallContext->SetArgDWord(0, value_as<asDWORD>((v_TEMP_dword_1)));
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
  l_valueRegister = CallContext->m_regs.valueRegister;
}
// JitEntry *
// CpyRtoV4 v3
memcpy((&v_TEMP_dword_3), &l_valueRegister, 4);
// MULIi v1, v3, 10
((int32&)v_TEMP_dword_1) = ((int32&)v_TEMP_dword_3) * value_as<int>((asDWORD)0xau);
// ADDi v4, v-2, v1
((int32&)v_TEMP_dword_4) = ((int32&)p_Value) + ((int32&)v_TEMP_dword_1);
// CpyVtoR4 v4
l_dwordRegister = v_TEMP_dword_4;
LABEL_UStaticJITAotFunctionCarrier__RecurseForAOT_45:
// RET 3
  return (int32)l_dwordRegister;
}
// AS Function : int RecurseForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:218:2
// JIT Entry   : VM
void ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_e2a0dedd7d7ebfe498830d40c0881570ab0cefd2eb6f2de8c3ffcdbf87c37157_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_e2a0dedd7d7ebfe498830d40c0881570ab0cefd2eb6f2de8c3ffcdbf87c37157(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : int RecurseForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:218:2
// JIT Entry   : Parms
void ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_e2a0dedd7d7ebfe498830d40c0881570ab0cefd2eb6f2de8c3ffcdbf87c37157_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_e2a0dedd7d7ebfe498830d40c0881570ab0cefd2eb6f2de8c3ffcdbf87c37157(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int GetVirtualValue_Implementation()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:310:3
 * StableFunctionKey  : 980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1
 * ExecutionHash      : d7eaf1c7675e62f28fd59cc6748fb409101a0ee62f733b718a57f18f9eefb57e
 * DebugHash          : 04becc36e30601421672c73a1ae5f2433d3db8f187b88446ac96997ed69dc4d7
 * EntryAbiHash       : 507cc902edc1d9bd3e2f2572fe86bb7d02b96a550445798d3f067222eb350188
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_d7eaf1c7675e62f28fd59cc6748fb409101a0ee62f733b718a57f18f9eefb57e = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_d7eaf1c7675e62f28fd59cc6748fb409101a0ee62f733b718a57f18f9eefb57e
#endif

// AS Function : int GetVirtualValue_Implementation()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:310:3
// JIT Entry   : Raw
int32 ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_d7eaf1c7675e62f28fd59cc6748fb409101a0ee62f733b718a57f18f9eefb57e(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotVirtualChild::GetVirtualValue_Implementation()", 312);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v1, 217
v_TEMP_dword_1 = 0xd9u;
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int GetVirtualValue_Implementation()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:310:3
// JIT Entry   : VM
void ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_d7eaf1c7675e62f28fd59cc6748fb409101a0ee62f733b718a57f18f9eefb57e_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_d7eaf1c7675e62f28fd59cc6748fb409101a0ee62f733b718a57f18f9eefb57e(Execution,
		*(UObject**)l_fp);
}
// AS Function : int GetVirtualValue_Implementation()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:310:3
// JIT Entry   : Parms
void ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_d7eaf1c7675e62f28fd59cc6748fb409101a0ee62f733b718a57f18f9eefb57e_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_d7eaf1c7675e62f28fd59cc6748fb409101a0ee62f733b718a57f18f9eefb57e(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int BumpReferenceForAOT(int&inout)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:115:1
 * StableFunctionKey  : 9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05
 * ExecutionHash      : 9e0ee7c8a454f7220efc004fa5e64d9f4a3b896d4cc8dc9597c518621b86e70c
 * DebugHash          : e6ebce0032bf30692e643d9698bb60e13ede42d0828bee2cd8cf02c5349d3487
 * EntryAbiHash       : b80efaaa778ead65430c5702610f23a7b6dccd414f3c044078aa45e30f15b055
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_9e0ee7c8a454f7220efc004fa5e64d9f4a3b896d4cc8dc9597c518621b86e70c = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_9e0ee7c8a454f7220efc004fa5e64d9f4a3b896d4cc8dc9597c518621b86e70c
#endif

// AS Function : int BumpReferenceForAOT(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:115:1
// JIT Entry   : Raw
int32 ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_9e0ee7c8a454f7220efc004fa5e64d9f4a3b896d4cc8dc9597c518621b86e70c(FScriptExecution& Execution, int32* p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int BumpReferenceForAOT(int&)", 117);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[8];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
asDWORD v_TEMP_dword_2 = {};
// TrackRef v0
// JitEntry *
// SUSPEND
// JitEntry *
// ValidateRef v0
// PshVPtr v0
// PopRPtr
l_valueRegister = (asQWORD)(p_Value);
// RDR4 v1
v_TEMP_dword_1 = value_read<asDWORD>((void*)l_valueRegister);
// ADDIi v1, v1, 5
((int32&)v_TEMP_dword_1) = ((int32&)v_TEMP_dword_1) + value_as<int>((asDWORD)0x5u);
// WRTV4 v1
memcpy((void*)l_valueRegister, (void*)(&v_TEMP_dword_1), 4);
// SUSPEND
// JitEntry *
// ValidateRef v0
// PshVPtr v0
// PopRPtr
l_valueRegister = (asQWORD)(p_Value);
// RDR4 v2
v_TEMP_dword_2 = value_read<asDWORD>((void*)l_valueRegister);
// MULIi v2, v2, 2
((int32&)v_TEMP_dword_2) = ((int32&)v_TEMP_dword_2) * value_as<int>((asDWORD)0x2u);
// CpyVtoR4 v2
l_dwordRegister = v_TEMP_dword_2;
// UntrackRef v0
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int BumpReferenceForAOT(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:115:1
// JIT Entry   : VM
void ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_9e0ee7c8a454f7220efc004fa5e64d9f4a3b896d4cc8dc9597c518621b86e70c_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_9e0ee7c8a454f7220efc004fa5e64d9f4a3b896d4cc8dc9597c518621b86e70c(Execution,
		*(int32**)(l_fp + 0));
}
// AS Function : int BumpReferenceForAOT(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:115:1
// JIT Entry   : Parms
void ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_9e0ee7c8a454f7220efc004fa5e64d9f4a3b896d4cc8dc9597c518621b86e70c_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_9e0ee7c8a454f7220efc004fa5e64d9f4a3b896d4cc8dc9597c518621b86e70c(Execution,
		(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ImportEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:149:1
 * StableFunctionKey  : 9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a
 * ExecutionHash      : f643f2af98b5abccabf8ba607388698dce70fef161600767865f502ab4ba7ec1
 * DebugHash          : 01120825f1c3a2e1f289bb68407e79dc79dc5c95b1138ff192d2fc027562015d
 * EntryAbiHash       : 4b531289681eab0fc4f4f249dee5c27f1a8dedc1b24ed061e98876f5dedce042
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_f643f2af98b5abccabf8ba607388698dce70fef161600767865f502ab4ba7ec1 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_f643f2af98b5abccabf8ba607388698dce70fef161600767865f502ab4ba7ec1
#endif

// AS Function : int ImportEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:149:1
// JIT Entry   : Raw
int32 ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_f643f2af98b5abccabf8ba607388698dce70fef161600767865f502ab4ba7ec1(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int ImportEntryForAOT()", 151);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[4];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
asDWORD v_TEMP_dword_2 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// PshC4 10
// CALL
// int ImportedValueForAOT(const int Value)
SCRIPT_DEBUG_CALLSTACK_POSITION(151, 2);
{
  asCScriptFunction* CallFunction = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 0u);
  FStaticJITCurrentFunctionCall CurrentFunctionCall(Execution, FAngelscriptStableFunctionKey{{FAngelscriptHash256{{FBlake3Hash(FWideStringView(TEXT("69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304")))}}}}, CallFunction, nullptr, false);
  CallFunction = CurrentFunctionCall.Get();
  if (CallFunction == nullptr) [[unlikely]]
  {
     SCRIPT_UNBOUND_EXCEPTION();
return {};
  }
  FAngelscriptContext CallContext(CallFunction->GetEngine());
  CallContext->Prepare(CallFunction);
  CallContext->SetArgDWord(0, value_as<asDWORD>(((asDWORD)0xau)));
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
  l_valueRegister = CallContext->m_regs.valueRegister;
}
// JitEntry *
// CpyRtoV4 v2
memcpy((&v_TEMP_dword_2), &l_valueRegister, 4);
// CpyVtoR4 v2
l_dwordRegister = v_TEMP_dword_2;
// RET 0
  return (int32)l_dwordRegister;
}
// AS Function : int ImportEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:149:1
// JIT Entry   : VM
void ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_f643f2af98b5abccabf8ba607388698dce70fef161600767865f502ab4ba7ec1_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_f643f2af98b5abccabf8ba607388698dce70fef161600767865f502ab4ba7ec1(Execution);
}
// AS Function : int ImportEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:149:1
// JIT Entry   : Parms
void ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_f643f2af98b5abccabf8ba607388698dce70fef161600767865f502ab4ba7ec1_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_f643f2af98b5abccabf8ba607388698dce70fef161600767865f502ab4ba7ec1(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ThreadSafeReturn()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:278:2
 * StableFunctionKey  : 9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7
 * ExecutionHash      : aa09074ddab30af509e57b20a66e3301fd95e90c0f53585e7954a5d76bec975c
 * DebugHash          : 4926cbd43899f0deff7d8fa15b03eca6f16af4cec4abd8b3cd7578d9c5d4e41e
 * EntryAbiHash       : dc83c109706793b798e437d104797c6bd2946d45dd2a7a22bc5dc94aa6809a03
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_aa09074ddab30af509e57b20a66e3301fd95e90c0f53585e7954a5d76bec975c = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_aa09074ddab30af509e57b20a66e3301fd95e90c0f53585e7954a5d76bec975c
#endif

// AS Function : int ThreadSafeReturn()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:278:2
// JIT Entry   : Raw
int32 ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_aa09074ddab30af509e57b20a66e3301fd95e90c0f53585e7954a5d76bec975c(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::ThreadSafeReturn()", 280);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v1, 73
v_TEMP_dword_1 = 0x49u;
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int ThreadSafeReturn()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:278:2
// JIT Entry   : VM
void ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_aa09074ddab30af509e57b20a66e3301fd95e90c0f53585e7954a5d76bec975c_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_aa09074ddab30af509e57b20a66e3301fd95e90c0f53585e7954a5d76bec975c(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ThreadSafeReturn()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:278:2
// JIT Entry   : Parms
void ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_aa09074ddab30af509e57b20a66e3301fd95e90c0f53585e7954a5d76bec975c_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_aa09074ddab30af509e57b20a66e3301fd95e90c0f53585e7954a5d76bec975c(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void IncrementReference(int&inout)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:251:2
 * StableFunctionKey  : 9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e
 * ExecutionHash      : 4db4ef9a2b689826b018585d8640d54df2ab20c17fcd503eebf7f94a30d515f1
 * DebugHash          : a53146aa6c6628f0b203845062678771a5bd464533da9f3c44c538028c485ae6
 * EntryAbiHash       : edfd275150d177872c0521a27feaed2db8935ff989a49d20f43a3627044ec161
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_4db4ef9a2b689826b018585d8640d54df2ab20c17fcd503eebf7f94a30d515f1 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_4db4ef9a2b689826b018585d8640d54df2ab20c17fcd503eebf7f94a30d515f1
#endif

// AS Function : void IncrementReference(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:251:2
// JIT Entry   : Raw
void ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_4db4ef9a2b689826b018585d8640d54df2ab20c17fcd503eebf7f94a30d515f1(FScriptExecution& Execution, UObject* l_This, int32* p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("void UStaticJITAotFunctionCarrier::IncrementReference(int&)", 253);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[8];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
asDWORD v_TEMP_dword_2 = {};
// TrackRef v-2
// JitEntry *
// SUSPEND
// JitEntry *
// ValidateRef v-2
// PshVPtr v-2
// PopRPtr
l_valueRegister = (asQWORD)(p_Value);
// RDR4 v1
v_TEMP_dword_1 = value_read<asDWORD>((void*)l_valueRegister);
// ADDIi v1, v1, 9
((int32&)v_TEMP_dword_1) = ((int32&)v_TEMP_dword_1) + value_as<int>((asDWORD)0x9u);
// WRTV4 v1
memcpy((void*)l_valueRegister, (void*)(&v_TEMP_dword_1), 4);
// SUSPEND
// JitEntry *
// ValidateRef v-2
// PshVPtr v-2
// PopRPtr
l_valueRegister = (asQWORD)(p_Value);
// RDR4 v2
v_TEMP_dword_2 = value_read<asDWORD>((void*)l_valueRegister);
// LoadThisR +48
l_valueRegister = ((asQWORD)l_This) + POFFSET_UStaticJITAotFunctionCarrier_StoredValue;
// WRTV4 v2
memcpy((void*)l_valueRegister, (void*)(&v_TEMP_dword_2), 4);
// SUSPEND
// JitEntry *
// UntrackRef v-2
// RET 4
  return;
}
// AS Function : void IncrementReference(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:251:2
// JIT Entry   : VM
void ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_4db4ef9a2b689826b018585d8640d54df2ab20c17fcd503eebf7f94a30d515f1_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_4db4ef9a2b689826b018585d8640d54df2ab20c17fcd503eebf7f94a30d515f1(Execution,
		*(UObject**)l_fp,
		*(int32**)(l_fp + 2));
}
// AS Function : void IncrementReference(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:251:2
// JIT Entry   : Parms
void ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_4db4ef9a2b689826b018585d8640d54df2ab20c17fcd503eebf7f94a30d515f1_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_4db4ef9a2b689826b018585d8640d54df2ab20c17fcd503eebf7f94a30d515f1(Execution,
		(UObject*)Object,
		(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void ClientStoreRpcValue(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:283:2
 * StableFunctionKey  : a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9
 * ExecutionHash      : 66b34169a0f72d658c4febf83548ea1a8edc0d55d86c772677e821ce0ccf3cf4
 * DebugHash          : 6b439672315ecba878b5faebb82211687f7c0e535564f04d748e3e6a6dbeef84
 * EntryAbiHash       : 97c4a3b9d12b23ba1b0401fc6dc59e5b24fabb6eb7c3ce760a58844d3692b2f9
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9_66b34169a0f72d658c4febf83548ea1a8edc0d55d86c772677e821ce0ccf3cf4 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9_66b34169a0f72d658c4febf83548ea1a8edc0d55d86c772677e821ce0ccf3cf4
#endif

// AS Function : void ClientStoreRpcValue(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:283:2
// JIT Entry   : Raw
void ASJIT_a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9_66b34169a0f72d658c4febf83548ea1a8edc0d55d86c772677e821ce0ccf3cf4(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("void UStaticJITAotFunctionCarrier::ClientStoreRpcValue(int)", 283);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[20];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
UObject* v_TEMP_ptr_6 = nullptr;
alignas(4) asBYTE MEM_v_TEMP_4[sizeof(FName)];
FName& v_TEMP_4 = (FName&)MEM_v_TEMP_4[0];
asDWORD v_TEMP_dword_1 = {};
alignas(16) asBYTE l_ParmStruct[4];
// JitEntry *
// SUSPEND
// JitEntry *
// PSF v-2
// CALLSYS *
// void __Evt_PushArgument__int(const int&inout Value)
{
(asDWORD&)l_ParmStruct[0] = *(asDWORD*)((&p_Value));
}
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PshC4 0
// PSF v4
// CALLSYS *
// FName __STATIC_NAME(int Id, const FString&inout CanonicalName)
{
new(((void*)((&v_TEMP_4)))) FName(FAngelscriptEngine::ResolveStaticName(value_as<int32>(((asDWORD)0x0u)), (*(FString*)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 7u)))));
}
// JitEntry *
// PSF v4
// PshVPtr v0
// RefCpyV v6, *
{
  asPWORD* d = (asPWORD*)(&v_TEMP_ptr_6);
  asPWORD s = (asPWORD)(l_This);
  asCObjectType* objType = (asCObjectType*)FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo(Execution, 4u);
  if (*d != s)
  {
    if ((objType->flags & (asOBJ_NOCOUNT | asOBJ_VALUE)) == 0)
    {
      if (*d != 0 && objType->beh.release != 0)
        SCRIPT_ENGINE->CallObjectMethod((void*)*d, objType->beh.release);
      if (s != 0 && objType->beh.addref != 0)
        SCRIPT_ENGINE->CallObjectMethod((void*)s, objType->beh.addref);
    }
  }
  if (*d != s)
    *d = s;
}
// PopPtr
// VAR v6
// GETOBJ
  asPWORD v = (asPWORD)(&v_TEMP_ptr_6);
// Materialize stack 1
value_assign_safe<asQWORD>(&l_stack[4], *(asPWORD*)v);
  *(asPWORD*)v = 0;
// CALLSYS *
// void __Evt_Execute(const UObject Object, const FName&inout Name)
{
UObject* EventObject = ((UObject*)((asQWORD&)l_stack[4]));
UFunction* EventFunction = EventObject->FindFunctionChecked(*(FName*)(((&v_TEMP_4))));
EventObject->ProcessEvent(EventFunction, &l_ParmStruct[0]);
}
// JitEntry *
// SUSPEND
// JitEntry *
// RET 3
  return;
}
// AS Function : void ClientStoreRpcValue(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:283:2
// JIT Entry   : VM
void ASJIT_a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9_66b34169a0f72d658c4febf83548ea1a8edc0d55d86c772677e821ce0ccf3cf4_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9_66b34169a0f72d658c4febf83548ea1a8edc0d55d86c772677e821ce0ccf3cf4(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : void ClientStoreRpcValue(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:283:2
// JIT Entry   : Parms
void ASJIT_a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9_66b34169a0f72d658c4febf83548ea1a8edc0d55d86c772677e821ce0ccf3cf4_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ASJIT_a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9_66b34169a0f72d658c4febf83548ea1a8edc0d55d86c772677e821ce0ccf3cf4(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:50:1
 * StableFunctionKey  : a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b
 * ExecutionHash      : f7ee08ffb0af7fd6427bebd4c9dfc21fb9ec36a2a04aabc61d7c2a60a1097872
 * DebugHash          : b13cf30324227b9a171896dc6287854a35be6e5d2dfefde39170521d32f42fb8
 * EntryAbiHash       : 315429bd98bc57e8264be8ad0e7bc5007a550d671431aa6b3c1f76f34b2aa667
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_f7ee08ffb0af7fd6427bebd4c9dfc21fb9ec36a2a04aabc61d7c2a60a1097872_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0,
	const int32 as_sem_s1,
	const bool as_sem_s2,
	const bool as_sem_s3);

/*
 * AngelScript Native Call Site 0
 * AS Declaration      : bool IsRunningCommandlet()
 * Emitted C++ Callee  : IsRunningCommandlet
 * Route               : DirectInline
 * Effective Target    : IsRunningCommandlet
 * Runtime DLL Core    : <none>
 * Metadata Row        : <none>
 * Reference Slot      : -1
 */
static int32 ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_f7ee08ffb0af7fd6427bebd4c9dfc21fb9ec36a2a04aabc61d7c2a60a1097872_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0,
	const int32 as_sem_s1,
	const bool as_sem_s2,
	const bool as_sem_s3)
{
	TypedFrame.SetSourcePosition(56, 6);
	int32 as_sem_s4 = ([&]() -> int32
	{
		const int32 as_sem_e2_left = as_sem_s0;
		const int32 as_sem_e2_right = as_sem_s1;
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e2_left, as_sem_e2_right);
	}());
	TypedFrame.SetSourcePosition(57, 2);
	([&]() -> int32
	{
		const int32 as_sem_e7_value = ([&]() -> int32
		{
			const int32 as_sem_e5_left = as_sem_s4;
			const int32 as_sem_e5_right = int32(1);
			return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_sem_e5_left, as_sem_e5_right);
		}());
		int32& as_sem_e7_target = as_sem_s4;
		as_sem_e7_target = as_sem_e7_value;
		return as_sem_e7_target;
	}());
	TypedFrame.SetSourcePosition(58, 2);
	([&]() -> int32
	{
		const int32 as_sem_e10_value = int32(3);
		int32& as_sem_e10_target = as_sem_s4;
		as_sem_e10_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e10_target, as_sem_e10_value);
		return as_sem_e10_target;
	}());
	TypedFrame.SetSourcePosition(59, 2);
	([&]() -> int32
	{
		const int32 as_sem_e13_value = int32(1);
		int32& as_sem_e13_target = as_sem_s4;
		as_sem_e13_target = AngelscriptTypedASTJIT::WrapSubtract<int32>(as_sem_e13_target, as_sem_e13_value);
		return as_sem_e13_target;
	}());
	TypedFrame.SetSourcePosition(60, 2);
	([&]() -> int32
	{
		const int32 as_sem_e16_value = int32(2);
		int32& as_sem_e16_target = as_sem_s4;
		as_sem_e16_target = AngelscriptTypedASTJIT::WrapMultiply<int32>(as_sem_e16_target, as_sem_e16_value);
		return as_sem_e16_target;
	}());
	TypedFrame.SetSourcePosition(62, 6);
	const int32 as_sem_s5 = ([&]() -> int32
	{
		int32& as_sem_e18_target = as_sem_s4;
		as_sem_e18_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e18_target, int32(1));
		return as_sem_e18_target;
	}());
	TypedFrame.SetSourcePosition(63, 6);
	const int32 as_sem_s6 = ([&]() -> int32
	{
		int32& as_sem_e20_target = as_sem_s4;
		const int32 as_sem_e20_old = as_sem_e20_target;
		as_sem_e20_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e20_old, int32(1));
		return as_sem_e20_old;
	}());
	TypedFrame.SetSourcePosition(64, 6);
	const int32 as_sem_s7 = AngelscriptTypedASTJIT::WrapNegate<int32>(as_sem_s5);
	TypedFrame.SetSourcePosition(65, 6);
	const int32 as_sem_s8 = AngelscriptTypedASTJIT::BitwiseNot<int32>(as_sem_s6);
	TypedFrame.SetSourcePosition(66, 6);
	const int32 as_sem_s9 = ([&]() -> int32
	{
		const int32 as_sem_e28_left = as_sem_s4;
		const uint32 as_sem_e28_right = AngelscriptTypedASTJIT::ConvertInteger<uint32>(int32(1));
		return AngelscriptTypedASTJIT::ShiftLeft<int32>(as_sem_e28_left, as_sem_e28_right);
	}());
	TypedFrame.SetSourcePosition(67, 6);
	const int32 as_sem_s10 = ([&]() -> int32
	{
		const int32 as_sem_e32_left = as_sem_s4;
		const uint32 as_sem_e32_right = AngelscriptTypedASTJIT::ConvertInteger<uint32>(int32(1));
		return AngelscriptTypedASTJIT::LogicalShiftRight<int32>(as_sem_e32_left, as_sem_e32_right);
	}());
	TypedFrame.SetSourcePosition(69, 7);
	const bool as_sem_s11 = (as_sem_s2 && ([&]() -> bool
	{
		const int32 as_sem_e37_left = ([&]() -> int32
	{
		int32& as_sem_e35_target = as_sem_s4;
		as_sem_e35_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e35_target, int32(1));
		return as_sem_e35_target;
	}());
		const int32 as_sem_e37_right = as_sem_s1;
		return (as_sem_e37_left > as_sem_e37_right);
	}()));
	TypedFrame.SetSourcePosition(70, 7);
	const bool as_sem_s12 = ((as_sem_s11 && as_sem_s3) && ([&]() -> bool
	{
		TypedFrame.SetSourcePosition(71, 35);
		return IsRunningCommandlet();
	}()));
	TypedFrame.SetSourcePosition(72, 2);
	if (as_sem_s12)
	{
		TypedFrame.SetSourcePosition(74, 3);
		return ([&]() -> int32
		{
			const int32 as_sem_e49_left = ([&]() -> int32
		{
			const int32 as_sem_e47_left = as_sem_s9;
			const int32 as_sem_e47_right = as_sem_s8;
			return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e47_left, as_sem_e47_right);
		}());
			const int32 as_sem_e49_right = as_sem_s4;
			return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e49_left, as_sem_e49_right);
		}());
	}
	TypedFrame.SetSourcePosition(76, 2);
	if (([&]() -> bool
	{
		const int32 as_sem_e52_left = as_sem_s10;
		const int32 as_sem_e52_right = int32(10);
		return (as_sem_e52_left > as_sem_e52_right);
	}()))
	{
		TypedFrame.SetSourcePosition(78, 3);
		return ([&]() -> int32
		{
			const int32 as_sem_e57_left = ([&]() -> int32
		{
			const int32 as_sem_e55_left = as_sem_s10;
			const int32 as_sem_e55_right = int32(2);
			return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_sem_e55_left, as_sem_e55_right);
		}());
			const int32 as_sem_e57_right = as_sem_s4;
			return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e57_left, as_sem_e57_right);
		}());
	}
	TypedFrame.SetSourcePosition(80, 2);
	return ([&]() -> int32
	{
		const int32 as_sem_e62_left = ([&]() -> int32
	{
		const int32 as_sem_e60_left = as_sem_s7;
		const int32 as_sem_e60_right = int32(1);
		return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_sem_e60_left, as_sem_e60_right);
	}());
		const int32 as_sem_e62_right = as_sem_s4;
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e62_left, as_sem_e62_right);
	}());
}

// AS Function : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:50:1
// JIT Entry   : Raw
int32 ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_f7ee08ffb0af7fd6427bebd4c9dfc21fb9ec36a2a04aabc61d7c2a60a1097872_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1, asBYTE p_2, asBYTE p_3)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITAotFixture.as",
		"int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)",
		50,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_f7ee08ffb0af7fd6427bebd4c9dfc21fb9ec36a2a04aabc61d7c2a60a1097872_TypedBody(TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1), static_cast<bool>(p_2), static_cast<bool>(p_3));
}

// AS Function : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:50:1
// JIT Entry   : VM
void ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_f7ee08ffb0af7fd6427bebd4c9dfc21fb9ec36a2a04aabc61d7c2a60a1097872_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_f7ee08ffb0af7fd6427bebd4c9dfc21fb9ec36a2a04aabc61d7c2a60a1097872_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1),
		*(asBYTE*)(l_fp + 2),
		*(asBYTE*)(l_fp + 3));
}

// AS Function : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:50:1
// JIT Entry   : Parms
void ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_f7ee08ffb0af7fd6427bebd4c9dfc21fb9ec36a2a04aabc61d7c2a60a1097872_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	static_cast<void>(Object);
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(bool));
	const SIZE_T ParmOffset_2 = ParmsOffset;
	ParmsOffset += sizeof(bool);
	ParmsOffset = Align(ParmsOffset, alignof(bool));
	const SIZE_T ParmOffset_3 = ParmsOffset;
	ParmsOffset += sizeof(bool);
	ParmsOffset = Align(ParmsOffset, alignof(void*));
	const SIZE_T GeneratedWorldContextParmOffset = ParmsOffset;
	ParmsOffset += sizeof(void*);
	static_cast<void>(GeneratedWorldContextParmOffset);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_f7ee08ffb0af7fd6427bebd4c9dfc21fb9ec36a2a04aabc61d7c2a60a1097872_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)),
		static_cast<asBYTE>(*(bool*)(((SIZE_T)Parms) + ParmOffset_2)),
		static_cast<asBYTE>(*(bool*)(((SIZE_T)Parms) + ParmOffset_3)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int64 DoubleToInt64ForAOT(const float)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:88:1
 * StableFunctionKey  : a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6
 * ExecutionHash      : 1139879851ec9364c88ba97c5f45668c3573553687be5d11c833c21e62a3094c
 * DebugHash          : ea434923de3f1b88da9b0cc7773ec00f75f8e6ae99c0983ff8346132d3c8fe4a
 * EntryAbiHash       : a1d1604b170c8afa6d8aebaea217a68d24d4f67ff1cce99952cd0a22877c25f4
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_1139879851ec9364c88ba97c5f45668c3573553687be5d11c833c21e62a3094c = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_1139879851ec9364c88ba97c5f45668c3573553687be5d11c833c21e62a3094c
#endif

// AS Function : int64 DoubleToInt64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:88:1
// JIT Entry   : Raw
int64 ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_1139879851ec9364c88ba97c5f45668c3573553687be5d11c833c21e62a3094c(FScriptExecution& Execution, double p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int64 DoubleToInt64ForAOT(float)", 90);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asQWORD v_TEMP_qword_2 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// dTOi64 v2, v0
((int64&)v_TEMP_qword_2) = int64(p_Value);
// CpyVtoR8 v2
l_valueRegister = v_TEMP_qword_2;
// RET 2
  return (int64)l_valueRegister;
}
// AS Function : int64 DoubleToInt64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:88:1
// JIT Entry   : VM
void ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_1139879851ec9364c88ba97c5f45668c3573553687be5d11c833c21e62a3094c_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int64*)l_outValue = ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_1139879851ec9364c88ba97c5f45668c3573553687be5d11c833c21e62a3094c(Execution,
		*(double*)(l_fp + 0));
}
// AS Function : int64 DoubleToInt64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:88:1
// JIT Entry   : Parms
void ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_1139879851ec9364c88ba97c5f45668c3573553687be5d11c833c21e62a3094c_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(int64));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int64*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_1139879851ec9364c88ba97c5f45668c3573553687be5d11c833c21e62a3094c(Execution,
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : uint64 DoubleToUint64ForAOT(const float)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:175:2
 * StableFunctionKey  : aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb
 * ExecutionHash      : e8872a3f3978b619f364a8c7013e3ff22283632f0fa56397688987ca97239a05
 * DebugHash          : a3fa1e7709a3abc01ff26c871c4cf7e49fa5246cef4df1bdc119260b2eda72b1
 * EntryAbiHash       : 5e708ac1a7afdc6a24646cbb3780e38d2f6639308f316ff6b2ba697920d65899
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_e8872a3f3978b619f364a8c7013e3ff22283632f0fa56397688987ca97239a05 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_e8872a3f3978b619f364a8c7013e3ff22283632f0fa56397688987ca97239a05
#endif

// AS Function : uint64 DoubleToUint64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:175:2
// JIT Entry   : Raw
uint64 ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_e8872a3f3978b619f364a8c7013e3ff22283632f0fa56397688987ca97239a05(FScriptExecution& Execution, UObject* l_This, double p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("uint64 UStaticJITAotFunctionCarrier::DoubleToUint64ForAOT(float)", 177);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asQWORD v_TEMP_qword_2 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// dTOu64 v2, v-2
v_TEMP_qword_2 = uint64(p_Value);
// CpyVtoR8 v2
l_valueRegister = v_TEMP_qword_2;
// RET 4
  return (uint64)l_valueRegister;
}
// AS Function : uint64 DoubleToUint64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:175:2
// JIT Entry   : VM
void ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_e8872a3f3978b619f364a8c7013e3ff22283632f0fa56397688987ca97239a05_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(uint64*)l_outValue = ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_e8872a3f3978b619f364a8c7013e3ff22283632f0fa56397688987ca97239a05(Execution,
		*(UObject**)l_fp,
		*(double*)(l_fp + 2));
}
// AS Function : uint64 DoubleToUint64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:175:2
// JIT Entry   : Parms
void ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_e8872a3f3978b619f364a8c7013e3ff22283632f0fa56397688987ca97239a05_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(uint64));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(uint64*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_e8872a3f3978b619f364a8c7013e3ff22283632f0fa56397688987ca97239a05(Execution,
		(UObject*)Object,
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ReturnPrimitive()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:245:2
 * StableFunctionKey  : b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367
 * ExecutionHash      : 55a527ed5db1a4ef9e97408837341ff2dec3c63487f911516686b4396a516004
 * DebugHash          : 0db6e626c0b79847658f77dc974bf6af359b70fff51a244c175a16e361542d17
 * EntryAbiHash       : 020eef17e1be8204de01770a557fc8e26ac1d841e9e33088abf1b7cb9f3456b4
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_55a527ed5db1a4ef9e97408837341ff2dec3c63487f911516686b4396a516004 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_55a527ed5db1a4ef9e97408837341ff2dec3c63487f911516686b4396a516004
#endif

// AS Function : int ReturnPrimitive()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:245:2
// JIT Entry   : Raw
int32 ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_55a527ed5db1a4ef9e97408837341ff2dec3c63487f911516686b4396a516004(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::ReturnPrimitive()", 247);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v1, 61
v_TEMP_dword_1 = 0x3du;
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int ReturnPrimitive()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:245:2
// JIT Entry   : VM
void ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_55a527ed5db1a4ef9e97408837341ff2dec3c63487f911516686b4396a516004_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_55a527ed5db1a4ef9e97408837341ff2dec3c63487f911516686b4396a516004(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ReturnPrimitive()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:245:2
// JIT Entry   : Parms
void ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_55a527ed5db1a4ef9e97408837341ff2dec3c63487f911516686b4396a516004_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_55a527ed5db1a4ef9e97408837341ff2dec3c63487f911516686b4396a516004(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ReferenceEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:203:2
 * StableFunctionKey  : bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726
 * ExecutionHash      : da38e64d8c8f99763e5ab028d7619561e42eafd63cd1855f7b3fd97328ac6cba
 * DebugHash          : bfa0fbd10d4779bad7016de2dc0306c3a0c4b250af4de1128774092bac8e534d
 * EntryAbiHash       : eb88e947baa4860face1585fa0d6b66fc0a33ee288c7ad728b2271e86a77d1e6
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_da38e64d8c8f99763e5ab028d7619561e42eafd63cd1855f7b3fd97328ac6cba = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_da38e64d8c8f99763e5ab028d7619561e42eafd63cd1855f7b3fd97328ac6cba
#endif

// AS Function : int ReferenceEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:203:2
// JIT Entry   : Raw
int32 ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_da38e64d8c8f99763e5ab028d7619561e42eafd63cd1855f7b3fd97328ac6cba(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::ReferenceEntryForAOT()", 205);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[16];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_Value = {};
asDWORD v_Returned = {};
asDWORD v_TEMP_dword_3 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v1, 7
v_Value = 0x7u;
// SUSPEND
// JitEntry *
// PSF v1
// PshVPtr v0
// CALLINTF
// int UStaticJITAotFunctionCarrier::BumpReferenceForAOT(int&inout Value)
SCRIPT_DEBUG_CALLSTACK_POSITION(206, 3);
{
  asCScriptFunction* CallFunction = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 1u);
  FStaticJITCurrentFunctionCall CurrentFunctionCall(Execution, FAngelscriptStableFunctionKey{{FAngelscriptHash256{{FBlake3Hash(FWideStringView(TEXT("5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65")))}}}}, CallFunction, (void*)((l_This)), true);
  CallFunction = CurrentFunctionCall.Get();
  if (CallFunction == nullptr) [[unlikely]]
  {
     SCRIPT_UNBOUND_EXCEPTION();
return {};
  }
  FAngelscriptContext CallContext(CallFunction->GetEngine());
  CallContext->Prepare(CallFunction);
  CallContext->SetObject((void*)(l_This));
  CallContext->SetArgAddress(0, (void*)((&v_Value)));
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
  l_valueRegister = CallContext->m_regs.valueRegister;
}
// JitEntry *
// CpyRtoV4 v2
memcpy((&v_Returned), &l_valueRegister, 4);
// SUSPEND
// JitEntry *
// MULIi v3, v1, 100
((int32&)v_TEMP_dword_3) = ((int32&)v_Value) * value_as<int>((asDWORD)0x64u);
// ADDi v3, v3, v2
((int32&)v_TEMP_dword_3) = ((int32&)v_TEMP_dword_3) + ((int32&)v_Returned);
// CpyVtoR4 v3
l_dwordRegister = v_TEMP_dword_3;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int ReferenceEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:203:2
// JIT Entry   : VM
void ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_da38e64d8c8f99763e5ab028d7619561e42eafd63cd1855f7b3fd97328ac6cba_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_da38e64d8c8f99763e5ab028d7619561e42eafd63cd1855f7b3fd97328ac6cba(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ReferenceEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:203:2
// JIT Entry   : Parms
void ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_da38e64d8c8f99763e5ab028d7619561e42eafd63cd1855f7b3fd97328ac6cba_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_da38e64d8c8f99763e5ab028d7619561e42eafd63cd1855f7b3fd97328ac6cba(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UStaticJITAotFunctionCarrier()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:155:0
 * StableFunctionKey  : c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5
 * ExecutionHash      : 51381d117be50d31068f645cd70db749143ad7a4aaef927b0b010ad6fbee923e
 * DebugHash          : e99c1b7c1598b7ed5632a3ec913c738a97522a396a8ed5610511320c8b6818b1
 * EntryAbiHash       : fbabfad5e9e11fb97c083af33dc177e642e8c4e4d0b3a67f937cb34fa1ead35a
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_51381d117be50d31068f645cd70db749143ad7a4aaef927b0b010ad6fbee923e = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_51381d117be50d31068f645cd70db749143ad7a4aaef927b0b010ad6fbee923e
#endif

// AS Function : UStaticJITAotFunctionCarrier()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:155:0
// JIT Entry   : Raw
void ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_51381d117be50d31068f645cd70db749143ad7a4aaef927b0b010ad6fbee923e(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("UStaticJITAotFunctionCarrier::UStaticJITAotFunctionCarrier()", 0);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[8];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
// JitEntry *
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
// AS Function : UStaticJITAotFunctionCarrier()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:155:0
// JIT Entry   : VM
void ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_51381d117be50d31068f645cd70db749143ad7a4aaef927b0b010ad6fbee923e_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_51381d117be50d31068f645cd70db749143ad7a4aaef927b0b010ad6fbee923e(Execution,
		*(UObject**)l_fp);
}
// AS Function : UStaticJITAotFunctionCarrier()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:155:0
// JIT Entry   : Parms
void ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_51381d117be50d31068f645cd70db749143ad7a4aaef927b0b010ad6fbee923e_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_51381d117be50d31068f645cd70db749143ad7a4aaef927b0b010ad6fbee923e(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UStaticJITAotVirtualParent UStaticJITAotVirtualParent()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:293:0
 * StableFunctionKey  : dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844
 * ExecutionHash      : ba6d296ea9a313ef441f8ef0185fb102be4ae81d8554a1bff61480bf2ac8043b
 * DebugHash          : e99c1b7c1598b7ed5632a3ec913c738a97522a396a8ed5610511320c8b6818b1
 * EntryAbiHash       : 75e09447a98126f68ca8f0632eb507b4066408116c83b331b7f0978c55324dfc
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844_ba6d296ea9a313ef441f8ef0185fb102be4ae81d8554a1bff61480bf2ac8043b = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844_ba6d296ea9a313ef441f8ef0185fb102be4ae81d8554a1bff61480bf2ac8043b
#endif

// AS Function : UStaticJITAotVirtualParent UStaticJITAotVirtualParent()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:293:0
// JIT Entry   : Raw
UObject* ASJIT_dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844_ba6d296ea9a313ef441f8ef0185fb102be4ae81d8554a1bff61480bf2ac8043b(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("UStaticJITAotVirtualParent UStaticJITAotVirtualParent()", 0);
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
// UStaticJITAotVirtualParent::UStaticJITAotVirtualParent()
SCRIPT_DEBUG_CALLSTACK_POSITION(0, 0);
{
  asCScriptFunction* CallFunction = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 1u);
  FStaticJITCurrentFunctionCall CurrentFunctionCall(Execution, FAngelscriptStableFunctionKey{{FAngelscriptHash256{{FBlake3Hash(FWideStringView(TEXT("f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef")))}}}}, CallFunction, (void*)(((asQWORD&)l_stack[0])), true);
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
// AS Function : UStaticJITAotVirtualParent UStaticJITAotVirtualParent()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:293:0
// JIT Entry   : VM
void ASJIT_dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844_ba6d296ea9a313ef441f8ef0185fb102be4ae81d8554a1bff61480bf2ac8043b_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(UObject**)l_outValue = ASJIT_dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844_ba6d296ea9a313ef441f8ef0185fb102be4ae81d8554a1bff61480bf2ac8043b(Execution);
}
// AS Function : UStaticJITAotVirtualParent UStaticJITAotVirtualParent()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:293:0
// JIT Entry   : Parms
void ASJIT_dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844_ba6d296ea9a313ef441f8ef0185fb102be4ae81d8554a1bff61480bf2ac8043b_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(UObject**)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844_ba6d296ea9a313ef441f8ef0185fb102be4ae81d8554a1bff61480bf2ac8043b(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ExceptionEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:109:1
 * StableFunctionKey  : e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7
 * ExecutionHash      : 8ae3aa1c4fdc8fcec04ac3db1312bb42a62728e9d6db911ef21f63abd00c8ac2
 * DebugHash          : b50aa2eaacd265f8166d7c18ef45a5edd6d017c247c616a03cd5289480d3f1da
 * EntryAbiHash       : 8e204c65d868a3511f949a72a8040d1e58f2c414351b97e6ddef4ad8bc9d5b08
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_8ae3aa1c4fdc8fcec04ac3db1312bb42a62728e9d6db911ef21f63abd00c8ac2 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_8ae3aa1c4fdc8fcec04ac3db1312bb42a62728e9d6db911ef21f63abd00c8ac2
#endif

// AS Function : int ExceptionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:109:1
// JIT Entry   : Raw
int32 ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_8ae3aa1c4fdc8fcec04ac3db1312bb42a62728e9d6db911ef21f63abd00c8ac2(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int ExceptionEntryForAOT()", 111);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// CALL
// void FailForAOT()
SCRIPT_DEBUG_CALLSTACK_POSITION(111, 2);
{
  asCScriptFunction* CallFunction = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 0u);
  FStaticJITCurrentFunctionCall CurrentFunctionCall(Execution, FAngelscriptStableFunctionKey{{FAngelscriptHash256{{FBlake3Hash(FWideStringView(TEXT("34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f")))}}}}, CallFunction, nullptr, false);
  CallFunction = CurrentFunctionCall.Get();
  if (CallFunction == nullptr) [[unlikely]]
  {
     SCRIPT_UNBOUND_EXCEPTION();
return {};
  }
  FAngelscriptContext CallContext(CallFunction->GetEngine());
  CallContext->Prepare(CallFunction);
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
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v1, 99
v_TEMP_dword_1 = 0x63u;
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 0
  return (int32)l_dwordRegister;
}
// AS Function : int ExceptionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:109:1
// JIT Entry   : VM
void ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_8ae3aa1c4fdc8fcec04ac3db1312bb42a62728e9d6db911ef21f63abd00c8ac2_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_8ae3aa1c4fdc8fcec04ac3db1312bb42a62728e9d6db911ef21f63abd00c8ac2(Execution);
}
// AS Function : int ExceptionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:109:1
// JIT Entry   : Parms
void ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_8ae3aa1c4fdc8fcec04ac3db1312bb42a62728e9d6db911ef21f63abd00c8ac2_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_8ae3aa1c4fdc8fcec04ac3db1312bb42a62728e9d6db911ef21f63abd00c8ac2(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void ClientStoreRpcValue_Implementation(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:284:2
 * StableFunctionKey  : edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72
 * ExecutionHash      : 115485552d69f37b25a471131db7b58cb5db52564aea662922e73474132c6d65
 * DebugHash          : 69fd3990f4490e69daccd321fe8b030ae5d245be870bd613f84d1e867dd081bb
 * EntryAbiHash       : 66bc09dfe9cf807bdbf1d43484d609be4abf09f8f99f244b39083673e669630d
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72_115485552d69f37b25a471131db7b58cb5db52564aea662922e73474132c6d65 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72_115485552d69f37b25a471131db7b58cb5db52564aea662922e73474132c6d65
#endif

// AS Function : void ClientStoreRpcValue_Implementation(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:284:2
// JIT Entry   : Raw
void ASJIT_edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72_115485552d69f37b25a471131db7b58cb5db52564aea662922e73474132c6d65(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("void UStaticJITAotFunctionCarrier::ClientStoreRpcValue_Implementation(int)", 286);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
// JitEntry *
// SUSPEND
// JitEntry *
// LoadThisR +48
l_valueRegister = ((asQWORD)l_This) + POFFSET_UStaticJITAotFunctionCarrier_StoredValue;
// WRTV4 v-2
memcpy((void*)l_valueRegister, (void*)(&p_Value), 4);
// SUSPEND
// JitEntry *
// RET 3
  return;
}
// AS Function : void ClientStoreRpcValue_Implementation(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:284:2
// JIT Entry   : VM
void ASJIT_edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72_115485552d69f37b25a471131db7b58cb5db52564aea662922e73474132c6d65_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72_115485552d69f37b25a471131db7b58cb5db52564aea662922e73474132c6d65(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : void ClientStoreRpcValue_Implementation(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:284:2
// JIT Entry   : Parms
void ASJIT_edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72_115485552d69f37b25a471131db7b58cb5db52564aea662922e73474132c6d65_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ASJIT_edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72_115485552d69f37b25a471131db7b58cb5db52564aea662922e73474132c6d65(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int RecurseForAOT(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:135:1
 * StableFunctionKey  : f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556
 * ExecutionHash      : 5aa1b5a0852c6ecb240e1926879919b0136db143d20d6c93f65de9c8ba52c402
 * DebugHash          : 956f2dc65bed6ce18d3f418905fc2879f13d47cd85b55299892711b6eb88c205
 * EntryAbiHash       : 805d2b5d622dfc1d79e7a6e1f19c0e4068732e3afa951e3f3b429e924ed5af85
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_5aa1b5a0852c6ecb240e1926879919b0136db143d20d6c93f65de9c8ba52c402 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_5aa1b5a0852c6ecb240e1926879919b0136db143d20d6c93f65de9c8ba52c402
#endif

// AS Function : int RecurseForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:135:1
// JIT Entry   : Raw
int32 ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_5aa1b5a0852c6ecb240e1926879919b0136db143d20d6c93f65de9c8ba52c402(FScriptExecution& Execution, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int RecurseForAOT(int)", 137);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[4];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
asBYTE v_TEMP_byte_2 = {};
asDWORD v_TEMP_dword_3 = {};
asDWORD v_TEMP_dword_4 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// CMPIi v0, 0
  if (((int32&)p_Value) == value_as<int>((asDWORD)0x0u))     l_byteRegister = 0;
  else if (((int32&)p_Value) < value_as<int>((asDWORD)0x0u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JNZ 13
if(l_byteRegister != 0) {
goto LABEL_RecurseForAOT_24;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v1, 0
v_TEMP_dword_1 = 0x0u;
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// JMP 20
goto LABEL_RecurseForAOT_44;
LABEL_RecurseForAOT_24:
// SUSPEND
// JitEntry *
// SUBIi v1, v0, 1
((int32&)v_TEMP_dword_1) = ((int32&)p_Value) - value_as<int>((asDWORD)0x1u);
// PshV4 v1
// CALL
// int RecurseForAOT(const int Value)
SCRIPT_DEBUG_CALLSTACK_POSITION(141, 2);
{
  asCScriptFunction* CallFunction = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 0u);
  FStaticJITCurrentFunctionCall CurrentFunctionCall(Execution, FAngelscriptStableFunctionKey{{FAngelscriptHash256{{FBlake3Hash(FWideStringView(TEXT("f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556")))}}}}, CallFunction, nullptr, false);
  CallFunction = CurrentFunctionCall.Get();
  if (CallFunction == nullptr) [[unlikely]]
  {
     SCRIPT_UNBOUND_EXCEPTION();
return {};
  }
  FAngelscriptContext CallContext(CallFunction->GetEngine());
  CallContext->Prepare(CallFunction);
  CallContext->SetArgDWord(0, value_as<asDWORD>((v_TEMP_dword_1)));
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
  l_valueRegister = CallContext->m_regs.valueRegister;
}
// JitEntry *
// CpyRtoV4 v3
memcpy((&v_TEMP_dword_3), &l_valueRegister, 4);
// MULIi v1, v3, 10
((int32&)v_TEMP_dword_1) = ((int32&)v_TEMP_dword_3) * value_as<int>((asDWORD)0xau);
// ADDi v4, v0, v1
((int32&)v_TEMP_dword_4) = ((int32&)p_Value) + ((int32&)v_TEMP_dword_1);
// CpyVtoR4 v4
l_dwordRegister = v_TEMP_dword_4;
LABEL_RecurseForAOT_44:
// RET 1
  return (int32)l_dwordRegister;
}
// AS Function : int RecurseForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:135:1
// JIT Entry   : VM
void ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_5aa1b5a0852c6ecb240e1926879919b0136db143d20d6c93f65de9c8ba52c402_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_5aa1b5a0852c6ecb240e1926879919b0136db143d20d6c93f65de9c8ba52c402(Execution,
		*(asDWORD*)(l_fp + 0));
}
// AS Function : int RecurseForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:135:1
// JIT Entry   : Parms
void ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_5aa1b5a0852c6ecb240e1926879919b0136db143d20d6c93f65de9c8ba52c402_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_5aa1b5a0852c6ecb240e1926879919b0136db143d20d6c93f65de9c8ba52c402(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ObjectLastNativeForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:98:1
 * StableFunctionKey  : f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df
 * ExecutionHash      : dd5551c67767322a161767993cc5b559e3c62f067170cfc0e60df8016f3650e4
 * DebugHash          : dd4a552510b187e587eb45a47ffb491e2da85024ed0be2193692dc4e647bd811
 * EntryAbiHash       : 37241b3ba70d4b0f0af53358d5dcd4fda67f64c6db1715fe217a46674e3ba526
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_dd5551c67767322a161767993cc5b559e3c62f067170cfc0e60df8016f3650e4 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_dd5551c67767322a161767993cc5b559e3c62f067170cfc0e60df8016f3650e4
#endif

// AS Function : int ObjectLastNativeForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:98:1
// JIT Entry   : Raw
int32 ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_dd5551c67767322a161767993cc5b559e3c62f067170cfc0e60df8016f3650e4(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int ObjectLastNativeForAOT()", 100);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[16];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
alignas(8) asBYTE MEM_v_Value[4];
FUnknownValueType& v_Value = (FUnknownValueType&)MEM_v_Value[0];
asDWORD v_TEMP_dword_3 = {};
asDWORD v_TEMP_dword_4 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// PshC4 97
// PshC4 39
// PSF v2
// CALLSYS *
// FAotObjectLastProbe::FAotObjectLastProbe(int Left, int Right)
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 0u).GetFunction();
  auto CastedFuncPtr = (void(*)(int32,int32,void*))RawFuncPtr;
  void* Object = (void*)((&v_Value));
SCRIPT_DEBUG_CALLSTACK_POSITION(100, 2);
CastedFuncPtr(value_as<int32>(((asDWORD)0x27u)),value_as<int32>(((asDWORD)0x61u)),Object);
if (Execution.bExceptionThrown) [[unlikely]]
{
return {};
}
}
// JitEntry *
// SUSPEND
// JitEntry *
// LoadVObjR
l_valueRegister = (asQWORD)((&v_Value)) + (short)0;
// RDR4 v3
v_TEMP_dword_3 = value_read<asDWORD>((void*)l_valueRegister);
// CpyVtoR4 v3
l_dwordRegister = v_TEMP_dword_3;
// RET 0
  return (int32)l_dwordRegister;
}
// AS Function : int ObjectLastNativeForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:98:1
// JIT Entry   : VM
void ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_dd5551c67767322a161767993cc5b559e3c62f067170cfc0e60df8016f3650e4_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_dd5551c67767322a161767993cc5b559e3c62f067170cfc0e60df8016f3650e4(Execution);
}
// AS Function : int ObjectLastNativeForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:98:1
// JIT Entry   : Parms
void ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_dd5551c67767322a161767993cc5b559e3c62f067170cfc0e60df8016f3650e4_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_dd5551c67767322a161767993cc5b559e3c62f067170cfc0e60df8016f3650e4(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int Entry()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:165:3
 * StableFunctionKey  : f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9
 * ExecutionHash      : 818820abe70c5eec2aeba72ed9ac354275077d2745ecc7386c49e318a16f91bb
 * DebugHash          : 3190280cdaac712d752b1c66c6eed16f707291b58228bff84b081a315a50e0a3
 * EntryAbiHash       : db0c8f3058412e5bdc683b188ff132b140d00caf79e3e0658bda0254082fdee0
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_818820abe70c5eec2aeba72ed9ac354275077d2745ecc7386c49e318a16f91bb = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_818820abe70c5eec2aeba72ed9ac354275077d2745ecc7386c49e318a16f91bb
#endif

// AS Function : int Entry()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:165:3
// JIT Entry   : Raw
int32 ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_818820abe70c5eec2aeba72ed9ac354275077d2745ecc7386c49e318a16f91bb(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::Entry()", 167);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[12];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
asDWORD v_TEMP_dword_2 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// PshC4 35
// PshVPtr v0
// CALLINTF
// int UStaticJITAotFunctionCarrier::AddForAOT(const int Value)
SCRIPT_DEBUG_CALLSTACK_POSITION(167, 3);
{
  asCScriptFunction* CallFunction = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 1u);
  FStaticJITCurrentFunctionCall CurrentFunctionCall(Execution, FAngelscriptStableFunctionKey{{FAngelscriptHash256{{FBlake3Hash(FWideStringView(TEXT("2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5")))}}}}, CallFunction, (void*)((l_This)), true);
  CallFunction = CurrentFunctionCall.Get();
  if (CallFunction == nullptr) [[unlikely]]
  {
     SCRIPT_UNBOUND_EXCEPTION();
return {};
  }
  FAngelscriptContext CallContext(CallFunction->GetEngine());
  CallContext->Prepare(CallFunction);
  CallContext->SetObject((void*)(l_This));
  CallContext->SetArgDWord(0, value_as<asDWORD>(((asDWORD)0x23u)));
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
  l_valueRegister = CallContext->m_regs.valueRegister;
}
// JitEntry *
// CpyRtoV4 v2
memcpy((&v_TEMP_dword_2), &l_valueRegister, 4);
// CpyVtoR4 v2
l_dwordRegister = v_TEMP_dword_2;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int Entry()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:165:3
// JIT Entry   : VM
void ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_818820abe70c5eec2aeba72ed9ac354275077d2745ecc7386c49e318a16f91bb_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_818820abe70c5eec2aeba72ed9ac354275077d2745ecc7386c49e318a16f91bb(Execution,
		*(UObject**)l_fp);
}
// AS Function : int Entry()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:165:3
// JIT Entry   : Parms
void ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_818820abe70c5eec2aeba72ed9ac354275077d2745ecc7386c49e318a16f91bb_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_818820abe70c5eec2aeba72ed9ac354275077d2745ecc7386c49e318a16f91bb(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticScalarBranch(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:8:1
 * StableFunctionKey  : f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e
 * ExecutionHash      : 22b3a11e5998369c4c01cb1cfdcd7404a0e6c1e56b461f121a2954a16debaf40
 * DebugHash          : ad4bdd8d752118754bff8ee15fb7e2f21f7d800fcd5a978dff48b5897d47737d
 * EntryAbiHash       : 108ad1ef8d5103b1a80d679116c872a1dc9d3ee674beaa1115ec65b288287e69
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e_22b3a11e5998369c4c01cb1cfdcd7404a0e6c1e56b461f121a2954a16debaf40 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e_22b3a11e5998369c4c01cb1cfdcd7404a0e6c1e56b461f121a2954a16debaf40
#endif

// AS Function : int SemanticScalarBranch(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:8:1
// JIT Entry   : Raw
int32 ASJIT_f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e_22b3a11e5998369c4c01cb1cfdcd7404a0e6c1e56b461f121a2954a16debaf40(FScriptExecution& Execution, asDWORD p_A, asDWORD p_B)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int SemanticScalarBranch(int, int)", 10);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_Sum = {};
asDWORD v_TEMP_dword_1 = {};
asBYTE v_TEMP_byte_3 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// ADDi v2, v0, v-1
((int32&)v_Sum) = ((int32&)p_A) + ((int32&)p_B);
// SUSPEND
// JitEntry *
// CMPIi v2, 10
  if (((int32&)v_Sum) == value_as<int>((asDWORD)0xau))     l_byteRegister = 0;
  else if (((int32&)v_Sum) < value_as<int>((asDWORD)0xau)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JNP 14
if(((int8&)l_byteRegister) <= 0) {
goto LABEL_SemanticScalarBranch_31;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// MULIi v1, v2, 2
((int32&)v_TEMP_dword_1) = ((int32&)v_Sum) * value_as<int>((asDWORD)0x2u);
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// JMP 8
goto LABEL_SemanticScalarBranch_39;
LABEL_SemanticScalarBranch_31:
// SUSPEND
// JitEntry *
// SUBIi v1, v2, 1
((int32&)v_TEMP_dword_1) = ((int32&)v_Sum) - value_as<int>((asDWORD)0x1u);
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
LABEL_SemanticScalarBranch_39:
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int SemanticScalarBranch(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:8:1
// JIT Entry   : VM
void ASJIT_f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e_22b3a11e5998369c4c01cb1cfdcd7404a0e6c1e56b461f121a2954a16debaf40_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e_22b3a11e5998369c4c01cb1cfdcd7404a0e6c1e56b461f121a2954a16debaf40(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}
// AS Function : int SemanticScalarBranch(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:8:1
// JIT Entry   : Parms
void ASJIT_f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e_22b3a11e5998369c4c01cb1cfdcd7404a0e6c1e56b461f121a2954a16debaf40_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_A = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1_B = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e_22b3a11e5998369c4c01cb1cfdcd7404a0e6c1e56b461f121a2954a16debaf40(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_A),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_1_B));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UStaticJITAotVirtualParent()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:293:0
 * StableFunctionKey  : f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef
 * ExecutionHash      : bbdf063a311ff21aa3fdd76875261a96989372d8d66d7a8034aff45ccf38428e
 * DebugHash          : e99c1b7c1598b7ed5632a3ec913c738a97522a396a8ed5610511320c8b6818b1
 * EntryAbiHash       : 1075e6d88a0fee09d9074b0d2222d8f08c7b158c0d6f762b28f4e154db4d37e2
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_bbdf063a311ff21aa3fdd76875261a96989372d8d66d7a8034aff45ccf38428e = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_bbdf063a311ff21aa3fdd76875261a96989372d8d66d7a8034aff45ccf38428e
#endif

// AS Function : UStaticJITAotVirtualParent()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:293:0
// JIT Entry   : Raw
void ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_bbdf063a311ff21aa3fdd76875261a96989372d8d66d7a8034aff45ccf38428e(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("UStaticJITAotVirtualParent::UStaticJITAotVirtualParent()", 0);
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
// AS Function : UStaticJITAotVirtualParent()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:293:0
// JIT Entry   : VM
void ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_bbdf063a311ff21aa3fdd76875261a96989372d8d66d7a8034aff45ccf38428e_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_bbdf063a311ff21aa3fdd76875261a96989372d8d66d7a8034aff45ccf38428e(Execution,
		*(UObject**)l_fp);
}
// AS Function : UStaticJITAotVirtualParent()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:293:0
// JIT Entry   : Parms
void ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_bbdf063a311ff21aa3fdd76875261a96989372d8d66d7a8034aff45ccf38428e_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_bbdf063a311ff21aa3fdd76875261a96989372d8d66d7a8034aff45ccf38428e(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int BytecodeFallbackGeneratedWorldContext(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:317:1
 * StableFunctionKey  : f509813c02da6025acae1af26ea86c8f9e3130851803399d0b2db25a166e56ae
 * ExecutionHash      : ac3d6db70d8eda0b2d0022704b5000e941f97d62310effe493dd2782fd442533
 * DebugHash          : 805704ff3ed9cc038b3431281557fc7e8d3bc09bd6c221a96a20c98fc1c9e06f
 * EntryAbiHash       : d9d9e18805e2fe81f0ff248a92bd9d347eefb34eb7a9fd3b93213e2e4ea0cbbc
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f509813c02da6025acae1af26ea86c8f9e3130851803399d0b2db25a166e56ae_ac3d6db70d8eda0b2d0022704b5000e941f97d62310effe493dd2782fd442533 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f509813c02da6025acae1af26ea86c8f9e3130851803399d0b2db25a166e56ae_ac3d6db70d8eda0b2d0022704b5000e941f97d62310effe493dd2782fd442533
#endif

// AS Function : int BytecodeFallbackGeneratedWorldContext(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:317:1
// JIT Entry   : Raw
int32 ASJIT_f509813c02da6025acae1af26ea86c8f9e3130851803399d0b2db25a166e56ae_ac3d6db70d8eda0b2d0022704b5000e941f97d62310effe493dd2782fd442533(FScriptExecution& Execution, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int BytecodeFallbackGeneratedWorldContext(int)", 319);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[16];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
alignas(8) asBYTE MEM_v_Marker[sizeof(FString)];
FString& v_Marker = (FString&)MEM_v_Marker[0];
asDWORD v_TEMP_dword_5 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PSF v4
// CALLSYS *
// FString::FString(const FString&inout Other)
{
  FString* Object = (FString*)((&v_Marker));
new (Object) FString((*(FString*)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 4u))));
}
// JitEntry *
// SUSPEND
// JitEntry *
// PSF v4
// CALLSYS *
// int FString::Len() const
{
  FString* Object = (FString*)((&v_Marker));
int32 FunctionReturnValue = Object->Len();
l_dwordRegister = (asDWORD&)FunctionReturnValue;
}
// JitEntry *
// CpyRtoV4 v5
v_TEMP_dword_5 = l_dwordRegister;
// ADDi v5, v0, v5
((int32&)v_TEMP_dword_5) = ((int32&)p_Value) + ((int32&)v_TEMP_dword_5);
// PSF v4
// CALLSYS *
// FString::~FString()
{
  FString* Object = (FString*)((&v_Marker));
((FString*)Object)->~FString();
}
// JitEntry *
// CpyVtoR4 v5
l_dwordRegister = v_TEMP_dword_5;
// RET 1
  return (int32)l_dwordRegister;
}
// AS Function : int BytecodeFallbackGeneratedWorldContext(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:317:1
// JIT Entry   : VM
void ASJIT_f509813c02da6025acae1af26ea86c8f9e3130851803399d0b2db25a166e56ae_ac3d6db70d8eda0b2d0022704b5000e941f97d62310effe493dd2782fd442533_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_f509813c02da6025acae1af26ea86c8f9e3130851803399d0b2db25a166e56ae_ac3d6db70d8eda0b2d0022704b5000e941f97d62310effe493dd2782fd442533(Execution,
		*(asDWORD*)(l_fp + 0));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void ResetStoredValue()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:233:2
 * StableFunctionKey  : f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7
 * ExecutionHash      : 31ab68e2d91d0c1e9600288f6f39222c55d3afeace5b10932325a2d3bb666edd
 * DebugHash          : 852ed095a9fff8eab96146813a114e252a9b75fc100a3b195022c872088c5334
 * EntryAbiHash       : 9b0857b686ac6c0536f6c1b33a7f4ce1dbb76c78831fe5af95fb30829626a6c6
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_31ab68e2d91d0c1e9600288f6f39222c55d3afeace5b10932325a2d3bb666edd = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_31ab68e2d91d0c1e9600288f6f39222c55d3afeace5b10932325a2d3bb666edd
#endif

// AS Function : void ResetStoredValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:233:2
// JIT Entry   : Raw
void ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_31ab68e2d91d0c1e9600288f6f39222c55d3afeace5b10932325a2d3bb666edd(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("void UStaticJITAotFunctionCarrier::ResetStoredValue()", 235);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v1, 71
v_TEMP_dword_1 = 0x47u;
// LoadThisR +48
l_valueRegister = ((asQWORD)l_This) + POFFSET_UStaticJITAotFunctionCarrier_StoredValue;
// WRTV4 v1
memcpy((void*)l_valueRegister, (void*)(&v_TEMP_dword_1), 4);
// SUSPEND
// JitEntry *
// RET 2
  return;
}
// AS Function : void ResetStoredValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:233:2
// JIT Entry   : VM
void ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_31ab68e2d91d0c1e9600288f6f39222c55d3afeace5b10932325a2d3bb666edd_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_31ab68e2d91d0c1e9600288f6f39222c55d3afeace5b10932325a2d3bb666edd(Execution,
		*(UObject**)l_fp);
}
// AS Function : void ResetStoredValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:233:2
// JIT Entry   : Parms
void ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_31ab68e2d91d0c1e9600288f6f39222c55d3afeace5b10932325a2d3bb666edd_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_31ab68e2d91d0c1e9600288f6f39222c55d3afeace5b10932325a2d3bb666edd(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int CallVirtualValue()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:300:2
 * StableFunctionKey  : fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce
 * ExecutionHash      : bf6aff981e022f413c315d1e1b0769d78612f114820c8c21b4f50467c94ab465
 * DebugHash          : 0a324441978b6e5f41ed8510b501b43bf1a4ce95189f0760d60238fe217a6966
 * EntryAbiHash       : 6031fb726462dbf41570b41d3d37659d395e860ef3d2bc07067ce4b4bce8b1ad
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_bf6aff981e022f413c315d1e1b0769d78612f114820c8c21b4f50467c94ab465 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_bf6aff981e022f413c315d1e1b0769d78612f114820c8c21b4f50467c94ab465
#endif

// AS Function : int CallVirtualValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:300:2
// JIT Entry   : Raw
int32 ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_bf6aff981e022f413c315d1e1b0769d78612f114820c8c21b4f50467c94ab465(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotVirtualParent::CallVirtualValue()", 302);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[8];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// PshVPtr v0
// CALLINTF
// int UStaticJITAotVirtualParent::GetVirtualValue()
SCRIPT_DEBUG_CALLSTACK_POSITION(302, 3);
{
  asCScriptFunction* CallFunction = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 1u);
  FStaticJITCurrentFunctionCall CurrentFunctionCall(Execution, FAngelscriptStableFunctionKey{{FAngelscriptHash256{{FBlake3Hash(FWideStringView(TEXT("598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c")))}}}}, CallFunction, (void*)((l_This)), true);
  CallFunction = CurrentFunctionCall.Get();
  if (CallFunction == nullptr) [[unlikely]]
  {
     SCRIPT_UNBOUND_EXCEPTION();
return {};
  }
  FAngelscriptContext CallContext(CallFunction->GetEngine());
  CallContext->Prepare(CallFunction);
  CallContext->SetObject((void*)(l_This));
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
  l_valueRegister = CallContext->m_regs.valueRegister;
}
// JitEntry *
// CpyRtoV4 v1
memcpy((&v_TEMP_dword_1), &l_valueRegister, 4);
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int CallVirtualValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:300:2
// JIT Entry   : VM
void ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_bf6aff981e022f413c315d1e1b0769d78612f114820c8c21b4f50467c94ab465_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_bf6aff981e022f413c315d1e1b0769d78612f114820c8c21b4f50467c94ab465(Execution,
		*(UObject**)l_fp);
}
// AS Function : int CallVirtualValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:300:2
// JIT Entry   : Parms
void ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_bf6aff981e022f413c315d1e1b0769d78612f114820c8c21b4f50467c94ab465_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_bf6aff981e022f413c315d1e1b0769d78612f114820c8c21b4f50467c94ab465(Execution,
		(UObject*)Object);
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
