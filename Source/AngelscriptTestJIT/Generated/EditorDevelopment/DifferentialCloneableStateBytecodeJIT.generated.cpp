// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : BytecodeJIT
 * Declaration        : int SemanticCloneableState(const int, const int, const bool, const bool)
 * StableFunctionKey  : 169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855
 * OriginalSymbolSet  : ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_c964e2e167add1c442b0af2499294ec8119f150f5efa85cc79dc400bd3954cb3
 * TestSymbolSet      : AS_Test_Differential_Bytecode_SemanticCloneableState
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
 * Declaration       : int SemanticCloneableState(const int, const int, const bool, const bool)
 * Source             : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:122:1
 * StableFunctionKey  : 169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855
 * ExecutionHash      : c964e2e167add1c442b0af2499294ec8119f150f5efa85cc79dc400bd3954cb3
 * DebugHash          : 0d781fb6790454ef631cc4fe361f310dc379e15f5b83eaa513bcc7a04adeff43
 * EntryAbiHash       : e5782875c3c4a2dd62aa61ee2f701f30dd65e1f3a6797584d14efc61ed23bebb
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_AS_Test_Differential_Bytecode_SemanticCloneableState = "ASStaticJITTypedControlFlowFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_AS_Test_Differential_Bytecode_SemanticCloneableState
#endif

