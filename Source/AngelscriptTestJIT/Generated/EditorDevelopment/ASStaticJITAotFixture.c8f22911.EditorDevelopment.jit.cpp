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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:93:8
 * StableFunctionKey  : 04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29
 * ExecutionHash      : 5339120f3604a462e8ba7912308525e606a6d0a204187242a0a5c144172ba5b9
 * DebugHash          : 8f04cb6290939de43a8f18e4e17413bd452857ac177d2c49b1cd309f8021e3a3
 * EntryAbiHash       : 5e708ac1a7afdc6a24646cbb3780e38d2f6639308f316ff6b2ba697920d65899
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_5339120f3604a462e8ba7912308525e606a6d0a204187242a0a5c144172ba5b9 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_5339120f3604a462e8ba7912308525e606a6d0a204187242a0a5c144172ba5b9
#endif

// AS Function : uint64 DoubleToUint64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:93:8
// JIT Entry   : Raw
uint64 ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_5339120f3604a462e8ba7912308525e606a6d0a204187242a0a5c144172ba5b9(FScriptExecution& Execution, double p_Value)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:93:8
// JIT Entry   : VM
void ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_5339120f3604a462e8ba7912308525e606a6d0a204187242a0a5c144172ba5b9_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(uint64*)l_outValue = ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_5339120f3604a462e8ba7912308525e606a6d0a204187242a0a5c144172ba5b9(Execution,
		*(double*)(l_fp + 0));
}
// AS Function : uint64 DoubleToUint64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:93:8
// JIT Entry   : Parms
void ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_5339120f3604a462e8ba7912308525e606a6d0a204187242a0a5c144172ba5b9_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(uint64));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(uint64*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_5339120f3604a462e8ba7912308525e606a6d0a204187242a0a5c144172ba5b9(Execution,
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UStaticJITAotVirtualChild UStaticJITAotVirtualChild()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:307:7
 * StableFunctionKey  : 0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe
 * ExecutionHash      : 5ff14395e534561f56d7569924ccefed5ca7db316e28b96e960c783d4963dbbe
 * DebugHash          : c82aa60ae252a364aa725893b2e58a817d9fa934ce295521e1f0b9ba9192efe7
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:307:7
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:307:7
// JIT Entry   : VM
void ASJIT_0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe_5ff14395e534561f56d7569924ccefed5ca7db316e28b96e960c783d4963dbbe_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(UObject**)l_outValue = ASJIT_0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe_5ff14395e534561f56d7569924ccefed5ca7db316e28b96e960c783d4963dbbe(Execution);
}
// AS Function : UStaticJITAotVirtualChild UStaticJITAotVirtualChild()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:307:7
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:28:9
 * StableFunctionKey  : 0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a
 * ExecutionHash      : 7c20fc1d5f0cea700f3b2cfca7daa6805bb89c538f47db70532981ed4d37a7a1
 * DebugHash          : 1245b40c64dbcb43afe5bb9a0e8d68b9ecb20255b8cfbb155f84a8ddc4fb8f62
 * EntryAbiHash       : baec52322166f9d9a578e658e1d6db3e7012f171953f1f761c1b35fa1d15862f
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a_7c20fc1d5f0cea700f3b2cfca7daa6805bb89c538f47db70532981ed4d37a7a1 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a_7c20fc1d5f0cea700f3b2cfca7daa6805bb89c538f47db70532981ed4d37a7a1
#endif

// AS Function : float32 SemanticPowerFloat(const float32, const float32)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:28:9
// JIT Entry   : Raw
float ASJIT_0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a_7c20fc1d5f0cea700f3b2cfca7daa6805bb89c538f47db70532981ed4d37a7a1(FScriptExecution& Execution, float p_Base, float p_Exponent)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:28:9
// JIT Entry   : VM
void ASJIT_0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a_7c20fc1d5f0cea700f3b2cfca7daa6805bb89c538f47db70532981ed4d37a7a1_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(float*)l_outValue = ASJIT_0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a_7c20fc1d5f0cea700f3b2cfca7daa6805bb89c538f47db70532981ed4d37a7a1(Execution,
		*(float*)(l_fp + 0),
		*(float*)(l_fp + 1));
}
// AS Function : float32 SemanticPowerFloat(const float32, const float32)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:28:9
// JIT Entry   : Parms
void ASJIT_0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a_7c20fc1d5f0cea700f3b2cfca7daa6805bb89c538f47db70532981ed4d37a7a1_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Base = ParmsOffset;
	ParmsOffset += sizeof(float);
	ParmsOffset = Align(ParmsOffset, alignof(float));
	const SIZE_T ParmOffset_1_Exponent = ParmsOffset;
	ParmsOffset += sizeof(float);
	ParmsOffset = Align(ParmsOffset, alignof(float));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(float*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_0f4f686d35d79037c5fdb84b4d056398147e22806d5928a3a4ee942458eda86a_7c20fc1d5f0cea700f3b2cfca7daa6805bb89c538f47db70532981ed4d37a7a1(Execution,
		(float)*(float*)(((SIZE_T)Parms) + ParmOffset_0_Base),
		(float)*(float*)(((SIZE_T)Parms) + ParmOffset_1_Exponent));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int RecursionEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:227:6
 * StableFunctionKey  : 13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c
 * ExecutionHash      : 7f40a365adf860490fe5496b5bca66d0169095f834272f631277e2611c561ea9
 * DebugHash          : a75e312fcd267ae0f244c2667bcdc3e8e36e1e68780555fbbeabb711e01ccb9f
 * EntryAbiHash       : 9807be95f7b037b5a524626bf011302bd0844098a4a72bbe407d8cd5e21fa5ae
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_7f40a365adf860490fe5496b5bca66d0169095f834272f631277e2611c561ea9 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_7f40a365adf860490fe5496b5bca66d0169095f834272f631277e2611c561ea9
#endif

// AS Function : int RecursionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:227:6
// JIT Entry   : Raw
int32 ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_7f40a365adf860490fe5496b5bca66d0169095f834272f631277e2611c561ea9(FScriptExecution& Execution, UObject* l_This)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:227:6
// JIT Entry   : VM
void ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_7f40a365adf860490fe5496b5bca66d0169095f834272f631277e2611c561ea9_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_7f40a365adf860490fe5496b5bca66d0169095f834272f631277e2611c561ea9(Execution,
		*(UObject**)l_fp);
}
// AS Function : int RecursionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:227:6
// JIT Entry   : Parms
void ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_7f40a365adf860490fe5496b5bca66d0169095f834272f631277e2611c561ea9_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_7f40a365adf860490fe5496b5bca66d0169095f834272f631277e2611c561ea9(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int GetVirtualValue_Implementation()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:294:7
 * StableFunctionKey  : 191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5
 * ExecutionHash      : c694207e4cdcc4978d3aa940c5f4685e39096d41665cf4dc4bb20949543fae3e
 * DebugHash          : 10e619e4c2017a5a488e22977add9ab4c80d93ba643b3073bb669478f46a0394
 * EntryAbiHash       : 507cc902edc1d9bd3e2f2572fe86bb7d02b96a550445798d3f067222eb350188
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_c694207e4cdcc4978d3aa940c5f4685e39096d41665cf4dc4bb20949543fae3e = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_c694207e4cdcc4978d3aa940c5f4685e39096d41665cf4dc4bb20949543fae3e
#endif

// AS Function : int GetVirtualValue_Implementation()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:294:7
// JIT Entry   : Raw
int32 ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_c694207e4cdcc4978d3aa940c5f4685e39096d41665cf4dc4bb20949543fae3e(FScriptExecution& Execution, UObject* l_This)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:294:7
// JIT Entry   : VM
void ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_c694207e4cdcc4978d3aa940c5f4685e39096d41665cf4dc4bb20949543fae3e_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_c694207e4cdcc4978d3aa940c5f4685e39096d41665cf4dc4bb20949543fae3e(Execution,
		*(UObject**)l_fp);
}
// AS Function : int GetVirtualValue_Implementation()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:294:7
// JIT Entry   : Parms
void ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_c694207e4cdcc4978d3aa940c5f4685e39096d41665cf4dc4bb20949543fae3e_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_c694207e4cdcc4978d3aa940c5f4685e39096d41665cf4dc4bb20949543fae3e(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ObjectLastNativeForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:180:6
 * StableFunctionKey  : 1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f
 * ExecutionHash      : 4078b2d8b80924cfd7c42e37b6d8af63e013c4ace5a2c9b601d62de259d102fa
 * DebugHash          : 7297f894fd5391c7f465ba507eee47b42580102a5196e2d3d50125a5d0468b82
 * EntryAbiHash       : 37241b3ba70d4b0f0af53358d5dcd4fda67f64c6db1715fe217a46674e3ba526
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_4078b2d8b80924cfd7c42e37b6d8af63e013c4ace5a2c9b601d62de259d102fa = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_4078b2d8b80924cfd7c42e37b6d8af63e013c4ace5a2c9b601d62de259d102fa
#endif

// AS Function : int ObjectLastNativeForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:180:6
// JIT Entry   : Raw
int32 ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_4078b2d8b80924cfd7c42e37b6d8af63e013c4ace5a2c9b601d62de259d102fa(FScriptExecution& Execution, UObject* l_This)
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
asDWORD v_TEMP_dword_2 = {};
asDWORD v_TEMP_dword_3 = {};
asDWORD v_TEMP_dword_4 = {};
asDWORD v_TEMP_dword_5 = {};
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
// LoadVObjR
l_valueRegister = (asQWORD)((&v_Value)) + (short)0;
// RDR4 v5
v_TEMP_dword_5 = value_read<asDWORD>((void*)l_valueRegister);
// CpyVtoV4 v2, v5
v_TEMP_dword_2 = v_TEMP_dword_5;
// JMP 0
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLastNativeForAOT_34;
LABEL_UStaticJITAotFunctionCarrier__ObjectLastNativeForAOT_34:
// CpyVtoR4 v2
l_dwordRegister = v_TEMP_dword_2;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int ObjectLastNativeForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:180:6
// JIT Entry   : VM
void ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_4078b2d8b80924cfd7c42e37b6d8af63e013c4ace5a2c9b601d62de259d102fa_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_4078b2d8b80924cfd7c42e37b6d8af63e013c4ace5a2c9b601d62de259d102fa(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ObjectLastNativeForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:180:6
// JIT Entry   : Parms
void ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_4078b2d8b80924cfd7c42e37b6d8af63e013c4ace5a2c9b601d62de259d102fa_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_4078b2d8b80924cfd7c42e37b6d8af63e013c4ace5a2c9b601d62de259d102fa(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void SetStoredValue(int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:157:7
 * StableFunctionKey  : 20afea0dd317110e89dad42ccf8623be8b515d245df9295331cd7ab124381646
 * ExecutionHash      : 1820c81e1e7d6d54a245f42028177fc83949b6a408db5b009470198af27f15b9
 * DebugHash          : 7c22dcbc09d648885e37cd7ed9064d357c70858c5e1b666b74bbb47b79e000ed
 * EntryAbiHash       : 3baea263a239ec0fa05b0b147616ad11e567b95161568aa95e53af4de5301117
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_20afea0dd317110e89dad42ccf8623be8b515d245df9295331cd7ab124381646_1820c81e1e7d6d54a245f42028177fc83949b6a408db5b009470198af27f15b9 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_20afea0dd317110e89dad42ccf8623be8b515d245df9295331cd7ab124381646_1820c81e1e7d6d54a245f42028177fc83949b6a408db5b009470198af27f15b9
#endif

// AS Function : void SetStoredValue(int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:157:7
// JIT Entry   : Raw
void ASJIT_20afea0dd317110e89dad42ccf8623be8b515d245df9295331cd7ab124381646_1820c81e1e7d6d54a245f42028177fc83949b6a408db5b009470198af27f15b9(FScriptExecution& Execution, UObject* l_This, asDWORD p_value)
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
void ASJIT_20afea0dd317110e89dad42ccf8623be8b515d245df9295331cd7ab124381646_1820c81e1e7d6d54a245f42028177fc83949b6a408db5b009470198af27f15b9_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_20afea0dd317110e89dad42ccf8623be8b515d245df9295331cd7ab124381646_1820c81e1e7d6d54a245f42028177fc83949b6a408db5b009470198af27f15b9(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : void SetStoredValue(int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:157:7
// JIT Entry   : Parms
void ASJIT_20afea0dd317110e89dad42ccf8623be8b515d245df9295331cd7ab124381646_1820c81e1e7d6d54a245f42028177fc83949b6a408db5b009470198af27f15b9_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ASJIT_20afea0dd317110e89dad42ccf8623be8b515d245df9295331cd7ab124381646_1820c81e1e7d6d54a245f42028177fc83949b6a408db5b009470198af27f15b9(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int AddForAOT(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:159:6
 * StableFunctionKey  : 2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5
 * ExecutionHash      : 3c6f8b9f73d4d65674d613478d3c420bb2eec748c8b8f8525e55b8d971e929ea
 * DebugHash          : bfdb5823d5279e088a734f23b52b847d74544c6b0818e0598faa9daa56ea4769
 * EntryAbiHash       : 73a436254e2d895a203c53bb56b8bd06ea4085e6086c3b045fabf536dcbc0131
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_3c6f8b9f73d4d65674d613478d3c420bb2eec748c8b8f8525e55b8d971e929ea = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_3c6f8b9f73d4d65674d613478d3c420bb2eec748c8b8f8525e55b8d971e929ea
#endif

// AS Function : int AddForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:159:6
// JIT Entry   : Raw
int32 ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_3c6f8b9f73d4d65674d613478d3c420bb2eec748c8b8f8525e55b8d971e929ea(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:159:6
// JIT Entry   : VM
void ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_3c6f8b9f73d4d65674d613478d3c420bb2eec748c8b8f8525e55b8d971e929ea_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_3c6f8b9f73d4d65674d613478d3c420bb2eec748c8b8f8525e55b8d971e929ea(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : int AddForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:159:6
// JIT Entry   : Parms
void ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_3c6f8b9f73d4d65674d613478d3c420bb2eec748c8b8f8525e55b8d971e929ea_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_3c6f8b9f73d4d65674d613478d3c420bb2eec748c8b8f8525e55b8d971e929ea(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ReferenceEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:121:5
 * StableFunctionKey  : 2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9
 * ExecutionHash      : 472e6e82db2f69ccc5b381c2f4063c65252bae725436fe421e25074997656ad0
 * DebugHash          : 0f66583ef024d35748417d3585af2a1da1284b255b31ef5074c7424828f23720
 * EntryAbiHash       : eb88e947baa4860face1585fa0d6b66fc0a33ee288c7ad728b2271e86a77d1e6
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_472e6e82db2f69ccc5b381c2f4063c65252bae725436fe421e25074997656ad0 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_472e6e82db2f69ccc5b381c2f4063c65252bae725436fe421e25074997656ad0
#endif

// AS Function : int ReferenceEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:121:5
// JIT Entry   : Raw
int32 ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_472e6e82db2f69ccc5b381c2f4063c65252bae725436fe421e25074997656ad0(FScriptExecution& Execution)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:121:5
// JIT Entry   : VM
void ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_472e6e82db2f69ccc5b381c2f4063c65252bae725436fe421e25074997656ad0_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_472e6e82db2f69ccc5b381c2f4063c65252bae725436fe421e25074997656ad0(Execution);
}
// AS Function : int ReferenceEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:121:5
// JIT Entry   : Parms
void ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_472e6e82db2f69ccc5b381c2f4063c65252bae725436fe421e25074997656ad0_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_472e6e82db2f69ccc5b381c2f4063c65252bae725436fe421e25074997656ad0(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int AddTwo(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:272:6
 * StableFunctionKey  : 30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c
 * ExecutionHash      : 7e724b85f70c8b56a7c68ef82bf05652a9c15a30dee9eb3e640bde40469b5f5c
 * DebugHash          : 8405cc71faefd7eba4221f9b04066f1f384e7dbb7d8409ce9717839e15562511
 * EntryAbiHash       : c5fdf7c9de85d7a785dfc3e36e262ff979f7b922c419614f25625ceaf01dc6bd
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_7e724b85f70c8b56a7c68ef82bf05652a9c15a30dee9eb3e640bde40469b5f5c = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_7e724b85f70c8b56a7c68ef82bf05652a9c15a30dee9eb3e640bde40469b5f5c
#endif

// AS Function : int AddTwo(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:272:6
// JIT Entry   : Raw
int32 ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_7e724b85f70c8b56a7c68ef82bf05652a9c15a30dee9eb3e640bde40469b5f5c(FScriptExecution& Execution, UObject* l_This, asDWORD p_A, asDWORD p_B)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:272:6
// JIT Entry   : VM
void ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_7e724b85f70c8b56a7c68ef82bf05652a9c15a30dee9eb3e640bde40469b5f5c_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_7e724b85f70c8b56a7c68ef82bf05652a9c15a30dee9eb3e640bde40469b5f5c(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2),
		*(asDWORD*)(l_fp + 3));
}
// AS Function : int AddTwo(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:272:6
// JIT Entry   : Parms
void ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_7e724b85f70c8b56a7c68ef82bf05652a9c15a30dee9eb3e640bde40469b5f5c_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_A = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1_B = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_7e724b85f70c8b56a7c68ef82bf05652a9c15a30dee9eb3e640bde40469b5f5c(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_A),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_1_B));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void FailForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:104:6
 * StableFunctionKey  : 34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f
 * ExecutionHash      : b9a99c317c0fb7ded6fe7fd3d0efd81671c8019ef4c395c421a2d2bed3058199
 * DebugHash          : e726668854ec00f8602f4e2118bdd1a1fe2e8474dbd07e3e448e938c51bf4d2b
 * EntryAbiHash       : 5c88cfe1c954db11e6955527a1273af1e6e20436cb7527444616a92e521bcb4d
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_b9a99c317c0fb7ded6fe7fd3d0efd81671c8019ef4c395c421a2d2bed3058199 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_b9a99c317c0fb7ded6fe7fd3d0efd81671c8019ef4c395c421a2d2bed3058199
#endif

// AS Function : void FailForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:104:6
// JIT Entry   : Raw
void ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_b9a99c317c0fb7ded6fe7fd3d0efd81671c8019ef4c395c421a2d2bed3058199(FScriptExecution& Execution)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:104:6
// JIT Entry   : VM
void ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_b9a99c317c0fb7ded6fe7fd3d0efd81671c8019ef4c395c421a2d2bed3058199_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_b9a99c317c0fb7ded6fe7fd3d0efd81671c8019ef4c395c421a2d2bed3058199(Execution);
}
// AS Function : void FailForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:104:6
// JIT Entry   : Parms
void ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_b9a99c317c0fb7ded6fe7fd3d0efd81671c8019ef4c395c421a2d2bed3058199_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_b9a99c317c0fb7ded6fe7fd3d0efd81671c8019ef4c395c421a2d2bed3058199(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int RecursionEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:144:5
 * StableFunctionKey  : 3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5
 * ExecutionHash      : 04e82a31003ddad3a65866b518ba43084c909595c2f6b5170fa9bc3ef2d8f03b
 * DebugHash          : 49445cc6b01a5d50bccf515d816c2c47e6a01646cca35368d7ca641a045f4d25
 * EntryAbiHash       : 9807be95f7b037b5a524626bf011302bd0844098a4a72bbe407d8cd5e21fa5ae
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_04e82a31003ddad3a65866b518ba43084c909595c2f6b5170fa9bc3ef2d8f03b = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_04e82a31003ddad3a65866b518ba43084c909595c2f6b5170fa9bc3ef2d8f03b
#endif

// AS Function : int RecursionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:144:5
// JIT Entry   : Raw
int32 ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_04e82a31003ddad3a65866b518ba43084c909595c2f6b5170fa9bc3ef2d8f03b(FScriptExecution& Execution)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:144:5
// JIT Entry   : VM
void ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_04e82a31003ddad3a65866b518ba43084c909595c2f6b5170fa9bc3ef2d8f03b_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_04e82a31003ddad3a65866b518ba43084c909595c2f6b5170fa9bc3ef2d8f03b(Execution);
}
// AS Function : int RecursionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:144:5
// JIT Entry   : Parms
void ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_04e82a31003ddad3a65866b518ba43084c909595c2f6b5170fa9bc3ef2d8f03b_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_04e82a31003ddad3a65866b518ba43084c909595c2f6b5170fa9bc3ef2d8f03b(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ObjectLifetimeEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:211:6
 * StableFunctionKey  : 40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135
 * ExecutionHash      : 5a315c9183d04cd068d99b017378245c629a47c555ac48237f59424d16973aa7
 * DebugHash          : 735e50ece98eadfdea26980611eea7577895a563a820f089df48fc365eb06fd3
 * EntryAbiHash       : c8041da3cad4a3fdc7be0bd5795a7456826fc964a0eeef2d2d35507862c860f0
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_5a315c9183d04cd068d99b017378245c629a47c555ac48237f59424d16973aa7 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_5a315c9183d04cd068d99b017378245c629a47c555ac48237f59424d16973aa7
#endif

// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:211:6
// JIT Entry   : Raw
int32 ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_5a315c9183d04cd068d99b017378245c629a47c555ac48237f59424d16973aa7(FScriptExecution& Execution, UObject* l_This)
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
alignas(8) asBYTE MEM_v_TEMP_19[sizeof(FString)];
FString& v_TEMP_19 = (FString&)MEM_v_TEMP_19[0];
void* v_TEMP_11 = nullptr;
void* v_TEMP_13 = nullptr;
void* v_TEMP_15 = nullptr;
void* v_TEMP_21 = nullptr;
void* v_TEMP_23 = nullptr;
void* v_TEMP_25 = nullptr;
void* v_TEMP_27 = nullptr;
void* v_TEMP_29 = nullptr;
void* v_TEMP_35 = nullptr;
asDWORD v_TEMP_dword_9 = {};
asBYTE v_TEMP_byte_30 = {};
asDWORD v_TEMP_dword_31 = {};
asDWORD v_TEMP_dword_32 = {};
asDWORD v_TEMP_dword_33 = {};
asDWORD v_TEMP_dword_36 = {};
asDWORD v_TEMP_dword_37 = {};
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
// PSF v4
// PopRPtr
l_valueRegister = (asQWORD)((&v_Original));
// CpyRtoV8 v13
v_TEMP_13 = (void*)l_valueRegister;
// PshVPtr v11
// PshVPtr v13
// CALLSYS *
// FString& FString::opAssign(const FString&inout Other)
{
  FString* Object = (FString*)(v_TEMP_13);
SCRIPT_DEBUG_CALLSTACK_POSITION(213, 11);
  if(Object != nullptr)
  {
const FString& FunctionReturnValue = Object->operator=((*(FString*)(v_TEMP_11)));
l_valueRegister = (asQWORD)&FunctionReturnValue;
} else [[unlikely]] {
SCRIPT_NULL_POINTER_EXCEPTION();
return {};
}
}
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PopRPtr
l_valueRegister = (asQWORD)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 8u));
// CpyRtoV8 v15
v_TEMP_15 = (void*)l_valueRegister;
// PSF v4
// PopRPtr
l_valueRegister = (asQWORD)((&v_Original));
// CpyRtoV8 v21
v_TEMP_21 = (void*)l_valueRegister;
// PshVPtr v15
// PSF v19
// PshVPtr v21
// CALLSYS *
// FString FString::opAdd(const FString&inout Other) const
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 3u).GetFunction();
  auto CastedFuncPtr = (FString(*)(void*,void*))RawFuncPtr;
  void* Object = (void*)(v_TEMP_21);
SCRIPT_DEBUG_CALLSTACK_POSITION(214, 11);
  if(Object != nullptr)
  {
new((void*)((&v_TEMP_19))) FString(CastedFuncPtr(Object,(void**)(v_TEMP_15)));
} else [[unlikely]] {
SCRIPT_NULL_POINTER_EXCEPTION();
return {};
}
if (Execution.bExceptionThrown) [[unlikely]]
{
return {};
}
}
// JitEntry *
// PSF v19
// PopRPtr
l_valueRegister = (asQWORD)((&v_TEMP_19));
// CpyRtoV8 v23
v_TEMP_23 = (void*)l_valueRegister;
// PSF v8
// PopRPtr
l_valueRegister = (asQWORD)((&v_Decorated));
// CpyRtoV8 v25
v_TEMP_25 = (void*)l_valueRegister;
// PshVPtr v23
// PshVPtr v25
// CALLSYS *
// FString& FString::opAssign(const FString&inout Other)
{
  FString* Object = (FString*)(v_TEMP_25);
SCRIPT_DEBUG_CALLSTACK_POSITION(214, 11);
  if(Object != nullptr)
  {
const FString& FunctionReturnValue = Object->operator=((*(FString*)(v_TEMP_23)));
l_valueRegister = (asQWORD)&FunctionReturnValue;
} else [[unlikely]] {
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_EXCEPTION_0;
}
}
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PopRPtr
l_valueRegister = (asQWORD)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 7u));
// CpyRtoV8 v27
v_TEMP_27 = (void*)l_valueRegister;
// PSF v4
// PopRPtr
l_valueRegister = (asQWORD)((&v_Original));
// CpyRtoV8 v29
v_TEMP_29 = (void*)l_valueRegister;
// PshVPtr v27
// PshVPtr v29
// CALLSYS *
// bool FString::opEquals(const FString&inout Other) const
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 1u).GetFunction();
  auto CastedFuncPtr = (bool(*)(void*,void*))RawFuncPtr;
  void* Object = (void*)(v_TEMP_29);
SCRIPT_DEBUG_CALLSTACK_POSITION(215, 3);
  if(Object != nullptr)
  {
bool FunctionReturnValue = CastedFuncPtr(Object,(void**)(v_TEMP_27));
l_byteRegister = (asBYTE&)FunctionReturnValue;
} else [[unlikely]] {
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_EXCEPTION_0;
}
if (Execution.bExceptionThrown) [[unlikely]]
{
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_EXCEPTION_0;
}
}
// JitEntry *
// JLowZ 4
if(l_byteRegister == 0) {
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_88;
}
// SetV4 v31, 100
v_TEMP_dword_31 = 0x64u;
// JMP 2
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_90;
LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_88:
// SetV4 v31, 0
v_TEMP_dword_31 = 0x0u;
LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_90:
// PSF v8
// PopRPtr
l_valueRegister = (asQWORD)((&v_Decorated));
// CpyRtoV8 v35
v_TEMP_35 = (void*)l_valueRegister;
// PshVPtr v35
// CALLSYS *
// int FString::Len() const
{
  FString* Object = (FString*)(v_TEMP_35);
SCRIPT_DEBUG_CALLSTACK_POSITION(215, 3);
  if(Object != nullptr)
  {
int32 FunctionReturnValue = Object->Len();
l_dwordRegister = (asDWORD&)FunctionReturnValue;
} else [[unlikely]] {
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_EXCEPTION_0;
}
}
// JitEntry *
// CpyRtoV4 v36
v_TEMP_dword_36 = l_dwordRegister;
// ADDi v9, v31, v36
((int32&)v_TEMP_dword_9) = ((int32&)v_TEMP_dword_31) + ((int32&)v_TEMP_dword_36);
// JMP 0
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_105;
LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_105:
// PSF v19
// CALLSYS *
// FString::~FString()
{
  FString* Object = (FString*)((&v_TEMP_19));
((FString*)Object)->~FString();
}
// JitEntry *
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
// CpyVtoR4 v9
l_dwordRegister = v_TEMP_dword_9;
// RET 2
  return (int32)l_dwordRegister;
