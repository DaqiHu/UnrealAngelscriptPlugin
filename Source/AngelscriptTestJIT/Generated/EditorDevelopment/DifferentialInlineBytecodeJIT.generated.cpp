// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : BytecodeJIT
 * Declaration        : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
 * StableFunctionKey  : a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b
 * OriginalSymbolSet  : ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_05fd317498c7485b03d324e5b6852891ad329d9d64d886363512903114895bb4
 * TestSymbolSet      : AS_Test_Differential_Bytecode_TypedASTCapabilityShowcase
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
 * Declaration       : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:50:1
 * StableFunctionKey  : a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b
 * ExecutionHash      : 05fd317498c7485b03d324e5b6852891ad329d9d64d886363512903114895bb4
 * DebugHash          : b13cf30324227b9a171896dc6287854a35be6e5d2dfefde39170521d32f42fb8
 * EntryAbiHash       : cb2a93da398c2fcf539260ba1e5124234b91d834988612b3e5ccec9481cdd5ce
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_AS_Test_Differential_Bytecode_TypedASTCapabilityShowcase = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_AS_Test_Differential_Bytecode_TypedASTCapabilityShowcase
#endif

// AS Function : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:50:1
// JIT Entry   : Raw
int32 AS_Test_Differential_Bytecode_TypedASTCapabilityShowcase(FScriptExecution& Execution, asDWORD p_A, asDWORD p_B, asBYTE p_bEnabled, asBYTE p_bExtraCheck)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int TypedASTCapabilityShowcase(int, int, bool, bool)", 56);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_Value = {};
asDWORD v_PrefixResult = {};
asDWORD v_PostfixResult = {};
asDWORD v_Negated = {};
asDWORD v_Inverted = {};
asDWORD v_ShiftedLeft = {};
asDWORD v_ShiftedRight = {};
asBYTE v_bMutationGate = {};
asBYTE v_bNativeGate = {};
asDWORD v_TEMP_dword_8 = {};
asDWORD v_TEMP_dword_9 = {};
asBYTE v_TEMP_byte_10 = {};
asBYTE v_TEMP_byte_13 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// ADDi v2, v0, v-1
((int32&)v_Value) = ((int32&)p_A) + ((int32&)p_B);
// SUSPEND
// JitEntry *
// SUBIi v2, v2, 1
((int32&)v_Value) = ((int32&)v_Value) - value_as<int>((asDWORD)0x1u);
// SUSPEND
// JitEntry *
// ADDIi v2, v2, 3
((int32&)v_Value) = ((int32&)v_Value) + value_as<int>((asDWORD)0x3u);
// SUSPEND
// JitEntry *
// SUBIi v2, v2, 1
((int32&)v_Value) = ((int32&)v_Value) - value_as<int>((asDWORD)0x1u);
// SUSPEND
// JitEntry *
// MULIi v2, v2, 2
((int32&)v_Value) = ((int32&)v_Value) * value_as<int>((asDWORD)0x2u);
// SUSPEND
// JitEntry *
// LDV v2
l_valueRegister = (asQWORD)(&v_Value);
// INCi
(**(int32**)&l_valueRegister)++;
// RDR4 v1
v_PostfixResult = value_read<asDWORD>((void*)l_valueRegister);
// CpyVtoV4 v3, v1
v_PrefixResult = v_PostfixResult;
// SUSPEND
// JitEntry *
// LDV v2
l_valueRegister = (asQWORD)(&v_Value);
// RDR4 v1
v_PostfixResult = value_read<asDWORD>((void*)l_valueRegister);
// INCi
(**(int32**)&l_valueRegister)++;
// SUSPEND
// JitEntry *
// CpyVtoV4 v4, v3
v_Negated = v_PrefixResult;
// NEGi v4
((int32&)v_Negated) = - ((int32&)v_Negated);
// SUSPEND
// JitEntry *
// CpyVtoV4 v5, v1
v_Inverted = v_PostfixResult;
// BNOT v5
v_Inverted = ~v_Inverted;
// SUSPEND
// JitEntry *
// SetV4 v8, 1
v_TEMP_dword_8 = 0x1u;
// BSLL v6, v2, v8
v_ShiftedLeft = v_Value << v_TEMP_dword_8;
// SUSPEND
// JitEntry *
// SetV4 v8, 1
v_TEMP_dword_8 = 0x1u;
// BSRL v7, v2, v8
v_ShiftedRight = v_Value >> v_TEMP_dword_8;
// SUSPEND
// JitEntry *
// CpyVtoR1 v-2
l_byteRegister = p_bEnabled;
// JLowNZ 4
if(l_byteRegister != 0) {
goto LABEL_TypedASTCapabilityShowcase_94;
}
// SetV4 v12, 0
v_bMutationGate = 0x0u;
// JMP 9
goto LABEL_TypedASTCapabilityShowcase_103;
LABEL_TypedASTCapabilityShowcase_94:
// LDV v2
l_valueRegister = (asQWORD)(&v_Value);
// INCi
(**(int32**)&l_valueRegister)++;
// RDR4 v9
v_TEMP_dword_9 = value_read<asDWORD>((void*)l_valueRegister);
// CMPi v9, v-1
  if (((int32&)v_TEMP_dword_9) == ((int32&)p_B))     l_byteRegister = 0;
  else if (((int32&)v_TEMP_dword_9) < ((int32&)p_B)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// TP
if(((int8&)l_byteRegister) > 0)
   l_byteRegister = 1;
else
   l_byteRegister = 0;
// CpyRtoV4 v11
v_bNativeGate = l_byteRegister;
// CpyVtoV4 v12, v11
v_bMutationGate = v_bNativeGate;
LABEL_TypedASTCapabilityShowcase_103:
// SUSPEND
// JitEntry *
// CpyVtoR1 v12
l_byteRegister = v_bMutationGate;
// JLowNZ 4
if(l_byteRegister != 0) {
goto LABEL_TypedASTCapabilityShowcase_114;
}
// SetV4 v11, 0
v_bNativeGate = 0x0u;
// JMP 2
goto LABEL_TypedASTCapabilityShowcase_116;
LABEL_TypedASTCapabilityShowcase_114:
// CpyVtoV4 v11, v-3
v_bNativeGate = p_bExtraCheck;
LABEL_TypedASTCapabilityShowcase_116:
// CpyVtoR1 v11
l_byteRegister = v_bNativeGate;
// JLowNZ 4
if(l_byteRegister != 0) {
goto LABEL_TypedASTCapabilityShowcase_123;
}
// SetV4 v11, 0
v_bNativeGate = 0x0u;
// JMP 9
goto LABEL_TypedASTCapabilityShowcase_132;
LABEL_TypedASTCapabilityShowcase_123:
// CALLSYS *
// bool IsRunningCommandlet()
{
bool FunctionReturnValue = IsRunningCommandlet();
l_byteRegister = (asBYTE&)FunctionReturnValue;
}
// JitEntry *
// CpyRtoV4 v10
v_TEMP_byte_10 = l_byteRegister;
// CpyVtoV4 v11, v10
v_bNativeGate = v_TEMP_byte_10;
LABEL_TypedASTCapabilityShowcase_132:
// SUSPEND
// JitEntry *
// CpyVtoR1 v11
l_byteRegister = v_bNativeGate;
// JLowZ 15
if(l_byteRegister == 0) {
goto LABEL_TypedASTCapabilityShowcase_154;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// ADDi v9, v6, v5
((int32&)v_TEMP_dword_9) = ((int32&)v_ShiftedLeft) + ((int32&)v_Inverted);
// ADDi v9, v9, v2
((int32&)v_TEMP_dword_9) = ((int32&)v_TEMP_dword_9) + ((int32&)v_Value);
// CpyVtoR4 v9
l_dwordRegister = v_TEMP_dword_9;
// JMP 34
goto LABEL_TypedASTCapabilityShowcase_188;
LABEL_TypedASTCapabilityShowcase_154:
// SUSPEND
// JitEntry *
// CMPIi v7, 10
  if (((int32&)v_ShiftedRight) == value_as<int>((asDWORD)0xau))     l_byteRegister = 0;
  else if (((int32&)v_ShiftedRight) < value_as<int>((asDWORD)0xau)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JNP 16
if(((int8&)l_byteRegister) <= 0) {
goto LABEL_TypedASTCapabilityShowcase_178;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// MULIi v9, v7, 2
((int32&)v_TEMP_dword_9) = ((int32&)v_ShiftedRight) * value_as<int>((asDWORD)0x2u);
// ADDi v9, v9, v2
((int32&)v_TEMP_dword_9) = ((int32&)v_TEMP_dword_9) + ((int32&)v_Value);
// CpyVtoR4 v9
l_dwordRegister = v_TEMP_dword_9;
// JMP 10
goto LABEL_TypedASTCapabilityShowcase_188;
LABEL_TypedASTCapabilityShowcase_178:
// SUSPEND
// JitEntry *
// SUBIi v9, v4, 1
((int32&)v_TEMP_dword_9) = ((int32&)v_Negated) - value_as<int>((asDWORD)0x1u);
// ADDi v9, v9, v2
((int32&)v_TEMP_dword_9) = ((int32&)v_TEMP_dword_9) + ((int32&)v_Value);
// CpyVtoR4 v9
l_dwordRegister = v_TEMP_dword_9;
LABEL_TypedASTCapabilityShowcase_188:
// RET 4
  return (int32)l_dwordRegister;
}
// AS Function : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:50:1
// JIT Entry   : VM
void AS_Test_Differential_Bytecode_TypedASTCapabilityShowcase_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_Test_Differential_Bytecode_TypedASTCapabilityShowcase(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1),
		*(asBYTE*)(l_fp + 2),
		*(asBYTE*)(l_fp + 3));
}

namespace
{
	static const FAngelscriptJITReferenceSlot FBytecodeJITDifferentialEntryRegistrationReferenceSlots[1] =
	{
		{sizeof(FAngelscriptJITReferenceSlot), 0u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(7), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("d29d598a9a8a090fd329db0c1d3fe9a9feaa707f0f7f52e7c6ed766e9a0324fe")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("fbe6e43e9bfbf5b301c74de7f09a0baea8de4212da0cbc901a1e39c68e1b8eb5")))}}},
	};

	struct FBytecodeJITDifferentialEntryRegistration
	{
		FBytecodeJITDifferentialEntryRegistration()
		{
			AngelscriptTestJITProbes::RegisterIsolatedDifferentialEntry(
				AngelscriptTestJITProbes::EIsolatedDifferentialBackend::BytecodeJIT,
				&AS_Test_Differential_Bytecode_TypedASTCapabilityShowcase_VMEntry,
				reinterpret_cast<asJITFunction_Raw>(&AS_Test_Differential_Bytecode_TypedASTCapabilityShowcase),
				nullptr,
				TEXT("AS_Test_Differential_Bytecode_TypedASTCapabilityShowcase"),
				TEXT("a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b"),
				TEXT("int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)"),
				TEXT("/Angelscript/Game/ASStaticJITAotFixture.as"),
				50u,
				1u,
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
