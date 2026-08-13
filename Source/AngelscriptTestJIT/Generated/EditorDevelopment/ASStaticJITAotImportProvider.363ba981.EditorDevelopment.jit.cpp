// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Module
 * VirtualSourcePath   : /Angelscript/Game/ASStaticJITAotImportProvider.as
 * CanonicalModuleName : ASStaticJITAotImportProvider
 * TargetProfile       : EditorDevelopment
 * StableModuleKey     : 363ba981d7c3e864c636826dc571c5d7e306f994e52ec1db353151967b367751
 * ProviderId          : 7dc70fd55a76cc55d726e32d5c2434713bd93755a32e4dd524a4b7c39c8b5d44
 * ArtifactProfile     : b58efc4c9f40879f0b98bcb54704d1b342ae56948a00e9ddbaa709787226e0c8
 * NativeEnvironment   : c8d9638e7c34100b327617ea9a4769bd7692413e17eb4705818e6b120acc9651
 * FunctionCount       : 2
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
 * Declaration       : int ImportedValueForAOT(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotImportProvider.as:1:1
 * StableFunctionKey  : 69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304
 * ExecutionHash      : ee7830a9de56f70fda6397fbd582a13fe0e06765496d8709f73959ffd657d386
 * DebugHash          : c92a72a1bc65d3358bb45389aa0f5115f6ec3e39794a7b31ea28c102a2067eac
 * EntryAbiHash       : 6d09f7b9082eb4f76d6e496f456be2ced48f7280b340a8e8918d15ba719fbe7e
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304_ee7830a9de56f70fda6397fbd582a13fe0e06765496d8709f73959ffd657d386 = "ASStaticJITAotImportProvider";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304_ee7830a9de56f70fda6397fbd582a13fe0e06765496d8709f73959ffd657d386
#endif

// AS Function : int ImportedValueForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotImportProvider.as:1:1
// JIT Entry   : Raw
int32 ASJIT_69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304_ee7830a9de56f70fda6397fbd582a13fe0e06765496d8709f73959ffd657d386(FScriptExecution& Execution, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int ImportedValueForAOT(int)", 3);
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
// ADDIi v1, v0, 77
((int32&)v_TEMP_dword_1) = ((int32&)p_Value) + value_as<int>((asDWORD)0x4du);
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 1
  return (int32)l_dwordRegister;
}
// AS Function : int ImportedValueForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotImportProvider.as:1:1
// JIT Entry   : VM
void ASJIT_69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304_ee7830a9de56f70fda6397fbd582a13fe0e06765496d8709f73959ffd657d386_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304_ee7830a9de56f70fda6397fbd582a13fe0e06765496d8709f73959ffd657d386(Execution,
		*(asDWORD*)(l_fp + 0));
}
// AS Function : int ImportedValueForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotImportProvider.as:1:1
// JIT Entry   : Parms
void ASJIT_69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304_ee7830a9de56f70fda6397fbd582a13fe0e06765496d8709f73959ffd657d386_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304_ee7830a9de56f70fda6397fbd582a13fe0e06765496d8709f73959ffd657d386(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int IndependentModuleEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotImportProvider.as:6:1
 * StableFunctionKey  : 6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17
 * ExecutionHash      : bfad2118806f08778b608ba288c00d6bbc4c4f48f942cf52dc1faa6cfc2e03b9
 * DebugHash          : b8ef3acb6782a01fd239a3e7d291977ba1ebdef595f70d4ce322ae69ac52488a
 * EntryAbiHash       : 7f083a9b1dd768e533b45cb3475599ddebd6427cb28a84f1cceb7b80435c2a67
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17_bfad2118806f08778b608ba288c00d6bbc4c4f48f942cf52dc1faa6cfc2e03b9 = "ASStaticJITAotImportProvider";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17_bfad2118806f08778b608ba288c00d6bbc4c4f48f942cf52dc1faa6cfc2e03b9
#endif

// AS Function : int IndependentModuleEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotImportProvider.as:6:1
// JIT Entry   : Raw
int32 ASJIT_6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17_bfad2118806f08778b608ba288c00d6bbc4c4f48f942cf52dc1faa6cfc2e03b9(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int IndependentModuleEntryForAOT()", 8);
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
// SetV4 v1, 87
v_TEMP_dword_1 = 0x57u;
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 0
  return (int32)l_dwordRegister;
}
// AS Function : int IndependentModuleEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotImportProvider.as:6:1
// JIT Entry   : VM
void ASJIT_6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17_bfad2118806f08778b608ba288c00d6bbc4c4f48f942cf52dc1faa6cfc2e03b9_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17_bfad2118806f08778b608ba288c00d6bbc4c4f48f942cf52dc1faa6cfc2e03b9(Execution);
}
// AS Function : int IndependentModuleEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotImportProvider.as:6:1
// JIT Entry   : Parms
void ASJIT_6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17_bfad2118806f08778b608ba288c00d6bbc4c4f48f942cf52dc1faa6cfc2e03b9_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17_bfad2118806f08778b608ba288c00d6bbc4c4f48f942cf52dc1faa6cfc2e03b9(Execution);
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
