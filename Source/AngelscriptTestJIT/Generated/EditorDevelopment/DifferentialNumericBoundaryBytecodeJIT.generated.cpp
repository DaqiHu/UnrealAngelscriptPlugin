// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : BytecodeJIT
 * Declaration        : int SemanticNumericBoundary(const int, const int, const int, const int)
 * StableFunctionKey  : de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e
 * OriginalSymbolSet  : ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_58539743e28e50faa5d5ee3e50261836e051b1dd0a7d68b805bab6072fd2d80a
 * TestSymbolSet      : AS_Test_Differential_Bytecode_SemanticNumericBoundary
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
 * Declaration       : int SemanticNumericBoundary(const int, const int, const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:2
 * StableFunctionKey  : de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e
 * ExecutionHash      : 58539743e28e50faa5d5ee3e50261836e051b1dd0a7d68b805bab6072fd2d80a
 * DebugHash          : 33ab9e7722bfe134d36a9e683a832e75cb1a6a5322a13ab18e7bd23d86aea50f
 * EntryAbiHash       : ce69b03c30f4aba54525c218c4551a91f6dfe1f289fefa74e6e6ea01b1bab739
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_AS_Test_Differential_Bytecode_SemanticNumericBoundary = "ASStaticJITTypedFinalScalarFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_AS_Test_Differential_Bytecode_SemanticNumericBoundary
#endif

// AS Function : int SemanticNumericBoundary(const int, const int, const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:2
// JIT Entry   : Raw
int32 AS_Test_Differential_Bytecode_SemanticNumericBoundary(FScriptExecution& Execution, UObject* l_This, asDWORD p_Left, asDWORD p_Right, asDWORD p_ShiftCount, asDWORD p_Mode)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UTypedASTFinalScalarReceiver::SemanticNumericBoundary(int, int, int, int)", 32);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asQWORD v_Wide = {};
asDWORD v_TEMP_dword_1 = {};
asQWORD v_TEMP_qword_4 = {};
asBYTE v_TEMP_byte_7 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// CMPIi v-5, 10
  if (((int32&)p_Mode) == value_as<int>((asDWORD)0xau))     l_byteRegister = 0;
  else if (((int32&)p_Mode) < value_as<int>((asDWORD)0xau)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JP 183
if(((int8&)l_byteRegister) > 0) {
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_194;
}
// CMPIi v-5, 0
  if (((int32&)p_Mode) == value_as<int>((asDWORD)0x0u))     l_byteRegister = 0;
  else if (((int32&)p_Mode) < value_as<int>((asDWORD)0x0u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JS 179
if(((int8&)l_byteRegister) < 0) {
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_194;
}
// SUBIi v1, v-5, 0
((int32&)v_TEMP_dword_1) = ((int32&)p_Mode) - value_as<int>((asDWORD)0x0u);
// JMPP v1, 10
switch(v_TEMP_dword_1)
{
case 0:
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_42;
break;
case 1:
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_51;
break;
case 2:
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_60;
break;
case 3:
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_69;
break;
case 4:
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_78;
break;
case 5:
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_87;
break;
case 6:
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_96;
break;
case 7:
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_105;
break;
case 8:
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_114;
break;
case 9:
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_154;
break;
case 10:
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_184;
break;
}
// JMP 20
// JMP 27
// JMP 34
// JMP 41
// JMP 48
// JMP 55
// JMP 62
// JMP 69
// JMP 76
// JMP 114
// JMP 142
LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_42:
// SUSPEND
// JitEntry *
// ADDi v1, v-2, v-3
((int32&)v_TEMP_dword_1) = ((int32&)p_Left) + ((int32&)p_Right);
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// JMP 150
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_201;
LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_51:
// SUSPEND
// JitEntry *
// SUBi v1, v-2, v-3
((int32&)v_TEMP_dword_1) = ((int32&)p_Left) - ((int32&)p_Right);
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// JMP 141
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_201;
LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_60:
// SUSPEND
// JitEntry *
// MULi v1, v-2, v-3
((int32&)v_TEMP_dword_1) = ((int32&)p_Left) * ((int32&)p_Right);
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// JMP 132
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_201;
LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_69:
// SUSPEND
// JitEntry *
// DIVi v1, v-2, v-3
{
  int divider = ((int32&)p_Right);
  int operand = ((int32&)p_Left);
  if (divider == 0) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(41, 4);
     SCRIPT_DIV0_EXCEPTION();
return {};
  }
  else if (divider == -1 && operand == int(0x80000000)) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(41, 4);
     SCRIPT_OVERFLOW_EXCEPTION();
return {};
  }
((int32&)v_TEMP_dword_1) = operand / divider;
}
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// JMP 123
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_201;
LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_78:
// SUSPEND
// JitEntry *
// MODi v1, v-2, v-3
{
  int divider = ((int32&)p_Right);
  int operand = ((int32&)p_Left);
  if (divider == 0) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(43, 4);
     SCRIPT_DIV0_EXCEPTION();
return {};
  }
  else if (divider == -1 && operand == int(0x80000000)) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(43, 4);
     SCRIPT_OVERFLOW_EXCEPTION();
return {};
  }
((int32&)v_TEMP_dword_1) = operand % divider;
}
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// JMP 114
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_201;
LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_87:
// SUSPEND
// JitEntry *
// BSLL v1, v-2, v-4
v_TEMP_dword_1 = p_Left << p_ShiftCount;
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// JMP 105
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_201;
LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_96:
// SUSPEND
// JitEntry *
// BSRL v1, v-2, v-4
v_TEMP_dword_1 = p_Left >> p_ShiftCount;
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// JMP 96
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_201;
LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_105:
// SUSPEND
// JitEntry *
// BSRA v1, v-2, v-4
v_TEMP_dword_1 = ((int32&)p_Left) >> p_ShiftCount;
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// JMP 87
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_201;
LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_114:
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// iTOi64 v6, v-2
((int64&)v_Wide) = int64(((int32&)p_Left));
// SUSPEND
// JitEntry *
// iTOi64 v4, v-2
((int64&)v_TEMP_qword_4) = int64(((int32&)p_Left));
// CMPi64 v6, v4
  if (((int64&)v_Wide) == ((int64&)v_TEMP_qword_4))     l_byteRegister = 0;
  else if (((int64&)v_Wide) < ((int64&)v_TEMP_qword_4)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JNZ 11
if(l_byteRegister != 0) {
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_145;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// CpyVtoR4 v-2
l_dwordRegister = p_Left;
// JMP 56
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_201;
LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_145:
// SUSPEND
// JitEntry *
// SetV4 v1, 0
v_TEMP_dword_1 = 0x0u;
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// JMP 47
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_201;
LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_154:
// SUSPEND
// JitEntry *
// CMPIi v-2, 0
  if (((int32&)p_Left) == value_as<int>((asDWORD)0x0u))     l_byteRegister = 0;
  else if (((int32&)p_Left) < value_as<int>((asDWORD)0x0u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JZ 13
if(l_byteRegister == 0) {
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_175;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v1, 1
v_TEMP_dword_1 = 0x1u;
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// JMP 26
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_201;
LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_175:
// SUSPEND
// JitEntry *
// SetV4 v1, 0
v_TEMP_dword_1 = 0x0u;
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// JMP 17
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_201;
LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_184:
// SUSPEND
// JitEntry *
// ADDIi v1, v-2, 7001
((int32&)v_TEMP_dword_1) = ((int32&)p_Left) + value_as<int>((asDWORD)0x1b59u);
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// JMP 7
goto LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_201;
LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_194:
// SUSPEND
// JitEntry *
// SetV4 v1, -1
v_TEMP_dword_1 = 0xffffffffu;
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
LABEL_UTypedASTFinalScalarReceiver__SemanticNumericBoundary_201:
// RET 6
  return (int32)l_dwordRegister;
}
// AS Function : int SemanticNumericBoundary(const int, const int, const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:2
// JIT Entry   : VM
void AS_Test_Differential_Bytecode_SemanticNumericBoundary_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_Test_Differential_Bytecode_SemanticNumericBoundary(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2),
		*(asDWORD*)(l_fp + 3),
		*(asDWORD*)(l_fp + 4),
		*(asDWORD*)(l_fp + 5));
}
// AS Function : int SemanticNumericBoundary(const int, const int, const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:2
// JIT Entry   : Parms
void AS_Test_Differential_Bytecode_SemanticNumericBoundary_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Left = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1_Right = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_2_ShiftCount = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_3_Mode = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_Test_Differential_Bytecode_SemanticNumericBoundary(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Left),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_1_Right),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_2_ShiftCount),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_3_Mode));
}

