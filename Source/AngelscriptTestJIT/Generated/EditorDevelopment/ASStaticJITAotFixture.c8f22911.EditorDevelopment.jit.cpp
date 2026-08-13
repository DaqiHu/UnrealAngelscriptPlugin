// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Module
 * VirtualSourcePath   : /Angelscript/Game/ASStaticJITAotFixture.as
 * CanonicalModuleName : ASStaticJITAotFixture
 * TargetProfile       : EditorDevelopment
 * StableModuleKey     : c8f2291154afd48cf9d8c267f8bfcf5bff307741bbaa61ea978863ed08f780ed
 * ProviderId          : 7dc70fd55a76cc55d726e32d5c2434713bd93755a32e4dd524a4b7c39c8b5d44
 * ArtifactProfile     : b58efc4c9f40879f0b98bcb54704d1b342ae56948a00e9ddbaa709787226e0c8
 * NativeEnvironment   : c8d9638e7c34100b327617ea9a4769bd7692413e17eb4705818e6b120acc9651
 * FunctionCount       : 44
 */

#if WITH_EDITOR && UE_BUILD_DEVELOPMENT
#include "StaticJIT/StaticJITConfig.h"
#ifndef AS_SKIP_JITTED_CODE


#include "StaticJIT/StaticJITHeader.h"