SCRIPT_ASSUME(false);

LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_EXCEPTION_0:
// Exception cleanup for variables 19
if (!Execution.bExceptionThrown) { SCRIPT_NULL_POINTER_EXCEPTION(); }
((FString*)((FString*)(&v_TEMP_19)))->~FString();
return {};
}
// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:211:6
// JIT Entry   : VM
void ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_5a315c9183d04cd068d99b017378245c629a47c555ac48237f59424d16973aa7_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_5a315c9183d04cd068d99b017378245c629a47c555ac48237f59424d16973aa7(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:211:6
// JIT Entry   : Parms
void ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_5a315c9183d04cd068d99b017378245c629a47c555ac48237f59424d16973aa7_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_5a315c9183d04cd068d99b017378245c629a47c555ac48237f59424d16973aa7(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int64 DoubleToInt64ForAOT(const float)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:170:8
 * StableFunctionKey  : 486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52
 * ExecutionHash      : 18e0fba28b0f3efc2baac2c10ce5aad3f5e5994d4753f5c94ce383fe25d7d0ad
 * DebugHash          : ba02272742713326760d7c3c22895d75bb8424ac0d55131c693c7ead6d3e4fe7
 * EntryAbiHash       : a1d1604b170c8afa6d8aebaea217a68d24d4f67ff1cce99952cd0a22877c25f4
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_18e0fba28b0f3efc2baac2c10ce5aad3f5e5994d4753f5c94ce383fe25d7d0ad = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_18e0fba28b0f3efc2baac2c10ce5aad3f5e5994d4753f5c94ce383fe25d7d0ad
#endif

// AS Function : int64 DoubleToInt64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:170:8
// JIT Entry   : Raw
int64 ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_18e0fba28b0f3efc2baac2c10ce5aad3f5e5994d4753f5c94ce383fe25d7d0ad(FScriptExecution& Execution, UObject* l_This, double p_Value)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:170:8
// JIT Entry   : VM
void ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_18e0fba28b0f3efc2baac2c10ce5aad3f5e5994d4753f5c94ce383fe25d7d0ad_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int64*)l_outValue = ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_18e0fba28b0f3efc2baac2c10ce5aad3f5e5994d4753f5c94ce383fe25d7d0ad(Execution,
		*(UObject**)l_fp,
		*(double*)(l_fp + 2));
}
// AS Function : int64 DoubleToInt64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:170:8
// JIT Entry   : Parms
void ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_18e0fba28b0f3efc2baac2c10ce5aad3f5e5994d4753f5c94ce383fe25d7d0ad_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(int64));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int64*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_18e0fba28b0f3efc2baac2c10ce5aad3f5e5994d4753f5c94ce383fe25d7d0ad(Execution,
		(UObject*)Object,
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int AddForAOT(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:3:5
 * StableFunctionKey  : 4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30
 * ExecutionHash      : c6e766be4e8d00717c189436dc39fd08f583e6e12db827e91d27402c69c6372c
 * DebugHash          : 75b7ef10cc591b05a48d2f3d2322f9da176d16ec13a358c2857e6220e2d71428
 * EntryAbiHash       : 73a436254e2d895a203c53bb56b8bd06ea4085e6086c3b045fabf536dcbc0131
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30_c6e766be4e8d00717c189436dc39fd08f583e6e12db827e91d27402c69c6372c = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30_c6e766be4e8d00717c189436dc39fd08f583e6e12db827e91d27402c69c6372c
#endif

// AS Function : int AddForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:3:5
// JIT Entry   : Raw
int32 ASJIT_4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30_c6e766be4e8d00717c189436dc39fd08f583e6e12db827e91d27402c69c6372c(FScriptExecution& Execution, asDWORD p_Value)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:3:5
// JIT Entry   : VM
void ASJIT_4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30_c6e766be4e8d00717c189436dc39fd08f583e6e12db827e91d27402c69c6372c_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30_c6e766be4e8d00717c189436dc39fd08f583e6e12db827e91d27402c69c6372c(Execution,
		*(asDWORD*)(l_fp + 0));
}
// AS Function : int AddForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:3:5
// JIT Entry   : Parms
void ASJIT_4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30_c6e766be4e8d00717c189436dc39fd08f583e6e12db827e91d27402c69c6372c_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_4b125aabee97028e61ca8ddce0932f818e07f9454a679f4de650a7adbdf53f30_c6e766be4e8d00717c189436dc39fd08f583e6e12db827e91d27402c69c6372c(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticMutationParity(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:18:5
 * StableFunctionKey  : 4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4
 * ExecutionHash      : 3c5c60f346d8a16d53c0c745c9062f09293b42a17f4bd55930dad17290e44949
 * DebugHash          : c6010a1ea87a8ab05e22dbb77f48a05e41eca5ef1d2800a7c81bf1b97d35ee21
 * EntryAbiHash       : 45c016e37ac90d9198599858f3806eb4651e70a0969f759160a4683df7eee13c
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4_3c5c60f346d8a16d53c0c745c9062f09293b42a17f4bd55930dad17290e44949 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4_3c5c60f346d8a16d53c0c745c9062f09293b42a17f4bd55930dad17290e44949
#endif

// AS Function : int SemanticMutationParity(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:18:5
// JIT Entry   : Raw
int32 ASJIT_4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4_3c5c60f346d8a16d53c0c745c9062f09293b42a17f4bd55930dad17290e44949(FScriptExecution& Execution, asDWORD p_Input, asDWORD p_Source)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:18:5
// JIT Entry   : VM
void ASJIT_4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4_3c5c60f346d8a16d53c0c745c9062f09293b42a17f4bd55930dad17290e44949_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4_3c5c60f346d8a16d53c0c745c9062f09293b42a17f4bd55930dad17290e44949(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}
// AS Function : int SemanticMutationParity(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:18:5
// JIT Entry   : Parms
void ASJIT_4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4_3c5c60f346d8a16d53c0c745c9062f09293b42a17f4bd55930dad17290e44949_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Input = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1_Source = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_4fa1dcdc68283850ec78bf0d4c0772b0dc4319d91d9d7fd793b339ef8afb21d4_3c5c60f346d8a16d53c0c745c9062f09293b42a17f4bd55930dad17290e44949(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Input),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_1_Source));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int BumpReferenceForAOT(int&inout)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:197:6
 * StableFunctionKey  : 5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65
 * ExecutionHash      : fb5c08f920779980ff0ba4e8f623a58a421fdde091a1bfa1a64e2c6cf55e41c0
 * DebugHash          : 2d5cd38e3b6002f367df9fd4a814e51aa038d019d8d764ad9ed5d3f0a9b07d10
 * EntryAbiHash       : b80efaaa778ead65430c5702610f23a7b6dccd414f3c044078aa45e30f15b055
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_fb5c08f920779980ff0ba4e8f623a58a421fdde091a1bfa1a64e2c6cf55e41c0 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_fb5c08f920779980ff0ba4e8f623a58a421fdde091a1bfa1a64e2c6cf55e41c0
#endif

// AS Function : int BumpReferenceForAOT(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:197:6
// JIT Entry   : Raw
int32 ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_fb5c08f920779980ff0ba4e8f623a58a421fdde091a1bfa1a64e2c6cf55e41c0(FScriptExecution& Execution, UObject* l_This, int32* p_Value)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:197:6
// JIT Entry   : VM
void ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_fb5c08f920779980ff0ba4e8f623a58a421fdde091a1bfa1a64e2c6cf55e41c0_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_fb5c08f920779980ff0ba4e8f623a58a421fdde091a1bfa1a64e2c6cf55e41c0(Execution,
		*(UObject**)l_fp,
		*(int32**)(l_fp + 2));
}
// AS Function : int BumpReferenceForAOT(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:197:6
// JIT Entry   : Parms
void ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_fb5c08f920779980ff0ba4e8f623a58a421fdde091a1bfa1a64e2c6cf55e41c0_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_fb5c08f920779980ff0ba4e8f623a58a421fdde091a1bfa1a64e2c6cf55e41c0(Execution,
		(UObject*)Object,
		(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UStaticJITAotVirtualChild()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:307:7
 * StableFunctionKey  : 521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7
 * ExecutionHash      : 5f9658feb421b0ec597412d53bcc0c9f132cfdc73e0d0e10449cb8fd92fd91d8
 * DebugHash          : c82aa60ae252a364aa725893b2e58a817d9fa934ce295521e1f0b9ba9192efe7
 * EntryAbiHash       : 847e0fd88661ad9588f2e5b3a2e560f536b2c027896b3664ab7a3f99a32fee92
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_5f9658feb421b0ec597412d53bcc0c9f132cfdc73e0d0e10449cb8fd92fd91d8 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_5f9658feb421b0ec597412d53bcc0c9f132cfdc73e0d0e10449cb8fd92fd91d8
#endif

// AS Function : UStaticJITAotVirtualChild()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:307:7
// JIT Entry   : Raw
void ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_5f9658feb421b0ec597412d53bcc0c9f132cfdc73e0d0e10449cb8fd92fd91d8(FScriptExecution& Execution, UObject* l_This)
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
// RET 2
  return;
}
// AS Function : UStaticJITAotVirtualChild()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:307:7
// JIT Entry   : VM
void ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_5f9658feb421b0ec597412d53bcc0c9f132cfdc73e0d0e10449cb8fd92fd91d8_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_5f9658feb421b0ec597412d53bcc0c9f132cfdc73e0d0e10449cb8fd92fd91d8(Execution,
		*(UObject**)l_fp);
}
// AS Function : UStaticJITAotVirtualChild()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:307:7
// JIT Entry   : Parms
void ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_5f9658feb421b0ec597412d53bcc0c9f132cfdc73e0d0e10449cb8fd92fd91d8_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_5f9658feb421b0ec597412d53bcc0c9f132cfdc73e0d0e10449cb8fd92fd91d8(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int Entry()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:83:5
 * StableFunctionKey  : 5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa
 * ExecutionHash      : bc214630e47c72abf82e473a1de0176231e6e808c766af32d5c4a49bb938d2fd
 * DebugHash          : 1d728c3ec8402fd418096e470802435a5824327c1e5968bb8d483ed6771f72c4
 * EntryAbiHash       : db0c8f3058412e5bdc683b188ff132b140d00caf79e3e0658bda0254082fdee0
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_bc214630e47c72abf82e473a1de0176231e6e808c766af32d5c4a49bb938d2fd = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_bc214630e47c72abf82e473a1de0176231e6e808c766af32d5c4a49bb938d2fd
#endif

// AS Function : int Entry()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:83:5
// JIT Entry   : Raw
int32 ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_bc214630e47c72abf82e473a1de0176231e6e808c766af32d5c4a49bb938d2fd(FScriptExecution& Execution)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:83:5
// JIT Entry   : VM
void ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_bc214630e47c72abf82e473a1de0176231e6e808c766af32d5c4a49bb938d2fd_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_bc214630e47c72abf82e473a1de0176231e6e808c766af32d5c4a49bb938d2fd(Execution);
}
// AS Function : int Entry()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:83:5
// JIT Entry   : Parms
void ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_bc214630e47c72abf82e473a1de0176231e6e808c766af32d5c4a49bb938d2fd_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_bc214630e47c72abf82e473a1de0176231e6e808c766af32d5c4a49bb938d2fd(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void FailForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:186:7
 * StableFunctionKey  : 58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9
 * ExecutionHash      : e35cae8ee1bf87f89a05a71a8a9bb7e268c4d30a6c6c1f53634000b6de85a3aa
 * DebugHash          : 02f0011ef64c64bd5713e456f97b1876768b935e56593f35b81ae8d831f89a42
 * EntryAbiHash       : 5c88cfe1c954db11e6955527a1273af1e6e20436cb7527444616a92e521bcb4d
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_e35cae8ee1bf87f89a05a71a8a9bb7e268c4d30a6c6c1f53634000b6de85a3aa = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_e35cae8ee1bf87f89a05a71a8a9bb7e268c4d30a6c6c1f53634000b6de85a3aa
#endif

// AS Function : void FailForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:186:7
// JIT Entry   : Raw
void ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_e35cae8ee1bf87f89a05a71a8a9bb7e268c4d30a6c6c1f53634000b6de85a3aa(FScriptExecution& Execution, UObject* l_This)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:186:7
// JIT Entry   : VM
void ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_e35cae8ee1bf87f89a05a71a8a9bb7e268c4d30a6c6c1f53634000b6de85a3aa_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_e35cae8ee1bf87f89a05a71a8a9bb7e268c4d30a6c6c1f53634000b6de85a3aa(Execution,
		*(UObject**)l_fp);
}
// AS Function : void FailForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:186:7
// JIT Entry   : Parms
void ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_e35cae8ee1bf87f89a05a71a8a9bb7e268c4d30a6c6c1f53634000b6de85a3aa_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_e35cae8ee1bf87f89a05a71a8a9bb7e268c4d30a6c6c1f53634000b6de85a3aa(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int GetVirtualValue()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:293:6
 * StableFunctionKey  : 598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c
 * ExecutionHash      : b85558bf00746c0f4134070208f2daf8f2899db3b4640a21ca606b7533eec5a2
 * DebugHash          : 1fa85316db014ca789ec0e2b1f09a17f1a7373a137b154119478146715260ed9
 * EntryAbiHash       : 3c7da2556903a2aede2bf7c4a70e333223d38b106a790953f977db702ff61900
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_b85558bf00746c0f4134070208f2daf8f2899db3b4640a21ca606b7533eec5a2 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_b85558bf00746c0f4134070208f2daf8f2899db3b4640a21ca606b7533eec5a2
#endif

// AS Function : int GetVirtualValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:293:6
// JIT Entry   : Raw
int32 ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_b85558bf00746c0f4134070208f2daf8f2899db3b4640a21ca606b7533eec5a2(FScriptExecution& Execution, UObject* l_This)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:293:6
// JIT Entry   : VM
void ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_b85558bf00746c0f4134070208f2daf8f2899db3b4640a21ca606b7533eec5a2_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_b85558bf00746c0f4134070208f2daf8f2899db3b4640a21ca606b7533eec5a2(Execution,
		*(UObject**)l_fp);
}
// AS Function : int GetVirtualValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:293:6
// JIT Entry   : Parms
void ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_b85558bf00746c0f4134070208f2daf8f2899db3b4640a21ca606b7533eec5a2_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_b85558bf00746c0f4134070208f2daf8f2899db3b4640a21ca606b7533eec5a2(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UStaticJITAotFunctionCarrier ReturnSelfObject()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:266:31
 * StableFunctionKey  : 5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f
 * ExecutionHash      : 228d5ce597362c2e5fde455e5b3b94a93392086cf2a10d3429d27202c2f00cd4
 * DebugHash          : 65dfc6b79b25f4e8d78da09d19f61f06000cf403eb84536e6aa8f19dc4abd576
 * EntryAbiHash       : 30ea0052fcb05cde2ff76d0f9f168b4c15908ca08694ae1622949ba51c05132e
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_228d5ce597362c2e5fde455e5b3b94a93392086cf2a10d3429d27202c2f00cd4 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_228d5ce597362c2e5fde455e5b3b94a93392086cf2a10d3429d27202c2f00cd4
#endif

// AS Function : UStaticJITAotFunctionCarrier ReturnSelfObject()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:266:31
// JIT Entry   : Raw
UObject* ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_228d5ce597362c2e5fde455e5b3b94a93392086cf2a10d3429d27202c2f00cd4(FScriptExecution& Execution, UObject* l_This)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:266:31
// JIT Entry   : VM
void ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_228d5ce597362c2e5fde455e5b3b94a93392086cf2a10d3429d27202c2f00cd4_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(UObject**)l_outValue = ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_228d5ce597362c2e5fde455e5b3b94a93392086cf2a10d3429d27202c2f00cd4(Execution,
		*(UObject**)l_fp);
}
// AS Function : UStaticJITAotFunctionCarrier ReturnSelfObject()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:266:31
// JIT Entry   : Parms
void ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_228d5ce597362c2e5fde455e5b3b94a93392086cf2a10d3429d27202c2f00cd4_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(UObject**)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_228d5ce597362c2e5fde455e5b3b94a93392086cf2a10d3429d27202c2f00cd4(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ObjectLifetimeEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:128:5
 * StableFunctionKey  : 5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d
 * ExecutionHash      : 88e9d4622ac72222d77542a4f470d0ca3ef87cceeef61aff6dd6f4ed992ca349
 * DebugHash          : bc5a9f8c679598b4b03a0d51a9dee288a6c873c1cc9e6522375b86b0a54ee232
 * EntryAbiHash       : c8041da3cad4a3fdc7be0bd5795a7456826fc964a0eeef2d2d35507862c860f0
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_88e9d4622ac72222d77542a4f470d0ca3ef87cceeef61aff6dd6f4ed992ca349 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_88e9d4622ac72222d77542a4f470d0ca3ef87cceeef61aff6dd6f4ed992ca349
#endif

// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:128:5
// JIT Entry   : Raw
int32 ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_88e9d4622ac72222d77542a4f470d0ca3ef87cceeef61aff6dd6f4ed992ca349(FScriptExecution& Execution)
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
alignas(8) asBYTE MEM_v_TEMP_19[sizeof(FString)];
FString& v_TEMP_19 = (FString&)MEM_v_TEMP_19[0];
void* v_TEMP_11 = nullptr;
void* v_TEMP_13 = nullptr;
void* v_TEMP_15 = nullptr;
void* v_TEMP_21 = nullptr;
void* v_TEMP_23 = nullptr;
void* v_TEMP_25 = nullptr;
void* v_TEMP_27 = nullptr;
void* v_TEMP_29 = nullptr;
void* v_TEMP_35 = nullptr;
asDWORD v_TEMP_dword_9 = {};
asBYTE v_TEMP_byte_30 = {};
asDWORD v_TEMP_dword_31 = {};
asDWORD v_TEMP_dword_32 = {};
asDWORD v_TEMP_dword_33 = {};
asDWORD v_TEMP_dword_36 = {};
asDWORD v_TEMP_dword_37 = {};
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
// PSF v4
// PopRPtr
l_valueRegister = (asQWORD)((&v_Original));
// CpyRtoV8 v13
v_TEMP_13 = (void*)l_valueRegister;
// PshVPtr v11
// PshVPtr v13
// CALLSYS *
// FString& FString::opAssign(const FString&inout Other)
{
  FString* Object = (FString*)(v_TEMP_13);
SCRIPT_DEBUG_CALLSTACK_POSITION(130, 10);
  if(Object != nullptr)
  {
const FString& FunctionReturnValue = Object->operator=((*(FString*)(v_TEMP_11)));
l_valueRegister = (asQWORD)&FunctionReturnValue;
} else [[unlikely]] {
SCRIPT_NULL_POINTER_EXCEPTION();
return {};
}
}
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PopRPtr
l_valueRegister = (asQWORD)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 7u));
// CpyRtoV8 v15
v_TEMP_15 = (void*)l_valueRegister;
// PSF v4
// PopRPtr
l_valueRegister = (asQWORD)((&v_Original));
// CpyRtoV8 v21
v_TEMP_21 = (void*)l_valueRegister;
// PshVPtr v15
// PSF v19
// PshVPtr v21
// CALLSYS *
// FString FString::opAdd(const FString&inout Other) const
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 2u).GetFunction();
  auto CastedFuncPtr = (FString(*)(void*,void*))RawFuncPtr;
  void* Object = (void*)(v_TEMP_21);