namespace
{
	static const FAngelscriptJITReferenceSlot FBytecodeJITDifferentialEntryRegistrationReferenceSlots[2] =
	{
		{sizeof(FAngelscriptJITReferenceSlot), 0u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(2), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("fad89b1b314e72fddfda397e555d92b3e881dce5158330451090697253e7b4e8")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("85b780d3d0320f0320f21f66806418c53611e2e04d160beb79f3c8a598f7a9bd")))}}},
		{sizeof(FAngelscriptJITReferenceSlot), 1u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(4), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("a5da6844fd295dd05d5a22cf9b50196a15b6cea4f30ae1fdebd1d0bac4766088")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("e7ef2b212521fee9013b2a9d94e54b7c7f48664d236c66b55651dc822f29b690")))}}},
	};

	struct FBytecodeJITDifferentialEntryRegistration
	{
		FBytecodeJITDifferentialEntryRegistration()
		{
			AngelscriptTestJITProbes::RegisterIsolatedDifferentialEntry(
				AngelscriptTestJITProbes::EIsolatedDifferentialBackend::BytecodeJIT,
				&AS_Test_Differential_Bytecode_SemanticNumericBoundary_VMEntry,
				reinterpret_cast<asJITFunction_Raw>(&AS_Test_Differential_Bytecode_SemanticNumericBoundary),
				&AS_Test_Differential_Bytecode_SemanticNumericBoundary_ParmsEntry,
				TEXT("AS_Test_Differential_Bytecode_SemanticNumericBoundary"),
				TEXT("de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e"),
				TEXT("int SemanticNumericBoundary(const int, const int, const int, const int)"),
				TEXT("/Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as"),
				26u,
				2u,
				AngelscriptTestJITProbes::EIsolatedDifferentialCleanupPlanState::NotCaptured,
				0u,
				FBytecodeJITDifferentialEntryRegistrationReferenceSlots,
				2u);
		}
	};

	FBytecodeJITDifferentialEntryRegistration GBytecodeJITDifferentialEntryRegistration;
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
