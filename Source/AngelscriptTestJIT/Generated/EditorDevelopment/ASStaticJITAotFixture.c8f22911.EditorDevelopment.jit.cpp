// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Module
 * VirtualSourcePath   : /Angelscript/Game/ASStaticJITAotFixture.as
 * CanonicalModuleName : ASStaticJITAotFixture
 * TargetProfile       : EditorDevelopment
 * StableModuleKey     : c8f2291154afd48cf9d8c267f8bfcf5bff307741bbaa61ea978863ed08f780ed
 * ProviderId          : 7dc70fd55a76cc55d726e32d5c2434713bd93755a32e4dd524a4b7c39c8b5d44
 * ArtifactProfile     : 018cc71ab6612eca7608aa13abe1f7a1a7d81964bb8cc5821f1b1e047fcf7a11
 * NativeEnvironment   : 76d620ec7d0db38aa2028996e595f75523f201e7e560c05d8c9000d60d04cd73
 * FunctionCount       : 56
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
 * ExecutionHash      : bb5a46ffc9705b99a22bf68825e1013b1b053dd3ed3b3c75e3dd2dbf1676d1cf
 * DebugHash          : 9b1c7ff2839dd9f6e345cef53192d08b5bb5d1d65792b637c9960abf08cb4d79
 * EntryAbiHash       : 5e708ac1a7afdc6a24646cbb3780e38d2f6639308f316ff6b2ba697920d65899
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_bb5a46ffc9705b99a22bf68825e1013b1b053dd3ed3b3c75e3dd2dbf1676d1cf = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_bb5a46ffc9705b99a22bf68825e1013b1b053dd3ed3b3c75e3dd2dbf1676d1cf
#endif

// AS Function : uint64 DoubleToUint64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:93:1
// JIT Entry   : Raw
uint64 ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_bb5a46ffc9705b99a22bf68825e1013b1b053dd3ed3b3c75e3dd2dbf1676d1cf(FScriptExecution& Execution, double p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("uint64 DoubleToUint64ForAOT(float)", 94);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asQWORD v_TEMP_qword_2 = {};
asQWORD v_TEMP_qword_4 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// dTOu64 v4, v0
v_TEMP_qword_4 = uint64(p_Value);
// CpyVtoV8 v2, v4
v_TEMP_qword_2 = v_TEMP_qword_4;
// JMP 0
goto LABEL_DoubleToUint64ForAOT_17;
LABEL_DoubleToUint64ForAOT_17:
// CpyVtoR8 v2
l_valueRegister = v_TEMP_qword_2;
// RET 2
  return (uint64)l_valueRegister;
}
// AS Function : uint64 DoubleToUint64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:93:1
// JIT Entry   : VM
void ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_bb5a46ffc9705b99a22bf68825e1013b1b053dd3ed3b3c75e3dd2dbf1676d1cf_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(uint64*)l_outValue = ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_bb5a46ffc9705b99a22bf68825e1013b1b053dd3ed3b3c75e3dd2dbf1676d1cf(Execution,
		*(double*)(l_fp + 0));
}
// AS Function : uint64 DoubleToUint64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:93:1
// JIT Entry   : Parms
void ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_bb5a46ffc9705b99a22bf68825e1013b1b053dd3ed3b3c75e3dd2dbf1676d1cf_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(uint64));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(uint64*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_bb5a46ffc9705b99a22bf68825e1013b1b053dd3ed3b3c75e3dd2dbf1676d1cf(Execution,
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UStaticJITAotVirtualChild UStaticJITAotVirtualChild()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:307:7
 * StableFunctionKey  : 0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe
 * ExecutionHash      : 4371de2089dfe511737f80326467d70f8add6531ba2d1b317fafe9823ad92080
 * DebugHash          : 7fb86588188fe1f9c9ffc5845cdf123e46eb054a502a542b19bf3cf6ee6e1ee6
 * EntryAbiHash       : 1cdfabcf3e8a6c5b336a09064baa4231093ea1b1252679df79c5a50f263c311d
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe_4371de2089dfe511737f80326467d70f8add6531ba2d1b317fafe9823ad92080 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe_4371de2089dfe511737f80326467d70f8add6531ba2d1b317fafe9823ad92080
#endif

// AS Function : UStaticJITAotVirtualChild UStaticJITAotVirtualChild()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:307:7
// JIT Entry   : Raw
UObject* ASJIT_0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe_4371de2089dfe511737f80326467d70f8add6531ba2d1b317fafe9823ad92080(FScriptExecution& Execution)
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
void* v_TEMP_2 = nullptr;
// JitEntry *
// PSF v2
// ALLOC
{
  asCObjectType* objType = (asCObjectType*)FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo(Execution, 0u);
  asDWORD* mem = (asDWORD*)SCRIPT_ENGINE->AllocScriptObject(objType);
  ScriptObject_Construct(objType, (asCScriptObject*)mem);
  asPWORD* a = (asPWORD*)((&v_TEMP_2));
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
l_objectRegister = (void*)v_TEMP_2;
v_TEMP_2 = nullptr;
// RET 0
  return (UObject*)l_objectRegister;
}
// AS Function : UStaticJITAotVirtualChild UStaticJITAotVirtualChild()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:307:7
// JIT Entry   : VM
void ASJIT_0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe_4371de2089dfe511737f80326467d70f8add6531ba2d1b317fafe9823ad92080_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(UObject**)l_outValue = ASJIT_0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe_4371de2089dfe511737f80326467d70f8add6531ba2d1b317fafe9823ad92080(Execution);
}
// AS Function : UStaticJITAotVirtualChild UStaticJITAotVirtualChild()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:307:7
// JIT Entry   : Parms
void ASJIT_0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe_4371de2089dfe511737f80326467d70f8add6531ba2d1b317fafe9823ad92080_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(UObject**)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe_4371de2089dfe511737f80326467d70f8add6531ba2d1b317fafe9823ad92080(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : float32 SemanticPowerFloat(const float32, const float32)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:28:1
 * StableFunctionKey  : 0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a
 * ExecutionHash      : ee77e083b56bdb43726aad0da39c22c823988aadd1cac20e0e7a900a060b7c9d
 * DebugHash          : 7f92c5960bb002846ced7203440122614dc560dd794dd972bdf46b5399ce7fb7
 * EntryAbiHash       : baec52322166f9d9a578e658e1d6db3e7012f171953f1f761c1b35fa1d15862f
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a_ee77e083b56bdb43726aad0da39c22c823988aadd1cac20e0e7a900a060b7c9d = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a_ee77e083b56bdb43726aad0da39c22c823988aadd1cac20e0e7a900a060b7c9d
#endif

// AS Function : float32 SemanticPowerFloat(const float32, const float32)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:28:1
// JIT Entry   : Raw
float ASJIT_0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a_ee77e083b56bdb43726aad0da39c22c823988aadd1cac20e0e7a900a060b7c9d(FScriptExecution& Execution, float p_Base, float p_Exponent)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("float32 SemanticPowerFloat(float32, float32)", 29);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
float v_TEMP_float_1 = {};
float v_TEMP_float_2 = {};
// JitEntry *
// SUSPEND
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
// JMP 0
goto LABEL_SemanticPowerFloat_15;
LABEL_SemanticPowerFloat_15:
// CpyVtoR4 v1
l_floatRegister = v_TEMP_float_1;
// RET 2
  return l_floatRegister;
}
// AS Function : float32 SemanticPowerFloat(const float32, const float32)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:28:1
// JIT Entry   : VM
void ASJIT_0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a_ee77e083b56bdb43726aad0da39c22c823988aadd1cac20e0e7a900a060b7c9d_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(float*)l_outValue = ASJIT_0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a_ee77e083b56bdb43726aad0da39c22c823988aadd1cac20e0e7a900a060b7c9d(Execution,
		*(float*)(l_fp + 0),
		*(float*)(l_fp + 1));
}
// AS Function : float32 SemanticPowerFloat(const float32, const float32)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:28:1
// JIT Entry   : Parms
void ASJIT_0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a_ee77e083b56bdb43726aad0da39c22c823988aadd1cac20e0e7a900a060b7c9d_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Base = ParmsOffset;
	ParmsOffset += sizeof(float);
	ParmsOffset = Align(ParmsOffset, alignof(float));
	const SIZE_T ParmOffset_1_Exponent = ParmsOffset;
	ParmsOffset += sizeof(float);
	ParmsOffset = Align(ParmsOffset, alignof(float));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(float*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a_ee77e083b56bdb43726aad0da39c22c823988aadd1cac20e0e7a900a060b7c9d(Execution,
		(float)*(float*)(((SIZE_T)Parms) + ParmOffset_0_Base),
		(float)*(float*)(((SIZE_T)Parms) + ParmOffset_1_Exponent));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int RecursionEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:227:2
 * StableFunctionKey  : 13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c
 * ExecutionHash      : 362243f98da199c9e35396d33446744ee11967e3a17e9e96f69d781371d195a1
 * DebugHash          : 18a0d3fd065760a479e0946afd5192bef2fcd99faf99c9cd0fb6c74981c0b03a
 * EntryAbiHash       : 9807be95f7b037b5a524626bf011302bd0844098a4a72bbe407d8cd5e21fa5ae
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_362243f98da199c9e35396d33446744ee11967e3a17e9e96f69d781371d195a1 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_362243f98da199c9e35396d33446744ee11967e3a17e9e96f69d781371d195a1
#endif

// AS Function : int RecursionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:227:2
// JIT Entry   : Raw
int32 ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_362243f98da199c9e35396d33446744ee11967e3a17e9e96f69d781371d195a1(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::RecursionEntryForAOT()", 228);
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
asDWORD v_TEMP_dword_3 = {};
// JitEntry *
// SUSPEND
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
// CpyRtoV4 v3
memcpy((&v_TEMP_dword_3), &l_valueRegister, 4);
// CpyVtoV4 v1, v3
v_TEMP_dword_1 = v_TEMP_dword_3;
// JMP 0
goto LABEL_UStaticJITAotFunctionCarrier__RecursionEntryForAOT_24;
LABEL_UStaticJITAotFunctionCarrier__RecursionEntryForAOT_24:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int RecursionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:227:2
// JIT Entry   : VM
void ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_362243f98da199c9e35396d33446744ee11967e3a17e9e96f69d781371d195a1_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_362243f98da199c9e35396d33446744ee11967e3a17e9e96f69d781371d195a1(Execution,
		*(UObject**)l_fp);
}
// AS Function : int RecursionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:227:2
// JIT Entry   : Parms
void ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_362243f98da199c9e35396d33446744ee11967e3a17e9e96f69d781371d195a1_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_362243f98da199c9e35396d33446744ee11967e3a17e9e96f69d781371d195a1(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int GetVirtualValue_Implementation()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:294:3
 * StableFunctionKey  : 191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5
 * ExecutionHash      : 90faa4260e80315a6fa4718cf3891100f0d673af71141cef231336d0ee078330
 * DebugHash          : 0254eaf8107aea0281084657b828536936b182e40262d916be1e152d6861c68b
 * EntryAbiHash       : 507cc902edc1d9bd3e2f2572fe86bb7d02b96a550445798d3f067222eb350188
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_90faa4260e80315a6fa4718cf3891100f0d673af71141cef231336d0ee078330 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_90faa4260e80315a6fa4718cf3891100f0d673af71141cef231336d0ee078330
#endif

// AS Function : int GetVirtualValue_Implementation()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:294:3
// JIT Entry   : Raw
int32 ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_90faa4260e80315a6fa4718cf3891100f0d673af71141cef231336d0ee078330(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotVirtualParent::GetVirtualValue_Implementation()", 295);
SCRIPT_ASSUME_NO_EXCEPTION()
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
// SUSPEND
// JitEntry *
// SetV4 v1, 21
v_TEMP_dword_1 = 0x15u;
// JMP 0
goto LABEL_UStaticJITAotVirtualParent__GetVirtualValue_Implementation_15;
LABEL_UStaticJITAotVirtualParent__GetVirtualValue_Implementation_15:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int GetVirtualValue_Implementation()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:294:3
// JIT Entry   : VM
void ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_90faa4260e80315a6fa4718cf3891100f0d673af71141cef231336d0ee078330_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_90faa4260e80315a6fa4718cf3891100f0d673af71141cef231336d0ee078330(Execution,
		*(UObject**)l_fp);
}
// AS Function : int GetVirtualValue_Implementation()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:294:3
// JIT Entry   : Parms
void ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_90faa4260e80315a6fa4718cf3891100f0d673af71141cef231336d0ee078330_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_90faa4260e80315a6fa4718cf3891100f0d673af71141cef231336d0ee078330(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ObjectLastNativeForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:180:2
 * StableFunctionKey  : 1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f
 * ExecutionHash      : fbcef2458cadf3371f50b782123f794d3135546b2384ccb63812cdb75a2c3f19
 * DebugHash          : 8d328b0ce671c34530643a1fa28f30f15a462bc608edca740680569cfa44dc25
 * EntryAbiHash       : 37241b3ba70d4b0f0af53358d5dcd4fda67f64c6db1715fe217a46674e3ba526
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_fbcef2458cadf3371f50b782123f794d3135546b2384ccb63812cdb75a2c3f19 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_fbcef2458cadf3371f50b782123f794d3135546b2384ccb63812cdb75a2c3f19
#endif

// AS Function : int ObjectLastNativeForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:180:2
// JIT Entry   : Raw
int32 ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_fbcef2458cadf3371f50b782123f794d3135546b2384ccb63812cdb75a2c3f19(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::ObjectLastNativeForAOT()", 181);
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
void* v_TEMP_6 = nullptr;
asDWORD v_TEMP_dword_2 = {};
asDWORD v_TEMP_dword_3 = {};
asDWORD v_TEMP_dword_4 = {};
asDWORD v_TEMP_dword_7 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// PshC4 97
// PshC4 39
// PSF v1
// CALLSYS *
// FAotObjectLastProbe::FAotObjectLastProbe(int Left, int Right)
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 1u).GetFunction();
  auto CastedFuncPtr = (void(*)(int32,int32,void*))RawFuncPtr;
  void* Object = (void*)((&v_Value));
SCRIPT_DEBUG_CALLSTACK_POSITION(182, 23);
CastedFuncPtr(value_as<int32>(((asDWORD)0x27u)),value_as<int32>(((asDWORD)0x61u)),Object);
if (Execution.bExceptionThrown) [[unlikely]]
{
return {};
}
}
// JitEntry *
// SUSPEND
// JitEntry *
// PSF v1
// PopRPtr
l_valueRegister = (asQWORD)((&v_Value));
// CpyRtoV8 v6
v_TEMP_6 = (void*)l_valueRegister;
// LoadRObjR
if (v_TEMP_6 == nullptr) [[unlikely]]
{
SCRIPT_DEBUG_CALLSTACK_POSITION(183, 3);
SCRIPT_NULL_POINTER_EXCEPTION();
return {};
}
l_valueRegister = (asQWORD)(v_TEMP_6) + (short)0;
// RDR4 v7
v_TEMP_dword_7 = value_read<asDWORD>((void*)l_valueRegister);
// CpyVtoV4 v2, v7
v_TEMP_dword_2 = v_TEMP_dword_7;
// JMP 0
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLastNativeForAOT_37;
LABEL_UStaticJITAotFunctionCarrier__ObjectLastNativeForAOT_37:
// CpyVtoR4 v2
l_dwordRegister = v_TEMP_dword_2;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int ObjectLastNativeForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:180:2
// JIT Entry   : VM
void ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_fbcef2458cadf3371f50b782123f794d3135546b2384ccb63812cdb75a2c3f19_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_fbcef2458cadf3371f50b782123f794d3135546b2384ccb63812cdb75a2c3f19(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ObjectLastNativeForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:180:2
// JIT Entry   : Parms
void ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_fbcef2458cadf3371f50b782123f794d3135546b2384ccb63812cdb75a2c3f19_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_fbcef2458cadf3371f50b782123f794d3135546b2384ccb63812cdb75a2c3f19(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void SetStoredValue(int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:157:7
 * StableFunctionKey  : 20afea0dd317110e89dad42ccf8623be8b515d245df9295331cd7ab124381646
 * ExecutionHash      : acec9d3dc3df91df8d5b253b82a3a31a43a844a142b2cbd404f3c4478a4fcf36
 * DebugHash          : 7c22dcbc09d648885e37cd7ed9064d357c70858c5e1b666b74bbb47b79e000ed
 * EntryAbiHash       : 3baea263a239ec0fa05b0b147616ad11e567b95161568aa95e53af4de5301117
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_20afea0dd317110e89dad42ccf8623be8b515d245df9295331cd7ab124381646_acec9d3dc3df91df8d5b253b82a3a31a43a844a142b2cbd404f3c4478a4fcf36 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_20afea0dd317110e89dad42ccf8623be8b515d245df9295331cd7ab124381646_acec9d3dc3df91df8d5b253b82a3a31a43a844a142b2cbd404f3c4478a4fcf36
#endif

// AS Function : void SetStoredValue(int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:157:7
// JIT Entry   : Raw
void ASJIT_20afea0dd317110e89dad42ccf8623be8b515d245df9295331cd7ab124381646_acec9d3dc3df91df8d5b253b82a3a31a43a844a142b2cbd404f3c4478a4fcf36(FScriptExecution& Execution, UObject* l_This, asDWORD p_value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("void UStaticJITAotFunctionCarrier::SetStoredValue(int)", 0);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
// JitEntry *
// LoadThisR +48
l_valueRegister = ((asQWORD)l_This) + POFFSET_UStaticJITAotFunctionCarrier_StoredValue;
// WRTV4 v-2
memcpy((void*)l_valueRegister, (void*)(&p_value), 4);
// RET 3
  return;
}
// AS Function : void SetStoredValue(int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:157:7
// JIT Entry   : VM
void ASJIT_20afea0dd317110e89dad42ccf8623be8b515d245df9295331cd7ab124381646_acec9d3dc3df91df8d5b253b82a3a31a43a844a142b2cbd404f3c4478a4fcf36_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_20afea0dd317110e89dad42ccf8623be8b515d245df9295331cd7ab124381646_acec9d3dc3df91df8d5b253b82a3a31a43a844a142b2cbd404f3c4478a4fcf36(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : void SetStoredValue(int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:157:7
// JIT Entry   : Parms
void ASJIT_20afea0dd317110e89dad42ccf8623be8b515d245df9295331cd7ab124381646_acec9d3dc3df91df8d5b253b82a3a31a43a844a142b2cbd404f3c4478a4fcf36_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ASJIT_20afea0dd317110e89dad42ccf8623be8b515d245df9295331cd7ab124381646_acec9d3dc3df91df8d5b253b82a3a31a43a844a142b2cbd404f3c4478a4fcf36(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int AddForAOT(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:159:2
 * StableFunctionKey  : 2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5
 * ExecutionHash      : ee6a38c70694d5cb89422f32bcd6365e3d6cc2cdd591a5efb00c963684f29569
 * DebugHash          : ace961845118b4c55ec053c0e9814523b3bd80068595976e413d11bee2ed3209
 * EntryAbiHash       : 73a436254e2d895a203c53bb56b8bd06ea4085e6086c3b045fabf536dcbc0131
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_ee6a38c70694d5cb89422f32bcd6365e3d6cc2cdd591a5efb00c963684f29569 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_ee6a38c70694d5cb89422f32bcd6365e3d6cc2cdd591a5efb00c963684f29569
#endif

// AS Function : int AddForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:159:2
// JIT Entry   : Raw
int32 ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_ee6a38c70694d5cb89422f32bcd6365e3d6cc2cdd591a5efb00c963684f29569(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::AddForAOT(int)", 160);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
asDWORD v_TEMP_dword_2 = {};
asDWORD v_TEMP_dword_3 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// ADDIi v1, v-2, 7
((int32&)v_TEMP_dword_1) = ((int32&)p_Value) + value_as<int>((asDWORD)0x7u);
// JMP 0
goto LABEL_UStaticJITAotFunctionCarrier__AddForAOT_16;
LABEL_UStaticJITAotFunctionCarrier__AddForAOT_16:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 3
  return (int32)l_dwordRegister;
}
// AS Function : int AddForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:159:2
// JIT Entry   : VM
void ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_ee6a38c70694d5cb89422f32bcd6365e3d6cc2cdd591a5efb00c963684f29569_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_ee6a38c70694d5cb89422f32bcd6365e3d6cc2cdd591a5efb00c963684f29569(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : int AddForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:159:2
// JIT Entry   : Parms
void ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_ee6a38c70694d5cb89422f32bcd6365e3d6cc2cdd591a5efb00c963684f29569_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_ee6a38c70694d5cb89422f32bcd6365e3d6cc2cdd591a5efb00c963684f29569(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ReferenceEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:121:1
 * StableFunctionKey  : 2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9
 * ExecutionHash      : ae2731c1e327cb9389984b0c6acc5c47bf4d3ba508ba3a708fa10c004bb690a9
 * DebugHash          : 852bd685ac4b5c0336eb02f3d3e717591ff4ce857158c6787fbafe7600b2399f
 * EntryAbiHash       : eb88e947baa4860face1585fa0d6b66fc0a33ee288c7ad728b2271e86a77d1e6
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_ae2731c1e327cb9389984b0c6acc5c47bf4d3ba508ba3a708fa10c004bb690a9 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_ae2731c1e327cb9389984b0c6acc5c47bf4d3ba508ba3a708fa10c004bb690a9
#endif

// AS Function : int ReferenceEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:121:1
// JIT Entry   : Raw
int32 ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_ae2731c1e327cb9389984b0c6acc5c47bf4d3ba508ba3a708fa10c004bb690a9(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int ReferenceEntryForAOT()", 122);
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
asDWORD v_TEMP_dword_4 = {};
asDWORD v_TEMP_dword_5 = {};
asDWORD v_TEMP_dword_6 = {};
asDWORD v_TEMP_dword_7 = {};
asDWORD v_TEMP_dword_8 = {};
// JitEntry *
// SUSPEND
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
SCRIPT_DEBUG_CALLSTACK_POSITION(124, 6);
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
// CpyRtoV4 v5
memcpy((&v_TEMP_dword_5), &l_valueRegister, 4);
// CpyVtoV4 v2, v5
v_Returned = v_TEMP_dword_5;
// SUSPEND
// JitEntry *
// MULIi v7, v1, 100
((int32&)v_TEMP_dword_7) = ((int32&)v_Value) * value_as<int>((asDWORD)0x64u);
// ADDi v3, v7, v2
((int32&)v_TEMP_dword_3) = ((int32&)v_TEMP_dword_7) + ((int32&)v_Returned);
// JMP 0
goto LABEL_ReferenceEntryForAOT_37;
LABEL_ReferenceEntryForAOT_37:
// CpyVtoR4 v3
l_dwordRegister = v_TEMP_dword_3;
// RET 0
  return (int32)l_dwordRegister;
}
// AS Function : int ReferenceEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:121:1
// JIT Entry   : VM
void ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_ae2731c1e327cb9389984b0c6acc5c47bf4d3ba508ba3a708fa10c004bb690a9_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_ae2731c1e327cb9389984b0c6acc5c47bf4d3ba508ba3a708fa10c004bb690a9(Execution);
}
// AS Function : int ReferenceEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:121:1
// JIT Entry   : Parms
void ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_ae2731c1e327cb9389984b0c6acc5c47bf4d3ba508ba3a708fa10c004bb690a9_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_ae2731c1e327cb9389984b0c6acc5c47bf4d3ba508ba3a708fa10c004bb690a9(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int AddTwo(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:272:2
 * StableFunctionKey  : 30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c
 * ExecutionHash      : bde799d838c6485c866f237dd665351a4230fc5c6473a7f42a9d128e519da152
 * DebugHash          : dd4850797625ea6423320a8835399aab6781c236669767347284ee64ae09c56a
 * EntryAbiHash       : c5fdf7c9de85d7a785dfc3e36e262ff979f7b922c419614f25625ceaf01dc6bd
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_bde799d838c6485c866f237dd665351a4230fc5c6473a7f42a9d128e519da152 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_bde799d838c6485c866f237dd665351a4230fc5c6473a7f42a9d128e519da152
#endif

// AS Function : int AddTwo(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:272:2
// JIT Entry   : Raw
int32 ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_bde799d838c6485c866f237dd665351a4230fc5c6473a7f42a9d128e519da152(FScriptExecution& Execution, UObject* l_This, asDWORD p_A, asDWORD p_B)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::AddTwo(int, int)", 273);
SCRIPT_ASSUME_NO_EXCEPTION()
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
// SUSPEND
// JitEntry *
// ADDi v1, v-2, v-3
((int32&)v_TEMP_dword_1) = ((int32&)p_A) + ((int32&)p_B);
// JMP 0
goto LABEL_UStaticJITAotFunctionCarrier__AddTwo_15;
LABEL_UStaticJITAotFunctionCarrier__AddTwo_15:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 4
  return (int32)l_dwordRegister;
}
// AS Function : int AddTwo(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:272:2
// JIT Entry   : VM
void ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_bde799d838c6485c866f237dd665351a4230fc5c6473a7f42a9d128e519da152_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_bde799d838c6485c866f237dd665351a4230fc5c6473a7f42a9d128e519da152(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2),
		*(asDWORD*)(l_fp + 3));
}
// AS Function : int AddTwo(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:272:2
// JIT Entry   : Parms
void ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_bde799d838c6485c866f237dd665351a4230fc5c6473a7f42a9d128e519da152_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_A = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1_B = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_bde799d838c6485c866f237dd665351a4230fc5c6473a7f42a9d128e519da152(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_A),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_1_B));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void FailForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:104:1
 * StableFunctionKey  : 34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f
 * ExecutionHash      : 421e9c54a61188cadaf173338e339ca824fde89d618541188785abcf3fc96d9d
 * DebugHash          : d0e11abfa8228b123c054530433ea84c0d39fb5c341fa3f54d2f3f8795ace32a
 * EntryAbiHash       : 5c88cfe1c954db11e6955527a1273af1e6e20436cb7527444616a92e521bcb4d
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_421e9c54a61188cadaf173338e339ca824fde89d618541188785abcf3fc96d9d = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_421e9c54a61188cadaf173338e339ca824fde89d618541188785abcf3fc96d9d
#endif

// AS Function : void FailForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:104:1
// JIT Entry   : Raw
void ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_421e9c54a61188cadaf173338e339ca824fde89d618541188785abcf3fc96d9d(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("void FailForAOT()", 105);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[8];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
void* v_TEMP_2 = nullptr;
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PopRPtr
l_valueRegister = (asQWORD)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 2u));
// CpyRtoV8 v2
v_TEMP_2 = (void*)l_valueRegister;
// PshVPtr v2
// CALLSYS *
// void throw(const FString&inout Message)
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 0u).GetFunction();
  auto CastedFuncPtr = (void(*)(void*))RawFuncPtr;
SCRIPT_DEBUG_CALLSTACK_POSITION(106, 2);
CastedFuncPtr((void**)(v_TEMP_2));
if (Execution.bExceptionThrown) [[unlikely]]
{
return;
}
}
// JitEntry *
// RET 0
  return;
}
// AS Function : void FailForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:104:1
// JIT Entry   : VM
void ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_421e9c54a61188cadaf173338e339ca824fde89d618541188785abcf3fc96d9d_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_421e9c54a61188cadaf173338e339ca824fde89d618541188785abcf3fc96d9d(Execution);
}
// AS Function : void FailForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:104:1
// JIT Entry   : Parms
void ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_421e9c54a61188cadaf173338e339ca824fde89d618541188785abcf3fc96d9d_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_421e9c54a61188cadaf173338e339ca824fde89d618541188785abcf3fc96d9d(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int RecursionEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:144:1
 * StableFunctionKey  : 3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5
 * ExecutionHash      : 351f181e53c921aa9961b02e2116c498e3c7ba2cdb7e113fbed8611cb92ab94d
 * DebugHash          : 032b45b0d05be551f287dd1e5d572a43bd2c45a98468d69c667e4a4314a45d17
 * EntryAbiHash       : 9807be95f7b037b5a524626bf011302bd0844098a4a72bbe407d8cd5e21fa5ae
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_351f181e53c921aa9961b02e2116c498e3c7ba2cdb7e113fbed8611cb92ab94d = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_351f181e53c921aa9961b02e2116c498e3c7ba2cdb7e113fbed8611cb92ab94d
#endif

// AS Function : int RecursionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:144:1
// JIT Entry   : Raw
int32 ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_351f181e53c921aa9961b02e2116c498e3c7ba2cdb7e113fbed8611cb92ab94d(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int RecursionEntryForAOT()", 145);
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
asDWORD v_TEMP_dword_3 = {};
// JitEntry *
// SUSPEND
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
// CpyRtoV4 v3
memcpy((&v_TEMP_dword_3), &l_valueRegister, 4);
// CpyVtoV4 v1, v3
v_TEMP_dword_1 = v_TEMP_dword_3;
// JMP 0
goto LABEL_RecursionEntryForAOT_23;
LABEL_RecursionEntryForAOT_23:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 0
  return (int32)l_dwordRegister;
}
// AS Function : int RecursionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:144:1
// JIT Entry   : VM
void ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_351f181e53c921aa9961b02e2116c498e3c7ba2cdb7e113fbed8611cb92ab94d_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_351f181e53c921aa9961b02e2116c498e3c7ba2cdb7e113fbed8611cb92ab94d(Execution);
}
// AS Function : int RecursionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:144:1
// JIT Entry   : Parms
void ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_351f181e53c921aa9961b02e2116c498e3c7ba2cdb7e113fbed8611cb92ab94d_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_351f181e53c921aa9961b02e2116c498e3c7ba2cdb7e113fbed8611cb92ab94d(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ObjectLifetimeEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:211:2
 * StableFunctionKey  : 40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135
 * ExecutionHash      : 65f335834cb18f01dab239cc265af33d585b3c50caff5f9318b6d84aae995bc6
 * DebugHash          : 1fdd4fd4615be2f6633e10e81ec8e0cc4dd1709cf1a49cf122f0f7bd8adac26f
 * EntryAbiHash       : c8041da3cad4a3fdc7be0bd5795a7456826fc964a0eeef2d2d35507862c860f0
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_65f335834cb18f01dab239cc265af33d585b3c50caff5f9318b6d84aae995bc6 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_65f335834cb18f01dab239cc265af33d585b3c50caff5f9318b6d84aae995bc6
#endif

// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:211:2
// JIT Entry   : Raw
int32 ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_65f335834cb18f01dab239cc265af33d585b3c50caff5f9318b6d84aae995bc6(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::ObjectLifetimeEntryForAOT()", 212);
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
alignas(8) asBYTE MEM_v_TEMP_17[sizeof(FString)];
FString& v_TEMP_17 = (FString&)MEM_v_TEMP_17[0];
void* v_TEMP_11 = nullptr;
void* v_TEMP_13 = nullptr;
void* v_TEMP_19 = nullptr;
void* v_TEMP_21 = nullptr;
void* v_TEMP_23 = nullptr;
void* v_TEMP_29 = nullptr;
asDWORD v_TEMP_dword_9 = {};
asBYTE v_TEMP_byte_24 = {};
asDWORD v_TEMP_dword_25 = {};
asDWORD v_TEMP_dword_26 = {};
asDWORD v_TEMP_dword_27 = {};
asDWORD v_TEMP_dword_30 = {};
asDWORD v_TEMP_dword_31 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PopRPtr
l_valueRegister = (asQWORD)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 7u));
// CpyRtoV8 v11
v_TEMP_11 = (void*)l_valueRegister;
// PshVPtr v11
// PSF v4
// CALLSYS *
// FString::FString(const FString&inout Other)
{
  FString* Object = (FString*)((&v_Original));
new (Object) FString((*(FString*)(v_TEMP_11)));
}
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PopRPtr
l_valueRegister = (asQWORD)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 8u));
// CpyRtoV8 v13
v_TEMP_13 = (void*)l_valueRegister;
// PSF v4
// PopRPtr
l_valueRegister = (asQWORD)((&v_Original));
// CpyRtoV8 v19
v_TEMP_19 = (void*)l_valueRegister;
// PshVPtr v13
// PSF v17
// PshVPtr v19
// CALLSYS *
// FString FString::opAdd(const FString&inout Other) const
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 2u).GetFunction();
  auto CastedFuncPtr = (FString(*)(void*,void*))RawFuncPtr;
  void* Object = (void*)(v_TEMP_19);
