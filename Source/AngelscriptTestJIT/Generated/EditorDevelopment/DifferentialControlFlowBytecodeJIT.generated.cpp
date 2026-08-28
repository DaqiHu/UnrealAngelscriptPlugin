// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : BytecodeJIT
 * Declaration        : int SemanticExhaustiveEnum(const int)
 * StableFunctionKey  : 0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4
 * OriginalSymbolSet  : ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_30d28816e1d190422c317e7590e3b1595313b06a2fc09fde5244d8276bf5d873
 * TestSymbolSet      : AS_Test_Differential_Bytecode_SemanticExhaustiveEnum
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
 * Declaration       : int SemanticExhaustiveEnum(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:101:1
 * StableFunctionKey  : 0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4
 * ExecutionHash      : 30d28816e1d190422c317e7590e3b1595313b06a2fc09fde5244d8276bf5d873
 * DebugHash          : efea96a8f7ec1863dc51c8ea8bafe3d4c509cabd638fab348147d14e69d754ee
 * EntryAbiHash       : 3b5a06c877b274f22ba1b63a3dd81f44e2041a70d3e47fd81bb7378a11c8129b
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_AS_Test_Differential_Bytecode_SemanticExhaustiveEnum = "ASStaticJITTypedControlFlowFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_AS_Test_Differential_Bytecode_SemanticExhaustiveEnum
#endif

// AS Function : int SemanticExhaustiveEnum(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:101:1
// JIT Entry   : Raw
int32 AS_Test_Differential_Bytecode_SemanticExhaustiveEnum(FScriptExecution& Execution, asDWORD p_RawMode)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int SemanticExhaustiveEnum(int)", 103);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asBYTE v_Mode = {};
asBYTE v_TEMP_byte_1 = {};
asDWORD v_TEMP_dword_3 = {};
asDWORD v_TEMP_dword_4 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// iTOb v2, v0
v_Mode = value_as<asBYTE>(p_RawMode);
// SUSPEND
// JitEntry *
// sbTOi v3, v2
((int32&)v_TEMP_dword_3) = int(((int8&)v_Mode));
// CMPIi v3, 2
  if (((int32&)v_TEMP_dword_3) == value_as<int>((asDWORD)0x2u))     l_byteRegister = 0;
  else if (((int32&)v_TEMP_dword_3) < value_as<int>((asDWORD)0x2u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JP 42
if(((int8&)l_byteRegister) > 0) {
goto LABEL_SemanticExhaustiveEnum_61;
}
// CMPIi v3, 0
  if (((int32&)v_TEMP_dword_3) == value_as<int>((asDWORD)0x0u))     l_byteRegister = 0;
  else if (((int32&)v_TEMP_dword_3) < value_as<int>((asDWORD)0x0u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JS 38
if(((int8&)l_byteRegister) < 0) {
goto LABEL_SemanticExhaustiveEnum_61;
}
// SUBIi v4, v3, 0
((int32&)v_TEMP_dword_4) = ((int32&)v_TEMP_dword_3) - value_as<int>((asDWORD)0x0u);
// JMPP v4, 2
switch(v_TEMP_dword_4)
{
case 0:
goto LABEL_SemanticExhaustiveEnum_34;
break;
case 1:
goto LABEL_SemanticExhaustiveEnum_43;
break;
case 2:
goto LABEL_SemanticExhaustiveEnum_52;
break;
}
// JMP 4
// JMP 11
// JMP 18
LABEL_SemanticExhaustiveEnum_34:
// SUSPEND
// JitEntry *
// SetV4 v4, 10
v_TEMP_dword_4 = 0xau;
// CpyVtoR4 v4
l_dwordRegister = v_TEMP_dword_4;
// JMP 23
goto LABEL_SemanticExhaustiveEnum_66;
LABEL_SemanticExhaustiveEnum_43:
// SUSPEND
// JitEntry *
// SetV4 v3, 20
v_TEMP_dword_3 = 0x14u;
// CpyVtoR4 v3
l_dwordRegister = v_TEMP_dword_3;
// JMP 14
goto LABEL_SemanticExhaustiveEnum_66;
LABEL_SemanticExhaustiveEnum_52:
// SUSPEND
// JitEntry *
// SetV4 v4, 30
v_TEMP_dword_4 = 0x1eu;
// CpyVtoR4 v4
l_dwordRegister = v_TEMP_dword_4;
// JMP 5
goto LABEL_SemanticExhaustiveEnum_66;
LABEL_SemanticExhaustiveEnum_61:
// ThrowException 0
SCRIPT_DEBUG_CALLSTACK_POSITION(111, 3);
SCRIPT_SWITCH_VALUE_INVALID_EXCEPTION();
return {};
// SUSPEND
// JitEntry *
LABEL_SemanticExhaustiveEnum_66:
// RET 1
  return (int32)l_dwordRegister;
}
// AS Function : int SemanticExhaustiveEnum(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:101:1
// JIT Entry   : VM
void AS_Test_Differential_Bytecode_SemanticExhaustiveEnum_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_Test_Differential_Bytecode_SemanticExhaustiveEnum(Execution,
		*(asDWORD*)(l_fp + 0));
}

namespace
{
	static const FAngelscriptJITReferenceSlot FBytecodeJITDifferentialEntryRegistrationReferenceSlots[1] =
	{
		{sizeof(FAngelscriptJITReferenceSlot), 0u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(2), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("da204858ced604390176b13c9258e987373f2b4e798ef2bae576892d1387d4a8")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("8746c9775c41ecdcb7c63d6370d6a1a82a6d6c81fab94b0d5db855661c5b9865")))}}},
	};

	struct FBytecodeJITDifferentialEntryRegistration
	{
		FBytecodeJITDifferentialEntryRegistration()
		{
			AngelscriptTestJITProbes::RegisterIsolatedDifferentialEntry(
				AngelscriptTestJITProbes::EIsolatedDifferentialBackend::BytecodeJIT,
				&AS_Test_Differential_Bytecode_SemanticExhaustiveEnum_VMEntry,
				reinterpret_cast<asJITFunction_Raw>(&AS_Test_Differential_Bytecode_SemanticExhaustiveEnum),
				nullptr,
				TEXT("AS_Test_Differential_Bytecode_SemanticExhaustiveEnum"),
				TEXT("0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4"),
				TEXT("int SemanticExhaustiveEnum(const int)"),
				TEXT("/Angelscript/Game/ASStaticJITTypedControlFlowFixture.as"),
				101u,
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
