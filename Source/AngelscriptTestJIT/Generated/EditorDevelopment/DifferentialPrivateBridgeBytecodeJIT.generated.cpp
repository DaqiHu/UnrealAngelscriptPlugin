// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : BytecodeJIT
 * Declaration        : int TypedASTPrivateBridgeShowcase(const int, const int)
 * StableFunctionKey  : 6608a6a94c2205850a5fb4a8b02ab7c08d63ec5722236ff400d6d9d50cd0625e
 * OriginalSymbolSet  : ASJIT_6608a6a94c2205850a5fb4a8b02ab7c08d63ec5722236ff400d6d9d50cd0625e_f26f01e02fe68450bf38f453b9b8c0dccde619755bb1cebb3c6825c38f148fd1
 * TestSymbolSet      : AS_Test_Differential_Bytecode_TypedASTPrivateBridgeShowcase
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
 * Declaration       : int TypedASTPrivateBridgeShowcase(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:2:1
 * StableFunctionKey  : 6608a6a94c2205850a5fb4a8b02ab7c08d63ec5722236ff400d6d9d50cd0625e
 * ExecutionHash      : f26f01e02fe68450bf38f453b9b8c0dccde619755bb1cebb3c6825c38f148fd1
 * DebugHash          : 85a5bdbe8d14c58398fc7a11eb45675f1e7584d0d5677b4c8c09f8ed282af10d
 * EntryAbiHash       : 50f6395fc30c9f2e22debd06bd6a0eb7177ae1bcab24fbbab419e6ea7e1a6e2e
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_AS_Test_Differential_Bytecode_TypedASTPrivateBridgeShowcase = "ASStaticJITTypedPrivateBridgeFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_AS_Test_Differential_Bytecode_TypedASTPrivateBridgeShowcase
#endif

// AS Function : int TypedASTPrivateBridgeShowcase(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:2:1
// JIT Entry   : Raw
int32 AS_Test_Differential_Bytecode_TypedASTPrivateBridgeShowcase(FScriptExecution& Execution, asDWORD p_A, asDWORD p_B)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int TypedASTPrivateBridgeShowcase(int, int)", 4);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[8];
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
// PshV4 v-1
// PshV4 v0
// CALLSYS *
// int TypedASTProviderPrivateAdd(int Left, int Right)
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 0u).GetFunction();
  auto CastedFuncPtr = (int32(*)(int32,int32))RawFuncPtr;
SCRIPT_DEBUG_CALLSTACK_POSITION(4, 2);
int32 FunctionReturnValue = CastedFuncPtr(value_as<int32>((p_A)),value_as<int32>((p_B)));
l_dwordRegister = (asDWORD&)FunctionReturnValue;
if (Execution.bExceptionThrown) [[unlikely]]
{
return {};
}
}
// JitEntry *
// CpyRtoV4 v1
v_TEMP_dword_1 = l_dwordRegister;
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int TypedASTPrivateBridgeShowcase(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:2:1
// JIT Entry   : VM
void AS_Test_Differential_Bytecode_TypedASTPrivateBridgeShowcase_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_Test_Differential_Bytecode_TypedASTPrivateBridgeShowcase(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}

namespace
{
	static const FAngelscriptJITReferenceSlot FBytecodeJITDifferentialEntryRegistrationReferenceSlots[1] =
	{
		{sizeof(FAngelscriptJITReferenceSlot), 0u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(7), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("6b00a476bd57a82fb216373cb0eef27d00c4acb64af32b342c47c9b7dee91aac")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("304fbe10a2078ab14e2f67d70c35fd3322a0e9ce8b25e1915cebe00e271b6e96")))}}},
	};

	struct FBytecodeJITDifferentialEntryRegistration
	{
		FBytecodeJITDifferentialEntryRegistration()
		{
			AngelscriptTestJITProbes::RegisterIsolatedDifferentialEntry(
				AngelscriptTestJITProbes::EIsolatedDifferentialBackend::BytecodeJIT,
				&AS_Test_Differential_Bytecode_TypedASTPrivateBridgeShowcase_VMEntry,
				reinterpret_cast<asJITFunction_Raw>(&AS_Test_Differential_Bytecode_TypedASTPrivateBridgeShowcase),
				nullptr,
				TEXT("AS_Test_Differential_Bytecode_TypedASTPrivateBridgeShowcase"),
				TEXT("6608a6a94c2205850a5fb4a8b02ab7c08d63ec5722236ff400d6d9d50cd0625e"),
				TEXT("int TypedASTPrivateBridgeShowcase(const int, const int)"),
				TEXT("/Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as"),
				2u,
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
