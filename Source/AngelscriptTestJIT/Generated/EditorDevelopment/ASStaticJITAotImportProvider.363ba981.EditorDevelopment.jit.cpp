// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Module
 * VirtualSourcePath   : /Angelscript/Game/ASStaticJITAotImportProvider.as
 * CanonicalModuleName : ASStaticJITAotImportProvider
 * TargetProfile       : EditorDevelopment
 * StableModuleKey     : 363ba981d7c3e864c636826dc571c5d7e306f994e52ec1db353151967b367751
 * ProviderId          : 7dc70fd55a76cc55d726e32d5c2434713bd93755a32e4dd524a4b7c39c8b5d44
 * ArtifactProfile     : 018cc71ab6612eca7608aa13abe1f7a1a7d81964bb8cc5821f1b1e047fcf7a11
 * NativeEnvironment   : 76d620ec7d0db38aa2028996e595f75523f201e7e560c05d8c9000d60d04cd73
 * FunctionCount       : 2
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
 * Declaration       : int ImportedValueForAOT(const int)
 * Source             : /Angelscript/Game/ASStaticJITAotImportProvider.as:1:5
 * StableFunctionKey  : 69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304
 * ExecutionHash      : b90c957b05c82e463e82b7bcd35afe10b797255a6e9ee8df24d62d2e1c0737b3
 * DebugHash          : f95388c4616d62cd5321aa25f1d08d28957b3f98f225c75d4adcaf6e90388f63
 * EntryAbiHash       : ea282433da811db8e11096c0eb4d74a8ad9f3e312688cd7887a915f4447e361e
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304_b90c957b05c82e463e82b7bcd35afe10b797255a6e9ee8df24d62d2e1c0737b3 = "ASStaticJITAotImportProvider";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304_b90c957b05c82e463e82b7bcd35afe10b797255a6e9ee8df24d62d2e1c0737b3
#endif

// AS Function : int ImportedValueForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotImportProvider.as:1:5
// JIT Entry   : Raw
int32 ASJIT_69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304_b90c957b05c82e463e82b7bcd35afe10b797255a6e9ee8df24d62d2e1c0737b3(FScriptExecution& Execution, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int ImportedValueForAOT(int)", 2);
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
// ADDIi v1, v0, 77
((int32&)v_TEMP_dword_1) = ((int32&)p_Value) + value_as<int>((asDWORD)0x4du);
// JMP 0
goto LABEL_ImportedValueForAOT_16;
LABEL_ImportedValueForAOT_16:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 1
  return (int32)l_dwordRegister;
}
// AS Function : int ImportedValueForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotImportProvider.as:1:5
// JIT Entry   : VM
void ASJIT_69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304_b90c957b05c82e463e82b7bcd35afe10b797255a6e9ee8df24d62d2e1c0737b3_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304_b90c957b05c82e463e82b7bcd35afe10b797255a6e9ee8df24d62d2e1c0737b3(Execution,
		*(asDWORD*)(l_fp + 0));
}
// AS Function : int ImportedValueForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotImportProvider.as:1:5
// JIT Entry   : Parms
void ASJIT_69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304_b90c957b05c82e463e82b7bcd35afe10b797255a6e9ee8df24d62d2e1c0737b3_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304_b90c957b05c82e463e82b7bcd35afe10b797255a6e9ee8df24d62d2e1c0737b3(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int IndependentModuleEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotImportProvider.as:6:5
 * StableFunctionKey  : 6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17
 * ExecutionHash      : 8186a69f387a122950bec39b74e9afc9fc7674f2150a1091ddb943928c82ed07
 * DebugHash          : 883d145de981a4ecd4b18db561881e8ccbc361a4ffed6a9cbe7ac5819635016b
 * EntryAbiHash       : bb45c9109478b2a9359d66c35fcbf092a8728f92b5800436659d51e95cc228a8
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17_8186a69f387a122950bec39b74e9afc9fc7674f2150a1091ddb943928c82ed07 = "ASStaticJITAotImportProvider";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17_8186a69f387a122950bec39b74e9afc9fc7674f2150a1091ddb943928c82ed07
#endif

// AS Function : int IndependentModuleEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotImportProvider.as:6:5
// JIT Entry   : Raw
int32 ASJIT_6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17_8186a69f387a122950bec39b74e9afc9fc7674f2150a1091ddb943928c82ed07(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int IndependentModuleEntryForAOT()", 7);
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
// SetV4 v1, 87
v_TEMP_dword_1 = 0x57u;
// JMP 0
goto LABEL_IndependentModuleEntryForAOT_15;
LABEL_IndependentModuleEntryForAOT_15:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 0
  return (int32)l_dwordRegister;
}
// AS Function : int IndependentModuleEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotImportProvider.as:6:5
// JIT Entry   : VM
void ASJIT_6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17_8186a69f387a122950bec39b74e9afc9fc7674f2150a1091ddb943928c82ed07_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17_8186a69f387a122950bec39b74e9afc9fc7674f2150a1091ddb943928c82ed07(Execution);
}
// AS Function : int IndependentModuleEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotImportProvider.as:6:5
// JIT Entry   : Parms
void ASJIT_6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17_8186a69f387a122950bec39b74e9afc9fc7674f2150a1091ddb943928c82ed07_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17_8186a69f387a122950bec39b74e9afc9fc7674f2150a1091ddb943928c82ed07(Execution);
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