SCRIPT_DEBUG_CALLSTACK_POSITION(131, 10);
  if(Object != nullptr)
  {
new((void*)((&v_TEMP_19))) FString(CastedFuncPtr(Object,(void**)(v_TEMP_15)));
} else [[unlikely]] {
SCRIPT_NULL_POINTER_EXCEPTION();
return {};
}
if (Execution.bExceptionThrown) [[unlikely]]
{
return {};
}
}
// JitEntry *
// PSF v19
// PopRPtr
l_valueRegister = (asQWORD)((&v_TEMP_19));
// CpyRtoV8 v23
v_TEMP_23 = (void*)l_valueRegister;
// PSF v8
// PopRPtr
l_valueRegister = (asQWORD)((&v_Decorated));
// CpyRtoV8 v25
v_TEMP_25 = (void*)l_valueRegister;
// PshVPtr v23
// PshVPtr v25
// CALLSYS *
// FString& FString::opAssign(const FString&inout Other)
{
  FString* Object = (FString*)(v_TEMP_25);
SCRIPT_DEBUG_CALLSTACK_POSITION(131, 10);
  if(Object != nullptr)
  {
const FString& FunctionReturnValue = Object->operator=((*(FString*)(v_TEMP_23)));
l_valueRegister = (asQWORD)&FunctionReturnValue;
} else [[unlikely]] {
goto LABEL_ObjectLifetimeEntryForAOT_EXCEPTION_0;
}
}
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PopRPtr
l_valueRegister = (asQWORD)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 6u));
// CpyRtoV8 v27
v_TEMP_27 = (void*)l_valueRegister;
// PSF v4
// PopRPtr
l_valueRegister = (asQWORD)((&v_Original));
// CpyRtoV8 v29
v_TEMP_29 = (void*)l_valueRegister;
// PshVPtr v27
// PshVPtr v29
// CALLSYS *
// bool FString::opEquals(const FString&inout Other) const
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 0u).GetFunction();
  auto CastedFuncPtr = (bool(*)(void*,void*))RawFuncPtr;
  void* Object = (void*)(v_TEMP_29);
