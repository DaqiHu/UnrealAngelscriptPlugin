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
 * Source             : /Angelscript/Game/ASStaticJITAotImportProvider.as:1:1
 * StableFunctionKey  : 69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304
 * ExecutionHash      : 9914a68310eddcf93e409b77b518a926b96cf924042aa1b8ad49987d08d59e64
 * DebugHash          : 7f90fd72243da11dc964db16306b645bb222a39cf73ea06179a9e287dda5a0af
 * EntryAbiHash       : ea282433da811db8e11096c0eb4d74a8ad9f3e312688cd7887a915f4447e361e
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304_9914a68310eddcf93e409b77b518a926b96cf924042aa1b8ad49987d08d59e64 = "ASStaticJITAotImportProvider";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304_9914a68310eddcf93e409b77b518a926b96cf924042aa1b8ad49987d08d59e64
#endif

// AS Function : int ImportedValueForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotImportProvider.as:1:1
// JIT Entry   : Raw
int32 ASJIT_69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304_9914a68310eddcf93e409b77b518a926b96cf924042aa1b8ad49987d08d59e64(FScriptExecution& Execution, asDWORD p_Value)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotImportProvider.as:1:1
// JIT Entry   : VM
void ASJIT_69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304_9914a68310eddcf93e409b77b518a926b96cf924042aa1b8ad49987d08d59e64_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304_9914a68310eddcf93e409b77b518a926b96cf924042aa1b8ad49987d08d59e64(Execution,
		*(asDWORD*)(l_fp + 0));
}
// AS Function : int ImportedValueForAOT(const int)
// AS Source   : /Angelscript/Game/ASStaticJITAotImportProvider.as:1:1
// JIT Entry   : Parms
void ASJIT_69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304_9914a68310eddcf93e409b77b518a926b96cf924042aa1b8ad49987d08d59e64_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304_9914a68310eddcf93e409b77b518a926b96cf924042aa1b8ad49987d08d59e64(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int IndependentModuleEntryForAOT()
 * Source             : /Angelscript/Game/ASStaticJITAotImportProvider.as:6:1
 * StableFunctionKey  : 6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17
 * ExecutionHash      : 5e078b1b9d52d1d84b98aba0fb3b39e3819af75f263adae24e4f05fc8c1fb58e
 * DebugHash          : 348ad201ee5dc38139e3dfcf4bfd58684e47735d07ae64a056ae5451315d27de
 * EntryAbiHash       : bb45c9109478b2a9359d66c35fcbf092a8728f92b5800436659d51e95cc228a8
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17_5e078b1b9d52d1d84b98aba0fb3b39e3819af75f263adae24e4f05fc8c1fb58e = "ASStaticJITAotImportProvider";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17_5e078b1b9d52d1d84b98aba0fb3b39e3819af75f263adae24e4f05fc8c1fb58e
#endif

// AS Function : int IndependentModuleEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotImportProvider.as:6:1
// JIT Entry   : Raw
int32 ASJIT_6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17_5e078b1b9d52d1d84b98aba0fb3b39e3819af75f263adae24e4f05fc8c1fb58e(FScriptExecution& Execution)
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
// AS Source   : /Angelscript/Game/ASStaticJITAotImportProvider.as:6:1
// JIT Entry   : VM
void ASJIT_6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17_5e078b1b9d52d1d84b98aba0fb3b39e3819af75f263adae24e4f05fc8c1fb58e_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17_5e078b1b9d52d1d84b98aba0fb3b39e3819af75f263adae24e4f05fc8c1fb58e(Execution);
}
// AS Function : int IndependentModuleEntryForAOT()
// AS Source   : /Angelscript/Game/ASStaticJITAotImportProvider.as:6:1
// JIT Entry   : Parms
void ASJIT_6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17_5e078b1b9d52d1d84b98aba0fb3b39e3819af75f263adae24e4f05fc8c1fb58e_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_6e91286ea26e37e3990fb6a25366dd42719eb66bac5a8e3ea5593c32105dcb17_5e078b1b9d52d1d84b98aba0fb3b39e3819af75f263adae24e4f05fc8c1fb58e(Execution);
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
