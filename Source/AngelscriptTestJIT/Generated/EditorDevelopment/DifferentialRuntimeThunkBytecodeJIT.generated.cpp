// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : BytecodeJIT
 * Declaration        : int TypedASTRuntimeThunkShowcase(const FString&in)
 * StableFunctionKey  : 59acf1e8407b0391645aa3d2d8820c635615ea548e9b9326e488a5d12bebf48e
 * OriginalSymbolSet  : ASJIT_59acf1e8407b0391645aa3d2d8820c635615ea548e9b9326e488a5d12bebf48e_b1f472281d31513ffceb9b98bdf143af7c79e2ed085271dffb54e20689c03beb
 * TestSymbolSet      : AS_Test_Differential_Bytecode_TypedASTRuntimeThunkShowcase
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
 * Declaration       : int TypedASTRuntimeThunkShowcase(const FString&in)
 * Source             : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:8:1
 * StableFunctionKey  : 59acf1e8407b0391645aa3d2d8820c635615ea548e9b9326e488a5d12bebf48e
 * ExecutionHash      : b1f472281d31513ffceb9b98bdf143af7c79e2ed085271dffb54e20689c03beb
 * DebugHash          : 84c4c455833d298a19e72e37e3dd5fd456053781b60624be02a142f198bb4733
 * EntryAbiHash       : e5c8bce0a8a9a9f9f3e38534d038a379049fe8f730739008758392ca101478f3
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_AS_Test_Differential_Bytecode_TypedASTRuntimeThunkShowcase = "ASStaticJITTypedPrivateBridgeFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_AS_Test_Differential_Bytecode_TypedASTRuntimeThunkShowcase
#endif

// AS Function : int TypedASTRuntimeThunkShowcase(const FString&in)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:8:1
// JIT Entry   : Raw
int32 AS_Test_Differential_Bytecode_TypedASTRuntimeThunkShowcase(FScriptExecution& Execution, FString* p_Text)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int TypedASTRuntimeThunkShowcase(FString)", 10);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[8];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
// TrackRef v0
// JitEntry *
// SUSPEND
// JitEntry *
// ValidateRef v0
// PshVPtr v0
// CALLSYS *
// int TypedASTRuntimeThunkProbe(const FString&in Text)
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 0u).GetFunction();
  auto CastedFuncPtr = (int32(*)(void*))RawFuncPtr;
SCRIPT_DEBUG_CALLSTACK_POSITION(10, 2);
int32 FunctionReturnValue = CastedFuncPtr((void**)(p_Text));
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
// UntrackRef v0
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int TypedASTRuntimeThunkShowcase(const FString&in)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:8:1
// JIT Entry   : VM
void AS_Test_Differential_Bytecode_TypedASTRuntimeThunkShowcase_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_Test_Differential_Bytecode_TypedASTRuntimeThunkShowcase(Execution,
		*(FString**)(l_fp + 0));
}

namespace
{
	static const FAngelscriptJITReferenceSlot FBytecodeJITDifferentialEntryRegistrationReferenceSlots[2] =
	{
		{sizeof(FAngelscriptJITReferenceSlot), 0u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(7), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("11b5263801f1c0eff5669352b69d7e14aefdbe45bf94da7108751575993f81d4")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("fe6ff001ec000eaef3a24f1ace9e45af7036e1e072a9b26ad0c923488701363b")))}}},
		{sizeof(FAngelscriptJITReferenceSlot), 1u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(7), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("fce382e1f207809532a575043a4e727bc6fb503d99b6b8700fd77d71906a1080")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("207b6b88fbdf7c084cb14ddada1e386a939d1754325d527f699e94d41cd5de56")))}}},
	};

	struct FBytecodeJITDifferentialEntryRegistration
	{
		FBytecodeJITDifferentialEntryRegistration()
		{
			AngelscriptTestJITProbes::RegisterIsolatedDifferentialEntry(
				AngelscriptTestJITProbes::EIsolatedDifferentialBackend::BytecodeJIT,
				&AS_Test_Differential_Bytecode_TypedASTRuntimeThunkShowcase_VMEntry,
				reinterpret_cast<asJITFunction_Raw>(&AS_Test_Differential_Bytecode_TypedASTRuntimeThunkShowcase),
				nullptr,
				TEXT("AS_Test_Differential_Bytecode_TypedASTRuntimeThunkShowcase"),
				TEXT("59acf1e8407b0391645aa3d2d8820c635615ea548e9b9326e488a5d12bebf48e"),
				TEXT("int TypedASTRuntimeThunkShowcase(const FString&in)"),
				TEXT("/Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as"),
				8u,
				1u,
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