// AS Function : int SemanticCloneableState(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:122:1
// JIT Entry   : Raw
int32 AS_Test_Differential_Bytecode_SemanticCloneableState(FScriptExecution& Execution, asDWORD p_Seed, asDWORD p_Mode, asBYTE p_bLeft, asBYTE p_bRight)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int SemanticCloneableState(int, int, bool, bool)", 128);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_Trace = {};
asDWORD v_Eager = {};
asBYTE v_bGate = {};
asDWORD v_Postfix = {};
asDWORD v_TEMP_dword_3 = {};
asDWORD v_TEMP_dword_5 = {};
asDWORD v_TEMP_dword_6 = {};
asBYTE v_TEMP_byte_7 = {};
asBYTE v_TEMP_byte_9 = {};
asDWORD v_TEMP_dword_10 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// CpyVtoV4 v1, v0
v_Trace = p_Seed;
// SUSPEND
// JitEntry *
// CMPIi v-1, 16
  if (((int32&)p_Mode) == value_as<int>((asDWORD)0x10u))     l_byteRegister = 0;
  else if (((int32&)p_Mode) < value_as<int>((asDWORD)0x10u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JP 1125
if(((int8&)l_byteRegister) > 0) {
goto LABEL_SemanticCloneableState_1142;
}
// CMPIi v-1, 0
  if (((int32&)p_Mode) == value_as<int>((asDWORD)0x0u))     l_byteRegister = 0;
  else if (((int32&)p_Mode) < value_as<int>((asDWORD)0x0u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JS 1121
if(((int8&)l_byteRegister) < 0) {
goto LABEL_SemanticCloneableState_1142;
}
// SUBIi v2, v-1, 0
((int32&)v_Postfix) = ((int32&)p_Mode) - value_as<int>((asDWORD)0x0u);
// JMPP v2, 16
switch(v_Postfix)
{
case 0:
goto LABEL_SemanticCloneableState_60;
break;
case 1:
goto LABEL_SemanticCloneableState_100;
break;
case 2:
goto LABEL_SemanticCloneableState_167;
break;
case 3:
goto LABEL_SemanticCloneableState_211;
break;
case 4:
goto LABEL_SemanticCloneableState_376;
break;
case 5:
goto LABEL_SemanticCloneableState_431;
break;
case 6:
goto LABEL_SemanticCloneableState_488;
break;
case 7:
goto LABEL_SemanticCloneableState_555;
break;
case 8:
goto LABEL_SemanticCloneableState_608;
break;
case 9:
goto LABEL_SemanticCloneableState_1142;
break;
case 10:
goto LABEL_SemanticCloneableState_662;
break;
case 11:
goto LABEL_SemanticCloneableState_711;
break;
case 12:
goto LABEL_SemanticCloneableState_774;
break;
case 13:
goto LABEL_SemanticCloneableState_819;
break;
case 14:
goto LABEL_SemanticCloneableState_869;
break;
case 15:
goto LABEL_SemanticCloneableState_943;
break;
case 16:
goto LABEL_SemanticCloneableState_1058;
break;
}
// JMP 32
// JMP 70
// JMP 135
// JMP 177
// JMP 340
// JMP 393
// JMP 448
// JMP 513
// JMP 564
// JMP 1096
// JMP 614
// JMP 661
// JMP 722
// JMP 765
// JMP 813
// JMP 885
// JMP 998
LABEL_SemanticCloneableState_60:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// LDV v1
l_valueRegister = (asQWORD)(&v_Trace);
// INCi
(**(int32**)&l_valueRegister)++;
// RDR4 v2
v_Postfix = value_read<asDWORD>((void*)l_valueRegister);
// SetV4 v4, 100
v_Eager = 0x64u;
// MULi v2, v2, v4
((int32&)v_Postfix) = ((int32&)v_Postfix) * ((int32&)v_Eager);
// LDV v1
l_valueRegister = (asQWORD)(&v_Trace);
// INCi
(**(int32**)&l_valueRegister)++;
// RDR4 v4
v_Eager = value_read<asDWORD>((void*)l_valueRegister);
// MULIi v4, v4, 10
((int32&)v_Eager) = ((int32&)v_Eager) * value_as<int>((asDWORD)0xau);
// ADDi v5, v2, v4
((int32&)v_TEMP_dword_5) = ((int32&)v_Postfix) + ((int32&)v_Eager);
// LDV v1
l_valueRegister = (asQWORD)(&v_Trace);
// INCi
(**(int32**)&l_valueRegister)++;
// RDR4 v6
v_TEMP_dword_6 = value_read<asDWORD>((void*)l_valueRegister);
// ADDi v4, v5, v6
((int32&)v_Eager) = ((int32&)v_TEMP_dword_5) + ((int32&)v_TEMP_dword_6);
// SUSPEND
// JitEntry *
// MULIi v3, v1, 1000
((int32&)v_TEMP_dword_3) = ((int32&)v_Trace) * value_as<int>((asDWORD)0x3e8u);
// ADDi v6, v3, v4
((int32&)v_TEMP_dword_6) = ((int32&)v_TEMP_dword_3) + ((int32&)v_Eager);
// CpyVtoR4 v6
l_dwordRegister = v_TEMP_dword_6;
// JMP 1049
goto LABEL_SemanticCloneableState_1149;
LABEL_SemanticCloneableState_100:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// CpyVtoR1 v-2
l_byteRegister = p_bLeft;
// JLowNZ 4
if(l_byteRegister != 0) {
goto LABEL_SemanticCloneableState_115;
}
// SetV4 v9, 0
v_TEMP_byte_9 = 0x0u;
// JMP 9
goto LABEL_SemanticCloneableState_124;
LABEL_SemanticCloneableState_115:
// LDV v1
l_valueRegister = (asQWORD)(&v_Trace);
// INCi
(**(int32**)&l_valueRegister)++;
// RDR4 v3
v_TEMP_dword_3 = value_read<asDWORD>((void*)l_valueRegister);
// CMPi v3, v0
  if (((int32&)v_TEMP_dword_3) == ((int32&)p_Seed))     l_byteRegister = 0;
  else if (((int32&)v_TEMP_dword_3) < ((int32&)p_Seed)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// TP
if(((int8&)l_byteRegister) > 0)
   l_byteRegister = 1;
else
   l_byteRegister = 0;
// CpyRtoV4 v8
v_bGate = l_byteRegister;
// CpyVtoV4 v9, v8
v_TEMP_byte_9 = v_bGate;
LABEL_SemanticCloneableState_124:
// CpyVtoR1 v9
l_byteRegister = v_TEMP_byte_9;
// JLowNZ 4
if(l_byteRegister != 0) {
goto LABEL_SemanticCloneableState_131;
}
// SetV4 v8, 0
v_bGate = 0x0u;
// JMP 2
goto LABEL_SemanticCloneableState_133;
LABEL_SemanticCloneableState_131:
// CpyVtoV4 v8, v-3
v_bGate = p_bRight;
LABEL_SemanticCloneableState_133:
// SUSPEND
// JitEntry *
// CpyVtoR1 v8
l_byteRegister = v_bGate;
// JLowZ 17
if(l_byteRegister == 0) {
goto LABEL_SemanticCloneableState_157;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// MULIi v5, v1, 10
((int32&)v_TEMP_dword_5) = ((int32&)v_Trace) * value_as<int>((asDWORD)0xau);
// ADDIi v2, v5, 1
((int32&)v_Postfix) = ((int32&)v_TEMP_dword_5) + value_as<int>((asDWORD)0x1u);
// CpyVtoR4 v2
l_dwordRegister = v_Postfix;
// JMP 992
goto LABEL_SemanticCloneableState_1149;
LABEL_SemanticCloneableState_157:
// SUSPEND
// JitEntry *
// MULIi v2, v1, 10
((int32&)v_Postfix) = ((int32&)v_Trace) * value_as<int>((asDWORD)0xau);
// CpyVtoR4 v2
l_dwordRegister = v_Postfix;
// JMP 982
goto LABEL_SemanticCloneableState_1149;
LABEL_SemanticCloneableState_167:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// ADDIi v1, v1, 3
((int32&)v_Trace) = ((int32&)v_Trace) + value_as<int>((asDWORD)0x3u);
// SUSPEND
// JitEntry *
// LDV v1
l_valueRegister = (asQWORD)(&v_Trace);
// INCi
(**(int32**)&l_valueRegister)++;
// RDR4 v3
v_TEMP_dword_3 = value_read<asDWORD>((void*)l_valueRegister);
// CpyVtoV4 v4, v3
v_Eager = v_TEMP_dword_3;
// SUSPEND
// JitEntry *
// LDV v1
l_valueRegister = (asQWORD)(&v_Trace);
// RDR4 v2
v_Postfix = value_read<asDWORD>((void*)l_valueRegister);
// INCi
(**(int32**)&l_valueRegister)++;
// SUSPEND
// JitEntry *
// MULIi v10, v1, 100
((int32&)v_TEMP_dword_10) = ((int32&)v_Trace) * value_as<int>((asDWORD)0x64u);
// MULIi v5, v4, 10
((int32&)v_TEMP_dword_5) = ((int32&)v_Eager) * value_as<int>((asDWORD)0xau);
// ADDi v3, v10, v5
((int32&)v_TEMP_dword_3) = ((int32&)v_TEMP_dword_10) + ((int32&)v_TEMP_dword_5);
// ADDi v5, v3, v2
((int32&)v_TEMP_dword_5) = ((int32&)v_TEMP_dword_3) + ((int32&)v_Postfix);
// CpyVtoR4 v5
l_dwordRegister = v_TEMP_dword_5;
// JMP 938
goto LABEL_SemanticCloneableState_1149;
LABEL_SemanticCloneableState_211:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v2, 0
v_Postfix = 0x0u;
// SUSPEND
// JitEntry *
LABEL_SemanticCloneableState_225:
// CMPIi v2, 3
  if (((int32&)v_Postfix) == value_as<int>((asDWORD)0x3u))     l_byteRegister = 0;
  else if (((int32&)v_Postfix) < value_as<int>((asDWORD)0x3u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JNS 39
if(((int8&)l_byteRegister) >= 0) {
goto LABEL_SemanticCloneableState_268;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// IncVi v2
v_Postfix++;
// SUSPEND
// JitEntry *
// CMPIi v2, 1
  if (((int32&)v_Postfix) == value_as<int>((asDWORD)0x1u))     l_byteRegister = 0;
  else if (((int32&)v_Postfix) < value_as<int>((asDWORD)0x1u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JNZ 10
if(l_byteRegister != 0) {
goto LABEL_SemanticCloneableState_260;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// JMP -35
goto LABEL_SemanticCloneableState_225;
LABEL_SemanticCloneableState_260:
// SUSPEND
// JitEntry *
// ADDi v1, v1, v2
((int32&)v_Trace) = ((int32&)v_Trace) + ((int32&)v_Postfix);
// JMP -43
goto LABEL_SemanticCloneableState_225;
LABEL_SemanticCloneableState_268:
// SUSPEND
// JitEntry *
// SetV4 v4, 0
v_Eager = 0x0u;
// JMP 85
goto LABEL_SemanticCloneableState_361;
LABEL_SemanticCloneableState_276:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// CMPIi v4, 2
  if (((int32&)v_Eager) == value_as<int>((asDWORD)0x2u))     l_byteRegister = 0;
  else if (((int32&)v_Eager) < value_as<int>((asDWORD)0x2u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JP 36
if(((int8&)l_byteRegister) > 0) {
goto LABEL_SemanticCloneableState_328;
}
// CMPIi v4, 1
  if (((int32&)v_Eager) == value_as<int>((asDWORD)0x1u))     l_byteRegister = 0;
  else if (((int32&)v_Eager) < value_as<int>((asDWORD)0x1u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JZ 6
if(l_byteRegister == 0) {
goto LABEL_SemanticCloneableState_302;
}
// CMPIi v4, 2
  if (((int32&)v_Eager) == value_as<int>((asDWORD)0x2u))     l_byteRegister = 0;
  else if (((int32&)v_Eager) < value_as<int>((asDWORD)0x2u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JZ 15
if(l_byteRegister == 0) {
goto LABEL_SemanticCloneableState_315;
}
// JMP 26
goto LABEL_SemanticCloneableState_328;
LABEL_SemanticCloneableState_302:
// SUSPEND
// JitEntry *
// ADDIi v1, v1, 10
((int32&)v_Trace) = ((int32&)v_Trace) + value_as<int>((asDWORD)0xau);
// SUSPEND
// JitEntry *
// JMP 41
goto LABEL_SemanticCloneableState_356;
LABEL_SemanticCloneableState_315:
// SUSPEND
// JitEntry *
// ADDIi v1, v1, 20
((int32&)v_Trace) = ((int32&)v_Trace) + value_as<int>((asDWORD)0x14u);
// SUSPEND
// JitEntry *
// JMP 10
goto LABEL_SemanticCloneableState_338;
LABEL_SemanticCloneableState_328:
// SUSPEND
// JitEntry *
// ADDi v1, v1, v4
((int32&)v_Trace) = ((int32&)v_Trace) + ((int32&)v_Eager);
// SUSPEND
// JitEntry *
LABEL_SemanticCloneableState_338:
// SUSPEND
// JitEntry *
// CMPIi v4, 2
  if (((int32&)v_Eager) == value_as<int>((asDWORD)0x2u))     l_byteRegister = 0;
  else if (((int32&)v_Eager) < value_as<int>((asDWORD)0x2u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JNZ 10
if(l_byteRegister != 0) {
goto LABEL_SemanticCloneableState_356;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// JMP 13
goto LABEL_SemanticCloneableState_369;
LABEL_SemanticCloneableState_356:
// SUSPEND
// JitEntry *
// IncVi v4
v_Eager++;
LABEL_SemanticCloneableState_361:
// SUSPEND
// JitEntry *
// CMPIi v4, 4
  if (((int32&)v_Eager) == value_as<int>((asDWORD)0x4u))     l_byteRegister = 0;
  else if (((int32&)v_Eager) < value_as<int>((asDWORD)0x4u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JS -93
if(((int8&)l_byteRegister) < 0) {
goto LABEL_SemanticCloneableState_276;
}
LABEL_SemanticCloneableState_369:
// SUSPEND
// JitEntry *
// CpyVtoR4 v1
l_dwordRegister = v_Trace;
// JMP 773
goto LABEL_SemanticCloneableState_1149;
LABEL_SemanticCloneableState_376:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v4, 0
v_Eager = 0x0u;
// SUSPEND
// JitEntry *
// SetV4 v5, 10
v_TEMP_dword_5 = 0xau;
// DIVi v10, v5, v0
{
  int divider = ((int32&)p_Seed);
  int operand = ((int32&)v_TEMP_dword_5);
  if (divider == 0) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(194, 3);
     SCRIPT_DIV0_EXCEPTION();
return {};
  }
  else if (divider == -1 && operand == int(0x80000000)) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(194, 3);
     SCRIPT_OVERFLOW_EXCEPTION();
return {};
  }
((int32&)v_TEMP_dword_10) = operand / divider;
}
// LDV v4
l_valueRegister = (asQWORD)(&v_Eager);
// INCi
(**(int32**)&l_valueRegister)++;
// RDR4 v3
v_TEMP_dword_3 = value_read<asDWORD>((void*)l_valueRegister);
// CMPi v10, v3
  if (((int32&)v_TEMP_dword_10) == ((int32&)v_TEMP_dword_3))     l_byteRegister = 0;
  else if (((int32&)v_TEMP_dword_10) < ((int32&)v_TEMP_dword_3)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JNP 21
if(((int8&)l_byteRegister) <= 0) {
goto LABEL_SemanticCloneableState_422;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// ADDIi v1, v1, 1
((int32&)v_Trace) = ((int32&)v_Trace) + value_as<int>((asDWORD)0x1u);
// SUSPEND
// JitEntry *
// JMP 0
goto LABEL_SemanticCloneableState_422;
LABEL_SemanticCloneableState_422:
// SUSPEND
// JitEntry *
// ADDi v5, v1, v4
((int32&)v_TEMP_dword_5) = ((int32&)v_Trace) + ((int32&)v_Eager);
// CpyVtoR4 v5
l_dwordRegister = v_TEMP_dword_5;
// JMP 718
goto LABEL_SemanticCloneableState_1149;
LABEL_SemanticCloneableState_431:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v2, 0
v_Postfix = 0x0u;
// JMP 30
goto LABEL_SemanticCloneableState_473;
LABEL_SemanticCloneableState_443:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v5, 10
v_TEMP_dword_5 = 0xau;
// DIVi v10, v5, v0
{
  int divider = ((int32&)p_Seed);
  int operand = ((int32&)v_TEMP_dword_5);
  if (divider == 0) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(205, 4);
     SCRIPT_DIV0_EXCEPTION();
return {};
  }
  else if (divider == -1 && operand == int(0x80000000)) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(205, 4);
     SCRIPT_OVERFLOW_EXCEPTION();
return {};
  }
((int32&)v_TEMP_dword_10) = operand / divider;
}
// ADDi v1, v1, v10
((int32&)v_Trace) = ((int32&)v_Trace) + ((int32&)v_TEMP_dword_10);
// SUSPEND
// JitEntry *
// ADDIi v1, v1, 100
((int32&)v_Trace) = ((int32&)v_Trace) + value_as<int>((asDWORD)0x64u);
// SUSPEND
// JitEntry *
// IncVi v2
v_Postfix++;
LABEL_SemanticCloneableState_473:
// SUSPEND
// JitEntry *
// CMPIi v2, 1
  if (((int32&)v_Postfix) == value_as<int>((asDWORD)0x1u))     l_byteRegister = 0;
  else if (((int32&)v_Postfix) < value_as<int>((asDWORD)0x1u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JS -38
if(((int8&)l_byteRegister) < 0) {
goto LABEL_SemanticCloneableState_443;
}
// SUSPEND
// JitEntry *
// CpyVtoR4 v1
l_dwordRegister = v_Trace;
// JMP 661
goto LABEL_SemanticCloneableState_1149;
LABEL_SemanticCloneableState_488:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v4, 0
v_Eager = 0x0u;
// SUSPEND
// JitEntry *
// SetV4 v2, 0
v_Postfix = 0x0u;
// JMP 32
goto LABEL_SemanticCloneableState_538;
LABEL_SemanticCloneableState_506:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// IncVi v2
v_Postfix++;
// SUSPEND
// JitEntry *
// JMP 0
goto LABEL_SemanticCloneableState_525;
LABEL_SemanticCloneableState_525:
// SUSPEND
// JitEntry *
// SetV4 v5, 10
v_TEMP_dword_5 = 0xau;
// DIVi v2, v5, v0
{
  int divider = ((int32&)p_Seed);
  int operand = ((int32&)v_TEMP_dword_5);
  if (divider == 0) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(214, 4);
     SCRIPT_DIV0_EXCEPTION();
return {};
  }
  else if (divider == -1 && operand == int(0x80000000)) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(214, 4);
     SCRIPT_OVERFLOW_EXCEPTION();
return {};
  }
((int32&)v_Postfix) = operand / divider;
}
// SUSPEND
// JitEntry *
// IncVi v4
v_Eager++;
LABEL_SemanticCloneableState_538:
// SUSPEND
// JitEntry *
// CMPIi v2, 1
  if (((int32&)v_Postfix) == value_as<int>((asDWORD)0x1u))     l_byteRegister = 0;
  else if (((int32&)v_Postfix) < value_as<int>((asDWORD)0x1u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JS -40
if(((int8&)l_byteRegister) < 0) {
goto LABEL_SemanticCloneableState_506;
}
// SUSPEND
// JitEntry *
// ADDi v5, v1, v4
((int32&)v_TEMP_dword_5) = ((int32&)v_Trace) + ((int32&)v_Eager);
// CpyVtoR4 v5
l_dwordRegister = v_TEMP_dword_5;
// JMP 594
goto LABEL_SemanticCloneableState_1149;
LABEL_SemanticCloneableState_555:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v6, 10
v_TEMP_dword_6 = 0xau;
// DIVi v3, v6, v0
{
  int divider = ((int32&)p_Seed);
  int operand = ((int32&)v_TEMP_dword_6);
  if (divider == 0) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(223, 3);
     SCRIPT_DIV0_EXCEPTION();
return {};
  }
  else if (divider == -1 && operand == int(0x80000000)) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(223, 3);
     SCRIPT_OVERFLOW_EXCEPTION();
return {};
  }
((int32&)v_TEMP_dword_3) = operand / divider;
}
// CMPIi v3, 0
  if (((int32&)v_TEMP_dword_3) == value_as<int>((asDWORD)0x0u))     l_byteRegister = 0;
  else if (((int32&)v_TEMP_dword_3) < value_as<int>((asDWORD)0x0u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JP 19
if(((int8&)l_byteRegister) > 0) {
goto LABEL_SemanticCloneableState_590;
}
// CMPIi v3, 0
  if (((int32&)v_TEMP_dword_3) == value_as<int>((asDWORD)0x0u))     l_byteRegister = 0;
  else if (((int32&)v_TEMP_dword_3) < value_as<int>((asDWORD)0x0u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JZ 2
if(l_byteRegister == 0) {
goto LABEL_SemanticCloneableState_577;
}
// JMP 13
goto LABEL_SemanticCloneableState_590;
LABEL_SemanticCloneableState_577:
// SUSPEND
// JitEntry *
// ADDIi v1, v1, 1
((int32&)v_Trace) = ((int32&)v_Trace) + value_as<int>((asDWORD)0x1u);
// SUSPEND
// JitEntry *
// JMP 11
goto LABEL_SemanticCloneableState_601;
LABEL_SemanticCloneableState_590:
// SUSPEND
// JitEntry *
// ADDIi v1, v1, 2
((int32&)v_Trace) = ((int32&)v_Trace) + value_as<int>((asDWORD)0x2u);
// SUSPEND
// JitEntry *
LABEL_SemanticCloneableState_601:
// SUSPEND
// JitEntry *
// CpyVtoR4 v1
l_dwordRegister = v_Trace;
// JMP 541
goto LABEL_SemanticCloneableState_1149;
LABEL_SemanticCloneableState_608:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v2, 0
v_Postfix = 0x0u;
// SUSPEND
// JitEntry *
LABEL_SemanticCloneableState_622:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// IncVi v2
v_Postfix++;
// SUSPEND
// JitEntry *
// JMP 0
goto LABEL_SemanticCloneableState_637;
LABEL_SemanticCloneableState_637:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v5, 10
v_TEMP_dword_5 = 0xau;
// DIVi v3, v5, v0
{
  int divider = ((int32&)p_Seed);
  int operand = ((int32&)v_TEMP_dword_5);
  if (divider == 0) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(242, 10);
     SCRIPT_DIV0_EXCEPTION();
return {};
  }
  else if (divider == -1 && operand == int(0x80000000)) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(242, 10);
     SCRIPT_OVERFLOW_EXCEPTION();
return {};
  }
((int32&)v_TEMP_dword_3) = operand / divider;
}
// CMPi v3, v2
  if (((int32&)v_TEMP_dword_3) == ((int32&)v_Postfix))     l_byteRegister = 0;
  else if (((int32&)v_TEMP_dword_3) < ((int32&)v_Postfix)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JP -31
if(((int8&)l_byteRegister) > 0) {
goto LABEL_SemanticCloneableState_622;
}
// SUSPEND
// JitEntry *
// ADDi v3, v1, v2
((int32&)v_TEMP_dword_3) = ((int32&)v_Trace) + ((int32&)v_Postfix);
// CpyVtoR4 v3
l_dwordRegister = v_TEMP_dword_3;
// JMP 487
goto LABEL_SemanticCloneableState_1149;
LABEL_SemanticCloneableState_662:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
LABEL_SemanticCloneableState_670:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v5, 10
v_TEMP_dword_5 = 0xau;
// DIVi v6, v5, v0
{
  int divider = ((int32&)p_Seed);
  int operand = ((int32&)v_TEMP_dword_5);
  if (divider == 0) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(249, 4);
     SCRIPT_DIV0_EXCEPTION();
return {};
  }
  else if (divider == -1 && operand == int(0x80000000)) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(249, 4);
     SCRIPT_OVERFLOW_EXCEPTION();
return {};
  }
((int32&)v_TEMP_dword_6) = operand / divider;
}
// ADDi v1, v1, v6
((int32&)v_Trace) = ((int32&)v_Trace) + ((int32&)v_TEMP_dword_6);
// SUSPEND
// JitEntry *
// ADDIi v1, v1, 100
((int32&)v_Trace) = ((int32&)v_Trace) + value_as<int>((asDWORD)0x64u);
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV1 v7, 0
v_TEMP_byte_7 = 0x0u;
// CpyVtoR1 v7
l_byteRegister = v_TEMP_byte_7;
// JLowNZ -34
if(l_byteRegister != 0) {
goto LABEL_SemanticCloneableState_670;
}
// SUSPEND
// JitEntry *
// CpyVtoR4 v1
l_dwordRegister = v_Trace;
// JMP 438
goto LABEL_SemanticCloneableState_1149;
LABEL_SemanticCloneableState_711:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v2, 0
v_Postfix = 0x0u;
// SUSPEND
// JitEntry *
LABEL_SemanticCloneableState_725:
// CMPIi v2, 0
  if (((int32&)v_Postfix) == value_as<int>((asDWORD)0x0u))     l_byteRegister = 0;
  else if (((int32&)v_Postfix) < value_as<int>((asDWORD)0x0u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JNZ 4
if(l_byteRegister != 0) {
goto LABEL_SemanticCloneableState_733;
}
// SetV1 v9, 1
v_TEMP_byte_9 = 0x1u;
// JMP 10
goto LABEL_SemanticCloneableState_743;
LABEL_SemanticCloneableState_733:
// SetV4 v5, 10
v_TEMP_dword_5 = 0xau;
// DIVi v10, v5, v0
{
  int divider = ((int32&)p_Seed);
  int operand = ((int32&)v_TEMP_dword_5);
  if (divider == 0) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(258, 3);
     SCRIPT_DIV0_EXCEPTION();
return {};
  }
  else if (divider == -1 && operand == int(0x80000000)) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(258, 3);
     SCRIPT_OVERFLOW_EXCEPTION();
return {};
  }
((int32&)v_TEMP_dword_10) = operand / divider;
}
// CMPi v10, v2
  if (((int32&)v_TEMP_dword_10) == ((int32&)v_Postfix))     l_byteRegister = 0;
  else if (((int32&)v_TEMP_dword_10) < ((int32&)v_Postfix)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// TP
if(((int8&)l_byteRegister) > 0)
   l_byteRegister = 1;
else
   l_byteRegister = 0;
// CpyRtoV4 v7
v_TEMP_byte_7 = l_byteRegister;
// CpyVtoV4 v9, v7
v_TEMP_byte_9 = v_TEMP_byte_7;
LABEL_SemanticCloneableState_743:
// CpyVtoR1 v9
l_byteRegister = v_TEMP_byte_9;
// JLowZ 19
if(l_byteRegister == 0) {
goto LABEL_SemanticCloneableState_765;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// IncVi v2
v_Postfix++;
// SUSPEND
// JitEntry *
// JMP -40
goto LABEL_SemanticCloneableState_725;
LABEL_SemanticCloneableState_765:
// SUSPEND
// JitEntry *
// ADDi v5, v1, v2
((int32&)v_TEMP_dword_5) = ((int32&)v_Trace) + ((int32&)v_Postfix);
// CpyVtoR4 v5
l_dwordRegister = v_TEMP_dword_5;
// JMP 375
goto LABEL_SemanticCloneableState_1149;
LABEL_SemanticCloneableState_774:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v4, 0
v_Eager = 0x0u;
// JMP 14
goto LABEL_SemanticCloneableState_800;
LABEL_SemanticCloneableState_786:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// JMP 12
goto LABEL_SemanticCloneableState_812;
LABEL_SemanticCloneableState_800:
// SUSPEND
// JitEntry *
// SetV4 v5, 10
v_TEMP_dword_5 = 0xau;
// DIVi v10, v5, v0
{
  int divider = ((int32&)p_Seed);
  int operand = ((int32&)v_TEMP_dword_5);
  if (divider == 0) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(267, 23);
     SCRIPT_DIV0_EXCEPTION();
return {};
  }
  else if (divider == -1 && operand == int(0x80000000)) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(267, 23);
     SCRIPT_OVERFLOW_EXCEPTION();
return {};
  }
((int32&)v_TEMP_dword_10) = operand / divider;
}
// CMPi v10, v4
  if (((int32&)v_TEMP_dword_10) == ((int32&)v_Eager))     l_byteRegister = 0;
  else if (((int32&)v_TEMP_dword_10) < ((int32&)v_Eager)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JP -26
if(((int8&)l_byteRegister) > 0) {
goto LABEL_SemanticCloneableState_786;
}
LABEL_SemanticCloneableState_812:
// SUSPEND
// JitEntry *
// CpyVtoR4 v1
l_dwordRegister = v_Trace;
// JMP 330
goto LABEL_SemanticCloneableState_1149;
LABEL_SemanticCloneableState_819:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// CMPi v1, v0
  if (((int32&)v_Trace) == ((int32&)p_Seed))     l_byteRegister = 0;
  else if (((int32&)v_Trace) < ((int32&)p_Seed)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JNZ 31
if(l_byteRegister != 0) {
goto LABEL_SemanticCloneableState_862;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v10, 10
v_TEMP_dword_10 = 0xau;
// DIVi v5, v10, v0
{
  int divider = ((int32&)p_Seed);
  int operand = ((int32&)v_TEMP_dword_10);
  if (divider == 0) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(277, 4);
     SCRIPT_DIV0_EXCEPTION();
return {};
  }
  else if (divider == -1 && operand == int(0x80000000)) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(277, 4);
     SCRIPT_OVERFLOW_EXCEPTION();
return {};
  }
((int32&)v_TEMP_dword_5) = operand / divider;
}
// ADDi v1, v1, v5
((int32&)v_Trace) = ((int32&)v_Trace) + ((int32&)v_TEMP_dword_5);
// SUSPEND
// JitEntry *
// ADDIi v1, v1, 100
((int32&)v_Trace) = ((int32&)v_Trace) + value_as<int>((asDWORD)0x64u);
// SUSPEND
// JitEntry *
// JMP 0
goto LABEL_SemanticCloneableState_862;
LABEL_SemanticCloneableState_862:
// SUSPEND
// JitEntry *
// CpyVtoR4 v1
l_dwordRegister = v_Trace;
// JMP 280
goto LABEL_SemanticCloneableState_1149;
LABEL_SemanticCloneableState_869:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v4, 0
v_Eager = 0x0u;
// SUSPEND
// JitEntry *
LABEL_SemanticCloneableState_883:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// IncVi v4
v_Eager++;
// SUSPEND
// JitEntry *
// CMPIi v4, 1
  if (((int32&)v_Eager) == value_as<int>((asDWORD)0x1u))     l_byteRegister = 0;
  else if (((int32&)v_Eager) < value_as<int>((asDWORD)0x1u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JNZ 10
if(l_byteRegister != 0) {
goto LABEL_SemanticCloneableState_910;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// JMP 9
goto LABEL_SemanticCloneableState_919;
LABEL_SemanticCloneableState_910:
// SUSPEND
// JitEntry *
// MULIi v10, v4, 10
((int32&)v_TEMP_dword_10) = ((int32&)v_Eager) * value_as<int>((asDWORD)0xau);
// ADDi v1, v1, v10
((int32&)v_Trace) = ((int32&)v_Trace) + ((int32&)v_TEMP_dword_10);
LABEL_SemanticCloneableState_919:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// CMPIi v4, 3
  if (((int32&)v_Eager) == value_as<int>((asDWORD)0x3u))     l_byteRegister = 0;
  else if (((int32&)v_Eager) < value_as<int>((asDWORD)0x3u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JS -48
if(((int8&)l_byteRegister) < 0) {
goto LABEL_SemanticCloneableState_883;
}
// SUSPEND
// JitEntry *
// MULIi v3, v1, 10
((int32&)v_TEMP_dword_3) = ((int32&)v_Trace) * value_as<int>((asDWORD)0xau);
// ADDi v10, v3, v4
((int32&)v_TEMP_dword_10) = ((int32&)v_TEMP_dword_3) + ((int32&)v_Eager);
// CpyVtoR4 v10
l_dwordRegister = v_TEMP_dword_10;
// JMP 206
goto LABEL_SemanticCloneableState_1149;
LABEL_SemanticCloneableState_943:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// CMPIi v0, 0
  if (((int32&)p_Seed) == value_as<int>((asDWORD)0x0u))     l_byteRegister = 0;
  else if (((int32&)p_Seed) < value_as<int>((asDWORD)0x0u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JNS 13
if(((int8&)l_byteRegister) >= 0) {
goto LABEL_SemanticCloneableState_968;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v3, -10
v_TEMP_dword_3 = 0xfffffff6u;
// CpyVtoR4 v3
l_dwordRegister = v_TEMP_dword_3;
// JMP 181
goto LABEL_SemanticCloneableState_1149;
LABEL_SemanticCloneableState_968:
// SUSPEND
// JitEntry *
// CMPIi v0, 0
  if (((int32&)p_Seed) == value_as<int>((asDWORD)0x0u))     l_byteRegister = 0;
  else if (((int32&)p_Seed) < value_as<int>((asDWORD)0x0u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JNZ 13
if(l_byteRegister != 0) {
goto LABEL_SemanticCloneableState_989;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v5, 0
v_TEMP_dword_5 = 0x0u;
// CpyVtoR4 v5
l_dwordRegister = v_TEMP_dword_5;
// JMP 160
goto LABEL_SemanticCloneableState_1149;
LABEL_SemanticCloneableState_989:
// SUSPEND
// JitEntry *
// CpyVtoR1 v-2
l_byteRegister = p_bLeft;
// JLowZ 33
if(l_byteRegister == 0) {
goto LABEL_SemanticCloneableState_1029;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// CpyVtoR1 v-3
l_byteRegister = p_bRight;
// JLowZ 13
if(l_byteRegister == 0) {
goto LABEL_SemanticCloneableState_1020;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v3, 11
v_TEMP_dword_3 = 0xbu;
// CpyVtoR4 v3
l_dwordRegister = v_TEMP_dword_3;
// JMP 129
goto LABEL_SemanticCloneableState_1149;
LABEL_SemanticCloneableState_1020:
// SUSPEND
// JitEntry *
// SetV4 v5, 12
v_TEMP_dword_5 = 0xcu;
// CpyVtoR4 v5
l_dwordRegister = v_TEMP_dword_5;
// JMP 120
goto LABEL_SemanticCloneableState_1149;
LABEL_SemanticCloneableState_1029:
// SUSPEND
// JitEntry *
// CpyVtoR1 v-3
l_byteRegister = p_bRight;
// JLowZ 13
if(l_byteRegister == 0) {
goto LABEL_SemanticCloneableState_1049;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v3, 13
v_TEMP_dword_3 = 0xdu;
// CpyVtoR4 v3
l_dwordRegister = v_TEMP_dword_3;
// JMP 100
goto LABEL_SemanticCloneableState_1149;
LABEL_SemanticCloneableState_1049:
// SUSPEND
// JitEntry *
// SetV4 v5, 14
v_TEMP_dword_5 = 0xeu;
// CpyVtoR4 v5
l_dwordRegister = v_TEMP_dword_5;
// JMP 91
goto LABEL_SemanticCloneableState_1149;
LABEL_SemanticCloneableState_1058:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// CMPIi v0, 1
  if (((int32&)p_Seed) == value_as<int>((asDWORD)0x1u))     l_byteRegister = 0;
  else if (((int32&)p_Seed) < value_as<int>((asDWORD)0x1u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JP 43
if(((int8&)l_byteRegister) > 0) {
goto LABEL_SemanticCloneableState_1113;
}
// CMPIi v0, 0
  if (((int32&)p_Seed) == value_as<int>((asDWORD)0x0u))     l_byteRegister = 0;
  else if (((int32&)p_Seed) < value_as<int>((asDWORD)0x0u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JZ 6
if(l_byteRegister == 0) {
goto LABEL_SemanticCloneableState_1080;
}
// CMPIi v0, 1
  if (((int32&)p_Seed) == value_as<int>((asDWORD)0x1u))     l_byteRegister = 0;
  else if (((int32&)p_Seed) < value_as<int>((asDWORD)0x1u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JZ 13
if(l_byteRegister == 0) {
goto LABEL_SemanticCloneableState_1091;
}
// JMP 33
goto LABEL_SemanticCloneableState_1113;
LABEL_SemanticCloneableState_1080:
// SUSPEND
// JitEntry *
// ADDIi v1, v1, 1
((int32&)v_Trace) = ((int32&)v_Trace) + value_as<int>((asDWORD)0x1u);
// SUSPEND
// JitEntry *
LABEL_SemanticCloneableState_1091:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v4, 20
v_Eager = 0x14u;
// SUSPEND
// JitEntry *
// ADDi v1, v1, v4
((int32&)v_Trace) = ((int32&)v_Trace) + ((int32&)v_Eager);
// SUSPEND
// JitEntry *
// JMP 22
goto LABEL_SemanticCloneableState_1135;
LABEL_SemanticCloneableState_1113:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v2, 30
v_Postfix = 0x1eu;
// SUSPEND
// JitEntry *
// ADDi v1, v1, v2
((int32&)v_Trace) = ((int32&)v_Trace) + ((int32&)v_Postfix);
// SUSPEND
// JitEntry *
// JMP 0
goto LABEL_SemanticCloneableState_1135;
LABEL_SemanticCloneableState_1135:
// SUSPEND
// JitEntry *
// CpyVtoR4 v1
l_dwordRegister = v_Trace;
// JMP 7
goto LABEL_SemanticCloneableState_1149;
LABEL_SemanticCloneableState_1142:
// SUSPEND
// JitEntry *
// SetV4 v3, -1
v_TEMP_dword_3 = 0xffffffffu;
// CpyVtoR4 v3
l_dwordRegister = v_TEMP_dword_3;
LABEL_SemanticCloneableState_1149:
// RET 4
  return (int32)l_dwordRegister;
}
// AS Function : int SemanticCloneableState(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:122:1
// JIT Entry   : VM
void AS_Test_Differential_Bytecode_SemanticCloneableState_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_Test_Differential_Bytecode_SemanticCloneableState(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1),
		*(asBYTE*)(l_fp + 2),
		*(asBYTE*)(l_fp + 3));
}

namespace
{
	struct FBytecodeJITDifferentialEntryRegistration
	{
		FBytecodeJITDifferentialEntryRegistration()
		{
			AngelscriptTestJITProbes::RegisterIsolatedDifferentialEntry(
				AngelscriptTestJITProbes::EIsolatedDifferentialBackend::BytecodeJIT,
				&AS_Test_Differential_Bytecode_SemanticCloneableState_VMEntry,
				reinterpret_cast<asJITFunction_Raw>(&AS_Test_Differential_Bytecode_SemanticCloneableState),
				nullptr,
				TEXT("AS_Test_Differential_Bytecode_SemanticCloneableState"),
				TEXT("169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855"),
				TEXT("int SemanticCloneableState(const int, const int, const bool, const bool)"),
				TEXT("/Angelscript/Game/ASStaticJITTypedControlFlowFixture.as"),
				122u,
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
