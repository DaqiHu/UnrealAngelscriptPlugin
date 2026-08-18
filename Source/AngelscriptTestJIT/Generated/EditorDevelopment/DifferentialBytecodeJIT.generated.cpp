// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : BytecodeJIT
 * Declaration        : int DifferentialScalarValue(const int, const int)
 * StableFunctionKey  : bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b
 * OriginalSymbolSet  : ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_24397a70f048c17cf8cc8051d54e6394dc379c349573bc515f4253b4d6d503ae
 * TestSymbolSet      : AS_Test_Differential_Bytecode_DifferentialScalarValue
 * CleanupPlan       : NotCaptured (0 actions)
 * Publication        : test-only entry accessors; no Provider registration
 */

#if WITH_EDITOR && UE_BUILD_DEVELOPMENT
#include "StaticJIT/StaticJITConfig.h"
#ifndef AS_SKIP_JITTED_CODE
#include "AngelscriptTestJITProbes.h"
#include "CoreGlobals.h"
#include "CoreMinimal.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallables.h"
#include "StaticJIT/StaticJITHeader.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallBridge.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITScalarOps.h"

/*
 * AngelScript Static JIT Function
 * Declaration       : int DifferentialScalarValue(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:14:2
 * StableFunctionKey  : bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b
 * ExecutionHash      : 24397a70f048c17cf8cc8051d54e6394dc379c349573bc515f4253b4d6d503ae
 * DebugHash          : 7e95ff287f42acc18ce60419452b035698306752b09373a328b1d598db200f98
 * EntryAbiHash       : dabb76ccb243078a57121d4c3a40ed732a70a68cca7584d9e9320a1fd5a679c4
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_AS_Test_Differential_Bytecode_DifferentialScalarValue = "ASStaticJITTypedFinalScalarFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_AS_Test_Differential_Bytecode_DifferentialScalarValue
#endif

// AS Function : int DifferentialScalarValue(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:14:2
// JIT Entry   : Raw
int32 AS_Test_Differential_Bytecode_DifferentialScalarValue(FScriptExecution& Execution, UObject* l_This, asDWORD p_A, asDWORD p_B)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UTypedASTFinalScalarReceiver::DifferentialScalarValue(int, int)", 16);
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
// MULIi v1, v-2, 3
((int32&)v_TEMP_dword_1) = ((int32&)p_A) * value_as<int>((asDWORD)0x3u);
// ADDi v1, v1, v-3
((int32&)v_TEMP_dword_1) = ((int32&)v_TEMP_dword_1) + ((int32&)p_B);
// SUBIi v1, v1, 7
((int32&)v_TEMP_dword_1) = ((int32&)v_TEMP_dword_1) - value_as<int>((asDWORD)0x7u);
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 4
  return (int32)l_dwordRegister;
}
// AS Function : int DifferentialScalarValue(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:14:2
// JIT Entry   : VM
void AS_Test_Differential_Bytecode_DifferentialScalarValue_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_Test_Differential_Bytecode_DifferentialScalarValue(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2),
		*(asDWORD*)(l_fp + 3));
}
// AS Function : int DifferentialScalarValue(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:14:2
// JIT Entry   : Parms
void AS_Test_Differential_Bytecode_DifferentialScalarValue_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_A = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1_B = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_Test_Differential_Bytecode_DifferentialScalarValue(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_A),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_1_B));
}

namespace
{
	static const FAngelscriptJITReferenceSlot FBytecodeJITDifferentialEntryRegistrationReferenceSlots[1] =
	{
		{sizeof(FAngelscriptJITReferenceSlot), 0u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(2), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("fad89b1b314e72fddfda397e555d92b3e881dce5158330451090697253e7b4e8")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("85b780d3d0320f0320f21f66806418c53611e2e04d160beb79f3c8a598f7a9bd")))}}},
	};

	struct FBytecodeJITDifferentialEntryRegistration
	{
		FBytecodeJITDifferentialEntryRegistration()
		{
			AngelscriptTestJITProbes::RegisterIsolatedDifferentialEntry(
				AngelscriptTestJITProbes::EIsolatedDifferentialBackend::BytecodeJIT,
				&AS_Test_Differential_Bytecode_DifferentialScalarValue_VMEntry,
				reinterpret_cast<asJITFunction_Raw>(&AS_Test_Differential_Bytecode_DifferentialScalarValue),
				&AS_Test_Differential_Bytecode_DifferentialScalarValue_ParmsEntry,
				TEXT("AS_Test_Differential_Bytecode_DifferentialScalarValue"),
				TEXT("bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b"),
				TEXT("int DifferentialScalarValue(const int, const int)"),
				TEXT("/Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as"),
				14u,
				2u,
				AngelscriptTestJITProbes::EIsolatedDifferentialCleanupPlanState::NotCaptured,
				0u,
				FBytecodeJITDifferentialEntryRegistrationReferenceSlots,
				1u);
		}
	};

	FBytecodeJITDifferentialEntryRegistration GBytecodeJITDifferentialEntryRegistration;
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