SCRIPT_DEBUG_CALLSTACK_POSITION(214, 11);
  if(Object != nullptr)
  {
new((void*)((&v_TEMP_17))) FString(CastedFuncPtr(Object,(void**)(v_TEMP_13)));
} else [[unlikely]] {
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_EXCEPTION_0;
}
if (Execution.bExceptionThrown) [[unlikely]]
{
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_EXCEPTION_0;
}
}
// JitEntry *
// PSF v17
// PSF v8
// CALLSYS *
// FString::FString(const FString&inout Other)
{
  FString* Object = (FString*)((&v_Decorated));
new (Object) FString((*(FString*)((&v_TEMP_17))));
}
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PopRPtr
l_valueRegister = (asQWORD)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 7u));
// CpyRtoV8 v21
v_TEMP_21 = (void*)l_valueRegister;
// PSF v4
// PopRPtr
l_valueRegister = (asQWORD)((&v_Original));
// CpyRtoV8 v23
v_TEMP_23 = (void*)l_valueRegister;
// PshVPtr v21
// PshVPtr v23
// CALLSYS *
// bool FString::opEquals(const FString&inout Other) const
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 1u).GetFunction();
  auto CastedFuncPtr = (bool(*)(void*,void*))RawFuncPtr;
  void* Object = (void*)(v_TEMP_23);
SCRIPT_DEBUG_CALLSTACK_POSITION(215, 3);
  if(Object != nullptr)
  {
bool FunctionReturnValue = CastedFuncPtr(Object,(void**)(v_TEMP_21));
l_byteRegister = (asBYTE&)FunctionReturnValue;
} else [[unlikely]] {
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_EXCEPTION_1;
}
if (Execution.bExceptionThrown) [[unlikely]]
{
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_EXCEPTION_1;
}
}
// JitEntry *
// JLowZ 4
if(l_byteRegister == 0) {
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_79;
}
// SetV4 v25, 100
v_TEMP_dword_25 = 0x64u;
// JMP 2
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_81;
LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_79:
// SetV4 v25, 0
v_TEMP_dword_25 = 0x0u;
LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_81:
// PSF v8
// PopRPtr
l_valueRegister = (asQWORD)((&v_Decorated));
// CpyRtoV8 v29
v_TEMP_29 = (void*)l_valueRegister;
// PshVPtr v29
// CALLSYS *
// int FString::Len() const
{
  FString* Object = (FString*)(v_TEMP_29);
SCRIPT_DEBUG_CALLSTACK_POSITION(215, 3);
  if(Object != nullptr)
  {
int32 FunctionReturnValue = Object->Len();
l_dwordRegister = (asDWORD&)FunctionReturnValue;
} else [[unlikely]] {
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_EXCEPTION_1;
}
}
// JitEntry *
// CpyRtoV4 v30
v_TEMP_dword_30 = l_dwordRegister;
// ADDi v9, v25, v30
((int32&)v_TEMP_dword_9) = ((int32&)v_TEMP_dword_25) + ((int32&)v_TEMP_dword_30);
// PSF v8
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
// JMP 0
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_110;
LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_110:
// PSF v17
// CALLSYS *
// FString::~FString()
{
  FString* Object = (FString*)((&v_TEMP_17));
((FString*)Object)->~FString();
}
// JitEntry *
// CpyVtoR4 v9
l_dwordRegister = v_TEMP_dword_9;
// RET 2
  return (int32)l_dwordRegister;
SCRIPT_ASSUME(false);

LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_EXCEPTION_0:
// Exception cleanup for variables 4
if (!Execution.bExceptionThrown) { SCRIPT_NULL_POINTER_EXCEPTION(); }
((FString*)((FString*)(&v_Original)))->~FString();
return {};
LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_EXCEPTION_1:
// Exception cleanup for variables 4, 8, 17
if (!Execution.bExceptionThrown) { SCRIPT_NULL_POINTER_EXCEPTION(); }
((FString*)((FString*)(&v_TEMP_17)))->~FString();
((FString*)((FString*)(&v_Decorated)))->~FString();
((FString*)((FString*)(&v_Original)))->~FString();
return {};
}
// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:211:2
// JIT Entry   : VM
void ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_65f335834cb18f01dab239cc265af33d585b3c50caff5f9318b6d84aae995bc6_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_65f335834cb18f01dab239cc265af33d585b3c50caff5f9318b6d84aae995bc6(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:211:2
// JIT Entry   : Parms
void ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_65f335834cb18f01dab239cc265af33d585b3c50caff5f9318b6d84aae995bc6_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_65f335834cb18f01dab239cc265af33d585b3c50caff5f9318b6d84aae995bc6(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int64 DoubleToInt64ForAOT(const float)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:170:2
 * StableFunctionKey  : 486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52
 * ExecutionHash      : c0905ecd0cd80d5e0ff4b7b80130dedda41b73d3ac7e58837d8edd8fbdf50fca
 * DebugHash          : 1304abea2a96d8f0e3d288604bc414d1d670f33e1d5351a98788670f06d42945
 * EntryAbiHash       : a1d1604b170c8afa6d8aebaea217a68d24d4f67ff1cce99952cd0a22877c25f4
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_c0905ecd0cd80d5e0ff4b7b80130dedda41b73d3ac7e58837d8edd8fbdf50fca = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_c0905ecd0cd80d5e0ff4b7b80130dedda41b73d3ac7e58837d8edd8fbdf50fca
#endif

// AS Function : int64 DoubleToInt64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:170:2
// JIT Entry   : Raw
int64 ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_c0905ecd0cd80d5e0ff4b7b80130dedda41b73d3ac7e58837d8edd8fbdf50fca(FScriptExecution& Execution, UObject* l_This, double p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int64 UStaticJITAotFunctionCarrier::DoubleToInt64ForAOT(float)", 171);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asQWORD v_TEMP_qword_2 = {};
asQWORD v_TEMP_qword_4 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// dTOi64 v4, v-2
((int64&)v_TEMP_qword_4) = int64(p_Value);
// CpyVtoV8 v2, v4
v_TEMP_qword_2 = v_TEMP_qword_4;
// JMP 0
goto LABEL_UStaticJITAotFunctionCarrier__DoubleToInt64ForAOT_17;
LABEL_UStaticJITAotFunctionCarrier__DoubleToInt64ForAOT_17:
// CpyVtoR8 v2
l_valueRegister = v_TEMP_qword_2;
// RET 4
  return (int64)l_valueRegister;
}
// AS Function : int64 DoubleToInt64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:170:2
// JIT Entry   : VM
void ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_c0905ecd0cd80d5e0ff4b7b80130dedda41b73d3ac7e58837d8edd8fbdf50fca_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int64*)l_outValue = ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_c0905ecd0cd80d5e0ff4b7b80130dedda41b73d3ac7e58837d8edd8fbdf50fca(Execution,
		*(UObject**)l_fp,
		*(double*)(l_fp + 2));
}
// AS Function : int64 DoubleToInt64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:170:2
// JIT Entry   : Parms
void ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_c0905ecd0cd80d5e0ff4b7b80130dedda41b73d3ac7e58837d8edd8fbdf50fca_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(int64));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int64*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_c0905ecd0cd80d5e0ff4b7b80130dedda41b73d3ac7e58837d8edd8fbdf50fca(Execution,
		(UObject*)Object,
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int AddForAOT(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:3:1
 * StableFunctionKey  : 4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30
 * ExecutionHash      : c628649494c1a36ee71c3badcbac1bf18a9806b03d4c043137b27551bd248a06
 * DebugHash          : dcf52bbe1d9342582222356f4acce5b17b14040cf0d991b34a0cda96c569a2f7
 * EntryAbiHash       : 73a436254e2d895a203c53bb56b8bd06ea4085e6086c3b045fabf536dcbc0131
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30_c628649494c1a36ee71c3badcbac1bf18a9806b03d4c043137b27551bd248a06 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30_c628649494c1a36ee71c3badcbac1bf18a9806b03d4c043137b27551bd248a06
#endif

// AS Function : int AddForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:3:1
// JIT Entry   : Raw
int32 ASJIT_4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30_c628649494c1a36ee71c3badcbac1bf18a9806b03d4c043137b27551bd248a06(FScriptExecution& Execution, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int AddForAOT(int)", 4);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
asDWORD v_TEMP_dword_2 = {};
asDWORD v_TEMP_dword_3 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// ADDIi v1, v0, 7
((int32&)v_TEMP_dword_1) = ((int32&)p_Value) + value_as<int>((asDWORD)0x7u);
// JMP 0
goto LABEL_AddForAOT_16;
LABEL_AddForAOT_16:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 1
  return (int32)l_dwordRegister;
}
// AS Function : int AddForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:3:1
// JIT Entry   : VM
void ASJIT_4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30_c628649494c1a36ee71c3badcbac1bf18a9806b03d4c043137b27551bd248a06_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30_c628649494c1a36ee71c3badcbac1bf18a9806b03d4c043137b27551bd248a06(Execution,
		*(asDWORD*)(l_fp + 0));
}
// AS Function : int AddForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:3:1
// JIT Entry   : Parms
void ASJIT_4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30_c628649494c1a36ee71c3badcbac1bf18a9806b03d4c043137b27551bd248a06_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30_c628649494c1a36ee71c3badcbac1bf18a9806b03d4c043137b27551bd248a06(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticMutationParity(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:18:1
 * StableFunctionKey  : 4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4
 * ExecutionHash      : a3269122a5afd6066af4c719ac129fb631a1e76ffe3691767b7fd098153da512
 * DebugHash          : 8fdbd29c8db2dbebbf01c429cdcfdc9319afd6733b24a3134239a1d0ca505b63
 * EntryAbiHash       : 45c016e37ac90d9198599858f3806eb4651e70a0969f759160a4683df7eee13c
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4_a3269122a5afd6066af4c719ac129fb631a1e76ffe3691767b7fd098153da512 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4_a3269122a5afd6066af4c719ac129fb631a1e76ffe3691767b7fd098153da512
#endif

// AS Function : int SemanticMutationParity(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:18:1
// JIT Entry   : Raw
int32 ASJIT_4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4_a3269122a5afd6066af4c719ac129fb631a1e76ffe3691767b7fd098153da512(FScriptExecution& Execution, asDWORD p_Input, asDWORD p_Source)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int SemanticMutationParity(int, int)", 19);
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
asDWORD v_TEMP_dword_6 = {};
asDWORD v_TEMP_dword_7 = {};
asDWORD v_TEMP_dword_8 = {};
asDWORD v_TEMP_dword_9 = {};
asDWORD v_TEMP_dword_10 = {};
asDWORD v_TEMP_dword_11 = {};
asDWORD v_TEMP_dword_12 = {};
asDWORD v_TEMP_dword_13 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
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
// IncVi v1
v_Value++;
// CpyVtoV4 v2, v1
v_Prefix = v_Value;
// SUSPEND
// JitEntry *
// CpyVtoV4 v7, v1
v_TEMP_dword_7 = v_Value;
// IncVi v1
v_Value++;
// CpyVtoV4 v3, v7
v_Postfix = v_TEMP_dword_7;
// SUSPEND
// JitEntry *
// MULIi v9, v1, 1000
((int32&)v_TEMP_dword_9) = ((int32&)v_Value) * value_as<int>((asDWORD)0x3e8u);
// MULIi v11, v2, 100
((int32&)v_TEMP_dword_11) = ((int32&)v_Prefix) * value_as<int>((asDWORD)0x64u);
// ADDi v12, v9, v11
((int32&)v_TEMP_dword_12) = ((int32&)v_TEMP_dword_9) + ((int32&)v_TEMP_dword_11);
// ADDi v4, v12, v3
((int32&)v_TEMP_dword_4) = ((int32&)v_TEMP_dword_12) + ((int32&)v_Postfix);
// JMP 0
goto LABEL_SemanticMutationParity_56;
LABEL_SemanticMutationParity_56:
// CpyVtoR4 v4
l_dwordRegister = v_TEMP_dword_4;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int SemanticMutationParity(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:18:1
// JIT Entry   : VM
void ASJIT_4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4_a3269122a5afd6066af4c719ac129fb631a1e76ffe3691767b7fd098153da512_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4_a3269122a5afd6066af4c719ac129fb631a1e76ffe3691767b7fd098153da512(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}
// AS Function : int SemanticMutationParity(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:18:1
// JIT Entry   : Parms
void ASJIT_4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4_a3269122a5afd6066af4c719ac129fb631a1e76ffe3691767b7fd098153da512_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Input = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1_Source = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4_a3269122a5afd6066af4c719ac129fb631a1e76ffe3691767b7fd098153da512(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Input),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_1_Source));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int BumpReferenceForAOT(int&inout)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:197:2
 * StableFunctionKey  : 5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65
 * ExecutionHash      : f144fe6186549be3f38625cf240b0b52a6f54acdfa62fa1bc0176b7fca1b508c
 * DebugHash          : cc03a20b42fbb34fa26132400ac4a655da333b0730e470e7fc58dd1ac0e8ebde
 * EntryAbiHash       : b80efaaa778ead65430c5702610f23a7b6dccd414f3c044078aa45e30f15b055
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_f144fe6186549be3f38625cf240b0b52a6f54acdfa62fa1bc0176b7fca1b508c = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_f144fe6186549be3f38625cf240b0b52a6f54acdfa62fa1bc0176b7fca1b508c
#endif

// AS Function : int BumpReferenceForAOT(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:197:2
// JIT Entry   : Raw
int32 ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_f144fe6186549be3f38625cf240b0b52a6f54acdfa62fa1bc0176b7fca1b508c(FScriptExecution& Execution, UObject* l_This, int32* p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::BumpReferenceForAOT(int&)", 198);
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
asDWORD v_TEMP_dword_3 = {};
asDWORD v_TEMP_dword_4 = {};
asDWORD v_TEMP_dword_5 = {};
asDWORD v_TEMP_dword_6 = {};
asDWORD v_TEMP_dword_7 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// PshVPtr v-2
// PopRPtr
l_valueRegister = (asQWORD)(p_Value);
// RDR4 v3
v_TEMP_dword_3 = value_read<asDWORD>((void*)l_valueRegister);
// ADDIi v4, v3, 5
((int32&)v_TEMP_dword_4) = ((int32&)v_TEMP_dword_3) + value_as<int>((asDWORD)0x5u);
// PshVPtr v-2
// PopRPtr
l_valueRegister = (asQWORD)(p_Value);
// WRTV4 v4
memcpy((void*)l_valueRegister, (void*)(&v_TEMP_dword_4), 4);
// SUSPEND
// JitEntry *
// PshVPtr v-2
// PopRPtr
l_valueRegister = (asQWORD)(p_Value);
// RDR4 v5
v_TEMP_dword_5 = value_read<asDWORD>((void*)l_valueRegister);
// MULIi v1, v5, 2
((int32&)v_TEMP_dword_1) = ((int32&)v_TEMP_dword_5) * value_as<int>((asDWORD)0x2u);
// JMP 0
goto LABEL_UStaticJITAotFunctionCarrier__BumpReferenceForAOT_32;
LABEL_UStaticJITAotFunctionCarrier__BumpReferenceForAOT_32:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 4
  return (int32)l_dwordRegister;
}
// AS Function : int BumpReferenceForAOT(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:197:2
// JIT Entry   : VM
void ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_f144fe6186549be3f38625cf240b0b52a6f54acdfa62fa1bc0176b7fca1b508c_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_f144fe6186549be3f38625cf240b0b52a6f54acdfa62fa1bc0176b7fca1b508c(Execution,
		*(UObject**)l_fp,
		*(int32**)(l_fp + 2));
}
// AS Function : int BumpReferenceForAOT(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:197:2
// JIT Entry   : Parms
void ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_f144fe6186549be3f38625cf240b0b52a6f54acdfa62fa1bc0176b7fca1b508c_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_f144fe6186549be3f38625cf240b0b52a6f54acdfa62fa1bc0176b7fca1b508c(Execution,
		(UObject*)Object,
		(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UStaticJITAotVirtualChild()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:307:7
 * StableFunctionKey  : 521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7
 * ExecutionHash      : 5e3b5d8162cdd23d758e88d685ca967ac1ce947fb1d0c8057e9d0ad3e0456209
 * DebugHash          : f824385d1fd5c39e946cae7ea3143f71b3a61c432b5b68bf2f0402da6b183d88
 * EntryAbiHash       : 847e0fd88661ad9588f2e5b3a2e560f536b2c027896b3664ab7a3f99a32fee92
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_5e3b5d8162cdd23d758e88d685ca967ac1ce947fb1d0c8057e9d0ad3e0456209 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_5e3b5d8162cdd23d758e88d685ca967ac1ce947fb1d0c8057e9d0ad3e0456209
#endif

// AS Function : UStaticJITAotVirtualChild()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:307:7
// JIT Entry   : Raw
void ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_5e3b5d8162cdd23d758e88d685ca967ac1ce947fb1d0c8057e9d0ad3e0456209(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("UStaticJITAotVirtualChild::UStaticJITAotVirtualChild()", 307);
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
// SetV4 v1, 0
v_TEMP_dword_1 = 0x0u;
// PshVPtr v0
// CALL
// UStaticJITAotVirtualParent::UStaticJITAotVirtualParent()
SCRIPT_DEBUG_CALLSTACK_POSITION(307, 7);
{
  asCScriptFunction* CallFunction = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 3u);
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
// SetV4 v1, 1
v_TEMP_dword_1 = 0x1u;
// SUSPEND
// JitEntry *
// PshVPtr v0
// FinConstruct *
{
  asIScriptObject* Object = (asIScriptObject*)(l_This);
  asITypeInfo* TypeInfo = (asITypeInfo*)FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo(Execution, 1u);
  SCRIPT_FINISH_CONSTRUCT(Object, TypeInfo);
}
// SetV4 v1, 2
v_TEMP_dword_1 = 0x2u;
// RET 2
  return;
}
// AS Function : UStaticJITAotVirtualChild()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:307:7
// JIT Entry   : VM
void ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_5e3b5d8162cdd23d758e88d685ca967ac1ce947fb1d0c8057e9d0ad3e0456209_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_5e3b5d8162cdd23d758e88d685ca967ac1ce947fb1d0c8057e9d0ad3e0456209(Execution,
		*(UObject**)l_fp);
}
// AS Function : UStaticJITAotVirtualChild()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:307:7
// JIT Entry   : Parms
void ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_5e3b5d8162cdd23d758e88d685ca967ac1ce947fb1d0c8057e9d0ad3e0456209_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_5e3b5d8162cdd23d758e88d685ca967ac1ce947fb1d0c8057e9d0ad3e0456209(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int Entry()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:83:1
 * StableFunctionKey  : 5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa
 * ExecutionHash      : d0428fce35cee0ef552af5eac044c0e9a8e1aa03bb7a910d7cb7152b9138e218
 * DebugHash          : 66c6856059f5d76cbb1c79e696e5b496f7a329b74b568bbde32d982ab666b4cd
 * EntryAbiHash       : db0c8f3058412e5bdc683b188ff132b140d00caf79e3e0658bda0254082fdee0
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_d0428fce35cee0ef552af5eac044c0e9a8e1aa03bb7a910d7cb7152b9138e218 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_d0428fce35cee0ef552af5eac044c0e9a8e1aa03bb7a910d7cb7152b9138e218
#endif

// AS Function : int Entry()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:83:1
// JIT Entry   : Raw
int32 ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_d0428fce35cee0ef552af5eac044c0e9a8e1aa03bb7a910d7cb7152b9138e218(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int Entry()", 84);
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
asDWORD v_TEMP_dword_3 = {};
// JitEntry *
// SUSPEND
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
// CpyRtoV4 v3
memcpy((&v_TEMP_dword_3), &l_valueRegister, 4);
// CpyVtoV4 v1, v3
v_TEMP_dword_1 = v_TEMP_dword_3;
// JMP 0
goto LABEL_Entry_23;
LABEL_Entry_23:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 0
  return (int32)l_dwordRegister;
}
// AS Function : int Entry()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:83:1
// JIT Entry   : VM
void ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_d0428fce35cee0ef552af5eac044c0e9a8e1aa03bb7a910d7cb7152b9138e218_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_d0428fce35cee0ef552af5eac044c0e9a8e1aa03bb7a910d7cb7152b9138e218(Execution);
}
// AS Function : int Entry()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:83:1
// JIT Entry   : Parms
void ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_d0428fce35cee0ef552af5eac044c0e9a8e1aa03bb7a910d7cb7152b9138e218_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_d0428fce35cee0ef552af5eac044c0e9a8e1aa03bb7a910d7cb7152b9138e218(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void FailForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:186:2
 * StableFunctionKey  : 58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9
 * ExecutionHash      : 40bb1f46127a8922796ac1ef80f5cea0fe2719490c9c4bc347014c41a7d9d97d
 * DebugHash          : 756d56f744ffff34ff62e59862231c66eeae44b0668ffea39d67799baed550dc
 * EntryAbiHash       : 5c88cfe1c954db11e6955527a1273af1e6e20436cb7527444616a92e521bcb4d
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_40bb1f46127a8922796ac1ef80f5cea0fe2719490c9c4bc347014c41a7d9d97d = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_40bb1f46127a8922796ac1ef80f5cea0fe2719490c9c4bc347014c41a7d9d97d
#endif

// AS Function : void FailForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:186:2
// JIT Entry   : Raw
void ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_40bb1f46127a8922796ac1ef80f5cea0fe2719490c9c4bc347014c41a7d9d97d(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("void UStaticJITAotFunctionCarrier::FailForAOT()", 187);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[8];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
void* v_TEMP_2 = nullptr;
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PopRPtr
l_valueRegister = (asQWORD)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 3u));
// CpyRtoV8 v2
v_TEMP_2 = (void*)l_valueRegister;
// PshVPtr v2
// CALLSYS *
// void throw(const FString&inout Message)
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 1u).GetFunction();
  auto CastedFuncPtr = (void(*)(void*))RawFuncPtr;
SCRIPT_DEBUG_CALLSTACK_POSITION(188, 3);
CastedFuncPtr((void**)(v_TEMP_2));
if (Execution.bExceptionThrown) [[unlikely]]
{
return;
}
}
// JitEntry *
// RET 2
  return;
}
// AS Function : void FailForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:186:2
// JIT Entry   : VM
void ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_40bb1f46127a8922796ac1ef80f5cea0fe2719490c9c4bc347014c41a7d9d97d_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_40bb1f46127a8922796ac1ef80f5cea0fe2719490c9c4bc347014c41a7d9d97d(Execution,
		*(UObject**)l_fp);
}
// AS Function : void FailForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:186:2
// JIT Entry   : Parms
void ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_40bb1f46127a8922796ac1ef80f5cea0fe2719490c9c4bc347014c41a7d9d97d_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_40bb1f46127a8922796ac1ef80f5cea0fe2719490c9c4bc347014c41a7d9d97d(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int GetVirtualValue()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:293:2
 * StableFunctionKey  : 598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c
 * ExecutionHash      : 881fd92919c7145e9d8612d5d8a2ab3f59d410c5a8af76a77a3bacc0d35b3a43
 * DebugHash          : c9757ac86a957d2be7bce6f6e35f64ad1212cd1f071d5084fc42882c09579828
 * EntryAbiHash       : 3c7da2556903a2aede2bf7c4a70e333223d38b106a790953f977db702ff61900
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_881fd92919c7145e9d8612d5d8a2ab3f59d410c5a8af76a77a3bacc0d35b3a43 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_881fd92919c7145e9d8612d5d8a2ab3f59d410c5a8af76a77a3bacc0d35b3a43
#endif

// AS Function : int GetVirtualValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:293:2
// JIT Entry   : Raw
int32 ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_881fd92919c7145e9d8612d5d8a2ab3f59d410c5a8af76a77a3bacc0d35b3a43(FScriptExecution& Execution, UObject* l_This)
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
alignas(4) asBYTE MEM_v_TEMP_9[sizeof(FName)];
FName& v_TEMP_9 = (FName&)MEM_v_TEMP_9[0];
void* v_TEMP_5 = nullptr;
void* v_TEMP_11 = nullptr;
asDWORD v_TEMP_dword_2 = {};
asDWORD v_TEMP_dword_3 = {};
asDWORD v_TEMP_dword_6 = {};
alignas(16) asBYTE l_ParmStruct[4];
// JitEntry *
// SUSPEND
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
// PopRPtr
l_valueRegister = (asQWORD)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 7u));
// CpyRtoV8 v5
v_TEMP_5 = (void*)l_valueRegister;
// PshVPtr v5
// PshC4 1
// PSF v9
// CALLSYS *
// FName __STATIC_NAME(int Id, const FString&inout CanonicalName)
{
new(((void*)((&v_TEMP_9)))) FName(FAngelscriptEngine::ResolveStaticName(value_as<int32>(((asDWORD)0x1u)), (*(FString*)(v_TEMP_5))));
}
// JitEntry *
// PSF v9
// PopRPtr
l_valueRegister = (asQWORD)((&v_TEMP_9));
// CpyRtoV8 v11
v_TEMP_11 = (void*)l_valueRegister;
// PshVPtr v11
// PshV8 v0
// CALLSYS *
// void __Evt_Execute(const UObject Object, const FName&inout Name)
{
UObject* EventObject = ((UObject*)(l_This));
UFunction* EventFunction = EventObject->FindFunctionChecked(*(FName*)((v_TEMP_11)));
EventObject->ProcessEvent(EventFunction, &l_ParmStruct[0]);
*(asDWORD*)(((&v___ReturnValue))) = (asDWORD&)l_ParmStruct[0];
}
// JitEntry *
// SUSPEND
// JitEntry *
// CpyVtoV4 v2, v1
v_TEMP_dword_2 = v___ReturnValue;
// JMP 0
goto LABEL_UStaticJITAotVirtualParent__GetVirtualValue_62;
LABEL_UStaticJITAotVirtualParent__GetVirtualValue_62:
// CpyVtoR4 v2
l_dwordRegister = v_TEMP_dword_2;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int GetVirtualValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:293:2
// JIT Entry   : VM
void ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_881fd92919c7145e9d8612d5d8a2ab3f59d410c5a8af76a77a3bacc0d35b3a43_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_881fd92919c7145e9d8612d5d8a2ab3f59d410c5a8af76a77a3bacc0d35b3a43(Execution,
		*(UObject**)l_fp);
}
// AS Function : int GetVirtualValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:293:2
// JIT Entry   : Parms
void ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_881fd92919c7145e9d8612d5d8a2ab3f59d410c5a8af76a77a3bacc0d35b3a43_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_881fd92919c7145e9d8612d5d8a2ab3f59d410c5a8af76a77a3bacc0d35b3a43(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UStaticJITAotFunctionCarrier ReturnSelfObject()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:266:2
 * StableFunctionKey  : 5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f
 * ExecutionHash      : e9a6a14e3ec22af8a9a69cdb3539d28978b7b8b4007a8669684fad936c310b1d
 * DebugHash          : e69dc8dbb3c071edcf64975669ad84f1a1eed5601faaba15407d7bd7ef6b3f2f
 * EntryAbiHash       : 30ea0052fcb05cde2ff76d0f9f168b4c15908ca08694ae1622949ba51c05132e
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_e9a6a14e3ec22af8a9a69cdb3539d28978b7b8b4007a8669684fad936c310b1d = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_e9a6a14e3ec22af8a9a69cdb3539d28978b7b8b4007a8669684fad936c310b1d
#endif

// AS Function : UStaticJITAotFunctionCarrier ReturnSelfObject()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:266:2
// JIT Entry   : Raw
UObject* ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_e9a6a14e3ec22af8a9a69cdb3539d28978b7b8b4007a8669684fad936c310b1d(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("UStaticJITAotFunctionCarrier UStaticJITAotFunctionCarrier::ReturnSelfObject()", 267);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
void* v_TEMP_2 = nullptr;
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// CpyVtoV8 v2, v0
v_TEMP_2 = l_This;
// JMP 0
goto LABEL_UStaticJITAotFunctionCarrier__ReturnSelfObject_15;
LABEL_UStaticJITAotFunctionCarrier__ReturnSelfObject_15:
// LOADOBJ v2
l_objectRegister = (void*)v_TEMP_2;
v_TEMP_2 = nullptr;
// RET 2
  return (UObject*)l_objectRegister;
}
// AS Function : UStaticJITAotFunctionCarrier ReturnSelfObject()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:266:2
// JIT Entry   : VM
void ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_e9a6a14e3ec22af8a9a69cdb3539d28978b7b8b4007a8669684fad936c310b1d_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(UObject**)l_outValue = ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_e9a6a14e3ec22af8a9a69cdb3539d28978b7b8b4007a8669684fad936c310b1d(Execution,
		*(UObject**)l_fp);
}
// AS Function : UStaticJITAotFunctionCarrier ReturnSelfObject()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:266:2
// JIT Entry   : Parms
void ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_e9a6a14e3ec22af8a9a69cdb3539d28978b7b8b4007a8669684fad936c310b1d_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(UObject**)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_e9a6a14e3ec22af8a9a69cdb3539d28978b7b8b4007a8669684fad936c310b1d(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ObjectLifetimeEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:128:1
 * StableFunctionKey  : 5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d
 * ExecutionHash      : 84f18fa5ca8ca8b12e0095a29e862e13676d2dae554394c98cf0bdc016635810
 * DebugHash          : 6fa3d26f0ef12c56e8a9f2220bcf1338dc407614aee4f8b9c0f6fd1d3a45b3c2
 * EntryAbiHash       : c8041da3cad4a3fdc7be0bd5795a7456826fc964a0eeef2d2d35507862c860f0
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_84f18fa5ca8ca8b12e0095a29e862e13676d2dae554394c98cf0bdc016635810 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_84f18fa5ca8ca8b12e0095a29e862e13676d2dae554394c98cf0bdc016635810
#endif

// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:128:1
// JIT Entry   : Raw
int32 ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_84f18fa5ca8ca8b12e0095a29e862e13676d2dae554394c98cf0bdc016635810(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int ObjectLifetimeEntryForAOT()", 129);
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
alignas(8) asBYTE MEM_v_TEMP_17[sizeof(FString)];
FString& v_TEMP_17 = (FString&)MEM_v_TEMP_17[0];
void* v_TEMP_11 = nullptr;
void* v_TEMP_13 = nullptr;
void* v_TEMP_19 = nullptr;
void* v_TEMP_21 = nullptr;
void* v_TEMP_23 = nullptr;
void* v_TEMP_29 = nullptr;
asDWORD v_TEMP_dword_9 = {};
asBYTE v_TEMP_byte_24 = {};
asDWORD v_TEMP_dword_25 = {};
asDWORD v_TEMP_dword_26 = {};
asDWORD v_TEMP_dword_27 = {};
asDWORD v_TEMP_dword_30 = {};
asDWORD v_TEMP_dword_31 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PopRPtr
l_valueRegister = (asQWORD)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 6u));
// CpyRtoV8 v11
v_TEMP_11 = (void*)l_valueRegister;
// PshVPtr v11
// PSF v4
// CALLSYS *
// FString::FString(const FString&inout Other)
{
  FString* Object = (FString*)((&v_Original));
new (Object) FString((*(FString*)(v_TEMP_11)));
}
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PopRPtr
l_valueRegister = (asQWORD)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 7u));
// CpyRtoV8 v13
v_TEMP_13 = (void*)l_valueRegister;
// PSF v4
// PopRPtr
l_valueRegister = (asQWORD)((&v_Original));
// CpyRtoV8 v19
v_TEMP_19 = (void*)l_valueRegister;
// PshVPtr v13
// PSF v17
// PshVPtr v19
// CALLSYS *
// FString FString::opAdd(const FString&inout Other) const
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 1u).GetFunction();
  auto CastedFuncPtr = (FString(*)(void*,void*))RawFuncPtr;
  void* Object = (void*)(v_TEMP_19);