SCRIPT_DEBUG_CALLSTACK_POSITION(132, 2);
  if(Object != nullptr)
  {
bool FunctionReturnValue = CastedFuncPtr(Object,(void**)(v_TEMP_27));
l_byteRegister = (asBYTE&)FunctionReturnValue;
} else [[unlikely]] {
goto LABEL_ObjectLifetimeEntryForAOT_EXCEPTION_0;
}
if (Execution.bExceptionThrown) [[unlikely]]
{
goto LABEL_ObjectLifetimeEntryForAOT_EXCEPTION_0;
}
}
// JitEntry *
// JLowZ 4
if(l_byteRegister == 0) {
goto LABEL_ObjectLifetimeEntryForAOT_88;
}
// SetV4 v31, 100
v_TEMP_dword_31 = 0x64u;
// JMP 2
goto LABEL_ObjectLifetimeEntryForAOT_90;
LABEL_ObjectLifetimeEntryForAOT_88:
// SetV4 v31, 0
v_TEMP_dword_31 = 0x0u;
LABEL_ObjectLifetimeEntryForAOT_90:
// PSF v8
// PopRPtr
l_valueRegister = (asQWORD)((&v_Decorated));
// CpyRtoV8 v35
v_TEMP_35 = (void*)l_valueRegister;
// PshVPtr v35
// CALLSYS *
// int FString::Len() const
{
  FString* Object = (FString*)(v_TEMP_35);
SCRIPT_DEBUG_CALLSTACK_POSITION(132, 2);
  if(Object != nullptr)
  {
int32 FunctionReturnValue = Object->Len();
l_dwordRegister = (asDWORD&)FunctionReturnValue;
} else [[unlikely]] {
goto LABEL_ObjectLifetimeEntryForAOT_EXCEPTION_0;
}
}
// JitEntry *
// CpyRtoV4 v36
v_TEMP_dword_36 = l_dwordRegister;
// ADDi v9, v31, v36
((int32&)v_TEMP_dword_9) = ((int32&)v_TEMP_dword_31) + ((int32&)v_TEMP_dword_36);
// JMP 0
goto LABEL_ObjectLifetimeEntryForAOT_105;
LABEL_ObjectLifetimeEntryForAOT_105:
// PSF v19
// CALLSYS *
// FString::~FString()
{
  FString* Object = (FString*)((&v_TEMP_19));
((FString*)Object)->~FString();
}
// JitEntry *
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
// CpyVtoR4 v9
l_dwordRegister = v_TEMP_dword_9;
// RET 0
  return (int32)l_dwordRegister;
SCRIPT_ASSUME(false);

LABEL_ObjectLifetimeEntryForAOT_EXCEPTION_0:
// Exception cleanup for variables 19
if (!Execution.bExceptionThrown) { SCRIPT_NULL_POINTER_EXCEPTION(); }
((FString*)((FString*)(&v_TEMP_19)))->~FString();
return {};
}
// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:128:5
// JIT Entry   : VM
void ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_88e9d4622ac72222d77542a4f470d0ca3ef87cceeef61aff6dd6f4ed992ca349_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_88e9d4622ac72222d77542a4f470d0ca3ef87cceeef61aff6dd6f4ed992ca349(Execution);
}
// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:128:5
// JIT Entry   : Parms
void ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_88e9d4622ac72222d77542a4f470d0ca3ef87cceeef61aff6dd6f4ed992ca349_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_88e9d4622ac72222d77542a4f470d0ca3ef87cceeef61aff6dd6f4ed992ca349(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int BumpReference(int&inout)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:258:6
 * StableFunctionKey  : 635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1
 * ExecutionHash      : f1136fb729b8cd02b91756ed1dc6c265d4265483810c9a55a79ad5e85bea1916
 * DebugHash          : 32b59e98daeca4768bc0935335a25b3ad9928ef1a365d101685744f6955e7fd0
 * EntryAbiHash       : 59029a9011b50bdad9e86b6ca1d8d31868558cdf84799ffea7ef7660667c250a
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_f1136fb729b8cd02b91756ed1dc6c265d4265483810c9a55a79ad5e85bea1916 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_f1136fb729b8cd02b91756ed1dc6c265d4265483810c9a55a79ad5e85bea1916
#endif

// AS Function : int BumpReference(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:258:6
// JIT Entry   : Raw
int32 ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_f1136fb729b8cd02b91756ed1dc6c265d4265483810c9a55a79ad5e85bea1916(FScriptExecution& Execution, UObject* l_This, int32* p_Value)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:258:6
// JIT Entry   : VM
void ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_f1136fb729b8cd02b91756ed1dc6c265d4265483810c9a55a79ad5e85bea1916_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_f1136fb729b8cd02b91756ed1dc6c265d4265483810c9a55a79ad5e85bea1916(Execution,
		*(UObject**)l_fp,
		*(int32**)(l_fp + 2));
}
// AS Function : int BumpReference(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:258:6
// JIT Entry   : Parms
void ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_f1136fb729b8cd02b91756ed1dc6c265d4265483810c9a55a79ad5e85bea1916_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_f1136fb729b8cd02b91756ed1dc6c265d4265483810c9a55a79ad5e85bea1916(Execution,
		(UObject*)Object,
		(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UStaticJITAotFunctionCarrier UStaticJITAotFunctionCarrier()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:155:7
 * StableFunctionKey  : 6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060
 * ExecutionHash      : fb05ff1cd40b005becc20bdca090866a9a681873a5ef622458c77a1146814bec
 * DebugHash          : e8390d9a8f8c28907e0cef3600eb43fd0ef32f35b43ae47a96a5f5ca83b1693e
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:155:7
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:155:7
// JIT Entry   : VM
void ASJIT_6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060_fb05ff1cd40b005becc20bdca090866a9a681873a5ef622458c77a1146814bec_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(UObject**)l_outValue = ASJIT_6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060_fb05ff1cd40b005becc20bdca090866a9a681873a5ef622458c77a1146814bec(Execution);
}
// AS Function : UStaticJITAotFunctionCarrier UStaticJITAotFunctionCarrier()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:155:7
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:44:5
 * StableFunctionKey  : 73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795
 * ExecutionHash      : a5a542511d65c37ee325b33f9671cda0457377d80380cac0f23a80439b1f71e2
 * DebugHash          : 98a0a1dee3bb349b4509f41f5434e0115e3b6b1e1025497750161b1beed805e7
 * EntryAbiHash       : 9309f3438eca43da589c8d5ac23b06a58e143747a47d2f54ff811daf7b2e9706
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_a5a542511d65c37ee325b33f9671cda0457377d80380cac0f23a80439b1f71e2_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d29,
	const int32 as_ast_d30);

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
static int32 ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_a5a542511d65c37ee325b33f9671cda0457377d80380cac0f23a80439b1f71e2_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d29,
	const int32 as_ast_d30)
{
	{
		TypedFrame.SetSourcePosition(46, 2);
		return ([&]() -> int32
	{
		const int32 as_ast_e56_arg1 = as_ast_d30;
		const int32 as_ast_e56_arg0 = as_ast_d29;
		const int32 as_ast_e56_result = AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32, int32>(Execution, ASJIT_Call_DaysInMonth_a23d24f5_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795, as_ast_e56_arg0, as_ast_e56_arg1);
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return as_ast_e56_result;
	}());
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int TypedASTExportedSymbolShowcase(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:44:5
// JIT Entry   : Raw
int32 ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_a5a542511d65c37ee325b33f9671cda0457377d80380cac0f23a80439b1f71e2_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1)
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
	return ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_a5a542511d65c37ee325b33f9671cda0457377d80380cac0f23a80439b1f71e2_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1));
}

// AS Function : int TypedASTExportedSymbolShowcase(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:44:5
// JIT Entry   : VM
void ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_a5a542511d65c37ee325b33f9671cda0457377d80380cac0f23a80439b1f71e2_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_a5a542511d65c37ee325b33f9671cda0457377d80380cac0f23a80439b1f71e2_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}

// AS Function : int TypedASTExportedSymbolShowcase(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:44:5
// JIT Entry   : Parms
void ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_a5a542511d65c37ee325b33f9671cda0457377d80380cac0f23a80439b1f71e2_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_73513764b7734eab1039f73f42a0624f5c32ba4af3544db4466a3263c2d1e795_a5a542511d65c37ee325b33f9671cda0457377d80380cac0f23a80439b1f71e2_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : float SemanticPowerDoubleInt(const float, const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:38:9
 * StableFunctionKey  : 75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f
 * ExecutionHash      : 35c70d94eb6eea22aeeced7de32ae1d709c016794ad3ef87fc5b20054161e12f
 * DebugHash          : 1efe62957608a5c1a209744e5a857e3f3a50ba55b5503ff29eb0d460fba07de9
 * EntryAbiHash       : db0465d360843dd633375f902a889661f9e22535246942bafbda919d3c3cbcf1
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f_35c70d94eb6eea22aeeced7de32ae1d709c016794ad3ef87fc5b20054161e12f = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f_35c70d94eb6eea22aeeced7de32ae1d709c016794ad3ef87fc5b20054161e12f
#endif

// AS Function : float SemanticPowerDoubleInt(const float, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:38:9
// JIT Entry   : Raw
double ASJIT_75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f_35c70d94eb6eea22aeeced7de32ae1d709c016794ad3ef87fc5b20054161e12f(FScriptExecution& Execution, double p_Base, asDWORD p_Exponent)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:38:9
// JIT Entry   : VM
void ASJIT_75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f_35c70d94eb6eea22aeeced7de32ae1d709c016794ad3ef87fc5b20054161e12f_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(double*)l_outValue = ASJIT_75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f_35c70d94eb6eea22aeeced7de32ae1d709c016794ad3ef87fc5b20054161e12f(Execution,
		*(double*)(l_fp + 0),
		*(asDWORD*)(l_fp + 2));
}
// AS Function : float SemanticPowerDoubleInt(const float, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:38:9
// JIT Entry   : Parms
void ASJIT_75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f_35c70d94eb6eea22aeeced7de32ae1d709c016794ad3ef87fc5b20054161e12f_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Base = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1_Exponent = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(double));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(double*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_75fed72a536fa8de4e4c4209cf0461899c8cf1afd105e24d240e22e1133b6d5f_35c70d94eb6eea22aeeced7de32ae1d709c016794ad3ef87fc5b20054161e12f(Execution,
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_0_Base),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_1_Exponent));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : float SemanticPowerDouble(const float, const float)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:33:9
 * StableFunctionKey  : 794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43
 * ExecutionHash      : d5c8fa0ea0e7a139b4e53610692d4bb444e80d0c8a4b007796df0b18e50015c2
 * DebugHash          : 56444a7c9d40277a2575b2ce28445020e7508a5fccdee4c827c7b16538b7529a
 * EntryAbiHash       : 802462d137df2242b59cb2b901557e6d9f177fed1caae7e133b299f6f631b506
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43_d5c8fa0ea0e7a139b4e53610692d4bb444e80d0c8a4b007796df0b18e50015c2 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43_d5c8fa0ea0e7a139b4e53610692d4bb444e80d0c8a4b007796df0b18e50015c2
#endif