constexpr SIZE_T POFFSET_UStaticJITAotFunctionCarrier_StoredValue = Align(sizeof(UObject) + 0, 4);
constexpr SIZE_T PALIGN_UStaticJITAotFunctionCarrier_StoredValue = AlignmentMax(alignof(UObject), 4);
constexpr SIZE_T TALIGN_UStaticJITAotFunctionCarrier = PALIGN_UStaticJITAotFunctionCarrier_StoredValue;
constexpr SIZE_T TSIZE_UStaticJITAotFunctionCarrier = Align(POFFSET_UStaticJITAotFunctionCarrier_StoredValue + 4, TALIGN_UStaticJITAotFunctionCarrier);
/*
 * AngelScript Static JIT Function
 * Declaration       : uint64 DoubleToUint64ForAOT(const float)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:18:1
 * StableFunctionKey  : 04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29
 * ExecutionHash      : 731ebcfc6e1ae2e5f16413e8e02b51ce0c34139e6530879c3d632720567416d6
 * DebugHash          : 54cf152f876eb02dadf36f8d6a4182486ffc908293850b867c94568a6f2b83f3
 * EntryAbiHash       : 6f4f7e6615f922448dfa37b3c2b25936c0efef3f3655e58bd8cd07adf451eb00
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_731ebcfc6e1ae2e5f16413e8e02b51ce0c34139e6530879c3d632720567416d6 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_731ebcfc6e1ae2e5f16413e8e02b51ce0c34139e6530879c3d632720567416d6
#endif

// AS Function : uint64 DoubleToUint64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:18:1
// JIT Entry   : Raw
uint64 ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_731ebcfc6e1ae2e5f16413e8e02b51ce0c34139e6530879c3d632720567416d6(FScriptExecution& Execution, double p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("uint64 DoubleToUint64ForAOT(float)", 20);
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:18:1
// JIT Entry   : VM
void ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_731ebcfc6e1ae2e5f16413e8e02b51ce0c34139e6530879c3d632720567416d6_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(uint64*)l_outValue = ASJIT_04656056b181615ce098ab4f0b249d3bfc60df346f3f2531c82d9ecffe190d29_731ebcfc6e1ae2e5f16413e8e02b51ce0c34139e6530879c3d632720567416d6(Execution,
		*(double*)(l_fp + 0));
}
// AS Function : uint64 DoubleToUint64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:18:1
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:227:0
 * StableFunctionKey  : 0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe
 * ExecutionHash      : 5ff14395e534561f56d7569924ccefed5ca7db316e28b96e960c783d4963dbbe
 * DebugHash          : e99c1b7c1598b7ed5632a3ec913c738a97522a396a8ed5610511320c8b6818b1
 * EntryAbiHash       : f4ea6bf6dce35401edbfb9140c0645eef885586f4d70b43ce14a5fd70e37976f
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe_5ff14395e534561f56d7569924ccefed5ca7db316e28b96e960c783d4963dbbe = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe_5ff14395e534561f56d7569924ccefed5ca7db316e28b96e960c783d4963dbbe
#endif

// AS Function : UStaticJITAotVirtualChild UStaticJITAotVirtualChild()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:227:0
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
SCRIPT_DEBUG_CALLSTACK_LINE(0);
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
  CallContext->Execute();
  if (Execution.bExceptionThrown || CallContext->m_status != asEXECUTION_FINISHED)
  {
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:227:0
// JIT Entry   : VM
void ASJIT_0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe_5ff14395e534561f56d7569924ccefed5ca7db316e28b96e960c783d4963dbbe_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(UObject**)l_outValue = ASJIT_0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe_5ff14395e534561f56d7569924ccefed5ca7db316e28b96e960c783d4963dbbe(Execution);
}
// AS Function : UStaticJITAotVirtualChild UStaticJITAotVirtualChild()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:227:0
// JIT Entry   : Parms
void ASJIT_0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe_5ff14395e534561f56d7569924ccefed5ca7db316e28b96e960c783d4963dbbe_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(UObject**)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_0d70786b00bbff45f906a90947a9586f5e66a13baf5d9bcf79ceeeabb01aa1fe_5ff14395e534561f56d7569924ccefed5ca7db316e28b96e960c783d4963dbbe(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int RecursionEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:151:2
 * StableFunctionKey  : 13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c
 * ExecutionHash      : 48d363b5454e3ef4da6a26fbaf9dc9113da26d724fc20ec8a3d016b950b2d9d6
 * DebugHash          : 87069aba39581f19f492b3e6750f601c971eef51ad0928a459702d5892034d95
 * EntryAbiHash       : 9f485de7d7b887e42e4be9c65cdea010c43234c198082f955ad363bccb9952f0
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_48d363b5454e3ef4da6a26fbaf9dc9113da26d724fc20ec8a3d016b950b2d9d6 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_48d363b5454e3ef4da6a26fbaf9dc9113da26d724fc20ec8a3d016b950b2d9d6
#endif

// AS Function : int RecursionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:151:2
// JIT Entry   : Raw
int32 ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_48d363b5454e3ef4da6a26fbaf9dc9113da26d724fc20ec8a3d016b950b2d9d6(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::RecursionEntryForAOT()", 153);
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
SCRIPT_DEBUG_CALLSTACK_LINE(153);
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
  CallContext->Execute();
  if (Execution.bExceptionThrown || CallContext->m_status != asEXECUTION_FINISHED)
  {
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:151:2
// JIT Entry   : VM
void ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_48d363b5454e3ef4da6a26fbaf9dc9113da26d724fc20ec8a3d016b950b2d9d6_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_13805b7f4695cd00c381752f76d1e724d269499b752fbf0c68780993237a7a5c_48d363b5454e3ef4da6a26fbaf9dc9113da26d724fc20ec8a3d016b950b2d9d6(Execution,
		*(UObject**)l_fp);
}
// AS Function : int RecursionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:151:2
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:212:3
 * StableFunctionKey  : 191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5
 * ExecutionHash      : 4bd3c1036a3f94095c4d862d4ad7636a344a6cb39d21e9879d8235c00b1f8772
 * DebugHash          : 9e8ec365ac3042782e302640b6a8110b1bda250c80ead6569a835ee981a923d9
 * EntryAbiHash       : 93970f84a6aa1bac802f6979d57ff2163ff4f46543f8fc9ab6574d7e2f0d4e63
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_4bd3c1036a3f94095c4d862d4ad7636a344a6cb39d21e9879d8235c00b1f8772 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_4bd3c1036a3f94095c4d862d4ad7636a344a6cb39d21e9879d8235c00b1f8772
#endif

// AS Function : int GetVirtualValue_Implementation()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:212:3
// JIT Entry   : Raw
int32 ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_4bd3c1036a3f94095c4d862d4ad7636a344a6cb39d21e9879d8235c00b1f8772(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotVirtualParent::GetVirtualValue_Implementation()", 214);
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:212:3
// JIT Entry   : VM
void ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_4bd3c1036a3f94095c4d862d4ad7636a344a6cb39d21e9879d8235c00b1f8772_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_191b01d814077af27a0fcedc634df0861216db35b52b9fd0e358c119049572d5_4bd3c1036a3f94095c4d862d4ad7636a344a6cb39d21e9879d8235c00b1f8772(Execution,
		*(UObject**)l_fp);
}
// AS Function : int GetVirtualValue_Implementation()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:212:3
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:105:2
 * StableFunctionKey  : 1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f
 * ExecutionHash      : f23fb7fb95649c31568b374d896d6d0cfbd204e8d2b30b4fa4e7b9e779061880
 * DebugHash          : b310d190a258fb3b43c812e15ca59f190f7d9a291d61d1049b507d54dc63892b
 * EntryAbiHash       : cfd6270223e115f05bd9303f7ac38a55aae7a365f523814f643a93593cef6158
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_f23fb7fb95649c31568b374d896d6d0cfbd204e8d2b30b4fa4e7b9e779061880 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_f23fb7fb95649c31568b374d896d6d0cfbd204e8d2b30b4fa4e7b9e779061880
#endif

// AS Function : int ObjectLastNativeForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:105:2
// JIT Entry   : Raw
int32 ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_f23fb7fb95649c31568b374d896d6d0cfbd204e8d2b30b4fa4e7b9e779061880(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::ObjectLastNativeForAOT()", 107);
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
SCRIPT_DEBUG_CALLSTACK_LINE(107);
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:105:2
// JIT Entry   : VM
void ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_f23fb7fb95649c31568b374d896d6d0cfbd204e8d2b30b4fa4e7b9e779061880_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_1f604268ad8bae0a35618644e03534c6f753062e1f13814655013f2e6b63c15f_f23fb7fb95649c31568b374d896d6d0cfbd204e8d2b30b4fa4e7b9e779061880(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ObjectLastNativeForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:105:2
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:84:2
 * StableFunctionKey  : 2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5
 * ExecutionHash      : 87e8e30475f9cbded1fa2410dfa61a9fa9ca8726cd71c4173e2bef0ab315039e
 * DebugHash          : c6523acd6794d06ffd57a151470649397ae09f6fc7999567a1751d184f6a6d9a
 * EntryAbiHash       : 9c054d8b7509b3db28b755f4a912c3b9368acf7b6b5a56b3141baa6e6afbded7
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_87e8e30475f9cbded1fa2410dfa61a9fa9ca8726cd71c4173e2bef0ab315039e = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_87e8e30475f9cbded1fa2410dfa61a9fa9ca8726cd71c4173e2bef0ab315039e
#endif

// AS Function : int AddForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:84:2
// JIT Entry   : Raw
int32 ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_87e8e30475f9cbded1fa2410dfa61a9fa9ca8726cd71c4173e2bef0ab315039e(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::AddForAOT(int)", 86);
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:84:2
// JIT Entry   : VM
void ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_87e8e30475f9cbded1fa2410dfa61a9fa9ca8726cd71c4173e2bef0ab315039e_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_2693c34fd3fe777fb146ac3c9ae6bcc0ca35e0c91513e8aa67749d7760db38a5_87e8e30475f9cbded1fa2410dfa61a9fa9ca8726cd71c4173e2bef0ab315039e(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : int AddForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:84:2
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:46:1
 * StableFunctionKey  : 2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9
 * ExecutionHash      : e73ef907c33b0fc6c37c610a5e074617f248d291b078002f51991fe36eec5674
 * DebugHash          : 3161109723ceb6667c4c828f61bd28d355770672355553b0aac0a31c27977546
 * EntryAbiHash       : 821f49e8200c05323110360fc816375596ff5caaa0e0b3a7583f8c90b8e600f7
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_e73ef907c33b0fc6c37c610a5e074617f248d291b078002f51991fe36eec5674 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_e73ef907c33b0fc6c37c610a5e074617f248d291b078002f51991fe36eec5674
#endif

// AS Function : int ReferenceEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:46:1
// JIT Entry   : Raw
int32 ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_e73ef907c33b0fc6c37c610a5e074617f248d291b078002f51991fe36eec5674(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int ReferenceEntryForAOT()", 48);
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
SCRIPT_DEBUG_CALLSTACK_LINE(49);
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
  CallContext->Execute();
  if (Execution.bExceptionThrown || CallContext->m_status != asEXECUTION_FINISHED)
  {
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:46:1
// JIT Entry   : VM
void ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_e73ef907c33b0fc6c37c610a5e074617f248d291b078002f51991fe36eec5674_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_2957df48067da70f007536544c759372c24cebdf8c13d22b7bcfbab8d45a19c9_e73ef907c33b0fc6c37c610a5e074617f248d291b078002f51991fe36eec5674(Execution);
}
// AS Function : int ReferenceEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:46:1
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:196:2
 * StableFunctionKey  : 30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c
 * ExecutionHash      : 974769158959206c4fbb982fd641349ba8f108266ac90ad1787b06d55e150876
 * DebugHash          : 175960bfeb1aca9c5e425e0cc78463bc2625b8706401e39763d3c5d3949130db
 * EntryAbiHash       : 331f1f4848cecb7fcef6fc7a39b5607f3e77b7bbaed38d45c4308c28ba5e2fdc
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_974769158959206c4fbb982fd641349ba8f108266ac90ad1787b06d55e150876 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_974769158959206c4fbb982fd641349ba8f108266ac90ad1787b06d55e150876
#endif

// AS Function : int AddTwo(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:196:2
// JIT Entry   : Raw
int32 ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_974769158959206c4fbb982fd641349ba8f108266ac90ad1787b06d55e150876(FScriptExecution& Execution, UObject* l_This, asDWORD p_A, asDWORD p_B)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::AddTwo(int, int)", 198);
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:196:2
// JIT Entry   : VM
void ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_974769158959206c4fbb982fd641349ba8f108266ac90ad1787b06d55e150876_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_30446f73aab3e43d2469b8b54f7edd9be9c8ab8606336a0a771471c1aa7a508c_974769158959206c4fbb982fd641349ba8f108266ac90ad1787b06d55e150876(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2),
		*(asDWORD*)(l_fp + 3));
}
// AS Function : int AddTwo(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:196:2
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:29:1
 * StableFunctionKey  : 34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f
 * ExecutionHash      : fabd5fa6aad88b7821a069f290857dc6bce85d88e48d44a9e9657b2eee35c1a0
 * DebugHash          : 339e0d5b6080801255146730686d007c2c64e20a066eb0b7ecdc50a134a77c95
 * EntryAbiHash       : 105cf35ee25eb110af27107d43aba405d91a3d33de666332164d5436f152e063
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_fabd5fa6aad88b7821a069f290857dc6bce85d88e48d44a9e9657b2eee35c1a0 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_fabd5fa6aad88b7821a069f290857dc6bce85d88e48d44a9e9657b2eee35c1a0
#endif

// AS Function : void FailForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:29:1
// JIT Entry   : Raw
void ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_fabd5fa6aad88b7821a069f290857dc6bce85d88e48d44a9e9657b2eee35c1a0(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("void FailForAOT()", 31);
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
SCRIPT_DEBUG_CALLSTACK_LINE(31);
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:29:1
// JIT Entry   : VM
void ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_fabd5fa6aad88b7821a069f290857dc6bce85d88e48d44a9e9657b2eee35c1a0_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_fabd5fa6aad88b7821a069f290857dc6bce85d88e48d44a9e9657b2eee35c1a0(Execution);
}
// AS Function : void FailForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:29:1
// JIT Entry   : Parms
void ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_fabd5fa6aad88b7821a069f290857dc6bce85d88e48d44a9e9657b2eee35c1a0_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_34715362da7aad5998bedecc1ec030d2b259a17300a75124152daacf24e4c54f_fabd5fa6aad88b7821a069f290857dc6bce85d88e48d44a9e9657b2eee35c1a0(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int RecursionEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:69:1
 * StableFunctionKey  : 3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5
 * ExecutionHash      : e25691bf1c6694ad7ed2809095608dd750fb4f658eb9106dc9346f07810da1e1
 * DebugHash          : a0adcc0eda7e24b1ba0469752d6f34280598c830c43e699ac7f71b755386d9eb
 * EntryAbiHash       : 9f485de7d7b887e42e4be9c65cdea010c43234c198082f955ad363bccb9952f0
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_e25691bf1c6694ad7ed2809095608dd750fb4f658eb9106dc9346f07810da1e1 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_e25691bf1c6694ad7ed2809095608dd750fb4f658eb9106dc9346f07810da1e1
#endif

// AS Function : int RecursionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:69:1
// JIT Entry   : Raw
int32 ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_e25691bf1c6694ad7ed2809095608dd750fb4f658eb9106dc9346f07810da1e1(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int RecursionEntryForAOT()", 71);
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
SCRIPT_DEBUG_CALLSTACK_LINE(71);
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
  CallContext->Execute();
  if (Execution.bExceptionThrown || CallContext->m_status != asEXECUTION_FINISHED)
  {
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:69:1
// JIT Entry   : VM
void ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_e25691bf1c6694ad7ed2809095608dd750fb4f658eb9106dc9346f07810da1e1_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_3d027f30fbfebd291c60460ba22437b8b1c8a83663cd4cec9001b3b31f195df5_e25691bf1c6694ad7ed2809095608dd750fb4f658eb9106dc9346f07810da1e1(Execution);
}
// AS Function : int RecursionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:69:1
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:135:2
 * StableFunctionKey  : 40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135
 * ExecutionHash      : 0b9e1f34b7f61af8d6807ddc1048c145b5001e3240c90d3c036b90465f9a167f
 * DebugHash          : cf8377f6c2d2538f568c55f16ec71727583e8ae6ad85a37aabc23fbe143c4c83
 * EntryAbiHash       : ad7dac870b2eb6d42697eb4d504c8b5072ece4829ebea8db79cafb9cfe196aee
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_0b9e1f34b7f61af8d6807ddc1048c145b5001e3240c90d3c036b90465f9a167f = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_0b9e1f34b7f61af8d6807ddc1048c145b5001e3240c90d3c036b90465f9a167f
#endif

// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:135:2
// JIT Entry   : Raw
int32 ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_0b9e1f34b7f61af8d6807ddc1048c145b5001e3240c90d3c036b90465f9a167f(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::ObjectLifetimeEntryForAOT()", 137);
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
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 3u).GetFunction();
  auto CastedFuncPtr = (void(*)(void*,void*))RawFuncPtr;
  void* Object = (void*)((&v_Original));
SCRIPT_DEBUG_CALLSTACK_LINE(137);
CastedFuncPtr(Object,(void**)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 7u)));
if (Execution.bExceptionThrown) [[unlikely]]
{
return {};
}
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
SCRIPT_DEBUG_CALLSTACK_LINE(138);
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
SCRIPT_DEBUG_CALLSTACK_LINE(139);
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
  asMETHOD_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 4u).GetMethod();
  auto CastedFuncPtr = (int32(asCUnknownClass::*)())RawFuncPtr;
  void* Object = (void*)((&v_Decorated));
SCRIPT_DEBUG_CALLSTACK_LINE(139);
int32 FunctionReturnValue = (((asCUnknownClass*)Object)->*CastedFuncPtr)();
l_dwordRegister = (asDWORD&)FunctionReturnValue;
if (Execution.bExceptionThrown) [[unlikely]]
{
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_EXCEPTION_1;
}
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
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 5u).GetFunction();
  auto CastedFuncPtr = (void(*)(void*))RawFuncPtr;
  void* Object = (void*)((&v_Decorated));
SCRIPT_DEBUG_CALLSTACK_LINE(139);
CastedFuncPtr(Object);
if (Execution.bExceptionThrown) [[unlikely]]
{
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_EXCEPTION_1;
}
}
// JitEntry *
// PSF v4
// CALLSYS *
// FString::~FString()
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 5u).GetFunction();
  auto CastedFuncPtr = (void(*)(void*))RawFuncPtr;
  void* Object = (void*)((&v_Original));
SCRIPT_DEBUG_CALLSTACK_LINE(139);
CastedFuncPtr(Object);
if (Execution.bExceptionThrown) [[unlikely]]
{
goto LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_EXCEPTION_0;
}
}
// JitEntry *
// CpyVtoR4 v14
l_dwordRegister = v_TEMP_dword_14;
// RET 2
  return (int32)l_dwordRegister;
SCRIPT_ASSUME(false);

LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_EXCEPTION_0:
// Exception cleanup for variables 4
{
  asFUNCTION_t RawDestructorPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 5u).GetFunction();
  auto CastedDestructorPtr = (void(*)(void*))RawDestructorPtr;
  CastedDestructorPtr((void*)(&v_Original));
}
return {};
LABEL_UStaticJITAotFunctionCarrier__ObjectLifetimeEntryForAOT_EXCEPTION_1:
// Exception cleanup for variables 4, 12
{
  asFUNCTION_t RawDestructorPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 5u).GetFunction();
  auto CastedDestructorPtr = (void(*)(void*))RawDestructorPtr;
  CastedDestructorPtr((void*)(&v_Original));
}
{
  asFUNCTION_t RawDestructorPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 5u).GetFunction();
  auto CastedDestructorPtr = (void(*)(void*))RawDestructorPtr;
  CastedDestructorPtr((void*)(&v_Decorated));
}
return {};
}
// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:135:2
// JIT Entry   : VM
void ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_0b9e1f34b7f61af8d6807ddc1048c145b5001e3240c90d3c036b90465f9a167f_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_40cb4932749f765ebd772672edd8b799321a5128acb7b63a1a307ee878c93135_0b9e1f34b7f61af8d6807ddc1048c145b5001e3240c90d3c036b90465f9a167f(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:135:2
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:95:2
 * StableFunctionKey  : 486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52
 * ExecutionHash      : f1d8185b41d4aff3510fd1255a5b7478b0a0845e9a45b7e1c21dd42d0f341f8f
 * DebugHash          : 6144026a285e45049aece2640becfb3806a62e4611772ea21f47be10551ad2f5
 * EntryAbiHash       : f7c84efcc7befb568753c74a139052d8deac838b1132e7123423cb9bd27b6830
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_f1d8185b41d4aff3510fd1255a5b7478b0a0845e9a45b7e1c21dd42d0f341f8f = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_f1d8185b41d4aff3510fd1255a5b7478b0a0845e9a45b7e1c21dd42d0f341f8f
#endif

// AS Function : int64 DoubleToInt64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:95:2
// JIT Entry   : Raw
int64 ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_f1d8185b41d4aff3510fd1255a5b7478b0a0845e9a45b7e1c21dd42d0f341f8f(FScriptExecution& Execution, UObject* l_This, double p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int64 UStaticJITAotFunctionCarrier::DoubleToInt64ForAOT(float)", 97);
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:95:2
// JIT Entry   : VM
void ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_f1d8185b41d4aff3510fd1255a5b7478b0a0845e9a45b7e1c21dd42d0f341f8f_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int64*)l_outValue = ASJIT_486927bfa5514bd2b6d8e65e12c708bc75e9dbfc7fe369b7ad69cab09c846b52_f1d8185b41d4aff3510fd1255a5b7478b0a0845e9a45b7e1c21dd42d0f341f8f(Execution,
		*(UObject**)l_fp,
		*(double*)(l_fp + 2));
}
// AS Function : int64 DoubleToInt64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:95:2
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
 * EntryAbiHash       : 9c054d8b7509b3db28b755f4a912c3b9368acf7b6b5a56b3141baa6e6afbded7
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
 * Declaration       : int BumpReferenceForAOT(int&inout)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:122:2
 * StableFunctionKey  : 5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65
 * ExecutionHash      : 0e62174e7a6ac532611bb5c523304c0baefade0a901b22ab785d62af07152acc
 * DebugHash          : 8758b7a946b282e5621a0afd85a5fbc3ceb5cbd810b81df33404dd3a63a302e6
 * EntryAbiHash       : 23a5665d2df5aa33bee9688695cf69ffc76a8b14539a9c65bbffe8c7a386a9ef
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_0e62174e7a6ac532611bb5c523304c0baefade0a901b22ab785d62af07152acc = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_0e62174e7a6ac532611bb5c523304c0baefade0a901b22ab785d62af07152acc
#endif

// AS Function : int BumpReferenceForAOT(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:122:2
// JIT Entry   : Raw
int32 ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_0e62174e7a6ac532611bb5c523304c0baefade0a901b22ab785d62af07152acc(FScriptExecution& Execution, UObject* l_This, int32* p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::BumpReferenceForAOT(int&)", 124);
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:122:2
// JIT Entry   : VM
void ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_0e62174e7a6ac532611bb5c523304c0baefade0a901b22ab785d62af07152acc_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_5192396f1de5ca755a03a0309eb930bb0941aa4be9ff9c67fe7f4c3851acad65_0e62174e7a6ac532611bb5c523304c0baefade0a901b22ab785d62af07152acc(Execution,
		*(UObject**)l_fp,
		*(int32**)(l_fp + 2));
}
// AS Function : int BumpReferenceForAOT(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:122:2
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:227:0
 * StableFunctionKey  : 521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7
 * ExecutionHash      : 08e5d45dc20b81f5eed1bfd2dffc742af8fbde6e6be8fa295cc4db70f9dabd4e
 * DebugHash          : e99c1b7c1598b7ed5632a3ec913c738a97522a396a8ed5610511320c8b6818b1
 * EntryAbiHash       : de44051d2af792a134ff30c580328c695d9c142e2535b3b3e4536ddc17b6db77
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_08e5d45dc20b81f5eed1bfd2dffc742af8fbde6e6be8fa295cc4db70f9dabd4e = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_08e5d45dc20b81f5eed1bfd2dffc742af8fbde6e6be8fa295cc4db70f9dabd4e
#endif

// AS Function : UStaticJITAotVirtualChild()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:227:0
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
SCRIPT_DEBUG_CALLSTACK_LINE(0);
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
  CallContext->Execute();
  if (Execution.bExceptionThrown || CallContext->m_status != asEXECUTION_FINISHED)
  {
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:227:0
// JIT Entry   : VM
void ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_08e5d45dc20b81f5eed1bfd2dffc742af8fbde6e6be8fa295cc4db70f9dabd4e_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_521eab370f221a5ed98d7359d2be6175ef3c5a9ee0343d41e18d6d4b5b5f4cf7_08e5d45dc20b81f5eed1bfd2dffc742af8fbde6e6be8fa295cc4db70f9dabd4e(Execution,
		*(UObject**)l_fp);
}
// AS Function : UStaticJITAotVirtualChild()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:227:0
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:8:1
 * StableFunctionKey  : 5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa
 * ExecutionHash      : 59c8c94c7be658662a71a29a388ac68fe8dfd9c1b43aa31eb7f3877d1758cbf9
 * DebugHash          : f1a9ca9f519e0006c2b1f4a330a5c00b157bdee9df3b64f611398bffc1a6fc84
 * EntryAbiHash       : 6f792c4fe2aa60b4f207a2ef231a3b0a50254004a3a9776bafd7b167237c85ae
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_59c8c94c7be658662a71a29a388ac68fe8dfd9c1b43aa31eb7f3877d1758cbf9 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_59c8c94c7be658662a71a29a388ac68fe8dfd9c1b43aa31eb7f3877d1758cbf9
#endif

// AS Function : int Entry()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:8:1
// JIT Entry   : Raw
int32 ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_59c8c94c7be658662a71a29a388ac68fe8dfd9c1b43aa31eb7f3877d1758cbf9(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int Entry()", 10);
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
SCRIPT_DEBUG_CALLSTACK_LINE(10);
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
  CallContext->Execute();
  if (Execution.bExceptionThrown || CallContext->m_status != asEXECUTION_FINISHED)
  {
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:8:1
// JIT Entry   : VM
void ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_59c8c94c7be658662a71a29a388ac68fe8dfd9c1b43aa31eb7f3877d1758cbf9_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_5559c341f686950af4ae1125538df84b3a24dfe0926a00ead46cf1610b0911fa_59c8c94c7be658662a71a29a388ac68fe8dfd9c1b43aa31eb7f3877d1758cbf9(Execution);
}
// AS Function : int Entry()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:8:1
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:111:2
 * StableFunctionKey  : 58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9
 * ExecutionHash      : 68246c952cd8e1da3ce4bf073623a504479ab8131a58e607cd2e8186928d1f00
 * DebugHash          : 9c8fe440a8a408b99fd4965baba4419e4cbb98fff74907a1cf6e22de700fcf3a
 * EntryAbiHash       : 105cf35ee25eb110af27107d43aba405d91a3d33de666332164d5436f152e063
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_68246c952cd8e1da3ce4bf073623a504479ab8131a58e607cd2e8186928d1f00 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_68246c952cd8e1da3ce4bf073623a504479ab8131a58e607cd2e8186928d1f00
#endif

// AS Function : void FailForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:111:2
// JIT Entry   : Raw
void ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_68246c952cd8e1da3ce4bf073623a504479ab8131a58e607cd2e8186928d1f00(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("void UStaticJITAotFunctionCarrier::FailForAOT()", 113);
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
SCRIPT_DEBUG_CALLSTACK_LINE(113);
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:111:2
// JIT Entry   : VM
void ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_68246c952cd8e1da3ce4bf073623a504479ab8131a58e607cd2e8186928d1f00_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_58a0dd2f4cd4cc11df5f18d605cdee91ac5735bab888c6a3fd318ff5e4f419c9_68246c952cd8e1da3ce4bf073623a504479ab8131a58e607cd2e8186928d1f00(Execution,
		*(UObject**)l_fp);
}
// AS Function : void FailForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:111:2
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:211:2
 * StableFunctionKey  : 598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c
 * ExecutionHash      : 615b01359e479a32f07c45857015bbd879132d9bc0ef0f750de64842abff27ba
 * DebugHash          : 97bedbd3591c0530d632465df66da118085e8dde4202e76a04b573478937da0d
 * EntryAbiHash       : d58dd33638f8ddba31b892eb3c70a7cce7d0c3d142f2bdd5e90ac408e17f567f
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_615b01359e479a32f07c45857015bbd879132d9bc0ef0f750de64842abff27ba = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_615b01359e479a32f07c45857015bbd879132d9bc0ef0f750de64842abff27ba
#endif

// AS Function : int GetVirtualValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:211:2
// JIT Entry   : Raw
int32 ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_615b01359e479a32f07c45857015bbd879132d9bc0ef0f750de64842abff27ba(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotVirtualParent::GetVirtualValue()", 211);
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
  asCScriptFunction* ScriptFunc = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 4u);
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 4u).GetFunction();
  auto CastedFuncPtr = (void(*)(void*))RawFuncPtr;
  FScopeInformSystemFunction Inform(Execution, ScriptFunc);
SCRIPT_DEBUG_CALLSTACK_LINE(211);
CastedFuncPtr((void**)((&v___ReturnValue)));
if (Execution.bExceptionThrown) [[unlikely]]
{
return {};
}
}
// JitEntry *
// SUSPEND
// JitEntry *
// PGA *
// PshC4 0
// PSF v5
// CALLSYS *
// FName __STATIC_NAME(int Id, const FString&inout CanonicalName)
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 5u).GetFunction();
  auto CastedFuncPtr = (FName(*)(int32,void*))RawFuncPtr;
SCRIPT_DEBUG_CALLSTACK_LINE(211);
new((void*)((&v_TEMP_5))) FName(CastedFuncPtr(value_as<int32>(((asDWORD)0x0u)),(void**)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 7u))));
if (Execution.bExceptionThrown) [[unlikely]]
{
return {};
}
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
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 2u).GetFunction();
  auto CastedFuncPtr = (void(*)(UObject*,void*))RawFuncPtr;
SCRIPT_DEBUG_CALLSTACK_LINE(211);
CastedFuncPtr(value_as<UObject*>(((asQWORD&)l_stack[4])),(void**)((&v_TEMP_5)));
if (Execution.bExceptionThrown) [[unlikely]]
{
return {};
}
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:211:2
// JIT Entry   : VM
void ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_615b01359e479a32f07c45857015bbd879132d9bc0ef0f750de64842abff27ba_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_615b01359e479a32f07c45857015bbd879132d9bc0ef0f750de64842abff27ba(Execution,
		*(UObject**)l_fp);
}
// AS Function : int GetVirtualValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:211:2
// JIT Entry   : Parms
void ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_615b01359e479a32f07c45857015bbd879132d9bc0ef0f750de64842abff27ba_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_598df8bb10d25347acf7f679783e5c13250cbaa02b1ebe130b2b9e419fe2e03c_615b01359e479a32f07c45857015bbd879132d9bc0ef0f750de64842abff27ba(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UStaticJITAotFunctionCarrier ReturnSelfObject()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:190:2
 * StableFunctionKey  : 5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f
 * ExecutionHash      : 3018370c1d5e1cc9de3fd683fadb75e7761f78e06dd8ce7987c9330a1bce062b
 * DebugHash          : 5a7aed7d3a3b6dc4fd8532150fd59d8c67193e5d2d42cdc0983036ff9d364b98
 * EntryAbiHash       : 4c07500ea3eb9c950f7093e694d9e17047a36a253901002753e0996b8efe2c8b
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_3018370c1d5e1cc9de3fd683fadb75e7761f78e06dd8ce7987c9330a1bce062b = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_3018370c1d5e1cc9de3fd683fadb75e7761f78e06dd8ce7987c9330a1bce062b
#endif

// AS Function : UStaticJITAotFunctionCarrier ReturnSelfObject()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:190:2
// JIT Entry   : Raw
UObject* ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_3018370c1d5e1cc9de3fd683fadb75e7761f78e06dd8ce7987c9330a1bce062b(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("UStaticJITAotFunctionCarrier UStaticJITAotFunctionCarrier::ReturnSelfObject()", 192);
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:190:2
// JIT Entry   : VM
void ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_3018370c1d5e1cc9de3fd683fadb75e7761f78e06dd8ce7987c9330a1bce062b_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(UObject**)l_outValue = ASJIT_5b773e32f9e00349bf6af077f499168d794e04d84f5b415ee4618690b9f8520f_3018370c1d5e1cc9de3fd683fadb75e7761f78e06dd8ce7987c9330a1bce062b(Execution,
		*(UObject**)l_fp);
}
// AS Function : UStaticJITAotFunctionCarrier ReturnSelfObject()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:190:2
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:53:1
 * StableFunctionKey  : 5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d
 * ExecutionHash      : 558ba86db59491db742bc487a8c7901ae4d87cef1db86f96ea7e5c0761de4eef
 * DebugHash          : 78999fb135e3d81268c3a1b02017bddc3aecab81fdd1d718d75dc7b677092532
 * EntryAbiHash       : ad7dac870b2eb6d42697eb4d504c8b5072ece4829ebea8db79cafb9cfe196aee
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_558ba86db59491db742bc487a8c7901ae4d87cef1db86f96ea7e5c0761de4eef = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_558ba86db59491db742bc487a8c7901ae4d87cef1db86f96ea7e5c0761de4eef
#endif

// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:53:1
// JIT Entry   : Raw
int32 ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_558ba86db59491db742bc487a8c7901ae4d87cef1db86f96ea7e5c0761de4eef(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int ObjectLifetimeEntryForAOT()", 55);
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
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 2u).GetFunction();
  auto CastedFuncPtr = (void(*)(void*,void*))RawFuncPtr;
  void* Object = (void*)((&v_Original));
SCRIPT_DEBUG_CALLSTACK_LINE(55);
CastedFuncPtr(Object,(void**)(FAngelscriptJITGeneratedReferenceAccess::GetGlobalStorage(Execution, 6u)));
if (Execution.bExceptionThrown) [[unlikely]]
{
return {};
}
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
SCRIPT_DEBUG_CALLSTACK_LINE(56);
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
SCRIPT_DEBUG_CALLSTACK_LINE(57);
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
  asMETHOD_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 3u).GetMethod();
  auto CastedFuncPtr = (int32(asCUnknownClass::*)())RawFuncPtr;
  void* Object = (void*)((&v_Decorated));
SCRIPT_DEBUG_CALLSTACK_LINE(57);
int32 FunctionReturnValue = (((asCUnknownClass*)Object)->*CastedFuncPtr)();
l_dwordRegister = (asDWORD&)FunctionReturnValue;
if (Execution.bExceptionThrown) [[unlikely]]
{
goto LABEL_ObjectLifetimeEntryForAOT_EXCEPTION_1;
}
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
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 4u).GetFunction();
  auto CastedFuncPtr = (void(*)(void*))RawFuncPtr;
  void* Object = (void*)((&v_Decorated));
SCRIPT_DEBUG_CALLSTACK_LINE(57);
CastedFuncPtr(Object);
if (Execution.bExceptionThrown) [[unlikely]]
{
goto LABEL_ObjectLifetimeEntryForAOT_EXCEPTION_1;
}
}
// JitEntry *
// PSF v4
// CALLSYS *
// FString::~FString()
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 4u).GetFunction();
  auto CastedFuncPtr = (void(*)(void*))RawFuncPtr;
  void* Object = (void*)((&v_Original));
SCRIPT_DEBUG_CALLSTACK_LINE(57);
CastedFuncPtr(Object);
if (Execution.bExceptionThrown) [[unlikely]]
{
goto LABEL_ObjectLifetimeEntryForAOT_EXCEPTION_0;
}
}
// JitEntry *
// CpyVtoR4 v14
l_dwordRegister = v_TEMP_dword_14;
// RET 0
  return (int32)l_dwordRegister;
SCRIPT_ASSUME(false);

LABEL_ObjectLifetimeEntryForAOT_EXCEPTION_0:
// Exception cleanup for variables 4
{
  asFUNCTION_t RawDestructorPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 4u).GetFunction();
  auto CastedDestructorPtr = (void(*)(void*))RawDestructorPtr;
  CastedDestructorPtr((void*)(&v_Original));
}
return {};
LABEL_ObjectLifetimeEntryForAOT_EXCEPTION_1:
// Exception cleanup for variables 4, 12
{
  asFUNCTION_t RawDestructorPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 4u).GetFunction();
  auto CastedDestructorPtr = (void(*)(void*))RawDestructorPtr;
  CastedDestructorPtr((void*)(&v_Original));
}
{
  asFUNCTION_t RawDestructorPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 4u).GetFunction();
  auto CastedDestructorPtr = (void(*)(void*))RawDestructorPtr;
  CastedDestructorPtr((void*)(&v_Decorated));
}
return {};
}
// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:53:1
// JIT Entry   : VM
void ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_558ba86db59491db742bc487a8c7901ae4d87cef1db86f96ea7e5c0761de4eef_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_5c613d356269432b6e605bb2642deba24520165e2b3d0d0e49a95a3a80f2b22d_558ba86db59491db742bc487a8c7901ae4d87cef1db86f96ea7e5c0761de4eef(Execution);
}
// AS Function : int ObjectLifetimeEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:53:1
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:182:2
 * StableFunctionKey  : 635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1
 * ExecutionHash      : b9b9cf20c0c6ef3cd5642c43083fc7bf8266b6b8b2df8437b2603377d837b03e
 * DebugHash          : 3160298098a8abf96612cb20dd8c2d8af74345f272444932d158ced1da8f2e5d
 * EntryAbiHash       : 7bbec1f84c5078a225350baa7ae2d7cfa7aa5c6502c6e51b34b94618b10cb5e8
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_b9b9cf20c0c6ef3cd5642c43083fc7bf8266b6b8b2df8437b2603377d837b03e = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_b9b9cf20c0c6ef3cd5642c43083fc7bf8266b6b8b2df8437b2603377d837b03e
#endif

// AS Function : int BumpReference(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:182:2
// JIT Entry   : Raw
int32 ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_b9b9cf20c0c6ef3cd5642c43083fc7bf8266b6b8b2df8437b2603377d837b03e(FScriptExecution& Execution, UObject* l_This, int32* p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::BumpReference(int&)", 184);
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:182:2
// JIT Entry   : VM
void ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_b9b9cf20c0c6ef3cd5642c43083fc7bf8266b6b8b2df8437b2603377d837b03e_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_635bd31fd9c1491607e4044ecd2e683fed66d4f0765045d47b7ca3b47ec88cb1_b9b9cf20c0c6ef3cd5642c43083fc7bf8266b6b8b2df8437b2603377d837b03e(Execution,
		*(UObject**)l_fp,
		*(int32**)(l_fp + 2));
}
// AS Function : int BumpReference(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:182:2
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:80:0
 * StableFunctionKey  : 6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060
 * ExecutionHash      : fb05ff1cd40b005becc20bdca090866a9a681873a5ef622458c77a1146814bec
 * DebugHash          : e99c1b7c1598b7ed5632a3ec913c738a97522a396a8ed5610511320c8b6818b1
 * EntryAbiHash       : c015745a127a499898453c626f0da9b3c8a945e2d009adeea0f5065f9117604c
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060_fb05ff1cd40b005becc20bdca090866a9a681873a5ef622458c77a1146814bec = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060_fb05ff1cd40b005becc20bdca090866a9a681873a5ef622458c77a1146814bec
#endif

// AS Function : UStaticJITAotFunctionCarrier UStaticJITAotFunctionCarrier()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:80:0
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
SCRIPT_DEBUG_CALLSTACK_LINE(0);
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
  CallContext->Execute();
  if (Execution.bExceptionThrown || CallContext->m_status != asEXECUTION_FINISHED)
  {
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:80:0
// JIT Entry   : VM
void ASJIT_6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060_fb05ff1cd40b005becc20bdca090866a9a681873a5ef622458c77a1146814bec_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(UObject**)l_outValue = ASJIT_6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060_fb05ff1cd40b005becc20bdca090866a9a681873a5ef622458c77a1146814bec(Execution);
}
// AS Function : UStaticJITAotFunctionCarrier UStaticJITAotFunctionCarrier()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:80:0
// JIT Entry   : Parms
void ASJIT_6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060_fb05ff1cd40b005becc20bdca090866a9a681873a5ef622458c77a1146814bec_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(UObject**)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_6a1d712788d1f9e6d19f814446686b67c2646e32117f3f6b9629aa80c0c38060_fb05ff1cd40b005becc20bdca090866a9a681873a5ef622458c77a1146814bec(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int StaticWorldContextCheck(UObject, const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:235:1
 * StableFunctionKey  : 89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652
 * ExecutionHash      : 8d91fd16350ec4e44575077d45598ce8ae04e9b9a5c5158835a0dae927ccd053
 * DebugHash          : 1f8819dd2a0a840d31918aec02ecde6b3fb298210e1b721f1abf0a7a3f0277e5
 * EntryAbiHash       : a339ee8b7ca83360352f8578e18fc7848ddcb9edb253a5c295f1110c9c23c618
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_8d91fd16350ec4e44575077d45598ce8ae04e9b9a5c5158835a0dae927ccd053 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_8d91fd16350ec4e44575077d45598ce8ae04e9b9a5c5158835a0dae927ccd053
#endif

// AS Function : int StaticWorldContextCheck(UObject, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:235:1
// JIT Entry   : Raw
int32 ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_8d91fd16350ec4e44575077d45598ce8ae04e9b9a5c5158835a0dae927ccd053(FScriptExecution& Execution, UObject* p_WorldContextObject, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int StaticWorldContextCheck(UObject, int)", 237);
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
SCRIPT_DEBUG_CALLSTACK_LINE(237);
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:235:1
// JIT Entry   : VM
void ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_8d91fd16350ec4e44575077d45598ce8ae04e9b9a5c5158835a0dae927ccd053_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_89af2a983148ab5615f6efba1837fdc9b306cb37cdd12898193e7d44b6439652_8d91fd16350ec4e44575077d45598ce8ae04e9b9a5c5158835a0dae927ccd053(Execution,
		*(UObject**)(l_fp + 0),
		*(asDWORD*)(l_fp + 2));
}
// AS Function : int StaticWorldContextCheck(UObject, const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:235:1
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:163:2
 * StableFunctionKey  : 8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf
 * ExecutionHash      : 8ebf159adfc3c0b241459d0fa1d50a697cd4b805685a94068e4eb11124e56f03
 * DebugHash          : 64c09ddeabdccd3abdba34018db4b626a8b8f83dc27ae3540f71d2e6159e5778
 * EntryAbiHash       : a552bbee02f0912539b7eadd7d97b3a59881289d5cff573d94b06a63de16344e
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_8ebf159adfc3c0b241459d0fa1d50a697cd4b805685a94068e4eb11124e56f03 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_8ebf159adfc3c0b241459d0fa1d50a697cd4b805685a94068e4eb11124e56f03
#endif

// AS Function : void StorePrimitiveArg(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:163:2
// JIT Entry   : Raw
void ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_8ebf159adfc3c0b241459d0fa1d50a697cd4b805685a94068e4eb11124e56f03(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("void UStaticJITAotFunctionCarrier::StorePrimitiveArg(int)", 165);
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:163:2
// JIT Entry   : VM
void ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_8ebf159adfc3c0b241459d0fa1d50a697cd4b805685a94068e4eb11124e56f03_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_8a3604f71e882b6abb85571af48a797c711a59f3ce5d97af8d1f32e4360effaf_8ebf159adfc3c0b241459d0fa1d50a697cd4b805685a94068e4eb11124e56f03(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : void StorePrimitiveArg(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:163:2
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:116:2
 * StableFunctionKey  : 91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277
 * ExecutionHash      : 37f8b3d211d13533245d7e8e2c19aa9094d05556c2240e1289b057acd844c5d7
 * DebugHash          : ffcc6196c22883662189d42a26a6e2a862aa9b652396663bc99c4f95a1eccab8
 * EntryAbiHash       : baf6fd1e8422cd3e2b6430c8afc94f6e4387239e34ddfd17e19fd34b0e5603b9
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_37f8b3d211d13533245d7e8e2c19aa9094d05556c2240e1289b057acd844c5d7 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_37f8b3d211d13533245d7e8e2c19aa9094d05556c2240e1289b057acd844c5d7
#endif

// AS Function : int ExceptionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:116:2
// JIT Entry   : Raw
int32 ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_37f8b3d211d13533245d7e8e2c19aa9094d05556c2240e1289b057acd844c5d7(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::ExceptionEntryForAOT()", 118);
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
SCRIPT_DEBUG_CALLSTACK_LINE(118);
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
  CallContext->Execute();
  if (Execution.bExceptionThrown || CallContext->m_status != asEXECUTION_FINISHED)
  {
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:116:2
// JIT Entry   : VM
void ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_37f8b3d211d13533245d7e8e2c19aa9094d05556c2240e1289b057acd844c5d7_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_91ead9fa14e7b5c2ffc1e318a63f46c64c38fd28c17cf8648a985806c9311277_37f8b3d211d13533245d7e8e2c19aa9094d05556c2240e1289b057acd844c5d7(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ExceptionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:116:2
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:142:2
 * StableFunctionKey  : 947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7
 * ExecutionHash      : e2a0dedd7d7ebfe498830d40c0881570ab0cefd2eb6f2de8c3ffcdbf87c37157
 * DebugHash          : 5dc127f003403df7e86dc52029521e727f61775dba544da2108ebf3b86861488
 * EntryAbiHash       : 156f37a674a4191f173f4119b1ed1aa15583248d6f4331c86b6ebd608c713fd0
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_e2a0dedd7d7ebfe498830d40c0881570ab0cefd2eb6f2de8c3ffcdbf87c37157 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_e2a0dedd7d7ebfe498830d40c0881570ab0cefd2eb6f2de8c3ffcdbf87c37157
#endif

// AS Function : int RecurseForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:142:2
// JIT Entry   : Raw
int32 ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_e2a0dedd7d7ebfe498830d40c0881570ab0cefd2eb6f2de8c3ffcdbf87c37157(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::RecurseForAOT(int)", 144);
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
SCRIPT_DEBUG_CALLSTACK_LINE(148);
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
  CallContext->Execute();
  if (Execution.bExceptionThrown || CallContext->m_status != asEXECUTION_FINISHED)
  {
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:142:2
// JIT Entry   : VM
void ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_e2a0dedd7d7ebfe498830d40c0881570ab0cefd2eb6f2de8c3ffcdbf87c37157_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_947430d3dda2641a6a6fa7990b225bd55cdac3fcb9717fac45077c92c4048cf7_e2a0dedd7d7ebfe498830d40c0881570ab0cefd2eb6f2de8c3ffcdbf87c37157(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : int RecurseForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:142:2
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:228:3
 * StableFunctionKey  : 980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1
 * ExecutionHash      : d7eaf1c7675e62f28fd59cc6748fb409101a0ee62f733b718a57f18f9eefb57e
 * DebugHash          : a1cbb1de41cb375a507b5be08c7bef7784818b8f384eb624dfdd0fc7a28129bf
 * EntryAbiHash       : 93970f84a6aa1bac802f6979d57ff2163ff4f46543f8fc9ab6574d7e2f0d4e63
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_d7eaf1c7675e62f28fd59cc6748fb409101a0ee62f733b718a57f18f9eefb57e = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_d7eaf1c7675e62f28fd59cc6748fb409101a0ee62f733b718a57f18f9eefb57e
#endif

// AS Function : int GetVirtualValue_Implementation()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:228:3
// JIT Entry   : Raw
int32 ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_d7eaf1c7675e62f28fd59cc6748fb409101a0ee62f733b718a57f18f9eefb57e(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotVirtualChild::GetVirtualValue_Implementation()", 230);
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:228:3
// JIT Entry   : VM
void ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_d7eaf1c7675e62f28fd59cc6748fb409101a0ee62f733b718a57f18f9eefb57e_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_980bd73503b215e1d3fc564495191651205d2d744a5a70f5e7dc7c0071d02ff1_d7eaf1c7675e62f28fd59cc6748fb409101a0ee62f733b718a57f18f9eefb57e(Execution,
		*(UObject**)l_fp);
}
// AS Function : int GetVirtualValue_Implementation()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:228:3
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:40:1
 * StableFunctionKey  : 9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05
 * ExecutionHash      : 9e0ee7c8a454f7220efc004fa5e64d9f4a3b896d4cc8dc9597c518621b86e70c
 * DebugHash          : 3a7f04c667df892cb11f364dfaef8888ad346e4f6b666ff25fba3f4aa472fe02
 * EntryAbiHash       : 23a5665d2df5aa33bee9688695cf69ffc76a8b14539a9c65bbffe8c7a386a9ef
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_9e0ee7c8a454f7220efc004fa5e64d9f4a3b896d4cc8dc9597c518621b86e70c = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_9e0ee7c8a454f7220efc004fa5e64d9f4a3b896d4cc8dc9597c518621b86e70c
#endif

// AS Function : int BumpReferenceForAOT(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:40:1
// JIT Entry   : Raw
int32 ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_9e0ee7c8a454f7220efc004fa5e64d9f4a3b896d4cc8dc9597c518621b86e70c(FScriptExecution& Execution, int32* p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int BumpReferenceForAOT(int&)", 42);
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:40:1
// JIT Entry   : VM
void ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_9e0ee7c8a454f7220efc004fa5e64d9f4a3b896d4cc8dc9597c518621b86e70c_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_9a847cdbcb6a205cf6e821b4060b98db55641e2eb760aa0ea4db56859ec0ea05_9e0ee7c8a454f7220efc004fa5e64d9f4a3b896d4cc8dc9597c518621b86e70c(Execution,
		*(int32**)(l_fp + 0));
}
// AS Function : int BumpReferenceForAOT(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:40:1
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:74:1
 * StableFunctionKey  : 9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a
 * ExecutionHash      : f643f2af98b5abccabf8ba607388698dce70fef161600767865f502ab4ba7ec1
 * DebugHash          : 28dc38e2497b964988d0426df2ef014de165fb3f27d81b4f5f76afb50bd53f9e
 * EntryAbiHash       : ac60f1864c77f2def1e11a94b3749762971cd3004b15141de7114b872311cffb
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_f643f2af98b5abccabf8ba607388698dce70fef161600767865f502ab4ba7ec1 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_f643f2af98b5abccabf8ba607388698dce70fef161600767865f502ab4ba7ec1
#endif

// AS Function : int ImportEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:74:1
// JIT Entry   : Raw
int32 ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_f643f2af98b5abccabf8ba607388698dce70fef161600767865f502ab4ba7ec1(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int ImportEntryForAOT()", 76);
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
SCRIPT_DEBUG_CALLSTACK_LINE(76);
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
  CallContext->Execute();
  if (Execution.bExceptionThrown || CallContext->m_status != asEXECUTION_FINISHED)
  {
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:74:1
// JIT Entry   : VM
void ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_f643f2af98b5abccabf8ba607388698dce70fef161600767865f502ab4ba7ec1_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_9bf32d35743c453ab252bde2cf30b5a04b2c0ee664b3995f90ed3b071da50e3a_f643f2af98b5abccabf8ba607388698dce70fef161600767865f502ab4ba7ec1(Execution);
}
// AS Function : int ImportEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:74:1
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:202:2
 * StableFunctionKey  : 9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7
 * ExecutionHash      : aa09074ddab30af509e57b20a66e3301fd95e90c0f53585e7954a5d76bec975c
 * DebugHash          : 594be3e15a310f5d46d1e6535bdb708c96c54ca8ec629dc80635a32d49670f0f
 * EntryAbiHash       : fde794e2dd799e81eb6f23c8d202194ce3c63752c57b73c1ac3445f0fef39ffd
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_aa09074ddab30af509e57b20a66e3301fd95e90c0f53585e7954a5d76bec975c = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_aa09074ddab30af509e57b20a66e3301fd95e90c0f53585e7954a5d76bec975c
#endif

// AS Function : int ThreadSafeReturn()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:202:2
// JIT Entry   : Raw
int32 ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_aa09074ddab30af509e57b20a66e3301fd95e90c0f53585e7954a5d76bec975c(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::ThreadSafeReturn()", 204);
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:202:2
// JIT Entry   : VM
void ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_aa09074ddab30af509e57b20a66e3301fd95e90c0f53585e7954a5d76bec975c_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_9cd3c344535a331f8ccdca49c1927750f2dd23fa6b1e3d447de2551994623ed7_aa09074ddab30af509e57b20a66e3301fd95e90c0f53585e7954a5d76bec975c(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ThreadSafeReturn()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:202:2
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:175:2
 * StableFunctionKey  : 9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e
 * ExecutionHash      : 4db4ef9a2b689826b018585d8640d54df2ab20c17fcd503eebf7f94a30d515f1
 * DebugHash          : 18e39c15702a5aa3a0ec69189925683cf52885665046d9da30787d98ee87f716
 * EntryAbiHash       : 264e9eee824472a6265e57ce03ba9bd446edbd73c77692de2bee50c963181c06
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_4db4ef9a2b689826b018585d8640d54df2ab20c17fcd503eebf7f94a30d515f1 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_4db4ef9a2b689826b018585d8640d54df2ab20c17fcd503eebf7f94a30d515f1
#endif

// AS Function : void IncrementReference(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:175:2
// JIT Entry   : Raw
void ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_4db4ef9a2b689826b018585d8640d54df2ab20c17fcd503eebf7f94a30d515f1(FScriptExecution& Execution, UObject* l_This, int32* p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("void UStaticJITAotFunctionCarrier::IncrementReference(int&)", 177);
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:175:2
// JIT Entry   : VM
void ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_4db4ef9a2b689826b018585d8640d54df2ab20c17fcd503eebf7f94a30d515f1_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_9e3e038bd73ec0c0b4c52e7fbb25ef63dc33a457a64d9420e557afacc04fe03e_4db4ef9a2b689826b018585d8640d54df2ab20c17fcd503eebf7f94a30d515f1(Execution,
		*(UObject**)l_fp,
		*(int32**)(l_fp + 2));
}
// AS Function : void IncrementReference(int&inout)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:175:2
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
 * Declaration       : int64 DoubleToInt64ForAOT(const float)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:13:1
 * StableFunctionKey  : a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6
 * ExecutionHash      : 1139879851ec9364c88ba97c5f45668c3573553687be5d11c833c21e62a3094c
 * DebugHash          : ce00f2f5579de6b582feb5e28c9632482fbb82a8239e403653f2f4052f2eb7b1
 * EntryAbiHash       : f7c84efcc7befb568753c74a139052d8deac838b1132e7123423cb9bd27b6830
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_1139879851ec9364c88ba97c5f45668c3573553687be5d11c833c21e62a3094c = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_1139879851ec9364c88ba97c5f45668c3573553687be5d11c833c21e62a3094c
#endif

// AS Function : int64 DoubleToInt64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:13:1
// JIT Entry   : Raw
int64 ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_1139879851ec9364c88ba97c5f45668c3573553687be5d11c833c21e62a3094c(FScriptExecution& Execution, double p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int64 DoubleToInt64ForAOT(float)", 15);
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:13:1
// JIT Entry   : VM
void ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_1139879851ec9364c88ba97c5f45668c3573553687be5d11c833c21e62a3094c_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int64*)l_outValue = ASJIT_a8cb30d75fe6eeb5c3a33f74ee94f2cc4dd32734ccde7a26dac377dc469a3be6_1139879851ec9364c88ba97c5f45668c3573553687be5d11c833c21e62a3094c(Execution,
		*(double*)(l_fp + 0));
}
// AS Function : int64 DoubleToInt64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:13:1
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:100:2
 * StableFunctionKey  : aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb
 * ExecutionHash      : e8872a3f3978b619f364a8c7013e3ff22283632f0fa56397688987ca97239a05
 * DebugHash          : 795a1c109049a637a0ce35f9babecf8644727545c52dd0949e913f24a4c46cc8
 * EntryAbiHash       : 6f4f7e6615f922448dfa37b3c2b25936c0efef3f3655e58bd8cd07adf451eb00
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_e8872a3f3978b619f364a8c7013e3ff22283632f0fa56397688987ca97239a05 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_e8872a3f3978b619f364a8c7013e3ff22283632f0fa56397688987ca97239a05
#endif

// AS Function : uint64 DoubleToUint64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:100:2
// JIT Entry   : Raw
uint64 ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_e8872a3f3978b619f364a8c7013e3ff22283632f0fa56397688987ca97239a05(FScriptExecution& Execution, UObject* l_This, double p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("uint64 UStaticJITAotFunctionCarrier::DoubleToUint64ForAOT(float)", 102);
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:100:2
// JIT Entry   : VM
void ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_e8872a3f3978b619f364a8c7013e3ff22283632f0fa56397688987ca97239a05_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(uint64*)l_outValue = ASJIT_aee0a322f61d0b83ad7b791ea0f64e54eb9b4a9ca74aabad5c7f78cd25d329fb_e8872a3f3978b619f364a8c7013e3ff22283632f0fa56397688987ca97239a05(Execution,
		*(UObject**)l_fp,
		*(double*)(l_fp + 2));
}
// AS Function : uint64 DoubleToUint64ForAOT(const float)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:100:2
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:169:2
 * StableFunctionKey  : b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367
 * ExecutionHash      : 55a527ed5db1a4ef9e97408837341ff2dec3c63487f911516686b4396a516004
 * DebugHash          : ebe7ae37f8914b2772b732723ee7d7efd1427de3b3ece31a340b6c8ffe0dabe6
 * EntryAbiHash       : 39088ac2abc617a5ddb6a46c03eca42dc48d094e759b6c318b2c049aee9d18cb
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_55a527ed5db1a4ef9e97408837341ff2dec3c63487f911516686b4396a516004 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_55a527ed5db1a4ef9e97408837341ff2dec3c63487f911516686b4396a516004
#endif

// AS Function : int ReturnPrimitive()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:169:2
// JIT Entry   : Raw
int32 ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_55a527ed5db1a4ef9e97408837341ff2dec3c63487f911516686b4396a516004(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::ReturnPrimitive()", 171);
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:169:2
// JIT Entry   : VM
void ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_55a527ed5db1a4ef9e97408837341ff2dec3c63487f911516686b4396a516004_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_b28505eef133171edbe5c882ea10d79c713ca6cb87f351cbd70cae422f8fb367_55a527ed5db1a4ef9e97408837341ff2dec3c63487f911516686b4396a516004(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ReturnPrimitive()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:169:2
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:128:2
 * StableFunctionKey  : bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726
 * ExecutionHash      : da38e64d8c8f99763e5ab028d7619561e42eafd63cd1855f7b3fd97328ac6cba
 * DebugHash          : d745fd2ffc47ebd384656bb8d0bf2b98b49965a951e94c1d20b7896e28b3f66c
 * EntryAbiHash       : 821f49e8200c05323110360fc816375596ff5caaa0e0b3a7583f8c90b8e600f7
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_da38e64d8c8f99763e5ab028d7619561e42eafd63cd1855f7b3fd97328ac6cba = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_da38e64d8c8f99763e5ab028d7619561e42eafd63cd1855f7b3fd97328ac6cba
#endif

// AS Function : int ReferenceEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:128:2
// JIT Entry   : Raw
int32 ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_da38e64d8c8f99763e5ab028d7619561e42eafd63cd1855f7b3fd97328ac6cba(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::ReferenceEntryForAOT()", 130);
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
SCRIPT_DEBUG_CALLSTACK_LINE(131);
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
  CallContext->Execute();
  if (Execution.bExceptionThrown || CallContext->m_status != asEXECUTION_FINISHED)
  {
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:128:2
// JIT Entry   : VM
void ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_da38e64d8c8f99763e5ab028d7619561e42eafd63cd1855f7b3fd97328ac6cba_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_bb41c2bfbc095a9d60f2625418818fc13bf04ec1a0bb79f469cc74d941bf7726_da38e64d8c8f99763e5ab028d7619561e42eafd63cd1855f7b3fd97328ac6cba(Execution,
		*(UObject**)l_fp);
}
// AS Function : int ReferenceEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:128:2
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:80:0
 * StableFunctionKey  : c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5
 * ExecutionHash      : 51381d117be50d31068f645cd70db749143ad7a4aaef927b0b010ad6fbee923e
 * DebugHash          : e99c1b7c1598b7ed5632a3ec913c738a97522a396a8ed5610511320c8b6818b1
 * EntryAbiHash       : ac9fa97b0776d138c00d924aef0d8dfc047f7c4bdda25bda69b2e33cfc48a163
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_51381d117be50d31068f645cd70db749143ad7a4aaef927b0b010ad6fbee923e = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_51381d117be50d31068f645cd70db749143ad7a4aaef927b0b010ad6fbee923e
#endif

// AS Function : UStaticJITAotFunctionCarrier()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:80:0
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:80:0
// JIT Entry   : VM
void ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_51381d117be50d31068f645cd70db749143ad7a4aaef927b0b010ad6fbee923e_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_c5c802cbf4c8a38a7b3faa86efda41f60157ec97b9bf469cb71c1cb577e4aae5_51381d117be50d31068f645cd70db749143ad7a4aaef927b0b010ad6fbee923e(Execution,
		*(UObject**)l_fp);
}
// AS Function : UStaticJITAotFunctionCarrier()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:80:0
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:211:0
 * StableFunctionKey  : dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844
 * ExecutionHash      : ba6d296ea9a313ef441f8ef0185fb102be4ae81d8554a1bff61480bf2ac8043b
 * DebugHash          : e99c1b7c1598b7ed5632a3ec913c738a97522a396a8ed5610511320c8b6818b1
 * EntryAbiHash       : 1d69eae6e736fa942165f3d0cbe3f72ba5f301ca36d6dfb309911c3e80acd55e
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844_ba6d296ea9a313ef441f8ef0185fb102be4ae81d8554a1bff61480bf2ac8043b = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844_ba6d296ea9a313ef441f8ef0185fb102be4ae81d8554a1bff61480bf2ac8043b
#endif

// AS Function : UStaticJITAotVirtualParent UStaticJITAotVirtualParent()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:211:0
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
SCRIPT_DEBUG_CALLSTACK_LINE(0);
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
  CallContext->Execute();
  if (Execution.bExceptionThrown || CallContext->m_status != asEXECUTION_FINISHED)
  {
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:211:0
// JIT Entry   : VM
void ASJIT_dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844_ba6d296ea9a313ef441f8ef0185fb102be4ae81d8554a1bff61480bf2ac8043b_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(UObject**)l_outValue = ASJIT_dd7185ee5ff69babce6b309d7a2954364368cd6df09defe19d4e81ff56fbe844_ba6d296ea9a313ef441f8ef0185fb102be4ae81d8554a1bff61480bf2ac8043b(Execution);
}
// AS Function : UStaticJITAotVirtualParent UStaticJITAotVirtualParent()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:211:0
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:34:1
 * StableFunctionKey  : e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7
 * ExecutionHash      : 8ae3aa1c4fdc8fcec04ac3db1312bb42a62728e9d6db911ef21f63abd00c8ac2
 * DebugHash          : adc24f03f71011c0dbf6248c75a359f8617db2ba907be36f543984fee59202a1
 * EntryAbiHash       : baf6fd1e8422cd3e2b6430c8afc94f6e4387239e34ddfd17e19fd34b0e5603b9
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_8ae3aa1c4fdc8fcec04ac3db1312bb42a62728e9d6db911ef21f63abd00c8ac2 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_8ae3aa1c4fdc8fcec04ac3db1312bb42a62728e9d6db911ef21f63abd00c8ac2
#endif

// AS Function : int ExceptionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:34:1
// JIT Entry   : Raw
int32 ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_8ae3aa1c4fdc8fcec04ac3db1312bb42a62728e9d6db911ef21f63abd00c8ac2(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int ExceptionEntryForAOT()", 36);
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
SCRIPT_DEBUG_CALLSTACK_LINE(36);
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
  CallContext->Execute();
  if (Execution.bExceptionThrown || CallContext->m_status != asEXECUTION_FINISHED)
  {
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:34:1
// JIT Entry   : VM
void ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_8ae3aa1c4fdc8fcec04ac3db1312bb42a62728e9d6db911ef21f63abd00c8ac2_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_8ae3aa1c4fdc8fcec04ac3db1312bb42a62728e9d6db911ef21f63abd00c8ac2(Execution);
}
// AS Function : int ExceptionEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:34:1
// JIT Entry   : Parms
void ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_8ae3aa1c4fdc8fcec04ac3db1312bb42a62728e9d6db911ef21f63abd00c8ac2_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_e7c817db4faceebe6230e85891473fe7f0309ef2d506e473da9d141c0d7f5ac7_8ae3aa1c4fdc8fcec04ac3db1312bb42a62728e9d6db911ef21f63abd00c8ac2(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int RecurseForAOT(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:60:1
 * StableFunctionKey  : f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556
 * ExecutionHash      : 5aa1b5a0852c6ecb240e1926879919b0136db143d20d6c93f65de9c8ba52c402
 * DebugHash          : 9913145e597402cbf8116a6ba65b3d5889b7eed82b7b5ef9c95bb201f5d64836
 * EntryAbiHash       : 156f37a674a4191f173f4119b1ed1aa15583248d6f4331c86b6ebd608c713fd0
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_5aa1b5a0852c6ecb240e1926879919b0136db143d20d6c93f65de9c8ba52c402 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_5aa1b5a0852c6ecb240e1926879919b0136db143d20d6c93f65de9c8ba52c402
#endif

// AS Function : int RecurseForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:60:1
// JIT Entry   : Raw
int32 ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_5aa1b5a0852c6ecb240e1926879919b0136db143d20d6c93f65de9c8ba52c402(FScriptExecution& Execution, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int RecurseForAOT(int)", 62);
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
SCRIPT_DEBUG_CALLSTACK_LINE(66);
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
  CallContext->Execute();
  if (Execution.bExceptionThrown || CallContext->m_status != asEXECUTION_FINISHED)
  {
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:60:1
// JIT Entry   : VM
void ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_5aa1b5a0852c6ecb240e1926879919b0136db143d20d6c93f65de9c8ba52c402_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_f07760e6464f68dc2af2fc2d0905bfa86578c126fb4a72316819ab24818d9556_5aa1b5a0852c6ecb240e1926879919b0136db143d20d6c93f65de9c8ba52c402(Execution,
		*(asDWORD*)(l_fp + 0));
}
// AS Function : int RecurseForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:60:1
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:23:1
 * StableFunctionKey  : f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df
 * ExecutionHash      : dd5551c67767322a161767993cc5b559e3c62f067170cfc0e60df8016f3650e4
 * DebugHash          : 0e79d5a6769e4f107da8302495f0948d77fd6f3890aae3b4ab3398f044c19381
 * EntryAbiHash       : cfd6270223e115f05bd9303f7ac38a55aae7a365f523814f643a93593cef6158
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_dd5551c67767322a161767993cc5b559e3c62f067170cfc0e60df8016f3650e4 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_dd5551c67767322a161767993cc5b559e3c62f067170cfc0e60df8016f3650e4
#endif

// AS Function : int ObjectLastNativeForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:23:1
// JIT Entry   : Raw
int32 ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_dd5551c67767322a161767993cc5b559e3c62f067170cfc0e60df8016f3650e4(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int ObjectLastNativeForAOT()", 25);
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
SCRIPT_DEBUG_CALLSTACK_LINE(25);
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:23:1
// JIT Entry   : VM
void ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_dd5551c67767322a161767993cc5b559e3c62f067170cfc0e60df8016f3650e4_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_f0e5d16c553aa4d9219681c7ddd1448bcecf226c1022dfed50547ac81b9a04df_dd5551c67767322a161767993cc5b559e3c62f067170cfc0e60df8016f3650e4(Execution);
}
// AS Function : int ObjectLastNativeForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:23:1
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:90:3
 * StableFunctionKey  : f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9
 * ExecutionHash      : 818820abe70c5eec2aeba72ed9ac354275077d2745ecc7386c49e318a16f91bb
 * DebugHash          : d832f6dd7a3aa91ebc59d741340d4c59501411bb87869f5a430af1fc86cb287e
 * EntryAbiHash       : 6f792c4fe2aa60b4f207a2ef231a3b0a50254004a3a9776bafd7b167237c85ae
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_818820abe70c5eec2aeba72ed9ac354275077d2745ecc7386c49e318a16f91bb = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_818820abe70c5eec2aeba72ed9ac354275077d2745ecc7386c49e318a16f91bb
#endif

// AS Function : int Entry()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:90:3
// JIT Entry   : Raw
int32 ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_818820abe70c5eec2aeba72ed9ac354275077d2745ecc7386c49e318a16f91bb(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::Entry()", 92);
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
SCRIPT_DEBUG_CALLSTACK_LINE(92);
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
  CallContext->Execute();
  if (Execution.bExceptionThrown || CallContext->m_status != asEXECUTION_FINISHED)
  {
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:90:3
// JIT Entry   : VM
void ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_818820abe70c5eec2aeba72ed9ac354275077d2745ecc7386c49e318a16f91bb_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_f26d69183a6e80c744b40df88fced3c8dcefb76b4aff9b91a8beb4fd10af47a9_818820abe70c5eec2aeba72ed9ac354275077d2745ecc7386c49e318a16f91bb(Execution,
		*(UObject**)l_fp);
}
// AS Function : int Entry()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:90:3
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
 * Declaration       : UStaticJITAotVirtualParent()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:211:0
 * StableFunctionKey  : f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef
 * ExecutionHash      : bbdf063a311ff21aa3fdd76875261a96989372d8d66d7a8034aff45ccf38428e
 * DebugHash          : e99c1b7c1598b7ed5632a3ec913c738a97522a396a8ed5610511320c8b6818b1
 * EntryAbiHash       : 636c5e547f5a6d78df0183cb6a977500ba2eb0b9eadabad66498aafee4a4a9d8
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_bbdf063a311ff21aa3fdd76875261a96989372d8d66d7a8034aff45ccf38428e = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_bbdf063a311ff21aa3fdd76875261a96989372d8d66d7a8034aff45ccf38428e
#endif

// AS Function : UStaticJITAotVirtualParent()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:211:0
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:211:0
// JIT Entry   : VM
void ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_bbdf063a311ff21aa3fdd76875261a96989372d8d66d7a8034aff45ccf38428e_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_bbdf063a311ff21aa3fdd76875261a96989372d8d66d7a8034aff45ccf38428e(Execution,
		*(UObject**)l_fp);
}
// AS Function : UStaticJITAotVirtualParent()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:211:0
// JIT Entry   : Parms
void ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_bbdf063a311ff21aa3fdd76875261a96989372d8d66d7a8034aff45ccf38428e_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_f4a3e236ba32d7ce2785c71c5606b807f64c64fd981b3330bed70cd39c6146ef_bbdf063a311ff21aa3fdd76875261a96989372d8d66d7a8034aff45ccf38428e(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void ResetStoredValue()
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:157:2
 * StableFunctionKey  : f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7
 * ExecutionHash      : 31ab68e2d91d0c1e9600288f6f39222c55d3afeace5b10932325a2d3bb666edd
 * DebugHash          : 1bafdad49d2239d770b18dc83d8858ec083c0055d48bb172727d46f5320adebf
 * EntryAbiHash       : bb78e4e405ca12441fcd3fdb2951f57bb074f51b2a79b1f2da54ba07009d53ce
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_31ab68e2d91d0c1e9600288f6f39222c55d3afeace5b10932325a2d3bb666edd = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_31ab68e2d91d0c1e9600288f6f39222c55d3afeace5b10932325a2d3bb666edd
#endif

// AS Function : void ResetStoredValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:157:2
// JIT Entry   : Raw
void ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_31ab68e2d91d0c1e9600288f6f39222c55d3afeace5b10932325a2d3bb666edd(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("void UStaticJITAotFunctionCarrier::ResetStoredValue()", 159);
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:157:2
// JIT Entry   : VM
void ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_31ab68e2d91d0c1e9600288f6f39222c55d3afeace5b10932325a2d3bb666edd_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_f5fdb43143a055e61bfd4d0dbd516cd29cc90eab1a26caac51f9fa7d498c58d7_31ab68e2d91d0c1e9600288f6f39222c55d3afeace5b10932325a2d3bb666edd(Execution,
		*(UObject**)l_fp);
}
// AS Function : void ResetStoredValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:157:2
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:218:2
 * StableFunctionKey  : fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce
 * ExecutionHash      : bf6aff981e022f413c315d1e1b0769d78612f114820c8c21b4f50467c94ab465
 * DebugHash          : 2e168a60e9e546020537ecb0d303d6d415b43f61df686a39c5559df1dae7a142
 * EntryAbiHash       : d590750907d0aa468de5a448d6b1ac3b81119aca733e4c67e3ddd7290923ca36
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_bf6aff981e022f413c315d1e1b0769d78612f114820c8c21b4f50467c94ab465 = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_bf6aff981e022f413c315d1e1b0769d78612f114820c8c21b4f50467c94ab465
#endif

// AS Function : int CallVirtualValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:218:2
// JIT Entry   : Raw
int32 ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_bf6aff981e022f413c315d1e1b0769d78612f114820c8c21b4f50467c94ab465(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotVirtualParent::CallVirtualValue()", 220);
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
SCRIPT_DEBUG_CALLSTACK_LINE(220);
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
  CallContext->Execute();
  if (Execution.bExceptionThrown || CallContext->m_status != asEXECUTION_FINISHED)
  {
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:218:2
// JIT Entry   : VM
void ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_bf6aff981e022f413c315d1e1b0769d78612f114820c8c21b4f50467c94ab465_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_fc5c737f9c2f346afa06bf44f10947349896efdec9559444f1d0016e8288b1ce_bf6aff981e022f413c315d1e1b0769d78612f114820c8c21b4f50467c94ab465(Execution,
		*(UObject**)l_fp);
}
// AS Function : int CallVirtualValue()
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:218:2
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