SCRIPT_DEBUG_CALLSTACK_POSITION(131, 10);
  if(Object != nullptr)
  {
new((void*)((&v_TEMP_17))) FString(CastedFuncPtr(Object,(void**)(v_TEMP_13)));
} else [[unlikely]] {
goto LABEL_ObjectLifetimeEntryForAOT_EXCEPTION_0;
}
if (Execution.bExceptionThrown) [[unlikely]]
{
goto LABEL_ObjectLifetimeEntryForAOT_EXCEPTION_0;
}
}
// JitEntry *
// PSF v17
// PSF v8
// CALLSYS *
// FString::FString(const FString&inout Other)
{
  FString* Object = (FString*)((&v_Decorated));
new (Object) FString((*(FString*)((&v_TEMP_17))));
}
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PopRPtr
l_valueRegister = (asQWORD)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 6u));
// CpyRtoV8 v21
v_TEMP_21 = (void*)l_valueRegister;
// PSF v4
// PopRPtr
l_valueRegister = (asQWORD)((&v_Original));
// CpyRtoV8 v23
v_TEMP_23 = (void*)l_valueRegister;
// PshVPtr v21
// PshVPtr v23
// CALLSYS *
// bool FString::opEquals(const FString&inout Other) const
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 0u).GetFunction();
  auto CastedFuncPtr = (bool(*)(void*,void*))RawFuncPtr;
  void* Object = (void*)(v_TEMP_23);
SCRIPT_DEBUG_CALLSTACK_POSITION(132, 2);
  if(Object != nullptr)
  {
bool FunctionReturnValue = CastedFuncPtr(Object,(void**)(v_TEMP_21));
l_byteRegister = (asBYTE&)FunctionReturnValue;
} else [[unlikely]] {
goto LABEL_ObjectLifetimeEntryForAOT_EXCEPTION_1;
}
if (Execution.bExceptionThrown) [[unlikely]]
{
goto LABEL_ObjectLifetimeEntryForAOT_EXCEPTION_1;
}
}
// JitEntry *
// JLowZ 4
if(l_byteRegister == 0) {
goto LABEL_ObjectLifetimeEntryForAOT_79;
}
// SetV4 v25, 100
v_TEMP_dword_25 = 0x64u;
// JMP 2
goto LABEL_ObjectLifetimeEntryForAOT_81;
LABEL_ObjectLifetimeEntryForAOT_79:
// SetV4 v25, 0
v_TEMP_dword_25 = 0x0u;
LABEL_ObjectLifetimeEntryForAOT_81:
// PSF v8
// PopRPtr
l_valueRegister = (asQWORD)((&v_Decorated));
// CpyRtoV8 v29
v_TEMP_29 = (void*)l_valueRegister;
// PshVPtr v29
// CALLSYS *
// int FString::Len() const
{
  FString* Object = (FString*)(v_TEMP_29);
SCRIPT_DEBUG_CALLSTACK_POSITION(132, 2);
  if(Object != nullptr)
  {
int32 FunctionReturnValue = Object->Len();
l_dwordRegister = (asDWORD&)FunctionReturnValue;
} else [[unlikely]] {
goto LABEL_ObjectLifetimeEntryForAOT_EXCEPTION_1;
}
}
// JitEntry *
// CpyRtoV4 v30
v_TEMP_dword_30 = l_dwordRegister;
// ADDi v9, v25, v30
((int32&)v_TEMP_dword_9) = ((int32&)v_TEMP_dword_25) + ((int32&)v_TEMP_dword_30);
// PSF v8
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
// JMP 0
goto LABEL_ObjectLifetimeEntryForAOT_110;
LABEL_ObjectLifetimeEntryForAOT_110:
// PSF v17
// CALLSYS *
// FString::~FString()
{
  FString* Object = (FString*)((&v_TEMP_17));
((FString*)Object)->~FString();
}
// JitEntry *
// CpyVtoR4 v9
l_dwordRegister = v_TEMP_dword_9;
// RET 0
  return (int32)l_dwordRegister;
SCRIPT_ASSUME(false);

LABEL_ObjectLifetimeEntryForAOT_EXCEPTION_0:
// Exception cleanup for variables 4
if (!Execution.bExceptionThrown) { SCRIPT_NULL_POINTER_EXCEPTION(); }
((FString*)((FString*)(&v_Original)))->~FString();
return {};
LABEL_ObjectLifetimeEntryForAOT_EXCEPTION_1:
// Exception cleanup for variables 4, 8, 17
if (!Execution.bExceptionThrown) { SCRIPT_NULL_POINTER_EXCEPTION(); }
((FString*)((FString*)(&v_TEMP_17)))->~FString();
((FString*)((FString*)(&v_Decorated)))->~FString();
((FString*)((FString*)(&v_Original)))->~FString();
return {};
}
// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:128:1
// JIT Entry   : VM
void ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_84f18fa5ca8ca8b12e0095a29e862e13676d2dae554394c98cf0bdc016635810_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_84f18fa5ca8ca8b12e0095a29e862e13676d2dae554394c98cf0bdc016635810(Execution);
}
// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:128:1
// JIT Entry   : Parms
void ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_84f18fa5ca8ca8b12e0095a29e862e13676d2dae554394c98cf0bdc016635810_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_84f18fa5ca8ca8b12e0095a29e862e13676d2dae554394c98cf0bdc016635810(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int BumpReference(int&inout)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:258:2
 * StableFunctionKey  : 635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1
 * ExecutionHash      : 01ca10f08938016714d20284a0c0f79ec96d52d9c37199d81b434d3934b730fd
 * DebugHash          : a7c21a991c8c51ca3938db00b11ab42fc0fa6bb333628f3b51835ff3d445c401
 * EntryAbiHash       : 59029a9011b50bdad9e86b6ca1d8d31868558cdf84799ffea7ef7660667c250a
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_01ca10f08938016714d20284a0c0f79ec96d52d9c37199d81b434d3934b730fd = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_01ca10f08938016714d20284a0c0f79ec96d52d9c37199d81b434d3934b730fd
#endif

// AS Function : int BumpReference(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:258:2
// JIT Entry   : Raw
int32 ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_01ca10f08938016714d20284a0c0f79ec96d52d9c37199d81b434d3934b730fd(FScriptExecution& Execution, UObject* l_This, int32* p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::BumpReference(int&)", 259);
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
asDWORD v_TEMP_dword_3 = {};
asDWORD v_TEMP_dword_4 = {};
asDWORD v_TEMP_dword_5 = {};
asDWORD v_TEMP_dword_6 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// PshVPtr v-2
// PopRPtr
l_valueRegister = (asQWORD)(p_Value);
// RDR4 v3
v_TEMP_dword_3 = value_read<asDWORD>((void*)l_valueRegister);
// ADDIi v4, v3, 5
((int32&)v_TEMP_dword_4) = ((int32&)v_TEMP_dword_3) + value_as<int>((asDWORD)0x5u);
// PshVPtr v-2
// PopRPtr
l_valueRegister = (asQWORD)(p_Value);
// WRTV4 v4
memcpy((void*)l_valueRegister, (void*)(&v_TEMP_dword_4), 4);
// SUSPEND
// JitEntry *
// PshVPtr v-2
// PopRPtr
l_valueRegister = (asQWORD)(p_Value);
// RDR4 v5
v_TEMP_dword_5 = value_read<asDWORD>((void*)l_valueRegister);
// LoadThisR +48
l_valueRegister = ((asQWORD)l_This) + POFFSET_UStaticJITAotFunctionCarrier_StoredValue;
// WRTV4 v5
memcpy((void*)l_valueRegister, (void*)(&v_TEMP_dword_5), 4);
// SUSPEND
// JitEntry *
// PshVPtr v-2
// PopRPtr
l_valueRegister = (asQWORD)(p_Value);
// RDR4 v6
v_TEMP_dword_6 = value_read<asDWORD>((void*)l_valueRegister);
// CpyVtoV4 v1, v6
v_TEMP_dword_1 = v_TEMP_dword_6;
// JMP 0
goto LABEL_UStaticJITAotFunctionCarrier__BumpReference_41;
LABEL_UStaticJITAotFunctionCarrier__BumpReference_41:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 4
  return (int32)l_dwordRegister;
}
// AS Function : int BumpReference(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:258:2
// JIT Entry   : VM
void ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_01ca10f08938016714d20284a0c0f79ec96d52d9c37199d81b434d3934b730fd_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_01ca10f08938016714d20284a0c0f79ec96d52d9c37199d81b434d3934b730fd(Execution,
		*(UObject**)l_fp,
		*(int32**)(l_fp + 2));
}
// AS Function : int BumpReference(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:258:2
// JIT Entry   : Parms
void ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_01ca10f08938016714d20284a0c0f79ec96d52d9c37199d81b434d3934b730fd_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_01ca10f08938016714d20284a0c0f79ec96d52d9c37199d81b434d3934b730fd(Execution,
		(UObject*)Object,
		(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UStaticJITAotFunctionCarrier UStaticJITAotFunctionCarrier()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:155:7
 * StableFunctionKey  : 6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060
 * ExecutionHash      : aeb353365a6c37141a1234c98cccf7c1d3adea5667821fffbb87bfe20a3e4a08
 * DebugHash          : 0d4c0f49c4494284326a9a00afe132329675e9fb9088e7ee77ffae40655b3776
 * EntryAbiHash       : b064ff3e8589a401db170315cb55c048d06915739f55618b8165aeca3b2dc0f1
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060_aeb353365a6c37141a1234c98cccf7c1d3adea5667821fffbb87bfe20a3e4a08 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060_aeb353365a6c37141a1234c98cccf7c1d3adea5667821fffbb87bfe20a3e4a08
#endif

// AS Function : UStaticJITAotFunctionCarrier UStaticJITAotFunctionCarrier()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:155:7
// JIT Entry   : Raw
UObject* ASJIT_6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060_aeb353365a6c37141a1234c98cccf7c1d3adea5667821fffbb87bfe20a3e4a08(FScriptExecution& Execution)
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
void* v_TEMP_2 = nullptr;
// JitEntry *
// PSF v2
// ALLOC
{
  asCObjectType* objType = (asCObjectType*)FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo(Execution, 0u);
  asDWORD* mem = (asDWORD*)SCRIPT_ENGINE->AllocScriptObject(objType);
  ScriptObject_Construct(objType, (asCScriptObject*)mem);
  asPWORD* a = (asPWORD*)((&v_TEMP_2));
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
l_objectRegister = (void*)v_TEMP_2;
v_TEMP_2 = nullptr;
// RET 0
  return (UObject*)l_objectRegister;
}
// AS Function : UStaticJITAotFunctionCarrier UStaticJITAotFunctionCarrier()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:155:7
// JIT Entry   : VM
void ASJIT_6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060_aeb353365a6c37141a1234c98cccf7c1d3adea5667821fffbb87bfe20a3e4a08_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(UObject**)l_outValue = ASJIT_6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060_aeb353365a6c37141a1234c98cccf7c1d3adea5667821fffbb87bfe20a3e4a08(Execution);
}
// AS Function : UStaticJITAotFunctionCarrier UStaticJITAotFunctionCarrier()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:155:7
// JIT Entry   : Parms
void ASJIT_6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060_aeb353365a6c37141a1234c98cccf7c1d3adea5667821fffbb87bfe20a3e4a08_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(UObject**)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060_aeb353365a6c37141a1234c98cccf7c1d3adea5667821fffbb87bfe20a3e4a08(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTExportedSymbolShowcase(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:44:1
 * StableFunctionKey  : 73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795
 * ExecutionHash      : ba49716ef3dcfe14534837ac54776b61b6bb0f2c265cb9ab49603c6364b8944c
 * DebugHash          : ad99c4745717a0bcfcee336e441d2839a4029b33732ee57e8110e315c747ca0c
 * EntryAbiHash       : 9309f3438eca43da589c8d5ac23b06a58e143747a47d2f54ff811daf7b2e9706
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_ba49716ef3dcfe14534837ac54776b61b6bb0f2c265cb9ab49603c6364b8944c_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d26,
	const int32 as_ast_d27);

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
static int32 ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_ba49716ef3dcfe14534837ac54776b61b6bb0f2c265cb9ab49603c6364b8944c_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d26,
	const int32 as_ast_d27)
{
	{
		TypedFrame.SetSourcePosition(46, 2);
		return ([&]() -> int32
	{
		const int32 as_ast_e58_arg1 = as_ast_d27;
		const int32 as_ast_e58_arg0 = as_ast_d26;
		const int32 as_ast_e58_result = AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32, int32>(Execution, ASJIT_Call_DaysInMonth_a23d24f5_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795, as_ast_e58_arg0, as_ast_e58_arg1);
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return as_ast_e58_result;
	}());
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int TypedASTExportedSymbolShowcase(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:44:1
// JIT Entry   : Raw
int32 ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_ba49716ef3dcfe14534837ac54776b61b6bb0f2c265cb9ab49603c6364b8944c_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1)
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
	return ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_ba49716ef3dcfe14534837ac54776b61b6bb0f2c265cb9ab49603c6364b8944c_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1));
}

// AS Function : int TypedASTExportedSymbolShowcase(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:44:1
// JIT Entry   : VM
void ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_ba49716ef3dcfe14534837ac54776b61b6bb0f2c265cb9ab49603c6364b8944c_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_ba49716ef3dcfe14534837ac54776b61b6bb0f2c265cb9ab49603c6364b8944c_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}

// AS Function : int TypedASTExportedSymbolShowcase(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:44:1
// JIT Entry   : Parms
void ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_ba49716ef3dcfe14534837ac54776b61b6bb0f2c265cb9ab49603c6364b8944c_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_ba49716ef3dcfe14534837ac54776b61b6bb0f2c265cb9ab49603c6364b8944c_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : float SemanticPowerDoubleInt(const float, const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:38:1
 * StableFunctionKey  : 75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f
 * ExecutionHash      : 3aa11dc9d0c29cd9c5e9e6d4e27f3225201c872199d071bfba924a13d41b3470
 * DebugHash          : f9a5f0d11eb3a8e5c5b86aff6e4fa5a99ee2cf853f4f05722c73a0bc25609fb8
 * EntryAbiHash       : db0465d360843dd633375f902a889661f9e22535246942bafbda919d3c3cbcf1
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f_3aa11dc9d0c29cd9c5e9e6d4e27f3225201c872199d071bfba924a13d41b3470 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f_3aa11dc9d0c29cd9c5e9e6d4e27f3225201c872199d071bfba924a13d41b3470
#endif

// AS Function : float SemanticPowerDoubleInt(const float, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:38:1
// JIT Entry   : Raw
double ASJIT_75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f_3aa11dc9d0c29cd9c5e9e6d4e27f3225201c872199d071bfba924a13d41b3470(FScriptExecution& Execution, double p_Base, asDWORD p_Exponent)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("float SemanticPowerDoubleInt(float, int)", 39);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
double v_TEMP_double_2 = {};
double v_TEMP_double_4 = {};
double v_TEMP_double_6 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// iTOd v4, v-2
v_TEMP_double_4 = ConvertPrimitiveValue<double,int>(((int32&)p_Exponent));
// POWd v2, v0, v4
v_TEMP_double_2 = FMath::Pow(p_Base, v_TEMP_double_4);
if (v_TEMP_double_2 == HUGE_VAL) [[unlikely]]
{
SCRIPT_DEBUG_CALLSTACK_POSITION(40, 2);
  SCRIPT_POWER_OVERFLOW_EXCEPTION();
return {};
}
// JMP 0
goto LABEL_SemanticPowerDoubleInt_17;
LABEL_SemanticPowerDoubleInt_17:
// CpyVtoR8 v2
l_doubleRegister = v_TEMP_double_2;
// RET 3
  return l_doubleRegister;
}
// AS Function : float SemanticPowerDoubleInt(const float, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:38:1
// JIT Entry   : VM
void ASJIT_75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f_3aa11dc9d0c29cd9c5e9e6d4e27f3225201c872199d071bfba924a13d41b3470_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(double*)l_outValue = ASJIT_75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f_3aa11dc9d0c29cd9c5e9e6d4e27f3225201c872199d071bfba924a13d41b3470(Execution,
		*(double*)(l_fp + 0),
		*(asDWORD*)(l_fp + 2));
}
// AS Function : float SemanticPowerDoubleInt(const float, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:38:1
// JIT Entry   : Parms
void ASJIT_75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f_3aa11dc9d0c29cd9c5e9e6d4e27f3225201c872199d071bfba924a13d41b3470_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Base = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1_Exponent = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(double));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(double*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f_3aa11dc9d0c29cd9c5e9e6d4e27f3225201c872199d071bfba924a13d41b3470(Execution,
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_0_Base),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_1_Exponent));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : float SemanticPowerDouble(const float, const float)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:33:1
 * StableFunctionKey  : 794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43
 * ExecutionHash      : 96a2c9f1fa0935eed9f0553c7e041653b5b287e2dd98e62f9b14cd4555af4a8d
 * DebugHash          : 7a81ac29c39fbd718f17f5b712d174d5924393db0fd4fc2d01880cd24fa385c0
 * EntryAbiHash       : 802462d137df2242b59cb2b901557e6d9f177fed1caae7e133b299f6f631b506
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43_96a2c9f1fa0935eed9f0553c7e041653b5b287e2dd98e62f9b14cd4555af4a8d = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43_96a2c9f1fa0935eed9f0553c7e041653b5b287e2dd98e62f9b14cd4555af4a8d
#endif