// AS Function : float SemanticPowerDouble(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:33:9
// JIT Entry   : Raw
double ASJIT_794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43_d5c8fa0ea0e7a139b4e53610692d4bb444e80d0c8a4b007796df0b18e50015c2(FScriptExecution& Execution, double p_Base, double p_Exponent)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:33:9
// JIT Entry   : VM
void ASJIT_794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43_d5c8fa0ea0e7a139b4e53610692d4bb444e80d0c8a4b007796df0b18e50015c2_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(double*)l_outValue = ASJIT_794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43_d5c8fa0ea0e7a139b4e53610692d4bb444e80d0c8a4b007796df0b18e50015c2(Execution,
		*(double*)(l_fp + 0),
		*(double*)(l_fp + 2));
}
// AS Function : float SemanticPowerDouble(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:33:9
// JIT Entry   : Parms
void ASJIT_794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43_d5c8fa0ea0e7a139b4e53610692d4bb444e80d0c8a4b007796df0b18e50015c2_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Base = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(double));
	const SIZE_T ParmOffset_1_Exponent = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(double));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(double*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_794176cc22220a890ca5552273721e71218367d2b1315751c06a78166f946f43_d5c8fa0ea0e7a139b4e53610692d4bb444e80d0c8a4b007796df0b18e50015c2(Execution,
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_0_Base),
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_1_Exponent));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int StaticWorldContextCheck(UObject, const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:324:5
 * StableFunctionKey  : 89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652
 * ExecutionHash      : 1702e631b2247cfc9c393f5ed92cd713195169b86d9799f6347e5a9147ffaeaf
 * DebugHash          : 924882274c9d49573379db54142e128eaa9570e480c78f746c788ab13582d1c1
 * EntryAbiHash       : 43bf555b1cdd803979e0fcaa94ab1a10d69fba57c0e1e44934d93cb64c5eb6d1
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_1702e631b2247cfc9c393f5ed92cd713195169b86d9799f6347e5a9147ffaeaf = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_1702e631b2247cfc9c393f5ed92cd713195169b86d9799f6347e5a9147ffaeaf
#endif

// AS Function : int StaticWorldContextCheck(UObject, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:324:5
// JIT Entry   : Raw
int32 ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_1702e631b2247cfc9c393f5ed92cd713195169b86d9799f6347e5a9147ffaeaf(FScriptExecution& Execution, UObject* p_WorldContextObject, asDWORD p_Value)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:324:5
// JIT Entry   : VM
void ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_1702e631b2247cfc9c393f5ed92cd713195169b86d9799f6347e5a9147ffaeaf_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_1702e631b2247cfc9c393f5ed92cd713195169b86d9799f6347e5a9147ffaeaf(Execution,
		*(UObject**)(l_fp + 0),
		*(asDWORD*)(l_fp + 2));
}
// AS Function : int StaticWorldContextCheck(UObject, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:324:5
// JIT Entry   : Parms
void ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_1702e631b2247cfc9c393f5ed92cd713195169b86d9799f6347e5a9147ffaeaf_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_WorldContextObject = ParmsOffset;
	ParmsOffset += sizeof(void*);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_1702e631b2247cfc9c393f5ed92cd713195169b86d9799f6347e5a9147ffaeaf(Execution,
		*(UObject**)(((SIZE_T)Parms) + ParmOffset_0_WorldContextObject),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_1_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void StorePrimitiveArg(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:239:7
 * StableFunctionKey  : 8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf
 * ExecutionHash      : 69cb1eab87c68262920370be1cba9c1d2849b417bb1c05547257a79d933dfab9
 * DebugHash          : 63c38dc33e9b465796ca0fe516cc3a30e8ff3cb75ecf70f57f2b70a3d3a19c7b
 * EntryAbiHash       : 59c253776ca706e4b802002aa63a0562f899465e5e8dcb6976ad0e14745f555a
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_69cb1eab87c68262920370be1cba9c1d2849b417bb1c05547257a79d933dfab9 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_69cb1eab87c68262920370be1cba9c1d2849b417bb1c05547257a79d933dfab9
#endif

// AS Function : void StorePrimitiveArg(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:239:7
// JIT Entry   : Raw
void ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_69cb1eab87c68262920370be1cba9c1d2849b417bb1c05547257a79d933dfab9(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:239:7
// JIT Entry   : VM
void ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_69cb1eab87c68262920370be1cba9c1d2849b417bb1c05547257a79d933dfab9_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_69cb1eab87c68262920370be1cba9c1d2849b417bb1c05547257a79d933dfab9(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : void StorePrimitiveArg(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:239:7
// JIT Entry   : Parms
void ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_69cb1eab87c68262920370be1cba9c1d2849b417bb1c05547257a79d933dfab9_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_69cb1eab87c68262920370be1cba9c1d2849b417bb1c05547257a79d933dfab9(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ExceptionEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:191:6
 * StableFunctionKey  : 91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277
 * ExecutionHash      : d8794b2352db4305cede1e40fd24e4d396c6562c8cc41bee03dd55d193036cd1
 * DebugHash          : e57ab1ae727029c4b496f4cf329ff64554b986a8b340784886b72cb8c235ed0a
 * EntryAbiHash       : 8e204c65d868a3511f949a72a8040d1e58f2c414351b97e6ddef4ad8bc9d5b08
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_d8794b2352db4305cede1e40fd24e4d396c6562c8cc41bee03dd55d193036cd1 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_d8794b2352db4305cede1e40fd24e4d396c6562c8cc41bee03dd55d193036cd1
#endif

// AS Function : int ExceptionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:191:6
// JIT Entry   : Raw
int32 ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_d8794b2352db4305cede1e40fd24e4d396c6562c8cc41bee03dd55d193036cd1(FScriptExecution& Execution, UObject* l_This)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:191:6
// JIT Entry   : VM
void ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_d8794b2352db4305cede1e40fd24e4d396c6562c8cc41bee03dd55d193036cd1_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_d8794b2352db4305cede1e40fd24e4d396c6562c8cc41bee03dd55d193036cd1(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ExceptionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:191:6
// JIT Entry   : Parms
void ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_d8794b2352db4305cede1e40fd24e4d396c6562c8cc41bee03dd55d193036cd1_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_d8794b2352db4305cede1e40fd24e4d396c6562c8cc41bee03dd55d193036cd1(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int RecurseForAOT(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:218:6
 * StableFunctionKey  : 947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7
 * ExecutionHash      : a5b2687dceaa7e598387645b23995ecf828e3bb1ae0506c1eb3b37690d97a4c5
 * DebugHash          : 2736376f14546c296cf1f22b79931fbc5253f37b3eab1fdbc62c654c6ab33652
 * EntryAbiHash       : 805d2b5d622dfc1d79e7a6e1f19c0e4068732e3afa951e3f3b429e924ed5af85
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_a5b2687dceaa7e598387645b23995ecf828e3bb1ae0506c1eb3b37690d97a4c5 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_a5b2687dceaa7e598387645b23995ecf828e3bb1ae0506c1eb3b37690d97a4c5
#endif

// AS Function : int RecurseForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:218:6
// JIT Entry   : Raw
int32 ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_a5b2687dceaa7e598387645b23995ecf828e3bb1ae0506c1eb3b37690d97a4c5(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:218:6
// JIT Entry   : VM
void ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_a5b2687dceaa7e598387645b23995ecf828e3bb1ae0506c1eb3b37690d97a4c5_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_a5b2687dceaa7e598387645b23995ecf828e3bb1ae0506c1eb3b37690d97a4c5(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : int RecurseForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:218:6
// JIT Entry   : Parms
void ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_a5b2687dceaa7e598387645b23995ecf828e3bb1ae0506c1eb3b37690d97a4c5_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_a5b2687dceaa7e598387645b23995ecf828e3bb1ae0506c1eb3b37690d97a4c5(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int GetVirtualValue_Implementation()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:310:7
 * StableFunctionKey  : 980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1
 * ExecutionHash      : ab8372186c6d939ce2f76bdbe84312468d68d5539f2e852adae019aaca801ec9
 * DebugHash          : 0c01248280c7b95c746f83338a6f1fc5c27f0da41120c2a283a294d6a29a0a60
 * EntryAbiHash       : 507cc902edc1d9bd3e2f2572fe86bb7d02b96a550445798d3f067222eb350188
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_ab8372186c6d939ce2f76bdbe84312468d68d5539f2e852adae019aaca801ec9 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_ab8372186c6d939ce2f76bdbe84312468d68d5539f2e852adae019aaca801ec9
#endif

// AS Function : int GetVirtualValue_Implementation()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:310:7
// JIT Entry   : Raw
int32 ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_ab8372186c6d939ce2f76bdbe84312468d68d5539f2e852adae019aaca801ec9(FScriptExecution& Execution, UObject* l_This)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:310:7
// JIT Entry   : VM
void ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_ab8372186c6d939ce2f76bdbe84312468d68d5539f2e852adae019aaca801ec9_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_ab8372186c6d939ce2f76bdbe84312468d68d5539f2e852adae019aaca801ec9(Execution,
		*(UObject**)l_fp);
}
// AS Function : int GetVirtualValue_Implementation()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:310:7
// JIT Entry   : Parms
void ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_ab8372186c6d939ce2f76bdbe84312468d68d5539f2e852adae019aaca801ec9_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_ab8372186c6d939ce2f76bdbe84312468d68d5539f2e852adae019aaca801ec9(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int BumpReferenceForAOT(int&inout)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:115:5
 * StableFunctionKey  : 9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05
 * ExecutionHash      : f8f442b53b4da051031a282719d96d1e294612c154e6aea2d0755311194e58f3
 * DebugHash          : 63d16f5b449c3a0b13a5b795758c25d7532689bc80fee08fd2a38f1a749ccbe8
 * EntryAbiHash       : b80efaaa778ead65430c5702610f23a7b6dccd414f3c044078aa45e30f15b055
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_f8f442b53b4da051031a282719d96d1e294612c154e6aea2d0755311194e58f3 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_f8f442b53b4da051031a282719d96d1e294612c154e6aea2d0755311194e58f3
#endif

// AS Function : int BumpReferenceForAOT(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:115:5
// JIT Entry   : Raw
int32 ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_f8f442b53b4da051031a282719d96d1e294612c154e6aea2d0755311194e58f3(FScriptExecution& Execution, int32* p_Value)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:115:5
// JIT Entry   : VM
void ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_f8f442b53b4da051031a282719d96d1e294612c154e6aea2d0755311194e58f3_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_f8f442b53b4da051031a282719d96d1e294612c154e6aea2d0755311194e58f3(Execution,
		*(int32**)(l_fp + 0));
}
// AS Function : int BumpReferenceForAOT(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:115:5
// JIT Entry   : Parms
void ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_f8f442b53b4da051031a282719d96d1e294612c154e6aea2d0755311194e58f3_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_f8f442b53b4da051031a282719d96d1e294612c154e6aea2d0755311194e58f3(Execution,
		(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ImportEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:149:5
 * StableFunctionKey  : 9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a
 * ExecutionHash      : bc08630e36a2da1376a543242f5471da90713ff8994134b1f01892007d244d35
 * DebugHash          : b1b4cb806648e427abfd4e7aaae2e9be0032a2e27f17c3019fbbdaaa8785f60e
 * EntryAbiHash       : 4b531289681eab0fc4f4f249dee5c27f1a8dedc1b24ed061e98876f5dedce042
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_bc08630e36a2da1376a543242f5471da90713ff8994134b1f01892007d244d35 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_bc08630e36a2da1376a543242f5471da90713ff8994134b1f01892007d244d35
#endif

// AS Function : int ImportEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:149:5
// JIT Entry   : Raw
int32 ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_bc08630e36a2da1376a543242f5471da90713ff8994134b1f01892007d244d35(FScriptExecution& Execution)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:149:5
// JIT Entry   : VM
void ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_bc08630e36a2da1376a543242f5471da90713ff8994134b1f01892007d244d35_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_bc08630e36a2da1376a543242f5471da90713ff8994134b1f01892007d244d35(Execution);
}
// AS Function : int ImportEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:149:5
// JIT Entry   : Parms
void ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_bc08630e36a2da1376a543242f5471da90713ff8994134b1f01892007d244d35_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_bc08630e36a2da1376a543242f5471da90713ff8994134b1f01892007d244d35(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ThreadSafeReturn()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:278:6
 * StableFunctionKey  : 9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7
 * ExecutionHash      : 6875bffdad0ee8bc048aef097bb13d7bbff856815c5919bd05e200d94e32f156
 * DebugHash          : 2e82456d2ff5f121c10c4e11043022add082284d18cb4006654c250a0177d630
 * EntryAbiHash       : dc83c109706793b798e437d104797c6bd2946d45dd2a7a22bc5dc94aa6809a03
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_6875bffdad0ee8bc048aef097bb13d7bbff856815c5919bd05e200d94e32f156 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_6875bffdad0ee8bc048aef097bb13d7bbff856815c5919bd05e200d94e32f156
#endif

// AS Function : int ThreadSafeReturn()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:278:6
// JIT Entry   : Raw
int32 ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_6875bffdad0ee8bc048aef097bb13d7bbff856815c5919bd05e200d94e32f156(FScriptExecution& Execution, UObject* l_This)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:278:6
// JIT Entry   : VM
void ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_6875bffdad0ee8bc048aef097bb13d7bbff856815c5919bd05e200d94e32f156_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_6875bffdad0ee8bc048aef097bb13d7bbff856815c5919bd05e200d94e32f156(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ThreadSafeReturn()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:278:6
// JIT Entry   : Parms
void ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_6875bffdad0ee8bc048aef097bb13d7bbff856815c5919bd05e200d94e32f156_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_6875bffdad0ee8bc048aef097bb13d7bbff856815c5919bd05e200d94e32f156(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void IncrementReference(int&inout)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:251:7
 * StableFunctionKey  : 9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e
 * ExecutionHash      : 74ab4d8dfbee841707ca8c56edc865a07b228ff55c787f548d12a4ae55fccda2
 * DebugHash          : 1be1f0083cfbbfd4a5fafd8abfd7580d4d5a455e1557f98e2b36154a0aa2d7f9
 * EntryAbiHash       : edfd275150d177872c0521a27feaed2db8935ff989a49d20f43a3627044ec161
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_74ab4d8dfbee841707ca8c56edc865a07b228ff55c787f548d12a4ae55fccda2 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_74ab4d8dfbee841707ca8c56edc865a07b228ff55c787f548d12a4ae55fccda2
#endif

// AS Function : void IncrementReference(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:251:7
// JIT Entry   : Raw
void ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_74ab4d8dfbee841707ca8c56edc865a07b228ff55c787f548d12a4ae55fccda2(FScriptExecution& Execution, UObject* l_This, int32* p_Value)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:251:7
// JIT Entry   : VM
void ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_74ab4d8dfbee841707ca8c56edc865a07b228ff55c787f548d12a4ae55fccda2_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_74ab4d8dfbee841707ca8c56edc865a07b228ff55c787f548d12a4ae55fccda2(Execution,
		*(UObject**)l_fp,
		*(int32**)(l_fp + 2));
}
// AS Function : void IncrementReference(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:251:7
// JIT Entry   : Parms
void ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_74ab4d8dfbee841707ca8c56edc865a07b228ff55c787f548d12a4ae55fccda2_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_74ab4d8dfbee841707ca8c56edc865a07b228ff55c787f548d12a4ae55fccda2(Execution,
		(UObject*)Object,
		(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void ClientStoreRpcValue(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:283:7
 * StableFunctionKey  : a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9
 * ExecutionHash      : d56bd6e1382cf480b779bbff12a35df154ce00a9638e9feb939ce0cba40a65a4
 * DebugHash          : 5fc0ddb7198cd3d38b1781a457343e1406d29a173bed07e8fe33ddfd4b58221d
 * EntryAbiHash       : 97c4a3b9d12b23ba1b0401fc6dc59e5b24fabb6eb7c3ce760a58844d3692b2f9
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9_d56bd6e1382cf480b779bbff12a35df154ce00a9638e9feb939ce0cba40a65a4 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9_d56bd6e1382cf480b779bbff12a35df154ce00a9638e9feb939ce0cba40a65a4
#endif

// AS Function : void ClientStoreRpcValue(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:283:7
// JIT Entry   : Raw
void ASJIT_a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9_d56bd6e1382cf480b779bbff12a35df154ce00a9638e9feb939ce0cba40a65a4(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:283:7
// JIT Entry   : VM
void ASJIT_a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9_d56bd6e1382cf480b779bbff12a35df154ce00a9638e9feb939ce0cba40a65a4_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9_d56bd6e1382cf480b779bbff12a35df154ce00a9638e9feb939ce0cba40a65a4(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : void ClientStoreRpcValue(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:283:7
// JIT Entry   : Parms
void ASJIT_a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9_d56bd6e1382cf480b779bbff12a35df154ce00a9638e9feb939ce0cba40a65a4_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ASJIT_a0700964917ac302b09c2bbc5e47cf1d01e9b65e39c619ec0d30af07ff9340a9_d56bd6e1382cf480b779bbff12a35df154ce00a9638e9feb939ce0cba40a65a4(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:50:5
 * StableFunctionKey  : a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b
 * ExecutionHash      : 18d15b8d5bd3f4789e921a45ff54539d9afce9dffd62cb44825a273ffda2328f
 * DebugHash          : 8bf65cc1e4776ade1eddfe8d564cb923a8b581a4b6b32e04bb343fb30a2638a0
 * EntryAbiHash       : 315429bd98bc57e8264be8ad0e7bc5007a550d671431aa6b3c1f76f34b2aa667
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_18d15b8d5bd3f4789e921a45ff54539d9afce9dffd62cb44825a273ffda2328f_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d40,
	const int32 as_ast_d41,
	const bool as_ast_d42,
	const bool as_ast_d43);

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
static int32 ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_18d15b8d5bd3f4789e921a45ff54539d9afce9dffd62cb44825a273ffda2328f_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d40,
	const int32 as_ast_d41,
	const bool as_ast_d42,
	const bool as_ast_d43)
{
	int32 as_ast_d44;
	{
		TypedFrame.SetSourcePosition(56, 6);
		([&]() -> int32
		{
			const int32 as_ast_e61_value = ([&]() -> int32
		{
			const int32 as_ast_e59_left = as_ast_d40;
			const int32 as_ast_e59_right = as_ast_d41;
			return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e59_left, as_ast_e59_right);
		}());
			int32& as_ast_e61_target = as_ast_d44;
			as_ast_e61_target = as_ast_e61_value;
			return as_ast_e61_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(57, 2);
		([&]() -> int32
		{
			const int32 as_ast_e66_value = ([&]() -> int32
		{
			const int32 as_ast_e65_left = as_ast_d44;
			const int32 as_ast_e65_right = int32(1);
			return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e65_left, as_ast_e65_right);
		}());
			int32& as_ast_e66_target = as_ast_d44;
			as_ast_e66_target = as_ast_e66_value;
			return as_ast_e66_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(58, 2);
		([&]() -> int32
		{
			const int32 as_ast_e69_value = int32(3);
			int32& as_ast_e69_target = as_ast_d44;
			as_ast_e69_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e69_target, as_ast_e69_value);
			return as_ast_e69_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(59, 2);
		([&]() -> int32
		{
			const int32 as_ast_e72_value = int32(1);
			int32& as_ast_e72_target = as_ast_d44;
			as_ast_e72_target = AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e72_target, as_ast_e72_value);
			return as_ast_e72_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(60, 2);
		([&]() -> int32
		{
			const int32 as_ast_e75_value = int32(2);
			int32& as_ast_e75_target = as_ast_d44;
			as_ast_e75_target = AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e75_target, as_ast_e75_value);
			return as_ast_e75_target;
		}());
	}
	int32 as_ast_d45;
	{
		TypedFrame.SetSourcePosition(62, 6);
		([&]() -> int32
		{
			const int32 as_ast_e79_value = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d44);
			int32& as_ast_e79_target = as_ast_d45;
			as_ast_e79_target = as_ast_e79_value;
			return as_ast_e79_target;
		}());
	}
	int32 as_ast_d46;
	{
		TypedFrame.SetSourcePosition(63, 6);
		([&]() -> int32
		{
			const int32 as_ast_e83_value = AngelscriptTypedASTJIT::WrapPostIncrement<int32>(as_ast_d44);
			int32& as_ast_e83_target = as_ast_d46;
			as_ast_e83_target = as_ast_e83_value;
			return as_ast_e83_target;
		}());
	}
	int32 as_ast_d47;
	{
		TypedFrame.SetSourcePosition(64, 6);
		([&]() -> int32
		{
			const int32 as_ast_e87_value = AngelscriptTypedASTJIT::WrapNegate<int32>(as_ast_d45);
			int32& as_ast_e87_target = as_ast_d47;
			as_ast_e87_target = as_ast_e87_value;
			return as_ast_e87_target;
		}());
	}
	int32 as_ast_d48;
	{
		TypedFrame.SetSourcePosition(65, 6);
		([&]() -> int32
		{
			const int32 as_ast_e91_value = AngelscriptTypedASTJIT::BitwiseNot<int32>(as_ast_d46);
			int32& as_ast_e91_target = as_ast_d48;
			as_ast_e91_target = as_ast_e91_value;
			return as_ast_e91_target;
		}());
	}
	int32 as_ast_d49;
	{
		TypedFrame.SetSourcePosition(66, 6);
		([&]() -> int32
		{
			const int32 as_ast_e96_value = ([&]() -> int32
		{
			const int32 as_ast_e94_left = as_ast_d44;
			const int32 as_ast_e94_right = int32(1);
			return AngelscriptTypedASTJIT::ShiftLeft<int32>(as_ast_e94_left, as_ast_e94_right);
		}());
			int32& as_ast_e96_target = as_ast_d49;
			as_ast_e96_target = as_ast_e96_value;
			return as_ast_e96_target;
		}());
	}
	int32 as_ast_d50;
	{
		TypedFrame.SetSourcePosition(67, 6);
		([&]() -> int32
		{
			const int32 as_ast_e101_value = ([&]() -> int32
		{
			const int32 as_ast_e99_left = as_ast_d44;
			const int32 as_ast_e99_right = int32(1);
			return AngelscriptTypedASTJIT::LogicalShiftRight<int32>(as_ast_e99_left, as_ast_e99_right);
		}());
			int32& as_ast_e101_target = as_ast_d50;
			as_ast_e101_target = as_ast_e101_value;
			return as_ast_e101_target;
		}());
	}
	bool as_ast_d51;
	{
		TypedFrame.SetSourcePosition(69, 7);
		([&]() -> bool
		{
			const bool as_ast_e109_value = (as_ast_d42 && ([&]() -> bool
		{
			const int32 as_ast_e106_left = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d44);
			const int32 as_ast_e106_right = as_ast_d41;
			return (as_ast_e106_left > as_ast_e106_right);
		}()));
			bool& as_ast_e109_target = as_ast_d51;
			as_ast_e109_target = as_ast_e109_value;
			return as_ast_e109_target;
		}());
	}
	bool as_ast_d52;
	{
		TypedFrame.SetSourcePosition(70, 7);
		([&]() -> bool
		{
			const bool as_ast_e116_value = ((as_ast_d51 && as_ast_d43) && IsRunningCommandlet());
			bool& as_ast_e116_target = as_ast_d52;
			as_ast_e116_target = as_ast_e116_value;
			return as_ast_e116_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(72, 2);
		if (as_ast_d52)
		{
			{
				TypedFrame.SetSourcePosition(74, 3);
				return ([&]() -> int32
			{
				const int32 as_ast_e122_left = ([&]() -> int32
			{
				const int32 as_ast_e121_left = as_ast_d49;
				const int32 as_ast_e121_right = as_ast_d48;
				return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e121_left, as_ast_e121_right);
			}());
				const int32 as_ast_e122_right = as_ast_d44;
				return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e122_left, as_ast_e122_right);
			}());
			}
		}
	}
	{
		TypedFrame.SetSourcePosition(76, 2);
		if (([&]() -> bool
		{
			const int32 as_ast_e125_left = as_ast_d50;
			const int32 as_ast_e125_right = int32(10);
			return (as_ast_e125_left > as_ast_e125_right);
		}()))
		{
			{
				TypedFrame.SetSourcePosition(78, 3);
				return ([&]() -> int32
			{
				const int32 as_ast_e130_left = ([&]() -> int32
			{
				const int32 as_ast_e129_left = as_ast_d50;
				const int32 as_ast_e129_right = int32(2);
				return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e129_left, as_ast_e129_right);
			}());
				const int32 as_ast_e130_right = as_ast_d44;
				return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e130_left, as_ast_e130_right);
			}());
			}
		}
	}
	{
		TypedFrame.SetSourcePosition(80, 2);
		return ([&]() -> int32
	{
		const int32 as_ast_e135_left = ([&]() -> int32
	{
		const int32 as_ast_e134_left = as_ast_d47;
		const int32 as_ast_e134_right = int32(1);
		return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e134_left, as_ast_e134_right);
	}());
		const int32 as_ast_e135_right = as_ast_d44;
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e135_left, as_ast_e135_right);
	}());
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:50:5
// JIT Entry   : Raw
int32 ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_18d15b8d5bd3f4789e921a45ff54539d9afce9dffd62cb44825a273ffda2328f_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1, asBYTE p_2, asBYTE p_3)
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
	return ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_18d15b8d5bd3f4789e921a45ff54539d9afce9dffd62cb44825a273ffda2328f_TypedBody(TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1), static_cast<bool>(p_2), static_cast<bool>(p_3));
}

// AS Function : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:50:5
// JIT Entry   : VM
void ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_18d15b8d5bd3f4789e921a45ff54539d9afce9dffd62cb44825a273ffda2328f_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_18d15b8d5bd3f4789e921a45ff54539d9afce9dffd62cb44825a273ffda2328f_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1),
		*(asBYTE*)(l_fp + 2),
		*(asBYTE*)(l_fp + 3));
}

// AS Function : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:50:5
// JIT Entry   : Parms
void ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_18d15b8d5bd3f4789e921a45ff54539d9afce9dffd62cb44825a273ffda2328f_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_18d15b8d5bd3f4789e921a45ff54539d9afce9dffd62cb44825a273ffda2328f_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)),
		static_cast<asBYTE>(*(bool*)(((SIZE_T)Parms) + ParmOffset_2)),
		static_cast<asBYTE>(*(bool*)(((SIZE_T)Parms) + ParmOffset_3)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int64 DoubleToInt64ForAOT(const float)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:88:7
 * StableFunctionKey  : a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6
 * ExecutionHash      : 1e2ca237c28c494967e7c30bc2817821cb48da2a29eb7d827ab3d44437443cb5
 * DebugHash          : afc4471deb7edbd30fc12cdf48c5347398880dbef3d01dc04da31dd332427378
 * EntryAbiHash       : a1d1604b170c8afa6d8aebaea217a68d24d4f67ff1cce99952cd0a22877c25f4
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_1e2ca237c28c494967e7c30bc2817821cb48da2a29eb7d827ab3d44437443cb5 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_1e2ca237c28c494967e7c30bc2817821cb48da2a29eb7d827ab3d44437443cb5
#endif

// AS Function : int64 DoubleToInt64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:88:7
// JIT Entry   : Raw
int64 ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_1e2ca237c28c494967e7c30bc2817821cb48da2a29eb7d827ab3d44437443cb5(FScriptExecution& Execution, double p_Value)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:88:7
// JIT Entry   : VM
void ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_1e2ca237c28c494967e7c30bc2817821cb48da2a29eb7d827ab3d44437443cb5_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int64*)l_outValue = ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_1e2ca237c28c494967e7c30bc2817821cb48da2a29eb7d827ab3d44437443cb5(Execution,
		*(double*)(l_fp + 0));
}
// AS Function : int64 DoubleToInt64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:88:7
// JIT Entry   : Parms
void ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_1e2ca237c28c494967e7c30bc2817821cb48da2a29eb7d827ab3d44437443cb5_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(int64));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int64*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_1e2ca237c28c494967e7c30bc2817821cb48da2a29eb7d827ab3d44437443cb5(Execution,
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : uint64 DoubleToUint64ForAOT(const float)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:175:9
 * StableFunctionKey  : aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb
 * ExecutionHash      : 8b311fefcf2f6effe78a491a03350940b76ed807fe4ae9e87a245edc9442616d
 * DebugHash          : b01c06b2132db825c16824dd98a484e7915e57f3aa39f8e9aa15e3abe69d11f9
 * EntryAbiHash       : 5e708ac1a7afdc6a24646cbb3780e38d2f6639308f316ff6b2ba697920d65899
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_8b311fefcf2f6effe78a491a03350940b76ed807fe4ae9e87a245edc9442616d = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_8b311fefcf2f6effe78a491a03350940b76ed807fe4ae9e87a245edc9442616d
#endif

// AS Function : uint64 DoubleToUint64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:175:9
// JIT Entry   : Raw
uint64 ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_8b311fefcf2f6effe78a491a03350940b76ed807fe4ae9e87a245edc9442616d(FScriptExecution& Execution, UObject* l_This, double p_Value)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:175:9
// JIT Entry   : VM
void ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_8b311fefcf2f6effe78a491a03350940b76ed807fe4ae9e87a245edc9442616d_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(uint64*)l_outValue = ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_8b311fefcf2f6effe78a491a03350940b76ed807fe4ae9e87a245edc9442616d(Execution,
		*(UObject**)l_fp,
		*(double*)(l_fp + 2));
}
// AS Function : uint64 DoubleToUint64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:175:9
// JIT Entry   : Parms
void ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_8b311fefcf2f6effe78a491a03350940b76ed807fe4ae9e87a245edc9442616d_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(uint64));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(uint64*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_8b311fefcf2f6effe78a491a03350940b76ed807fe4ae9e87a245edc9442616d(Execution,
		(UObject*)Object,
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ReturnPrimitive()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:245:6
 * StableFunctionKey  : b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367
 * ExecutionHash      : 9eddc7a2b780e553368cd408639b27f4cb107353ddc0d0e300f0bc69184f8e15
 * DebugHash          : 551c4c328ea3c7d0bb46efd5b9a7c44c4bf72028d4007f52e1d8022a6fc8464d
 * EntryAbiHash       : 020eef17e1be8204de01770a557fc8e26ac1d841e9e33088abf1b7cb9f3456b4
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_9eddc7a2b780e553368cd408639b27f4cb107353ddc0d0e300f0bc69184f8e15 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_9eddc7a2b780e553368cd408639b27f4cb107353ddc0d0e300f0bc69184f8e15
#endif

// AS Function : int ReturnPrimitive()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:245:6
// JIT Entry   : Raw
int32 ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_9eddc7a2b780e553368cd408639b27f4cb107353ddc0d0e300f0bc69184f8e15(FScriptExecution& Execution, UObject* l_This)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:245:6
// JIT Entry   : VM
void ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_9eddc7a2b780e553368cd408639b27f4cb107353ddc0d0e300f0bc69184f8e15_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_9eddc7a2b780e553368cd408639b27f4cb107353ddc0d0e300f0bc69184f8e15(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ReturnPrimitive()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:245:6
// JIT Entry   : Parms
void ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_9eddc7a2b780e553368cd408639b27f4cb107353ddc0d0e300f0bc69184f8e15_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_9eddc7a2b780e553368cd408639b27f4cb107353ddc0d0e300f0bc69184f8e15(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ReferenceEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:203:6
 * StableFunctionKey  : bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726
 * ExecutionHash      : 767d318bf06117ca44b154f0bc971f470646041676f0fa557b23513ac4f0b2c6
 * DebugHash          : 24ba0b274cd346f6d60068954899de2e93e49da3b7e6ab8d57f2263666f8018d
 * EntryAbiHash       : eb88e947baa4860face1585fa0d6b66fc0a33ee288c7ad728b2271e86a77d1e6
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_767d318bf06117ca44b154f0bc971f470646041676f0fa557b23513ac4f0b2c6 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_767d318bf06117ca44b154f0bc971f470646041676f0fa557b23513ac4f0b2c6
#endif

// AS Function : int ReferenceEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:203:6
// JIT Entry   : Raw
int32 ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_767d318bf06117ca44b154f0bc971f470646041676f0fa557b23513ac4f0b2c6(FScriptExecution& Execution, UObject* l_This)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:203:6
// JIT Entry   : VM
void ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_767d318bf06117ca44b154f0bc971f470646041676f0fa557b23513ac4f0b2c6_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_767d318bf06117ca44b154f0bc971f470646041676f0fa557b23513ac4f0b2c6(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ReferenceEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:203:6
// JIT Entry   : Parms
void ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_767d318bf06117ca44b154f0bc971f470646041676f0fa557b23513ac4f0b2c6_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_767d318bf06117ca44b154f0bc971f470646041676f0fa557b23513ac4f0b2c6(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UStaticJITAotFunctionCarrier()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:155:7
 * StableFunctionKey  : c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5
 * ExecutionHash      : cefdeb03d4bc239ba1973a62814f050d655dff01fe3da3cb3eedeb5ce9cd3cf2
 * DebugHash          : 04de1885d33170bc2c91b420df32fa2c555a525cbb581a12965c75ecb1a39004
 * EntryAbiHash       : fbabfad5e9e11fb97c083af33dc177e642e8c4e4d0b3a67f937cb34fa1ead35a
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_cefdeb03d4bc239ba1973a62814f050d655dff01fe3da3cb3eedeb5ce9cd3cf2 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_cefdeb03d4bc239ba1973a62814f050d655dff01fe3da3cb3eedeb5ce9cd3cf2
#endif

// AS Function : UStaticJITAotFunctionCarrier()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:155:7
// JIT Entry   : Raw
void ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_cefdeb03d4bc239ba1973a62814f050d655dff01fe3da3cb3eedeb5ce9cd3cf2(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("UStaticJITAotFunctionCarrier::UStaticJITAotFunctionCarrier()", 0);
SCRIPT_ASSUME_NO_EXCEPTION()
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
// LoadThisR +48
l_valueRegister = ((asQWORD)l_This) + POFFSET_UStaticJITAotFunctionCarrier_StoredValue;
// WRTV4 v1
memcpy((void*)l_valueRegister, (void*)(&v_TEMP_dword_1), 4);
// RET 2
  return;
}
// AS Function : UStaticJITAotFunctionCarrier()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:155:7
// JIT Entry   : VM
void ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_cefdeb03d4bc239ba1973a62814f050d655dff01fe3da3cb3eedeb5ce9cd3cf2_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_cefdeb03d4bc239ba1973a62814f050d655dff01fe3da3cb3eedeb5ce9cd3cf2(Execution,
		*(UObject**)l_fp);
}
// AS Function : UStaticJITAotFunctionCarrier()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:155:7
// JIT Entry   : Parms
void ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_cefdeb03d4bc239ba1973a62814f050d655dff01fe3da3cb3eedeb5ce9cd3cf2_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_cefdeb03d4bc239ba1973a62814f050d655dff01fe3da3cb3eedeb5ce9cd3cf2(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int GetStoredValue()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:157:7
 * StableFunctionKey  : c729a9d246ec742f89eb03aeacdc9681edbce4c78fb3dce09c719941be146f24
 * ExecutionHash      : 99a24355ff3d08e80fed16f2b58fd27762c800f1edefced783a8bc53d343e770
 * DebugHash          : 2e3fa190207e2426e02834031bb11e890a2321147118c8d8727778b8d81f5afe
 * EntryAbiHash       : 44e1144686c690ce60aa414a6c45c126690e4049536c603785aaaa3583dffca5
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_c729a9d246ec742f89eb03aeacdc9681edbce4c78fb3dce09c719941be146f24_99a24355ff3d08e80fed16f2b58fd27762c800f1edefced783a8bc53d343e770 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_c729a9d246ec742f89eb03aeacdc9681edbce4c78fb3dce09c719941be146f24_99a24355ff3d08e80fed16f2b58fd27762c800f1edefced783a8bc53d343e770
#endif

// AS Function : int GetStoredValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:157:7
// JIT Entry   : Raw
int32 ASJIT_c729a9d246ec742f89eb03aeacdc9681edbce4c78fb3dce09c719941be146f24_99a24355ff3d08e80fed16f2b58fd27762c800f1edefced783a8bc53d343e770(FScriptExecution& Execution, UObject* l_This)
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
void ASJIT_c729a9d246ec742f89eb03aeacdc9681edbce4c78fb3dce09c719941be146f24_99a24355ff3d08e80fed16f2b58fd27762c800f1edefced783a8bc53d343e770_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_c729a9d246ec742f89eb03aeacdc9681edbce4c78fb3dce09c719941be146f24_99a24355ff3d08e80fed16f2b58fd27762c800f1edefced783a8bc53d343e770(Execution,
		*(UObject**)l_fp);
}
// AS Function : int GetStoredValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:157:7
// JIT Entry   : Parms
void ASJIT_c729a9d246ec742f89eb03aeacdc9681edbce4c78fb3dce09c719941be146f24_99a24355ff3d08e80fed16f2b58fd27762c800f1edefced783a8bc53d343e770_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_c729a9d246ec742f89eb03aeacdc9681edbce4c78fb3dce09c719941be146f24_99a24355ff3d08e80fed16f2b58fd27762c800f1edefced783a8bc53d343e770(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UStaticJITAotVirtualParent UStaticJITAotVirtualParent()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:292:7
 * StableFunctionKey  : dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844
 * ExecutionHash      : ba6d296ea9a313ef441f8ef0185fb102be4ae81d8554a1bff61480bf2ac8043b
 * DebugHash          : 73a5467e793b745fa42d3ecfb02acb8f5106678d8669c38cf6ff178961918460
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:292:7
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:292:7
// JIT Entry   : VM
void ASJIT_dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844_ba6d296ea9a313ef441f8ef0185fb102be4ae81d8554a1bff61480bf2ac8043b_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(UObject**)l_outValue = ASJIT_dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844_ba6d296ea9a313ef441f8ef0185fb102be4ae81d8554a1bff61480bf2ac8043b(Execution);
}
// AS Function : UStaticJITAotVirtualParent UStaticJITAotVirtualParent()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:292:7
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:109:5
 * StableFunctionKey  : e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7
 * ExecutionHash      : 12fe2d9db6dd7f29ae0ae78796649b0d2d75aaa99c22e7f8dabf2203d504d9af
 * DebugHash          : f195afa05dba9801747854152962e0ccec1271316c873cc9894d4f18e83cd0cc
 * EntryAbiHash       : 8e204c65d868a3511f949a72a8040d1e58f2c414351b97e6ddef4ad8bc9d5b08
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_12fe2d9db6dd7f29ae0ae78796649b0d2d75aaa99c22e7f8dabf2203d504d9af = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_12fe2d9db6dd7f29ae0ae78796649b0d2d75aaa99c22e7f8dabf2203d504d9af
#endif

// AS Function : int ExceptionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:109:5
// JIT Entry   : Raw
int32 ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_12fe2d9db6dd7f29ae0ae78796649b0d2d75aaa99c22e7f8dabf2203d504d9af(FScriptExecution& Execution)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:109:5
// JIT Entry   : VM
void ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_12fe2d9db6dd7f29ae0ae78796649b0d2d75aaa99c22e7f8dabf2203d504d9af_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_12fe2d9db6dd7f29ae0ae78796649b0d2d75aaa99c22e7f8dabf2203d504d9af(Execution);
}
// AS Function : int ExceptionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:109:5
// JIT Entry   : Parms
void ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_12fe2d9db6dd7f29ae0ae78796649b0d2d75aaa99c22e7f8dabf2203d504d9af_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_12fe2d9db6dd7f29ae0ae78796649b0d2d75aaa99c22e7f8dabf2203d504d9af(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void ClientStoreRpcValue_Implementation(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:284:7
 * StableFunctionKey  : edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72
 * ExecutionHash      : 28666f138ea60bd7f6d05da13a6ec370632c5cb4733c39f8954d4978b35fbfba
 * DebugHash          : d50b0d8a98fd9dd7913c82269cc95243bc9c94e9cd4ffe8595b9bc5d2b633f4f
 * EntryAbiHash       : 66bc09dfe9cf807bdbf1d43484d609be4abf09f8f99f244b39083673e669630d
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72_28666f138ea60bd7f6d05da13a6ec370632c5cb4733c39f8954d4978b35fbfba = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72_28666f138ea60bd7f6d05da13a6ec370632c5cb4733c39f8954d4978b35fbfba
#endif

// AS Function : void ClientStoreRpcValue_Implementation(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:284:7
// JIT Entry   : Raw
void ASJIT_edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72_28666f138ea60bd7f6d05da13a6ec370632c5cb4733c39f8954d4978b35fbfba(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:284:7
// JIT Entry   : VM
void ASJIT_edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72_28666f138ea60bd7f6d05da13a6ec370632c5cb4733c39f8954d4978b35fbfba_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72_28666f138ea60bd7f6d05da13a6ec370632c5cb4733c39f8954d4978b35fbfba(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : void ClientStoreRpcValue_Implementation(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:284:7
// JIT Entry   : Parms
void ASJIT_edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72_28666f138ea60bd7f6d05da13a6ec370632c5cb4733c39f8954d4978b35fbfba_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ASJIT_edbf45b913acfefb2f99e93af3f48a2289ff929fa30c7476c74cc6fd4017ab72_28666f138ea60bd7f6d05da13a6ec370632c5cb4733c39f8954d4978b35fbfba(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int RecurseForAOT(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:135:5
 * StableFunctionKey  : f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556
 * ExecutionHash      : 782d82bd8196062633a7f4c5b0bc45f53f8ebc53f40331f6a05c410256529a4f
 * DebugHash          : b57d1331e0db6aba3dc1b38a36cca57cd0d7f4b4ac395495c3f02496c08abe13
 * EntryAbiHash       : 805d2b5d622dfc1d79e7a6e1f19c0e4068732e3afa951e3f3b429e924ed5af85
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_782d82bd8196062633a7f4c5b0bc45f53f8ebc53f40331f6a05c410256529a4f = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_782d82bd8196062633a7f4c5b0bc45f53f8ebc53f40331f6a05c410256529a4f
#endif

// AS Function : int RecurseForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:135:5
// JIT Entry   : Raw
int32 ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_782d82bd8196062633a7f4c5b0bc45f53f8ebc53f40331f6a05c410256529a4f(FScriptExecution& Execution, asDWORD p_Value)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:135:5
// JIT Entry   : VM
void ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_782d82bd8196062633a7f4c5b0bc45f53f8ebc53f40331f6a05c410256529a4f_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_782d82bd8196062633a7f4c5b0bc45f53f8ebc53f40331f6a05c410256529a4f(Execution,
		*(asDWORD*)(l_fp + 0));
}
// AS Function : int RecurseForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:135:5
// JIT Entry   : Parms
void ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_782d82bd8196062633a7f4c5b0bc45f53f8ebc53f40331f6a05c410256529a4f_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_782d82bd8196062633a7f4c5b0bc45f53f8ebc53f40331f6a05c410256529a4f(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int ObjectLastNativeForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:98:5
 * StableFunctionKey  : f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df
 * ExecutionHash      : c3d1ef99314a1c90ba08bfad485e030c6549c840e5bf07b5588af02758124e45
 * DebugHash          : a332361a68921a1323f086143bcfd1bb452cf5fde94df384e956fd44351f394c
 * EntryAbiHash       : 37241b3ba70d4b0f0af53358d5dcd4fda67f64c6db1715fe217a46674e3ba526
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_c3d1ef99314a1c90ba08bfad485e030c6549c840e5bf07b5588af02758124e45 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_c3d1ef99314a1c90ba08bfad485e030c6549c840e5bf07b5588af02758124e45
#endif

// AS Function : int ObjectLastNativeForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:98:5
// JIT Entry   : Raw
int32 ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_c3d1ef99314a1c90ba08bfad485e030c6549c840e5bf07b5588af02758124e45(FScriptExecution& Execution)
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
asDWORD v_TEMP_dword_2 = {};
asDWORD v_TEMP_dword_3 = {};
asDWORD v_TEMP_dword_4 = {};
asDWORD v_TEMP_dword_5 = {};
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
// LoadVObjR
l_valueRegister = (asQWORD)((&v_Value)) + (short)0;
// RDR4 v5
v_TEMP_dword_5 = value_read<asDWORD>((void*)l_valueRegister);
// CpyVtoV4 v2, v5
v_TEMP_dword_2 = v_TEMP_dword_5;
// JMP 0
goto LABEL_ObjectLastNativeForAOT_34;
LABEL_ObjectLastNativeForAOT_34:
// CpyVtoR4 v2
l_dwordRegister = v_TEMP_dword_2;
// RET 0
  return (int32)l_dwordRegister;
}
// AS Function : int ObjectLastNativeForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:98:5
// JIT Entry   : VM
void ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_c3d1ef99314a1c90ba08bfad485e030c6549c840e5bf07b5588af02758124e45_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_c3d1ef99314a1c90ba08bfad485e030c6549c840e5bf07b5588af02758124e45(Execution);
}
// AS Function : int ObjectLastNativeForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:98:5
// JIT Entry   : Parms
void ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_c3d1ef99314a1c90ba08bfad485e030c6549c840e5bf07b5588af02758124e45_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_c3d1ef99314a1c90ba08bfad485e030c6549c840e5bf07b5588af02758124e45(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int Entry()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:165:7
 * StableFunctionKey  : f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9
 * ExecutionHash      : f4cd91e40bdb070ac2bdfbc3e1f7cea4f66b9e18d5b6c9ccf161d1415afdca13
 * DebugHash          : 991757b4cb0e9a2cb54e09e083abb705d6d631d365ac3aeefad76fa43d27d7c7
 * EntryAbiHash       : db0c8f3058412e5bdc683b188ff132b140d00caf79e3e0658bda0254082fdee0
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_f4cd91e40bdb070ac2bdfbc3e1f7cea4f66b9e18d5b6c9ccf161d1415afdca13 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_f4cd91e40bdb070ac2bdfbc3e1f7cea4f66b9e18d5b6c9ccf161d1415afdca13
#endif

// AS Function : int Entry()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:165:7
// JIT Entry   : Raw
int32 ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_f4cd91e40bdb070ac2bdfbc3e1f7cea4f66b9e18d5b6c9ccf161d1415afdca13(FScriptExecution& Execution, UObject* l_This)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:165:7
// JIT Entry   : VM
void ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_f4cd91e40bdb070ac2bdfbc3e1f7cea4f66b9e18d5b6c9ccf161d1415afdca13_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_f4cd91e40bdb070ac2bdfbc3e1f7cea4f66b9e18d5b6c9ccf161d1415afdca13(Execution,
		*(UObject**)l_fp);
}
// AS Function : int Entry()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:165:7
// JIT Entry   : Parms
void ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_f4cd91e40bdb070ac2bdfbc3e1f7cea4f66b9e18d5b6c9ccf161d1415afdca13_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_f4cd91e40bdb070ac2bdfbc3e1f7cea4f66b9e18d5b6c9ccf161d1415afdca13(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticScalarBranch(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:8:5
 * StableFunctionKey  : f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e
 * ExecutionHash      : 38ca34e027ee1c58d210c9809e32affb1697361918797e9c2d7e65ba59d965d7
 * DebugHash          : 8dc933c86a3d17e858fb2bbdd042bd444442e3bcfdc11fab43b14e9de9348796
 * EntryAbiHash       : 108ad1ef8d5103b1a80d679116c872a1dc9d3ee674beaa1115ec65b288287e69
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e_38ca34e027ee1c58d210c9809e32affb1697361918797e9c2d7e65ba59d965d7 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e_38ca34e027ee1c58d210c9809e32affb1697361918797e9c2d7e65ba59d965d7
#endif

// AS Function : int SemanticScalarBranch(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:8:5
// JIT Entry   : Raw
int32 ASJIT_f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e_38ca34e027ee1c58d210c9809e32affb1697361918797e9c2d7e65ba59d965d7(FScriptExecution& Execution, asDWORD p_A, asDWORD p_B)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:8:5
// JIT Entry   : VM
void ASJIT_f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e_38ca34e027ee1c58d210c9809e32affb1697361918797e9c2d7e65ba59d965d7_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e_38ca34e027ee1c58d210c9809e32affb1697361918797e9c2d7e65ba59d965d7(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}
// AS Function : int SemanticScalarBranch(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:8:5
// JIT Entry   : Parms
void ASJIT_f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e_38ca34e027ee1c58d210c9809e32affb1697361918797e9c2d7e65ba59d965d7_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_A = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1_B = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_f27dbbf19f895fff90a610aba95f8894aac43049ffbb56ff2c0464841c1c4e2e_38ca34e027ee1c58d210c9809e32affb1697361918797e9c2d7e65ba59d965d7(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_A),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_1_B));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UStaticJITAotVirtualParent()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:292:7
 * StableFunctionKey  : f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef
 * ExecutionHash      : cb29352ee1d15f8bdbe055b7af0e99b734549c22f7571af0e803ba54ab80ead9
 * DebugHash          : 73a5467e793b745fa42d3ecfb02acb8f5106678d8669c38cf6ff178961918460
 * EntryAbiHash       : 1075e6d88a0fee09d9074b0d2222d8f08c7b158c0d6f762b28f4e154db4d37e2
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_cb29352ee1d15f8bdbe055b7af0e99b734549c22f7571af0e803ba54ab80ead9 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_cb29352ee1d15f8bdbe055b7af0e99b734549c22f7571af0e803ba54ab80ead9
#endif

// AS Function : UStaticJITAotVirtualParent()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:292:7
// JIT Entry   : Raw
void ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_cb29352ee1d15f8bdbe055b7af0e99b734549c22f7571af0e803ba54ab80ead9(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("UStaticJITAotVirtualParent::UStaticJITAotVirtualParent()", 0);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
// JitEntry *
// RET 2
  return;
}
// AS Function : UStaticJITAotVirtualParent()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:292:7
// JIT Entry   : VM
void ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_cb29352ee1d15f8bdbe055b7af0e99b734549c22f7571af0e803ba54ab80ead9_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_cb29352ee1d15f8bdbe055b7af0e99b734549c22f7571af0e803ba54ab80ead9(Execution,
		*(UObject**)l_fp);
}
// AS Function : UStaticJITAotVirtualParent()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:292:7
// JIT Entry   : Parms
void ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_cb29352ee1d15f8bdbe055b7af0e99b734549c22f7571af0e803ba54ab80ead9_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_cb29352ee1d15f8bdbe055b7af0e99b734549c22f7571af0e803ba54ab80ead9(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int BytecodeFallbackGeneratedWorldContext(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:317:5
 * StableFunctionKey  : f509813c02da6025acae1af26ea86c8f9e3130851803399d0b2db25a166e56ae
 * ExecutionHash      : e1f5512913551001ff94fe74bb1c7a9d59fbc2222e12ae55d72e43b78f7400b5
 * DebugHash          : 5b805df8231595e26815d3a45e90fde3bda2738baa2dd00f974563698957a08d
 * EntryAbiHash       : d9d9e18805e2fe81f0ff248a92bd9d347eefb34eb7a9fd3b93213e2e4ea0cbbc
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f509813c02da6025acae1af26ea86c8f9e3130851803399d0b2db25a166e56ae_e1f5512913551001ff94fe74bb1c7a9d59fbc2222e12ae55d72e43b78f7400b5 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f509813c02da6025acae1af26ea86c8f9e3130851803399d0b2db25a166e56ae_e1f5512913551001ff94fe74bb1c7a9d59fbc2222e12ae55d72e43b78f7400b5
#endif

// AS Function : int BytecodeFallbackGeneratedWorldContext(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:317:5
// JIT Entry   : Raw
int32 ASJIT_f509813c02da6025acae1af26ea86c8f9e3130851803399d0b2db25a166e56ae_e1f5512913551001ff94fe74bb1c7a9d59fbc2222e12ae55d72e43b78f7400b5(FScriptExecution& Execution, asDWORD p_Value)
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
void* v_TEMP_11 = nullptr;
asDWORD v_TEMP_dword_5 = {};
asDWORD v_TEMP_dword_12 = {};
asDWORD v_TEMP_dword_13 = {};
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
// PSF v4
// PopRPtr
l_valueRegister = (asQWORD)((&v_Marker));
// CpyRtoV8 v9
v_TEMP_9 = (void*)l_valueRegister;
// PshVPtr v7
// PshVPtr v9
// CALLSYS *
// FString& FString::opAssign(const FString&inout Other)
{
  FString* Object = (FString*)(v_TEMP_9);
SCRIPT_DEBUG_CALLSTACK_POSITION(319, 10);
  if(Object != nullptr)
  {
const FString& FunctionReturnValue = Object->operator=((*(FString*)(v_TEMP_7)));
l_valueRegister = (asQWORD)&FunctionReturnValue;
} else [[unlikely]] {
SCRIPT_NULL_POINTER_EXCEPTION();
return {};
}
}
// JitEntry *
// SUSPEND
// JitEntry *
// PSF v4
// PopRPtr
l_valueRegister = (asQWORD)((&v_Marker));
// CpyRtoV8 v11
v_TEMP_11 = (void*)l_valueRegister;
// PshVPtr v11
// CALLSYS *
// int FString::Len() const
{
  FString* Object = (FString*)(v_TEMP_11);
SCRIPT_DEBUG_CALLSTACK_POSITION(320, 2);
  if(Object != nullptr)
  {
int32 FunctionReturnValue = Object->Len();
l_dwordRegister = (asDWORD&)FunctionReturnValue;
} else [[unlikely]] {
SCRIPT_NULL_POINTER_EXCEPTION();
return {};
}
}
// JitEntry *
// CpyRtoV4 v12
v_TEMP_dword_12 = l_dwordRegister;
// ADDi v5, v0, v12
((int32&)v_TEMP_dword_5) = ((int32&)p_Value) + ((int32&)v_TEMP_dword_12);
// JMP 0
goto LABEL_BytecodeFallbackGeneratedWorldContext_46;
LABEL_BytecodeFallbackGeneratedWorldContext_46:
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:317:5
// JIT Entry   : VM
void ASJIT_f509813c02da6025acae1af26ea86c8f9e3130851803399d0b2db25a166e56ae_e1f5512913551001ff94fe74bb1c7a9d59fbc2222e12ae55d72e43b78f7400b5_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_f509813c02da6025acae1af26ea86c8f9e3130851803399d0b2db25a166e56ae_e1f5512913551001ff94fe74bb1c7a9d59fbc2222e12ae55d72e43b78f7400b5(Execution,
		*(asDWORD*)(l_fp + 0));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void ResetStoredValue()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:233:7
 * StableFunctionKey  : f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7
 * ExecutionHash      : 1dd0bbca94859e2412175b28a8f11689b0eb12ced7efef993c58a870993c55c3
 * DebugHash          : 3cf58fe956ec1cea0ac9b47a0800faf8b9b861768c075e0dbfa919dfe79e90a5
 * EntryAbiHash       : 9b0857b686ac6c0536f6c1b33a7f4ce1dbb76c78831fe5af95fb30829626a6c6
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_1dd0bbca94859e2412175b28a8f11689b0eb12ced7efef993c58a870993c55c3 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_1dd0bbca94859e2412175b28a8f11689b0eb12ced7efef993c58a870993c55c3
#endif

// AS Function : void ResetStoredValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:233:7
// JIT Entry   : Raw
void ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_1dd0bbca94859e2412175b28a8f11689b0eb12ced7efef993c58a870993c55c3(FScriptExecution& Execution, UObject* l_This)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:233:7
// JIT Entry   : VM
void ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_1dd0bbca94859e2412175b28a8f11689b0eb12ced7efef993c58a870993c55c3_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_1dd0bbca94859e2412175b28a8f11689b0eb12ced7efef993c58a870993c55c3(Execution,
		*(UObject**)l_fp);
}
// AS Function : void ResetStoredValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:233:7
// JIT Entry   : Parms
void ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_1dd0bbca94859e2412175b28a8f11689b0eb12ced7efef993c58a870993c55c3_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_1dd0bbca94859e2412175b28a8f11689b0eb12ced7efef993c58a870993c55c3(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int CallVirtualValue()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:300:6
 * StableFunctionKey  : fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce
 * ExecutionHash      : 909e62365f5e0496ee335055ac80edd758b6afeea32536512b6a481fe9fb2a45
 * DebugHash          : f4d8a0afd868222ea9db0452db83b1fa0b7c458d3724e3027fd6a1928dfae51c
 * EntryAbiHash       : 6031fb726462dbf41570b41d3d37659d395e860ef3d2bc07067ce4b4bce8b1ad
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_909e62365f5e0496ee335055ac80edd758b6afeea32536512b6a481fe9fb2a45 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_909e62365f5e0496ee335055ac80edd758b6afeea32536512b6a481fe9fb2a45
#endif

// AS Function : int CallVirtualValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:300:6
// JIT Entry   : Raw
int32 ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_909e62365f5e0496ee335055ac80edd758b6afeea32536512b6a481fe9fb2a45(FScriptExecution& Execution, UObject* l_This)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:300:6
// JIT Entry   : VM
void ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_909e62365f5e0496ee335055ac80edd758b6afeea32536512b6a481fe9fb2a45_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_909e62365f5e0496ee335055ac80edd758b6afeea32536512b6a481fe9fb2a45(Execution,
		*(UObject**)l_fp);
}
// AS Function : int CallVirtualValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:300:6
// JIT Entry   : Parms
void ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_909e62365f5e0496ee335055ac80edd758b6afeea32536512b6a481fe9fb2a45_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_909e62365f5e0496ee335055ac80edd758b6afeea32536512b6a481fe9fb2a45(Execution,
		(UObject*)Object);
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
