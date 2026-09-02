// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : BytecodeJIT
 * Declaration        : float SemanticPowerBoundary(const float, const float)
 * StableFunctionKey  : 23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67
 * OriginalSymbolSet  : ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_1a68b973ffdfd389d207a1096a9048cae38409c2d8b1e367abfd2dbb6a3e532f
 * TestSymbolSet      : AS_Test_Differential_Bytecode_SemanticPowerBoundary
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
 * Declaration       : float SemanticPowerBoundary(const float, const float)
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:73:2
 * StableFunctionKey  : 23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67
 * ExecutionHash      : 1a68b973ffdfd389d207a1096a9048cae38409c2d8b1e367abfd2dbb6a3e532f
 * DebugHash          : b09e16ad5f9a3216822a51e060ae2a349bff1f29c5fca30db7ced65dd95ffeb8
 * EntryAbiHash       : 5bdf2107db2403a739f0c1ec988a215ef8d4496714c3a85248de4057037ca025
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_AS_Test_Differential_Bytecode_SemanticPowerBoundary = "ASStaticJITTypedFinalScalarFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_AS_Test_Differential_Bytecode_SemanticPowerBoundary
#endif

// AS Function : float SemanticPowerBoundary(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:73:2
// JIT Entry   : Raw
double AS_Test_Differential_Bytecode_SemanticPowerBoundary(FScriptExecution& Execution, UObject* l_This, double p_Base, double p_Exponent)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("float UTypedASTFinalScalarReceiver::SemanticPowerBoundary(float, float)", 77);
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
// POWd v2, v-2, v-4
v_TEMP_double_2 = FMath::Pow(p_Base, p_Exponent);
if (v_TEMP_double_2 == HUGE_VAL) [[unlikely]]
{
SCRIPT_DEBUG_CALLSTACK_POSITION(77, 3);
  SCRIPT_POWER_OVERFLOW_EXCEPTION();
return {};
}
// CpyVtoR8 v2
l_doubleRegister = v_TEMP_double_2;
// RET 6
  return l_doubleRegister;
}
// AS Function : float SemanticPowerBoundary(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:73:2
// JIT Entry   : VM
void AS_Test_Differential_Bytecode_SemanticPowerBoundary_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(double*)l_outValue = AS_Test_Differential_Bytecode_SemanticPowerBoundary(Execution,
		*(UObject**)l_fp,
		*(double*)(l_fp + 2),
		*(double*)(l_fp + 4));
}
// AS Function : float SemanticPowerBoundary(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:73:2
// JIT Entry   : Parms
void AS_Test_Differential_Bytecode_SemanticPowerBoundary_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Base = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(double));
	const SIZE_T ParmOffset_1_Exponent = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(double));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(double*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_Test_Differential_Bytecode_SemanticPowerBoundary(Execution,
		(UObject*)Object,
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_0_Base),
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_1_Exponent));
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
				&AS_Test_Differential_Bytecode_SemanticPowerBoundary_VMEntry,
				reinterpret_cast<asJITFunction_Raw>(&AS_Test_Differential_Bytecode_SemanticPowerBoundary),
				&AS_Test_Differential_Bytecode_SemanticPowerBoundary_ParmsEntry,
				TEXT("AS_Test_Differential_Bytecode_SemanticPowerBoundary"),
				TEXT("23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67"),
				TEXT("float SemanticPowerBoundary(const float, const float)"),
				TEXT("/Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as"),
				73u,
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