// AS Function : float SemanticPowerDouble(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:33:1
// JIT Entry   : Raw
double ASJIT_794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43_96a2c9f1fa0935eed9f0553c7e041653b5b287e2dd98e62f9b14cd4555af4a8d(FScriptExecution& Execution, double p_Base, double p_Exponent)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("float SemanticPowerDouble(float, float)", 34);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
double v_TEMP_double_2 = {};
double v_TEMP_double_4 = {};
// JitEntry *
// SUSPEND
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
// JMP 0
goto LABEL_SemanticPowerDouble_15;
LABEL_SemanticPowerDouble_15:
// CpyVtoR8 v2
l_doubleRegister = v_TEMP_double_2;
// RET 4
  return l_doubleRegister;
}
// AS Function : float SemanticPowerDouble(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:33:1
// JIT Entry   : VM
void ASJIT_794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43_96a2c9f1fa0935eed9f0553c7e041653b5b287e2dd98e62f9b14cd4555af4a8d_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(double*)l_outValue = ASJIT_794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43_96a2c9f1fa0935eed9f0553c7e041653b5b287e2dd98e62f9b14cd4555af4a8d(Execution,
		*(double*)(l_fp + 0),
		*(double*)(l_fp + 2));
}
// AS Function : float SemanticPowerDouble(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:33:1
// JIT Entry   : Parms
void ASJIT_794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43_96a2c9f1fa0935eed9f0553c7e041653b5b287e2dd98e62f9b14cd4555af4a8d_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Base = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(double));
	const SIZE_T ParmOffset_1_Exponent = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(double));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(double*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43_96a2c9f1fa0935eed9f0553c7e041653b5b287e2dd98e62f9b14cd4555af4a8d(Execution,
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_0_Base),
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_1_Exponent));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int StaticWorldContextCheck(UObject, const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:324:1
 * StableFunctionKey  : 89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652
 * ExecutionHash      : 3d0e3bdf64c5474c3d0278f7fa54c1acea15472ab15d55fcd7f81750d86fc709
 * DebugHash          : fad41f6d7a4dc015285e305043f94c7dcee1f49082ff44883d447b1c50a959c0
 * EntryAbiHash       : 43bf555b1cdd803979e0fcaa94ab1a10d69fba57c0e1e44934d93cb64c5eb6d1
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_3d0e3bdf64c5474c3d0278f7fa54c1acea15472ab15d55fcd7f81750d86fc709 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_3d0e3bdf64c5474c3d0278f7fa54c1acea15472ab15d55fcd7f81750d86fc709
#endif

// AS Function : int StaticWorldContextCheck(UObject, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:324:1
// JIT Entry   : Raw
int32 ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_3d0e3bdf64c5474c3d0278f7fa54c1acea15472ab15d55fcd7f81750d86fc709(FScriptExecution& Execution, UObject* p_WorldContextObject, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int StaticWorldContextCheck(UObject, int)", 325);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
UObject* v_TEMP_ptr_3 = nullptr;
asDWORD v_TEMP_dword_1 = {};
asBYTE v_TEMP_byte_4 = {};
asDWORD v_TEMP_dword_5 = {};
asDWORD v_TEMP_dword_6 = {};
asDWORD v_TEMP_dword_7 = {};
asDWORD v_TEMP_dword_8 = {};
// JitEntry *
// SUSPEND
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
// ClrVPtr v3
v_TEMP_ptr_3 = nullptr;
// STOREOBJ v3
v_TEMP_ptr_3 = (UObject*)l_objectRegister;
l_objectRegister = nullptr;
// CmpPtr v3, v0
l_byteRegister = ((void*)v_TEMP_ptr_3 == (void*)p_WorldContextObject) ? 0 : 1;
// JZ 15
if(l_byteRegister == 0) {
goto LABEL_StaticWorldContextCheck_38;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v6, 1
v_TEMP_dword_6 = 0x1u;
// NEGi v6
((int32&)v_TEMP_dword_6) = - ((int32&)v_TEMP_dword_6);
// CpyVtoV4 v1, v6
v_TEMP_dword_1 = v_TEMP_dword_6;
// JMP 9
goto LABEL_StaticWorldContextCheck_47;
LABEL_StaticWorldContextCheck_38:
// SUSPEND
// JitEntry *
// ADDIi v1, v-2, 2
((int32&)v_TEMP_dword_1) = ((int32&)p_Value) + value_as<int>((asDWORD)0x2u);
// JMP 0
goto LABEL_StaticWorldContextCheck_47;
LABEL_StaticWorldContextCheck_47:
// FREE v3, *
{
  void* obj = (void*)v_TEMP_ptr_3;
  if (obj != nullptr)
  {
    asCObjectType* objType = (asCObjectType*)FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo(Execution, 1u);
    if ((objType->flags & asOBJ_NOCOUNT) == 0 && objType->beh.release != 0)
      SCRIPT_ENGINE->CallObjectMethod(obj, objType->beh.release);
  }
}
v_TEMP_ptr_3 = nullptr;
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 3
  return (int32)l_dwordRegister;
}
// AS Function : int StaticWorldContextCheck(UObject, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:324:1
// JIT Entry   : VM
void ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_3d0e3bdf64c5474c3d0278f7fa54c1acea15472ab15d55fcd7f81750d86fc709_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_3d0e3bdf64c5474c3d0278f7fa54c1acea15472ab15d55fcd7f81750d86fc709(Execution,
		*(UObject**)(l_fp + 0),
		*(asDWORD*)(l_fp + 2));
}
// AS Function : int StaticWorldContextCheck(UObject, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:324:1
// JIT Entry   : Parms
void ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_3d0e3bdf64c5474c3d0278f7fa54c1acea15472ab15d55fcd7f81750d86fc709_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_WorldContextObject = ParmsOffset;
	ParmsOffset += sizeof(void*);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_3d0e3bdf64c5474c3d0278f7fa54c1acea15472ab15d55fcd7f81750d86fc709(Execution,
		*(UObject**)(((SIZE_T)Parms) + ParmOffset_0_WorldContextObject),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_1_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void StorePrimitiveArg(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:239:2
 * StableFunctionKey  : 8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf
 * ExecutionHash      : 230549cd5accaddff4860f1130ae8c0ebcfa519d7ad86076a649f44d7da17ff1
 * DebugHash          : 047bb4235797ff9fd447955084df475bb2a8c887d3d8a689ebc3a27e31217302
 * EntryAbiHash       : 59c253776ca706e4b802002aa63a0562f899465e5e8dcb6976ad0e14745f555a
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_230549cd5accaddff4860f1130ae8c0ebcfa519d7ad86076a649f44d7da17ff1 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_230549cd5accaddff4860f1130ae8c0ebcfa519d7ad86076a649f44d7da17ff1
#endif

// AS Function : void StorePrimitiveArg(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:239:2
// JIT Entry   : Raw
void ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_230549cd5accaddff4860f1130ae8c0ebcfa519d7ad86076a649f44d7da17ff1(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("void UStaticJITAotFunctionCarrier::StorePrimitiveArg(int)", 240);
SCRIPT_ASSUME_NO_EXCEPTION()
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
// SUSPEND
// JitEntry *
// ADDIi v2, v-2, 3
((int32&)v_TEMP_dword_2) = ((int32&)p_Value) + value_as<int>((asDWORD)0x3u);
// LoadThisR +48
l_valueRegister = ((asQWORD)l_This) + POFFSET_UStaticJITAotFunctionCarrier_StoredValue;
// WRTV4 v2
memcpy((void*)l_valueRegister, (void*)(&v_TEMP_dword_2), 4);
// RET 3
  return;
}
// AS Function : void StorePrimitiveArg(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:239:2
// JIT Entry   : VM
void ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_230549cd5accaddff4860f1130ae8c0ebcfa519d7ad86076a649f44d7da17ff1_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_230549cd5accaddff4860f1130ae8c0ebcfa519d7ad86076a649f44d7da17ff1(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : void StorePrimitiveArg(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:239:2
// JIT Entry   : Parms
void ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_230549cd5accaddff4860f1130ae8c0ebcfa519d7ad86076a649f44d7da17ff1_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_230549cd5accaddff4860f1130ae8c0ebcfa519d7ad86076a649f44d7da17ff1(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ExceptionEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:191:2
 * StableFunctionKey  : 91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277
 * ExecutionHash      : 20e7cecf29d10a4a764cb5cb44b8a451e3c28179f942e1915a1ed050f2bc2658
 * DebugHash          : 457781b0af7fcc5065af4c327569c6e850eb1bd67892ebd4c7870c1d9317a9c3
 * EntryAbiHash       : 8e204c65d868a3511f949a72a8040d1e58f2c414351b97e6ddef4ad8bc9d5b08
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_20e7cecf29d10a4a764cb5cb44b8a451e3c28179f942e1915a1ed050f2bc2658 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_20e7cecf29d10a4a764cb5cb44b8a451e3c28179f942e1915a1ed050f2bc2658
#endif

// AS Function : int ExceptionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:191:2
// JIT Entry   : Raw
int32 ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_20e7cecf29d10a4a764cb5cb44b8a451e3c28179f942e1915a1ed050f2bc2658(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::ExceptionEntryForAOT()", 192);
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
// JitEntry *
// SUSPEND
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
// JMP 0
goto LABEL_UStaticJITAotFunctionCarrier__ExceptionEntryForAOT_25;
LABEL_UStaticJITAotFunctionCarrier__ExceptionEntryForAOT_25:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int ExceptionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:191:2
// JIT Entry   : VM
void ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_20e7cecf29d10a4a764cb5cb44b8a451e3c28179f942e1915a1ed050f2bc2658_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_20e7cecf29d10a4a764cb5cb44b8a451e3c28179f942e1915a1ed050f2bc2658(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ExceptionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:191:2
// JIT Entry   : Parms
void ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_20e7cecf29d10a4a764cb5cb44b8a451e3c28179f942e1915a1ed050f2bc2658_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_20e7cecf29d10a4a764cb5cb44b8a451e3c28179f942e1915a1ed050f2bc2658(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int RecurseForAOT(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:218:2
 * StableFunctionKey  : 947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7
 * ExecutionHash      : 5b1c8d8da0f0fd29d8b87de4f735a6a7caa732b760418a27ffd38e7f87f1180f
 * DebugHash          : 1cfdbc08392d3b284fecf05b3856bc48d9b6998177c4017d4a9a6c0d409d579f
 * EntryAbiHash       : 805d2b5d622dfc1d79e7a6e1f19c0e4068732e3afa951e3f3b429e924ed5af85
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_5b1c8d8da0f0fd29d8b87de4f735a6a7caa732b760418a27ffd38e7f87f1180f = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_5b1c8d8da0f0fd29d8b87de4f735a6a7caa732b760418a27ffd38e7f87f1180f
#endif

// AS Function : int RecurseForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:218:2
// JIT Entry   : Raw
int32 ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_5b1c8d8da0f0fd29d8b87de4f735a6a7caa732b760418a27ffd38e7f87f1180f(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::RecurseForAOT(int)", 219);
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
asBYTE v_TEMP_byte_3 = {};
asDWORD v_TEMP_dword_4 = {};
asDWORD v_TEMP_dword_5 = {};
asDWORD v_TEMP_dword_6 = {};
asDWORD v_TEMP_dword_7 = {};
asDWORD v_TEMP_dword_8 = {};
asDWORD v_TEMP_dword_9 = {};
asDWORD v_TEMP_dword_10 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// CMPIi v-2, 0
  if (((int32&)p_Value) == value_as<int>((asDWORD)0x0u))     l_byteRegister = 0;
  else if (((int32&)p_Value) < value_as<int>((asDWORD)0x0u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JNZ 12
if(l_byteRegister != 0) {
goto LABEL_UStaticJITAotFunctionCarrier__RecurseForAOT_27;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v1, 0
v_TEMP_dword_1 = 0x0u;
// JMP 22
goto LABEL_UStaticJITAotFunctionCarrier__RecurseForAOT_49;
LABEL_UStaticJITAotFunctionCarrier__RecurseForAOT_27:
// SUSPEND
// JitEntry *
// SUBIi v6, v-2, 1
((int32&)v_TEMP_dword_6) = ((int32&)p_Value) - value_as<int>((asDWORD)0x1u);
// PshV4 v6
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
  CallContext->SetArgDWord(0, value_as<asDWORD>((v_TEMP_dword_6)));
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
// CpyRtoV4 v7
memcpy((&v_TEMP_dword_7), &l_valueRegister, 4);
// MULIi v9, v7, 10
((int32&)v_TEMP_dword_9) = ((int32&)v_TEMP_dword_7) * value_as<int>((asDWORD)0xau);
// ADDi v1, v-2, v9
((int32&)v_TEMP_dword_1) = ((int32&)p_Value) + ((int32&)v_TEMP_dword_9);
// JMP 0
goto LABEL_UStaticJITAotFunctionCarrier__RecurseForAOT_49;
LABEL_UStaticJITAotFunctionCarrier__RecurseForAOT_49:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 3
  return (int32)l_dwordRegister;
}
// AS Function : int RecurseForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:218:2
// JIT Entry   : VM
void ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_5b1c8d8da0f0fd29d8b87de4f735a6a7caa732b760418a27ffd38e7f87f1180f_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_5b1c8d8da0f0fd29d8b87de4f735a6a7caa732b760418a27ffd38e7f87f1180f(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : int RecurseForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:218:2
// JIT Entry   : Parms
void ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_5b1c8d8da0f0fd29d8b87de4f735a6a7caa732b760418a27ffd38e7f87f1180f_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_5b1c8d8da0f0fd29d8b87de4f735a6a7caa732b760418a27ffd38e7f87f1180f(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int GetVirtualValue_Implementation()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:310:3
 * StableFunctionKey  : 980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1
 * ExecutionHash      : c0d1516c730f11263fb6e0c2f910f62bc31032d339a324a921bfbc9df05b47a4
 * DebugHash          : e54666948f8d891ad717d240af94ceeac1e495d38bffc818455fe58ffdee2d1e
 * EntryAbiHash       : 507cc902edc1d9bd3e2f2572fe86bb7d02b96a550445798d3f067222eb350188
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_c0d1516c730f11263fb6e0c2f910f62bc31032d339a324a921bfbc9df05b47a4 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_c0d1516c730f11263fb6e0c2f910f62bc31032d339a324a921bfbc9df05b47a4
#endif

// AS Function : int GetVirtualValue_Implementation()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:310:3
// JIT Entry   : Raw
int32 ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_c0d1516c730f11263fb6e0c2f910f62bc31032d339a324a921bfbc9df05b47a4(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotVirtualChild::GetVirtualValue_Implementation()", 311);
SCRIPT_ASSUME_NO_EXCEPTION()
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
// SUSPEND
// JitEntry *
// SetV4 v1, 217
v_TEMP_dword_1 = 0xd9u;
// JMP 0
goto LABEL_UStaticJITAotVirtualChild__GetVirtualValue_Implementation_15;
LABEL_UStaticJITAotVirtualChild__GetVirtualValue_Implementation_15:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int GetVirtualValue_Implementation()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:310:3
// JIT Entry   : VM
void ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_c0d1516c730f11263fb6e0c2f910f62bc31032d339a324a921bfbc9df05b47a4_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_c0d1516c730f11263fb6e0c2f910f62bc31032d339a324a921bfbc9df05b47a4(Execution,
		*(UObject**)l_fp);
}
// AS Function : int GetVirtualValue_Implementation()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:310:3
// JIT Entry   : Parms
void ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_c0d1516c730f11263fb6e0c2f910f62bc31032d339a324a921bfbc9df05b47a4_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_c0d1516c730f11263fb6e0c2f910f62bc31032d339a324a921bfbc9df05b47a4(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int BumpReferenceForAOT(int&inout)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:115:1
 * StableFunctionKey  : 9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05
 * ExecutionHash      : 55fb5feb08ae420f144b40ae0750e2ebbb949e917c653ebbc5c168d146352d67
 * DebugHash          : 61730d1c8d64ab867e0e003e019fdca7bdb33c769181bd02a3552044753edb2e
 * EntryAbiHash       : b80efaaa778ead65430c5702610f23a7b6dccd414f3c044078aa45e30f15b055
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_55fb5feb08ae420f144b40ae0750e2ebbb949e917c653ebbc5c168d146352d67 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_55fb5feb08ae420f144b40ae0750e2ebbb949e917c653ebbc5c168d146352d67
#endif

// AS Function : int BumpReferenceForAOT(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:115:1
// JIT Entry   : Raw
int32 ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_55fb5feb08ae420f144b40ae0750e2ebbb949e917c653ebbc5c168d146352d67(FScriptExecution& Execution, int32* p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int BumpReferenceForAOT(int&)", 116);
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
asDWORD v_TEMP_dword_3 = {};
asDWORD v_TEMP_dword_4 = {};
asDWORD v_TEMP_dword_5 = {};
asDWORD v_TEMP_dword_6 = {};
asDWORD v_TEMP_dword_7 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// PshVPtr v0
// PopRPtr
l_valueRegister = (asQWORD)(p_Value);
// RDR4 v3
v_TEMP_dword_3 = value_read<asDWORD>((void*)l_valueRegister);
// ADDIi v4, v3, 5
((int32&)v_TEMP_dword_4) = ((int32&)v_TEMP_dword_3) + value_as<int>((asDWORD)0x5u);
// PshVPtr v0
// PopRPtr
l_valueRegister = (asQWORD)(p_Value);
// WRTV4 v4
memcpy((void*)l_valueRegister, (void*)(&v_TEMP_dword_4), 4);
// SUSPEND
// JitEntry *
// PshVPtr v0
// PopRPtr
l_valueRegister = (asQWORD)(p_Value);
// RDR4 v5
v_TEMP_dword_5 = value_read<asDWORD>((void*)l_valueRegister);
// MULIi v1, v5, 2
((int32&)v_TEMP_dword_1) = ((int32&)v_TEMP_dword_5) * value_as<int>((asDWORD)0x2u);
// JMP 0
goto LABEL_BumpReferenceForAOT_32;
LABEL_BumpReferenceForAOT_32:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int BumpReferenceForAOT(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:115:1
// JIT Entry   : VM
void ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_55fb5feb08ae420f144b40ae0750e2ebbb949e917c653ebbc5c168d146352d67_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_55fb5feb08ae420f144b40ae0750e2ebbb949e917c653ebbc5c168d146352d67(Execution,
		*(int32**)(l_fp + 0));
}
// AS Function : int BumpReferenceForAOT(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:115:1
// JIT Entry   : Parms
void ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_55fb5feb08ae420f144b40ae0750e2ebbb949e917c653ebbc5c168d146352d67_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_55fb5feb08ae420f144b40ae0750e2ebbb949e917c653ebbc5c168d146352d67(Execution,
		(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ImportEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:149:1
 * StableFunctionKey  : 9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a
 * ExecutionHash      : 0c971fc0692d199bc0d046ef716a6c7d92539d65a54825f01bb3c47e0c503da3
 * DebugHash          : c74518ee893a84744cf1042dbb9099dca87703fe2827c83ee7465592d467ab61
 * EntryAbiHash       : 4b531289681eab0fc4f4f249dee5c27f1a8dedc1b24ed061e98876f5dedce042
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_0c971fc0692d199bc0d046ef716a6c7d92539d65a54825f01bb3c47e0c503da3 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_0c971fc0692d199bc0d046ef716a6c7d92539d65a54825f01bb3c47e0c503da3
#endif

// AS Function : int ImportEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:149:1
// JIT Entry   : Raw
int32 ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_0c971fc0692d199bc0d046ef716a6c7d92539d65a54825f01bb3c47e0c503da3(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int ImportEntryForAOT()", 150);
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
asDWORD v_TEMP_dword_3 = {};
// JitEntry *
// SUSPEND
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
// CpyRtoV4 v3
memcpy((&v_TEMP_dword_3), &l_valueRegister, 4);
// CpyVtoV4 v1, v3
v_TEMP_dword_1 = v_TEMP_dword_3;
// JMP 0
goto LABEL_ImportEntryForAOT_23;
LABEL_ImportEntryForAOT_23:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 0
  return (int32)l_dwordRegister;
}
// AS Function : int ImportEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:149:1
// JIT Entry   : VM
void ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_0c971fc0692d199bc0d046ef716a6c7d92539d65a54825f01bb3c47e0c503da3_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_0c971fc0692d199bc0d046ef716a6c7d92539d65a54825f01bb3c47e0c503da3(Execution);
}
// AS Function : int ImportEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:149:1
// JIT Entry   : Parms
void ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_0c971fc0692d199bc0d046ef716a6c7d92539d65a54825f01bb3c47e0c503da3_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_0c971fc0692d199bc0d046ef716a6c7d92539d65a54825f01bb3c47e0c503da3(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ThreadSafeReturn()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:278:2
 * StableFunctionKey  : 9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7
 * ExecutionHash      : 4efb79e3d87bb35b63583075374a3a1c52d0eff1027947ff813464c74bd8277f
 * DebugHash          : e7473ec9ec9b4b940cf68766688a9649762e3ee929fe524763ae67f3a9cfe0a5
 * EntryAbiHash       : dc83c109706793b798e437d104797c6bd2946d45dd2a7a22bc5dc94aa6809a03
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_4efb79e3d87bb35b63583075374a3a1c52d0eff1027947ff813464c74bd8277f = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_4efb79e3d87bb35b63583075374a3a1c52d0eff1027947ff813464c74bd8277f
#endif

// AS Function : int ThreadSafeReturn()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:278:2
// JIT Entry   : Raw
int32 ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_4efb79e3d87bb35b63583075374a3a1c52d0eff1027947ff813464c74bd8277f(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::ThreadSafeReturn()", 279);
SCRIPT_ASSUME_NO_EXCEPTION()
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
// SUSPEND
// JitEntry *
// SetV4 v1, 73
v_TEMP_dword_1 = 0x49u;
// JMP 0
goto LABEL_UStaticJITAotFunctionCarrier__ThreadSafeReturn_15;
LABEL_UStaticJITAotFunctionCarrier__ThreadSafeReturn_15:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int ThreadSafeReturn()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:278:2
// JIT Entry   : VM
void ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_4efb79e3d87bb35b63583075374a3a1c52d0eff1027947ff813464c74bd8277f_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_4efb79e3d87bb35b63583075374a3a1c52d0eff1027947ff813464c74bd8277f(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ThreadSafeReturn()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:278:2
// JIT Entry   : Parms
void ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_4efb79e3d87bb35b63583075374a3a1c52d0eff1027947ff813464c74bd8277f_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_4efb79e3d87bb35b63583075374a3a1c52d0eff1027947ff813464c74bd8277f(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void IncrementReference(int&inout)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:251:2
 * StableFunctionKey  : 9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e
 * ExecutionHash      : de25e57b367c49e1044cf4a0633f7e993c1e69df1cf5e3b5d99aeb5d3052a383
 * DebugHash          : c9a162bfdf5dd6f229c9570ca849aee5e6308c94cb661eabd653b8f04b6a62e1
 * EntryAbiHash       : edfd275150d177872c0521a27feaed2db8935ff989a49d20f43a3627044ec161
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_de25e57b367c49e1044cf4a0633f7e993c1e69df1cf5e3b5d99aeb5d3052a383 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_de25e57b367c49e1044cf4a0633f7e993c1e69df1cf5e3b5d99aeb5d3052a383
#endif

// AS Function : void IncrementReference(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:251:2
// JIT Entry   : Raw
void ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_de25e57b367c49e1044cf4a0633f7e993c1e69df1cf5e3b5d99aeb5d3052a383(FScriptExecution& Execution, UObject* l_This, int32* p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("void UStaticJITAotFunctionCarrier::IncrementReference(int&)", 252);
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
asDWORD v_TEMP_dword_3 = {};
asDWORD v_TEMP_dword_4 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// PshVPtr v-2
// PopRPtr
l_valueRegister = (asQWORD)(p_Value);
// RDR4 v2
v_TEMP_dword_2 = value_read<asDWORD>((void*)l_valueRegister);
// ADDIi v3, v2, 9
((int32&)v_TEMP_dword_3) = ((int32&)v_TEMP_dword_2) + value_as<int>((asDWORD)0x9u);
// PshVPtr v-2
// PopRPtr
l_valueRegister = (asQWORD)(p_Value);
// WRTV4 v3
memcpy((void*)l_valueRegister, (void*)(&v_TEMP_dword_3), 4);
// SUSPEND
// JitEntry *
// PshVPtr v-2
// PopRPtr
l_valueRegister = (asQWORD)(p_Value);
// RDR4 v4
v_TEMP_dword_4 = value_read<asDWORD>((void*)l_valueRegister);
// LoadThisR +48
l_valueRegister = ((asQWORD)l_This) + POFFSET_UStaticJITAotFunctionCarrier_StoredValue;
// WRTV4 v4
memcpy((void*)l_valueRegister, (void*)(&v_TEMP_dword_4), 4);
// RET 4
  return;
}
// AS Function : void IncrementReference(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:251:2
// JIT Entry   : VM
void ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_de25e57b367c49e1044cf4a0633f7e993c1e69df1cf5e3b5d99aeb5d3052a383_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_de25e57b367c49e1044cf4a0633f7e993c1e69df1cf5e3b5d99aeb5d3052a383(Execution,
		*(UObject**)l_fp,
		*(int32**)(l_fp + 2));
}
// AS Function : void IncrementReference(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:251:2
// JIT Entry   : Parms
void ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_de25e57b367c49e1044cf4a0633f7e993c1e69df1cf5e3b5d99aeb5d3052a383_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_de25e57b367c49e1044cf4a0633f7e993c1e69df1cf5e3b5d99aeb5d3052a383(Execution,
		(UObject*)Object,
		(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void ClientStoreRpcValue(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:283:2
 * StableFunctionKey  : a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9
 * ExecutionHash      : bf18f126dd3c0cd82310467023fa436305cc8de8eb06f0f226b85dd687040a02
 * DebugHash          : 37305d08863c6604530fe29fddaca9b6ffa0936438afc12cce0cb16eec06e2af
 * EntryAbiHash       : 97c4a3b9d12b23ba1b0401fc6dc59e5b24fabb6eb7c3ce760a58844d3692b2f9
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9_bf18f126dd3c0cd82310467023fa436305cc8de8eb06f0f226b85dd687040a02 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9_bf18f126dd3c0cd82310467023fa436305cc8de8eb06f0f226b85dd687040a02
#endif

// AS Function : void ClientStoreRpcValue(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:283:2
// JIT Entry   : Raw
void ASJIT_a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9_bf18f126dd3c0cd82310467023fa436305cc8de8eb06f0f226b85dd687040a02(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
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
alignas(4) asBYTE MEM_v_TEMP_6[sizeof(FName)];
FName& v_TEMP_6 = (FName&)MEM_v_TEMP_6[0];
void* v_TEMP_2 = nullptr;
void* v_TEMP_8 = nullptr;
asDWORD v_TEMP_dword_3 = {};
alignas(16) asBYTE l_ParmStruct[4];
// JitEntry *
// SUSPEND
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
// PopRPtr
l_valueRegister = (asQWORD)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 7u));
// CpyRtoV8 v2
v_TEMP_2 = (void*)l_valueRegister;
// PshVPtr v2
// PshC4 0
// PSF v6
// CALLSYS *
// FName __STATIC_NAME(int Id, const FString&inout CanonicalName)
{
new(((void*)((&v_TEMP_6)))) FName(FAngelscriptEngine::ResolveStaticName(value_as<int32>(((asDWORD)0x0u)), (*(FString*)(v_TEMP_2))));
}
// JitEntry *
// PSF v6
// PopRPtr
l_valueRegister = (asQWORD)((&v_TEMP_6));
// CpyRtoV8 v8
v_TEMP_8 = (void*)l_valueRegister;
// PshVPtr v8
// PshV8 v0
// CALLSYS *
// void __Evt_Execute(const UObject Object, const FName&inout Name)
{
UObject* EventObject = ((UObject*)(l_This));
UFunction* EventFunction = EventObject->FindFunctionChecked(*(FName*)((v_TEMP_8)));
EventObject->ProcessEvent(EventFunction, &l_ParmStruct[0]);
}
// JitEntry *
// RET 3
  return;
}
// AS Function : void ClientStoreRpcValue(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:283:2
// JIT Entry   : VM
void ASJIT_a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9_bf18f126dd3c0cd82310467023fa436305cc8de8eb06f0f226b85dd687040a02_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9_bf18f126dd3c0cd82310467023fa436305cc8de8eb06f0f226b85dd687040a02(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : void ClientStoreRpcValue(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:283:2
// JIT Entry   : Parms
void ASJIT_a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9_bf18f126dd3c0cd82310467023fa436305cc8de8eb06f0f226b85dd687040a02_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ASJIT_a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9_bf18f126dd3c0cd82310467023fa436305cc8de8eb06f0f226b85dd687040a02(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:50:1
 * StableFunctionKey  : a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b
 * ExecutionHash      : e75fe728e40dc52aa18508e8758d65b02d0c622da99b6e53f885a65d7dd31bf5
 * DebugHash          : f1675cc2d072786fb7cbe4777d1caac574993ce75d470854da1303c4a98ee287
 * EntryAbiHash       : 315429bd98bc57e8264be8ad0e7bc5007a550d671431aa6b3c1f76f34b2aa667
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_e75fe728e40dc52aa18508e8758d65b02d0c622da99b6e53f885a65d7dd31bf5_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d37,
	const int32 as_ast_d38,
	const bool as_ast_d39,
	const bool as_ast_d40);

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
static int32 ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_e75fe728e40dc52aa18508e8758d65b02d0c622da99b6e53f885a65d7dd31bf5_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d37,
	const int32 as_ast_d38,
	const bool as_ast_d39,
	const bool as_ast_d40)
{
	int32 as_ast_d41;
	{
		TypedFrame.SetSourcePosition(56, 6);
		([&]() -> int32
		{
			const int32 as_ast_e63_value = ([&]() -> int32
		{
			const int32 as_ast_e61_left = as_ast_d37;
			const int32 as_ast_e61_right = as_ast_d38;
			return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e61_left, as_ast_e61_right);
		}());
			int32& as_ast_e63_target = as_ast_d41;
			as_ast_e63_target = as_ast_e63_value;
			return as_ast_e63_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(57, 2);
		([&]() -> int32
		{
			const int32 as_ast_e68_value = ([&]() -> int32
		{
			const int32 as_ast_e67_left = as_ast_d41;
			const int32 as_ast_e67_right = int32(1);
			return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e67_left, as_ast_e67_right);
		}());
			int32& as_ast_e68_target = as_ast_d41;
			as_ast_e68_target = as_ast_e68_value;
			return as_ast_e68_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(58, 2);
		([&]() -> int32
		{
			const int32 as_ast_e71_value = int32(3);
			int32& as_ast_e71_target = as_ast_d41;
			as_ast_e71_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e71_target, as_ast_e71_value);
			return as_ast_e71_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(59, 2);
		([&]() -> int32
		{
			const int32 as_ast_e74_value = int32(1);
			int32& as_ast_e74_target = as_ast_d41;
			as_ast_e74_target = AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e74_target, as_ast_e74_value);
			return as_ast_e74_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(60, 2);
		([&]() -> int32
		{
			const int32 as_ast_e77_value = int32(2);
			int32& as_ast_e77_target = as_ast_d41;
			as_ast_e77_target = AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e77_target, as_ast_e77_value);
			return as_ast_e77_target;
		}());
	}
	int32 as_ast_d42;
	{
		TypedFrame.SetSourcePosition(62, 6);
		([&]() -> int32
		{
			const int32 as_ast_e82_value = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d41);
			int32& as_ast_e82_target = as_ast_d42;
			as_ast_e82_target = as_ast_e82_value;
			return as_ast_e82_target;
		}());
	}
	int32 as_ast_d43;
	{
		TypedFrame.SetSourcePosition(63, 6);
		([&]() -> int32
		{
			const int32 as_ast_e87_value = AngelscriptTypedASTJIT::WrapPostIncrement<int32>(as_ast_d41);
			int32& as_ast_e87_target = as_ast_d43;
			as_ast_e87_target = as_ast_e87_value;
			return as_ast_e87_target;
		}());
	}
	int32 as_ast_d44;
	{
		TypedFrame.SetSourcePosition(64, 6);
		([&]() -> int32
		{
			const int32 as_ast_e91_value = AngelscriptTypedASTJIT::WrapNegate<int32>(as_ast_d42);
			int32& as_ast_e91_target = as_ast_d44;
			as_ast_e91_target = as_ast_e91_value;
			return as_ast_e91_target;
		}());
	}
	int32 as_ast_d45;
	{
		TypedFrame.SetSourcePosition(65, 6);
		([&]() -> int32
		{
			const int32 as_ast_e95_value = AngelscriptTypedASTJIT::BitwiseNot<int32>(as_ast_d43);
			int32& as_ast_e95_target = as_ast_d45;
			as_ast_e95_target = as_ast_e95_value;
			return as_ast_e95_target;
		}());
	}
	int32 as_ast_d46;
	{
		TypedFrame.SetSourcePosition(66, 6);
		([&]() -> int32
		{
			const int32 as_ast_e100_value = ([&]() -> int32
		{
			const int32 as_ast_e98_left = as_ast_d41;
			const int32 as_ast_e98_right = int32(1);
			return AngelscriptTypedASTJIT::ShiftLeft<int32>(as_ast_e98_left, as_ast_e98_right);
		}());
			int32& as_ast_e100_target = as_ast_d46;
			as_ast_e100_target = as_ast_e100_value;
			return as_ast_e100_target;
		}());
	}
	int32 as_ast_d47;
	{
		TypedFrame.SetSourcePosition(67, 6);
		([&]() -> int32
		{
			const int32 as_ast_e105_value = ([&]() -> int32
		{
			const int32 as_ast_e103_left = as_ast_d41;
			const int32 as_ast_e103_right = int32(1);
			return AngelscriptTypedASTJIT::LogicalShiftRight<int32>(as_ast_e103_left, as_ast_e103_right);
		}());
			int32& as_ast_e105_target = as_ast_d47;
			as_ast_e105_target = as_ast_e105_value;
			return as_ast_e105_target;
		}());
	}
	bool as_ast_d48;
	{
		TypedFrame.SetSourcePosition(69, 7);
		([&]() -> bool
		{
			const bool as_ast_e114_value = (as_ast_d39 && ([&]() -> bool
		{
			const int32 as_ast_e111_left = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d41);
			const int32 as_ast_e111_right = as_ast_d38;
			return (as_ast_e111_left > as_ast_e111_right);
		}()));
			bool& as_ast_e114_target = as_ast_d48;
			as_ast_e114_target = as_ast_e114_value;
			return as_ast_e114_target;
		}());
	}
	bool as_ast_d49;
	{
		TypedFrame.SetSourcePosition(70, 7);
		([&]() -> bool
		{
			const bool as_ast_e121_value = ((as_ast_d48 && as_ast_d40) && IsRunningCommandlet());
			bool& as_ast_e121_target = as_ast_d49;
			as_ast_e121_target = as_ast_e121_value;
			return as_ast_e121_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(72, 2);
		if (as_ast_d49)
		{
			{
				TypedFrame.SetSourcePosition(74, 3);
				return ([&]() -> int32
			{
				const int32 as_ast_e127_left = ([&]() -> int32
			{
				const int32 as_ast_e126_left = as_ast_d46;
				const int32 as_ast_e126_right = as_ast_d45;
				return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e126_left, as_ast_e126_right);
			}());
				const int32 as_ast_e127_right = as_ast_d41;
				return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e127_left, as_ast_e127_right);
			}());
			}
		}
	}
	{
		TypedFrame.SetSourcePosition(76, 2);
		if (([&]() -> bool
		{
			const int32 as_ast_e130_left = as_ast_d47;
			const int32 as_ast_e130_right = int32(10);
			return (as_ast_e130_left > as_ast_e130_right);
		}()))
		{
			{
				TypedFrame.SetSourcePosition(78, 3);
				return ([&]() -> int32
			{
				const int32 as_ast_e135_left = ([&]() -> int32
			{
				const int32 as_ast_e134_left = as_ast_d47;
				const int32 as_ast_e134_right = int32(2);
				return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e134_left, as_ast_e134_right);
			}());
				const int32 as_ast_e135_right = as_ast_d41;
				return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e135_left, as_ast_e135_right);
			}());
			}
		}
	}
	{
		TypedFrame.SetSourcePosition(80, 2);
		return ([&]() -> int32
	{
		const int32 as_ast_e140_left = ([&]() -> int32
	{
		const int32 as_ast_e139_left = as_ast_d44;
		const int32 as_ast_e139_right = int32(1);
		return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e139_left, as_ast_e139_right);
	}());
		const int32 as_ast_e140_right = as_ast_d41;
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e140_left, as_ast_e140_right);
	}());
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:50:1
// JIT Entry   : Raw
int32 ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_e75fe728e40dc52aa18508e8758d65b02d0c622da99b6e53f885a65d7dd31bf5_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1, asBYTE p_2, asBYTE p_3)
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
	return ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_e75fe728e40dc52aa18508e8758d65b02d0c622da99b6e53f885a65d7dd31bf5_TypedBody(TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1), static_cast<bool>(p_2), static_cast<bool>(p_3));
}

// AS Function : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:50:1
// JIT Entry   : VM
void ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_e75fe728e40dc52aa18508e8758d65b02d0c622da99b6e53f885a65d7dd31bf5_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_e75fe728e40dc52aa18508e8758d65b02d0c622da99b6e53f885a65d7dd31bf5_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1),
		*(asBYTE*)(l_fp + 2),
		*(asBYTE*)(l_fp + 3));
}

// AS Function : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:50:1
// JIT Entry   : Parms
void ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_e75fe728e40dc52aa18508e8758d65b02d0c622da99b6e53f885a65d7dd31bf5_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_e75fe728e40dc52aa18508e8758d65b02d0c622da99b6e53f885a65d7dd31bf5_Raw(Execution,
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
 * ExecutionHash      : b37455bde67c162769dd761df3d87526929babe4c49d0f0edd97e9d91c1e1067
 * DebugHash          : 2a582c29651971c2db69d80efdbeb12d3e368a687c4fb50b0a9c972fa8ac2b71
 * EntryAbiHash       : a1d1604b170c8afa6d8aebaea217a68d24d4f67ff1cce99952cd0a22877c25f4
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_b37455bde67c162769dd761df3d87526929babe4c49d0f0edd97e9d91c1e1067 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_b37455bde67c162769dd761df3d87526929babe4c49d0f0edd97e9d91c1e1067
#endif

// AS Function : int64 DoubleToInt64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:88:1
// JIT Entry   : Raw
int64 ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_b37455bde67c162769dd761df3d87526929babe4c49d0f0edd97e9d91c1e1067(FScriptExecution& Execution, double p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int64 DoubleToInt64ForAOT(float)", 89);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asQWORD v_TEMP_qword_2 = {};
asQWORD v_TEMP_qword_4 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// dTOi64 v4, v0
((int64&)v_TEMP_qword_4) = int64(p_Value);
// CpyVtoV8 v2, v4
v_TEMP_qword_2 = v_TEMP_qword_4;
// JMP 0
goto LABEL_DoubleToInt64ForAOT_17;
LABEL_DoubleToInt64ForAOT_17:
// CpyVtoR8 v2
l_valueRegister = v_TEMP_qword_2;
// RET 2
  return (int64)l_valueRegister;
}
// AS Function : int64 DoubleToInt64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:88:1
// JIT Entry   : VM
void ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_b37455bde67c162769dd761df3d87526929babe4c49d0f0edd97e9d91c1e1067_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int64*)l_outValue = ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_b37455bde67c162769dd761df3d87526929babe4c49d0f0edd97e9d91c1e1067(Execution,
		*(double*)(l_fp + 0));
}
// AS Function : int64 DoubleToInt64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:88:1
// JIT Entry   : Parms
void ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_b37455bde67c162769dd761df3d87526929babe4c49d0f0edd97e9d91c1e1067_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(int64));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int64*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_b37455bde67c162769dd761df3d87526929babe4c49d0f0edd97e9d91c1e1067(Execution,
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : uint64 DoubleToUint64ForAOT(const float)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:175:2
 * StableFunctionKey  : aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb
 * ExecutionHash      : b69494644f343f02430f8c5d0b795b1077b4379eb62992ad00460f187af40a2f
 * DebugHash          : ff38bce86c6e06beb71b7dad1dc994504aa61e2ce0cbb1e71bb61a3953e6dd79
 * EntryAbiHash       : 5e708ac1a7afdc6a24646cbb3780e38d2f6639308f316ff6b2ba697920d65899
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_b69494644f343f02430f8c5d0b795b1077b4379eb62992ad00460f187af40a2f = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_b69494644f343f02430f8c5d0b795b1077b4379eb62992ad00460f187af40a2f
#endif

// AS Function : uint64 DoubleToUint64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:175:2
// JIT Entry   : Raw
uint64 ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_b69494644f343f02430f8c5d0b795b1077b4379eb62992ad00460f187af40a2f(FScriptExecution& Execution, UObject* l_This, double p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("uint64 UStaticJITAotFunctionCarrier::DoubleToUint64ForAOT(float)", 176);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asQWORD v_TEMP_qword_2 = {};
asQWORD v_TEMP_qword_4 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// dTOu64 v4, v-2
v_TEMP_qword_4 = uint64(p_Value);
// CpyVtoV8 v2, v4
v_TEMP_qword_2 = v_TEMP_qword_4;
// JMP 0
goto LABEL_UStaticJITAotFunctionCarrier__DoubleToUint64ForAOT_17;
LABEL_UStaticJITAotFunctionCarrier__DoubleToUint64ForAOT_17:
// CpyVtoR8 v2
l_valueRegister = v_TEMP_qword_2;
// RET 4
  return (uint64)l_valueRegister;
}
// AS Function : uint64 DoubleToUint64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:175:2
// JIT Entry   : VM
void ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_b69494644f343f02430f8c5d0b795b1077b4379eb62992ad00460f187af40a2f_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(uint64*)l_outValue = ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_b69494644f343f02430f8c5d0b795b1077b4379eb62992ad00460f187af40a2f(Execution,
		*(UObject**)l_fp,
		*(double*)(l_fp + 2));
}
// AS Function : uint64 DoubleToUint64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:175:2
// JIT Entry   : Parms
void ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_b69494644f343f02430f8c5d0b795b1077b4379eb62992ad00460f187af40a2f_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(uint64));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(uint64*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_b69494644f343f02430f8c5d0b795b1077b4379eb62992ad00460f187af40a2f(Execution,
		(UObject*)Object,
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ReturnPrimitive()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:245:2
 * StableFunctionKey  : b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367
 * ExecutionHash      : 59b407cd05a0ee13399eeac5c22ccf549068e5cad127cd73b21fd5ee1db7f91b
 * DebugHash          : ca7ba0a72b5443e48247070a21bd16ca74447e3b94328ee5560b04d4482ed3fa
 * EntryAbiHash       : 020eef17e1be8204de01770a557fc8e26ac1d841e9e33088abf1b7cb9f3456b4
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_59b407cd05a0ee13399eeac5c22ccf549068e5cad127cd73b21fd5ee1db7f91b = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_59b407cd05a0ee13399eeac5c22ccf549068e5cad127cd73b21fd5ee1db7f91b
#endif

// AS Function : int ReturnPrimitive()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:245:2
// JIT Entry   : Raw
int32 ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_59b407cd05a0ee13399eeac5c22ccf549068e5cad127cd73b21fd5ee1db7f91b(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::ReturnPrimitive()", 246);
SCRIPT_ASSUME_NO_EXCEPTION()
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
// SUSPEND
// JitEntry *
// SetV4 v1, 61
v_TEMP_dword_1 = 0x3du;
// JMP 0
goto LABEL_UStaticJITAotFunctionCarrier__ReturnPrimitive_15;
LABEL_UStaticJITAotFunctionCarrier__ReturnPrimitive_15:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int ReturnPrimitive()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:245:2
// JIT Entry   : VM
void ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_59b407cd05a0ee13399eeac5c22ccf549068e5cad127cd73b21fd5ee1db7f91b_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_59b407cd05a0ee13399eeac5c22ccf549068e5cad127cd73b21fd5ee1db7f91b(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ReturnPrimitive()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:245:2
// JIT Entry   : Parms
void ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_59b407cd05a0ee13399eeac5c22ccf549068e5cad127cd73b21fd5ee1db7f91b_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_59b407cd05a0ee13399eeac5c22ccf549068e5cad127cd73b21fd5ee1db7f91b(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ReferenceEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:203:2
 * StableFunctionKey  : bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726
 * ExecutionHash      : d779d1038af28e1c668f5cb93cc665e5eea4d533ab9caf3732fe479add7e8edc
 * DebugHash          : 1617383ee6aa3fdc678a71c0295ed6bcd6180234868f4cc54be3d2b7d2bdd10a
 * EntryAbiHash       : eb88e947baa4860face1585fa0d6b66fc0a33ee288c7ad728b2271e86a77d1e6
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_d779d1038af28e1c668f5cb93cc665e5eea4d533ab9caf3732fe479add7e8edc = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_d779d1038af28e1c668f5cb93cc665e5eea4d533ab9caf3732fe479add7e8edc
#endif

// AS Function : int ReferenceEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:203:2
// JIT Entry   : Raw
int32 ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_d779d1038af28e1c668f5cb93cc665e5eea4d533ab9caf3732fe479add7e8edc(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::ReferenceEntryForAOT()", 204);
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
asDWORD v_TEMP_dword_4 = {};
asDWORD v_TEMP_dword_5 = {};
asDWORD v_TEMP_dword_6 = {};
asDWORD v_TEMP_dword_7 = {};
asDWORD v_TEMP_dword_8 = {};
// JitEntry *
// SUSPEND
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
SCRIPT_DEBUG_CALLSTACK_POSITION(206, 7);
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
// CpyRtoV4 v5
memcpy((&v_TEMP_dword_5), &l_valueRegister, 4);
// CpyVtoV4 v2, v5
v_Returned = v_TEMP_dword_5;
// SUSPEND
// JitEntry *
// MULIi v7, v1, 100
((int32&)v_TEMP_dword_7) = ((int32&)v_Value) * value_as<int>((asDWORD)0x64u);
// ADDi v3, v7, v2
((int32&)v_TEMP_dword_3) = ((int32&)v_TEMP_dword_7) + ((int32&)v_Returned);
// JMP 0
goto LABEL_UStaticJITAotFunctionCarrier__ReferenceEntryForAOT_38;
LABEL_UStaticJITAotFunctionCarrier__ReferenceEntryForAOT_38:
// CpyVtoR4 v3
l_dwordRegister = v_TEMP_dword_3;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int ReferenceEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:203:2
// JIT Entry   : VM
void ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_d779d1038af28e1c668f5cb93cc665e5eea4d533ab9caf3732fe479add7e8edc_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_d779d1038af28e1c668f5cb93cc665e5eea4d533ab9caf3732fe479add7e8edc(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ReferenceEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:203:2
// JIT Entry   : Parms
void ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_d779d1038af28e1c668f5cb93cc665e5eea4d533ab9caf3732fe479add7e8edc_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_d779d1038af28e1c668f5cb93cc665e5eea4d533ab9caf3732fe479add7e8edc(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UStaticJITAotFunctionCarrier()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:155:7
 * StableFunctionKey  : c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5
 * ExecutionHash      : 05d867ad960bcffaa8e136590ff10084c0a032ac651ed2d0139e49815384ab48
 * DebugHash          : 07e405d77aea925e38770d2bca5f023ff379e779565145ef8975e74b79a96e72
 * EntryAbiHash       : fbabfad5e9e11fb97c083af33dc177e642e8c4e4d0b3a67f937cb34fa1ead35a
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_05d867ad960bcffaa8e136590ff10084c0a032ac651ed2d0139e49815384ab48 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_05d867ad960bcffaa8e136590ff10084c0a032ac651ed2d0139e49815384ab48
#endif

// AS Function : UStaticJITAotFunctionCarrier()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:155:7
// JIT Entry   : Raw
void ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_05d867ad960bcffaa8e136590ff10084c0a032ac651ed2d0139e49815384ab48(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("UStaticJITAotFunctionCarrier::UStaticJITAotFunctionCarrier()", 155);
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
// JitEntry *
// SetV4 v1, 0
v_TEMP_dword_1 = 0x0u;
// SetV4 v2, 0
v_TEMP_dword_2 = 0x0u;
// LoadThisR +48
l_valueRegister = ((asQWORD)l_This) + POFFSET_UStaticJITAotFunctionCarrier_StoredValue;
// WRTV4 v2
memcpy((void*)l_valueRegister, (void*)(&v_TEMP_dword_2), 4);
// SUSPEND
// JitEntry *
// PshVPtr v0
// FinConstruct *
{
  asIScriptObject* Object = (asIScriptObject*)(l_This);
  asITypeInfo* TypeInfo = (asITypeInfo*)FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo(Execution, 0u);
  SCRIPT_FINISH_CONSTRUCT(Object, TypeInfo);
}
// SetV4 v1, 1
v_TEMP_dword_1 = 0x1u;
// RET 2
  return;
}
// AS Function : UStaticJITAotFunctionCarrier()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:155:7
// JIT Entry   : VM
void ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_05d867ad960bcffaa8e136590ff10084c0a032ac651ed2d0139e49815384ab48_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_05d867ad960bcffaa8e136590ff10084c0a032ac651ed2d0139e49815384ab48(Execution,
		*(UObject**)l_fp);
}
// AS Function : UStaticJITAotFunctionCarrier()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:155:7
// JIT Entry   : Parms
void ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_05d867ad960bcffaa8e136590ff10084c0a032ac651ed2d0139e49815384ab48_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_05d867ad960bcffaa8e136590ff10084c0a032ac651ed2d0139e49815384ab48(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int GetStoredValue()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:157:7
 * StableFunctionKey  : c729a9d246ec742f89eb03aeacdc9681edbce4c78fb3dce09c719941be146f24
 * ExecutionHash      : b5ccf1e6a59e1aa4379f6ac60af119f5dfea1ac0fbc59bd399626a6012638af4
 * DebugHash          : 2e3fa190207e2426e02834031bb11e890a2321147118c8d8727778b8d81f5afe
 * EntryAbiHash       : 44e1144686c690ce60aa414a6c45c126690e4049536c603785aaaa3583dffca5
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_c729a9d246ec742f89eb03aeacdc9681edbce4c78fb3dce09c719941be146f24_b5ccf1e6a59e1aa4379f6ac60af119f5dfea1ac0fbc59bd399626a6012638af4 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_c729a9d246ec742f89eb03aeacdc9681edbce4c78fb3dce09c719941be146f24_b5ccf1e6a59e1aa4379f6ac60af119f5dfea1ac0fbc59bd399626a6012638af4
#endif

// AS Function : int GetStoredValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:157:7
// JIT Entry   : Raw
int32 ASJIT_c729a9d246ec742f89eb03aeacdc9681edbce4c78fb3dce09c719941be146f24_b5ccf1e6a59e1aa4379f6ac60af119f5dfea1ac0fbc59bd399626a6012638af4(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::GetStoredValue()", 0);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
asDWORD v_TEMP_dword_2 = {};
// JitEntry *
// LoadThisR +48
l_valueRegister = ((asQWORD)l_This) + POFFSET_UStaticJITAotFunctionCarrier_StoredValue;
// RDR4 v2
v_TEMP_dword_2 = value_read<asDWORD>((void*)l_valueRegister);
// CpyVtoV4 v1, v2
v_TEMP_dword_1 = v_TEMP_dword_2;
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int GetStoredValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:157:7
// JIT Entry   : VM
void ASJIT_c729a9d246ec742f89eb03aeacdc9681edbce4c78fb3dce09c719941be146f24_b5ccf1e6a59e1aa4379f6ac60af119f5dfea1ac0fbc59bd399626a6012638af4_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_c729a9d246ec742f89eb03aeacdc9681edbce4c78fb3dce09c719941be146f24_b5ccf1e6a59e1aa4379f6ac60af119f5dfea1ac0fbc59bd399626a6012638af4(Execution,
		*(UObject**)l_fp);
}
// AS Function : int GetStoredValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:157:7
// JIT Entry   : Parms
void ASJIT_c729a9d246ec742f89eb03aeacdc9681edbce4c78fb3dce09c719941be146f24_b5ccf1e6a59e1aa4379f6ac60af119f5dfea1ac0fbc59bd399626a6012638af4_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_c729a9d246ec742f89eb03aeacdc9681edbce4c78fb3dce09c719941be146f24_b5ccf1e6a59e1aa4379f6ac60af119f5dfea1ac0fbc59bd399626a6012638af4(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UStaticJITAotVirtualParent UStaticJITAotVirtualParent()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:292:7
 * StableFunctionKey  : dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844
 * ExecutionHash      : 669075fa1b88639ff6b154b468b54891e1c0952b734fad3aa4e9dbdc13c59688
 * DebugHash          : c2b51c679e44cce31118901524cbc7ae1be6a98d4f04a75edc10da2254612d55
 * EntryAbiHash       : 75e09447a98126f68ca8f0632eb507b4066408116c83b331b7f0978c55324dfc
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844_669075fa1b88639ff6b154b468b54891e1c0952b734fad3aa4e9dbdc13c59688 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844_669075fa1b88639ff6b154b468b54891e1c0952b734fad3aa4e9dbdc13c59688
#endif

// AS Function : UStaticJITAotVirtualParent UStaticJITAotVirtualParent()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:292:7
// JIT Entry   : Raw
UObject* ASJIT_dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844_669075fa1b88639ff6b154b468b54891e1c0952b734fad3aa4e9dbdc13c59688(FScriptExecution& Execution)
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
void* v_TEMP_2 = nullptr;
// JitEntry *
// PSF v2
// ALLOC
{
  asCObjectType* objType = (asCObjectType*)FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo(Execution, 0u);
  asDWORD* mem = (asDWORD*)SCRIPT_ENGINE->AllocScriptObject(objType);
  ScriptObject_Construct(objType, (asCScriptObject*)mem);
  asPWORD* a = (asPWORD*)((&v_TEMP_2));
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
l_objectRegister = (void*)v_TEMP_2;
v_TEMP_2 = nullptr;
// RET 0
  return (UObject*)l_objectRegister;
}
// AS Function : UStaticJITAotVirtualParent UStaticJITAotVirtualParent()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:292:7
// JIT Entry   : VM
void ASJIT_dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844_669075fa1b88639ff6b154b468b54891e1c0952b734fad3aa4e9dbdc13c59688_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(UObject**)l_outValue = ASJIT_dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844_669075fa1b88639ff6b154b468b54891e1c0952b734fad3aa4e9dbdc13c59688(Execution);
}
// AS Function : UStaticJITAotVirtualParent UStaticJITAotVirtualParent()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:292:7
// JIT Entry   : Parms
void ASJIT_dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844_669075fa1b88639ff6b154b468b54891e1c0952b734fad3aa4e9dbdc13c59688_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(UObject**)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844_669075fa1b88639ff6b154b468b54891e1c0952b734fad3aa4e9dbdc13c59688(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ExceptionEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:109:1
 * StableFunctionKey  : e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7
 * ExecutionHash      : 092d01e038d213065ee0fd3a4d4110365afb96791f7e86a2f983ccae7734b263
 * DebugHash          : 4c15cf293cee257c1c902b19779fb99c4d760d0962fa7d9ca99afcfdea7f1871
 * EntryAbiHash       : 8e204c65d868a3511f949a72a8040d1e58f2c414351b97e6ddef4ad8bc9d5b08
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_092d01e038d213065ee0fd3a4d4110365afb96791f7e86a2f983ccae7734b263 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_092d01e038d213065ee0fd3a4d4110365afb96791f7e86a2f983ccae7734b263
#endif

// AS Function : int ExceptionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:109:1
// JIT Entry   : Raw
int32 ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_092d01e038d213065ee0fd3a4d4110365afb96791f7e86a2f983ccae7734b263(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int ExceptionEntryForAOT()", 110);
SCRIPT_ASSUME_NO_EXCEPTION()
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
// JMP 0
goto LABEL_ExceptionEntryForAOT_24;
LABEL_ExceptionEntryForAOT_24:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 0
  return (int32)l_dwordRegister;
}
// AS Function : int ExceptionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:109:1
// JIT Entry   : VM
void ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_092d01e038d213065ee0fd3a4d4110365afb96791f7e86a2f983ccae7734b263_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_092d01e038d213065ee0fd3a4d4110365afb96791f7e86a2f983ccae7734b263(Execution);
}
// AS Function : int ExceptionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:109:1
// JIT Entry   : Parms
void ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_092d01e038d213065ee0fd3a4d4110365afb96791f7e86a2f983ccae7734b263_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_092d01e038d213065ee0fd3a4d4110365afb96791f7e86a2f983ccae7734b263(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void ClientStoreRpcValue_Implementation(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:284:2
 * StableFunctionKey  : edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72
 * ExecutionHash      : dfc57e9efc5a5f13ff95455bbac47e4fbb356cb62fb20eaa1a60bde51785b556
 * DebugHash          : 11bfad5d8fecd048856f2548cc7b28415b2fb48dcc382a37a2dc1818db0159ce
 * EntryAbiHash       : 66bc09dfe9cf807bdbf1d43484d609be4abf09f8f99f244b39083673e669630d
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72_dfc57e9efc5a5f13ff95455bbac47e4fbb356cb62fb20eaa1a60bde51785b556 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72_dfc57e9efc5a5f13ff95455bbac47e4fbb356cb62fb20eaa1a60bde51785b556
#endif

// AS Function : void ClientStoreRpcValue_Implementation(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:284:2
// JIT Entry   : Raw
void ASJIT_edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72_dfc57e9efc5a5f13ff95455bbac47e4fbb356cb62fb20eaa1a60bde51785b556(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("void UStaticJITAotFunctionCarrier::ClientStoreRpcValue_Implementation(int)", 285);
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
// SUSPEND
// JitEntry *
// LoadThisR +48
l_valueRegister = ((asQWORD)l_This) + POFFSET_UStaticJITAotFunctionCarrier_StoredValue;
// WRTV4 v-2
memcpy((void*)l_valueRegister, (void*)(&p_Value), 4);
// RET 3
  return;
}
// AS Function : void ClientStoreRpcValue_Implementation(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:284:2
// JIT Entry   : VM
void ASJIT_edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72_dfc57e9efc5a5f13ff95455bbac47e4fbb356cb62fb20eaa1a60bde51785b556_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72_dfc57e9efc5a5f13ff95455bbac47e4fbb356cb62fb20eaa1a60bde51785b556(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : void ClientStoreRpcValue_Implementation(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:284:2
// JIT Entry   : Parms
void ASJIT_edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72_dfc57e9efc5a5f13ff95455bbac47e4fbb356cb62fb20eaa1a60bde51785b556_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ASJIT_edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72_dfc57e9efc5a5f13ff95455bbac47e4fbb356cb62fb20eaa1a60bde51785b556(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int RecurseForAOT(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:135:1
 * StableFunctionKey  : f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556
 * ExecutionHash      : db188b69eab01cce39a0449ee79719174c713a31e6b31096c0c14cb439e7d6d8
 * DebugHash          : 5dd1230c1542325cf1bb89b6035fb36fc21e770c047cdb42a59d7633b0fa7b42
 * EntryAbiHash       : 805d2b5d622dfc1d79e7a6e1f19c0e4068732e3afa951e3f3b429e924ed5af85
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_db188b69eab01cce39a0449ee79719174c713a31e6b31096c0c14cb439e7d6d8 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_db188b69eab01cce39a0449ee79719174c713a31e6b31096c0c14cb439e7d6d8
#endif

// AS Function : int RecurseForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:135:1
// JIT Entry   : Raw
int32 ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_db188b69eab01cce39a0449ee79719174c713a31e6b31096c0c14cb439e7d6d8(FScriptExecution& Execution, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int RecurseForAOT(int)", 136);
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
asBYTE v_TEMP_byte_3 = {};
asDWORD v_TEMP_dword_4 = {};
asDWORD v_TEMP_dword_5 = {};
asDWORD v_TEMP_dword_6 = {};
asDWORD v_TEMP_dword_7 = {};
asDWORD v_TEMP_dword_8 = {};
asDWORD v_TEMP_dword_9 = {};
asDWORD v_TEMP_dword_10 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// CMPIi v0, 0
  if (((int32&)p_Value) == value_as<int>((asDWORD)0x0u))     l_byteRegister = 0;
  else if (((int32&)p_Value) < value_as<int>((asDWORD)0x0u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JNZ 12
if(l_byteRegister != 0) {
goto LABEL_RecurseForAOT_27;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v1, 0
v_TEMP_dword_1 = 0x0u;
// JMP 21
goto LABEL_RecurseForAOT_48;
LABEL_RecurseForAOT_27:
// SUSPEND
// JitEntry *
// SUBIi v6, v0, 1
((int32&)v_TEMP_dword_6) = ((int32&)p_Value) - value_as<int>((asDWORD)0x1u);
// PshV4 v6
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
  CallContext->SetArgDWord(0, value_as<asDWORD>((v_TEMP_dword_6)));
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
// CpyRtoV4 v7
memcpy((&v_TEMP_dword_7), &l_valueRegister, 4);
// MULIi v9, v7, 10
((int32&)v_TEMP_dword_9) = ((int32&)v_TEMP_dword_7) * value_as<int>((asDWORD)0xau);
// ADDi v1, v0, v9
((int32&)v_TEMP_dword_1) = ((int32&)p_Value) + ((int32&)v_TEMP_dword_9);
// JMP 0
goto LABEL_RecurseForAOT_48;
LABEL_RecurseForAOT_48:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 1
  return (int32)l_dwordRegister;
}
// AS Function : int RecurseForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:135:1
// JIT Entry   : VM
void ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_db188b69eab01cce39a0449ee79719174c713a31e6b31096c0c14cb439e7d6d8_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_db188b69eab01cce39a0449ee79719174c713a31e6b31096c0c14cb439e7d6d8(Execution,
		*(asDWORD*)(l_fp + 0));
}
// AS Function : int RecurseForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:135:1
// JIT Entry   : Parms
void ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_db188b69eab01cce39a0449ee79719174c713a31e6b31096c0c14cb439e7d6d8_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_db188b69eab01cce39a0449ee79719174c713a31e6b31096c0c14cb439e7d6d8(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ObjectLastNativeForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:98:1
 * StableFunctionKey  : f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df
 * ExecutionHash      : ea9214bb9fe17fc7d7e837dc17801f34fce77699cd0da30b512894b51e820be8
 * DebugHash          : 9dcb4478a5a07fca181ed5278c6da4af509cc3a360da37bcbfdd31c04dee8073
 * EntryAbiHash       : 37241b3ba70d4b0f0af53358d5dcd4fda67f64c6db1715fe217a46674e3ba526
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_ea9214bb9fe17fc7d7e837dc17801f34fce77699cd0da30b512894b51e820be8 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_ea9214bb9fe17fc7d7e837dc17801f34fce77699cd0da30b512894b51e820be8
#endif

// AS Function : int ObjectLastNativeForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:98:1
// JIT Entry   : Raw
int32 ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_ea9214bb9fe17fc7d7e837dc17801f34fce77699cd0da30b512894b51e820be8(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int ObjectLastNativeForAOT()", 99);
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
void* v_TEMP_6 = nullptr;
asDWORD v_TEMP_dword_2 = {};
asDWORD v_TEMP_dword_3 = {};
asDWORD v_TEMP_dword_4 = {};
asDWORD v_TEMP_dword_7 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// PshC4 97
// PshC4 39
// PSF v1
// CALLSYS *
// FAotObjectLastProbe::FAotObjectLastProbe(int Left, int Right)
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 0u).GetFunction();
  auto CastedFuncPtr = (void(*)(int32,int32,void*))RawFuncPtr;
  void* Object = (void*)((&v_Value));
SCRIPT_DEBUG_CALLSTACK_POSITION(100, 22);
CastedFuncPtr(value_as<int32>(((asDWORD)0x27u)),value_as<int32>(((asDWORD)0x61u)),Object);
if (Execution.bExceptionThrown) [[unlikely]]
{
return {};
}
}
// JitEntry *
// SUSPEND
// JitEntry *
// PSF v1
// PopRPtr
l_valueRegister = (asQWORD)((&v_Value));
// CpyRtoV8 v6
v_TEMP_6 = (void*)l_valueRegister;
// LoadRObjR
if (v_TEMP_6 == nullptr) [[unlikely]]
{
SCRIPT_DEBUG_CALLSTACK_POSITION(101, 2);
SCRIPT_NULL_POINTER_EXCEPTION();
return {};
}
l_valueRegister = (asQWORD)(v_TEMP_6) + (short)0;
// RDR4 v7
v_TEMP_dword_7 = value_read<asDWORD>((void*)l_valueRegister);
// CpyVtoV4 v2, v7
v_TEMP_dword_2 = v_TEMP_dword_7;
// JMP 0
goto LABEL_ObjectLastNativeForAOT_37;
LABEL_ObjectLastNativeForAOT_37:
// CpyVtoR4 v2
l_dwordRegister = v_TEMP_dword_2;
// RET 0
  return (int32)l_dwordRegister;
}
// AS Function : int ObjectLastNativeForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:98:1
// JIT Entry   : VM
void ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_ea9214bb9fe17fc7d7e837dc17801f34fce77699cd0da30b512894b51e820be8_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_ea9214bb9fe17fc7d7e837dc17801f34fce77699cd0da30b512894b51e820be8(Execution);
}
// AS Function : int ObjectLastNativeForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:98:1
// JIT Entry   : Parms
void ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_ea9214bb9fe17fc7d7e837dc17801f34fce77699cd0da30b512894b51e820be8_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_ea9214bb9fe17fc7d7e837dc17801f34fce77699cd0da30b512894b51e820be8(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int Entry()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:165:3
 * StableFunctionKey  : f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9
 * ExecutionHash      : 94f49659aa74365df6c309336dece23bc3a0ae93774863e3680fedd57e25415c
 * DebugHash          : 2f3436dcfa224c19151744e6a7b4b3e5a8e78461d60bc423ec6d266fffec6ae2
 * EntryAbiHash       : db0c8f3058412e5bdc683b188ff132b140d00caf79e3e0658bda0254082fdee0
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_94f49659aa74365df6c309336dece23bc3a0ae93774863e3680fedd57e25415c = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_94f49659aa74365df6c309336dece23bc3a0ae93774863e3680fedd57e25415c
#endif

// AS Function : int Entry()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:165:3
// JIT Entry   : Raw
int32 ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_94f49659aa74365df6c309336dece23bc3a0ae93774863e3680fedd57e25415c(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::Entry()", 166);
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
asDWORD v_TEMP_dword_3 = {};
// JitEntry *
// SUSPEND
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
// CpyRtoV4 v3
memcpy((&v_TEMP_dword_3), &l_valueRegister, 4);
// CpyVtoV4 v1, v3
v_TEMP_dword_1 = v_TEMP_dword_3;
// JMP 0
goto LABEL_UStaticJITAotFunctionCarrier__Entry_24;
LABEL_UStaticJITAotFunctionCarrier__Entry_24:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int Entry()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:165:3
// JIT Entry   : VM
void ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_94f49659aa74365df6c309336dece23bc3a0ae93774863e3680fedd57e25415c_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_94f49659aa74365df6c309336dece23bc3a0ae93774863e3680fedd57e25415c(Execution,
		*(UObject**)l_fp);
}
// AS Function : int Entry()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:165:3
// JIT Entry   : Parms
void ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_94f49659aa74365df6c309336dece23bc3a0ae93774863e3680fedd57e25415c_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_94f49659aa74365df6c309336dece23bc3a0ae93774863e3680fedd57e25415c(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticScalarBranch(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:8:1
 * StableFunctionKey  : f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e
 * ExecutionHash      : 0f5683ae0d660ffeda6596b389611da2f939e5142dd9f2cb6e70361b64bb5c7a
 * DebugHash          : dd36673adf2f0fddeec9da86602c6dcf2ffdda827b8affae2b7bb770b420ef5a
 * EntryAbiHash       : 108ad1ef8d5103b1a80d679116c872a1dc9d3ee674beaa1115ec65b288287e69
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e_0f5683ae0d660ffeda6596b389611da2f939e5142dd9f2cb6e70361b64bb5c7a = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e_0f5683ae0d660ffeda6596b389611da2f939e5142dd9f2cb6e70361b64bb5c7a
#endif

// AS Function : int SemanticScalarBranch(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:8:1
// JIT Entry   : Raw
int32 ASJIT_f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e_0f5683ae0d660ffeda6596b389611da2f939e5142dd9f2cb6e70361b64bb5c7a(FScriptExecution& Execution, asDWORD p_A, asDWORD p_B)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int SemanticScalarBranch(int, int)", 9);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_Sum = {};
asDWORD v_TEMP_dword_2 = {};
asDWORD v_TEMP_dword_3 = {};
asDWORD v_TEMP_dword_4 = {};
asBYTE v_TEMP_byte_5 = {};
asDWORD v_TEMP_dword_6 = {};
asDWORD v_TEMP_dword_7 = {};
asDWORD v_TEMP_dword_8 = {};
asDWORD v_TEMP_dword_9 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// ADDi v1, v0, v-1
((int32&)v_Sum) = ((int32&)p_A) + ((int32&)p_B);
// SUSPEND
// JitEntry *
// CMPIi v1, 10
  if (((int32&)v_Sum) == value_as<int>((asDWORD)0xau))     l_byteRegister = 0;
  else if (((int32&)v_Sum) < value_as<int>((asDWORD)0xau)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JNP 13
if(((int8&)l_byteRegister) <= 0) {
goto LABEL_SemanticScalarBranch_34;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// MULIi v2, v1, 2
((int32&)v_TEMP_dword_2) = ((int32&)v_Sum) * value_as<int>((asDWORD)0x2u);
// JMP 9
goto LABEL_SemanticScalarBranch_43;
LABEL_SemanticScalarBranch_34:
// SUSPEND
// JitEntry *
// SUBIi v2, v1, 1
((int32&)v_TEMP_dword_2) = ((int32&)v_Sum) - value_as<int>((asDWORD)0x1u);
// JMP 0
goto LABEL_SemanticScalarBranch_43;
LABEL_SemanticScalarBranch_43:
// CpyVtoR4 v2
l_dwordRegister = v_TEMP_dword_2;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int SemanticScalarBranch(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:8:1
// JIT Entry   : VM
void ASJIT_f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e_0f5683ae0d660ffeda6596b389611da2f939e5142dd9f2cb6e70361b64bb5c7a_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e_0f5683ae0d660ffeda6596b389611da2f939e5142dd9f2cb6e70361b64bb5c7a(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}
// AS Function : int SemanticScalarBranch(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:8:1
// JIT Entry   : Parms
void ASJIT_f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e_0f5683ae0d660ffeda6596b389611da2f939e5142dd9f2cb6e70361b64bb5c7a_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_A = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1_B = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e_0f5683ae0d660ffeda6596b389611da2f939e5142dd9f2cb6e70361b64bb5c7a(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_A),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_1_B));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UStaticJITAotVirtualParent()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:292:7
 * StableFunctionKey  : f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef
 * ExecutionHash      : 352689d396fafa92b08b3f9fecc0326180fd3dc31b1e27896629a6836cf81444
 * DebugHash          : dcaf575d265fa81fbbb1851707583f0c6a6c0ac356205c219f3ff16062049c73
 * EntryAbiHash       : 1075e6d88a0fee09d9074b0d2222d8f08c7b158c0d6f762b28f4e154db4d37e2
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_352689d396fafa92b08b3f9fecc0326180fd3dc31b1e27896629a6836cf81444 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_352689d396fafa92b08b3f9fecc0326180fd3dc31b1e27896629a6836cf81444
#endif

// AS Function : UStaticJITAotVirtualParent()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:292:7
// JIT Entry   : Raw
void ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_352689d396fafa92b08b3f9fecc0326180fd3dc31b1e27896629a6836cf81444(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("UStaticJITAotVirtualParent::UStaticJITAotVirtualParent()", 292);
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
// SetV4 v1, 0
v_TEMP_dword_1 = 0x0u;
// SUSPEND
// JitEntry *
// PshVPtr v0
// FinConstruct *
{
  asIScriptObject* Object = (asIScriptObject*)(l_This);
  asITypeInfo* TypeInfo = (asITypeInfo*)FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo(Execution, 0u);
  SCRIPT_FINISH_CONSTRUCT(Object, TypeInfo);
}
// SetV4 v1, 1
v_TEMP_dword_1 = 0x1u;
// RET 2
  return;
}
// AS Function : UStaticJITAotVirtualParent()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:292:7
// JIT Entry   : VM
void ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_352689d396fafa92b08b3f9fecc0326180fd3dc31b1e27896629a6836cf81444_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_352689d396fafa92b08b3f9fecc0326180fd3dc31b1e27896629a6836cf81444(Execution,
		*(UObject**)l_fp);
}
// AS Function : UStaticJITAotVirtualParent()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:292:7
// JIT Entry   : Parms
void ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_352689d396fafa92b08b3f9fecc0326180fd3dc31b1e27896629a6836cf81444_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_352689d396fafa92b08b3f9fecc0326180fd3dc31b1e27896629a6836cf81444(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int BytecodeFallbackGeneratedWorldContext(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:317:1
 * StableFunctionKey  : f509813c02da6025acae1af26ea86c8f9e3130851803399d0b2db25a166e56ae
 * ExecutionHash      : 17bee61bb8dbf6c3a79c36646733999ddadb1d6a169b4b3755bdb291175da7c6
 * DebugHash          : bf7ade1aaa4f9af46ed3944d39c819f96e0a261496032fb450451ce8e16563bc
 * EntryAbiHash       : d9d9e18805e2fe81f0ff248a92bd9d347eefb34eb7a9fd3b93213e2e4ea0cbbc
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f509813c02da6025acae1af26ea86c8f9e3130851803399d0b2db25a166e56ae_17bee61bb8dbf6c3a79c36646733999ddadb1d6a169b4b3755bdb291175da7c6 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f509813c02da6025acae1af26ea86c8f9e3130851803399d0b2db25a166e56ae_17bee61bb8dbf6c3a79c36646733999ddadb1d6a169b4b3755bdb291175da7c6
#endif

// AS Function : int BytecodeFallbackGeneratedWorldContext(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:317:1
// JIT Entry   : Raw
int32 ASJIT_f509813c02da6025acae1af26ea86c8f9e3130851803399d0b2db25a166e56ae_17bee61bb8dbf6c3a79c36646733999ddadb1d6a169b4b3755bdb291175da7c6(FScriptExecution& Execution, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int BytecodeFallbackGeneratedWorldContext(int)", 318);
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
void* v_TEMP_7 = nullptr;
void* v_TEMP_9 = nullptr;
asDWORD v_TEMP_dword_5 = {};
asDWORD v_TEMP_dword_10 = {};
asDWORD v_TEMP_dword_11 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PopRPtr
l_valueRegister = (asQWORD)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 4u));
// CpyRtoV8 v7
v_TEMP_7 = (void*)l_valueRegister;
// PshVPtr v7
// PSF v4
// CALLSYS *
// FString::FString(const FString&inout Other)
{
  FString* Object = (FString*)((&v_Marker));
new (Object) FString((*(FString*)(v_TEMP_7)));
}
// JitEntry *
// SUSPEND
// JitEntry *
// PSF v4
// PopRPtr
l_valueRegister = (asQWORD)((&v_Marker));
// CpyRtoV8 v9
v_TEMP_9 = (void*)l_valueRegister;
// PshVPtr v9
// CALLSYS *
// int FString::Len() const
{
  FString* Object = (FString*)(v_TEMP_9);
SCRIPT_DEBUG_CALLSTACK_POSITION(320, 2);
  if(Object != nullptr)
  {
int32 FunctionReturnValue = Object->Len();
l_dwordRegister = (asDWORD&)FunctionReturnValue;
} else [[unlikely]] {
goto LABEL_BytecodeFallbackGeneratedWorldContext_EXCEPTION_0;
}
}
// JitEntry *
// CpyRtoV4 v10
v_TEMP_dword_10 = l_dwordRegister;
// ADDi v5, v0, v10
((int32&)v_TEMP_dword_5) = ((int32&)p_Value) + ((int32&)v_TEMP_dword_10);
// PSF v4
// CALLSYS *
// FString::~FString()
{
  FString* Object = (FString*)((&v_Marker));
((FString*)Object)->~FString();
}
// JitEntry *
// JMP 0
goto LABEL_BytecodeFallbackGeneratedWorldContext_50;
LABEL_BytecodeFallbackGeneratedWorldContext_50:
// CpyVtoR4 v5
l_dwordRegister = v_TEMP_dword_5;
// RET 1
  return (int32)l_dwordRegister;
SCRIPT_ASSUME(false);

LABEL_BytecodeFallbackGeneratedWorldContext_EXCEPTION_0:
// Exception cleanup for variables 4
SCRIPT_NULL_POINTER_EXCEPTION();
((FString*)((FString*)(&v_Marker)))->~FString();
return {};
}
// AS Function : int BytecodeFallbackGeneratedWorldContext(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:317:1
// JIT Entry   : VM
void ASJIT_f509813c02da6025acae1af26ea86c8f9e3130851803399d0b2db25a166e56ae_17bee61bb8dbf6c3a79c36646733999ddadb1d6a169b4b3755bdb291175da7c6_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_f509813c02da6025acae1af26ea86c8f9e3130851803399d0b2db25a166e56ae_17bee61bb8dbf6c3a79c36646733999ddadb1d6a169b4b3755bdb291175da7c6(Execution,
		*(asDWORD*)(l_fp + 0));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void ResetStoredValue()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:233:2
 * StableFunctionKey  : f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7
 * ExecutionHash      : 6d86069ad834835513efecd53700b159a5aba4449b09e4fdd4af8ff773ea4297
 * DebugHash          : eb5a90dc11783d0c50d2f7a15473c8ec90687ae7e652827f0ebb56d9cf2875c3
 * EntryAbiHash       : 9b0857b686ac6c0536f6c1b33a7f4ce1dbb76c78831fe5af95fb30829626a6c6
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_6d86069ad834835513efecd53700b159a5aba4449b09e4fdd4af8ff773ea4297 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_6d86069ad834835513efecd53700b159a5aba4449b09e4fdd4af8ff773ea4297
#endif

// AS Function : void ResetStoredValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:233:2
// JIT Entry   : Raw
void ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_6d86069ad834835513efecd53700b159a5aba4449b09e4fdd4af8ff773ea4297(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("void UStaticJITAotFunctionCarrier::ResetStoredValue()", 234);
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
// SUSPEND
// JitEntry *
// SetV4 v1, 71
v_TEMP_dword_1 = 0x47u;
// LoadThisR +48
l_valueRegister = ((asQWORD)l_This) + POFFSET_UStaticJITAotFunctionCarrier_StoredValue;
// WRTV4 v1
memcpy((void*)l_valueRegister, (void*)(&v_TEMP_dword_1), 4);
// RET 2
  return;
}
// AS Function : void ResetStoredValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:233:2
// JIT Entry   : VM
void ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_6d86069ad834835513efecd53700b159a5aba4449b09e4fdd4af8ff773ea4297_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_6d86069ad834835513efecd53700b159a5aba4449b09e4fdd4af8ff773ea4297(Execution,
		*(UObject**)l_fp);
}
// AS Function : void ResetStoredValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:233:2
// JIT Entry   : Parms
void ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_6d86069ad834835513efecd53700b159a5aba4449b09e4fdd4af8ff773ea4297_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_6d86069ad834835513efecd53700b159a5aba4449b09e4fdd4af8ff773ea4297(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int CallVirtualValue()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:300:2
 * StableFunctionKey  : fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce
 * ExecutionHash      : f82de9e78063130fc2475bf4a7e487f60c95a2c93649f4e6f07f48b0c82ea6e5
 * DebugHash          : 8a69deb842af6a278dc2a3b45264a25b2b97265c977f2c756382a61cafe714c1
 * EntryAbiHash       : 6031fb726462dbf41570b41d3d37659d395e860ef3d2bc07067ce4b4bce8b1ad
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_f82de9e78063130fc2475bf4a7e487f60c95a2c93649f4e6f07f48b0c82ea6e5 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_f82de9e78063130fc2475bf4a7e487f60c95a2c93649f4e6f07f48b0c82ea6e5
#endif

// AS Function : int CallVirtualValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:300:2
// JIT Entry   : Raw
int32 ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_f82de9e78063130fc2475bf4a7e487f60c95a2c93649f4e6f07f48b0c82ea6e5(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotVirtualParent::CallVirtualValue()", 301);
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
// JitEntry *
// SUSPEND
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
// CpyRtoV4 v2
memcpy((&v_TEMP_dword_2), &l_valueRegister, 4);
// CpyVtoV4 v1, v2
v_TEMP_dword_1 = v_TEMP_dword_2;
// JMP 0
goto LABEL_UStaticJITAotVirtualParent__CallVirtualValue_22;
LABEL_UStaticJITAotVirtualParent__CallVirtualValue_22:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int CallVirtualValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:300:2
// JIT Entry   : VM
void ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_f82de9e78063130fc2475bf4a7e487f60c95a2c93649f4e6f07f48b0c82ea6e5_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_f82de9e78063130fc2475bf4a7e487f60c95a2c93649f4e6f07f48b0c82ea6e5(Execution,
		*(UObject**)l_fp);
}
// AS Function : int CallVirtualValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:300:2
// JIT Entry   : Parms
void ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_f82de9e78063130fc2475bf4a7e487f60c95a2c93649f4e6f07f48b0c82ea6e5_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_f82de9e78063130fc2475bf4a7e487f60c95a2c93649f4e6f07f48b0c82ea6e5(Execution,
		(UObject*)Object);
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
