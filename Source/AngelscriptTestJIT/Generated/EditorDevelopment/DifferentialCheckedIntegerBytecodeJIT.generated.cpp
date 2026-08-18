// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : BytecodeJIT
 * Declaration        : int SemanticCheckedDivide(const int, const int)
 * StableFunctionKey  : a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5
 * OriginalSymbolSet  : ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_3c9395cb48100816600076228d1517860b80fdd10e52295aac2a31f1b9af6f3d
 * TestSymbolSet      : AS_Test_Differential_Bytecode_SemanticCheckedDivide
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
 * Declaration       : int SemanticCheckedDivide(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:116:1
 * StableFunctionKey  : a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5
 * ExecutionHash      : 3c9395cb48100816600076228d1517860b80fdd10e52295aac2a31f1b9af6f3d
 * DebugHash          : 7be5fa7ce3d293b37a860e95bb2fc9f0448c71efd8c89d5f7d190ea0fd86bf06
 * EntryAbiHash       : 595fada6d5d56a6ca4f2ca100ae14df66a5e879dcfb48c99dce7f4ee901501f4
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_AS_Test_Differential_Bytecode_SemanticCheckedDivide = "ASStaticJITTypedControlFlowFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_AS_Test_Differential_Bytecode_SemanticCheckedDivide
#endif

// AS Function : int SemanticCheckedDivide(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:116:1
// JIT Entry   : Raw
int32 AS_Test_Differential_Bytecode_SemanticCheckedDivide(FScriptExecution& Execution, asDWORD p_Numerator, asDWORD p_Denominator)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int SemanticCheckedDivide(int, int)", 118);
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
// DIVi v1, v0, v-1
{
  int divider = ((int32&)p_Denominator);
  int operand = ((int32&)p_Numerator);
  if (divider == 0) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(118, 2);
     SCRIPT_DIV0_EXCEPTION();
return {};
  }
  else if (divider == -1 && operand == int(0x80000000)) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(118, 2);
     SCRIPT_OVERFLOW_EXCEPTION();
return {};
  }
((int32&)v_TEMP_dword_1) = operand / divider;
}
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int SemanticCheckedDivide(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:116:1
// JIT Entry   : VM
void AS_Test_Differential_Bytecode_SemanticCheckedDivide_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_Test_Differential_Bytecode_SemanticCheckedDivide(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}

namespace
{
	struct FBytecodeJITDifferentialEntryRegistration
	{
		FBytecodeJITDifferentialEntryRegistration()
		{
			AngelscriptTestJITProbes::RegisterIsolatedDifferentialEntry(
				AngelscriptTestJITProbes::EIsolatedDifferentialBackend::BytecodeJIT,
				&AS_Test_Differential_Bytecode_SemanticCheckedDivide_VMEntry,
				reinterpret_cast<asJITFunction_Raw>(&AS_Test_Differential_Bytecode_SemanticCheckedDivide),
				nullptr,
				TEXT("AS_Test_Differential_Bytecode_SemanticCheckedDivide"),
				TEXT("a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5"),
				TEXT("int SemanticCheckedDivide(const int, const int)"),
				TEXT("/Angelscript/Game/ASStaticJITTypedControlFlowFixture.as"),
				116u,
				1u,
				AngelscriptTestJITProbes::EIsolatedDifferentialCleanupPlanState::NotCaptured,
				0u,
				nullptr,
				0u);
		}
	};

	FBytecodeJITDifferentialEntryRegistration GBytecodeJITDifferentialEntryRegistration;
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
